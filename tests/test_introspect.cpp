#include <yugong.hpp>

#include <cinttypes>
#include <cstdint>
#include <cstdio>

using namespace yg;

YGStack main_stack, coro_stack;

void h(int m) {
    yg_stack_swap(&coro_stack, &main_stack, m);
}

void g(int n) {
    h(n*10);
}

void f(uintptr_t arg) {
    int n = static_cast<int>(arg);
    g(n+1);

    yg_stack_swap(&coro_stack, &main_stack, 42);
}

int main(int argc, char *argv[]) {
    coro_stack = YGStack::alloc(4096);
    coro_stack.init((uintptr_t)f);

    printf("Before we start, we know that...\n");
    printf("  f = %p\n", f);
    printf("  g = %p\n", g);
    printf("  h = %p\n", h);

    printf("A leap of faith...\n");

    int result = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 42));

    printf("Welcome back. Result is %d\n", result);

    YGCursor cursor(coro_stack);

    int i;

    for(i=0; i<3; i++) {
        uintptr_t pc = cursor.cur_pc();

        printf("  pc = %16" PRIxPTR "\n", pc);

        cursor.step();
    } 

    coro_stack.free();

    return 0;
}
