#include "yg_asm_common.S.inc"

begin_func yg_stack_swap
#if defined(__x86_64__)
    pushq %rbp
    movq  %rsp, %rbp

    pushq %rbx
    pushq %r12

    pushq %r13
    pushq %r14

    pushq %r15

    movq  %rdx, %rax   // Move the 3rd arg to the return value reg.

    lea   __yg_stack_swap_cont_local(%rip), %rcx
    push  %rcx

    movq  %rsp, (%rdi)
    movq  (%rsi), %rsp
    ret
#elif defined(__arm64__) || defined(__aarch64__)
    sub  sp, sp, #0xb0

    // Save the continue point on the top of the stack
    adr  x4,  _yg_stack_swap_cont
    str  x4, [sp, #0x00]

    stp  d8,  d9,  [sp, #0x10]
    stp  d10, d11, [sp, #0x20]
    stp  d12, d13, [sp, #0x30]
    stp  d14, d15, [sp, #0x40]

    stp  x19, x20, [sp, #0x50]
    stp  x21, x22, [sp, #0x60]
    stp  x23, x24, [sp, #0x70]
    stp  x25, x26, [sp, #0x80]
    stp  x27, x28, [sp, #0x90]
    stp  x29, x30, [sp, #0xa0]

    // Save my sp
    mov  x4, sp
    str  x4, [x0]

    // Set sp to swappee sp
    ldr  x4, [x1]
    mov  sp, x4

    // Resume from the continue point of the opposite stack
    ldr  x30, [sp, #0x00]
    br   x30
#endif
end_func yg_stack_swap

begin_func _yg_stack_swap_cont
__yg_stack_swap_cont_local:   // local label
#if defined(__x86_64__)
    popq %r15

    popq %r14
    popq %r13

    popq %r12
    popq %rbx

    popq %rbp
    ret
#elif defined(__arm64__) || defined(__aarch64__)
    ldp  d8,  d9,  [sp, #0x10]
    ldp  d10, d11, [sp, #0x20]
    ldp  d12, d13, [sp, #0x30]
    ldp  d14, d15, [sp, #0x40]

    ldp  x19, x20, [sp, #0x50]
    ldp  x21, x22, [sp, #0x60]
    ldp  x23, x24, [sp, #0x70]
    ldp  x25, x26, [sp, #0x80]
    ldp  x27, x28, [sp, #0x90]
    ldp  x29, x30, [sp, #0xa0]

    // Move the passed value (still in x2) into the return value register x0
    mov  x0, x4

    // Restore sp to callee sp
    add  sp, sp, #0xb0

    // Resume from the continue point of the opposite stack
    ret  x30
#endif
end_func _yg_stack_swap_cont

begin_func _yg_func_begin_resume
#if defined(__x86_64__)
    movq %rax, %rdi   // argument
    ret
#elif defined(__arm64__) || defined(__aarch64__)
    ldp  x9, x30, [sp], 16
    br   x9
#endif
end_func _yg_func_begin_resume

# vim: ts=4 sw=4 et sts=4
