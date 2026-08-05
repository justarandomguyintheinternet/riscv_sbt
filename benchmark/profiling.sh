#!/bin/bash

if [ ! -d "bin/riscv" ]; then
  echo "bin/riscv does not exist, run build.sh first."
  exit 1
fi

cmake --build "../cmake-build-release-wsl/emulator" --target emulator || exit 1
mkdir -p "profiling"

for BINARY in bin/riscv/*; do
    if [ ! -f "$BINARY" ]; then
        continue
    fi

    BINARY_NAME=$(basename "$BINARY")

    rm ./profiling.json
    "../cmake-build-release-wsl/emulator/emulator" "$BINARY" || exit 1

    cp "./profiling.json" "./profiling/$BINARY_NAME.json" || exit 1
done