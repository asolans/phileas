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

**Estat de la decisió:** ✅ Confirmada (2026-10-25): **C sol**, amb un fitxer de configuració declaratiu i un registre d'ordres amb nom des de la F1. Lua queda com a possible ampliació futura.

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

## Decisió d'estructura del buffer

### Context que condiciona la tria

- **Prosa en Markdown:** cada paràgraf és una sola línia lògica, que pot fer de centenars a milers de bytes. El wordwrap és visual.
- **Mida típica:** des d'un article (10–50 KB) fins a una novel·la (0,5–2 MB, 5.000–40.000 paràgrafs).
- **Operacions que en depenen:**
  - dibuix amb wordwrap (cal saber on comença cada paràgraf);
  - cerca i substitució;
  - correcció ortogràfica per paràgraf;
  - desfer/refer.
- **Codi:** ha de ser simple i robust en C.

### Opcions

| Estructura | Idea | Avantatges | Inconvenients |
|-----------|------|------------|---------------|
| **Vector de línies** | Array de punters a línies; cada línia és un array dinàmic d'UTF-8 | Molt simple. Encaixa amb Markdown (línia = paràgraf). Permet cache per línia (wrap, colors, ortografia). Cerca per línia amb `regexec` | Inserir o esborrar línies desplaça l'array (memmove de punters: ~300 KB en el pitjor cas, negligible). Les cerques que travessen línies són més incòmodes |
| **Gap buffer únic** | Tot el text en un bloc amb un forat al cursor | Molt simple i molt compacte. Cerca sobre memòria contigua | Cal un índex d'inicis de línia a part, que s'ha d'anar actualitzant. Saltar lluny mou el forat (memmove de fins a 2 MB) |
| **Piece table** | Fitxer original intacte + buffer d'afegits + taula de peces | El desfer és gairebé gratuït. Obertura instantània (mmap) | Més complexa. La cerca i el dibuix travessen peces. Cal índex de línies igualment |
| **Rope** | Arbre de fragments | Escala a fitxers enormes | Excessiu per a aquest ús. Molt codi |
| **Vector de línies amb gap per línia** | Com el primer, però cada línia té el seu forat | Edició O(1) dins paràgrafs llargs | Optimització prematura: un memmove de 5 KB són nanosegons |

Referents: nano i mg (llista de línies), kilo (vector de línies), emacs (gap buffer), VS Code i vis (piece table).

### Proposta: **vector de línies**

```c
typedef struct {
    char   *text;       /* UTF-8, sense '\n' final */
    size_t  len, cap;
    /* caches, invalidades en editar la línia */
    int     wrap_dirty; /* posicions de tall visual */
    int     spell_dirty;
} Line;

typedef struct {
    Line   *lines;
    size_t  nlines, cap;
    int     modified;
} Buffer;

/* Posició: (línia, offset en bytes); sempre en una frontera de caràcter UTF-8 */
typedef struct { size_t line, col; } Pos;
```

Motius:

1. **Línia = paràgraf:** l'editor pensa en les mateixes unitats que l'escriptor. El wordwrap, l'ortografia i els colors de Markdown es calculen i es guarden a la cache paràgraf a paràgraf. Una edició només n'invalida un.
2. **Rendiment sobrat a la RPi:** cap operació habitual mou més de pocs KB. Sobrecost de memòria: ~40 bytes per línia (40.000 línies → 1,6 MB en el cas extrem).
3. **Codi mínim i fàcil de depurar,** que és el que més compta en C.
4. **Desfer/refer independent:** un registre d'operacions (`insert`/`delete` amb posició i text), agrupant les pulsacions consecutives. No depèn de l'estructura.

Salvaguarda: la resta de l'editor només accedirà al text a través de l'API de `buffer.c` (`buf_insert`, `buf_delete`, `buf_line`, `buf_nlines`…). Si mai calgués canviar a un gap buffer o una piece table, el canvi quedaria localitzat en aquest mòdul.

**Estat de la decisió:** ✅ Confirmada (2026-10-25): **vector de línies** (línia = paràgraf).

## Decisió de wordwrap

### Punt de partida: totes dues opcions són Markdown vàlid

En Markdown, un salt de línia simple dins d'un paràgraf es converteix en un espai en el resultat final. Un paràgraf en una sola línia i el mateix paràgraf tallat a 72 columnes donen el mateix resultat. La diferència és només de **com s'edita** i de **com es veu el fitxer** fora de Phileas.

### Comparativa

| Aspecte | Suau (visual) | Dur (salts reals al fitxer) |
|---------|---------------|-----------------------------|
| Editar al mig d'un paràgraf | El text es reorganitza sol | Cal reformatar el paràgraf (automàticament o amb una ordre, com `gq` a vim) |
| Canviar el nombre de columnes | Instantani, el fitxer no canvia | Cal reformatar tot el document |
| Encaix amb el vector de línies | Perfecte: 1 línia = 1 paràgraf | Un paràgraf són N línies: cal agrupar-les per a la cache, l'ortografia i el reformatat |
| Cerca | Una frase mai queda partida | «de la casa» no es troba si hi ha un salt entre «la» i «casa» |
| Reformatar sense trencar el Markdown | No cal | Cal reconèixer llistes, capçaleres, blocs de codi, taules i cites per no ajuntar-los |
| Fitxer vist amb `cat`, `less` o el correu | Línies molt llargues | Es veu ben formatat |
| Diffs de git | Un canvi petit marca tot el paràgraf | Diffs més fins (però reformatar també genera soroll) |
| Complexitat del codi | Baixa | Mitjana-alta |

### Proposta: **wordwrap suau** com a model d'edició

Detalls de comportament proposats:

1. **Amplada:** `columnes = 72` per defecte, configurable. Si el terminal és més estret, s'ajusta a l'amplada del terminal.
2. **Talls:** a les fronteres de paraula, mesurant l'amplada real dels caràcters amb `wcwidth()`. Les paraules més llargues que l'amplada (URL) es tallen per caràcter. No es talla mai per dins d'un caràcter UTF-8 ni d'una seqüència combinada.
3. **Moviment del cursor:** ↑/↓ i Inici/Fi es mouen per **línies visuals**, que és com s'espera quan s'escriu prosa.
4. **Sagnat penjant:** les línies de continuació d'un element de llista (`- `, `1. `) o d'una cita (`> `) s'alineen amb el text, no amb el marcador.
5. **Columna centrada** (opcional, `centrar = si`): si el terminal és més ample que `columnes`, el text es dibuixa centrat, en mode escriptura.
6. **Cache:** les posicions de tall es guarden a cada `Line` i només es recalculen quan s'edita la línia o canvien les columnes o l'amplada del terminal.

Interoperabilitat amb fitxers tallats a mà (fases posteriors):

- Ordre `desfer-salts`: ajunta les línies de cada paràgraf d'un fitxer amb salts durs, respectant l'estructura Markdown.
- Ordre `reformatar` (opcional): genera una còpia amb salts durs a N columnes, per si cal enviar el text per correu o a un repositori amb aquesta convenció.

**Estat de la decisió:** ✅ Confirmada (2026-10-25): **wordwrap suau a 72 columnes**, amb tots els detalls proposats.

## Dreceres de teclat

### Limitacions del terminal (condicionen tot el disseny)

| Tecla | Problema | Solució |
|-------|----------|---------|
| Ctrl+S / Ctrl+Q | Control de flux XON/XOFF: congelen el terminal | Desactivar `IXON` en mode *raw* → queden lliures |
| Ctrl+C / Ctrl+Z | Envien SIGINT / SIGTSTP | `raw()` de ncurses les captura com a tecles normals |
| Ctrl+I, Ctrl+M, Ctrl+[ | Són idèntiques a Tab, Enter i Esc | **No es poden fer servir** |
| Ctrl+H | És Retrocés en alguns terminals | Només com a extra (esborrar paraula), mai per a res essencial |
| Ctrl+Maj+lletra | Indistingible de Ctrl+lletra | No es fa servir |
| Ctrl/Maj + fletxes, Ctrl+Inici/Fi | Cada terminal envia una seqüència diferent. **A la consola de Linux** (RPi sense escriptori) normalment no arriben | Es poden fer servir, però sempre amb una alternativa |
| Alt+lletra | Arriba com Esc + lletra | Funciona a tot arreu; `ESCDELAY` = 25 ms perquè Esc sol respongui ràpid |
| F1–F12 | Funcionen a tot arreu | Bones per a funcions secundàries |

**Norma de disseny:** tota ordre essencial ha de tenir una drecera amb **Ctrl+lletra, Alt+lletra o tecla F**. Les combinacions amb fletxes són sempre una comoditat addicional.

### Estils considerats

- **nano** (Ctrl+O desar, Ctrl+W cercar, Ctrl+K tallar): conegut a Linux, però poc intuïtiu per a qui ve dels processadors de text.
- **CUA** (Ctrl+S/Z/X/C/V/F, com als editors gràfics): el que coneix tothom que escriu. Encaixa amb el públic de Phileas.
- **emacs / vim modal:** pensats per programar; corba d'aprenentatge alta. Descartats.

### Proposta: **estil CUA** adaptat al terminal + paleta d'ordres

**Fitxer**

| Drecera | Ordre |
|---------|-------|
| Ctrl+S | `desar` |
| Alt+S | `desar-com` |
| Ctrl+O | `obrir` (navegador de fitxers) |
| Ctrl+N | `nou` |
| Ctrl+Q | `sortir` (demana confirmació si hi ha canvis) |

**Edició i selecció**

| Drecera | Ordre |
|---------|-------|
| Ctrl+Z / Ctrl+Y | `desfer` / `refer` |
| Ctrl+X / Ctrl+C / Ctrl+V | `tallar` / `copiar` / `enganxar` |
| Ctrl+A | `seleccionar-tot` |
| Ctrl+Espai | `marca` (comença/acaba la selecció; alternativa universal) |
| Maj+fletxes | ampliar selecció (on el terminal ho permeti) |
| Ctrl+W (i Ctrl+Retrocés on arribi) | `esborrar-paraula-anterior` |
| Alt+D | `esborrar-paraula-seguent` |

**Cerca**

| Drecera | Ordre |
|---------|-------|
| Ctrl+F | `cercar` |
| Ctrl+G / F3 | `cercar-seguent` |
| Alt+G / Maj+F3 | `cercar-anterior` |
| Ctrl+R | `substituir` (Ctrl+H no és fiable) |

**Navegació** (fletxes, Inici/Fi i RePàg/AvPàg per línies visuals)

| Drecera | Alternativa universal | Ordre |
|---------|----------------------|-------|
| Ctrl+← / → | Alt+B / Alt+F | `paraula-anterior` / `paraula-seguent` |
| Ctrl+↑ / ↓ | Alt+P / Alt+N | `paragraf-anterior` / `paragraf-seguent` |
| Ctrl+Inici / Fi | Alt+< / Alt+> | `inici-document` / `fi-document` |
| — | Alt+L | `anar-a-linia` |

**Markdown**

| Drecera | Ordre |
|---------|-------|
| Ctrl+B | `negreta` (envolta la selecció o la paraula amb `**`) |
| Alt+I | `cursiva` (`*`; Ctrl+I no és possible) |
| Alt+1 … Alt+6 | `capcalera-1` … `capcalera-6` (Alt+0 la treu) |

**Ortografia, vista i ajuda**

| Drecera | Ordre |
|---------|-------|
| F7 | `revisar-ortografia` (salta d'error en error) |
| Alt+Enter | `suggeriments` (per a la paraula sota el cursor) |
| Alt+C | `centrar` (activar o desactivar la columna centrada) |
| Ctrl+L | `redibuixar` (útil per SSH) |
| F2 | mostrar/amagar la barra d'ajuda inferior (estil nano) |
| F1 | `ajuda` |
| **Ctrl+P** | **`paleta`**: escriure el nom de qualsevol ordre (amb autocompleció) |
| Esc | cancel·lar el diàleg o la selecció |

Lliures per al futur: Ctrl+D, Ctrl+E, Ctrl+J, Ctrl+K, Ctrl+T, Ctrl+U, F4–F6, F8–F12 i la majoria d'Alt+lletra.

**Paleta d'ordres:** és la conseqüència directa del registre d'ordres. Qualsevol ordre és accessible pel nom encara que no tingui drecera. Així el joc de tecles es pot mantenir petit.

**Personalització:** totes les dreceres es poden reassignar al fitxer de configuració (`tecla.ctrl-k = tallar`). Més endavant es podria oferir un perfil `nano` predefinit.

**Estat de la decisió:** ⏳ pendent de confirmar.

## Arquitectura preliminar

```
src/
  main.c        arrencada, arguments, bucle principal
  buffer.c      emmagatzematge del text (vector de línies, proposat)
  command.c     registre d'ordres amb nom (les dreceres apunten a noms)
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

- Format del fitxer de configuració.

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
- **2026-10-25** — ✅ Confirmat: C sol + configuració declarativa + registre d'ordres. Anàlisi de l'estructura del buffer; proposta: vector de línies (línia = paràgraf).
- **2026-10-25** — ✅ Confirmat: vector de línies. Anàlisi del wordwrap; proposta: suau a 72 columnes, amb moviment per línies visuals i sagnat penjant.
- **2026-10-25** — ✅ Confirmat: wordwrap suau a 72 columnes. Proposta de dreceres: estil CUA adaptat al terminal, amb alternatives Alt per a la consola de Linux i una paleta d'ordres (Ctrl+P).
