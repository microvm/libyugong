#include "yugong.hpp"

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
        _push_return_address(0);
        _push_function_starter(func);
        _push_empty_ss_top();
    }

    void YGStack::_push_return_address(uintptr_t addr) {
        _push_word(addr);
    }

    void YGStack::_push_function_starter(uintptr_t func) {
        _push_word(reinterpret_cast<uintptr_t>(func));
        _push_word(reinterpret_cast<uintptr_t>(_yg_func_begin_resume));
    }

    void YGStack::_push_empty_ss_top() {
        _push_word(0);  // rbp = 0
        _push_word(0);  // rbx = 0
        _push_word(0);  // r12 = 0
        _push_word(0);  // r13 = 0
        _push_word(0);  // r14 = 0
        _push_word(0);  // r15 = 0
        _push_word(reinterpret_cast<uintptr_t>(_yg_stack_swap_cont));  // rip = _yg_stack_swap_cont
    }
}
