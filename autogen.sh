#!/bin/sh

set -e

bindir=${0%/*}
: ${bindir:=.}
cd "$bindir"

mkdir -p m4
autoreconf --install --force
