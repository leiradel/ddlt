INCLUDES=-Isrc -Isrc/lua/src
CFLAGS=-O3 -Wall

LUA=src/lua/src/lapi.o src/lua/src/lauxlib.o src/lua/src/lbaselib.o \
	src/lua/src/lbitlib.o src/lua/src/lcode.o src/lua/src/lcorolib.o \
	src/lua/src/lctype.o src/lua/src/ldblib.o src/lua/src/ldebug.o \
	src/lua/src/ldo.o src/lua/src/ldump.o src/lua/src/lfunc.o \
 	src/lua/src/lgc.o src/lua/src/linit.o src/lua/src/liolib.o \
	src/lua/src/llex.o src/lua/src/lmathlib.o src/lua/src/lmem.o \
	src/lua/src/loadlib.o src/lua/src/lobject.o src/lua/src/lopcodes.o \
	src/lua/src/loslib.o src/lua/src/lparser.o src/lua/src/lstate.o \
	src/lua/src/lstring.o src/lua/src/lstrlib.o src/lua/src/ltable.o \
	src/lua/src/ltablib.o src/lua/src/ltm.o src/lua/src/lundump.o \
	src/lua/src/lutf8lib.o src/lua/src/lvm.o src/lua/src/lzio.o

%.o: %.c
	gcc $(INCLUDES) $(CFLAGS) -c $< -o $@

all: ddlt

ddlt: src/main.o src/lexer.o src/path.o src/templ.o src/realpath.o $(LUA)
	gcc -o $@ $+ -lm

src/main.o: src/main.c src/lexer.h src/path.h src/templ.h src/boot_lua.h

src/lexer.o: src/lexer.c src/lexer.h

src/path.o: src/path.c src/path.h

src/templ.o: src/templ.c src/templ.h

src/boot_lua.h: src/boot.lua
	xxd -i $< | sed "s@unsigned@const@" | sed "s@src_@@" > $@

clean:
	rm -f ddlt src/main.o src/lexer.o src/path.o src/templ.o src/realpath.o src/boot_lua.h

distclean: clean
	rm -f $(LUA)

.PHONY: clean distclean
