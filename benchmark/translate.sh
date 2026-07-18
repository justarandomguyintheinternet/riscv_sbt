#!/bin/bash

if [ ! -d "bin/riscv" ]; then
  echo "bin/riscv does not exist, run build.sh first."
  exit 1
fi

SBT_PROJECT_DIR="../sbt"
SBT_PATH="$SBT_PROJECT_DIR/sbt"
SBT_BUILD_DIR="../cmake-build-debug-wsl"
TRANSLATED_PROJECT_DIR="$SBT_PROJECT_DIR/translated"
TRANSLATED_SOURCE="$TRANSLATED_PROJECT_DIR/src.cpp"
TRANSLATED_BINARY="$TRANSLATED_PROJECT_DIR/translated"
OUTPUT_DIR="./translated"
TIMING_FILE="translation-times.csv"

if [ ! -f "$SBT_PATH" ]; then
    echo "Translator binary \"$SBT_PATH\" not found."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "type,value" > "$TIMING_FILE" || exit 1

LOOP_START_NS=$(date +%s%N)
TRANSLATED_BINARIES=()

for BINARY in bin/riscv/*; do
    if [ ! -f "$BINARY" ]; then
        continue
    fi

    BINARY_NAME=$(basename "$BINARY")

    echo "Translating $BINARY..."
    "$SBT_PATH" "$BINARY" "$TRANSLATED_SOURCE" || exit 1

    echo "Compiling translated target for $BINARY_NAME..."
    cmake --build "$SBT_BUILD_DIR" --target translated || exit 1

    echo "Copying translated binary to $OUTPUT_DIR/$BINARY_NAME..."
    cp "$TRANSLATED_BINARY" "$OUTPUT_DIR/$BINARY_NAME" || exit 1

    TRANSLATED_BINARIES+=("$BINARY_NAME")
done

LOOP_END_NS=$(date +%s%N)
TOTAL_ELAPSED_NS=$((LOOP_END_NS - LOOP_START_NS))
TOTAL_ELAPSED_S=$(awk "BEGIN { printf \"%.3f\", $TOTAL_ELAPSED_NS / 1000000000 }")

for BINARY_NAME in "${TRANSLATED_BINARIES[@]}"; do
    echo "binary,$BINARY_NAME" >> "$TIMING_FILE" || exit 1
done

echo "TOTAL,$TOTAL_ELAPSED_S" >> "$TIMING_FILE" || exit 1

echo "Wrote translation timings to $TIMING_FILE"
