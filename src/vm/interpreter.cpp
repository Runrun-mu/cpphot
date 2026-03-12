#include "hotvm/interpreter.h"
#include "hotvm/runtime.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace hotvm {

Interpreter::Interpreter() = default;
Interpreter::~Interpreter() = default;

void Interpreter::RegisterFunction(const VmFunction& func) {
    functions_[func.func_id] = func;
}

void Interpreter::RegisterFunction(VmFunction&& func) {
    FuncId id = func.func_id;
    functions_[id] = std::move(func);
}

void Interpreter::UnregisterFunction(FuncId id) {
    functions_.erase(id);
}

bool Interpreter::HasFunction(FuncId id) const {
    return functions_.count(id) > 0;
}

const VmFunction* Interpreter::GetFunction(FuncId id) const {
    auto it = functions_.find(id);
    return it != functions_.end() ? &it->second : nullptr;
}

uint64_t Interpreter::Execute(FuncId func_id, const uint64_t* args, int arg_count) {
    auto it = functions_.find(func_id);
    if (it == functions_.end()) {
        throw std::runtime_error("VM function not found: " + std::to_string(func_id));
    }
    return ExecFunction(&it->second, args, arg_count);
}

uint64_t Interpreter::Execute(FuncId func_id, const std::vector<uint64_t>& args) {
    return Execute(func_id, args.data(), static_cast<int>(args.size()));
}

// ── Helper: reinterpret uint64 ↔ double ──────────────────────

static inline double u64_to_f64(uint64_t v) {
    double d;
    std::memcpy(&d, &v, sizeof(d));
    return d;
}

static inline uint64_t f64_to_u64(double d) {
    uint64_t v;
    std::memcpy(&v, &d, sizeof(v));
    return v;
}

// ── Core execution loop ──────────────────────────────────────

uint64_t Interpreter::ExecFunction(const VmFunction* func,
                                    const uint64_t* args, int arg_count) {
    VmFrame frame;
    frame.func = func;
    frame.pc = 0;

    // Load arguments into registers r0..rN-1
    for (int i = 0; i < arg_count && i < kMaxRegisters; ++i) {
        frame.regs[i] = args[i];
    }

    auto& regs = frame.regs;
    const auto& code = func->code;
    const int code_size = static_cast<int>(code.size());

    while (frame.pc < static_cast<uint32_t>(code_size)) {
        const Instruction& inst = code[frame.pc];
        frame.pc++;

        switch (inst.op) {

        // ── Data movement ──
        case OpCode::kNop:
            break;

        case OpCode::kLdi:
            regs[inst.a] = inst.imm64;
            break;

        case OpCode::kMov:
            regs[inst.a] = regs[inst.b];
            break;

        case OpCode::kLdF64:
            regs[inst.a] = inst.imm64;  // bit pattern stored directly
            break;

        // ── Integer arithmetic ──
        case OpCode::kAddI64:
            regs[inst.a] = regs[inst.b] + regs[inst.c];
            break;

        case OpCode::kSubI64:
            regs[inst.a] = regs[inst.b] - regs[inst.c];
            break;

        case OpCode::kMulI64:
            regs[inst.a] = static_cast<uint64_t>(
                static_cast<int64_t>(regs[inst.b]) *
                static_cast<int64_t>(regs[inst.c]));
            break;

        case OpCode::kDivI64: {
            int64_t divisor = static_cast<int64_t>(regs[inst.c]);
            if (divisor == 0) throw std::runtime_error("Division by zero");
            regs[inst.a] = static_cast<uint64_t>(
                static_cast<int64_t>(regs[inst.b]) / divisor);
            break;
        }

        case OpCode::kModI64: {
            int64_t divisor = static_cast<int64_t>(regs[inst.c]);
            if (divisor == 0) throw std::runtime_error("Modulo by zero");
            regs[inst.a] = static_cast<uint64_t>(
                static_cast<int64_t>(regs[inst.b]) % divisor);
            break;
        }

        case OpCode::kNegI64:
            regs[inst.a] = static_cast<uint64_t>(-static_cast<int64_t>(regs[inst.b]));
            break;

        // ── Float arithmetic ──
        case OpCode::kAddF64:
            regs[inst.a] = f64_to_u64(u64_to_f64(regs[inst.b]) + u64_to_f64(regs[inst.c]));
            break;

        case OpCode::kSubF64:
            regs[inst.a] = f64_to_u64(u64_to_f64(regs[inst.b]) - u64_to_f64(regs[inst.c]));
            break;

        case OpCode::kMulF64:
            regs[inst.a] = f64_to_u64(u64_to_f64(regs[inst.b]) * u64_to_f64(regs[inst.c]));
            break;

        case OpCode::kDivF64: {
            double divisor = u64_to_f64(regs[inst.c]);
            if (divisor == 0.0) throw std::runtime_error("Float division by zero");
            regs[inst.a] = f64_to_u64(u64_to_f64(regs[inst.b]) / divisor);
            break;
        }

        case OpCode::kNegF64:
            regs[inst.a] = f64_to_u64(-u64_to_f64(regs[inst.b]));
            break;

        // ── Comparison ──
        case OpCode::kCmpEq:
            regs[inst.a] = (regs[inst.b] == regs[inst.c]) ? 1ULL : 0ULL;
            break;
        case OpCode::kCmpNe:
            regs[inst.a] = (regs[inst.b] != regs[inst.c]) ? 1ULL : 0ULL;
            break;
        case OpCode::kCmpLt:
            regs[inst.a] = (static_cast<int64_t>(regs[inst.b]) <
                            static_cast<int64_t>(regs[inst.c])) ? 1ULL : 0ULL;
            break;
        case OpCode::kCmpLe:
            regs[inst.a] = (static_cast<int64_t>(regs[inst.b]) <=
                            static_cast<int64_t>(regs[inst.c])) ? 1ULL : 0ULL;
            break;
        case OpCode::kCmpGt:
            regs[inst.a] = (static_cast<int64_t>(regs[inst.b]) >
                            static_cast<int64_t>(regs[inst.c])) ? 1ULL : 0ULL;
            break;
        case OpCode::kCmpGe:
            regs[inst.a] = (static_cast<int64_t>(regs[inst.b]) >=
                            static_cast<int64_t>(regs[inst.c])) ? 1ULL : 0ULL;
            break;

        // ── Type conversion ──
        case OpCode::kI64ToF64:
            regs[inst.a] = f64_to_u64(static_cast<double>(static_cast<int64_t>(regs[inst.b])));
            break;
        case OpCode::kF64ToI64:
            regs[inst.a] = static_cast<uint64_t>(static_cast<int64_t>(u64_to_f64(regs[inst.b])));
            break;
        case OpCode::kI64ToPtr:
            regs[inst.a] = regs[inst.b];  // reinterpret
            break;
        case OpCode::kPtrToI64:
            regs[inst.a] = regs[inst.b];  // reinterpret
            break;

        // ── Memory load/store ──
        case OpCode::kLoadI64:
            std::memcpy(&regs[inst.a], reinterpret_cast<void*>(regs[inst.b]), 8);
            break;
        case OpCode::kLoadF64:
            std::memcpy(&regs[inst.a], reinterpret_cast<void*>(regs[inst.b]), 8);
            break;
        case OpCode::kLoadPtr:
            std::memcpy(&regs[inst.a], reinterpret_cast<void*>(regs[inst.b]), sizeof(void*));
            break;
        case OpCode::kStoreI64:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], 8);
            break;
        case OpCode::kStoreF64:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], 8);
            break;
        case OpCode::kStorePtr:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], sizeof(void*));
            break;
        case OpCode::kLoadField: {
            auto ptr = reinterpret_cast<uint8_t*>(regs[inst.b]) + inst.imm32;
            std::memcpy(&regs[inst.a], ptr, 8);
            break;
        }
        case OpCode::kStoreField: {
            auto ptr = reinterpret_cast<uint8_t*>(regs[inst.a]) + inst.imm32;
            std::memcpy(ptr, &regs[inst.b], 8);
            break;
        }
        case OpCode::kLoadI8: {
            int8_t v;
            std::memcpy(&v, reinterpret_cast<void*>(regs[inst.b]), 1);
            regs[inst.a] = static_cast<uint64_t>(static_cast<int64_t>(v));
            break;
        }
        case OpCode::kLoadI16: {
            int16_t v;
            std::memcpy(&v, reinterpret_cast<void*>(regs[inst.b]), 2);
            regs[inst.a] = static_cast<uint64_t>(static_cast<int64_t>(v));
            break;
        }
        case OpCode::kLoadI32: {
            int32_t v;
            std::memcpy(&v, reinterpret_cast<void*>(regs[inst.b]), 4);
            regs[inst.a] = static_cast<uint64_t>(static_cast<int64_t>(v));
            break;
        }
        case OpCode::kStoreI8:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], 1);
            break;
        case OpCode::kStoreI16:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], 2);
            break;
        case OpCode::kStoreI32:
            std::memcpy(reinterpret_cast<void*>(regs[inst.a]), &regs[inst.b], 4);
            break;

        // ── Object operations ──
        case OpCode::kAlloc:
            regs[inst.a] = reinterpret_cast<uint64_t>(std::calloc(1, inst.imm32));
            if (regs[inst.a] == 0) throw std::runtime_error("Allocation failed");
            break;

        case OpCode::kFree:
            std::free(reinterpret_cast<void*>(regs[inst.a]));
            regs[inst.a] = 0;
            break;

        case OpCode::kGetVTablePtr: {
            // Object's first qword is the vtable pointer
            void* obj = reinterpret_cast<void*>(regs[inst.b]);
            uint64_t vtable_ptr;
            std::memcpy(&vtable_ptr, obj, sizeof(uint64_t));
            regs[inst.a] = vtable_ptr;
            break;
        }

        case OpCode::kCallVirtual: {
            uint64_t result = DispatchVirtualCall(
                regs[inst.a], inst.imm32,
                &regs[inst.a], inst.b);  // b = arg count
            regs[inst.a] = result;
            break;
        }

        // ── Bitwise / logical ──
        case OpCode::kAnd:
            regs[inst.a] = regs[inst.b] & regs[inst.c];
            break;
        case OpCode::kOr:
            regs[inst.a] = regs[inst.b] | regs[inst.c];
            break;
        case OpCode::kXor:
            regs[inst.a] = regs[inst.b] ^ regs[inst.c];
            break;
        case OpCode::kNot:
            regs[inst.a] = ~regs[inst.b];
            break;
        case OpCode::kShl:
            regs[inst.a] = regs[inst.b] << (regs[inst.c] & 63);
            break;
        case OpCode::kShr:
            regs[inst.a] = static_cast<uint64_t>(
                static_cast<int64_t>(regs[inst.b]) >> (regs[inst.c] & 63));
            break;

        // ── Control flow ──
        case OpCode::kJmp:
            frame.pc = inst.imm32;
            break;

        case OpCode::kJz:
            if (regs[inst.a] == 0) frame.pc = inst.imm32;
            break;

        case OpCode::kJnz:
            if (regs[inst.a] != 0) frame.pc = inst.imm32;
            break;

        case OpCode::kCall: {
            // Call func_id=imm32, args start at reg[a], arg count = b
            FuncId target = static_cast<FuncId>(inst.imm32);
            uint64_t result = DispatchCall(target, &regs[inst.a + 1], inst.b);
            regs[inst.a] = result;
            break;
        }

        case OpCode::kRet:
            return regs[inst.a];

        case OpCode::kRetVoid:
            return 0;

        // ── Exception handling ──
        case OpCode::kThrow:
            // For now, throw as runtime_error with the value
            throw std::runtime_error("VM exception: type=" +
                std::to_string(inst.imm32) +
                " value=" + std::to_string(regs[inst.a]));

        case OpCode::kTryEnter:
            // Push unwind entry (simplified: just record catch PC)
            // Full implementation would use setjmp or C++ exception mechanism
            break;

        case OpCode::kTryExit:
            // Pop unwind entry
            break;

        // ── Native call ──
        case OpCode::kCallNative: {
            // Call native function pointer in reg[b]
            // This is a simplified version - real impl would use adapter
            auto fn = reinterpret_cast<uint64_t(*)(uint64_t)>(regs[inst.b]);
            regs[inst.a] = fn(regs[inst.a + 1]);
            break;
        }

        case OpCode::kBreakpoint:
            // Debug hook point
            break;

        default:
            throw std::runtime_error("Unknown opcode: " +
                std::to_string(static_cast<int>(inst.op)));
        }
    }

    return 0;  // Implicit return 0
}

uint64_t Interpreter::DispatchCall(FuncId target_id,
                                    const uint64_t* args, int arg_count) {
    // First, check if we have the function in VM
    auto it = functions_.find(target_id);
    if (it != functions_.end()) {
        return ExecFunction(&it->second, args, arg_count);
    }

    // Otherwise, try runtime dispatch (may go to native)
    auto& rt = Runtime::Instance();
    WrapperId wid = rt.GetWrapperForFunc(target_id);
    if (wid != kInvalidWrapperId) {
        const WrapperMeta* meta = rt.GetWrapperMeta(wid);
        if (meta && meta->bind_info.mode == DispatchMode::kNative &&
            meta->bind_info.native_fn) {
            // Direct native call (simplified: single-arg forwarding)
            // Full implementation would marshal args properly through adapter
            using NativeFn = uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t,
                                          uint64_t, uint64_t);
            auto fn = reinterpret_cast<NativeFn>(meta->bind_info.native_fn);
            uint64_t a[6] = {};
            for (int i = 0; i < arg_count && i < 6; ++i) a[i] = args[i];
            return fn(a[0], a[1], a[2], a[3], a[4], a[5]);
        }
    }

    throw std::runtime_error("Cannot dispatch call to func_id=" +
                              std::to_string(target_id));
}

uint64_t Interpreter::DispatchVirtualCall(uint64_t obj_ptr, uint32_t vtable_slot,
                                           const uint64_t* args, int arg_count) {
    // Read vtable pointer from object (first qword)
    uint64_t vtable_addr;
    std::memcpy(&vtable_addr, reinterpret_cast<void*>(obj_ptr), sizeof(uint64_t));

    // Read function pointer from vtable[slot]
    auto vtable = reinterpret_cast<uint64_t*>(vtable_addr);
    uint64_t fn_ptr = vtable[vtable_slot];

    // Try to find it as a wrapper → dispatch through runtime
    // For now, call as native function pointer
    using VirtFn = uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t);
    auto fn = reinterpret_cast<VirtFn>(fn_ptr);
    uint64_t a[4] = {};
    a[0] = obj_ptr;  // this pointer
    for (int i = 0; i < arg_count && i < 3; ++i) a[i + 1] = args[i + 1];
    return fn(a[0], a[1], a[2], a[3]);
}

}  // namespace hotvm
