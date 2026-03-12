#include "hotvm/function_patch.h"
#include "hotvm/runtime.h"

namespace hotvm {

bool FunctionPatchTable::Patch(FuncId func_id, WrapperId wrapper_id) {
    auto& rt = Runtime::Instance();

    // Save original binding
    const WrapperMeta* meta = rt.GetWrapperMeta(wrapper_id);
    if (!meta) return false;

    PatchEntry entry;
    entry.func_id = func_id;
    entry.wrapper_id = wrapper_id;
    entry.original_bind = meta->bind_info;

    entries_[func_id] = entry;
    return true;
}

bool FunctionPatchTable::Unpatch(FuncId func_id) {
    auto it = entries_.find(func_id);
    if (it == entries_.end()) return false;

    auto& rt = Runtime::Instance();
    // Restore original binding
    rt.RebindWrapper(it->second.wrapper_id, it->second.original_bind);
    entries_.erase(it);
    return true;
}

bool FunctionPatchTable::IsPatched(FuncId func_id) const {
    return entries_.count(func_id) > 0;
}

const FunctionPatchTable::PatchEntry* FunctionPatchTable::GetEntry(FuncId func_id) const {
    auto it = entries_.find(func_id);
    return it != entries_.end() ? &it->second : nullptr;
}

}  // namespace hotvm
