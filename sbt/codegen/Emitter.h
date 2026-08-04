#ifndef RISCV_TOOLS_EMITTER_H
#define RISCV_TOOLS_EMITTER_H

#include "Options.h"

#include <runtime/registers.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using TemplateValues = std::initializer_list<std::pair<std::string_view, std::string>>;

inline constexpr std::array<std::pair<int, std::string_view>, 8> x86RegisterMap = {{
    {ra, "r8"}, {sp, "r9"}, {a5, "r10"}, {a4, "r11"}, {a0, "r12"}, {s0, "r13"}, {a3, "r14"}, {a2, "r15"}
}};

constexpr std::optional<std::string_view> getHostRegister(int sourceRegister) {
    for (auto& [s, h] : x86RegisterMap) {
        if (s == sourceRegister) return h;
    }

    return {};
}

class Emitter {
public:
    Emitter(const std::filesystem::path& outputPath, const Options::TranslationOptions& options) : output(outputPath), options(options) {};

    bool isOpen() const;
    void setIndent(uint8_t level);

    void emit(std::string_view text);
    void emitIf(std::string_view text, bool condition);

    void emitTemplate(const std::string& name);
    void emitFilledTemplate(const std::string& name, TemplateValues values);

    void emitPinnedRegisters();
    void emitRegisterStore();
    void emitRegisterLoad();

    const Options::TranslationOptions& getOptions() const;

private:
    std::ofstream output;
    const Options::TranslationOptions& options;
    uint8_t indent = 0;
};

#endif //RISCV_TOOLS_EMITTER_H
