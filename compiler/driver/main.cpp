#include "compiler/frontend/ast_visitor.h"
#include "compiler/codegen/codegen.h"
#include "compiler/codegen/module_writer.h"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace hotvm;
using namespace hotvm::compiler;

static void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.cpp> -o <output.hotmod> [options]\n"
              << "\nOptions:\n"
              << "  -o <file>        Output .hotmod file\n"
              << "  --emit-ir        Print IR to stdout\n"
              << "  --dump-bytecode  Print bytecode disassembly to stdout\n"
              << "  -I <dir>         Add include directory\n"
              << "  -D <macro>       Define preprocessor macro\n"
              << "  --               Extra clang arguments follow\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string input_file;
    std::string output_file;
    bool emit_ir = false;
    bool dump_bytecode = false;
    std::vector<std::string> clang_args;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (std::strcmp(argv[i], "--emit-ir") == 0) {
            emit_ir = true;
        } else if (std::strcmp(argv[i], "--dump-bytecode") == 0) {
            dump_bytecode = true;
        } else if (std::strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            clang_args.push_back(std::string("-I") + argv[++i]);
        } else if (std::strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            clang_args.push_back(std::string("-D") + argv[++i]);
        } else if (std::strcmp(argv[i], "--") == 0) {
            for (int j = i + 1; j < argc; ++j) {
                clang_args.push_back(argv[j]);
            }
            break;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: no input file specified\n";
        return 1;
    }
    if (output_file.empty()) {
        // Default output name
        output_file = input_file;
        auto dot = output_file.rfind('.');
        if (dot != std::string::npos) {
            output_file = output_file.substr(0, dot);
        }
        output_file += ".hotmod";
    }

    try {
        // Phase 1: Parse C++ → IR
        std::cout << "Parsing " << input_file << "...\n";
        ASTVisitor visitor;
        ir::IRModule ir_mod = visitor.Parse(input_file, clang_args);
        std::cout << "  " << ir_mod.functions.size() << " functions, "
                  << ir_mod.types.size() << " types\n";

        if (emit_ir) {
            std::cout << "\n=== IR Dump ===\n";
            for (auto& func : ir_mod.functions) {
                std::cout << "func " << func.mangled_name
                          << " (id=" << func.func_id << ")\n";
                for (auto& inst : func.body) {
                    std::cout << "  " << static_cast<int>(inst.op) << "\n";
                }
                std::cout << "\n";
            }
        }

        // Phase 2: IR → Bytecode
        std::cout << "Generating bytecode...\n";
        Codegen codegen;
        Module mod = codegen.Generate(ir_mod);

        // Phase 3: Write .hotmod
        ModuleWriter::Options opts;
        opts.emit_ir = emit_ir;
        opts.dump_bytecode = dump_bytecode;

        std::cout << "Writing " << output_file << "...\n";
        if (!ModuleWriter::Write(output_file, mod, opts)) {
            std::cerr << "Error: failed to write output\n";
            return 1;
        }

        std::cout << "Done. Output: " << output_file << "\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
