#!/bin/sh
# Type-checks src/*.c against minimal Win32 stub headers (no MinGW needed). NOT a real build.
cd "$(dirname "$0")/../../src" || exit 1
for f in *.c; do gcc -fsyntax-only -I../tools/syntax-check -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-misleading-indentation -Werror=implicit-function-declaration "$f"; done
