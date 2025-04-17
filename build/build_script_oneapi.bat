@echo off
setlocal enabledelayedexpansion

:: Build Script for AudioEngine with Intel oneAPI (Debug, All Options)

:: --- Default Settings ---
set CLEAN_BUILD=0
set RUN_INSTALL=0
set BUILD_PRESET=intel-debug

:: --- Parse Arguments ---
:arg_loop
if /i "%~1" == "/clean" (
    set CLEAN_BUILD=1
    echo Clean build requested.
    shift
    goto arg_loop
)
if /i "%~1" == "/install" (
    set RUN_INSTALL=1
    echo Install step requested after build.
    shift
    goto arg_loop
)
if /i "%~1" == "/preset" (
    if not "%~2" == "" (
        set BUILD_PRESET=%~2
        echo Build preset set to: %BUILD_PRESET%
        shift
        shift
        goto arg_loop
    ) else (
        echo ERROR: /preset requires an argument.
        goto :usage
    )
)
if /i "%~1" == "/help" (
    goto :usage
)
if not "%~1" == "" (
    echo Unrecognized argument: %~1
    goto :usage
)

:: --- Configuration ---
set ONEAPI_ROOT=C:\Program Files (x86)\Intel\oneAPI
set SETVARS_BAT="%ONEAPI_ROOT%\setvars.bat"
set SOURCE_DIR=%~dp0..
set BUILD_DIR=%SOURCE_DIR%\build\%BUILD_PRESET%

:: --- Generate Timestamped Log Filename ---
for /f "tokens=2 delims==" %%I in ('wmic os get LocalDateTime /value') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,6%
set LOG_FILE=%SOURCE_DIR%\build\build_log_%BUILD_PRESET%_%TIMESTAMP%.txt

:: --- Print Build Info ---
echo Build Configuration:
echo   Source directory: %SOURCE_DIR%
echo   Build directory: %BUILD_DIR%
echo   Build preset: %BUILD_PRESET%
echo   Clean build: %CLEAN_BUILD%
echo   Run install: %RUN_INSTALL%
echo   Log file: %LOG_FILE%
echo.

:: --- Clean Log File (Create New) ---
echo Build started on %DATE% at %TIME% > "%LOG_FILE%"
echo Timestamp: %TIMESTAMP% >> "%LOG_FILE%"
echo Configuration: >> "%LOG_FILE%"
echo   Source Dir: %SOURCE_DIR% >> "%LOG_FILE%"
echo   Build Dir: %BUILD_DIR% >> "%LOG_FILE%"
echo   Build Preset: %BUILD_PRESET% >> "%LOG_FILE%"
echo   Clean Build: %CLEAN_BUILD% >> "%LOG_FILE%"
echo   Run Install: %RUN_INSTALL% >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

:: --- Optional Clean Build ---
if %CLEAN_BUILD% == 1 (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory: %BUILD_DIR% | tee -a "%LOG_FILE%"
        rd /s /q "%BUILD_DIR%" >> "%LOG_FILE%" 2>&1
        if %errorlevel% neq 0 (
            echo WARNING: Failed to completely remove build directory. | tee -a "%LOG_FILE%"
        )
    ) else (
        echo Build directory does not exist, no clean needed. | tee -a "%LOG_FILE%"
    )
)

:: --- Setup oneAPI Environment ---
echo Setting up Intel oneAPI environment... | tee -a "%LOG_FILE%"
if not exist %SETVARS_BAT% (
    echo ERROR: setvars.bat not found at %SETVARS_BAT% | tee -a "%LOG_FILE%"
    goto :error
)
call %SETVARS_BAT% intel64 vs2022 > nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Failed to run setvars.bat. Check oneAPI installation. | tee -a "%LOG_FILE%"
    goto :error
)
echo oneAPI environment setup complete. | tee -a "%LOG_FILE%"

:: --- Create Build Directory ---
if not exist "%BUILD_DIR%" (
    echo Creating build directory: %BUILD_DIR% | tee -a "%LOG_FILE%"
    mkdir "%BUILD_DIR%"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to create build directory. | tee -a "%LOG_FILE%"
        goto :error
    )
)

:: --- Run CMake Configure using Preset ---
echo Configuring project using CMake preset: %BUILD_PRESET% | tee -a "%LOG_FILE%"
cd /D "%SOURCE_DIR%"
cmake --preset %BUILD_PRESET% >> "%LOG_FILE%" 2>&1

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed. See %LOG_FILE% for details. | tee -a "%LOG_FILE%"
    goto :error
)
echo CMake configuration successful. | tee -a "%LOG_FILE%"

:: --- Build Project ---
echo Building project using preset: %BUILD_PRESET% | tee -a "%LOG_FILE%"
cmake --build --preset %BUILD_PRESET%-build --verbose >> "%LOG_FILE%" 2>&1

if %errorlevel% neq 0 (
    echo ERROR: Build failed. See %LOG_FILE% for details. | tee -a "%LOG_FILE%"
    goto :error
)
echo Build successful! | tee -a "%LOG_FILE%"

:: --- Optional Install Step ---
if %RUN_INSTALL% == 1 (
    echo Running install step... | tee -a "%LOG_FILE%"
    cmake --install "%BUILD_DIR%" --prefix "%SOURCE_DIR%\install\%BUILD_PRESET%" >> "%LOG_FILE%" 2>&1
    if %errorlevel% neq 0 (
        echo ERROR: Install step failed. See %LOG_FILE% for details. | tee -a "%LOG_FILE%"
        goto :error
    )
    echo Install step successful. | tee -a "%LOG_FILE%"
)

echo Build finished on %DATE% at %TIME% >> "%LOG_FILE%"
goto :success

:usage
echo.
echo AudioEngine Build Script
echo Usage: %~nx0 [options]
echo.
echo Options:
echo   /clean         - Clean build directory before building
echo   /install       - Run install step after successful build
echo   /preset NAME   - Use specified CMake preset (default: intel-debug)
echo   /help          - Show this help
echo.
echo Available presets:
echo   default        - Uses system compiler, Release build
echo   intel-release  - Uses Intel compilers, Release build
echo   intel-debug    - Uses Intel compilers, Debug build
echo   vs2022-desktop - Uses Visual Studio 2022, Release build
echo.
exit /b 1

:error
echo Build process failed.
endlocal
exit /b 1

:success
echo Build process completed successfully.
endlocal
exit /b 0
