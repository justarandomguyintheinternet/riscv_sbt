cmake_minimum_required(VERSION 3.28.3)
set(CMAKE_CXX_STANDARD 23)

project(riscv_tools)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

function(riscv_target_interpreter_profiling TARGET_NAME ENABLED)
    if (NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target '${TARGET_NAME}' does not exist")
    endif()

    if (${ENABLED})
        target_compile_definitions(${TARGET_NAME} PRIVATE RISCV_EMU_PROFILING=1)
    else()
        target_compile_definitions(${TARGET_NAME} PRIVATE RISCV_EMU_PROFILING=0)
    endif()
endfunction()

add_subdirectory(lib)
add_subdirectory(emulator)
add_subdirectory(sbt)
add_subdirectory(test)
add_subdirectory(scripts)
