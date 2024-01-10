# [![htop logo](htop.png)](https://htop.dev)

[![CI](https://github.com/htop-dev/htop/workflows/CI/badge.svg)](https://github.com/htop-dev/htop/actions)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/21665/badge.svg)](https://scan.coverity.com/projects/21665)
[![Mailing List](https://img.shields.io/badge/Mailing%20List-htop-blue.svg)](https://groups.io/g/htop)
[![IRC #htop](https://img.shields.io/badge/IRC-htop-blue.svg)](https://web.libera.chat/#htop)
[![GitHub Release](https://img.shields.io/github/release/htop-dev/htop.svg)](https://github.com/htop-dev/htop/releases/latest)
[![Packaging status](https://repology.org/badge/tiny-repos/htop.svg)](https://repology.org/project/htop/versions)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](COPYING?raw=true)

![Screenshot of htop](docs/images/screenshot.png?raw=true)

## Introduction

`htop` is a cross-platform interactive process viewer.

`htop` allows scrolling the list of processes vertically and horizontally to see their full command lines and related information like memory and CPU consumption.
Also system wide information, like load average or swap usage, is shown.

The information displayed is configurable through a graphical setup and can be sorted and filtered interactively.

Tasks related to processes (e.g. killing and renicing) can be done without entering their PIDs.

Running `htop` requires `ncurses` libraries, typically named libncurses(w).

`htop` is written in C.

For more information and details visit [htop.dev](https://htop.dev).

## Build instructions

### Prerequisite
List of build-time dependencies:
  * standard GNU autotools-based C toolchain
    - C99 compliant compiler
    - `autoconf`
    - `automake`
    - `autotools`
  * `ncurses`

**Note about `ncurses`:**
> `htop` requires `ncurses` 6.0. Be aware the appropriate package is sometimes still called libncurses5 (on Debian/Ubuntu). Also `ncurses` usually comes in two flavours:
>* With Unicode support.
>* Without Unicode support.
>
> This is also something that is reflected in the package name on Debian/Ubuntu (via the additional 'w' - 'w'ide character support).

List of additional build-time dependencies (based on feature flags):
*  `sensors`
*  `hwloc`
*  `libcap` (v2.21 or later)
*  `libnl-3`

Install these and other required packages for C development from your package manager.

**Debian/Ubuntu**
~~~ shell
sudo apt install libncursesw5-dev autotools-dev autoconf automake build-essential
~~~

**Fedora/RHEL**
~~~ shell
sudo dnf install ncurses-devel automake autoconf gcc
~~~

**Archlinux/Manjaro**
~~~ shell
sudo pacman -S ncurses automake autoconf gcc
~~~

**macOS**
~~~ shell
brew install ncurses automake autoconf gcc
~~~

### Compile from source:
To compile from source, download from the Git repository (`git clone` or downloads from [GitHub releases](https://github.com/htop-dev/htop/releases/)), then run:
~~~ shell
./autogen.sh && ./configure && make
~~~

### Install
To install on the local system run `make install`. By default `make install` installs into `/usr/local`. To change this path use `./configure --prefix=/some/path`.

### Build Options

`htop` has several build-time options to enable/disable additional features.

#### Generic

  * `--enable-unicode`:
    enable Unicode support
    - dependency: *libncursesw*
    - default: *yes*
  * `--enable-affinity`:
    enable `sched_setaffinity(2)` and `sched_getaffinity(2)` for affinity support; conflicts with hwloc
    - default: *check*
  * `--enable-hwloc`:
    enable hwloc support for CPU affinity; disables affinity support
    - dependency: *libhwloc*
    - default: *no*
  * `--enable-static`:
    build a static htop binary; hwloc and delay accounting are not supported
    - default: *no*
  * `--enable-debug`:
    Enable asserts and internal sanity checks; implies a performance penalty
    - default: *no*

#### Performance Co-Pilot

  * `--enable-pcp`:
    enable Performance Co-Pilot support via a new pcp-htop utility
    - dependency: *libpcp*
    - default: *no*

#### Linux

  * `--enable-sensors`:
    enable libsensors(3) support for reading temperature data
    - dependencies: *libsensors-dev*(build-time), at runtime *libsensors* is loaded via `dlopen(3)` if available
    - default: *check*
  * `--enable-capabilities`:
    enable Linux capabilities support
    - dependency: *libcap*
    - default: *check*
  * `--with-proc`:
    location of a Linux-compatible proc filesystem
    - default: */proc*
  * `--enable-openvz`:
    enable OpenVZ support
    - default: *no*
  * `--enable-vserver`:
    enable VServer support
    - default: *no*
  * `--enable-ancient-vserver`:
    enable ancient VServer support (implies `--enable-vserver`)
    - default: *no*
  * `--enable-delayacct`:
    enable Linux delay accounting support
    - dependencies: *pkg-config*(build-time), *libnl-3* and *libnl-genl-3*
    - default: *check*


## Runtime dependencies:
`htop` has a set of fixed minimum runtime dependencies, which is kept as minimal as possible:
* `ncurses` libraries for terminal handling (wide character support).

### Runtime optional dependencies:
`htop` has a set of fixed optional dependencies, depending on build/configure option used:

#### Linux
* `libdl`, if not building a static binary, is always required when support for optional dependencies (i.e. `libsensors`, `libsystemd`) is present.
* `libcap`, user-space interfaces to POSIX 1003.1e capabilities, is always required when `--enable-capabilities` was used to configure `htop`.
* `libsensors`, readout of temperatures and CPU speeds, is optional even when `--enable-sensors` was used to configure `htop`.
* `libsystemd` is optional when `--enable-static` was not used to configure `htop`. If building statically and `libsystemd` is not found by `configure`, support for the systemd meter is disabled entirely.

`htop` checks for the availability of the actual runtime libraries as `htop` runs.

#### BSD
On most BSD systems `kvm` is a requirement to read kernel information.

More information on required and optional dependencies can be found in [configure.ac](configure.ac).

## Usage
See the manual page (`man htop`) or the help menu (**F1** or **h** inside `htop`) for a list of supported key commands.

## Support

If you have trouble running `htop` please consult your operating system / Linux distribution documentation for getting support and filing bugs.

## Bugs, development feedback

We have a [development mailing list](https://htop.dev/mailinglist.html). Feel free to subscribe for release announcements or asking questions on the development of `htop`.

You can also join our IRC channel [#htop on Libera.Chat](https://web.libera.chat/#htop) and talk to the developers there.

If you have found an issue within the source of `htop`, please check whether this has already been reported in our [GitHub issue tracker](https://github.com/htop-dev/htop/issues).
If not, please file a new issue describing the problem you have found, the potential location in the source code you are referring to and a possible fix if available.

## History

`htop` was invented, developed and maintained by [Hisham Muhammad](https://hisham.hm/) from 2004 to 2019. His [legacy repository](https://github.com/hishamhm/htop/) has been archived to preserve the history.

In 2020 a [team](https://github.com/orgs/htop-dev/people) took over the development amicably and continues to maintain `htop` collaboratively.

## License

GNU General Public License, version 2 (GPL-2.0) or, at your option, any later version.
