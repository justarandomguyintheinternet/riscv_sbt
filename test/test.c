int _write(int fd, const void* buf, int count) {
    register int a0 asm("a0") = fd;
    register const void* a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    register int a7 asm("a7") = 64;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
    return a0;
}

int main() {
	_write(1, "nice!\nhello world...\n", 22);
	return 0xa4;
}
