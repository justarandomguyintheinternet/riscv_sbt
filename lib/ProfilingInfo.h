#ifndef RISCV_TOOLS_PROFILINGINFO_H
#define RISCV_TOOLS_PROFILINGINFO_H

#include "decoding/Instructions.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

class ProfilingInfo {
public:
    using InstructionCounts = std::array<uint64_t, static_cast<std::size_t>(EInstruction::TYPE_COUNT)>;
    using BranchDestinations = std::unordered_map<uint32_t, uint64_t>;
    using IndirectBranchTargets = std::unordered_map<uint32_t, BranchDestinations>;

    explicit ProfilingInfo(std::filesystem::path filePath, bool appendMode = false);
    ~ProfilingInfo() {
        save();
    };

    void incrementInstructionCount(EInstruction instruction);
    void recordIndirectBranch(uint32_t branchAddress, uint32_t destinationAddress);

    void load();
    void save();

    const std::filesystem::path& getFilePath() const;
    const InstructionCounts& getInstructionCounts() const;
    const IndirectBranchTargets& getIndirectBranchTargets() const;

private:
    std::filesystem::path filePath;
    bool appendMode;
    bool hasLoadedExistingData = false;
    std::fstream file;
    InstructionCounts instructionCounts;
    IndirectBranchTargets indirectBranchTargets;

    void openFile();
};

#endif //RISCV_TOOLS_PROFILINGINFO_H
