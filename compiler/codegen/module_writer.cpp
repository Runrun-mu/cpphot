#include "compiler/codegen/module_writer.h"
#include <iomanip>
#include <iostream>

namespace hotvm {
namespace compiler {

bool ModuleWriter::Write(const std::string& path, const Module& mod,
                           const Options& opts) {
    if (opts.dump_bytecode) {
        DumpBytecode(std::cout, mod);
    }
    return WriteModule(path, mod);
}

const char* ModuleWriter::OpCodeName(OpCode op) {
    switch (op) {
    case OpCode::kNop:        return "NOP";
    case OpCode::kLdi:        return "LDI";
    case OpCode::kMov:        return "MOV";
    case OpCode::kLdF64:      return "LDF64";
    case OpCode::kAddI64:     return "ADD_I64";
    case OpCode::kSubI64:     return "SUB_I64";
    case OpCode::kMulI64:     return "MUL_I64";
    case OpCode::kDivI64:     return "DIV_I64";
    case OpCode::kModI64:     return "MOD_I64";
    case OpCode::kNegI64:     return "NEG_I64";
    case OpCode::kAddF64:     return "ADD_F64";
    case OpCode::kSubF64:     return "SUB_F64";
    case OpCode::kMulF64:     return "MUL_F64";
    case OpCode::kDivF64:     return "DIV_F64";
    case OpCode::kNegF64:     return "NEG_F64";
    case OpCode::kCmpEq:      return "CMP_EQ";
    case OpCode::kCmpNe:      return "CMP_NE";
    case OpCode::kCmpLt:      return "CMP_LT";
    case OpCode::kCmpLe:      return "CMP_LE";
    case OpCode::kCmpGt:      return "CMP_GT";
    case OpCode::kCmpGe:      return "CMP_GE";
    case OpCode::kI64ToF64:   return "I64_TO_F64";
    case OpCode::kF64ToI64:   return "F64_TO_I64";
    case OpCode::kI64ToPtr:   return "I64_TO_PTR";
    case OpCode::kPtrToI64:   return "PTR_TO_I64";
    case OpCode::kLoadI64:    return "LOAD_I64";
    case OpCode::kLoadF64:    return "LOAD_F64";
    case OpCode::kLoadPtr:    return "LOAD_PTR";
    case OpCode::kStoreI64:   return "STORE_I64";
    case OpCode::kStoreF64:   return "STORE_F64";
    case OpCode::kStorePtr:   return "STORE_PTR";
    case OpCode::kLoadField:  return "LOAD_FIELD";
    case OpCode::kStoreField: return "STORE_FIELD";
    case OpCode::kLoadI8:     return "LOAD_I8";
    case OpCode::kLoadI16:    return "LOAD_I16";
    case OpCode::kLoadI32:    return "LOAD_I32";
    case OpCode::kStoreI8:    return "STORE_I8";
    case OpCode::kStoreI16:   return "STORE_I16";
    case OpCode::kStoreI32:   return "STORE_I32";
    case OpCode::kAlloc:      return "ALLOC";
    case OpCode::kFree:       return "FREE";
    case OpCode::kGetVTablePtr: return "GET_VTABLE";
    case OpCode::kCallVirtual:  return "CALL_VIRT";
    case OpCode::kAnd:        return "AND";
    case OpCode::kOr:         return "OR";
    case OpCode::kXor:        return "XOR";
    case OpCode::kNot:        return "NOT";
    case OpCode::kShl:        return "SHL";
    case OpCode::kShr:        return "SHR";
    case OpCode::kJmp:        return "JMP";
    case OpCode::kJz:         return "JZ";
    case OpCode::kJnz:        return "JNZ";
    case OpCode::kCall:       return "CALL";
    case OpCode::kRet:        return "RET";
    case OpCode::kRetVoid:    return "RET_VOID";
    case OpCode::kThrow:      return "THROW";
    case OpCode::kTryEnter:   return "TRY_ENTER";
    case OpCode::kTryExit:    return "TRY_EXIT";
    case OpCode::kCallNative: return "CALL_NATIVE";
    case OpCode::kBreakpoint: return "BREAKPOINT";
    default:                  return "UNKNOWN";
    }
}

void ModuleWriter::DumpFunction(std::ostream& os, const VmFunction& func) {
    os << "function " << func.name
       << " (id=" << func.func_id
       << ", regs=" << func.reg_count
       << ", params=" << func.param_count << ")\n";

    for (size_t i = 0; i < func.code.size(); ++i) {
        auto& inst = func.code[i];
        os << "  " << std::setw(4) << i << ": "
           << std::setw(12) << std::left << OpCodeName(inst.op)
           << " r" << (int)inst.a;

        if (inst.b || inst.c) {
            os << ", r" << (int)inst.b;
        }
        if (inst.c) {
            os << ", r" << (int)inst.c;
        }

        if (inst.imm32) {
            os << "  imm32=" << inst.imm32;
        }
        if (inst.imm64) {
            os << "  imm64=" << inst.imm64;
        }

        os << "\n";
    }
    os << "\n";
}

void ModuleWriter::DumpBytecode(std::ostream& os, const Module& mod) {
    os << "=== HotVM Module ===\n";
    os << "Types: " << mod.types.size() << "\n";
    os << "Functions: " << mod.functions.size() << "\n\n";

    for (auto& t : mod.types) {
        os << "type " << t.name << " (id=" << t.type_id
           << ", size=" << t.size << ", align=" << t.align << ")\n";
        for (auto& f : t.fields) {
            os << "  field " << f.name << " offset=" << f.offset << "\n";
        }
        for (auto& vm : t.vmethods) {
            os << "  vmethod " << vm.name << " slot=" << vm.vtable_slot << "\n";
        }
        os << "\n";
    }

    for (auto& func : mod.functions) {
        DumpFunction(os, func);
    }
}

}  // namespace compiler
}  // namespace hotvm
