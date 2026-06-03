#ifndef RISCV_TOOLS_SYSCALLS_H
#define RISCV_TOOLS_SYSCALLS_H

// https://github.com/picolibc/picolibc/blob/main/doc/os.md#stdinstdoutstderr
void _exit(int status);
int _write(int fd, const void* buf, int count);

#endif //RISCV_TOOLS_SYSCALLS_H
