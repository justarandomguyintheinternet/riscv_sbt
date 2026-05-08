int _write(int fd, const void* buf, int count) {
    register int a0 asm("a0") = fd;
    register const void* a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    register int a7 asm("a7") = 64;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
    return a0;
}

int fib(int a) {
    if (a == 0) return 0;
    if (a == 1) return 1;
    return fib(a - 1) + fib(a - 2);
}

int mod10(int n) {
    while (n >= 10) n -= 10;
    return n;
}

int div10(int n) {
    int q = 0;
    while (n >= 10) { n -= 10; q++; }
    return q;
}

void print_int(int n) {
    char buf[12];
    int i = 11;
    buf[i] = '\n';
    do {
        buf[--i] = '0' + mod10(n);
        n = div10(n);
    } while (n > 0);
    _write(1, buf + i, 12 - i);
}

int main() {
    int result = fib(10);
    print_int(result);
    return 0;
}