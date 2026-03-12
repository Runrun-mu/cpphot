// ═══════════════════════════════════════════════════════════════
// HotVM Integration Test: VM Interpreter
//
// Tests the core VM bytecode execution without native code or
// assembly trampolines. Verifies:
// - Basic arithmetic (integer + float)
// - Comparisons and control flow
// - Memory operations (load/store/fields)
// - Function calls (VM → VM)
// - Hot-patching (replace function bytecode at runtime)
// - Rollback
// ═══════════════════════════════════════════════════════════════

#include "hotvm/bytecode.h"
#include "hotvm/interpreter.h"
#include "hotvm/runtime.h"
#include "hotvm/patch_manager.h"
#include "hotvm/module.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

using namespace hotvm;

// ── Helpers ──────────────────────────────────────────────────

static Instruction MakeInst(OpCode op, uint8_t a = 0, uint8_t b = 0,
                              uint8_t c = 0, uint32_t imm32 = 0,
                              uint64_t imm64 = 0) {
    return Instruction(op, a, b, c, imm32, imm64);
}

static uint64_t I64(int64_t v) { return static_cast<uint64_t>(v); }

static uint64_t F64Bits(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(double));
    return bits;
}

static double ToF64(uint64_t bits) {
    double v;
    std::memcpy(&v, &bits, sizeof(double));
    return v;
}

// ── Test: basic integer arithmetic ───────────────────────────

static void TestIntegerArithmetic() {
    std::cout << "Test: Integer arithmetic... ";

    Interpreter interp;

    // add(a, b) → a + b
    VmFunction add_func;
    add_func.func_id = 0;
    add_func.name = "add";
    add_func.param_count = 2;
    add_func.reg_count = 3;
    add_func.ret_kind = RetKind::kI64;
    add_func.param_kinds = {ArgKind::kI64, ArgKind::kI64};
    add_func.code = {
        MakeInst(OpCode::kAddI64, 2, 0, 1),  // r2 = r0 + r1
        MakeInst(OpCode::kRet, 2),             // return r2
    };
    interp.RegisterFunction(add_func);

    uint64_t args[] = {I64(10), I64(20)};
    uint64_t result = interp.Execute(0, args, 2);
    assert(static_cast<int64_t>(result) == 30);

    // mul(a, b) → a * b
    VmFunction mul_func;
    mul_func.func_id = 1;
    mul_func.name = "mul";
    mul_func.param_count = 2;
    mul_func.reg_count = 3;
    mul_func.ret_kind = RetKind::kI64;
    mul_func.param_kinds = {ArgKind::kI64, ArgKind::kI64};
    mul_func.code = {
        MakeInst(OpCode::kMulI64, 2, 0, 1),
        MakeInst(OpCode::kRet, 2),
    };
    interp.RegisterFunction(mul_func);

    uint64_t args2[] = {I64(6), I64(7)};
    result = interp.Execute(1, args2, 2);
    assert(static_cast<int64_t>(result) == 42);

    // div and mod
    VmFunction divmod_func;
    divmod_func.func_id = 2;
    divmod_func.name = "divmod";
    divmod_func.param_count = 2;
    divmod_func.reg_count = 5;
    divmod_func.ret_kind = RetKind::kI64;
    divmod_func.param_kinds = {ArgKind::kI64, ArgKind::kI64};
    divmod_func.code = {
        MakeInst(OpCode::kDivI64, 2, 0, 1),  // r2 = r0 / r1
        MakeInst(OpCode::kModI64, 3, 0, 1),  // r3 = r0 % r1
        MakeInst(OpCode::kAddI64, 4, 2, 3),  // r4 = r2 + r3 (quotient + remainder)
        MakeInst(OpCode::kRet, 4),
    };
    interp.RegisterFunction(divmod_func);

    uint64_t args3[] = {I64(17), I64(5)};
    result = interp.Execute(2, args3, 2);
    // 17/5=3, 17%5=2, 3+2=5
    assert(static_cast<int64_t>(result) == 5);

    std::cout << "PASSED\n";
}

// ── Test: float arithmetic ───────────────────────────────────

static void TestFloatArithmetic() {
    std::cout << "Test: Float arithmetic... ";

    Interpreter interp;

    // fadd(a, b) → a + b
    VmFunction func;
    func.func_id = 0;
    func.name = "fadd";
    func.param_count = 2;
    func.reg_count = 3;
    func.ret_kind = RetKind::kF64;
    func.param_kinds = {ArgKind::kF64, ArgKind::kF64};
    func.code = {
        MakeInst(OpCode::kAddF64, 2, 0, 1),
        MakeInst(OpCode::kRet, 2),
    };
    interp.RegisterFunction(func);

    uint64_t args[] = {F64Bits(3.14), F64Bits(2.72)};
    uint64_t result = interp.Execute(0, args, 2);
    double d = ToF64(result);
    assert(std::abs(d - 5.86) < 0.001);

    std::cout << "PASSED\n";
}

// ── Test: comparison + conditional branching ─────────────────

static void TestControlFlow() {
    std::cout << "Test: Control flow... ";

    Interpreter interp;

    // max(a, b) → a > b ? a : b
    VmFunction func;
    func.func_id = 0;
    func.name = "max";
    func.param_count = 2;
    func.reg_count = 4;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kI64, ArgKind::kI64};
    func.code = {
        // r2 = (r0 > r1)
        MakeInst(OpCode::kCmpGt, 2, 0, 1),
        // if r2 != 0, jump to 3 (return r0)
        MakeInst(OpCode::kJnz, 2, 0, 0, 3),
        // return r1
        MakeInst(OpCode::kRet, 1),
        // return r0
        MakeInst(OpCode::kRet, 0),
    };
    interp.RegisterFunction(func);

    uint64_t args1[] = {I64(10), I64(20)};
    assert(static_cast<int64_t>(interp.Execute(0, args1, 2)) == 20);

    uint64_t args2[] = {I64(30), I64(5)};
    assert(static_cast<int64_t>(interp.Execute(0, args2, 2)) == 30);

    std::cout << "PASSED\n";
}

// ── Test: loop (sum 1..N) ────────────────────────────────────

static void TestLoop() {
    std::cout << "Test: Loop (sum 1..N)... ";

    Interpreter interp;

    // sum(n) → 1 + 2 + ... + n
    VmFunction func;
    func.func_id = 0;
    func.name = "sum";
    func.param_count = 1;
    func.reg_count = 4;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kI64};
    func.code = {
        // r1 = 0 (accumulator)
        MakeInst(OpCode::kLdi, 1, 0, 0, 0, 0),
        // r2 = 1 (counter)
        MakeInst(OpCode::kLdi, 2, 0, 0, 0, 1),
        // r3 = 1 (increment)
        MakeInst(OpCode::kLdi, 3, 0, 0, 0, 1),
        // LOOP (pc=3):
        //   r1 = r1 + r2
        MakeInst(OpCode::kAddI64, 1, 1, 2),
        //   r2 = r2 + r3
        MakeInst(OpCode::kAddI64, 2, 2, 3),
        //   if r2 <= r0, jump to LOOP (pc=3)
        MakeInst(OpCode::kCmpLe, 3, 2, 0),
        MakeInst(OpCode::kJnz, 3, 0, 0, 3),
        // return r1
        MakeInst(OpCode::kRet, 1),
    };

    // Fix: r3 is reused for comparison result, need separate register
    // Actually let's use r3 for both since after increment r3 is no longer needed
    // Wait, r3=1 is used as increment too. Let me restructure.
    func.reg_count = 5;
    func.code = {
        MakeInst(OpCode::kLdi, 1, 0, 0, 0, 0),   // r1 = 0 (sum)
        MakeInst(OpCode::kLdi, 2, 0, 0, 0, 1),   // r2 = 1 (counter)
        MakeInst(OpCode::kLdi, 3, 0, 0, 0, 1),   // r3 = 1 (const)
        // LOOP (pc=3):
        MakeInst(OpCode::kAddI64, 1, 1, 2),       // r1 += r2
        MakeInst(OpCode::kAddI64, 2, 2, 3),       // r2 += 1
        MakeInst(OpCode::kCmpLe, 4, 2, 0),        // r4 = (r2 <= n)
        MakeInst(OpCode::kJnz, 4, 0, 0, 3),       // if r4, goto LOOP
        MakeInst(OpCode::kRet, 1),                  // return sum
    };
    interp.RegisterFunction(func);

    uint64_t args[] = {I64(100)};
    uint64_t result = interp.Execute(0, args, 1);
    assert(static_cast<int64_t>(result) == 5050);

    std::cout << "PASSED\n";
}

// ── Test: VM → VM function call ──────────────────────────────

static void TestVmCall() {
    std::cout << "Test: VM → VM call... ";

    Interpreter interp;

    // double_it(x) → x * 2
    VmFunction double_fn;
    double_fn.func_id = 0;
    double_fn.name = "double_it";
    double_fn.param_count = 1;
    double_fn.reg_count = 3;
    double_fn.ret_kind = RetKind::kI64;
    double_fn.param_kinds = {ArgKind::kI64};
    double_fn.code = {
        MakeInst(OpCode::kLdi, 1, 0, 0, 0, 2),
        MakeInst(OpCode::kMulI64, 2, 0, 1),
        MakeInst(OpCode::kRet, 2),
    };
    interp.RegisterFunction(double_fn);

    // quadruple(x) → double_it(double_it(x))
    VmFunction quad_fn;
    quad_fn.func_id = 1;
    quad_fn.name = "quadruple";
    quad_fn.param_count = 1;
    quad_fn.reg_count = 4;
    quad_fn.ret_kind = RetKind::kI64;
    quad_fn.param_kinds = {ArgKind::kI64};
    quad_fn.code = {
        // r1 = double_it(r0): move r0 to arg position r1+1=r2
        MakeInst(OpCode::kMov, 2, 0),              // r2 = r0 (arg for call)
        MakeInst(OpCode::kCall, 1, 1, 0, 0),       // r1 = call func_id=0, 1 arg starting at r2
        // r2 = double_it(r1): move r1 to arg position r2+1=r3
        MakeInst(OpCode::kMov, 3, 1),              // r3 = r1 (arg for call)
        MakeInst(OpCode::kCall, 2, 1, 0, 0),       // r2 = call func_id=0, 1 arg starting at r3
        MakeInst(OpCode::kRet, 2),
    };
    interp.RegisterFunction(quad_fn);

    uint64_t args[] = {I64(5)};
    uint64_t result = interp.Execute(1, args, 1);
    assert(static_cast<int64_t>(result) == 20);

    std::cout << "PASSED\n";
}

// ── Test: memory operations ──────────────────────────────────

static void TestMemoryOps() {
    std::cout << "Test: Memory operations... ";

    Interpreter interp;

    // alloc_and_store(value) → allocates 8 bytes, stores value, reads back
    VmFunction func;
    func.func_id = 0;
    func.name = "alloc_store_load";
    func.param_count = 1;
    func.reg_count = 3;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kI64};
    func.code = {
        MakeInst(OpCode::kAlloc, 1, 0, 0, 8),     // r1 = malloc(8)
        MakeInst(OpCode::kStoreI64, 1, 0),          // *r1 = r0
        MakeInst(OpCode::kLoadI64, 2, 1),           // r2 = *r1
        MakeInst(OpCode::kFree, 1),                  // free(r1)
        MakeInst(OpCode::kRet, 2),                   // return r2
    };
    interp.RegisterFunction(func);

    uint64_t args[] = {I64(42)};
    uint64_t result = interp.Execute(0, args, 1);
    assert(static_cast<int64_t>(result) == 42);

    std::cout << "PASSED\n";
}

// ── Test: field access ───────────────────────────────────────

static void TestFieldAccess() {
    std::cout << "Test: Field access... ";

    Interpreter interp;

    // Simulates a struct { int64_t a; int64_t b; }
    // get_sum(ptr) → ptr->a + ptr->b
    VmFunction func;
    func.func_id = 0;
    func.name = "get_sum";
    func.param_count = 1;
    func.reg_count = 4;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kPtr};
    func.code = {
        MakeInst(OpCode::kLoadField, 1, 0, 0, 0),  // r1 = *(r0 + 0) (field a)
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),  // r2 = *(r0 + 8) (field b)
        MakeInst(OpCode::kAddI64, 3, 1, 2),          // r3 = r1 + r2
        MakeInst(OpCode::kRet, 3),
    };
    interp.RegisterFunction(func);

    // Create a "struct" in memory
    struct TestStruct { int64_t a; int64_t b; };
    TestStruct s = {100, 200};

    uint64_t args[] = {reinterpret_cast<uint64_t>(&s)};
    uint64_t result = interp.Execute(0, args, 1);
    assert(static_cast<int64_t>(result) == 300);

    std::cout << "PASSED\n";
}

// ── Test: hot-patching + rollback ────────────────────────────

static void TestHotPatch() {
    std::cout << "Test: Hot-patching + rollback... ";

    auto& rt = Runtime::Instance();
    auto& interp = rt.GetInterpreter();
    auto& pm = PatchManager::Instance();

    // Register compute(this_ptr, x) → value_ + x (addition version)
    VmFunction compute_v1;
    compute_v1.func_id = 100;
    compute_v1.name = "Calculator::compute";
    compute_v1.param_count = 2;
    compute_v1.reg_count = 4;
    compute_v1.ret_kind = RetKind::kI64;
    compute_v1.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_v1.code = {
        // r0 = this, r1 = x
        // r2 = this->value_ (offset 0, skipping vtable ptr at offset 0 → field at offset 8)
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        // r3 = r2 + r1
        MakeInst(OpCode::kAddI64, 3, 2, 1),
        MakeInst(OpCode::kRet, 3),
    };
    interp.RegisterFunction(compute_v1);

    // Simulate Calculator object: [vtable_ptr, value_=10]
    uint64_t fake_obj[2] = {0, 10};

    // Test v1: 10 + 5 = 15
    uint64_t args[] = {reinterpret_cast<uint64_t>(fake_obj), I64(5)};
    uint64_t result = interp.Execute(100, args, 2);
    assert(static_cast<int64_t>(result) == 15);

    // Create patch: change compute to multiplication
    PatchManifest patch;
    patch.version = 1;
    patch.base_version_hash = 0;

    VmFunction compute_v2;
    compute_v2.func_id = 100;
    compute_v2.name = "Calculator::compute";
    compute_v2.param_count = 2;
    compute_v2.reg_count = 4;
    compute_v2.ret_kind = RetKind::kI64;
    compute_v2.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_v2.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        MakeInst(OpCode::kMulI64, 3, 2, 1),  // Changed: multiply instead of add
        MakeInst(OpCode::kRet, 3),
    };

    PatchEntry entry;
    entry.action = PatchAction::kModified;
    entry.func_name = "Calculator::compute";
    entry.func_id = 100;
    entry.new_func = compute_v2;
    patch.entries.push_back(entry);

    // Apply patch
    bool ok = pm.ApplyPatch(patch);
    assert(ok);

    // Test v2: 10 * 5 = 50
    result = interp.Execute(100, args, 2);
    assert(static_cast<int64_t>(result) == 50);

    // Rollback
    ok = pm.Rollback();
    assert(ok);

    // Test: back to v1: 10 + 5 = 15
    result = interp.Execute(100, args, 2);
    assert(static_cast<int64_t>(result) == 15);

    std::cout << "PASSED\n";
}

// ── Test: module serialization ───────────────────────────────

static void TestModuleSerialization() {
    std::cout << "Test: Module serialization... ";

    Module mod;
    mod.header.magic = kModuleMagic;
    mod.header.version = kModuleVersion;

    // Add a type
    TypeInfo ti;
    ti.type_id = 0;
    ti.name = "Calculator";
    ti.size = 16;
    ti.align = 8;
    FieldInfo fi;
    fi.name = "value_";
    fi.offset = 8;
    fi.kind = ArgKind::kI64;
    fi.type_id = kInvalidTypeId;
    ti.fields.push_back(fi);
    VMethodInfo vmi;
    vmi.name = "compute";
    vmi.vtable_slot = 0;
    vmi.func_id = 0;
    ti.vmethods.push_back(vmi);
    mod.types.push_back(ti);
    mod.header.type_count = 1;

    // Add a function
    VmFunction func;
    func.func_id = 0;
    func.name = "Calculator::compute";
    func.param_count = 2;
    func.reg_count = 4;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    func.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        MakeInst(OpCode::kAddI64, 3, 2, 1),
        MakeInst(OpCode::kRet, 3),
    };
    mod.functions.push_back(func);
    mod.header.func_count = 1;

    // Write
    std::string path = "test_output.hotmod";
    bool ok = WriteModule(path, mod);
    assert(ok);

    // Read back
    Module loaded = ReadModule(path);
    assert(loaded.header.magic == kModuleMagic);
    assert(loaded.types.size() == 1);
    assert(loaded.functions.size() == 1);
    assert(loaded.types[0].name == "Calculator");
    assert(loaded.functions[0].name == "Calculator::compute");
    assert(loaded.functions[0].code.size() == 3);

    // Verify bytecode matches
    assert(loaded.functions[0].code[0].op == OpCode::kLoadField);
    assert(loaded.functions[0].code[1].op == OpCode::kAddI64);
    assert(loaded.functions[0].code[2].op == OpCode::kRet);

    // Cleanup
    std::remove(path.c_str());

    std::cout << "PASSED\n";
}

// ── Test: bitwise operations ─────────────────────────────────

static void TestBitwiseOps() {
    std::cout << "Test: Bitwise operations... ";

    Interpreter interp;

    VmFunction func;
    func.func_id = 0;
    func.name = "bitops";
    func.param_count = 2;
    func.reg_count = 8;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kI64, ArgKind::kI64};
    func.code = {
        MakeInst(OpCode::kAnd, 2, 0, 1),    // r2 = r0 & r1
        MakeInst(OpCode::kOr, 3, 0, 1),     // r3 = r0 | r1
        MakeInst(OpCode::kXor, 4, 0, 1),    // r4 = r0 ^ r1
        MakeInst(OpCode::kAddI64, 5, 2, 3), // r5 = r2 + r3
        MakeInst(OpCode::kAddI64, 6, 5, 4), // r6 = r5 + r4
        MakeInst(OpCode::kRet, 6),
    };
    interp.RegisterFunction(func);

    // 0xFF & 0x0F = 0x0F, 0xFF | 0x0F = 0xFF, 0xFF ^ 0x0F = 0xF0
    // 0x0F + 0xFF + 0xF0 = 15 + 255 + 240 = 510
    uint64_t args[] = {I64(0xFF), I64(0x0F)};
    uint64_t result = interp.Execute(0, args, 2);
    assert(static_cast<int64_t>(result) == 510);

    std::cout << "PASSED\n";
}

// ── Test: type conversion ────────────────────────────────────

static void TestTypeConversion() {
    std::cout << "Test: Type conversion... ";

    Interpreter interp;

    // int_to_float_back(x) → (int64_t)(double)x
    VmFunction func;
    func.func_id = 0;
    func.name = "convert";
    func.param_count = 1;
    func.reg_count = 3;
    func.ret_kind = RetKind::kI64;
    func.param_kinds = {ArgKind::kI64};
    func.code = {
        MakeInst(OpCode::kI64ToF64, 1, 0),   // r1 = (double)r0
        MakeInst(OpCode::kF64ToI64, 2, 1),   // r2 = (int64_t)r1
        MakeInst(OpCode::kRet, 2),
    };
    interp.RegisterFunction(func);

    uint64_t args[] = {I64(42)};
    uint64_t result = interp.Execute(0, args, 1);
    assert(static_cast<int64_t>(result) == 42);

    std::cout << "PASSED\n";
}

// ── Main ─────────────────────────────────────────────────────

int main() {
    std::cout << "═══ HotVM Integration Tests ═══\n\n";

    TestIntegerArithmetic();
    TestFloatArithmetic();
    TestControlFlow();
    TestLoop();
    TestVmCall();
    TestMemoryOps();
    TestFieldAccess();
    TestBitwiseOps();
    TestTypeConversion();
    TestModuleSerialization();
    TestHotPatch();

    std::cout << "\n═══ All tests PASSED ═══\n";
    return 0;
}
