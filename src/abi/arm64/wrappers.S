// ═══════════════════════════════════════════════════════════════
// HotVM — ARM64 Wrapper Stubs (AAPCS64)
//
// Each wrapper stub loads its ID into w10 and branches to AdapterEntry.
// ═══════════════════════════════════════════════════════════════

#ifdef __aarch64__

.text

.macro GEN_WRAPPER id
.globl _wrapper_\id
.p2align 2
_wrapper_\id:
    mov     w10, #\id
    b       _AdapterEntry
.endm

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

// Wrapper address table
.data
.globl _hotvm_wrapper_table
.p2align 3
_hotvm_wrapper_table:
    .quad _wrapper_0
    .quad _wrapper_1
    .quad _wrapper_2
    .quad _wrapper_3
    .quad _wrapper_4
    .quad _wrapper_5
    .quad _wrapper_6
    .quad _wrapper_7
    .quad _wrapper_8
    .quad _wrapper_9
    .quad _wrapper_10
    .quad _wrapper_11
    .quad _wrapper_12
    .quad _wrapper_13
    .quad _wrapper_14
    .quad _wrapper_15
    .quad _wrapper_16
    .quad _wrapper_17
    .quad _wrapper_18
    .quad _wrapper_19
    .quad _wrapper_20
    .quad _wrapper_21
    .quad _wrapper_22
    .quad _wrapper_23
    .quad _wrapper_24
    .quad _wrapper_25
    .quad _wrapper_26
    .quad _wrapper_27
    .quad _wrapper_28
    .quad _wrapper_29
    .quad _wrapper_30
    .quad _wrapper_31

#endif // __aarch64__
