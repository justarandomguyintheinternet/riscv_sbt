if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src.cpp)
    add_executable(translated src.cpp)
    target_link_libraries(translated PRIVATE riscv_lib)

    set_target_properties(translated PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()