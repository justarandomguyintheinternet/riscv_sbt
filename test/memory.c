#include <stdint.h>

int _write(int fd, const void* buf, int count) {
    register int a0 asm("a0") = fd;
    register const void* a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    register int a7 asm("a7") = 64;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
    return a0;
}

void print_hex(uint32_t n) {
    char buf[11];
    int i = 10;
    buf[i] = '\n';
    do {
        int digit = n & 0xF;
        buf[--i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        n >>= 4;
    } while (n > 0);
    buf[--i] = 'x';
    buf[--i] = '0';
    _write(1, buf + i, 11 - i);
}

uint32_t origin[] = {
    0xDEADBEEF,
    0xFACE,
    0xFEED
};

uint32_t target[3];

int main() {
    for (int i = 0; i < 3; ++i) {
       target[i] = origin[i];
    }

    for (int i = 0; i < 3; ++i) {
        print_hex(target[i]);
    }

    return 0;
}