#pragma once
#include "hotvm/types.h"
#include <cstdint>
#include <vector>

namespace hotvm {

// ── VTable patcher ───────────────────────────────────────────
// Modifies C++ vtable slots in-memory to redirect virtual calls.

class VTablePatcher {
public:
    struct VTablePatchEntry {
        void*     vtable_addr;     // Address of vtable
        uint32_t  slot_index;      // Which slot in the vtable
        void*     original_fn;     // Original function pointer (for rollback)
        void*     new_fn;          // New function pointer (wrapper stub)
    };

    // Patch a vtable slot: vtable[slot] = new_fn
    bool PatchSlot(void* vtable_addr, uint32_t slot, void* new_fn);

    // Unpatch: restore original
    bool UnpatchSlot(void* vtable_addr, uint32_t slot);

    // Unpatch all
    void UnpatchAll();

    const std::vector<VTablePatchEntry>& Entries() const { return entries_; }

private:
    // Make memory writable for vtable patching
    static bool MakeWritable(void* addr, size_t len);
    static bool RestoreProtection(void* addr, size_t len);

    std::vector<VTablePatchEntry> entries_;
};

}  // namespace hotvm
