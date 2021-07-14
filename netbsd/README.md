NetBSD support in htop(1)
===

This implementation utilizes kvm_getprocs(3), sysctl(3), etc, eliminating the
need for a /proc file system to be mounted with Linux compatibility enabled.

The implementation was initially based on the OpenBSD support in htop(1).

Notes on NetBSD curses
---

NetBSD is one of the last operating systems to use and maintain its own
implementation of Curses.

htop can be compiled against either ncurses or NetBSD's libcurses.
In order for NetBSD curses to be used, htop must be configured with
`--disable-unicode`. This is necessary because htop with Unicode enabled
directly accesses ncurses's cchar_t struct, which has different contents
in NetBSD's curses.

Versions of libcurses in NetBSD 9 and prior have no mouse support
(this is an ncurses extension). Newer versions contain no-op mouse functions
for compatibility with ncurses.

What needs improvement
---

* Kernel and userspace threads are not displayed or counted -
  maybe look at NetBSD top(1).
* Battery display - use envsys(4).
* Support for compiling using libcurses's Unicode support.
* Support for fstat(1) (view open files, like lsof on Linux).
* Support for ktrace(1) (like strace on Linux).
