#include "yugong.hpp"
#include "yugong-stacktop.hpp"

#include <cstdint>
#include <cstdlib>

namespace yg {

    YGStack YGStack::alloc(uintptr_t size) {
        void *mem = std::malloc(size);
        uintptr_t start = reinterpret_cast<uintptr_t>(mem);
        uintptr_t sp = start + size;

        return YGStack(sp, start, size);
    }

    void YGStack::free() {
        void *mem = reinterpret_cast<void*>(start);
        std::free(mem);
    }

    void YGStack::init(uintptr_t func) {
#if defined(__x86_64__)
        _move_sp(-(sizeof(YGInitialStackTop_x86_64)));
        YGInitialStackTop_x86_64 *init_top =
            reinterpret_cast<YGInitialStackTop_x86_64*>(sp);

        init_top->ss_top.ss_cont = reinterpret_cast<uintptr_t>(_yg_stack_swap_cont);
        init_top->ss_top.ret_addr = reinterpret_cast<uintptr_t>(_yg_func_begin_resume);
        init_top->rop_frame.func_addr = func;
#elif defined(__arm64__) || defined(__aarch64__)
        _move_sp(-(sizeof(YGInitialStackTop_aarch64)));
        YGInitialStackTop_aarch64 *init_top =
            reinterpret_cast<YGInitialStackTop_aarch64*>(sp);

        init_top->ss_top.ss_cont = reinterpret_cast<uintptr_t>(_yg_stack_swap_cont);
        init_top->ss_top.lr() = reinterpret_cast<uintptr_t>(_yg_func_begin_resume);
        init_top->rop_frame.func_addr = func;
#endif
    }
}
