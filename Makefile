INCLUDES=-Isrc -I/usr/include/lua5.2
CFLAGS=-O3 -Wall
LUALIB=-llua

ifeq ($(shell uname -s),)
  TARGET=ddlt.dll
else ifneq ($(findstring MINGW,$(shell uname -a)),)
  TARGET=ddlt.dll
else ifneq ($(findstring Darwin,$(shell uname -a)),)
  TARGET=ddlt.so
else ifneq ($(findstring win,$(shell uname -a)),)
  TARGET=ddlt.dll
else
  TARGET=ddlt.so
	CFLAGS+=-fPIC
	LUALIB=-llua5.2
endif

%.o: %.c
	gcc $(INCLUDES) $(CFLAGS) -c $< -o $@

all: $(TARGET)

$(TARGET): src/ddlt.o src/lexer.o src/path.o src/templ.o src/realpath.o
	gcc -shared -o $@ $+ $(LUALIB) -lm

src/ddlt.o: src/ddlt.c src/lexer.h src/path.h src/templ.h src/boot_lua.h

src/lexer.o: src/lexer.c src/lexer.h src/lexer_cpp.c src/lexer_bas.c src/lexer_pas.c

src/path.o: src/path.c src/path.h

src/templ.o: src/templ.c src/templ.h

src/boot_lua.h: src/boot.lua
	xxd -i $< | sed "s@unsigned@const@" | sed "s@src_@@" > $@

install: $(TARGET)
	cp $(TARGET) $(INST_LIBDIR)

clean:
	rm -f $(TARGET) src/ddlt.o src/lexer.o src/path.o src/templ.o src/realpath.o src/boot_lua.h

.PHONY: clean
