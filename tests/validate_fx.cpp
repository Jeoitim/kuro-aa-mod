#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include <iostream>
#include <filesystem>

int main(int argc, char **argv)
{
    if (argc != 4) return 1;
    reshadefx::preprocessor pp;
    pp.add_macro_definition("BUFFER_WIDTH", argv[2]);
    pp.add_macro_definition("BUFFER_HEIGHT", argv[3]);
    pp.add_macro_definition("__RESHADE__", "60000");
    pp.add_include_path(std::filesystem::path(argv[1]).parent_path());
    if (!pp.append_file(argv[1])) { std::cerr << pp.errors(); return 2; }
    auto backend = std::unique_ptr<reshadefx::codegen>(
        reshadefx::create_codegen_dxbc(50, false, false, 1));
    reshadefx::parser parser;
    if (!parser.parse(pp.output(), backend.get())) {
        std::cerr << pp.errors() << parser.errors(); return 3;
    }
    std::cout << pp.errors() << parser.errors();
    for (const auto &entry : backend->module().entry_points) {
        std::string code, assembly, errors;
        if (!backend->assemble_code_for_entry_point(entry.first, code, assembly, errors)) {
            std::cerr << entry.first << ": " << errors; return 4;
        }
        std::cout << entry.first << ": DXBC OK (" << code.size() << " bytes)\n" << errors;
    }
    return 0;
}
