# SQLite per a Recerca

Recerca compila l'amalgamació de SQLite dins del binari (FTS5 activat), perquè
el PC i la Pi tinguin el mateix motor i les mateixes opcions.

Cal posar aquí `sqlite3.c` i `sqlite3.h` de l'amalgamació oficial
(<https://www.sqlite.org/download.html>, `sqlite-amalgamation-*.zip`).
No es desen al repositori (9 MB); `make recerca` avisa si falten.

Versió en ús: **3.51.3** (la de Buildroot), baixada el 2026-11-13:

    curl -O https://www.sqlite.org/2026/sqlite-amalgamation-3510300.zip
    # 2.876.305 bytes, sha256 acb1e6f5d832484bf6d32b681e858c38add8b2acdfd42ac5df24b8afb46552b4
    unzip -j sqlite-amalgamation-3510300.zip '*/sqlite3.c' '*/sqlite3.h' -d recerca/sqlite/

Opcions de compilació: vegeu `SQLDEF` al Makefile.
