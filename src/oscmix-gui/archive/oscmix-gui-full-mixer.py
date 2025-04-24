# oscmix_gui/main.py
# Updated based on oscmix C source, packaging, Tabs, Web GUI elements, and Mixer Tab.
import sys
import time
import threading
import queue
from collections import defaultdict
import functools # For partial function application in connect

# Attempt to import PySide6, provide helpful error if missing
try:
    from PySide6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QGridLayout, QLabel, QLineEdit, QPushButton, QSlider, QCheckBox,
        QProgressBar, QGroupBox, QStatusBar, QMessageBox, QTextEdit,
        QTabWidget, QDoubleSpinBox, QSpinBox, QComboBox, QScrollArea # Added QComboBox, QSpinBox, QScrollArea
    )
    from PySide6.QtCore import Qt, QThread, Signal, Slot, QObject, QTimer, QMetaObject, Q_ARG
    from PySide6.QtGui import QColor, QPalette, QFont # For status indication and small fonts
except ImportError:
    print("ERROR: 'PySide6' library not found.")
    print("PySide6 is required for the GUI.")
    print("Please install it, preferably within a virtual environment:")
    print("  pip install PySide6")
    sys.exit(1)

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

# --- Configuration ---
# --- OSC Address Patterns based on oscmix.c analysis ---
# !! VERIFY these paths against your specific oscmix implementation !!
OSC_INPUT_GAIN = "/input/{ch}/gain"         # float 0.0 to 75.0
OSC_INPUT_STEREO = "/input/{ch}/stereo"     # int 0/1 (odd ch)
OSC_INPUT_LEVEL = "/input/{ch}/level"       # multiple floats + int
OSC_INPUT_MUTE = "/input/{ch}/mute"         # int 0/1 (Assumed path)
OSC_INPUT_SOLO = "/input/{ch}/solo"         # int 0/1 (Assumed path - check if oscmix supports solo)
OSC_INPUT_PHASE = "/input/{ch}/phase"       # int 0/1 (Assumed path)
OSC_INPUT_EQ_ENABLE = "/input/{ch}/eq"      # int 0/1 (Assumed path)
OSC_INPUT_DYN_ENABLE = "/input/{ch}/dynamics" # int 0/1 (Assumed path)
OSC_INPUT_LC_ENABLE = "/input/{ch}/lowcut"    # int 0/1 (Assumed path)

OSC_OUTPUT_VOLUME = "/output/{ch}/volume"    # float -65.0 to +6.0 dB
OSC_OUTPUT_STEREO = "/output/{ch}/stereo"    # int 0/1 (odd ch)
OSC_OUTPUT_LEVEL = "/output/{ch}/level"      # multiple floats + int
OSC_OUTPUT_MUTE = "/output/{ch}/mute"        # int 0/1 (Assumed path)
OSC_OUTPUT_SOLO = "/output/{ch}/solo"        # int 0/1 (Assumed path - check if oscmix supports solo)
OSC_OUTPUT_PHASE = "/output/{ch}/phase"      # int 0/1 (Assumed path)
OSC_OUTPUT_EQ_ENABLE = "/output/{ch}/eq"     # int 0/1 (Assumed path)
OSC_OUTPUT_DYN_ENABLE = "/output/{ch}/dynamics"# int 0/1 (Assumed path)
OSC_OUTPUT_LC_ENABLE = "/output/{ch}/lowcut"   # int 0/1 (Assumed path)

# Playback channels - Assuming only level meters are sent by oscmix under this path.
OSC_PLAYBACK_LEVEL = "/playback/{ch}/level" # Placeholder - Verify if oscmix sends this

# Mixer Matrix Sends (Based on oscmix.c analysis)
# oscmix seems to send volume and pan separately when receiving hardware updates
OSC_MIX_SEND_VOLUME = "/mix/{out_ch}/{in_type}/{in_ch}" # Expected incoming path for volume (dB float)
OSC_MIX_SEND_PAN = "/mix/{out_ch}/{in_type}/{in_ch}/pan" # Expected incoming path for pan (int -100 to 100)
# For sending, oscmix's setmix function seems to take volume, pan, width in one message
OSC_MIX_SEND_SET = "/mix/{out_ch}/{in_type}/{in_ch}" # Path to send volume + pan + width

# --- Connection Defaults ---
DEFAULT_OSCMIX_IP = "127.0.0.1"
DEFAULT_OSCMIX_SEND_PORT = 8000
DEFAULT_LISTEN_IP = "0.0.0.0"
DEFAULT_LISTEN_PORT = 9001

# --- Channel Counts ---
NUM_INPUT_CHANNELS = 8
NUM_OUTPUT_CHANNELS = 8
NUM_PLAYBACK_CHANNELS = 8

# GUI Update Rates
METER_GUI_UPDATE_RATE_HZ = 25
METER_GUI_UPDATE_INTERVAL_MS = int(1000 / METER_GUI_UPDATE_RATE_HZ)

# --- Helper Functions ---
def float_to_slider_linear(val):
    try: return int(max(0.0, min(1.0, float(val))) * 100)
    except (ValueError, TypeError): return 0
def input_gain_to_slider(db_value):
    try: return int(max(0.0, min(75.0, float(db_value))) * 10.0)
    except (ValueError, TypeError): return 0
def slider_to_input_gain(slider_value): return float(slider_value) / 10.0
# Use output volume scaling for mix sends as well (assuming dB)
def mix_send_volume_to_slider(db_value):
    return output_volume_to_slider(db_value)
def slider_to_mix_send_volume(slider_value):
    return slider_to_output_volume(slider_value)
# Pan uses -100 to 100 directly for SpinBox
def pan_to_spinbox(pan_val):
    try: return int(max(-100, min(100, int(pan_val))))
    except (ValueError, TypeError): return 0
def spinbox_to_pan(spinbox_val):
    return int(spinbox_val) # Already in correct range

#=============================================================================
# OSC Communication Handler Class
#=============================================================================
class OSCHandler(QObject):
    """Handles OSC sending/receiving for oscmix."""
    connected = Signal()
    disconnected = Signal(str)
    message_received_signal = Signal(str, list)
    meter_received = Signal(str, int, float) # type, ch_idx, value
    gain_received = Signal(str, int, float)  # type ('input'/'output'), ch_idx, value (dB)
    link_received = Signal(str, int, bool)   # type ('input'/'output'), ch_idx (odd), value
    mute_received = Signal(str, int, bool)   # type, ch_idx, value
    solo_received = Signal(str, int, bool)   # type, ch_idx, value
    phase_received = Signal(str, int, bool)  # type, ch_idx, value
    eq_enable_received = Signal(str, int, bool) # type, ch_idx, value
    dyn_enable_received = Signal(str, int, bool)# type, ch_idx, value
    lc_enable_received = Signal(str, int, bool) # type, ch_idx, value
    # Signals for Mixer Tab
    mix_send_received = Signal(int, str, int, float) # out_idx, in_type, in_idx, value_db
    mix_pan_received = Signal(int, str, int, int)    # out_idx, in_type, in_idx, pan_value

    def __init__(self, listen_ip, listen_port, parent=None):
        super().__init__(parent)
        self.listen_ip = listen_ip
        self.listen_port = listen_port
        self.target_ip = ""; self.target_port = 0
        self.client = None; self.server = None; self.server_thread = None
        self.dispatcher = Dispatcher()
        self._stop_event = threading.Event(); self._is_connected = False
        print("--- OSC Address Mapping (Verify paths!) ---")
        self._map_osc_addresses()
        print("-------------------------------------------")
        self.dispatcher.set_default_handler(self._default_handler)

    def _map_osc_addresses(self):
        """Sets up the OSC address mappings in the dispatcher."""
        # Print mappings (unchanged)
        print(f"Mapping Inputs : Gain={OSC_INPUT_GAIN.format(ch='<ch>')}, Stereo={OSC_INPUT_STEREO.format(ch='<ch>')}, Level={OSC_INPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_INPUT_MUTE.format(ch='<ch>')}, Solo={OSC_INPUT_SOLO.format(ch='<ch>')}, Phase={OSC_INPUT_PHASE.format(ch='<ch>')}, EQ={OSC_INPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_INPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_INPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Outputs: Vol={OSC_OUTPUT_VOLUME.format(ch='<ch>')}, Stereo={OSC_OUTPUT_STEREO.format(ch='<ch>')}, Level={OSC_OUTPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_OUTPUT_MUTE.format(ch='<ch>')}, Solo={OSC_OUTPUT_SOLO.format(ch='<ch>')}, Phase={OSC_OUTPUT_PHASE.format(ch='<ch>')}, EQ={OSC_OUTPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_OUTPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_OUTPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Playback: Level={OSC_PLAYBACK_LEVEL.format(ch='<ch>')} (Meters only)")
        print(f"Mapping Mix Send Volume: {OSC_MIX_SEND_VOLUME.format(out_ch='<out>', in_type='<type>', in_ch='<in>')}")
        print(f"Mapping Mix Send Pan   : {OSC_MIX_SEND_PAN.format(out_ch='<out>', in_type='<type>', in_ch='<in>')}")


        # Map input/output/playback channels (as before)
        for type_key, num_ch, gain_path, stereo_path, level_path, mute_path, solo_path, phase_path, eq_path, dyn_path, lc_path in [
            ("input", NUM_INPUT_CHANNELS, OSC_INPUT_GAIN, OSC_INPUT_STEREO, OSC_INPUT_LEVEL, OSC_INPUT_MUTE, OSC_INPUT_SOLO, OSC_INPUT_PHASE, OSC_INPUT_EQ_ENABLE, OSC_INPUT_DYN_ENABLE, OSC_INPUT_LC_ENABLE),
            ("output", NUM_OUTPUT_CHANNELS, OSC_OUTPUT_VOLUME, OSC_OUTPUT_STEREO, OSC_OUTPUT_LEVEL, OSC_OUTPUT_MUTE, OSC_OUTPUT_SOLO, OSC_OUTPUT_PHASE, OSC_OUTPUT_EQ_ENABLE, OSC_OUTPUT_DYN_ENABLE, OSC_OUTPUT_LC_ENABLE),
            ("playback", NUM_PLAYBACK_CHANNELS, None, None, OSC_PLAYBACK_LEVEL, None, None, None, None, None, None) # Only level for playback initially
        ]:
            for i in range(1, num_ch + 1):
                ch_idx_0_based = i - 1
                if gain_path: self.dispatcher.map(gain_path.format(ch=i), self._handle_gain, type_key, ch_idx_0_based)
                if level_path: self.dispatcher.map(level_path.format(ch=i), self._handle_meter, type_key, ch_idx_0_based)
                if mute_path: self.dispatcher.map(mute_path.format(ch=i), self._handle_bool, f"{type_key}_mute", ch_idx_0_based)
                if solo_path: self.dispatcher.map(solo_path.format(ch=i), self._handle_bool, f"{type_key}_solo", ch_idx_0_based)
                if phase_path: self.dispatcher.map(phase_path.format(ch=i), self._handle_bool, f"{type_key}_phase", ch_idx_0_based)
                if eq_path: self.dispatcher.map(eq_path.format(ch=i), self._handle_bool, f"{type_key}_eq_enable", ch_idx_0_based)
                if dyn_path: self.dispatcher.map(dyn_path.format(ch=i), self._handle_bool, f"{type_key}_dyn_enable", ch_idx_0_based)
                if lc_path: self.dispatcher.map(lc_path.format(ch=i), self._handle_bool, f"{type_key}_lc_enable", ch_idx_0_based)
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
                    self.dispatcher.map(vol_path, self._handle_mix_update, out_idx_0_based, in_type, in_idx_0_based, "volume")
                    # Map pan path (e.g., /mix/1/input/5/pan)
                    pan_path = OSC_MIX_SEND_PAN.format(out_ch=out_ch, in_type=in_type, in_ch=in_ch)
                    self.dispatcher.map(pan_path, self._handle_mix_update, out_idx_0_based, in_type, in_idx_0_based, "pan")


    # --- Handlers for specific OSC messages ---
    def _handle_meter(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (float, int)): QMetaObject.invokeMethod(self, "emit_meter_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_gain(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (float, int)): QMetaObject.invokeMethod(self, "emit_gain_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_link(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (int, float, bool)): QMetaObject.invokeMethod(self, "emit_link_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(bool, bool(value)))

    def _handle_bool(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
            signal_type, ch_idx, value = args[0], args[1], args[2]
            if isinstance(value, (int, float, bool)):
                bool_value = bool(value)
                base_type, control_type = signal_type.split('_', 1)
                # Emit specific signal based on control_type
                if control_type == "mute": signal_emitter = self.emit_mute_received
                elif control_type == "solo": signal_emitter = self.emit_solo_received
                elif control_type == "phase": signal_emitter = self.emit_phase_received
                elif control_type == "eq_enable": signal_emitter = self.emit_eq_enable_received
                elif control_type == "dyn_enable": signal_emitter = self.emit_dyn_enable_received
                elif control_type == "lc_enable": signal_emitter = self.emit_lc_enable_received
                else: return # Unknown boolean type
                QMetaObject.invokeMethod(self, signal_emitter.__name__, Qt.QueuedConnection, Q_ARG(str, base_type), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))

    def _handle_mix_update(self, address, *args):
        """Handles incoming mix send volume or pan updates."""
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 5: # Need out_idx, in_type, in_idx, update_type, value
            out_idx, in_type, in_idx, update_type, value = args[0], args[1], args[2], args[3], args[4]
            if update_type == "volume":
                if isinstance(value, (float, int)):
                    QMetaObject.invokeMethod(self, "emit_mix_send_received", Qt.QueuedConnection, Q_ARG(int, out_idx), Q_ARG(str, in_type), Q_ARG(int, in_idx), Q_ARG(float, float(value)))
            elif update_type == "pan":
                 if isinstance(value, (int, float)): # Pan is likely int
                     QMetaObject.invokeMethod(self, "emit_mix_pan_received", Qt.QueuedConnection, Q_ARG(int, out_idx), Q_ARG(str, in_type), Q_ARG(int, in_idx), Q_ARG(int, int(value)))


    # --- Slots for Safe Signal Emission ---
    @Slot(str, int, float)
    def emit_meter_received(self, type, ch_idx, val): self.meter_received.emit(type, ch_idx, val)
    @Slot(str, int, float)
    def emit_gain_received(self, type, ch_idx, val): self.gain_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_link_received(self, type, ch_idx, val): self.link_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_mute_received(self, type, ch_idx, val): self.mute_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_solo_received(self, type, ch_idx, val): self.solo_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_phase_received(self, type, ch_idx, val): self.phase_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_eq_enable_received(self, type, ch_idx, val): self.eq_enable_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_dyn_enable_received(self, type, ch_idx, val): self.dyn_enable_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_lc_enable_received(self, type, ch_idx, val): self.lc_enable_received.emit(type, ch_idx, val)
    # Mixer slots
    @Slot(int, str, int, float)
    def emit_mix_send_received(self, out_idx, in_type, in_idx, val): self.mix_send_received.emit(out_idx, in_type, in_idx, val)
    @Slot(int, str, int, int)
    def emit_mix_pan_received(self, out_idx, in_type, in_idx, val): self.mix_pan_received.emit(out_idx, in_type, in_idx, val)


    def _default_handler(self, address, *args):
        self.message_received_signal.emit(address, list(args))

    # --- Server Thread Logic (Unchanged) ---
    def _server_thread_target(self):
        self._is_connected = False; server_error = None
        try:
            self.server = BlockingOSCUDPServer((self.listen_ip, self.listen_port), self.dispatcher)
            print(f"[OSC Handler] Server listening on {self.server.server_address}")
            self._is_connected = True; QMetaObject.invokeMethod(self, "connected", Qt.QueuedConnection)
            while not self._stop_event.is_set(): self.server.handle_request_timeout(0.1)
        except OSError as e: server_error = f"Error starting server (Port {self.listen_port} in use?): {e}"; print(f"!!! [OSC Handler] {server_error}")
        except Exception as e: server_error = f"Server error: {e}"; print(f"!!! [OSC Handler] {server_error}")
        finally:
            self._is_connected = False; disconnect_reason = server_error if server_error else "Disconnected normally"
            if self.server:
                try: self.server.server_close()
                except Exception as e_close: print(f"[OSC Handler] Error closing server socket: {e_close}")
            print("[OSC Handler] Server stopped.")
            QMetaObject.invokeMethod(self, "emit_disconnected", Qt.QueuedConnection, Q_ARG(str, disconnect_reason))

    @Slot(str)
    def emit_disconnected(self, message): self.disconnected.emit(message)

    # --- Public Control Methods (Unchanged) ---
    def start(self, target_ip, target_port):
        if self.server_thread and self.server_thread.is_alive(): return False
        self.target_ip = target_ip; self.target_port = target_port
        try:
            if not (0 < target_port < 65536): raise ValueError("Invalid target port number")
            self.client = SimpleUDPClient(self.target_ip, self.target_port)
            print(f"[OSC Handler] Client configured for target {self.target_ip}:{self.target_port}")
        except Exception as e: self.emit_disconnected(f"Client config error: {e}"); return False
        self._stop_event.clear()
        self.server_thread = threading.Thread(target=self._server_thread_target, daemon=True)
        self.server_thread.start(); return True

    def stop(self):
        if not self.server_thread or not self.server_thread.is_alive():
            if not self._is_connected: self.emit_disconnected("Stopped manually (was not running)")
            self._is_connected = False; return
        print("[OSC Handler] Stopping..."); self._stop_event.set()
        server_instance = getattr(self, 'server', None)
        if server_instance:
             try: server_instance.shutdown()
             except Exception as e: print(f"[OSC Handler] Error during server shutdown call: {e}")
        self.server_thread.join(timeout=1.0)
        if self.server_thread.is_alive(): print("[OSC Handler] Warning: Server thread did not exit cleanly.")
        self.server_thread = None; self.server = None; self.client = None; self._is_connected = False
        print("[OSC Handler] Stopped.")

    def send(self, address, value):
        """Sends an OSC message with a single argument."""
        if self.client and self._is_connected:
            try: self.client.send_message(address, value)
            except Exception as e: print(f"!!! [OSC Handler] Send Error to {address}: {e}")
        elif not self._is_connected: print(f"[OSC Handler] Not connected, cannot send: {address} {value}")
        else: print(f"[OSC Handler] Client is None, cannot send: {address} {value}")

    def send_multi(self, address, values):
        """Sends an OSC message with multiple arguments."""
        if self.client and self._is_connected:
            try:
                # print(f"[OSC SEND MULTI] {address} {values}") # Debug
                self.client.send_message(address, values)
            except Exception as e:
                print(f"!!! [OSC Handler] Send Multi Error to {address}: {e}")
        elif not self._is_connected:
             print(f"[OSC Handler] Not connected, cannot send multi: {address} {values}")
        else:
             print(f"[OSC Handler] Client is None, cannot send multi: {address} {values}")


#=============================================================================
# Main GUI Window Class
#=============================================================================
class OscmixControlGUI(QMainWindow):
    """Main application window for controlling oscmix via OSC."""
    def __init__(self):
        """Initializes the main GUI window."""
        super().__init__()
        self.setWindowTitle("oscmix GUI Control (RME UCX II)")
        self.setGeometry(100, 100, 1150, 800) # Adjusted size for mixer tab

        self.osc_handler = OSCHandler(DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT)

        self.last_meter_values = {
            'input': defaultdict(float), 'output': defaultdict(float), 'playback': defaultdict(float)
        }
        # Store mixer values: self.current_mixer_sends[out_idx][in_type][in_idx] = {'volume': db, 'pan': p}
        self.current_mixer_sends = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: {'volume': -65.0, 'pan': 0})))
        self.current_mixer_output_idx = 0 # Currently viewed output submix

        self.widgets = defaultdict(list) # Use defaultdict for easier appending

        self.init_ui()
        self.connect_signals()

        self.meter_timer = QTimer(self)
        self.meter_timer.setInterval(METER_GUI_UPDATE_INTERVAL_MS)
        self.meter_timer.timeout.connect(self.update_gui_meters)

        self.status_bar.showMessage("Disconnected. Ensure 'oscmix' is running and configured.")

    def init_ui(self):
        """Creates and arranges all the GUI widgets, using Tabs."""
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)

        # --- Connection Group (Stays at the top) ---
        conn_group = QGroupBox("Connection to 'oscmix' Server")
        conn_layout = QHBoxLayout(); conn_group.setLayout(conn_layout)
        self.ip_input = QLineEdit(DEFAULT_OSCMIX_IP); self.ip_input.setToolTip("IP address where the oscmix server is running (often 127.0.0.1)")
        self.port_input = QLineEdit(str(DEFAULT_OSCMIX_SEND_PORT)); self.port_input.setToolTip("Port number the oscmix server is LISTENING on")
        self.listen_port_input = QLineEdit(str(DEFAULT_LISTEN_PORT)); self.listen_port_input.setToolTip("Port number this GUI should listen on for replies from oscmix")
        self.connect_button = QPushButton("Connect"); self.disconnect_button = QPushButton("Disconnect"); self.disconnect_button.setEnabled(False)
        self.connection_status_label = QLabel("DISCONNECTED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        conn_layout.addWidget(QLabel("'oscmix' Server IP:")); conn_layout.addWidget(self.ip_input); conn_layout.addWidget(QLabel("Server Port:"))
        conn_layout.addWidget(self.port_input); conn_layout.addWidget(QLabel("GUI Listen Port:")); conn_layout.addWidget(self.listen_port_input)
        conn_layout.addWidget(self.connect_button); conn_layout.addWidget(self.disconnect_button); conn_layout.addStretch(); conn_layout.addWidget(self.connection_status_label)
        main_layout.addWidget(conn_group)

        # --- Tab Widget for Channels ---
        self.tab_widget = QTabWidget()
        main_layout.addWidget(self.tab_widget)

        # --- Create Tab Pages ---
        self.input_tab = QWidget(); self.playback_tab = QWidget(); self.output_tab = QWidget(); self.mixer_tab = QWidget()
        self.tab_widget.addTab(self.input_tab, "Inputs")
        self.tab_widget.addTab(self.playback_tab, "Playback")
        self.tab_widget.addTab(self.output_tab, "Outputs")
        self.tab_widget.addTab(self.mixer_tab, "Mixer") # Added Mixer Tab

        # --- Populate Input/Playback/Output Tabs ---
        self._populate_channel_tab(self.input_tab, "Inputs", "Hardware Inputs", NUM_INPUT_CHANNELS, True)
        self._populate_channel_tab(self.playback_tab, "Playback", "Software Playback", NUM_PLAYBACK_CHANNELS, False) # No controls for playback yet
        self._populate_channel_tab(self.output_tab, "Outputs", "Hardware Outputs", NUM_OUTPUT_CHANNELS, True)

        # --- Populate Mixer Tab ---
        self._populate_mixer_tab()

        # --- OSC Log (Stays below tabs) ---
        log_group = QGroupBox("OSC Message Log (Incoming from oscmix)")
        log_layout = QVBoxLayout(); log_group.setLayout(log_layout)
        self.osc_log = QTextEdit(); self.osc_log.setReadOnly(True)
        self.osc_log.setMaximumHeight(100); self.osc_log.setToolTip("Shows raw OSC messages received")
        log_layout.addWidget(self.osc_log); main_layout.addWidget(log_group)

        # --- Status Bar ---
        self.status_bar = QStatusBar(); self.setStatusBar(self.status_bar)

    def _populate_channel_tab(self, tab_widget, type_prefix, group_title, num_channels, add_controls):
        """Helper function to create the content for Input/Output/Playback tabs."""
        tab_layout = QVBoxLayout(tab_widget)
        group_box = QGroupBox(group_title)
        tab_layout.addWidget(group_box)
        grid_layout = QGridLayout()
        group_box.setLayout(grid_layout)

        # Define grid rows
        ROW_METER_LABEL = 0; ROW_METER = 1; ROW_GAIN_LABEL = 2; ROW_GAIN_SLIDER = 3
        ROW_GAIN_SPINBOX = 4; ROW_BOOL_BUTTONS_1 = 5; ROW_BOOL_BUTTONS_2 = 6
        ROW_LINK = 7; ROW_CH_LABEL = 8

        # Add Header Row
        grid_layout.addWidget(QLabel("Meter"), ROW_METER_LABEL, 0, 1, num_channels, alignment=Qt.AlignCenter)
        if add_controls:
            gain_label = "Gain (dB)" if type_prefix == "Inputs" else "Volume (dB)"
            grid_layout.addWidget(QLabel(gain_label), ROW_GAIN_LABEL, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("Controls"), ROW_BOOL_BUTTONS_1, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("DSP"), ROW_BOOL_BUTTONS_2, 0, 1, num_channels, alignment=Qt.AlignCenter)
            grid_layout.addWidget(QLabel("Link"), ROW_LINK, 0, 1, num_channels, alignment=Qt.AlignCenter)

        # Create widgets for each channel
        for i in range(num_channels):
            ch_label = QLabel(f"{i+1}"); ch_idx_0_based = i
            widget_type_key = type_prefix.lower()

            # Meter
            meter_bar = QProgressBar(); meter_bar.setRange(0, 100); meter_bar.setValue(0)
            meter_bar.setTextVisible(False); meter_bar.setOrientation(Qt.Vertical)
            meter_bar.setFixedHeight(80); meter_bar.setToolTip(f"{type_prefix} Level")
            meter_bar.setObjectName(f"{widget_type_key}_meter_{ch_idx_0_based}")
            grid_layout.addWidget(meter_bar, ROW_METER, i, alignment=Qt.AlignCenter)
            self.widgets[f"{widget_type_key}_meter"].append(meter_bar)

            if add_controls:
                # Gain/Volume Slider and SpinBox
                gain_slider = QSlider(Qt.Vertical)
                gain_spinbox = QDoubleSpinBox(); gain_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons)
                gain_spinbox.setDecimals(1); gain_spinbox.setFixedWidth(50)

                if type_prefix == "Inputs":
                    gain_slider.setRange(0, 750); gain_slider.setValue(0); gain_slider.setToolTip("Input Gain (0.0 to +75.0 dB)")
                    gain_spinbox.setRange(0.0, 75.0); gain_spinbox.setValue(0.0); gain_spinbox.setSingleStep(0.5)
                else: # Outputs
                    gain_slider.setRange(-650, 60); gain_slider.setValue(-650); gain_slider.setToolTip("Output Volume (-65.0 to +6.0 dB)")
                    gain_spinbox.setRange(-65.0, 6.0); gain_spinbox.setValue(-65.0); gain_spinbox.setSingleStep(0.5)

                gain_slider.setObjectName(f"{widget_type_key}_gain_{ch_idx_0_based}")
                gain_spinbox.setObjectName(f"{widget_type_key}_gain_spinbox_{ch_idx_0_based}")
                gain_spinbox.setToolTip(gain_slider.toolTip())

                grid_layout.addWidget(gain_slider, ROW_GAIN_SLIDER, i, alignment=Qt.AlignCenter)
                grid_layout.addWidget(gain_spinbox, ROW_GAIN_SPINBOX, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_gain"].append(gain_slider)
                self.widgets[f"{widget_type_key}_gain_spinbox"].append(gain_spinbox)

                # Link SpinBox and Slider (Use lambda to capture current widgets)
                gain_slider.valueChanged.connect(lambda value, s=gain_spinbox, t=type_prefix: s.setValue(slider_to_input_gain(value) if t == "Inputs" else slider_to_output_volume(value)))
                gain_spinbox.valueChanged.connect(lambda value, s=gain_slider, t=type_prefix: s.setValue(input_gain_to_slider(value) if t == "Inputs" else output_volume_to_slider(value)))

                # Boolean Buttons (Mute, Solo, Phase)
                bool_layout_1 = QHBoxLayout(); bool_layout_1.setSpacing(2)
                mute_cb = QCheckBox("M"); mute_cb.setObjectName(f"{widget_type_key}_mute_{ch_idx_0_based}"); mute_cb.setToolTip("Mute")
                solo_cb = QCheckBox("S"); solo_cb.setObjectName(f"{widget_type_key}_solo_{ch_idx_0_based}"); solo_cb.setToolTip("Solo")
                phase_cb = QCheckBox("Ø"); phase_cb.setObjectName(f"{widget_type_key}_phase_{ch_idx_0_based}"); phase_cb.setToolTip("Phase Invert")
                bool_layout_1.addWidget(mute_cb); bool_layout_1.addWidget(solo_cb); bool_layout_1.addWidget(phase_cb)
                grid_layout.addLayout(bool_layout_1, ROW_BOOL_BUTTONS_1, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_mute"].append(mute_cb)
                self.widgets[f"{widget_type_key}_solo"].append(solo_cb)
                self.widgets[f"{widget_type_key}_phase"].append(phase_cb)

                # Boolean Buttons (DSP Toggles)
                bool_layout_2 = QHBoxLayout(); bool_layout_2.setSpacing(2)
                eq_cb = QCheckBox("EQ"); eq_cb.setObjectName(f"{widget_type_key}_eq_enable_{ch_idx_0_based}"); eq_cb.setToolTip("EQ Enable")
                dyn_cb = QCheckBox("Dyn"); dyn_cb.setObjectName(f"{widget_type_key}_dyn_enable_{ch_idx_0_based}"); dyn_cb.setToolTip("Dynamics Enable")
                lc_cb = QCheckBox("LC"); lc_cb.setObjectName(f"{widget_type_key}_lc_enable_{ch_idx_0_based}"); lc_cb.setToolTip("Low Cut Enable")
                bool_layout_2.addWidget(eq_cb); bool_layout_2.addWidget(dyn_cb); bool_layout_2.addWidget(lc_cb)
                grid_layout.addLayout(bool_layout_2, ROW_BOOL_BUTTONS_2, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_eq_enable"].append(eq_cb)
                self.widgets[f"{widget_type_key}_dyn_enable"].append(dyn_cb)
                self.widgets[f"{widget_type_key}_lc_enable"].append(lc_cb)

                # Link Checkbox
                link_cb = QCheckBox()
                link_cb.setObjectName(f"{widget_type_key}_link_{ch_idx_0_based}")
                link_cb.setVisible((i % 2) == 0)
                link_cb.setToolTip("Link Ch {}".format(i+1) + "/{}".format(i+2) if (i%2)==0 else "")
                grid_layout.addWidget(link_cb, ROW_LINK, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_link"].append(link_cb)

            # Channel Label (always add)
            grid_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)


    def _populate_mixer_tab(self):
        """Creates the content for the Mixer tab."""
        mixer_tab_layout = QVBoxLayout(self.mixer_tab)

        # --- Output Channel Selector ---
        selector_layout = QHBoxLayout()
        selector_layout.addWidget(QLabel("Show Mix For Output:"))
        self.mixer_output_selector = QComboBox()
        for i in range(NUM_OUTPUT_CHANNELS):
            # Add item with user-friendly name, store 0-based index in UserData role
            self.mixer_output_selector.addItem(f"Output {i+1}", userData=i)
        self.mixer_output_selector.currentIndexChanged.connect(self._on_mixer_output_selected)
        selector_layout.addWidget(self.mixer_output_selector)
        selector_layout.addStretch()
        mixer_tab_layout.addLayout(selector_layout)

        # --- Scroll Area for Mixer Strips ---
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True) # Allow inner widget to resize
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff) # Vertical sliders, no vertical scroll needed
        mixer_tab_layout.addWidget(scroll_area)

        # --- Container Widget for Mixer Strips ---
        self.mixer_strips_container = QWidget()
        scroll_area.setWidget(self.mixer_strips_container)
        self.mixer_strips_layout = QHBoxLayout(self.mixer_strips_container) # Horizontal layout for strips
        self.mixer_strips_layout.setSpacing(5) # Spacing between strips

        # Initial population for the first output channel
        self._update_mixer_view(0) # Show mix for Output 1 initially

    def _update_mixer_view(self, output_ch_idx):
        """Clears and repopulates the mixer tab for the selected output channel."""
        self.current_mixer_output_idx = output_ch_idx
        print(f"[GUI] Updating mixer view for Output {output_ch_idx + 1}")

        # Clear existing widgets from the layout and our storage
        while self.mixer_strips_layout.count():
            item = self.mixer_strips_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater() # Delete widget properly

        # Clear widget references for the mixer specifically
        self.widgets['mixer_input_gain'].clear()
        self.widgets['mixer_input_gain_spinbox'].clear()
        self.widgets['mixer_input_pan_spinbox'].clear()
        self.widgets['mixer_playback_gain'].clear()
        self.widgets['mixer_playback_gain_spinbox'].clear()
        self.widgets['mixer_playback_pan_spinbox'].clear()

        # --- Populate Input Sends ---
        input_sends_group = QGroupBox(f"Input Sends to Output {output_ch_idx + 1}")
        input_sends_layout = QGridLayout(input_sends_group)
        self.mixer_strips_layout.addWidget(input_sends_group)

        ROW_VOL_LABEL = 0; ROW_VOL_SLIDER = 1; ROW_VOL_SPIN = 2
        ROW_PAN_LABEL = 3; ROW_PAN_SPIN = 4; ROW_CH_LABEL = 5

        input_sends_layout.addWidget(QLabel("Volume"), ROW_VOL_LABEL, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter)
        input_sends_layout.addWidget(QLabel("Pan"), ROW_PAN_LABEL, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter)

        for i in range(NUM_INPUT_CHANNELS):
            in_ch_idx_0_based = i
            ch_label = QLabel(f"In {i+1}")

            # Volume Slider & SpinBox
            vol_slider = QSlider(Qt.Vertical); vol_slider.setRange(-650, 60); vol_slider.setValue(-650)
            vol_spinbox = QDoubleSpinBox(); vol_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons)
            vol_spinbox.setDecimals(1); vol_spinbox.setFixedWidth(50); vol_spinbox.setRange(-65.0, 6.0); vol_spinbox.setValue(-65.0)
            vol_slider.setObjectName(f"mixer_input_gain_{output_ch_idx}_{in_ch_idx_0_based}")
            vol_spinbox.setObjectName(f"mixer_input_gain_spinbox_{output_ch_idx}_{in_ch_idx_0_based}")
            vol_slider.setToolTip(f"Input {i+1} Send Volume"); vol_spinbox.setToolTip(vol_slider.toolTip())
            vol_slider.valueChanged.connect(lambda value, s=vol_spinbox: s.setValue(slider_to_mix_send_volume(value)))
            vol_spinbox.valueChanged.connect(lambda value, s=vol_slider: s.setValue(mix_send_volume_to_slider(value)))
            vol_spinbox.valueChanged.connect(self.send_mix_gain_from_gui) # Connect spinbox change to OSC send

            # Pan SpinBox
            pan_spinbox = QSpinBox(); pan_spinbox.setRange(-100, 100); pan_spinbox.setValue(0)
            pan_spinbox.setButtonSymbols(QSpinBox.NoButtons); pan_spinbox.setFixedWidth(50)
            pan_spinbox.setObjectName(f"mixer_input_pan_spinbox_{output_ch_idx}_{in_ch_idx_0_based}")
            pan_spinbox.setToolTip(f"Input {i+1} Send Pan (L-100..0..R100)")
            pan_spinbox.valueChanged.connect(self.send_mix_pan_from_gui) # Connect spinbox change to OSC send

            input_sends_layout.addWidget(vol_slider, ROW_VOL_SLIDER, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(vol_spinbox, ROW_VOL_SPIN, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(pan_spinbox, ROW_PAN_SPIN, i, alignment=Qt.AlignCenter)
            input_sends_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)

            self.widgets['mixer_input_gain'].append(vol_slider)
            self.widgets['mixer_input_gain_spinbox'].append(vol_spinbox)
            self.widgets['mixer_input_pan_spinbox'].append(pan_spinbox)

            # Set initial value from cache if available
            cached_val = self.current_mixer_sends[output_ch_idx]['input'][in_ch_idx_0_based]
            vol_spinbox.setValue(cached_val['volume'])
            pan_spinbox.setValue(cached_val['pan'])


        # --- Populate Playback Sends ---
        playback_sends_group = QGroupBox(f"Playback Sends to Output {output_ch_idx + 1}")
        playback_sends_layout = QGridLayout(playback_sends_group)
        self.mixer_strips_layout.addWidget(playback_sends_group)

        playback_sends_layout.addWidget(QLabel("Volume"), ROW_VOL_LABEL, 0, 1, NUM_PLAYBACK_CHANNELS, alignment=Qt.AlignCenter)
        playback_sends_layout.addWidget(QLabel("Pan"), ROW_PAN_LABEL, 0, 1, NUM_PLAYBACK_CHANNELS, alignment=Qt.AlignCenter)

        for i in range(NUM_PLAYBACK_CHANNELS):
            pb_ch_idx_0_based = i
            ch_label = QLabel(f"PB {i+1}")

            # Volume Slider & SpinBox
            vol_slider = QSlider(Qt.Vertical); vol_slider.setRange(-650, 60); vol_slider.setValue(-650)
            vol_spinbox = QDoubleSpinBox(); vol_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons)
            vol_spinbox.setDecimals(1); vol_spinbox.setFixedWidth(50); vol_spinbox.setRange(-65.0, 6.0); vol_spinbox.setValue(-65.0)
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

            playback_sends_layout.addWidget(vol_slider, ROW_VOL_SLIDER, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(vol_spinbox, ROW_VOL_SPIN, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(pan_spinbox, ROW_PAN_SPIN, i, alignment=Qt.AlignCenter)
            playback_sends_layout.addWidget(ch_label, ROW_CH_LABEL, i, alignment=Qt.AlignCenter)

            self.widgets['mixer_playback_gain'].append(vol_slider)
            self.widgets['mixer_playback_gain_spinbox'].append(vol_spinbox)
            self.widgets['mixer_playback_pan_spinbox'].append(pan_spinbox)

            # Set initial value from cache if available
            cached_val = self.current_mixer_sends[output_ch_idx]['playback'][pb_ch_idx_0_based]
            vol_spinbox.setValue(cached_val['volume'])
            pan_spinbox.setValue(cached_val['pan'])

        self.mixer_strips_layout.addStretch() # Push strips to the left


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
        self.osc_handler.solo_received.connect(self.on_bool_received)
        self.osc_handler.phase_received.connect(self.on_bool_received)
        self.osc_handler.eq_enable_received.connect(self.on_bool_received)
        self.osc_handler.dyn_enable_received.connect(self.on_bool_received)
        self.osc_handler.lc_enable_received.connect(self.on_bool_received)
        # Connect Mixer signals
        self.osc_handler.mix_send_received.connect(self.on_mix_send_received)
        self.osc_handler.mix_pan_received.connect(self.on_mix_pan_received)

        # GUI -> OSC Sending
        self.connect_button.clicked.connect(self.connect_osc)
        self.disconnect_button.clicked.connect(self.disconnect_osc)

        # Connect controls for inputs and outputs (as before)
        for type_key in ["input", "output"]:
            for i in range(len(self.widgets[f"{type_key}_gain"])):
                self.widgets[f'{type_key}_gain_spinbox'][i].valueChanged.connect(self.send_gain_from_gui)
                self.widgets[f'{type_key}_mute'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_solo'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_phase'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_eq_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_dyn_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_lc_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                if (i % 2) == 0:
                     self.widgets[f'{type_key}_link'][i].stateChanged.connect(self.send_link_from_gui)

        # Note: Mixer controls are connected dynamically in _update_mixer_view


    # --- Slots for Handling GUI Actions and OSC Events ---

    @Slot(int)
    def _on_mixer_output_selected(self, index):
        """Called when the user selects a different output channel in the Mixer tab dropdown."""
        selected_output_idx = self.mixer_output_selector.itemData(index)
        if selected_output_idx is not None:
            self._update_mixer_view(selected_output_idx)

    # connect_osc, disconnect_osc, on_osc_connected, log_osc_message remain unchanged
    @Slot()
    def connect_osc(self):
        ip = self.ip_input.text().strip()
        try:
            send_port = int(self.port_input.text()); listen_port = int(self.listen_port_input.text())
            if not (0 < send_port < 65536 and 0 < listen_port < 65536): raise ValueError("Port numbers must be between 1 and 65535")
        except ValueError as e: QMessageBox.warning(self, "Input Error", f"Invalid port number: {e}"); return
        self.status_bar.showMessage(f"Attempting connection to oscmix at {ip}:{send_port}...")
        self.log_osc_message("[GUI]", [f"Connecting to oscmix @ {ip}:{send_port}...", f"Listening on {self.osc_handler.listen_ip}:{listen_port}"])
        self.connection_status_label.setText("CONNECTING..."); self.connection_status_label.setStyleSheet("font-weight: bold; color: orange;")
        self.osc_handler.listen_port = listen_port
        if not self.osc_handler.start(ip, send_port):
             self.status_bar.showMessage("Connection Failed (Check Console/Log)"); self.connection_status_label.setText("FAILED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")

    @Slot()
    def disconnect_osc(self):
        self.status_bar.showMessage("Disconnecting...")
        self.log_osc_message("[GUI]", ["Disconnecting OSC Handler."])
        self.osc_handler.stop()

    @Slot()
    def on_osc_connected(self):
        status_msg = f"Connected to oscmix ({self.osc_handler.target_ip}:{self.osc_handler.target_port}) | GUI Listening on {self.osc_handler.listen_port}"
        self.status_bar.showMessage(status_msg); self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("CONNECTED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: green;")
        self.connect_button.setEnabled(False); self.disconnect_button.setEnabled(True)
        self.ip_input.setEnabled(False); self.port_input.setEnabled(False); self.listen_port_input.setEnabled(False)
        self.meter_timer.start()
        # Optional: Request initial state from oscmix if it supports it
        # self.osc_handler.send("/refresh", [])

    @Slot(str)
    def on_osc_disconnected(self, reason):
        status_msg = f"Disconnected: {reason}"
        self.status_bar.showMessage(status_msg); self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("DISCONNECTED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        self.connect_button.setEnabled(True); self.disconnect_button.setEnabled(False)
        self.ip_input.setEnabled(True); self.port_input.setEnabled(True); self.listen_port_input.setEnabled(True)
        self.meter_timer.stop()
        for meter_type in ['input_meter', 'output_meter', 'playback_meter']:
            for meter in self.widgets.get(meter_type, []):
                 if meter: meter.setValue(0)
        self.last_meter_values['input'].clear(); self.last_meter_values['output'].clear(); self.last_meter_values['playback'].clear()
        # Clear mixer cache on disconnect
        self.current_mixer_sends.clear()


    @Slot(str, list)
    def log_osc_message(self, address, args):
        if self.osc_log.document().blockCount() > 200:
             cursor = self.osc_log.textCursor(); cursor.movePosition(cursor.Start)
             cursor.select(cursor.BlockUnderCursor); cursor.removeSelectedText(); cursor.deleteChar()
        self.osc_log.append(f"{address} {args}")
        self.osc_log.verticalScrollBar().setValue(self.osc_log.verticalScrollBar().maximum())

    @Slot(str, int, float)
    def on_meter_received(self, type, ch_idx, value):
        if type in self.last_meter_values: self.last_meter_values[type][ch_idx] = value
        else: print(f"[Warning] Received meter for unknown type: {type}")

    @Slot()
    def update_gui_meters(self):
        for type_key, meter_values in self.last_meter_values.items():
            widget_key = f"{type_key}_meter"
            meter_widget_list = self.widgets.get(widget_key)
            if meter_widget_list:
                for ch_idx, value in meter_values.items():
                    if 0 <= ch_idx < len(meter_widget_list):
                        meter_widget_list[ch_idx].setValue(float_to_slider_linear(value))
            meter_values.clear()

    @Slot(str, int, float)
    def on_gain_received(self, type, ch_idx, value_db):
        gain_widget_key = f"{type}_gain"; spinbox_widget_key = f"{type}_gain_spinbox"
        slider_list = self.widgets.get(gain_widget_key); spinbox_list = self.widgets.get(spinbox_widget_key)
        if slider_list and spinbox_list and 0 <= ch_idx < len(slider_list):
            slider = slider_list[ch_idx]; spinbox = spinbox_list[ch_idx]
            slider.blockSignals(True); spinbox.blockSignals(True)
            if type == 'input': slider.setValue(input_gain_to_slider(value_db)); spinbox.setValue(value_db)
            elif type == 'output': slider.setValue(output_volume_to_slider(value_db)); spinbox.setValue(value_db)
            slider.blockSignals(False); spinbox.blockSignals(False)

    @Slot(str, int, bool)
    def on_link_received(self, type, ch_idx, value):
        widget_key = f"{type}_link"; cb_list = self.widgets.get(widget_key)
        if cb_list and 0 <= ch_idx < len(cb_list) and (ch_idx % 2 == 0):
            cb = cb_list[ch_idx]; cb.blockSignals(True); cb.setChecked(value); cb.blockSignals(False)
            self._update_linked_channel_state(type, ch_idx, value)

    @Slot(str, int, bool)
    def on_bool_received(self, type, ch_idx, value):
        base_type, control_type = type.split('_', 1)
        widget_key = f"{base_type}_{control_type}"
        cb_list = self.widgets.get(widget_key)
        if cb_list and 0 <= ch_idx < len(cb_list):
            cb = cb_list[ch_idx]; cb.blockSignals(True); cb.setChecked(value); cb.blockSignals(False)

    @Slot(int, str, int, float)
    def on_mix_send_received(self, out_idx, in_type, in_idx, value_db):
        """Updates a mixer send volume slider/spinbox."""
        # Cache the value
        self.current_mixer_sends[out_idx][in_type][in_idx]['volume'] = value_db
        # Update GUI only if this output mix is currently displayed
        if out_idx == self.current_mixer_output_idx:
            widget_base = f"mixer_{in_type}_gain"
            slider_list = self.widgets.get(widget_base)
            spinbox_list = self.widgets.get(f"{widget_base}_spinbox")
            if slider_list and spinbox_list and 0 <= in_idx < len(slider_list):
                slider = slider_list[in_idx]; spinbox = spinbox_list[in_idx]
                slider.blockSignals(True); spinbox.blockSignals(True)
                slider.setValue(mix_send_volume_to_slider(value_db))
                spinbox.setValue(value_db)
                slider.blockSignals(False); spinbox.blockSignals(False)

    @Slot(int, str, int, int)
    def on_mix_pan_received(self, out_idx, in_type, in_idx, pan_value):
        """Updates a mixer send pan spinbox."""
        # Cache the value
        self.current_mixer_sends[out_idx][in_type][in_idx]['pan'] = pan_value
        # Update GUI only if this output mix is currently displayed
        if out_idx == self.current_mixer_output_idx:
            widget_base = f"mixer_{in_type}_pan_spinbox"
            spinbox_list = self.widgets.get(widget_base)
            if spinbox_list and 0 <= in_idx < len(spinbox_list):
                spinbox = spinbox_list[in_idx]
                spinbox.blockSignals(True)
                spinbox.setValue(pan_to_spinbox(pan_value))
                spinbox.blockSignals(False)

    @Slot(float) # Connected to main gain/volume QDoubleSpinBox
    def send_gain_from_gui(self, value_db):
        sender_widget = self.sender(); obj_name = sender_widget.objectName()
        try: type, gain_or_vol, _, idx_str = obj_name.split('_'); ch_idx = int(idx_str)
        except Exception as e: print(f"Error parsing sender object name '{obj_name}': {e}"); return
        ch_num_1_based = ch_idx + 1; float_value_db = value_db
        if type == 'input': osc_address = OSC_INPUT_GAIN.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_VOLUME.format(ch=ch_num_1_based)
        else: return
        self.osc_handler.send(osc_address, float_value_db)

    @Slot(int) # Connected to main link QCheckBox
    def send_link_from_gui(self, state):
        sender_widget = self.sender(); obj_name = sender_widget.objectName()
        try: type, _, idx_str = obj_name.split('_'); ch_idx = int(idx_str)
        except Exception as e: print(f"Error parsing sender object name '{obj_name}': {e}"); return
        bool_value = (state == Qt.Checked); osc_value = 1 if bool_value else 0
        ch_num_1_based = ch_idx + 1
        if type == 'input': osc_address = OSC_INPUT_STEREO.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_STEREO.format(ch=ch_num_1_based)
        else: return
        self.osc_handler.send(osc_address, osc_value)
        self._update_linked_channel_state(type, ch_idx, bool_value)

    @Slot(int) # Connected to main boolean QCheckBoxes
    def send_bool_from_gui(self, state):
        sender_widget = self.sender(); obj_name = sender_widget.objectName()
        try:
            parts = obj_name.split('_'); type = parts[0]; control_name = "_".join(parts[1:-1]); ch_idx = int(parts[-1])
        except Exception as e: print(f"Error parsing sender object name '{obj_name}': {e}"); return
        bool_value = (state == Qt.Checked); osc_value = 1 if bool_value else 0
        ch_num_1_based = ch_idx + 1
        # Construct address based on type and control name
        addr_map = { "mute": (OSC_INPUT_MUTE, OSC_OUTPUT_MUTE), "solo": (OSC_INPUT_SOLO, OSC_OUTPUT_SOLO),
                     "phase": (OSC_INPUT_PHASE, OSC_OUTPUT_PHASE), "eq_enable": (OSC_INPUT_EQ_ENABLE, OSC_OUTPUT_EQ_ENABLE),
                     "dyn_enable": (OSC_INPUT_DYN_ENABLE, OSC_OUTPUT_DYN_ENABLE), "lc_enable": (OSC_INPUT_LC_ENABLE, OSC_OUTPUT_LC_ENABLE) }
        paths = addr_map.get(control_name)
        if not paths: print(f"Unknown boolean control: {control_name}"); return
        osc_address = (paths[0] if type == "input" else paths[1]).format(ch=ch_num_1_based)
        self.osc_handler.send(osc_address, osc_value)

    @Slot(float) # Connected to Mixer Volume QDoubleSpinBox
    def send_mix_gain_from_gui(self, value_db):
        """Sends OSC message for a mixer send volume change."""
        sender_widget = self.sender(); obj_name = sender_widget.objectName()
        try: # e.g., "mixer_input_gain_spinbox_5_0" -> out 5, in 0
            _, in_type, gain_str, _, out_idx_str, in_idx_str = obj_name.split('_')
            out_idx = int(out_idx_str); in_idx = int(in_idx_str)
        except Exception as e: print(f"Error parsing mixer sender object name '{obj_name}': {e}"); return

        # Get current pan value for this send from cache or default
        current_pan = self.current_mixer_sends[out_idx][in_type][in_idx].get('pan', 0)

        # Construct address and values for setmix command
        osc_address = OSC_MIX_SEND_SET.format(out_ch=out_idx + 1, in_type=in_type, in_ch=in_idx + 1)
        # oscmix setmix expects volume (dB float), pan (int), width (float, default 1.0?)
        # Sending only volume and pan for now, assuming width=1.0 default
        values_to_send = [float(value_db), int(current_pan)]
        self.osc_handler.send_multi(osc_address, values_to_send)
        # Update cache immediately
        self.current_mixer_sends[out_idx][in_type][in_idx]['volume'] = value_db

    @Slot(int) # Connected to Mixer Pan QSpinBox
    def send_mix_pan_from_gui(self, pan_value):
        """Sends OSC message for a mixer send pan change."""
        sender_widget = self.sender(); obj_name = sender_widget.objectName()
        try: # e.g., "mixer_input_pan_spinbox_5_0" -> out 5, in 0
            _, in_type, pan_str, _, out_idx_str, in_idx_str = obj_name.split('_')
            out_idx = int(out_idx_str); in_idx = int(in_idx_str)
        except Exception as e: print(f"Error parsing mixer sender object name '{obj_name}': {e}"); return

        # Get current volume value for this send from cache or default
        current_volume = self.current_mixer_sends[out_idx][in_type][in_idx].get('volume', -65.0)

        # Construct address and values for setmix command
        osc_address = OSC_MIX_SEND_SET.format(out_ch=out_idx + 1, in_type=in_type, in_ch=in_idx + 1)
        # oscmix setmix expects volume (dB float), pan (int), width (float, default 1.0?)
        values_to_send = [float(current_volume), int(pan_value)]
        self.osc_handler.send_multi(osc_address, values_to_send)
        # Update cache immediately
        self.current_mixer_sends[out_idx][in_type][in_idx]['pan'] = pan_value


    def _update_linked_channel_state(self, type, odd_ch_idx, is_linked):
         """Enables/disables controls of the paired (even) channel."""
         even_ch_idx = odd_ch_idx + 1
         # Disable gain slider and spinbox
         gain_list = self.widgets.get(f"{type}_gain"); spinbox_list = self.widgets.get(f"{type}_gain_spinbox")
         if gain_list and 0 <= even_ch_idx < len(gain_list): gain_list[even_ch_idx].setEnabled(not is_linked)
         if spinbox_list and 0 <= even_ch_idx < len(spinbox_list): spinbox_list[even_ch_idx].setEnabled(not is_linked)
         # Disable boolean controls
         for control in ["mute", "solo", "phase", "eq_enable", "dyn_enable", "lc_enable"]:
             cb_list = self.widgets.get(f"{type}_{control}")
             if cb_list and 0 <= even_ch_idx < len(cb_list): cb_list[even_ch_idx].setEnabled(not is_linked)

    def closeEvent(self, event):
        print("[GUI] Closing application..."); self.meter_timer.stop(); self.osc_handler.stop(); event.accept()

# --- Main execution function (Unchanged) ---
def run():
    app = QApplication(sys.argv); window = OscmixControlGUI(); window.show(); sys.exit(app.exec())

# --- Entry point for direct execution (Unchanged) ---
if __name__ == '__main__': run()
