#pragma once
#include "hotvm/types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace hotvm {

// ── Type registry ────────────────────────────────────────────
// Runtime registry for type information (classes, structs).

class TypeRegistry {
public:
    static TypeRegistry& Instance();

    // Register a type
    TypeId RegisterType(const TypeInfo& info);
    TypeId RegisterType(TypeInfo&& info);

    // Lookup
    const TypeInfo* GetType(TypeId id) const;
    const TypeInfo* GetTypeByName(const std::string& name) const;
    TypeId GetTypeId(const std::string& name) const;

    // Check if a type is derived from another
    bool IsDerivedFrom(TypeId derived, TypeId base) const;

    // Get field offset
    bool GetFieldOffset(TypeId type_id, const std::string& field_name,
                         uint32_t* out_offset) const;

    // Get virtual method slot
    bool GetVMethodSlot(TypeId type_id, const std::string& method_name,
                         uint32_t* out_slot) const;

    // Bulk register from module
    void RegisterFromModule(const std::vector<TypeInfo>& types);

    uint32_t TypeCount() const { return static_cast<uint32_t>(types_.size()); }

private:
    TypeRegistry() = default;
    std::unordered_map<TypeId, TypeInfo>      types_;
    std::unordered_map<std::string, TypeId>   name_to_id_;
    TypeId next_id_ = 0;
};

}  // namespace hotvm
