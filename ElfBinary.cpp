#include "ElfBinary.h"
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

    return Success;
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

        const Elf_Data* data = elf_getdata(section, nullptr);
        if (!data) {
            continue;
        }

        std::vector<uint32_t> sectionData;
        const size_t count = data->d_size / sizeof(uint32_t);
        const auto* rawData = static_cast<uint32_t*>(data->d_buf);

        for (size_t i = 0; i < count; ++i) {
            // .sbss data is null
            if (rawData == nullptr) {
                sectionData.push_back(0);
            } else {
                sectionData.push_back(rawData[i]);
            }
        }

        sections.emplace_back(std::string(name), sectionHeader.sh_addr, std::move(sectionData));
    }
}

std::optional<std::reference_wrapper<const ElfBinarySection>> ElfBinary::getSection(ElfBinarySection::SectionType type) const {
    for (const auto& section : sections) {
        if (section.getType() == type) {
            return section;
        }
    }
    return std::nullopt;
}

void ElfBinary::loadToMemory(uint8_t* memory) const {
    for (auto& section : getSections()) {
        printf("Loading section %s\n", section.getName().c_str());
        uint32_t addr = section.getStartAddress();

        for (const auto word : section.getData()) {
            memory[addr] = static_cast<uint8_t>(word & 0xFF);
            memory[addr + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
            memory[addr + 2] = static_cast<uint8_t>((word >> 16) & 0xFF);
            memory[addr + 3] = static_cast<uint8_t>((word >> 24) & 0xFF);
            addr += 4;
        }
    }
}
