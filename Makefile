# Compilació creuada: make CC=arm-linux-gcc PKG_CONFIG=.../pkg-config
CC        ?= cc
PKG_CONFIG ?= pkg-config
OPT       ?= -O2
CFLAGS    += $(OPT) -std=c11 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 \
             $(shell $(PKG_CONFIG) --cflags ncursesw)
LDLIBS    += $(shell $(PKG_CONFIG) --libs ncursesw)

SRC  = $(wildcard src/*.c)
OBJ  = $(SRC:src/%.c=build/%.o)
# Mòduls sense dependència de ncurses, que es poden provar aïlladament
CORE = build/editor.o build/spell.o build/dirlist.o build/utf8.o build/buffer.o build/wrap.o build/md.o build/undo.o build/search.o build/util.o

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

clean:
	rm -rf build phileas

-include $(OBJ:.o=.d)

.PHONY: test debug clean
