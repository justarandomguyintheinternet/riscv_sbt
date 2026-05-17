#include "../lib/elf/ElfBinary.h"
#include <iostream>
#include <fstream>
#include <bitset>
#include <format>
#include <set>

std::vector<Instruction> instructions;
uint8_t indent = 0;
std::ofstream output;

#define BASE_RA 0xdeadbeef
#define STACK_SIZE 0x80000
#define MULTILINE(...) #__VA_ARGS__

void emit(std::string_view text) {
    for (size_t i = 0; i < indent; ++i) {
        output << "\t";
    }
    output << text;
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

void emitInstruction(const Instruction& instruction, bool isLeader) {
    indent = 2;
    if (isLeader) {
        emit(std::format("L{:X}:\n", instruction.address));
    }
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
            emit(std::format("reg[{}] = static_cast<int32_t>(static_cast<int8_t>(mem[address]));\n", instruction.rd), instruction.rd);
            break;
        case EInstruction::LH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = static_cast<int32_t>(*reinterpret_cast<int16_t *>(&mem[address]));\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = *reinterpret_cast<uint32_t *>(&mem[address]);\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LBU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = mem[address];\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::LHU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("reg[{}] = *reinterpret_cast<uint16_t *>(&mem[address]);\n", instruction.rd), instruction.rd);
            break;
        }
        case EInstruction::SB: {
            emitLoadSaveAddress(instruction);
            emit(std::format("mem[address] = static_cast<uint8_t>(reg[{}]);\n", instruction.rs2));
            break;
        }
        case EInstruction::SH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("*reinterpret_cast<uint16_t *>(&mem[address]) = static_cast<uint16_t>(reg[{}]);\n", instruction.rs2));
            break;
        }
        case EInstruction::SW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("*reinterpret_cast<uint32_t *>(&mem[address]) = reg[{}];\n", instruction.rs2));
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
            emit(std::format("reg[{}] = {};\n", instruction.rd, instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::AUIPC:
            emit(std::format("reg[{}] = 0x{:X} + 0x{:X};\n", instruction.rd, instruction.address, instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::ECALL:
            emit(MULTILINE(switch (reg[17]) {
                case 64: // write
                    for (uint32_t i = 0; i < reg[12]; ++i) {
                        std::cout << (char)mem[reg[11] + i];
                    }
                    std::cout << std::flush;
                    break;
                case 93: // exit
                        return 0;
                default:
                    std::cout << "Unknown ecall with code " << reg[17] << std::endl;
            }));
            break;
        case EInstruction::EBREAK:
            break;
        case EInstruction::FENCE:
            break;
        // RV32-M
        case EInstruction::MUL:
            emit(std::format("reg[{}] = static_cast<uint64_t>(reg[{}]) * static_cast<uint64_t>(reg[{}]) & 0xFFFFFFFF;\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::MULH:
            emit(std::format("reg[{}] = (static_cast<int64_t>(static_cast<int32_t>(reg[{}])) * static_cast<int64_t>(static_cast<int32_t>(reg[{}]))) >> 32;\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::MULHSU:
            emit(std::format("reg[{}] = (static_cast<int64_t>(static_cast<int32_t>(reg[{}])) * static_cast<uint64_t>(reg[{}])) >> 32;\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::MULHU:
            emit(std::format("reg[{}] = (static_cast<uint64_t>(reg[{}]) * static_cast<uint64_t>(reg[{}])) >> 32;\n", instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::DIV:
            emit(std::format(R"(
                if (reg[{}] == 0) {{
                    reg[{}] = -1;
                }} else if (reg[{}] == 0x80000000 && reg[{}] == 0xFFFFFFFF) {{
                    reg[{}] = 0x80000000;
                }} else {{
                    reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(reg[{}]) / static_cast<int32_t>(reg[{}]));
                }}
            )", instruction.rs2, instruction.rd, instruction.rs1, instruction.rs2, instruction.rd, instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::DIVU:
            emit(std::format(R"(
                if (reg[{}] == 0) {{
                    reg[{}] = 0xFFFFFFFF;
                }} else {{
                    reg[{}] = reg[{}] / reg[{}];
                }}
            )", instruction.rs2, instruction.rd, instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::REM:
            emit(std::format(R"(
                if (reg[{}] == 0) {{
                    reg[{}] = reg[{}];
                }} else if (reg[{}] == 0x80000000 && reg[{}] == 0xFFFFFFFF) {{
                    reg[{}] = 0x80000000;
                }} else {{
                    reg[{}] = static_cast<uint32_t>(static_cast<int32_t>(reg[{}]) % static_cast<int32_t>(reg[{}]));
                }}
            )", instruction.rs2, instruction.rd, instruction.rs1, instruction.rs1, instruction.rs2, instruction.rd, instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::REMU:
            emit(std::format(R"(
                if (reg[{}] == 0) {{
                    reg[{}] = reg[{}];
                }} else {{
                    reg[{}] = reg[{}] % reg[{}];
                }}
            )", instruction.rs2, instruction.rd, instruction.rs1, instruction.rd, instruction.rs1, instruction.rs2), instruction.rd);
            break;
        case EInstruction::INVALID:
            emit(std::format("// Invalid instruction at 0x{:X}\n", instruction.address));
            break;
    }
}

std::set<uint32_t> getBasicBlocksLeaders(const std::vector<Instruction>& instructions) {
    std::set<uint32_t> leaders;

    if (instructions.empty()) {
        return leaders;
    }

    leaders.insert(instructions.front().address);

    for (auto& instruction: instructions) {
        if (instruction.type == EInstruction::BEQ || instruction.type == EInstruction::BNE ||
            instruction.type == EInstruction::BLT || instruction.type == EInstruction::BGE ||
            instruction.type == EInstruction::BLTU || instruction.type == EInstruction::BGEU ||
            instruction.type == EInstruction::JAL ) {
            leaders.insert(instruction.address + instruction.immediate);
            leaders.insert(instruction.address + 4);
        } else if (instruction.type == EInstruction::JALR) {
            leaders.insert(instruction.address + 4);
        }
    }

    return leaders;
}

// todo: make reg[0] read just const 0

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
    std::set<uint32_t> leaders = getBasicBlocksLeaders(instructions);

    auto text = binary.getTypeSections(ElfBinarySection::Text);
    uint32_t textSize = 0; // Sum of the size of all sections which are executable
    uint32_t textStartAddress = 1 << 31; // Lowest start address among executable sections

    for (const auto& ref : text) {
        textSize += ref.get().getSize();
        textStartAddress = std::min(textStartAddress, ref.get().getStartAddress());
    }

    output.open("./sbt/output/translated.cpp");

    emit("#include <iostream>\n");
    emit("#include <cstdint>\n");
    emit("#include <iomanip>\n\n");

    emit(std::format("uint8_t mem[0x{:X}];\n", STACK_SIZE));
    emit("uint32_t reg[32];\n");
    emit(std::format("uint32_t pc = 0x{:X};\n\n", binary.getEntryAddress()));
    emit(std::format("void* dispatch[{}] = {{0}};\n", textSize));

    emitInfoPrint();

    emit("\n\nint main() {\n");

    bool hasStartup = binary.getSymbolAddress("_start").has_value();

    if (!hasStartup) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        emit(std::format("\treg[1] = 0x{:X};\n", BASE_RA));
        emit(std::format("\treg[2] = 0x{:X};\n", STACK_SIZE));
        emit(std::format("\treg[3] = 0x{:X};\n\n", binary.getSymbolAddress("__global_pointer$").value_or(0)));
    }

    // build dispatch table https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
    for (const auto& inst : instructions) {
        if (leaders.contains(inst.address)) {
            emit(std::format("\tdispatch[{}] = &&L{:X};\n", (inst.address - textStartAddress) / 4, inst.address));
        } else {
            emit(std::format("\tdispatch[{}] = &&INVALID;\n", (inst.address - textStartAddress) / 4));
        }
    }

    // load static data
    for (const auto& ref : binary.getTypeSections(ElfBinarySection::SectionType::Data)) {
        const auto& dataSection = ref.get();
        uint32_t dataAddr = dataSection.getLoadAddress(); // Use load address, not virtual one, for when crt0 copies data into to the virtual address

        for (auto word : dataSection.getData()) {
            emit(std::format("\t*reinterpret_cast<uint32_t *>(&mem[0x{:X}]) = 0x{:X};\n", dataAddr, word), word != 0);
            dataAddr += 4;
        }
    }

    emit("\n\tuint32_t address;\n");
    emit("\n\twhile (true) {\n");
    emit(std::format("\t\tuint32_t pcDispatchIndex = (pc - 0x{:X}) / 4;\n", textStartAddress));
    emit(std::format("\t\tif (pc == 0x{:X}) {{ printInfo(); return 0; }}\n", BASE_RA));
    emit("\t\tgoto *dispatch[pcDispatchIndex];\n\n");

    for (const auto& inst : instructions) {
        emitInstruction(inst, leaders.contains(inst.address));
    }

    indent = 1;
    emit("}\n");

    emit("INVALID:\n");
    emit("\tstd::cout << \"Invalid instruction at 0x\" << std::hex << pc << std::dec << std::endl;\n");
    emit("\treturn 1;\n");

    indent = 0;
    emit("}\n");

    output.close();

    return 0;
}