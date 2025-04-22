@echo off
REM intel_build.bat - Sets up Intel oneAPI environment and launches CMake build

REM Call the Intel oneAPI environment setup script
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat" intel64

REM Use CMake preset to configure and build the project
cmake --preset intel-release
cmake --build --preset intel-release-build

echo.
echo Build complete! Check the output above for any errors.
echo.

pause
