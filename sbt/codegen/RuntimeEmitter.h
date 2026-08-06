#ifndef RISCV_TOOLS_RUNTIMEEMITTER_H
#define RISCV_TOOLS_RUNTIMEEMITTER_H

#include "codegen/Emitter.h"

#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>

#include <set>
#include <vector>

// Emits everything around the main translated instructions
class RuntimeEmitter {
public:
    RuntimeEmitter(Emitter& emitter, const ElfBinary& binary, const std::set<uint32_t>& leaders) : emitter(emitter), binary(binary), leaders(leaders) {};

    void emitPrologue();
    void emitDispatchTable();
    void emitDispatchLoopPrologue();
    void emitInterpreterFallback();
    void emitGeneratedMain(const std::vector<Instruction>& instructions);

private:
    // Used for baremetal
    uint32_t getBaseRa() const { return binary.getTextEndAddress(); }

    Emitter& emitter;
    const ElfBinary& binary;
    const std::set<uint32_t>& leaders;
};

#endif //RISCV_TOOLS_RUNTIMEEMITTER_H
