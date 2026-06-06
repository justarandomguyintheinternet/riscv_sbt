#ifndef RISCV_TOOLS_MEMORY_H
#define RISCV_TOOLS_MEMORY_H

#include <cstdint>

#define DATA_SIZE 0x800000 // todo: allocate either Memory instance or internal memory on heap, must be stored global otherwise

struct Auxiliary {
    uint8_t argc;
    char** argv;
};

class Memory {
public:
    explicit Memory() : data{}, initialSP(DATA_SIZE - 1) {};

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

    void loadAux(Auxiliary& aux);
    uint32_t getStackPointer() const; // Initial stack pointer after loading aux
    void* getHostAddress(uint32_t guestAddress);
    inline uint8_t* getMemory() { return data; }

    static uint32_t getSize() { return DATA_SIZE; }
private:
    uint8_t data[DATA_SIZE];
    uint32_t initialSP;
};

#endif //RISCV_TOOLS_MEMORY_H
