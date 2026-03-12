// ═══════════════════════════════════════════════════════════════
// HotVM Example: Module serialization round-trip
//
// Demonstrates:
// 1. Creating a Module with types and functions
// 2. Writing to .hotmod binary format
// 3. Reading back and verifying
// 4. Creating a diff / patch between two versions
// ═══════════════════════════════════════════════════════════════

#include "hotvm/bytecode.h"
#include "hotvm/module.h"
#include "hotvm/patch_manifest.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

using namespace hotvm;

static Instruction MakeInst(OpCode op, uint8_t a = 0, uint8_t b = 0,
                              uint8_t c = 0, uint32_t imm32 = 0,
                              uint64_t imm64 = 0) {
    return Instruction(op, a, b, c, imm32, imm64);
}

int main() {
    std::cout << "═══ HotVM Module Serialization Demo ═══\n\n";

    // ── Build version A module ───────────────────────────────

    Module mod_a;
    mod_a.header = {kModuleMagic, kModuleVersion, 1, 1, 0};

    TypeInfo calc_type;
    calc_type.type_id = 0;
    calc_type.name = "Calculator";
    calc_type.size = 16;
    calc_type.align = 8;
    FieldInfo fi;
    fi.name = "value_";
    fi.offset = 8;
    fi.kind = ArgKind::kI64;
    fi.type_id = kInvalidTypeId;
    calc_type.fields.push_back(fi);
    mod_a.types.push_back(calc_type);

    VmFunction compute_a;
    compute_a.func_id = 0;
    compute_a.name = "Calculator::compute";
    compute_a.param_count = 2;
    compute_a.reg_count = 4;
    compute_a.ret_kind = RetKind::kI64;
    compute_a.param_kinds = {ArgKind::kPtr, ArgKind::kI64};
    compute_a.code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        MakeInst(OpCode::kAddI64, 3, 2, 1),
        MakeInst(OpCode::kRet, 3),
    };
    mod_a.functions.push_back(compute_a);

    // Write version A
    std::cout << "Writing version A (addition)...\n";
    assert(WriteModule("example_a.hotmod", mod_a));

    // ── Build version B module ───────────────────────────────

    Module mod_b = mod_a;  // Copy structure
    mod_b.functions[0].code = {
        MakeInst(OpCode::kLoadField, 2, 0, 0, 8),
        MakeInst(OpCode::kMulI64, 3, 2, 1),  // Changed to multiply
        MakeInst(OpCode::kRet, 3),
    };

    std::cout << "Writing version B (multiplication)...\n";
    assert(WriteModule("example_b.hotmod", mod_b));

    // ── Read back and verify ─────────────────────────────────

    std::cout << "Reading back modules...\n";
    Module loaded_a = ReadModule("example_a.hotmod");
    Module loaded_b = ReadModule("example_b.hotmod");

    assert(loaded_a.functions.size() == 1);
    assert(loaded_b.functions.size() == 1);
    assert(loaded_a.functions[0].code[1].op == OpCode::kAddI64);
    assert(loaded_b.functions[0].code[1].op == OpCode::kMulI64);
    std::cout << "  Version A: AddI64 (addition) ✓\n";
    std::cout << "  Version B: MulI64 (multiplication) ✓\n";

    // ── Create and write patch ───────────────────────────────

    std::cout << "\nCreating patch A → B...\n";

    PatchManifest patch;
    patch.version = 1;
    patch.base_version_hash = 0;

    // Compare functions
    bool bytecode_same = (loaded_a.functions[0].code.size() == loaded_b.functions[0].code.size());
    if (bytecode_same) {
        for (size_t i = 0; i < loaded_a.functions[0].code.size(); ++i) {
            if (std::memcmp(&loaded_a.functions[0].code[i],
                             &loaded_b.functions[0].code[i],
                             sizeof(Instruction)) != 0) {
                bytecode_same = false;
                break;
            }
        }
    }

    if (!bytecode_same) {
        PatchEntry entry;
        entry.action = PatchAction::kModified;
        entry.func_name = loaded_a.functions[0].name;
        entry.func_id = loaded_a.functions[0].func_id;
        entry.new_func = loaded_b.functions[0];
        patch.entries.push_back(entry);
        std::cout << "  Function '" << entry.func_name << "' modified\n";
    }

    assert(WritePatchManifest("example_a_to_b.hotpatch", patch));
    std::cout << "  Written example_a_to_b.hotpatch\n";

    // ── Read back patch and verify ───────────────────────────

    PatchManifest loaded_patch = ReadPatchManifest("example_a_to_b.hotpatch");
    assert(loaded_patch.entries.size() == 1);
    assert(loaded_patch.entries[0].action == PatchAction::kModified);
    assert(loaded_patch.entries[0].new_func.code[1].op == OpCode::kMulI64);
    std::cout << "  Patch verified: contains MulI64 ✓\n";

    // Cleanup
    std::remove("example_a.hotmod");
    std::remove("example_b.hotmod");
    std::remove("example_a_to_b.hotpatch");

    std::cout << "\n═══ Module serialization demo complete ═══\n";
    return 0;
}
