htop
====

by Hisham Muhammad <hisham@gobolinux.org>

May, 2004 - January, 2014

Note
----

![Alert](http://i.imgur.com/ugIqB9s.png)  **Mac users, [click here](https://www.bountysource.com/fundraisers/554-mac-os-x-support-in-the-official-htop-1-x-tree)!** The htop version you are using is a 5-year old fork -- help bring htop 1.x to the Mac!

Introduction
------------

This is htop, an interactive process viewer.
It requires ncurses. It is tested with Linux 2.6,
but is also reported to work (and was originally developed)
with the 2.4 series.

Note that, while, htop is Linux specific -- it is based
on the Linux /proc filesystem -- it is also reported to work
with FreeBSD systems featuring a Linux-compatible /proc.
This is, however, unsupported. Contact the packager for your
system when reporting problems on platforms other than Linux.

This software has evolved considerably during the last years,
and is reasonably complete, but there is still room for
improvement. Read the TODO file to see what's known to be missing.

Comparison between 'htop' and 'top'
-----------------------------------

* In 'htop' you can scroll the list vertically and horizontally
  to see all processes and full command lines.
* In 'top' you are subject to a delay for each unassigned
  key you press (especially annoying when multi-key escape
  sequences are triggered by accident).
* 'htop' starts faster ('top' seems to collect data for a while
  before displaying anything).
* In 'htop' you don't need to type the process number to
  kill a process, in 'top' you do.
* In 'htop' you don't need to type the process number or
  the priority value to renice a process, in 'top' you do.
* In 'htop' you can kill multiple processes at once.
* 'top' is older, hence, more tested.

Compilation instructions
------------------------

This program is distributed as a standard autotools-based package.
See the INSTALL file for detailed instructions, but you are
probably used to the common `./configure`/`make`/`make install` routine.

When fetching the code from the development repository, you need
to run the `./autogen.sh` script, which in turn requires autotools
to be installed.

See the manual page (man htop) or the on-line help ('F1' or 'h'
inside htop) for a list of supported key commands.

if not all keys work check your curses configuration.
