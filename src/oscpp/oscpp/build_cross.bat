@echo off
setlocal enabledelayedexpansion

REM Cross-compilation build script for Intel oneAPI
echo Intel oneAPI Cross-Compilation Build Script
echo ==========================================

REM Check for Intel oneAPI environment
if not defined ONEAPI_ROOT (
    echo Intel oneAPI environment not detected.
    echo Please run the setvars.bat script from your Intel oneAPI installation first.
    echo Typically: "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
    exit /b 1
)

SET TARGET=%1
SET BUILD_TYPE=%2

if "%BUILD_TYPE%"=="" (
    SET BUILD_TYPE=Release
)

if "%TARGET%"=="" (
    echo No target specified. Building for all platforms...
    call :build_windows
    call :build_linux
    call :build_arm64
    call :build_macos
    goto :end
)

if /i "%TARGET%"=="windows" (
    call :build_windows
    goto :end
)

if /i "%TARGET%"=="linux" (
    call :build_linux
    goto :end
)

if /i "%TARGET%"=="arm64" (
    call :build_arm64
    goto :end
)

if /i "%TARGET%"=="macos" (
    call :build_macos
    goto :end
)

echo Unknown target: %TARGET%
echo Available targets: windows, linux, arm64, macos, or leave blank for all
exit /b 1

:build_windows
echo.
echo Building for Windows (x86_64)...
echo -------------------------------
if not exist build_intel mkdir build_intel
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi.cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -B build_intel .
if %ERRORLEVEL% neq 0 (
    echo CMake configuration for Windows failed
    exit /b %ERRORLEVEL%
)
cmake --build build_intel --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo Build for Windows failed
    exit /b %ERRORLEVEL%
)
cmake --build build_intel --config %BUILD_TYPE% --target examples
if %ERRORLEVEL% neq 0 (
    echo Examples build for Windows failed
    exit /b %ERRORLEVEL%
)
echo Windows build completed successfully.
exit /b 0

:build_linux
echo.
echo Building for Linux (x86_64)...
echo -----------------------------
if not exist build_linux_x64 mkdir build_linux_x64
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_linux.cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -B build_linux_x64 .
if %ERRORLEVEL% neq 0 (
    echo CMake configuration for Linux failed
    exit /b %ERRORLEVEL%
)
cmake --build build_linux_x64 --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo Build for Linux failed
    exit /b %ERRORLEVEL%
)
cmake --build build_linux_x64 --config %BUILD_TYPE% --target examples
if %ERRORLEVEL% neq 0 (
    echo Examples build for Linux failed
    exit /b %ERRORLEVEL%
)
echo Linux build completed successfully.
exit /b 0

:build_arm64
echo.
echo Building for ARM64...
echo -------------------
if not exist build_arm64 mkdir build_arm64
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_arm64.cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -B build_arm64 .
if %ERRORLEVEL% neq 0 (
    echo CMake configuration for ARM64 failed
    exit /b %ERRORLEVEL%
)
cmake --build build_arm64 --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo Build for ARM64 failed
    exit /b %ERRORLEVEL%
)
cmake --build build_arm64 --config %BUILD_TYPE% --target examples
if %ERRORLEVEL% neq 0 (
    echo Examples build for ARM64 failed
    exit /b %ERRORLEVEL%
)
echo ARM64 build completed successfully.
exit /b 0

:build_macos
echo.
echo Building for macOS...
echo ------------------
if not exist build_macos mkdir build_macos
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/intel_oneapi_cross_macos.cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -B build_macos .
if %ERRORLEVEL% neq 0 (
    echo CMake configuration for macOS failed
    exit /b %ERRORLEVEL%
)
cmake --build build_macos --config %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo Build for macOS failed
    exit /b %ERRORLEVEL%
)
cmake --build build_macos --config %BUILD_TYPE% --target examples
if %ERRORLEVEL% neq 0 (
    echo Examples build for macOS failed
    exit /b %ERRORLEVEL%
)
echo macOS build completed successfully.
exit /b 0

:end
echo.
echo Cross-compilation completed.
endlocal
