#include "syscall.h"

#include <iostream>

// todo: figure out a way / check how to make compiler emit only asm for correct syscall, if syscall num is known (during SBT)
void Syscall::handle(Context& ctx) {
    switch (ctx.reg[a7]) {
        case 64: // write
            _write(ctx);
            break;
        case 93: // exit
            _exit(ctx);
            break;
        case 66: { // writev https://man7.org/linux/man-pages/man3/writev.3p.html
            _writev(ctx);
            break;
        }
        default:
            std::cout << "Unknown ecall with code " << ctx.reg[a7] << std::endl;
    }
}
