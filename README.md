# libyugong

*Yu Gong is the protagonist in the story of [The Foolish Old Man
Removes the
Mountains](https://en.wikipedia.org/wiki/The_Foolish_Old_Man_Removes_the_Mountains)
in Chinese myths.*

libyugong is an LLVM-based platform-independent API for stack introspection and
on-stack replacement.

This library has two parts:

1. An LLVM helper library that generates and manages stackmaps and their
   metadata at compile time.

2. A run-time library that actually performs stack unwinding, stack
   introspection and on-stack replacement.

A JIT compiler usually has both of them at run time.

This library currently only supports x86\_64 on OSX and Linux.

# How to build

Dependencies:

* libunwind: Installed by default on OSX. You need to install it manually on
  Linux.

* LLVM 3.7: This library is developed and tested on LLVM 3.7. Older versions may
  work, too.

Just type `make` to build.

# How to use

todo

Functions starting with `yg_ll_` are low-level functions. They deal with the
"resumption protocols". Functions starting with `yg_` are part of the high-level
API.

High-level API functions (may also be defined in C++):

* `void yg_stack_new(YGStack *stack, uintptr_t size)`: create new stack.
  The `YGStack` object is canonical and cannot be copied.

* `uintptr_t yg_stack_swap(YGStack *cur_stack, YGStack *new_stack, uintptr_t
  value)`: swap stack

* `void yg_stack_free(YGStack *stack)`: deallocate a stack

* `void yg_cursor_new(YGCursor *cursor, YGStack *stack)`: create a cursor. The
  cursor has a pointer to the underlying `YGStack`. Cursors can be copied
  by `memcpy`.

* `void yg_cursor_step(YGCursor *cursor)`: move cursor towards the stack bottom
  by one frame

* `YGCSID yg_get_csid(YGCursor *cursor)`: get the call-site ID at the current
  frame

* `void yg_dump_keepalives(YGCursor *cursor)`: dump the keep-alive variables at
  the current frame

* `void yg_pop_frames_to(YGCursor *cursor)`: pop all frames of the stack above
  (not including) the cursor. Modifies both the stack and the cursor so that
  `cursor` now refers to the new top frame.

* `void yg_push_frame(YGCursor *cursor, YGFP *fp)`: push a frame on the top of
  the stack. `cursor` must point to the top frames.  Modifies both the stack and
  the cursor so that `cursor` now refers to the new top frame. `fp` must be a
  0-ary or unary function.

Low-level functions:

* `yg_ll_cursor_new`, `yg_push_adapter_frame`, ...

# Stack

A stack can be allocated anywhere in the read-write memory. A reference to a
paused stack is a struct that contains the current SP, the beginning of its
stack memory region and its size.

```c
struct yg_stack {
    uintptr_t sp;
    uintptr_t start;
    uintptr_t size;
};
```

Resuming a stack only needs the SP, while the latter two are only useful when
de-allocating the stack.

On x86\_64, SP is always 8-byte aligned, but not always 16-byte aligned.

# Resumption protocol

A resumption protocol specifies how to resume a frame. There are several
protocols defined by this library, including `SS`, `C-Ret`, `C-Call`, to name a
few.

When resuming, zero or more values can be passed to the frame resumed. The
number of values may be limited by the protocol.

The state of a frame is described as a pair `(RP, <Ts>)` where RP is the
resumption protocol and Ts is the list of the types of the values to be passed
in.

The top frame of a stack is usually in the `SS` protocol, since this protocol
does not require any values other than the saved SP and the value to be passed
in.

## `SS`: Swap-stack

On the swappee stack, the saved SP points to the saved PC. The thread should set
the RSP register to the saved SP and pop the saved PC to the RIP register.

```
low address
+-------------------+
| RIP               |   <- saved SP
+-------------------+
| (anything else)   |
| ...               |
+-------------------+
high address
```

An `uintptr_t` value can be passed via swap-stack from the swapper to the
swappee. On x86\_64, this value is expected to be in the RAX register when
popping the saved PC.

The SP may or may not be 16-byte aligned.

## `C-Ret`: C calling convention return

This is the protocol for returning to a function that is at a call site that
follows the [System V ABI for
AMD64](http://www.x86-64.org/documentation/abi.pdf). Most function calls have
this protocol. See the "function return" part of the ABI for the expected
register values.

The SP must be 16-byte aligned *after* popping the return address.

## `C-Call`: C calling convention call-site

This kind of frame will return to the beginning of a function, which is before
the prologue and is not a call site. Such frames can only be created by OSR.

```
low address
+-------------------+
| to service routine|   <- 16-byte unaligned. SP points to here.
+-------------------+
| func ptr          |   <- 16-byte aligned
+-------------------+ ------ Everything below belongs to the caller.
| return addr       |   <- 16-byte unaligned. SP points here after popping func ptr
+-------------------+
| (caller frame)    |   <- 16-byte aligned
| ...               |
+-------------------+
high address
```

The "service routine" is `_yg_func_begin_resume`.

# License

The MIT license. See [LICENSE](LICENSE).

# Author

Kunshan Wang <kunshan.wang@anu.edu.au>


<!--
vim: tw=80
-->
