#ifndef __YUGONG_STACKTOP_HPP__
#define __YUGONG_STACKTOP_HPP__

namespace yg {

#if defined(__x86_64__)
    struct YGStackTop_x86_64 {
        // 16-byte aligned
        uintptr_t ss_cont;          // Address of the _yg_stack_swap_cont
        uintptr_t r15;
        uintptr_t r14;
        uintptr_t r13;
        uintptr_t r12;
        uintptr_t rbx;
        uintptr_t rbp;
        uintptr_t ret_addr;
    };

    struct YGRopFrame_x86_64 {
        uintptr_t func_addr;
        uintptr_t saved_ret_addr;
    };

    struct YGInitialStackTop_x86_64 {
        YGStackTop_x86_64 ss_top;
        YGRopFrame_x86_64 rop_frame;
    };
#elif defined(__arm64__) || defined(__aarch64__)
    struct YGStackTop_aarch64 {
        uintptr_t ss_cont;
        uintptr_t unused;

        double d8_to_d15[15-8+1];
        uintptr_t x19_to_x30[30-19+1];
        uintptr_t& fp() { return x19_to_x30[29-19]; }  // Frame pointer (x29)
        uintptr_t& lr() { return x19_to_x30[30-19]; }  // Link register (x30, resumption point of the caller to yg_stack_swap)
    };

    struct YGRopFrame_aarch64 {
        uintptr_t func_addr;
        uintptr_t saved_lr;
    };

    struct YGInitialStackTop_aarch64 {
        YGStackTop_aarch64 ss_top;
        YGRopFrame_aarch64 rop_frame;
    };
#endif
}

#endif // __YUGONG_STACKTOP_HPP__
