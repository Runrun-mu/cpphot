#pragma once
#include "hotvm/types.h"
#include "hotvm/interpreter.h"
#include "hotvm/wrapper_table.h"
#include <functional>
#include <mutex>
#include <vector>

namespace hotvm {

// ── Runtime singleton ────────────────────────────────────────
// Manages wrapper table, dispatch, and native ↔ VM bridging.

class Runtime {
public:
    static Runtime& Instance();

    // ── Wrapper management ──

    // Allocate a wrapper slot and bind it to a function
    WrapperId AllocWrapper(const BindInfo& bind);

    // Rebind an existing wrapper to new BindInfo (used during patching)
    void RebindWrapper(WrapperId id, const BindInfo& bind);

    // Get wrapper metadata
    const WrapperMeta* GetWrapperMeta(WrapperId id) const;

    // ── Function registration ──

    // Register a native function, returns its wrapper ID
    WrapperId RegisterNativeFunction(FuncId func_id, void* fn_ptr,
                                      const ArgKind* param_kinds, uint8_t param_count,
                                      RetKind ret_kind);

    // Register a VM function (bytecode)
    void RegisterVmFunction(const VmFunction& func);
    void RegisterVmFunction(VmFunction&& func);

    // ── Dispatch ──

    // Called from assembly adapter: dispatches to native or VM
    static void AdapterDispatch(AdapterFrame* frame);

    // Native fast-path forward (no VM involvement)
    static void NativeForward(AdapterFrame* frame, const BindInfo& bind);

    // ── Accessors ──

    Interpreter& GetInterpreter() { return interpreter_; }
    WrapperTable& GetWrapperTable() { return wrapper_table_; }

    // ── Dispatch table ──
    // Maps FuncId → WrapperId for global function dispatch
    WrapperId GetWrapperForFunc(FuncId func_id) const;
    void SetDispatchEntry(FuncId func_id, WrapperId wrapper_id);

    // Get the current call convention based on platform
    static CallConv PlatformCallConv();

private:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    void UnpackArgsToVm(const AdapterFrame* frame, const BindInfo& bind,
                         uint64_t* out_args);

    Interpreter   interpreter_;
    WrapperTable  wrapper_table_;

    // FuncId → WrapperId dispatch table
    std::unordered_map<FuncId, WrapperId> dispatch_table_;

    mutable std::mutex mu_;
};

// ── C linkage for assembly ───────────────────────────────────
extern "C" {
    void AdapterDispatch(void* frame);
}

}  // namespace hotvm
