#include "Options.h"
#include "analysis/BasicBlocks.h"
#include "codegen/Emitter.h"
#include "codegen/InstructionEmitter.h"
#include "codegen/RuntimeEmitter.h"

#include <ProfilingInfo.h>
#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>
#include <runtime/registers.h>

#include <array>
#include <iostream>
#include <set>
#include <vector>

constexpr std::array<RegisterMapping, 8> x86RegisterMap = {{
    {ra, "r8"}, {sp, "r9"}, {a5, "r10"}, {a4, "r11"}, {a0, "r12"}, {s0, "r13"}, {a3, "r14"}, {a2, "r15"}
}};

int main(int argc, char** argv) {
    Options::TranslationOptions options;
    std::vector<char*> positional = Options::parseArguments(argc, argv, options);

    if (positional.empty()) {
        Options::printHelp(argv);
        return 1;
    }

    ElfBinary binary(positional[0]);

    if (binary.load() != ElfBinary::Success) {
        std::cerr << "Failed to load elf binary" << std::endl;
        return 1;
    }

    std::vector<Instruction> instructions;
    binary.decodeToContainer(instructions);

    ProfilingInfo profilingInfo("./profiling.json", true);

    // Try to recover as many jump targets as possible
    std::set<uint32_t> leaders = BasicBlocks::getLeaders(instructions, options, profilingInfo);
    BasicBlocks::harvestStaticData(binary, leaders);

    ////////////////////////////////////
    // Translated code emission start //
    ////////////////////////////////////

    Emitter emitter(positional.size() < 2 ? "./sbt/translated/src.cpp" : positional[1], options, x86RegisterMap);
    RuntimeEmitter runtimeEmitter(emitter, binary, leaders);
    InstructionEmitter instructionEmitter(emitter, profilingInfo, leaders, binary.getTextStartAddress());

    runtimeEmitter.emitPrologue();
    runtimeEmitter.emitDispatchTable();
    runtimeEmitter.emitDispatchLoopPrologue();

    // todo: peephole pass over `instructions` before emission, collapsing idiom sequences into simpler C
    for (const auto& inst : instructions) {
        instructionEmitter.emitInstruction(inst);
    }

    runtimeEmitter.emitInterpreterFallback();
    runtimeEmitter.emitGeneratedMain(instructions);

    return 0;
}
