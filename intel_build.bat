@echo off
REM intel_build.bat - Sets up Intel oneAPI environment and launches CMake build

echo Setting up Intel oneAPI environment...
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat" intel64

echo.
echo Do you want to build FFmpeg from source? (y/n)
set /p build_ffmpeg=

if /i "%build_ffmpeg%"=="y" (
    echo Building FFmpeg from source...
    call build_ffmpeg.bat
)

echo.
echo Configuring project with CMake...
cmake --preset intel-release -DBUILD_FFMPEG_FROM_SOURCE=ON

echo.
echo Building project...
cmake --build --preset intel-release-build

echo.
echo Build complete! Check the output above for any errors.
echo.

pause
