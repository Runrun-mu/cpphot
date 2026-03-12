// ═══════════════════════════════════════════════════════════════
// HotVM — ARM64 Adapter Entry (AAPCS64)
//
// AdapterEntry saves caller registers into AdapterFrame, calls
// AdapterDispatch, then restores return values.
//
// AdapterFrame layout (264 bytes):
//   offset 0:    GPR args x0-x7                   (8 × 8 = 64 bytes)
//   offset 64:   FP/SIMD q0-q7                    (8 × 16 = 128 bytes)
//   offset 192:  wrapper_id                       (8 bytes)
//   offset 200:  caller_sp                        (8 bytes)
//   offset 208:  ret_x0                           (8 bytes)
//   offset 216:  ret_x1                           (8 bytes)
//   offset 224:  ret_q0                           (16 bytes)
//   offset 240:  ret_q1                           (16 bytes)
//   offset 256:  ret_kind                         (4 bytes)
//   offset 260:  reserved                         (4 bytes)
//   Total: 264 bytes (padded to 272 for 16-byte alignment)
// ═══════════════════════════════════════════════════════════════

#ifdef __aarch64__

.text
.globl _AdapterEntry
.p2align 2

_AdapterEntry:
    // w10 = wrapper_id (set by wrapper stub)

    // Save link register and frame pointer
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Allocate AdapterFrame (272 bytes, 16-byte aligned)
    sub     sp, sp, #272

    // Save GPR arguments x0-x7
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]

    // Save FP/SIMD arguments q0-q7
    stp     q0, q1, [sp, #64]
    stp     q2, q3, [sp, #96]
    stp     q4, q5, [sp, #128]
    stp     q6, q7, [sp, #160]

    // Save metadata
    // wrapper_id: zero-extend w10 to x10
    uxtw    x10, w10
    str     x10, [sp, #192]
    // caller_sp
    add     x11, x29, #16
    str     x11, [sp, #200]

    // Clear return area
    stp     xzr, xzr, [sp, #208]    // ret_x0, ret_x1
    stp     q0, q0, [sp, #224]      // ret_q0, ret_q1 (zero via q0 which may not be 0, but will be overwritten)
    str     wzr, [sp, #256]          // ret_kind
    str     wzr, [sp, #260]          // reserved

    // Call _AdapterDispatch(frame*)
    mov     x0, sp
    bl      _AdapterDispatch

    // Restore return values based on ret_kind
    ldr     w9, [sp, #256]           // ret_kind

    cbz     w9, .Larm_ret_done      // 0 = void

    cmp     w9, #1
    b.eq    .Larm_ret_i64

    cmp     w9, #2
    b.eq    .Larm_ret_f64

    cmp     w9, #3
    b.eq    .Larm_ret_ptr

    b       .Larm_ret_done

.Larm_ret_i64:
    ldp     x0, x1, [sp, #208]
    b       .Larm_ret_done

.Larm_ret_f64:
    ldp     q0, q1, [sp, #224]
    b       .Larm_ret_done

.Larm_ret_ptr:
    ldr     x0, [sp, #208]
    b       .Larm_ret_done

.Larm_ret_done:
    add     sp, sp, #272
    ldp     x29, x30, [sp], #16
    ret

#endif // __aarch64__
