#!/bin/bash
# build_ffmpeg.sh - Script to build FFmpeg components from source on Linux/macOS

echo "Building FFmpeg components from source..."
cd "$(dirname "$0")/vendor/ffmpeg"

# Configure FFmpeg with required components
./configure --disable-programs --disable-doc --enable-static --disable-shared \
    --disable-avdevice --disable-postproc --disable-network \
    --enable-gpl --enable-version3 \
    --prefix="$(dirname "$0")/vendor/ffmpeg" \
    --libdir="$(dirname "$0")/vendor/ffmpeg/lib"

# Build and install
make -j $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
make install

echo "FFmpeg build complete!"
cd "$(dirname "$0")"
