#include "Options.h"

#include <iostream>
#include <string_view>

std::vector<char*> Options::parseArguments(int argc, char** argv, TranslationOptions& options) {
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
        } else if (arg == "--no-function-splitting") {
            options.functionSplitting = false;
        } else {
            positional.push_back(argv[i]);
        }
    }

    return positional;
}

void Options::printHelp(char** argv) {
    std::cerr << "Usage: " << argv[0] << " <elf binary> [output directory] [options]\n"
               << "Options:\n"
               << "  --no-function-splitting       Lift the whole binary into one host function instead of one per function symbol (default: split)\n"
               << "  --no-translation-chaining     No effect since indirect jumps resolve inside their own function (kept for compatibility)\n"
               << "  --profile-indirect            Collect overhead data for indirect branch handling, requires --no-translation-chaining (default: disabled)\n"
               << "  --software-branch-prediction  Hardcode most frequent indirect branch targets using profiling data (default: disabled)\n"
               << "  --use-profiling-data          Use profiling data to supplement jump target identification (default: disabled)\n"
               << "  --pin-registers               Pins most frequently used guest registers to host ones (default: disabled)\n";
}
