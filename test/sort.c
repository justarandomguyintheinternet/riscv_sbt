#include <stdint.h>
#include <stdio.h>

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
            printf("%d\n", values[j]);
        }
    }

    return 0;
}