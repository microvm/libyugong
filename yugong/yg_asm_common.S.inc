#if defined (__linux__)

#define CNAME(n) n

  .macro begin_func n
  .text
  .globl CNAME(\n)
  .type CNAME(\n),@function
#if defined(__x86_64__)
  .p2align 4
#elif defined(__arm64__) || defined(__aarch64__)
  .p2align 2
#endif
CNAME(\n):
  .endm

  .macro end_func n
  .size CNAME(\n), .-CNAME(\n)
  .endm

#elif defined (__APPLE__)

#define CNAME(n) _##n

  .macro begin_func n
  .text
  .globl CNAME(\n)
  .p2align 4
CNAME(\n):
  .endm

  .macro end_func n
  .endm

#else
  #error "Only Linux and OSX are supported."
#endif

; vim: ft=asm
