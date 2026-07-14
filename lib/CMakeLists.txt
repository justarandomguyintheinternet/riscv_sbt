add_library(riscv_lib
        elf/ElfBinary.cpp
        elf/ElfBinarySection.cpp
        decoding/Decoder.cpp
        ProfilingInfo.cpp
        ProfilingInfo.h
        runtime/registers.h
        runtime/Memory.cpp
        runtime/Memory.h
        runtime/Context.h
        runtime/syscall.cpp
        runtime/syscall.h
)

target_link_libraries(riscv_lib PRIVATE elf)
target_link_libraries(riscv_lib PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(riscv_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
