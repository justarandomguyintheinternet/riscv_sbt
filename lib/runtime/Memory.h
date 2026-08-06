#ifndef RISCV_TOOLS_MEMORY_H
#define RISCV_TOOLS_MEMORY_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#define DATA_SIZE 0x80000
#define MMAP_SIZE 0x80000

struct Auxiliary {
    int argc;
    char** argv;
    char** envp;
};

class Memory {
public:
    explicit Memory() : initialSP(DATA_SIZE - 1), heapEnd(DATA_SIZE / 2), heapBase(DATA_SIZE / 2), pageSize(4096) {};

    inline uint8_t read(uint32_t address) { return data[address]; };
    uint8_t& operator[](uint32_t n) {
        return data[n];
    }

    template<typename T, typename V>
    inline void write(uint32_t address, V value) {
        T tmp = static_cast<T>(value);
        std::memcpy(&data[address], &tmp, sizeof(T));
    };
    template<typename T>
    inline T read(uint32_t address) {
        T tmp;
        std::memcpy(&tmp, &data[address], sizeof(T));
        return tmp;
    }; // Read the specified amount of data

    void loadAux(Auxiliary aux, bool skipSecondArg);
    uint32_t writeAuxValue(uint32_t type, uint32_t value, uint32_t address); // Write a single aux pair to the stack
    uint32_t getStackPointer() const; // Initial stack pointer after loading aux

    void* getHostAddress(uint32_t guestAddress);
    uint32_t getGuestAddress(void *hostAddress) const;
    uint64_t pageAlignAddress(uint64_t address) const;

    inline uint8_t* getMemory() { return data; }
    static uint32_t getSize() { return DATA_SIZE; }

    uint32_t getHeapEnd() const { return this->heapEnd; }
    void setHeapEnd(uint32_t end) { this->heapEnd = end; }
    void initializeHeap(uint32_t base);
    uint32_t getHeapBase() const { return this->heapBase; }
    uint64_t getPageSize() const { return this->pageSize; }

    uint32_t getMmapAddress(uint32_t size);
    bool reserveMmapSpace(uint32_t size);
    bool freeMmapSpace(uint32_t address, uint32_t size);

    struct MappedArea {
        uint32_t base;
        uint32_t size;
    };
private:
    alignas(4096) static inline uint8_t data[DATA_SIZE + MMAP_SIZE];

    uint32_t initialSP;
    uint32_t heapEnd;
    uint32_t heapBase;
    uint64_t pageSize;
    std::vector<MappedArea> mappedAreas;
};

#endif //RISCV_TOOLS_MEMORY_H
