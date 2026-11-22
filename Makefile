# WinShell (C port) - build with MinGW-w64
#   Linux/macOS cross-compile:  make            (needs gcc-mingw-w64-x86-64 / -i686)
#   Windows (MSYS2 MinGW):      make CC=gcc WINDRES=windres
SRC      := $(wildcard src/*.c)
CFLAGS   ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -D_UNICODE -DUNICODE
LDFLAGS  ?= -mwindows -static -static-libgcc -s
LIBS     := -luser32 -lgdi32 -lshell32 -lmsimg32 -lcomdlg32 -ladvapi32 -lkernel32

CC64      ?= x86_64-w64-mingw32-gcc
CC32      ?= i686-w64-mingw32-gcc
WINDRES64 ?= x86_64-w64-mingw32-windres
WINDRES32 ?= i686-w64-mingw32-windres

all: dist/WinShell-x64.exe dist/WinShell-x86.exe

build/%-x64.o: src/%.c src/app.h src/json.h ; @mkdir -p build ; $(CC64) $(CFLAGS) -c $< -o $@
build/%-x86.o: src/%.c src/app.h src/json.h ; @mkdir -p build ; $(CC32) $(CFLAGS) -c $< -o $@
build/res-x64.o: src/winshell.rc src/winshell.manifest src/winshell.ico ; @mkdir -p build ; cd src && $(WINDRES64) winshell.rc -O coff -o ../$@
build/res-x86.o: src/winshell.rc src/winshell.manifest src/winshell.ico ; @mkdir -p build ; cd src && $(WINDRES32) winshell.rc -O coff -o ../$@

OBJ64 := $(patsubst src/%.c,build/%-x64.o,$(SRC)) build/res-x64.o
OBJ32 := $(patsubst src/%.c,build/%-x86.o,$(SRC)) build/res-x86.o

dist/WinShell-x64.exe: $(OBJ64) ; @mkdir -p dist ; $(CC64) $(OBJ64) -o $@ $(LDFLAGS) $(LIBS)
dist/WinShell-x86.exe: $(OBJ32) ; @mkdir -p dist ; $(CC32) $(OBJ32) -o $@ $(LDFLAGS) $(LIBS)

# Native build on Windows/MSYS2: single exe
native: CC64 := $(CC)
native: WINDRES64 := windres
native: dist/WinShell-x64.exe

clean: ; rm -rf build dist
.PHONY: all native clean
