#include "hotvm/wrapper_table.h"
#include <cstring>

namespace hotvm {

WrapperTable::WrapperTable() {
    std::memset(slots_, 0, sizeof(slots_));
    for (int i = 0; i < kMaxWrappers; ++i) {
        slots_[i].wrapper_id = static_cast<WrapperId>(i);
    }
}

WrapperTable::~WrapperTable() = default;

WrapperId WrapperTable::Alloc(const BindInfo& bind) {
    // Linear scan for free slot starting from next_free_
    for (uint32_t i = 0; i < kMaxWrappers; ++i) {
        uint32_t idx = (next_free_ + i) % kMaxWrappers;
        if (!slots_[idx].in_use) {
            slots_[idx].in_use = true;
            slots_[idx].bind_info = bind;
            slots_[idx].this_adjust = 0;
            alloc_count_++;
            next_free_ = (idx + 1) % kMaxWrappers;
            return static_cast<WrapperId>(idx);
        }
    }
    return kInvalidWrapperId;  // Full
}

void WrapperTable::Free(WrapperId id) {
    if (id < kMaxWrappers && slots_[id].in_use) {
        slots_[id].in_use = false;
        slots_[id].bind_info = BindInfo{};
        slots_[id].this_adjust = 0;
        alloc_count_--;
    }
}

void WrapperTable::Rebind(WrapperId id, const BindInfo& bind) {
    if (id < kMaxWrappers && slots_[id].in_use) {
        slots_[id].bind_info = bind;
    }
}

WrapperMeta* WrapperTable::Get(WrapperId id) {
    if (id < kMaxWrappers) return &slots_[id];
    return nullptr;
}

const WrapperMeta* WrapperTable::Get(WrapperId id) const {
    if (id < kMaxWrappers) return &slots_[id];
    return nullptr;
}

}  // namespace hotvm
