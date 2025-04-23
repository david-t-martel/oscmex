# setup.py
import os

import setuptools


# Function to read requirements from requirements.txt
def parse_requirements(filename="requirements.txt"):
    """Load requirements from a pip requirements file."""
    lineiter = (line.strip() for line in open(filename))
    return [line for line in lineiter if line and not line.startswith("#")]


# Read the contents of the README file
this_directory = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(this_directory, "README.md"), encoding="utf-8") as f:
    long_description = f.read()

setuptools.setup(
    name="oscmix-gui",
    version="0.1.0",  # Initial version
    author="David T. Martel",  # Replace with your name
    author_email="david.martel@auricleinc.com",  # Replace with your email
    description="A PySide6 GUI for controlling the oscmix RME interface controller",
    long_description=long_description,
    long_description_content_type="text/markdown",
    # url="https://github.com/your_username/oscmix-gui",  # Optional: Replace with your repo URL
    # Specify the package directory
    packages=setuptools.find_packages(where="."),  # Finds the 'oscmix_gui' package
    # Define dependencies
    install_requires=parse_requirements(),
    # Define the entry point for the command-line script
    entry_points={
        "console_scripts": [
            "oscmix-gui=oscmix_gui.main:run",  # Creates 'oscmix-gui' command that runs the run() function in main.py
        ],
        # Or use 'gui_scripts' if you want it treated more like a GUI app on Windows (no console window)
        # 'gui_scripts': [
        #     'oscmix-gui=oscmix_gui.main:run',
        # ]
    },
    python_requires=">=3.7",  # Specify minimum Python version compatibility
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",  # Choose your license
        "Operating System :: OS Independent",  # Or specify Linux if it only works there due to oscmix
        "Environment :: X11 Applications :: Qt",
        "Intended Audience :: End Users/Desktop",
        "Topic :: Multimedia :: Sound/Audio :: Mixers",
    ],
    # If you have data files to include (e.g., icons), use include_package_data=True and MANIFEST.in
    # include_package_data=True,
)
