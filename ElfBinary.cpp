#include "ElfBinary.h"
#include <gelf.h>
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

        const Elf_Data* data = elf_getdata(section, nullptr);
        if (!data) {
            continue;
        }

        std::vector<uint32_t> sectionData;
        const size_t count = data->d_size / sizeof(uint32_t);
        const auto* rawData = static_cast<uint32_t*>(data->d_buf);
        
        for (size_t i = 0; i < count; ++i) {
            sectionData.push_back(rawData[i]);
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
