#include <stdint.h>

int _write(int fd, const void* buf, int count) {
    register int a0 asm("a0") = fd;
    register const void* a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    register int a7 asm("a7") = 64;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
    return a0;
}

void print_int(int n) {
    char buf[12];
    int i = 11;
    buf[i] = '\n';
    do {
        buf[--i] = '0' + n % 10;
        n = n / 10;
    } while (n > 0);
    _write(1, buf + i, 12 - i);
}

#define SIZE 8
uint32_t values[8] = {
    34,
    12,
    78,
    1,
    983,
    55,
    22,
    5
};

void sortAscending() {
    uint32_t temp;

    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (values[i] < values[j]) {
                temp = values[j];
                values[j] = values[i];
                values[i] = temp;
            }
        }
    }
};

void sortDescending() {
    uint32_t temp;

    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (values[i] > values[j]) {
                temp = values[j];
                values[j] = values[i];
                values[i] = temp;
            }
        }
    }
};

void (*modes[2])() = {
    sortAscending,
    sortDescending
};

int main() {
    for (int i = 0; i < 2; ++i) {
        modes[i]();

        for (int j = 0; j < SIZE; ++j) {
            print_int(values[j]);
        }
    }

    return 0;
}