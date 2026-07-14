#ifndef RISCV_TOOLS_PROFILINGINFO_H
#define RISCV_TOOLS_PROFILINGINFO_H

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ProfilingInfo {
public:
    using InstructionCounts = std::unordered_map<std::string, uint64_t>;
    using IndirectBranchTargets = std::unordered_map<uint32_t, std::vector<uint32_t>>;

    explicit ProfilingInfo(std::filesystem::path filePath, bool appendMode = false);
    ~ProfilingInfo() {
        save();
    };

    void incrementInstructionCount(std::string_view instructionName);
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
