#include "ElfBinarySection.h"
#include <utility>

ElfBinarySection::SectionType ElfBinarySection::stringToType(const std::string& name) {
    if (name == ".text") {
        return Text;
    }
    if (name == ".sdata") {
        return SData;
    }
    return Other;
}

ElfBinarySection::ElfBinarySection(std::string name, uint64_t startAddress, std::vector<uint32_t> data)
    : name(std::move(name)), startAddress(startAddress), data(std::move(data)), type(stringToType(this->name)) {}

uint32_t ElfBinarySection::getWord(uint32_t address) const {
    return data[address - startAddress];
}
