#!/bin/sh

# Install Python platformio 
# pip install -U platformio

# Update library
# git fetch
# git pull

# Build example(s)
platformio ci --lib=".." --project-conf=platformio.ini ../examples/CRC32/CRC32.ino
platformio ci --lib=".." --project-conf=platformio.ini ../tests/CRC32Benchmark/CRC32Benchmark.ino
platformio ci --lib=".." --project-conf=platformio.ini ../tests/CRC32UnitTest/CRC32UnitTest.ino
