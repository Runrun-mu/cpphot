#pragma once
#include "hotvm/types.h"
#include <string>
#include <vector>

namespace hotvm {

// ── Patch manifest ───────────────────────────────────────────
// Describes what changes a patch applies.

enum class PatchAction : uint8_t {
    kModified = 1,
    kAdded    = 2,
    kRemoved  = 3,
};

struct PatchEntry {
    PatchAction action;
    std::string func_name;     // Mangled name
    FuncId      func_id;
    // For MODIFIED / ADDED:
    VmFunction  new_func;      // New bytecode (empty for REMOVED)
};

struct PatchManifest {
    uint32_t version = 0;
    uint32_t base_version_hash = 0;
    std::vector<PatchEntry> entries;
};

// Serialize / deserialize .hotpatch files
bool WritePatchManifest(const std::string& path, const PatchManifest& manifest);
PatchManifest ReadPatchManifest(const std::string& path);

}  // namespace hotvm
