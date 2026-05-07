#include <iostream>
#include <bitset>
#include <iomanip>
#include "../lib/elf/ElfBinary.h"
#include <format>

// load binary
// decode all instructions into vector of instructions
// write out program:
//    include maybe printf
//    create memory array
//    fill memory array with data
//    init pc, ra, gp
//    switch over PC
//    iterate all instructions, create switch case with label being address of instruction
//    end prologue

std::vector<Instruction> instructions;
uint8_t indent = 0;

#define BASE_RA 0xdeadbeef
#define STACK_SIZE 0x80000
#define MULTILINE(...) #__VA_ARGS__

void emit(std::string_view text) {
    for (size_t i = 0; i < indent; ++i) {
        std::cout << "\t";
    }
    std::cout << text;
}

// Useful for e.g. omitting instructions that write to x0
void emit(std::string_view text, uint8_t condition) {
    if (condition > 0) {
        emit(text);
    }
}

void emitInfoPrint() {
    emit(MULTILINE(
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

        std::cout << std::dec << std::setfill(' ');
    }
    ));
}

void emitLoadSaveAddress(const Instruction& instruction) {
    emit(std::format("address = reg[{}] + {};\n", instruction.rs1, instruction.immediate));
}

void emitInstruction(const Instruction& instruction) {
    indent = 2;
    emit(std::format("L{:X}:\n", instruction.address));
    indent = 3;

    switch (instruction.type) {
        case EInstruction::ADD:
            emit(std::format("reg[{}] = reg[{}] + reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SUB:
            emit(std::format("reg[{}] = reg[{}] - reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::XOR:
            emit(std::format("reg[{}] = reg[{}] ^ reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::OR:
            emit(std::format("reg[{}] = reg[{}] | reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::AND:
            emit(std::format("reg[{}] = reg[{}] & reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SLL:
            emit(std::format("reg[{}] = reg[{}] << (reg[{}] & 0x1f);\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SRL:
            emit(std::format("reg[{}] = reg[{}] >> (reg[{}] & 0x1f);\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SRA:
            emit(std::format("reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(reg[{}]) >> reg[{}] & 0x1f);\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SLT:
            emit(std::format("reg[{}] = static_cast<int32_t>(reg[{}]) < static_cast<int32_t>(reg[{}]);\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::SLTU:
            emit(std::format("reg[{}] = reg[{}] < reg[{}];\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::ADDI:
            emit(std::format("reg[{}] = reg[{}] + {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::XORI:
            emit(std::format("reg[{}] = reg[{}] ^ {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::ORI:
            emit(std::format("reg[{}] = reg[{}] | {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::ANDI:
            emit(std::format("reg[{}] = reg[{}] & {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::SLLI:
            emit(std::format("reg[{}] = reg[{}] << {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::SRLI:
            emit(std::format("reg[{}] = reg[{}] >> {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::SRAI:
            emit(std::format("reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(reg[{}]) >> {});\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd); // rs2 is same as shift amount
            break;
        case EInstruction::SLTI:
            emit(std::format("reg[{}] = static_cast<int32_t>(reg[{}]) < {};\n", instruction.rd, instruction.rs1, instruction.immediate), instruction.rd);
            break;
        case EInstruction::SLTIU:
            emit(std::format("reg[{}] = reg[{}] < {};\n", instruction.rd, instruction.rs1, static_cast<uint32_t>(instruction.immediate)), instruction.rd);
            break;
        case EInstruction::LB:
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(mem[address])));\n", instruction.rd), instruction.rd);
            break;
        case EInstruction::LH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(mem[address] | mem[address + 1] << 8)));\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8 | mem[address + 2] << 16 | mem[address + 3] << 24);\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LBU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = static_cast<uint32_t>(mem[address]);\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LHU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[rd] = static_cast<uint32_t>(mem[address] | mem[address + 1] << 8);\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::SB: {
            emitLoadSaveAddress(instruction);
            emit(std::format("mem[address] = static_cast<uint8_t>(reg[{}] & 0xFF);\n", instruction.rs2));
            break;
        }
        case EInstruction::SH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("mem[address] = static_cast<uint8_t>(reg[{}] & 0xFF);\n", instruction.rs2));
            emit(std::format("mem[address + 1] = static_cast<uint8_t>((reg[{}] >> 8) & 0xFF);\n", instruction.rs2));
            break;
        }
        case EInstruction::SW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("mem[address] = static_cast<uint8_t>(reg[{}] & 0xFF);\n", instruction.rs2));
            emit(std::format("mem[address + 1] = static_cast<uint8_t>((reg[{}] >> 8) & 0xFF);\n", instruction.rs2));
            emit(std::format("mem[address + 2] = static_cast<uint8_t>((reg[{}] >> 16) & 0xFF);\n", instruction.rs2));
            emit(std::format("mem[address + 3] = static_cast<uint8_t>((reg[{}] >> 24) & 0xFF);\n", instruction.rs2));
            break;
        }
        case EInstruction::BEQ:
            emit(std::format("if (reg[{}] == reg[{}]) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::BNE:
            emit(std::format("if (reg[{}] != reg[{}]) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::BLT:
            emit(std::format("if (static_cast<int32_t>(reg[{}]) < static_cast<int32_t>(reg[{}])) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::BGE:
            emit(std::format("if (static_cast<int32_t>(reg[{}]) >= static_cast<int32_t>(reg[{}])) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::BLTU:
            emit(std::format("if (reg[{}] < reg[{}]) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::BGEU:
            emit(std::format("if (reg[{}] >= reg[{}]) goto L{:X};\n", instruction.rs1, instruction.rs2, instruction.address + instruction.immediate));
            break;
        case EInstruction::JAL:
            emit(std::format("reg[{}] = 0x{:X};\n", instruction.rd, instruction.address + 4), instruction.rd);
            emit(std::format("goto L{:X};\n", instruction.address + instruction.immediate));
            break;
        case EInstruction::JALR:
            emit(std::format("reg[{}] = 0x{:X};\n", instruction.rd, instruction.address + 4), instruction.rd);
            emit(std::format("pc = {} + reg[{}];\n", instruction.immediate, instruction.rs1));
            emit("continue;\n");
            break;
        case EInstruction::LUI:
            emit(std::format("reg[{}] = {} << 12;\n", instruction.rd, instruction.immediate), instruction.rd);
            break;
        case EInstruction::AUIPC:
            emit(std::format("goto L{:X};\n", instruction.address + (instruction.immediate << 12)));
            break;
        case EInstruction::ECALL:
            emit(MULTILINE(switch (reg[17]) {
                case 64: // write
                    for (uint32_t i = 0; i < reg[12]; ++i) {
                        std::cout << (char)mem[reg[11] + i];
                    }
                    std::cout << std::flush;
                    break;
                default:
                    std::cout << "Unknown ecall with code " << reg[17] << std::endl;
            }));
            break;
        case EInstruction::EBREAK:
            break;
        case EInstruction::FENCE:
            break;
        case EInstruction::INVALID:
            emit(std::format("// Invalid instruction at 0x{:X}\n", instruction.address));
            break;
    }
}

// todo: make reg[0] read just const 0
// todo: fill remaining values of dispatch table with handler for invalid instructions

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
    auto& text = binary.getSection(ElfBinarySection::Text).value().get();

    emit("#include <iostream>\n");
    emit("#include <cstdint>\n");
    emit("#include <iomanip>\n\n");

    emit(std::format("uint32_t mem[0x{:X}];\n", STACK_SIZE));
    emit("uint32_t reg[32];\n");
    emit(std::format("uint32_t pc = 0x{:X};\n\n", binary.getSymbolAddress("main").value_or(0)));
    emit(std::format("void* dispatch[{}] = {{0}};\n", text.getSize()));

    emitInfoPrint();

    emit("\n\nint main() {\n");
    emit(std::format("\treg[1] = 0x{:X};\n", BASE_RA));
    emit(std::format("\treg[2] = 0x{:X};\n", STACK_SIZE));
    emit(std::format("\treg[3] = 0x{:X};\n\n", binary.getSymbolAddress("__global_pointer$").value_or(0)));

    // build dispatch table https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
    for (const auto& inst : instructions) {
        emit(std::format("\tdispatch[{}] = &&L{:X};\n", (inst.address - text.getStartAddress()) / 4, inst.address));
    }

    // load static data
    auto& dataSection = binary.getSection(ElfBinarySection::Data).value().get();
    uint32_t dataAddr = dataSection.getStartAddress();

    for (auto word : dataSection.getData()) {
        emit(std::format("\tmem[0x{:X}] = 0x{:X};", dataAddr, static_cast<uint8_t>(word & 0xFF)));
        emit(std::format("\tmem[0x{:X}] = 0x{:X};", dataAddr + 1, static_cast<uint8_t>((word >> 8) & 0xFF)));
        emit(std::format("\tmem[0x{:X}] = 0x{:X};", dataAddr + 2, static_cast<uint8_t>((word >> 16) & 0xFF)));
        emit(std::format("\tmem[0x{:X}] = 0x{:X};", dataAddr + 3, static_cast<uint8_t>((word >> 24) & 0xFF)));
        emit(std::format(" // 0x{:X}\n", word));

        dataAddr += 4;
    }

    emit("\n\tuint32_t address;\n");
    emit("\n\twhile (true) {\n");
    emit(std::format("\t\tuint32_t pcDispatchIndex = (pc - 0x{:X}) / 4;\n", text.getStartAddress()));
    emit(std::format("\t\tif (pc == 0x{:X}) {{ printInfo(); return 0; }}\n", BASE_RA));
    emit("\t\tgoto *dispatch[pcDispatchIndex];\n\n");

    for (const auto& inst : instructions) {
        emitInstruction(inst);
    }

    indent = 0;
    emit("\t}\n");
    emit("}\n");

    return 0;
}