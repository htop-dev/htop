htop coding style guide
=======================

Naming conventions
------------------

Names are important to convey what all those things inside the project are for.
Filenames for source code traditionally used camel-case naming with the first letter written in uppercase.
The file extension is always lowercase.

The only exception here is `htop.c` which is the main entrance point into the code.

Folders for e.g. platform-specific code or complex features spawning multiple files are written in lowercase, e.g. `linux`, `freebsd`, `zfs`.

Inside files, the naming somewhat depends on the context.
For functions names should include a camel-case prefix before the actual name, separated by an underscore.
While this prefix usually coincides with the module name, this is not required, yet strongly advised.
One important exception to this rule are the memory management and the string utility functions from `XUtils.h`.

Variable names inside functions should be short and precise.
Using `i` for some loop counter is totally fine, using `someCounterValueForThisSimpleLoop` is not.
On the other hand, when you need to hold global storage try to keep this local to your module, i.e. declare such variables `static` within the C source file.
Only if your variable really needs to be visible for the whole project (which is really rare) it deserves a declaration in the header, marked `extern`.

File content structure
----------------------

The content within each file is usually structured according to the following loose template:

* Copyright declaration
* Inclusion of used headers
* Necessary data structures and forward declarations
* Static module-private function implementations
* Externally visible function implementations
* Externally visible constant structures (pseudo-OOP definitions)

For header files header guards based on `#ifdef` should be used.
These header guards are historically placed **before** the Copyright declaration.
Stick to that for consistency please.

Example:

```c
#ifndef HEADER_FILENAME
#define HEADER_FILENAME
/*
htop - Filename.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/
```

Import and use of headers
-------------------------

We use the GPLv2+ as a shorthand indication that we release `htop` under the GNU Public license version 2 but are totally fine with users opting to apply the "any later version" clause.

Every file should import headers for all symbols it's using.
Thus when using a symbol from a header, even if that symbol is already imported by something else you use, you should declare an import for that header.
Doing so allows for easier restructuring of the code when things need to be moved around.
If you are unsure if all necessary headers are included you might use IWYU to tell you.

The list of includes should be the first thing in the file, after the copyright comment and be followed by two blank lines.
The include list should be in the following order, with each group separated by one blank line:

1. `include "config.h" // IWYU pragma: keep` if the global configuration
    from automake&autoconfigure or any of the feature guards for C library headers
    (like `__GNU_SOURCE`) are required, optional otherwise. Beware of the IWYU comment.
2. Accompanying module header file (for C source files only, missing inside headers)
3. List of used system headers (non-conditional includes)
4. List of used program headers
5. Conditionally included header files, system headers first

The list of headers should be sorted with includes from subdirectories following after files inside their parent directory.
Thus `unistd.h` sorts before `sys/time.h`.

Symbol Exports
--------------

Exports of symbols should be used sparingly.
Thus unless a function you write is intended to become public API of a module you should mark it as `static`.
If a function should be public API an appropriate declaration for that function has to be placed in the accompanying header file.

Please avoid function-like macros, in particular when exporting them in a header file.
They have several downsides (re-evaluation of arguments, syntactic escapes, weak typing) for which usually a better alternative like an actual function exists.
Furthermore when using function-like `define`s you may need to mark certain headers for IWYU so tracking of used symbols works.

Memory Management
-----------------

When allocating memory make sure to free resources properly.
For allocation this project uses a set of tiny wrappers around the common functions `malloc`, `calloc` and `realloc` named `xMalloc`, `xCalloc` and `xRealloc`.
These functions check that memory allocation worked and error out on failure.

Allocation functions assert the amount of memory requested is non-zero.
Trying to allocate 0 bytes of memory is an error.
Please use the explicit value `NULL` in this case and handle it in your code accordingly.

Working with Strings
--------------------

It is strongly encouraged to use the functions starting with `String_` from `XUtils.h` for working with zero-terminated strings as these make the API easier to use and are intended to make the intent of your code easier to grasp.

Thus instead of `!strcmp(foo, "foo")` it's preferred to use `String_eq(foo, "foo")` instead.
While sometimes a bit more to type, this helps a lot with making the code easier to follow.

Styling the code
----------------

Now for the style details that can mostly be automated: Indentation, spacing and bracing.
While there is no definitive code style we use, a set of rules loosely enforced has evolved.

Indentation in the code is done by three (3) spaces. No tabs are used. Ever.

Before and after keywords should be a space, e.g. `if (condition)` and `do { … } while (condition);`.

After opening and before closing braces a new line should be started.
Content of such encoded blocks should be indented one level further than their enclosing block.

If a line of source code becomes too long, or when structuring it into multiple parts for clarity, the continuation line should be indented one more level than the first line it continues:

```c
if (very_long_condition &&
   another_very_complex_expression &&
   something_else_to_check) {
   // Code follows as normal ...
} else {

}
```

Braces around simple single code statements (return, break, continue, goto, trivial assignments) are usually left out.

```c
if (answer)
   return 42;
```

If it helps readability (with several unrelated if statements in a row) or to avoid dangling-else situations braces can be added.

Control flow statements and the instruction making up their body should not be put on a single line,
i.e. after the condition of an if statement a new line should be inserted and the body indented accordingly.

```c
if (answer)
   return 42;
else if (again)
   continue;
else
   break;
```

When the statements that form control flow constructs are complex (e.g. more than just a simple assignment or jump) or need explanatory comments you should use braces.
If any block of such a statement uses braces then all blocks of that statement must have braces too.

```c
if ((fd = open(filename, O_RDONLY)) >= 0 &&
   (amtRead = read(buffer, sizeof(buffer))) > 0) {
   // Parse the information further ...
   metric = handleBufferContent(buffer, amtRead);
} else {
   metric = -1;
}

if (fd >= 0)
   close(fd);
```

While the existing code base isn't fully consistent with this code style yet it is strongly recommended that new code follows these rules.
Adapting surrounding code near places you need to touch is encouraged.
Try to separate such changes into a single, clean-up only commit to reduce noise while reviewing your changes.

When writing your code consistency with the surrounding codebase is favoured.

Don't shy away from leaving (single) blank lines to separate different groups of related statements.
They can be a great asset to structure the flow of a method.

```c
   int stuff = 0;

   // If asked for gives only half the answer ...
   if (param)
      stuff = 21;

   // Compute the answer
   stuff %= 2;
   stuff *= 4;
   stuff *= 5;
   stuff += !!stuff;
   stuff *= 2;

   return stuff;
```

If you want to automate formatting your code, the following command gives you a good baseline of how it should look:

```bash
astyle -r -xb -s3 -p -xg -c -k1 -W1 -H \*.c \*.h
```

Working with System APIs
------------------------

Please try to be considerate when using modern platform features.
While they usually provide quite a performance gain or make your life easier, it is beneficial if `htop` runs on rather ancient systems.
Thus when you want to use such features you should try to have an alternative available that works as a fallback.

An example for this are functions like `fstatat` on Linux that extend the kernel API on modern systems.
But even though it has been around for over a decade you are asked to provide a POSIX alternative like emulating such calls by `fstat` if this is doable.
If an alternative can not be provided you should gracefully downgrade. That could make a feature that requires this shiny API unavailable on systems that lack support for that API. Make this case visually clear to the user.

In general, code written for the project should be able to compile on any C99-compliant compiler.

Writing documentation
---------------------

The primary user documentation should be the man file which you can find in `htop.1.in`.

Additional documentation, like this file, should be written in gh-style markdown.
Make each sentence one line.
Markdown will combine these in output formats.
It does only insert a paragraph if you insert a blank line into the source file.
This way git can better diff and present the changes when documentation is altered.

Documentation files reside in the `docs/` directory and have a `.md` extension.

Writing pull-requests (PRs)
---------------------------

When writing your PR or patch, the set of patches should contain the minimal changes required.
Each patch in itself should ideally be self-contained and runable.

A PR should not contain any merge commits.
To follow the upstream branch of your PR rebase your work instead.

Avoid small commits that just fix typos that another of your commits introduced.
Instead squash those changes in the appropriate commit that introduced that mistake.
Git offers `git commit --fixup=<commit>` and `git rebase -i --autosquash` to help you with this.

Your final PR should contain a minimal set of reasonably sized commits that by themselves are easy to review.

Rebase early. Rebase often.
