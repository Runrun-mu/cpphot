#pragma once
#include "hotvm/module.h"
#include <string>

namespace hotvm {
namespace compiler {

// ── Module writer ────────────────────────────────────────────
// Wrapper around WriteModule with additional options.

class ModuleWriter {
public:
    struct Options {
        bool emit_ir = false;       // Dump IR to stdout
        bool dump_bytecode = false; // Dump bytecode disassembly to stdout
    };

    // Write module to .hotmod file
    static bool Write(const std::string& path, const Module& mod,
                       const Options& opts = {});

    // Dump bytecode disassembly to stream
    static void DumpBytecode(std::ostream& os, const Module& mod);

    // Dump a single function's bytecode
    static void DumpFunction(std::ostream& os, const VmFunction& func);

    // Get opcode name as string
    static const char* OpCodeName(OpCode op);
};

}  // namespace compiler
}  // namespace hotvm
