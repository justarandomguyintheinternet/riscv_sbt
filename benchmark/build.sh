#!/bin/bash

TARGET="${1:-riscv}"

# Setup output dir
mkdir -p bin

case "$TARGET" in
    riscv)
        CC_COMPILER="riscv32-linux-musl-gcc"
        ;;
    x86)
        CC_COMPILER="gcc"
        ;;
    *)
        echo "Unknown target: $TARGET"
        echo "Usage: ./build.sh [riscv|x86]"
        exit 1
        ;;
esac

mkdir -p bin/"$TARGET"
echo "Selected target: $TARGET"
echo "Using compiler: $CC_COMPILER"

# Build coremark

echo "Building Coremark"
cd coremark
make PORT_DIR=linux \
    CC="$CC_COMPILER" \
    XCFLAGS="-O3 -static" \
    compile

# Build embench

cd ..
cp boardsupport.c embench-iot/examples/native/speed/
cd embench-iot

if ! command -v scons &> /dev/null; then
    sudo apt install scons
fi

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
      cc="$CC_COMPILER" \
      cflags="-O3 -static" \
      ldflags="-static" \
      user_libs="m" \
      gsf="${MAX_FREQ_MHZ}"

cd ..

OUTPUT_DIR="bin/$TARGET"
cp coremark/coremark.exe "$OUTPUT_DIR/coremark"

for BENCHMARK_DIR in embench-iot/bd/src/*; do
    if [ -d "$BENCHMARK_DIR" ]; then
        BENCHMARK_NAME=$(basename "$BENCHMARK_DIR")
        BENCHMARK_FILE="$BENCHMARK_DIR/$BENCHMARK_NAME"

        if [ -f "$BENCHMARK_FILE" ]; then
            cp "$BENCHMARK_FILE" "$OUTPUT_DIR/"
        else
            echo "Warning: expected benchmark file not found: $BENCHMARK_FILE"
        fi
    fi
done