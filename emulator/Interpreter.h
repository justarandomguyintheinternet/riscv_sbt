#ifndef RISCV_TOOLS_INTERPRETER_H
#define RISCV_TOOLS_INTERPRETER_H

#include <runtime/Context.h>
#include <ProfilingInfo.h>
#include "decoding/Decoder.h"

#ifndef RISCV_EMU_PROFILING
    #define RISCV_EMU_PROFILING 0
#endif

namespace Interpreter {
    inline constexpr bool profileInstructions = RISCV_EMU_PROFILING != 0;
    inline ProfilingInfo* activeProfilingInfo = nullptr;

    void runInstruction(Context& ctx);
    void runInstruction(Context& ctx, uint32_t instruction);
    void runInstructionSwitch(Context& ctx);
    void runInstructionSwitch(Context& ctx, uint32_t instruction);
    void runInstructionPredecoded(Context& ctx, Instruction& instruction);
    void runInstructionsThreaded(Context& ctx, std::vector<Instruction>& instructions, uint32_t textStartAddress);
    void logInstruction(Context& ctx, EInstruction type);
    void logJump(Context& ctx, uint32_t target);
}

#define LOG_INST(ctx, type) \
    do { \
        if constexpr (Interpreter::profileInstructions) { \
            Interpreter::logInstruction(ctx, type); \
        } \
    } while (false)

#define LOG_JMP(ctx, target) \
    do { \
        if constexpr (Interpreter::profileInstructions) { \
            Interpreter::logJump(ctx, target); \
        } \
    } while (false)

#endif //RISCV_TOOLS_INTERPRETER_H
