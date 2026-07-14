#include "ProfilingInfo.h"

#include <algorithm>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
    std::string formatAddress(uint32_t address) {
        std::ostringstream stream;
        stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << address;
        return stream.str();
    }

    uint32_t parseAddress(const std::string& value) {
        return static_cast<uint32_t>(std::stoul(value, nullptr, 0));
    }
}

ProfilingInfo::ProfilingInfo(std::filesystem::path filePath, bool appendMode)
        : filePath(std::move(filePath)), appendMode(appendMode) {
    openFile();
    if (appendMode) {
        load();
    }
}

void ProfilingInfo::incrementInstructionCount(std::string_view instructionName) {
    instructionCounts[std::string(instructionName)]++;
}

void ProfilingInfo::recordIndirectBranch(uint32_t branchAddress, uint32_t destinationAddress) {
    auto& destinations = indirectBranchTargets[branchAddress];
    if (std::find(destinations.begin(), destinations.end(), destinationAddress) == destinations.end()) {
        destinations.push_back(destinationAddress);
    }
}

void ProfilingInfo::load() {
    openFile();

    instructionCounts.clear();
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
        instructionCounts = data.at("instruction_counts").get<InstructionCounts>();
    }

    if (data.contains("indirect_branch_targets")) {
        for (const auto& [branchAddress, destinations] : data.at("indirect_branch_targets").items()) {
            indirectBranchTargets[parseAddress(branchAddress)] = destinations.get<std::vector<uint32_t>>();
        }
    }

    hasLoadedExistingData = !instructionCounts.empty() || !indirectBranchTargets.empty();

    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
}

void ProfilingInfo::save() {
    json data;
    InstructionCounts persistedInstructionCounts = instructionCounts;
    if (appendMode && hasLoadedExistingData) {
        for (auto& entry : persistedInstructionCounts) {
            entry.second /= 2;
        }
    }
    data["instruction_counts"] = std::move(persistedInstructionCounts);

    json branchTargets = json::object();
    for (const auto& [branchAddress, destinations] : indirectBranchTargets) {
        branchTargets[formatAddress(branchAddress)] = destinations;
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

const ProfilingInfo::IndirectBranchTargets& ProfilingInfo::getIndirectBranchTargets() const {
    return indirectBranchTargets;
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
