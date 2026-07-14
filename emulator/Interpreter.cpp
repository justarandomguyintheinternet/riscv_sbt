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
        LOG_INST(ctx, "addi");
    }
    // xori
    else if (op == 0b0010011 && funct3 == 0x4) {
        ctx.reg[rd] = ctx.reg[rs1] ^ Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, "xori");
    }
    // ori
    else if (op == 0b0010011 && funct3 == 0x6) {
        ctx.reg[rd] = ctx.reg[rs1] | Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, "ori");
    }
    // andi
    else if (op == 0b0010011 && funct3 == 0x7) {
        ctx.reg[rd] = ctx.reg[rs1] & Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, "andi");
    }
    // slli
    else if (op == 0b0010011 && funct3 == 0x1 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << Decoder::getShift(instruction);
        LOG_INST(ctx, "slli");
    }
    // srli
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> Decoder::getShift(instruction);
        LOG_INST(ctx, "srli");
    }
    // srai
    else if (op == 0b0010011 && funct3 == 0x5 && Decoder::getSHType(instruction) == 0x20) {
        uint8_t shift = Decoder::getShift(instruction);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx, "srai");
    }
    // slti
    else if (op == 0b0010011 && funct3 == 0x2) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < Decoder::I_FMT_imm(instruction);
        LOG_INST(ctx, "slti");
    }
    // sltiu
    else if (op == 0b0010011 && funct3 == 0x3) {
        ctx.reg[rd] = ctx.reg[rs1] < static_cast<uint32_t>(Decoder::I_FMT_imm(instruction));
        LOG_INST(ctx, "sltiu");
    }

    // add
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] + ctx.reg[rs2];
        LOG_INST(ctx, "add");
    }
    // sub
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x20) {
        ctx.reg[rd] = ctx.reg[rs1] - ctx.reg[rs2];
        LOG_INST(ctx, "sub");
    }
    // xor
    else if (op == 0b0110011 && funct3 == 0x4 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] ^ ctx.reg[rs2];
        LOG_INST(ctx, "xor");
    }
    // or
    else if (op == 0b0110011 && funct3 == 0x6 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] | ctx.reg[rs2];
        LOG_INST(ctx, "or");
    }
    // and
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] & ctx.reg[rs2];
        LOG_INST(ctx, "and");
    }
    // sll
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] << (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx, "sll");
    }
    // srl
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] >> (ctx.reg[rs2] & 0x1f);
        LOG_INST(ctx, "srl");
    }
    // sra
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x20) {
        uint8_t shift = static_cast<uint8_t>(ctx.reg[rs2] & 0x1f);
        ctx.reg[rd] = static_cast<uint32_t>(static_cast<int32_t>(ctx.reg[rs1]) >> shift);
        LOG_INST(ctx, "sra");
    }
    // slt
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x00) {
        ctx.reg[rd] = static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2]);
        LOG_INST(ctx, "slt");
    }
    // sltu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x00) {
        ctx.reg[rd] = ctx.reg[rs1] < ctx.reg[rs2];
        LOG_INST(ctx, "sltu");
    }
    // lb
    else if (op == 0b0000011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int8_t>(address));
        LOG_INST(ctx, "lb");
    }
    // lbu
    else if (op == 0b0000011 && funct3 == 0x4) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint8_t>(address);
        LOG_INST(ctx, "lbu");
    }
    // lh
    else if (op == 0b0000011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = static_cast<int32_t>(ctx.memory.read<int16_t>(address));
        LOG_INST(ctx, "lh");
    }
    // lhu
    else if (op == 0b0000011 && funct3 == 0x5) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<uint16_t>(address);
        LOG_INST(ctx, "lhu");
    }
    // lw
    else if (op == 0b0000011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::I_FMT_imm(instruction);
        ctx.reg[rd] = ctx.memory.read<int32_t>(address);
        LOG_INST(ctx, "lw");
    }

    // sb
    else if (op == 0b0100011 && funct3 == 0x0) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint8_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, "sb");
    }
    // sh
    else if (op == 0b0100011 && funct3 == 0x1) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint16_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, "sh");
    }
    // sw
    else if (op == 0b0100011 && funct3 == 0x2) {
        uint32_t address = ctx.reg[rs1] + Decoder::S_FMT_imm(instruction);
        ctx.memory.write<uint32_t>(address, ctx.reg[rs2]);
        LOG_INST(ctx, "sw");
    }

    // beq
    else if (op == 0b1100011 && funct3 == 0x0) {
        LOG_INST(ctx, "beq");
        if (ctx.reg[rs1] == ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bne
    else if (op == 0b1100011 && funct3 == 0x1) {
        LOG_INST(ctx, "bne");
        if (ctx.reg[rs1] != ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // blt
    else if (op == 0b1100011 && funct3 == 0x4) {
        LOG_INST(ctx, "blt");
        if (static_cast<int32_t>(ctx.reg[rs1]) < static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bge
    else if (op == 0b1100011 && funct3 == 0x5) {
        LOG_INST(ctx, "bge");
        if (static_cast<int32_t>(ctx.reg[rs1]) >= static_cast<int32_t>(ctx.reg[rs2])) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bltu
    else if (op == 0b1100011 && funct3 == 0x6) {
        LOG_INST(ctx, "bltu");
        if (ctx.reg[rs1] < ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }
    // bgeu
    else if (op == 0b1100011 && funct3 == 0x7) {
        LOG_INST(ctx, "bgeu");
        if (ctx.reg[rs1] >= ctx.reg[rs2]) {
            ctx.pc += Decoder::B_FMT_imm(instruction) - 4;
        }
    }

    // jal
    else if (op == 0b1101111) {
        LOG_INST(ctx, "jal");
        ctx.reg[rd] = ctx.pc + 4;
        ctx.pc += Decoder::J_FMT_imm(instruction) - 4;
    }
    // jalr
    else if (op == 0b1100111 && funct3 == 0x0) {
        LOG_JMP(ctx, Decoder::I_FMT_imm(instruction) + ctx.reg[rs1]);
        LOG_INST(ctx, "jalr");

        ctx.reg[rd] = ctx.pc + 4;
        ctx.pc = Decoder::I_FMT_imm(instruction) + ctx.reg[rs1] - 4;
    }

    // lui
    else if (op == 0b0110111) {
        ctx.reg[rd] = Decoder::U_FMT_imm(instruction) << 12;
        LOG_INST(ctx, "lui");
    }
    // auipc
    else if (op == 0b0010111) {
        ctx.reg[rd] = ctx.pc + (Decoder::U_FMT_imm(instruction) << 12);
        LOG_INST(ctx, "auipc");
    }

    // ecall
    else if (op == 0b1110011 && Decoder::I_FMT_imm(instruction) == 0x0) {
        LOG_INST(ctx, "ecall");
        Syscall::handle(ctx);
    }

    // RV32M

    // mul
    else if (op == 0b0110011 && funct3 == 0x0 && funct7 == 0x01) {
        ctx.reg[rd] = static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2]) & 0xFFFFFFFF;
        LOG_INST(ctx, "mul");
    }
    // mulh
    else if (op == 0b0110011 && funct3 == 0x1 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs2]))) >> 32;
        LOG_INST(ctx, "mulh");
    }
    // mulhsu
    else if (op == 0b0110011 && funct3 == 0x2 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<int64_t>(static_cast<int32_t>(ctx.reg[rs1])) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx, "mulhsu");
    }
    // mulhu
    else if (op == 0b0110011 && funct3 == 0x3 && funct7 == 0x01) {
        ctx.reg[rd] = (static_cast<uint64_t>(ctx.reg[rs1]) * static_cast<uint64_t>(ctx.reg[rs2])) >> 32;
        LOG_INST(ctx, "mulhu");
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
        LOG_INST(ctx, "div");
    }
    // divu
    else if (op == 0b0110011 && funct3 == 0x5 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = 0xFFFFFFFF;
        } else {
            ctx.reg[rd] = ctx.reg[rs1] / ctx.reg[rs2];
        }
        LOG_INST(ctx, "divu");
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
        LOG_INST(ctx, "rem");
    }
    // remu
    else if (op == 0b0110011 && funct3 == 0x7 && funct7 == 0x01) {
        if (ctx.reg[rs2] == 0) {
            ctx.reg[rd] = ctx.reg[rs1];
        } else {
            ctx.reg[rd] = ctx.reg[rs1] % ctx.reg[rs2];
        }
        LOG_INST(ctx, "remu");
    // LR_W
    } else if (op == 0b0101111 && funct3 == 0x2 && Decoder::getFunct5(instruction) == 0x02) {
        ctx.reg[rd] = ctx.memory.read<int32_t>(ctx.reg[rs1]);
        LOG_INST(ctx, "lr.w");
    // SC_W
    } else if (op == 0b0101111 && funct3 == 0x2 && Decoder::getFunct5(instruction) == 0x03) {
        ctx.memory.write<uint32_t>(ctx.reg[rs1], ctx.reg[rs2]);
        ctx.reg[rd] = 0; // success
        LOG_INST(ctx, "sc.w");
    } else {
        printf("Unsupported instruction at 0x%08x\n", ctx.pc);
    }

    ctx.pc += 4;
    ctx.reg[x0] = 0;
}

void Interpreter::logInstruction(Context& ctx, std::string_view name) {
    if (activeProfilingInfo) {
        activeProfilingInfo->incrementInstructionCount(name);
    }
}

void Interpreter::logJump(Context& ctx, uint32_t target) {
    if (activeProfilingInfo) {
        activeProfilingInfo->recordIndirectBranch(ctx.pc, target);
    }
}