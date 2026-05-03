#include <iostream>
#include <bitset>
#include <iomanip>
#include "ElfBinary.h"

uint8_t mem[0x80000];
uint32_t reg[32] = { 0 };
uint32_t pc = 0;

#define BASE_RA 0xdeadbeef

uint32_t inline instIdx(uint32_t address) {
    return address >> 2;
}

uint32_t inline readWord(uint32_t address) {
    return static_cast<uint32_t>(mem[address])
        | (static_cast<uint32_t>(mem[address + 1]) << 8)
        | (static_cast<uint32_t>(mem[address + 2]) << 16)
        | (static_cast<uint32_t>(mem[address + 3]) << 24);
}

// Extract minor opcode, 3 bit
uint8_t getFunct3(uint32_t instruction) {
    return (instruction >> 12) & 0x7;
}

// Extra distinction for R formated instructions, 7 bit
uint8_t getFunct7(uint32_t instruction) {
    return (instruction >> 25) & 0x7f;
}

// 7 bit
uint8_t getOpcode(uint32_t instruction) {
    return instruction & 0x7f;
}

// 5 bit register adresses
uint8_t getRS1(uint32_t instruction) {
    return (instruction >> 15) & 0x1f;
}
uint8_t getRS2(uint32_t instruction) {
    return (instruction >> 20) & 0x1f;
}
uint8_t getRD(uint32_t instruction) {
    return (instruction >> 7) & 0x1f;
}

int32_t I_FMT_imm(uint32_t instruction) {
    return static_cast<int32_t>(instruction & 0xFFF00000) >> 20;
}

int32_t S_FMT_imm(uint32_t instruction) {
    uint32_t imm = ((instruction >> 7) & 0x1f) | (((instruction >> 25) & 0x7f) << 5);
    if (imm & 0x800) {
        imm |= 0xfffff000;
    }
    return static_cast<int32_t>(imm);
}

int32_t B_FMT_imm(uint32_t instruction) {
    uint32_t imm =
        ((instruction >> 31) & 0x1) << 12 |
        ((instruction >> 7) & 0x1) << 11 |
        ((instruction >> 25) & 0x3F) << 5 |
        ((instruction >> 8) & 0xF) << 1;

    return (static_cast<int32_t>(imm) << 19) >> 19;
}

int32_t J_FMT_imm(uint32_t instruction) {
    uint32_t imm =
        ((instruction >> 31) & 0x1)  << 20 |
        ((instruction >> 12) & 0xFF) << 12 |
        ((instruction >> 20) & 0x1)  << 11 |
        ((instruction >> 21) & 0x3FF) << 1;

    return (static_cast<int32_t>(imm) << 11) >> 11;
}

int32_t U_FMT_imm(uint32_t instruction) {
    return (static_cast<int32_t>(instruction & 0xFFFFF000)) >> 12;
}

uint8_t getShift(uint32_t instruction) {
    return (instruction >> 20) & 0x1f;
}

uint8_t getSHType(uint32_t instruction) {
    return (instruction >> 25) & 0x7f;
}

void printInstruction(uint32_t instruction) {
    std::cout << "Instruction: " << std::hex << instruction << std::endl;
    std::cout << "\tRaw: " << std::bitset<32>(instruction) << std::endl;
    std::cout << "\tOpcode: " << std::bitset<7>(getOpcode(instruction)) << std::endl;
    std::cout << "\tfunct3: " << std::bitset<3>(getFunct3(instruction)) << std::endl;
    std::cout << "\tfunc7: " << std::bitset<7>(getFunct7(instruction)) << std::endl;
    std::cout << "\trs1: " << std::bitset<5>(getRS1(instruction)) << std::endl;
    std::cout << "\trs2: " << std::bitset<5>(getRS2(instruction)) << std::endl;
    std::cout << "\trd: " << std::bitset<5>(getRD(instruction)) << std::endl;
    std::cout << "\tI_FMT_imm: " << std::bitset<11>(I_FMT_imm(instruction)) << std::endl;
    std::cout << "\tS_FMT_imm: " << std::bitset<11>(S_FMT_imm(instruction)) << std::endl;
}

void printInfo() {
    bool hasRegOutput = false;
    for (int i = 0; i < 32; ++i) {
        if (reg[i] != 0) {
            if (!hasRegOutput) {
                std::cout << "\nRegisters:\n";
                std::cout << "  idx   hex         signed\n";
                hasRegOutput = true;
            }

            std::cout << "  x" << std::dec << std::setw(2) << std::setfill('0') << i
                      << "   0x" << std::hex << std::setw(8) << std::setfill('0') << reg[i]
                      << "  " << std::dec << static_cast<int32_t>(reg[i]) << '\n';
        }
    }

    bool hasMemOutput = false;
    for (int i = 0; i < 32; ++i) {
        if (mem[i] != 0) {
            if (!hasMemOutput) {
                std::cout << "\nMemory:\n";
                std::cout << "  addr    hex   signed\n";
                hasMemOutput = true;
            }

            std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0') << i
                      << "   0x" << std::setw(2) << static_cast<unsigned>(mem[i])
                      << "   " << std::dec << static_cast<int>(static_cast<int8_t>(mem[i])) << '\n';
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

    binary.loadToMemory(mem);

    pc = binary.getSymbolAddress("main").value_or(0);
    reg[1] = BASE_RA; // init ra
    reg[2] = sizeof(mem); // init sp
    reg[3] = binary.getSymbolAddress("__global_pointer$").value_or(0); // Should usually be data.getStartAddress() + 0x800; https://groups.google.com/a/groups.riscv.org/g/sw-dev/c/60IdaZj27dY

    while (true) {
        reg[0] = 0;

        uint32_t instruction = readWord(pc);
        uint8_t op = getOpcode(instruction);
        uint8_t funct3 = getFunct3(instruction);
        uint8_t funct7 = getFunct7(instruction);
        uint8_t rs1 = getRS1(instruction);
        uint8_t rs2 = getRS2(instruction);
        uint8_t rd = getRD(instruction);

        // printInstruction(instruction);

        // addi
        if (op == 0b0010011 && funct3 == 0x0) {
            reg[rd] = reg[rs1] + I_FMT_imm(instruction);
            printf("addi\n");
        }
        // xori
        if (op == 0b0010011 && funct3 == 0x4) {
            reg[rd] = reg[rs1] ^ I_FMT_imm(instruction);
            printf("xori\n");
        }
        // ori
        if (op == 0b0010011 && funct3 == 0x6) {
            reg[rd] = reg[rs1] | I_FMT_imm(instruction);
            printf("ori\n");
        }
        // andi
        if (op == 0b0010011 && funct3 == 0x7) {
            reg[rd] = reg[rs1] & I_FMT_imm(instruction);
            printf("andi\n");
        }
        // slli
        if (op == 0b0010011 && funct3 == 0x1 && getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] << getShift(instruction);
            printf("slli\n");
        }
        // srli
        if (op == 0b0010011 && funct3 == 0x5 && getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] >> getShift(instruction);
            printf("srli\n");
        }
        // srai
        if (op == 0b0010011 && funct3 == 0x5 && getSHType(instruction) == 0x20) {
            uint8_t shift = getShift(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) >> shift);
            printf("srai\n");
        }
        // slti
        if (op == 0b0010011 && funct3 == 0x2) {
            reg[rd] = static_cast<int32_t>(reg[rs1]) < I_FMT_imm(instruction);
            printf("slti\n");
        }
        // sltiu
        if (op == 0b0010011 && funct3 == 0x3) {
            reg[rd] = reg[rs1] < static_cast<uint32_t>(I_FMT_imm(instruction));
            printf("sltiu\n");
        }

        // add
        if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x00) {
            reg[rd] = reg[rs1] + reg[rs2];
            printf("add\n");
        }
        // sub
        if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x20) {
            reg[rd] = reg[rs1] - reg[rs2];
            printf("sub\n");
        }
        // xor
        if (op == 0b0110011 && funct3 == 0x4) {
            reg[rd] = reg[rs1] ^ reg[rs2];
            printf("xor\n");
        }
        // or
        if (op == 0b0110011 && funct3 == 0x6) {
            reg[rd] = reg[rs1] | reg[rs2];
            printf("or\n");
        }
        // and
        if (op == 0b0110011 && funct3 == 0x7) {
            reg[rd] = reg[rs1] & reg[rs2];
            printf("and\n");
        }
        // sll
        if (op == 0b0110011 && funct3 == 0x1) {
            reg[rd] = reg[rs1] << (reg[rs2] & 0x1f);
            printf("sll\n");
        }
        // srl
        if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x00) {
            reg[rd] = reg[rs1] >> (reg[rs2] & 0x1f);
            printf("srl\n");
        }
        // sra
        if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x20) {
            uint8_t shift = static_cast<uint8_t>(reg[rs2] & 0x1f);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) >> shift);
            printf("sra\n");
        }
        // slt
        if (op == 0b0110011 && funct3 == 0x2) {
            reg[rd] = static_cast<int32_t>(reg[rs1]) < static_cast<int32_t>(reg[rs2]);
            printf("slt\n");
        }
        // sltu
        if (op == 0b0110011 && funct3 == 0x3) {
            reg[rd] = reg[rs1] < reg[rs2];
            printf("sltu\n");
        }
        // lb
        if (op == 0b0000011 && funct3 == 0x0) {
            uint32_t address = reg[rs1] + I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem[address])));
            printf("lb\n");
        }
        // lbu
        if (op == 0b0000011 && funct3 == 0x4) {
            uint32_t address = reg[rs1] + I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address]);
            printf("lbu\n");
        }
        // lh
        if (op == 0b0000011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(mem[address] | mem[address + 1] << 8)));
            printf("lh\n");
        }
        // lhu
        if (op == 0b0000011 && funct3 == 0x5) {
            uint32_t address = reg[rs1] + I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8);
            printf("lhu\n");
        }
        // lw
        if (op == 0b0000011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8 | mem[address + 2] << 16 | mem[address + 3] << 24);
            printf("lw\n");
        }

        // sb
        if (op == 0b0100011 && funct3 == 0x0) {
            uint32_t address = reg[rs1] + S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            printf("sb\n");
        }
        // sh
        if (op == 0b0100011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            printf("sh\n");
        }
        // sw
        if (op == 0b0100011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            mem[address + 2] = static_cast<uint8_t>((reg[rs2] >> 16) & 0xFF);
            mem[address + 3] = static_cast<uint8_t>((reg[rs2] >> 24) & 0xFF);
            printf("sw\n");
        }

        // beq
        if (op == 0b1100011 && funct3 == 0x0) {
            if (reg[rs1] == reg[rs2]) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("beq\n");
        }
        // bne
        if (op == 0b1100011 && funct3 == 0x1) {
            if (reg[rs1] != reg[rs2]) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("bne\n");
        }
        // blt
        if (op == 0b1100011 && funct3 == 0x4) {
            if (static_cast<int32_t>(reg[rs1]) < static_cast<int32_t>(reg[rs2])) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("blt\n");
        }
        // bge
        if (op == 0b1100011 && funct3 == 0x5) {
            if (static_cast<int32_t>(reg[rs1]) >= static_cast<int32_t>(reg[rs2])) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("bge\n");
        }
        // bltu
        if (op == 0b1100011 && funct3 == 0x6) {
            if (reg[rs1] < reg[rs2]) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("bltu\n");
        }
        // bgeu
        if (op == 0b1100011 && funct3 == 0x7) {
            if (reg[rs1] >= reg[rs2]) {
                pc += B_FMT_imm(instruction) - 4;
            }
            printf("bgeu\n");
        }

        // jal
        if (op == 0b1101111) {
            reg[rd] = pc + 4;
            pc += J_FMT_imm(instruction) - 4;
            printf("jal\n");
        }
        // jalr
        if (op == 0b1100111 && funct3 == 0x0) {
            reg[rd] = pc + 4;

            pc = I_FMT_imm(instruction) + reg[rs1] - 4;
            printf("jalr\n");
        }

        // lui
        if (op == 0b0110111) {
            reg[rd] = U_FMT_imm(instruction) << 12;
            printf("lui\n");
        }
        // auipc
        if (op == 0b0010111) {
            pc += U_FMT_imm(instruction) << 12;
            printf("auipc\n");
        }

        // ecall
        if (op == 0b1110011 && I_FMT_imm(instruction) == 0x0) {
            switch (reg[17]) {
                case 64: // write
                    for (uint32_t i = 0; i < reg[12]; ++i) {
                        std::cout << mem[reg[11] + i];
                    }
                    std::cout << std::flush;
                    break;
                default:
                    std::cout << "Unknown ecall with code " << reg[17] << std::endl;
            }
            printf("ecall\n");
        }

        pc += 4;

        if (pc == BASE_RA) {
            std::cout << "Returned to BASE_RA" << std::endl;
            printInfo();
            return 0;
        }

        if (pc < 0x0 || pc >= sizeof(mem)) {
            std::cout << "pc out of bounds" << std::hex << pc << std::endl;
            break;
        }
    }
}