#include <iostream>
#include <bitset>
#include <iomanip>
#include "elf/ElfBinary.h"
#include "decoding/Decoder.h"

uint8_t mem[0x80000];
uint32_t reg[32] = { 0 };
uint32_t pc = 0;

#define BASE_RA 0xdeadbeef
#define LOG_INSTRUCTIONS 0

#if LOG_INSTRUCTIONS == 1
    #define LOG_INST(addr, name) logInstruction(addr, name)
#else
    #define LOG_INST(addr, name)
#endif

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

void logInstruction(uint32_t address, const char* name) {
    printf("0x%08x: %s\n", address, name);
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
            LOG_INST(pc, "addi");
        }
        // xori
        if (op == 0b0010011 && funct3 == 0x4) {
            reg[rd] = reg[rs1] ^ Decoder::I_FMT_imm(instruction);
            LOG_INST(pc, "xori");
        }
        // ori
        if (op == 0b0010011 && funct3 == 0x6) {
            reg[rd] = reg[rs1] | Decoder::I_FMT_imm(instruction);
            LOG_INST(pc, "ori");
        }
        // andi
        if (op == 0b0010011 && funct3 == 0x7) {
            reg[rd] = reg[rs1] & Decoder::I_FMT_imm(instruction);
            LOG_INST(pc, "andi");
        }
        // slli
        if (op == 0b0010011 && funct3 == 0x1 && Decoder::getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] << Decoder::getShift(instruction);
            LOG_INST(pc, "slli");
        }
        // srli
        if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x00) {
            reg[rd] = reg[rs1] >> Decoder::getShift(instruction);
            LOG_INST(pc, "srli");
        }
        // srai
        if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x20) {
            uint8_t shift = Decoder::getShift(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) >> shift);
            LOG_INST(pc, "srai");
        }
        // slti
        if (op == 0b0010011 && funct3 == 0x2) {
            reg[rd] = static_cast<int32_t>(reg[rs1]) < Decoder::I_FMT_imm(instruction);
            LOG_INST(pc, "slti");
        }
        // sltiu
        if (op == 0b0010011 && funct3 == 0x3) {
            reg[rd] = reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
            LOG_INST(pc, "sltiu");
        }

        // add
        if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x00) {
            reg[rd] = reg[rs1] + reg[rs2];
            LOG_INST(pc, "add");
        }
        // sub
        if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x20) {
            reg[rd] = reg[rs1] - reg[rs2];
            LOG_INST(pc, "sub");
        }
        // xor
        if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x00) {
            reg[rd] = reg[rs1] ^ reg[rs2];
            LOG_INST(pc, "xor");
        }
        // or
        if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x00) {
            reg[rd] = reg[rs1] | reg[rs2];
            LOG_INST(pc, "or");
        }
        // and
        if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x00) {
            reg[rd] = reg[rs1] & reg[rs2];
            LOG_INST(pc, "and");
        }
        // sll
        if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x00) {
            reg[rd] = reg[rs1] << (reg[rs2] & 0x1f);
            LOG_INST(pc, "sll");
        }
        // srl
        if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x00) {
            reg[rd] = reg[rs1] >> (reg[rs2] & 0x1f);
            LOG_INST(pc, "srl");
        }
        // sra
        if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x20) {
            uint8_t shift = static_cast<uint8_t>(reg[rs2] & 0x1f);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) >> shift);
            LOG_INST(pc, "sra");
        }
        // slt
        if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x00) {
            reg[rd] = static_cast<int32_t>(reg[rs1]) < static_cast<int32_t>(reg[rs2]);
            LOG_INST(pc, "slt");
        }
        // sltu
        if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x00) {
            reg[rd] = reg[rs1] < reg[rs2];
            LOG_INST(pc, "sltu");
        }
        // lb
        if (op == 0b0000011 && funct3 == 0x0) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem[address])));
            LOG_INST(pc, "lb");
        }
        // lbu
        if (op == 0b0000011 && funct3 == 0x4) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address]);
            LOG_INST(pc, "lbu");
        }
        // lh
        if (op == 0b0000011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(mem[address] | mem[address + 1] << 8)));
            LOG_INST(pc, "lh");
        }
        // lhu
        if (op == 0b0000011 && funct3 == 0x5) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8);
            LOG_INST(pc, "lhu");
        }
        // lw
        if (op == 0b0000011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + Decoder::I_FMT_imm(instruction);
            reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8 | mem[address + 2] << 16 | mem[address + 3] << 24);
            LOG_INST(pc, "lw");
        }

        // sb
        if (op == 0b0100011 && funct3 == 0x0) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            LOG_INST(pc, "sb");
        }
        // sh
        if (op == 0b0100011 && funct3 == 0x1) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            LOG_INST(pc, "sh");
        }
        // sw
        if (op == 0b0100011 && funct3 == 0x2) {
            uint32_t address = reg[rs1] + Decoder::S_FMT_imm(instruction);
            mem[address] = static_cast<uint8_t>(reg[rs2] & 0xFF);
            mem[address + 1] = static_cast<uint8_t>((reg[rs2] >> 8) & 0xFF);
            mem[address + 2] = static_cast<uint8_t>((reg[rs2] >> 16) & 0xFF);
            mem[address + 3] = static_cast<uint8_t>((reg[rs2] >> 24) & 0xFF);
            LOG_INST(pc, "sw");
        }

        // beq
        if (op == 0b1100011 && funct3 == 0x0) {
            if (reg[rs1] == reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "beq");
        }
        // bne
        if (op == 0b1100011 && funct3 == 0x1) {
            if (reg[rs1] != reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "bne");
        }
        // blt
        if (op == 0b1100011 && funct3 == 0x4) {
            if (static_cast<int32_t>(reg[rs1]) < static_cast<int32_t>(reg[rs2])) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "blt");
        }
        // bge
        if (op == 0b1100011 && funct3 == 0x5) {
            if (static_cast<int32_t>(reg[rs1]) >= static_cast<int32_t>(reg[rs2])) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "bge");
        }
        // bltu
        if (op == 0b1100011 && funct3 == 0x6) {
            if (reg[rs1] < reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "bltu");
        }
        // bgeu
        if (op == 0b1100011 && funct3 == 0x7) {
            if (reg[rs1] >= reg[rs2]) {
                pc += Decoder::B_FMT_imm(instruction) - 4;
            }
            LOG_INST(pc, "bgeu");
        }

        // jal
        if (op == 0b1101111) {
            reg[rd] = pc + 4;
            pc += Decoder::J_FMT_imm(instruction) - 4;
            LOG_INST(pc, "jal");
        }
        // jalr
        if (op == 0b1100111 && funct3 == 0x0) {
            reg[rd] = pc + 4;

            pc = Decoder::I_FMT_imm(instruction) + reg[rs1] - 4;
            LOG_INST(pc, "jalr");
        }

        // lui
        if (op == 0b0110111) {
            reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
            LOG_INST(pc, "lui");
        }
        // auipc
        if (op == 0b0010111) {
            pc += Decoder::U_FMT_imm(instruction) << 12;
            LOG_INST(pc, "auipc");
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
            LOG_INST(pc, "ecall");
        }

        // RV32M

        // mul
        if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x01) {
            reg[rd] = static_cast<uint64_t>(reg[rs1]) * static_cast<uint64_t>(reg[rs2]) & 0xFFFFFFFF;
            LOG_INST(pc, "mul");
        }
        // mulh
        if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x01) {
            reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(reg[rs1])) * static_cast<int64_t>(static_cast<int32_t>(reg[rs2]))) >> 32;
            LOG_INST(pc, "mulh");
        }
        // mulhsu
        if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x01) {
            reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(reg[rs1])) * static_cast<uint64_t>(reg[rs2])) >> 32;
            LOG_INST(pc, "mulhsu");
        }
        // mulhu
        if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x01) {
            reg[rd] = (static_cast<uint64_t>(reg[rs1]) * static_cast<uint64_t>(reg[rs2])) >> 32;
            LOG_INST(pc, "mulhu");
        }
        // div
        if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x01) {
            if (reg[rs2] == 0) {
                reg[rd] = -1;
            } else if (reg[rs1] == 0x80000000 && reg[rs2] == 0xFFFFFFFF) {
                reg[rd] = 0x80000000;
            } else {
                reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) / static_cast<int32_t>(reg[rs2]));
            }
            LOG_INST(pc, "div");
        }
        // divu
        if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x01) {
            if (reg[rs2] == 0) {
                reg[rd] = 0xFFFFFFFF;
            } else {
                reg[rd] = reg[rs1] / reg[rs2];
            }
            LOG_INST(pc, "divu");
        }
        // rem
        if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x01) {
            if (reg[rs2] == 0) {
                reg[rd] = reg[rs1];
            } else if (reg[rs1] == 0x80000000 && reg[rs2] == 0xFFFFFFFF) {
                reg[rd] = 0;
            } else {
                reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(reg[rs1]) % static_cast<int32_t>(reg[rs2]));
            }
            LOG_INST(pc, "rem");
        }
        // remu
        if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x01) {
            if (reg[rs2] == 0) {
                reg[rd] = reg[rs2];
            } else {
                reg[rd] = reg[rs1] % reg[rs2];
            }
            LOG_INST(pc, "remu");
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