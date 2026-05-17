#include <stdint.h>
#include <stdio.h>

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
        printf("0x%08x\n", target[i]);
    }

    return 0;
}