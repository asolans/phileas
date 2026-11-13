# SQLite per a Recerca

Recerca compila l'amalgamació de SQLite dins del binari (FTS5 activat), perquè
el PC i la Pi tinguin el mateix motor i les mateixes opcions.

Cal posar aquí `sqlite3.c` i `sqlite3.h` de l'amalgamació oficial
(<https://www.sqlite.org/download.html>, `sqlite-amalgamation-*.zip`).
No es desen al repositori (9 MB); `make recerca` avisa si falten.

Opcions de compilació: vegeu `SQLDEF` al Makefile.
