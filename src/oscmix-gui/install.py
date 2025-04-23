#!/usr/bin/env python3
# install.py
# Automates the setup of the oscmix-gui virtual environment and installation.

import os
import platform
import subprocess
import sys
import venv

# --- Configuration ---
VENV_DIR = ".venv"  # Name of the virtual environment directory
REQUIREMENTS_FILE = "requirements.txt"
PYTHON_MIN_VERSION = (3, 7)

# --- Helper Functions ---


def check_python_version():
    """Checks if the current Python version meets the minimum requirement."""
    print(
        f"--- Checking Python version ({'.'.join(map(str, PYTHON_MIN_VERSION))}+ required)..."
    )
    if sys.version_info < PYTHON_MIN_VERSION:
        print(
            f"ERROR: Python {'.'.join(map(str, PYTHON_MIN_VERSION))} or later is required."
        )
        print(f"       You are using Python {platform.python_version()}")
        sys.exit(1)
    print(f"Python version {platform.python_version()} is sufficient.")
    return True


def get_venv_paths(venv_dir_path):
    """Gets platform-specific paths for executables within the venv."""
    paths = {}
    if platform.system() == "Windows":
        paths["python"] = os.path.join(venv_dir_path, "Scripts", "python.exe")
        paths["pip"] = os.path.join(venv_dir_path, "Scripts", "pip.exe")
        paths["activate"] = os.path.join(
            venv_dir_path, "Scripts", "activate"
        )  # General activate script path
    else:  # Linux/macOS
        paths["python"] = os.path.join(venv_dir_path, "bin", "python")
        paths["pip"] = os.path.join(venv_dir_path, "bin", "pip")
        paths["activate"] = os.path.join(venv_dir_path, "bin", "activate")
    return paths


def create_virtual_environment(venv_dir_path):
    """Creates the virtual environment if it doesn't exist."""
    print(f"--- Checking for virtual environment '{VENV_DIR}'...")
    if os.path.exists(venv_dir_path) and os.path.isdir(venv_dir_path):
        print(f"Virtual environment '{VENV_DIR}' already exists.")
        # Optional: Could add logic here to check if it's usable or needs update
    else:
        print(f"Creating virtual environment '{VENV_DIR}'...")
        try:
            # Using subprocess instead of venv module directly to ensure proper creation
            subprocess.run(
                [sys.executable, "-m", "venv", "--system-site-packages", venv_dir_path],
                check=True,
                capture_output=True,
                text=True,
            )
            print("Virtual environment created successfully.")
        except Exception as e:
            print(f"ERROR: Failed to create virtual environment: {e}")
            sys.exit(1)


def verify_venv(venv_path):
    """Verify that the virtual environment was created correctly."""
    venv_exec_paths = get_venv_paths(venv_path)

    # Check if Python exists in the venv
    if not os.path.exists(venv_exec_paths["python"]):
        print(f"Warning: Python executable not found at {venv_exec_paths['python']}")
        # Try to find it in a different location
        if platform.system() == "Windows":
            alt_python = os.path.join(venv_path, "python.exe")
            if os.path.exists(alt_python):
                print(f"Found Python at alternate location: {alt_python}")
                venv_exec_paths["python"] = alt_python
            else:
                # Windows Python 3.11+ might use a different structure
                # Try creating again with system Python directly
                print("Attempting to recreate virtual environment...")
                subprocess.run(
                    [sys.executable, "-m", "venv", venv_path],
                    check=True,
                    capture_output=True,
                    text=True,
                )
                if not os.path.exists(venv_exec_paths["python"]):
                    print(f"ERROR: Could not create a working virtual environment.")
                    print(
                        f"Please try creating it manually with: python -m venv {VENV_DIR}"
                    )
                    sys.exit(1)

    # If we get here, Python exists in the venv
    return venv_exec_paths


def run_command(command, check=True, cwd=None, venv_paths=None):
    """Runs a command using subprocess, optionally checking for errors."""
    print(f"Running command: {' '.join(command)}")
    try:
        # Use the venv's Python/pip if provided, otherwise rely on system PATH
        # (This assumes install.py might be run by system python before venv exists)
        executable = command[0]
        if venv_paths:
            if (
                command[0] == "pip"
                and "pip" in venv_paths
                and os.path.exists(venv_paths["pip"])
            ):
                executable = venv_paths["pip"]
            elif (
                command[0] == "python"
                and "python" in venv_paths
                and os.path.exists(venv_paths["python"])
            ):
                executable = venv_paths["python"]

        process = subprocess.run(
            [executable] + command[1:],
            check=check,
            cwd=cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        print(process.stdout)
        if process.stderr:
            print("--- Command stderr ---", file=sys.stderr)
            print(process.stderr, file=sys.stderr)
            print("----------------------", file=sys.stderr)
        if check and process.returncode != 0:
            print(f"ERROR: Command failed with exit code {process.returncode}")
            # Exit handled by subprocess check=True raising CalledProcessError
        return True
    except FileNotFoundError:
        print(f"ERROR: Command not found: {command[0]}. Is it installed or in PATH?")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Command failed: {' '.join(command)}")
        # Error output was already printed above
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: An unexpected error occurred while running command: {e}")
        sys.exit(1)


# --- Main Installation Logic ---


def main():
    """Main function to orchestrate the installation."""
    print("Starting oscmix-gui setup...")
    project_root = os.path.abspath(os.path.dirname(__file__))
    print(f"Project root directory: {project_root}")

    # 1. Check Python Version
    check_python_version()

    # 2. Create Virtual Environment
    venv_path = os.path.join(project_root, VENV_DIR)
    create_virtual_environment(venv_path)

    # 3. Verify virtual environment was created correctly
    venv_exec_paths = verify_venv(venv_path)

    # Check for pip in the virtual environment
    if not os.path.exists(venv_exec_paths["pip"]):
        print(f"WARNING: Pip executable not found in venv: {venv_exec_paths['pip']}")
        # Attempt to ensure pip is installed
        print("Attempting to ensure pip is installed...")
        run_command([venv_exec_paths["python"], "-m", "ensurepip", "--upgrade"])

        # If that doesn't work, try installing pip directly
        if not os.path.exists(venv_exec_paths["pip"]):
            print("Attempting to install pip directly...")
            run_command(
                [venv_exec_paths["python"], "-m", "pip", "install", "--upgrade", "pip"]
            )

        # Re-check after attempts
        if not os.path.exists(venv_exec_paths["pip"]):
            print(f"ERROR: Unable to install pip in the virtual environment.")
            print(f"Please try installing manually after activating the environment.")
            sys.exit(1)

    # 4. Install Requirements
    print(f"\n--- Installing dependencies from {REQUIREMENTS_FILE} ---")
    requirements_path = os.path.join(project_root, REQUIREMENTS_FILE)
    if not os.path.exists(requirements_path):
        print(f"ERROR: '{REQUIREMENTS_FILE}' not found in {project_root}")
        sys.exit(1)

    # Use the python executable from the virtual environment to run pip
    run_command(
        [venv_exec_paths["python"], "-m", "pip", "install", "-r", requirements_path]
    )
    print("Dependencies installed successfully.")

    # 5. Install the oscmix-gui package itself
    print(f"\n--- Installing oscmix-gui package ---")
    setup_py_path = os.path.join(project_root, "setup.py")
    if not os.path.exists(setup_py_path):
        print(f"ERROR: 'setup.py' not found in {project_root}")
        sys.exit(1)

    # Use python from venv to run pip install
    run_command(
        [venv_exec_paths["python"], "-m", "pip", "install", "."], cwd=project_root
    )
    print("oscmix-gui installed successfully.")

    # 6. Print Final Instructions
    print("\n--- Setup Complete ---")
    print("To run the application:")
    print("1. Activate the virtual environment:")
    if platform.system() == "Windows":
        print(
            f"   In Command Prompt: cd {project_root} && {VENV_DIR}\\Scripts\\activate.bat"
        )
        print(
            f"   In PowerShell:   cd {project_root} ; .\\{VENV_DIR}\\Scripts\\Activate.ps1"
        )
    else:
        print(f"   In bash/zsh: cd {project_root} && source {VENV_DIR}/bin/activate")
    print(
        "   (Your terminal prompt should change to indicate the active environment, e.g., '(.venv)')"
    )
    print("2. Ensure the 'oscmix' C application is running on the Linux host.")
    print("3. Run the GUI command:")
    print("   oscmix-gui")
    print("4. Use the 'Connect' button in the GUI, ensuring the IP/Ports are correct.")
    print("5. To deactivate the virtual environment later, simply type: deactivate")


if __name__ == "__main__":
    main()
