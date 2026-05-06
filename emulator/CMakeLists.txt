add_executable(emulator main.cpp)

target_link_libraries(emulator PRIVATE riscv_lib)