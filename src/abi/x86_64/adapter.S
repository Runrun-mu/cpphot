// ═══════════════════════════════════════════════════════════════
// HotVM — x86_64 Adapter Entry (System V AMD64 ABI)
//
// AdapterEntry is the unified entry point called by all wrapper stubs.
// It saves the caller's registers into an AdapterFrame, calls the
// C++ AdapterDispatch function, then restores return values.
//
// AdapterFrame layout (248 bytes):
//   offset 0:    GPR args  rdi,rsi,rdx,rcx,r8,r9  (6 × 8 = 48 bytes)
//   offset 48:   SSE args  xmm0-xmm7              (8 × 16 = 128 bytes)
//   offset 176:  wrapper_id                        (8 bytes)
//   offset 184:  caller_rsp                        (8 bytes)
//   offset 192:  ret_rax                           (8 bytes)
//   offset 200:  ret_rdx                           (8 bytes)
//   offset 208:  ret_xmm0                          (16 bytes)
//   offset 224:  ret_xmm1                          (16 bytes)
//   offset 240:  ret_kind                          (4 bytes)
//   offset 244:  reserved                          (4 bytes)
//   Total: 248 bytes
// ═══════════════════════════════════════════════════════════════

#ifdef __x86_64__

.text
.globl AdapterEntry
.type AdapterEntry, @function
.p2align 4

AdapterEntry:
    // r10d = wrapper_id (set by wrapper stub)

    // Allocate AdapterFrame on stack (256 bytes for 16-byte alignment)
    sub     rsp, 256

    // Save GPR arguments
    mov     [rsp + 0],   rdi
    mov     [rsp + 8],   rsi
    mov     [rsp + 16],  rdx
    mov     [rsp + 24],  rcx
    mov     [rsp + 32],  r8
    mov     [rsp + 40],  r9

    // Save SSE arguments
    movaps  [rsp + 48],  xmm0
    movaps  [rsp + 64],  xmm1
    movaps  [rsp + 80],  xmm2
    movaps  [rsp + 96],  xmm3
    movaps  [rsp + 112], xmm4
    movaps  [rsp + 128], xmm5
    movaps  [rsp + 144], xmm6
    movaps  [rsp + 160], xmm7

    // Save metadata
    mov     [rsp + 176], r10        // wrapper_id (zero-extended from r10d)
    lea     rax, [rsp + 256]        // caller_rsp (before our sub)
    mov     [rsp + 184], rax

    // Clear return area
    xor     eax, eax
    mov     [rsp + 192], rax        // ret_rax
    mov     [rsp + 200], rax        // ret_rdx
    pxor    xmm0, xmm0
    movaps  [rsp + 208], xmm0      // ret_xmm0
    movaps  [rsp + 224], xmm0      // ret_xmm1
    mov     dword ptr [rsp + 240], 0 // ret_kind
    mov     dword ptr [rsp + 244], 0 // reserved

    // Call AdapterDispatch(frame*)
    // System V: first arg in rdi
    mov     rdi, rsp
    call    AdapterDispatch

    // Restore return values based on ret_kind
    mov     eax, [rsp + 240]       // ret_kind
    // 0=void, 1=i64, 2=f64, 3=ptr

    cmp     eax, 1
    je      .Lret_i64
    cmp     eax, 2
    je      .Lret_f64
    cmp     eax, 3
    je      .Lret_ptr
    jmp     .Lret_done              // void

.Lret_i64:
    mov     rax, [rsp + 192]       // ret_rax
    mov     rdx, [rsp + 200]       // ret_rdx
    jmp     .Lret_done

.Lret_f64:
    movaps  xmm0, [rsp + 208]     // ret_xmm0
    movaps  xmm1, [rsp + 224]     // ret_xmm1
    jmp     .Lret_done

.Lret_ptr:
    mov     rax, [rsp + 192]       // ret_rax
    jmp     .Lret_done

.Lret_done:
    add     rsp, 256
    ret

.size AdapterEntry, .-AdapterEntry

#endif // __x86_64__
