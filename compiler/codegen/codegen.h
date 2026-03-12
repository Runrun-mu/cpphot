#pragma once
#include "compiler/ir/ir_types.h"
#include "hotvm/bytecode.h"
#include "hotvm/module.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace hotvm {
namespace compiler {

// ── Codegen: IR → Bytecode ───────────────────────────────────
// Converts an IR module to a binary Module with bytecoded functions.

class Codegen {
public:
    Codegen();
    ~Codegen();

    // Convert entire IR module to bytecode module
    Module Generate(const ir::IRModule& ir_mod);

private:
    // Convert one IR function to a VmFunction
    VmFunction GenerateFunction(const ir::IRFunction& ir_func);

    // Register allocation: IR virtual register → VM register
    uint8_t AllocReg(const ir::IRValue& val);
    uint8_t GetReg(const ir::IRValue& val);

    // Emit instructions
    void EmitInst(OpCode op, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0,
                   uint32_t imm32 = 0, uint64_t imm64 = 0);

    // Label resolution
    uint32_t GetLabelPC(const std::string& label);
    void RecordLabel(const std::string& label, uint32_t pc);
    void PatchJumps();

    // Map ArgKind from IR type
    static ArgKind IRTypeToArgKind(ir::IRType type);
    static RetKind IRTypeToRetKind(ir::IRType type);

    // Current function being generated
    std::vector<Instruction> code_;
    std::unordered_map<uint32_t, uint8_t> reg_map_;  // IR reg_id → VM reg
    uint8_t next_vm_reg_ = 0;

    // Label → PC
    std::unordered_map<std::string, uint32_t> label_pcs_;

    // Forward references: code index → label name
    std::vector<std::pair<uint32_t, std::string>> forward_refs_;

    // Function name → func_id
    std::unordered_map<std::string, uint32_t> func_ids_;
};

}  // namespace compiler
}  // namespace hotvm
