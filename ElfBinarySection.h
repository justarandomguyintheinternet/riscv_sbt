#ifndef RISCV_EMU_ELFBINARYSECTION_H
#define RISCV_EMU_ELFBINARYSECTION_H

#include <string>
#include <vector>
#include <cstdint>

class ElfBinarySection {
public:
    enum SectionType {
        Text,
        SData,
        Other
    };

    static SectionType stringToType(const std::string& name);

    ElfBinarySection(std::string name, uint64_t startAddress, std::vector<uint32_t> data);

    std::string getName() const { return name; }
    uint32_t getStartAddress() const { return startAddress; }
    const std::vector<uint32_t>& getData() const { return data; }
    SectionType getType() const { return type; }
    uint32_t getWord(uint32_t address) const;

private:
    std::string name;
    uint32_t startAddress;
    std::vector<uint32_t> data;
    SectionType type;
};

#endif //RISCV_EMU_ELFBINARYSECTION_H
