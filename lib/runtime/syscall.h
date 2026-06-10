#ifndef RISCV_TOOLS_SYSCALL_H
#define RISCV_TOOLS_SYSCALL_H

#include <cassert>
#include <runtime/Context.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <sys/mman.h>

#include "registers.h"

namespace Syscall {
    void handle(Context& ctx);
    void handle(Context& ctx, uint32_t num);
    void handleError(Context& ctx);

    // Specific handlers, inline small ones (?)
    inline void _exit(Context& ctx) {
        exit(static_cast<uint8_t>(ctx.reg[a0]));
    };

    inline void _write(Context& ctx) {
        ctx.reg[a0] = write(static_cast<uint8_t>(ctx.reg[a0]), ctx.memory.getHostAddress(ctx.reg[a1]), ctx.reg[a2]);
        handleError(ctx);
    }

    inline void _read(Context& ctx) {
        ctx.reg[a0] = read(static_cast<uint8_t>(ctx.reg[a0]), ctx.memory.getHostAddress(ctx.reg[a1]), ctx.reg[a2]);
        handleError(ctx);
    }

    inline void _writev(Context& ctx) {
        uint32_t fd = ctx.reg[a0];
        uint32_t iov = ctx.reg[a1]; // base address of io vector, each entry consists of pointer to start of string to output, and length (2x uint32_t)
        uint32_t iovcnt = ctx.reg[a2]; // number of entries in io vector

        ctx.reg[a0] = 0;
        for (uint32_t i = 0; i < iovcnt; i++) {
            uint32_t base = ctx.memory.read<uint32_t>(iov + i * 2 * sizeof(uint32_t));
            uint32_t len  = ctx.memory.read<uint32_t>(iov + i * 2 * sizeof(uint32_t) + sizeof(uint32_t));
            ctx.reg[a0] += write(fd, ctx.memory.getHostAddress(base), len);

            handleError(ctx);
        }
    }

    inline void _brk(Context& ctx) {
        if (ctx.reg[a0] == 0 || ctx.reg[a0] < ctx.memory.getHeapBase()) {
            ctx.reg[a0] = ctx.memory.getHeapEnd();
            return;
        }

        if (ctx.reg[0] > ctx.reg[sp]) { // hitting the stack
            ctx.reg[a0] = ctx.memory.getHeapEnd();
        } else {
            ctx.memory.setHeapEnd(ctx.reg[a0]);
        }
    }

    inline void _mmap(Context& ctx) {
        bool placeFreely = ctx.reg[a0] == 0; // If specific address is given, no need to keep track of it

        if (placeFreely) {
            ctx.reg[a0] = ctx.memory.getMmapAddress(ctx.reg[a1]);
        }

        assert(reinterpret_cast<uint64_t>(ctx.memory.getHostAddress(ctx.reg[a0])) % ctx.memory.getPageSize() == 0);

        // use MAP_FIXED to force kernel to map directly into the guest memory
        void* address = mmap(ctx.memory.getHostAddress(ctx.reg[a0]), ctx.reg[a1], ctx.reg[a2], ctx.reg[a3] | MAP_FIXED, ctx.reg[a4], ctx.reg[a5]);

        if (address == MAP_FAILED) {
            ctx.reg[a0] = -1;
        } else {
            ctx.reg[a0] = ctx.memory.getGuestAddress(address);

            if (placeFreely) {
                ctx.memory.reserveMmapSpace(ctx.reg[a1]);
            }
        }
        handleError(ctx);
    }

    inline void _munmap(Context& ctx) {
        ctx.memory.freeMmapSpace(ctx.reg[a0], ctx.reg[a1]);
        ctx.reg[a0] = munmap(ctx.memory.getHostAddress(ctx.reg[a0]), ctx.reg[a1]);
        handleError(ctx);
    }

    // https://man7.org/linux/man-pages/man3/clock_gettime.3.html
    inline void _clock_gettime64(Context& ctx) {
        timespec ts{};

        ctx.reg[a0] = clock_gettime(ctx.reg[a0], &ts);

        // rv32 also uses 64 bit time
        ctx.memory.write<uint64_t>(ctx.reg[a1], static_cast<uint64_t>(ts.tv_sec));
        ctx.memory.write<uint64_t>(ctx.reg[a1] + sizeof(uint64_t), static_cast<uint64_t>(ts.tv_nsec));

        handleError(ctx);
    }

    // https://man7.org/linux/man-pages/man2/openat2.2.html
    inline void _openat(Context& ctx) {
        ctx.reg[a0] = openat(ctx.reg[a0], static_cast<const char *>(ctx.memory.getHostAddress(ctx.reg[a1])), ctx.reg[a2]);
        handleError(ctx);
    }

    // https://man7.org/linux/man-pages/man2/statx.2.html, really neat because its struct uses all fixed size types
    inline void _statx(Context& ctx) {
        // this should be fine even for unaligned pointer addresses, assuming target arch allows unaligned mem access (like x86)
        ctx.reg[a0] = statx(
            ctx.reg[a0],
            static_cast<const char*>(ctx.memory.getHostAddress(ctx.reg[a1])), // path
            ctx.reg[a2],
            ctx.reg[a3],
            static_cast<struct statx*>(ctx.memory.getHostAddress(ctx.reg[a4])) // statx struct pointer to be written to
        );

        handleError(ctx);
    }
}

#endif //RISCV_TOOLS_SYSCALL_H
