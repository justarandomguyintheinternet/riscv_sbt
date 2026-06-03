# Currently uses baremetal compiler with picolibc, easiest way of having basic stdio support without having to worry about all the other syscalls that e.g. musl libc utilizes in its startup code.

set(RISCV_CC riscv64-unknown-elf-gcc)
set(RISCV_FLAGS -march=rv32im -mabi=ilp32 -O3 --specs=picolibc.specs -T ${CMAKE_CURRENT_SOURCE_DIR}/picostdlib.ld --crt0=hosted)
set(ELF_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
# https://gcc.gnu.org/onlinedocs/gcc/Picolibc-Options.html

# make output directory if it doesnt exist
file(MAKE_DIRECTORY ${ELF_OUTPUT_DIR})

# make a list of all .c files
file(GLOB TEST_SOURCES "*.c")

set(TEST_OUTPUTS)
foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    set(out ${ELF_OUTPUT_DIR}/${name}.elf)
    add_custom_command(
            OUTPUT ${out}
            COMMAND ${RISCV_CC} ${RISCV_FLAGS} -o ${out} ${src} ${CMAKE_CURRENT_SOURCE_DIR}/lib/syscalls.c
            DEPENDS ${src}
    )
    list(APPEND TEST_OUTPUTS ${out})
endforeach()

add_custom_target(test_programs
        DEPENDS ${TEST_OUTPUTS}
)

# add all files to some dummy target, so that clion highlighting works
add_library(test_sources_ide EXCLUDE_FROM_ALL ${TEST_SOURCES} lib/)