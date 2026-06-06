#include "elf/ElfBinary.h"
#include "runtime/syscallMap.h"
#include <iostream>
#include <fstream>

std::vector<Instruction> instructions;

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

    binary.decodeToContainer(instructions);

    int last_syscall = 0;
    for (auto& instruction : instructions) {
        if (instruction.type == EInstruction::ADDI && instruction.rd == 17 && instruction.rs1 == 0) {
            last_syscall = instruction.immediate;
        }

        if (instruction.type == EInstruction::ECALL) {
            printf("0x%08x: %d / %s\n", instruction.address, last_syscall, syscallToString(last_syscall).c_str());
        }
    }

    return 0;
}