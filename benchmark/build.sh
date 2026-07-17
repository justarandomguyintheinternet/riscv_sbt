#!/bin/bash

# Build coremark

echo "Building Coremark"
cd coremark
make PORT_DIR=linux \
    CC=riscv32-linux-musl-gcc \
    XCFLAGS="-O3 -static" \
    compile

# Build embench

echo "Building Embench"
cd ..
cp boardsupport.c embench-iot/examples/native/speed/
cd embench-iot

sudo apt install scons

# get cpu base freq on wsl2
if command -v wmic.exe &> /dev/null; then
    MAX_FREQ_MHZ=$(wmic.exe cpu get maxclockspeed 2>/dev/null | grep -Eo '[0-9]+' | head -n 1)
fi

# get cpu freq on "normal" linux
if [ -z "$MAX_FREQ_MHZ" ] && [ -f /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq ]; then
    MAX_FREQ_KHZ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq)
    MAX_FREQ_MHZ=$((MAX_FREQ_KHZ / 1000))
fi

if [ -z "$MAX_FREQ_MHZ" ]; then
    echo "Could not detect CPU frequency, fallback to 4000 MHz."
    MAX_FREQ_MHZ=4000
fi

echo "Building Embench, gsf=${MAX_FREQ_MHZ}"

scons --config-dir=examples/native/speed \
      cc=riscv32-linux-musl-gcc \
      cflags="-O3 -static" \
      ldflags="-static" \
      user_libs="m" \
      gsf="${MAX_FREQ_MHZ}"