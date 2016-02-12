#include <yugong.hpp>
#include "test_common.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace yg;

YGStack main_stack, coro_stack;

long factorial(long n) {
    if (n == 0) {
        return 1;
    } else {
        return n * factorial(n-1);
    }
}

long time_two(long n) {
    return n * 2;
}

long plus_one(long n) {
    return n + 1;
}

void bot(long result) {
    yg_stack_swap(&coro_stack, &main_stack, static_cast<uintptr_t>(result));
}

int main(int argc, char *argv[]) {
    coro_stack = YGStack::alloc(4096);
    coro_stack.init((uintptr_t)bot);

    ygt_print("Before we start, we know that...\n");
    ygt_print("  coro_stack.begin = %" PRIxPTR "\n", coro_stack.start);
    ygt_print("  coro_stack.size = %" PRIxPTR "\n", coro_stack.size);
    ygt_print("  coro_stack ends at %" PRIxPTR "\n", coro_stack.start + coro_stack.size);

    std::map<uintptr_t, std::string> func_map;
    func_map[reinterpret_cast<uintptr_t>(bot                    )] = "bot";
    func_map[reinterpret_cast<uintptr_t>(factorial              )] = "factorial";
    func_map[reinterpret_cast<uintptr_t>(time_two               )] = "time_two";
    func_map[reinterpret_cast<uintptr_t>(plus_one               )] = "plus_one";
    func_map[reinterpret_cast<uintptr_t>(_yg_func_begin_resume  )] = "_yg_func_begin_resume";
    func_map[reinterpret_cast<uintptr_t>(_yg_stack_swap_cont    )] = "_yg_stack_swap_cont";

    for (auto &kw : func_map) {
        ygt_print("  %s = %" PRIxPTR "\n", kw.second.c_str(), kw.first);
    }

    ygt_print("We want to compute factorial(5*2) + 1 which should be 3628801.\n");

    ygt_print("Make a cursor...\n");
    YGCursor cursor(coro_stack);

    ygt_print("  Push plus_one...\n");
    cursor.push_frame(reinterpret_cast<uintptr_t>(plus_one));

    ygt_print("  Push factorial...\n");
    cursor.push_frame(reinterpret_cast<uintptr_t>(factorial));

    ygt_print("  Push time_two...\n");
    cursor.push_frame(reinterpret_cast<uintptr_t>(time_two));

    ygt_print("Now let's see what frames are on the coro_stack. We will stop at bot.\n");

    int i;
    for(i=0; i<10; i++) {
        uintptr_t cur_pc = cursor._cur_pc();
        ygt_print("  pc = %" PRIxPTR "\n", cur_pc);

        if (cur_pc == reinterpret_cast<uintptr_t>(_yg_func_begin_resume)) {
            uintptr_t cont_pc = _load_word(cursor._cur_sp());
            
            auto entry = func_map.find(cont_pc);
            std::string name;
            if (entry == func_map.end()) {
                name = "(Unknown function)";
            } else {
                name = entry->second;
            }

            ygt_print("    Func begin frame for func %" PRIxPTR ": %s\n", cont_pc, name.c_str());

            if(cont_pc == reinterpret_cast<uintptr_t>(bot)) {
                ygt_print("    This is bot. Break.\n");
                break;
            }
        } else {
            ygt_print("    Unknown function.\n");
        }

        cursor.step();
    }

    if (i == 10) {
        ygt_print("ERROR: The cursor has gone too far. Terminate.\n");
        return 1;
    }

    ygt_print("Swap-stack, passing value 5...\n");
    long result = yg_stack_swap(&main_stack, &coro_stack, 5);

    ygt_print("Welcome back!\n");

    ygt_print("  result = %ld\n", result);

    coro_stack.free();

    return 0;
}
