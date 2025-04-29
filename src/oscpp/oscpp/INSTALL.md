# Installing the OSCPP Library

This document explains how to install the OSCPP library, including packaging and installing cross-compiled builds for different platforms.

## Installation from Source

### Standard Installation

After [building the library](BUILD.md), you can install it using CMake:

```bash
# On Windows (from the build directory)
cmake --install build_intel

# On Linux (from the build directory)
cmake --install build_linux_x64

# On macOS (from the build directory)
cmake --install build_macos

# On ARM64 Linux (from the build directory)
cmake --install build_arm64
```

By default, this will install to the system-appropriate location. You can specify a custom installation directory:

```bash
cmake --install build_intel --prefix C:/oscpp
```

## Creating Installable Packages

The project supports creating installable packages for different platforms.

### Using CMake Presets (Recommended)

The most streamlined way to create packages is using the CMake presets:

1. Configure the package preset:

   ```batch
   cmake --preset package
   ```

2. Build the package:

   ```batch
   cmake --build --preset package --target package
   ```

For cross-platform packages, you can use a more specific configuration:

```batch
# First configure with package option enabled
cmake --preset intel-cross-linux-release -DOSCPP_CREATE_PACKAGE=ON
# Then build the package
cmake --build --preset intel-cross-linux-release --target package
```

### Using Visual Studio Code Tasks

1. First, build the project (see [BUILD.md](BUILD.md))
2. Then, use one of the following packaging tasks:

   - **Package Windows (Intel oneAPI)** - Creates Windows installers and ZIP packages
   - **Package Linux (Cross-Compile)** - Creates Linux DEB, RPM, and TGZ packages
   - **Package ARM64 (Cross-Compile)** - Creates ARM64 DEB, RPM, and TGZ packages
   - **Package macOS (Cross-Compile)** - Creates macOS DMG and TGZ packages
   - **Package All Platforms** - Creates packages for all platforms in sequence

### Using Command Line

The project includes a batch script for package creation:

```batch
# Set up Intel oneAPI environment first
"C:\Program Files (x86)\Intel\oneAPI\setvars.bat"

# Package for all platforms
package_cross.bat

# Package for a specific platform (windows, linux, arm64, macos)
package_cross.bat linux
```

### Using CMake Directly

You can also use CMake directly to create packages:

```batch
# Windows packages
cmake -DOSCPP_CREATE_PACKAGE=ON -B build_intel .
cmake --build build_intel --target package

# Linux packages
cmake -DOSCPP_CREATE_PACKAGE=ON -B build_linux_x64 .
cmake --build build_linux_x64 --target package

# ARM64 packages
cmake -DOSCPP_CREATE_PACKAGE=ON -B build_arm64 .
cmake --build build_arm64 --target package

# macOS packages
cmake -DOSCPP_CREATE_PACKAGE=ON -B build_macos .
cmake --build build_macos --target package
```

## Installing from Packages

### Windows

#### Installing from ZIP Package

1. Extract the `oscpp-1.0.0-win64.zip` file to a location of your choice
2. The extracted folder contains:
   - `bin/` - Executables (client_example.exe, server_example.exe)
   - `lib/` - Library files (oscpp.dll, oscpp.lib)
   - `include/` - Header files for development
   - `share/` - Documentation and config files

#### Installing from NSIS Installer

1. Run the `oscpp-1.0.0-win64.exe` installer
2. Follow the installation prompts
3. The installer will:
   - Install the library and executables
   - Add the installation directory to your PATH (optional)
   - Create shortcuts in the Start Menu (optional)

### Linux (x86_64 and ARM64)

#### Installing from DEB Package

```bash
# Copy the .deb file to your Linux system
sudo dpkg -i oscpp-1.0.0-Linux.deb
# Or
sudo apt install ./oscpp-1.0.0-Linux.deb
```

#### Installing from RPM Package

```bash
# Copy the .rpm file to your Linux system
sudo rpm -ivh oscpp-1.0.0-Linux.rpm
# Or
sudo dnf install oscpp-1.0.0-Linux.rpm
```

#### Installing from TGZ Archive

```bash
# Copy the .tar.gz file to your Linux system
tar -xzf oscpp-1.0.0-Linux.tar.gz -C /opt
# Add to your PATH (add to ~/.bashrc for persistence)
export PATH=$PATH:/opt/oscpp-1.0.0/bin
```

### macOS

#### Installing from DMG File

1. Copy the .dmg file to your macOS system
2. Double-click to mount it
3. Drag the oscpp application to your Applications folder
4. Optionally, create symlinks to the command-line tools:

   ```bash
   ln -s /Applications/oscpp.app/Contents/MacOS/client_example /usr/local/bin/
   ```

#### Installing from TGZ Archive

```bash
# Copy the .tar.gz file to your macOS system
tar -xzf oscpp-1.0.0-Darwin.tar.gz -C /Applications
# Add to your PATH (add to ~/.bash_profile for persistence)
export PATH=$PATH:/Applications/oscpp-1.0.0/bin
```

## Using the Installed Library

### Using with CMake Projects

After installing the library, you can use it in your own CMake projects:

```cmake
# Find the OSCPP package
find_package(oscpp REQUIRED)

# Create your application
add_executable(myapp main.cpp)

# Link against the OSCPP library
target_link_libraries(myapp PRIVATE osc::oscpp)
```

### Using with pkg-config (Linux)

On Linux platforms, you can use pkg-config:

```bash
g++ -o myapp main.cpp $(pkg-config --cflags --libs oscpp)
```

### Using with Visual Studio (Windows)

1. Add the include directory to your project settings:
   - `C:\Program Files (x86)\OSCPP\include`
2. Add the library directory to your project settings:
   - `C:\Program Files (x86)\OSCPP\lib`
3. Add `oscpp.lib` to your linker input
4. Make sure `oscpp.dll` is in your PATH or copy it to your executable directory

## Running the Applications

### Windows

```batch
# If installed via MSI/NSIS installer and added to PATH
client_example
server_example

# If manually extracted
C:\path\to\oscpp\bin\client_example.exe
C:\path\to\oscpp\bin\server_example.exe
```

### Linux (x86_64 and ARM64)

```bash
# If installed via package manager
client_example
server_example

# If manually installed
/opt/oscpp-1.0.0/bin/client_example
/opt/oscpp-1.0.0/bin/server_example
```

### macOS

```bash
# If installed via DMG and symlinked
client_example
server_example

# Otherwise
/Applications/oscpp-1.0.0/bin/client_example
/Applications/oscpp-1.0.0/bin/server_example
```

## Uninstalling

### Windows

1. Use the "Add or Remove Programs" feature in Windows Settings
2. Find "OSCPP Library" and click "Uninstall"

### Linux (via package manager)

```bash
# Debian/Ubuntu
sudo apt remove oscpp

# RedHat/Fedora
sudo dnf remove oscpp
```

### Manual Uninstallation

If you installed manually, simply delete the installation directory:

```bash
# Windows
rmdir /s /q C:\path\to\oscpp

# Linux/macOS
rm -rf /path/to/oscpp
```

## Verifying Cross-Compiled Packages

To verify that your applications are correctly built for each platform:

### Windows

Check executable file type:

```batch
file client_example.exe
# Should show "PE32+ executable (console) x86-64, for MS Windows"
```

### Linux x86_64

Check binary format:

```bash
file client_example
# Should show "ELF 64-bit LSB executable, x86-64, ..."
```

### ARM64

Check binary format:

```bash
file client_example
# Should show "ELF 64-bit LSB executable, ARM aarch64, ..."
```

### macOS

Check binary format:

```bash
file client_example
# Should show "Mach-O 64-bit executable x86_64" or "universal binary"
```
