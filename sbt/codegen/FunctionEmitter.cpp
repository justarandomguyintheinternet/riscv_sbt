#include "codegen/FunctionEmitter.h"

#include <format>

void FunctionEmitter::emitForwardDeclarations() {
    emitter.setIndent(0);

    for (const auto& function : functionMap.all()) {
        emitter.emit(std::format("static uint32_t {}();\n", function.name));
    }

    emitter.emit("\n");
}

void FunctionEmitter::emitFunctionOpen(const LiftedFunction& function) {
    emitter.setIndent(0);
    emitter.emitFilledTemplate("lifted_function_open", {
        {"NAME", function.name},
    });

    emitLocalDispatch(function);
    emitEntryDispatch(function);
}

// Resume at whichever block the dispatcher handed us instead of at the first one
void FunctionEmitter::emitEntryDispatch(const LiftedFunction& function) {
    if (!function.entryDispatch) {
        return;
    }

    emitter.setIndent(1);
    emitter.emit("target = ctx.pc;\n");
    emitter.emit(std::format("dispatchIndex = (target - 0x{:X}) / 4;\n", function.start));
    emitter.emit(std::format("if (dispatchIndex >= {}) {{ goto ESCAPE; }}\n", function.wordCount()));
    emitter.emit("goto *localDispatch[dispatchIndex];\n");
}

// Label table covering this function's own address range, the only place computed goto survives
void FunctionEmitter::emitLocalDispatch(const LiftedFunction& function) {
    if (!function.hasLocalDispatch) {
        return;
    }

    emitter.setIndent(0);
    emitter.emitFilledTemplate("local_dispatch_open", {
        {"SPAN", std::format("{}", function.wordCount())},
    });

    for (uint32_t address = function.start; address < function.end; address += 4) {
        emitter.emit(leaders.contains(address) ? std::format("\t\t&&L{:X},\n", address) : "\t\t&&ESCAPE,\n");
    }

    emitter.emitTemplate("local_dispatch_close");
}

void FunctionEmitter::emitFunctionClose(const LiftedFunction& function, const Instruction* last) {
    emitter.setIndent(2);

    // Running off the end of the range means falling through into whatever follows it
    if (!endsControlFlow(function, last)) {
        if (functionMap.isSplit() && functionMap.isEntry(function.end)) {
            emitter.emit(std::format("return {}();\n", functionMap.lookup(function.end)->name));
        } else {
            emitter.emit(std::format("return 0x{:X};\n", function.end));
        }
    }

    if (function.hasLocalDispatch) {
        emitter.setIndent(1);
        emitter.emit("ESCAPE:\n");
        emitter.setIndent(2);
        emitter.emit("return target;\n");
    }

    emitter.setIndent(0);
    emitter.emit("}\n");
}

bool FunctionEmitter::endsControlFlow(const LiftedFunction& function, const Instruction* last) const {
    if (last == nullptr || last->address + 4 != function.end) {
        return false; // nothing decoded at the tail of the range, so control could reach it
    }

    switch (classifyTransfer(*last)) {
        case TransferKind::DirectJump:
        case TransferKind::IndirectJump:
        case TransferKind::Return:
            return true;
        case TransferKind::Call:
        case TransferKind::IndirectCall:
            return !functionMap.isSplit(); // unsplit these are jumps, split they come back and fall through
        default:
            return false;
    }
}
