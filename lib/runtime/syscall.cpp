#include "syscall.h"
#include <iostream>

void Syscall::handleError(Context& ctx) {
    if (static_cast<int32_t>(ctx.reg[a0]) < 0) {
        ctx.reg[a0] = -errno;
    }
}

// todo: figure out a way / check how to make compiler emit only asm for correct syscall, if syscall num is known (during SBT)
void Syscall::handle(Context& ctx) {
    handle(ctx, ctx.reg[a7]);
}

void Syscall::handle(Context& ctx, uint32_t num) {
    switch (num) {
        case 56: // openat
            _openat(ctx);
            break;
        case 57: // close
            _close(ctx);
            break;
        case 63: // read
            _read(ctx);
            break;
        case 64: // write
            _write(ctx);
            break;
        case 66: { // writev https://man7.org/linux/man-pages/man3/writev.3p.html
            _writev(ctx);
            break;
        }
        case 93: // exit
        case 94: // exit_group
            _exit(ctx);
            break;
        case 96: // set_tid_address, ignore for now, used for pthread
            ctx.reg[a0] = 0;
            break;
        case 214: {
            _brk(ctx);
            break;
        }
        case 215: {
            _munmap(ctx);
            break;
        }
        case 222: {
            _mmap(ctx);
            break;
        }
        case 291: { // statx
            _statx(ctx);
            break;
        }
        case 403: {
            _clock_gettime64(ctx);
            break;
        }
        default:
            std::cout << "Unknown ecall with code " << num << std::endl;
            ctx.reg[a0] = -1;
    }
}
