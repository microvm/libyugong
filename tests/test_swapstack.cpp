#include <yugong.hpp>

#include <cstdint>
#include <cstdio>

using namespace yg;

YGStack main_stack, coro_stack;

void sbf(uintptr_t arg) {
    char *name = (char*)arg;
    printf("Hello '%s' from coro!\n", name);

    yg_stack_swap(&coro_stack, &main_stack, 42);
}

int main(int argc, char *argv[]) {
    coro_stack = YGStack::alloc(4096);
    coro_stack.init((uintptr_t)sbf);

    const char *name = "world";

    printf("Hello '%s' from main!\n", name);

    long result = yg_stack_swap(&main_stack, &coro_stack, reinterpret_cast<uintptr_t>(name));

    printf("Welcome back. Result is %ld\n", result);

    coro_stack.free();

    return 0;
}
