# Requirements
- Linux host (x86-64 for the translator's register pinning), 64-bit.
- [libelf](https://sourceware.org/elfutils/): `sudo apt-get install libelf-dev`
- CMake `>= 3.28.3` and a build tool (`make`/`ninja`).
- A C++23 compiler with GNU extensions (computed `goto`, explicit register variables)

## Cross Compiler
- Needed for compiling compatible RISC-V binaries
- [musl-cross-make](https://github.com/richfelker/musl-cross-make)
- Setup `config.mak` for a minimal rv32ima cross compiler:
```
TARGET = riscv32-linux-musl
COMMON_CONFIG += --with-arch=rv32ima --with-abi=ilp32
GCC_CONFIG += --disable-shared
OUTPUT = /opt/riscv32-musl-ima
GCC_VER = 14.2.0
```

## Optional
- Baremetal + [picolibc](https://github.com/picolibc/picolibc/tree/main), only needed for the `pico_test_programs` target in `test/`:
```sh
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf picolibc-riscv64-unknown-elf
```
- `scons` and Python 3 for the benchmark suite (`benchmark/build.sh`, `benchmark/*.py`).

## Building
```sh
# Build all
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build target, e.g. SBT
cmake --build build --target sbt
```
Targets are:
- `emulator`: `build/emulator/emulator`
- `sbt`: `build/sbt/sbt`
- `translated`: `sbt/translated/translated` (Only configured if `sbt/translated/src.cpp` exists)
- `musl_test_programs` / `pico_test_programs`: test ELFs in `test/bin/<variant>/`

# Supported Binaries / Limitations
- Only supports `rv32ima`, using the `ilp32` ABI.
  - Of the `a` (atomic) extension, only `LR/SC` are implemented, non-atomically.
- Binaries must be single-threaded, and statically linked (Preferably using the musl stdlib).
- Supported syscalls: `openat`, `close`, `read`, `write`, `exit`, `exit_group`, `brk`, `munmap`, `clone`, `execve`, `mmap`, `statx`, `clock_gettime64`
  - `clone` only supported when used as fork, i.e. nothing that requests a shared address space.
- Target must be 64-bit Linux, register pinning is only supported on x86-64 hosts.

# Interpreter
```sh
./build/emulator/emulator <elf binary> [guest args...]
```

## Options
- Options enabled by setting defines in `emulator/main.cpp`:
  - `SWITCH`: Uses a switch-based dispatch instead of chained `if`'s.
  - `PREDECODE`: Predecodes instruction stream, is implicitly active for any type of threading.
  - `THREADING`: Enables threading, default type is indirect threading.
- Options enabled by setting defines in `emulator/Interpreter.cpp`:
  - `DIRECT_THREADING`: Enables direct threading, otherwise indirect threading is used
- Profiling:
  - Flip target in `emulator/CMakeLists.txt` to `riscv_target_interpreter_profiling(riscv_emu ON)`
  - Written to `./profiling.json`, relative to the current working directory
  - By default, previous profiling data is overwritten

# Translator
```sh
./build/sbt/sbt path/to/binary.elf

# Make sure CMake is configured
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target translated

# Build translated source
./sbt/translated/translated [guest args...]
```
- Generated `src.cpp` is placed in the `sbt/translated` directory.
- Translated binary will be placed next to the `src.cpp` file.

## Options
- Options are passed to the SBT at translate-time.
- `--translation-chaining`: Turns on translation chaining, appending dispatch code to each indirect jump site directly.
- `--software-branch-prediction`: Hardcodes three most frequent observed targets as direct jumps, for `jalr`'s. Requires `./profiling.json` to be present in the current working directory.
- `--use-profiling-data`: Feeds every observed indirect branch target from `./profiling.json` into basic block leader set.
- `--pin-registers`: Pins eight hottest guest registers as explicit host register variables. Requires x86-64 host and compiler supporting explicit register variables.