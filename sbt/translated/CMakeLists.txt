if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src.cpp)
    set(FLAGS_TRANSLATED -O3 -fno-gcse)

    add_executable(translated src.cpp)
    target_compile_options(translated PRIVATE ${FLAGS_TRANSLATED})
    target_link_libraries(translated PRIVATE riscv_lib)
    target_link_libraries(translated PRIVATE riscv_emu)
    riscv_target_interpreter_profiling(translated OFF)

    set_target_properties(translated PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
