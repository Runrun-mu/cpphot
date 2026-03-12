#include "compiler/codegen/codegen.h"
#include <cstring>
#include <stdexcept>

namespace hotvm {
namespace compiler {

Codegen::Codegen() = default;
Codegen::~Codegen() = default;

// ── Type mapping ─────────────────────────────────────────────

ArgKind Codegen::IRTypeToArgKind(ir::IRType type) {
    switch (type) {
    case ir::IRType::kI8:    return ArgKind::kI8;
    case ir::IRType::kI16:   return ArgKind::kI16;
    case ir::IRType::kI32:   return ArgKind::kI32;
    case ir::IRType::kI64:   return ArgKind::kI64;
    case ir::IRType::kF32:   return ArgKind::kF32;
    case ir::IRType::kF64:   return ArgKind::kF64;
    case ir::IRType::kPtr:   return ArgKind::kPtr;
    case ir::IRType::kBool:  return ArgKind::kBool;
    default:                 return ArgKind::kI64;
    }
}

RetKind Codegen::IRTypeToRetKind(ir::IRType type) {
    switch (type) {
    case ir::IRType::kVoid:  return RetKind::kVoid;
    case ir::IRType::kF32:
    case ir::IRType::kF64:   return RetKind::kF64;
    case ir::IRType::kPtr:   return RetKind::kPtr;
    default:                 return RetKind::kI64;
    }
}

// ── Register allocation ──────────────────────────────────────

uint8_t Codegen::AllocReg(const ir::IRValue& val) {
    if (!val.isReg()) return 0;
    auto it = reg_map_.find(val.reg_id);
    if (it != reg_map_.end()) return it->second;
    uint8_t r = next_vm_reg_++;
    reg_map_[val.reg_id] = r;
    return r;
}

uint8_t Codegen::GetReg(const ir::IRValue& val) {
    if (!val.isReg()) return 0;
    auto it = reg_map_.find(val.reg_id);
    if (it != reg_map_.end()) return it->second;
    // Auto-allocate
    return AllocReg(val);
}

// ── Instruction emission ─────────────────────────────────────

void Codegen::EmitInst(OpCode op, uint8_t a, uint8_t b, uint8_t c,
                         uint32_t imm32, uint64_t imm64) {
    code_.emplace_back(op, a, b, c, imm32, imm64);
}

void Codegen::RecordLabel(const std::string& label, uint32_t pc) {
    label_pcs_[label] = pc;
}

uint32_t Codegen::GetLabelPC(const std::string& label) {
    auto it = label_pcs_.find(label);
    if (it != label_pcs_.end()) return it->second;
    return UINT32_MAX;
}

void Codegen::PatchJumps() {
    for (auto& [code_idx, label] : forward_refs_) {
        uint32_t target_pc = GetLabelPC(label);
        if (target_pc != UINT32_MAX) {
            code_[code_idx].imm32 = target_pc;
        }
    }
    forward_refs_.clear();
}

// ── Module generation ────────────────────────────────────────

Module Codegen::Generate(const ir::IRModule& ir_mod) {
    Module mod;
    mod.header.magic = kModuleMagic;
    mod.header.version = kModuleVersion;

    // Copy types
    mod.types = ir_mod.types;
    mod.header.type_count = static_cast<uint32_t>(mod.types.size());

    // Build function ID map
    for (auto& ir_func : ir_mod.functions) {
        func_ids_[ir_func.mangled_name] = ir_func.func_id;
    }

    // Generate each function
    for (auto& ir_func : ir_mod.functions) {
        mod.functions.push_back(GenerateFunction(ir_func));
    }
    mod.header.func_count = static_cast<uint32_t>(mod.functions.size());

    return mod;
}

// ── Function generation ──────────────────────────────────────

VmFunction Codegen::GenerateFunction(const ir::IRFunction& ir_func) {
    // Reset per-function state
    code_.clear();
    reg_map_.clear();
    next_vm_reg_ = 0;
    label_pcs_.clear();
    forward_refs_.clear();

    VmFunction vm_func;
    vm_func.func_id = ir_func.func_id;
    vm_func.name = ir_func.mangled_name;
    vm_func.param_count = static_cast<uint16_t>(ir_func.params.size());
    vm_func.ret_kind = IRTypeToRetKind(ir_func.return_type);

    // Map parameters to registers 0..N-1
    for (uint32_t i = 0; i < ir_func.params.size(); ++i) {
        reg_map_[i] = static_cast<uint8_t>(i);
        vm_func.param_kinds.push_back(IRTypeToArgKind(ir_func.params[i].type));
    }
    next_vm_reg_ = static_cast<uint8_t>(ir_func.params.size());

    // Lower each IR instruction to bytecode
    for (auto& ir_inst : ir_func.body) {
        switch (ir_inst.op) {

        // ── Data movement ──
        case ir::IROpCode::kLoadImm: {
            uint8_t dest = AllocReg(ir_inst.dest);
            EmitInst(OpCode::kLdi, dest, 0, 0, 0,
                     static_cast<uint64_t>(ir_inst.operands[0].imm_i64));
            break;
        }
        case ir::IROpCode::kLoadImmF: {
            uint8_t dest = AllocReg(ir_inst.dest);
            Instruction inst(OpCode::kLdF64, dest, 0, 0);
            inst.set_f64(ir_inst.operands[0].imm_f64);
            code_.push_back(inst);
            break;
        }
        case ir::IROpCode::kMov: {
            uint8_t dest = AllocReg(ir_inst.dest);
            uint8_t src = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kMov, dest, src);
            break;
        }

        // ── Integer arithmetic ──
        case ir::IROpCode::kAdd: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kAddI64, d, b, c);
            break;
        }
        case ir::IROpCode::kSub: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kSubI64, d, b, c);
            break;
        }
        case ir::IROpCode::kMul: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kMulI64, d, b, c);
            break;
        }
        case ir::IROpCode::kDiv: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kDivI64, d, b, c);
            break;
        }
        case ir::IROpCode::kMod: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kModI64, d, b, c);
            break;
        }
        case ir::IROpCode::kNeg: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kNegI64, d, b);
            break;
        }

        // ── Float arithmetic ──
        case ir::IROpCode::kFAdd: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kAddF64, d, b, c);
            break;
        }
        case ir::IROpCode::kFSub: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kSubF64, d, b, c);
            break;
        }
        case ir::IROpCode::kFMul: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kMulF64, d, b, c);
            break;
        }
        case ir::IROpCode::kFDiv: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kDivF64, d, b, c);
            break;
        }
        case ir::IROpCode::kFNeg: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kNegF64, d, b);
            break;
        }

        // ── Comparison ──
        case ir::IROpCode::kCmpEq: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpEq, d, b, c);
            break;
        }
        case ir::IROpCode::kCmpNe: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpNe, d, b, c);
            break;
        }
        case ir::IROpCode::kCmpLt: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpLt, d, b, c);
            break;
        }
        case ir::IROpCode::kCmpLe: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpLe, d, b, c);
            break;
        }
        case ir::IROpCode::kCmpGt: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpGt, d, b, c);
            break;
        }
        case ir::IROpCode::kCmpGe: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kCmpGe, d, b, c);
            break;
        }

        // ── Bitwise ──
        case ir::IROpCode::kAnd: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kAnd, d, b, c);
            break;
        }
        case ir::IROpCode::kOr: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kOr, d, b, c);
            break;
        }
        case ir::IROpCode::kXor: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kXor, d, b, c);
            break;
        }
        case ir::IROpCode::kNot: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kNot, d, b);
            break;
        }
        case ir::IROpCode::kShl: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kShl, d, b, c);
            break;
        }
        case ir::IROpCode::kShr: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            uint8_t c = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kShr, d, b, c);
            break;
        }

        // ── Memory ──
        case ir::IROpCode::kLoad: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kLoadI64, d, b);
            break;
        }
        case ir::IROpCode::kStore: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            uint8_t b = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kStoreI64, a, b);
            break;
        }
        case ir::IROpCode::kLoadField:
        case ir::IROpCode::kGetField: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kLoadField, d, b, 0, ir_inst.field_offset);
            break;
        }
        case ir::IROpCode::kStoreField:
        case ir::IROpCode::kSetField: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            uint8_t b = GetReg(ir_inst.operands[1]);
            EmitInst(OpCode::kStoreField, a, b, 0, ir_inst.field_offset);
            break;
        }
        case ir::IROpCode::kAlloc: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint32_t size = static_cast<uint32_t>(ir_inst.operands[0].imm_i64);
            EmitInst(OpCode::kAlloc, d, 0, 0, size);
            break;
        }
        case ir::IROpCode::kFree: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kFree, a);
            break;
        }

        // ── Conversion ──
        case ir::IROpCode::kI64ToF64: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kI64ToF64, d, b);
            break;
        }
        case ir::IROpCode::kF64ToI64: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kF64ToI64, d, b);
            break;
        }
        case ir::IROpCode::kBitcast: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kMov, d, b);
            break;
        }

        // ── Control flow ──
        case ir::IROpCode::kLabel:
            RecordLabel(ir_inst.label, static_cast<uint32_t>(code_.size()));
            break;

        case ir::IROpCode::kJmp: {
            uint32_t target = GetLabelPC(ir_inst.label);
            if (target == UINT32_MAX) {
                forward_refs_.push_back({static_cast<uint32_t>(code_.size()),
                                          ir_inst.label});
            }
            EmitInst(OpCode::kJmp, 0, 0, 0, target);
            break;
        }
        case ir::IROpCode::kBranchIf: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            uint32_t target = GetLabelPC(ir_inst.label);
            if (target == UINT32_MAX) {
                forward_refs_.push_back({static_cast<uint32_t>(code_.size()),
                                          ir_inst.label});
            }
            EmitInst(OpCode::kJnz, a, 0, 0, target);
            break;
        }
        case ir::IROpCode::kBranchIfNot: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            uint32_t target = GetLabelPC(ir_inst.label);
            if (target == UINT32_MAX) {
                forward_refs_.push_back({static_cast<uint32_t>(code_.size()),
                                          ir_inst.label});
            }
            EmitInst(OpCode::kJz, a, 0, 0, target);
            break;
        }

        case ir::IROpCode::kCall: {
            uint8_t d = AllocReg(ir_inst.dest);
            // Resolve function ID
            uint32_t target_id = UINT32_MAX;
            auto it = func_ids_.find(ir_inst.func_name);
            if (it != func_ids_.end()) target_id = it->second;

            // Place args in consecutive registers starting at d+1
            uint8_t arg_count = 0;
            for (int i = 0; i < 3; ++i) {
                if (!ir_inst.operands[i].isNone()) {
                    uint8_t src = GetReg(ir_inst.operands[i]);
                    if (src != d + 1 + i) {
                        EmitInst(OpCode::kMov, d + 1 + i, src);
                    }
                    arg_count++;
                }
            }
            EmitInst(OpCode::kCall, d, arg_count, 0, target_id);
            break;
        }

        case ir::IROpCode::kCallVirtual: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t obj = GetReg(ir_inst.operands[0]);
            uint8_t arg_count = 0;
            for (int i = 1; i < 3; ++i) {
                if (!ir_inst.operands[i].isNone()) arg_count++;
            }
            EmitInst(OpCode::kCallVirtual, obj, arg_count, 0, ir_inst.field_offset);
            if (obj != d) EmitInst(OpCode::kMov, d, obj);
            break;
        }

        case ir::IROpCode::kRet: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kRet, a);
            break;
        }
        case ir::IROpCode::kRetVoid:
            EmitInst(OpCode::kRetVoid);
            break;

        // ── Object ──
        case ir::IROpCode::kGetVTablePtr: {
            uint8_t d = AllocReg(ir_inst.dest);
            uint8_t b = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kGetVTablePtr, d, b);
            break;
        }

        // ── Exception ──
        case ir::IROpCode::kThrow: {
            uint8_t a = GetReg(ir_inst.operands[0]);
            EmitInst(OpCode::kThrow, a, 0, 0, ir_inst.type_id);
            break;
        }
        case ir::IROpCode::kTryBegin:
            EmitInst(OpCode::kTryEnter);
            break;
        case ir::IROpCode::kTryEnd:
            EmitInst(OpCode::kTryExit);
            break;

        default:
            // Unknown IR op - emit NOP
            EmitInst(OpCode::kNop);
            break;
        }
    }

    // Patch forward jump references
    PatchJumps();

    vm_func.code = std::move(code_);
    vm_func.reg_count = next_vm_reg_;

    return vm_func;
}

}  // namespace compiler
}  // namespace hotvm
