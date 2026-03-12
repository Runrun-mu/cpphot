#pragma once
#include "hotvm/types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace hotvm {
namespace ir {

// ── IR Value ─────────────────────────────────────────────────

enum class IRValueKind : uint8_t {
    kNone = 0,
    kReg,          // Virtual register
    kImm64,        // 64-bit integer immediate
    kImmF64,       // Double immediate
    kGlobal,       // Global variable reference
    kLabel,        // Basic block label
};

struct IRValue {
    IRValueKind kind = IRValueKind::kNone;
    uint32_t    reg_id = 0;        // For kReg
    int64_t     imm_i64 = 0;      // For kImm64
    double      imm_f64 = 0.0;    // For kImmF64
    std::string name;              // For kGlobal / kLabel

    static IRValue Reg(uint32_t id) {
        IRValue v; v.kind = IRValueKind::kReg; v.reg_id = id; return v;
    }
    static IRValue Imm(int64_t val) {
        IRValue v; v.kind = IRValueKind::kImm64; v.imm_i64 = val; return v;
    }
    static IRValue ImmF(double val) {
        IRValue v; v.kind = IRValueKind::kImmF64; v.imm_f64 = val; return v;
    }
    static IRValue Label(const std::string& lbl) {
        IRValue v; v.kind = IRValueKind::kLabel; v.name = lbl; return v;
    }
    static IRValue None() { return IRValue{}; }

    bool isReg()   const { return kind == IRValueKind::kReg; }
    bool isImm()   const { return kind == IRValueKind::kImm64; }
    bool isImmF()  const { return kind == IRValueKind::kImmF64; }
    bool isLabel() const { return kind == IRValueKind::kLabel; }
    bool isNone()  const { return kind == IRValueKind::kNone; }
};

// ── IR OpCode ────────────────────────────────────────────────

enum class IROpCode : uint8_t {
    // Arithmetic
    kAdd, kSub, kMul, kDiv, kMod, kNeg,
    kFAdd, kFSub, kFMul, kFDiv, kFNeg,

    // Comparison
    kCmpEq, kCmpNe, kCmpLt, kCmpLe, kCmpGt, kCmpGe,

    // Bitwise
    kAnd, kOr, kXor, kNot, kShl, kShr,

    // Memory
    kLoad, kStore,
    kLoadField, kStoreField,     // obj.field access
    kAlloc, kFree,

    // Conversion
    kI64ToF64, kF64ToI64,
    kBitcast,                    // reinterpret cast

    // Data movement
    kMov, kLoadImm, kLoadImmF,

    // Control flow
    kJmp, kBranchIf, kBranchIfNot,
    kCall, kCallVirtual, kCallNative,
    kRet, kRetVoid,

    // Object
    kGetField, kSetField,
    kGetVTablePtr,

    // Labels / phi
    kLabel,

    // Exception handling
    kThrow, kTryBegin, kTryEnd,
};

// ── IR Type ──────────────────────────────────────────────────

enum class IRType : uint8_t {
    kVoid = 0,
    kI8, kI16, kI32, kI64,
    kF32, kF64,
    kPtr,
    kBool,
    kStruct,  // type_id needed
};

// ── IR Instruction ───────────────────────────────────────────

struct IRInst {
    IROpCode  op;
    IRValue   dest;
    IRValue   operands[3];
    IRType    type = IRType::kVoid;
    uint32_t  type_id = UINT32_MAX;  // For struct types
    uint32_t  field_offset = 0;      // For field access
    std::string label;               // For kLabel / branch targets
    std::string func_name;           // For kCall

    IRInst() : op(IROpCode::kMov) {}
    IRInst(IROpCode op_) : op(op_) {}
};

// ── IR Parameter ─────────────────────────────────────────────

struct IRParam {
    std::string name;
    IRType      type;
    uint32_t    type_id = UINT32_MAX;
};

// ── IR Function ──────────────────────────────────────────────

struct IRFunction {
    std::string mangled_name;
    uint32_t    func_id = 0;
    std::vector<IRParam> params;
    IRType      return_type = IRType::kVoid;
    uint32_t    return_type_id = UINT32_MAX;
    std::vector<IRInst> body;
    bool        is_virtual = false;
    uint32_t    vtable_slot = UINT32_MAX;
    uint32_t    next_reg = 0;

    IRValue NewReg() { return IRValue::Reg(next_reg++); }
};

// ── IR Global ────────────────────────────────────────────────

struct GlobalVar {
    std::string name;
    IRType      type;
    uint32_t    type_id = UINT32_MAX;
    uint64_t    init_value = 0;
};

// ── IR Module ────────────────────────────────────────────────

struct IRModule {
    std::string source_file;
    std::vector<TypeInfo> types;
    std::vector<IRFunction> functions;
    std::vector<GlobalVar> globals;
};

}  // namespace ir
}  // namespace hotvm
