#include "hotvm/patch_manifest.h"
#include "hotvm/module.h"
#include <fstream>
#include <stdexcept>

namespace hotvm {

constexpr uint32_t kPatchMagic = 0x50544F48;  // "HOTP" little-endian

// ── Serialization helpers (reuse from module.cpp) ────────────

static void WriteU8(std::ostream& os, uint8_t v) { os.write(reinterpret_cast<char*>(&v), 1); }
static void WriteU16(std::ostream& os, uint16_t v) { os.write(reinterpret_cast<char*>(&v), 2); }
static void WriteU32(std::ostream& os, uint32_t v) { os.write(reinterpret_cast<char*>(&v), 4); }
static void WriteU64(std::ostream& os, uint64_t v) { os.write(reinterpret_cast<char*>(&v), 8); }
static void WriteStr(std::ostream& os, const std::string& s) {
    WriteU32(os, static_cast<uint32_t>(s.size()));
    os.write(s.data(), s.size());
}

static uint8_t ReadU8(std::istream& is) { uint8_t v; is.read(reinterpret_cast<char*>(&v), 1); return v; }
static uint16_t ReadU16(std::istream& is) { uint16_t v; is.read(reinterpret_cast<char*>(&v), 2); return v; }
static uint32_t ReadU32(std::istream& is) { uint32_t v; is.read(reinterpret_cast<char*>(&v), 4); return v; }
static uint64_t ReadU64(std::istream& is) { uint64_t v; is.read(reinterpret_cast<char*>(&v), 8); return v; }
static std::string ReadStr(std::istream& is) {
    uint32_t len = ReadU32(is);
    std::string s(len, '\0');
    is.read(&s[0], len);
    return s;
}

// ── Write patch manifest ─────────────────────────────────────

bool WritePatchManifest(const std::string& path, const PatchManifest& manifest) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    WriteU32(out, kPatchMagic);
    WriteU32(out, manifest.version);
    WriteU32(out, manifest.base_version_hash);
    WriteU32(out, static_cast<uint32_t>(manifest.entries.size()));

    for (auto& entry : manifest.entries) {
        WriteU8(out, static_cast<uint8_t>(entry.action));
        WriteStr(out, entry.func_name);
        WriteU32(out, entry.func_id);

        if (entry.action == PatchAction::kModified ||
            entry.action == PatchAction::kAdded) {
            auto& fn = entry.new_func;
            WriteU16(out, fn.reg_count);
            WriteU16(out, fn.param_count);
            for (int i = 0; i < fn.param_count; ++i) {
                WriteU8(out, static_cast<uint8_t>(fn.param_kinds[i]));
            }
            WriteU8(out, static_cast<uint8_t>(fn.ret_kind));
            WriteU32(out, static_cast<uint32_t>(fn.code.size()));
            for (auto& inst : fn.code) {
                WriteU8(out, static_cast<uint8_t>(inst.op));
                WriteU8(out, inst.a);
                WriteU8(out, inst.b);
                WriteU8(out, inst.c);
                WriteU32(out, inst.imm32);
                WriteU64(out, inst.imm64);
            }
            WriteU32(out, static_cast<uint32_t>(fn.unwind_entries.size()));
            for (auto& uw : fn.unwind_entries) {
                WriteU32(out, uw.try_begin);
                WriteU32(out, uw.try_end);
                WriteU32(out, uw.catch_pc);
                WriteU32(out, uw.dtor_slot);
                WriteU32(out, uw.catch_type);
            }
        }
    }

    return out.good();
}

// ── Read patch manifest ──────────────────────────────────────

PatchManifest ReadPatchManifest(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open patch: " + path);

    PatchManifest manifest;
    uint32_t magic = ReadU32(in);
    if (magic != kPatchMagic) {
        throw std::runtime_error("Invalid patch magic");
    }

    manifest.version = ReadU32(in);
    manifest.base_version_hash = ReadU32(in);
    uint32_t entry_count = ReadU32(in);

    manifest.entries.resize(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        auto& entry = manifest.entries[i];
        entry.action = static_cast<PatchAction>(ReadU8(in));
        entry.func_name = ReadStr(in);
        entry.func_id = ReadU32(in);

        if (entry.action == PatchAction::kModified ||
            entry.action == PatchAction::kAdded) {
            auto& fn = entry.new_func;
            fn.func_id = entry.func_id;
            fn.name = entry.func_name;
            fn.reg_count = ReadU16(in);
            fn.param_count = ReadU16(in);
            fn.param_kinds.resize(fn.param_count);
            for (int j = 0; j < fn.param_count; ++j) {
                fn.param_kinds[j] = static_cast<ArgKind>(ReadU8(in));
            }
            fn.ret_kind = static_cast<RetKind>(ReadU8(in));
            uint32_t code_size = ReadU32(in);
            fn.code.resize(code_size);
            for (uint32_t j = 0; j < code_size; ++j) {
                fn.code[j].op = static_cast<OpCode>(ReadU8(in));
                fn.code[j].a = ReadU8(in);
                fn.code[j].b = ReadU8(in);
                fn.code[j].c = ReadU8(in);
                fn.code[j].imm32 = ReadU32(in);
                fn.code[j].imm64 = ReadU64(in);
            }
            uint32_t unwind_count = ReadU32(in);
            fn.unwind_entries.resize(unwind_count);
            for (uint32_t j = 0; j < unwind_count; ++j) {
                fn.unwind_entries[j].try_begin = ReadU32(in);
                fn.unwind_entries[j].try_end = ReadU32(in);
                fn.unwind_entries[j].catch_pc = ReadU32(in);
                fn.unwind_entries[j].dtor_slot = ReadU32(in);
                fn.unwind_entries[j].catch_type = ReadU32(in);
            }
        }
    }

    return manifest;
}

}  // namespace hotvm
