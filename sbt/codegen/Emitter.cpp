#include "codegen/Emitter.h"

#include <cstdio>
#include <format>
#include <iostream>
#include <iterator>

namespace {
    std::string loadTemplate(const std::string& name) {
        std::string path = std::format("./sbt/templates/{}.cpp.in", name);
        std::string scriptPath = std::format("../sbt/templates/{}.cpp.in", name);

        std::ifstream file(path);
        if (!file) {
            file.open(scriptPath);
        }
        if (!file) {
            std::cerr << "Failed to open template: " << path << std::endl;
            printf("%s\n", std::filesystem::current_path().string().c_str());
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
}

bool Emitter::isOpen() const {
    return output.is_open();
}

void Emitter::setIndent(uint8_t level) {
    indent = level;
}

void Emitter::emit(std::string_view text) {
    for (size_t i = 0; i < indent; ++i) {
        output << "\t";
    }
    output << text;
}

void Emitter::emitIf(std::string_view text, bool condition) {
    if (condition) {
        emit(text);
    }
}

void Emitter::emitTemplate(const std::string& name) {
    emit(loadTemplate(name));
}

void Emitter::emitFilledTemplate(const std::string& name, TemplateValues values) {
    emit(fillTemplate(loadTemplate(name), values));
}

void Emitter::emitPinnedRegisters() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("register uint32_t x{} asm (\"{}\");\n", s, h));
    }
}

void Emitter::emitRegisterStore() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("ctx.reg[{}] = x{};\n", s, s));
    }
}

void Emitter::emitRegisterLoad() {
    if (!options.pinRegisters) { return; }

    for (auto& [s, h] : x86RegisterMap) {
        emit(std::format("x{} = ctx.reg[{}];\n", s, s));
    }
}

const Options::TranslationOptions& Emitter::getOptions() const {
    return options;
}
