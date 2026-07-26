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
    static constexpr std::size_t RegisterCount = 32;

    using InstructionCounts = std::array<uint64_t, static_cast<std::size_t>(EInstruction::TYPE_COUNT)>;
    using RegisterCounts = std::array<uint64_t, RegisterCount>;
    using BranchDestinations = std::unordered_map<uint32_t, uint64_t>;
    using IndirectBranchTargets = std::unordered_map<uint32_t, BranchDestinations>;

    explicit ProfilingInfo(std::filesystem::path filePath, bool appendMode = false);
    ~ProfilingInfo() {
        save();
    };

    void incrementInstructionCount(EInstruction instruction);
    void recordIndirectBranch(uint32_t branchAddress, uint32_t destinationAddress);
    void recordRegisterAccess(uint32_t rs1, uint32_t rs2, uint32_t rd);

    void load();
    void save();

    const std::filesystem::path& getFilePath() const;
    const InstructionCounts& getInstructionCounts() const;
    const RegisterCounts& getRegisterAccessCounts() const;
    const IndirectBranchTargets& getAllBranchTargets() const;
    const BranchDestinations& getIndirectBranchTargets(uint32_t branchAddress) const;

private:
    std::filesystem::path filePath;
    bool appendMode;
    bool hasLoadedExistingData = false;
    std::fstream file;
    InstructionCounts instructionCounts{};
    RegisterCounts registerAccessCounts{};
    IndirectBranchTargets indirectBranchTargets;

    void openFile();
};

#endif //RISCV_TOOLS_PROFILINGINFO_H
