#include "hotvm/vtable_patch.h"
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace hotvm {

bool VTablePatcher::MakeWritable(void* addr, size_t len) {
#if defined(_WIN32)
    DWORD old_protect;
    return VirtualProtect(addr, len, PAGE_READWRITE, &old_protect) != 0;
#else
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = reinterpret_cast<uintptr_t>(addr) & ~(page_size - 1);
    size_t total_len = reinterpret_cast<uintptr_t>(addr) + len - page_start;
    return mprotect(reinterpret_cast<void*>(page_start), total_len,
                    PROT_READ | PROT_WRITE) == 0;
#endif
}

bool VTablePatcher::RestoreProtection(void* addr, size_t len) {
#if defined(_WIN32)
    DWORD old_protect;
    return VirtualProtect(addr, len, PAGE_READONLY, &old_protect) != 0;
#else
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = reinterpret_cast<uintptr_t>(addr) & ~(page_size - 1);
    size_t total_len = reinterpret_cast<uintptr_t>(addr) + len - page_start;
    return mprotect(reinterpret_cast<void*>(page_start), total_len,
                    PROT_READ) == 0;
#endif
}

bool VTablePatcher::PatchSlot(void* vtable_addr, uint32_t slot, void* new_fn) {
    auto vtable = static_cast<void**>(vtable_addr);
    void* slot_addr = &vtable[slot];

    // Save original
    VTablePatchEntry entry;
    entry.vtable_addr = vtable_addr;
    entry.slot_index = slot;
    std::memcpy(&entry.original_fn, slot_addr, sizeof(void*));
    entry.new_fn = new_fn;

    // Make writable, patch, restore
    if (!MakeWritable(slot_addr, sizeof(void*))) return false;
    std::memcpy(slot_addr, &new_fn, sizeof(void*));
    RestoreProtection(slot_addr, sizeof(void*));

    entries_.push_back(entry);
    return true;
}

bool VTablePatcher::UnpatchSlot(void* vtable_addr, uint32_t slot) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->vtable_addr == vtable_addr && it->slot_index == slot) {
            auto vtable = static_cast<void**>(vtable_addr);
            void* slot_addr = &vtable[slot];

            if (!MakeWritable(slot_addr, sizeof(void*))) return false;
            std::memcpy(slot_addr, &it->original_fn, sizeof(void*));
            RestoreProtection(slot_addr, sizeof(void*));

            entries_.erase(it);
            return true;
        }
    }
    return false;
}

void VTablePatcher::UnpatchAll() {
    for (auto& entry : entries_) {
        auto vtable = static_cast<void**>(entry.vtable_addr);
        void* slot_addr = &vtable[entry.slot_index];
        if (MakeWritable(slot_addr, sizeof(void*))) {
            std::memcpy(slot_addr, &entry.original_fn, sizeof(void*));
            RestoreProtection(slot_addr, sizeof(void*));
        }
    }
    entries_.clear();
}

}  // namespace hotvm
