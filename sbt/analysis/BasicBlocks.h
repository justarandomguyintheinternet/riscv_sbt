#ifndef RISCV_TOOLS_BASICBLOCKS_H
#define RISCV_TOOLS_BASICBLOCKS_H

#include "Options.h"

#include <ProfilingInfo.h>
#include <decoding/Decoder.h>
#include <elf/ElfBinary.h>

#include <set>
#include <vector>

namespace BasicBlocks {
    std::set<uint32_t> getLeaders(const std::vector<Instruction>& instructions, const Options::TranslationOptions& options, const ProfilingInfo& profilingInfo);
    void harvestStaticData(const ElfBinary& binary, std::set<uint32_t>& leaders);
}

#endif //RISCV_TOOLS_BASICBLOCKS_H
