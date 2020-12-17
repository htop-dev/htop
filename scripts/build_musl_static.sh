#!/bin/sh

set -eu


if [ $# -ne 1 ] || [ "x$1" != "xagree-download" ]; then
    echo "This script needs to download ncurses and lm-sensors from"
    echo " https://ftp.gnu.org/pub/gnu/ncurses/"
    echo "and"
    echo " https://github.com/lm-sensors/lm-sensors/archive/"
    echo ""
    echo "Call with \"agree-download\" to continue."
    exit 1;
fi


MUSL_CFLAGS=${CFLAGS:-"-fstack-protector-strong -D_FORTIFY_SOURCE=2 -g -O3 -flto -ffast-math -fno-finite-math-only -Wno-error=null-dereference"}
MUSL_LDFLAGS=${LDFLAGS:-"-Wl,-z,relro,-z,now,-z,defs,--as-needed"}


message()
{
    echo ""
    echo "[+]"
    echo "[+] $1"
    echo "[+]"
    echo ""
}

SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

cd "${SCRIPTDIR}" || exit

mkdir -p musl_static_builddir/

cd musl_static_builddir/ || exit


NCURSES_VERSION=6.2
NCURSES_FILE=ncurses-${NCURSES_VERSION}.tar.gz

if ! [ -e ${NCURSES_FILE} ]; then
    message "Downloading ncurses v${NCURSES_VERSION}..."
    wget https://ftp.gnu.org/pub/gnu/ncurses/${NCURSES_FILE}
else
    message "Using cached ncurses v${NCURSES_VERSION}"
fi

tar xf ${NCURSES_FILE}

cd ncurses-${NCURSES_VERSION}/ || exit

NCURSES_INSTALL_DIR=${SCRIPTDIR}/musl_static_builddir/ncurses-installdir

if ! [ -e .htop_build_stamp ]; then
    message "Configuring ncurses ..."
    ./configure CC=musl-gcc \
        CFLAGS="${MUSL_CFLAGS}" \
        LDFLAGS="${MUSL_LDFLAGS}" \
        --prefix="${NCURSES_INSTALL_DIR}" \
        --without-cxx \
        --without-cxx-binding \
        --without-ada \
        --without-manpages \
        --without-progs \
        --without-tack \
        --without-tests \
        --without-curses-h \
        --without-pkg-config \
        --without-shared \
        --without-debug \
        --without-develop \
        --without-dlsym \
        --without-gpm \
        --without-sysmouse \
        --enable-widec \
        --with-default-terminfo-dir=/usr/share/terminfo \
        --with-terminfo-dirs=/usr/share/terminfo:/lib/terminfo:/usr/local/share/terminfo \
        --with-fallbacks="screen linux vt100 xterm"

    message "Building ncurses ..."
    make clean
    make -j "$(nproc)"

    touch .htop_build_stamp
else
    message "Using previously build ncurses"
fi

message "Installing ncurses ..."
make install.libs -j "$(nproc)"


cd ../ || exit


LMSENSORS_VERSION=3-6-0
LMSENSORS_FILE=V${LMSENSORS_VERSION}.tar.gz

if ! [ -e ${LMSENSORS_FILE} ]; then
    message "Downloading lm-sensors v${LMSENSORS_VERSION}..."
    wget https://github.com/lm-sensors/lm-sensors/archive/${LMSENSORS_FILE}
else
    message "Using cached lm-sensors v${LMSENSORS_VERSION}"
fi

tar xf ${LMSENSORS_FILE}

cd lm-sensors-${LMSENSORS_VERSION}/ || exit

LMSENSORS_DESTDIR=${SCRIPTDIR}/musl_static_builddir/lm-sensors-${LMSENSORS_VERSION}/installdir
make CC=musl-gcc CFLAGS="${MUSL_CFLAGS}" LDFLAGS="${MUSL_LDFLAGS}" DESTDIR="${LMSENSORS_DESTDIR}" install -j "$(nproc)"


cd "${SCRIPTDIR}/../" || exit


if ! [ -e configure ]; then
    message "Initializing autoconf for htop ..."
    ./autogen.sh
fi

message "Configuring htop ..."
./configure CC=musl-gcc \
            CFLAGS="${MUSL_CFLAGS} \
                    -I${NCURSES_INSTALL_DIR}/include \
                    -I${NCURSES_INSTALL_DIR}/include/ncursesw \
                    -I${LMSENSORS_DESTDIR}/usr/local/include/" \
            LDFLAGS="${MUSL_LDFLAGS} \
                     -L${NCURSES_INSTALL_DIR}/lib \
                     -L${LMSENSORS_DESTDIR}/usr/local/lib" \
            PKG_CONFIG=false \
            --with-ncursesw \
            --with-sensors \
            --enable-static \
            --enable-werror

message "Building htop ..."
make clean
make -j "$(nproc)"

message "Done"
