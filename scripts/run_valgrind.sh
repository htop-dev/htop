#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all --errors-for-leak-kinds=all --suppressions="${SCRIPTDIR}/htop_suppressions.valgrind" "${SCRIPTDIR}/../htop"
