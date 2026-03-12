#pragma once
#include "hotvm/module.h"
#include "hotvm/patch_manifest.h"
#include <string>

namespace hotvm {
namespace tools {

// ── Module diff ──────────────────────────────────────────────
// Compares two .hotmod modules and produces a .hotpatch manifest.

class ModuleDiff {
public:
    // Diff two modules, produce a patch manifest
    static PatchManifest Diff(const Module& old_mod, const Module& new_mod);

    // Diff from file paths
    static PatchManifest DiffFiles(const std::string& old_path,
                                    const std::string& new_path);

    // Check if two functions have identical bytecode
    static bool FunctionsEqual(const VmFunction& a, const VmFunction& b);

    // Compute a hash of a module (for base_version_hash)
    static uint32_t ModuleHash(const Module& mod);
};

}  // namespace tools
}  // namespace hotvm
