# htop - signals/PrintSignals.sed
# (C) 2023 htop dev team
# Released under the GNU GPLv2+, see the COPYING file
# in the source distribution for its full text.

# Single digit, justify name by one space.
s/^\([0-9]\)	"\(.*\)"$/{ .number=(\1), .name=(" \1 \2") },/
# Multiple digits or a complex expression, don't justify.
s/^\(.*\)	"\(.*\)"$/{ .number=(\1), .name=("\1 \2") },/
