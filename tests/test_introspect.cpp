#include <yugong.hpp>
#include "test_common.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>

using namespace yg;

YGStack main_stack, coro_stack;

void h(int m) {
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));
    yg_stack_swap(&coro_stack, &main_stack, m);
}

void g(int n) {
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));
    h(n*10);
}

void f(uintptr_t arg) {
    int n = static_cast<int>(arg);
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));
    g(n+1);

    yg_stack_swap(&coro_stack, &main_stack, 999);
}

int main(int argc, char *argv[]) {
    coro_stack = YGStack::alloc(4096);
    coro_stack.init((uintptr_t)f);

    ygt_print("Before we start, we know that...\n");
    ygt_print("  coro_stack.begin = %" PRIxPTR "\n", coro_stack.start);
    ygt_print("  coro_stack.size = %" PRIxPTR "\n", coro_stack.size);
    ygt_print("  coro_stack ends at %" PRIxPTR "\n", coro_stack.start + coro_stack.size);
    ygt_print("  f = %p\n", f);
    ygt_print("  g = %p\n", g);
    ygt_print("  h = %p\n", h);

    ygt_print("A leap of faith...\n");

    int result = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 42));

    ygt_print("Welcome back. Result is %d\n", result);

    YGCursor cursor(coro_stack);

    int i;

    for(i=0; i<3; i++) {
        uintptr_t pc = cursor.cur_pc();

        ygt_print("  pc = %" PRIxPTR "\n", pc);

        cursor.step();
    } 

    ygt_print("Let coro continue...\n");

    int result2 = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 0));

    ygt_print("Welcome back. Result2 is %d\n", result2);

    coro_stack.free();

    return 0;
}
