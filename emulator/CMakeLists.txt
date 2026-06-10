add_library(riscv_emu
        Interpreter.cpp
        Interpreter.h
)

target_include_directories(riscv_emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(riscv_emu PUBLIC riscv_lib)

add_executable(emulator main.cpp)
target_link_libraries(emulator PRIVATE riscv_emu)