#!/usr/bin/env python3
# Check that htop's first rendered process row has the expected PID.
#
# Used by the macOS CI job to verify that sorting by a column which is not
# visible (htop -s M_FOOTPRINT) still collects the data backing that column:
# the scan is given a big-memory process and a small one whose PID sorts
# first among ties. Without collection all footprints stay 0 and the PID
# tie-breaker puts the smaller PID (the small process) on top; with
# collection the big process must be on top.
#
# Usage: ./htop -n 5 -s M_FOOTPRINT -p <small>,<big> | check-htop-first-row.py <big>
import re
import sys

expected_first = sys.argv[1]

data = sys.stdin.buffer.read().decode("utf-8", "replace")
# strip CSI/OSC style escape sequences emitted by ncurses
data = re.sub(r"\x1b[\[\]?]*[0-9;]*[a-zA-Z]", "", data)

# The column header row starts the process table and contains no digits, so
# the first number below it is the PID of the first row actually rendered.
hdr = data.find("PID USER")
assert hdr >= 0, "no column header found in htop output"
tail = data[hdr:]

assert expected_first in tail, f"row for PID {expected_first} missing, the check would be vacuous"

m = re.search(r"\s*(\d+)", tail)
assert m, "no process row found in htop output"
first_pid = m.group(1)

if first_pid != expected_first:
    sys.exit(f"FAIL: expected PID {expected_first} first, got {first_pid}"
             " => FOOTPRINT data was not collected for the sort key")
print(f"OK: first row has PID {first_pid} (big footprint sorted on top)")
