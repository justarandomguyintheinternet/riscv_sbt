#ifndef RISCV_TOOLS_MEMORY_H
#define RISCV_TOOLS_MEMORY_H

#include <cstdint>

#define DATA_SIZE 0x800000 // todo: allocate either Memory instance or internal memory on heap, must be stored global otherwise

struct Auxiliary {
    int argc;
    char** argv;
    char** envp;
};

class Memory {
public:
    explicit Memory() : data{}, initialSP(DATA_SIZE - 1), heapEnd(DATA_SIZE / 2) {};

    inline uint8_t read(uint32_t address) { return data[address]; };
    uint8_t& operator[](uint32_t n) {
        return data[n];
    }

    template<typename T>
    inline void write(uint32_t address, uint32_t value) {
        *reinterpret_cast<T *>(&data[address]) = static_cast<T>(value);
    };
    template<typename T>
    inline T read(uint32_t address) {
        return *reinterpret_cast<T *>(&data[address]);
    }; // Read the specified amount of data

    void loadAux(Auxiliary aux, bool skipSecondArg);
    uint32_t writeAuxValue(uint32_t type, uint32_t value, uint32_t address); // Write a single aux pair to the stack
    uint32_t getStackPointer() const; // Initial stack pointer after loading aux
    void* getHostAddress(uint32_t guestAddress);

    uint32_t getGuestAddress(void *hostAddress);

    inline uint8_t* getMemory() { return data; }

    static uint32_t getSize() { return DATA_SIZE; }

    uint32_t getHeapEnd() const { return this->heapEnd; }
    void setHeapEnd(uint32_t end) { this->heapEnd = end; }
    void initializeHeap(uint32_t base);
    uint32_t getHeapBase() const { return this->heapBase; }
    uint64_t getPageSize() const { return this->pageSize; }
private:
    uint8_t data[DATA_SIZE];
    uint32_t initialSP;
    uint32_t heapEnd;
    uint32_t heapBase;
    uint64_t pageSize;
};

#endif //RISCV_TOOLS_MEMORY_H
