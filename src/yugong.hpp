#ifndef __YUGONG_HPP__
#define __YUGONG_HPP__

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE
#include <sys/ucontext.h>
#endif // __linux__

#ifndef UNW_LOCAL_ONLY
#define UNW_LOCAL_ONLY
#endif // UNW_LOCAL_ONLY
#include <libunwind.h>

#include <cstdint>

namespace yg {
    inline void _store_word(uintptr_t addr, uintptr_t word) {
        *reinterpret_cast<uintptr_t*>(addr) = word;
    }

    struct YGStack {
        uintptr_t sp;
        uintptr_t start;
        uintptr_t size;

        YGStack() {}
        YGStack(uintptr_t sp, uintptr_t start, uintptr_t size)
            : sp(sp), start(start), size(size) {}

        static YGStack alloc(uintptr_t size);
        void free();

        void init(uintptr_t func);

        inline void _push_word(uintptr_t word) {
            sp -= sizeof(uintptr_t), 
            _store_word(sp, word);
        }

        inline YGStack& operator<<(uintptr_t word) {
            _push_word(word);
            return *this;
        }

        void _push_return_address(uintptr_t addr);
        void _push_function_starter(uintptr_t func);
        void _push_empty_ss_top();
    };

    struct YGCursor {
        unw_context_t unw_context;
        unw_cursor_t unw_cursor;
        YGStack *stack;
            
        YGCursor(YGStack &stack);

        void step();
        uintptr_t cur_pc();

        void pop_frames_to();
        void push_frame(uintptr_t func);

        void _push_ss_top();
    };

    void _ss_top_to_unw_context(YGStack &stack, unw_context_t *ctx);
}

extern "C" {
    uintptr_t yg_stack_swap(yg::YGStack *from, yg::YGStack *to, uintptr_t value);

    // ASM-implemented service routines. Not real functions.
    // Don't call from C++!
    void _yg_func_begin_resume();
    void _yg_stack_swap_cont();
}

#endif // __YUGONG_HPP__
