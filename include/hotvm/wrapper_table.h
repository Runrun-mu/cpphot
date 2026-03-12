#pragma once
#include "hotvm/types.h"
#include <vector>

namespace hotvm {

// Maximum number of wrapper stubs (must match assembly generation)
constexpr int kMaxWrappers = 1024;

// ── Wrapper table ────────────────────────────────────────────
// Manages the metadata for each wrapper stub slot.

class WrapperTable {
public:
    WrapperTable();
    ~WrapperTable();

    // Allocate the next free slot, returns kInvalidWrapperId if full
    WrapperId Alloc(const BindInfo& bind);

    // Free a previously allocated slot
    void Free(WrapperId id);

    // Rebind a slot to new BindInfo
    void Rebind(WrapperId id, const BindInfo& bind);

    // Access metadata
    WrapperMeta* Get(WrapperId id);
    const WrapperMeta* Get(WrapperId id) const;

    // Number of allocated slots
    uint32_t AllocatedCount() const { return alloc_count_; }

private:
    WrapperMeta  slots_[kMaxWrappers];
    uint32_t     alloc_count_ = 0;
    WrapperId    next_free_   = 0;
};

}  // namespace hotvm
