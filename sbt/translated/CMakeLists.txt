if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src.cpp)
    add_executable(translated src.cpp)
    target_link_libraries(translated PRIVATE riscv_lib)
endif()