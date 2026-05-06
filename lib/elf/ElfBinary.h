#ifndef RISCV_EMU_ELFBINARY_H
#define RISCV_EMU_ELFBINARY_H

#include <libelf.h>
#include <vector>
#include <optional>
#include "ElfBinarySection.h"
#include "../decoding/Decoder.h"
#include <gelf.h>

class ElfBinary {
public:
    explicit ElfBinary(const char* filename) : elf(nullptr), filename(filename), fd(0) {};
    ~ElfBinary();

    enum LoadResult {
        Success,
        FileNotFound,
        NotAnElf,
        LoadError,
        NotRISCV
    };

    LoadResult load();
    void loadToMemory(uint8_t* memory) const;
    void decodeToMemory(Instruction* memory) const;
    const std::vector<ElfBinarySection>& getSections() const { return sections; }
    std::optional<std::reference_wrapper<const ElfBinarySection>> getSection(ElfBinarySection::SectionType type) const;
    std::optional<uint32_t> getSymbolAddress(const char* symbolName) const;

private:
    void decode();

    Elf* elf;
    const char* filename;
    int fd;
    std::vector<ElfBinarySection> sections;
};

#endif //RISCV_EMU_ELFBINARY_H
