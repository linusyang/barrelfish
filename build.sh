#!/bin/bash

NOWDIR="$(cd "$(dirname "$0")" && pwd)"
ORIGPATH="$PATH"
GNUPATH="/opt/local/libexec/gnubin:$ORIGPATH"
CXPATH="$NOWDIR/build/toolchain:$GNUPATH"

pushd "$NOWDIR"
! [ -d build ] && mkdir -p build

pushd build
if ! [ -d toolchain ]; then
    mkdir toolchain && pushd toolchain
    for CXBIN in /usr/local/gcc-4.5.2-for-linux64/bin/x86_64-pc-linux-*
    do
        ln -s "$CXBIN" "${CXBIN/\/usr\/local\/gcc-4.5.2-for-linux64\/bin\/x86_64-pc-linux-/}"
    done
    popd
fi

export PATH="$GNUPATH"
! [ -f Makefile ] && ../hake/hake.sh -s ..

export PATH="$CXPATH"
make "$@"
