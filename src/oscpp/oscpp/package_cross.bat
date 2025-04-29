@echo off
setlocal enabledelayedexpansion

REM Cross-platform packaging script for Intel oneAPI
echo Intel oneAPI Cross-Platform Packaging Script
echo ============================================

REM Check for Intel oneAPI environment
if not defined ONEAPI_ROOT (
    echo Intel oneAPI environment not detected.
    echo Please run the setvars.bat script from your Intel oneAPI installation first.
    echo Typically: "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
    exit /b 1
)

SET TARGET=%1

if "%TARGET%"=="" (
    echo No target specified. Packaging for all platforms...
    call :package_windows
    call :package_linux
    call :package_arm64
    call :package_macos
    goto :end
)

if /i "%TARGET%"=="windows" (
    call :package_windows
    goto :end
)

if /i "%TARGET%"=="linux" (
    call :package_linux
    goto :end
)

if /i "%TARGET%"=="arm64" (
    call :package_arm64
    goto :end
)

if /i "%TARGET%"=="macos" (
    call :package_macos
    goto :end
)

echo Unknown target: %TARGET%
echo Available targets: windows, linux, arm64, macos, or leave blank for all
exit /b 1

:package_windows
echo.
echo Packaging for Windows (x86_64)...
echo -------------------------------
if not exist build_intel (
    echo Windows build not found. Build first using build_cross.bat windows
    exit /b 1
)
cd build_intel
cmake -DOSCPP_CREATE_PACKAGE=ON -DOSCPP_PLATFORM=WINDOWS ..
cmake --build . --target package
cd ..
echo Windows packaging completed. Packages available in build_intel directory.
exit /b 0

:package_linux
echo.
echo Packaging for Linux (x86_64)...
echo -----------------------------
if not exist build_linux_x64 (
    echo Linux build not found. Build first using build_cross.bat linux
    exit /b 1
)
cd build_linux_x64
cmake -DOSCPP_CREATE_PACKAGE=ON -DOSCPP_PLATFORM=LINUX ..
cmake --build . --target package
cd ..
echo Linux packaging completed. Packages available in build_linux_x64 directory.
exit /b 0

:package_arm64
echo.
echo Packaging for ARM64...
echo -------------------
if not exist build_arm64 (
    echo ARM64 build not found. Build first using build_cross.bat arm64
    exit /b 1
)
cd build_arm64
cmake -DOSCPP_CREATE_PACKAGE=ON -DOSCPP_PLATFORM=ARM64 ..
cmake --build . --target package
cd ..
echo ARM64 packaging completed. Packages available in build_arm64 directory.
exit /b 0

:package_macos
echo.
echo Packaging for macOS...
echo ------------------
if not exist build_macos (
    echo macOS build not found. Build first using build_cross.bat macos
    exit /b 1
)
cd build_macos
cmake -DOSCPP_CREATE_PACKAGE=ON -DOSCPP_PLATFORM=MACOS ..
cmake --build . --target package
cd ..
echo macOS packaging completed. Packages available in build_macos directory.
exit /b 0

:end
echo.
echo Cross-platform packaging completed.
endlocal
