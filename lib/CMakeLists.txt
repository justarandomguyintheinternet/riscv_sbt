add_library(riscv_lib
        elf/ElfBinary.cpp
        elf/ElfBinarySection.cpp
        decoding/Decoder.cpp
)

target_link_libraries(riscv_lib elf)
target_include_directories(riscv_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})