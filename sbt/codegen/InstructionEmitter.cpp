#include "codegen/InstructionEmitter.h"

#include <runtime/registers.h>

#include <algorithm>
#include <cstdio>
#include <format>

std::string InstructionEmitter::REG(InstructionField field) const {
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

    if (field != InstructionField::RD && regKnown[index]) {
        return std::format("{}", regValues[index]);
    }

    auto mapped = emitter.getHostRegister(index);
    if (emitter.getOptions().pinRegisters && mapped.has_value()) {
        return std::format("x{}", index);
    }

    return std::format("ctx.reg[{}]", index);
}

void InstructionEmitter::emit(std::string_view text, uint8_t rd) {
    if (rd > 0) {
        emit(text);
    }

    regKnown[rd] = false;
}

void InstructionEmitter::emitLoadSaveAddress(const Instruction& instruction) {
    if (instruction.immediate == 0) {
        emit(std::format("address = {};\n", REG(RS1)));
    } else {
        emit(std::format("address = {} + {};\n", REG(RS1), instruction.immediate));
    }
}

// https://github.com/libriscv/libriscv/blob/master/lib/libriscv/tr_emit.cpp#L243-L254
void InstructionEmitter::emitOp(std::string_view op, std::string_view shortOp, bool ordered) {
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

void InstructionEmitter::track(uint8_t reg, uint32_t value) {
    regValues[reg] = value;
    regKnown[reg] = true;
}

void InstructionEmitter::resetTracked() {
    for (bool& i : regKnown) {
        i = false;
    }
}

void InstructionEmitter::emitInstruction(const Instruction& instruction) {
    emitter.setIndent(1);
    if (leaders.contains(instruction.address)) {
        emit(std::format("L{:X}:\n", instruction.address));
        resetTracked();
    }
    emitter.setIndent(2);

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
            emit(std::format("{} = static_cast<uint32_t>(static_cast<int32_t>({}) >> ({} & 0x1f));\n", REG(RD), REG(RS1), REG(RS2)), instruction.rd);
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
        case EInstruction::JALR: {
            emit(std::format("{} = 0x{:X};\n", REG(RD), instruction.address + 4), instruction.rd);
            emit(std::format("ctx.pc = {} + {};\n", instruction.immediate, REG(RS1)));
            if (emitter.getOptions().profileIndirect && !emitter.getOptions().translationChaining) {
                emit("timerActive = true; timer = __rdtsc();\n");
            }

            if (emitter.getOptions().softwareBranchPrediction) {
                auto branchDestinations = profilingInfo.getIndirectBranchTargets(instruction.address);

                // Hardcode top 3 most frequently used branches with direct targets
                for (int i = 0; i < 3 && !branchDestinations.empty(); ++i) {
                    auto max_it = std::max_element(branchDestinations.begin(), branchDestinations.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; });

                    if (leaders.contains(max_it->first)) {
                        emit(std::format("if (ctx.pc == 0x{:X}) goto L{:X};\n", max_it->first, max_it->first));
                    }

                    branchDestinations.erase(max_it);
                }
            }

            if (emitter.getOptions().translationChaining) {
                emit(std::format("pcDispatchIndex = (ctx.pc - 0x{:X}) / 4;\n", textStartAddress));
                emit("goto *dispatch[pcDispatchIndex];\n");
            } else {
                emit("continue;\n");
            }
            break;
        }
        case EInstruction::LUI:
            emit(std::format("{} = {};\n", REG(RD), instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::AUIPC:
            emit(std::format("{} = 0x{:X} + 0x{:X};\n", REG(RD), instruction.address, instruction.immediate << 12), instruction.rd);
            break;
        case EInstruction::ECALL:
            emitter.emitRegisterStore();
            if (!regKnown[a7]) {
                printf("a7 not tracked, 0x%x\n", instruction.address);
                emit(std::format("Syscall::handle(ctx);\n"));
            } else {
                emit(std::format("Syscall::handle(ctx, {});\n", regValues[a7]));
            }
            emitter.emitRegisterLoad();
            regKnown[a0] = false; // syscall return values gets written into a0
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
        // RV32A
        case EInstruction::LR_W:
            emit(std::format("{} = memory.read<uint32_t>({});\n", REG(RD), REG(RS1)), instruction.rd);
            break;
        case EInstruction::SC_W:
            emit(std::format("memory.write<uint32_t>({}, {});\n", REG(RS1), REG(RS2)), instruction.rd);
            emit(std::format("{} = 0;\n", REG(RD)), instruction.rd);
            break;
        case EInstruction::INVALID:
            emit(std::format("printf(\"Unsupported instruction at 0x{:X}\\n\");\n", instruction.address));
            break;
    }

    current = nullptr;
}
