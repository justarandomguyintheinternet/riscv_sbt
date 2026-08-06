add_subdirectory(translated)
add_executable(sbt
        main.cpp
        Options.cpp
        Options.h
        analysis/BasicBlocks.cpp
        analysis/BasicBlocks.h
        analysis/Functions.cpp
        analysis/Functions.h
        codegen/Emitter.cpp
        codegen/Emitter.h
        codegen/FunctionEmitter.cpp
        codegen/FunctionEmitter.h
        codegen/InstructionEmitter.cpp
        codegen/InstructionEmitter.h
        codegen/RuntimeEmitter.cpp
        codegen/RuntimeEmitter.h
)

target_include_directories(sbt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sbt PRIVATE riscv_lib)
target_link_libraries(sbt PRIVATE riscv_emu)

set(C_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/translated)
file(MAKE_DIRECTORY ${C_OUTPUT_DIR})