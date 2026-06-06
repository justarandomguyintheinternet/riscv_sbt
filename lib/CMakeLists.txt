add_library(riscv_lib
        elf/ElfBinary.cpp
        elf/ElfBinarySection.cpp
        decoding/Decoder.cpp
        runtime/registers.h
        runtime/Memory.cpp
        runtime/Memory.h
        runtime/Context.h
        runtime/syscall.cpp
        runtime/syscall.h
)

target_link_libraries(riscv_lib elf)
target_include_directories(riscv_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})