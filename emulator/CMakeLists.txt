add_executable(emulator main.cpp
        Interpreter.cpp
        Interpreter.h)

target_link_libraries(emulator PRIVATE riscv_lib)