set(ELF_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# make output directory if it doesnt exist
file(MAKE_DIRECTORY ${ELF_OUTPUT_DIR})

# make a list of all .c files
file(GLOB TEST_SOURCES "*.c")

function(addTarget TARGET_NAME CC_EXECUTABLE CC_FLAGS SUBDIR_NAME)
    set(VARIANT_OUTPUT_DIR ${ELF_OUTPUT_DIR}/${SUBDIR_NAME}) # subdir for specific target
    file(MAKE_DIRECTORY ${VARIANT_OUTPUT_DIR})

    set(TEST_OUTPUTS)

    foreach(src ${TEST_SOURCES})
        get_filename_component(name ${src} NAME_WE)
        set(out ${VARIANT_OUTPUT_DIR}/${name}.elf)

        add_custom_command(
                OUTPUT ${out}
                COMMAND ${CC_EXECUTABLE} ${CC_FLAGS} -o ${out} ${src}
                DEPENDS ${src}
        )
        list(APPEND TEST_OUTPUTS ${out})
    endforeach()

    add_custom_target(${TARGET_NAME}
            DEPENDS ${TEST_OUTPUTS}
    )
endfunction()

# add all files to some dummy target, so that clion highlighting works
add_library(test_sources_ide EXCLUDE_FROM_ALL ${TEST_SOURCES} lib/)

# Uses baremetal compiler with picolibc, easiest way of having basic stdio support without full OS support
# https://gcc.gnu.org/onlinedocs/gcc/Picolibc-Options.html
set(CC_PICO riscv64-unknown-elf-gcc)
set(FLAGS_PICO -march=rv32im -mabi=ilp32 -O3 ${CMAKE_CURRENT_SOURCE_DIR}/lib/syscalls.c --specs=picolibc.specs -T ${CMAKE_CURRENT_SOURCE_DIR}/picostdlib.ld --crt0=hosted) # crt0=hosted to make it emit an exit syscall
addTarget(pico_test_programs "${CC_PICO}" "${FLAGS_PICO}" "picolibc")

# Compile for linux, with musl libc
set(CC_MUSL riscv32-linux-musl-gcc)
set(FLAGS_MUSL -static -O3)
addTarget(musl_test_programs "${CC_MUSL}" "${FLAGS_MUSL}" "musl")