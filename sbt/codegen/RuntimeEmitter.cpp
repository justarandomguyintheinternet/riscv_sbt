#include "codegen/RuntimeEmitter.h"

#include <format>
#include <iostream>

// Includes and global definitions
void RuntimeEmitter::emitPrologue() {
    emitter.emitTemplate("output_prologue");

    if (emitter.getOptions().profileIndirect && !emitter.getOptions().translationChaining) {
        emitter.emitTemplate("profiling_globals");
    }

    emitter.emitPinnedRegisters();
}

// Dispatch table containing source PC to label mapping, inside runTranslated()
void RuntimeEmitter::emitDispatchTable() {
    const uint32_t wordCount = binary.getTextWordCount(); // One dispatch slot per instruction
    const uint32_t textStartAddress = binary.getTextStartAddress();

    emitter.emitFilledTemplate("run_translated_open", {
        {"TEXT_SIZE", std::format("{}", wordCount)},
    });

    // build dispatch table https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
    for (uint32_t i = 0; i < wordCount; i++) {
        uint32_t currentAddress = textStartAddress + (i * 4);

        if (leaders.contains(currentAddress)) {
            emitter.emit(std::format("\t\t&&L{:X},\n", currentAddress));
        } else {
            emitter.emit("\t\t&&INVALID,\n");
        }
    }

    emitter.emitTemplate("dispatch_table_close");
}

// Fetch next target, jump to it
void RuntimeEmitter::emitDispatchLoopPrologue() {
    emitter.setIndent(1);
    emitter.emitRegisterLoad(); // pull in the initial register state

    // BASE_RA is checked to stop execution on baremetal, if no exit syscall is used
    emitter.emitFilledTemplate("dispatch_loop_prologue", {
        {"TEXT_START_ADDR", std::format("0x{:X}", binary.getTextStartAddress())},
        {"BASE_RA", std::format("0x{:X}", BASE_RA)},
    });

    if (emitter.getOptions().profileIndirect && !emitter.getOptions().translationChaining) {
        emitter.emitTemplate("indirect_profiling_check");
    }

    emitter.emit("\tgoto *target;\n\n");
}

// Last label of runTranslated(), handling any jumps to instructions not identified as basic block leaders
void RuntimeEmitter::emitInterpreterFallback() {
    emitter.setIndent(2);
    emitter.emit("INVALID: {\n");

    emitter.setIndent(3);
    emitter.emitRegisterStore(); //  must run before anything else does in here

    emitter.emitFilledTemplate("interpreter_fallback", {
        {"TEXT_START_ADDR", std::format("0x{:X}", binary.getTextStartAddress())},
        {"BASE_RA", std::format("0x{:X}", BASE_RA)},
    });

    emitter.emitRegisterLoad();
    emitter.setIndent(0);
    emitter.emit("}}}\n"); // Close fallback, dispatch loop and runTranslated
}

// Main function responsible for memory loading and calling runTranslated()
void RuntimeEmitter::emitGeneratedMain(const std::vector<Instruction>& instructions) {
    emitter.emitFilledTemplate("main_open", {
        {"ENTRY_ADDRESS", std::format("0x{:X}", binary.getEntryAddress())},
    });

    // not present for binaries compiled for baremetal, without picolibc
    bool hasStartup = binary.getSymbolAddress("_start").has_value();

    if (!hasStartup) {
        std::cout << "No _start symbol found, using default init" << std::endl;
        emitter.emitFilledTemplate("no_startup_init", {
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
