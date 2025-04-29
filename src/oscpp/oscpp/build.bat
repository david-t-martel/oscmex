@echo off
setlocal enabledelayedexpansion

REM OSCPP Build Script for Windows
REM =============================
REM This script builds the OSCPP library with CMake
REM Usage: build.bat [clean] [debug|release] [examples] [tests] [install] [preset=name] [test]

REM Parse command line arguments
set CLEAN=0
set BUILD_TYPE=Release
set BUILD_EXAMPLES=ON
set BUILD_TESTS=ON
set INSTALL=OFF
set GENERATOR=
set TOOLSET=
set PLATFORM=x64
set VERBOSE=OFF
set USE_PRESET=0
set PRESET_NAME=
set RUN_TESTS=0
set STANDALONE_TEST=OFF

REM Process arguments
:arg_loop
if "%1"=="" goto arg_done
if /i "%1"=="clean" set CLEAN=1
if /i "%1"=="debug" set BUILD_TYPE=Debug
if /i "%1"=="release" set BUILD_TYPE=Release
if /i "%1"=="examples" set BUILD_EXAMPLES=ON
if /i "%1"=="no-examples" set BUILD_EXAMPLES=OFF
if /i "%1"=="tests" set BUILD_TESTS=ON
if /i "%1"=="no-tests" set BUILD_TESTS=OFF
if /i "%1"=="install" set INSTALL=ON
if /i "%1"=="verbose" set VERBOSE=ON
if /i "%1"=="x86" set PLATFORM=Win32
if /i "%1"=="x64" set PLATFORM=x64
if /i "%1"=="test" set RUN_TESTS=1
if /i "%1"=="standalone-test" set STANDALONE_TEST=ON

REM Check for preset option
set _ARG=%1
if "!_ARG:~0,7!"=="preset=" (
    set USE_PRESET=1
    set PRESET_NAME=!_ARG:~7!
)

shift
goto arg_loop
:arg_done

REM Check CMake version for presets support (CMake 3.20+)
set CMAKE_SUPPORTS_PRESETS=0
for /f "tokens=*" %%i in ('cmake --version') do (
    set VERSION_LINE=%%i
    goto :break_cmake_version
)
:break_cmake_version
set VERSION_LINE=!VERSION_LINE:cmake version =!
for /f "tokens=1 delims=." %%a in ("!VERSION_LINE!") do set MAJOR=%%a
for /f "tokens=2 delims=." %%a in ("!VERSION_LINE!") do set MINOR=%%a
if !MAJOR! GTR 3 (
    set CMAKE_SUPPORTS_PRESETS=1
) else if !MAJOR! EQU 3 (
    if !MINOR! GEQ 20 (
        set CMAKE_SUPPORTS_PRESETS=1
    )
)

REM Check if presets file exists
set PRESETS_FILE_EXISTS=0
if exist CMakePresets.json (
    set PRESETS_FILE_EXISTS=1
)

REM Check for Visual Studio installation
set VS_ENTERPRISE="%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.com"
set VS_PROFESSIONAL="%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.com"
set VS_COMMUNITY="%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com"
set VS_BUILDTOOLS="%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\devenv.com"

if exist %VS_ENTERPRISE% (
    echo Using Visual Studio 2022 Enterprise
    set GENERATOR="Visual Studio 17 2022"
    set HAS_VS=1
) else if exist %VS_PROFESSIONAL% (
    echo Using Visual Studio 2022 Professional
    set GENERATOR="Visual Studio 17 2022"
    set HAS_VS=1
) else if exist %VS_COMMUNITY% (
    echo Using Visual Studio 2022 Community
    set GENERATOR="Visual Studio 17 2022"
    set HAS_VS=1
) else if exist %VS_BUILDTOOLS% (
    echo Using Visual Studio 2022 Build Tools
    set GENERATOR="Visual Studio 17 2022"
    set HAS_VS=1
) else (
    REM Check for older versions if needed
    echo Visual Studio 2022 not found, using default generator
    set GENERATOR=""
    set HAS_VS=0
)

REM Check if CMake is installed
cmake --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: CMake not found. Please install CMake and ensure it's in your PATH.
    exit /b 1
)

REM Check for path issues by examining include directory structure
set INCLUDE_STRUCTURE=STANDARD
if not exist include\osc (
    if exist ..\include\osc (
        echo Warning: Using parent directory's include structure
        set INCLUDE_STRUCTURE=PARENT
    ) else (
        echo Error: Could not find include\osc directory or ..\include\osc
        echo Please run this script from the correct directory
        exit /b 1
    )
)

REM Determine if we should use presets
if %USE_PRESET% EQU 1 (
    if %CMAKE_SUPPORTS_PRESETS% EQU 0 (
        echo Error: CMake presets require CMake 3.20 or newer. Your version does not support presets.
        exit /b 1
    )
    if %PRESETS_FILE_EXISTS% EQU 0 (
        echo Error: CMakePresets.json not found in current directory.
        exit /b 1
    )
    set USING_PRESETS=1
) else if %CMAKE_SUPPORTS_PRESETS% EQU 1 (
    if %PRESETS_FILE_EXISTS% EQU 1 (
        REM Auto-select preset based on build type
        if "%BUILD_TYPE%"=="Debug" (
            set PRESET_NAME=windows-msvc-debug
            set USING_PRESETS=1
        ) else (
            set PRESET_NAME=windows-msvc-release
            set USING_PRESETS=1
        )
    ) else (
        set USING_PRESETS=0
    )
) else (
    set USING_PRESETS=0
)

REM Display build information
echo ========================================
echo OSCPP Library Build Script
echo ========================================
echo Build configuration:
if %USING_PRESETS% EQU 1 (
    echo   Using CMake preset: %PRESET_NAME%
) else (
    echo   Build type: %BUILD_TYPE%
    echo   Build examples: %BUILD_EXAMPLES%
    echo   Build tests: %BUILD_TESTS%
    echo   Install: %INSTALL%
    echo   Platform: %PLATFORM%
    if defined GENERATOR echo   Generator: %GENERATOR%
)
if "%STANDALONE_TEST%"=="ON" echo   Building standalone test executable
echo ========================================

REM Create build directory
if %USING_PRESETS% EQU 1 (
    REM For presets, the build directory is defined in the preset
    REM but ensure its parent exists
    if not exist build mkdir build
) else (
    if not exist build mkdir build
)

REM Clean if requested
if %CLEAN% equ 1 (
    echo Cleaning build directory...
    if %USING_PRESETS% EQU 1 (
        if "%PRESET_NAME%"=="windows-msvc-debug" (
            if exist build\windows-debug\CMakeCache.txt del /q build\windows-debug\CMakeCache.txt
            if exist build\windows-debug\*.sln del /q build\windows-debug\*.sln
        ) else if "%PRESET_NAME%"=="windows-msvc-release" (
            if exist build\windows-release\CMakeCache.txt del /q build\windows-release\CMakeCache.txt
            if exist build\windows-release\*.sln del /q build\windows-release\*.sln
        ) else (
            echo Unknown preset: %PRESET_NAME%
            echo Performing general clean...
            if exist build\CMakeCache.txt del /q build\CMakeCache.txt
            if exist build\*.sln del /q build\*.sln
        )
    ) else (
        if exist build\CMakeCache.txt del /q build\CMakeCache.txt
        if exist build\*.sln del /q build\*.sln
        if exist build\*.vcxproj del /q build\*.vcxproj
    )
)

REM Configure and build using either presets or traditional CMake
if %USING_PRESETS% EQU 1 (
    echo Using CMake preset: %PRESET_NAME%

    REM Configure
    echo Configuring with preset...
    cmake --preset %PRESET_NAME% -DOSC_STANDALONE_TEST=%STANDALONE_TEST%
    if %ERRORLEVEL% neq 0 (
        echo Error: CMake configuration with preset failed.
        exit /b 1
    )

    REM Build
    echo Building with preset...
    if "%VERBOSE%"=="ON" (
        cmake --build --preset %PRESET_NAME% --verbose
    ) else (
        cmake --build --preset %PRESET_NAME%
    )

    if %ERRORLEVEL% neq 0 (
        echo Error: Build failed.
        exit /b 1
    )

    REM Run tests if built and requested
    if %RUN_TESTS% EQU 1 (
        if "%BUILD_TESTS%"=="ON" (
            echo Running tests...
            ctest --preset %PRESET_NAME%
            if %ERRORLEVEL% neq 0 (
                echo Warning: Some tests failed.
            )
        ) else (
            echo Tests were not built, skipping test run
        )
    )

    REM Run standalone test if built
    if "%STANDALONE_TEST%"=="ON" (
        echo Running standalone test...
        REM Determine executable path
        if "%PRESET_NAME%"=="windows-msvc-debug" (
            if exist build\windows-debug\Debug\oscpp_test.exe (
                build\windows-debug\Debug\oscpp_test.exe
            ) else (
                echo Cannot find standalone test executable
            )
        ) else if "%PRESET_NAME%"=="windows-msvc-release" (
            if exist build\windows-release\Release\oscpp_test.exe (
                build\windows-release\Release\oscpp_test.exe
            ) else (
                echo Cannot find standalone test executable
            )
        )
    )

    REM Install if requested (not supported directly with presets)
    if "%INSTALL%"=="ON" (
        echo Installing...
        REM Determine the build directory from preset name
        if "%PRESET_NAME%"=="windows-msvc-debug" (
            cmake --install build\windows-debug --config Debug
        ) else if "%PRESET_NAME%"=="windows-msvc-release" (
            cmake --install build\windows-release --config Release
        ) else (
            echo Unknown preset for installation. Please install manually.
            exit /b 1
        )

        if %ERRORLEVEL% neq 0 (
            echo Error: Installation failed.
            exit /b 1
        )
        echo Installation complete. Files are in 'install' directory.
    )
) else (
    REM Traditional CMake workflow
    cd build

    REM Configure CMake
    echo Configuring CMake...
    set CMAKE_ARGS=..

    REM Add generator if defined
    if not "%GENERATOR%"=="" set CMAKE_ARGS=%CMAKE_ARGS% -G %GENERATOR%

    REM Add platform architecture
    if not "%PLATFORM%"=="" (
        if %HAS_VS% equ 1 (
            set CMAKE_ARGS=%CMAKE_ARGS% -A %PLATFORM%
        )
    )

    REM Add toolchain file
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_TOOLCHAIN_FILE=../cmake/windows.cmake

    REM Add other args
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    set CMAKE_ARGS=%CMAKE_ARGS% -DOSCPP_BUILD_EXAMPLES=%BUILD_EXAMPLES%
    set CMAKE_ARGS=%CMAKE_ARGS% -DOSCPP_BUILD_TESTS=%BUILD_TESTS%
    set CMAKE_ARGS=%CMAKE_ARGS% -DOSCPP_INSTALL=%INSTALL%
    set CMAKE_ARGS=%CMAKE_ARGS% -DOSC_STANDALONE_TEST=%STANDALONE_TEST%

    REM Handle include path issues if needed
    if "%INCLUDE_STRUCTURE%"=="PARENT" (
        set CMAKE_ARGS=%CMAKE_ARGS% -DOSCPP_INCLUDE_DIR=../../include
    )

    REM Add installation path if installing
    if "%INSTALL%"=="ON" (
        set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_INSTALL_PREFIX=../install
    )

    REM Configure
    echo Running: cmake %CMAKE_ARGS%
    cmake %CMAKE_ARGS%
    if %ERRORLEVEL% neq 0 (
        echo Error: CMake configuration failed.
        cd ..
        exit /b 1
    )

    REM Build
    echo Building %BUILD_TYPE% configuration...
    if "%VERBOSE%"=="ON" (
        cmake --build . --config %BUILD_TYPE% --verbose
    ) else (
        cmake --build . --config %BUILD_TYPE%
    )

    if %ERRORLEVEL% neq 0 (
        echo Error: Build failed.
        cd ..
        exit /b 1
    )

    REM Run tests if built and requested
    if %RUN_TESTS% EQU 1 (
        if "%BUILD_TESTS%"=="ON" (
            echo Running tests...
            ctest -C %BUILD_TYPE% --output-on-failure
            if %ERRORLEVEL% neq 0 (
                echo Warning: Some tests failed.
            )
        ) else (
            echo Tests were not built, skipping test run
        )
    )

    REM Run standalone test if built
    if "%STANDALONE_TEST%"=="ON" (
        echo Running standalone test...
        if exist %BUILD_TYPE%\oscpp_test.exe (
            %BUILD_TYPE%\oscpp_test.exe
        ) else if exist bin\%BUILD_TYPE%\oscpp_test.exe (
            bin\%BUILD_TYPE%\oscpp_test.exe
        ) else (
            echo Cannot find standalone test executable
        )
    )

    REM Install if requested
    if "%INSTALL%"=="ON" (
        echo Installing...
        cmake --install . --config %BUILD_TYPE%
        if %ERRORLEVEL% neq 0 (
            echo Error: Installation failed.
            cd ..
            exit /b 1
        )
        echo Installation complete. Files are in 'install' directory.
    )

    cd ..
)

echo ========================================
echo Build completed successfully!
echo ========================================
if %USING_PRESETS% EQU 1 (
    if "%PRESET_NAME%"=="windows-msvc-debug" (
        echo Using preset: windows-msvc-debug
        echo You can find:
        echo - Library files in build\windows-debug\Debug
        if "%BUILD_EXAMPLES%"=="ON" echo - Example executables in build\windows-debug\bin\Debug
        if "%BUILD_TESTS%"=="ON" echo - Test executables in build\windows-debug\bin\tests\Debug
        if "%STANDALONE_TEST%"=="ON" echo - Standalone test executable in build\windows-debug\Debug\oscpp_test.exe
    ) else if "%PRESET_NAME%"=="windows-msvc-release" (
        echo Using preset: windows-msvc-release
        echo You can find:
        echo - Library files in build\windows-release\Release
        if "%BUILD_EXAMPLES%"=="ON" echo - Example executables in build\windows-release\bin\Release
        if "%BUILD_TESTS%"=="ON" echo - Test executables in build\windows-release\bin\tests\Release
        if "%STANDALONE_TEST%"=="ON" echo - Standalone test executable in build\windows-release\Release\oscpp_test.exe
    )
) else (
    echo You can find:
    echo - Library files in build\%BUILD_TYPE%
    if "%BUILD_EXAMPLES%"=="ON" echo - Example executables in build\bin\%BUILD_TYPE%
    if "%BUILD_TESTS%"=="ON" echo - Test executables in build\bin\tests\%BUILD_TYPE%
    if "%STANDALONE_TEST%"=="ON" echo - Standalone test executable in build\%BUILD_TYPE%\oscpp_test.exe
)
if "%INSTALL%"=="ON" echo - Installed files in install
echo ========================================

exit /b 0
