# Compilació creuada: make CC=arm-linux-gcc PKG_CONFIG=.../pkg-config
CC        ?= cc
PKG_CONFIG ?= pkg-config
OPT       ?= -O2
CFLAGS    += $(OPT) -std=c11 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 \
             $(shell $(PKG_CONFIG) --cflags ncursesw)
LDLIBS    += $(shell $(PKG_CONFIG) --libs ncursesw)
# Executable estàtic (Phileas portable): make HUNSPELL_STATIC=1 LDFLAGS=-static
# hi enllaça hunspell, que no es pot carregar amb dlopen
ifdef HUNSPELL_STATIC
CFLAGS    += -DPHILEAS_HUNSPELL_STATIC
LDLIBS    += $(shell $(PKG_CONFIG) --static --libs hunspell) -lstdc++
endif

SRC  = $(wildcard src/*.c)
OBJ  = $(SRC:src/%.c=build/%.o)
# Mòduls sense dependència de ncurses, que es poden provar aïlladament
CORE = build/autosave.o build/complete.o build/config.o build/editor.o build/spell.o build/dirlist.o build/utf8.o build/buffer.o build/wrap.o build/md.o build/undo.o build/search.o build/util.o

phileas: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

build:
	mkdir -p build

test: build/test_phileas
	./build/test_phileas

build/test_phileas: tests/test.c $(CORE)
	$(CC) $(CFLAGS) -Isrc $(LDFLAGS) -o $@ $^

# Compilació de depuració amb sanititzadors: make clean && make debug
debug: OPT = -O0 -g -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: phileas build/test_phileas

# Instal·lació (també per a un paquet de buildroot: make DESTDIR=... install)
PREFIX  ?= /usr
install: phileas
	install -D -m 0755 phileas $(DESTDIR)$(PREFIX)/bin/phileas

clean:
	rm -rf build phileas

-include $(OBJ:.o=.d)

.PHONY: test debug clean install
