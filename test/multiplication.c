#include <stdio.h>

int main() {
    int a = 3;
    int b = 12;
    int c = a * b;
    int d = b / a;
    int e = 10 % a;

    printf("%d * %d = %d\n", a, b, c);
    printf("%d / %d = %d\n", b, a, d);
    printf("10 %% %d = %d\n", a, e);

    return 0;
}