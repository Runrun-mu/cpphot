// ═══════════════════════════════════════════════════════════════
// HotVM — x86_64 Wrapper Stubs (System V AMD64 ABI)
//
// Each wrapper stub loads its ID into r10d and jumps to AdapterEntry.
// 1024 stubs are generated via macro.
// ═══════════════════════════════════════════════════════════════

#ifdef __x86_64__

.text

// ── Macro: generate one wrapper stub ─────────────────────────
// Each stub:
//   lea r11, [rip + AdapterEntry]
//   mov r10d, <id>
//   jmp r11

.macro GEN_WRAPPER id
.globl wrapper_\id
.type wrapper_\id, @function
.p2align 4
wrapper_\id:
    lea     r11, [rip + AdapterEntry]
    mov     r10d, \id
    jmp     r11
.size wrapper_\id, .-wrapper_\id
.endm

// ── Generate 1024 wrapper stubs ──────────────────────────────

GEN_WRAPPER 0
GEN_WRAPPER 1
GEN_WRAPPER 2
GEN_WRAPPER 3
GEN_WRAPPER 4
GEN_WRAPPER 5
GEN_WRAPPER 6
GEN_WRAPPER 7
GEN_WRAPPER 8
GEN_WRAPPER 9
GEN_WRAPPER 10
GEN_WRAPPER 11
GEN_WRAPPER 12
GEN_WRAPPER 13
GEN_WRAPPER 14
GEN_WRAPPER 15
GEN_WRAPPER 16
GEN_WRAPPER 17
GEN_WRAPPER 18
GEN_WRAPPER 19
GEN_WRAPPER 20
GEN_WRAPPER 21
GEN_WRAPPER 22
GEN_WRAPPER 23
GEN_WRAPPER 24
GEN_WRAPPER 25
GEN_WRAPPER 26
GEN_WRAPPER 27
GEN_WRAPPER 28
GEN_WRAPPER 29
GEN_WRAPPER 30
GEN_WRAPPER 31
// ... (in production, use .altmacro + .irp or a script to generate all 1024)
// For brevity, we generate 32 stubs here. A build script would expand to 1024.

// ── Wrapper address table ────────────────────────────────────
// Array of function pointers to all wrappers, for dispatch table init.

.data
.globl hotvm_wrapper_table
.p2align 3
hotvm_wrapper_table:
    .quad wrapper_0
    .quad wrapper_1
    .quad wrapper_2
    .quad wrapper_3
    .quad wrapper_4
    .quad wrapper_5
    .quad wrapper_6
    .quad wrapper_7
    .quad wrapper_8
    .quad wrapper_9
    .quad wrapper_10
    .quad wrapper_11
    .quad wrapper_12
    .quad wrapper_13
    .quad wrapper_14
    .quad wrapper_15
    .quad wrapper_16
    .quad wrapper_17
    .quad wrapper_18
    .quad wrapper_19
    .quad wrapper_20
    .quad wrapper_21
    .quad wrapper_22
    .quad wrapper_23
    .quad wrapper_24
    .quad wrapper_25
    .quad wrapper_26
    .quad wrapper_27
    .quad wrapper_28
    .quad wrapper_29
    .quad wrapper_30
    .quad wrapper_31

#endif // __x86_64__
