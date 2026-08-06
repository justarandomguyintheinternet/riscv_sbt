#ifndef RISCV_TOOLS_FUNCTIONS_H
#define RISCV_TOOLS_FUNCTIONS_H

#include "Options.h"

#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>

#include <set>
#include <string>
#include <vector>

// What a control transfer does, decided from the instruction alone. The target still has to be
// resolved against the function map before it can be emitted.
enum class TransferKind {
    None,          // not a control transfer
    Branch,        // conditional, falls through when not taken
    DirectJump,    // jal with rd == x0
    Call,          // jal with a link register
    IndirectCall,  // jalr with a link register
    Return,        // jalr x0, ra, 0
    IndirectJump,  // any other jalr, in practice a switch jump table or a computed tail call
};

TransferKind classifyTransfer(const Instruction& instruction);

// One lifted host function, covering the guest addresses [start, end)
struct LiftedFunction {
    uint32_t start = 0;
    uint32_t end = 0;
    std::string name;               // unique C++ identifier, e.g. F_memcpy_11FA4
    bool hasLocalDispatch = false;  // contains an indirect jump, so it needs its own label table
    bool entryDispatch = false;     // enterable at any of its blocks, not just at start, so it dispatches on ctx.pc first
    bool synthetic = false;         // boundary recovered from the text layout instead of a symbol

    bool contains(uint32_t address) const { return address >= start && address < end; }
    uint32_t wordCount() const { return (end - start) / 4; }
};

// Partitions the text bounds into non-overlapping lifted functions, ordered by address
class FunctionMap {
public:
    static FunctionMap build(const ElfBinary& binary, const std::vector<Instruction>& instructions,
                             const Options::TranslationOptions& options);

    const LiftedFunction* lookup(uint32_t address) const; // nullptr outside the covered ranges
    bool isEntry(uint32_t address) const;

    const std::vector<LiftedFunction>& all() const { return functions; }
    // False for the degenerate single-function configuration, where calls and returns stay internal
    bool isSplit() const { return split; }

    void printSummary() const;

private:
    std::vector<LiftedFunction> functions;
    bool split = true;
};

#endif //RISCV_TOOLS_FUNCTIONS_H
