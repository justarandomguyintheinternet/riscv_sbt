#ifndef RISCV_EMU_ELFBINARYSECTION_H
#define RISCV_EMU_ELFBINARYSECTION_H

#include <string>
#include <vector>
#include <cstdint>

class ElfBinarySection {
public:
    enum SectionType {
        Text,
        Data,
        Other
    };

    struct SegmentInfo {
        uint32_t virtualAddress;
        uint32_t loadAddress;
        uint32_t flags;
    };

    static SectionType stringToType(const std::string& name);

    ElfBinarySection(std::string name, uint64_t startAddress, std::vector<uint32_t> data, SegmentInfo segmentInfo);

    std::string getName() const { return name; }
    uint32_t getStartAddress() const { return startAddress; }
    const std::vector<uint32_t>& getData() const { return data; }
    SectionType getType() const { return type; }
    const SegmentInfo& getSegmentInfo() const { return segmentInfo; }
    uint32_t getWord(uint32_t address) const;
    uint32_t getSize() const { return data.size(); }

private:
    std::string name;
    uint32_t startAddress;
    std::vector<uint32_t> data;
    SectionType type;
    SegmentInfo segmentInfo;
};

#endif //RISCV_EMU_ELFBINARYSECTION_H
