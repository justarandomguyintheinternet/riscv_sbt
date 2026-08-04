#ifndef RISCV_TOOLS_EMITTER_H
#define RISCV_TOOLS_EMITTER_H

#include "Options.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

using TemplateValues = std::initializer_list<std::pair<std::string_view, std::string>>;

using RegisterMapping = std::pair<int, std::string_view>;
using RegisterMap = std::span<const RegisterMapping>;

class Emitter {
public:
    Emitter(const std::filesystem::path& outputPath, const Options::TranslationOptions& options, RegisterMap registerMap) : output(outputPath), options(options), registerMap(registerMap) {};

    bool isOpen() const;
    void setIndent(uint8_t level);

    void emit(std::string_view text);
    void emitIf(std::string_view text, bool condition);

    void emitTemplate(const std::string& name);
    void emitFilledTemplate(const std::string& name, TemplateValues values);

    void emitPinnedRegisters();
    void emitRegisterStore();
    void emitRegisterLoad();

    std::optional<std::string_view> getHostRegister(int sourceRegister) const;

    const Options::TranslationOptions& getOptions() const;

private:
    std::ofstream output;
    const Options::TranslationOptions& options;
    RegisterMap registerMap;
    uint8_t indent = 0;
};

#endif //RISCV_TOOLS_EMITTER_H
