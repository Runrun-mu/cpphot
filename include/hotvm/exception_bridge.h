#pragma once
#include "hotvm/types.h"
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace hotvm {

// ── Exception bridge ─────────────────────────────────────────
// Maps between VM exception type IDs and C++ exception types.

class ExceptionBridge {
public:
    using ThrowFn = std::function<void(uint64_t value)>;
    using CatchFn = std::function<bool(uint64_t* out_value)>;

    static ExceptionBridge& Instance();

    // Register a C++ exception type
    template<typename T>
    void Register(TypeId type_id) {
        throw_fns_[type_id] = [](uint64_t value) {
            T v;
            std::memcpy(&v, &value, sizeof(T) < 8 ? sizeof(T) : 8);
            throw v;
        };
        catch_type_ids_[std::type_index(typeid(T))] = type_id;
    }

    // Throw a VM exception as a C++ exception
    void ThrowAsNative(TypeId type_id, uint64_t value);

    // Get type ID for a caught C++ exception
    TypeId GetTypeId(const std::type_index& ti) const;

private:
    ExceptionBridge() = default;
    std::unordered_map<TypeId, ThrowFn> throw_fns_;
    std::unordered_map<std::type_index, TypeId> catch_type_ids_;
};

}  // namespace hotvm
