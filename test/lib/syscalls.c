#include "syscalls.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

void _exit(int status) {
    register int a0 asm("a0") = status;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" :: "r"(a0), "r"(a7));
    __builtin_unreachable();
}

int _write(int fd, const void* buf, int count) {
    register int a0 asm("a0") = fd;
    register const void* a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    register int a7 asm("a7") = 64;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
    return a0;
}

static int _putc(char c, FILE *file)
{
    (void) file;
    volatile char buf = c; // Force compiler to emit store instruction for this, as it otherwise might just not store it to the stack cuz it cant see it being read by anything (its read by ecall)
    _write(1, (void*)&buf, 1);
    return c;
}

static int _getc(FILE *file)
{
    unsigned char c;
    (void) file;
    return c;
}

static FILE __stdio = FDEV_SETUP_STREAM(_putc,
					_getc,
					NULL,
					_FDEV_SETUP_WRITE);

FILE *const stdout = &__stdio;
FILE *const stderr = &__stdio;
FILE *const stdin  = &__stdio;