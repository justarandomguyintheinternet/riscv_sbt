#ifndef RISCV_TOOLS_OPTIONS_H
#define RISCV_TOOLS_OPTIONS_H

#include <vector>

namespace Options {
    struct TranslationOptions {
        bool profileIndirect = false; // Collect data on overhead of indirect branch handling, for now not compatible with translation chaining
        bool translationChaining = true;
        bool useProfilingData = false; // Use profiling data to supplement jump target identification
        bool softwareBranchPrediction = false; // Requires presence of profiling data
        bool pinRegisters = false; // Pin most frequently used guest registers to host ones
    };

    // TODO: maybe use some actual argparse type lib
    std::vector<char*> parseArguments(int argc, char** argv, TranslationOptions& options);

    void printHelp(char** argv);
}

#endif //RISCV_TOOLS_OPTIONS_H
