#pragma once
#include "hotvm/bytecode.h"
#include "hotvm/types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace hotvm {

// ── .hotmod module format ────────────────────────────────────
// Binary module containing types and compiled bytecode functions.
//
// File layout:
//   [Header]       20 bytes
//   [StringTable]  variable
//   [TypeSection]  variable
//   [FuncSection]  variable
//   [Checksum]     4 bytes (CRC32)

constexpr uint32_t kModuleMagic   = 0x4D544F48;  // "HOTM" little-endian
constexpr uint32_t kModuleVersion = 1;

struct ModuleHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t type_count;
    uint32_t func_count;
    uint32_t string_table_size;
};

// ── Module (in-memory representation) ────────────────────────

struct Module {
    ModuleHeader             header;
    std::vector<std::string> strings;     // String table
    std::vector<TypeInfo>    types;
    std::vector<VmFunction>  functions;

    // Lookup helpers
    const VmFunction* FindFunction(const std::string& name) const;
    const VmFunction* FindFunction(FuncId id) const;
    const TypeInfo*   FindType(const std::string& name) const;
    const TypeInfo*   FindType(TypeId id) const;
};

// ── Module I/O ───────────────────────────────────────────────

// Write module to .hotmod file
bool WriteModule(const std::string& path, const Module& mod);

// Read module from .hotmod file
Module ReadModule(const std::string& path);

// Compute CRC32 checksum
uint32_t ComputeCRC32(const uint8_t* data, size_t len);

}  // namespace hotvm
