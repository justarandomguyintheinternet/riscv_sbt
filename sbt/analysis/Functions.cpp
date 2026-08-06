#include "analysis/Functions.h"

#include <runtime/registers.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <map>

namespace {
    // Longest identifier we are willing to paste into the output, C++ mangled names get unwieldy
    constexpr size_t maxNameLength = 64;

    struct Entry {
        std::string name;
        bool global = false;
        bool fromSymbol = false;
    };

    struct Range {
        uint32_t start;
        uint32_t end;
    };

    std::string makeName(std::string_view symbol, uint32_t address) {
        std::string result = "F_";

        for (const char c : symbol.substr(0, std::min(symbol.size(), maxNameLength))) {
            result += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        }

        if (symbol.empty()) {
            result += "sub";
        }

        // The address suffix is what actually guarantees uniqueness, static symbols repeat names across objects
        return result + std::format("_{:X}", address);
    }

    std::vector<Range> textRanges(const ElfBinary& binary) {
        std::vector<Range> ranges;

        for (const auto& ref : binary.getTypeSections(ElfBinarySection::Text)) {
            const auto& section = ref.get();

            if (section.getWordCount() > 0) {
                ranges.push_back(Range{section.getStartAddress(), section.getEndAddress()});
            }
        }

        std::sort(ranges.begin(), ranges.end(), [](const Range& a, const Range& b) { return a.start < b.start; });

        return ranges;
    }

    const Range* containingRange(const std::vector<Range>& ranges, uint32_t address) {
        for (const auto& range : ranges) {
            if (address >= range.start && address < range.end) {
                return &range;
            }
        }

        return nullptr;
    }

    // Keeps the most descriptive name when several symbols or a synthetic boundary share an address
    void insertEntry(std::map<uint32_t, Entry>& entries, uint32_t address, Entry candidate) {
        const auto it = entries.find(address);

        if (it == entries.end()) {
            entries.emplace(address, std::move(candidate));
            return;
        }

        const Entry& existing = it->second;
        const bool namesIt = !existing.fromSymbol && candidate.fromSymbol;
        const bool namesItBetter = existing.fromSymbol && candidate.fromSymbol && !existing.global && candidate.global;

        if (namesIt || namesItBetter) {
            it->second = std::move(candidate);
        }
    }
}

TransferKind classifyTransfer(const Instruction& instruction) {
    switch (instruction.type) {
        case EInstruction::BEQ:
        case EInstruction::BNE:
        case EInstruction::BLT:
        case EInstruction::BGE:
        case EInstruction::BLTU:
        case EInstruction::BGEU:
            return TransferKind::Branch;
        case EInstruction::JAL:
            return instruction.rd == 0 ? TransferKind::DirectJump : TransferKind::Call;
        case EInstruction::JALR:
            if (instruction.rd != 0) {
                return TransferKind::IndirectCall;
            }
            if (instruction.rs1 == ra && instruction.immediate == 0) {
                return TransferKind::Return;
            }
            return TransferKind::IndirectJump;
        default:
            return TransferKind::None;
    }
}

FunctionMap FunctionMap::build(const ElfBinary& binary, const std::vector<Instruction>& instructions,
                               const Options::TranslationOptions& options) {
    FunctionMap map;

    if (!options.functionSplitting) {
        map.split = false;
        map.functions.push_back(LiftedFunction{
            .start = binary.getTextStartAddress(),
            .end = binary.getTextEndAddress(),
            .name = "F_text",
            // Without calls there is nothing to enter at the top, every block is an entry into the one function
            .entryDispatch = true,
            .synthetic = true,
        });
    } else {
        const std::vector<Range> ranges = textRanges(binary);
        std::map<uint32_t, Entry> entries;

        // Every executable section starts a function, so no range ever spans the gap between two sections
        for (const auto& range : ranges) {
            insertEntry(entries, range.start, Entry{makeName({}, range.start), false, false});
        }

        for (const auto& symbol : binary.getFunctionSymbols()) {
            if (containingRange(ranges, symbol.address) != nullptr) {
                insertEntry(entries, symbol.address, Entry{makeName(symbol.name, symbol.address), symbol.global, true});
            }
        }

        // Direct calls must always land on an entry, otherwise there is no host function to call
        for (const auto& instruction : instructions) {
            if (classifyTransfer(instruction) != TransferKind::Call) {
                continue;
            }

            const uint32_t target = instruction.address + instruction.immediate;

            if (containingRange(ranges, target) != nullptr) {
                insertEntry(entries, target, Entry{makeName({}, target), false, false});
            }
        }

        const auto entryAddress = static_cast<uint32_t>(binary.getEntryAddress());
        if (containingRange(ranges, entryAddress) != nullptr) {
            insertEntry(entries, entryAddress, Entry{makeName("start", entryAddress), false, false});
        }

        // A function runs until the next entry, or until the end of its section
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            const Range* range = containingRange(ranges, it->first);
            if (range == nullptr) {
                continue;
            }

            uint32_t end = range->end;
            if (const auto next = std::next(it); next != entries.end() && next->first < end) {
                end = next->first;
            }

            map.functions.push_back(LiftedFunction{
                .start = it->first,
                .end = end,
                .name = it->second.name,
                .synthetic = !it->second.fromSymbol,
            });
        }
    }

    // An indirect jump can only be resolved against the labels of the function it sits in
    for (const auto& instruction : instructions) {
        const TransferKind kind = classifyTransfer(instruction);

        const bool needsDispatch = map.split
            ? kind == TransferKind::IndirectJump
            : kind == TransferKind::IndirectJump || kind == TransferKind::IndirectCall || kind == TransferKind::Return;

        if (!needsDispatch) {
            continue;
        }

        if (const auto* function = map.lookup(instruction.address)) {
            map.functions[function - map.functions.data()].hasLocalDispatch = true;
        }
    }

    return map;
}

const LiftedFunction* FunctionMap::lookup(uint32_t address) const {
    const auto it = std::upper_bound(functions.begin(), functions.end(), address,
        [](uint32_t value, const LiftedFunction& function) { return value < function.start; });

    if (it == functions.begin()) {
        return nullptr;
    }

    const LiftedFunction& candidate = *std::prev(it);

    return candidate.contains(address) ? &candidate : nullptr;
}

bool FunctionMap::isEntry(uint32_t address) const {
    const auto* function = lookup(address);

    return function != nullptr && function->start == address;
}

void FunctionMap::printSummary() const {
    uint32_t synthetic = 0;
    uint32_t localDispatch = 0;

    for (const auto& function : functions) {
        synthetic += function.synthetic;
        localDispatch += function.hasLocalDispatch;
    }

    printf("Lifting %zu functions (%u recovered without a symbol, %u needing a local dispatch table)\n",
           functions.size(), synthetic, localDispatch);
}
