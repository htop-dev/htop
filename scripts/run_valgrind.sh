#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

valgrind --leak-check=full --show-reachable=yes --show-leak-kinds=all --track-fds=yes --errors-for-leak-kinds=all --track-origins=yes --suppressions="${SCRIPTDIR}/htop_suppressions.valgrind" "${SCRIPTDIR}/../htop"
