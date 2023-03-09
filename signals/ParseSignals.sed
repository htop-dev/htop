# htop - signals/ParseSignals.sed
# (C) 2023 htop dev team
# Released under the GNU GPLv2+, see the COPYING file
# in the source distribution for its full text.

# Parse a preprocessed C file with a list of signal numbers and names; output
# the numbers and names in a sortable format.

# The reason why this script exists at all is that the preprocessed C output
# doesn't correspond line-to-line with the original source file. Among other
# issues, it contains preprocessor directives with line numbers, which break
# up the output. To allow sorting the file by signal name at compile time,
# the preprocessed output has to be "parsed" and reformatted.

# The script should be relatively robust wrt. C compiler output, but it should
# still be considered a hack. It is possible that unforeseen input breaks it.

# It works by building up each multiline signal entry (number and name) in the
# hold space until it is complete, postprocessing and printing it. The entry
# is recognized by the htop_sig{start,mid,end} markers, because these are all
# but guaranteed not to be mangled or duplicated by the preprocessing, unlike
# e.g. parens, braces or semicolons.


# Skip preprocessor directives. This breaks the relationship between input and
# output files, but it makes sorting easy.
/^#/ d

:start

# Only process data between htop_sigstart and htop_sigend, deleting the rest.
/htop_sigstart/ {
   # Skip the start marker itself.
   # We can't write simply `s/.*htop_sigstart[[:space:]]*//`, because the
   # initial star is eager, so it would happily consume multiple start
   # markers. This should never happen, but the script should be robust
   # just in case.
   # Solution: Rename the first start marker to a temporary name and
   # delete using the new name.
   /htop_delsigstart/ i\
#error "The sed input contains `htop_sigdelstart`, which is a reserved marker."
   s/htop_sigstart/htop_delsigstart/
   s/.*htop_delsigstart[[:space:]]*//

   :loop
   # Read input until we see the end marker.

   # The end marker has been seen, we can go on with processing.
   /htop_sigend/ {
      # We need to save the rest of the line for further processing,
      # so store the whole line and deal with that later.
      h

      # Delete the end marker and the rest of the line, keeping just
      # the signal definition.
      /htop_sigdelend/ i\
#error "The sed input contains 'htop_sigdelend', which is a reserved marker."
      s/htop_sigend/htop_sigdelend/
      s/htop_sigdelend.*//

      # Normalize spaces to get rid of tabs and newlines and clean
      # up the output.
      s/[[:space:]][[:space:]]*/ /g
      s/^[[:space:]]*//
      s/[[:space:]]*$//

      # Replace the separator between the signal number and name
      # with a tab for sorting.
      s/ htop_sigmid /	/
      # TODO test whether the above succeeded and report error otherwise.

      # Test whether the signal number is actually a number, as it
      # would not be sortable otherwise.
      /^[0-9][0-9]*	/ !i\
#warning "Encountered non-numeric signal number; sorting may be broken."

      # Print the signal definition.
      p

      # Process the rest of the line.
      g
      s/htop_sigend/htop_sigdelend/
      s/.*htop_sigdelend//
      b start
   }

   :dprep
   # The end marker was not seen yet, read one more line.
   N
   # If the next line is a preprocessor directive, delete it, read
   # another one and test for preprocessor directives again.
   s/\n#.*//
   t dprep
   # Test for the end marker again.
   b loop
}

# The line is outside a range marked as signal definition, suppress it.
d
