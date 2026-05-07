#include <iostream>
#include <bitset>
#include <iomanip>
#include "elf/ElfBinary.h"
#include "decoding/Decoder.h"

uint8_t mem[0x80000];
uint32_t reg[32] = { 0 };
uint32_t pc = 0;

#define BASE_RA 0xdeadbeef

uint32_t inline readWord(uint32_t address) {
    return static_cast<uint32_t>(mem[address])
        | (static_cast<uint32_t>(mem[address + 1]) << 8)
        | (static_cast<uint32_t>(mem[address + 2]) << 16)
        | (static_cast<uint32_t>(mem[address + 3]) << 24);
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
        uint8_t op = Decoder::getOpcode(instruction);
        uint8_t funct3 = Decoder::getFunct3(instruction);
        uint8_t funct7 = Decoder::getFunct7(instruction);
        uint8_t rs1 = Decoder::getRS1(instruction);
        uint8_t rs2 = Decoder::getRS2(instruction);
        uint8_t rd = Decoder::getRD(instruction);

        // addi
        if (op == 0b0010011 && funct3 == 0x0) {
            reg[rd] = reg[rs1] + Decoder::I_FMT_imm(instruction);
            printf("addi\n");
        }
        // xori
        if (op == 0b0010011 && funct3 == 0x4) {
            reg[rd] = reg[rs1] ^ Decoder::I_FMT_imm(instruction);
            printf("xori\n");
        }
        // ori
        if (op == 0b0010011 && funct3 == 0x6) {
            reg[rd] = reg[rs1] | Decoder::I_FMT_imm(instruction);
            printf("ori\n");
        }
        // andi
        if (op == 0b0010011 && funct3 == 0x7) {
            reg[rd] = reg[rs1] & Decoder::I_FMT_imm(instruction);
            printf("andi\n");
        }
        // slli
        if (op == 0b0010011 && funct3 == 0x1 && Decoder::getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] << Decoder::getShift(instruction);
            printf("slli\n");
        }
        // srli
        if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] >> Decoder::getShift(instruction);
            printf("srli\n");
        }
        // srai
        if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x20) {
            uint8_t shift = Decoder::getShift(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) >> shift);
            printf("srai\n");
        }
        // slti
        if (op == 0b0010011 && funct3 == 0x2) {
            reg[rd] = static_cast<int32_t>(reg[rs1]) < Decoder::I_FMT_imm(instruction);
            printf("slti\n");
        }
        // sltiu
        if (op == 0b0010011 && funct3 == 0x3) {
            reg[rd] = reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
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
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem[address])));
            printf("lb\n");
        }
        // lbu
        if (op == 0b0000011 && funct3 == 0x4) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address]);
            printf("lbu\n");
        }
        // lh
        if (op == 0b0000011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(mem[address] | mem[address + 1] << 8)));
            printf("lh\n");
        }
        // lhu
        if (op == 0b0000011 && funct3 == 0x5) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8);
            printf("lhu\n");
        }
        // lw
        if (op == 0b0000011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            printf("address: 0x%X\n", address);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8 | mem[address + 2] << 16 | mem[address + 3] << 24);
            printf("lw\n");
        }

        // sb
        if (op == 0b0100011 && funct3 == 0x0) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            printf("sb\n");
        }
        // sh
        if (op == 0b0100011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            printf("sh\n");
        }
        // sw
        if (op == 0b0100011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            mem[address + 2] = static_cast<uint8_t>((reg[rs2] >> 16) & 0xFF);
            mem[address + 3] = static_cast<uint8_t>((reg[rs2] >> 24) & 0xFF);
            printf("sw\n");
        }

        // beq
        if (op == 0b1100011 && funct3 == 0x0) {
            if (reg[rs1] == reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("beq\n");
        }
        // bne
        if (op == 0b1100011 && funct3 == 0x1) {
            if (reg[rs1] != reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("bne\n");
        }
        // blt
        if (op == 0b1100011 && funct3 == 0x4) {
            if (static_cast<int32_t>(reg[rs1]) < static_cast<int32_t>(reg[rs2])) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("blt\n");
        }
        // bge
        if (op == 0b1100011 && funct3 == 0x5) {
            if (static_cast<int32_t>(reg[rs1]) >= static_cast<int32_t>(reg[rs2])) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("bge\n");
        }
        // bltu
        if (op == 0b1100011 && funct3 == 0x6) {
            if (reg[rs1] < reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("bltu\n");
        }
        // bgeu
        if (op == 0b1100011 && funct3 == 0x7) {
            if (reg[rs1] >= reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            printf("bgeu\n");
        }

        // jal
        if (op == 0b1101111) {
            reg[rd] = pc + 4;
            pc += Decoder::J_FMT_imm(instruction) - 4;
            printf("jal\n");
        }
        // jalr
        if (op == 0b1100111 && funct3 == 0x0) {
            reg[rd] = pc + 4;

            pc = Decoder::I_FMT_imm(instruction) + reg[rs1] - 4;
            printf("jalr\n");
        }

        // lui
        if (op == 0b0110111) {
            reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
            printf("lui\n");
        }
        // auipc
        if (op == 0b0010111) {
            pc += Decoder::U_FMT_imm(instruction) << 12;
            printf("auipc\n");
        }

        // ecall
        if (op == 0b1110011 && Decoder::I_FMT_imm(instruction) == 0x0) {
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