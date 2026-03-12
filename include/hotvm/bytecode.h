#pragma once
#include "hotvm/types.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace hotvm {

// ── OpCode ───────────────────────────────────────────────────
// Register-machine design: most instructions are reg[a] = reg[b] op reg[c]
// or reg[a] = reg[b] op imm

enum class OpCode : uint8_t {
    // ── Data movement ──
    kNop        = 0x00,
    kLdi        = 0x01,  // reg[a] = imm64                    (load immediate)
    kMov        = 0x02,  // reg[a] = reg[b]
    kLdF64      = 0x03,  // reg[a] = imm_f64                  (load float immediate)

    // ── Integer arithmetic ──
    kAddI64     = 0x10,  // reg[a] = reg[b] + reg[c]
    kSubI64     = 0x11,  // reg[a] = reg[b] - reg[c]
    kMulI64     = 0x12,  // reg[a] = reg[b] * reg[c]
    kDivI64     = 0x13,  // reg[a] = reg[b] / reg[c]
    kModI64     = 0x14,  // reg[a] = reg[b] % reg[c]
    kNegI64     = 0x15,  // reg[a] = -reg[b]

    // ── Float arithmetic ──
    kAddF64     = 0x20,  // freg[a] = freg[b] + freg[c]
    kSubF64     = 0x21,  // freg[a] = freg[b] - freg[c]
    kMulF64     = 0x22,  // freg[a] = freg[b] * freg[c]
    kDivF64     = 0x23,  // freg[a] = freg[b] / freg[c]
    kNegF64     = 0x24,  // freg[a] = -freg[b]

    // ── Comparison (result → integer reg) ──
    kCmpEq      = 0x30,  // reg[a] = (reg[b] == reg[c]) ? 1 : 0
    kCmpNe      = 0x31,  // reg[a] = (reg[b] != reg[c]) ? 1 : 0
    kCmpLt      = 0x32,  // reg[a] = (reg[b] <  reg[c]) ? 1 : 0
    kCmpLe      = 0x33,  // reg[a] = (reg[b] <= reg[c]) ? 1 : 0
    kCmpGt      = 0x34,  // reg[a] = (reg[b] >  reg[c]) ? 1 : 0
    kCmpGe      = 0x35,  // reg[a] = (reg[b] >= reg[c]) ? 1 : 0

    // ── Type conversion ──
    kI64ToF64   = 0x40,  // freg[a] = (double)reg[b]
    kF64ToI64   = 0x41,  // reg[a] = (int64_t)freg[b]
    kI64ToPtr   = 0x42,  // reg[a] = (void*)reg[b]            (reinterpret)
    kPtrToI64   = 0x43,  // reg[a] = (int64_t)reg[b]          (reinterpret)

    // ── Memory load/store ──
    kLoadI64    = 0x50,  // reg[a] = *(int64_t*)reg[b]
    kLoadF64    = 0x51,  // freg[a] = *(double*)reg[b]
    kLoadPtr    = 0x52,  // reg[a] = *(void**)reg[b]
    kStoreI64   = 0x53,  // *(int64_t*)reg[a] = reg[b]
    kStoreF64   = 0x54,  // *(double*)reg[a]  = freg[b]
    kStorePtr   = 0x55,  // *(void**)reg[a]   = reg[b]
    kLoadField  = 0x56,  // reg[a] = *(reg[b] + imm)          (field access)
    kStoreField = 0x57,  // *(reg[a] + imm) = reg[b]          (field store)
    kLoadI8     = 0x58,  // reg[a] = *(int8_t*)reg[b]         (sign-extended)
    kLoadI16    = 0x59,  // reg[a] = *(int16_t*)reg[b]
    kLoadI32    = 0x5A,  // reg[a] = *(int32_t*)reg[b]
    kStoreI8    = 0x5B,  // *(int8_t*)reg[a]  = reg[b]
    kStoreI16   = 0x5C,  // *(int16_t*)reg[a] = reg[b]
    kStoreI32   = 0x5D,  // *(int32_t*)reg[a] = reg[b]

    // ── Object operations ──
    kAlloc      = 0x60,  // reg[a] = malloc(imm)
    kFree       = 0x61,  // free(reg[a])
    kGetVTablePtr = 0x62, // reg[a] = *(void**)reg[b]          (first qword = vtable ptr)
    kCallVirtual  = 0x63, // call vtable[imm] on object reg[a], args in reg[a+1..]

    // ── Bitwise / logical ──
    kAnd        = 0x70,  // reg[a] = reg[b] & reg[c]
    kOr         = 0x71,  // reg[a] = reg[b] | reg[c]
    kXor        = 0x72,  // reg[a] = reg[b] ^ reg[c]
    kNot        = 0x73,  // reg[a] = ~reg[b]
    kShl        = 0x74,  // reg[a] = reg[b] << reg[c]
    kShr        = 0x75,  // reg[a] = reg[b] >> reg[c]  (arithmetic)

    // ── Control flow ──
    kJmp        = 0x80,  // pc = imm
    kJz         = 0x81,  // if reg[a] == 0 then pc = imm
    kJnz        = 0x82,  // if reg[a] != 0 then pc = imm
    kCall       = 0x83,  // call func_id=imm, args in reg[a..a+N-1], result → reg[a]
    kRet        = 0x84,  // return reg[a]
    kRetVoid    = 0x85,  // return void

    // ── Exception handling ──
    kThrow      = 0x90,  // throw reg[a] (type in imm)
    kTryEnter   = 0x91,  // begin try block, catch at imm
    kTryExit    = 0x92,  // end try block

    // ── Misc ──
    kCallNative = 0xA0,  // call native fn pointer in reg[b], args in reg[a..], result → reg[a]
    kBreakpoint = 0xFF,  // debug breakpoint
};

// ── Instruction encoding ─────────────────────────────────────
// Fixed 16-byte instruction for simplicity and alignment.
//
// Layout:
//   [0]    opcode   (1 byte)
//   [1]    reg_a    (1 byte)
//   [2]    reg_b    (1 byte)
//   [3]    reg_c    (1 byte)
//   [4..7] imm32    (4 bytes)  - small immediate / offset / func_id
//   [8..15] imm64   (8 bytes)  - large immediate / pointer / f64

struct Instruction {
    OpCode   op;
    uint8_t  a;     // destination register (or first operand)
    uint8_t  b;     // second operand register
    uint8_t  c;     // third operand register
    uint32_t imm32; // small immediate
    uint64_t imm64; // large immediate

    Instruction()
        : op(OpCode::kNop), a(0), b(0), c(0), imm32(0), imm64(0) {}

    Instruction(OpCode op_, uint8_t a_, uint8_t b_, uint8_t c_,
                uint32_t imm32_ = 0, uint64_t imm64_ = 0)
        : op(op_), a(a_), b(b_), c(c_), imm32(imm32_), imm64(imm64_) {}

    // Convenience: get imm64 as double
    double as_f64() const {
        double v;
        std::memcpy(&v, &imm64, sizeof(double));
        return v;
    }

    // Convenience: set imm64 from double
    void set_f64(double v) {
        std::memcpy(&imm64, &v, sizeof(double));
    }
};

static_assert(sizeof(Instruction) == 16, "Instruction must be 16 bytes");

// ── VM function descriptor ───────────────────────────────────

struct VmFunction {
    FuncId                   func_id    = kInvalidFuncId;
    std::string              name;
    uint16_t                 reg_count  = 0;
    uint16_t                 param_count = 0;
    std::vector<ArgKind>     param_kinds;
    RetKind                  ret_kind   = RetKind::kVoid;
    std::vector<Instruction> code;
    std::vector<UnwindEntry> unwind_entries;
};

}  // namespace hotvm
