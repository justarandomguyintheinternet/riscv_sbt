#ifndef RISCV_EMU_INSTRUCTIONS_H
#define RISCV_EMU_INSTRUCTIONS_H

#include <cstdint>

enum class EInstruction {
    ADD,
    SUB,
    XOR,
    OR,
    AND,
    SLL,
    SRL,
    SRA,
    SLT,
    SLTU,
    ADDI,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,
    SLTI,
    SLTIU,
    LB,
    LH,
    LW,
    LBU,
    LHU,
    SB,
    SH,
    SW,
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,
    JAL,
    JALR,
    LUI,
    AUIPC,
    ECALL,
    EBREAK,
    FENCE,
    // RV32M
    MUL,
    MULH,
    MULHSU,
    MULHU,
    DIV,
    DIVU,
    REM,
    REMU,

    // RV32A (subset)
    LR_W,
    SC_W,
    INVALID,
};

enum class EInstructionFMT {
    R, // Register
    I, // Immediate
    S, // Store
    B, // Branch
    J, // Jump
    U // Upper IMM
};

struct InstructionSpec {
    EInstruction type;
    EInstructionFMT format;
    uint8_t opcode;
    bool useFunct3;
    uint8_t funct3;
    bool useFunct7;
    uint8_t funct7;
    uint32_t bits_mask; // Bits to check, used for ecall/ebreak
    uint32_t bits_pattern; // Expected bits to be set
};

inline constexpr InstructionSpec RV32I[] = {
    // R FMT: opcode + funct3 + funct7
    { EInstruction::ADD,    EInstructionFMT::R, 0b0110011, true,  0x0, true,  0x00, 0x0, 0x0 },
    { EInstruction::SUB,    EInstructionFMT::R, 0b0110011, true,  0x0, true,  0x20, 0x0, 0x0 },
    { EInstruction::XOR,    EInstructionFMT::R, 0b0110011, true,  0x4, true,  0x00, 0x0, 0x0 },
    { EInstruction::OR,     EInstructionFMT::R, 0b0110011, true,  0x6, true,  0x00, 0x0, 0x0 },
    { EInstruction::AND,    EInstructionFMT::R, 0b0110011, true,  0x7, true,  0x00, 0x0, 0x0 },
    { EInstruction::SLL,    EInstructionFMT::R, 0b0110011, true,  0x1, true,  0x00, 0x0, 0x0 },
    { EInstruction::SRL,    EInstructionFMT::R, 0b0110011, true,  0x5, true,  0x00, 0x0, 0x0 },
    { EInstruction::SRA,    EInstructionFMT::R, 0b0110011, true,  0x5, true,  0x20, 0x0, 0x0 },
    { EInstruction::SLT,    EInstructionFMT::R, 0b0110011, true,  0x2, true,  0x00, 0x0, 0x0 },
    { EInstruction::SLTU,   EInstructionFMT::R, 0b0110011, true,  0x3, true,  0x00, 0x0, 0x0 },

    // I FMT arithmetics: opcode + funct3
    { EInstruction::ADDI,   EInstructionFMT::I, 0b0010011, true,  0x0, false, 0x00, 0x0, 0x0 },
    { EInstruction::XORI,   EInstructionFMT::I, 0b0010011, true,  0x4, false, 0x00, 0x0, 0x0 },
    { EInstruction::ORI,    EInstructionFMT::I, 0b0010011, true,  0x6, false, 0x00, 0x0, 0x0 },
    { EInstruction::ANDI,   EInstructionFMT::I, 0b0010011, true,  0x7, false, 0x00, 0x0, 0x0 },
    // I FMT shifts: opcode + funct3 + funct7 (SHType)
    { EInstruction::SLLI,   EInstructionFMT::I, 0b0010011, true,  0x1, true,  0x00, 0x0, 0x0 },
    { EInstruction::SRLI,   EInstructionFMT::I, 0b0010011, true,  0x5, true,  0x00, 0x0, 0x0 },
    { EInstruction::SRAI,   EInstructionFMT::I, 0b0010011, true,  0x5, true,  0x20, 0x0, 0x0 },
    { EInstruction::SLTI,   EInstructionFMT::I, 0b0010011, true,  0x2, false, 0x00, 0x0, 0x0 },
    { EInstruction::SLTIU,  EInstructionFMT::I, 0b0010011, true,  0x3, false, 0x00, 0x0, 0x0 },

    // I FMT loading: opcode + funct3
    { EInstruction::LB,     EInstructionFMT::I, 0b0000011, true,  0x0, false, 0x00, 0x0, 0x0 },
    { EInstruction::LH,     EInstructionFMT::I, 0b0000011, true,  0x1, false, 0x00, 0x0, 0x0 },
    { EInstruction::LW,     EInstructionFMT::I, 0b0000011, true,  0x2, false, 0x00, 0x0, 0x0 },
    { EInstruction::LBU,    EInstructionFMT::I, 0b0000011, true,  0x4, false, 0x00, 0x0, 0x0 },
    { EInstruction::LHU,    EInstructionFMT::I, 0b0000011, true,  0x5, false, 0x00, 0x0, 0x0 },

    // S FMT: opcode + funct3
    { EInstruction::SB,     EInstructionFMT::S, 0b0100011, true,  0x0, false, 0x00, 0x0, 0x0 },
    { EInstruction::SH,     EInstructionFMT::S, 0b0100011, true,  0x1, false, 0x00, 0x0, 0x0 },
    { EInstruction::SW,     EInstructionFMT::S, 0b0100011, true,  0x2, false, 0x00, 0x0, 0x0 },

    // B FMT: opcode + funct3
    { EInstruction::BEQ,    EInstructionFMT::B, 0b1100011, true,  0x0, false, 0x00, 0x0, 0x0 },
    { EInstruction::BNE,    EInstructionFMT::B, 0b1100011, true,  0x1, false, 0x00, 0x0, 0x0 },
    { EInstruction::BLT,    EInstructionFMT::B, 0b1100011, true,  0x4, false, 0x00, 0x0, 0x0 },
    { EInstruction::BGE,    EInstructionFMT::B, 0b1100011, true,  0x5, false, 0x00, 0x0, 0x0 },
    { EInstruction::BLTU,   EInstructionFMT::B, 0b1100011, true,  0x6, false, 0x00, 0x0, 0x0 },
    { EInstruction::BGEU,   EInstructionFMT::B, 0b1100011, true,  0x7, false, 0x00, 0x0, 0x0 },

    // J FMT: opcode only
    { EInstruction::JAL,    EInstructionFMT::J, 0b1101111, false, 0x0, false, 0x00, 0x0, 0x0 },

    // JALR: opcode + funct3
    { EInstruction::JALR,   EInstructionFMT::I, 0b1100111, true,  0x0, false, 0x00, 0x0, 0x0 },

    // U FMT: opcode only
    { EInstruction::LUI,    EInstructionFMT::U, 0b0110111, false, 0x0, false, 0x00, 0x0, 0x0 },
    { EInstruction::AUIPC,  EInstructionFMT::U, 0b0010111, false, 0x0, false, 0x00, 0x0, 0x0 },

    // ecall / ebreak: opcode only, differentiated by bits_mask/pattern (funct12 field [31:20])
    { EInstruction::ECALL,  EInstructionFMT::I, 0b1110011, false, 0x0, false, 0x00, 0xFFF00000, 0x00000000 },
    { EInstruction::EBREAK, EInstructionFMT::I, 0b1110011, false, 0x0, false, 0x00, 0xFFF00000, 0x00100000 },
};

inline constexpr InstructionSpec RV32M[] = {
    { EInstruction::MUL,     EInstructionFMT::R, 0b0110011, true,  0x0, true,  0x01 },
    { EInstruction::MULH,    EInstructionFMT::R, 0b0110011, true,  0x1, true,  0x01 },
    { EInstruction::MULHSU,  EInstructionFMT::R, 0b0110011, true,  0x2, true,  0x01 },
    { EInstruction::MULHU,   EInstructionFMT::R, 0b0110011, true,  0x3, true,  0x01 },
    { EInstruction::DIV,     EInstructionFMT::R, 0b0110011, true,  0x4, true,  0x01 },
    { EInstruction::DIVU,    EInstructionFMT::R, 0b0110011, true,  0x5, true,  0x01 },
    { EInstruction::REM,     EInstructionFMT::R, 0b0110011, true,  0x6, true,  0x01 },
    { EInstruction::REMU,    EInstructionFMT::R, 0b0110011, true,  0x7, true,  0x01 }
};

inline constexpr InstructionSpec RV32A[] = {
    { EInstruction::LR_W,     EInstructionFMT::R, 0b0101111, true,  0x2, false,  0x0, 0xF8000000, 0x10000000 },
    { EInstruction::SC_W,    EInstructionFMT::R, 0b0101111, true,  0x2, false,  0x0, 0xF8000000, 0x18000000 }
};

#endif //RISCV_EMU_INSTRUCTIONS_H
