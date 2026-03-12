#pragma once
#include "hotvm/bytecode.h"
#include <unordered_map>
#include <vector>

namespace hotvm {

// Maximum number of registers per VM frame
constexpr int kMaxRegisters = 256;

// ── VM execution frame (one per call) ────────────────────────

struct VmFrame {
    uint64_t regs[kMaxRegisters] = {};  // General-purpose registers (also hold pointers)
    const VmFunction* func = nullptr;
    uint32_t pc = 0;
    uint8_t  result_reg = 0;           // Which register receives call return value
};

// ── Interpreter ──────────────────────────────────────────────

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // Register a VM function
    void RegisterFunction(const VmFunction& func);
    void RegisterFunction(VmFunction&& func);

    // Unregister / replace
    void UnregisterFunction(FuncId id);
    bool HasFunction(FuncId id) const;
    const VmFunction* GetFunction(FuncId id) const;

    // Execute a function by ID, passing args as raw uint64_t values
    // Returns the result as uint64_t (caller reinterprets based on RetKind)
    uint64_t Execute(FuncId func_id, const uint64_t* args, int arg_count);

    // Execute with typed arguments (convenience)
    uint64_t Execute(FuncId func_id, const std::vector<uint64_t>& args);

private:
    // Core execution loop for one function
    uint64_t ExecFunction(const VmFunction* func, const uint64_t* args, int arg_count);

    // Dispatch a CALL instruction (may recurse into VM or call native)
    uint64_t DispatchCall(FuncId target_id, const uint64_t* args, int arg_count);

    // Dispatch a virtual call
    uint64_t DispatchVirtualCall(uint64_t obj_ptr, uint32_t vtable_slot,
                                  const uint64_t* args, int arg_count);

    std::unordered_map<FuncId, VmFunction> functions_;
};

}  // namespace hotvm
