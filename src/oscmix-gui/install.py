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
            venv.create(venv_dir_path, with_pip=True)
            print("Virtual environment created successfully.")
        except Exception as e:
            print(f"ERROR: Failed to create virtual environment: {e}")
            sys.exit(1)


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
    venv_exec_paths = get_venv_paths(venv_path)

    # Verify essential venv executables exist
    if not os.path.exists(venv_exec_paths["python"]):
        print(
            f"ERROR: Python executable not found in venv: {venv_exec_paths['python']}"
        )
        sys.exit(1)
    if not os.path.exists(venv_exec_paths["pip"]):
        print(f"ERROR: Pip executable not found in venv: {venv_exec_paths['pip']}")
        # Attempt upgrade? Might be complex if pip itself is broken.
        # Let's try upgrading pip using the venv's python first.
        print("Attempting to ensure pip is installed/upgraded...")
        run_command(
            ["python", "-m", "ensurepip", "--upgrade"], venv_paths=venv_exec_paths
        )
        # Re-check after ensurepip
        if not os.path.exists(venv_exec_paths["pip"]):
            print(f"ERROR: Pip still not found after ensurepip attempt.")
            sys.exit(1)

    # 3. Install Requirements
    print(f"\n--- Installing dependencies from {REQUIREMENTS_FILE} ---")
    requirements_path = os.path.join(project_root, REQUIREMENTS_FILE)
    if not os.path.exists(requirements_path):
        print(f"ERROR: '{REQUIREMENTS_FILE}' not found in {project_root}")
        sys.exit(1)
    # Use the pip executable from the virtual environment
    run_command(["pip", "install", "-r", requirements_path], venv_paths=venv_exec_paths)
    print("Dependencies installed successfully.")

    # 4. Install the oscmix-gui package itself
    print(f"\n--- Installing oscmix-gui package ---")
    setup_py_path = os.path.join(project_root, "setup.py")
    if not os.path.exists(setup_py_path):
        print(f"ERROR: 'setup.py' not found in {project_root}")
        sys.exit(1)
    # Use pip to install the package defined by setup.py in the current directory (.)
    run_command(["pip", "install", "."], cwd=project_root, venv_paths=venv_exec_paths)
    print("oscmix-gui installed successfully.")

    # 5. Print Final Instructions
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
