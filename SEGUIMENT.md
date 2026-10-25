# Phileas — Document de seguiment

Editor de text per a consola (Linux), orientat a escriure prosa en Markdown.

## Estat

- **Fase actual:** 0 — Decisió d'arquitectura
- **Última actualització:** 2026-10-25

## Requeriments (versió inicial)

| # | Requeriment | Prioritat |
|---|-------------|-----------|
| R1 | Només consola, només Linux | Imprescindible |
| R2 | Format Markdown: només marcatge, sense previsualització. Colors mínims (capçaleres) | Imprescindible |
| R3 | Ha de funcionar en Raspberry Pi de pocs recursos | Imprescindible |
| R4 | Wordwrap i nombre de columnes per línia configurable | Imprescindible |
| R5 | Eines d'edició: cerca-substitució, desfer, copiar/enganxar | Imprescindible |
| R6 | Diccionaris (correcció ortogràfica, català com a mínim) | Alta |
| R7 | Un sol document obert alhora | Imprescindible |
| R8 | Navegador de fitxers per obrir documents nous | Alta |
| R9 | UTF-8 complet (accents, ç, l·l, guions, cometes tipogràfiques) | Imprescindible (implícit) |

## Decisió de llenguatge

### Opcions analitzades

| Criteri | C | C++ | Lua | Rust |
|---------|---|-----|-----|------|
| Consum de memòria / CPU (R3) | Excel·lent | Excel·lent | Bo (intèrpret ~300 KB) | Excel·lent |
| Compilació a la mateixa RPi | Ràpida | Moderada | No cal | Lenta i pesada |
| Accés a ncursesw / hunspell | Directe (API C) | Directe | Necessita bindings en C | Crates (bindings) |
| Gestió de text UTF-8 | Manual (cal disciplina) | Manual / std limitada | Feble (`utf8` bàsic) | Excel·lent |
| Dependències en temps d'execució | Mínimes | libstdc++ | Lua + mòduls C | Mínimes |
| Seguretat de memòria | Responsabilitat nostra | Millor amb RAII | Garantida | Garantida |
| Referents existents | nano, kilo, vim, mg | — | — (textadept és C+Lua) | helix, kibi |

### Proposta: **C (C11) + ncursesw + hunspell**

Motius:

1. **Recursos (R3):** binari petit (centenars de KB), sense runtime, arrenca a l'instant en una RPi Zero.
2. **Les dues biblioteques clau són en C:** `ncursesw` (pantalla, teclat, colors, amplada de caràcters) i `libhunspell` (diccionaris; ja hi ha `ca`, `ca_ES-valencia`, `es`, `en_US` instal·lats al sistema). Ús directe, sense capes.
3. **Abast acotat:** un editor d'un sol document és un problema ben conegut i de mida manejable en C (kilo són ~1000 línies; nano és C).
4. **Portabilitat a ARM:** compila amb `gcc` a qualsevol Raspberry Pi OS sense eines addicionals.

Riscos i mitigacions:

- *Errors de memòria* → compilar amb `-Wall -Wextra`, proves amb `-fsanitize=address,undefined` i Valgrind.
- *Complexitat UTF-8* → aïllar-ho en un mòdul propi (`utf8.c`) des del principi; usar `wcwidth()` per a l'amplada en pantalla.

Alternativa raonable: **Lua amb nucli en C** (C per a pantalla i buffer, Lua per a configuració i extensions). Es pot afegir més endavant sense reescriure res si el nucli es dissenya en C.

**Estat de la decisió:** ⏳ pendent de confirmar.

### Anàlisi: C sol vs. C + Lua per a la configuració

**C sol.** Fitxer de configuració declaratiu (`clau = valor`, estil `nanorc`) llegit per un analitzador propi d'unes 150–300 línies. Cobreix columnes, colors, diccionari i dreceres. No permet lògica, condicions ni ordres noves sense recompilar.

**C + Lua.** El fitxer de configuració és un programa Lua (`~/.config/phileas/init.lua`). A més d'opcions, permet definir ordres pròpies, dreceres que criden funcions i *hooks* (en obrir, en desar…).

Hi ha tres nivells d'integració:

| Nivell | Què fa Lua | Exemples |
|--------|-----------|----------|
| A | Només llegeix la configuració | — |
| B | Configuració + ordres d'usuari + hooks | neovim, micro |
| C | La major part de l'editor és en Lua; C és el nucli | textadept, wordgrinder |

| Aspecte | C sol | C + Lua (nivell B) |
|---------|-------|--------------------|
| Mida del binari | ~100–200 KB | +~250 KB (Lua 5.4 estàtic) |
| RAM addicional | 0 | ~20–100 KB (estat Lua + configuració) |
| Temps d'arrencada | Instantani | Imperceptible (<5 ms) |
| Dependències | ncursesw, hunspell | + liblua5.4, o les fonts incloses al repositori (MIT, ~30 fitxers .c) |
| Codi propi per a la configuració | Analitzador + validació | API C↔Lua (pila de Lua) + gestió d'errors (`pcall`) |
| Extensibilitat de l'usuari | Cap | Alta |
| Errors de configuració | Missatge de l'analitzador | Error Lua a la barra d'estat; l'editor ha de continuar funcionant |
| Proves i manteniment | Un llenguatge | Dos llenguatges i una API pública que cal mantenir estable |
| Seguretat | El fitxer és només dades | La configuració executa codi (com a vim o emacs; acceptable en ús personal) |

Punts clau:

- **El cost real de Lua no són els recursos** (a la RPi són negligibles), sinó el disseny i manteniment de l'API que s'exposa (`phileas.buffer`, `phileas.bind`, `phileas.on`…).
- **El text de Lua no entén UTF-8:** `string.sub` treballa amb bytes. Cal exposar des de C funcions UTF-8 (paraula sota el cursor, longitud en caràcters, etc.) perquè les extensions no trenquin els accents.
- **El factor decisiu és arquitectònic:** si el nucli es dissenya des del principi amb un **registre d'ordres amb nom** (`save`, `search`, `wrap-toggle`…) i les dreceres apunten a noms d'ordre, afegir Lua més endavant és barat: Lua només registra ordres noves i crida les existents. Sense aquest registre, afegir-lo a posteriori obliga a refactoritzar.

**Recomanació:** començar amb **C sol + fitxer declaratiu**, però amb el registre d'ordres des de la F1. Decidir si s'afegeix Lua (nivell B) quan apareguin necessitats concretes (macros, plantilles, exportació amb pandoc, recompte de paraules personalitzat…). Si ja se sap que es volen extensions, val la pena decidir-ho ara per dissenyar l'API amb temps.

## Arquitectura preliminar (si es confirma C)

```
src/
  main.c        arrencada, arguments, bucle principal
  buffer.c      emmagatzematge del text (gap buffer o vector de línies)
  utf8.c        utilitats UTF-8 i amplada de caràcters
  wrap.c        wordwrap suau a N columnes (línies lògiques vs. visuals)
  render.c      dibuix amb ncursesw, colors de Markdown mínims
  input.c       tecles i dreceres
  search.c      cerca i substitució
  undo.c        historial desfer/refer
  spell.c       integració amb hunspell
  filebrowser.c navegació pel disc i obertura de fitxers
  config.c      configuració (~/.config/phileas/)
```

Decisions de disseny pendents:

- Estructura del buffer: *gap buffer* vs. *vector de línies* vs. *piece table*.
- Wordwrap: suau (només visual) — recomanat per a prosa en Markdown — o dur (insereix salts).
- Format del fitxer de configuració.
- Joc de dreceres de teclat (estil nano? propi?).

## Entorn de desenvolupament

- gcc 14.2, ncurses 6.5 (`libncurses-dev` ✅ instal·lat)
- hunspell 1.7.2: biblioteca ✅, **`libhunspell-dev` ❌ cal instal·lar-lo** (`sudo apt install libhunspell-dev`)
- Diccionaris disponibles a `/usr/share/hunspell/`: ca, ca_ES, ca_ES-valencia, es, en_US

## Full de ruta (provisional)

- [ ] **F0** — Confirmar llenguatge i estructura del buffer
- [ ] **F1** — Esquelet: obrir, mostrar, desplaçar-se, desar un fitxer UTF-8
- [ ] **F2** — Edició bàsica + wordwrap a N columnes
- [ ] **F3** — Desfer/refer, cerca i substitució
- [ ] **F4** — Colors mínims de Markdown (capçaleres, èmfasi)
- [ ] **F5** — Navegador de fitxers
- [ ] **F6** — Correcció ortogràfica amb hunspell
- [ ] **F7** — Configuració i proves en Raspberry Pi

## Registre

- **2026-10-25** — Inici del projecte. Repositori git creat. Anàlisi de llenguatges; proposta: C + ncursesw + hunspell.
- **2026-10-25** — Anàlisi C sol vs. C + Lua per a la configuració. Recomanació: C sol amb registre d'ordres, deixant Lua com a ampliació opcional.
