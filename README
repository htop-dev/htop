# [![htop](htop.png)](https://htop.dev)

[![CI](https://github.com/htop-dev/htop/workflows/CI/badge.svg)](https://github.com/htop-dev/htop/actions)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21665/badge.svg)](https://scan.coverity.com/projects/21665)
[![Mailing List](https://img.shields.io/badge/Mailing%20List-htop-blue.svg)](https://groups.io/g/htop)
[![IRC #htop](https://img.shields.io/badge/IRC-htop-blue.svg)](https://webchat.freenode.net/#htop)
[![Github Release](https://img.shields.io/github/release/htop-dev/htop.svg)](https://github.com/htop-dev/htop/releases/latest)
[![Download](https://api.bintray.com/packages/htop/source/htop/images/download.svg)](https://bintray.com/htop/source/htop/_latestVersion)

![Screenshot of htop](docs/images/screenshot.png?raw=true)

## Introduction

`htop` is a cross-platform interactive process viewer.

`htop` allows scrolling the list of processes vertically and horizontally to see their full command lines and related information like memory and CPU consumption.

The information displayed is configurable through a graphical setup and can be sorted and filtered interactively.

Tasks related to processes (e.g. killing and renicing) can be done without entering their PIDs.

Running `htop` requires `ncurses` libraries (typically named libncursesw*).

For more information and details on how to contribute to `htop` visit [htop.dev](https://htop.dev).

## Build instructions

This program is distributed as a standard GNU autotools-based package.

Compiling `htop` requires the header files for `ncurses` (libncursesw*-dev). Install these and other required packages for C development from your package manager.

Then, when compiling from a [release tarball](https://bintray.com/htop/source/htop), run:

~~~ shell
./configure && make
~~~

Alternatively, for compiling sources downloaded from the Git repository (`git clone` or downloads from [Github releases](https://github.com/htop-dev/htop/releases/)),
install the header files for `ncurses` (libncursesw*-dev) and other required development packages from your distribution's package manager. Then run:

~~~ shell
./autogen.sh && ./configure && make
~~~

By default `make install` will install into `/usr/local`, for changing the path use `./configure --prefix=/some/path`.

See the manual page (`man htop`) or the on-line help ('F1' or 'h' inside `htop`) for a list of supported key commands.

## Support

If you have trouble running `htop` please consult your Operating System / Linux distribution documentation for getting support and filing bugs.

## Bugs, development feedback

We have a [development mailing list](https://htop.dev/mailinglist.html). Feel free to subscribe for release announcements or asking questions on the development of htop.

You can also join our IRC channel #htop on freenode and talk to the developers there.

If you have found an issue with the source of htop, please check whether this has already been reported in our [Github issue tracker](https://github.com/htop-dev/htop/issues).
If not, please file a new issue describing the problem you have found, the location in the source code you are referring to and a possible fix.

## History

`htop` was invented, developed and maintained by Hisham Muhammad from 2004 to 2019. His [legacy repository](https://github.com/hishamhm/htop/) has been archived to preserve the history.

In 2020 a [team](https://github.com/orgs/htop-dev/people) took over the development amicably and continues to maintain `htop` collaboratively.

## License

GNU General Public License, version 2 (GPL-2.0)
