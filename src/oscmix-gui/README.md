# oscmix GUI Control

A Python-based GUI using PySide6 to control an RME audio interface via the `oscmix` server running on a Linux host.

## Prerequisites

1. **Python:** Python 3.7 or later installed.
2. **`oscmix` Server:** The `michaelforney/oscmix` program must be compiled and running on the Linux computer connected to your RME UCX II (or other supported RME interface). Make note of the IP address and port the `oscmix` server is listening on (e.g., `127.0.0.1:8000`).
3. **`oscmix` Configuration:** Ensure `oscmix` is configured to send replies/status updates (like meter levels) back to the IP address and port where this GUI application will listen (default is port 9001 on all interfaces of the machine running the GUI).

## Installation

It is highly recommended to install this application within a Python virtual environment.

1. **Clone the Repository (Optional):**
    If you have the code in a Git repository:

    ```bash
    git clone <your-repo-url>
    cd oscmix-gui
    ```

    Otherwise, ensure you have the `oscmix_gui` directory, `setup.py`, `requirements.txt`, and this `README.md` in a common parent directory (let's call it `oscmix-gui-project`).

2. **Create and Activate Virtual Environment:**
    Navigate to the `oscmix-gui-project` directory in your terminal.

    ```bash
    # Create the virtual environment (e.g., named .venv)
    python -m venv .venv

    # Activate the virtual environment
    # On Linux/macOS:
    source .venv/bin/activate
    # On Windows (cmd.exe):
    # .venv\Scripts\activate.bat
    # On Windows (PowerShell):
    # .venv\Scripts\Activate.ps1
    ```

    You should see `(.venv)` at the beginning of your terminal prompt.

3. **Install the GUI Application:**
    While inside the activated virtual environment and in the `oscmix-gui-project` directory (where `setup.py` is located):

    ```bash
    pip install .
    ```

    This command reads `setup.py`, installs the dependencies listed in `requirements.txt` (`PySide6`, `python-osc`), and installs the `oscmix-gui` package itself, including the command-line entry point.

## Running the GUI

1. **Ensure `oscmix` is Running:** Start the `oscmix` server on the Linux host connected to the RME interface.
2. **Activate Virtual Environment:** If not already active, activate the virtual environment:

    ```bash
    # On Linux/macOS:
    source .venv/bin/activate
    # On Windows: (use appropriate command)
    ```

3. **Run the GUI Command:**
    Simply type the command created during installation:

    ```bash
    oscmix-gui
    ```

4. **Connect:** In the GUI window:
    * Verify the IP address and Port for the running `oscmix` server.
    * Verify the "GUI Listen Port" (ensure it matches where `oscmix` is sending replies).
    * Click "Connect".

## Development

To install for development (allowing you to edit the code without reinstalling):

```bash
pip install -e .
TroubleshootingConnection Failed:Verify oscmix is running and accessible from the machine running the GUI (check IP, port, firewalls).Verify the "GUI Listen Port" is correct and not blocked by a firewall.Check the terminal output of both oscmix and oscmix-gui for error messages.No Controls Working / No Meters:Double-check the OSC address paths defined at the top of `oscmix_

oscmix-gui-project/
├── oscmix_gui/          # This is the actual Python package
│   ├── __init__.py      # Empty file, makes 'oscmix_gui' a package
│   └── main.py          # The GUI code (from the first artifact)
├── setup.py             # The setup script (from the third artifact)
├── requirements.txt     # The requirements file (from the second artifact)
└── README.md            # The instructions (from the fourth artifact)
