#include "elf/ElfBinary.h"
#include <iostream>
#include <fstream>
#include <bitset>
#include <format>
#include <set>

#include "runtime/registers.h"
#include <ProfilingInfo.h>
#include <ranges>
#include <algorithm>

struct TranslationOptions {
    bool profileIndirect = false; // Collect data on overhead of indirect branch handling, for now not compatible with translation chaining
    bool translationChaining = true;
    bool useProfilingData = false; // Use profiling data to supplement jump target identification
    bool softwareBranchPrediction = false; // Requires presence of profiling data
    bool pinRegisters = false; // Pin most frequently used guest registers to host ones
};

TranslationOptions options;

inline constexpr std::array<std::pair<int, std::string_view>, 8> x86RegisterMap = {{
    {ra, "r8"}, {sp, "r9"}, {a5, "r10"}, {a4, "r11"}, {a0, "r12"}, {s0, "r13"}, {a3, "r14"}, {a2, "r15"}
}};

constexpr std::optional<std::string_view> getHostRegister(int sourceRegister) {
    for (auto& [s, h] : x86RegisterMap) {
        if (s == sourceRegister) return h;
    }

    return {};
}

std::vector<Instruction> instructions;
const Instruction* current;
uint8_t indent = 0;
std::ofstream output;
ProfilingInfo profilingInfo("./profiling.json", true);

uint32_t reg_values[32];
bool reg_known[32];

#define BASE_RA 0xdeadbeef // Used for baremetal, not compatible with translation chaining

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

    if (field != InstructionField::RD && reg_known[index]) {
        return std::format("{}", reg_values[index]);
    }

    auto mapped = getHostRegister(index);
    if (options.pinRegisters && mapped.has_value()) {
        return std::format("x{}", index);
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

using TemplateValues = std::initializer_list<std::pair<std::string_view, std::string>>;

std::string loadTemplate(const std::string& name) {
    std::string path = std::format("./sbt/templates/{}.cpp.in", name);
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open template: " << path << std::endl;
        std::exit(1);
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// Placeholders use %%KEY%%
std::string fillTemplate(std::string tmpl, TemplateValues values) {
    for (const auto& [key, value] : values) {
        std::string token = std::format("%%{}%%", key);
        for (size_t pos; (pos = tmpl.find(token)) != std::string::npos;) {
            tmpl.replace(pos, token.size(), value);
        }
    }

    return tmpl;
}

void emitTemplate(const std::string& name) {
    emit(loadTemplate(name));
}

void emitFilledTemplate(const std::string& name, TemplateValues values) {
    emit(fillTemplate(loadTemplate(name), values));
}

void emitLoadSaveAddress(const Instruction& instruction) {
    if (instruction.immediate == 0) {
        emit(std::format("address = {};\n", REG(RS1)));
    } else {
        emit(std::format("address = {} + {};\n", REG(RS1), instruction.immediate));
    }
}

void emitPinnedRegisters() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("register uint32_t x{} asm (\"{}\");\n", s, h));
    }
}

void emitRegisterStore() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("ctx.reg[{}] = x{};\n", s, s));
    }
}

void emitRegisterLoad() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("x{} = ctx.reg[{}];\n", s, s));
    }
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

void emitInstruction(const Instruction& instruction, std::set<uint32_t>& leaders, uint32_t textStartAddress) {
    indent = 2;
    if (leaders.contains(instruction.address)) {
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
            if (options.profileIndirect && !options.translationChaining) {
                emit("timerActive = true; timer = __rdtsc();\n");
            }

            if (options.softwareBranchPrediction) {
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

            if (options.translationChaining) {
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
            emitRegisterStore();
            if (!reg_known[a7]) {
                printf("a7 not tracked, 0x%x\n", instruction.address);
                emit(std::format("Syscall::handle(ctx);\n"));
            } else {
                emit(std::format("Syscall::handle(ctx, {});\n", reg_values[a7]));
            }
            emitRegisterLoad();
            reg_known[a0] = false; // syscall return values gets written into a0
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

    if (options.useProfilingData) {
        auto branchTargets = profilingInfo.getAllBranchTargets();

        for (const auto& [address, targets] : branchTargets) {
            for (const auto& target : targets) {
                leaders.insert(target.first);
            }
        }
    }

    return leaders;
}

void harvestStaticData(ElfBinary& binary, std::set<uint32_t>& leaders, uint32_t textStart, uint32_t textEnd) {
    auto sections = binary.getTypeSections(ElfBinarySection::Data);

    uint32_t discovered = 0;

    for (auto section : sections) {
        for (auto address : section.get().getData()) {
            if (address >= textStart && address < textEnd) {
                discovered += !leaders.contains(address);
                leaders.insert(address);
            }
        }
    }

    printf("Discovered %d potential jump targets from data sections\n", discovered);
}

std::vector<char*> parseArguments(int argc, char** argv) {
    std::vector<char*> positional;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--no-translation-chaining") {
            options.translationChaining = false;
        } else if (arg == "--profile-indirect") {
            options.profileIndirect = true;
        } else if (arg == "--software-branch-prediction") {
            options.softwareBranchPrediction = true;
        } else if (arg == "--use-profiling-data") {
            options.useProfilingData = true;
        } else if (arg == "--pin-registers") {
            options.pinRegisters = true;
        } else {
            positional.push_back(argv[i]);
        }
    }

    return positional;
}

int main(int argc, char** argv) {
    std::vector<char*> positional = parseArguments(argc, argv);

    if (positional.empty()) {
        std::cerr << "Usage: " << argv[0] << " <elf binary> [output directory] [options]\n"
                   << "Options:\n"
                   << "  --no-translation-chaining     Disable translation chaining between indirect jumps (default: enabled)\n"
                   << "  --profile-indirect            Collect overhead data for indirect branch handling, requires --no-translation-chaining (default: disabled)\n"
                   << "  --software-branch-prediction  Hardcode most frequent indirect branch targets using profiling data (default: disabled)\n"
                   << "  --use-profiling-data          Use profiling data to supplement jump target identification (default: disabled)\n"
                   << "  --pin-registers               Pins most frequently used guest registers to host ones (default: disabled)\n";
        return 1;
    }

    resetTracked();

    ElfBinary binary(positional[0]);

    if (binary.load() != ElfBinary::Success) {
        std::cerr << "Failed to load elf binary" << std::endl;
        return 1;
    }

    binary.decodeToContainer(instructions);

    // Calculate instructions address bounds
    auto text = binary.getTypeSections(ElfBinarySection::Text);
    uint32_t textEndAddress = 0;
    uint32_t textStartAddress = 1 << 31; // Lowest start address among executable sections, used for calculating zero based pc (To make dispatch array more compact)

    for (const auto& ref : text) {
        textStartAddress = std::min(textStartAddress, ref.get().getStartAddress());
        textEndAddress = std::max(textEndAddress, ref.get().getStartAddress() + ref.get().getSize());
    }

    uint32_t textSize = textEndAddress - textStartAddress; // Sum of the size of all sections which are executable, including potential gaps in between

    // Try to recover as many jump targets as possible
    std::set<uint32_t> leaders = getBasicBlocksLeaders(instructions);
    harvestStaticData(binary, leaders, textStartAddress, textEndAddress);

    ////////////////////////////////////
    // Translated code emission start //
    ////////////////////////////////////

    output.open(positional.size() < 2 ? "./sbt/translated/src.cpp" : positional[1]);
    emitTemplate("output_prologue");

    if (options.profileIndirect && !options.translationChaining) {
        emitTemplate("profiling_globals");
    }

    emitPinnedRegisters();

    emitFilledTemplate("run_translated_open", {
        {"TEXT_SIZE", std::format("{}", textSize)},
    });

    // build dispatch table https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
    for (uint32_t i = 0; i < textSize; i++) {
        uint32_t currentAddress = textStartAddress + (i * 4);

        if (leaders.contains(currentAddress)) {
            emit(std::format("\t\t&&L{:X},\n", currentAddress));
        } else {
            emit("\t\t&&INVALID,\n");
        }
    }

    emitTemplate("dispatch_table_close");
    indent = 1;
    emitRegisterLoad(); // pull in the initial register state

    // BASE_RA is checked to stop execution on baremetal, if no exit syscall is used
    emitFilledTemplate("dispatch_loop_prologue", {
        {"TEXT_START_ADDR", std::format("0x{:X}", textStartAddress)},
        {"BASE_RA", std::format("0x{:X}", BASE_RA)},
    });

    if (options.profileIndirect && !options.translationChaining) {
        emitTemplate("indirect_profiling_check");
    }

    emit("\tgoto *target;\n\n");

    // todo: peephole pass over `instructions` before emission, collapsing idiom sequences into simpler C
    for (const auto& inst : instructions) {
        emitInstruction(inst, leaders, textStartAddress);
    }

    // Emulation fallback
    emit("INVALID: {\n");
    indent = 3;
    emitRegisterStore(); //  must run before anything else does in here

    emitFilledTemplate("interpreter_fallback", {
        {"TEXT_START_ADDR", std::format("0x{:X}", textStartAddress)},
        {"BASE_RA", std::format("0x{:X}", BASE_RA)},
    });

    emitRegisterLoad();
    indent = 0;
    emit("}}}\n"); // Close fallback, dispatch loop and run_translated

    // Dispatch table and memory loading

    emitFilledTemplate("main_open", {
        {"ENTRY_ADDRESS", std::format("0x{:X}", binary.getEntryAddress())},
    });

    // not present for binaries compiled for baremetal, without picolibc
    bool hasStartup = binary.getSymbolAddress("_start").has_value();

    if (!hasStartup) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        emitFilledTemplate("no_startup_init", {
            {"BASE_RA", std::format("0x{:X}", BASE_RA)},
            {"GLOBAL_POINTER", std::format("0x{:X}", binary.getSymbolAddress("__global_pointer$").value_or(0))},
        });
    }

    // load static data
    uint32_t dataEnd = 0;
    for (const auto& ref : binary.getTypeSections(ElfBinarySection::SectionType::Data)) {
        const auto& dataSection = ref.get();
        uint32_t dataAddr = dataSection.getLoadAddress(); // Use load address, not virtual one, for when crt0 copies data into to the virtual address

        for (auto word : dataSection.getData()) {
            emit(std::format("\tctx.memory.write<uint32_t>(0x{:X}, 0x{:X});\n", dataAddr, word), word != 0);
            dataAddr += 4;
        }

        if (dataAddr > dataEnd) {
            dataEnd = dataAddr;
        }
    }

    emit(std::format("\tmemory.initializeHeap(0x{:X});\n\n", dataEnd));

    // load instructions for fallback emulation
    for (const auto& instruction : instructions) {
        emit(std::format("\tctx.memory.write<uint32_t>(0x{:X}, 0x{:X});\n", instruction.address, instruction.instruction));
    }

    indent = 0;
    emitTemplate("main_close");

    output.close();

    return 0;
}