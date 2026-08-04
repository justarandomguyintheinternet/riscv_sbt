#include "analysis/BasicBlocks.h"

#include <cstdio>

std::set<uint32_t> BasicBlocks::getLeaders(const std::vector<Instruction>& instructions, const Options::TranslationOptions& options, const ProfilingInfo& profilingInfo) {
    std::set<uint32_t> leaders;

    if (instructions.empty()) {
        return leaders;
    }

    leaders.insert(instructions.front().address);

    for (auto& instruction: instructions) {
        if (instruction.type == EInstruction::BEQ || instruction.type == EInstruction::BNE ||
            instruction.type == EInstruction::BLT || instruction.type == EInstruction::BGE ||
            instruction.type == EInstruction::BLTU || instruction.type == EInstruction::BGEU ||
            instruction.type == EInstruction::JAL ) {
            leaders.insert(instruction.address + instruction.immediate);
            leaders.insert(instruction.address + 4);
        } else if (instruction.type == EInstruction::JALR) {
            leaders.insert(instruction.address + 4);
        }
    }

    if (options.useProfilingData) {
        auto branchTargets = profilingInfo.getAllBranchTargets();

        for (const auto& [address, targets] : branchTargets) {
            for (const auto& target : targets) {
                leaders.insert(target.first);
            }
        }
    }

    return leaders;
}

void BasicBlocks::harvestStaticData(const ElfBinary& binary, std::set<uint32_t>& leaders) {
    auto sections = binary.getTypeSections(ElfBinarySection::Data);

    const uint32_t textStart = binary.getTextStartAddress();
    const uint32_t textEnd = binary.getTextEndAddress();

    uint32_t discovered = 0;

    for (auto section : sections) {
        for (auto address : section.get().getData()) {
            if (address >= textStart && address < textEnd) {
                discovered += !leaders.contains(address);
                leaders.insert(address);
            }
        }
    }

    printf("Discovered %d potential jump targets from data sections\n", discovered);
}
