#include "ElfBinary.h"
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

ElfBinary::~ElfBinary() {
    if (elf) {
        elf_end(elf);
    }
    if (fd > 0) {
        close(fd);
    }
}

ElfBinary::LoadResult ElfBinary::load() {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        return LoadError;
    }

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        return FileNotFound;
    }

    elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (!elf) {
        close(fd);
        fd = 0;
        return LoadError;
    }

    if (elf_kind(elf) != ELF_K_ELF) {
        elf_end(elf);
        elf = nullptr;
        close(fd);
        fd = 0;
        return NotAnElf;
    }

    GElf_Ehdr header;
    if(gelf_getehdr(elf, &header) == nullptr) {
        return LoadError;
    }

    if(header.e_machine != EM_RISCV) {
        return NotRISCV;
    }

    decode();
    computeTextBounds();

    return Success;
}

// Span covering every executable section
void ElfBinary::computeTextBounds() {
    bool found = false;

    for (const auto& section : sections) {
        if (section.getType() != ElfBinarySection::Text) {
            continue;
        }

        if (!found) {
            textStartAddress = section.getStartAddress();
            textEndAddress = section.getEndAddress();
            found = true;
            continue;
        }

        textStartAddress = std::min(textStartAddress, section.getStartAddress());
        textEndAddress = std::max(textEndAddress, section.getEndAddress());
    }
}

std::optional<uint32_t> ElfBinary::getSymbolAddress(const char* symbolName) const {
    Elf_Scn* section = nullptr;

    while ((section = elf_nextscn(elf, section)) != nullptr) {
        GElf_Shdr sectionHeader;

        if (gelf_getshdr(section, &sectionHeader) != &sectionHeader) {
            continue;
        }

        if (sectionHeader.sh_type == SHT_SYMTAB) {
            Elf_Data* data = elf_getdata(section, nullptr);
            int count = sectionHeader.sh_size / sectionHeader.sh_entsize;

            for (int i = 0; i < count; i++) {
                GElf_Sym symbol;
                gelf_getsym(data, i, &symbol);

                const char* name = elf_strptr(elf, sectionHeader.sh_link, symbol.st_name);

                if (name && std::string(name) == symbolName) {
                    return symbol.st_value;
                }
            }

            return std::nullopt;
        }
    }

    return std::nullopt;
}

void ElfBinary::decode() {
    // collect program headers of type LOAD
    size_t phdrCount = 0;
    if (elf_getphdrnum(elf, &phdrCount) != 0) {
        return;
    }

    std::vector<GElf_Phdr> loadSegments;
    for (size_t i = 0; i < phdrCount; ++i) {
        GElf_Phdr phdr;
        if (gelf_getphdr(elf, static_cast<int>(i), &phdr) != &phdr) {
            continue;
        }
        if (phdr.p_type == PT_LOAD) {
            loadSegments.push_back(phdr);
        }
    }

    size_t sectionNameStringTableIndex;
    if (elf_getshdrstrndx(elf, &sectionNameStringTableIndex) != 0) {
        return;
    }

    Elf_Scn* section = nullptr;
    while ((section = elf_nextscn(elf, section)) != nullptr) {
        GElf_Shdr sectionHeader;
        if (gelf_getshdr(section, &sectionHeader) != &sectionHeader) {
            continue;
        }

        char* name = elf_strptr(elf, sectionNameStringTableIndex, sectionHeader.sh_name);
        if (!name) {
            continue;
        }

        if (!(sectionHeader.sh_flags & SHF_ALLOC)) {
            continue;
        }

        // find segment to which this section belongs, for getting the physical/load address (not virtual address)
        const GElf_Phdr* matchedSegment = nullptr;
        for (const auto& phdr : loadSegments) {
            if (sectionHeader.sh_addr >= phdr.p_vaddr &&
                sectionHeader.sh_addr < phdr.p_vaddr + phdr.p_memsz) {
                matchedSegment = &phdr;
                break;
            }
        }

        if (!matchedSegment) {
            continue;
        }

        ElfBinarySection::SegmentInfo segmentInfo {
            static_cast<uint32_t>(matchedSegment->p_vaddr),
            static_cast<uint32_t>(matchedSegment->p_paddr),
            static_cast<uint32_t>(matchedSegment->p_flags)
        };

        const Elf_Data* data = elf_getdata(section, nullptr);
        if (!data) {
            continue;
        }

        std::vector<uint32_t> sectionData;
        const size_t count = (data->d_size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
        const auto* rawData = static_cast<uint32_t*>(data->d_buf);

        for (size_t i = 0; i < count; ++i) {
            // .sbss data is null
            if (rawData == nullptr) {
                sectionData.push_back(0);
            } else {
                sectionData.push_back(rawData[i]);
            }
        }

        sections.emplace_back(std::string(name), sectionHeader.sh_addr, std::move(sectionData), segmentInfo, sectionHeader.sh_flags);
    }
}

// Returns first section with the corresponding SectionType
std::optional<std::reference_wrapper<const ElfBinarySection>> ElfBinary::getSection(ElfBinarySection::SectionType type) const {
    for (const auto& section : sections) {
        if (section.getType() == type) {
            return section;
        }
    }
    return std::nullopt;
}

std::optional<std::reference_wrapper<const ElfBinarySection>> ElfBinary::getSection(std::string_view name) const {
    for (const auto& section : sections) {
        if (section.getName() == name) {
            return section;
        }
    }
    return std::nullopt;
}

std::vector<std::reference_wrapper<const ElfBinarySection>> ElfBinary::getTypeSections(ElfBinarySection::SectionType type) const {
    std::vector<std::reference_wrapper<const ElfBinarySection>> result;

    for (const auto& section : sections) {
        if (section.getType() == type) {
            result.push_back(section);
        }
    }
    return result;
}

uint32_t ElfBinary::loadToMemory(Memory& memory) const {
    uint32_t dataEnd = 0;

    for (auto& section : getSections()) {
        printf("Loading section %s\n", section.getName().c_str());
        const auto& seg = section.getSegmentInfo();
        uint32_t addr = section.getLoadAddress();

        if (addr + section.getByteSize() > memory.getSize()) {
            printf("Section %s exceeds memory size\n", section.getName().c_str());
            continue;
        }

        for (const auto word : section.getData()) {
            memory.write<uint32_t>(addr, word);
            addr += 4;
        }

        if (addr > dataEnd) {
            dataEnd = addr;
        }
    }

    return dataEnd;
}

int32_t ElfBinary::getEntryAddress() const {
    GElf_Ehdr header;
    gelf_getehdr(elf, &header); // if we got this far, this should never fail

    return header.e_entry;
}