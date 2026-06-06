#include <iostream>
#include <iomanip>

#include "elf/ElfBinary.h"
#include "runtime/Memory.h"
#include "Interpreter.h"

#define BASE_RA 0xdeadbeef

Memory memory;

void printInfo(Context& ctx) {
    bool hasRegOutput = false;
    for (int i = 0; i < 32; ++i) {
        if (ctx.reg[i] != 0) {
            if (!hasRegOutput) {
                std::cout << "\nRegisters:\n";
                std::cout << "  idx   hex         signed\n";
                hasRegOutput = true;
            }

            std::cout << "  x" << std::dec << std::setw(2) << std::setfill('0') << i
                      << "   0x" << std::hex << std::setw(8) << std::setfill('0') << ctx.reg[i]
                      << "  " << std::dec << static_cast<int32_t>(ctx.reg[i]) << '\n';
        }
    }

    std::cout << std::dec << std::setfill(' ');
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <elf binary>" << std::endl;
        return 1;
    }

    ElfBinary binary(argv[1]);

    if (binary.load() != ElfBinary::Success) {
        std::cerr << "Failed to load elf binary" << std::endl;
        return 1;
    }

    binary.loadToMemory(memory.getMemory(), memory.getSize());

    Context ctx(memory);
    ctx.pc = binary.getEntryAddress();
    ctx.reg[2] = memory.getStackPointer();

    if (!binary.getSymbolAddress("_start").has_value()) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        ctx.reg[1] = BASE_RA; // init ra to known address, as no exit syscall will exist
        ctx.reg[3] = binary.getSymbolAddress("__global_pointer$").value_or(0); // Should usually be data.getStartAddress() + 0x800; https://groups.google.com/a/groups.riscv.org/g/sw-dev/c/60IdaZj27dY
    }

    while (true) {
        Interpreter::runInstruction(ctx);

        // baremetal return without exit syscall
        if (ctx.pc == BASE_RA) {
            std::cout << "Returned to BASE_RA" << std::endl;
            printInfo(ctx);
            return 0;
        }

        if (ctx.pc < 0x0 || ctx.pc >= memory.getSize()) {
            std::cout << "pc out of bounds" << std::hex << ctx.pc << std::endl;
            break;
        }
    }
}