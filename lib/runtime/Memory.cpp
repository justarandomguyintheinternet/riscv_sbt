#include "Memory.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

// https://articles.manugarg.com/aboutelfauxiliaryvectors
void Memory::loadAux(Auxiliary aux, bool skipSecondArg) {
    uint32_t argc = aux.argc - (skipSecondArg ? 1 : 0); // emu needs to skip second arg, as thats the path to the to be emulated file
    uint32_t argsSize = argc * sizeof(uint32_t); // size of argv pointers
    argsSize += 2 * sizeof(uint32_t); // argc and argv[n] nullptr

    uint32_t strSize = 0;

    // Figure out space needed for actual string data, to calculate SP
    for (uint32_t i = 0; i < aux.argc; i++) {
        if (i == 1 && skipSecondArg) { continue; }
        strSize += strlen(aux.argv[i]) + 1; // +1 to take null termination into account
    }

    initialSP = this->getSize() - argsSize - strSize * sizeof(char);
    this->write<uint32_t>(initialSP, argc);

    uint32_t strPtr = initialSP + argsSize;
    uint32_t argPtr = initialSP + sizeof(uint32_t);

    // Write argv string pointer + actual string data pairs into memory
    for (uint32_t i = 0; i < aux.argc; i++) {
        if (i == 1 && skipSecondArg) { continue; }

        uint32_t argLen = (strlen(aux.argv[i]) * sizeof(char)) + 1;
        this->write<uint32_t>(argPtr, strPtr);
        memcpy(this->data + strPtr, aux.argv[i], argLen);

        argPtr += sizeof(uint32_t);
        strPtr += argLen;
    }
}

uint32_t Memory::getStackPointer() const {
    return this->initialSP;
}

void* Memory::getHostAddress(uint32_t guestAddress) {
    return &(this->data[guestAddress]);
}