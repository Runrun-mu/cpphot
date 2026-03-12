#pragma once
#include "hotvm/types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace hotvm {

// ── Function patch table ─────────────────────────────────────
// Redirects non-virtual function calls through wrapper indirection.

class FunctionPatchTable {
public:
    // Patch: redirect func_id to go through wrapper
    bool Patch(FuncId func_id, WrapperId wrapper_id);

    // Unpatch: restore original binding
    bool Unpatch(FuncId func_id);

    // Query
    bool IsPatched(FuncId func_id) const;

    struct PatchEntry {
        FuncId    func_id;
        WrapperId wrapper_id;
        BindInfo  original_bind;  // Saved for rollback
    };

    const PatchEntry* GetEntry(FuncId func_id) const;

private:
    std::unordered_map<FuncId, PatchEntry> entries_;
};

}  // namespace hotvm
