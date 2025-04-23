# oscmix_gui/main.py
# Updated based on oscmix C source, packaging, Tabs, and Web GUI elements.
import sys
import time
import threading
import queue
from collections import defaultdict

# Attempt to import PySide6, provide helpful error if missing
try:
    from PySide6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QGridLayout, QLabel, QLineEdit, QPushButton, QSlider, QCheckBox,
        QProgressBar, QGroupBox, QStatusBar, QMessageBox, QTextEdit,
        QTabWidget, QDoubleSpinBox # Added QDoubleSpinBox
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
# Using 1-based channel index {ch} as defined in oscmix C code oscnode trees
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
def output_volume_to_slider(db_value):
    try: return int(max(-65.0, min(6.0, float(db_value))) * 10.0)
    except (ValueError, TypeError): return -650
def slider_to_output_volume(slider_value): return float(slider_value) / 10.0

#=============================================================================
# OSC Communication Handler Class
#=============================================================================
class OSCHandler(QObject):
    """Handles OSC sending/receiving for oscmix."""
    connected = Signal()
    disconnected = Signal(str)
    message_received_signal = Signal(str, list)
    meter_received = Signal(str, int, float) # type, ch_idx, value
    gain_received = Signal(str, int, float)  # type, ch_idx, value (dB)
    link_received = Signal(str, int, bool)   # type, ch_idx (odd), value
    # Added signals for new boolean controls
    mute_received = Signal(str, int, bool)   # type, ch_idx, value
    solo_received = Signal(str, int, bool)   # type, ch_idx, value
    phase_received = Signal(str, int, bool)  # type, ch_idx, value
    eq_enable_received = Signal(str, int, bool) # type, ch_idx, value
    dyn_enable_received = Signal(str, int, bool)# type, ch_idx, value
    lc_enable_received = Signal(str, int, bool) # type, ch_idx, value

    def __init__(self, listen_ip, listen_port, parent=None):
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
        self.dispatcher.set_default_handler(self._default_handler)

    def _map_osc_addresses(self):
        """Sets up the OSC address mappings in the dispatcher."""
        print(f"Mapping Inputs : Gain={OSC_INPUT_GAIN.format(ch='<ch>')}, Stereo={OSC_INPUT_STEREO.format(ch='<ch>')}, Level={OSC_INPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_INPUT_MUTE.format(ch='<ch>')}, Solo={OSC_INPUT_SOLO.format(ch='<ch>')}, Phase={OSC_INPUT_PHASE.format(ch='<ch>')}, EQ={OSC_INPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_INPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_INPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Outputs: Vol={OSC_OUTPUT_VOLUME.format(ch='<ch>')}, Stereo={OSC_OUTPUT_STEREO.format(ch='<ch>')}, Level={OSC_OUTPUT_LEVEL.format(ch='<ch>')}, Mute={OSC_OUTPUT_MUTE.format(ch='<ch>')}, Solo={OSC_OUTPUT_SOLO.format(ch='<ch>')}, Phase={OSC_OUTPUT_PHASE.format(ch='<ch>')}, EQ={OSC_OUTPUT_EQ_ENABLE.format(ch='<ch>')}, Dyn={OSC_OUTPUT_DYN_ENABLE.format(ch='<ch>')}, LC={OSC_OUTPUT_LC_ENABLE.format(ch='<ch>')}")
        print(f"Mapping Playback: Level={OSC_PLAYBACK_LEVEL.format(ch='<ch>')} (Meters only)")

        # Map input channels
        for i in range(1, NUM_INPUT_CHANNELS + 1):
             ch_idx_0_based = i - 1
             self.dispatcher.map(OSC_INPUT_GAIN.format(ch=i), self._handle_gain, "input", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_LEVEL.format(ch=i), self._handle_meter, "input", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_MUTE.format(ch=i), self._handle_bool, "input_mute", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_SOLO.format(ch=i), self._handle_bool, "input_solo", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_PHASE.format(ch=i), self._handle_bool, "input_phase", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_EQ_ENABLE.format(ch=i), self._handle_bool, "input_eq_enable", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_DYN_ENABLE.format(ch=i), self._handle_bool, "input_dyn_enable", ch_idx_0_based)
             self.dispatcher.map(OSC_INPUT_LC_ENABLE.format(ch=i), self._handle_bool, "input_lc_enable", ch_idx_0_based)
             if (i % 2) != 0:
                 self.dispatcher.map(OSC_INPUT_STEREO.format(ch=i), self._handle_link, "input", ch_idx_0_based)

        # Map output channels
        for i in range(1, NUM_OUTPUT_CHANNELS + 1):
            ch_idx_0_based = i - 1
            self.dispatcher.map(OSC_OUTPUT_VOLUME.format(ch=i), self._handle_gain, "output", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_LEVEL.format(ch=i), self._handle_meter, "output", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_MUTE.format(ch=i), self._handle_bool, "output_mute", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_SOLO.format(ch=i), self._handle_bool, "output_solo", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_PHASE.format(ch=i), self._handle_bool, "output_phase", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_EQ_ENABLE.format(ch=i), self._handle_bool, "output_eq_enable", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_DYN_ENABLE.format(ch=i), self._handle_bool, "output_dyn_enable", ch_idx_0_based)
            self.dispatcher.map(OSC_OUTPUT_LC_ENABLE.format(ch=i), self._handle_bool, "output_lc_enable", ch_idx_0_based)
            if (i % 2) != 0:
                 self.dispatcher.map(OSC_OUTPUT_STEREO.format(ch=i), self._handle_link, "output", ch_idx_0_based)

        # Map playback channels (only meters for now)
        for i in range(1, NUM_PLAYBACK_CHANNELS + 1):
            ch_idx_0_based = i - 1
            self.dispatcher.map(OSC_PLAYBACK_LEVEL.format(ch=i), self._handle_meter, "playback", ch_idx_0_based)

    # --- Handlers for specific OSC messages ---
    def _handle_meter(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (float, int)):
                 QMetaObject.invokeMethod(self, "emit_meter_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_gain(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (float, int)):
                 QMetaObject.invokeMethod(self, "emit_gain_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(float, float(value)))

    def _handle_link(self, address, *args):
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
             osc_type, ch_idx, value = args[0], args[1], args[2]
             if isinstance(value, (int, float, bool)):
                 QMetaObject.invokeMethod(self, "emit_link_received", Qt.QueuedConnection, Q_ARG(str, osc_type), Q_ARG(int, ch_idx), Q_ARG(bool, bool(value)))

    def _handle_bool(self, address, *args):
        """Generic handler for boolean (on/off) OSC messages."""
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
            signal_type, ch_idx, value = args[0], args[1], args[2] # signal_type is e.g., "input_mute"
            if isinstance(value, (int, float, bool)):
                bool_value = bool(value)
                # Emit the corresponding specific signal based on signal_type
                if signal_type == "input_mute" or signal_type == "output_mute":
                    QMetaObject.invokeMethod(self, "emit_mute_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                elif signal_type == "input_solo" or signal_type == "output_solo":
                    QMetaObject.invokeMethod(self, "emit_solo_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                elif signal_type == "input_phase" or signal_type == "output_phase":
                    QMetaObject.invokeMethod(self, "emit_phase_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                elif signal_type == "input_eq_enable" or signal_type == "output_eq_enable":
                    QMetaObject.invokeMethod(self, "emit_eq_enable_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                elif signal_type == "input_dyn_enable" or signal_type == "output_dyn_enable":
                    QMetaObject.invokeMethod(self, "emit_dyn_enable_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                elif signal_type == "input_lc_enable" or signal_type == "output_lc_enable":
                    QMetaObject.invokeMethod(self, "emit_lc_enable_received", Qt.QueuedConnection, Q_ARG(str, signal_type.split('_')[0]), Q_ARG(int, ch_idx), Q_ARG(bool, bool_value))
                # Add more elif blocks here for other boolean controls if needed

    # --- Slots for Safe Signal Emission ---
    @Slot(str, int, float)
    def emit_meter_received(self, type, ch_idx, val): self.meter_received.emit(type, ch_idx, val)
    @Slot(str, int, float)
    def emit_gain_received(self, type, ch_idx, val): self.gain_received.emit(type, ch_idx, val)
    @Slot(str, int, bool)
    def emit_link_received(self, type, ch_idx, val): self.link_received.emit(type, ch_idx, val)
    # Added slots for new boolean signals
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
        if self.client and self._is_connected:
            try: self.client.send_message(address, value)
            except Exception as e: print(f"!!! [OSC Handler] Send Error to {address}: {e}")
        elif not self._is_connected: print(f"[OSC Handler] Not connected, cannot send: {address} {value}")
        else: print(f"[OSC Handler] Client is None, cannot send: {address} {value}")


#=============================================================================
# Main GUI Window Class
#=============================================================================
class OscmixControlGUI(QMainWindow):
    """Main application window for controlling oscmix via OSC."""
    def __init__(self):
        """Initializes the main GUI window."""
        super().__init__()
        self.setWindowTitle("oscmix GUI Control (RME UCX II)")
        self.setGeometry(100, 100, 1050, 750) # Made wider/taller for new controls

        self.osc_handler = OSCHandler(DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT)

        self.last_meter_values = {
            'input': defaultdict(float), 'output': defaultdict(float), 'playback': defaultdict(float)
        }

        # Dictionary to hold references to GUI widgets
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
        conn_layout = QHBoxLayout()
        conn_group.setLayout(conn_layout)
        self.ip_input = QLineEdit(DEFAULT_OSCMIX_IP)
        self.ip_input.setToolTip("IP address where the oscmix server is running (often 127.0.0.1)")
        self.port_input = QLineEdit(str(DEFAULT_OSCMIX_SEND_PORT))
        self.port_input.setToolTip("Port number the oscmix server is LISTENING on")
        self.listen_port_input = QLineEdit(str(DEFAULT_LISTEN_PORT))
        self.listen_port_input.setToolTip("Port number this GUI should listen on for replies from oscmix")
        self.connect_button = QPushButton("Connect")
        self.disconnect_button = QPushButton("Disconnect")
        self.disconnect_button.setEnabled(False)
        self.connection_status_label = QLabel("DISCONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        conn_layout.addWidget(QLabel("'oscmix' Server IP:"))
        conn_layout.addWidget(self.ip_input); conn_layout.addWidget(QLabel("Server Port:"))
        conn_layout.addWidget(self.port_input); conn_layout.addWidget(QLabel("GUI Listen Port:"))
        conn_layout.addWidget(self.listen_port_input); conn_layout.addWidget(self.connect_button)
        conn_layout.addWidget(self.disconnect_button); conn_layout.addStretch()
        conn_layout.addWidget(self.connection_status_label)
        main_layout.addWidget(conn_group)

        # --- Tab Widget for Channels ---
        self.tab_widget = QTabWidget()
        main_layout.addWidget(self.tab_widget)

        # --- Create Tab Pages ---
        self.input_tab = QWidget(); self.playback_tab = QWidget(); self.output_tab = QWidget()
        self.tab_widget.addTab(self.input_tab, "Inputs")
        self.tab_widget.addTab(self.playback_tab, "Playback")
        self.tab_widget.addTab(self.output_tab, "Outputs")

        # --- Populate Tabs ---
        self._populate_channel_tab(self.input_tab, "Inputs", "Hardware Inputs", NUM_INPUT_CHANNELS, True)
        self._populate_channel_tab(self.playback_tab, "Playback", "Software Playback", NUM_PLAYBACK_CHANNELS, False)
        self._populate_channel_tab(self.output_tab, "Outputs", "Hardware Outputs", NUM_OUTPUT_CHANNELS, True)

        # --- OSC Log (Stays below tabs) ---
        log_group = QGroupBox("OSC Message Log (Incoming from oscmix)")
        log_layout = QVBoxLayout(); log_group.setLayout(log_layout)
        self.osc_log = QTextEdit(); self.osc_log.setReadOnly(True)
        self.osc_log.setMaximumHeight(100); self.osc_log.setToolTip("Shows raw OSC messages received")
        log_layout.addWidget(self.osc_log); main_layout.addWidget(log_group)

        # --- Status Bar ---
        self.status_bar = QStatusBar(); self.setStatusBar(self.status_bar)

    def _populate_channel_tab(self, tab_widget, type_prefix, group_title, num_channels, add_controls):
        """Helper function to create the content for a channel tab."""
        tab_layout = QVBoxLayout(tab_widget)
        group_box = QGroupBox(group_title)
        tab_layout.addWidget(group_box)
        grid_layout = QGridLayout()
        group_box.setLayout(grid_layout)

        # Define grid rows
        ROW_METER_LABEL = 0
        ROW_METER = 1
        ROW_GAIN_LABEL = 2
        ROW_GAIN_SLIDER = 3
        ROW_GAIN_SPINBOX = 4
        ROW_BOOL_BUTTONS_1 = 5 # Mute, Solo, Phase
        ROW_BOOL_BUTTONS_2 = 6 # EQ, Dyn, LC
        ROW_LINK = 7
        ROW_CH_LABEL = 8

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
            ch_label = QLabel(f"{i+1}")
            ch_idx_0_based = i
            widget_type_key = type_prefix.lower() # 'inputs', 'outputs', 'playback'

            # Meter
            meter_bar = QProgressBar()
            meter_bar.setRange(0, 100); meter_bar.setValue(0)
            meter_bar.setTextVisible(False); meter_bar.setOrientation(Qt.Vertical)
            meter_bar.setFixedHeight(80); meter_bar.setToolTip(f"{type_prefix} Level")
            meter_bar.setObjectName(f"{widget_type_key}_meter_{ch_idx_0_based}")
            grid_layout.addWidget(meter_bar, ROW_METER, i, alignment=Qt.AlignCenter)
            self.widgets[f"{widget_type_key}_meter"].append(meter_bar)

            if add_controls:
                # Gain/Volume Slider and SpinBox
                gain_slider = QSlider(Qt.Vertical)
                gain_spinbox = QDoubleSpinBox()
                gain_spinbox.setButtonSymbols(QDoubleSpinBox.NoButtons) # Hide spinbox arrows
                gain_spinbox.setDecimals(1)
                gain_spinbox.setFixedWidth(50) # Adjust width as needed

                if type_prefix == "Inputs":
                    gain_slider.setRange(0, 750); gain_slider.setValue(0)
                    gain_slider.setToolTip("Input Gain (0.0 to +75.0 dB)")
                    gain_spinbox.setRange(0.0, 75.0); gain_spinbox.setValue(0.0)
                    gain_spinbox.setSingleStep(0.5)
                else: # Outputs
                    gain_slider.setRange(-650, 60); gain_slider.setValue(-650)
                    gain_slider.setToolTip("Output Volume (-65.0 to +6.0 dB)")
                    gain_spinbox.setRange(-65.0, 6.0); gain_spinbox.setValue(-65.0)
                    gain_spinbox.setSingleStep(0.5)

                gain_slider.setObjectName(f"{widget_type_key}_gain_{ch_idx_0_based}")
                gain_spinbox.setObjectName(f"{widget_type_key}_gain_spinbox_{ch_idx_0_based}")
                gain_spinbox.setToolTip(gain_slider.toolTip()) # Copy tooltip

                grid_layout.addWidget(gain_slider, ROW_GAIN_SLIDER, i, alignment=Qt.AlignCenter)
                grid_layout.addWidget(gain_spinbox, ROW_GAIN_SPINBOX, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_gain"].append(gain_slider)
                self.widgets[f"{widget_type_key}_gain_spinbox"].append(gain_spinbox) # Store spinbox too

                # Link SpinBox and Slider
                gain_slider.valueChanged.connect(lambda value, s=gain_spinbox, t=type_prefix: s.setValue(slider_to_input_gain(value) if t == "Inputs" else slider_to_output_volume(value)))
                gain_spinbox.valueChanged.connect(lambda value, s=gain_slider, t=type_prefix: s.setValue(input_gain_to_slider(value) if t == "Inputs" else output_volume_to_slider(value)))


                # Boolean Buttons (Mute, Solo, Phase) - Row 1
                bool_layout_1 = QHBoxLayout()
                bool_layout_1.setSpacing(2)
                mute_cb = QCheckBox("M"); mute_cb.setObjectName(f"{widget_type_key}_mute_{ch_idx_0_based}"); mute_cb.setToolTip("Mute")
                solo_cb = QCheckBox("S"); solo_cb.setObjectName(f"{widget_type_key}_solo_{ch_idx_0_based}"); solo_cb.setToolTip("Solo")
                phase_cb = QCheckBox("Ø"); phase_cb.setObjectName(f"{widget_type_key}_phase_{ch_idx_0_based}"); phase_cb.setToolTip("Phase Invert")
                bool_layout_1.addWidget(mute_cb); bool_layout_1.addWidget(solo_cb); bool_layout_1.addWidget(phase_cb)
                grid_layout.addLayout(bool_layout_1, ROW_BOOL_BUTTONS_1, i, alignment=Qt.AlignCenter)
                self.widgets[f"{widget_type_key}_mute"].append(mute_cb)
                self.widgets[f"{widget_type_key}_solo"].append(solo_cb)
                self.widgets[f"{widget_type_key}_phase"].append(phase_cb)

                # Boolean Buttons (DSP Toggles) - Row 2
                bool_layout_2 = QHBoxLayout()
                bool_layout_2.setSpacing(2)
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


    def connect_signals(self):
        """Connects Qt signals from widgets and OSC handler to corresponding slots."""
        # OSC Handler -> GUI Updates
        self.osc_handler.connected.connect(self.on_osc_connected)
        self.osc_handler.disconnected.connect(self.on_osc_disconnected)
        self.osc_handler.message_received_signal.connect(self.log_osc_message)
        self.osc_handler.meter_received.connect(self.on_meter_received)
        self.osc_handler.gain_received.connect(self.on_gain_received)
        self.osc_handler.link_received.connect(self.on_link_received)
        # Connect new boolean signals
        self.osc_handler.mute_received.connect(self.on_bool_received)
        self.osc_handler.solo_received.connect(self.on_bool_received)
        self.osc_handler.phase_received.connect(self.on_bool_received)
        self.osc_handler.eq_enable_received.connect(self.on_bool_received)
        self.osc_handler.dyn_enable_received.connect(self.on_bool_received)
        self.osc_handler.lc_enable_received.connect(self.on_bool_received)


        # GUI -> OSC Sending
        self.connect_button.clicked.connect(self.connect_osc)
        self.disconnect_button.clicked.connect(self.disconnect_osc)

        # Connect controls for inputs and outputs
        for type_key in ["input", "output"]:
            for i in range(len(self.widgets[f"{type_key}_gain"])): # Use length of gain list
                # Connect Gain Slider (valueChanged is handled by spinbox connection now)
                # self.widgets[f'{type_key}_gain'][i].valueChanged.connect(self.send_gain_from_gui)
                # Connect Gain SpinBox
                self.widgets[f'{type_key}_gain_spinbox'][i].valueChanged.connect(self.send_gain_from_gui)
                # Connect Boolean Checkboxes
                self.widgets[f'{type_key}_mute'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_solo'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_phase'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_eq_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_dyn_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                self.widgets[f'{type_key}_lc_enable'][i].stateChanged.connect(self.send_bool_from_gui)
                # Connect Link Checkbox (only for odd channels)
                if (i % 2) == 0:
                     self.widgets[f'{type_key}_link'][i].stateChanged.connect(self.send_link_from_gui)


    # --- Slots for Handling GUI Actions and OSC Events ---

    # connect_osc, disconnect_osc, on_osc_connected, log_osc_message remain unchanged
    @Slot()
    def connect_osc(self):
        ip = self.ip_input.text().strip()
        try:
            send_port = int(self.port_input.text())
            listen_port = int(self.listen_port_input.text())
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

    @Slot(str)
    def on_osc_disconnected(self, reason):
        status_msg = f"Disconnected: {reason}"
        self.status_bar.showMessage(status_msg); self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("DISCONNECTED"); self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        self.connect_button.setEnabled(True); self.disconnect_button.setEnabled(False)
        self.ip_input.setEnabled(True); self.port_input.setEnabled(True); self.listen_port_input.setEnabled(True)
        self.meter_timer.stop()
        for meter_type in ['input_meter', 'output_meter', 'playback_meter']:
            meter_list = self.widgets.get(meter_type, [])
            for meter in meter_list:
                 if meter: meter.setValue(0)
        self.last_meter_values['input'].clear(); self.last_meter_values['output'].clear(); self.last_meter_values['playback'].clear()

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
        """Updates gain/volume slider AND spinbox when a message is received."""
        gain_widget_key = f"{type}_gain"
        spinbox_widget_key = f"{type}_gain_spinbox"
        slider_list = self.widgets.get(gain_widget_key)
        spinbox_list = self.widgets.get(spinbox_widget_key)

        if slider_list and spinbox_list and 0 <= ch_idx < len(slider_list):
            slider = slider_list[ch_idx]
            spinbox = spinbox_list[ch_idx]
            # Block signals to prevent feedback loop
            slider.blockSignals(True)
            spinbox.blockSignals(True)
            if type == 'input':
                slider.setValue(input_gain_to_slider(value_db))
                spinbox.setValue(value_db)
            elif type == 'output':
                slider.setValue(output_volume_to_slider(value_db))
                spinbox.setValue(value_db)
            slider.blockSignals(False)
            spinbox.blockSignals(False)

    @Slot(str, int, bool)
    def on_link_received(self, type, ch_idx, value):
        widget_key = f"{type}_link"
        cb_list = self.widgets.get(widget_key)
        if cb_list and 0 <= ch_idx < len(cb_list) and (ch_idx % 2 == 0):
            cb = cb_list[ch_idx]; cb.blockSignals(True)
            cb.setChecked(value); cb.blockSignals(False)
            self._update_linked_channel_state(type, ch_idx, value)

    @Slot(str, int, bool)
    def on_bool_received(self, type, ch_idx, value):
        """Handles updates for Mute, Solo, Phase, EQ, Dyn, LC checkboxes."""
        # Determine the specific control type (mute, solo, etc.) from the signal type
        # The signal emitted by OSCHandler is like 'input_mute', 'output_phase', etc.
        # We need the base type ('input'/'output') and the control type ('mute', 'phase')
        base_type, control_type = type.split('_', 1) # e.g., "input", "mute" or "output", "eq_enable"

        widget_key = f"{base_type}_{control_type}" # e.g., "input_mute", "output_eq_enable"
        cb_list = self.widgets.get(widget_key)

        if cb_list and 0 <= ch_idx < len(cb_list):
            cb = cb_list[ch_idx]
            cb.blockSignals(True)
            cb.setChecked(value)
            cb.blockSignals(False)

    @Slot(float) # Connected to QDoubleSpinBox valueChanged[float]
    def send_gain_from_gui(self, value_db):
        """Sends OSC gain/volume message when GUI spinbox value changes."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_gain_spinbox_0"
        try:
            type, gain_or_vol, _, idx_str = obj_name.split('_') # Need to ignore 'spinbox' part
            ch_idx = int(idx_str)
        except Exception as e: print(f"Error parsing sender object name '{obj_name}': {e}"); return

        ch_num_1_based = ch_idx + 1
        float_value_db = value_db # Value from spinbox is already the correct float dB

        if type == 'input': osc_address = OSC_INPUT_GAIN.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_VOLUME.format(ch=ch_num_1_based)
        else: return

        self.osc_handler.send(osc_address, float_value_db)

    @Slot(int)
    def send_link_from_gui(self, state):
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_link_0"
        try: type, _, idx_str = obj_name.split('_'); ch_idx = int(idx_str)
        except Exception as e: print(f"Error parsing sender object name '{obj_name}': {e}"); return

        bool_value = (state == Qt.Checked); osc_value = 1 if bool_value else 0
        ch_num_1_based = ch_idx + 1

        if type == 'input': osc_address = OSC_INPUT_STEREO.format(ch=ch_num_1_based)
        elif type == 'output': osc_address = OSC_OUTPUT_STEREO.format(ch=ch_num_1_based)
        else: return

        self.osc_handler.send(osc_address, osc_value)
        self._update_linked_channel_state(type, ch_idx, bool_value)

    @Slot(int)
    def send_bool_from_gui(self, state):
        """Sends OSC message for Mute, Solo, Phase, EQ, Dyn, LC checkboxes."""
        sender_widget = self.sender()
        if not sender_widget: return
        obj_name = sender_widget.objectName() # e.g., "input_mute_0", "output_eq_enable_1"
        try:
            parts = obj_name.split('_')
            type = parts[0] # 'input' or 'output'
            control_name = "_".join(parts[1:-1]) # 'mute', 'solo', 'phase', 'eq_enable', etc.
            ch_idx = int(parts[-1]) # 0-based index
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}"); return

        bool_value = (state == Qt.Checked)
        osc_value = 1 if bool_value else 0
        ch_num_1_based = ch_idx + 1

        # Construct address based on type and control name
        if control_name == "mute": osc_address = (OSC_INPUT_MUTE if type == "input" else OSC_OUTPUT_MUTE).format(ch=ch_num_1_based)
        elif control_name == "solo": osc_address = (OSC_INPUT_SOLO if type == "input" else OSC_OUTPUT_SOLO).format(ch=ch_num_1_based)
        elif control_name == "phase": osc_address = (OSC_INPUT_PHASE if type == "input" else OSC_OUTPUT_PHASE).format(ch=ch_num_1_based)
        elif control_name == "eq_enable": osc_address = (OSC_INPUT_EQ_ENABLE if type == "input" else OSC_OUTPUT_EQ_ENABLE).format(ch=ch_num_1_based)
        elif control_name == "dyn_enable": osc_address = (OSC_INPUT_DYN_ENABLE if type == "input" else OSC_OUTPUT_DYN_ENABLE).format(ch=ch_num_1_based)
        elif control_name == "lc_enable": osc_address = (OSC_INPUT_LC_ENABLE if type == "input" else OSC_OUTPUT_LC_ENABLE).format(ch=ch_num_1_based)
        else: print(f"Unknown boolean control: {control_name}"); return

        self.osc_handler.send(osc_address, osc_value)


    def _update_linked_channel_state(self, type, odd_ch_idx, is_linked):
         """Enables/disables controls of the paired (even) channel."""
         even_ch_idx = odd_ch_idx + 1
         # Disable gain slider and spinbox
         gain_list = self.widgets.get(f"{type}_gain")
         spinbox_list = self.widgets.get(f"{type}_gain_spinbox")
         if gain_list and 0 <= even_ch_idx < len(gain_list): gain_list[even_ch_idx].setEnabled(not is_linked)
         if spinbox_list and 0 <= even_ch_idx < len(spinbox_list): spinbox_list[even_ch_idx].setEnabled(not is_linked)
         # Disable boolean controls (Mute, Solo, Phase, DSP toggles)
         for control in ["mute", "solo", "phase", "eq_enable", "dyn_enable", "lc_enable"]:
             cb_list = self.widgets.get(f"{type}_{control}")
             if cb_list and 0 <= even_ch_idx < len(cb_list):
                 cb_list[even_ch_idx].setEnabled(not is_linked)


    def closeEvent(self, event):
        print("[GUI] Closing application...")
        self.meter_timer.stop()
        self.osc_handler.stop()
        event.accept()


# --- Main execution function (Unchanged) ---
def run():
    app = QApplication(sys.argv)
    window = OscmixControlGUI()
    window.show()
    sys.exit(app.exec())

# --- Entry point for direct execution (Unchanged) ---
if __name__ == '__main__':
    run()
