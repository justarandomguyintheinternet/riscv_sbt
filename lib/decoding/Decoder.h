#ifndef RISCV_EMU_DECODER_H
#define RISCV_EMU_DECODER_H

#include <cstdint>
#include "Instructions.h"

struct Instruction {
    EInstruction type;
    uint32_t instruction;
    uint32_t immediate;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
};

namespace Decoder {
    // Extract minor opcode, 3 bit
    uint8_t getFunct3(uint32_t instruction);
    // Extra distinction for R formated instructions, 7 bit
    uint8_t getFunct7(uint32_t instruction);
    // 7 bit
    uint8_t getOpcode(uint32_t instruction);

    // 5 bit register adresses
    uint8_t getRS1(uint32_t instruction) ;
    uint8_t getRS2(uint32_t instruction);
    uint8_t getRD(uint32_t instruction);

    int32_t I_FMT_imm(uint32_t instruction);
    int32_t S_FMT_imm(uint32_t instruction);
    int32_t B_FMT_imm(uint32_t instruction);
    int32_t J_FMT_imm(uint32_t instruction);
    int32_t U_FMT_imm(uint32_t instruction);
    uint8_t getShift(uint32_t instruction) ;
    uint8_t getSHType(uint32_t instruction);

    Instruction decode(uint32_t data);
}

#endif //RISCV_EMU_DECODER_H
