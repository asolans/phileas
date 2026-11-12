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
CORE = build/autosave.o build/calaix.o build/theme.o build/complete.o build/config.o build/editor.o build/spell.o build/dirlist.o build/utf8.o build/buffer.o build/wrap.o build/md.o build/outline.o build/todo.o build/footnote.o build/history.o build/undo.o build/search.o build/util.o

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


# ---- Fàbula (eina companya, fabula/): objectes compartits de src/ i propis ----
FSHARED = build/utf8.o build/util.o build/md.o build/buffer.o build/undo.o build/outline.o
FOBJ    = build/fabula_yaml.o build/fabula_project.o build/fabula_find.o build/fabula_compile.o build/fabula_diary.o build/fabula_timeline.o
FUI     = build/fabula_flat.o build/fabula_table.o build/fabula_fterm.o build/fabula_fview.o build/fabula_launch.o build/fabula_main.o
FSHARED_UI = build/theme.o build/keys.o

.PHONY: fabula test-fabula
# L'executable és build/fabula (el nom «fabula» és el de la carpeta de fonts)
fabula: build/fabula

build/fabula: $(FOBJ) $(FUI) $(FSHARED) $(FSHARED_UI)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/fabula_%.o: fabula/%.c | build
	$(CC) $(CFLAGS) -Isrc -Ifabula -MMD -MP -c -o $@ $<

build/test_fabula: tests/test_fabula.c $(FOBJ) build/fabula_flat.o build/fabula_table.o $(FSHARED)
	$(CC) $(CFLAGS) -Isrc -Ifabula $(LDFLAGS) -o $@ $^

test-fabula: build/test_fabula
	./build/test_fabula

# Compilació de depuració amb sanititzadors: make clean && make debug
debug: OPT = -O0 -g -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: phileas build/test_phileas

# Instal·lació (també per a un paquet de buildroot: make DESTDIR=... install)
PREFIX  ?= /usr
install: phileas build/fabula
	install -D -m 0755 phileas $(DESTDIR)$(PREFIX)/bin/phileas
	install -D -m 0755 build/fabula $(DESTDIR)$(PREFIX)/bin/fabula

# Llançador per als escriptoris (GNOME, KDE...): obre Phileas en un terminal.
# No forma part d'install perquè la imatge de la BYOK no té escriptori.
install-desktop:
	install -D -m 0644 packaging/phileas.desktop $(DESTDIR)$(PREFIX)/share/applications/phileas.desktop
	install -D -m 0644 packaging/phileas.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/phileas.svg

# Paquets a dist/: .deb (Debian, Ubuntu) i codi font de HEAD en .tar.gz
VERSION = $(shell sed -n 's/^\#define PHILEAS_VERSION *"\(.*\)"/\1/p' src/version.h)
deb: phileas
	sh packaging/mkdeb.sh $(VERSION) dist

dist:
	mkdir -p dist
	git archive --format=tar.gz --prefix=phileas-$(VERSION)/ -o dist/phileas-$(VERSION).tar.gz HEAD

# Manual en PDF (cal pandoc i LaTeX; la capçalera defineix els caràcters Unicode que pdflatex no coneix)
manual:
	mkdir -p dist
	pandoc -f gfm MANUAL.md -o dist/MANUAL.pdf -H packaging/manual-cap.tex -V lang=ca -V geometry:margin=2.5cm --toc

clean:
	rm -rf build phileas dist

-include $(OBJ:.o=.d)
-include $(wildcard build/fabula_*.d)

.PHONY: test debug clean install install-desktop deb dist manual
