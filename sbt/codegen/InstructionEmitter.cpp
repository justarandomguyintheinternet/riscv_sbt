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

void InstructionEmitter::beginFunction(const LiftedFunction& lifted, std::span<const Instruction> instructions) {
    function = &lifted;
    activeLabels.clear();
    resetTracked();

    // With a label table every leader is reachable, without one only the branches inside the body reach a block
    if (lifted.hasLocalDispatch) {
        for (uint32_t address = lifted.start; address < lifted.end; address += 4) {
            if (leaders.contains(address)) {
                activeLabels.insert(address);
            }
        }

        return;
    }

    for (const auto& instruction : instructions) {
        const TransferKind kind = classifyTransfer(instruction);
        const uint32_t target = instruction.address + instruction.immediate;

        const bool isLocal = kind == TransferKind::Branch || kind == TransferKind::DirectJump
            ? lifted.contains(target)
            : kind == TransferKind::Call && lifted.contains(target) && !functionMap.isEntry(target);

        if (isLocal && leaders.contains(target)) {
            activeLabels.insert(target);
        }
    }
}

// Jumps have no return path, so a target outside this function is either a tail call or an escape
std::string InstructionEmitter::transferTo(uint32_t target) const {
    if (function->contains(target)) {
        return std::format("goto L{:X};", target);
    }

    if (functionMap.isSplit() && functionMap.isEntry(target)) {
        return std::format("return {}();", functionMap.lookup(target)->name);
    }

    return std::format("return 0x{:X};", target); // escape, runTranslated() picks it up from here
}

void InstructionEmitter::emitBranch(std::string_view condition) {
    emit(std::format("if ({}) {}\n", condition, transferTo(current->address + current->immediate)));
}

void InstructionEmitter::emitCall() {
    const uint32_t target = current->address + current->immediate;
    const uint32_t returnAddress = current->address + 4;

    emit(std::format("{} = 0x{:X};\n", REG(RD), returnAddress), current->rd);

    if (functionMap.isSplit() && functionMap.isEntry(target)) {
        emit(std::format("if (const uint32_t escaped = {}(); escaped != 0x{:X}) {{ return escaped; }}\n",
                         functionMap.lookup(target)->name, returnAddress));
        resetTracked(); // the callee is free to clobber every guest register
        return;
    }

    emit(std::format("{}\n", transferTo(target)));
}

void InstructionEmitter::emitReturn() {
    emit(std::format("return {};\n", REG(RS1)));
}

void InstructionEmitter::emitIndirectTransfer(bool linking) {
    // The target has to be materialised before rd is written, jalr is allowed to use the same register for both
    if (current->immediate == 0) {
        emit(std::format("target = {};\n", REG(RS1)));
    } else {
        emit(std::format("target = {} + {};\n", REG(RS1), current->immediate));
    }

    const uint32_t returnAddress = current->address + 4;

    if (linking) {
        emit(std::format("{} = 0x{:X};\n", REG(RD), returnAddress), current->rd);
    }

    if (emitter.getOptions().profileIndirect && !functionMap.isSplit()) {
        emit("timerActive = true; timer = __rdtsc();\n");
    }

    if (functionMap.isSplit() && linking) {
        emit(std::format("if (const uint32_t escaped = callIndirect(target); escaped != 0x{:X}) {{ return escaped; }}\n",
                         returnAddress));
        resetTracked();
        return;
    }

    emitPredictedTargets();
    emitIndirectDispatch();
}

// Hardcode the most frequently taken targets, so the common case does not go through the label table
void InstructionEmitter::emitPredictedTargets() {
    if (!emitter.getOptions().softwareBranchPrediction) {
        return;
    }

    auto branchDestinations = profilingInfo.getIndirectBranchTargets(current->address);

    for (int i = 0; i < 3 && !branchDestinations.empty(); ++i) {
        auto max_it = std::max_element(branchDestinations.begin(), branchDestinations.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        if (leaders.contains(max_it->first)) {
            emit(std::format("if (target == 0x{:X}) {}\n", max_it->first, transferTo(max_it->first)));
        }

        branchDestinations.erase(max_it);
    }
}

void InstructionEmitter::emitIndirectDispatch() {
    emit(std::format("dispatchIndex = (target - 0x{:X}) / 4;\n", function->start));
    emit(std::format("if (dispatchIndex >= {}) {{ goto ESCAPE; }}\n", function->wordCount()));
    emit("goto *localDispatch[dispatchIndex];\n");
}

void InstructionEmitter::emitInstruction(const Instruction& instruction) {
    emitter.setIndent(1);
    if (leaders.contains(instruction.address)) {
        emitter.emitIf(std::format("L{:X}:\n", instruction.address), activeLabels.contains(instruction.address));
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
            emitBranch(std::format("{} == {}", REG(RS1), REG(RS2)));
            break;
        case EInstruction::BNE:
            emitBranch(std::format("{} != {}", REG(RS1), REG(RS2)));
            break;
        case EInstruction::BLT:
            emitBranch(std::format("static_cast<int32_t>({}) < static_cast<int32_t>({})", REG(RS1), REG(RS2)));
            break;
        case EInstruction::BGE:
            emitBranch(std::format("static_cast<int32_t>({}) >= static_cast<int32_t>({})", REG(RS1), REG(RS2)));
            break;
        case EInstruction::BLTU:
            emitBranch(std::format("{} < {}", REG(RS1), REG(RS2)));
            break;
        case EInstruction::BGEU:
            emitBranch(std::format("{} >= {}", REG(RS1), REG(RS2)));
            break;
        case EInstruction::JAL:
            if (classifyTransfer(instruction) == TransferKind::Call) {
                emitCall();
            } else {
                emit(std::format("{}\n", transferTo(instruction.address + instruction.immediate)));
            }
            break;
        case EInstruction::JALR: {
            const TransferKind kind = classifyTransfer(instruction);

            if (functionMap.isSplit() && kind == TransferKind::Return) {
                emitReturn();
            } else {
                emitIndirectTransfer(kind == TransferKind::IndirectCall);
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
            emit(std::format("memory.write<uint32_t>({}, {});\n", REG(RS1), REG(RS2))); // store happens even when the result is discarded into x0
            emit(std::format("{} = 0;\n", REG(RD)), instruction.rd);
            break;
        case EInstruction::INVALID:
            emitter.emitRegisterStore(); // printf clobbers the caller saved hosts registers pinned guest registers live in
            emit(std::format("printf(\"Unsupported instruction at 0x{:X}\\n\");\n", instruction.address));
            emitter.emitRegisterLoad();
            break;
    }

    current = nullptr;
}
