# oscmix_gui/main.py
# Updated based on analysis of oscmix C source code and for packaging.
import queue
import sys
import threading
import time
from collections import defaultdict

# Attempt to import PySide6, provide helpful error if missing
try:
    from PySide6.QtCore import (
        Q_ARG,
        QMetaObject,
        QObject,
        Qt,
        QThread,
        QTimer,
        Signal,
        Slot,
    )
    from PySide6.QtGui import QColor, QPalette  # For status indication
    from PySide6.QtWidgets import (
        QApplication,
        QCheckBox,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QProgressBar,
        QPushButton,
        QSlider,
        QStatusBar,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )
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
OSC_INPUT_GAIN = "/input/{ch}/gain"  # Expects/Sends float (e.g., 0.0 to 75.0)
OSC_INPUT_STEREO = "/input/{ch}/stereo"  # Expects/Sends int 0/1 (on odd channel index)
OSC_INPUT_LEVEL = "/input/{ch}/level"  # Sends multiple floats (peak, rms...) + int; GUI uses first float

OSC_OUTPUT_VOLUME = "/output/{ch}/volume"  # Expects/Sends float (-65.0 to +6.0 dB)
OSC_OUTPUT_STEREO = (
    "/output/{ch}/stereo"  # Expects/Sends int 0/1 (on odd channel index)
)
OSC_OUTPUT_LEVEL = (
    "/output/{ch}/level"  # Sends multiple floats + int; GUI uses first float
)

# --- Connection Defaults (Likely for local oscmix server) ---
DEFAULT_OSCMIX_IP = "127.0.0.1"  # IP where oscmix server is running
DEFAULT_OSCMIX_SEND_PORT = (
    8000  # Port oscmix server LISTENS on (VERIFY THIS from oscmix output/docs)
)
DEFAULT_LISTEN_IP = (
    "0.0.0.0"  # IP this GUI listens on (use 0.0.0.0 to listen on all interfaces)
)
DEFAULT_LISTEN_PORT = (
    9001  # Port this GUI listens on (oscmix needs to send replies/meters here)
)

# --- Channel Counts (Adjust if your UCX II setup differs or oscmix exposes differently) ---
# Based on common UCX II specs, verify if oscmix limits this
NUM_INPUT_CHANNELS = 8
NUM_OUTPUT_CHANNELS = 8

# GUI Update Rates
METER_GUI_UPDATE_RATE_HZ = 25  # Update GUI meters N times per second
METER_GUI_UPDATE_INTERVAL_MS = int(1000 / METER_GUI_UPDATE_RATE_HZ)

# --- Helper Functions ---


# Generic float to slider (0-100 range, representing 0.0-1.0 linear)
# Used for meters, assuming the first float from /level is linear peak 0.0-1.0
def float_to_slider_linear(val):
    """Converts a float (0.0-1.0) to an integer slider value (0-100)."""
    try:
        return int(max(0.0, min(1.0, float(val))) * 100)
    except (ValueError, TypeError):
        return 0


# Input Gain (dB) to Slider (0-750)
def input_gain_to_slider(db_value):
    """Converts input gain float (0.0-75.0) to slider int (0-750)."""
    try:
        # Clamp dB value, scale by 10, convert to int
        return int(max(0.0, min(75.0, float(db_value))) * 10.0)
    except (ValueError, TypeError):
        return 0


# Slider (0-750) to Input Gain (dB)
def slider_to_input_gain(slider_value):
    """Converts slider int (0-750) to input gain float (0.0-75.0)."""
    return float(slider_value) / 10.0


# Output Volume (dB) to Slider (-650 to 60)
def output_volume_to_slider(db_value):
    """Converts output volume float (-65.0 to 6.0) to slider int (-650 to 60)."""
    try:
        # Clamp dB value, scale by 10, convert to int
        return int(max(-65.0, min(6.0, float(db_value))) * 10.0)
    except (ValueError, TypeError):
        # Default to minimum slider value on error
        return -650


# Slider (-650 to 60) to Output Volume (dB)
def slider_to_output_volume(slider_value):
    """Converts slider int (-650 to 60) to output volume float (-65.0 to 6.0)."""
    return float(slider_value) / 10.0


# =============================================================================
# OSC Communication Handler Class
# =============================================================================
class OSCHandler(QObject):
    """
    Handles OSC sending and receiving in a separate thread.
    Communicates with the main GUI thread via Qt signals.
    """

    # Signals to send received data safely to the GUI thread
    connected = Signal()
    disconnected = Signal(str)  # Carries reason for disconnection
    message_received_signal = Signal(
        str, list
    )  # Generic signal for logging all incoming messages

    # Specific signals for updating GUI elements based on received OSC data
    meter_received = Signal(
        str, int, float
    )  # type ('input'/'output'), channel_idx (0-based), value (first float from /level)
    gain_received = Signal(
        str, int, float
    )  # type ('input'/'output'), channel_idx (0-based), value (dB float)
    link_received = Signal(
        str, int, bool
    )  # type ('input'/'output'), channel_idx (0-based, odd channel), value (True/False)

    def __init__(self, listen_ip, listen_port, parent=None):
        """Initializes the OSC Handler."""
        super().__init__(parent)
        self.listen_ip = listen_ip
        self.listen_port = listen_port
        self.target_ip = ""
        self.target_port = 0

        self.client = None  # pythonosc UDP client instance
        self.server = None  # pythonosc OSC server instance
        self.server_thread = None  # Thread object for the server
        self.dispatcher = Dispatcher()  # Maps OSC addresses to handler functions
        self._stop_event = threading.Event()  # Used to signal the server thread to stop
        self._is_connected = False  # Tracks the connection state

        # --- Map OSC addresses to handlers ---
        print("--- OSC Address Mapping (Based on oscmix.c) ---")
        self._map_osc_addresses()
        print("-----------------------------------------------")

        # Catch-all for debugging unmapped messages received from oscmix
        self.dispatcher.set_default_handler(self._default_handler)

    def _map_osc_addresses(self):
        """Sets up the OSC address mappings in the dispatcher based on oscmix.c."""
        print(f"Mapping based on:")
        print(f"  Input Gain : {OSC_INPUT_GAIN.format(ch='<ch>')}")
        print(f"  Input Stereo:{OSC_INPUT_STEREO.format(ch='<ch>')}")
        print(f"  Input Level: {OSC_INPUT_LEVEL.format(ch='<ch>')}")
        print(f"  Output Vol: {OSC_OUTPUT_VOLUME.format(ch='<ch>')}")
        print(f"  Output Stereo:{OSC_OUTPUT_STEREO.format(ch='<ch>')}")
        print(f"  Output Level:{OSC_OUTPUT_LEVEL.format(ch='<ch>')}")

        # Map input channels (using 1-based channel numbers in OSC paths)
        for i in range(1, NUM_INPUT_CHANNELS + 1):
            ch_idx_0_based = i - 1
            self.dispatcher.map(
                OSC_INPUT_GAIN.format(ch=i), self._handle_gain, "input", ch_idx_0_based
            )
            self.dispatcher.map(
                OSC_INPUT_LEVEL.format(ch=i),
                self._handle_meter,
                "input",
                ch_idx_0_based,
            )
            if (i % 2) != 0:
                self.dispatcher.map(
                    OSC_INPUT_STEREO.format(ch=i),
                    self._handle_link,
                    "input",
                    ch_idx_0_based,
                )

        # Map output channels (using 1-based channel numbers in OSC paths)
        for i in range(1, NUM_OUTPUT_CHANNELS + 1):
            ch_idx_0_based = i - 1
            self.dispatcher.map(
                OSC_OUTPUT_VOLUME.format(ch=i),
                self._handle_gain,
                "output",
                ch_idx_0_based,
            )
            self.dispatcher.map(
                OSC_OUTPUT_LEVEL.format(ch=i),
                self._handle_meter,
                "output",
                ch_idx_0_based,
            )
            if (i % 2) != 0:
                self.dispatcher.map(
                    OSC_OUTPUT_STEREO.format(ch=i),
                    self._handle_link,
                    "output",
                    ch_idx_0_based,
                )

    # --- Handlers for specific OSC messages ---
    def _handle_meter(self, address, *args):
        """Handles incoming level messages (e.g., /input/1/level)."""
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
            osc_type = args[0]
            ch_idx = args[1]
            value = args[2]
            if isinstance(value, (float, int)):
                QMetaObject.invokeMethod(
                    self,
                    "emit_meter_received",
                    Qt.QueuedConnection,
                    Q_ARG(str, osc_type),
                    Q_ARG(int, ch_idx),
                    Q_ARG(float, float(value)),
                )

    def _handle_gain(self, address, *args):
        """Handles incoming gain/volume messages (e.g., /input/1/gain, /output/1/volume)."""
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
            osc_type = args[0]
            ch_idx = args[1]
            value = args[2]
            if isinstance(value, (float, int)):
                QMetaObject.invokeMethod(
                    self,
                    "emit_gain_received",
                    Qt.QueuedConnection,
                    Q_ARG(str, osc_type),
                    Q_ARG(int, ch_idx),
                    Q_ARG(float, float(value)),
                )

    def _handle_link(self, address, *args):
        """Handles incoming stereo link status messages (e.g., /input/1/stereo)."""
        self.message_received_signal.emit(address, list(args))
        if len(args) >= 3:
            osc_type = args[0]
            ch_idx = args[1]
            value = args[2]
            if isinstance(value, (int, float, bool)):
                QMetaObject.invokeMethod(
                    self,
                    "emit_link_received",
                    Qt.QueuedConnection,
                    Q_ARG(str, osc_type),
                    Q_ARG(int, ch_idx),
                    Q_ARG(bool, bool(value)),
                )

    # --- Slots for Safe Signal Emission ---
    @Slot(str, int, float)
    def emit_meter_received(self, type, ch_idx, val):
        self.meter_received.emit(type, ch_idx, val)

    @Slot(str, int, float)
    def emit_gain_received(self, type, ch_idx, val):
        self.gain_received.emit(type, ch_idx, val)

    @Slot(str, int, bool)
    def emit_link_received(self, type, ch_idx, val):
        self.link_received.emit(type, ch_idx, val)

    def _default_handler(self, address, *args):
        """Handles any unmapped OSC messages received, primarily for debugging."""
        self.message_received_signal.emit(address, list(args))

    # --- Server Thread Logic ---
    def _server_thread_target(self):
        """The function that runs in the separate OSC server thread."""
        self._is_connected = False
        server_error = None
        try:
            self.server = BlockingOSCUDPServer(
                (self.listen_ip, self.listen_port), self.dispatcher
            )
            print(f"[OSC Handler] Server listening on {self.server.server_address}")
            self._is_connected = True
            QMetaObject.invokeMethod(self, "connected", Qt.QueuedConnection)

            while not self._stop_event.is_set():
                self.server.handle_request_timeout(0.1)

        except OSError as e:
            server_error = (
                f"Error starting server (Port {self.listen_port} in use?): {e}"
            )
            print(f"!!! [OSC Handler] {server_error}")
        except Exception as e:
            server_error = f"Server error: {e}"
            print(f"!!! [OSC Handler] {server_error}")
        finally:
            self._is_connected = False
            disconnect_reason = (
                server_error if server_error else "Disconnected normally"
            )
            if self.server:
                try:
                    self.server.server_close()
                except Exception as e_close:
                    print(f"[OSC Handler] Error closing server socket: {e_close}")
            print("[OSC Handler] Server stopped.")
            QMetaObject.invokeMethod(
                self,
                "emit_disconnected",
                Qt.QueuedConnection,
                Q_ARG(str, disconnect_reason),
            )

    @Slot(str)
    def emit_disconnected(self, message):
        """Slot to ensure the disconnected signal is emitted correctly."""
        self.disconnected.emit(message)

    # --- Public Control Methods ---
    def start(self, target_ip, target_port):
        """Starts the OSC server thread and initializes the OSC client."""
        if self.server_thread and self.server_thread.is_alive():
            print("[OSC Handler] Already running.")
            return False

        self.target_ip = target_ip
        self.target_port = target_port
        try:
            if not (0 < target_port < 65536):
                raise ValueError("Invalid target port number")
            self.client = SimpleUDPClient(self.target_ip, self.target_port)
            print(
                f"[OSC Handler] Client configured for target {self.target_ip}:{self.target_port}"
            )
        except Exception as e:
            self.emit_disconnected(f"Client config error: {e}")
            return False

        self._stop_event.clear()
        self.server_thread = threading.Thread(
            target=self._server_thread_target, daemon=True
        )
        self.server_thread.start()
        return True

    def stop(self):
        """Stops the OSC server thread and cleans up."""
        if not self.server_thread or not self.server_thread.is_alive():
            print("[OSC Handler] Not running.")
            if not self._is_connected:
                self.emit_disconnected("Stopped manually (was not running)")
            self._is_connected = False
            return

        print("[OSC Handler] Stopping...")
        self._stop_event.set()
        server_instance = getattr(self, "server", None)
        if server_instance:
            try:
                server_instance.shutdown()
            except Exception as e:
                print(f"[OSC Handler] Error during server shutdown call: {e}")

        self.server_thread.join(timeout=1.0)
        if self.server_thread.is_alive():
            print("[OSC Handler] Warning: Server thread did not exit cleanly.")

        self.server_thread = None
        self.server = None
        self.client = None
        self._is_connected = False
        print("[OSC Handler] Stopped.")

    def send(self, address, value):
        """Sends an OSC message to the target (oscmix)."""
        if self.client and self._is_connected:
            try:
                # print(f"[OSC SEND] {address} {value}") # Uncomment for debug
                self.client.send_message(address, value)
            except Exception as e:
                print(f"!!! [OSC Handler] Send Error to {address}: {e}")
        elif not self._is_connected:
            print(f"[OSC Handler] Not connected, cannot send: {address} {value}")
        else:
            print(f"[OSC Handler] Client is None, cannot send: {address} {value}")


# =============================================================================
# Main GUI Window Class
# =============================================================================
class OscmixControlGUI(QMainWindow):
    """Main application window for controlling oscmix via OSC."""

    def __init__(self):
        """Initializes the main GUI window."""
        super().__init__()
        self.setWindowTitle("oscmix GUI Control (RME UCX II)")
        self.setGeometry(100, 100, 900, 650)

        self.osc_handler = OSCHandler(DEFAULT_LISTEN_IP, DEFAULT_LISTEN_PORT)

        self.last_meter_values = {
            "input": defaultdict(float),
            "output": defaultdict(float),
        }

        self.widgets = {
            "input_gain": [],
            "input_link": [],
            "input_meter": [],
            "output_gain": [],
            "output_link": [],
            "output_meter": [],
        }

        self.init_ui()
        self.connect_signals()

        self.meter_timer = QTimer(self)
        self.meter_timer.setInterval(METER_GUI_UPDATE_INTERVAL_MS)
        self.meter_timer.timeout.connect(self.update_gui_meters)

        self.status_bar.showMessage(
            "Disconnected. Ensure 'oscmix' is running and configured."
        )

    def init_ui(self):
        """Creates and arranges all the GUI widgets."""
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)

        # --- Connection Group ---
        conn_group = QGroupBox("Connection to 'oscmix' Server")
        conn_layout = QHBoxLayout()
        conn_group.setLayout(conn_layout)
        self.ip_input = QLineEdit(DEFAULT_OSCMIX_IP)
        self.ip_input.setToolTip(
            "IP address where the oscmix server is running (often 127.0.0.1)"
        )
        self.port_input = QLineEdit(str(DEFAULT_OSCMIX_SEND_PORT))
        self.port_input.setToolTip("Port number the oscmix server is LISTENING on")
        self.listen_port_input = QLineEdit(str(DEFAULT_LISTEN_PORT))
        self.listen_port_input.setToolTip(
            "Port number this GUI should listen on for replies from oscmix"
        )
        self.connect_button = QPushButton("Connect")
        self.disconnect_button = QPushButton("Disconnect")
        self.disconnect_button.setEnabled(False)
        self.connection_status_label = QLabel("DISCONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        conn_layout.addWidget(QLabel("'oscmix' Server IP:"))
        conn_layout.addWidget(self.ip_input)
        conn_layout.addWidget(QLabel("Server Port:"))
        conn_layout.addWidget(self.port_input)
        conn_layout.addWidget(QLabel("GUI Listen Port:"))
        conn_layout.addWidget(self.listen_port_input)
        conn_layout.addWidget(self.connect_button)
        conn_layout.addWidget(self.disconnect_button)
        conn_layout.addStretch()
        conn_layout.addWidget(self.connection_status_label)
        main_layout.addWidget(conn_group)

        # --- Channels Layout ---
        channels_layout = QHBoxLayout()

        # --- Input Channels Group ---
        input_group = QGroupBox("Inputs")
        input_grid = QGridLayout()
        input_group.setLayout(input_grid)
        input_grid.addWidget(
            QLabel("Meter"), 0, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter
        )
        input_grid.addWidget(
            QLabel("Gain (dB)"), 2, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter
        )
        input_grid.addWidget(
            QLabel("Link"), 4, 0, 1, NUM_INPUT_CHANNELS, alignment=Qt.AlignCenter
        )

        for i in range(NUM_INPUT_CHANNELS):
            ch_label = QLabel(f"{i+1}")
            gain_slider = QSlider(Qt.Vertical)
            gain_slider.setRange(0, 750)  # Range 0.0 to 75.0 dB, scaled by 10
            gain_slider.setValue(0)
            gain_slider.setObjectName(f"input_gain_{i}")
            gain_slider.setToolTip("Input Gain (0.0 to +75.0 dB)")

            meter_bar = QProgressBar()
            meter_bar.setRange(0, 100)
            meter_bar.setValue(0)
            meter_bar.setTextVisible(False)
            meter_bar.setOrientation(Qt.Vertical)
            meter_bar.setFixedHeight(100)
            meter_bar.setToolTip("Input Level")

            link_cb = QCheckBox()
            link_cb.setObjectName(f"input_link_{i}")
            link_cb.setVisible((i % 2) == 0)
            link_cb.setToolTip(
                "Link Ch {}".format(i + 1) + "/{}".format(i + 2) if (i % 2) == 0 else ""
            )

            input_grid.addWidget(meter_bar, 1, i, alignment=Qt.AlignCenter)
            input_grid.addWidget(gain_slider, 3, i, alignment=Qt.AlignCenter)
            input_grid.addWidget(link_cb, 5, i, alignment=Qt.AlignCenter)
            input_grid.addWidget(ch_label, 6, i, alignment=Qt.AlignCenter)

            self.widgets["input_gain"].append(gain_slider)
            self.widgets["input_link"].append(link_cb)
            self.widgets["input_meter"].append(meter_bar)

        channels_layout.addWidget(input_group)

        # --- Output Channels Group ---
        output_group = QGroupBox("Outputs")
        output_grid = QGridLayout()
        output_group.setLayout(output_grid)
        output_grid.addWidget(
            QLabel("Meter"), 0, 0, 1, NUM_OUTPUT_CHANNELS, alignment=Qt.AlignCenter
        )
        output_grid.addWidget(
            QLabel("Volume (dB)"),
            2,
            0,
            1,
            NUM_OUTPUT_CHANNELS,
            alignment=Qt.AlignCenter,
        )
        output_grid.addWidget(
            QLabel("Link"), 4, 0, 1, NUM_OUTPUT_CHANNELS, alignment=Qt.AlignCenter
        )

        for i in range(NUM_OUTPUT_CHANNELS):
            ch_label = QLabel(f"{i+1}")
            gain_slider = QSlider(Qt.Vertical)
            gain_slider.setRange(-650, 60)  # Range -65.0 to +6.0 dB, scaled by 10
            gain_slider.setValue(-650)
            gain_slider.setObjectName(f"output_gain_{i}")
            gain_slider.setToolTip("Output Volume (-65.0 to +6.0 dB)")

            meter_bar = QProgressBar()
            meter_bar.setRange(0, 100)
            meter_bar.setValue(0)
            meter_bar.setTextVisible(False)
            meter_bar.setOrientation(Qt.Vertical)
            meter_bar.setFixedHeight(100)
            meter_bar.setToolTip("Output Level")

            link_cb = QCheckBox()
            link_cb.setObjectName(f"output_link_{i}")
            link_cb.setVisible((i % 2) == 0)
            link_cb.setToolTip(
                "Link Ch {}".format(i + 1) + "/{}".format(i + 2) if (i % 2) == 0 else ""
            )

            output_grid.addWidget(meter_bar, 1, i, alignment=Qt.AlignCenter)
            output_grid.addWidget(gain_slider, 3, i, alignment=Qt.AlignCenter)
            output_grid.addWidget(link_cb, 5, i, alignment=Qt.AlignCenter)
            output_grid.addWidget(ch_label, 6, i, alignment=Qt.AlignCenter)

            self.widgets["output_gain"].append(gain_slider)
            self.widgets["output_link"].append(link_cb)
            self.widgets["output_meter"].append(meter_bar)

        channels_layout.addWidget(output_group)
        main_layout.addLayout(channels_layout)

        # --- OSC Log ---
        log_group = QGroupBox("OSC Message Log (Incoming from oscmix)")
        log_layout = QVBoxLayout()
        self.osc_log = QTextEdit()
        self.osc_log.setReadOnly(True)
        self.osc_log.setMaximumHeight(100)
        self.osc_log.setToolTip(
            "Shows raw OSC messages received from the oscmix server"
        )
        log_layout.addWidget(self.osc_log)
        log_group.setLayout(log_layout)
        main_layout.addWidget(log_group)

        # --- Status Bar ---
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

    def connect_signals(self):
        """Connects Qt signals from widgets and OSC handler to corresponding slots."""
        # OSC Handler -> GUI Updates
        self.osc_handler.connected.connect(self.on_osc_connected)
        self.osc_handler.disconnected.connect(self.on_osc_disconnected)
        self.osc_handler.message_received_signal.connect(self.log_osc_message)
        self.osc_handler.meter_received.connect(self.on_meter_received)
        self.osc_handler.gain_received.connect(self.on_gain_received)
        self.osc_handler.link_received.connect(self.on_link_received)

        # GUI -> OSC Sending
        self.connect_button.clicked.connect(self.connect_osc)
        self.disconnect_button.clicked.connect(self.disconnect_osc)

        # Connect sliders and checkboxes
        for i in range(NUM_INPUT_CHANNELS):
            self.widgets["input_gain"][i].valueChanged.connect(self.send_gain_from_gui)
            if (i % 2) == 0:
                self.widgets["input_link"][i].stateChanged.connect(
                    self.send_link_from_gui
                )
        for i in range(NUM_OUTPUT_CHANNELS):
            self.widgets["output_gain"][i].valueChanged.connect(self.send_gain_from_gui)
            if (i % 2) == 0:
                self.widgets["output_link"][i].stateChanged.connect(
                    self.send_link_from_gui
                )

    # --- Slots for Handling GUI Actions and OSC Events ---

    @Slot()
    def connect_osc(self):
        """Initiates connection to the oscmix server when Connect button is clicked."""
        ip = self.ip_input.text().strip()
        try:
            send_port = int(self.port_input.text())
            listen_port = int(self.listen_port_input.text())
            if not (0 < send_port < 65536 and 0 < listen_port < 65536):
                raise ValueError("Port numbers must be between 1 and 65535")
        except ValueError as e:
            QMessageBox.warning(self, "Input Error", f"Invalid port number: {e}")
            return

        self.status_bar.showMessage(
            f"Attempting connection to oscmix at {ip}:{send_port}..."
        )
        self.log_osc_message(
            "[GUI]",
            [
                f"Connecting to oscmix @ {ip}:{send_port}...",
                f"Listening on {self.osc_handler.listen_ip}:{listen_port}",
            ],
        )
        self.connection_status_label.setText("CONNECTING...")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: orange;")
        self.osc_handler.listen_port = listen_port

        if not self.osc_handler.start(ip, send_port):
            self.status_bar.showMessage("Connection Failed (Check Console/Log)")
            self.connection_status_label.setText("FAILED")
            self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")

    @Slot()
    def disconnect_osc(self):
        """Stops the OSC handler when Disconnect button is clicked."""
        self.status_bar.showMessage("Disconnecting...")
        self.log_osc_message("[GUI]", ["Disconnecting OSC Handler."])
        self.osc_handler.stop()

    @Slot()
    def on_osc_connected(self):
        """Slot called when the OSC handler successfully connects."""
        status_msg = f"Connected to oscmix ({self.osc_handler.target_ip}:{self.osc_handler.target_port}) | GUI Listening on {self.osc_handler.listen_port}"
        self.status_bar.showMessage(status_msg)
        self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("CONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: green;")
        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)
        self.ip_input.setEnabled(False)
        self.port_input.setEnabled(False)
        self.listen_port_input.setEnabled(False)
        self.meter_timer.start()
        # self.query_initial_state() # Optional: Query state if oscmix supports it

    @Slot(str)
    def on_osc_disconnected(self, reason):
        """Slot called when the OSC handler disconnects."""
        status_msg = f"Disconnected: {reason}"
        self.status_bar.showMessage(status_msg)
        self.log_osc_message("[GUI]", [status_msg])
        self.connection_status_label.setText("DISCONNECTED")
        self.connection_status_label.setStyleSheet("font-weight: bold; color: red;")
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.ip_input.setEnabled(True)
        self.port_input.setEnabled(True)
        self.listen_port_input.setEnabled(True)
        self.meter_timer.stop()
        for meter_list in [self.widgets["input_meter"], self.widgets["output_meter"]]:
            for meter in meter_list:
                meter.setValue(0)
        self.last_meter_values["input"].clear()
        self.last_meter_values["output"].clear()

    @Slot(str, list)
    def log_osc_message(self, address, args):
        """Appends details of incoming OSC messages to the log view."""
        if self.osc_log.document().blockCount() > 200:
            cursor = self.osc_log.textCursor()
            cursor.movePosition(cursor.Start)
            cursor.select(cursor.BlockUnderCursor)
            cursor.removeSelectedText()
            cursor.deleteChar()

        self.osc_log.append(f"{address} {args}")
        self.osc_log.verticalScrollBar().setValue(
            self.osc_log.verticalScrollBar().maximum()
        )

    @Slot(str, int, float)
    def on_meter_received(self, type, ch_idx, value):
        """Stores received meter value (first float from /level message)."""
        self.last_meter_values[type][ch_idx] = value

    @Slot()
    def update_gui_meters(self):
        """Periodically updates meter progress bars based on stored values."""
        for ch_idx, value in self.last_meter_values["input"].items():
            if 0 <= ch_idx < len(self.widgets["input_meter"]):
                self.widgets["input_meter"][ch_idx].setValue(
                    float_to_slider_linear(value)
                )
        for ch_idx, value in self.last_meter_values["output"].items():
            if 0 <= ch_idx < len(self.widgets["output_meter"]):
                self.widgets["output_meter"][ch_idx].setValue(
                    float_to_slider_linear(value)
                )
        self.last_meter_values["input"].clear()
        self.last_meter_values["output"].clear()

    @Slot(str, int, float)
    def on_gain_received(self, type, ch_idx, value_db):
        """Updates gain/volume slider when a message is received from oscmix."""
        if type == "input":
            slider_list = self.widgets["input_gain"]
            if slider_list and 0 <= ch_idx < len(slider_list):
                slider = slider_list[ch_idx]
                slider.blockSignals(True)
                slider.setValue(input_gain_to_slider(value_db))
                slider.blockSignals(False)
        elif type == "output":
            slider_list = self.widgets["output_gain"]
            if slider_list and 0 <= ch_idx < len(slider_list):
                slider = slider_list[ch_idx]
                slider.blockSignals(True)
                slider.setValue(output_volume_to_slider(value_db))
                slider.blockSignals(False)

    @Slot(str, int, bool)
    def on_link_received(self, type, ch_idx, value):
        """Updates link checkbox when a message is received from oscmix."""
        cb_list = self.widgets.get(f"{type}_link")
        if cb_list and 0 <= ch_idx < len(cb_list) and (ch_idx % 2 == 0):
            cb = cb_list[ch_idx]
            cb.blockSignals(True)
            cb.setChecked(value)
            cb.blockSignals(False)
            self._update_linked_channel_state(type, ch_idx, value)

    @Slot(int)
    def send_gain_from_gui(self, slider_value):
        """Sends OSC gain/volume message when a GUI slider value changes."""
        sender_widget = self.sender()
        if not sender_widget:
            return
        obj_name = sender_widget.objectName()
        try:
            type, gain_or_vol, idx_str = obj_name.split("_")
            ch_idx = int(idx_str)
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}")
            return

        ch_num_1_based = ch_idx + 1

        if type == "input":
            osc_address = OSC_INPUT_GAIN.format(ch=ch_num_1_based)
            float_value_db = slider_to_input_gain(slider_value)
        elif type == "output":
            osc_address = OSC_OUTPUT_VOLUME.format(ch=ch_num_1_based)
            float_value_db = slider_to_output_volume(slider_value)
        else:
            return

        self.osc_handler.send(osc_address, float_value_db)

    @Slot(int)
    def send_link_from_gui(self, state):
        """Sends OSC link message when a GUI checkbox state changes."""
        sender_widget = self.sender()
        if not sender_widget:
            return
        obj_name = sender_widget.objectName()
        try:
            type, _, idx_str = obj_name.split("_")
            ch_idx = int(idx_str)
        except Exception as e:
            print(f"Error parsing sender object name '{obj_name}': {e}")
            return

        bool_value = state == Qt.Checked
        osc_value = 1 if bool_value else 0
        ch_num_1_based = ch_idx + 1

        if type == "input":
            osc_address = OSC_INPUT_STEREO.format(ch=ch_num_1_based)
        elif type == "output":
            osc_address = OSC_OUTPUT_STEREO.format(ch=ch_num_1_based)
        else:
            return

        self.osc_handler.send(osc_address, osc_value)
        self._update_linked_channel_state(type, ch_idx, bool_value)

    def _update_linked_channel_state(self, type, odd_ch_idx, is_linked):
        """Enables/disables the slider of the paired (even) channel."""
        even_ch_idx = odd_ch_idx + 1
        gain_list = self.widgets.get(f"{type}_gain")
        if gain_list and 0 <= even_ch_idx < len(gain_list):
            gain_list[even_ch_idx].setEnabled(not is_linked)

    def closeEvent(self, event):
        """Ensures the OSC handler is stopped cleanly when the GUI window is closed."""
        print("[GUI] Closing application...")
        self.meter_timer.stop()
        self.osc_handler.stop()
        event.accept()


# --- Main execution function ---
def run():
    """Creates the application instance and runs the main GUI window."""
    app = QApplication(sys.argv)
    window = OscmixControlGUI()
    window.show()
    sys.exit(app.exec())


# --- Entry point for direct execution ---
if __name__ == "__main__":
    run()
