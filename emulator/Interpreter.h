#ifndef RISCV_TOOLS_INTERPRETER_H
#define RISCV_TOOLS_INTERPRETER_H

#include <runtime/Context.h>
#include <ProfilingInfo.h>

#ifndef RISCV_EMU_PROFILING
    #define RISCV_EMU_PROFILING 0
#endif

namespace Interpreter {
    inline constexpr bool profileInstructions = RISCV_EMU_PROFILING != 0;
    inline ProfilingInfo* activeProfilingInfo = nullptr;

    void runInstruction(Context& ctx);
    void runInstruction(Context& ctx, uint32_t instruction);
    void logInstruction(Context& ctx, std::string_view name);
    void logJump(Context& ctx, uint32_t target);
}

#define LOG_INST(ctx, name) \
    do { \
        if constexpr (Interpreter::profileInstructions) { \
            Interpreter::logInstruction(ctx, name); \
        } \
    } while (false)

#define LOG_JMP(ctx, target) \
    do { \
        if constexpr (Interpreter::profileInstructions) { \
            Interpreter::logJump(ctx, target); \
        } \
    } while (false)

#endif //RISCV_TOOLS_INTERPRETER_H
