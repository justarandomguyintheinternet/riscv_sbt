#!/bin/bash

if [ ! -d "bin/riscv" ]; then
  echo "bin/riscv does not exist, run build.sh first."
  exit 1
fi

SBT_PROJECT_DIR="../sbt"
SBT_BUILD_DIR="../cmake-build-release-wsl"
SBT_PATH="$SBT_BUILD_DIR/sbt/sbt"
TRANSLATED_PROJECT_DIR="$SBT_PROJECT_DIR/translated"
TRANSLATED_SOURCE="$TRANSLATED_PROJECT_DIR/src.cpp"
TRANSLATED_BINARY="$TRANSLATED_PROJECT_DIR/translated"
OUTPUT_DIR="./translated"
TIMING_FILE="translation-times.csv"
LINE_COUNT_FILE="translation-loc.csv"
INSTRUCTION_EXPANSION_FILE="translation-instruction-expansion.csv"

cmake --build "$SBT_BUILD_DIR" --target sbt || exit 1

if [ ! -f "$SBT_PATH" ]; then
    echo "Translator binary \"$SBT_PATH\" not found."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "type,value" > "$TIMING_FILE" || exit 1

LOOP_START_NS=$(date +%s%N)
TRANSLATED_BINARIES=()
TRANSLATED_LINE_COUNTS=()
ORIGINAL_INSTRUCTION_COUNTS=()
TRANSLATED_INSTRUCTION_COUNTS=()
INSTRUCTION_EXPANSION_FACTORS=()

for BINARY in bin/riscv/*; do
    if [ ! -f "$BINARY" ]; then
        continue
    fi

    BINARY_NAME=$(basename "$BINARY")

    cp ./profiling/"$BINARY_NAME.json" "./profiling.json"

    echo "Translating $BINARY..."
    "$SBT_PATH" "$BINARY" "$TRANSLATED_SOURCE" "$@" || exit 1

    echo "Compiling translated target for $BINARY_NAME..."
    cmake --build "$SBT_BUILD_DIR" --target translated || exit 1

    ORIGINAL_INSTRUCTION_COUNT=$(riscv32-linux-musl-objdump -d -j .text "$BINARY" | grep -c '^[[:space:]]*[0-9a-f]\+:')
    TRANSLATED_INSTRUCTION_COUNT=$(objdump -d -j .translated_text "$TRANSLATED_BINARY" | grep -c '^[[:space:]]*[0-9a-f]\+:')

    if [ "$ORIGINAL_INSTRUCTION_COUNT" -eq 0 ]; then
        echo "Original instruction count for $BINARY_NAME is zero; cannot compute expansion factor."
        exit 1
    fi

    INSTRUCTION_EXPANSION_FACTOR=$(awk "BEGIN { printf \"%.6f\", $TRANSLATED_INSTRUCTION_COUNT / $ORIGINAL_INSTRUCTION_COUNT }")

    echo "Copying translated binary to $OUTPUT_DIR/$BINARY_NAME..."
    cp "$TRANSLATED_BINARY" "$OUTPUT_DIR/$BINARY_NAME" || exit 1

    TRANSLATED_BINARIES+=("$BINARY_NAME")
    ORIGINAL_INSTRUCTION_COUNTS+=("$ORIGINAL_INSTRUCTION_COUNT")
    TRANSLATED_INSTRUCTION_COUNTS+=("$TRANSLATED_INSTRUCTION_COUNT")
    INSTRUCTION_EXPANSION_FACTORS+=("$INSTRUCTION_EXPANSION_FACTOR")
done

LOOP_END_NS=$(date +%s%N)
TOTAL_ELAPSED_NS=$((LOOP_END_NS - LOOP_START_NS))
TOTAL_ELAPSED_S=$(awk "BEGIN { printf \"%.3f\", $TOTAL_ELAPSED_NS / 1000000000 }")

for BINARY_NAME in "${TRANSLATED_BINARIES[@]}"; do
    echo "binary,$BINARY_NAME" >> "$TIMING_FILE" || exit 1
done

echo "binaryName,linesCount" > "$LINE_COUNT_FILE" || exit 1
for i in "${!TRANSLATED_BINARIES[@]}"; do
    echo "${TRANSLATED_BINARIES[$i]},${TRANSLATED_LINE_COUNTS[$i]}" >> "$LINE_COUNT_FILE" || exit 1
done

echo "binaryName,originalInstructionCount,translatedInstructionCount,expansionFactor" > "$INSTRUCTION_EXPANSION_FILE" || exit 1
for i in "${!TRANSLATED_BINARIES[@]}"; do
    echo "${TRANSLATED_BINARIES[$i]},${ORIGINAL_INSTRUCTION_COUNTS[$i]},${TRANSLATED_INSTRUCTION_COUNTS[$i]},${INSTRUCTION_EXPANSION_FACTORS[$i]}" >> "$INSTRUCTION_EXPANSION_FILE" || exit 1
done

echo "TOTAL,$TOTAL_ELAPSED_S" >> "$TIMING_FILE" || exit 1

echo "Wrote translation timings to $TIMING_FILE"
echo "Wrote translation line counts to $LINE_COUNT_FILE"
echo "Wrote instruction expansion metrics to $INSTRUCTION_EXPANSION_FILE"
