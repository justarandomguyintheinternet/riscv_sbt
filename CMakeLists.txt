cmake_minimum_required(VERSION 3.31)
set(CMAKE_CXX_STANDARD 23)

project(riscv_tools)

add_subdirectory(lib)
add_subdirectory(emulator)
add_subdirectory(sbt)
add_subdirectory(test)
add_subdirectory(scripts)
