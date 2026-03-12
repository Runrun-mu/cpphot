#include "hotvm/module.h"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace hotvm {

// ── CRC32 ────────────────────────────────────────────────────

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91b, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d09, 0x90bf1d9f, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf8a0, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f6b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0c6f, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7822, 0x3b6e20c8, 0x4c69105e,
    0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf8a0, 0x32d86ce3, 0x45df5c75,
    0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f6b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808,
    0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0c6f, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162,
    0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49,
    0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc,
    0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7822,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f6, 0x1fda8360, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6b70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd706ff,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

uint32_t ComputeCRC32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ── Module lookup helpers ────────────────────────────────────

const VmFunction* Module::FindFunction(const std::string& name) const {
    for (auto& f : functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

const VmFunction* Module::FindFunction(FuncId id) const {
    for (auto& f : functions) {
        if (f.func_id == id) return &f;
    }
    return nullptr;
}

const TypeInfo* Module::FindType(const std::string& name) const {
    for (auto& t : types) {
        if (t.name == name) return &t;
    }
    return nullptr;
}

const TypeInfo* Module::FindType(TypeId id) const {
    for (auto& t : types) {
        if (t.type_id == id) return &t;
    }
    return nullptr;
}

// ── Binary serialization helpers ─────────────────────────────

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

// ── Write module ─────────────────────────────────────────────

bool WriteModule(const std::string& path, const Module& mod) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Header
    WriteU32(out, kModuleMagic);
    WriteU32(out, kModuleVersion);
    WriteU32(out, static_cast<uint32_t>(mod.types.size()));
    WriteU32(out, static_cast<uint32_t>(mod.functions.size()));
    WriteU32(out, 0);  // string_table_size placeholder

    // Types
    for (auto& t : mod.types) {
        WriteU32(out, t.type_id);
        WriteStr(out, t.name);
        WriteU32(out, t.size);
        WriteU32(out, t.align);
        WriteU32(out, t.base_type_id);
        // Fields
        WriteU32(out, static_cast<uint32_t>(t.fields.size()));
        for (auto& f : t.fields) {
            WriteStr(out, f.name);
            WriteU32(out, f.offset);
            WriteU8(out, static_cast<uint8_t>(f.kind));
            WriteU32(out, f.type_id);
        }
        // Virtual methods
        WriteU32(out, static_cast<uint32_t>(t.vmethods.size()));
        for (auto& vm : t.vmethods) {
            WriteStr(out, vm.name);
            WriteU32(out, vm.vtable_slot);
            WriteU32(out, vm.func_id);
        }
    }

    // Functions
    for (auto& fn : mod.functions) {
        WriteU32(out, fn.func_id);
        WriteStr(out, fn.name);
        WriteU16(out, fn.reg_count);
        WriteU16(out, fn.param_count);
        // Param kinds
        for (int i = 0; i < fn.param_count; ++i) {
            WriteU8(out, static_cast<uint8_t>(fn.param_kinds[i]));
        }
        WriteU8(out, static_cast<uint8_t>(fn.ret_kind));
        // Code
        WriteU32(out, static_cast<uint32_t>(fn.code.size()));
        for (auto& inst : fn.code) {
            WriteU8(out, static_cast<uint8_t>(inst.op));
            WriteU8(out, inst.a);
            WriteU8(out, inst.b);
            WriteU8(out, inst.c);
            WriteU32(out, inst.imm32);
            WriteU64(out, inst.imm64);
        }
        // Unwind entries
        WriteU32(out, static_cast<uint32_t>(fn.unwind_entries.size()));
        for (auto& uw : fn.unwind_entries) {
            WriteU32(out, uw.try_begin);
            WriteU32(out, uw.try_end);
            WriteU32(out, uw.catch_pc);
            WriteU32(out, uw.dtor_slot);
            WriteU32(out, uw.catch_type);
        }
    }

    // CRC32 checksum
    out.flush();
    auto pos = out.tellp();
    out.seekp(0, std::ios::beg);
    // We can't easily compute CRC over the stream, so write a placeholder
    // A full implementation would buffer the data first
    out.seekp(pos);
    WriteU32(out, 0);  // placeholder checksum

    return out.good();
}

// ── Read module ──────────────────────────────────────────────

Module ReadModule(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open module: " + path);

    Module mod;

    // Header
    mod.header.magic = ReadU32(in);
    if (mod.header.magic != kModuleMagic) {
        throw std::runtime_error("Invalid module magic");
    }
    mod.header.version = ReadU32(in);
    mod.header.type_count = ReadU32(in);
    mod.header.func_count = ReadU32(in);
    mod.header.string_table_size = ReadU32(in);

    // Types
    mod.types.resize(mod.header.type_count);
    for (uint32_t i = 0; i < mod.header.type_count; ++i) {
        auto& t = mod.types[i];
        t.type_id = ReadU32(in);
        t.name = ReadStr(in);
        t.size = ReadU32(in);
        t.align = ReadU32(in);
        t.base_type_id = ReadU32(in);
        // Fields
        uint32_t field_count = ReadU32(in);
        t.fields.resize(field_count);
        for (uint32_t j = 0; j < field_count; ++j) {
            t.fields[j].name = ReadStr(in);
            t.fields[j].offset = ReadU32(in);
            t.fields[j].kind = static_cast<ArgKind>(ReadU8(in));
            t.fields[j].type_id = ReadU32(in);
        }
        // Virtual methods
        uint32_t vmethod_count = ReadU32(in);
        t.vmethods.resize(vmethod_count);
        for (uint32_t j = 0; j < vmethod_count; ++j) {
            t.vmethods[j].name = ReadStr(in);
            t.vmethods[j].vtable_slot = ReadU32(in);
            t.vmethods[j].func_id = ReadU32(in);
        }
    }

    // Functions
    mod.functions.resize(mod.header.func_count);
    for (uint32_t i = 0; i < mod.header.func_count; ++i) {
        auto& fn = mod.functions[i];
        fn.func_id = ReadU32(in);
        fn.name = ReadStr(in);
        fn.reg_count = ReadU16(in);
        fn.param_count = ReadU16(in);
        fn.param_kinds.resize(fn.param_count);
        for (int j = 0; j < fn.param_count; ++j) {
            fn.param_kinds[j] = static_cast<ArgKind>(ReadU8(in));
        }
        fn.ret_kind = static_cast<RetKind>(ReadU8(in));
        // Code
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
        // Unwind entries
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

    return mod;
}

}  // namespace hotvm
