#include "hotvm/type_registry.h"
#include <stdexcept>

namespace hotvm {

TypeRegistry& TypeRegistry::Instance() {
    static TypeRegistry instance;
    return instance;
}

TypeId TypeRegistry::RegisterType(const TypeInfo& info) {
    TypeId id = info.type_id;
    if (id == kInvalidTypeId) {
        id = next_id_++;
    } else {
        if (id >= next_id_) next_id_ = id + 1;
    }

    TypeInfo copy = info;
    copy.type_id = id;
    name_to_id_[copy.name] = id;
    types_[id] = std::move(copy);
    return id;
}

TypeId TypeRegistry::RegisterType(TypeInfo&& info) {
    TypeId id = info.type_id;
    if (id == kInvalidTypeId) {
        id = next_id_++;
    } else {
        if (id >= next_id_) next_id_ = id + 1;
    }

    info.type_id = id;
    name_to_id_[info.name] = id;
    types_[id] = std::move(info);
    return id;
}

const TypeInfo* TypeRegistry::GetType(TypeId id) const {
    auto it = types_.find(id);
    return it != types_.end() ? &it->second : nullptr;
}

const TypeInfo* TypeRegistry::GetTypeByName(const std::string& name) const {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) return nullptr;
    return GetType(it->second);
}

TypeId TypeRegistry::GetTypeId(const std::string& name) const {
    auto it = name_to_id_.find(name);
    return it != name_to_id_.end() ? it->second : kInvalidTypeId;
}

bool TypeRegistry::IsDerivedFrom(TypeId derived, TypeId base) const {
    if (derived == base) return true;
    const TypeInfo* info = GetType(derived);
    while (info && info->base_type_id != kInvalidTypeId) {
        if (info->base_type_id == base) return true;
        info = GetType(info->base_type_id);
    }
    return false;
}

bool TypeRegistry::GetFieldOffset(TypeId type_id, const std::string& field_name,
                                    uint32_t* out_offset) const {
    const TypeInfo* info = GetType(type_id);
    if (!info) return false;
    for (auto& f : info->fields) {
        if (f.name == field_name) {
            *out_offset = f.offset;
            return true;
        }
    }
    // Check base class
    if (info->base_type_id != kInvalidTypeId) {
        return GetFieldOffset(info->base_type_id, field_name, out_offset);
    }
    return false;
}

bool TypeRegistry::GetVMethodSlot(TypeId type_id, const std::string& method_name,
                                    uint32_t* out_slot) const {
    const TypeInfo* info = GetType(type_id);
    if (!info) return false;
    for (auto& vm : info->vmethods) {
        if (vm.name == method_name) {
            *out_slot = vm.vtable_slot;
            return true;
        }
    }
    if (info->base_type_id != kInvalidTypeId) {
        return GetVMethodSlot(info->base_type_id, method_name, out_slot);
    }
    return false;
}

void TypeRegistry::RegisterFromModule(const std::vector<TypeInfo>& types) {
    for (auto& t : types) {
        RegisterType(t);
    }
}

}  // namespace hotvm
