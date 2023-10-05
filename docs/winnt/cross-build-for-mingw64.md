# Cross-compiling `htop` for Windows from Linux

There is work being done on Windows support isolated to the Cygwin and MSYS2 environments. See @ookiineko as of 2023-09-28, see #1097.

## Fun naming conventions bits

* `Win32`: name of the current Windows API. When Windows added 64-bit support, this API was extended to provide appropriate 64-bit extended versions of everything, but the name wasn't changed (to something like `Win64`). So `Win32` denotes the API, and may be either 32 or 64 bits.

* `w64` : The MinGW-w64 is an advancement of the MinGW project, both providing a GCC compiler on Windows, which specifically targeted the 64-bit version of the `Win32` API. It has since basically taken over the role of being the main GCC compiler targeting Windows/`Win32`, providing both 32-bit and 64-bit support. So `w64` denotes this compiler, and it may be either 32 (for `i686`) or 64 (for `x86_64`) bites.

* `winnt`: Name of the Windows kernel. This name is used being used to denote things for the Windows port. The constant `HTOP_WINNT` when building for Windows.

## Cross-compiling basics

For cross-compiling, Autoconf and related tools will handle pretty much everything. We just need to tell it what platform we want to compile for. Autoconf names that platform the `host` platform, for us this is `x86_64-w64-mingw32`. We just provide it to the `configure` script:

```bash
./configure --host=x86_64-w64-mingw32
```

This will configure the build for the appropriate compiler, linkers, and libraries for that platform. Assuming everything in the expected location.

## Setting up the Cross-Build Environment

Going through specifics, we are going to assume a base install of Ubuntu 22.04 for `x86_64`; the same version also used by GitHub's `ubuntu-latest` runner. It has almost everything needed in its packaging system. While package names may differ, most major distributions will have these tools available.

These should be the minimal packages for a clean install. If you have a development environment of any sort setup, you likely only need to pull in `mingw-64`.

```bash
sudo apt install build-essential \
        git \
        autoconf automake \
        libncurses-dev \
        mingw-w64 \
        pkg-config
```

To keep everything isolated, and not muddy the installed system, we'll set up a base location for the cross-built software not provided by the package system, `MINGW_ROOT`, and set our host build platform as `HOST_PLATFORM`:

```bash
export MINGW_ROOT=/mingw64
sudo mkdir $MINGW_ROOT
sudo chown $USER:$USER $MINGW_ROOT

# set host platform
export HOST_PLATFORM='x86_64-w64-mingw32'
```

The `sudo` and `chown` are only needed if you are sticking it somewhere like the root directory.

## Cross-compiling `ncurses`

The only outside dependency we need is `ncurses`. Use the latest stable release. Support specifically targeted for MinGW-based ports was added in 5.8. These provide all the features of a normal terminal inside of standard Windows Console. The latest stable release is best here (6.4 as of writing).

Grab the source for `ncurses` from [*https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz*](https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz). Extract into a directory of your choosing, change into the that directory. Configure with these options:

```bash
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

This configuration is based on the recommended one for compilation to Windows from `ncurses` developers (see [README.MinGW](https://github.com/mirror/ncurses/blob/master/README.MinGW)) with the addition of `--enable-widec` to provide Unicode support. Note this is for building a static library only.

Standard `make` and `make install` will build the library and install it and its headers into `$MINGW_ROOT`.

## Configure and Cross-compile `htop`

This should be everything we need to cross-compile `htop` itself.

Clone the `htop` repo and checkout the `winnt` branch

```bash
    git clone git@github.com:htop-dev/htop.git
    cd htop
    git checkout winnt
```

To configure `htop` for cross-compiling we just need to tell `autoconf` we want to target the desired platform, and make sure that it looks to our `$MINGW_ROOT` for `ncurses`.

```bash
./autogen.sh

# Make sure configure can find the cross-built ncurses library and headers

export CFLAGS="-I$MINGW_ROOT/include"
export LDFLAGS="-L$MINGW_ROOT/lib"

./configure \
    --host=$HOST_PLATFORM \
    --prefix=$MINGW_ROOT \
    --enable-static \
    --enable-debug \
    --enable-unicode
```

At the moment, the `configure` script will fail with this error:

```text
checking for sys/utsname.h... no
configure: error: can not find required generic header files
```

This header is needed for the `uname` function (see [uname(2)](https://man7.org/linux/man-pages/man2/uname.2.html)), which isn't available on Windows. For the moment, we can work around this error to get the `configure` script to complete, so we have a working `Makefile`. The `configure` script is only concerned that this header exists. So placing a blank file in its place will work:

```bash
mkdir -p $MINGW_ROOT/include/sys
touch $MINGW_ROOT/include/sys/utsname.h
```

Rerun the `configure` command from above. The `configure` script will complete, but give some warnings:

```text
configure: WARNING: runtime behavior with NaN is not compliant - some functionality might break; consider using '-fno-finite-math-only'
```

The underlying reason for this warning is unknown, so we want it to remain until we either fix the issue or determine it is not a problem.

```text
****************************************************************
WARNING! This platform is not currently supported by htop.

The code will build, but it will produce a dummy version of htop
which shows no processes, using the files from the unsupported/
directory. This is meant to be a skeleton, to be used as a
starting point if you are porting htop to a new platform.
****************************************************************
```

This is warning is expected, it should be fixed once `HTOP_WINNT` is added to `autoconf`.

`configure` should finish with build configuration summary:

```text
  htop 3.3.0-dev

  platform:                  unsupported
  os-release file:           /etc/os-release
  (Linux) proc directory:    /proc
  (Linux) openvz:            no
  (Linux) vserver:           no
  (Linux) ancient vserver:   no
  (Linux) delay accounting:  no
  (Linux) sensors:           no
  (Linux) capabilities:      no
  unicode:                   yes
  affinity:                  no
  unwind:                    no
  hwloc:                     no
  debug:                     yes
  static:                    yes
```

You will be able to run `make` at this point, giving numerous errors. However, we can confirm that the build system is using the cross-compiler, and it is properly producing the COFF object files used by Windows:

```bash
$ file htop.o
htop.o: Intel amd64 COFF object file, no line number info, not stripped, 7 sections, symbol offset=0x1f0, 21 symbols, 1st section name ".text"
```

## Porting Plan/Progress

The platform-specific code will be placed in the subdirectory `winnt`, and the `configure` script will define and use the variable `HTOP_WINNT` while building for this platform.

At the moment a number of functions use functionality that Windows doesn't provide directly, mainly definitions and functions from `pwd.h` and `uts_name.h`.

Relevant notes on `htop` code layout:

* The platform specific code is split out into folders, `linux`, `freebsd`, etc.
* Code that is basically the same across Linux/Unix-like platforms is stored in the `generic` folder.
* In the main folder, `Compat.c` is where preprocessor-based platform code is all sewn together.
* `Makefile.am` determines, based on platform defines, which files are actually built and linked into the end product.

Rough steps from here:

1. Move code from main folder files into `generic` that belongs there (Unix-like things, mainly `pwd.h` and `uts_name.h` stuff).
2. Update `configure.ac` and `Makefile.am` accordingly. Defining a category for the root files + `generic` and one without `generic` for `winnt`.
3. Build and test on currently supported platforms. That is, make sure refactoring so far and a place for Windows-specific code didn't break anything.
4. Fill in missing pieces to get the most basic binary using `ncurses` building. This should shake out any issues with the cross-build setup.
5. Work on Windows proof-of-concept code.

Goals for Proof-of-Concept

* [ ] Cross-build everything from Linux using `autoconf` tools.
* [ ] GitHub Action for CI that goes through the steps documented here
* [ ] Produce binary that runs on Windows Console
* [ ] Demonstrate we can produce a working Windows binary that uses `ncurses`.
* [ ] Have `winnt` build able to enumerate existing processes.

The current cross-build setup provides the core Windows API (basically `libc` + everything under the `windows.h` header) plus `ncurses`. This will likely provide all needed functionality. If more parts of the SDK are needed, this document will be updated to instruct on building/installing them.

## Notes

These instructions build a static binary with debugging.

I am not familiar with the `affinity` or `hwloc` that are being used. Affinity can be enabled by just adding the `--enable-affinity` to `htop`'s `configure`

The mutually exclusive `hwloc` can alternatively be enabled. This requires the `hwloc` library which be easily built/installed virtually identically to `ncurses`.
