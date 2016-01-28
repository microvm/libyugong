#ifndef __YUGONG_HPP__
#define __YUGONG_HPP__

#include <cstdint>

namespace yg {
    typedef void *Ptr;
    typedef void (*FuncPtr)();

    struct YGStack {
        uintptr_t sp;
        uintptr_t start;
        uintptr_t size;

        static void alloc(YGStack &stack, uintptr_t size);
        static void free(YGStack &stack);
    };

    namespace ll {
        void push_initial_frame(YGStack &stack, FuncPtr func);
    }
}

extern "C" {
    uintptr_t yg_stack_swap(yg::YGStack *from, yg::YGStack *to, uintptr_t value);

    // ASM-implemented service routines. Not real functions.
    // Don't call from C++!
    void _yg_func_begin_resume();
}

#endif // __YUGONG_HPP__
