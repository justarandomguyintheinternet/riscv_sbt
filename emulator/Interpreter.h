#ifndef RISCV_TOOLS_INTERPRETER_H
#define RISCV_TOOLS_INTERPRETER_H

#include "runtime/Context.h"

#define LOG_INSTRUCTIONS 0

#if LOG_INSTRUCTIONS == 1
    #define LOG_INST(addr, name) Interpreter::logInstruction(addr, name)
#else
    #define LOG_INST(addr, name)
#endif

namespace Interpreter {
    void runInstruction(Context& ctx);
    void runInstruction(Context& ctx, uint32_t instruction);
    void logInstruction(uint32_t address, const char* name);
}

#endif //RISCV_TOOLS_INTERPRETER_H
