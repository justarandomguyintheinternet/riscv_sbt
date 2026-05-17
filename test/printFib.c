#include <stdio.h>

// printf with picolibc: https://github.com/picolibc/picolibc/blob/main/doc/os.md#stdinstdoutstderr
// riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32   --specs=/usr/lib/picolibc/riscv64-unknown-elf/picolibc.specs -o printFib.elf printFib.c

int fib(int a) {
    if (a == 0) return 0;
    if (a == 1) return 1;
    return fib(a - 1) + fib(a - 2);
}

int main() {
    int result = fib(10);
    printf("Result: %d\n", result);
    return 0;
}