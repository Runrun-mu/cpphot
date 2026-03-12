#include "tools/hotdiff/diff.h"
#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace hotvm {
namespace tools {

bool ModuleDiff::FunctionsEqual(const VmFunction& a, const VmFunction& b) {
    if (a.code.size() != b.code.size()) return false;
    if (a.param_count != b.param_count) return false;
    if (a.ret_kind != b.ret_kind) return false;

    for (size_t i = 0; i < a.code.size(); ++i) {
        if (std::memcmp(&a.code[i], &b.code[i], sizeof(Instruction)) != 0) {
            return false;
        }
    }
    return true;
}

uint32_t ModuleDiff::ModuleHash(const Module& mod) {
    // Simple hash based on function count, names, and code sizes
    uint32_t hash = 0x811c9dc5;  // FNV-1a offset basis
    for (auto& func : mod.functions) {
        for (char c : func.name) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 0x01000193;  // FNV-1a prime
        }
        uint32_t code_size = static_cast<uint32_t>(func.code.size());
        hash ^= code_size;
        hash *= 0x01000193;
    }
    return hash;
}

PatchManifest ModuleDiff::Diff(const Module& old_mod, const Module& new_mod) {
    PatchManifest manifest;
    manifest.base_version_hash = ModuleHash(old_mod);
    manifest.version = 1;

    // Build name → function maps
    std::unordered_map<std::string, const VmFunction*> old_funcs;
    std::unordered_map<std::string, const VmFunction*> new_funcs;

    for (auto& f : old_mod.functions) old_funcs[f.name] = &f;
    for (auto& f : new_mod.functions) new_funcs[f.name] = &f;

    // Check for modified and removed functions
    for (auto& [name, old_fn] : old_funcs) {
        auto it = new_funcs.find(name);
        if (it == new_funcs.end()) {
            // Removed
            PatchEntry entry;
            entry.action = PatchAction::kRemoved;
            entry.func_name = name;
            entry.func_id = old_fn->func_id;
            manifest.entries.push_back(std::move(entry));
        } else if (!FunctionsEqual(*old_fn, *it->second)) {
            // Modified
            PatchEntry entry;
            entry.action = PatchAction::kModified;
            entry.func_name = name;
            entry.func_id = old_fn->func_id;
            entry.new_func = *it->second;
            entry.new_func.func_id = old_fn->func_id;  // Preserve original func_id
            manifest.entries.push_back(std::move(entry));
        }
        // else: identical, skip
    }

    // Check for added functions
    for (auto& [name, new_fn] : new_funcs) {
        if (old_funcs.find(name) == old_funcs.end()) {
            PatchEntry entry;
            entry.action = PatchAction::kAdded;
            entry.func_name = name;
            entry.func_id = new_fn->func_id;
            entry.new_func = *new_fn;
            manifest.entries.push_back(std::move(entry));
        }
    }

    return manifest;
}

PatchManifest ModuleDiff::DiffFiles(const std::string& old_path,
                                     const std::string& new_path) {
    Module old_mod = ReadModule(old_path);
    Module new_mod = ReadModule(new_path);
    return Diff(old_mod, new_mod);
}

}  // namespace tools
}  // namespace hotvm
