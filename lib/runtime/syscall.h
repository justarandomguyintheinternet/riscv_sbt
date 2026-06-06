#ifndef RISCV_TOOLS_SYSCALL_H
#define RISCV_TOOLS_SYSCALL_H

#include <runtime/Context.h>
#include <cstdlib>
#include "registers.h"

namespace Syscall {
    void handle(Context& ctx);

    // Specific handlers, inline small ones (?)
    inline void _exit(Context& ctx) {
        exit(static_cast<uint8_t>(ctx.reg[a0]));
    };
}

#endif //RISCV_TOOLS_SYSCALL_H
