#ifndef __LIBUNWIND_SUPPORT_HPP__
#define __LIBUNWIND_SUPPORT_HPP__

#include <libunwind.h>

namespace yg {

#if defined(__x86_64__)
    // Taken from LLVM libunwind. We have to hack into the impl details.
    struct Registers_x86_64 {
        struct GPRs {
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
        GPRs _registers;
    };
#elif defined(__arm64__) || defined(__aarch64__)
    struct Registers_arm64 {
        struct GPRs {
            uint64_t __x[29]; // x0-x28
            uint64_t __fp;    // Frame pointer x29
            uint64_t __lr;    // Link register x30
            uint64_t __sp;    // Stack pointer x31
            uint64_t __pc;    // Program counter
            uint64_t padding; // 16-byte align
        };

        GPRs    _registers;
        double  _vectorHalfRegisters[32];
    };
#endif
}

#endif // __LIBUNWIND_SUPPORT_HPP__
