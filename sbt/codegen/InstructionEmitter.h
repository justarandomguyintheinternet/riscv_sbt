#ifndef RISCV_TOOLS_INSTRUCTIONEMITTER_H
#define RISCV_TOOLS_INSTRUCTIONEMITTER_H

#include "codegen/Emitter.h"

#include <ProfilingInfo.h>
#include <decoding/Decoder.h>

#include <set>
#include <string>
#include <string_view>

// Translates guest instructions and does basic register tracking
class InstructionEmitter {
public:
    InstructionEmitter(Emitter& emitter, const ProfilingInfo& profilingInfo, const std::set<uint32_t>& leaders, uint32_t textStartAddress) : emitter(emitter), profilingInfo(profilingInfo), leaders(leaders), textStartAddress(textStartAddress) {};

    void emitInstruction(const Instruction& instruction);

private:
    void emit(std::string_view text) { emitter.emit(text); }
    // Helper to skip instructions writing to x0
    void emit(std::string_view text, uint8_t rd);

    std::string REG(InstructionField field) const;
    void emitOp(std::string_view op, std::string_view shortOp, bool ordered);
    void emitLoadSaveAddress(const Instruction& instruction);

    void track(uint8_t reg, uint32_t value);
    void resetTracked();

    Emitter& emitter;
    const ProfilingInfo& profilingInfo;
    const std::set<uint32_t>& leaders;
    uint32_t textStartAddress;

    const Instruction* current = nullptr;
    uint32_t regValues[32] = {};
    bool regKnown[32] = {};
};

#endif //RISCV_TOOLS_INSTRUCTIONEMITTER_H
