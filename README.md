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

# License

The MIT license. See [LICENSE](LICENSE).

# Author

Kunshan Wang <kunshan.wang@anu.edu.au>


<!--
vim: tw=80
-->
