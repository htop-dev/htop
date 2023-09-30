# Cross-compiling htop for Windows from Linux

An intermediate build using Cygwin/MSYS is being worked on by @ookiineko as of 2023-09-28 (see #1097)

## Fun naming conventions bits:

* `Win32`: name of the current Windows API. When Windows added 64-bit support, this API was extended to provide appropriate 64-bit extended versions of everything, but the name wasn't changed (to something like `Win64`). So `Win32` denotes the API, and may be either 32 or 64 bits.

* `w64` : The MinGW-w64 is an advancement of the MinGW project, both providing a GCC compiler on Windows, which specifically targeted the  64-bit version of the `Win32` API. It has since basically taken over the role of being the main GCC compiler targeting Windows/`Win32`, providing both 32-bit and 64-bit support. So `w64` denotes this compiler, and it may be either 32 (for `i686`) or 64 (for `x86_64`) bites.

* `winnt`: Name of the Windows kernel. This name is used being used to denote things for the Windows port. The constant `HTOP_WINNT` when building for Windows.

## Cross-compiling basics

For cross-compiling, autoconf and related tools will handle pretty much everything. We just need to tell it what platform we want to compile for. Autoconf names that platform the `host` platform, for us this is `x86_64-w64-mingw32`. We just provide it to the `configure` script:

```
./configure --host=x86_64-w64-mingw32
```

and it will use the appropriate compiler, linkers, and libraries for that platform. Assuming everything in the expected location.

## Setting up the Cross-Build Environment

Going through specifics, we going assume a base install of Ubuntu 22.04 for `x86_64`. The same version used by GitHub's ubuntu-latest runner. It has (almost) everything needed in its packaging system. While package names may differ,  most major distributions will have same tools available.

These should be the minimal needed packages on a clean install. If you have a development environment of any sort setup, you likely only need to pull in `mingw-64`.

```
apt install \
    build-essential     # basic C/C++ compiler               \
    git                 # cloning htop                       \
    autoconf automake   # autotools                          \
    libncurses-dev      # only need for native/Linux build   \
    mingw-w64           # cross-compiler for Win32           \
    pkg-config          # needed to explicitly pull on Ubuntu
```

To keep everything isolated, and not muddy the installed system, we'll setup base location for the cross-built software `MINGW_ROOT` and set our host build platform as  `HOST_PLATFORM`

```
export MINGW_ROOT=/mingw64
sudo mkdir $MINGW_ROOT
sudo chown $USER:$USER $MINGW_ROOT

# set host platform
export HOST_PLATFORM='x86_64-w64-mingw32'
```

The sudo and chown are only needed if you are sticking it somewhere like the root directory.

## Cross-compiling NCurses Library

The only outside dependency we need is [ncurses](https://invisible-island.net/ncurses/). Use the latest stable release. Support specifically targeted for MinGW-based ports was added in 5.8. These provide all the features of a normal terminal inside of standard Windows Console .The latest stable release is best here (6.4 as of writing).

Grab the source for NCurses, [*https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz*](https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz). Extract into a directory of your choosing, change into the that directory. Configure with these options:

```
./configure \
	--host=$HOST_PLATFORM \
	--prefix=$MINGW_ROOT \
	--disable-echo \
	--disable-getcap \
	--disable-hard-tabs \
	--disable-home-terminfo \
	--disable-leaks \
	--disable-macros \
	--disable-overwrite \
	--disable-termcap \
	--enable-assertions \
	--enable-database \
	--enable-exp-win32 \
	--enable-ext-funcs \
	--enable-interop \
	--enable-opaque-curses \
	--enable-opaque-form \
	--enable-opaque-menu \
	--enable-opaque-panel \
	--enable-pc-files \
	--enable-sp-funcs \
	--enable-static\
	--enable-term-driver \
	--enable-warnings \
	--enable-widec \
	--with-fallbacks=ms-terminal \
	--with-normal \
	--with-progs \
	--without-ada \
	--without-debug \
	--without-libtool \
	--without-manpages \
	--without-shared
```

This configuration is based on the recommended one for compilation to Windows from NCurses developers ([README.MinGW](https://github.com/mirror/ncurses/blob/master/README.MinGW)) with the addition of `--enable-widec` to provide Unicode support.

Standard `make` and `make install` to build the library and install it and its headers into `$MINGW_ROOT`

## Configure and Cross-compile htop

This should be everything we need to cross compile `htop` itself.

Clone the `htop` repo and checkout the `windows` branch (currently work on the `winnt` Port is separated into that branch):

```
    git clone https://github.com/htop-dev/htop.git
    cd htop
    git checkout windows
```

To configure `htop` for cross-compiling we just need to tell autoconf we want to target the desired platform, and make sure that it looks to our `$MINGW_ROOT` for NCurses.

```
./autogen.sh

# Make sure configure can find the cross built NCurses library and headers

export CFLAGS="-I$MINGW_ROOT/include"
export LDFLAGS="-L$MINGW_ROOT/lib"

./configure \
	--host=$HOST_PLATFORM \
	--prefix=$MINGW_ROOT \
	--enable-static \
	--enable-unicode
```

At the moment, the `configure` script will fail with this error:

```
checking for sys/utsname.h... no
configure: error: can not find required generic header files
```

This header is needed for the `uname` function (see UNAME(3)), which isn't available on Windows. For the moment, let's workaround this error to get the `configure` script to complete to complete so we have a makefile.

The `configure` script is only concerned that this header exists. So placing a blank file in its place will work:

```
mkdir -p $MINGW_ROOT/include/sys
touch $MINGW_ROOT/include/sys/utsname.h
```

Rerun the `configure` command from above. It should complete,  displaying a warning box about the platform not currently being supported, and printing the normal configuration summary.

You will be able to run `make` at this point, giving numerous errors. However, we can confirm that the build system is using the cross-compiler and it is properly producing the COFF object files used by Windows:

```
$ file htop.o
htop.o: Intel amd64 COFF object file, no line number info, not stripped, 7 sections, symbol offset=0x1f0, 21 symbols, 1st section name ".text"
```

## Porting Plan/Progress

The platform-specific code will be placed in the subdirectory `winnt`, and the configure scripts will define and use the variable `HTOP_WINNT` while building for that platform.

At the moment a number of functions use functionality that Windows doesn't provide directly, mainly definitions and functions from `pwd.h` and `uts_name.h`.

Relevant notes on `htop` code layout:

* The platform specific code is split out into folders, `linux`, `freebsd`, etc.
* Code that is basically the same across Linux/Unix-like platforms is stored in the `generic` folder.
* In the main folder, `Compat.c` is where preprocessor-based platform code is all sown together.
* Makefile.am determines, based on platform defines, which files are actually built and linked into the end product.

Rough steps from here:

1. Move code from main folder files into `generic` that belongs there (mainly `pwd.h` and `uts_name.h` stuff)
2. Update `configure.ac` and `Makefile.am` accordingly. Defining a category for the root files + `generic` and one without `generic` for `winnt`
3. Build and test on currently supported platforms. That is, make sure refactoring so there is a tidy place to add Windows-specific code didn't break anything
4. Fill in missing pieces to get the most basic binary using NCurses building. This will shake out any issues with the cross-build setup
5.  Work on Windows proof-of-concept code.

Goals for Proof-of-Concept
* Cross build everything from Linux using autoconf tools
* Github Action for CI that goes through the steps documented here
* Produce binary that runs on Windows Console
* Demonstrate existing `ncurses`-based code is working
* Have `winnt` build enumerate existing processes

The current cross-build setup provides the core Windows API (basically libc + everything under the `windows.h` header) plus NCurses. This is expected to be enough functionality. The full Windows SDK can be added, if it turns out to be necessary.
__

This is a work in progress, see issues #1097 for on-going details. This document, modified and added code, scripts and anything else is authored or modified by @dnabre. Everything is license under GPL 2.0 (and/or additional licenses per existing `htop` project).
