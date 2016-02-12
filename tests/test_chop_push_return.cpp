#include <yugong.hpp>
#include "test_common.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace yg;

YGStack main_stack, coro_stack;

int h2(int param) {
    ygt_print("Hi! I am h2. You have never called me, haven you?\n");
    ygt_print("But I am still here. param = %d\n", param);

    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));

    return param*3;
}

int h(int m) {
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));

    yg_stack_swap(&coro_stack, &main_stack, m);

    return m*2;
}

int g(int n) {
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));

    int result = h(n*10);
    ygt_print("The result is %d\n", result);

    return result;
}

void f(uintptr_t arg) {
    int n = static_cast<int>(arg);
    ygt_print("my return addr is %p\n", __builtin_return_address(0));
    ygt_print("my frame ptr is %p\n", __builtin_frame_address(0));

    int result = g(n+1);
    ygt_print("The result is %d\n", result);

    yg_stack_swap(&coro_stack, &main_stack, result);
}

void print_stack_dump(uintptr_t sp) {
    ygt_print("Stack dump:\n");
    uintptr_t addr;
    for (addr = sp; addr < sp + 160; addr+=8) {
        ygt_print("  [%" PRIxPTR "] = %" PRIxPTR "\n", addr, _load_word(addr));
    }
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
    ygt_print("  h2 = %p\n", h2);
    ygt_print("  _yg_func_begin_resume = %p\n", _yg_func_begin_resume);

    ygt_print("A leap of faith...\n");

    int result = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 42));

    ygt_print("Welcome back. Result is %d\n", result);

    YGCursor cursor(coro_stack);

    int i;

    for(i=0; i<3; i++) {
        uintptr_t pc = cursor._cur_pc();

        ygt_print("  pc = %" PRIxPTR "\n", pc);

        cursor.step();
    } 

    print_stack_dump(coro_stack.sp);

    if (argc > 1) {
        ygt_print("Now we will modify coro_stack.\n");
        int n;
        if (sscanf(argv[1], "%d", &n) != 1) {
            ygt_print("ERROR: Command line argument must be int.\n");
            exit(1);
        }

        ygt_print("We will replace h with %d frames for h2, and then return.\n", n);

        ygt_print("Create another cursor...\n");
        YGCursor cursor2(coro_stack);

        ygt_print("Go down one step...\n");
        cursor2.step();

        ygt_print("Pop frames above this...\n");
        cursor2.pop_frames_to();

        ygt_print("... and push %d new frames for h2.\n", n);

        int i;
        for (i = 0; i < n; i++) {
            ygt_print("  Frame %d...\n", i);
            cursor2.push_frame(reinterpret_cast<uintptr_t>(h2));
            //cursor2.push_frame(0xdeadbeeflu);
        }
    } else {
        ygt_print("Not modifying coro_stack now.\n");
        ygt_print("** Invoke %s <n> to replace h with n instances of h2 frames on coro_stack\n", argv[0]);
    }

    print_stack_dump(coro_stack.sp);

    ygt_print("Let coro continue...\n");
    int result2 = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 1000));

    ygt_print("Welcome back. Result2 is %d\n", result2);

    coro_stack.free();

    return 0;
}
