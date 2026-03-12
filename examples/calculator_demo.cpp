// ═══════════════════════════════════════════════════════════════
// HotVM Example: Calculator hot-patching demo
//
// Demonstrates the core use case:
// 1. Build a Calculator with compute() that does addition
// 2. Apply a hot-patch to change compute() to multiplication
// 3. Verify the behavior change without restarting
// 4. Rollback to original behavior
// ═══════════════════════════════════════════════════════════════

#include "hotvm/bytecode.h"
#include "hotvm/interpreter.h"
#include "hotvm/runtime.h"
#include "hotvm/patch_manager.h"
#include "hotvm/module.h"
#include "hotvm/type_registry.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace hotvm;

static Instruction MakeInst(OpCode op, uint8_t a = 0, uint8_t b = 0,
                              uint8_t c = 0, uint32_t imm32 = 0,
                              uint64_t imm64 = 0) {
    return Instruction(op, a, b, c, imm32, imm64);
}

int main() {
    std::cout << "═══ HotVM Calculator Demo ═══\n\n";

    auto& rt = Runtime::Instance();
    auto& interp = rt.GetInterpreter();
    auto& type_reg = TypeRegistry::Instance();
    auto& pm = PatchManager::Instance();

    // ── Register Calculator type ─────────────────────────────

    TypeInfo calc_type;
    calc_type.type_id = 0;
    calc_type.name = "Calculator";
    calc_type.size = 16;   // vtable_ptr(8) + value_(8)
    calc_type.align = 8;

    FieldInfo value_field;
    value_field.name = "value_";
    value_field.offset = 8;
    value_field.kind = ArgKind::kI64;
    value_field.type_id = kInvalidTypeId;
    calc_type.fields.push_back(value_field);

    VMethodInfo compute_method;
    compute_method.name = "compute";
    compute_method.vtable_slot = 0;
    compute_method.func_id = 1;
    calc_type.vmethods.push_back(compute_method);

    type_reg.RegisterType(calc_type);

    // ── Register bytecode functions ──────────────────────────

    // Constructor: Calculator(this, v) → this->value_ = v
    VmFunction ctor;
    ctor.func_id = 0;
    ctor.name = "Calculator::Calculator";
    ctor.param_count = 2;
    ctor.reg_count = 3;
    ctor.ret_kind = RetKind::kVoid;
    ctor.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    ctor.code = {
        // this->value_ = v
        MakeInst(OpCode::kStoreField, 0, 1, 0, 8),  // *(r0 + 8) = r1
        MakeInst(OpCode::kRetVoid),
    };
    interp.RegisterFunction(ctor);

    // compute v1: compute(this, x) → this->value_ + x
    VmFunction compute_v1;
    compute_v1.func_id = 1;
    compute_v1.name = "Calculator::compute";
    compute_v1.param_count = 2;
    compute_v1.reg_count = 4;
    compute_v1.ret_kind = RetKind::kI64;
    compute_v1.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_v1.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),   // r2 = this->value_
        MakeInst(OpCode::kAddI64, 3, 2, 1),           // r3 = value_ + x
        MakeInst(OpCode::kRet, 3),
    };
    interp.RegisterFunction(compute_v1);

    // ── Create Calculator object ─────────────────────────────

    // Simulated object: [vtable_ptr(unused), value_]
    uint64_t calc_obj[2] = {0, 0};
    uint64_t calc_ptr = reinterpret_cast<uint64_t>(calc_obj);

    // Call constructor: Calculator(ptr, 10)
    uint64_t ctor_args[] = {calc_ptr, 10};
    interp.Execute(0, ctor_args, 2);
    std::cout << "Created Calculator with value_ = " << calc_obj[1] << "\n";

    // ── Test v1: addition ────────────────────────────────────

    uint64_t compute_args[] = {calc_ptr, 5};
    uint64_t result = interp.Execute(1, compute_args, 2);
    std::cout << "compute(5) = " << static_cast<int64_t>(result)
              << " (expected 15, addition)\n";
    assert(static_cast<int64_t>(result) == 15);

    // ── Apply hot-patch: change to multiplication ────────────

    std::cout << "\nApplying hot-patch: compute → multiplication...\n";

    PatchManifest patch;
    patch.version = 1;

    VmFunction compute_v2;
    compute_v2.func_id = 1;
    compute_v2.name = "Calculator::compute";
    compute_v2.param_count = 2;
    compute_v2.reg_count = 4;
    compute_v2.ret_kind = RetKind::kI64;
    compute_v2.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_v2.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),   // r2 = this->value_
        MakeInst(OpCode::kMulI64, 3, 2, 1),           // r3 = value_ * x (CHANGED!)
        MakeInst(OpCode::kRet, 3),
    };

    PatchEntry entry;
    entry.action = PatchAction::kModified;
    entry.func_name = "Calculator::compute";
    entry.func_id = 1;
    entry.new_func = compute_v2;
    patch.entries.push_back(entry);

    pm.ApplyPatch(patch);

    // ── Test v2: multiplication ──────────────────────────────

    result = interp.Execute(1, compute_args, 2);
    std::cout << "compute(5) = " << static_cast<int64_t>(result)
              << " (expected 50, multiplication)\n";
    assert(static_cast<int64_t>(result) == 50);

    // ── Rollback ─────────────────────────────────────────────

    std::cout << "\nRolling back to v1...\n";
    pm.Rollback();

    result = interp.Execute(1, compute_args, 2);
    std::cout << "compute(5) = " << static_cast<int64_t>(result)
              << " (expected 15, addition restored)\n";
    assert(static_cast<int64_t>(result) == 15);

    // ── Apply another patch: change to subtraction ───────────

    std::cout << "\nApplying second patch: compute → subtraction...\n";

    PatchManifest patch2;
    patch2.version = 2;

    VmFunction compute_v3;
    compute_v3.func_id = 1;
    compute_v3.name = "Calculator::compute";
    compute_v3.param_count = 2;
    compute_v3.reg_count = 4;
    compute_v3.ret_kind = RetKind::kI64;
    compute_v3.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_v3.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        MakeInst(OpCode::kSubI64, 3, 2, 1),           // value_ - x
        MakeInst(OpCode::kRet, 3),
    };

    PatchEntry entry2;
    entry2.action = PatchAction::kModified;
    entry2.func_name = "Calculator::compute";
    entry2.func_id = 1;
    entry2.new_func = compute_v3;
    patch2.entries.push_back(entry2);

    pm.ApplyPatch(patch2);

    result = interp.Execute(1, compute_args, 2);
    std::cout << "compute(5) = " << static_cast<int64_t>(result)
              << " (expected 5, subtraction)\n";
    assert(static_cast<int64_t>(result) == 5);

    std::cout << "\n═══ Demo complete. All assertions passed. ═══\n";
    return 0;
}
