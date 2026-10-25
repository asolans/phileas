# Phileas — Document de seguiment

Editor de text per a consola (Linux), orientat a escriure prosa en Markdown.

## Estat

- **Fase actual:** 4 — Colors mínims de Markdown (F3 completada)
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

**Estat de la decisió:** ✅ Confirmada (2026-10-25).

## Model de paràgrafs (✅ confirmat i implementat a la F2b)

**Petició:** un Enter separa paràgrafs. A la pantalla, la separació es veu com una fila en blanc que no existeix al buffer. Al fitxer Markdown hi ha la separació normal de paràgrafs: una línia en blanc real.

**Model:** cada línia del buffer és un bloc (paràgraf, capçalera, element de llista…).
- **Desar:** entre dos blocs s'escriu `\n\n` (separació de paràgraf). Entre dos blocs *units* s'escriu `\n`, sense línia en blanc.
- **Carregar:** cada línia en blanc entre blocs es converteix en una separació i desapareix del buffer.
- **Pantalla:** una fila buida visual després de cada bloc no unit.

**Blocs units** (un sol `\n` al fitxer, sense fila visual entre ells):
- elements consecutius d'una llista;
- línies consecutives d'una cita (`>`);
- files d'una taula (`|`);
- línies dins d'un bloc de codi (` ``` `) i d'una capçalera de metadades YAML (`---` al principi);
- una línia acabada en salt dur (dos espais o `\` al final).

**Fitxers amb paràgrafs tallats a mà:** dues línies de text seguides que no formen un bloc unit són un sol paràgraf en Markdown. En carregar s'ajunten amb un espai. El significat del document no canvia, però el fitxer desat ja no tindrà els talls.

**Línies buides al buffer:** si es prem Enter dues vegades, queda una línia buida. Es desa com una línia en blanc addicional i es torna a carregar igual. Funciona en tots dos sentits: el fitxer amb dues línies en blanc torna a tenir una línia buida al buffer.

**Detalls afegits en implementar-ho:**
- **Blocs units separats per una línia en blanc** (dues taules, dues llistes): la línia en blanc es conserva com a línia buida, perquè no s'ajuntin en desar.
- **Llistes niades** amb 4 espais o més: continuen sent llistes, no codi sagnat. Un paràgraf sagnat dins d'un element de llista hi va unit.
- **Línies buides del final del document:** no es desen, ja que en carregar també es descarten.
- **Text enganxat des del terminal:** segueix les mateixes regles que en carregar un fitxer, de manera que una línia en blanc entre paràgrafs és una separació, no una línia buida. No ajunta línies, i dins de blocs de codi ho conserva tot.
- **Normalització coneguda:** un paràgraf seguit directament d'una llista (sense línia en blanc) rep una línia en blanc entre tots dos. Per a la resta de casos provats (taules, codi, metadades, cites, llistes niades, salts durs, HTML), el fitxer queda idèntic byte a byte.

**Estimació de pàgines:** cada separació compta mitja línia.

## Autodesat (✅ implementat a la F3)

- **Comportament:** desa sobre el mateix document, sense còpies de seguretat. Fa servir el mateix desat atòmic que Ctrl+S.
- **Quan desa:** quan hi ha canvis sense desar i fa *N* segons que no es prem cap tecla. Si s'escriu sense parar, desa com a molt cada *M* segons, perquè no es perdi feina en una sessió llarga.
- **Configuració:**
  - opcions `autodesat = 30` (segons d'inactivitat; `0` = desactivat) i `autodesat-maxim = 300`;
  - fins que hi hagi el fitxer de configuració (F7), l'opció de línia d'ordres `-a SEGONS`;
  - ordre `autodesat` a la paleta, per activar-lo o desactivar-lo durant la sessió.
- **Documents sense nom:** no es desen sols. La barra d'estat avisa que cal desar-los primer amb un nom.
- **Avís:** la barra mostra breument «desat automàticament hh:mm».
- **Implementació:** la lectura de tecles espera amb un temps límit (`timeout()` de ncurses) en lloc de bloquejar-se. No cal cap fil ni consum en repòs.
- **Per què a la F3:** com que l'autodesat sobreescriu l'original, ha d'anar amb el desfer. Si es desa sol un canvi no volgut, s'ha de poder recuperar amb Ctrl+Z.

## Colors mínims de Markdown (planificat a la F4)

Només atributs del terminal, que funcionen també a la consola de Linux i en terminals de 8 colors:

| Element | Aspecte |
|---------|---------|
| Capçaleres `#` | Negreta + color (un per nivell, com a molt 3 de diferents) |
| `**negreta**` | Negreta |
| `*cursiva*` | Cursiva si el terminal en té; si no, subratllat |
| `` `codi` `` i blocs ` ``` ` | Color atenuat |
| Marcadors (`#`, `**`, `-`, `>`) | Atenuats, per destacar el text |
| Cites `>` | Color suau |
| Enllaços `[text](url)` | Text subratllat, URL atenuada |

L'estat de bloc (dins de codi, de metadades…) es calcula una sola vegada per línia i es guarda a la cache. És el mateix que necessita el model de paràgrafs per saber quins blocs van units.

## Arquitectura

```
src/
  main.c        ✅ arrencada, arguments, bucle principal
  util.c        ✅ assignació de memòria i errors fatals
  utf8.c        ✅ descodificació UTF-8, amplada de caràcters, diacrítics combinats
  buffer.c      ✅ vector de línies; inserir/esborrar/extreure; càrrega i desat atòmic
  md.c          ✅ prefixos de bloc Markdown i tipus de bloc de cada línia (model de paràgrafs)
  wrap.c        ✅ wordwrap suau amb sagnat penjant i cache per línia
  editor.c      ✅ estat de l'editor, cursor, moviment, edició, selecció i porta-retalls
  command.c     ✅ registre d'ordres amb nom i taula de dreceres
  keys.c        ✅ lectura de tecles, noms de tecla ("ctrl-s", "alt-<"...) i enganxat delimitat
  render.c      ✅ dibuix amb ncursesw, barra d'estat i diàlegs a la línia inferior
  search.c      ✅ cerca (sense distingir majúscules si el patró no en té), endavant i enrere
  undo.c        ✅ historial desfer/refer per grups, amb límit de memòria (8 MB)
  autosave.c    ✅ autodesat per inactivitat i interval màxim
  spell.c       integració amb hunspell
  filebrowser.c navegació pel disc i obertura de fitxers
  config.c      configuració (~/.config/phileas/)
tests/
  test.c        ✅ proves de utf8, wrap, buffer, edició i md (sense ncurses)
```

Decisions de disseny pendents:

- Format del fitxer de configuració.

## Compilació i ús

```
make            # compila ./phileas
make test       # proves unitàries
make clean && make debug   # amb AddressSanitizer i UBSan
./phileas [-c COLUMNES] [-a SEGONS] [fitxer.md]   # -a 0 desactiva l'autodesat
```

## Entorn de desenvolupament

- gcc 14.2, ncurses 6.5 (`libncurses-dev` ✅ instal·lat)
- hunspell 1.7.2: biblioteca ✅, **`libhunspell-dev` ❌ cal instal·lar-lo** (`sudo apt install libhunspell-dev`)
- Diccionaris disponibles a `/usr/share/hunspell/`: ca, ca_ES, ca_ES-valencia, es, en_US

## Full de ruta (provisional)

- [x] **F0** — Decisions d'arquitectura: llenguatge, buffer, wordwrap, dreceres
- [x] **F1** — Esquelet: obrir, mostrar, desplaçar-se, desar un fitxer UTF-8 (inclou el wordwrap suau, necessari per mostrar els paràgrafs)
- [x] **F2** — Edició bàsica, sagnat penjant de llistes i cites, columna centrada
- [x] **F2b** — Separació visual de paràgrafs (vegeu «Model de paràgrafs»)
- [x] **F3** — Desfer/refer, cerca i substitució, autodesat
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
- **2026-10-25** — ✅ Confirmades les dreceres. **F0 tancada.**
- **2026-10-25** — **F1 completada.**
  - **Funcionalitat:**
    - obrir un fitxer (o començar-ne un de nou);
    - mostrar-lo amb wordwrap suau;
    - moure's per caràcters, línies visuals, paraules, paràgrafs, pàgines i document;
    - anar a una línia;
    - desar i desar com;
    - sortir;
    - paleta d'ordres bàsica (sense autocompleció).
  - **Tecles:** les Ctrl+fletxes s'activen només si el terminal les té; si no, queden les alternatives amb Alt.
  - **Desat:**
    - és atòmic (fitxer temporal + `rename`);
    - conserva els permisos, els enllaços simbòlics, els salts CRLF, la BOM, el salt final i els bytes no UTF-8.
  - **Proves:**
    - 1.230 comprovacions unitàries, també amb sanititzadors;
    - proves interactives en un pseudoterminal (TERM=xterm i TERM=linux), incloent-hi el canvi de mida;
    - document de 2,7 MB (40.000 línies): navegació fluida, ~9 MB de RSS.
  - **Pendent de verificar:** l'execució en una Raspberry Pi real.
- **2026-10-25** — Prova de l'usuari: el wordwrap i el moviment del cursor són correctes. La prova en una Raspberry Pi queda pendent.
- **2026-10-25** — Barra d'estat: estimació orientativa de pàgines (`≈N pàg.`).
  - **Model:** A4 amb marges de 2,5 cm, font de 12 punts amb interlineat senzill, 80 caràcters per línia i 46 línies per pàgina (~600 paraules).
  - **Compte:** les línies en blanc compten mitja línia (l'espai entre paràgrafs) i les capçaleres, una línia més.
  - **Rendiment:** el resultat es guarda per línia, de manera que només es recompta el que canvia.
  - **Correcció:** els noms de fitxer llargs es retallen i ja no se superposen a la part dreta de la barra.
- **2026-10-25** — **F2 completada.**
  - **Edició:**
    - escriure i Enter;
    - Retrocés i Supr, que uneixen línies als extrems;
    - esborrar paraules amb Ctrl+W, Alt+D i Ctrl+Supr;
    - Tab insereix un tabulador.
  - **Markdown:**
    - Enter continua llistes (`- `, `* `, `1. ` → `2. `), cites (`> `), tasques (`- [ ] `) i el sagnat;
    - Enter sobre un element buit acaba la llista.
  - **Selecció:**
    - Maj+fletxes, que es cancel·la amb qualsevol moviment sense Maj;
    - Ctrl+Espai (marca), que s'amplia amb qualsevol moviment i funciona també a la consola de Linux;
    - Ctrl+A per seleccionar-ho tot i Esc per cancel·lar;
    - escriure amb una selecció activa la substitueix.
  - **Porta-retalls intern:** Ctrl+X, Ctrl+C i Ctrl+V. No comparteix el porta-retalls del sistema.
  - **Enganxat del terminal:** amb el mode delimitat («bracketed paste»), el text enganxat amb el ratolí o amb Ctrl+Maj+V s'insereix tal qual i d'una sola vegada, sense continuació automàtica de llistes.
  - **Pantalla:**
    - sagnat penjant: les files de continuació de llistes i cites s'alineen amb el text;
    - columna centrada (Alt+C), quan el terminal és més ample que la columna de text.
  - **Sortir:** Ctrl+Q avisa si hi ha canvis sense desar; una segona pulsació surt.
  - **Limitació coneguda:** en terminals que envien ^H com a Retrocés, Ctrl+Retrocés fa de Retrocés normal. Ctrl+W és l'alternativa fiable.
  - **Proves:**
    - 1.281 comprovacions unitàries, també amb sanititzadors;
    - proves interactives de totes les operacions;
    - escriure al mig d'un document de 2,7 MB: ~60 ms de CPU per a 210 pulsacions, càrrega inclosa.
- **2026-10-25** — Prova de l'usuari de la F2: l'edició és correcta.
  - **Centrat:** el terminal de l'aplicació fa 54 columnes, menys que la columna de text (72), i no hi ha marge per centrar. Ara el missatge ho explica en lloc de dir «Columna centrada».
  - **Peticions:**
    - separació de paràgrafs estrictament visual a la pantalla, amb separació normal al fitxer Markdown: model proposat a «Model de paràgrafs», pendent de confirmar;
    - autodesat configurable sobre el mateix document: planificat a la F3.
- **2026-10-25** — ✅ Model de paràgrafs confirmat. **F2b completada.**
  - **Pantalla:** una fila buida visual després de cada paràgraf; el cursor no s'hi pot posar.
  - **Fitxer:** la separació normal de Markdown (una línia en blanc).
  - **Classificació:** cada línia té un tipus de bloc que es manté al dia amb un escaneig incremental. S'atura quan l'estat de bloc de codi i de metadades torna a coincidir.
  - **Prova amb el mateix SEGUIMENT.md:** en desar-lo, només afegeix les dues línies en blanc de la normalització paràgraf → llista.
  - **Proves:** 1.439 comprovacions unitàries, també amb sanititzadors, i proves interactives de pantalla, moviment, desplaçament en el document gran, Retrocés entre paràgrafs i enganxat.
- **2026-10-25** — Prova de l'usuari de la F2b. Ajustos:
  - **En obrir un fitxer:** el cursor es col·loca al final del document.
  - **Copiar:** el cursor queda al final del bloc copiat, encara que la selecció s'hagi fet cap enrere.
  - **Enter al final del document:** la fila de separació es mostra de seguida. El cursor espera a l'inici del paràgraf nou i no es mou en escriure la primera lletra. També passa en acabar una llista.
- **2026-10-25** — **F3 completada.**
  - **Desfer/refer** (Ctrl+Z / Ctrl+Y):
    - totes les modificacions passen per `buf_insert`/`buf_delete` i queden a l'historial, agrupades per acció;
    - escrivint, cada paraula és un pas; els Retrocés o Supr seguits, també;
    - una substitució de tot el document es desfà d'un sol cop;
    - el desfer recol·loca el cursor;
    - si es desfà fins a l'estat desat, el document torna a constar com a «sense canvis»;
    - l'historial té un límit de 8 MB, i en passar-lo s'obliden els grups més antics.
  - **Cerca** (Ctrl+F):
    - la coincidència se selecciona mentre s'escriu, i la pregunta indica «(no trobat)»;
    - Esc torna on era el cursor;
    - l'última cerca es mostra entre claudàtors, i Enter sense escriure res la reutilitza;
    - Ctrl+G / F3 busquen la següent i Alt+G / Maj+F3 l'anterior, fent la volta al document amb un avís;
    - si el text cercat no té majúscules, no les distingeix.
  - **Substitució** (Ctrl+R): demana el text i el substitut, i per a cada coincidència pregunta (s)í, (n)o, (t)ots o Esc. Acaba en tornar al punt de partida.
  - **Autodesat:**
    - per defecte, 30 s sense escriure (`-a SEGONS`, i `-a 0` el desactiva), i com a molt cada 300 s si s'escriu sense parar;
    - desa sobre el mateix fitxer amb el desat atòmic;
    - l'ordre `autodesat` de la paleta l'activa o el desactiva;
    - els documents sense nom no es desen sols, i la barra avisa una vegada que cal desar-los amb un nom;
    - l'espera es fa amb el temps límit de ncurses: no consumeix CPU en repòs.
  - **Proves:** 1.467 comprovacions unitàries, també amb sanititzadors, i proves interactives de totes les funcions.
- **2026-10-25** — Prova de l'usuari de la F3: tot correcte. La barra d'estat mostra les pàgines sense el signe ≈ (`N pàg.`).
