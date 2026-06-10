#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint32_t* alloc(uint32_t size) {
    return malloc(size * sizeof(uint32_t));
}

int main() {
    uint32_t* pointers[4];

    for (int i = 0; i < 4; i++) {
        pointers[i] = alloc((i + 1) * 0x1000);
        printf("pointer %d: %p\n", i, pointers[i]);
    }

    for (int i = 0; i < 4; i++) {
        free(pointers[i]);
    }

    // test gaps
    uint32_t* a;
    uint32_t* b;
    uint32_t* c;
    uint32_t* d;

    a = alloc(0x1000);
    b = alloc(0x2000);
    c = alloc(0x1000);

    printf("a: %p\n", a);
    printf("b: %p\n", b);
    printf("c: %p\n", c);

    free(b);
    b = alloc(0x500);
    d = alloc(0x2000);
    printf("b: %p\n", b);
    printf("d: %p\n", d);

    free(a);
    free(b);
    free(c);
    free(d);

    return 0;
}