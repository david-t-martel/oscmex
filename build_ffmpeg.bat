@echo off
REM build_ffmpeg.bat - Script to build FFmpeg components from source

echo Building FFmpeg components from source...
cd %~dp0\vendor\ffmpeg

REM Configure FFmpeg with required components
configure --disable-programs --disable-doc --enable-static --disable-shared ^
    --disable-avdevice --disable-postproc --disable-network ^
    --enable-gpl --enable-version3 ^
    --prefix="%~dp0\vendor\ffmpeg" ^
    --libdir="%~dp0\vendor\ffmpeg\lib"

REM Build and install
mingw32-make
mingw32-make install

echo FFmpeg build complete!
cd %~dp0
