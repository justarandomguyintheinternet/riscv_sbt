#include <cstring>
#include <iostream>
#include <iomanip>

#include "elf/ElfBinary.h"
#include "runtime/Memory.h"
#include "Interpreter.h"
#include "ProfilingInfo.h"
#include "runtime/registers.h"

#define BASE_RA 0xdeadbeef

#define PREDECODE 1
#define THREADING 1
// #define DIRECT_THREADING 1 Located in Interpreter.cpp
#define SWITCH 1

Memory memory;
ProfilingInfo info("./profiling.json", false);

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

int main(int argc, char** argv, char** envp) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <elf binary>" << std::endl;
        return 1;
    }

    ElfBinary binary(argv[1]);

    if (binary.load() != ElfBinary::Success) {
        std::cerr << "Failed to load elf binary" << std::endl;
        return 1;
    }

    uint32_t dataEnd = binary.loadToMemory(memory);

    strcpy(argv[0], argv[1]); // some programs like busybox check their own name and might not work correctly otherwise
    memory.loadAux(Auxiliary{ .argc = argc, .argv = argv, .envp = envp }, true);
    memory.initializeHeap(dataEnd);

    Context ctx(memory);
    ctx.pc = binary.getEntryAddress();
    ctx.reg[2] = memory.getStackPointer();

    if (!binary.getSymbolAddress("_start").has_value()) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        ctx.reg[ra] = BASE_RA; // init ra to known address, as no exit syscall will exist
        ctx.reg[gp] = binary.getSymbolAddress("__global_pointer$").value_or(0); // Should usually be data.getStartAddress() + 0x800; https://groups.google.com/a/groups.riscv.org/g/sw-dev/c/60IdaZj27dY
    }

    Interpreter::activeProfilingInfo = &info;

#ifdef PREDECODE
    const uint32_t textStartAddress = binary.getTextStartAddress();

    // Default init to INVALID instruction, so that gaps are invalid (if it somehow manages to jump there)
    std::vector<Instruction> instructions(binary.getTextWordCount(), Instruction{EInstruction::INVALID});

    std::vector<Instruction> iVec;
    binary.decodeToContainer(iVec);
    for (auto inst : iVec) {
        instructions[(inst.address - textStartAddress) / 4] = inst;
    }
#endif

    while (true) {
#if PREDECODE == 1
    #if THREADING == 1
            Interpreter::runInstructionsThreaded(ctx, instructions, textStartAddress);
    #else
            Interpreter::runInstructionPredecoded(ctx, instructions[(ctx.pc - textStartAddress) / 4]);
    #endif
#elif SWITCH == 1
        Interpreter::runInstructionSwitch(ctx);
#else
        Interpreter::runInstruction(ctx);
#endif

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
