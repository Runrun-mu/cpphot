<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white" />
  <img src="https://img.shields.io/badge/Platform-x86__64%20%7C%20ARM64-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge" />
  <img src="https://img.shields.io/github/stars/Runrun-mu/cpphot?style=for-the-badge&color=orange" />
</p>

<h1 align="center">🔥 HotVM</h1>

<p align="center">
  <strong>Production-Grade C++ Hot-Reload Virtual Machine</strong><br>
  <sub>Zero-overhead native execution · Surgical bytecode patching · Sub-millisecond hot-swap</sub>
</p>

<p align="center">
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-how-it-works">How It Works</a> •
  <a href="#-benchmark">Benchmark</a> •
  <a href="#-build">Build</a> •
  <a href="#-roadmap">Roadmap</a>
</p>

---

## 💡 What is HotVM?

HotVM is a **C++ hot-reload engine** that lets you patch running C++ programs **without restarting**. Unlike scripting-based approaches, HotVM starts with **native machine code at zero overhead** — only functions you explicitly patch get compiled to bytecode and run in the VM.

```
                    ┌──────────────────────────────────┐
    Before patch:   │  All functions → Native Code     │  ← Zero overhead
                    └──────────────────────────────────┘

                    ┌──────────────────────────────────┐
    After patch:    │  Unchanged  → Native Code        │  ← Still zero overhead
                    │  Patched    → VM Interpreter     │  ← Only patched funcs
                    └──────────────────────────────────┘
```

> **Key insight**: In a typical hot-reload scenario, <1% of functions change. HotVM ensures the other 99% run at full native speed.

---

## ✨ Features

| Feature | Description |
|:--------|:------------|
| **🚀 Zero-Overhead Default** | All functions start as native machine code. No VM tax until you patch. |
| **🔧 Surgical Patching** | Only modified functions switch to VM execution. Everything else stays native. |
| **⚡ 60+ Bytecode Opcodes** | Full register-machine ISA: integer/float arithmetic, memory ops, virtual dispatch, exceptions. |
| **🏗️ Clang Frontend** | Compile C++ source → IR → bytecode via `libclang` AST traversal. |
| **📦 Binary Module Format** | `.hotmod` modules with types, functions, and CRC32 checksums. |
| **🔄 Incremental Diff** | `hotvm-diff` compares two `.hotmod` files → `.hotpatch` delta package. |
| **⏪ Instant Rollback** | Undo any patch and restore original native behavior in microseconds. |
| **🖥️ Multi-Architecture** | x86_64 (System V ABI) + ARM64 (AAPCS64) assembly trampolines. |
| **🎯 Virtual Dispatch** | Hot-patch virtual functions by rewriting vtable slots through wrappers. |
| **🛡️ Exception Bridging** | VM exceptions propagate through native C++ `try/catch` boundaries. |

---

## 🏛️ Architecture

```
┌──────────────────── Compile Time ─────────────────────┐  ┌─────────── Runtime ───────────┐
│                                                        │  │                               │
│  C++ Source (.cpp)                                     │  │   Native Binary                │
│       │                                                │  │       │                        │
│       ▼                                                │  │       ▼                        │
│  ┌──────────────────┐                                  │  │  ┌─────────────────────────┐   │
│  │  Clang Frontend   │  libclang AST traversal         │  │  │  Function Dispatch Table │   │
│  └────────┬─────────┘                                  │  │  │  (asm trampoline layer)  │   │
│           ▼                                            │  │  └──────────┬──────────────┘   │
│  ┌──────────────────┐                                  │  │             │                   │
│  │   HotVM IR        │  Three-address code             │  │    ┌───────┴────────┐          │
│  └────────┬─────────┘                                  │  │    │                │          │
│           │                                            │  │    ▼                ▼          │
│     ┌─────┴─────┐                                      │  │  Native Code    VM Interp     │
│     ▼           ▼                                      │  │  (default)      (after patch) │
│  Native      Bytecode                                  │  │                               │
│  Codegen     Codegen                                   │  │  ┌───────────────────────┐    │
│              (.hotmod)                                  │  │  │    Patch Manager      │    │
│                                                        │  │  │  load .hotpatch       │    │
│  ┌──────────────────┐                                  │  │  │  rebind dispatch      │    │
│  │  hotvm-diff       │ old.hotmod ──┐                  │  │  │  rollback support     │    │
│  │                   │ new.hotmod ──┤→ .hotpatch       │  │  └───────────────────────┘    │
│  └──────────────────┘              │                   │  │                               │
└────────────────────────────────────┘───────────────────┘  └───────────────────────────────┘
```

### Assembly Trampoline Layer

Every function call goes through a **stable wrapper stub**. The stub never changes — only the metadata behind it does:

```
┌──────────┐     ┌───────────────┐     ┌──────────────┐
│ call site │────▶│ wrapper_42    │────▶│ AdapterEntry │
└──────────┘     │ mov r10d, 42  │     │ save regs    │
                 │ jmp Adapter   │     │ lookup meta  │──┐
                 └───────────────┘     └──────────────┘  │
                                                          │
                       ┌──────────────────────────────────┘
                       │
                       ▼
              ┌─────────────────┐
              │  WrapperMeta[42]│
              │  mode: Native ──│──▶ direct call → original function
              │  mode: VM    ──│──▶ interpret bytecode
              └─────────────────┘
```

---

## 🚀 Quick Start

### The Calculator Demo

```cpp
// Version A: compute does addition
class Calculator {
    int value_;
public:
    Calculator(int v) : value_(v) {}
    virtual int compute(int x) { return value_ + x; }
};

// At runtime:
Calculator c(10);
c.compute(5);  // → 15 (native, zero overhead)

// Hot-patch: swap compute to multiplication
PatchManager::Instance().ApplyPatch("v1_to_v2.hotpatch");
c.compute(5);  // → 50 (now runs in VM)

// Rollback: restore original
PatchManager::Instance().Rollback();
c.compute(5);  // → 15 (back to native)
```

### Build & Run

```bash
# Build the project
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run integration tests
ctest --output-on-failure

# Run the calculator demo
./example_calculator_demo
```

### Compile → Diff → Patch Workflow

```bash
# Compile two versions to bytecode modules
./hotvm-compile version_a.cpp -o a.hotmod
./hotvm-compile version_b.cpp -o b.hotmod

# Generate incremental patch
./hotvm-diff a.hotmod b.hotmod -o v1_to_v2.hotpatch

# Apply at runtime (from your C++ code)
PatchManager::Instance().ApplyPatch("v1_to_v2.hotpatch");
```

---

## ⚙️ Bytecode ISA

HotVM uses a **register-machine** design with **16-byte fixed-width instructions** for cache-friendly dispatch:

```
┌────────┬──────┬──────┬──────┬────────┬────────────────┐
│ opcode │ regA │ regB │ regC │ imm32  │     imm64      │
│ 1 byte │  1B  │  1B  │  1B  │ 4 bytes│    8 bytes     │
└────────┴──────┴──────┴──────┴────────┴────────────────┘
```

<details>
<summary><b>Full Instruction Set (60+ opcodes)</b></summary>

| Category | Opcodes | Description |
|:---------|:--------|:------------|
| **Data** | `LDI` `MOV` `LDF64` | Load immediate, register move |
| **Integer Math** | `ADD_I64` `SUB_I64` `MUL_I64` `DIV_I64` `MOD_I64` `NEG_I64` | 64-bit integer arithmetic |
| **Float Math** | `ADD_F64` `SUB_F64` `MUL_F64` `DIV_F64` `NEG_F64` | Double-precision floating point |
| **Compare** | `CMP_EQ` `CMP_NE` `CMP_LT` `CMP_LE` `CMP_GT` `CMP_GE` | Comparison → boolean |
| **Convert** | `I64_TO_F64` `F64_TO_I64` `I64_TO_PTR` `PTR_TO_I64` | Type conversion |
| **Memory** | `LOAD_I8/16/32/64` `STORE_I8/16/32/64` `LOAD_F64` `STORE_F64` `LOAD_PTR` `STORE_PTR` | Multi-width memory access |
| **Fields** | `LOAD_FIELD` `STORE_FIELD` | `obj + offset` field access |
| **Objects** | `ALLOC` `FREE` `GET_VTABLE` `CALL_VIRT` | Object lifecycle & virtual dispatch |
| **Bitwise** | `AND` `OR` `XOR` `NOT` `SHL` `SHR` | Bitwise operations |
| **Control** | `JMP` `JZ` `JNZ` `CALL` `RET` `RET_VOID` | Branches, calls, returns |
| **Exceptions** | `THROW` `TRY_ENTER` `TRY_EXIT` | Exception handling |
| **Native** | `CALL_NATIVE` | FFI to native functions |

</details>

---

## 📊 Benchmark

| Scenario | Overhead |
|:---------|:---------|
| Unpatched function call (native fast-path) | **~2ns** (one indirect jump + metadata check) |
| Patched function call (VM interpretation) | **~50-200ns** per bytecode instruction |
| Patch application | **<1ms** (rebind metadata, no code copying) |
| Rollback | **<1ms** (restore saved metadata) |

> The key design goal: **you only pay for what you patch**. Unpatched code runs at native speed.

---

## 📁 Project Structure

```
cpphot/
├── CMakeLists.txt                 # Build system
├── include/hotvm/                 # Public headers
│   ├── types.h                    #   Type system, ABI structs
│   ├── bytecode.h                 #   Opcode enum, Instruction, VmFunction
│   ├── interpreter.h              #   VM interpreter
│   ├── runtime.h                  #   Runtime singleton, dispatch
│   ├── wrapper_table.h            #   Wrapper slot management
│   ├── module.h                   #   .hotmod binary format
│   ├── type_registry.h            #   Runtime type info
│   ├── patch_manager.h            #   Hot-patch orchestration
│   ├── patch_manifest.h           #   .hotpatch format
│   ├── function_patch.h           #   Non-virtual function patching
│   ├── vtable_patch.h             #   Virtual table patching
│   └── exception_bridge.h         #   VM ↔ native exception bridging
├── src/
│   ├── vm/interpreter.cpp         # Bytecode execution engine
│   ├── runtime/                   # Runtime, wrapper table, module I/O, type registry
│   ├── patch/                     # Function patch, vtable patch, patch manager
│   ├── eh/                        # Exception bridge
│   └── abi/
│       ├── x86_64/                # System V AMD64 wrappers + adapter
│       └── arm64/                 # AAPCS64 wrappers + adapter
├── compiler/
│   ├── frontend/                  # Clang AST → IR
│   ├── ir/                        # Three-address code IR
│   ├── codegen/                   # IR → bytecode + module writer
│   └── driver/                    # hotvm-compile CLI
├── tools/
│   └── hotdiff/                   # Module diff → .hotpatch generator
├── tests/                         # Integration tests (11 test cases)
└── examples/                      # Calculator demo, module serialization demo
```

---

## 🔨 Build

### Prerequisites

- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- *(Optional)* libclang — required only for the compiler frontend

### Build Options

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DHOTVM_BUILD_COMPILER=ON \      # Build hotvm-compile (needs libclang)
  -DHOTVM_BUILD_TESTS=ON \         # Build test suite
  -DHOTVM_BUILD_EXAMPLES=ON        # Build examples
```

### Supported Platforms

| Platform | Architecture | ABI | Status |
|:---------|:------------|:----|:-------|
| Linux | x86_64 | System V AMD64 | ✅ Full support |
| macOS | ARM64 | AAPCS64 | ✅ Full support |
| Linux | ARM64 | AAPCS64 | ✅ Full support |
| Windows | x86_64 | Win64 | 🔧 Planned |

---

## 🔬 How It Works

### 1. Compile Time

```
source.cpp  →  Clang AST  →  HotVM IR  →  Bytecode (.hotmod)
```

The compiler extracts functions, classes, and type info from C++ source using `libclang`, lowers them to a three-address-code IR, then generates register-machine bytecode packed into `.hotmod` binary modules.

### 2. Runtime Initialization

Every function gets a **wrapper stub** — a tiny assembly trampoline that loads a wrapper ID and jumps to `AdapterEntry`. Initially, all wrappers point to native code:

```
WrapperMeta[N].mode = Native  →  direct forward to original function
```

### 3. Hot-Patch

When you apply a `.hotpatch`:

1. New bytecode is registered in the VM interpreter
2. The wrapper's dispatch mode flips from `Native` to `VM`
3. Next call through the wrapper executes bytecode instead of native code
4. **No code is rewritten.** Only metadata changes. Thread-safe. Sub-millisecond.

### 4. Rollback

The PatchManager saves undo records. Rollback restores the previous bytecode and flips dispatch mode back to `Native`. Instantaneous.

---

## 🗺️ Roadmap

- [x] Extended bytecode ISA (60+ opcodes)
- [x] Register-machine VM interpreter
- [x] x86_64 + ARM64 assembly trampolines
- [x] Native fast-path dispatch
- [x] Clang compiler frontend
- [x] `.hotmod` / `.hotpatch` binary formats
- [x] Incremental diff tool
- [x] PatchManager with rollback
- [x] Integration test suite
- [ ] JIT tiering (hot VM functions → native via LLVM)
- [ ] Multi-threaded patch coordination (RCU-based)
- [ ] Windows x64 ABI support
- [ ] Template instantiation support
- [ ] RTTI-based exception type matching
- [ ] Remote patch delivery (TCP/HTTP)
- [ ] IDE plugin (VSCode live-reload)

---

## 📜 License

MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
  <sub>Built with 🔥 for C++ developers who refuse to restart.</sub>
</p>
