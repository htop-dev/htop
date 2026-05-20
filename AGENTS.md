# AGENTS.md

This file provides guidance to AI coding agents working with code in this repository.

## Build Commands

```bash
# Build for local platform - htop binary
./autogen.sh && ./configure && make

# Build with Performance Co-Pilot (PCP) support - pcp-htop binary
./autogen.sh && ./configure --enable-pcp && make

# Rebuild after code changes (no autogen needed)
make
```

Key configure flags: `--enable-capabilities`, `--enable-debug`, `--enable-delayacct`, `--enable-pcp`, `--enable-sensors`, `--enable-unicode`.

## Code Style (from docs/styleguide.md)

- **Indentation**: 3 spaces, never tabs
- **Filenames**: CamelCase.c/.h (exceptions: `htop.c`, `pcp-htop.c`); directories lowercase
- **Functions**: `ModuleName_functionName()` (e.g. `Process_compare()`)
- **Variables**: short and precise (`i` not `someCounterValueForLoop`); module-private globals must be `static`
- **Header guards**: `#ifndef HEADER_FILENAME` placed **before** the copyright comment
- **Include order** (each group separated by blank line):
  1. `#include "config.h" // IWYU pragma: keep` (required if using XUtils.h)
  2. Accompanying module header (in .c files only)
  3. System headers
  4. Program headers
  5. Conditional headers
- Includes must be sorted alphabetically, with subdirectory paths sorting after parent-level files (`unistd.h` before `sys/time.h`)
- **Braces**: omit around simple single statements (return, break, continue, trivial assignments); never put control flow body on same line as condition; if any block in an if/else chain needs braces, all blocks get braces
- **Exports**: mark functions `static` unless they are public API with a header declaration; avoid function-like macros
- **Strings**: use `String_eq()`, `String_cat()` etc. from XUtils.h instead of raw strcmp/strcat
- **Memory**: use `xMalloc()`, `xCalloc()`, `xRealloc()` (never raw malloc); use `xReallocArray()` for arrays; never allocate 0 bytes
- **Blank lines**: use single blank lines to separate groups of related statements within functions

## Architecture

htop is a cross-platform interactive process viewer in C99. It uses an object-oriented C pattern with virtual method tables.

### Class hierarchy

`Object` (base with vtable: Display, Compare, Delete) → `Row` → `Process` (per-process data)
`Object` → `Panel` (UI widget with event handling, scrolling)
`Object` → `Meter` (system metric with text/bar/graph modes)

### Platform abstraction

Each platform directory (`linux/`, `darwin/`, `freebsd/`, `netbsd/`, `openbsd/`, `dragonflybsd/`, `solaris/`, `pcp/`, `unsupported/`) provides:
- `Platform.c/.h` — platform init/done, load average, clock, etc.
- `<Platform>Machine.c/.h` — system state (CPU counts, memory, uptime)
- `<Platform>ProcessTable.c/.h` — process enumeration from OS
- `<Platform>Process.c/.h` — platform-specific process fields

The `pcp/` platform uses PMAPI to fetch metrics and supports dynamic column/meter/screen definitions via config files in `pcp/columns/`, `pcp/meters/`, `pcp/screens/`.

### Main loop

`htop.c` or `pcp-htop.c` → `CommandLine_run()` → `ScreenManager_run()` which loops:
1. Timer-based recalculation (fetch process data, update meters)
2. Redraw if dirty
3. Read keyboard input via `Panel_getCh()`
4. Dispatch to action handler → returns `Htop_Reaction` flags (REFRESH, RECALCULATE, QUIT, etc.)

### Key subsystems

- **Action.c** — keyboard action handlers, each returns `Htop_Reaction`
- **Settings.c** — config persistence (`~/.config/htop/htoprc`)
- **Header.c** — meter layout in the top area
- **FunctionBar.c** — F1-F10 key labels at bottom
- **CRT.c** — ncurses abstraction (colors, attributes, input)
- **Vector.c / Hashtable.c** — generic containers
- **XUtils.c** — memory wrappers, string utilities (requires `config.h` in any .c that uses it)
- **RichString.c** — attributed/colored string buffers
- **generic/** — routines that may be shared across multiple platforms

## AI Contributions Policy

Disclose AI use via `Assisted-by:` or `Co-authored-by:` commit trailers. Contributor is fully responsible for quality and license compliance. See `docs/ai-contributions-policy.md`.

Sign off commits with a `Signed-off-by:` trailer (`git commit -s`) to certify the [Developer Certificate of Origin](https://developercertificate.org/). Sign-off is distinct from AI disclosure: `Assisted-by:` records tool use; `Signed-off-by:` records human responsibility. An agent must not sign off on a contributor's behalf without confirmation.
