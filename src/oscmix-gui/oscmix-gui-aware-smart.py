# oscmix_gui/main.py
# Refactored and documented version of the PySide6 GUI for oscmix control.
# Updated based on oscmix C source, device definitions, packaging, Tabs, Web GUI elements, and Mixer Tab.

import sys
import time
import threading
import queue
from collections import defaultdict
import functools # For partial function application in connect

# --- Qt Imports ---
# Attempt to import PySide6, provide helpful error if missing
try:
    from PySide6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QGridLayout, QLabel, QLineEdit, QPushButton, QSlider, QCheckBox,
        QProgressBar, QGroupBox, QStatusBar, QMessageBox, QTextEdit,
        QTabWidget, QDoubleSpinBox, QSpinBox, QComboBox, QScrollArea
    )
    from PySide6.QtCore import Qt, QThread, Signal, Slot, QObject, QTimer, QMetaObject, Q_ARG
    from PySide6.QtGui import QColor, QPalette, QFont
except ImportError:
    print("ERROR: 'PySide6' library not found.")
    print("PySide6 is required for the GUI.")
    print("Please install it, preferably within a virtual environment:")
    print("  pip install PySide6")
    sys.exit(1)

# --- OSC Imports ---
# Attempt to import python-osc, provide helpful error if missing
try:
    from pythonosc.dispatcher import Dispatcher
    from pythonosc.osc_server import BlockingOSCUDPServer
    from pythonosc.udp_client import SimpleUDPClient
except ImportError:
    print("ERROR: 'python-osc' library not found.")
    print("python-osc is required for OSC communication.")
    print("Please install it, preferably within a virtual environment:")
    print("  pip install python-osc")
    sys.exit(1)

# =============================================================================
# --- Configuration Constants ---
# =============================================================================

# --- OSC Address Patterns (Based on oscmix.c analysis) ---
# !! VERIFY these paths against your specific oscmix implementation !!
# {ch} represents the 1-based channel index
# {out_ch}, {in_ch} represent 1-based output/input channel indices
# {in_type} is "input" or "playback"

# Input Channel OSC Paths
OSC_INPUT_GAIN = "/input/{ch}/gain"         # float 0.0 to 75.0
OSC_INPUT_STEREO = "/input/{ch}/stereo"     # int 0/1 (on odd ch index)
OSC_INPUT_LEVEL = "/input/{ch}/level"       # multiple floats + int (meter data)
OSC_INPUT_MUTE = "/input/{ch}/mute"         # int 0/1
OSC_INPUT_PHASE = "/input/{ch}/phase"       # int 0/1
OSC_INPUT_48V = "/input/{ch}/48v"           # int 0/1 (Phantom Power)
OSC_INPUT_HIZ = "/input/{ch}/hi-z"          # int 0/1 (Instrument/Hi-Z)
OSC_INPUT_EQ_ENABLE = "/input/{ch}/eq"      # int 0/1 (EQ Bypass)
OSC_INPUT_DYN_ENABLE = "/input/{ch}/dynamics" # int 0/1 (Dynamics Bypass)
OSC_INPUT_LC_ENABLE = "/input/{ch}/lowcut"    # int 0/1 (Low Cut Bypass)

# Output Channel OSC Paths
OSC_OUTPUT_VOLUME = "/output/{ch}/volume"    # float -65.0 to +6.0 dB
OSC_OUTPUT_STEREO = "/output/{ch}/stereo"    # int 0/1 (on odd ch index)
OSC_OUTPUT_LEVEL = "/output/{ch}/level"      # multiple floats + int (meter data)
OSC_OUTPUT_MUTE = "/output/{ch}/mute"        # int 0/1
OSC_OUTPUT_PHASE = "/output/{ch}/phase"      # int 0/1
OSC_OUTPUT_EQ_ENABLE = "/output/{ch}/eq"     # int 0/1 (EQ Bypass)
OSC_OUTPUT_DYN_ENABLE = "/output/{ch}/dynamics"# int 0/1 (Dynamics Bypass)
OSC_OUTPUT_LC_ENABLE = "/output/{ch}/lowcut"   # int 0/1 (Low Cut Bypass)

# Playback Channel OSC Paths
OSC_PLAYBACK_LEVEL = "/playback/{ch}/level" # multiple floats + int (meter data)

# Mixer Matrix Send OSC Paths
OSC_MIX_SEND_VOLUME = "/mix/{out_ch}/{in_type}/{in_ch}" # Incoming volume update path (dB float)
OSC_MIX_SEND_PAN = "/mix/{out_ch}/{in_type}/{in_ch}/pan" # Incoming pan update path (int -100 to 100)
OSC_MIX_SEND_SET = "/mix/{out_ch}/{in_type}/{in_ch}" # Path to send volume + pan + width (width not implemented yet)

# Global OSC Paths
OSC_REFRESH = "/refresh" # Command to request full state update from oscmix

# --- Connection Defaults ---
DEFAULT_OSCMIX_IP = "127.0.0.1"     # Default IP for the oscmix server
DEFAULT_OSCMIX_SEND_PORT = 8000     # Default Port oscmix server LISTENS on (VERIFY!)
DEFAULT_LISTEN_IP = "0.0.0.0"       # Default IP this GUI listens on
DEFAULT_LISTEN_PORT = 9001          # Default Port this GUI listens on

# --- Channel Counts & Info (Specific to Fireface UCX II based on device_ffucxii.c) ---
# Flags constants (mirroring device.h for clarity)
INPUT_GAIN_FLAG = 1 << 0
INPUT_REFLEVEL_FLAG = 1 << 1 # Not currently used in GUI
INPUT_48V_FLAG = 1 << 2
INPUT_HIZ_FLAG = 1 << 3 # Assuming Hi-Z uses this bit based on oscmix.c path

# Input Channel Info: Tuple of (Name, Flags)
INPUT_CHANNEL_INFO = [
    ("Mic/Line 1", INPUT_GAIN_FLAG | INPUT_48V_FLAG), ("Mic/Line 2", INPUT_GAIN_FLAG | INPUT_48V_FLAG),
    ("Inst/Line 3", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG | INPUT_HIZ_FLAG), ("Inst/Line 4", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG | INPUT_HIZ_FLAG),
    ("Analog 5", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG), ("Analog 6", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG),
    ("Analog 7", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG), ("Analog 8", INPUT_GAIN_FLAG | INPUT_REFLEVEL_FLAG),
    ("SPDIF L", 0), ("SPDIF R", 0), ("AES L", 0), ("AES R", 0),
    ("ADAT 1", 0), ("ADAT 2", 0), ("ADAT 3", 0), ("ADAT 4", 0),
    ("ADAT 5", 0), ("ADAT 6", 0), ("ADAT 7", 0), ("ADAT 8", 0),
]
# Output Channel Info: Tuple of (Name, Flags) - OUTPUT_REFLEVEL=1 (not used in GUI yet)
OUTPUT_CHANNEL_INFO = [
    ("Analog 1", 1), ("Analog 2", 1), ("Analog 3", 1), ("Analog 4", 1),
    ("Analog 5", 1), ("Analog 6", 1), ("Phones 7", 1), ("Phones 8", 1),
    ("SPDIF L", 0), ("SPDIF R", 0), ("AES L", 0), ("AES R", 0),
    ("ADAT 1", 0), ("ADAT 2", 0), ("ADAT 3", 0), ("ADAT 4", 0),
    ("ADAT 5", 0), ("ADAT 6", 0), ("ADAT 7", 0), ("ADAT 8", 0),
]
# Playback Channel Info: Tuple of (Name, Flags)
PLAYBACK_CHANNEL_INFO = [(f"PB {i+1}", 0) for i in range(len(OUTPUT_CHANNEL_INFO))] # Assume same count as outputs

NUM_INPUT_CHANNELS = len(INPUT_CHANNEL_INFO)
NUM_OUTPUT_CHANNELS = len(OUTPUT_CHANNEL_INFO)
NUM_PLAYBACK_CHANNELS = len(PLAYBACK_CHANNEL_INFO)

# --- GUI Update Rates ---
METER_GUI_UPDATE_RATE_HZ = 25 # Target meter update frequency
METER_GUI_UPDATE_INTERVAL_MS = int(1000 / METER_GUI_UPDATE_RATE_HZ) # Timer interval

# =============================================================================
# --- Helper Functions ---
# =============================================================================

def float_to_slider_linear(val):
    """Converts a float (0.0-1.0) to an integer slider value (0-100). Used for meters."""
    try: return int(max(0.0, min(1.0, float(val))) * 100)
    except (ValueError, TypeError): return 0

def input_gain_to_slider(db_value):
    """Converts input gain float (0.0-75.0 dB) to slider int (0-750)."""
    try: return int(max(0.0, min(75.0, float(db_value))) * 10.0)
    except (ValueError, TypeError): return 0

def slider_to_input_gain(slider_value):
    """Converts slider int (0-750) to input gain float (0.0-75.0 dB)."""
    return float(slider_value) / 10.0

def output_volume_to_slider(db_value):
    """Converts output/mix volume float (-65.0 to 6.0 dB) to slider int (-650 to 60)."""
    try: return int(max(-65.0, min(6.0, float(db_value))) * 10.0)
    except (ValueError, TypeError): return -650 # Default to minimum slider value

def slider_to_output_volume(slider_value):
    """Converts slider int (-650 to 60) to output/mix volume float (-65.0 to 6.0 dB)."""
    return float(slider_value) / 10.0

def mix_send_volume_to_slider(db_value):
    """Alias for converting mix send volume (dB) to slider value."""
    return output_volume_to_slider(db_value)

def slider_to_mix_send_volume(slider_value):
    """Alias for converting slider value to mix send volume (dB)."""
    return slider_to_output_volume(slider_value)

def pan_to_spinbox(pan_val):
    """Converts pan value (-100 to 100) to integer spinbox value, clamping."""
    try: return int(max(-100, min(100, int(pan_val))))
    except (ValueError, TypeError): return 0 # Default to center

def spinbox_to_pan(spinbox_val):
    """Converts integer spinbox value to pan value (-100 to 100)."""
    return int(spinbox_val) # Assumes spinbox range is already set correctly

#=============================================================================
# OSC Communication Handler Class
#=============================================================================
class OSCHandler(QObject):
    """
    Manages OSC communication (sending and receiving) with the oscmix server.

    Runs an OSC server in a separate thread to receive messages from oscmix.
    Provides methods to send OSC messages to oscmix.
    Uses Qt signals to communicate received data safely to the main GUI thread.

    Signals:
        connected: Emitted when the OSC server successfully starts listening.
        disconnected(str): Emitted when the OSC server stops or fails to start.
                           The string argument provides the reason.
        message_received_signal(str, list): Emitted for every received OSC message (for logging).
                                            Args: address (str), arguments (list).
        meter_received(str, int, float): Emitted for meter level updates.
                                         Args: channel_type ('input'/'output'/'playback'),
                                               channel_index (0-based), value (float, first from /level msg).
        gain_received(str, int, float): Emitted for input gain or output volume updates.
                                        Args: channel_type ('input'/'output'),
                                              channel_index (0-based), value (float, dB).
        link_received(str, int, bool): Emitted for stereo link status updates.
                                       Args: channel_type ('input'/'output'),
                                             channel_index (0-based, odd channel), value (bool).
        mute_received(str, int, bool): Emitted for mute status updates. Args: channel_type, index, value.
        phase_received(str, int, bool): Emitted for phase status updates. Args: channel_type, index, value.
        phantom_received(str, int, bool): Emitted for +48V status updates. Args: 'input', index, value.
        hiz_received(str, int, bool): Emitted for Hi-Z status updates. Args: 'input', index, value.
        eq_enable_received(str, int, bool): Emitted for EQ bypass status updates. Args: channel_type, index, value.
        dyn_enable_received(str, int, bool): Emitted for Dynamics bypass status updates. Args: channel_type, index, value.
        lc_enable_received(str, int, bool): Emitted for Low Cut bypass status updates. Args: channel_type, index, value.
        mix_send_received(int, str, int, float): Emitted for mixer send volume updates.
                                                 Args: output_index (0-based), input_type ('input'/'playback'),
                                                       input_index (0-based), value (float, dB).
        mix_pan_received(int, str, int, int): Emitted for mixer send pan updates.
                                              Args: output_index (0-based), input_type ('input'/'playback'),
                                                    input_index (0-based), value (int, -100 to 100).
    """
    connected = Signal()
    disconnected = Signal(str)
    message_received_signal = Signal(str, list)
    meter_received = Signal(str, int, float)
    gain_received = Signal(str, int, float)
    link_received = Signal(str, int, bool)
    mute_received = Signal(str, int, bool)
    phase_received = Signal(str, int, bool)
    phantom_received = Signal(str, int, bool)
    hiz_received = Signal(str, int, bool)
    eq_enable_received = Signal(str, int, bool)
    dyn_enable_received = Signal(str, int, bool)
    lc_enable_received = Signal(str, int, bool)
    mix_send_received = Signal(int, str, int, float)
    mix_pan_received = Signal(int, str, int, int)

    def __init__(self, listen_ip, listen_port, parent=None):
        """
        Initializes the OSCHandler.

        Args:
            listen_ip (str): The IP address for the internal OSC server to listen on.
            listen_port (int): The port number for the internal OSC server to listen on.
            parent (QObject, optional): Parent object in the Qt hierarchy. Defaults to None.
        """
        super().__init__(parent)
        self.listen_ip = listen_ip
        self.listen_port = listen_port
        self.target_ip = ""
        self.target_port = 0
        self.client = None
        self.server = None
        self.server_thread = None
        self.dispatcher = Dispatcher()
        self._stop_event = threading.Event()
        self._is_connected = False

        print("--- OSC Address Mapping (Verify paths!) ---")
        self._map_osc_addresses()
        print("-------------------------------------------")
        # Catch any messages from oscmix that aren't explicitly mapped
        self.dispatcher.set_default_handler(self._default_handler)

    def _map_osc_addresses(self):
        """Sets up the OSC address mappings in the dispatcher based on constants."""
        # Print summary of mappings for verification
        # ... (mapping print statements remain the same) ...
        print(f"Mapping Inputs : Gain={OSC_INPUT_GAIN.format(ch='<ch>')}, Stereo={OSC_INPUT_STEREO.format(ch='<ch>')}, Level={OSC_INPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_INPUT_MUTE.format(ch='<ch>')}, Phase={OSC_INPUT_PHASE.format(ch='<ch>')}, 48V={OSC_INPUT_48V.format(ch='<ch>')}, HiZ={OSC_INPUT_HIZ.format(ch='<ch>')}, EQ={OSC_INPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_INPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_INPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Outputs: Vol={OSC_OUTPUT_VOLUME.format(ch='<ch>')}, Stereo={OSC_OUTPUT_STEREO.format(ch='<ch>')}, Level={OSC_OUTPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_OUTPUT_MUTE.format(ch='<ch>')}, Phase={OSC_OUTPUT_PHASE.format(ch='<ch>')}, EQ={OSC_OUTPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_OUTPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_OUTPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Playback: Level={OSC_PLAYBACK_LEVEL.format(ch='<ch>')} (Meters only)")
        print(f"Mapping Mix Send Volume: {OSC_MIX_SEND_VOLUME.format(out_ch='<out>', in_type='<type>', in_ch='<in>')}")
        print(f"Mapping Mix Send Pan   : {OSC_MIX_SEND_PAN.format(out_ch='<out>', in_type='<type>', in_ch='<in>')}")

        # Map input/output/playback channels using a loop for brevity
        # Tuple structure: (type_key, channel_info_list, gain_path, stereo_path, ...)
        channel_configs = [
            ("input", INPUT_CHANNEL_INFO, OSC_INPUT_GAIN, OSC_INPUT_STEREO, OSC_INPUT_LEVEL, OSC_INPUT_MUTE, OSC_INPUT_PHASE, OSC_INPUT_EQ_ENABLE, OSC_INPUT_DYN_ENABLE, OSC_INPUT_LC_ENABLE, OSC_INPUT_48V, OSC_INPUT_HIZ),
            ("output", OUTPUT_CHANNEL_INFO, OSC_OUTPUT_VOLUME, OSC_OUTPUT_STEREO, OSC_OUTPUT_LEVEL, OSC_OUTPUT_MUTE, OSC_OUTPUT_PHASE, OSC_OUTPUT_EQ_ENABLE, OSC_OUTPUT_DYN_ENABLE, OSC_OUTPUT_LC_ENABLE, None, None),
            ("playback", PLAYBACK_CHANNEL_INFO, None, None, OSC_PLAYBACK_LEVEL, None, None, None, None, None, None, None)
        ]
        for config in channel_configs:
            type_key, ch_info_list, gain_path, stereo_path, level_path, mute_path, \
            phase_path, eq_path, dyn_path, lc_path, ph_path, hiz_path = config
            num_ch = len(ch_info_list)
            for i in range(1, num_ch + 1):
                ch_idx_0_based = i - 1
                # Map each path if it's defined for this channel type
                if gain_path: self.dispatcher.map(gain_path.format(ch=i), self._handle_gain, type_key, ch_idx_0_based)
                if level_path: self.dispatcher.map(level_path.format(ch=i), self._handle_meter, type_key, ch_idx_0_based)
                if mute_path: self.dispatcher.map(mute_path.format(ch=i), self._handle_bool, f"{type_key}_mute", ch_idx_0_based)
                if phase_path: self.dispatcher.map(phase_path.format(ch=i), self._handle_bool, f"{type_key}_phase", ch_idx_0_based)
                if eq_path: self.dispatcher.map(eq_path.format(ch=i), self._handle_bool, f"{type_key}_eq_enable", ch_idx_0_based)
                if dyn_path: self.dispatcher.map(dyn_path.format(ch=i), self._handle_bool, f"{type_key}_dyn_enable", ch_idx_0_based)
                if lc_path: self.dispatcher.map(lc_path.format(ch=i), self._handle_bool, f"{type_key}_lc_enable", ch_idx_0_based)
                if ph_path: self.dispatcher.map(ph_path.format(ch=i), self._handle_bool, f"{type_key}_phantom", ch_idx_0_based)
                if hiz_path: self.dispatcher.map(hiz_path.format(ch=i), self._handle_bool, f"{type_key}_hiz", ch_idx_0_based)
                # Map stereo link only for odd-numbered channels (index i is 1-based)
                if stereo_path and (i % 2) != 0:
                    self.dispatcher.map(stereo_path.format(ch=i), self._handle_link, type_key, ch_idx_0_based)

        # Map Mixer Sends (Volume and Pan separately as received from oscmix)
        for out_ch in range(1, NUM_OUTPUT_CHANNELS + 1):
            out_idx_0_based = out_ch - 1
            for in_type, num_in_ch in [("input", NUM_INPUT_CHANNELS), ("playback", NUM_PLAYBACK_CHANNELS)]:
                for in_ch in range(1, num_in_ch + 1):
                    in_idx_0_based = in_ch - 1
                    # Map volume path (e.g., /mix/1/input/5)
                    vol_path = OSC_MIX_SEND_VOLUME.format(out_ch=out_ch, in_type=in_type, in_ch=in_ch)
                    # Pass "volume" as the update_type argument to the handler
                    self.dispatcher.map(vol_path, self._handle_mix_update, out_idx_0_based, in_type, in_idx_0_based, "volume")
                    # Map pan path (e.g., /mix/1/input/5/pan)
                    pan_path = OSC_MIX_SEND_PAN.format(out_ch=out_ch, in_type=in_type, in_ch=in_ch)
                    # Pass "pan" as the update_type argument to the handler
                    self.dispatcher.map(pan_path, self._handle_mix_update, out_idx_0_based, in_type, in_idx_0_based, "pan")

    # --- Handlers for specific OSC messages ---
    # These methods parse incoming OSC messages and emit Qt signals.
    # They run in the OSC server thread, so GUI updates must use invokeMethod.

    def _handle_meter(self, address, *args):
        """Handles incoming meter level messages (e.g., /input/1/level)."""
        self.message_received_signal.emit(address, list(args)) # Log raw message first
        # args[0]=channel_type, args[1]=ch_idx_0_based, args[2]=float1, args[3]=float2,...
        if len(args) >= 3:
             channel_type, ch_idx, value = args[0], args[1], args[2] # Use the first float value
             if isinstance(value, (float, int)):
                 QMetaObject.invokeMethod(self, "emit_meter_received", Qt.QueuedConnection,
                                          Q_ARG(str, channel_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_gain(self, address, *args):
        """Handles incoming gain/volume messages (e.g., /input/1/gain, /output/1/volume)."""
        self.message_received_signal.emit(address, list(args))
        # args[0]=channel_type, args[1]=ch_idx_0_based, args[2]=value_db
        if len(args) >= 3:
             channel_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (float, int)):
                 QMetaObject.invokeMethod(self, "emit_gain_received", Qt.QueuedConnection,
                                          Q_ARG(str, channel_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_link(self, address, *args):
        """Handles incoming stereo link status messages (e.g., /input/1/stereo)."""
        self.message_received_signal.emit(address, list(args))
        # args[0]=channel_type, args[1]=ch_idx_0_based (odd), args[2]=value (0 or 1)
        if len(args) >= 3:
             channel_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (int, float, bool)): # oscmix likely sends int 0/1
                 QMetaObject.invokeMethod(self, "emit_link_received", Qt.QueuedConnection,
                                          Q_ARG(str, channel_type), Q_ARG(int, ch_idx), Q_ARG(bool, bool(value)))

    def _handle_bool(self, address, *args):
        """Generic handler for boolean (on/off) OSC messages (Mute, Phase, 48V, etc.)."""
        self.message_received_signal.emit(address, list(args))
        # args[0]=signal_type (e.g., "input_mute"), args[1]=ch_idx_0_based, args[2]=value (0 or 1)
        if len(args) >= 3:
            signal_type, ch_idx, value = args[0], args[1], args[2]
            if isinstance(value, (int, float, bool)):
                bool_value = bool(value)
                # Determine the base type (input/output) and control type (mute/phase/etc.)
                try:
                    base_type, control_type = signal_type.split('_', 1)
                except ValueError:
                    print(f"Warning: Could not parse boolean signal type '{signal_type}'")
                    return

                # Map control_type to the correct signal emitter method
                signal_map = {
                    "mute": self.emit_mute_received,
                    "phase": self.emit_phase_received,
                    "phantom": self.emit_phantom_received,
                    "hiz": self.emit_hiz_received,
                    "eq_enable": self.emit_eq_enable_received,
                    "dyn_enable": self.emit_dyn_enable_received,
                    "lc_enable": self.emit_lc_enable_received,
                    # Add other boolean controls here if needed
                }
                signal_emitter = signal_map.get(control_type)

                if signal_emitter:
                    # Emit the specific signal safely to the GUI thread
                    QMetaObject.invokeMethod(self, signal_emitter.__name__, Qt.QueuedConnection,
                                             Q_ARG(str, base_type), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                else:
                    print(f"Warning: Unhandled boolean control type '{control_type}' derived from '{signal_type}'")

    def _handle_mix_update(self, address, *args):
        """Handles incoming mix send volume or pan updates from oscmix."""
        self.message_received_signal.emit(address, list(args))
        # args[0]=out_idx, args[1]=in_type, args[2]=in_idx, args[3]=update_type ("volume" or "pan"), args[4]=value
        if len(args) >= 5:
            out_idx, in_type, in_idx, update_type, value = args[0], args[1], args[2], args[3], args[4]
            if update_type == "volume":
                if isinstance(value, (float, int)):
                    QMetaObject.invokeMethod(self, "emit_mix_send_received", Qt.QueuedConnection,
                                             Q_ARG(int, out_idx), Q_ARG(str, in_type), Q_ARG(int, in_idx), Q_ARG(float, float(value)))
            elif update_type == "pan":
                 if isinstance(value, (int, float)): # Pan is likely int
                     QMetaObject.invokeMethod(self, "emit_mix_pan_received", Qt.QueuedConnection,
                                              Q_ARG(int, out_idx), Q_ARG(str, in_type), Q_ARG(int, in_idx), Q_ARG(int, int(value)))

    # --- Slots for Safe Signal Emission ---
    # These slots are targets for QMetaObject.invokeMethod calls from the handlers.
    # They ensure the corresponding signals are emitted in the main GUI thread context.
    @Slot(str, int, float)
    def emit_meter_received(self, channel_type, ch_idx, val): self.meter_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, float)
    def emit_gain_received(self, channel_type, ch_idx, val): self.gain_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_link_received(self, channel_type, ch_idx, val): self.link_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_mute_received(self, channel_type, ch_idx, val): self.mute_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_phase_received(self, channel_type, ch_idx, val): self.phase_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_phantom_received(self, channel_type, ch_idx, val): self.phantom_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_hiz_received(self, channel_type, ch_idx, val): self.hiz_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_eq_enable_received(self, channel_type, ch_idx, val): self.eq_enable_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_dyn_enable_received(self, channel_type, ch_idx, val): self.dyn_enable_received.emit(channel_type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_lc_enable_received(self, channel_type, ch_idx, val): self.lc_enable_received.emit(channel_type, ch_idx, val)
    @Slot(int, str, int, float)
    def emit_mix_send_received(self, out_idx, in_type, in_idx, val): self.mix_send_received.emit(out_idx, in_type, in_idx, val)
    @Slot(int, str, int, int)
    def emit_mix_pan_received(self, out_idx, in_type, in_idx, val): self.mix_pan_received.emit(out_idx, in_type, in_idx, val)

    def _default_handler(self, address, *args):
        """Handles any unmapped OSC messages received, emitting them for logging."""
        self.message_received_signal.emit(address, list(args))

    # --- Server Thread Logic ---
    def _server_thread_target(self):
        """Target function for the OSC server thread."""
        self._is_connected = False
        server_error = None
        try:
            self.server = BlockingOSCUDPServer((self.listen_ip, self.listen_port), self.dispatcher)
            print(f"[OSC Handler] Server listening on {self.server.server_address}")
            self._is_connected = True
            QMetaObject.invokeMethod(self, "connected", Qt.QueuedConnection)
            # Loop handling requests with a timeout to allow checking the stop event
            while not self._stop_event.is_set():
                self.server.handle_request_timeout(0.1)
        except OSError as e:
            server_error = f"Error starting server (Port {self.listen_port} in use?): {e}"
            print(f"!!! [OSC Handler] {server_error}")
        except Exception as e:
            server_error = f"Server error: {e}"
            print(f"!!! [OSC Handler] {server_error}")
        finally:
            # Cleanup actions when the server loop exits
            self._is_connected = False
            disconnect_reason = server_error if server_error else "Disconnected normally"
            if self.server:
                try:
                    self.server.server_close()
                except Exception as e_close:
                     print(f"[OSC Handler] Error closing server socket: {e_close}")
            print("[OSC Handler] Server stopped.")
            # Ensure disconnected signal is emitted from the main thread
            QMetaObject.invokeMethod(self, "emit_disconnected", Qt.QueuedConnection, Q_ARG(str, disconnect_reason))

    @Slot(str)
    def emit_disconnected(self, message):
         """Slot to ensure the disconnected signal is emitted in the correct thread."""
         self.disconnected.emit(message)

    # --- Public Control Methods ---
    def start(self, target_ip, target_port):
        """
        Starts the OSC server thread and initializes the OSC client.

        Args:
            target_ip (str): The IP address of the oscmix server to send messages to.
            target_port (int): The port number of the oscmix server.

        Returns:
            bool: True if the start attempt was initiated, False otherwise.
        """
        if self.server_thread and self.server_thread.is_alive():
            print("[OSC Handler] Already running.")
            return False

        self.target_ip = target_ip
        self.target_port = target_port
        try:
            if not (0 < target_port < 65536): raise ValueError("Invalid target port number")
            self.client = SimpleUDPClient(self.target_ip, self.target_port)
            print(f"[OSC Handler] Client configured for target {self.target_ip}:{self.target_port}")
        except Exception as e:
            self.emit_disconnected(f"Client config error: {e}")
            return False

        self._stop_event.clear()
        self.server_thread = threading.Thread(target=self._server_thread_target, daemon=True)
        self.server_thread.start()
        return True # Indicates start attempt was made

    def stop(self):
        """Stops the OSC server thread and cleans up resources."""
        if not self.server_thread or not self.server_thread.is_alive():
            if not self._is_connected: self.emit_disconnected("Stopped manually (was not running)")
            self._is_connected = False
            return

        print("[OSC Handler] Stopping...")
        self._stop_event.set()
        server_instance = getattr(self, 'server', None)
        if server_instance:
             try: server_instance.shutdown() # Non-blocking request
             except Exception as e: print(f"[OSC Handler] Error during server shutdown call: {e}")

        self.server_thread.join(timeout=1.0) # Wait briefly
        if self.server_thread.is_alive(): print("[OSC Handler] Warning: Server thread did not exit cleanly.")

        self.server_thread = None; self.server = None; self.client = None; self._is_connected = False
        print("[OSC Handler] Stopped.")
        # Disconnected signal is emitted by the server thread's finally block

    def send(self, address, value):
        """
        Sends an OSC message with a single argument to the target (oscmix).

        Args:
            address (str): The OSC address path.
            value: The single argument value to send.
        """
        if self.client and self._is_connected:
            try:
                # print(f"[OSC SEND] {address} {value}") # Uncomment for debug
                self.client.send_message(address, value)
            except Exception as e: print(f"!!! [OSC Handler] Send Error to {address}: {e}")
        elif not self._is_connected: print(f"[OSC Handler] Not connected, cannot send: {address} {value}")
        else: print(f"[OSC Handler] Client is None, cannot send: {address} {value}")

    def send_multi(self, address, values):
        """
        Sends an OSC message with multiple arguments to the target (oscmix).

        Args:
            address (str): The OSC address path.
            values (list): A list of argument values to send.
        """
        if self.client and self._is_connected:
            try:
                # print(f"[OSC SEND MULTI] {address} {values}") # Uncomment for debug
                self.client.send_message(address, values)
            except Exception as e: print(f"!!! [OSC Handler] Send Multi Error to {address}: {e}")
        elif not self._is_connected: print(f"[OSC Handler] Not connected, cannot send multi: {address} {values}")
        else: print(f"[OSC Handler] Client is None, cannot send multi: {address} {values}")


#=============================================================================
# Main GUI Window Class
#=============================================================================
class OscmixControlGUI(QMainWindow):
    """
    Main application window for controlling the oscmix server via OSC.

    Provides tabbed views for Hardware Inputs, Software Playback channels,
    Hardware Outputs, and the Mixer matrix. Allows viewing levels and
    controlling parameters like gain, volume, mute, phase, link, etc.
    """
    def __init__(self):
        """Initializes the main GUI window and its components."""
        super().__init__()
        self.setWindowTitle("oscmix GUI Control (RME UCX II)")
        self.setGeometry(100, 100, 1250, 800) # Adjusted size

        # Initialize the OSC communication handler
        self.osc_handler = OSCHandler(DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT)

        # --- State Storage ---
        # Cache for meter levels between GUI updates
        self.last_meter_values = {'input': defaultdict(float), 'output': defaultdict(float), 'playback': defaultdict(float)}
        # Cache for mixer send levels and pans
        # Format: self.current_mixer_sends[out_idx][in_type][in_idx] = {'volume': db, 'pan': p}
        self.current_mixer_sends = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: {'volume': -65.0, 'pan': 0})))
        # Currently selected output channel index in the Mixer tab
        self.current_mixer_output_idx = 0
        # Dictionary to store references to dynamically created widgets
        self.widgets = defaultdict(list)

        # Build the UI elements
        self.init_ui()
        # Connect signals and slots
        self.connect_signals()

        # Timer for throttled GUI meter updates
        self.meter_timer = QTimer(self)
        self.meter_timer.setInterval(METER_GUI_UPDATE_INTERVAL_MS)
        self.meter_timer.timeout.connect(self.update_gui_meters)

        # Initial status message
        self.status_bar.showMessage("Disconnected. Ensure 'oscmix' is running and configured.")

    # --- UI Initialization Methods ---

    def init_ui(self):
        """Creates and arranges the main GUI layout and widgets."""
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)

        # Create main sections
        self._create_connection_group(main_layout)
        self._create_main_tabs(main_layout)
        self._create_log_group(main_layout)
        self._create_status_bar()

    def _create_connection_group(self, parent_layout):
        """Creates the top connection status and control group."""
        conn_group = QGroupBox("Connection to 'oscmix' Server")
        conn_layout = QHBoxLayout(); conn_group.setLayout(conn_layout)

        # IP and Port input fields
        self.ip_input = QLineEdit(DEFAULT_OSCMIX_IP); self.ip_input.setToolTip("IP address where the oscmix server is running (often 127.0.0.1)")
        self.port_input = QLineEdit(str(DEFAULT_OSCMIX_SEND_PORT)); self.port_input.setToolTip("Port number the oscmix server is LISTENING on")
        self.listen_port_input = QLineEdit(str(DEFAULT_LISTEN_PORT)); self.listen_port_input.setToolTip("Port number this GUI should listen on for replies from oscmix")

        # Buttons
        self.connect_button = QPushButton("Connect")
        self.disconnect_button = QPushButton("Disconnect"); self.disconnect_button.setEnabled(False)
        self.refresh_button = QPushButton("Refresh State"); self.refresh_button.setToolTip("Request current state from oscmix"); self.refresh_button.setEnabled(False)

        # Status Label
        self.connection_status_label = QLabel("DISCONNECTED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")

        # Add widgets to layout
        conn_layout.addWidget(QLabel("'oscmix' Server IP:")); conn_layout.addWidget(self.ip_input)
        conn_layout.addWidget(QLabel("Server Port:")); conn_layout.addWidget(self.port_input)
        conn_layout.addWidget(QLabel("GUI Listen Port:")); conn_layout.addWidget(self.listen_port_input)
        conn_layout.addWidget(self.connect_button); conn_layout.addWidget(self.disconnect_button); conn_layout.addWidget(self.refresh_button)
        conn_layout.addStretch(); conn_layout.addWidget(self.connection_status_label)
        parent_layout.addWidget(conn_group)

    def _create_main_tabs(self, parent_layout):
        """Creates the main QTabWidget and populates its tabs."""
        self.tab_widget = QTabWidget()
        parent_layout.addWidget(self.tab_widget)

        # Create tab pages (QWidget containers)
        self.input_tab = QWidget()
        self.playback_tab = QWidget()
        self.output_tab = QWidget()
        self.mixer_tab = QWidget()

        # Add tabs to the widget
        self.tab_widget.addTab(self.input_tab, "Inputs")
        self.tab_widget.addTab(self.playback_tab, "Playback")
        self.tab_widget.addTab(self.output_tab, "Outputs")
        self.tab_widget.addTab(self.mixer_tab, "Mixer")

        # Populate each tab with its specific content
        self._populate_channel_tab(self.input_tab, "Inputs", "Hardware Inputs", INPUT_CHANNEL_INFO, True)
        self._populate_channel_tab(self.playback_tab, "Playback", "Software Playback", PLAYBACK_CHANNEL_INFO, False) # No controls for playback yet
        self._populate_channel_tab(self.output_tab, "Outputs", "Hardware Outputs", OUTPUT_CHANNEL_INFO, True)
        self._populate_mixer_tab()

    def _create_log_group(self, parent_layout):
        """Creates the OSC message log group box."""
        log_group = QGroupBox("OSC Message Log (Incoming from oscmix)")
        log_layout = QVBoxLayout(); log_group.setLayout(log_layout)
        self.osc_log = QTextEdit(); self.osc_log.setReadOnly(True)
        self.osc_log.setMaximumHeight(100); self.osc_log.setToolTip("Shows raw OSC messages received")
        log_layout.addWidget(self.osc_log)
        parent_layout.addWidget(log_group)

    def _create_status_bar(self):
        """Creates the application status bar."""
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

    def _populate_channel_tab(self, tab_widget, type_prefix, group_title, channel_info_list, add_controls):
        """
        Helper function to create the content for Input/Output/Playback tabs.

        Args:
            tab_widget (QWidget): The parent QWidget for the tab content.
            type_prefix (str): "Inputs", "Outputs", or "Playback".
            group_title (str): Title for the QGroupBox within the tab.
            channel_info_list (list): List of tuples, each containing (channel_name, channel_flags).
            add_controls (bool): If True, adds gain/volume sliders, checkboxes, etc. If False, only adds meters.
        """
        tab_layout = QVBoxLayout(tab_widget)
        group_box = QGroupBox(group_title)
        tab_layout.addWidget(group_box)
        grid_layout = QGridLayout()
        group_box.setLayout(grid_layout)
        num_channels = len(channel_info_list)

        # Define grid rows for layout consistency
        ROW_METER_LABEL = 0; ROW_METER = 1; ROW_GAIN_LABEL = 2; ROW_GAIN_SLIDER = 3
        ROW_GAIN_SPINBOX = 4; ROW_BOOL_BUTTONS_1 = 5; ROW_BOOL_BUTTONS_2 = 6
        ROW_SPECIAL_BUTTONS = 7; ROW_LINK = 8; ROW_CH_LABEL = 9

        # Add Header Row
        grid_layout.addWidget(QLabel("Meter"), ROW_METER_LABEL, 0, 1, num_channels, alignment=Qt.AlignCenter)
        if add_controls:
            gain_label = "Gain (dB)" if type_prefix == "Inputs" else "Volume (dB)"
            grid_layout.addWidget(QLabel(gain_label), ROW_GAIN_LABEL, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("Controls"), ROW_BOOL_BUTTONS_1, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("DSP"), ROW_BOOL_BUTTONS_2, 0, 1, num_channels, alignment=Qt.AlignCenter)
            if type_prefix == "Inputs": grid_layout.addWidget(QLabel("Input"), ROW_SPECIAL_BUTTONS, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("Link"), ROW_LINK, 0, 1, num_channels, alignment=Qt.AlignCenter)

        # Create widgets for each channel
        for i in range(num_channels):
            ch_name, ch_flags = channel_info_list[i]
            ch_label = QLabel(ch_name); ch_label.setToolTip(ch_name) # Use actual name from device info
            ch_idx_0_based = i
            widget_type_key = type_prefix.lower() # 'inputs', 'outputs', 'playback'

            # --- Meter ---
            meter_bar = QProgressBar(); meter_bar.setRange(0, 100); meter_bar.setValue(0)
            meter_bar.setTextVisible(False); meter_bar.setOrientation(Qt.Vertical)
            meter_bar.setFixedHeight(80); meter_bar.setToolTip(f"{type_prefix} Level")
            meter_bar.setObjectName(f"{widget_type_key}_meter_{ch_idx_0_based}")
            grid_layout.addWidget(meter_bar, ROW_METER, i, alignment=Qt.AlignCenter)
            self.widgets[f"{widget_type_key}_meter"].append(meter_bar)

            # --- Controls (Only if add_controls is True) ---
            if add_controls:
                # --- Gain/Volume Slider and SpinBox ---
                gain_slider = QSlider(Qt.Vertical)
                gain_spinbox = QDoubleSpinBox(); gain_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons)
                gain_spinbox.setDecimals(1); gain_spinbox.setFixedWidth(50)

                if type_prefix == "Inputs":
                    gain_slider.setRange(0, 750); gain_slider.setValue(0); gain_slider.setToolTip("Input Gain (0.0 to +75.0 dB)")
                    gain_spinbox.setRange(0.0, 75.0); gain_spinbox.setValue(0.0); gain_spinbox.setSingleStep(0.5)
                    # Disable gain controls if channel doesn't support it based on flags
                    has_gain = bool(ch_flags & INPUT_GAIN_FLAG)
                    gain_slider.setEnabled(has_gain); gain_spinbox.setEnabled(has_gain)
                else: # Outputs
                    gain_slider.setRange(-650, 60); gain_slider.setValue(-650); gain_slider.setToolTip("Output Volume (-65.0 to +6.0 dB)")
                    gain_spinbox.setRange(-65.0, 6.0); gain_spinbox.setValue(-65.0); gain_spinbox.setSingleStep(0.5)
                    # Outputs always have volume control in this model
                    has_gain = True

                gain_slider.setObjectName(f"{widget_type_key}_gain_{ch_idx_0_based}")
                gain_spinbox.setObjectName(f"{widget_type_key}_gain_spinbox_{ch_idx_0_based}")
                gain_spinbox.setToolTip(gain_slider.toolTip())

                grid_layout.addWidget(gain_slider, ROW_GAIN_SLIDER, i, alignment=Qt.AlignCenter)
                grid_layout.addWidget(gain_spinbox, ROW_GAIN_SPINBOX, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_gain"].append(gain_slider)
                self.widgets[f"{widget_type_key}_gain_spinbox"].append(gain_spinbox)

                # Link SpinBox and Slider value changes
                # Use functools.partial to capture current widget instances in lambda
                slider_slot = functools.partial(lambda s, t, value: s.setValue(slider_to_input_gain(value) if t == "Inputs" else slider_to_output_volume(value)), gain_spinbox, type_prefix)
                spinbox_slot = functools.partial(lambda s, t, value: s.setValue(input_gain_to_slider(value) if t == "Inputs" else output_volume_to_slider(value)), gain_slider, type_prefix)
                gain_slider.valueChanged.connect(slider_slot)
                gain_spinbox.valueChanged.connect(spinbox_slot)

                # --- Boolean Buttons (Mute, Phase) ---
                bool_layout_1 = QHBoxLayout(); bool_layout_1.setSpacing(2)
                mute_cb = QCheckBox("M"); mute_cb.setObjectName(f"{widget_type_key}_mute_{ch_idx_0_based}"); mute_cb.setToolTip("Mute")
                phase_cb = QCheckBox("Ø"); phase_cb.setObjectName(f"{widget_type_key}_phase_{ch_idx_0_based}"); phase_cb.setToolTip("Phase Invert")
                bool_layout_1.addWidget(mute_cb); bool_layout_1.addWidget(phase_cb)
                grid_layout.addLayout(bool_layout_1, ROW_BOOL_BUTTONS_1, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_mute"].append(mute_cb)
                self.widgets[f"{widget_type_key}_phase"].append(phase_cb)

                # --- Boolean Buttons (DSP Toggles) ---
                bool_layout_2 = QHBoxLayout(); bool_layout_2.setSpacing(2)
                eq_cb = QCheckBox("EQ"); eq_cb.setObjectName(f"{widget_type_key}_eq_enable_{ch_idx_0_based}"); eq_cb.setToolTip("EQ Enable")
                dyn_cb = QCheckBox("Dyn"); dyn_cb.setObjectName(f"{widget_type_key}_dyn_enable_{ch_idx_0_based}"); dyn_cb.setToolTip("Dynamics Enable")
                lc_cb = QCheckBox("LC"); lc_cb.setObjectName(f"{widget_type_key}_lc_enable_{ch_idx_0_based}"); lc_cb.setToolTip("Low Cut Enable")
                bool_layout_2.addWidget(eq_cb); bool_layout_2.addWidget(dyn_cb); bool_layout_2.addWidget(lc_cb)
                grid_layout.addLayout(bool_layout_2, ROW_BOOL_BUTTONS_2, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_eq_enable"].append(eq_cb)
                self.widgets[f"{widget_type_key}_dyn_enable"].append(dyn_cb)
                self.widgets[f"{widget_type_key}_lc_enable"].append(lc_cb)

                # --- Special Input Buttons (48V, Hi-Z) ---
                if type_prefix == "Inputs":
                    special_layout = QHBoxLayout(); special_layout.setSpacing(2)
                    phantom_cb = QCheckBox("48V"); phantom_cb.setObjectName(f"{widget_type_key}_phantom_{ch_idx_0_based}"); phantom_cb.setToolTip("Phantom Power (+48V)")
                    hiz_cb = QCheckBox("Inst"); hiz_cb.setObjectName(f"{widget_type_key}_hiz_{ch_idx_0_based}"); hiz_cb.setToolTip("Instrument (Hi-Z) Input")
                    # Set visibility AND enabled state based on flags
                    has_48v = bool(ch_flags & INPUT_48V_FLAG)
                    has_hiz = bool(ch_flags & INPUT_HIZ_FLAG)
                    phantom_cb.setVisible(has_48v); phantom_cb.setEnabled(has_48v)
                    hiz_cb.setVisible(has_hiz); hiz_cb.setEnabled(has_hiz)
                    special_layout.addWidget(phantom_cb); special_layout.addWidget(hiz_cb)
                    grid_layout.addLayout(special_layout, ROW_SPECIAL_BUTTONS, i, alignment=Qt.AlignCenter)
                    self.widgets[f"{widget_type_key}_phantom"].append(phantom_cb)
                    self.widgets[f"{widget_type_key}_hiz"].append(hiz_cb)
                else:
                    # Add placeholders for non-input channels to keep lists aligned
                    self.widgets[f"{widget_type_key}_phantom"].append(None)
                    self.widgets[f"{widget_type_key}_hiz"].append(None)

                # --- Link Checkbox ---
                link_cb = QCheckBox()
                link_cb.setObjectName(f"{widget_type_key}_link_{ch_idx_0_based}")
                link_cb.setVisible((i % 2) == 0) # Only show on odd channels (index 0, 2, ...)
                link_cb.setToolTip("Link Ch {}".format(i+1) + "/{}".format(i+2) if (i%2)==0 else "")
                grid_layout.addWidget(link_cb, ROW_LINK, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_link"].append(link_cb)
            else:
                 # Add placeholders for controls if not added, to keep lists aligned
                 self.widgets[f"{widget_type_key}_gain"].append(None)
                 self.widgets[f"{widget_type_key}_gain_spinbox"].append(None)
                 self.widgets[f"{widget_type_key}_mute"].append(None)
                 self.widgets[f"{widget_type_key}_phase"].append(None)
                 self.widgets[f"{widget_type_key}_eq_enable"].append(None)
                 self.widgets[f"{widget_type_key}_dyn_enable"].append(None)
                 self.widgets[f"{widget_type_key}_lc_enable"].append(None)
                 self.widgets[f"{widget_type_key}_phantom"].append(None)
                 self.widgets[f"{widget_type_key}_hiz"].append(None)
                 self.widgets[f"{widget_type_key}_link"].append(None)


            # --- Channel Label (always add) ---
            grid_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)


    def _populate_mixer_tab(self):
        """Creates the content for the Mixer tab."""
        mixer_tab_layout = QVBoxLayout(self.mixer_tab)

        # --- Output Channel Selector ---
        selector_layout = QHBoxLayout()
        selector_layout.addWidget(QLabel("Show Mix For Output:"))
        self.mixer_output_selector = QComboBox()
        for i in range(NUM_OUTPUT_CHANNELS):
            name, flags = OUTPUT_CHANNEL_INFO[i]
            # Store 0-based index in UserData role for easy retrieval
            self.mixer_output_selector.addItem(f"{i+1}: {name}", userData=i)
        # Connect the signal for when the selected index changes
        self.mixer_output_selector.currentIndexChanged.connect(self._on_mixer_output_selected)
        selector_layout.addWidget(self.mixer_output_selector)
        selector_layout.addStretch()
        mixer_tab_layout.addLayout(selector_layout)

        # --- Scroll Area for Mixer Strips ---
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True) # Important for the inner widget to resize correctly
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff) # Vertical sliders, no vertical scroll needed
        mixer_tab_layout.addWidget(scroll_area)

        # --- Container Widget for Mixer Strips (lives inside ScrollArea) ---
        self.mixer_strips_container = QWidget()
        scroll_area.setWidget(self.mixer_strips_container)
        # Use a QHBoxLayout to arrange the Input and Playback groups horizontally
        self.mixer_strips_layout = QHBoxLayout(self.mixer_strips_container)
        self.mixer_strips_layout.setSpacing(10) # Add some space between groups

        # Initial population for the first output channel
        self._update_mixer_view(0) # Show mix for Output 1 (index 0) initially

    def _update_mixer_view(self, output_ch_idx):
        """
        Clears and repopulates the mixer tab content for the selected output channel.

        Args:
            output_ch_idx (int): The 0-based index of the selected output channel.
        """
        self.current_mixer_output_idx = output_ch_idx
        print(f"[GUI] Updating mixer view for Output {output_ch_idx + 1}")

        # --- Clear previous widgets ---
        # Clear layout by taking and deleting widgets
        while self.mixer_strips_layout.count():
            item = self.mixer_strips_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater() # Ensure proper Qt cleanup

        # Clear widget references specific to the mixer tab
        for key in list(self.widgets.keys()): # Iterate over copy of keys
            if key.startswith("mixer_"):
                self.widgets[key].clear()

        # Get the name of the currently selected output for group titles
        output_name, _ = OUTPUT_CHANNEL_INFO[output_ch_idx]

        # --- Populate Input Sends Group ---
        input_sends_group = QGroupBox(f"Input Sends to Out {output_ch_idx + 1} ({output_name})")
        input_sends_layout = QGridLayout(input_sends_group)
        self.mixer_strips_layout.addWidget(input_sends_group) # Add group to horizontal layout

        # Define grid rows for mixer strips
        ROW_VOL_LABEL=0; ROW_VOL_SLIDER=1; ROW_VOL_SPIN=2
        ROW_PAN_LABEL=3; ROW_PAN_SPIN=4; ROW_CH_LABEL=5

        # Add headers
        input_sends_layout.addWidget(QLabel("Volume"), ROW_VOL_LABEL, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter)
        input_sends_layout.addWidget(QLabel("Pan"), ROW_PAN_LABEL, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter)

        # Create controls for each input channel send
        for i in range(NUM_INPUT_CHANNELS):
            in_ch_idx_0_based = i
            in_name, _ = INPUT_CHANNEL_INFO[i]
            ch_label = QLabel(f"{i+1}\n{in_name}"); ch_label.setAlignment(Qt.AlignCenter) # Multi-line label

            # Volume Slider & SpinBox for the send
            vol_slider = QSlider(Qt.Vertical); vol_slider.setRange(-650, 60); vol_slider.setValue(-650)
            vol_spinbox = QDoubleSpinBox(); vol_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons); vol_spinbox.setDecimals(1)
            vol_spinbox.setFixedWidth(50); vol_spinbox.setRange(-65.0, 6.0); vol_spinbox.setValue(-65.0)
            # Set object names encoding output index, type, and input index
            vol_slider.setObjectName(f"mixer_input_gain_{output_ch_idx}_{in_ch_idx_0_based}")
            vol_spinbox.setObjectName(f"mixer_input_gain_spinbox_{output_ch_idx}_{in_ch_idx_0_based}")
            vol_slider.setToolTip(f"Input {i+1} Send Volume"); vol_spinbox.setToolTip(vol_slider.toolTip())
            # Link slider and spinbox
            vol_slider.valueChanged.connect(lambda value, s=vol_spinbox: s.setValue(slider_to_mix_send_volume(value)))
            vol_spinbox.valueChanged.connect(lambda value, s=vol_slider: s.setValue(mix_send_volume_to_slider(value)))
            # Connect spinbox change directly to OSC sending function
            vol_spinbox.valueChanged.connect(self.send_mix_gain_from_gui)

            # Pan SpinBox for the send
            pan_spinbox = QSpinBox(); pan_spinbox.setRange(-100, 100); pan_spinbox.setValue(0)
            pan_spinbox.setButtonSymbols(QSpinBox.NoButtons); pan_spinbox.setFixedWidth(50)
            pan_spinbox.setObjectName(f"mixer_input_pan_spinbox_{output_ch_idx}_{in_ch_idx_0_based}")
            pan_spinbox.setToolTip(f"Input {i+1} Send Pan (L-100..0..R100)")
            # Connect spinbox change directly to OSC sending function
            pan_spinbox.valueChanged.connect(self.send_mix_pan_from_gui)

            # Add widgets to grid
            input_sends_layout.addWidget(vol_slider, ROW_VOL_SLIDER, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(vol_spinbox, ROW_VOL_SPIN, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(pan_spinbox, ROW_PAN_SPIN, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)

            # Store widget references
            self.widgets['mixer_input_gain'].append(vol_slider)
            self.widgets['mixer_input_gain_spinbox'].append(vol_spinbox)
            self.widgets['mixer_input_pan_spinbox'].append(pan_spinbox)

            # Set initial value from cache if available
            cached_val = self.current_mixer_sends[output_ch_idx]['input'][in_ch_idx_0_based]
            vol_spinbox.setValue(cached_val['volume'])
            pan_spinbox.setValue(cached_val['pan'])

        # --- Populate Playback Sends Group ---
        playback_sends_group = QGroupBox(f"Playback Sends to Out {output_ch_idx + 1} ({output_name})")
        playback_sends_layout = QGridLayout(playback_sends_group)
        self.mixer_strips_layout.addWidget(playback_sends_group) # Add group to horizontal layout

        # Add headers
        playback_sends_layout.addWidget(QLabel("Volume"), ROW_VOL_LABEL, 0, 1, NUM_PLAYBACK_CHANNELS, alignment=Qt.AlignCenter)
        playback_sends_layout.addWidget(QLabel("Pan"), ROW_PAN_LABEL, 0, 1, NUM_PLAYBACK_CHANNELS, alignment=Qt.AlignCenter)

        # Create controls for each playback channel send
        for i in range(NUM_PLAYBACK_CHANNELS):
            pb_ch_idx_0_based = i
            pb_name, _ = PLAYBACK_CHANNEL_INFO[i]
            ch_label = QLabel(f"{i+1}\n{pb_name}"); ch_label.setAlignment(Qt.AlignCenter)

            # Volume Slider & SpinBox
            vol_slider = QSlider(Qt.Vertical); vol_slider.setRange(-650, 60); vol_slider.setValue(-650)
            vol_spinbox = QDoubleSpinBox(); vol_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons); vol_spinbox.setDecimals(1)
            vol_spinbox.setFixedWidth(50); vol_spinbox.setRange(-65.0, 6.0); vol_spinbox.setValue(-65.0)
            vol_slider.setObjectName(f"mixer_playback_gain_{output_ch_idx}_{pb_ch_idx_0_based}")
            vol_spinbox.setObjectName(f"mixer_playback_gain_spinbox_{output_ch_idx}_{pb_ch_idx_0_based}")
            vol_slider.setToolTip(f"Playback {i+1} Send Volume"); vol_spinbox.setToolTip(vol_slider.toolTip())
            vol_slider.valueChanged.connect(lambda value, s=vol_spinbox: s.setValue(slider_to_mix_send_volume(value)))
            vol_spinbox.valueChanged.connect(lambda value, s=vol_slider: s.setValue(mix_send_volume_to_slider(value)))
            vol_spinbox.valueChanged.connect(self.send_mix_gain_from_gui)

            # Pan SpinBox
            pan_spinbox = QSpinBox(); pan_spinbox.setRange(-100, 100); pan_spinbox.setValue(0)
            pan_spinbox.setButtonSymbols(QSpinBox.NoButtons); pan_spinbox.setFixedWidth(50)
            pan_spinbox.setObjectName(f"mixer_playback_pan_spinbox_{output_ch_idx}_{pb_ch_idx_0_based}")
            pan_spinbox.setToolTip(f"Playback {i+1} Send Pan (L-100..0..R100)")
            pan_spinbox.valueChanged.connect(self.send_mix_pan_from_gui)

            # Add widgets to grid
            playback_sends_layout.addWidget(vol_slider, ROW_VOL_SLIDER, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(vol_spinbox, ROW_VOL_SPIN, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(pan_spinbox, ROW_PAN_SPIN, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)

            # Store widget references
            self.widgets['mixer_playback_gain'].append(vol_slider)
            self.widgets['mixer_playback_gain_spinbox'].append(vol_spinbox)
            self.widgets['mixer_playback_pan_spinbox'].append(pan_spinbox)

            # Set initial value from cache if available
            cached_val = self.current_mixer_sends[output_ch_idx]['playback'][pb_ch_idx_0_based]
            vol_spinbox.setValue(cached_val['volume'])
            pan_spinbox.setValue(cached_val['pan'])

        # Add stretch to push groups to the left within the horizontal layout
        self.mixer_strips_layout.addStretch()


    def connect_signals(self):
        """Connects Qt signals from widgets and OSC handler to corresponding slots."""
        # OSC Handler -> GUI Updates
        self.osc_handler.connected.connect(self.on_osc_connected)
        self.osc_handler.disconnected.connect(self.on_osc_disconnected)
        self.osc_handler.message_received_signal.connect(self.log_osc_message)
        self.osc_handler.meter_received.connect(self.on_meter_received)
        self.osc_handler.gain_received.connect(self.on_gain_received)
        self.osc_handler.link_received.connect(self.on_link_received)
        self.osc_handler.mute_received.connect(self.on_bool_received)
        self.osc_handler.phase_received.connect(self.on_bool_received)
        self.osc_handler.phantom_received.connect(self.on_bool_received)
        self.osc_handler.hiz_received.connect(self.on_bool_received)
        self.osc_handler.eq_enable_received.connect(self.on_bool_received)
        self.osc_handler.dyn_enable_received.connect(self.on_bool_received)
        self.osc_handler.lc_enable_received.connect(self.on_bool_received)
        self.osc_handler.mix_send_received.connect(self.on_mix_send_received)
        self.osc_handler.mix_pan_received.connect(self.on_mix_pan_received)

        # GUI -> OSC Sending
        self.connect_button.clicked.connect(self.connect_osc)
        self.disconnect_button.clicked.connect(self.disconnect_osc)
        self.refresh_button.clicked.connect(self.send_refresh_command)

        # Connect controls for inputs and outputs dynamically based on widget lists
        for type_key in ["input", "output"]:
            num_ch = NUM_INPUT_CHANNELS if type_key == "input" else NUM_OUTPUT_CHANNELS
            for i in range(num_ch):
                # Connect gain spinbox (slider is linked to it) if it exists and is enabled
                if i < len(self.widgets[f'{type_key}_gain_spinbox']) and self.widgets[f'{type_key}_gain_spinbox'][i] is not None and self.widgets[f'{type_key}_gain_spinbox'][i].isEnabled():
                    self.widgets[f'{type_key}_gain_spinbox'][i].valueChanged.connect(self.send_gain_from_gui)

                # Connect boolean checkboxes if they exist
                for control_key in ["mute", "phase", "phantom", "hiz", "eq_enable", "dyn_enable", "lc_enable", "link"]:
                    widget_list = self.widgets.get(f'{type_key}_{control_key}')
                    if widget_list and i < len(widget_list) and widget_list[i] is not None:
                        # Link checkboxes are only connected for odd indices
                        if control_key == "link" and (i % 2) == 0:
                            widget_list[i].stateChanged.connect(self.send_link_from_gui)
                        elif control_key != "link": # Connect all other bools
                            widget_list[i].stateChanged.connect(self.send_bool_from_gui)

        # Note: Mixer controls are connected dynamically in _update_mixer_view when they are created.

    # --- Slots for Handling GUI Actions and OSC Events ---

    @Slot(int)
    def _on_mixer_output_selected(self, index):
        """Slot called when the user selects a different output channel in the Mixer tab dropdown."""
        selected_output_idx = self.mixer_output_selector.itemData(index)
        if selected_output_idx is not None:
            self._update_mixer_view(selected_output_idx)

    @Slot()
    def connect_osc(self):
        """Slot called when the Connect button is clicked."""
        ip = self.ip_input.text().strip()
        try:
            send_port = int(self.port_input.text())
            listen_port = int(self.listen_port_input.text())
            if not (0 < send_port < 65536 and 0 < listen_port < 65536):
                 raise ValueError("Port numbers must be between 1 and 65535")
        except ValueError as e:
            QMessageBox.warning(self, "Input Error", f"Invalid port number: {e}")
            return

        self.status_bar.showMessage(f"Attempting connection to oscmix at {ip}:{send_port}...")
        self.log_osc_message("[GUI]", [f"Connecting to oscmix @ {ip}:{send_port}...", f"Listening on {self.osc_handler.listen_ip}:{listen_port}"])
        self.connection_status_label.setText("CONNECTING...")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: orange;")
        self.osc_handler.listen_port = listen_port # Update listen port from GUI

        if not self.osc_handler.start(ip, send_port):
             # Error message is usually handled by the disconnected signal, but set status here too
             self.status_bar.showMessage("Connection Failed (Check Console/Log)")
             self.connection_status_label.setText("FAILED")
             self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")

    @Slot()
    def disconnect_osc(self):
        """Slot called when the Disconnect button is clicked."""
        self.status_bar.showMessage("Disconnecting...")
        self.log_osc_message("[GUI]", ["Disconnecting OSC Handler."])
        self.osc_handler.stop() # Triggers on_osc_disconnected via signal

    @Slot()
    def send_refresh_command(self):
        """Sends the /refresh OSC command to oscmix."""
        print("[GUI] Sending /refresh command...")
        self.log_osc_message("[GUI]", ["Sending /refresh command."])
        self.osc_handler.send(OSC_REFRESH, []) # Send with empty args list

    @Slot()
    def on_osc_connected(self):
        """Slot called when the OSC handler successfully connects."""
        status_msg = f"Connected to oscmix ({self.osc_handler.target_ip}:{self.osc_handler.target_port}) | GUI Listening on {self.osc_handler.listen_port}"
        self.status_bar.showMessage(status_msg)
        self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("CONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: green;")
        # Update UI element states
        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)
        self.refresh_button.setEnabled(True)
        self.ip_input.setEnabled(False)
        self.port_input.setEnabled(False)
        self.listen_port_input.setEnabled(False)
        # Start the meter update timer
        self.meter_timer.start()
        # Request full state refresh from oscmix on successful connect
        self.send_refresh_command()

    @Slot(str)
    def on_osc_disconnected(self, reason):
        """Slot called when the OSC handler disconnects."""
        status_msg = f"Disconnected: {reason}"
        self.status_bar.showMessage(status_msg)
        self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("DISCONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        # Update UI element states
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.refresh_button.setEnabled(False)
        self.ip_input.setEnabled(True)
        self.port_input.setEnabled(True)
        self.listen_port_input.setEnabled(True)
        # Stop the meter timer
        self.meter_timer.stop()
        # Reset all meters visually
        for meter_type in ['input_meter', 'output_meter', 'playback_meter']:
            for meter in self.widgets.get(meter_type, []):
                 if meter: meter.setValue(0)
        # Clear cached data
        self.last_meter_values['input'].clear()
        self.last_meter_values['output'].clear()
        self.last_meter_values['playback'].clear()
        self.current_mixer_sends.clear()
        # Optionally reset sliders/checkboxes to default state here

    @Slot(str, list)
    def log_osc_message(self, address, args):
        """Appends details of incoming OSC messages to the log view."""
        # Trim log to prevent excessive memory use
        if self.osc_log.document().blockCount() > 200:
             cursor = self.osc_log.textCursor()
             cursor.movePosition(cursor.Start)
             cursor.select(cursor.BlockUnderCursor)
             cursor.removeSelectedText()
             cursor.deleteChar() # Remove trailing newline
        # Append new message and scroll to bottom
        self.osc_log.append(f"{address} {args}")
        self.osc_log.verticalScrollBar().setValue(self.osc_log.verticalScrollBar().maximum())

    @Slot(str, int, float)
    def on_meter_received(self, channel_type, ch_idx, value):
        """Stores received meter value (handles input/output/playback)."""
        if channel_type in self.last_meter_values:
            self.last_meter_values[channel_type][ch_idx] = value
        else:
            print(f"[Warning] Received meter for unknown type: {channel_type}")

    @Slot()
    def update_gui_meters(self):
        """Periodically updates meter progress bars based on stored values."""
        for type_key, meter_values in self.last_meter_values.items():
            widget_key = f"{type_key}_meter"
            meter_widget_list = self.widgets.get(widget_key)
            if meter_widget_list:
                for ch_idx, value in meter_values.items():
                    # Ensure index is valid for the widget list
                    if 0 <= ch_idx < len(meter_widget_list):
                        # Update the corresponding progress bar
                        meter_widget_list[ch_idx].setValue(float_to_slider_linear(value))
            # Clear stored values for this type to implement simple decay
            meter_values.clear()

    @Slot(str, int, float)
    def on_gain_received(self, channel_type, ch_idx, value_db):
        """Updates gain/volume slider AND spinbox when a message is received."""
        gain_widget_key = f"{channel_type}_gain"
        spinbox_widget_key = f"{channel_type}_gain_spinbox"
        slider_list = self.widgets.get(gain_widget_key)
        spinbox_list = self.widgets.get(spinbox_widget_key)

        # Check if widgets exist for this channel index
        if slider_list and spinbox_list and 0 <= ch_idx < len(slider_list):
            slider = slider_list[ch_idx]
            spinbox = spinbox_list[ch_idx]
            if slider is None or spinbox is None: return # Skip if controls don't exist (e.g., digital input)

            # Block signals temporarily to prevent feedback loops
            slider.blockSignals(True)
            spinbox.blockSignals(True)

            # Apply the correct scaling based on channel type
            if channel_type == 'input':
                slider.setValue(input_gain_to_slider(value_db))
                spinbox.setValue(value_db)
            elif channel_type == 'output':
                slider.setValue(output_volume_to_slider(value_db))
                spinbox.setValue(value_db)
            # Add handling for 'playback' gain if implemented later

            # Re-enable signals
            slider.blockSignals(False)
            spinbox.blockSignals(False)

    @Slot(str, int, bool)
    def on_link_received(self, channel_type, ch_idx, value):
        """Updates link checkbox when a message is received from oscmix."""
        widget_key = f"{channel_type}_link"
        cb_list = self.widgets.get(widget_key)
        # Ensure index is valid and widget exists (link only on odd channels)
        if cb_list and 0 <= ch_idx < len(cb_list) and (ch_idx % 2 == 0) and cb_list[ch_idx] is not None:
            cb = cb_list[ch_idx]
            cb.blockSignals(True)
            cb.setChecked(value)
            cb.blockSignals(False)
            # Update the linked channel's state as well
            self._update_linked_channel_state(channel_type, ch_idx, value)

    @Slot(str, int, bool)
    def on_bool_received(self, channel_type, ch_idx, value):
        """Handles updates for Mute, Phase, 48V, HiZ, EQ, Dyn, LC checkboxes."""
        # Determine the specific control type (mute, phase, etc.) from the signal type
        base_type, control_type = channel_type.split('_', 1) # e.g., "input_mute", "input_phantom"
        widget_key = f"{base_type}_{control_type}" # e.g., "input_mute", "input_phantom"
        cb_list = self.widgets.get(widget_key)

        # Ensure index is valid and widget exists
        if cb_list and 0 <= ch_idx < len(cb_list) and cb_list[ch_idx] is not None:
            cb = cb_list[ch_idx]
            cb.blockSignals(True)
            cb.setChecked(value)
            cb.blockSignals(False)

    @Slot(int, str, int, float)
    def on_mix_send_received(self, out_idx, in_type, in_idx, value_db):
        """Updates a mixer send volume slider/spinbox based on incoming OSC."""
        # Cache the received value
        self.current_mixer_sends[out_idx][in_type][in_idx]['volume'] = value_db
        # Update GUI only if this output mix is currently displayed
        if out_idx == self.current_mixer_output_idx:
            widget_base = f"mixer_{in_type}_gain"
            slider_list = self.widgets.get(widget_base)
            spinbox_list = self.widgets.get(f"{widget_base}_spinbox")
            # Check if widgets exist for this input index
            if slider_list and spinbox_list and 0 <= in_idx < len(slider_list):
                slider = slider_list[in_idx]; spinbox = spinbox_list[in_idx]
                slider.blockSignals(True); spinbox.blockSignals(True)
                slider.setValue(mix_send_volume_to_slider(value_db))
                spinbox.setValue(value_db)
                slider.blockSignals(False); spinbox.blockSignals(False)

    @Slot(int, str, int, int)
    def on_mix_pan_received(self, out_idx, in_type, in_idx, pan_value):
        """Updates a mixer send pan spinbox based on incoming OSC."""
        # Cache the received value
        self.current_mixer_sends[out_idx][in_type][in_idx]['pan'] = pan_value
        # Update GUI only if this output mix is currently displayed
        if out_idx == self.current_mixer_output_idx:
            widget_base = f"mixer_{in_type}_pan_spinbox"
            spinbox_list = self.widgets.get(widget_base)
            # Check if widget exists for this input index
            if spinbox_list and 0 <= in_idx < len(spinbox_list):
                spinbox = spinbox_list[in_idx]
                spinbox.blockSignals(True)
                spinbox.setValue(pan_to_spinbox(pan_value))
                spinbox.blockSignals(False)

    @Slot(float) # Connected to main gain/volume QDoubleSpinBox
    def send_gain_from_gui(self, value_db):
        """Sends OSC gain/volume message when a main channel spinbox value changes."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_gain_spinbox_0"
        try:
            # Parse type ('input'/'output'), control name, "spinbox", and index
            type, gain_or_vol, _, idx_str = obj_name.split('_')
            ch_idx = int(idx_str)
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}"); return

        ch_num_1_based = ch_idx + 1
        float_value_db = value_db # Value from spinbox is already the correct float dB

        # Determine OSC address based on type
        if type == 'input': osc_address = OSC_INPUT_GAIN.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_VOLUME.format(ch=ch_num_1_based)
        else: return # Should not happen

        # Send the OSC message
        self.osc_handler.send(osc_address, float_value_db)

    @Slot(int) # Connected to main link QCheckBox
    def send_link_from_gui(self, state):
        """Sends OSC link message when a main channel link checkbox state changes."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_link_0"
        try:
            type, _, idx_str = obj_name.split('_') # Type is 'input' or 'output'
            ch_idx = int(idx_str) # 0-based odd index (0, 2, ...)
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}"); return

        bool_value = (state == Qt.Checked)
        osc_value = 1 if bool_value else 0 # Send integer 0 or 1
        ch_num_1_based = ch_idx + 1 # 1-based odd channel number for OSC path

        # Determine OSC address based on type
        if type == 'input': osc_address = OSC_INPUT_STEREO.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_STEREO.format(ch=ch_num_1_based)
        else: return

        # Send OSC message
        self.osc_handler.send(osc_address, osc_value)
        # Update the linked channel's GUI state immediately
        self._update_linked_channel_state(type, ch_idx, bool_value)

    @Slot(int) # Connected to main boolean QCheckBoxes (Mute, Phase, 48V, HiZ, EQ, Dyn, LC)
    def send_bool_from_gui(self, state):
        """Sends OSC message for Mute, Phase, 48V, HiZ, EQ, Dyn, LC checkboxes."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_mute_0", "input_phantom_1"
        try:
            parts = obj_name.split('_')
            type = parts[0] # 'input' or 'output'
            # Handle multi-word control names like "eq_enable"
            control_name = "_".join(parts[1:-1])
            ch_idx = int(parts[-1]) # 0-based index
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}"); return

        bool_value = (state == Qt.Checked)
        osc_value = 1 if bool_value else 0 # Send integer 0 or 1
        ch_num_1_based = ch_idx + 1 # 1-based channel index for OSC path

        # Map control name to OSC path constants
        addr_map = {
            "mute": (OSC_INPUT_MUTE, OSC_OUTPUT_MUTE),
            "phase": (OSC_INPUT_PHASE, OSC_OUTPUT_PHASE),
            "phantom": (OSC_INPUT_48V, None), # 48V only exists for input
            "hiz": (OSC_INPUT_HIZ, None),     # HiZ only exists for input
            "eq_enable": (OSC_INPUT_EQ_ENABLE, OSC_OUTPUT_EQ_ENABLE),
            "dyn_enable": (OSC_INPUT_DYN_ENABLE, OSC_OUTPUT_DYN_ENABLE),
            "lc_enable": (OSC_INPUT_LC_ENABLE, OSC_OUTPUT_LC_ENABLE),
        }
        paths = addr_map.get(control_name)

        if not paths:
            print(f"Unknown boolean control name derived from widget: {control_name}"); return

        # Select the correct path based on channel type (input/output)
        osc_address_fmt = paths[0] if type == "input" else paths[1]

        # Check if the control is applicable for this channel type
        if not osc_address_fmt:
            print(f"Control '{control_name}' not applicable for channel type '{type}'"); return

        # Format the final OSC address and send
        osc_address = osc_address_fmt.format(ch=ch_num_1_based)
        self.osc_handler.send(osc_address, osc_value)

    @Slot(float) # Connected to Mixer Volume QDoubleSpinBox
    def send_mix_gain_from_gui(self, value_db):
        """Sends OSC message for a mixer send volume change."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "mixer_input_gain_spinbox_0_3"
        try:
            # Parse out_idx, in_type, in_idx from the object name
            _, in_type, gain_str, _, out_idx_str, in_idx_str = obj_name.split('_')
            out_idx = int(out_idx_str)
            in_idx = int(in_idx_str)
        except Exception as e:
            print(f"Error parsing mixer sender object name '{obj_name}': {e}"); return

        # Get current pan value for this send from cache or default to 0
        current_pan = self.current_mixer_sends[out_idx][in_type][in_idx].get('pan', 0)

        # Construct address and values for the setmix command
        osc_address = OSC_MIX_SEND_SET.format(out_ch=out_idx + 1, in_type=in_type, in_ch=in_idx + 1)
        # oscmix setmix expects volume (dB float), pan (int), [width (float)]
        # Send volume and pan. Width might default or need separate control.
        values_to_send = [float(value_db), int(current_pan)]
        self.osc_handler.send_multi(osc_address, values_to_send)

        # Update local cache immediately
        self.current_mixer_sends[out_idx][in_type][in_idx]['volume'] = value_db

    @Slot(int) # Connected to Mixer Pan QSpinBox
    def send_mix_pan_from_gui(self, pan_value):
        """Sends OSC message for a mixer send pan change."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "mixer_input_pan_spinbox_0_3"
        try:
            # Parse out_idx, in_type, in_idx from the object name
            _, in_type, pan_str, _, out_idx_str, in_idx_str = obj_name.split('_')
            out_idx = int(out_idx_str)
            in_idx = int(in_idx_str)
        except Exception as e:
            print(f"Error parsing mixer sender object name '{obj_name}': {e}"); return

        # Get current volume value for this send from cache or default to -65.0
        current_volume = self.current_mixer_sends[out_idx][in_type][in_idx].get('volume', -65.0)

        # Construct address and values for the setmix command
        osc_address = OSC_MIX_SEND_SET.format(out_ch=out_idx + 1, in_type=in_type, in_ch=in_idx + 1)
        # oscmix setmix expects volume (dB float), pan (int), [width (float)]
        values_to_send = [float(current_volume), int(pan_value)]
        self.osc_handler.send_multi(osc_address, values_to_send)

        # Update local cache immediately
        self.current_mixer_sends[out_idx][in_type][in_idx]['pan'] = pan_value


    def _update_linked_channel_state(self, channel_type, odd_ch_idx, is_linked):
         """
         Enables or disables controls of the paired (even) channel based on link state.
         Handles disabling gain/volume sliders/spinboxes and boolean checkboxes.
         """
         even_ch_idx = odd_ch_idx + 1

         # --- Disable Gain/Volume ---
         gain_list = self.widgets.get(f"{channel_type}_gain")
         spinbox_list = self.widgets.get(f"{channel_type}_gain_spinbox")
         # Check if the even channel index is valid and widgets exist
         if gain_list and 0 <= even_ch_idx < len(gain_list) and gain_list[even_ch_idx] is not None:
             gain_list[even_ch_idx].setEnabled(not is_linked)
         if spinbox_list and 0 <= even_ch_idx < len(spinbox_list) and spinbox_list[even_ch_idx] is not None:
             spinbox_list[even_ch_idx].setEnabled(not is_linked)

         # --- Disable Boolean Controls ---
         # List of control types (excluding 'link' itself)
         bool_controls = ["mute", "phase", "phantom", "hiz", "eq_enable", "dyn_enable", "lc_enable"]
         for control in bool_controls:
             widget_key = f"{channel_type}_{control}"
             cb_list = self.widgets.get(widget_key)
             # Check if list exists, index is valid, and widget exists before disabling
             if cb_list and 0 <= even_ch_idx < len(cb_list) and cb_list[even_ch_idx] is not None:
                 cb_list[even_ch_idx].setEnabled(not is_linked)

    def closeEvent(self, event):
        """Ensures the OSC handler is stopped cleanly when the GUI window is closed."""
        print("[GUI] Closing application...")
        self.meter_timer.stop() # Stop the meter update timer
        self.osc_handler.stop() # Stop the OSC handler thread
        event.accept() # Allow the window to close


# --- Main execution function ---
def run():
    """Creates the Qt application instance and runs the main GUI window."""
    # Create the Qt Application instance
    # QApplication expects sys.argv for command line arguments (like style options)
    app = QApplication(sys.argv)

    # Create and show the main window
    window = OscmixControlGUI()
    window.show()

    # Start the Qt event loop. Execution stays here until the application quits.
    # sys.exit() ensures the application's exit code is returned to the OS.
    sys.exit(app.exec())

# --- Entry point for direct execution ---
# This allows running the script directly (python main.py)
# It's also called by the 'oscmix-gui' command created by setup.py
if __name__ == '__main__':
    run()
