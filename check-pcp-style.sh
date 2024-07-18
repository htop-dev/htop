#!/bin/bash

set -eu -o pipefail

PCP_DIR="$(dirname "$0")/pcp"

if [ ! -d "${PCP_DIR}" ]; then
   echo Could not find PCP platform sources. >&2
   exit 1
fi

echo Scanning definitions in ${PCP_DIR} ...

PCP_COLUMNS="${PCP_DIR}/columns"
PCP_METERS="${PCP_DIR}/meters"
PCP_SCREENS="${PCP_DIR}/screens"

if [ ! -d "${PCP_COLUMNS}" ]; then
   echo Could not find PCP column definitions. >&2
   exit 1
fi

if [ ! -d "${PCP_METERS}" ]; then
   echo Could not find PCP meter definitions. >&2
   exit 1
fi

if [ ! -d "${PCP_SCREENS}" ]; then
   echo Could not find PCP screen definitions. >&2
   exit 1
fi

check_file() {
   f="$1"
   r="$2"

   echo "Processing $f ..."

   awk -v required_names_str="$2" '
      BEGIN {
         # Define the required names
         split(required_names_str, required_names)
         section = ""
      }

      # Skip comment lines
      /^#/ {
         next
      }

      # Detect section headers
      /^\[.*\]$/ {
         if (section != "") {
            check_section()
         }
         section = $0
         if (section in sections_seen) {
            print "Error: Duplicate section " section
            exit 1
         }
         sections_seen[section] = 1
         delete seen
         delete groups
         next
      }

      # Process name = value pairs with whitespace around the equals sign
      /^[^=]+ = [^=]+$/ {
         split($0, pair, " = ")

         name = trim(pair[1])
         value = trim(pair[2])
         group = ""

         known = 0
         for (i in required_names) {
            rname = required_names[i]
            if (rname ~ /\?$/) {
               rname = substr(rname, 1, length(rname) - 1)
            }
            if (rname ~ /^\*\./) {
               rlname = substr(rname, 2, length(rname) - 1)
               if (substr(name, length(name) - length(rlname) + 1) == rlname) {
                  group = substr(name, 1, length(name) - length(rlname) + 1)
                  known = 1
                  break
               }
            }
            if (rname == name) {
               known = 1
               break
            }
         }
         if (!known) {
            print "Error: Unknown name " name " in section " section
            exit 1
         }

         if (name in seen) {
            print "Error: Duplicate name " name " in section " section
            exit 1
         }

         seen[name] = 1
         groups[group] = 1

         if (name ~ /(^|\.)width$/) {
            if (!(value ~ /^-?[0-9]{1,3}$/)) {
               print "Error: Specified width " value " for " name " in section " section " is not an integer or out of range (-999..999)."
               exit 1
            }
         }

         if (name ~ /(^|\.)type$/) {
            if (!(value ~ /^(bar|text|graph|led)$/)) {
               print "Error: Specified type " value " for " name " in section " section " is not recognized."
               exit 1
            }
         }

         if (name ~ /(^|\.)color$/) {
            if (!(value ~ /^(black|blue|green|red|cyan|magenta|yellow|(dark)?gray|white)$/)) {
               print "Error: Specified color " value " for " name " in section " section " is not recognized."
               exit 1
            }
         }

         next
      }

      # Function to trim whitespace
      function trim(s) {
         gsub(/^[ \t]+|[ \t]+$/, "", s)
         return s
      }

      # Function to check if all required names are present
      function check_section() {
         missing = ""
         for (i in required_names) {
            rname = required_names[i]
            if (rname ~ /\?$/) {
               continue
            }
            if (rname ~ /^\*\./) {
               rname = substr(rname, 3, length(rname) - 2)
               for (g in groups) {
                  if (g == "") {
                     continue
                  }
                  if (!(g rname in seen)) {
                     missing = missing g rname " "
                  }
               }
               continue
            }
            if (!(rname in seen)) {
               missing = missing rname " "
            }
         }
         if (missing != "") {
            print "Error: Missing " missing "in section " section
            exit 1
         }
      }

      # End of file processing
      END {
         if (section != "") {
            check_section()
         }

         # Ensure the file ends with a single newline
         if (NR > 0 && length($0) < 1) {
            print "Error: Whitespace at EOF."
            exit 1
         }
      }
   ' "$f"
}

error_occurred=0

for pcp_file in "${PCP_COLUMNS}"/*; do
   if ! check_file "$pcp_file" "heading caption? width metric description"; then
      echo "Error processing file: $pcp_file" >&2
      error_occurred=1
   fi
done

for pcp_file in "${PCP_METERS}"/*; do
   if ! check_file "$pcp_file" "caption description? type? *.metric *.color? *.label? *.suffix?"; then
      echo "Error processing file: $pcp_file" >&2
      error_occurred=1
   fi
done

for pcp_file in "${PCP_SCREENS}"/*; do
   if ! check_file "$pcp_file" "heading caption default? *.heading *.metric *.default? *.caption? *.format? *.instances? *.width?"; then
      echo "Error processing file: $pcp_file" >&2
      error_occurred=1
   fi
done

if [ $error_occurred -ne 0 ]; then
   echo "One or more files failed to process." >&2
   exit 1
else
   echo "All files processed successfully." >&2
fi
