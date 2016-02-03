#include "yugong.hpp"
#include "yugong-debug.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstring>

namespace yg {
    YGCursor::YGCursor(YGStack &the_stack): stack(&the_stack) {
        _init_unw();
    }

    void YGCursor::_init_unw() {
        memset(&unw_context, 0, sizeof(unw_context));

        _ss_top_to_unw_context(*stack, &unw_context);

        unw_init_local(&unw_cursor, &unw_context);
    }

    void _ss_top_to_unw_context(YGStack &stack, unw_context_t *ctx) {
        uintptr_t* s = reinterpret_cast<uintptr_t*>(stack.sp);

        yg_debug("r15: %" PRIxPTR "\n", s[1]);
        yg_debug("r14: %" PRIxPTR "\n", s[2]);
        yg_debug("r13: %" PRIxPTR "\n", s[3]);
        yg_debug("r12: %" PRIxPTR "\n", s[4]);
        yg_debug("rbx: %" PRIxPTR "\n", s[5]);
        yg_debug("rbp: %" PRIxPTR "\n", s[6]);
        yg_debug("rip: %" PRIxPTR "\n", s[7]);
        yg_debug("rsp: %" PRIxPTR "\n", reinterpret_cast<uintptr_t>(&s[8]));

#if defined(__APPLE__)
        ctx->data[15] = s[1];     // r15
        ctx->data[14] = s[2];     // r14
        ctx->data[13] = s[3];     // r13
        ctx->data[12] = s[4];     // r12
        ctx->data[ 1] = s[5];     // rbx
        ctx->data[ 6] = s[6];     // rbp
        ctx->data[16] = s[7];     // rip (return address)
        ctx->data[ 7] = reinterpret_cast<uintptr_t>(&s[8]);    // rsp
#elif defined(__linux__)
        ucontext_t *uctx = (ucontext_t*)ctx;
        mcontext_t *mctx = &uctx->uc_mcontext;

        // see ucontext_i.h in libunwind
        mctx->gregs[REG_R15] = s[1];     // r15
        mctx->gregs[REG_R14] = s[2];     // r14
        mctx->gregs[REG_R13] = s[3];     // r13
        mctx->gregs[REG_R12] = s[4];     // r12
        mctx->gregs[REG_RBX] = s[5];     // rbx
        mctx->gregs[REG_RBP] = s[6];     // rbp
        mctx->gregs[REG_RIP] = s[7];     // rip (return address)
        mctx->gregs[REG_RSP] = reinterpret_cast<uintptr_t>(&s[8]);    // rsp
#else
#error "Expect __APPLE__ or __linux__"
#endif
    }

    void YGCursor::step() {
        int rv = unw_step(&unw_cursor);
        if (rv < 0) {
#ifdef __APPLE__
            yg_debug("Failed in unw_step. Error: %d\n", rv);
#else
            yg_debug("Failed in unw_step. Error: %d: %s\n", rv, unw_strerror(rv));
#endif
        }
    }

    uintptr_t YGCursor::cur_pc() {
        uintptr_t ip;
        unw_get_reg(&unw_cursor, UNW_REG_IP, reinterpret_cast<unw_word_t*>(&ip));
        return ip;
    }

    void YGCursor::pop_frames_to() {
        _set_sp_to_unw_sp();
        _push_current_return_address();
        _push_ss_top();

        // _init_unw();
        // No need to re-initialize. The stack is modified specifically to match
        // the current unw_*_t states.
    }

    void YGCursor::_set_sp_to_unw_sp() {
        uintptr_t new_sp;
        unw_get_reg(&unw_cursor, UNW_REG_SP, reinterpret_cast<unw_word_t*>(&new_sp));
        yg_debug("new_sp = %" PRIxPTR "\n", new_sp);
        stack->sp = new_sp;
    }

    void YGCursor::_push_current_return_address() {
        uintptr_t rip;
        unw_get_reg(&unw_cursor, UNW_REG_IP,     reinterpret_cast<unw_word_t*>(&rip));
        stack->_push_word(rip); // return address
    }

    void YGCursor::_push_ss_top() {
        uintptr_t rbp, rbx, r12, r13, r14, r15;
        unw_get_reg(&unw_cursor, UNW_X86_64_RBP, reinterpret_cast<unw_word_t*>(&rbp));
        unw_get_reg(&unw_cursor, UNW_X86_64_RBX, reinterpret_cast<unw_word_t*>(&rbx));
        unw_get_reg(&unw_cursor, UNW_X86_64_R12, reinterpret_cast<unw_word_t*>(&r12));
        unw_get_reg(&unw_cursor, UNW_X86_64_R13, reinterpret_cast<unw_word_t*>(&r13));
        unw_get_reg(&unw_cursor, UNW_X86_64_R14, reinterpret_cast<unw_word_t*>(&r14));
        unw_get_reg(&unw_cursor, UNW_X86_64_R15, reinterpret_cast<unw_word_t*>(&r15));

        stack->_push_word(rbp);
        stack->_push_word(rbx);
        stack->_push_word(r12);
        stack->_push_word(r13);
        stack->_push_word(r14);
        stack->_push_word(r15);

        stack->_push_word(reinterpret_cast<uintptr_t>(_yg_stack_swap_cont));  // rip = _yg_stack_swap_cont
    }

    void YGCursor::push_frame(uintptr_t func) {
        _set_sp_to_unw_sp();
        _push_current_return_address();
        stack->_push_function_starter(func);
        _push_ss_top();

        _init_unw(); // The stack top changed. Re-initailize unw_*_t
    }
}
