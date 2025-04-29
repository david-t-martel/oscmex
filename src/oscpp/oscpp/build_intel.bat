@echo off
setlocal

REM Build script for Intel oneAPI compiler
echo Building with Intel oneAPI compiler...

REM Check for Intel oneAPI environment
if not defined ONEAPI_ROOT (
    echo Intel oneAPI environment not detected.
    echo Please run the setvars.bat script from your Intel oneAPI installation first.
    echo Typically: "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
    exit /b 1
)

REM Create build directory
if not exist build_intel mkdir build_intel
cd build_intel

REM Configure with CMake using Intel toolchain
cmake -G "Ninja" ^
      -DCMAKE_TOOLCHAIN_FILE=../cmake/intel_oneapi.cmake ^
      -DCMAKE_BUILD_TYPE=Release ^
      ..

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM Build the project
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
echo Binaries can be found in the build_intel directory.

cd ..
endlocal
