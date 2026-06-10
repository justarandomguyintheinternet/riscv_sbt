#include "Memory.h"

#include <algorithm>
#include <elf.h>
#include <cstring>

uint32_t ptrStringSize(char* address) {
    return strlen(address) + 1; // +1 to take null termination into account
}

uint32_t ptrStringSize(uint32_t address) {
    return ptrStringSize(reinterpret_cast<char*>(address));
}

uint64_t getAuxValue(uint64_t* auxv, uint32_t type) {
    // auxv is in 64 bit space, as both emu and SBT output run in 64 bit

    for (uint32_t i = 0; auxv[i] != 0; i += 2) {
        if (auxv[i] == type) {
            return auxv[i + 1];
        }
    }
    return 0;
}

uint32_t Memory::writeAuxValue(uint32_t type, uint32_t value, uint32_t address) {
    this->write<uint32_t>(address, type);
    this->write<uint32_t>(address + sizeof(uint32_t), value);
    return sizeof(uint32_t) * 2; // Amount written
}

// https://articles.manugarg.com/aboutelfauxiliaryvectors
void Memory::loadAux(Auxiliary aux, bool skipSecondArg) {
    uint32_t argc = aux.argc - (skipSecondArg ? 1 : 0); // emu needs to skip second arg, as thats the path to the to be emulated file
    uint32_t argsSize = 0;
    argsSize += 2 * sizeof(uint32_t); // argc and argv[n] nullptr
    argsSize += argc * sizeof(uint32_t); // size of argv pointers

    // Total space needed for strings from argv and envp
    uint32_t strSize = 0;

    // Space needed for argv strings
    for (uint32_t i = 0; i < aux.argc; i++) {
        if (i == 1 && skipSecondArg) { continue; }
        strSize += ptrStringSize(aux.argv[i]);
    }

    // Space needed for envp strings
    uint32_t envc = 0;
    for (uint32_t i = 0; aux.envp[i] != nullptr; i++) {
        strSize += ptrStringSize(aux.envp[i]);
        envc++;
    }

    argsSize += envc * sizeof(uint32_t); // size of envp pointers
    argsSize += sizeof(uint32_t); // envp nullptr

    // Space for auxv data
    argsSize += sizeof(Elf32_auxv_t); // AT_PAGESZ
    argsSize += sizeof(Elf32_auxv_t); // AT_NULL

    initialSP = this->getSize() - argsSize - strSize * sizeof(char);
    initialSP = initialSP - (initialSP % 2); // 16-bit align stack
    this->write<uint32_t>(initialSP, argc);

    // For keeping track of where we are writing string data and "data" data (pointers to strings, argc, null terminators and auxv pairs)
    uint32_t strPtr = initialSP + argsSize;
    uint32_t dataPtr = initialSP + sizeof(uint32_t);

    // Write argv string pointer + actual string data pairs into memory
    for (uint32_t i = 0; i < aux.argc; i++) {
        if (i == 1 && skipSecondArg) { continue; }

        this->write<uint32_t>(dataPtr, strPtr);
        uint32_t argLen = ptrStringSize(aux.argv[i]);
        memcpy(this->data + strPtr, aux.argv[i], argLen);

        dataPtr += sizeof(uint32_t);
        strPtr += argLen;
    }

    dataPtr += sizeof(uint32_t); // argv nullptr

    // Write envp string pointer + actual string data pairs into memory
    for (uint32_t i = 0; i < envc; i++) {
        this->write<uint32_t>(dataPtr, strPtr);
        uint32_t argLen = ptrStringSize(aux.envp[i]);
        memcpy(this->data + strPtr, aux.envp[i], argLen);

        dataPtr += sizeof(uint32_t);
        strPtr += argLen;
    }

    dataPtr += sizeof(uint32_t); // envp nullptr

    // Write auxv data
    auto* auxv = reinterpret_cast<uint64_t*>(aux.envp);
    while (*auxv++ != 0) {};

    this->pageSize = getAuxValue(auxv, AT_PAGESZ);
    dataPtr += writeAuxValue(AT_PAGESZ, this->pageSize, dataPtr);
    writeAuxValue(AT_NULL, getAuxValue(auxv, AT_NULL), dataPtr);
}

uint32_t Memory::getStackPointer() const {
    return this->initialSP;
}

void* Memory::getHostAddress(uint32_t guestAddress) {
    return &(this->data[guestAddress]);
}

uint32_t Memory::getGuestAddress(void* hostAddress) const {
    return static_cast<uint32_t>(static_cast<uint8_t *>(hostAddress) - data);
}

uint64_t Memory::pageAlignAddress(uint64_t address) const {
    return (address + (pageSize - 1)) & ~(pageSize - 1);
}

void Memory::initializeHeap(uint32_t base) {
    base = pageAlignAddress(base);

    this->heapBase = base;
    this->heapEnd = base;
}

// Get the next free and page aligned address where memory of size can be mapped to
uint32_t Memory::getMmapAddress(uint32_t size) {
    for (auto it = mappedAreas.begin(); it != mappedAreas.end();) {
        uint32_t previousEnd = it->base + it->size;
        previousEnd = pageAlignAddress(previousEnd);

        if (++it == mappedAreas.end()) {
            return previousEnd;
        }

        if (it->base - previousEnd >= size) {
            return previousEnd;
        }
    }

    return pageAlignAddress(DATA_SIZE);
}

bool Memory::reserveMmapSpace(uint32_t size) {
    uint32_t address = getMmapAddress(size);

    if (address + size > DATA_SIZE + MMAP_SIZE) {
        return false;
    }

    mappedAreas.push_back({ address, size });
    std::ranges::sort(mappedAreas, [](const MappedArea& a, const MappedArea& b) { return a.base < b.base; });

    return true;
}

bool Memory::freeMmapSpace(uint32_t address, uint32_t size) {
    auto it = std::ranges::find_if(mappedAreas, [address](const MappedArea& area) { return area.base == address; });

    // todo: actually handle cases where an area is unmapped which was not mapped as a single block (splitting / removing only parts of mappings)
    if (it == mappedAreas.end() || it->size != size) {
        return false;
    }

    mappedAreas.erase(it);
    return true;
}