#ifndef __LIBUNWIND_SUPPORT_HPP__
#define __LIBUNWIND_SUPPORT_HPP__

#include <libunwind.h>

namespace yg {
    
    // Taken from LLVM libunwind. We have to hack into the impl details.
    struct X86_64_GPRs {
        uint64_t __rax;
        uint64_t __rbx;
        uint64_t __rcx;
        uint64_t __rdx;
        uint64_t __rdi;
        uint64_t __rsi;
        uint64_t __rbp;
        uint64_t __rsp;
        uint64_t __r8;
        uint64_t __r9;
        uint64_t __r10;
        uint64_t __r11;
        uint64_t __r12;
        uint64_t __r13;
        uint64_t __r14;
        uint64_t __r15;
        uint64_t __rip;
        uint64_t __rflags;
        uint64_t __cs;
        uint64_t __fs;
        uint64_t __gs;
    };
}

#endif // __LIBUNWIND_SUPPORT_HPP__
