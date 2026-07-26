#include "ProfilingInfo.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
    constexpr std::array<std::string_view, static_cast<std::size_t>(EInstruction::TYPE_COUNT)> InstructionNames = {
            "ADD", "SUB", "XOR", "OR", "AND", "SLL", "SRL", "SRA", "SLT", "SLTU",
            "ADDI", "XORI", "ORI", "ANDI", "SLLI", "SRLI", "SRAI", "SLTI", "SLTIU",
            "LB", "LH", "LW", "LBU", "LHU", "SB", "SH", "SW",
            "BEQ", "BNE", "BLT", "BGE", "BLTU", "BGEU",
            "JAL", "JALR", "LUI", "AUIPC", "ECALL", "EBREAK", "FENCE",
            "MUL", "MULH", "MULHSU", "MULHU", "DIV", "DIVU", "REM", "REMU",
            "LR_W", "SC_W", "INVALID"
    };

    std::string formatAddress(uint32_t address) {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << address;
        return stream.str();
    }

    uint32_t parseAddress(const std::string& value) {
        return static_cast<uint32_t>(std::stoul(value, nullptr, 0));
    }

    std::size_t toIndex(EInstruction instruction) {
        return static_cast<std::size_t>(instruction);
    }

    std::string_view instructionName(EInstruction instruction) {
        return InstructionNames[toIndex(instruction)];
    }

    EInstruction parseInstructionName(const std::string& name) {
        for (std::size_t index = 0; index < InstructionNames.size(); ++index) {
            if (InstructionNames[index] == name) {
                return static_cast<EInstruction>(index);
            }
        }

        throw std::runtime_error("Unknown instruction name in profiling data: " + name);
    }
}

ProfilingInfo::ProfilingInfo(std::filesystem::path filePath, bool appendMode)
        : filePath(std::move(filePath)), appendMode(appendMode) {
    openFile();
    if (appendMode) {
        load();
    }
}

void ProfilingInfo::incrementInstructionCount(EInstruction instruction) {
    instructionCounts[toIndex(instruction)]++;
}

void ProfilingInfo::recordIndirectBranch(uint32_t branchAddress, uint32_t destinationAddress) {
    indirectBranchTargets[branchAddress][destinationAddress]++;
}

void ProfilingInfo::recordRegisterAccess(uint32_t rs1, uint32_t rs2, uint32_t rd) {
    if (rs1 < RegisterCount) {
        registerAccessCounts[rs1]++;
    }
    if (rs2 < RegisterCount) {
        registerAccessCounts[rs2]++;
    }
    if (rd < RegisterCount) {
        registerAccessCounts[rd]++;
    }
}

void ProfilingInfo::load() {
    openFile();

    instructionCounts.fill(0);
    registerAccessCounts.fill(0);
    indirectBranchTargets.clear();
    hasLoadedExistingData = false;

    file.clear();
    file.seekg(0, std::ios::beg);

    if (file.peek() == std::char_traits<char>::eof()) {
        file.clear();
        file.seekg(0, std::ios::beg);
        file.seekp(0, std::ios::beg);
        return;
    }

    const auto data = json::parse(file);

    if (data.contains("instruction_counts")) {
        for (const auto& [name, count] : data.at("instruction_counts").items()) {
            instructionCounts[toIndex(parseInstructionName(name))] = count.get<uint64_t>();
        }
    }

    if (data.contains("register_accesses")) {
        const auto& counts = data.at("register_accesses");
        for (std::size_t index = 0; index < std::min(counts.size(), registerAccessCounts.size()); ++index) {
            registerAccessCounts[index] = counts[index].get<uint64_t>();
        }
    }


    if (data.contains("indirect_branch_targets")) {
        for (const auto& [branchAddress, destinations] : data.at("indirect_branch_targets").items()) {
            auto& destinationCounts = indirectBranchTargets[parseAddress(branchAddress)];
            if (destinations.is_array()) {
                for (const auto& destination : destinations) {
                    destinationCounts[destination.get<uint32_t>()]++;
                }
                continue;
            }

            for (const auto& [destinationAddress, count] : destinations.items()) {
                destinationCounts[parseAddress(destinationAddress)] = count.get<uint64_t>();
            }
        }
    }

    hasLoadedExistingData = indirectBranchTargets.size() > 0;
    if (!hasLoadedExistingData) {
        const auto hasNonZeroCount = [](const auto& counts) {
            for (uint64_t count : counts) {
                if (count > 0) {
                    return true;
                }
            }
            return false;
        };

        hasLoadedExistingData = hasNonZeroCount(instructionCounts) || hasNonZeroCount(registerAccessCounts);
    }

    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
}

void ProfilingInfo::save() {
    json data;
    InstructionCounts persistedInstructionCounts = instructionCounts;
    if (appendMode && hasLoadedExistingData) {
        for (auto& count : persistedInstructionCounts) {
            count /= 2;
        }
    }

    json serializedInstructionCounts = json::object();
    for (std::size_t index = 0; index < persistedInstructionCounts.size(); ++index) {
        const auto count = persistedInstructionCounts[index];
        if (count == 0) {
            continue;
        }

        serializedInstructionCounts[std::string(instructionName(static_cast<EInstruction>(index)))] = count;
    }
    data["instruction_counts"] = std::move(serializedInstructionCounts);

    RegisterCounts persistedRegisterCounts = registerAccessCounts;
    if (appendMode && hasLoadedExistingData) {
        for (auto& count : persistedRegisterCounts) {
            count /= 2;
        }
    }
    data["register_accesses"] = persistedRegisterCounts;

    json branchTargets = json::object();
    for (const auto& [branchAddress, destinations] : indirectBranchTargets) {
        json destinationCounts = json::object();
        for (const auto& [destinationAddress, count] : destinations) {
            destinationCounts[formatAddress(destinationAddress)] = count;
        }
        branchTargets[formatAddress(branchAddress)] = std::move(destinationCounts);
    }
    data["indirect_branch_targets"] = std::move(branchTargets);

    file.close();
    file.open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open profiling file for writing: " + filePath.string());
    }

    file << data.dump(4);
    file.flush();

    file.close();
    openFile();
}

const std::filesystem::path& ProfilingInfo::getFilePath() const {
    return filePath;
}

const ProfilingInfo::InstructionCounts& ProfilingInfo::getInstructionCounts() const {
    return instructionCounts;
}

const ProfilingInfo::RegisterCounts& ProfilingInfo::getRegisterAccessCounts() const {
    return registerAccessCounts;
}

const ProfilingInfo::IndirectBranchTargets& ProfilingInfo::getAllBranchTargets() const {
    return indirectBranchTargets;
}

const ProfilingInfo::BranchDestinations& ProfilingInfo::getIndirectBranchTargets(uint32_t branchAddress) const {
    auto it = indirectBranchTargets.find(branchAddress);
    if (it != indirectBranchTargets.end()) {
        return it->second;
    }
    static const ProfilingInfo::BranchDestinations emptyDestinations;
    return emptyDestinations;
}

void ProfilingInfo::openFile() {
    if (file.is_open()) {
        return;
    }

    file.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        return;
    }

    std::ofstream createFile(filePath, std::ios::out | std::ios::binary);
    if (!createFile.is_open()) {
        throw std::runtime_error("Failed to create profiling file: " + filePath.string());
    }

    file.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open profiling file: " + filePath.string());
    }
}
