#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace hotvm {

// ── Basic identifiers ────────────────────────────────────────

using FuncId    = uint32_t;
using WrapperId = uint32_t;
using TypeId    = uint32_t;

constexpr FuncId    kInvalidFuncId    = UINT32_MAX;
constexpr WrapperId kInvalidWrapperId = UINT32_MAX;
constexpr TypeId    kInvalidTypeId    = UINT32_MAX;

// ── Argument / value kinds ───────────────────────────────────

enum class ArgKind : uint8_t {
    kI8  = 0,
    kI16 = 1,
    kI32 = 2,
    kI64 = 3,
    kF32 = 4,
    kF64 = 5,
    kPtr = 6,
    kBool = 7,
};

enum class RetKind : uint8_t {
    kVoid = 0,
    kI64  = 1,
    kF64  = 2,
    kPtr  = 3,
};

// ── Calling convention ───────────────────────────────────────

enum class CallConv : uint8_t {
    kAapcs64 = 0,   // ARM64 (macOS / Linux)
    kSystemV = 1,    // x86_64 System V AMD64 ABI
    kWin64   = 2,    // x86_64 Windows (future)
};

// ── Dispatch mode ────────────────────────────────────────────

enum class DispatchMode : uint8_t {
    kVM     = 0,     // Execute through VM interpreter
    kNative = 1,     // Direct forward to native function
};

// ── Bind info ────────────────────────────────────────────────

struct BindInfo {
    FuncId       func_id    = kInvalidFuncId;
    DispatchMode mode       = DispatchMode::kNative;
    CallConv     call_conv  = CallConv::kSystemV;
    RetKind      ret_kind   = RetKind::kVoid;
    uint8_t      param_count = 0;
    ArgKind      param_kinds[16] = {};

    // Native function pointer (used when mode == kNative)
    void*        native_fn  = nullptr;
};

// ── Wrapper metadata ─────────────────────────────────────────

struct WrapperMeta {
    WrapperId    wrapper_id  = kInvalidWrapperId;
    BindInfo     bind_info;
    int64_t      this_adjust = 0;  // For multiple inheritance thunks
    bool         in_use      = false;
};

// ── Adapter frame (x86_64 System V ABI) ─────────────────────
// Layout must match assembly in adapter.S

#if defined(HOTVM_ARCH_x86_64)
struct AdapterFrame {
    // GPR args: rdi, rsi, rdx, rcx, r8, r9
    uint64_t gpr[6];          // offset 0,   48 bytes
    // SSE args: xmm0-xmm7 (128-bit each)
    alignas(16) uint8_t sse[8][16]; // offset 48, 128 bytes
    // Metadata
    uint64_t wrapper_id;      // offset 176
    uint64_t caller_rsp;      // offset 184
    // Return values
    uint64_t ret_rax;         // offset 192
    uint64_t ret_rdx;         // offset 200
    alignas(16) uint8_t ret_xmm0[16]; // offset 208
    alignas(16) uint8_t ret_xmm1[16]; // offset 224
    uint32_t ret_kind;        // offset 240
    uint32_t reserved;        // offset 244
};
static_assert(sizeof(AdapterFrame) == 248);

#elif defined(HOTVM_ARCH_arm64)
struct AdapterFrame {
    // GPR args: x0-x7
    uint64_t gpr[8];          // offset 0,   64 bytes
    // FP/SIMD: q0-q7 (128-bit each)
    alignas(16) uint8_t fp[8][16]; // offset 64, 128 bytes
    // Metadata
    uint64_t wrapper_id;      // offset 192
    uint64_t caller_sp;       // offset 200
    // Return values
    uint64_t ret_x0;          // offset 208
    uint64_t ret_x1;          // offset 216
    alignas(16) uint8_t ret_q0[16]; // offset 224
    alignas(16) uint8_t ret_q1[16]; // offset 240
    uint32_t ret_kind;        // offset 256
    uint32_t reserved;        // offset 260
};
static_assert(sizeof(AdapterFrame) == 264);
#else
// Generic stub for compilation on unsupported platforms
struct AdapterFrame {
    uint64_t gpr[8];
    alignas(16) uint8_t fp[8][16];
    uint64_t wrapper_id;
    uint64_t caller_sp;
    uint64_t ret_0;
    uint64_t ret_1;
    alignas(16) uint8_t ret_fp0[16];
    alignas(16) uint8_t ret_fp1[16];
    uint32_t ret_kind;
    uint32_t reserved;
};
#endif

// ── Type system (runtime type info) ──────────────────────────

struct FieldInfo {
    std::string name;
    uint32_t    offset;
    ArgKind     kind;
    TypeId      type_id;   // For class-typed fields
};

struct VMethodInfo {
    std::string name;
    uint32_t    vtable_slot;
    FuncId      func_id;
};

struct TypeInfo {
    TypeId      type_id    = kInvalidTypeId;
    std::string name;
    uint32_t    size       = 0;   // sizeof
    uint32_t    align      = 0;   // alignof
    TypeId      base_type_id = kInvalidTypeId;  // Single inheritance
    std::vector<FieldInfo>   fields;
    std::vector<VMethodInfo> vmethods;
};

// ── Unwind entry (for exception handling) ────────────────────

struct UnwindEntry {
    uint32_t try_begin;    // PC range start
    uint32_t try_end;      // PC range end
    uint32_t catch_pc;     // Catch handler PC
    uint32_t dtor_slot;    // Destructor callback slot (or UINT32_MAX)
    TypeId   catch_type;   // Exception type to catch (kInvalidTypeId = catch-all)
};

}  // namespace hotvm
