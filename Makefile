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
CORE = build/autosave.o build/calaix.o build/theme.o build/complete.o build/config.o build/editor.o build/spell.o build/dirlist.o build/utf8.o build/buffer.o build/wrap.o build/md.o build/outline.o build/todo.o build/footnote.o build/history.o build/undo.o build/search.o build/util.o build/chars.o build/stats.o build/pairs.o build/comu_rc.o

phileas: $(OBJ) build/comu_rc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -Isrc -Icomu -MMD -MP -c -o $@ $<

build:
	mkdir -p build

test: build/test_phileas
	./build/test_phileas

build/test_phileas: tests/test.c $(CORE)
	$(CC) $(CFLAGS) -Isrc -Icomu $(LDFLAGS) -o $@ $^


# ---- Fàbula (eina companya, fabula/): objectes compartits de src/ i propis ----
FSHARED = build/utf8.o build/util.o build/md.o build/buffer.o build/undo.o build/outline.o
FOBJ    = build/comu_yaml.o build/comu_fold.o build/fabula_project.o build/fabula_find.o build/fabula_compile.o build/fabula_diary.o build/fabula_timeline.o
FUI     = build/fabula_flat.o build/fabula_table.o build/comu_fterm.o build/comu_ui.o build/comu_chooser.o build/fabula_fview.o build/comu_launch.o build/fabula_main.o build/comu_rc.o
FSHARED_UI = build/theme.o build/keys.o

.PHONY: fabula test-fabula
# L'executable és build/fabula (el nom «fabula» és el de la carpeta de fonts)
fabula: build/fabula

build/fabula: $(FOBJ) $(FUI) $(FSHARED) $(FSHARED_UI)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/fabula_%.o: fabula/%.c | build
	$(CC) $(CFLAGS) -Isrc -Ifabula -Icomu -MMD -MP -c -o $@ $<

build/test_fabula: tests/test_fabula.c $(FOBJ) build/fabula_flat.o build/fabula_table.o $(FSHARED)
	$(CC) $(CFLAGS) -Isrc -Ifabula -Icomu $(LDFLAGS) -o $@ $^

test-fabula: build/test_fabula
	./build/test_fabula

# Compilació de depuració amb sanititzadors: make clean && make debug
debug: OPT = -O0 -g -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: phileas build/test_phileas

# Instal·lació (també per a un paquet de buildroot: make DESTDIR=... install)
PREFIX  ?= /usr
install: phileas build/fabula build/recerca
	install -D -m 0755 phileas $(DESTDIR)$(PREFIX)/bin/phileas
	install -D -m 0755 build/fabula $(DESTDIR)$(PREFIX)/bin/fabula
	install -D -m 0755 build/recerca $(DESTDIR)$(PREFIX)/bin/recerca

# Llançador per als escriptoris (GNOME, KDE...): obre Phileas en un terminal.
# No forma part d'install perquè la imatge de la BYOK no té escriptori.
install-desktop:
	install -D -m 0644 packaging/phileas.desktop $(DESTDIR)$(PREFIX)/share/applications/phileas.desktop
	install -D -m 0644 packaging/phileas.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/phileas.svg

# Paquets a dist/: .deb (Debian, Ubuntu) i codi font de HEAD en .tar.gz
VERSION = $(shell sed -n 's/^\#define PHILEAS_VERSION *"\(.*\)"/\1/p' src/version.h)
deb: phileas build/fabula build/recerca
	sh packaging/mkdeb.sh $(VERSION) dist

# El codi de HEAD i l'amalgamació de SQLite, que no és a git (recerca/sqlite/)
dist: recerca/sqlite/sqlite3.c recerca/sqlite/sqlite3.h
	mkdir -p dist
	git archive --format=tar --prefix=phileas-$(VERSION)/ -o dist/phileas-$(VERSION).tar HEAD
	tar --append -f dist/phileas-$(VERSION).tar --transform 's|^|phileas-$(VERSION)/|' \
		recerca/sqlite/sqlite3.c recerca/sqlite/sqlite3.h
	gzip -9nf dist/phileas-$(VERSION).tar

# Manual en PDF (cal pandoc i LaTeX; la capçalera defineix els caràcters Unicode que pdflatex no coneix)
manual:
	mkdir -p dist
	pandoc -f gfm MANUAL.md -o dist/MANUAL.pdf -H packaging/manual-cap.tex -V lang=ca -V geometry:margin=2.5cm --toc

# clean no toca dist/ (els paquets i les notes de les versions); distclean, sí
clean:
	rm -rf build phileas

distclean: clean
	rm -rf dist

-include $(OBJ:.o=.d)
-include $(wildcard build/fabula_*.d)
# ---- Recerca (recerca/, codi comú a comu/): SQLite amalgamat amb FTS5 ----
SQLDEF  = -DSQLITE_ENABLE_FTS5 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION \
          -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DQS=0 -DSQLITE_OMIT_DEPRECATED \
          -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
RCOMU   = build/comu_fold.o build/comu_mdscan.o build/comu_front.o build/comu_rc.o
ROBJ    = $(RCOMU) build/recerca_vault.o build/recerca_model.o build/recerca_nova.o build/comu_yaml.o build/recerca_sqlite3.o build/utf8.o build/util.o build/theme.o
RUI     = build/recerca_rview.o build/recerca_main.o build/comu_fterm.o build/comu_ui.o build/comu_chooser.o build/comu_launch.o build/keys.o
RINC    = -Isrc -Icomu -Irecerca -Irecerca/sqlite

.PHONY: recerca test-recerca
# L'executable és build/recerca (el nom «recerca» és el de la carpeta de fonts)
recerca: build/recerca
build/recerca: $(ROBJ) $(RUI)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lm
recerca/sqlite/sqlite3.c recerca/sqlite/sqlite3.h:
	@echo "Falta l'SQLite: vegeu recerca/sqlite/LLEGEIX.md"; exit 1
build/recerca_sqlite3.o: recerca/sqlite/sqlite3.c | build
	$(CC) $(OPT) $(SQLDEF) -c -o $@ $<
build/recerca_%.o: recerca/%.c recerca/sqlite/sqlite3.h | build
	$(CC) $(CFLAGS) $(RINC) -MMD -MP -c -o $@ $<
build/comu_%.o: comu/%.c | build
	$(CC) $(CFLAGS) $(RINC) -MMD -MP -c -o $@ $<
build/test_recerca: tests/test_recerca.c $(ROBJ)
	$(CC) $(CFLAGS) $(RINC) $(LDFLAGS) -o $@ $^ -lm
test-recerca: build/test_recerca
	./build/test_recerca
-include $(wildcard build/recerca_*.d build/comu_*.d)


.PHONY: test debug clean distclean install install-desktop deb dist manual
