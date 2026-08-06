#ifndef RISCV_TOOLS_FUNCTIONEMITTER_H
#define RISCV_TOOLS_FUNCTIONEMITTER_H

#include "analysis/Functions.h"
#include "codegen/Emitter.h"

#include <decoding/Decoder.h>

#include <set>

// Emits the host function frame the lifted basic blocks live in
class FunctionEmitter {
public:
    FunctionEmitter(Emitter& emitter, const FunctionMap& functionMap, const std::set<uint32_t>& leaders)
        : emitter(emitter), functionMap(functionMap), leaders(leaders) {};

    void emitForwardDeclarations();
    void emitFunctionOpen(const LiftedFunction& function);
    // `last` is the final instruction of the function's range, or nullptr when the range decoded to nothing
    void emitFunctionClose(const LiftedFunction& function, const Instruction* last);

private:
    void emitLocalDispatch(const LiftedFunction& function);
    void emitEntryDispatch(const LiftedFunction& function);
    bool endsControlFlow(const LiftedFunction& function, const Instruction* last) const;

    Emitter& emitter;
    const FunctionMap& functionMap;
    const std::set<uint32_t>& leaders;
};

#endif //RISCV_TOOLS_FUNCTIONEMITTER_H
