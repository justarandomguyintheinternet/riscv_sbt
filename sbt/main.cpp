#include "Options.h"
#include "analysis/BasicBlocks.h"
#include "analysis/Functions.h"
#include "codegen/Emitter.h"
#include "codegen/FunctionEmitter.h"
#include "codegen/InstructionEmitter.h"
#include "codegen/RuntimeEmitter.h"

#include <ProfilingInfo.h>
#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>
#include <runtime/registers.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <span>
#include <vector>

constexpr std::array<RegisterMapping, 8> x86RegisterMap = {{
    {ra, "r8"}, {sp, "r9"}, {a5, "r10"}, {a4, "r11"}, {a0, "r12"}, {s0, "r13"}, {a3, "r14"}, {a2, "r15"}
}};

// Instructions covering [start, end), the container is sorted by address
static std::span<const Instruction> sliceOf(const std::vector<Instruction>& instructions, const LiftedFunction& function) {
    const auto byAddress = [](const Instruction& instruction, uint32_t address) { return instruction.address < address; };

    const auto begin = std::lower_bound(instructions.begin(), instructions.end(), function.start, byAddress);
    const auto end = std::lower_bound(begin, instructions.end(), function.end, byAddress);

    return std::span<const Instruction>(begin, end);
}

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

    // Sections do not have to be listed in address order, but the function slices assume they are
    std::sort(instructions.begin(), instructions.end(),
              [](const Instruction& a, const Instruction& b) { return a.address < b.address; });

    ProfilingInfo profilingInfo("./profiling.json", true);

    // Try to recover as many jump targets as possible
    std::set<uint32_t> leaders = BasicBlocks::getLeaders(instructions, options, profilingInfo);
    BasicBlocks::harvestStaticData(binary, leaders);

    // Partition the recovered blocks into the host functions they get lifted into
    FunctionMap functionMap = FunctionMap::build(binary, instructions, options);
    functionMap.printSummary();

    ////////////////////////////////////
    // Translated code emission start //
    ////////////////////////////////////

    Emitter emitter(positional.size() < 2 ? "./sbt/translated/src.cpp" : positional[1], options, x86RegisterMap);
    RuntimeEmitter runtimeEmitter(emitter, binary, functionMap, leaders);
    FunctionEmitter functionEmitter(emitter, functionMap, leaders);
    InstructionEmitter instructionEmitter(emitter, profilingInfo, functionMap, leaders, binary.getTextStartAddress());

    runtimeEmitter.emitPrologue();
    functionEmitter.emitForwardDeclarations();
    runtimeEmitter.emitFunctionTable();

    // todo: peephole pass over the instructions of a function before emission, collapsing idiom sequences into simpler C
    for (const auto& function : functionMap.all()) {
        const std::span<const Instruction> body = sliceOf(instructions, function);

        instructionEmitter.beginFunction(function, body);
        functionEmitter.emitFunctionOpen(function);

        for (const auto& instruction : body) {
            instructionEmitter.emitInstruction(instruction);
        }

        functionEmitter.emitFunctionClose(function, body.empty() ? nullptr : &body.back());
    }

    runtimeEmitter.emitTopLevelDispatcher();
    runtimeEmitter.emitGeneratedMain(instructions);

    return 0;
}
