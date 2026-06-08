# Requirements
- libelf: `sudo apt-get install libelf-dev`

## Cross Compiler
- Needed when actual OS is the target
- [musl-cross-make](https://github.com/richfelker/musl-cross-make)
- Setup `config.mak` for a minimal rv32 cross compiler with IMA extensions:
```
TARGET = riscv32-linux-musl
COMMON_CONFIG += --with-arch=rv32ima --with-abi=ilp32
GCC_CONFIG += --disable-shared
OUTPUT = /opt/riscv32-musl-ima
```

## Baremetal + picolibc
- [picolibc](https://github.com/picolibc/picolibc/tree/main)
- `gcc-riscv64-unknown-elf`
```sh
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf picolibc-riscv64-unknown-elf
```