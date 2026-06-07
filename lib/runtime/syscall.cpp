#include "syscall.h"
#include <iostream>

void Syscall::handleError(Context& ctx) {
    if (ctx.reg[a0] < 0) {
        ctx.reg[a0] = -errno;
    }
}

// todo: figure out a way / check how to make compiler emit only asm for correct syscall, if syscall num is known (during SBT)
void Syscall::handle(Context& ctx) {
    handle(ctx, ctx.reg[a7]);
}

void Syscall::handle(Context& ctx, uint32_t num) {
    switch (num) {
        case 64: // write
            _write(ctx);
            break;
        case 93: // exit
        case 94: // exit_group
            _exit(ctx);
            break;
        case 66: { // writev https://man7.org/linux/man-pages/man3/writev.3p.html
            _writev(ctx);
            break;
        }
        case 214: {
            _brk(ctx);
            break;
        }
        case 222: {
            _mmap(ctx);
            break;
        }
        case 403: {
            _clock_gettime(ctx);
            break;
        }
        default:
            std::cout << "Unknown ecall with code " << num << std::endl;
            ctx.reg[a0] = -1;
    }
}
