#include "yugong.hpp"

#include <cstdint>
#include <cstdlib>

namespace yg {

    void YGStack::alloc(YGStack &stack, uintptr_t size) {
        void *mem = std::malloc(size);
        uintptr_t start = reinterpret_cast<uintptr_t>(mem);

        stack.start = start;
        stack.size = size;
        stack.sp = start + size;
    }

    void YGStack::free(YGStack &stack) {
        void *mem = reinterpret_cast<void*>(stack.start);
        std::free(mem);
    }

    void ll::push_initial_frame(YGStack &stack, FuncPtr func) {
        uintptr_t *stack_bottom = reinterpret_cast<uintptr_t*>(stack.start + stack.size);
        stack_bottom[-1] = 0;
        stack_bottom[-2] = reinterpret_cast<uintptr_t>(func);
        stack_bottom[-3] = reinterpret_cast<uintptr_t>(_yg_func_begin_resume);
        stack.sp         = reinterpret_cast<uintptr_t>(&stack_bottom[-3]);
    }

}
