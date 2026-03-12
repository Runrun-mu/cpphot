#include "hotvm/patch_manager.h"
#include "hotvm/runtime.h"
#include <stdexcept>

namespace hotvm {

PatchManager& PatchManager::Instance() {
    static PatchManager instance;
    return instance;
}

bool PatchManager::ApplyPatch(const std::string& hotpatch_path) {
    PatchManifest manifest = ReadPatchManifest(hotpatch_path);
    return ApplyPatch(manifest);
}

bool PatchManager::ApplyPatch(const PatchManifest& manifest) {
    auto& rt = Runtime::Instance();
    auto& interp = rt.GetInterpreter();

    PatchRecord record;
    record.version = ++current_version_;

    for (auto& entry : manifest.entries) {
        switch (entry.action) {
        case PatchAction::kModified: {
            // Save old bytecode for rollback
            const VmFunction* old_func = interp.GetFunction(entry.func_id);
            if (old_func) {
                record.modified_funcs.push_back(entry.func_id);
                record.old_bytecodes.push_back(*old_func);
            }

            // Save old bind info
            WrapperId wid = rt.GetWrapperForFunc(entry.func_id);
            if (wid != kInvalidWrapperId) {
                const WrapperMeta* meta = rt.GetWrapperMeta(wid);
                if (meta) {
                    record.old_binds.push_back(meta->bind_info);
                }
            }

            // Register new bytecode
            interp.RegisterFunction(entry.new_func);

            // Switch dispatch mode to VM
            if (wid != kInvalidWrapperId) {
                BindInfo new_bind;
                new_bind.func_id = entry.func_id;
                new_bind.mode = DispatchMode::kVM;
                new_bind.ret_kind = entry.new_func.ret_kind;
                new_bind.param_count = entry.new_func.param_count;
                for (int i = 0; i < entry.new_func.param_count; ++i) {
                    new_bind.param_kinds[i] = entry.new_func.param_kinds[i];
                }
                rt.RebindWrapper(wid, new_bind);
            }
            break;
        }

        case PatchAction::kAdded: {
            // Register new function in VM
            interp.RegisterFunction(entry.new_func);

            // Allocate wrapper for it
            BindInfo bind;
            bind.func_id = entry.func_id;
            bind.mode = DispatchMode::kVM;
            bind.ret_kind = entry.new_func.ret_kind;
            bind.param_count = entry.new_func.param_count;
            for (int i = 0; i < entry.new_func.param_count; ++i) {
                bind.param_kinds[i] = entry.new_func.param_kinds[i];
            }
            WrapperId wid = rt.AllocWrapper(bind);
            rt.SetDispatchEntry(entry.func_id, wid);

            record.modified_funcs.push_back(entry.func_id);
            break;
        }

        case PatchAction::kRemoved: {
            // Mark as unavailable but don't free (prevent dangling pointers)
            const VmFunction* old_func = interp.GetFunction(entry.func_id);
            if (old_func) {
                record.modified_funcs.push_back(entry.func_id);
                record.old_bytecodes.push_back(*old_func);
            }
            interp.UnregisterFunction(entry.func_id);
            break;
        }
        }
    }

    history_.push_back(std::move(record));
    return true;
}

bool PatchManager::Rollback() {
    if (history_.empty()) return false;

    auto& rt = Runtime::Instance();
    auto& interp = rt.GetInterpreter();
    auto& record = history_.back();

    // Restore old bytecodes
    for (auto& old_func : record.old_bytecodes) {
        interp.RegisterFunction(old_func);
    }

    // Restore old bind infos
    for (size_t i = 0; i < record.modified_funcs.size() && i < record.old_binds.size(); ++i) {
        FuncId fid = record.modified_funcs[i];
        WrapperId wid = rt.GetWrapperForFunc(fid);
        if (wid != kInvalidWrapperId) {
            rt.RebindWrapper(wid, record.old_binds[i]);
        }
    }

    history_.pop_back();
    current_version_--;
    return true;
}

}  // namespace hotvm
