NetBSD support in htop(1)
===

This implementation utilizes kvm_getprocs(3), sysctl(3), etc, eliminating the
need for mount_procfs(8) with Linux compatibility enabled.

The implementation was initially based on the OpenBSD support in htop(1).

Notes on NetBSD curses
---

NetBSD is one of the last operating systems to use and maintain its own
implementation of Curses.

htop(1) can be compiled against either ncurses or NetBSD's curses(3).
In order for NetBSD's libcurses to be used, htop(1) must be configured with
`--disable-unicode`. This is necessary because htop(1) with Unicode enabled
directly accesses ncurses's cchar_t struct, which has different contents
in NetBSD's curses.

Versions of libcurses in NetBSD 9 and prior have no mouse support
(this is an ncurses extension). Newer versions contain no-op mouse functions
for compatibility with ncurses.

What needs improvement
---

* Kernel and userspace threads are not displayed or counted -
  maybe look at NetBSD top(1).
* Support for compiling using libcurses's Unicode support.
* Support for fstat(1) (view open files, like lsof(8) on Linux).
* Support for ktrace(1) (like strace(1) on Linux).
