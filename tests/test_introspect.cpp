#include <yugong.hpp>

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

    printf("A leap of faith...\n");

    int result = static_cast<int>(yg_stack_swap(&main_stack, &coro_stack, 42));

    printf("Welcome back. Result is %d\n", result);

    coro_stack.free();

    return 0;
}
