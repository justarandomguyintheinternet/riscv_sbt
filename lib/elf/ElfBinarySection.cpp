#include "ElfBinarySection.h"
#include <gelf.h>
#include <utility>

ElfBinarySection::SectionType ElfBinarySection::resolveSectionType() const {
    if (sectionFlags & SHF_EXECINSTR) {
        return Text;
    }
    if (sectionFlags & SHF_ALLOC) {
        return Data;
    }

    return Other;
}

ElfBinarySection::ElfBinarySection(std::string name, uint64_t startAddress, std::vector<uint32_t> data, SegmentInfo segmentInfo, uint64_t sectionFlags)
    : name(std::move(name)), startAddress(startAddress), data(std::move(data)), segmentInfo(segmentInfo), sectionFlags(sectionFlags) {
    type = resolveSectionType();
}

uint32_t ElfBinarySection::getWord(uint32_t address) const {
    return data[address - startAddress];
}
