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
        const char *str;
        switch(rv) {
            case UNW_ESUCCESS      : str = "no error"; break;
            case UNW_EUNSPEC       : str = "unspecified (general) error"; break;
            case UNW_ENOMEM        : str = "out of memory"; break;
            case UNW_EBADREG       : str = "bad register number"; break;
            case UNW_EREADONLYREG  : str = "attempt to write read-only register"; break;
            case UNW_ESTOPUNWIND   : str = "stop unwinding"; break;
            case UNW_EINVALIDIP    : str = "invalid IP"; break;
            case UNW_EBADFRAME     : str = "bad frame"; break;
            case UNW_EINVAL        : str = "unsupported operation or bad value"; break;
            case UNW_EBADVERSION   : str = "unwind info has unsupported version"; break;
            case UNW_ENOINFO       : str = "no unwind info found"; break;
            default: str = "unknown error code";
        }
        return str;
    }

    void unw_ctx_from_ss_top(unw_context_t &ctx, uintptr_t sp) {
        memset(&ctx, 0, sizeof(unw_context_t));

        X86_64_GPRs *gprs = reinterpret_cast<X86_64_GPRs*>(&ctx);
        uintptr_t* s = reinterpret_cast<uintptr_t*>(sp);

        gprs->__r15 = s[1];
        gprs->__r14 = s[2];
        gprs->__r13 = s[3];
        gprs->__r12 = s[4];
        gprs->__rbx = s[5];
        gprs->__rbp = s[6];
        gprs->__rip = s[7];
        gprs->__rsp = reinterpret_cast<uintptr_t>(&s[8]);
    }

    YGCursor::YGCursor(YGStack &the_stack): stack(&the_stack) {
        unw_context_t ctx;
        unw_ctx_from_ss_top(ctx, the_stack.sp);

        // We know the LLVM libunwind copies ctx rather than referring to it.
        int rv = unw_init_local(&unw_cursor, &ctx);
        YG_CHECK_UNW(rv, rv == 0);
    }

    void YGCursor::step() {
        uintptr_t pc = _cur_pc();
        if (pc == reinterpret_cast<uintptr_t>(_yg_func_begin_resume)) {
            yg_debug("Function begin frame. Unwind it manually without libunwind.\n");

            uintptr_t sp = _cur_sp();
            
            uintptr_t retaddr = _load_word(sp + 8);
            uintptr_t new_sp = sp + 16;
            yg_debug("Cur ip = %" PRIxPTR ", cur sp = %" PRIxPTR "\n", pc, sp);
            yg_debug("New ip = %" PRIxPTR ", new sp = %" PRIxPTR "\n", retaddr, new_sp);

            yg_debug("Additional stack dump:\n");
            uintptr_t i;
            for (i = sp; i < sp + 80; i+=8) {
                yg_debug("  [%" PRIxPTR "] = %" PRIxPTR "\n", i, _load_word(i));
            }

            _set_cur_pc(retaddr);
            _set_cur_sp(new_sp);
        } else {
            int rv = unw_step(&unw_cursor);
            YG_CHECK_UNW(rv, rv >= 0);
        }
    }

    uintptr_t YGCursor::_cur_pc() {
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

    void YGCursor::_set_cur_pc(uintptr_t new_pc) {
        int rv = unw_set_reg(&unw_cursor, UNW_REG_IP, new_pc);
        YG_CHECK_UNW(rv, rv == 0);
    }

    void YGCursor::_set_cur_sp(uintptr_t new_sp) {
        int rv = unw_set_reg(&unw_cursor, UNW_REG_SP, new_sp);
        YG_CHECK_UNW(rv, rv == 0);
    }

    void YGCursor::pop_frames_to() {
        uintptr_t pc = _cur_pc(), sp = _cur_sp();

        // Set stack->sp to the current CFI.
        yg_debug("new sp = %" PRIxPTR ", new pc = %" PRIxPTR "\n", sp, pc);
        stack->sp = sp;

        // Push the current return address
        stack->_push_return_address(pc); // return address

        // Push an swap-stack top so that it still has the SS return protocol.
        _push_ss_top();
    }

    void YGCursor::_push_ss_top() {
        static int regs[] = {
            UNW_X86_64_RBP,
            UNW_X86_64_RBX,
            UNW_X86_64_R12,
            UNW_X86_64_R13,
            UNW_X86_64_R14,
            UNW_X86_64_R15};

        for (int reg : regs) {
            uintptr_t value;
            int rv = unw_get_reg(&unw_cursor, reg, reinterpret_cast<unw_word_t*>(&value));
            YG_CHECK_UNW(rv, rv == 0);
            stack->_push_word(value);
        }

        stack->_push_word(reinterpret_cast<uintptr_t>(_yg_stack_swap_cont));  // rip = _yg_stack_swap_cont
    }

    void YGCursor::push_frame(uintptr_t func) {
        uintptr_t pc = _cur_pc(), sp = _cur_sp();

        // Remove the swap-stack top on the physical stack.
        stack->sp = sp;

        // Push the current return address
        stack->_push_return_address(pc); // return address

        // Push the function starter frame
        stack->_push_function_starter(func);

        // Push an swap-stack top so that it still has the SS return protocol.
        _push_ss_top();

        uintptr_t new_sp = sp - 16;   // does not include the function starter's return address
        uintptr_t retaddr = reinterpret_cast<uintptr_t>(_yg_func_begin_resume); // this is the "return address".
        _set_cur_sp(new_sp);
        _set_cur_pc(retaddr);
    }
}
