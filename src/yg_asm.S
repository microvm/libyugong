#include "yg_asm_common.S.inc"

begin_func yg_stack_swap
  .cfi_startproc
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp

  pushq %rbx
  .cfi_offset %rbx, -24
  pushq %r12
  .cfi_offset %r12, -32

  pushq %r13
  .cfi_offset %r13, -40
  pushq %r14
  .cfi_offset %r14, -48

  pushq %r15
  .cfi_offset %r15, -56

  movq %rdx, %rax   // Move the 3rd arg to the return value reg.

  lea __yg_stack_swap_cont_local(%rip), %rcx
  push %rcx

  movq %rsp, (%rdi)
  movq (%rsi), %rsp
  ret
  .cfi_endproc
end_func yg_stack_swap

begin_func _yg_stack_swap_cont
__yg_stack_swap_cont_local:   // local label
  .cfi_startproc simple
  .cfi_def_cfa %rsp, 64
  .cfi_offset %r15, -56
  .cfi_offset %r14, -48
  .cfi_offset %r13, -40
  .cfi_offset %r12, -32
  .cfi_offset %rbx, -24
  .cfi_offset %rbp, -16
  popq %r15
  .cfi_same_value %r15

  popq %r14
  .cfi_same_value %r14
  popq %r13
  .cfi_same_value %r13

  popq %r12
  .cfi_same_value %r12
  popq %rbx
  .cfi_same_value %rbx

  popq %rbp
  .cfi_same_value %rbp
  ret
  .cfi_endproc
end_func _yg_stack_swap_cont

begin_func _yg_func_begin_resume
  .cfi_startproc
  .cfi_def_cfa_offset 16
  movq %rax, %rdi   // argument
  ret
  .cfi_endproc
end_func _yg_func_begin_resume
