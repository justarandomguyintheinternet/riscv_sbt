#ifndef RISCV_TOOLS_INSTRUCTIONEMITTER_H
#define RISCV_TOOLS_INSTRUCTIONEMITTER_H

#include "analysis/Functions.h"
#include "codegen/Emitter.h"

#include <ProfilingInfo.h>
#include <decoding/Decoder.h>

#include <set>
#include <span>
#include <string>
#include <string_view>

// Translates guest instructions and does basic register tracking
class InstructionEmitter {
public:
    InstructionEmitter(Emitter& emitter, const ProfilingInfo& profilingInfo, const FunctionMap& functionMap, const std::set<uint32_t>& leaders, uint32_t textStartAddress)
        : emitter(emitter), profilingInfo(profilingInfo), functionMap(functionMap), leaders(leaders), textStartAddress(textStartAddress) {};

    // Has to run before the instructions of `function` are emitted, decides which labels the body needs
    void beginFunction(const LiftedFunction& function, std::span<const Instruction> instructions);
    void emitInstruction(const Instruction& instruction);

private:
    void emit(std::string_view text) { emitter.emit(text); }
    // Helper to skip instructions writing to x0
    void emit(std::string_view text, uint8_t rd);

    std::string REG(InstructionField field) const;
    void emitOp(std::string_view op, std::string_view shortOp, bool ordered);
    void emitLoadSaveAddress(const Instruction& instruction);

    // Control transfer helpers, all of them assume `current` is set
    std::string transferTo(uint32_t target) const;
    void emitBranch(std::string_view condition);
    void emitDirectJump();
    void emitCall();
    void emitReturn();
    void emitIndirectTransfer(bool linking);
    void emitIndirectDispatch();
    void emitPredictedTargets();

    void track(uint8_t reg, uint32_t value);
    void resetTracked();

    Emitter& emitter;
    const ProfilingInfo& profilingInfo;
    const FunctionMap& functionMap;
    const std::set<uint32_t>& leaders;
    uint32_t textStartAddress;

    const LiftedFunction* function = nullptr;
    std::set<uint32_t> activeLabels; // labels actually branched to, emitting the rest only warns

    const Instruction* current = nullptr;
    uint32_t regValues[32] = {};
    bool regKnown[32] = {};
};

#endif //RISCV_TOOLS_INSTRUCTIONEMITTER_H
