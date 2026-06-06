#ifndef RISCV_TOOLS_CONTEXT_H
#define RISCV_TOOLS_CONTEXT_H

#include <cstdint>
#include <runtime/Memory.h>

struct Context {
    uint32_t reg[32] = { 0 };
    uint32_t pc{};
    Memory& memory;
    explicit Context(Memory& mem) : memory(mem) {};
};

#endif //RISCV_TOOLS_CONTEXT_H
