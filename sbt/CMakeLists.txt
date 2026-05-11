add_executable(sbt main.cpp)

target_link_libraries(sbt PRIVATE riscv_lib)

set(C_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/output)
file(MAKE_DIRECTORY ${C_OUTPUT_DIR})