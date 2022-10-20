#!/usr/bin/env bash

# Scripts are provided mainly to document the process and may
# not be the best way of automated it.

# Define build variables

# root of MinGW cross-build system
export MINGW_ROOT=/mingw64

# for building 64-bit binaries
export HOST_PLATFORM="x86_64-w64-mingw32"

# for building 32-bit binaries
# export HOST_PLATFORM="i686-w64-mingw32"
