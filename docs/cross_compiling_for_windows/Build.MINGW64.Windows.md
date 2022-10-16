Cross-compiling htop for Windows from Linux

Work in progress

Fun naming conventions bits:

win32 name of the current Windows API. When Windows added 64-bit
support, this API was extended to provide appropriate 64-bit extended
versions of everything, but the name wasn't changed (to say 'Win64'). So
win32 denotes the API, and may be either 32 or 64 bits.

w64 This is denotes the MinGW-w64 compiler suite. It was initially a
work of

MinGW/MinGW-w32 to specifically provide support for targeting the 64-bit

version of the win32 API. It has since basically taken over the role of
being the\
main GCC/LLVM compiler targeting Windows/win32, providing both 32-bit
and 64-bit support. So w64 denotes this compiler, and may be either 32
or 64 bits.

Under Linux and similar platforms, autoconf provides most of the heavy
lifting for cross-compilation that is needed. You just need to provide
it the platforms triplet for the build platform and the host platform
(autoconf calling the target platform the HOST). Assuming your system is
setup to match all its conventions, it will do all its normal magic
using the proper flavors of everything from there.

In theory, this means to compile an autoconf source based that targets
Linux-like systems to Windows we just need to do:

./configure --host=x86_64-w64-mingw32

\
the build host (current systems you are running on), for Linux this is
generally something like 'x86_64-pc-linux-gnu' will be detected, and the
makefile created will use proper tools. All the ugly corner cases are
where things get messy.

Going through specifics, we going assume a base platform of Debian Linux
11.x, bullseye, x86_64. Because it has everything needed in its
packaging system, and because I have setup where I spin easily spin up
VM with base-install of it. Package names may differ, but most
distributions will have the packages. See <https://mxe.cc/#requirements>
for solid list for most platforms. (MXE is designed for this whole task,
but for a number reasons, most importantly very outdated binaries, isn't
currently helpful).

For a basic build environment, we want to install these packages
(assuming both 32-bit and 64-bit targets). This list definitely isn't
minimal, but given a solid base to compiler most anything, including
compiling cross-compilers if needed (the ones we need are standard
packages in Debian.

apt install \\

autoconf \\

automake \\

autopoint \\

bash \\

bison \\

build-essential \\

bzip2 \\

flex \\

g++ \\

g++-multilib \\

gettext \\

git \\

gperf \\

intltool \\

libc6-dev-i386 \\

libgdk-pixbuf2.0-dev \\

libltdl-dev \\

libssl-dev \\

libtool-bin \\

libxml-parser-perl \\

make \\

openssl \\

patch \\

perl \\

python3 \\

ruby \\

sed \\

sudo \\

unzip \\

wget \\

xz-utils

For the required cross-compilation, we just need the one meta-package:\
\

apt install mingw-w64

To keep everything isolated, and not muddy the installed system, we'll
setup base location for the cross-piled software. This can be anywhere,
using sudo and chown are only needed if you are sticking it root like I
am:

export MINGW_ROOT=/mingw64

sudo mkdir \$MINGW_ROOT

sudo chown \$USER:\$USER \$MINGW_ROOT

While setting up handy environment variables, we should setup the host
platform.

\# for 32-bit

export HOST_PLATFORM="i686_w64-mingw32"

\# for 64-bit

export HOST_PLATFORM="x86_64-w64-mingw32"

This triplet can be read as architecture (i686 or x86_64) -- system (w64
for minGW-w64) -- API (mingw's version of the win32 API). I'll assume
these are setup ('source env.sh' with the scripts provided).

The only outside dependency we need is
[ncurses](https://invisible-island.net/ncurses/), the latest version as
of writing is 6.3. The latest is best here, since support for providing
a terminal-like environment in a basic Windows console is being
constantly improved.

There is a port of it called PDCurses that was designed to specifically
support DOS and Windows, but it missing a lot of features. The general
consensus is not use it if possible.

*\# grab and extract*

* wget
*[*https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.3.tar.gz*](https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.3.tar.gz)

tar xvf ncurses-6.3.tar.gz

cd ncurses-6.3

\# setup build, alternately see the provided build_ncurses.sh run from
this directory

./configure \\

\--host=\$HOST_PLATFORM \\

\--prefix=\$MINGW_ROOT \\

\--without-ada \\

\--enable-warnings \\

\--enable-assertions \\

\--enable-exp-win32 \\

\--enable-ext-funcs \\

\--disable-home-terminfo \\

\--disable-echo \\

\--disable-getcap \\

\--disable-hard-tabs \\

\--disable-leaks \\

\--disable-macros \\

\--disable-overwrite \\

\--enable-opaque-curses \\

\--enable-opaque-panel \\

\--enable-opaque-menu \\

\--enable-opaque-form \\

\--enable-database \\

\--enable-sp-funcs \\

\--enable-term-driver \\

\--enable-interop \\

\--enable-database \\

\--with-progs \\

\--without-libtool \\

\--with-shared \\

\--with-normal \\

\--without-debug \\

\--with-fallbacks=ms-terminal \\

\--without-manpages \\

\--enable-widec

\# build and install to our mingw64 root

make

make install\

This configuration is based on the recommended ones for compilation to
Windows from (location I've can't find in my notes, MXE most likely)
with the addition of *'--enable-widec'* to provide Unicode.\
This sets up everything for htop, to build htop eventually all that will
be needed is:

\

*#from htop clone*

* *

* #include headers and libraries from out mingw64 root*

* export CFLAGS=\"-I\$MINGW_ROOT/include\"*

export LDFLAGS=\"-L\$MINGW_ROOT/lib\"

./autogen.sh

./configure \\

\--host=\$HOST_PLATFORM \\

\--prefix=\$MINGW_ROOT \\

\--enable-static \\

\--enable-unicode

make

\
At this point, we get into things we need to change in htop. A *very*
rough list

1.  Add a platform in *configure.ac* (e.g. exclude *sys/utsname.h *from
    the current generic headers)
2.  Replace Linux-calls in main code (call to POSIX-style *getpw\**
    functions minimally)
3.  Create *windows* plugin folder from *generic*\
    \

Haven't been able to test a binary from the htop build at this point.
Added workaround for generic in configure.ac, to get #2. Not sure on the
best way to isolate changes in #2 or what Windows-equivalents to use.
