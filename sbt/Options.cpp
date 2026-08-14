#include "Options.h"

#include <iostream>
#include <string_view>

std::vector<char*> Options::parseArguments(int argc, char** argv, TranslationOptions& options) {
    std::vector<char*> positional;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--translation-chaining") {
            options.translationChaining = true;
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

void Options::printHelp(char** argv) {
    std::cerr << "Usage: " << argv[0] << " <elf binary> [output directory] [options]\n"
               << "Options:\n"
               << "  --translation-chaining        Enable translation chaining between indirect jumps (default: disabled)\n"
               << "  --profile-indirect            Collect overhead data for indirect branch handling, requires --translation-chaining (default: disabled)\n"
               << "  --software-branch-prediction  Hardcode most frequent indirect branch targets using profiling data (default: disabled)\n"
               << "  --use-profiling-data          Use profiling data to supplement jump target identification (default: disabled)\n"
               << "  --pin-registers               Pins most frequently used guest registers to host ones (default: enabled)\n";
}
