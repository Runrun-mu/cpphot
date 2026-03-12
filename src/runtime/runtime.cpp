#include "hotvm/runtime.h"
#include <cstring>
#include <stdexcept>

namespace hotvm {

Runtime& Runtime::Instance() {
    static Runtime instance;
    return instance;
}

Runtime::Runtime() = default;
Runtime::~Runtime() = default;

CallConv Runtime::PlatformCallConv() {
#if defined(HOTVM_ARCH_arm64)
    return CallConv::kAapcs64;
#elif defined(HOTVM_ARCH_x86_64)
    #if defined(_WIN32)
        return CallConv::kWin64;
    #else
        return CallConv::kSystemV;
    #endif
#else
    return CallConv::kSystemV;
#endif
}

// ── Wrapper management ───────────────────────────────────────

WrapperId Runtime::AllocWrapper(const BindInfo& bind) {
    std::lock_guard<std::mutex> lock(mu_);
    return wrapper_table_.Alloc(bind);
}

void Runtime::RebindWrapper(WrapperId id, const BindInfo& bind) {
    std::lock_guard<std::mutex> lock(mu_);
    wrapper_table_.Rebind(id, bind);
}

const WrapperMeta* Runtime::GetWrapperMeta(WrapperId id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return wrapper_table_.Get(id);
}

// ── Function registration ────────────────────────────────────

WrapperId Runtime::RegisterNativeFunction(FuncId func_id, void* fn_ptr,
                                           const ArgKind* param_kinds,
                                           uint8_t param_count,
                                           RetKind ret_kind) {
    BindInfo bind;
    bind.func_id = func_id;
    bind.mode = DispatchMode::kNative;
    bind.call_conv = PlatformCallConv();
    bind.ret_kind = ret_kind;
    bind.param_count = param_count;
    bind.native_fn = fn_ptr;
    for (uint8_t i = 0; i < param_count && i < 16; ++i) {
        bind.param_kinds[i] = param_kinds[i];
    }

    WrapperId wid = AllocWrapper(bind);
    if (wid != kInvalidWrapperId) {
        std::lock_guard<std::mutex> lock(mu_);
        dispatch_table_[func_id] = wid;
    }
    return wid;
}

void Runtime::RegisterVmFunction(const VmFunction& func) {
    interpreter_.RegisterFunction(func);
}

void Runtime::RegisterVmFunction(VmFunction&& func) {
    interpreter_.RegisterFunction(std::move(func));
}

// ── Dispatch ─────────────────────────────────────────────────

void Runtime::AdapterDispatch(AdapterFrame* frame) {
    auto& rt = Instance();
    WrapperId wid = static_cast<WrapperId>(frame->wrapper_id);

    const WrapperMeta* meta = rt.wrapper_table_.Get(wid);
    if (!meta || !meta->in_use) {
        // Invalid wrapper
        return;
    }

    const BindInfo& bind = meta->bind_info;

    if (bind.mode == DispatchMode::kNative) {
        // Fast path: forward directly to native function
        NativeForward(frame, bind);
        return;
    }

    // Slow path: unpack args and execute in VM
    uint64_t vm_args[16] = {};
    rt.UnpackArgsToVm(frame, bind, vm_args);

    uint64_t result = rt.interpreter_.Execute(
        bind.func_id, vm_args, bind.param_count);

    // Store result back to frame
    switch (bind.ret_kind) {
    case RetKind::kI64:
    case RetKind::kPtr:
#if defined(HOTVM_ARCH_x86_64)
        frame->ret_rax = result;
#elif defined(HOTVM_ARCH_arm64)
        frame->ret_x0 = result;
#endif
        break;
    case RetKind::kF64: {
        double d;
        std::memcpy(&d, &result, sizeof(double));
#if defined(HOTVM_ARCH_x86_64)
        std::memcpy(frame->ret_xmm0, &d, sizeof(double));
#elif defined(HOTVM_ARCH_arm64)
        std::memcpy(frame->ret_q0, &d, sizeof(double));
#endif
        break;
    }
    case RetKind::kVoid:
        break;
    }

    frame->ret_kind = static_cast<uint32_t>(bind.ret_kind);
}

void Runtime::NativeForward(AdapterFrame* frame, const BindInfo& bind) {
    // In a full implementation, this would:
    // 1. Restore native registers from frame
    // 2. Jump to bind.native_fn
    // 3. Capture return value
    //
    // For the software-only path (no assembly), we do a C-level call.
    // The assembly adapter handles the real register-level forwarding.

    if (!bind.native_fn) return;

    // Simplified: call native with up to 6 integer args from frame
    using NativeFn6 = uint64_t(*)(uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t);
    auto fn = reinterpret_cast<NativeFn6>(bind.native_fn);

#if defined(HOTVM_ARCH_x86_64)
    uint64_t result = fn(frame->gpr[0], frame->gpr[1], frame->gpr[2],
                          frame->gpr[3], frame->gpr[4], frame->gpr[5]);
    frame->ret_rax = result;
#elif defined(HOTVM_ARCH_arm64)
    uint64_t result = fn(frame->gpr[0], frame->gpr[1], frame->gpr[2],
                          frame->gpr[3], frame->gpr[4], frame->gpr[5]);
    frame->ret_x0 = result;
#else
    uint64_t result = fn(frame->gpr[0], frame->gpr[1], frame->gpr[2],
                          frame->gpr[3], frame->gpr[4], frame->gpr[5]);
    frame->ret_0 = result;
#endif
}

void Runtime::UnpackArgsToVm(const AdapterFrame* frame, const BindInfo& bind,
                               uint64_t* out_args) {
    // Unpack arguments from adapter frame to flat array for VM
    int gpr_idx = 0;
    int sse_idx = 0;

    for (int i = 0; i < bind.param_count; ++i) {
        switch (bind.param_kinds[i]) {
        case ArgKind::kI8:
        case ArgKind::kI16:
        case ArgKind::kI32:
        case ArgKind::kI64:
        case ArgKind::kBool:
        case ArgKind::kPtr:
            if (gpr_idx < 6) {
                out_args[i] = frame->gpr[gpr_idx++];
            }
            break;
        case ArgKind::kF32:
        case ArgKind::kF64:
#if defined(HOTVM_ARCH_x86_64)
            if (sse_idx < 8) {
                std::memcpy(&out_args[i], frame->sse[sse_idx++], 8);
            }
#elif defined(HOTVM_ARCH_arm64)
            if (sse_idx < 8) {
                std::memcpy(&out_args[i], frame->fp[sse_idx++], 8);
            }
#endif
            break;
        }
    }
}

// ── Dispatch table ───────────────────────────────────────────

WrapperId Runtime::GetWrapperForFunc(FuncId func_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = dispatch_table_.find(func_id);
    return it != dispatch_table_.end() ? it->second : kInvalidWrapperId;
}

void Runtime::SetDispatchEntry(FuncId func_id, WrapperId wrapper_id) {
    std::lock_guard<std::mutex> lock(mu_);
    dispatch_table_[func_id] = wrapper_id;
}

// ── C linkage ────────────────────────────────────────────────

extern "C" void AdapterDispatch(void* frame) {
    Runtime::AdapterDispatch(static_cast<AdapterFrame*>(frame));
}

}  // namespace hotvm
