#ifndef RISCV_TOOLS_RUNTIMEEMITTER_H
#define RISCV_TOOLS_RUNTIMEEMITTER_H

#include "codegen/Emitter.h"

#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>

#include <set>
#include <vector>

// Used for baremetal, not compatible with translation chaining
inline constexpr uint32_t BASE_RA = 0xdeadbeef;

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
    Emitter& emitter;
    const ElfBinary& binary;
    const std::set<uint32_t>& leaders;
};

#endif //RISCV_TOOLS_RUNTIMEEMITTER_H
