#!/bin/sh
# Paquet .deb de Phileas per a Debian i Ubuntu (el crida `make deb`).
# Fa servir l'executable ja compilat (./phileas), enllaçat dinàmicament amb
# ncursesw; hunspell es carrega amb dlopen i per això només és recomanat.
# Ús: packaging/mkdeb.sh VERSIÓ [carpeta de sortida]
set -eu

VERSIO="$1"
SORTIDA="${2:-dist}"
ARQ="$(dpkg --print-architecture)"
PAQ="phileas_${VERSIO}_${ARQ}"
ARREL="build/deb/$PAQ"

rm -rf "$ARREL"
make DESTDIR="$ARREL" PREFIX=/usr install install-desktop
strip --strip-unneeded "$ARREL/usr/bin/phileas"

DOC="$ARREL/usr/share/doc/phileas"
install -D -m 0644 README.md "$DOC/README.md"
install -D -m 0644 packaging/copyright "$DOC/copyright"
printf 'phileas (%s) unstable; urgency=low\n\n  * Versió %s.\n\n -- %s  %s\n' \
    "$VERSIO" "$VERSIO" "Àngel Solans <asolans@xtec.cat>" "$(date -R)" |
    gzip -9n >"$DOC/changelog.gz"
chmod 0644 "$DOC/changelog.gz"

MIDA="$(du -sk "$ARREL/usr" | cut -f1)"
mkdir -p "$ARREL/DEBIAN"
cat >"$ARREL/DEBIAN/control" <<EOF
Package: phileas
Version: $VERSIO
Architecture: $ARQ
Maintainer: Àngel Solans <asolans@xtec.cat>
Installed-Size: $MIDA
Depends: libc6 (>= 2.34), libncursesw6 (>= 6), libtinfo6 (>= 6)
Recommends: libhunspell-1.7-0, hunspell-ca, hunspell-es
Section: editors
Priority: optional
Homepage: https://github.com/asolans/phileas
Description: editor de text per a la consola, per escriure prosa en Markdown
 Phileas és un editor de terminal pensat per escriure text, no codi:
 ajust de línia suau, colors mínims de Markdown, correcció ortogràfica en
 català i castellà amb hunspell, desfer i refer, cerca i substitució,
 autodesat, menús, paleta d'ordres i navegador de fitxers. La interfície
 és en català.
EOF

mkdir -p "$SORTIDA"
fakeroot dpkg-deb --build -Zxz "$ARREL" "$SORTIDA/$PAQ.deb"
