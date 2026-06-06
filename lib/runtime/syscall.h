#ifndef RISCV_TOOLS_SYSCALL_H
#define RISCV_TOOLS_SYSCALL_H

#include <runtime/Context.h>
#include <cstdlib>
#include <unistd.h>

#include "registers.h"

namespace Syscall {
    void handle(Context& ctx);
    void handle(Context& ctx, uint32_t num);

    // Specific handlers, inline small ones (?)
    inline void _exit(Context& ctx) {
        exit(static_cast<uint8_t>(ctx.reg[a0]));
    };

    inline void _write(Context& ctx) {
        ctx.reg[a0] = write(static_cast<uint8_t>(ctx.reg[a0]), ctx.memory.getHostAddress(ctx.reg[a1]), ctx.reg[a2]);
    }

    inline void _writev(Context& ctx) {
        uint32_t fd = ctx.reg[a0];
        uint32_t iov = ctx.reg[a1]; // base address of io vector, each entry consists of pointer to start of string to output, and length (2x uint32_t)
        uint32_t iovcnt = ctx.reg[a2]; // number of entries in io vector

        ssize_t total = 0;
        for (uint32_t i = 0; i < iovcnt; i++) {
            uint32_t base = ctx.memory.read<uint32_t>(iov + i * 2 * sizeof(uint32_t));
            uint32_t len  = ctx.memory.read<uint32_t>(iov + i * 2 * sizeof(uint32_t) + sizeof(uint32_t));
            write(fd, ctx.memory.getHostAddress(base), len);
            total += len;
        }
        ctx.reg[a0] = total;
    }
}

#endif //RISCV_TOOLS_SYSCALL_H
