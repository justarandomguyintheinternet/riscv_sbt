#ifndef RISCV_EMU_ELFBINARY_H
#define RISCV_EMU_ELFBINARY_H

#include <libelf.h>
#include <vector>
#include <optional>
#include "ElfBinarySection.h"
#include "../decoding/Decoder.h"
#include <gelf.h>

#include "runtime/Memory.h"

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
    uint32_t loadToMemory(Memory& memory) const; // Returns highest address to which a value was loaded

    template<typename Container>
    void decodeToContainer(Container& container) const {
        auto textSections = getTypeSections(ElfBinarySection::Text);

        for (const auto& ref : textSections) {
            const auto& textSection = ref.get();

            printf("Loading section %s\n", textSection.getName().c_str());
            uint32_t addr = textSection.getStartAddress();

            for (const auto word : textSection.getData()) {
                Instruction inst = Decoder::decode(word, addr);

                if (inst.type == EInstruction::INVALID) {
                    printf("Invalid instruction at %p=%p\n", reinterpret_cast<void *>(addr), reinterpret_cast<void *>(word));
                }
                container.push_back(inst);

                addr += 4;
            }
        }
    }

    const std::vector<ElfBinarySection>& getSections() const { return sections; }
    std::vector<std::reference_wrapper<const ElfBinarySection>> getTypeSections(ElfBinarySection::SectionType type) const;
    std::optional<std::reference_wrapper<const ElfBinarySection>> getSection(ElfBinarySection::SectionType type) const;
    std::optional<std::reference_wrapper<const ElfBinarySection>> getSection(std::string_view name) const;
    std::optional<uint32_t> getSymbolAddress(const char* symbolName) const;
    int32_t getEntryAddress() const;

    uint32_t getTextStartAddress() const { return textStartAddress; } // Lowest start address among executable sections
    uint32_t getTextEndAddress() const { return textEndAddress; } // Highest end address among executable sections, exclusive
    uint32_t getTextWordCount() const { return (textEndAddress - textStartAddress) / 4; } // Words spanned, including any gaps between sections

private:
    void decode();
    void computeTextBounds();

    Elf* elf;
    const char* filename;
    int fd;
    std::vector<ElfBinarySection> sections;
    uint32_t textStartAddress = 0;
    uint32_t textEndAddress = 0;
};

#endif //RISCV_EMU_ELFBINARY_H
