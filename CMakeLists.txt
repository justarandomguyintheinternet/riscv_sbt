cmake_minimum_required(VERSION 3.31)
set(CMAKE_CXX_COMPILER_LINKER_TYPE SYSTEM)

project(riscv_emu)

set(CMAKE_CXX_STANDARD 23)

add_executable(riscv_emu main.cpp
        ElfBinary.cpp
        ElfBinary.h
        ElfBinarySection.cpp)

target_link_libraries(riscv_emu elf)
