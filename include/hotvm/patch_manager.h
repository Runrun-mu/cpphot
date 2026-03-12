#pragma once
#include "hotvm/types.h"
#include "hotvm/bytecode.h"
#include "hotvm/patch_manifest.h"
#include <string>
#include <vector>

namespace hotvm {

// ── Patch manager ────────────────────────────────────────────
// Applies and rolls back .hotpatch incremental patches at runtime.

class PatchManager {
public:
    static PatchManager& Instance();

    // Apply a patch from file
    bool ApplyPatch(const std::string& hotpatch_path);

    // Apply a patch from manifest
    bool ApplyPatch(const PatchManifest& manifest);

    // Rollback the last applied patch
    bool Rollback();

    // Current patch version (0 = original)
    uint32_t CurrentVersion() const { return current_version_; }

    // Number of patches applied
    size_t PatchCount() const { return history_.size(); }

private:
    PatchManager() = default;

    struct PatchRecord {
        uint32_t version;
        std::vector<FuncId>     modified_funcs;
        std::vector<VmFunction> old_bytecodes;   // For rollback
        std::vector<BindInfo>   old_binds;        // Original dispatch bindings
    };

    std::vector<PatchRecord> history_;
    uint32_t current_version_ = 0;
};

}  // namespace hotvm
