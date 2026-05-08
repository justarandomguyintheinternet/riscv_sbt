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
        buf[--i] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    _write(1, buf + i, 12 - i);
}

int main() {
    int a = 3;
    int b = 12;
    int c = a * b;
    int d = b / a;
    int e = 10 % a;

    print_int(c);
    print_int(d);
    print_int(e);

    return 0;
}