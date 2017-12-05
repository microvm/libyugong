#include "yugong.hpp"
#include "yugong-stacktop.hpp"
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
        uintptr_t* s = reinterpret_cast<uintptr_t*>(sp);

#if defined(__x86_64__)
        Registers_x86_64 *regs = reinterpret_cast<Registers_x86_64*>(&ctx);
        Registers_x86_64::GPRs *gprs = &regs->_registers;
        gprs->__r15 = s[1];
        gprs->__r14 = s[2];
        gprs->__r13 = s[3];
        gprs->__r12 = s[4];
        gprs->__rbx = s[5];
        gprs->__rbp = s[6];
        gprs->__rip = s[7];
        gprs->__rsp = reinterpret_cast<uintptr_t>(&s[8]);
#elif defined(__arm64__) || defined(__aarch64__)
        Registers_arm64 *regs = reinterpret_cast<Registers_x86_64>(&ctx);
        Registers_arm64::GPRs *gprs = &regs->_registers;
#error "Not implemented yet"
#endif
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
            if (rv == 0) {
                yg_debug("unw_step returned 0. Last frame?\n");
            }
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
        _set_reg(UNW_REG_IP, new_pc);
    }

    void YGCursor::_set_cur_sp(uintptr_t new_sp) {
        _set_reg(UNW_REG_SP, new_sp);
    }

    void YGCursor::pop_frames_to() {
        uintptr_t pc = _cur_pc();
        uintptr_t sp = _cur_sp();

        // Set stack->sp to the current CFI.
        yg_debug("new sp = %" PRIxPTR ", new pc = %" PRIxPTR "\n", sp, pc);
        stack->sp = sp;

        _reconstruct_ss_top();
    }

    void YGCursor::_reconstruct_ss_top() {
#if defined(__x86_64__)
        YGStackTop_x86_64 *ss_top = stack->_push_structure<YGStackTop_x86_64>();

        ss_top->ss_cont = reinterpret_cast<uintptr_t>(_yg_stack_swap_cont);
        _get_reg(UNW_X86_64_R15, &ss_top->r15);
        _get_reg(UNW_X86_64_R14, &ss_top->r14);
        _get_reg(UNW_X86_64_R13, &ss_top->r13);
        _get_reg(UNW_X86_64_R12, &ss_top->r12);
        _get_reg(UNW_X86_64_RBX, &ss_top->rbx);
        _get_reg(UNW_X86_64_RBP, &ss_top->rbp);
        _get_reg(UNW_REG_IP    , &ss_top->ret_addr);
#elif defined(__arm64__) || defined(__aarch64__)
#error unimplemented
#endif
    }

    void YGCursor::push_frame(uintptr_t func) {
#if defined(__x86_64__)
        uintptr_t pc = _cur_pc(), sp = _cur_sp();

        // Remove the swap-stack top on the physical stack.
        stack->sp = sp;

        // Construct a ROP frame
        YGRopFrame_x86_64 *rop_frame = stack->_push_structure<YGRopFrame_x86_64>();

        // The current return address is saved in the ROP frame so that after
        // `func` returns, we resume from the current PC.
        rop_frame->saved_ret_addr = pc;

        // The function address
        rop_frame->func_addr = func;

        // The ROP frame should be entered from the adapter, not `func`.
        // Therefore we set the current PC to the adapter.
        _set_reg(UNW_REG_IP, reinterpret_cast<uintptr_t>(_yg_func_begin_resume));

        // The SP has changed because we just pushed a ROP frame.
        _set_reg(UNW_REG_SP, stack->sp);

        // Reconstruct the swap-stack top so that the stack top always has the
        // swap-stack resumption protocol.
        _reconstruct_ss_top();
#elif defined(__arm64__) || defined(__aarch64__)
#error unimplemented
#endif
    }

    void YGCursor::_get_reg(unw_regnum_t reg, uintptr_t *value) {
        int rv = unw_get_reg(&unw_cursor, reg, reinterpret_cast<unw_word_t*>(value));
        YG_CHECK_UNW(rv, rv == 0);
    }

    void YGCursor::_set_reg(unw_regnum_t reg, uintptr_t newval) {
        int rv = unw_set_reg(&unw_cursor, reg, static_cast<unw_word_t>(newval));
        YG_CHECK_UNW(rv, rv == 0);
    }
}
