#!/bin/bash

if [ ! -d "bin/riscv" ]; then
  echo "bin/riscv does not exist, run build.sh first."
  exit 1
fi

SBT_PROJECT_DIR="../sbt"
SBT_BUILD_DIR="../cmake-build-debug-wsl"
SBT_PATH="$SBT_BUILD_DIR/sbt/sbt"
TRANSLATED_PROJECT_DIR="$SBT_PROJECT_DIR/translated"
TRANSLATED_SOURCE="$TRANSLATED_PROJECT_DIR/src.cpp"
TRANSLATED_BINARY="$TRANSLATED_PROJECT_DIR/translated"
OUTPUT_DIR="./translated"
TIMING_FILE="translation-times.csv"
LINE_COUNT_FILE="translation-line-counts.csv"

if [ ! -f "$SBT_PATH" ]; then
    echo "Translator binary \"$SBT_PATH\" not found."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "type,value" > "$TIMING_FILE" || exit 1

LOOP_START_NS=$(date +%s%N)
TRANSLATED_BINARIES=()
TRANSLATED_LINE_COUNTS=()

for BINARY in bin/riscv/*; do
    if [ ! -f "$BINARY" ]; then
        continue
    fi

    BINARY_NAME=$(basename "$BINARY")

    echo "Translating $BINARY..."
    "$SBT_PATH" "$BINARY" "$TRANSLATED_SOURCE" || exit 1

    WHILE_TRUE_LINE=$(grep -nF 'while (true) {' "$TRANSLATED_SOURCE" | head -n1 | cut -d: -f1)
    MAIN_LINE=$(grep -nF 'int main' "$TRANSLATED_SOURCE" | head -n1 | cut -d: -f1)

    if [ -z "$MAIN_LINE" ] || [ -z "$WHILE_TRUE_LINE" ]; then
        echo "Failed to locate loop/main markers in $TRANSLATED_SOURCE for $BINARY_NAME."
        exit 1
    fi

    LINE_COUNT_DIFF=$((MAIN_LINE - WHILE_TRUE_LINE))

    echo "Compiling translated target for $BINARY_NAME..."
    cmake --build "$SBT_BUILD_DIR" --target translated || exit 1

    echo "Copying translated binary to $OUTPUT_DIR/$BINARY_NAME..."
    cp "$TRANSLATED_BINARY" "$OUTPUT_DIR/$BINARY_NAME" || exit 1

    TRANSLATED_BINARIES+=("$BINARY_NAME")
    TRANSLATED_LINE_COUNTS+=("$LINE_COUNT_DIFF")
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

echo "TOTAL,$TOTAL_ELAPSED_S" >> "$TIMING_FILE" || exit 1

echo "Wrote translation timings to $TIMING_FILE"
echo "Wrote translation line counts to $LINE_COUNT_FILE"
