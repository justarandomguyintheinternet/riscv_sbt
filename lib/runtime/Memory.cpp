#include "Memory.h"
#include <cstdint>
#include <cstring>

void Memory::loadAux(Auxiliary& aux) {
    uint32_t argsSize = 0;

    for (int i = 0; i < aux.argc; i++) {
        argsSize += strlen(aux.argv[i]) + 1;
    }

    argsSize += 2 * sizeof(uint32_t); // argc + argv null pointer

    initialSP = this->getSize() - argsSize;
    // todo actually load aux stuff onto stack
}

uint32_t Memory::getStackPointer() const {
    return this->initialSP;
}

void* Memory::getHostAddress(uint32_t guestAddress) {
    return &(this->data[guestAddress]);
}