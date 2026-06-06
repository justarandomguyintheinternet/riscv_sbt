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
        LOG_INST(ctx.pc, "addi");
    }
    // xori
    else if (op == 0b0010011 && funct3 == 0x4) {
        ctx.reg[rd] = ctx.reg[rs1] ^ Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx.pc, "xori");
    }
    // ori
    else if (op == 0b0010011 && funct3 == 0x6) {
        ctx.reg[rd] = ctx.reg[rs1] | Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx.pc, "ori");
    }
    // andi
    else if (op == 0b0010011 && funct3 == 0x7) {
        ctx.reg[rd] = ctx.reg[rs1] & Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx.pc, "andi");
    }
    // slli
    else if (op == 0b0010011 && funct3 == 0x1 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << Decoder::getShift(instruction);
        LOG_INST(ctx.pc, "slli");
    }
    // srli
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> Decoder::getShift(instruction);
        LOG_INST(ctx.pc, "srli");
    }
    // srai
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x20) {
        uint8_t shift = Decoder::getShift(instruction);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx.pc, "srai");
    }
    // slti
    else if (op == 0b0010011 && funct3 == 0x2) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx.pc, "slti");
    }
    // sltiu
    else if (op == 0b0010011 && funct3 == 0x3) {
        ctx.reg[rd] = ctx.reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
        LOG_INST(ctx.pc, "sltiu");
    }

    // add
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] + ctx.reg[rs2];
        LOG_INST(ctx.pc, "add");
    }
    // sub
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x20) {
        ctx.reg[rd] = ctx.reg[rs1] - ctx.reg[rs2];
        LOG_INST(ctx.pc, "sub");
    }
    // xor
    else if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] ^ ctx.reg[rs2];
        LOG_INST(ctx.pc, "xor");
    }
    // or
    else if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] | ctx.reg[rs2];
        LOG_INST(ctx.pc, "or");
    }
    // and
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] & ctx.reg[rs2];
        LOG_INST(ctx.pc, "and");
    }
    // sll
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx.pc, "sll");
    }
    // srl
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx.pc, "srl");
    }
    // sra
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x20) {
        uint8_t shift = static_cast<uint8_t>(ctx.reg[rs2] & 0x1f);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx.pc, "sra");
    }
    // slt
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x00) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2]);
        LOG_INST(ctx.pc, "slt");
    }
    // sltu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] < ctx.reg[rs2];
        LOG_INST(ctx.pc, "sltu");
    }
    // lb
    else if (op == 0b0000011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
        LOG_INST(ctx.pc, "lb");
    }
    // lbu
    else if (op == 0b0000011 && funct3 == 0x4) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint8_t>(address);
        LOG_INST(ctx.pc, "lbu");
    }
    // lh
    else if (op == 0b0000011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
        LOG_INST(ctx.pc, "lh");
    }
    // lhu
    else if (op == 0b0000011 && funct3 == 0x5) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint16_t>(address);
        LOG_INST(ctx.pc, "lhu");
    }
    // lw
    else if (op == 0b0000011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<int32_t>(address);
        LOG_INST(ctx.pc, "lw");
    }

    // sb
    else if (op == 0b0100011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint8_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx.pc, "sb");
    }
    // sh
    else if (op == 0b0100011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint16_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx.pc, "sh");
    }
    // sw
    else if (op == 0b0100011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint32_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx.pc, "sw");
    }

    // beq
    else if (op == 0b1100011 && funct3 == 0x0) {
        LOG_INST(ctx.pc, "beq");
        if (ctx.reg[rs1] == ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bne
    else if (op == 0b1100011 && funct3 == 0x1) {
        LOG_INST(ctx.pc, "bne");
        if (ctx.reg[rs1] != ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // blt
    else if (op == 0b1100011 && funct3 == 0x4) {
        LOG_INST(ctx.pc, "blt");
        if (static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bge
    else if (op == 0b1100011 && funct3 == 0x5) {
        LOG_INST(ctx.pc, "bge");
        if (static_cast<int32_t>(ctx.reg[rs1]) >= static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bltu
    else if (op == 0b1100011 && funct3 == 0x6) {
        LOG_INST(ctx.pc, "bltu");
        if (ctx.reg[rs1] < ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bgeu
    else if (op == 0b1100011 && funct3 == 0x7) {
        LOG_INST(ctx.pc, "bgeu");
        if (ctx.reg[rs1] >= ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }

    // jal
    else if (op == 0b1101111) {
        LOG_INST(ctx.pc, "jal");
        ctx.reg[rd] = ctx.pc + 4;
        ctx.pc += Decoder::J_FMT_imm(instruction) - 4;
    }
    // jalr
    else if (op == 0b1100111 && funct3 == 0x0) {
        LOG_INST(ctx.pc, "jalr");
        ctx.reg[rd] = ctx.pc + 4;

        ctx.pc = Decoder::I_FMT_imm(instruction) + ctx.reg[rs1] - 4;
    }

    // lui
    else if (op == 0b0110111) {
        ctx.reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
        LOG_INST(ctx.pc, "lui");
    }
    // auipc
    else if (op == 0b0010111) {
        ctx.reg[rd] = ctx.pc + (Decoder::U_FMT_imm(instruction) << 12);
        LOG_INST(ctx.pc, "auipc");
    }

    // ecall
    else if (op == 0b1110011 && Decoder::I_FMT_imm(instruction) == 0x0) {
        LOG_INST(ctx.pc, "ecall");
        Syscall::handle(ctx);
    }

    // RV32M

    // mul
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x01) {
        ctx.reg[rd] = static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2]) & 0xFFFFFFFF;
        LOG_INST(ctx.pc, "mul");
    }
    // mulh
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs2]))) >> 32;
        LOG_INST(ctx.pc, "mulh");
    }
    // mulhsu
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx.pc, "mulhsu");
    }
    // mulhu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx.pc, "mulhu");
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
        LOG_INST(ctx.pc, "div");
    }
    // divu
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = 0xFFFFFFFF;
        } else {
            ctx.reg[rd] = ctx.reg[rs1] / ctx.reg[rs2];
        }
        LOG_INST(ctx.pc, "divu");
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
        LOG_INST(ctx.pc, "rem");
    }
    // remu
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = ctx.reg[rs1];
        } else {
            ctx.reg[rd] = ctx.reg[rs1] % ctx.reg[rs2];
        }
        LOG_INST(ctx.pc, "remu");
    } else {
        printf("Unsupported instruction at 0x%08x\n", ctx.pc);
    }

    ctx.pc += 4; // this might be dumb for SBT, idk
    ctx.reg[x0] = 0;
}

void Interpreter::logInstruction(uint32_t address, const char* name) {
    printf("0x%08x: %s\n", address, name);
}