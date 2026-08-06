#include "codegen/RuntimeEmitter.h"

#include <format>
#include <iostream>

// Includes and global definitions
void RuntimeEmitter::emitPrologue() {
    emitter.emitTemplate("output_prologue");

    if (emitter.getOptions().profileIndirect && !functionMap.isSplit()) {
        emitter.emitTemplate("profiling_globals");
    }

    emitter.emitPinnedRegisters();
    emitter.emitTemplate("function_table_open");
}

// Source PC to lifted function mapping, plus the helpers resolving against it
void RuntimeEmitter::emitFunctionTable() {
    const uint32_t textStartAddress = binary.getTextStartAddress();

    emitter.setIndent(0);
    emitter.emit(std::format("static LiftedFn functionTable[{}] = {{\n", getTableSize()));

    for (uint32_t i = 0; i < binary.getTextWordCount(); i++) {
        const uint32_t currentAddress = textStartAddress + (i * 4);

        // Split, only the first block of a function can be entered from outside, unsplit every block can
        const bool enterable = functionMap.isSplit() ? functionMap.isEntry(currentAddress) : leaders.contains(currentAddress);

        if (enterable && functionMap.lookup(currentAddress) != nullptr) {
            emitter.emit(std::format("\t&{},\n", functionMap.lookup(currentAddress)->name));
        } else {
            emitter.emit("\tnullptr,\n");
        }
    }

    // The slot for getBaseRa(), which terminates the dispatch loop rather than resolving to anything
    emitter.emit("\tnullptr,\n");

    emitter.emitFilledTemplate("function_table_close", {
        {"TEXT_START_ADDR", std::format("0x{:X}", textStartAddress)},
        {"TEXT_SIZE", std::format("{}", getTableSize())},
    });
}

// Enters lifted functions until one of them ends up at the base return address
void RuntimeEmitter::emitTopLevelDispatcher() {
    emitter.setIndent(0);
    emitter.emitTemplate("dispatcher_open");

    emitter.setIndent(1);
    emitter.emitRegisterLoad(); // pull in the initial register state

    emitter.setIndent(0);
    emitter.emitFilledTemplate("dispatcher_loop", {{"BASE_RA", std::format("0x{:X}", getBaseRa())}});

    emitter.setIndent(2);
    emitter.emitRegisterStore(); // the interpreter works off ctx.reg[]

    emitter.setIndent(0);
    emitter.emitFilledTemplate("dispatcher_fallback", {{"BASE_RA", std::format("0x{:X}", getBaseRa())}});

    emitter.setIndent(2);
    emitter.emitRegisterLoad();

    emitter.setIndent(0);
    emitter.emitTemplate("dispatcher_close");

    emitter.setIndent(1);
    emitter.emitRegisterStore(); // printInfo reads ctx.reg[]

    emitter.setIndent(0);
    emitter.emitTemplate("dispatcher_exit");
}

// Main function responsible for memory loading and calling runTranslated()
void RuntimeEmitter::emitGeneratedMain(const std::vector<Instruction>& instructions) {
    emitter.setIndent(0);
    emitter.emitFilledTemplate("main_open", {
        {"ENTRY_ADDRESS", std::format("0x{:X}", binary.getEntryAddress())},
    });

    // not present for binaries compiled for baremetal, without picolibc
    bool hasStartup = binary.getSymbolAddress("_start").has_value();

    if (!hasStartup) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        emitter.emitFilledTemplate("no_startup_init", {
            {"BASE_RA", std::format("0x{:X}", getBaseRa())},
            {"GLOBAL_POINTER", std::format("0x{:X}", binary.getSymbolAddress("__global_pointer$").value_or(0))},
        });
    }

    // load static data
    uint32_t dataEnd = 0;
    for (const auto& ref : binary.getTypeSections(ElfBinarySection::SectionType::Data)) {
        const auto& dataSection = ref.get();
        uint32_t dataAddr = dataSection.getLoadAddress(); // Use load address, not virtual one, for when crt0 copies data into to the virtual address

        for (auto word : dataSection.getData()) {
            emitter.emitIf(std::format("\tctx.memory.write<uint32_t>(0x{:X}, 0x{:X});\n", dataAddr, word), word != 0);
            dataAddr += 4;
        }

        if (dataAddr > dataEnd) {
            dataEnd = dataAddr;
        }
    }

    emitter.emit(std::format("\tmemory.initializeHeap(0x{:X});\n\n", dataEnd));

    // load instructions for fallback emulation
    for (const auto& instruction : instructions) {
        emitter.emit(std::format("\tctx.memory.write<uint32_t>(0x{:X}, 0x{:X});\n", instruction.address, instruction.instruction));
    }

    emitter.setIndent(0);
    emitter.emitTemplate("main_close");
}
