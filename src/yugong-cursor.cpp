#include "yugong.hpp"

#include <cstdint>
#include <cstdlib>

namespace yg {
    YGCursor::YGCursor(YGStack &the_stack): stack(&the_stack) {
        unw_context_t ctx;

        _ss_top_to_unw_context(the_stack, &ctx);

        unw_init_local(&unw_cursor, &ctx);
    }

    void _ss_top_to_unw_context(YGStack &stack, unw_context_t *ctx) {
        uintptr_t* s = reinterpret_cast<uintptr_t*>(stack.sp);

#if defined(__APPLE__)
        ctx->data[15] = s[1];     // r15
        ctx->data[14] = s[2];     // r14
        ctx->data[13] = s[3];     // r13
        ctx->data[12] = s[4];     // r12
        ctx->data[ 1] = s[5];     // rbx
        ctx->data[ 6] = s[6];     // rbp
        ctx->data[16] = s[7];     // rip (return address)
        ctx->data[ 7] = (uintptr_t)&s[8];    // rsp
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
        mctx->gregs[REG_RSP] = (uint64_t)&s[8];    // rsp
#else
#error "Expect __APPLE__ or __linux__"
#endif
    }
}
