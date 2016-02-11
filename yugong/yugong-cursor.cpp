#include "yugong.hpp"
#include "yugong-debug.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstring>

#define YG_CHECK_UNW(rv, pred) if (!(pred)) {\
    yg_error("libunwind error %d: %s\n", rv, _yg_unw_strerror(rv));\
}

namespace yg {
    const char *_yg_unw_strerror(int rv) {
#if defined(__APPLE__)
        return "(unw_strerror not available on OSX)";
#else
        return "(unw_strerror not available on Linux, either)";
        //return unw_strerror(rv);
#endif
    }

    void YGContext::from_ss_top(uintptr_t sp) {
        uintptr_t* s = reinterpret_cast<uintptr_t*>(sp);

        r15 = s[1];
        r14 = s[2];
        r13 = s[3];
        r12 = s[4];
        rbx = s[5];
        rbp = s[6];
        rip = s[7];
        rsp = reinterpret_cast<uintptr_t>(&s[8]);
    }

    void YGCursor::_load_yg_ctx(YGContext &yg_ctx) {
        yg_debug("Reset libunwind context:\n");
        yg_debug("  r15: %" PRIxPTR "\n", yg_ctx.r15);
        yg_debug("  r14: %" PRIxPTR "\n", yg_ctx.r14);
        yg_debug("  r13: %" PRIxPTR "\n", yg_ctx.r13);
        yg_debug("  r12: %" PRIxPTR "\n", yg_ctx.r12);
        yg_debug("  rbx: %" PRIxPTR "\n", yg_ctx.rbx);
        yg_debug("  rbp: %" PRIxPTR "\n", yg_ctx.rbp);
        yg_debug("  rip: %" PRIxPTR "\n", yg_ctx.rip);
        yg_debug("  rsp: %" PRIxPTR "\n", yg_ctx.rsp);

        memset(&unw_context, 0, sizeof(unw_context));

#if defined(__APPLE__)
        unw_context.data[15] = yg_ctx.r15;
        unw_context.data[14] = yg_ctx.r14;
        unw_context.data[13] = yg_ctx.r13;
        unw_context.data[12] = yg_ctx.r12;
        unw_context.data[ 1] = yg_ctx.rbx;
        unw_context.data[ 6] = yg_ctx.rbp;
        unw_context.data[16] = yg_ctx.rip;
        unw_context.data[ 7] = yg_ctx.rsp;
#elif defined(__linux__)
        ucontext_t *uctx = reinterpret_cast<ucontext_t*>(&unw_context);
        mcontext_t *mctx = &uctx->uc_mcontext;

        // see ucontext_i.h in libunwind
        mctx->gregs[REG_R15] = yg_ctx.r15;
        mctx->gregs[REG_R14] = yg_ctx.r14;
        mctx->gregs[REG_R13] = yg_ctx.r13;
        mctx->gregs[REG_R12] = yg_ctx.r12;
        mctx->gregs[REG_RBX] = yg_ctx.rbx;
        mctx->gregs[REG_RBP] = yg_ctx.rbp;
        mctx->gregs[REG_RIP] = yg_ctx.rip;
        mctx->gregs[REG_RSP] = yg_ctx.rsp;
#else
#error "Expect __APPLE__ or __linux__"
#endif

        int rv = unw_init_local(&unw_cursor, &unw_context);
        YG_CHECK_UNW(rv, rv == 0);
    }

    void YGCursor::_dump_yg_ctx(YGContext &yg_ctx) {
        int rv;
        rv = unw_get_reg(&unw_cursor, UNW_REG_SP    , reinterpret_cast<unw_word_t*>(&yg_ctx.rsp)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_REG_IP    , reinterpret_cast<unw_word_t*>(&yg_ctx.rip)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_RBP, reinterpret_cast<unw_word_t*>(&yg_ctx.rbp)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_RBX, reinterpret_cast<unw_word_t*>(&yg_ctx.rbx)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_R12, reinterpret_cast<unw_word_t*>(&yg_ctx.r12)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_R13, reinterpret_cast<unw_word_t*>(&yg_ctx.r13)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_R14, reinterpret_cast<unw_word_t*>(&yg_ctx.r14)); YG_CHECK_UNW(rv, rv == 0);
        rv = unw_get_reg(&unw_cursor, UNW_X86_64_R15, reinterpret_cast<unw_word_t*>(&yg_ctx.r15)); YG_CHECK_UNW(rv, rv == 0);
    }

    YGCursor::YGCursor(YGStack &the_stack): stack(&the_stack) {
        _init_unw();
    }

    void YGCursor::_init_unw() {
        YGContext yg_ctx;
        yg_ctx.from_ss_top(stack->sp);
        _load_yg_ctx(yg_ctx);
    }

    void YGCursor::step() {
        uintptr_t pc = cur_pc();
        if (pc == reinterpret_cast<uintptr_t>(_yg_func_begin_resume)) {
            yg_debug("Function begin frame. Unwind it manually without libunwind.\n");

            YGContext yg_ctx;
            _dump_yg_ctx(yg_ctx);

            uintptr_t sp = yg_ctx.rsp;
            uintptr_t retaddr = _load_word(sp + 8);
            uintptr_t new_sp = sp + 16;
            yg_debug("Cur ip = %" PRIxPTR ", cur sp = %" PRIxPTR "\n", pc, sp);
            yg_debug("New ip = %" PRIxPTR ", new sp = %" PRIxPTR "\n", retaddr, new_sp);

            yg_debug("Additional stack dump:\n");
            uintptr_t i;
            for (i = sp; i < sp + 80; i+=8) {
                yg_debug("  [%" PRIxPTR "] = %" PRIxPTR "\n", i, _load_word(i));
            }

            yg_ctx.rip = retaddr;
            yg_ctx.rsp = new_sp;
            _load_yg_ctx(yg_ctx);
        } else {
            int rv = unw_step(&unw_cursor);
            YG_CHECK_UNW(rv, rv >= 0);
        }
    }

    uintptr_t YGCursor::cur_pc() {
        uintptr_t ip;
        int rv = unw_get_reg(&unw_cursor, UNW_REG_IP, reinterpret_cast<unw_word_t*>(&ip));
        YG_CHECK_UNW(rv, rv == 0);
        return ip;
    }

    uintptr_t YGCursor::_cur_sp() {
        uintptr_t sp;
        int rv = unw_get_reg(&unw_cursor, UNW_REG_SP, reinterpret_cast<unw_word_t*>(&sp));
        YG_CHECK_UNW(rv, rv == 0);
        return sp;
    }

    void YGCursor::pop_frames_to() {
        YGContext yg_ctx;
        _dump_yg_ctx(yg_ctx);

        // Set stack->sp to the current CFI.
        yg_debug("new sp = %" PRIxPTR ", new pc = %" PRIxPTR "\n", yg_ctx.rsp, yg_ctx.rip);
        stack->sp = yg_ctx.rsp;

        // Push the current return address
        stack->_push_return_address(yg_ctx.rip); // return address

        // Push an swap-stack top so that it still has the SS return protocol.
        _push_ss_top(yg_ctx);
    }

    void YGCursor::_push_ss_top(YGContext &yg_ctx) {
        stack->_push_word(yg_ctx.rbp);
        stack->_push_word(yg_ctx.rbx);
        stack->_push_word(yg_ctx.r12);
        stack->_push_word(yg_ctx.r13);
        stack->_push_word(yg_ctx.r14);
        stack->_push_word(yg_ctx.r15);

        yg_debug("Now rbp is %" PRIxPTR "\n", yg_ctx.rbp);

        stack->_push_word(reinterpret_cast<uintptr_t>(_yg_stack_swap_cont));  // rip = _yg_stack_swap_cont
    }

    void YGCursor::push_frame(uintptr_t func) {
        YGContext yg_ctx;
        _dump_yg_ctx(yg_ctx);

        // Remove the swap-stack top on the physical stack.
        stack->sp = yg_ctx.rsp;

        // Push the current return address
        stack->_push_return_address(yg_ctx.rip); // return address

        // Push the function starter frame
        stack->_push_function_starter(func);

        // Push an swap-stack top so that it still has the SS return protocol.
        _push_ss_top(yg_ctx);

        yg_ctx.rsp -= 16;   // does not include the function starter's return address
        yg_ctx.rip = reinterpret_cast<uintptr_t>(_yg_func_begin_resume); // this is the "return address".
        _load_yg_ctx(yg_ctx); // The stack top changed. Re-initailize unw_*_t
    }
}
