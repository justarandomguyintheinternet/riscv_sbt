#ifndef RISCV_TOOLS_RUNTIMEEMITTER_H
#define RISCV_TOOLS_RUNTIMEEMITTER_H

#include "analysis/Functions.h"
#include "codegen/Emitter.h"

#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>

#include <set>
#include <vector>

// Emits everything around the main translated instructions
class RuntimeEmitter {
public:
    RuntimeEmitter(Emitter& emitter, const ElfBinary& binary, const FunctionMap& functionMap, const std::set<uint32_t>& leaders)
        : emitter(emitter), binary(binary), functionMap(functionMap), leaders(leaders) {};

    void emitPrologue();
    void emitFunctionTable();
    void emitTopLevelDispatcher();
    void emitGeneratedMain(const std::vector<Instruction>& instructions);

private:
    // Used for baremetal
    uint32_t getBaseRa() const { return binary.getTextEndAddress(); }
    // One slot per instruction word plus the BASE_RA sentinel
    uint32_t getTableSize() const { return binary.getTextWordCount() + 1; }

    Emitter& emitter;
    const ElfBinary& binary;
    const FunctionMap& functionMap;
    const std::set<uint32_t>& leaders;
};

#endif //RISCV_TOOLS_RUNTIMEEMITTER_H
