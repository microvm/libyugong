#include "yugong.hpp"
#include "yugong-debug.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace yg {
    YGCursor::YGCursor(YGStack &the_stack): stack(&the_stack) {
        memset(&unw_context, 0, sizeof(unw_context));

        _ss_top_to_unw_context(the_stack, &unw_context);

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
            yg_debug("Failed in unw_step. Error: %d: %s\n", rv, unw_strerror(rv));
        }
    }

    uintptr_t YGCursor::cur_pc() {
        uintptr_t ip;
        unw_get_reg(&unw_cursor, UNW_REG_IP, reinterpret_cast<unw_word_t*>(&ip));
        return ip;
    }
}
