#ifndef __YUGONG_HPP__
#define __YUGONG_HPP__

#include "yugong-libunwind-support.hpp"

#include <cstdint>

#define yg_error(fmt, ...) fprintf(stdout, "YG ERROR [%s:%d:%s] " fmt,\
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)

namespace yg {
    // Assumed 64-bit word.  This applies for both x64 and AArch64, but not
    // other archs.
    inline uintptr_t _load_word(uintptr_t addr) {
        return *reinterpret_cast<uintptr_t*>(addr);
    }

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

        inline uintptr_t* _top_buffer() {
            return reinterpret_cast<uintptr_t*>(sp);
        }

        inline void _move_sp(uintptr_t offset) {
            sp += offset;
        }
        
        template<class T>
        inline T* _push_structure() {
            _move_sp(-(sizeof(T)));
            T *ps = reinterpret_cast<T*>(sp);
            return ps;
        }
    };

    struct YGCursor {
        unw_cursor_t unw_cursor;
        YGStack *stack;
            
        YGCursor(YGStack &stack);

        void _init_unw();

        void step();
        uintptr_t _cur_pc();
        uintptr_t _cur_sp();
        void _set_cur_pc(uintptr_t new_pc);
        void _set_cur_sp(uintptr_t new_sp);

        void pop_frames_to();
        void push_frame(uintptr_t func);

        void _reconstruct_ss_top();

        void _get_reg(unw_regnum_t reg, uintptr_t *out);
        void _set_reg(unw_regnum_t reg, uintptr_t newval);
    };
}

extern "C" {
    uintptr_t yg_stack_swap(yg::YGStack *from, yg::YGStack *to, uintptr_t value);

    // ASM-implemented service routines. Not real functions.
    // Don't call from C++!
    void _yg_func_begin_resume();
    void _yg_stack_swap_cont();
}

#endif // __YUGONG_HPP__
