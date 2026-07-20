#include "Interpreter.h"
#include <cstdio>
#include "decoding/Decoder.h"
#include "runtime/registers.h"
#include "runtime/syscall.h"

// Fetch instruction at pc from memory, decode and execute it
void Interpreter::runInstruction(Context& ctx) {
    auto instruction = ctx.memory.read<uint32_t>(ctx.pc);
    runInstruction(ctx, instruction);
};

void Interpreter::runInstruction(Context &ctx, uint32_t instruction) {
    uint8_t op = Decoder::getOpcode(instruction);
    uint8_t funct3 = Decoder::getFunct3(instruction);
    uint8_t funct7 = Decoder::getFunct7(instruction);
    uint8_t rs1 = Decoder::getRS1(instruction);
    uint8_t rs2 = Decoder::getRS2(instruction);
    uint8_t rd = Decoder::getRD(instruction);

    // addi
    if (op == 0b0010011 && funct3 == 0x0) {
        ctx.reg[rd] = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, EInstruction::ADDI);
    }
    // xori
    else if (op == 0b0010011 && funct3 == 0x4) {
        ctx.reg[rd] = ctx.reg[rs1] ^ Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, EInstruction::XORI);
    }
    // ori
    else if (op == 0b0010011 && funct3 == 0x6) {
        ctx.reg[rd] = ctx.reg[rs1] | Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, EInstruction::ORI);
    }
    // andi
    else if (op == 0b0010011 && funct3 == 0x7) {
        ctx.reg[rd] = ctx.reg[rs1] & Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, EInstruction::ANDI);
    }
    // slli
    else if (op == 0b0010011 && funct3 == 0x1 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << Decoder::getShift(instruction);
        LOG_INST(ctx, EInstruction::SLLI);
    }
    // srli
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> Decoder::getShift(instruction);
        LOG_INST(ctx, EInstruction::SRLI);
    }
    // srai
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x20) {
        uint8_t shift = Decoder::getShift(instruction);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx, EInstruction::SRAI);
    }
    // slti
    else if (op == 0b0010011 && funct3 == 0x2) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, EInstruction::SLTI);
    }
    // sltiu
    else if (op == 0b0010011 && funct3 == 0x3) {
        ctx.reg[rd] = ctx.reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
        LOG_INST(ctx, EInstruction::SLTIU);
    }

    // add
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] + ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::ADD);
    }
    // sub
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x20) {
        ctx.reg[rd] = ctx.reg[rs1] - ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::SUB);
    }
    // xor
    else if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] ^ ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::XOR);
    }
    // or
    else if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] | ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::OR);
    }
    // and
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] & ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::AND);
    }
    // sll
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx, EInstruction::SLL);
    }
    // srl
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx, EInstruction::SRL);
    }
    // sra
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x20) {
        uint8_t shift = static_cast<uint8_t>(ctx.reg[rs2] & 0x1f);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx, EInstruction::SRA);
    }
    // slt
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x00) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2]);
        LOG_INST(ctx, EInstruction::SLT);
    }
    // sltu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] < ctx.reg[rs2];
        LOG_INST(ctx, EInstruction::SLTU);
    }
    // lb
    else if (op == 0b0000011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
        LOG_INST(ctx, EInstruction::LB);
    }
    // lbu
    else if (op == 0b0000011 && funct3 == 0x4) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint8_t>(address);
        LOG_INST(ctx, EInstruction::LBU);
    }
    // lh
    else if (op == 0b0000011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
        LOG_INST(ctx, EInstruction::LH);
    }
    // lhu
    else if (op == 0b0000011 && funct3 == 0x5) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint16_t>(address);
        LOG_INST(ctx, EInstruction::LHU);
    }
    // lw
    else if (op == 0b0000011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<int32_t>(address);
        LOG_INST(ctx, EInstruction::LW);
    }

    // sb
    else if (op == 0b0100011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint8_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, EInstruction::SB);
    }
    // sh
    else if (op == 0b0100011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint16_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, EInstruction::SH);
    }
    // sw
    else if (op == 0b0100011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint32_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, EInstruction::SW);
    }

    // beq
    else if (op == 0b1100011 && funct3 == 0x0) {
        LOG_INST(ctx, EInstruction::BEQ);
        if (ctx.reg[rs1] == ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bne
    else if (op == 0b1100011 && funct3 == 0x1) {
        LOG_INST(ctx, EInstruction::BNE);
        if (ctx.reg[rs1] != ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // blt
    else if (op == 0b1100011 && funct3 == 0x4) {
        LOG_INST(ctx, EInstruction::BLT);
        if (static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bge
    else if (op == 0b1100011 && funct3 == 0x5) {
        LOG_INST(ctx, EInstruction::BGE);
        if (static_cast<int32_t>(ctx.reg[rs1]) >= static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bltu
    else if (op == 0b1100011 && funct3 == 0x6) {
        LOG_INST(ctx, EInstruction::BLTU);
        if (ctx.reg[rs1] < ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bgeu
    else if (op == 0b1100011 && funct3 == 0x7) {
        LOG_INST(ctx, EInstruction::BGEU);
        if (ctx.reg[rs1] >= ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }

    // jal
    else if (op == 0b1101111) {
        LOG_INST(ctx, EInstruction::JAL);
        ctx.reg[rd] = ctx.pc + 4;
        ctx.pc += Decoder::J_FMT_imm(instruction) - 4;
    }
    // jalr
    else if (op == 0b1100111 && funct3 == 0x0) {
        LOG_JMP(ctx, Decoder::I_FMT_imm(instruction) + ctx.reg[rs1]);
        LOG_INST(ctx, EInstruction::JALR);

        ctx.reg[rd] = ctx.pc + 4;
        ctx.pc = Decoder::I_FMT_imm(instruction) + ctx.reg[rs1] - 4;
    }

    // lui
    else if (op == 0b0110111) {
        ctx.reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
        LOG_INST(ctx, EInstruction::LUI);
    }
    // auipc
    else if (op == 0b0010111) {
        ctx.reg[rd] = ctx.pc + (Decoder::U_FMT_imm(instruction) << 12);
        LOG_INST(ctx, EInstruction::AUIPC);
    }

    // ecall
    else if (op == 0b1110011 && Decoder::I_FMT_imm(instruction) == 0x0) {
        LOG_INST(ctx, EInstruction::ECALL);
        Syscall::handle(ctx);
    }

    // RV32M

    // mul
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x01) {
        ctx.reg[rd] = static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2]) & 0xFFFFFFFF;
        LOG_INST(ctx, EInstruction::MUL);
    }
    // mulh
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs2]))) >> 32;
        LOG_INST(ctx, EInstruction::MULH);
    }
    // mulhsu
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx, EInstruction::MULHSU);
    }
    // mulhu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx, EInstruction::MULHU);
    }
    // div
    else if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = -1;
        } else if (ctx.reg[rs1] == 0x80000000 && ctx.reg[rs2] == 0xFFFFFFFF) {
            ctx.reg[rd] = 0x80000000;
        } else {
            ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) / static_cast<int32_t>(ctx.reg[rs2]));
        }
        LOG_INST(ctx, EInstruction::DIV);
    }
    // divu
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = 0xFFFFFFFF;
        } else {
            ctx.reg[rd] = ctx.reg[rs1] / ctx.reg[rs2];
        }
        LOG_INST(ctx, EInstruction::DIVU);
    }
    // rem
    else if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = ctx.reg[rs1];
        } else if (ctx.reg[rs1] == 0x80000000 && ctx.reg[rs2] == 0xFFFFFFFF) {
            ctx.reg[rd] = 0;
        } else {
            ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) % static_cast<int32_t>(ctx.reg[rs2]));
        }
        LOG_INST(ctx, EInstruction::REM);
    }
    // remu
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = ctx.reg[rs1];
        } else {
            ctx.reg[rd] = ctx.reg[rs1] % ctx.reg[rs2];
        }
        LOG_INST(ctx, EInstruction::REMU);
    // LR_W
    } else if (op == 0b0101111 && funct3 == 0x2 && Decoder::getFunct5(instruction) == 0x02) {
        ctx.reg[rd] = ctx.memory.read<int32_t>(ctx.reg[rs1]);
        LOG_INST(ctx, EInstruction::LR_W);
    // SC_W
    } else if (op == 0b0101111 && funct3 == 0x2 && Decoder::getFunct5(instruction) == 0x03) {
        ctx.memory.write<uint32_t>(ctx.reg[rs1], ctx.reg[rs2]);
        ctx.reg[rd] = 0; // success
        LOG_INST(ctx, EInstruction::SC_W);
    } else {
        printf("Unsupported instruction at 0x%08x\n", ctx.pc);
    }

    ctx.pc += 4;
    ctx.reg[x0] = 0;
}

void Interpreter::runInstructionSwitch(Context& ctx) {
    auto instruction = ctx.memory.read<uint32_t>(ctx.pc);
    runInstructionSwitch(ctx, instruction);
};

void Interpreter::runInstructionSwitch(Context &ctx, uint32_t instruction) {
    uint8_t op = Decoder::getOpcode(instruction);
    uint8_t funct3 = Decoder::getFunct3(instruction);
    uint8_t funct7 = Decoder::getFunct7(instruction);
    uint8_t rs1 = Decoder::getRS1(instruction);
    uint8_t rs2 = Decoder::getRS2(instruction);
    uint8_t rd = Decoder::getRD(instruction);

    switch (op) {
        case 0b0010011:
            switch (funct3) {
                case 0x0:
                    ctx.reg[rd] = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    LOG_INST(ctx, EInstruction::ADDI);
                    break;
                case 0x1:
                    if (Decoder::getSHType(instruction) == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] << Decoder::getShift(instruction);
                        LOG_INST(ctx, EInstruction::SLLI);
                    }
                    break;
                case 0x2:
                    ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < Decoder::I_FMT_imm(instruction);
                    LOG_INST(ctx, EInstruction::SLTI);
                    break;
                case 0x3:
                    ctx.reg[rd] = ctx.reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
                    LOG_INST(ctx, EInstruction::SLTIU);
                    break;
                case 0x4:
                    ctx.reg[rd] = ctx.reg[rs1] ^ Decoder::I_FMT_imm(instruction);
                    LOG_INST(ctx, EInstruction::XORI);
                    break;
                case 0x5:
                    if (Decoder::getSHType(instruction) == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] >> Decoder::getShift(instruction);
                        LOG_INST(ctx, EInstruction::SRLI);
                    } else if (Decoder::getSHType(instruction) == 0x20) {
                        uint8_t shift = Decoder::getShift(instruction);
                        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
                        LOG_INST(ctx, EInstruction::SRAI);
                    }
                    break;
                case 0x6:
                    ctx.reg[rd] = ctx.reg[rs1] | Decoder::I_FMT_imm(instruction);
                    LOG_INST(ctx, EInstruction::ORI);
                    break;
                case 0x7:
                    ctx.reg[rd] = ctx.reg[rs1] & Decoder::I_FMT_imm(instruction);
                    LOG_INST(ctx, EInstruction::ANDI);
                    break;
            }
            break;
        case 0b0110011:
            switch (funct3) {
                case 0x0:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] + ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::ADD);
                    } else if (funct7 == 0x20) {
                        ctx.reg[rd] = ctx.reg[rs1] - ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::SUB);
                    } else if (funct7 == 0x01) {
                        ctx.reg[rd] = static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2]) & 0xFFFFFFFF;
                        LOG_INST(ctx, EInstruction::MUL);
                    }
                    break;
                case 0x1:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] << (ctx.reg[rs2] & 0x1f);
                        LOG_INST(ctx, EInstruction::SLL);
                    } else if (funct7 == 0x01) {
                        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs2]))) >> 32;
                        LOG_INST(ctx, EInstruction::MULH);
                    }
                    break;
                case 0x2:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2]);
                        LOG_INST(ctx, EInstruction::SLT);
                    } else if (funct7 == 0x01) {
                        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
                        LOG_INST(ctx, EInstruction::MULHSU);
                    }
                    break;
                case 0x3:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] < ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::SLTU);
                    } else if (funct7 == 0x01) {
                        ctx.reg[rd] = (static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
                        LOG_INST(ctx, EInstruction::MULHU);
                    }
                    break;
                case 0x4:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] ^ ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::XOR);
                    } else if (funct7 == 0x01) {
                        if (ctx.reg[rs2] == 0) {
                            ctx.reg[rd] = -1;
                        } else if (ctx.reg[rs1] == 0x80000000 && ctx.reg[rs2] == 0xFFFFFFFF) {
                            ctx.reg[rd] = 0x80000000;
                        } else {
                            ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) / static_cast<int32_t>(ctx.reg[rs2]));
                        }
                        LOG_INST(ctx, EInstruction::DIV);
                    }
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] >> (ctx.reg[rs2] & 0x1f);
                        LOG_INST(ctx, EInstruction::SRL);
                    } else if (funct7 == 0x20) {
                        uint8_t shift = static_cast<uint8_t>(ctx.reg[rs2] & 0x1f);
                        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
                        LOG_INST(ctx, EInstruction::SRA);
                    } else if (funct7 == 0x01) {
                        if (ctx.reg[rs2] == 0) {
                            ctx.reg[rd] = 0xFFFFFFFF;
                        } else {
                            ctx.reg[rd] = ctx.reg[rs1] / ctx.reg[rs2];
                        }
                        LOG_INST(ctx, EInstruction::DIVU);
                    }
                    break;
                case 0x6:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] | ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::OR);
                    } else if (funct7 == 0x01) {
                        if (ctx.reg[rs2] == 0) {
                            ctx.reg[rd] = ctx.reg[rs1];
                        } else if (ctx.reg[rs1] == 0x80000000 && ctx.reg[rs2] == 0xFFFFFFFF) {
                            ctx.reg[rd] = 0;
                        } else {
                            ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) % static_cast<int32_t>(ctx.reg[rs2]));
                        }
                        LOG_INST(ctx, EInstruction::REM);
                    }
                    break;
                case 0x7:
                    if (funct7 == 0x00) {
                        ctx.reg[rd] = ctx.reg[rs1] & ctx.reg[rs2];
                        LOG_INST(ctx, EInstruction::AND);
                    } else if (funct7 == 0x01) {
                        if (ctx.reg[rs2] == 0) {
                            ctx.reg[rd] = ctx.reg[rs1];
                        } else {
                            ctx.reg[rd] = ctx.reg[rs1] % ctx.reg[rs2];
                        }
                        LOG_INST(ctx, EInstruction::REMU);
                    }
                    break;
            }
            break;
        case 0b0000011:
            switch (funct3) {
                case 0x0: {
                    uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
                    LOG_INST(ctx, EInstruction::LB);
                    break;
                }
                case 0x1: {
                    uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
                    LOG_INST(ctx, EInstruction::LH);
                    break;
                }
                case 0x2: {
                    uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    ctx.reg[rd] = ctx.memory.read<int32_t>(address);
                    LOG_INST(ctx, EInstruction::LW);
                    break;
                }
                case 0x4: {
                    uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    ctx.reg[rd] = ctx.memory.read<uint8_t>(address);
                    LOG_INST(ctx, EInstruction::LBU);
                    break;
                }
                case 0x5: {
                    uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
                    ctx.reg[rd] = ctx.memory.read<uint16_t>(address);
                    LOG_INST(ctx, EInstruction::LHU);
                    break;
                }
            }
            break;
        case 0b0100011:
            switch (funct3) {
                case 0x0: {
                    uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
                    ctx.memory.write<uint8_t>(address, ctx.reg[rs2]);
                    LOG_INST(ctx, EInstruction::SB);
                    break;
                }
                case 0x1: {
                    uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
                    ctx.memory.write<uint16_t>(address, ctx.reg[rs2]);
                    LOG_INST(ctx, EInstruction::SH);
                    break;
                }
                case 0x2: {
                    uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
                    ctx.memory.write<uint32_t>(address, ctx.reg[rs2]);
                    LOG_INST(ctx, EInstruction::SW);
                    break;
                }
            }
            break;
        case 0b1100011:
            switch (funct3) {
                case 0x0:
                    LOG_INST(ctx, EInstruction::BEQ);
                    if (ctx.reg[rs1] == ctx.reg[rs2]) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
                case 0x1:
                    LOG_INST(ctx, EInstruction::BNE);
                    if (ctx.reg[rs1] != ctx.reg[rs2]) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
                case 0x4:
                    LOG_INST(ctx, EInstruction::BLT);
                    if (static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2])) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
                case 0x5:
                    LOG_INST(ctx, EInstruction::BGE);
                    if (static_cast<int32_t>(ctx.reg[rs1]) >= static_cast<int32_t>(ctx.reg[rs2])) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
                case 0x6:
                    LOG_INST(ctx, EInstruction::BLTU);
                    if (ctx.reg[rs1] < ctx.reg[rs2]) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
                case 0x7:
                    LOG_INST(ctx, EInstruction::BGEU);
                    if (ctx.reg[rs1] >= ctx.reg[rs2]) {
                        ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
                    }
                    break;
            }
            break;
        case 0b1101111:
            LOG_INST(ctx, EInstruction::JAL);
            ctx.reg[rd] = ctx.pc + 4;
            ctx.pc += Decoder::J_FMT_imm(instruction) - 4;
            break;
        case 0b1100111:
            if (funct3 == 0x0) {
                LOG_JMP(ctx, Decoder::I_FMT_imm(instruction) + ctx.reg[rs1]);
                LOG_INST(ctx, EInstruction::JALR);

                ctx.reg[rd] = ctx.pc + 4;
                ctx.pc = Decoder::I_FMT_imm(instruction) + ctx.reg[rs1] - 4;
                break;
            }
        case 0b0110111:
            ctx.reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
            LOG_INST(ctx, EInstruction::LUI);
            break;
        case 0b0010111:
            ctx.reg[rd] = ctx.pc + (Decoder::U_FMT_imm(instruction) << 12);
            LOG_INST(ctx, EInstruction::AUIPC);
            break;
        case 0b1110011:
            if (funct3 == 0x0 && Decoder::I_FMT_imm(instruction) == 0x0) {
                LOG_INST(ctx, EInstruction::ECALL);
                Syscall::handle(ctx);
            }
            break;
        case 0b0101111:
            switch (funct3) {
                case 0x2:
                    if (Decoder::getFunct5(instruction) == 0x02) {
                        ctx.reg[rd] = ctx.memory.read<int32_t>(ctx.reg[rs1]);
                        LOG_INST(ctx, EInstruction::LR_W);
                    } else if (Decoder::getFunct5(instruction) == 0x03) {
                        ctx.memory.write<uint32_t>(ctx.reg[rs1], ctx.reg[rs2]);
                        ctx.reg[rd] = 0; // success
                        LOG_INST(ctx, EInstruction::SC_W);
                    }
                    break;
            }
            break;
        default:
            printf("Unsupported base opcode at 0x%08x\n", ctx.pc);
            break;
    }

    ctx.pc += 4;
    ctx.reg[x0] = 0;
}

void Interpreter::runInstructionPredecoded(Context& ctx, Instruction& instruction) {
    switch (instruction.type) {
        case EInstruction::ADD:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] + ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::ADD);
            break;
        case EInstruction::SUB:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] - ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::SUB);
            break;
        case EInstruction::XOR:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] ^ ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::XOR);
            break;
        case EInstruction::OR:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] | ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::OR);
            break;
        case EInstruction::AND:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] & ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::AND);
            break;
        case EInstruction::SLL:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] << (ctx.reg[instruction.rs2] & 0x1f);
            LOG_INST(ctx, EInstruction::SLL);
            break;
        case EInstruction::SRL:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] >> (ctx.reg[instruction.rs2] & 0x1f);
            LOG_INST(ctx, EInstruction::SRL);
            break;
        case EInstruction::SRA: {
            uint8_t shift = static_cast<uint8_t>(ctx.reg[instruction.rs2] & 0x1f);
            ctx.reg[instruction.rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[instruction.rs1]) >> shift);
            LOG_INST(ctx, EInstruction::SRA);
            break;
        }
        case EInstruction::SLT:
            ctx.reg[instruction.rd] =
                static_cast<int32_t>(ctx.reg[instruction.rs1]) < static_cast<int32_t>(ctx.reg[instruction.rs2]);
            LOG_INST(ctx, EInstruction::SLT);
            break;
        case EInstruction::SLTU:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] < ctx.reg[instruction.rs2];
            LOG_INST(ctx, EInstruction::SLTU);
            break;
        case EInstruction::ADDI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] + instruction.immediate;
            LOG_INST(ctx, EInstruction::ADDI);
            break;
        case EInstruction::XORI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] ^ instruction.immediate;
            LOG_INST(ctx, EInstruction::XORI);
            break;
        case EInstruction::ORI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] | instruction.immediate;
            LOG_INST(ctx, EInstruction::ORI);
            break;
        case EInstruction::ANDI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] & instruction.immediate;
            LOG_INST(ctx, EInstruction::ANDI);
            break;
        case EInstruction::SLLI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] << static_cast<uint8_t>(instruction.immediate & 0x1f);
            LOG_INST(ctx, EInstruction::SLLI);
            break;
        case EInstruction::SRLI:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] >> static_cast<uint8_t>(instruction.immediate & 0x1f);
            LOG_INST(ctx, EInstruction::SRLI);
            break;
        case EInstruction::SRAI: {
            uint8_t shift = static_cast<uint8_t>(instruction.immediate & 0x1f);
            ctx.reg[instruction.rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[instruction.rs1]) >> shift);
            LOG_INST(ctx, EInstruction::SRAI);
            break;
        }
        case EInstruction::SLTI:
            ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.reg[instruction.rs1]) < instruction.immediate;
            LOG_INST(ctx, EInstruction::SLTI);
            break;
        case EInstruction::SLTIU:
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] < static_cast<uint32_t>(instruction.immediate);
            LOG_INST(ctx, EInstruction::SLTIU);
            break;
        case EInstruction::LB: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
            LOG_INST(ctx, EInstruction::LB);
            break;
        }
        case EInstruction::LH: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
            LOG_INST(ctx, EInstruction::LH);
            break;
        }
        case EInstruction::LW: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.reg[instruction.rd] = ctx.memory.read<int32_t>(address);
            LOG_INST(ctx, EInstruction::LW);
            break;
        }
        case EInstruction::LBU: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.reg[instruction.rd] = ctx.memory.read<uint8_t>(address);
            LOG_INST(ctx, EInstruction::LBU);
            break;
        }
        case EInstruction::LHU: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.reg[instruction.rd] = ctx.memory.read<uint16_t>(address);
            LOG_INST(ctx, EInstruction::LHU);
            break;
        }
        case EInstruction::SB: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.memory.write<uint8_t>(address, ctx.reg[instruction.rs2]);
            LOG_INST(ctx, EInstruction::SB);
            break;
        }
        case EInstruction::SH: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.memory.write<uint16_t>(address, ctx.reg[instruction.rs2]);
            LOG_INST(ctx, EInstruction::SH);
            break;
        }
        case EInstruction::SW: {
            uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
            ctx.memory.write<uint32_t>(address, ctx.reg[instruction.rs2]);
            LOG_INST(ctx, EInstruction::SW);
            break;
        }
        case EInstruction::BEQ:
            LOG_INST(ctx, EInstruction::BEQ);
            if (ctx.reg[instruction.rs1] == ctx.reg[instruction.rs2]) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::BNE:
            LOG_INST(ctx, EInstruction::BNE);
            if (ctx.reg[instruction.rs1] != ctx.reg[instruction.rs2]) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::BLT:
            LOG_INST(ctx, EInstruction::BLT);
            if (static_cast<int32_t>(ctx.reg[instruction.rs1]) < static_cast<int32_t>(ctx.reg[instruction.rs2])) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::BGE:
            LOG_INST(ctx, EInstruction::BGE);
            if (static_cast<int32_t>(ctx.reg[instruction.rs1]) >= static_cast<int32_t>(ctx.reg[instruction.rs2])) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::BLTU:
            LOG_INST(ctx, EInstruction::BLTU);
            if (ctx.reg[instruction.rs1] < ctx.reg[instruction.rs2]) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::BGEU:
            LOG_INST(ctx, EInstruction::BGEU);
            if (ctx.reg[instruction.rs1] >= ctx.reg[instruction.rs2]) {
                ctx.pc += instruction.immediate - 4;
            }
            break;
        case EInstruction::JAL:
            LOG_INST(ctx, EInstruction::JAL);
            ctx.reg[instruction.rd] = ctx.pc + 4;
            ctx.pc += instruction.immediate - 4;
            break;
        case EInstruction::JALR:
            LOG_JMP(ctx, instruction.immediate + ctx.reg[instruction.rs1]);
            LOG_INST(ctx, EInstruction::JALR);
            ctx.reg[instruction.rd] = ctx.pc + 4;
            ctx.pc = instruction.immediate + ctx.reg[instruction.rs1] - 4;
            break;
        case EInstruction::LUI:
            ctx.reg[instruction.rd] = static_cast<uint32_t>(instruction.immediate) << 12;
            LOG_INST(ctx, EInstruction::LUI);
            break;
        case EInstruction::AUIPC:
            ctx.reg[instruction.rd] = ctx.pc + (static_cast<uint32_t>(instruction.immediate) << 12);
            LOG_INST(ctx, EInstruction::AUIPC);
            break;
        case EInstruction::ECALL:
            LOG_INST(ctx, EInstruction::ECALL);
            Syscall::handle(ctx);
            break;
        case EInstruction::EBREAK:
            printf("Unsupported instruction at 0x%08x\n", ctx.pc);
            break;
        case EInstruction::FENCE:
            printf("Unsupported instruction at 0x%08x\n", ctx.pc);
            break;
        case EInstruction::MUL:
            ctx.reg[instruction.rd] =
                static_cast<uint64_t>(ctx.reg[instruction.rs1]) * static_cast<uint64_t>(ctx.reg[instruction.rs2]) &
                0xFFFFFFFF;
            LOG_INST(ctx, EInstruction::MUL);
            break;
        case EInstruction::MULH:
            ctx.reg[instruction.rd] =
                (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs1])) *
                 static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs2]))) >>
                32;
            LOG_INST(ctx, EInstruction::MULH);
            break;
        case EInstruction::MULHSU:
            ctx.reg[instruction.rd] =
                (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs1])) *
                 static_cast<uint64_t>(ctx.reg[instruction.rs2])) >>
                32;
            LOG_INST(ctx, EInstruction::MULHSU);
            break;
        case EInstruction::MULHU:
            ctx.reg[instruction.rd] =
                (static_cast<uint64_t>(ctx.reg[instruction.rs1]) * static_cast<uint64_t>(ctx.reg[instruction.rs2])) >>
                32;
            LOG_INST(ctx, EInstruction::MULHU);
            break;
        case EInstruction::DIV:
            if (ctx.reg[instruction.rs2] == 0) {
                ctx.reg[instruction.rd] = -1;
            } else if (ctx.reg[instruction.rs1] == 0x80000000 && ctx.reg[instruction.rs2] == 0xFFFFFFFF) {
                ctx.reg[instruction.rd] = 0x80000000;
            } else {
                ctx.reg[instruction.rd] = static_cast<uint32_t>(
                    static_cast<int32_t>(ctx.reg[instruction.rs1]) / static_cast<int32_t>(ctx.reg[instruction.rs2]));
            }
            LOG_INST(ctx, EInstruction::DIV);
            break;
        case EInstruction::DIVU:
            if (ctx.reg[instruction.rs2] == 0) {
                ctx.reg[instruction.rd] = 0xFFFFFFFF;
            } else {
                ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] / ctx.reg[instruction.rs2];
            }
            LOG_INST(ctx, EInstruction::DIVU);
            break;
        case EInstruction::REM:
            if (ctx.reg[instruction.rs2] == 0) {
                ctx.reg[instruction.rd] = ctx.reg[instruction.rs1];
            } else if (ctx.reg[instruction.rs1] == 0x80000000 && ctx.reg[instruction.rs2] == 0xFFFFFFFF) {
                ctx.reg[instruction.rd] = 0;
            } else {
                ctx.reg[instruction.rd] = static_cast<uint32_t>(
                    static_cast<int32_t>(ctx.reg[instruction.rs1]) % static_cast<int32_t>(ctx.reg[instruction.rs2]));
            }
            LOG_INST(ctx, EInstruction::REM);
            break;
        case EInstruction::REMU:
            if (ctx.reg[instruction.rs2] == 0) {
                ctx.reg[instruction.rd] = ctx.reg[instruction.rs1];
            } else {
                ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] % ctx.reg[instruction.rs2];
            }
            LOG_INST(ctx, EInstruction::REMU);
            break;
        case EInstruction::LR_W:
            ctx.reg[instruction.rd] = ctx.memory.read<int32_t>(ctx.reg[instruction.rs1]);
            LOG_INST(ctx, EInstruction::LR_W);
            break;
        case EInstruction::SC_W:
            ctx.memory.write<uint32_t>(ctx.reg[instruction.rs1], ctx.reg[instruction.rs2]);
            ctx.reg[instruction.rd] = 0;
            LOG_INST(ctx, EInstruction::SC_W);
            break;
        case EInstruction::INVALID:
            printf("Unsupported instruction at 0x%08x\n", ctx.pc);
            break;
    }

    ctx.pc += 4;
    ctx.reg[x0] = 0;
}

#define DIRECT_THREADING 1

#if DIRECT_THREADING == 1
#define DISPATCH() \
do { \
    ctx.pc += 4; \
    ctx.reg[x0] = 0; \
    instruction = instructions[(ctx.pc - textStartAddress) / 4]; \
    goto *instruction.handler; \
} while(0)
#else
#define DISPATCH() \
    do { \
    ctx.pc += 4; \
    ctx.reg[x0] = 0; \
    instruction = instructions[(ctx.pc - textStartAddress) / 4]; \
    goto *dispatch[static_cast<uint8_t>(instruction.type)]; \
    } while(0)
#endif

void Interpreter::runInstructionsThreaded(Context& ctx, std::vector<Instruction>& instructions, uint32_t textStartAddress) {
    const static void* dispatch[] = {
        &&L_ADD,
        &&L_SUB,
        &&L_XOR,
        &&L_OR,
        &&L_AND,
        &&L_SLL,
        &&L_SRL,
        &&L_SRA,
        &&L_SLT,
        &&L_SLTU,
        &&L_ADDI,
        &&L_XORI,
        &&L_ORI,
        &&L_ANDI,
        &&L_SLLI,
        &&L_SRLI,
        &&L_SRAI,
        &&L_SLTI,
        &&L_SLTIU,
        &&L_LB,
        &&L_LH,
        &&L_LW,
        &&L_LBU,
        &&L_LHU,
        &&L_SB,
        &&L_SH,
        &&L_SW,
        &&L_BEQ,
        &&L_BNE,
        &&L_BLT,
        &&L_BGE,
        &&L_BLTU,
        &&L_BGEU,
        &&L_JAL,
        &&L_JALR,
        &&L_LUI,
        &&L_AUIPC,
        &&L_ECALL,
        &&L_INVALID,
        &&L_INVALID,
        &&L_MUL,
        &&L_MULH,
        &&L_MULHSU,
        &&L_MULHU,
        &&L_DIV,
        &&L_DIVU,
        &&L_REM,
        &&L_REMU,
        &&L_LR_W,
        &&L_SC_W,
        &&L_INVALID
    };

#if DIRECT_THREADING == 1
    // Must be done in here where the labels live, avoid lookup in dispatch table at runtime to allow for "proper" direct threading
    for (auto& instruction : instructions) {
        instruction.handler = dispatch[static_cast<uint8_t>(instruction.type)];
    }
#endif

    Instruction& instruction = instructions[(ctx.pc - textStartAddress) / 4];
    goto *dispatch[static_cast<uint8_t>(instruction.type)];

    L_ADD:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] + ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::ADD);
        DISPATCH();
    L_SUB:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] - ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::SUB);
        DISPATCH();
    L_XOR:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] ^ ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::XOR);
        DISPATCH();
    L_OR:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] | ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::OR);
        DISPATCH();
    L_AND:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] & ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::AND);
        DISPATCH();
    L_SLL:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] << (ctx.reg[instruction.rs2] & 0x1f);
        LOG_INST(ctx, EInstruction::SLL);
        DISPATCH();
    L_SRL:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] >> (ctx.reg[instruction.rs2] & 0x1f);
        LOG_INST(ctx, EInstruction::SRL);
        DISPATCH();
    L_SRA: {
        uint8_t shift = static_cast<uint8_t>(ctx.reg[instruction.rs2] & 0x1f);
        ctx.reg[instruction.rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[instruction.rs1]) >> shift);
        LOG_INST(ctx, EInstruction::SRA);
        DISPATCH();
    }
    L_SLT:
        ctx.reg[instruction.rd] =
            static_cast<int32_t>(ctx.reg[instruction.rs1]) < static_cast<int32_t>(ctx.reg[instruction.rs2]);
        LOG_INST(ctx, EInstruction::SLT);
        DISPATCH();
    L_SLTU:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] < ctx.reg[instruction.rs2];
        LOG_INST(ctx, EInstruction::SLTU);
        DISPATCH();
    L_ADDI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] + instruction.immediate;
        LOG_INST(ctx, EInstruction::ADDI);
        DISPATCH();
    L_XORI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] ^ instruction.immediate;
        LOG_INST(ctx, EInstruction::XORI);
        DISPATCH();
    L_ORI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] | instruction.immediate;
        LOG_INST(ctx, EInstruction::ORI);
        DISPATCH();
    L_ANDI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] & instruction.immediate;
        LOG_INST(ctx, EInstruction::ANDI);
        DISPATCH();
    L_SLLI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] << static_cast<uint8_t>(instruction.immediate & 0x1f);
        LOG_INST(ctx, EInstruction::SLLI);
        DISPATCH();
    L_SRLI:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] >> static_cast<uint8_t>(instruction.immediate & 0x1f);
        LOG_INST(ctx, EInstruction::SRLI);
        DISPATCH();
    L_SRAI: {
        uint8_t shift = static_cast<uint8_t>(instruction.immediate & 0x1f);
        ctx.reg[instruction.rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[instruction.rs1]) >> shift);
        LOG_INST(ctx, EInstruction::SRAI);
        DISPATCH();
    }
    L_SLTI:
        ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.reg[instruction.rs1]) < instruction.immediate;
        LOG_INST(ctx, EInstruction::SLTI);
        DISPATCH();
    L_SLTIU:
        ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] < static_cast<uint32_t>(instruction.immediate);
        LOG_INST(ctx, EInstruction::SLTIU);
        DISPATCH();
    L_LB: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
        LOG_INST(ctx, EInstruction::LB);
        DISPATCH();
    }
    L_LH: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.reg[instruction.rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
        LOG_INST(ctx, EInstruction::LH);
        DISPATCH();
    }
    L_LW: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.reg[instruction.rd] = ctx.memory.read<int32_t>(address);
        LOG_INST(ctx, EInstruction::LW);
        DISPATCH();
    }
    L_LBU: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.reg[instruction.rd] = ctx.memory.read<uint8_t>(address);
        LOG_INST(ctx, EInstruction::LBU);
        DISPATCH();
    }
    L_LHU: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.reg[instruction.rd] = ctx.memory.read<uint16_t>(address);
        LOG_INST(ctx, EInstruction::LHU);
        DISPATCH();
    }
    L_SB: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.memory.write<uint8_t>(address, ctx.reg[instruction.rs2]);
        LOG_INST(ctx, EInstruction::SB);
        DISPATCH();
    }
    L_SH: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.memory.write<uint16_t>(address, ctx.reg[instruction.rs2]);
        LOG_INST(ctx, EInstruction::SH);
        DISPATCH();
    }
    L_SW: {
        uint32_t address = ctx.reg[instruction.rs1] + instruction.immediate;
        ctx.memory.write<uint32_t>(address, ctx.reg[instruction.rs2]);
        LOG_INST(ctx, EInstruction::SW);
        DISPATCH();
    }
    L_BEQ:
        LOG_INST(ctx, EInstruction::BEQ);
        if (ctx.reg[instruction.rs1] == ctx.reg[instruction.rs2]) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_BNE:
        LOG_INST(ctx, EInstruction::BNE);
        if (ctx.reg[instruction.rs1] != ctx.reg[instruction.rs2]) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_BLT:
        LOG_INST(ctx, EInstruction::BLT);
        if (static_cast<int32_t>(ctx.reg[instruction.rs1]) < static_cast<int32_t>(ctx.reg[instruction.rs2])) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_BGE:
        LOG_INST(ctx, EInstruction::BGE);
        if (static_cast<int32_t>(ctx.reg[instruction.rs1]) >= static_cast<int32_t>(ctx.reg[instruction.rs2])) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_BLTU:
        LOG_INST(ctx, EInstruction::BLTU);
        if (ctx.reg[instruction.rs1] < ctx.reg[instruction.rs2]) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_BGEU:
        LOG_INST(ctx, EInstruction::BGEU);
        if (ctx.reg[instruction.rs1] >= ctx.reg[instruction.rs2]) {
            ctx.pc += instruction.immediate - 4;
        }
        DISPATCH();
    L_JAL:
        LOG_INST(ctx, EInstruction::JAL);
        ctx.reg[instruction.rd] = ctx.pc + 4;
        ctx.pc += instruction.immediate - 4;
        DISPATCH();
    L_JALR:
        LOG_JMP(ctx, instruction.immediate + ctx.reg[instruction.rs1]);
        LOG_INST(ctx, EInstruction::JALR);
        ctx.reg[instruction.rd] = ctx.pc + 4;
        ctx.pc = instruction.immediate + ctx.reg[instruction.rs1] - 4;
        DISPATCH();
    L_LUI:
        ctx.reg[instruction.rd] = static_cast<uint32_t>(instruction.immediate) << 12;
        LOG_INST(ctx, EInstruction::LUI);
        DISPATCH();
    L_AUIPC:
        ctx.reg[instruction.rd] = ctx.pc + (static_cast<uint32_t>(instruction.immediate) << 12);
        LOG_INST(ctx, EInstruction::AUIPC);
        DISPATCH();
    L_ECALL:
        LOG_INST(ctx, EInstruction::ECALL);
        Syscall::handle(ctx);
        DISPATCH();
    L_MUL:
        ctx.reg[instruction.rd] =
            static_cast<uint64_t>(ctx.reg[instruction.rs1]) * static_cast<uint64_t>(ctx.reg[instruction.rs2]) &
            0xFFFFFFFF;
        LOG_INST(ctx, EInstruction::MUL);
        DISPATCH();
    L_MULH:
        ctx.reg[instruction.rd] =
            (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs1])) *
             static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs2]))) >>
            32;
        LOG_INST(ctx, EInstruction::MULH);
        DISPATCH();
    L_MULHSU:
        ctx.reg[instruction.rd] =
            (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[instruction.rs1])) *
             static_cast<uint64_t>(ctx.reg[instruction.rs2])) >>
            32;
        LOG_INST(ctx, EInstruction::MULHSU);
        DISPATCH();
    L_MULHU:
        ctx.reg[instruction.rd] =
            (static_cast<uint64_t>(ctx.reg[instruction.rs1]) * static_cast<uint64_t>(ctx.reg[instruction.rs2])) >>
            32;
        LOG_INST(ctx, EInstruction::MULHU);
        DISPATCH();
    L_DIV:
        if (ctx.reg[instruction.rs2] == 0) {
            ctx.reg[instruction.rd] = -1;
        } else if (ctx.reg[instruction.rs1] == 0x80000000 && ctx.reg[instruction.rs2] == 0xFFFFFFFF) {
            ctx.reg[instruction.rd] = 0x80000000;
        } else {
            ctx.reg[instruction.rd] = static_cast<uint32_t>(
                static_cast<int32_t>(ctx.reg[instruction.rs1]) / static_cast<int32_t>(ctx.reg[instruction.rs2]));
        }
        LOG_INST(ctx, EInstruction::DIV);
        DISPATCH();
    L_DIVU:
        if (ctx.reg[instruction.rs2] == 0) {
            ctx.reg[instruction.rd] = 0xFFFFFFFF;
        } else {
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] / ctx.reg[instruction.rs2];
        }
        LOG_INST(ctx, EInstruction::DIVU);
        DISPATCH();
    L_REM:
        if (ctx.reg[instruction.rs2] == 0) {
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1];
        } else if (ctx.reg[instruction.rs1] == 0x80000000 && ctx.reg[instruction.rs2] == 0xFFFFFFFF) {
            ctx.reg[instruction.rd] = 0;
        } else {
            ctx.reg[instruction.rd] = static_cast<uint32_t>(
                static_cast<int32_t>(ctx.reg[instruction.rs1]) % static_cast<int32_t>(ctx.reg[instruction.rs2]));
        }
        LOG_INST(ctx, EInstruction::REM);
        DISPATCH();
    L_REMU:
        if (ctx.reg[instruction.rs2] == 0) {
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1];
        } else {
            ctx.reg[instruction.rd] = ctx.reg[instruction.rs1] % ctx.reg[instruction.rs2];
        }
        LOG_INST(ctx, EInstruction::REMU);
        DISPATCH();
    L_LR_W:
        ctx.reg[instruction.rd] = ctx.memory.read<int32_t>(ctx.reg[instruction.rs1]);
        LOG_INST(ctx, EInstruction::LR_W);
        DISPATCH();
    L_SC_W:
        ctx.memory.write<uint32_t>(ctx.reg[instruction.rs1], ctx.reg[instruction.rs2]);
        ctx.reg[instruction.rd] = 0;
        LOG_INST(ctx, EInstruction::SC_W);
        DISPATCH();
    L_INVALID:
        printf("Unsupported instruction at 0x%08x\n", ctx.pc);
        DISPATCH();
}

void Interpreter::logInstruction(Context& ctx, EInstruction type) {
    if (activeProfilingInfo) {
        activeProfilingInfo->incrementInstructionCount(type);
    }
}

void Interpreter::logJump(Context& ctx, uint32_t target) {
    if (activeProfilingInfo) {
        activeProfilingInfo->recordIndirectBranch(ctx.pc, target);
    }
}
