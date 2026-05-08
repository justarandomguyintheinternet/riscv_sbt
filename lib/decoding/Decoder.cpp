#include "Decoder.h"
#include <span>

namespace Decoder {
    uint8_t getFunct3(uint32_t instruction) {
        return (instruction >> 12) & 0x7;
    }

    uint8_t getFunct7(uint32_t instruction) {
        return (instruction >> 25) & 0x7f;
    }

    uint8_t getOpcode(uint32_t instruction) {
        return instruction & 0x7f;
    }

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
        return getFunct7(instruction);
    }

    Instruction decode(uint32_t data, uint32_t address) {
        static constexpr std::span<const InstructionSpec> SPEC_LISTS[] = { RV32I, RV32M };

        for (auto list : SPEC_LISTS) {
            for (auto& spec : list) {
                if (getOpcode(data) == spec.opcode &&
                    (spec.useFunct3 ? getFunct3(data) == spec.funct3 : true) &&
                    (spec.useFunct7 ? getFunct7(data) == spec.funct7 : true) &&
                    (spec.bits_mask == 0 || (data & spec.bits_mask) == spec.bits_pattern)) {

                    Instruction instr{};

                    instr.type = spec.type;
                    instr.instruction = data;
                    instr.rs1 = getRS1(data);
                    instr.rs2 = getRS2(data);
                    instr.rd = getRD(data);
                    instr.address = address;

                    switch (spec.format) {
                        case EInstructionFMT::R:
                            break;
                        case EInstructionFMT::I:
                            instr.immediate = I_FMT_imm(data);
                            break;
                        case EInstructionFMT::S:
                            instr.immediate = S_FMT_imm(data);
                            break;
                        case EInstructionFMT::B:
                            instr.immediate = B_FMT_imm(data);
                            break;
                        case EInstructionFMT::J:
                            instr.immediate = J_FMT_imm(data);
                            break;
                        case EInstructionFMT::U:
                            instr.immediate = U_FMT_imm(data);
                            break;
                    }
                    return instr;
                }
            }
        }

        return { .type = EInstruction::INVALID };
    }
}