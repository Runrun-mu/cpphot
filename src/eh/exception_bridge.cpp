#include "hotvm/exception_bridge.h"
#include <stdexcept>

namespace hotvm {

ExceptionBridge& ExceptionBridge::Instance() {
    static ExceptionBridge instance;
    return instance;
}

void ExceptionBridge::ThrowAsNative(TypeId type_id, uint64_t value) {
    auto it = throw_fns_.find(type_id);
    if (it != throw_fns_.end()) {
        it->second(value);
    }
    // Fallback: throw generic runtime_error
    throw std::runtime_error("VM exception: type=" + std::to_string(type_id) +
                              " value=" + std::to_string(value));
}

TypeId ExceptionBridge::GetTypeId(const std::type_index& ti) const {
    auto it = catch_type_ids_.find(ti);
    return it != catch_type_ids_.end() ? it->second : kInvalidTypeId;
}

}  // namespace hotvm
