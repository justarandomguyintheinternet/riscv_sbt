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

    template<typename Container>
    void decodeToContainer(Container& container) const {
        auto& section = getSection(ElfBinarySection::Text).value().get();
        printf("Loading section %s\n", section.getName().c_str());
        uint32_t addr = section.getStartAddress();

        for (const auto word : section.getData()) {
            Instruction inst = Decoder::decode(word, addr);

            if (inst.type == EInstruction::INVALID) {
                printf("Invalid instruction at %p\n", reinterpret_cast<void *>(addr));
            } else {
                container.push_back(inst);
            }

            addr += 4;
        }
    }

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
