#include "elf/ElfBinary.h"
#include <iostream>
#include <fstream>
#include <bitset>
#include <format>
#include <set>

#include "runtime/registers.h"

std::vector<Instruction> instructions;
const Instruction* current;
uint8_t indent = 0;
std::ofstream output;

uint32_t reg_values[32];
bool reg_known[32];

#define BASE_RA 0xdeadbeef
#define MEM_SIZE 0x80000
#define MULTILINE(...) #__VA_ARGS__

std::string REG(InstructionField field) {
    if (current == nullptr) {
        return "0";
    }

    uint8_t index = 0;

    switch (field) {
        case InstructionField::RS1:
            index = current->rs1;
            break;
        case InstructionField::RS2:
            index = current->rs2;
            break;
        case InstructionField::RD:
            index = current->rd;
            break;
        case InstructionField::IMMEDIATE:
            index = current->immediate;
            break;
    }

    if (index == 0) {
        return "0"; // In case of assignment to 0, emit will just skip this instruction anyways
    }

    return std::format("ctx.reg[{}]", index);
}

void emit(std::string_view text) {
    for (size_t i = 0; i < indent; ++i) {
        output << "\t";
    }
    output << text;
}

// Helper to skip instructions writing to x0
void emit(std::string_view text, uint8_t rd) {
    if (rd > 0) {
        emit(text);
    }

    reg_known[rd] = false;
}

void emitInfoPrint() {
    emit(MULTILINE(
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
    ));
}

void emitLoadSaveAddress(const Instruction& instruction) {
    emit(std::format("address = {} + {};\n", REG(RS1), instruction.immediate));
}

// https://github.com/libriscv/libriscv/blob/master/lib/libriscv/tr_emit.cpp#L243-L254
void emitOp(std::string_view op, std::string_view shortOp, bool ordered) {
    if (current == nullptr) {
        return;
    }

    if (current->rd == current->rs1) {
        emit(std::format("{} {} {};\n", REG(RD), shortOp, REG(RS2)), current->rd);
    } else if (current->rd == current->rs2 && !ordered) {
        emit(std::format("{} {} {};\n", REG(RD), shortOp, REG(RS1)), current->rd);
    } else if (current->rd == current->rs2 && current->rs1 == 0) { // 0 - rs2
        emit(std::format("{} = {} {};\n", REG(RD), op, REG(RS2)), current->rd);
    } else {
        emit(std::format("{} = {} {} {};\n", REG(RD), REG(RS1), op, REG(RS2)), current->rd);
    }
}

void track(uint8_t reg, uint32_t value) {
    reg_values[reg] = value;
    reg_known[reg] = true;
}

void resetTracked() {
    for (bool& i : reg_known) {
        i = false;
    }
}

void emitInstruction(const Instruction& instruction, bool isLeader) {
    indent = 2;
    if (isLeader) {
        emit(std::format("L{:X}:\n", instruction.address));
        resetTracked();
    }
    indent = 3;

    current = &instruction;

    switch (instruction.type) {
        case EInstruction::ADD:
            emitOp("+", "+=", false);
            break;
        case EInstruction::SUB:
            emitOp("-", "-=", true);
            break;
        case EInstruction::XOR:
            emitOp("^", "^=", false);
            break;
        case EInstruction::OR:
            emitOp("|", "|=", false);
            break;
        case EInstruction::AND:
            emitOp("&", "&=", false);
            break;
        case EInstruction::SLL:
            emit(std::format("{} = {} << ({} & 0x1f);\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::SRL:
            emit(std::format("{} = {} >> ({} & 0x1f);\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::SRA:
            emit(std::format("{} = static_cast<uint32_t>(static_cast<int32_t>({}) >> {} & 0x1f);\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::SLT:
            emit(std::format("{} = static_cast<int32_t>({}) < static_cast<int32_t>({});\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::SLTU:
            emit(std::format("{} = {} < {};\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::ADDI:
            if (instruction.immediate == 0) {
                emit(std::format("{} = {};\n", REG(RD), REG(RS1)), instruction.rd);
            } else if (instruction.rs1 == 0) {
                emit(std::format("{} = {};\n", REG(RD), instruction.immediate), instruction.rd);
                track(instruction.rd, instruction.immediate);
            } else if (instruction.immediate < 0) { // Replace negative addition with subtract, condense same register operation to op assignment
                if (instruction.rs1 == instruction.rd) {
                    emit(std::format("{} -= {};\n", REG(RD), -instruction.immediate), instruction.rd);
                } else {
                    emit(std::format("{} = {} - {};\n", REG(RD), REG(RS1), -instruction.immediate), instruction.rd);
                }
            } else {
                if (instruction.rs1 == instruction.rd) {
                    emit(std::format("{} += {};\n", REG(RD), instruction.immediate), instruction.rd);
                } else {
                    emit(std::format("{} = {} + {};\n", REG(RD), REG(RS1), instruction.immediate), instruction.rd);
                }
            }
            break;
        case EInstruction::XORI:
            emit(std::format("{} = {} ^ {};\n", REG(RD), REG(RS1), instruction.immediate), instruction.rd);
            break;
        case EInstruction::ORI:
            emit(std::format("{} = {} | {};\n", REG(RD), REG(RS1), instruction.immediate), instruction.rd);
            break;
        case EInstruction::ANDI:
            emit(std::format("{} = {} & {};\n", REG(RD), REG(RS1), instruction.immediate), instruction.rd);
            break;
        case EInstruction::SLLI:
            emit(std::format("{} = {} << {};\n", REG(RD), REG(RS1), instruction.rs2), instruction.rd);
            break;
        case EInstruction::SRLI:
            emit(std::format("{} = {} >> {};\n", REG(RD), REG(RS1), instruction.rs2), instruction.rd);
            break;
        case EInstruction::SRAI:
            emit(std::format("{} = static_cast<uint32_t>(static_cast<int32_t>({}) >> {});\n", REG(RD), REG(RS1), instruction.rs2), instruction.rd); // rs2 is same as shift amount
            break;
        case EInstruction::SLTI:
            emit(std::format("{} = static_cast<int32_t>({}) < {};\n", REG(RD), REG(RS1), instruction.immediate), instruction.rd);
            break;
        case EInstruction::SLTIU:
            emit(std::format("{} = {} < {};\n", REG(RD), REG(RS1), static_cast<uint32_t>(instruction.immediate)), instruction.rd);
            break;
        // todo: move memory functions to generic function
        case EInstruction::LB:
            emitLoadSaveAddress(instruction);
            emit(std::format("{} = static_cast<int32_t>(memory.read<int8_t>(address));\n", REG(RD)), instruction.rd);
            break;
        case EInstruction::LH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("{} = static_cast<int32_t>(memory.read<int16_t>(address));\n", REG(RD)), instruction.rd);
            break;
        }
        case EInstruction::LW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("{} = memory.read<int32_t>(address);\n", REG(RD)), instruction.rd);
            break;
        }
        case EInstruction::LBU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("{} = ctx.memory.read<uint8_t>(address);\n", REG(RD)), instruction.rd);
            break;
        }
        case EInstruction::LHU: {
            emitLoadSaveAddress(instruction);
            emit(std::format("{} = memory.read<uint16_t>(address);\n", REG(RD)), instruction.rd);
            break;
        }
        case EInstruction::SB: {
            emitLoadSaveAddress(instruction);
            emit(std::format("memory.write<uint8_t>(address, {});\n", REG(RS2)));
            break;
        }
        case EInstruction::SH: {
            emitLoadSaveAddress(instruction);
            emit(std::format("memory.write<uint16_t>(address, {});\n", REG(RS2)));
            break;
        }
        case EInstruction::SW: {
            emitLoadSaveAddress(instruction);
            emit(std::format("memory.write<uint32_t>(address, {});\n", REG(RS2)));
            break;
        }
        case EInstruction::BEQ:
            emit(std::format("if ({} == {}) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::BNE:
            emit(std::format("if ({} != {}) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::BLT:
            emit(std::format("if (static_cast<int32_t>({}) < static_cast<int32_t>({})) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::BGE:
            emit(std::format("if (static_cast<int32_t>({}) >= static_cast<int32_t>({})) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::BLTU:
            emit(std::format("if ({} < {}) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::BGEU:
            emit(std::format("if ({} >= {}) goto L{:X};\n", REG(RS1), REG(RS2), instruction.address + instruction.immediate));
            break;
        case EInstruction::JAL:
            emit(std::format("{} = 0x{:X};\n", REG(RD), instruction.address + 4), instruction.rd);
            emit(std::format("goto L{:X};\n", instruction.address + instruction.immediate));
            break;
        case EInstruction::JALR:
            emit(std::format("{} = 0x{:X};\n", REG(RD), instruction.address + 4), instruction.rd);
            emit(std::format("ctx.pc = {} + {};\n", instruction.immediate, REG(RS1)));
            emit("continue;\n");
            break;
        case EInstruction::LUI:
            emit(std::format("{} = {};\n", REG(RD), instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::AUIPC:
            emit(std::format("{} = 0x{:X} + 0x{:X};\n", REG(RD), instruction.address, instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::ECALL:
            if (!reg_known[a7]) {
                printf("a7 not tracked, 0x%x\n", instruction.address); //todo: format this stuff out, emit switch as fallback
                emit(std::format("Syscall::handle(ctx);\n"));
            } else {
                emit(std::format("Syscall::handle(ctx, {});\n", reg_values[a7]));
            }
            break;
        case EInstruction::EBREAK:
        case EInstruction::FENCE:
            break;
        // RV32-M
        case EInstruction::MUL:
            emit(std::format("{} = static_cast<uint64_t>({}) * static_cast<uint64_t>({}) & 0xFFFFFFFF;\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::MULH:
            emit(std::format("{} = (static_cast<int64_t>(static_cast<int32_t>({})) * static_cast<int64_t>(static_cast<int32_t>({}))) >> 32;\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::MULHSU:
            emit(std::format("{} = (static_cast<int64_t>(static_cast<int32_t>({})) * static_cast<uint64_t>({})) >> 32;\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::MULHU:
            emit(std::format("{} = (static_cast<uint64_t>({}) * static_cast<uint64_t>({})) >> 32;\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::DIV:
            emit(std::format(R"(
                if ({} == 0) {{
                    {} = -1;
                }} else if ({} == 0x80000000 && {} == 0xFFFFFFFF) {{
                    {} = 0x80000000;
                }} else {{
                    {} = static_cast<uint32_t>(static_cast<int32_t>({}) / static_cast<int32_t>({}));
                }}
            )", REG(RS2), REG(RD), REG(RS1), REG(RS2), REG(RD), REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::DIVU:
            emit(std::format(R"(
                if ({} == 0) {{
                    {} = 0xFFFFFFFF;
                }} else {{
                    {} = {} / {};
                }}
            )", REG(RS2), REG(RD), REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::REM:
            emit(std::format(R"(
                if ({} == 0) {{
                    {} = {};
                }} else if ({} == 0x80000000 && {} == 0xFFFFFFFF) {{
                    {} = 0x80000000;
                }} else {{
                    {} = static_cast<uint32_t>(static_cast<int32_t>({}) % static_cast<int32_t>({}));
                }}
            )", REG(RS2), REG(RD), REG(RS1), REG(RS1), REG(RS2), REG(RD), REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::REMU:
            emit(std::format(R"(
                if ({} == 0) {{
                    {} = {};
                }} else {{
                    {} = {} % {};
                }}
            )", REG(RS2), REG(RD), REG(RS1), REG(RD), REG(RS1), REG(RS2)), instruction.rd);
            break;
        case EInstruction::INVALID:
            emit(std::format("printf(\"Unsupported instruction at 0x{:X}\");\n", instruction.address));
            break;
    }

    current = nullptr;
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

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <elf binary>" << std::endl;
        return 1;
    }

    resetTracked();

    ElfBinary binary(argv[1]);

    if (binary.load() != ElfBinary::Success) {
        std::cerr << "Failed to load elf binary" << std::endl;
        return 1;
    }

    binary.decodeToContainer(instructions);
    std::set<uint32_t> leaders = getBasicBlocksLeaders(instructions);

    auto text = binary.getTypeSections(ElfBinarySection::Text);
    uint32_t textSize = 0; // Sum of the size of all sections which are executable
    uint32_t textStartAddress = 1 << 31; // Lowest start address among executable sections, used for calculating zero based pc (To make dispatch array more compact)

    for (const auto& ref : text) {
        textSize += ref.get().getSize();
        textStartAddress = std::min(textStartAddress, ref.get().getStartAddress());
    }

    output.open("./sbt/translated/src.cpp");

    emit("#include <iostream>\n");
    emit("#include <cstdint>\n");
    emit("#include <iomanip>\n");
    emit("#include <runtime/syscall.h>\n");
    emit("#include <runtime/Context.h>\n");
    emit("#include <runtime/Memory.h>\n\n");

    emit("Memory memory;\n");
    emit(std::format("void* dispatch[{}] = {{0}};\n", textSize));

    emitInfoPrint();

    emit("\n\nint main() {\n");

    emit("Context ctx(memory);\n");
    emit(std::format("ctx.pc = 0x{:X};\n", binary.getEntryAddress()));
    emit(std::format("\tctx.reg[2] = 0x{:X};\n", MEM_SIZE));

    // not present for binaries compiled for baremetal, without picolibc
    bool hasStartup = binary.getSymbolAddress("_start").has_value();

    if (!hasStartup) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        emit(std::format("\tctx.reg[1] = 0x{:X};\n", BASE_RA));
        emit(std::format("\tctx.reg[3] = 0x{:X};\n\n", binary.getSymbolAddress("__global_pointer$").value_or(0)));
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
            emit(std::format("\tctx.memory.write<uint32_t>(0x{:X}, 0x{:X});\n", dataAddr, word), word != 0);
            dataAddr += 4;
        }
    }

    emit("\n\tuint32_t address;\n");
    emit("\n\twhile (true) {\n");
    emit(std::format("\t\tuint32_t pcDispatchIndex = (ctx.pc - 0x{:X}) / 4;\n", textStartAddress));
    emit(std::format("\t\tif (ctx.pc == 0x{:X}) {{ printInfo(ctx); return 0; }}\n", BASE_RA)); // stop execution on baremetal, if no exit syscall is used
    emit("\t\tgoto *dispatch[pcDispatchIndex];\n\n");

    for (const auto& inst : instructions) {
        emitInstruction(inst, leaders.contains(inst.address));
    }

    indent = 1;
    emit("}\n");

    emit("INVALID:\n");
    emit("\tstd::cout << \"Invalid instruction at 0x\" << std::hex << ctx.pc << std::dec << std::endl;\n");
    emit("\treturn 1;\n");

    indent = 0;
    emit("}\n");

    output.close();

    return 0;
}