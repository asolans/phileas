# Phileas — Document de seguiment

Editor de text per a consola (Linux), orientat a escriure prosa en Markdown.

## Estat

- **Fase actual:** 8 — Integració al projecte BYOK (F0–F7 completades). Proposta de la F9 (Pi i escriptori), pendent de confirmar
- **Última actualització:** 2026-10-30

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
   **Marges** (2026-10-26): `marge-esquerre = 4` i `marge-dret = 4` per defecte (0–100; eren 2 i 2 fins al 2026-10-26, a la tarda). El text comença a la columna `marge-esquerre` i, si el terminal és més estret que `columnes` + marges, l'amplada es redueix perquè quedin `marge-dret` columnes buides a la dreta. Amb `centrar = si`, es centra entre els marges. Si amb els marges no hi caben 20 columnes, s'ignoren. La barra d'estat ocupa tota l'amplada.
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
| F7 | `ortografia` (activa o desactiva la correcció; la barra d'estat mostra `ca` o `es`) |
| F8 | `alternar-idioma` (rotació de diccionaris: català ↔ castellà) |
| Maj+F7 | `revisar-ortografia` (salta d'error en error) |
| Alt+Enter | `suggeriments` (per a la paraula sota el cursor) |
| Alt+C | `centrar` (activar o desactivar la columna centrada) |
| Ctrl+L | `redibuixar` (útil per SSH) |
| F2 | mostrar/amagar la barra d'ajuda inferior (estil nano) |
| F1 | `ajuda` (✅ ordres per temes, amb les tecles actuals; escriure filtra) |
| **Ctrl+P** | **`paleta`**: escriure el nom de qualsevol ordre (amb autocompleció) |
| Esc | ✅ `menu` (2026-10-27): menús i submenús a l'estil WordGrinder; si hi ha selecció, primer la cancel·la. Als diàlegs, continua cancel·lant |

Lliures per al futur: Ctrl+D, Ctrl+E, Ctrl+J, Ctrl+K, Ctrl+T, Ctrl+U, F4–F6, F9–F12 i la majoria d'Alt+lletra.

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

## Colors mínims de Markdown (✅ implementat a la F4)

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

Colors triats (provisionals, configurables a la F7):

| Element | xterm i similars | Consola de Linux |
|---------|------------------|------------------|
| Capçalera nivell 1 / 2 / 3–6 | Negreta magenta / cian / verd | Igual |
| Marcadors | Atenuats | Atenuats, sense color |
| `*cursiva*` | Cursiva | Subratllat |
| Codi | Groc | Groc |
| Cites | Verd | Verd (el text en cursiva o atenuat, sense color) |

La consola de Linux no combina el subratllat ni l'atenuat amb colors (capacitat `ncv` del terminfo); en aquests casos es manté l'atribut i es treu el color. L'ordre `colors` de la paleta els activa o els desactiva.

## Mode màquina d'escriure (✅ confirmat i implementat a la F6c)

### Context

- **La pantalla de l'usuari:** a la Pi, en vertical, fa 1920×1200 a 16 bits, amb una consola de 60 files × 75 columnes (lletra de 16×32), de les quals 58 són de text.
- **Comportament actual:** el cursor pot baixar fins a l'última fila, i a partir d'aquí cada fila nova desplaça el text una línia. S'escriu sempre a baix de tot.
- **Referent:** WordGrinder. S'escriu baixant per la pantalla. Quan el cursor arriba gairebé a baix, el text puja d'un salt i el cursor queda una mica per sota de la meitat.

### Opcions

| | A. Salt (WordGrinder) | B. Línia fixa (typewriter pur) | C. Clàssic (l'actual) |
|---|---|---|---|
| On s'escriu | Entre la meitat i el 85 % de l'alçada | Sempre a la mateixa fila | A baix de tot |
| Moviment del text | Un salt cada ~17 files (amb 58 files) | Una línia a cada fila nova | Una línia a cada fila nova |
| Context visible a sobre | Entre un 55 % i un 85 % de la pantalla | Fix, p. ex. un 55 % | Tota la pantalla |
| Cost a la Pi | Un redibuix sencer de tant en tant | Redibuix sencer a cada fila: la consola girada no té acceleració | Igual que B |
| Distracció | Mínima: el text es queda quiet mentre s'escriu | El text es mou contínuament | El text es mou i s'escriu al límit |

### Proposta: **un sol mecanisme amb dos paràmetres, i el salt (A) per defecte**

- **Límit** (per defecte, el 80 % de l'alçada; inicialment el 85 %): si el cursor el passa en baixar, el text es desplaça.
- **Ancoratge** (per defecte, el 50 %; inicialment el 55 %): la fila on queda el cursor després del desplaçament.

Amb aquests dos valors surten els tres modes:

| Mode | Límit | Ancoratge |
|------|-------|-----------|
| `salt` (per defecte) | 80 % | 50 % |
| `fixa` | 50 % | 50 % (el cursor no es mou de la fila) |
| `classic` | 100 % | 100 % (el comportament actual) |

**Detalls:**

- **Final del document:** es pot desplaçar més enllà de l'última línia, de manera que en escriure al final el text no arriba mai a baix: per sota del cursor hi ha espai en blanc.
- **Pujar amb el cursor:** quan surt per dalt, es col·loca a l'ancoratge, i així també es veu el context de sobre. A l'inici del document no hi ha espai en blanc per dalt: el cursor queda on toqui.
- **Salts llargs** (obrir un document, Ctrl+Fi, cerca, anar a línia, desfer lluny): el cursor queda a l'ancoratge, i no a baix de tot com ara.
- **RePàg/AvPàg:** el cursor manté la seva fila de pantalla.
- **Canvi de mida o gir de pantalla:** els percentatges es tornen a aplicar a la nova alçada.
- **Ordre `maquina-escriure`** a la paleta: passa per `salt` → `fixa` → `classic`. Els percentatges, i el mode per defecte, aniran al fitxer de configuració (F7).
- **Implementació:** tot és a `ed_layout()`, que ja decideix la fila de dalt. Cal calcular la fila de pantalla del cursor (com a molt, recórrer l'alçada de la pantalla) i aplicar-hi les dues regles. No afecta el buffer, el wordwrap ni el model de paràgrafs.

**Per què el salt per defecte:**

- és el que demana l'usuari i el que fa WordGrinder;
- el text es queda quiet mentre s'escriu;
- a la Pi, amb la consola girada (sense acceleració), redibuixar la pantalla sencera a cada fila nova probablement es notaria. El salt només ho fa una vegada cada ~17 files. Aquest punt s'ha de mesurar a la consola real; si la línia fixa resultés fluida, també hi és com a mode.

**Estat de la decisió:** ✅ Confirmada (2026-10-25).

## Configuració (✅ confirmada i implementada a la F7)

Ja decidit a la F0: C sol, amb un fitxer declaratiu (`clau = valor`) i les dreceres apuntant a ordres amb nom.

### On és el fitxer

Per ordre de prioritat (el primer que mana):

1. **Línia d'ordres:** `-c`, `-a`, `-d`… manen sobre el fitxer.
2. **Fitxer de l'usuari:** `$PHILEAS_CONFIG/phileasrc` si existeix aquesta variable; si no, `$XDG_CONFIG_HOME/phileas/phileasrc`; si no, `~/.config/phileas/phileasrc`.
3. **Fitxer del sistema:** `/etc/phileas/phileasrc`, amb els valors per defecte d'una instal·lació (p. ex. la imatge BYOK).
4. **Valors interns:** els d'ara.

**Per què `PHILEAS_CONFIG`:** a la Pi, `/root` és de només lectura i només es pot escriure a `/data` (a la imatge, micro ja fa servir `MICRO_CONFIG_HOME=/data/micro`). Amb `PHILEAS_CONFIG=/data/phileas`, la configuració i els diccionaris personals hi van a parar. Ara mateix, a la Pi, afegir una paraula al diccionari personal falla per aquest motiu.

### Format

```
# Phileas: configuració. Les línies que comencen per # són comentaris.
# Els valors de sí/no admeten: si, sí, no.

# Text
columnes = 72
centrar = no
colors = si

# Màquina d'escriure: salt, fixa o classic
maquina-escriure = salt
maquina-escriure-limit = 80        # % de l'alçada
maquina-escriure-ancoratge = 50    # % de l'alçada

# Desat
autodesat = 30                     # segons sense escriure; 0 = desactivat
autodesat-maxim = 300

# Ortografia
ortografia = no                    # activa en arrencar?
idiomes = ca es                    # rotació de F8; el primer és el de l'arrencada

# Colors: negre, vermell, verd, groc, blau, magenta, cian, blanc o cap
color-capcalera-1 = magenta
color-capcalera-2 = cian
color-capcalera-3 = verd
color-codi = groc
color-cita = verd
color-error = vermell

# Dreceres: tecla.NOM = ordre (buit = sense drecera)
tecla.ctrl-t = maquina-escriure
tecla.f9 = colors
tecla.ctrl-h =
```

- **Errors:** una línia incorrecta no impedeix arrencar. La línia s'ignora i la barra d'estat avisa: «phileasrc, línia 12: opció desconeguda «colums»». Si n'hi ha més d'una, avisa de la primera i en diu el total.
- **Noms de tecla:** els que ja fa servir el registre (`ctrl-s`, `alt-<`, `f7`, `shift-f7`, `ctrl-shift-left`…).
- **Ordres noves:**
  - `recarregar-configuracio` (paleta): torna a llegir el fitxer sense sortir de Phileas;
  - `phileas --config` escriu a la sortida estàndard un fitxer complet amb tots els valors per defecte comentats, per fer-ne una còpia i retocar-la.

### Un problema que cal resoldre abans: els fitxers que no són Markdown

En carregar un fitxer, Phileas ajunta les línies seguides d'un paràgraf (text tallat a mà) i, en desar-lo, hi posa una línia en blanc entre blocs. Per a un `.md` és el comportament desitjat. Per a un fitxer de configuració (`columnes = 72` i `centrar = no` en dues línies seguides), en canvi, el desfaria: quedaria `columnes = 72 centrar = no`.

**Proposta:** els fitxers que no acaben en `.md`, `.markdown` o `.txt` s'obren en **mode text pla**:
- cada línia del fitxer és una línia, i no s'ajunten;
- no hi ha separació visual de paràgrafs;
- no hi ha colors de Markdown ni continuació de llistes;
- la barra d'estat diu `text`.

Així el fitxer de configuració (i qualsevol altre) es pot editar amb el mateix Phileas sense fer-lo malbé. És una mica més de feina, però avui obrir un fitxer qualsevol el pot alterar en desar-lo.

**Estat de la decisió:** ✅ Confirmada (2026-10-25).

## Phileas a la Pi i a l'escriptori (proposta, pendent de confirmar)

Objectiu (2026-10-26): millorar el cor de l'aplicació i tenir-la disponible tant a la BYOK (Raspberry Pi 1B, consola de Linux) com en ordinadors Linux x86_64 (terminals d'escriptori).

### Principi: un sol programa

- **Una sola línia de desenvolupament** (`master`), sense cap branca «versió PC». Cada millora del cor arriba als dos llocs alhora.
- **Les diferències van fora del codi:**
  - **Pi:** `br2-phileas`, amb el seu `/etc/phileas/phileasrc` (ordres `lletra`, `gira`, `exporta`, `envia`);
  - **PC:** l'empaquetat (vegeu D5) i un `phileasrc` pensat per a l'escriptori.
- **El que depèn de l'entorn es detecta en temps d'execució**, com ja es fa amb hunspell (`dlopen`): si una eina hi és, es fa servir; si no, Phileas funciona igual sense. No hi ha opcions de compilació per plataforma.
- **Branques curtes** (`git switch -c NOM`) només per a canvis grossos del cor que ocupin diverses sessions; es fusionen a `master` quan funcionen. Els canvis petits van directament a `master`.
- **Una sola numeració:** les etiquetes `vX.Y` de Phileas serveixen per als dos llocs. `br2-phileas` en tria una per a cada imatge; el paquet de PC es genera de la mateixa etiqueta.

### Proves de cada versió

1. `make test` i `make debug` (proves unitàries, sense ncurses: no depenen de la plataforma).
2. Prova interactiva al PC en els terminals de D1.
3. Abans d'etiquetar: `make O=../output-phileas phileas-rebuild all` i prova a la Pi.

### Requisits de l'escriptori

| # | Requisit | Àmbit | Estat |
|---|----------|-------|-------|
| D1 | **Tecles als terminals d'escriptori.** Provar GNOME Terminal, Konsole, xterm i, si cal, kitty/foot. Combinacions sensibles: Ctrl+Maj+fletxes, Alt+lletra, Maj+F7, Ctrl+Espai, F1 i F10 (alguns terminals les capturen). Possibles ajustos a `keys.c` i una taula a l'ajuda de les tecles que no arriben. | PC | En curs: GNOME Terminal ✅, xterm ✅ (2026-10-26) |
| D2 | **Desat en tancar.** En rebre SIGHUP (s'ha tancat el terminal o la connexió SSH) o SIGTERM (p. ex. l'apagada de la BYOK), Phileas desa el document si té canvis i surt (codi 128 + senyal). Si no té nom o no s'hi pot desar, el desa a `rescat-AAAAMMDD-HHMMSS.md` (o `.txt`) al directori actual (a la Pi, `/data/textos`); si no s'hi pot escriure, al de configuració i, si tampoc, a `$HOME`. Funciona també amb un diàleg obert. | Tots dos | ✅ Fet (2026-10-26) |
| D3 | **Porta-retalls del sistema.** Opcional i detectat: `wl-copy`/`wl-paste` (Wayland), `xclip` o `xsel` (X11), o OSC 52 (també per SSH). Copiar envia el text també al sistema; enganxar continua sent el de Phileas, i Maj+Ins (o l'enganxat delimitat del terminal, que ja funciona) porta el del sistema. Sense cap eina, com ara. **Fet el 2026-10-30, amb un canvi:** Ctrl+V porta el del sistema (l'usuari no podia enganxar a GNOME des d'altres aplicacions); si és el que Phileas mateix hi ha posat, o no n'hi ha, el de Phileas. `src/clipboard.c`; el text surt en format de fitxer (`buf_export`, línia en blanc entre paràgrafs) i entra com un enganxat del terminal (`buf_paste_text`). Límit d'1,5 s per a l'eina. OSC 52 no (GNOME Terminal no l'admet). | PC | ✅ Fet |
| D4 | **Colors.** Avui 8 colors bàsics, que el tema del terminal ja adapta. Proposta: mantenir-ho (és el que veu la consola de la Pi) i provar que es llegeix bé amb temes clars i foscos. | PC | Per provar |
| D5 | **Empaquetat.** Opcions: (a) `make install` des del codi, ja funciona; (b) paquet `.deb` amb dependència de `libncursesw6` i recomanació de `libhunspell-1.7-0`, `hunspell-ca` i `hunspell-es`; (c) binari estàtic amb musl, un sol fitxer per a qualsevol distribució; però un binari estàtic no pot carregar amb `dlopen` la `libhunspell` del sistema, de manera que caldria enllaçar-hi hunspell (uns 700 KB més) o quedar-se sense ortografia. Proposta: (b) per a Debian/Ubuntu i (a) per a la resta. **(c) fet el 2026-10-27:** `make HUNSPELL_STATIC=1` hi enllaça hunspell; l'executable estàtic musl el compila Buildroot (br2-phileas, `phileas_portable_x86_64_defconfig`) i `byok/scripts/phileas-portable.sh` en fa la carpeta del pendrive. | PC | (c) fet |
| D6 | **`phileasrc` d'escriptori.** Sense les ordres de BYOK. Possibles ordres externes d'exemple: `exporta` amb pandoc (`.docx`, `.odt`, `.pdf`) si hi és, i `paraules = wc -w`. | PC | Proposta |
| D7 | **Diccionaris.** Al PC es fan servir els del sistema (`/usr/share/hunspell`). Comprovar que `ca` i `es` es troben amb els noms dels paquets de Debian/Ubuntu i de Fedora/Arch, i que el missatge quan no n'hi ha cap explica què cal instal·lar. | PC | Per provar |

### Millores del cor (candidates, per prioritzar)

Llista oberta: l'usuari hi afegeix, en treu i n'ordena la prioritat.

| # | Millora | Àmbit | Notes |
|---|---------|-------|-------|
| C1 | Autocompleció a la paleta (Ctrl+P) | Tots dos | ✅ Fet (2026-10-26): Tab completa el nom de l'ordre; amb diverses opcions, les mostra atenuades. |
| C2 | Recordar la posició del cursor per fitxer | Tots dos | ✅ Fet (2026-10-29): fitxer `historial` del directori de configuració (`LÍNIA BYTE CAMÍ`), mai dins del document; opció `recordar-posicio`. |
| C3 | Fitxers recents | Tots dos | ✅ Fet (2026-10-29): ordre `recents`, Alt+O, Esc → Fitxer → Recents. Llista filtrable de l'`historial`; Supr en treu un. |
| C4 | Comptador de paraules | Tots dos | ✅ A la barra d'estat (2.0.2). Pendent: amb la selecció, les de la selecció. |
| C5 | Ordre `reformatar` | Tots dos | Ja prevista com a opcional a «Decisió de wordwrap»: còpia amb salts durs a N columnes. |
| C6 | Objectiu d'escriptura | Tots dos | Paraules per sessió o per document, amb el progrés a la barra d'estat. |
| C7 | Esquema | Tots dos | ✅ Fet (2026-10-29): ordre `esquema`, F2 o Alt+E, menú Cerca → Esquema. Diàleg amb les capçaleres `#` i les paraules de cada secció; Retorn hi salta. Una passada pel document en obrir-lo. Base de C8, C9, C22 i C24. |
| C8 | Moure seccions i paràgrafs | Tots dos | ✅ Fet (2026-10-29): `pujar-paragraf`/`baixar-paragraf`, Alt+↑/↓, menú Edita. Des de l'esquema, Alt+↑/↓ mou una secció sencera; al text, el paràgraf actual. Entra al desfer. |
| C9 | Projecte multifitxer | Tots dos | Carpeta amb `01-*.md`, `02-*.md`…: navegador de capítols, recompte total i ordre `compila` (concatena en un `.md` per a `exporta`/`envia`). Per a una 3.0. |
| C10 | Instantànies | Tots dos | En desar a mà, i com a molt cada hora, còpia a `.versions/nom-AAAAMMDD-HHMM.md`; ordre per recuperar-les, sense diff; poda (p. ex. 20 per document). Per a una 3.0. |
| C11 | Marcadors i pendents | Tots dos | `TODO`, `[?]`, `%%nota%%` ressaltats a les línies visibles; ordre `pendents` que els llista i hi salta. |
| C12 | Mode enfocament | Tots dos | ✅ Fet (2026-10-29): `enfocament`, F4, Vista. Atenua tot menys el paràgraf (o la frase) actual, amb un color del tema. Només afecta el pintat. |
| C13 | ~~Sprint amb temporitzador~~ | Tots dos | ❌ Abandonada (decisió de l'usuari, 2026-10-29). Era: `sprint 25`: compte enrere a la barra (el `timeout` de l'autodesat); en acabar, paraules escrites i ppm. Encaixa amb C6. |
| C14 | ~~Mode «endavant»~~ | Tots dos | ❌ Abandonada (decisió de l'usuari, 2026-10-29). Era: opcional durant l'sprint: sense retrocés ni moviment enrere. Filtre de tecles. |
| C15 | Diari d'escriptura | Tots dos | En sortir, `data;fitxer;paraules+` a un CSV del directori de configuració; ordre `estadistiques` amb ratxa de dies i mitjana. |
| C16 | Ecos i paraules crossa | Tots dos | Ordre `repeticions`: repeticions en un radi de N paraules dins el paràgraf i llista configurable («molt», «doncs», «muy», adverbis -ment/-mente). A demanda. |
| C17 | Tipografia de diàleg | Tots dos | ✅ Fet (2026-10-29): `tipografia`, Edita; `apostrof-tipografic`. Per idioma: `--` → «—», `<<`/`>>` → «», `...` → «…», apòstrof tipogràfic opcional; fora del codi Markdown. |
| C18 | Calaix d'idees | Tots dos | ✅ Implementat (2026-11-02, etiqueta v2.1.3, sense provar a la Pi): F9 afegeix una idea; `calaix-seleccio` envia la selecció amb la referència; `calaix` llista i filtra per etiqueta o text. Fitxer `calaix.md` (opció `calaix`); a la BYOK ha d'apuntar a `/data/textos`, que `envia` copia. Vegeu «Proposta: calaix d'idees». |
| C19 | Notes al peu | Tots dos | ✅ Fet (2026-10-30): `nota-peu` (F5) insereix `[^n]` amb el número següent i la definició al final, on queda el cursor; `anar-a-nota` (F6) va i torna; `renumerar-notes` numera en l'ordre de les crides i ordena les definicions als llocs on eren (les etiquetes amb nom es queden). Menú Edita. Sintaxi de pandoc; les del codi no compten. `src/footnote.c`. |
| C20 | Citacions | Tots dos | Llegeix un `.bib` o CSL-JSON (YAML o phileasrc); `cita` obre una llista filtrable i insereix `[@clau, p. 23]` amb la compleció de la paleta. La bibliografia la fa `pandoc --citeproc` a l'ordinador. |
| C21 | Capçalera YAML | Tots dos | `lang: ca`/`es` fixa l'idioma del corrector per document; `title`/`author` a la barra; plantilles (article, capítol, ressenya, acta). |
| C22 | Límits per secció | Tots dos | `<!-- max: 250 -->` sota una capçalera: «Resum 212/250» a la barra i avís en passar-se. Reaprofita C7. |
| C23 | Llegibilitat a demanda | Tots dos | Longitud mitjana de frase i de paràgraf; marca les frases de més de N paraules. Sense índexs anglesos (Flesch). |
| C24 | Referències internes | Tots dos | Autocompleta `[text](#ancora)` amb les capçaleres del document o del projecte. Reaprofita C7. |
| C25 | Sinònims | Tots dos | Tesaurus de LibreOffice (`th_ca_ES_v3`, `th_es_ES`, `.dat` + `.idx`) amb cerca binària a l'índex, sense carregar-lo; es mostra com els suggeriments. Uns 5–10 MB a `/data`. Detectat: sense tesaurus, no hi és. |
| C26 | Abreviatures | Tots dos | `abrev.qvq = que vol dir que` al phileasrc; s'expandeix en escriure l'espai. |

**Criteris de C7–C26** (proposta del 2026-10-29, pensada per a la Pi 1B): l'anàlisi corre a demanda i no a cada tecla; el que es pinta només es calcula per a les línies visibles; cap dependència nova obligatòria. Descartat per pesat: corrector gramatical (LanguageTool, Java), previsualització renderitzada, plegament de seccions a la pantalla i diff entre versions.

**Prioritat (decidida per l'usuari el 2026-10-29):** (1) C7 esquema; (2) C8 moure seccions i paràgrafs; (3) C12 mode enfocament; (4) C17 tipografia de diàleg. La resta, a valorar.

**C17 i el Markdown.** Escriure «—», «», «…» o «’» deixa al fitxer el caràcter Unicode, que per a Markdown és text normal: el fitxer continua sent Markdown estàndard i, de fet, es veu igual a tot arreu, mentre que `--` només es converteix amb extensions com `smart` de pandoc (CommonMark el deixa com a `--`). A més, «—» a l'inici de paràgraf no és marca de llista, i «- » sí. Condicions de la implementació: (a) no substituir dins de codi (en línia i blocs), a les línies fetes només de guions (`---`: separador, subratllat de capçalera, YAML) ni a les taules; (b) Ctrl+Z just després torna a deixar el `--`; (c) opció al phileasrc, per idioma, desactivada per defecte; (d) cercar `'` ha de trobar `’` (i a l'inrevés), i cal comprovar que el corrector accepta `l’home` amb els diccionaris ca i es.

### Estat de la decisió

Pendent de confirmar: el principi d'un sol programa, els requisits D1–D7 i l'ordre de les millores C2–C26 (C7–C26 afegides el 2026-10-29; prioritat de l'usuari: C7, C8, C12, C17).

## Proposta: dues finestres de text (C27, sense decidir)

**Petició:** obrir dos documents, una finestra al costat de l'altra, i passar d'un a l'altre amb una combinació de tecles. Pensat per tenir un text obert en una banda i treballar sobre l'altre a l'altra.

**Dificultat:** mitjana-alta. Canviar de document és senzill; el cost és dibuixar dues àrees en paral·lel.

**Estat actual:** tot l'estat d'un document és a `struct Editor` (`src/editor.h`) i `main.c` en té una sola instància. `ed_layout(e, COLS, LINES)` i `render.c` (unes 35 referències) suposen tota la pantalla. El desat d'emergència de `main.c` guarda un únic `Editor *`.

**Canvis necessaris:**

| Part | Dificultat | Nota |
|------|------------|------|
| Dos documents i canvi amb una drecera | Baixa | Array de dos `Editor` i un punter al focus. Desfer i autodesat ja són per document |
| Dibuix de dues àrees | Mitjana-alta | `ed_layout` i `render` han de treballar amb un rectangle (fila, columna, amplada, alçada): wordwrap, màquina d'escriure, selecció i cursor |
| Barra d'estat | Mitjana | Una per finestra o una de compartida |
| Desat d'emergència | Baixa | Desar els dos documents en SIGTERM i SIGHUP |
| Portaretalls intern | Baixa | Passar-lo a global per copiar d'una finestra a l'altra |
| Obrir fitxers, recents, navegador, rescat | Mitjana | Decidir en quina finestra s'obre cada cosa |
| Menús, paleta i ajuda | Baixa | Ordres de finestra |
| Proves | Mitjana | Les proves actuals suposen una sola instància |

**Riscos:**

- **R7 (un sol document obert alhora):** és imprescindible en els requisits. Aquesta proposta el contradiu, així que cal modificar R7 abans de començar.
- **Amplada a la Pi:** la pantalla vertical fa 75 columnes. Dues finestres esquerra–dreta en donen unes 37 cada una, poc per al wordwrap de 72. Un repartiment dalt–baix (60 files) seria més útil a la Pi.
- **Màquina d'escriure:** l'ancoratge i el límit depenen de l'alçada. Cada finestra hauria de tenir la seva.

**Proposta de disseny:** una divisió fixa de dues finestres, amb el focus canviable per tecla. Pendent de decidir l'orientació (vertical o horitzontal) i la tecla de canvi.

## Proposta: calaix d'idees (C18, decisions preses)

**Petició:** un dipòsit d'idees que surten mentre s'escriu, i també a partir d'un bloc seleccionat. Ha d'encaixar amb `envia` (còpia al PC per SSH) i tenir un etiquetatge mínim.

**Dificultat:** baixa-mitjana. No calen dependències noves; el codi es pot basar en el navegador de recents (`recentview.c`) i en l'escriptura atòmica del desat.

**Format:** un fitxer `calaix.md` a `/data/textos`, que `envia` ja copia al PC. Cada idea és una entrada Markdown amb data, origen i etiquetes:

```markdown
## 2026-11-02 10:15 · capitol-3.md, línia 42 · #personatge #final

Potser la mare ja sap què ha passat i calla.
```

- L'etiqueta és un `#paraula` lliure a la línia de capçalera. Es pot filtrar des del diàleg; no hi ha llista tancada.
- Una idea sense selecció és text lliure. Una idea amb selecció porta el bloc sencer i la referència al document i a la línia.
- El fitxer és Markdown normal: es llegeix i es cerca amb qualsevol editor, també al PC.

**Entrada:**

| Acció | Proposta | Nota |
|-------|----------|------|
| Afegir idea mentre s'escriu | F9 (decidit) | Diàleg d'una línia per al text i les etiquetes. El document no es mou |
| Enviar la selecció al calaix | Ordre `calaix-seleccio` (Edita) | Copia el bloc amb la referència i demana les etiquetes |
| Veure i filtrar | Ordre `calaix` (Fitxer o Vista) | Llista com `recents`; escriure filtra per etiqueta o text; Enter obre el calaix a l'entrada |

**Decisions (2026-11-02, decidides per l'usuari):**

1. Tecla d'entrada: **F9** (no està assignada per defecte).
2. Si el calaix és obert com a document, **no es pot afegir-hi** una idea: el dipòsit es nega i avisa. Així no hi ha conflicte entre el document obert i el fitxer.
3. `envia` **no serà bidireccional**: el calaix només l'escriu la Pi i el PC el rep per còpia.

**Pi:** afegir una entrada és una escriptura al final del fitxer; el diàleg només llegeix el fitxer quan s'obre.

## Arquitectura

```
src/
  main.c        ✅ arrencada, arguments, bucle principal
  util.c        ✅ assignació de memòria i errors fatals
  utf8.c        ✅ descodificació UTF-8, amplada de caràcters, diacrítics combinats
  buffer.c      ✅ vector de línies; inserir/esborrar/extreure; càrrega i desat atòmic
  md.c          ✅ prefixos de bloc Markdown, tipus de bloc de cada línia i estils per als colors
  wrap.c        ✅ wordwrap suau amb sagnat penjant i cache per línia
  editor.c      ✅ estat de l'editor, cursor, moviment, edició, selecció, porta-retalls i desplaçament de màquina d'escriure
  command.c     ✅ registre d'ordres amb nom i taula de dreceres
  keys.c        ✅ lectura de tecles, noms de tecla ("ctrl-s", "alt-<"...) i enganxat delimitat
  render.c      ✅ dibuix amb ncursesw, colors, barra d'estat i diàlegs a la línia inferior
  search.c      ✅ cerca (sense distingir majúscules si el patró no en té), endavant i enrere
  undo.c        ✅ historial desfer/refer per grups, amb límit de memòria (8 MB)
  autosave.c    ✅ autodesat per inactivitat i interval màxim
  spell.c       ✅ ortografia amb hunspell (carregat amb dlopen), cache de paraules, diccionari personal
  dirlist.c     ✅ lectura i ordenació de directoris, filtre, camins i mides (sense ncurses)
  filebrowser.c ✅ navegador de fitxers a pantalla completa
  help.c        ✅ ajuda (F1): ordres per temes amb les tecles assignades, filtrable
  config.c      ✅ configuració: phileasrc del sistema i de l'usuari, línia d'ordres, plantilla (sense ncurses)
  history.c     ✅ historial de documents: posició del cursor i recents (sense ncurses)
  recentview.c  ✅ diàleg de documents recents
tests/
  test.c        ✅ proves de utf8, wrap, buffer, edició, md, estils i dirlist (sense ncurses)
```


## Compilació i ús

```
make            # compila ./phileas
make test       # proves unitàries
make clean && make debug   # amb AddressSanitizer i UBSan
./phileas [-c COLUMNES] [-a SEGONS] [-d IDIOMA] [fitxer.md]
                     # -a 0 desactiva l'autodesat; -d activa l'ortografia en arrencar
./phileas --config > ~/.config/phileas/phileasrc   # configuració de mostra
```

## Entorn de desenvolupament

- gcc 14.2, ncurses 6.5 (`libncurses-dev` ✅ instal·lat)
- hunspell 1.7.2: biblioteca ✅. No cal `libhunspell-dev`: Phileas carrega `libhunspell-1.7.so.0` en temps d'execució (`dlopen`) i, si no hi és, funciona sense ortografia
- Diccionaris disponibles a `/usr/share/hunspell/`: ca, ca_ES, ca_ES-valencia, es, en_US

## Full de ruta (provisional)

- [x] **F0** — Decisions d'arquitectura: llenguatge, buffer, wordwrap, dreceres
- [x] **F1** — Esquelet: obrir, mostrar, desplaçar-se, desar un fitxer UTF-8 (inclou el wordwrap suau, necessari per mostrar els paràgrafs)
- [x] **F2** — Edició bàsica, sagnat penjant de llistes i cites, columna centrada
- [x] **F2b** — Separació visual de paràgrafs (vegeu «Model de paràgrafs»)
- [x] **F3** — Desfer/refer, cerca i substitució, autodesat
- [x] **F4** — Colors mínims de Markdown (capçaleres, èmfasi)
- [x] **F5** — Navegador de fitxers
- [x] **F6** — Correcció ortogràfica amb hunspell
- [x] **F6b** — Ajuda: F1 obre una finestra amb una guia de totes les dreceres de teclat
- [x] **F6c** — Mode màquina d'escriure («typewriter»): la línia on s'escriu es manté a una altura fixa de la pantalla i el text no arriba mai a baix de tot (pensat per a una pantalla en vertical)
- [x] **F7** — Configuració i proves en Raspberry Pi
- [ ] **F8** — Integració al projecte BYOK (`~/byok-build`). **En curs des del 2026-10-26:** fork `byok/br2-phileas` de la base BYOK, amb Phileas en lloc de micro i WordGrinder. Instruccions, decisions i particions: `BYOK/byok/02 - Fork Phileas.md`.
  - afegir Phileas a la imatge com a paquet de buildroot;
  - afegir-hi la biblioteca `libhunspell-1.7.so.0` (compilada com a la prova del 2026-10-25) a `/usr/lib`;
  - posar els diccionaris `ca` i `es_ES` (`.aff` i `.dic`) a `/usr/share/hunspell`, on Phileas els troba sense variables d'entorn;
  - tenir en compte el mapa de teclat de la imatge (`/usr/share/byok/teclat.map`), que Phileas ja interpreta;
  - exportar `PHILEAS_CONFIG=/data/phileas` al perfil (l'arrel és de només lectura), i posar els valors per defecte de la imatge a `/etc/phileas/phileasrc`.

  **Notes per a la integració** (2026-10-25):
  - **Compilació:** el Makefile ja accepta `CC` i `PKG_CONFIG`, i té `make DESTDIR=… PREFIX=/usr install`. Només depèn de ncursesw en compilar; hunspell es carrega en temps d'execució (`dlopen`) i no cal en compilar.
  - **hunspell:** el buildroot de `~/byok-build` no en té paquet. Cal un paquet propi (font oficial `hunspell-1.7.2.tar.gz`, SHA-256 `11ddfa39afe28c28539fe65fc4f1592d410c1e9b6dd7d8a91ca25d85e9ec65b8`, `--disable-nls --without-ui --without-readline`). Només s'ha d'instal·lar `libhunspell-1.7.so.0`, ~711 KB, que depèn de `libstdc++`, ja present.
  - **Diccionaris:** `ca.aff/.dic` (3,1 MB) i `es_ES.aff/.dic` (0,9 MB) a `/usr/share/hunspell`. **L'arrel (squashfs) és plena** (41,5 MB al 100 %): caldrà fer lloc o fer-la créixer en uns 5 MB (menys un cop comprimits).
  - **Arrencada:** al missatge de `tty1` (`/etc/profile.d/byok.sh`), afegir Phileas al costat de `wg`.
  - **Eines de prova** a `tools/`: `ptyrun.c` (pseudoterminal a la Pi, on dropbear no n'obre), `screen.py` i `simple.ti` (emulador de pantalla mínim per a les proves interactives).
  - **A la Pi, `/tmp/phileas`** (es perd en reiniciar): `phileas-nou`, el guió `ph`, `lib/libhunspell-1.7.so.0`, `dict/` i `config/phileasrc`.

  **Fet al fork `br2-phileas`** (2026-10-26):
  - paquets `phileas` (font local: aquest repositori, `make phileas-rebuild`), `hunspell` (només la biblioteca), `hunspell-ca` (Softcatalà 3.0.9) i `hunspell-es` (LibreOffice, `es_ES`);
  - l'executable a `/usr/libexec/phileas/phileas`; `/usr/bin/phileas` (i l'enllaç `ph`) és un script que posa `PHILEAS_CONFIG=/data/phileas` i en sortir fa `sync` i `clear`, com feia `wg`;
  - `/etc/phileas/phileasrc` de la imatge: `idiomes = es ca` i les ordres externes `lletra`, `gira`, `exporta` (breus) i `envia` (interactiva).

- [ ] **F9** — Phileas a la Pi i a l'escriptori: requisits D1–D7 i millores del cor C1–C26 (vegeu «Phileas a la Pi i a l'escriptori»). Proposta del 2026-10-26, pendent de confirmar.

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
- **2026-10-25** — **F4 completada.**
  - **Capçaleres:**
    - `#` a `######`, amb un color per nivell (els nivells 3 a 6 comparteixen el tercer);
    - les capçaleres subratllades amb `===` o `---` també;
    - els `#` (també els de tancament opcionals) queden atenuats.
  - **Èmfasi:**
    - `**negreta**`, `*cursiva*`, `_cursiva_`, `***totes dues***` i èmfasis niats;
    - no s'activa amb asteriscs envoltats d'espais (`2 * 3`), ni amb `_` dins d'una paraula (`nom_de_fitxer`), ni si no es tanca.
  - **Altres:**
    - codi entre accents oberts i blocs de codi en groc, sense interpretar-ne el contingut;
    - enllaços i imatges amb el text subratllat i la resta atenuada; adreces `<http://...>`;
    - cites en verd; marcadors de llista, de tasca i separadors de taula atenuats;
    - metadades, HTML i separadors atenuats;
    - els escapaments (`\*`) es respecten.
  - **Terminals:** sense cursiva, es fa servir el subratllat; sense colors, només negreta, subratllat i atenuat. A la consola de Linux, vegeu la taula de «Colors mínims de Markdown».
  - **Rendiment:** els estils es calculen només per a les línies visibles, una vegada per línia i dibuix. Document d'11 MB: ~10 ms més de CPU en tota la sessió de prova.
  - **Ordre nova:** `colors` (paleta) activa o desactiva els colors.
  - **Proves:** 1.495 comprovacions unitàries, també amb sanititzadors, i proves interactives amb TERM=xterm-256color, linux i vt100.
- **2026-10-25** — Prova de l'usuari de la F4: els colors estan bé.
- **2026-10-25** — **F5 completada.**
  - **Obrir** (Ctrl+O): navegador de fitxers a pantalla completa.
    - Comença al directori del document actual, amb el document seleccionat. Si no té nom, a l'últim directori visitat o al de treball.
    - Primer `../` i els directoris, després els fitxers, per ordre alfabètic. Cada fitxer mostra la mida i la data de modificació. En terminals estrets es treu la data, i després la mida.
    - La capçalera mostra el camí amb `~` per al directori personal. Si és massa llarg, se'n retalla el principi.
  - **Tecles del navegador:**
    - ↑ ↓, RePàg/AvPàg, Inici/Fi per moure's;
    - Enter obre el fitxer o entra al directori; → entra al directori;
    - ← (o Retrocés sense filtre) puja al directori pare i hi selecciona el que es deixa;
    - escriure filtra la llista (sense distingir majúscules, també amb accents); Retrocés esborra el filtre;
    - un filtre que comença per `/` o `~` és un camí: Enter hi va directament;
    - si el filtre no coincideix amb res, Enter crea un fitxer nou amb aquest nom al directori;
    - Ctrl+A mostra o amaga els fitxers ocults;
    - Esc buida el filtre i, si ja és buit, surt sense obrir res.
  - **Document actual:**
    - si té canvis sense desar, pregunta si cal desar-los (s/n, Esc cancel·la);
    - no torna a obrir el document que ja està obert;
    - els fitxers binaris (amb bytes nuls) no s'obren;
    - el porta-retalls intern es conserva: es pot copiar d'un document a l'altre.
  - **Document nou** (Ctrl+N): document buit sense nom, amb la mateixa pregunta si hi ha canvis.
  - **Errors:** un directori sense permís mostra el motiu i el navegador es queda on era.
  - **Proves:**
    - 1.520 comprovacions unitàries, també amb sanititzadors;
    - proves interactives amb un emulador de pantalla mínim (terminfo propi): filtre, directoris, pare, `~`, ocults, fitxer nou, binari, canvis sense desar, `/usr/bin` (3.100 entrades) i directori sense permís.
- **2026-10-25** — Prova de l'usuari de la F5: tot correcte. Petició afegida al full de ruta (F6b): F1 obre una finestra amb una guia de totes les dreceres de teclat.
- **2026-10-25** — **F6 completada.**
  - **Decisió tècnica:** hunspell es carrega en temps d'execució (`dlopen`) en lloc d'enllaçar-lo en compilar.
    - No cal el paquet `libhunspell-dev` per compilar.
    - Si una màquina no té hunspell, Phileas funciona igual, sense ortografia, i la barra n'explica el motiu.
    - La biblioteca té una API C estable: Phileas hi declara les set funcions que fa servir.
  - **Diccionari:**
    - per defecte, el de la configuració regional (`ca_ES.UTF-8` → `ca_ES` → `ca`), i si no n'hi ha, `ca`;
    - `-d IDIOMA` a la línia d'ordres, o l'ordre `idioma` de la paleta per canviar-lo en marxa;
    - `-d no` no carrega l'ortografia;
    - es busca a `$DICPATH`, `~/.local/share/hunspell`, `/usr/share/hunspell` i `/usr/share/myspell`;
    - els diccionaris que no són UTF-8 es converteixen amb `iconv`.
  - **A la pantalla:**
    - els errors surten en vermell i subratllats; a la consola de Linux, només en vermell, i sense colors, només subratllats;
    - la paraula que s'està escrivint no es marca fins que s'acaba;
    - no es revisen el codi, els marcadors, les adreces (`http://`, `www.`, correus), els noms de fitxer (`prova.md`) ni les paraules amb xifres (`2a`, `mp3`);
    - les paraules inclouen l'apòstrof (`l'home`, `l’home`), la ela geminada (`col·lecció`) i els pronoms amb guionet (`fes-ho`).
  - **Ordres:**
    - F7 (`revisar-ortografia`) selecciona l'error següent, fent la volta al document;
    - Alt+Enter (`suggeriments`) mostra fins a 9 suggeriments per a la paraula del cursor o la seleccionada: el número la substitueix (es pot desfer), `a` l'afegeix al diccionari personal, `i` la ignora fins que es tanqui Phileas;
    - `ortografia` (paleta) marca o no els errors.
  - **Diccionari personal:** `~/.config/phileas/diccionari.txt`, una paraula per línia. Es carrega amb el diccionari.
  - **Rendiment:**
    - els resultats es guarden en una cache de paraules, de manera que cada paraula només es consulta a hunspell una vegada;
    - el diccionari català ocupa ~17 MB de memòria;
    - el document es mostra abans de carregar el diccionari, perquè en una Raspberry Pi petita pot trigar uns segons (pendent de mesurar a la F7);
    - F7 en el document d'11 MB: 0,19 s de CPU.
  - **Proves:** 1.547 comprovacions unitàries, també amb sanititzadors (tokenització i, si hi ha el diccionari català, consultes, cache, suggeriments i paraules ignorades), i proves interactives de marcatge, F7, suggeriments, afegir, ignorar, `idioma` i `-d`.
  - **Observació:** amb la normativa del 2017, el diccionari ja no accepta «dóna» (ara «dona»).
- **2026-10-25** — Petició de l'usuari: l'ortografia **desactivada per defecte**, pensant en una Raspberry Pi de primera generació.
  - **En arrencar:** no es carrega cap diccionari. Phileas ocupa ~3 MB de memòria, i ~27 MB amb el diccionari català.
  - **Per activar-la:**
    - F7 carrega el diccionari la primera vegada, activa el marcatge i salta al primer error;
    - l'ordre `ortografia` de la paleta l'activa o la desactiva;
    - Alt+Enter carrega el diccionari si cal, però no activa el marcatge;
    - `-d IDIOMA` l'activa des de l'arrencada amb aquest diccionari.
  - **Diccionari:** sense `-d`, el de la configuració regional.
- **2026-10-25** — **Primera prova en una Raspberry Pi real:** Raspberry Pi 1 Model B Rev 2 (ARMv6 a 700 MHz, 475 MB de RAM), amb una imatge de Buildroot 2026.02.
  - **Compilació creuada:** amb la toolchain de buildroot de l'usuari (`~/byok-build/output-pi1b-wg`, només de lectura). La Pi no té compilador, i l'arrel és de només lectura (squashfs, plena): les proves es fan a `/tmp`, que és en RAM.
    ```
    make CC=.../host/bin/arm-linux-gcc PKG_CONFIG=.../host/bin/pkg-config \
         PKG_CONFIG_SYSROOT_DIR=.../sysroot PKG_CONFIG_LIBDIR=.../sysroot/usr/lib/pkgconfig
    ```
    Executable: 83 KB (sense símbols). Només depèn de `libncursesw.so.6` i de la glibc.
  - **Correccions trobades:**
    - el hash de la cache d'ortografia feia servir una constant de 64 bits que es truncava a 32 bits: ara el càlcul sempre es fa en 64 bits;
    - les macros `_DEFAULT_SOURCE` i `_XOPEN_SOURCE` arribaven per casualitat amb `pkg-config ncursesw`: ara són al Makefile, que també accepta `PKG_CONFIG` per a la compilació creuada;
    - les proves unitàries no requereixen `C.UTF-8`, que la Pi no té: provaran també `ca_ES.UTF-8` i `en_US.UTF-8`.
  - **Proves unitàries a la Pi:** 1.530 comprovacions, 0 errors, en 0,14 s. Les de hunspell se salten perquè la Pi no té la biblioteca.
  - **Rendiment** (document de 2,7 MB, 10.047 paràgrafs, per SSH):

    | Prova | CPU | Memòria (RSS) |
    |-------|-----|---------------|
    | Carregar i mostrar el document | 0,32 s | 5,6 MB |
    | Cada pulsació de navegació (AvPàg, fletxes) | ~6 ms | 5,6 MB |
    | Cada lletra escrita | ~10 ms | 5,7 MB |
    | Inici / fi del document | 0,02 s | 5,7 MB |

  - **Observació:** el servidor SSH de la Pi (dropbear) no obre terminals (`ssh -tt` es tanca). Per a les proves s'ha fet servir un petit programa auxiliar (`ptyrun`, amb `forkpty`), que no forma part de Phileas.
  - **Pendent:** l'ortografia a la Pi. Cal hunspell compilat per a ARM i el diccionari català.
- **2026-10-25** — Petició de l'usuari sobre el plantejament de l'ortografia: el diccionari ha de ser fàcil de commutar entre català i castellà.
  - **F7** activa o desactiva la correcció (abans saltava a l'error següent). La primera vegada carrega el diccionari.
  - **F8** alterna el diccionari: català ↔ castellà.
    - Si la correcció està activa, el canvi es veu de seguida.
    - Si no ho està, només es tria l'idioma, i el diccionari no es carrega fins que es prem F7.
    - El català i el castellà es mantenen carregats tots dos: només el primer canvi a cada idioma carrega el diccionari.
  - **Barra d'estat:** amb la correcció activa, hi consta `ca` o `es` abans de la línia i la columna.
  - **Maj+F7** salta a l'error següent (també l'ordre `revisar-ortografia` de la paleta).
  - **Diccionari personal per idioma:** `diccionari-ca.txt`, `diccionari-es.txt`… Una paraula afegida en castellà no s'accepta en català.
  - **Noms de diccionari:** `-d es` i l'ordre `idioma` accepten només l'idioma: `es` troba `es_ES`. Una tria com `ca_ES-valencia` es manté fins que es prem F8.
  - **Proves:** 1.552 comprovacions unitàries (codis d'idioma, `es` → `es_ES`) i proves interactives de F7, F8, la barra d'estat, Maj+F7 i el diccionari personal per idioma.
- **2026-10-25** — Petició afegida al full de ruta (F6c): mode màquina d'escriure. La línia on s'escriu es manté a una altura fixa i el text no arriba a baix de la pantalla (l'usuari escriu amb la pantalla en vertical).
- **2026-10-25** — **Error corregit (informat per l'usuari a la Pi):** les tecles de selecció escrivien text (`;2A`…) en lloc de seleccionar.
  - **Causa:** el mapa de teclat de la imatge (`/usr/share/byok/teclat.map`, fet per a l'editor micro) fa que Maj/Ctrl/Alt + fletxes, Inici/Fi i RePàg/AvPàg enviïn seqüències d'estil xterm (`\e[1;2A` = Maj+↑, `\e[1;5C` = Ctrl+→, `\e[5;2~` = Maj+RePàg…). El terminfo `linux` no les té, i ncurses les passava com un Esc seguit de text.
  - **Solució:**
    - Phileas interpreta directament les seqüències CSI (`\e[…`) i SS3 (`\eO…`) que ncurses no reconeix, amb tots els modificadors xterm (Maj, Alt, Ctrl i les seves combinacions);
    - les seqüències desconegudes es descarten en lloc d'escriure-les al text;
    - cada tecla amb modificadors té un codi intern fix (`KEY_MODIFIED`), tant si arriba pel terminfo com interpretada per Phileas. Així les dreceres funcionen igual a xterm, a la consola de Linux i per SSH.
  - **Dreceres noves de selecció:** Ctrl+Maj+↑/↓ (fins al paràgraf anterior o següent) i Ctrl+Maj+Inici/Fi (fins a l'inici o el final del document).
  - **Proves:**
    - tallar, copiar, enganxar i esborrar seleccions fetes amb Maj+fletxes, Ctrl+Maj+fletxes, Maj+Inici, Maj+RePàg i Ctrl+Maj+Inici/Fi, amb TERM=simple (sense les tecles al terminfo), linux i xterm-256color;
    - les mateixes proves a la Raspberry Pi amb TERM=linux;
    - una seqüència desconeguda (`\e[1;9Z`) no deixa rastre al text.
- **2026-10-25** — **Ortografia a la Raspberry Pi 1.**
  - **hunspell 1.7.2** compilat amb la toolchain de buildroot, a partir del codi font oficial (SHA-256 `11ddfa39…65b8`). Només la biblioteca: `libhunspell-1.7.so.0`, 711 KB sense símbols, que depèn de `libstdc++` (ja és a la imatge).
  - **A la Pi, a `/tmp/phileas`:** la biblioteca a `lib/` i els diccionaris `ca` i `es_ES` a `dict/`, copiats del PC. Phileas els troba amb `LD_LIBRARY_PATH` i `DICPATH`, sense tocar la imatge.
  - **Proves unitàries a la Pi:** 1.552 comprovacions, 0 errors, ara també amb hunspell (5 s, sobretot per la càrrega dels diccionaris).
  - **Mesures** (temps real per SSH):

    | Acció | Temps | Memòria (RSS) |
    |-------|-------|---------------|
    | Obrir un document, sense ortografia | — | 2,4 MB |
    | F7: carregar el català i activar | 2,4 s | 16 MB |
    | F8: passar al castellà | 1,4 s | 20 MB |
    | F8: tornar al català | 3,1 s | 20 MB |
    | F7: desactivar o reactivar (sense recarregar) | instantani | 20 MB |
    | Maj+F7: error següent | 0,04 s | 20 MB |
    | Alt+Enter: suggeriments | 1,3 s | 20 MB |
    | Escriure amb l'ortografia activa | ~6 ms per lletra | 20 MB |

  - **Conclusions:**
    - desactivar l'ortografia per defecte és encertat: sense ortografia, Phileas ocupa 2,4 MB;
    - escriure amb l'ortografia activa no es nota, gràcies a la cache de paraules;
    - el cost és cada canvi d'idioma amb F8 (1,4–3,1 s), perquè es torna a carregar el diccionari;
    - els suggeriments triguen ~1,3 s, i mentrestant la barra diu «Buscant suggeriments…».
  - **Per fer-ho permanent a la imatge** (decisió de l'usuari): afegir `libhunspell-1.7.so.0` i els diccionaris a l'arrel (p. ex. `/usr/lib` i `/usr/share/hunspell`), o deixar-los a `/data` i exportar `LD_LIBRARY_PATH` i `DICPATH` al perfil.
- **2026-10-25** — Decisió de l'usuari: **F8 instantani**, mantenint el català i el castellà carregats alhora.
  - **Funcionament:**
    - `spell.c` manté fins a dos diccionaris carregats, cadascun amb la seva cache de paraules (32.768 entrades);
    - tornar a un diccionari que ja hi és no carrega res i no mostra «Carregant…»;
    - si se'n carrega un tercer (per exemple, `ca_ES-valencia` amb l'ordre `idioma`), es descarta el que fa més temps que no es fa servir;
    - les paraules ignorades valen per a tots dos.
  - **A la Pi:**

    | Acció | Temps | Memòria (RSS) |
    |-------|-------|---------------|
    | F7: carregar el català | 2,5 s | 16 MB |
    | F8: primer canvi al castellà (el carrega) | 1,0 s | 20 MB |
    | F8: canvis següents | 0,01 s | 20 MB |
    | F7 i F8 amb l'ortografia desactivada | 0,01 s | 20 MB |

    La memòria és la mateixa que abans: la glibc no retornava al sistema la memòria del diccionari descarregat.
  - **Proves:** 1.561 comprovacions unitàries, també amb sanititzadors (dos diccionaris, cache independent per idioma, paraules ignorades compartides, descart del menys usat).
  - **A la Pi, per provar-ho:** `/tmp/phileas/ph FITXER`, un guió que arrenca `phileas-nou` amb la biblioteca i els diccionaris de `/tmp/phileas`.
- **2026-10-25** — Decisió de l'usuari: quan el processador estigui avançat, s'integrarà al projecte BYOK, i llavors s'hi afegiran `libhunspell` i els diccionaris a `/usr`. Anotat al full de ruta com a **F8**.
- **2026-10-25** — Anàlisi del mode màquina d'escriure (F6c). Proposta: un sol mecanisme amb dos paràmetres (límit i ancoratge), amb el salt a l'estil WordGrinder per defecte (85 % → 55 %) i els modes `fixa` i `classic` com a alternatives. Pendent de confirmar.
- **2026-10-25** — ✅ Proposta del mode màquina d'escriure confirmada. **F6c completada.**
  - **Per defecte, salt a l'estil WordGrinder:** quan el cursor passa el 85 % de l'alçada, el text puja i el cursor queda al 55 %. Si el cursor surt per dalt, també torna al 55 %.
  - **Final del document:** el text es pot desplaçar més enllà de l'última línia. En escriure al final, sempre hi ha espai en blanc per sota.
  - **Salts llargs:** en obrir un document, amb Ctrl+Fi, en una cerca o anant a una línia, el cursor queda a l'ancoratge.
  - **Ordre `maquina-escriure`** (paleta): passa per salt → línia fixa (el cursor sempre al 55 %) → clàssic (el comportament anterior).
  - **Paràmetres:** `tw_limit` i `tw_anchor` (85 i 55), que aniran al fitxer de configuració a la F7.
  - **Implementació:** tot a `ed_layout()`. Calcula la fila de pantalla del cursor, recorrent com a molt l'alçada de la pantalla, i hi aplica les regles.
  - **Proves:**
    - `editor.c` no depèn de ncurses i ara també entra a les proves unitàries: 1.579 comprovacions (salt, línia fixa, clàssic, inici i final del document, canvi de mida);
    - proves interactives i a la Pi: 1.577 comprovacions; les dues que falten necessiten el diccionari valencià, que la Pi no té.
  - **Pendent de verificar a la consola de la Pi:** si la línia fixa és fluida amb la pantalla girada.
- **2026-10-25** — Prova de l'usuari de la F6c: tot correcte. Ajust visual a l'estil WordGrinder: **en obrir o crear un document, el cursor va a l'ancoratge** (55 %).
  - **Document nou o buit:** el cursor comença a mitja pantalla.
  - **Document existent:** el final del text queda a l'ancoratge, encara que el document no ompli la pantalla. En aquest cas, per sobre del text hi ha espai en blanc.
  - **En escriure:** el text va baixant des d'aquest punt i entra en la lògica del salt (85 % → 55 %), que es menja l'espai en blanc de dalt.
  - **En pujar dins la primera pantalla:** el text no es mou.
  - **AvPàg:** primer consumeix l'espai en blanc.
  - **Mode clàssic:** sense espai en blanc a dalt.
  - **Implementació:** `top_blank`, les files en blanc per sobre de l'inici del document, i `recenter`, que activen `ed_open` i `ed_init`.
  - **Proves:** 1.587 comprovacions unitàries (1.585 a la Pi) i proves interactives: sense fitxer, fitxer nou, document curt, Ctrl+N i Ctrl+O.
- **2026-10-25** — Petició de l'usuari: la relació de salts, una mica més amunt que 55 %–85 %.
  - **Nous valors per defecte:** límit 80 % i ancoratge 50 %. El salt manté la mida (un 30 % de l'alçada).
  - **A la pantalla vertical de la Pi** (58 files de text): en passar la fila 46, el cursor torna a la 29.
  - **Proves:** 1.588 comprovacions unitàries (1.586 a la Pi).
- **2026-10-25** — Prova de l'usuari del salt 80 % → 50 %: va bé. Proposta de la configuració (F7): fitxer `phileasrc` declaratiu, amb prioritats (línia d'ordres > usuari > `/etc/phileas` > valors interns) i `PHILEAS_CONFIG` per a la Pi, on només `/data` és escrivible. Detectat un problema previ: els fitxers que no són Markdown es poden alterar en desar-los (s'ajunten línies); proposta de mode text pla. Pendent de confirmar.
- **2026-10-25** — ✅ Proposta de configuració confirmada. **F7 completada.**
  - **Mode text pla:**
    - els fitxers que no acaben en `.md`, `.markdown`, `.mdown`, `.mkd` o `.txt` es carreguen línia a línia, sense ajuntar paràgrafs ni separacions;
    - no hi ha colors de Markdown ni continuació de llistes, i l'enganxat s'insereix tal qual;
    - es desen exactament igual, també les línies en blanc del final;
    - la barra d'estat diu `(text)`.

    El mode es tria en obrir el fitxer i es manté si es desa amb un altre nom.
  - **Fitxer `phileasrc`:**
    - es llegeixen `/etc/phileas/phileasrc` i després el de l'usuari: `$PHILEAS_CONFIG`, `$XDG_CONFIG_HOME/phileas` o `~/.config/phileas`;
    - les opcions `-c`, `-a` i `-d` manen sobre tots dos, i es validen igual: un valor incorrecte atura l'arrencada amb un missatge;
    - opcions: `columnes`, `centrar`, `colors`, `maquina-escriure` (i `-limit`, `-ancoratge`), `autodesat`, `autodesat-maxim`, `ortografia`, `idiomes`, `diccionari`, `color-capcalera-1/2/3`, `color-codi`, `color-cita`, `color-error` i `tecla.NOM`;
    - `tecla.NOM =` (buit) treu una drecera.
  - **Errors:** la línia s'ignora i la barra d'estat avisa de la primera (fitxer, línia i motiu) i del nombre de les altres.
  - **Ordres:**
    - `recarregar-configuracio` (paleta) torna als valors interns, rellegeix els fitxers i torna a aplicar la línia d'ordres;
    - `phileas --config` escriu una plantilla completa i comentada.
  - **Idiomes:** l'opció `idiomes` defineix la rotació de F8 (fins a 4). Sense configuració, és `ca es`, o `es ca` si la configuració regional és castellana.
  - **Diccionari personal:** ara va al directori de configuració (`$PHILEAS_CONFIG`), i per tant a la Pi pot anar a `/data`.
  - **Proves:**
    - 1.628 comprovacions unitàries, també amb sanititzadors: valors correctes i incorrectes, comentaris, errors amb número de línia, prioritat sistema < usuari < línia d'ordres, tornada als valors interns, plantilla sense errors, text pla (anada i tornada exacta, i el mateix fitxer com a Markdown, que s'ajuntaria);
    - proves interactives: columnes, `-c`, F9 i Ctrl+T configurades, errors, `idiomes = es ca`, diccionari personal a `PHILEAS_CONFIG`, i el `phileasrc` editat amb el mateix Phileas i recarregat;
    - a la Pi: 1.626 comprovacions. El guió `/tmp/phileas/ph` fa servir `PHILEAS_CONFIG=/tmp/phileas/config`, amb una plantilla.
- **2026-10-25** — **F6b completada: ajuda amb F1.**
  - **Pantalla completa** amb totes les ordres agrupades per temes: fitxer, moviment, selecció i porta-retalls, edició, cerca, ortografia, vista, i configuració i ajuda. Cada ordre mostra les tecles, la descripció i el nom (per a la paleta i el `phileasrc`).
  - **Generada del registre:**
    - les tecles són les assignades en aquell moment, incloses les del `phileasrc`;
    - una ordre sense tecla es marca amb «—»;
    - una ordre nova que no tingui secció surt a «Altres».
  - **Noms de tecla llegibles** (`key_describe`): `Ctrl+S`, `Alt+B`, `Maj+F7`, `Ctrl+Maj+←`, `RePàg`, `Supr`…
  - **Ús:** escriure filtra per tecla, nom o descripció (sense distingir majúscules); ↑ ↓, RePàg/AvPàg i Inici/Fi per desplaçar-se; Esc buida el filtre o tanca, i F1 també tanca.
  - **A sobre**, quatre línies sobre la paleta, el `phileasrc`, el navegador i els suggeriments.
  - **Pensada per a 75 columnes** (la consola vertical de la Pi): el nom d'ordre només es mostra si hi cap sencer.
  - **Proves:** interactives a 80 i a 75 columnes (filtre, tecles del `phileasrc`, Esc), i a la Pi amb TERM=linux. Unitàries: 1.628 (1.626 a la Pi).
- **2026-10-25** — Fases F0–F7 completades. Pla de l'usuari: començar la integració al projecte BYOK (F8) l'endemà.
- **2026-10-25** — Petició de l'usuari: l'ajuda inclou ara les **ordres de la paleta**.
  - **Secció pròpia, al principi:** «Ordres de la paleta», amb les ordres que no tenen cap tecla assignada. A la primera columna hi ha el nom que cal escriure després de Ctrl+P.
  - **Es genera sola:** si al `phileasrc` s'assigna una tecla a una d'aquestes ordres, deixa de sortir en aquesta secció.
  - **A les seccions per temes**, les ordres sense tecla es marquen amb «paleta».
- **2026-10-25** — **Error corregit (informat per l'usuari a la Pi):** Ctrl+Z (desfer) no feia res.
  - **Causa:** els terminfo `linux` (la consola) i `screen` declaren Ctrl+Z com a tecla de suspendre (`kspd=^Z`). ncurses el convertia en el codi `KEY_SUSPEND`, que no té cap ordre. No era el mapa de teclat (Ctrl+Z hi envia 0x1A) ni els senyals (el terminal té `-isig`). A xterm funcionava perquè `xterm-256color` no té `kspd`.
  - **Solució:** `keyok(KEY_SUSPEND, FALSE)` a `key_init()`, perquè ncurses no tradueixi aquesta tecla. És l'única tecla d'un sol caràcter de control en aquesta situació: `kbs`, el Retrocés, ja es tractava.
  - **Proves:** desfer després d'escriure i d'esborrar amb Supr, i refer, amb TERM=linux, screen i xterm-256color al PC, i a la Pi amb TERM=linux.
- **2026-10-25** — Final de la jornada: totes les fases F0–F7 completades i provades a la Raspberry Pi 1. Afegits `make install` (per al paquet de buildroot) i les eines de prova a `tools/`. **Pla:** l'endemà s'integra Phileas al projecte BYOK, i tots dos projectes evolucionaran junts des d'allà (vegeu les notes de la F8).
- **2026-10-26** — **Inici de la F8.** Instruccions de l'usuari: fork de BYOK anomenat phileas, sense micro ni WordGrinder; la paleta ha de cridar `lletra`, `gira`, `exporta` i `envia`; en sortir, `sync` i `clear`. Document: `BYOK/byok/02 - Fork Phileas.md`.
  - **Ordres externes** (perquè Phileas continuï sent genèric, sense les ordres de BYOK escrites al codi):
    - `externa.NOM = ORDRE` i `externa-breu.NOM = ORDRE` al `phileasrc`. `NOM` passa a ser una ordre més: surt a la paleta i a l'ajuda (secció «Ordres de la paleta») i se li pot assignar una tecla amb `tecla.`, en una línia posterior;
    - a la paleta, el que s'escriu després del nom en són els arguments (`lletra +`, `gira dreta`), cadascun entre cometes simples. Les ordres de Phileas responen «no admet arguments»;
    - abans d'executar-la es desa el document, si té nom i canvis;
    - **breu:** sense teclat (`</dev/null`), «NOM: executant…» a la barra, i després la darrera línia de la sortida (o «NOM: fet»). Redibuixa la pantalla sencera i adopta la mida nova del terminal (`ui_sync_size`), perquè `lletra` i `gira` la canvien;
    - **interactiva:** el terminal torna al mode normal (`ui_suspend`), s'esborra la pantalla, s'executa l'ordre, «Prem Retorn per tornar a Phileas.» i es torna a l'editor (`ui_resume`);
    - errors: «no s'ha trobat l'ordre» (127) i «(error N)» amb la darrera línia;
    - `recarregar-configuracio` les esborra i les torna a llegir. La plantilla (`--config`) les explica.
  - **Proves:** 1.636 comprovacions unitàries, també amb sanititzadors (sintaxi, noms incorrectes, valor buit, nom d'una ordre de Phileas, comentari al final de la línia); interactives: arguments amb cometes, desat previ, error amb codi, tecla F12, ordre interna amb arguments, ordre interactiva i tornada, recàrrega, i tot plegat amb ASan/UBSan sense errors.
- **2026-10-26** — Provat a la Pi dins la imatge BYOK Phileas: tot correcte. **Versió 1.0** (`PHILEAS_VERSION` 1.0.0, etiqueta `v1.0`), la de la imatge BYOK Phileas 1.0.
- **2026-10-26** — **Marges** `marge-esquerre` i `marge-dret` (2 i 2 per defecte), a petició de l'usuari. Proposta de la **F9**: un sol programa per a la Pi i per a l'escriptori, sense branca pròpia; requisits de l'escriptori (D1–D7) i millores candidates del cor (C1–C6). Pendent de confirmar.
- **2026-10-26** — **D2: desat en rebre SIGHUP o SIGTERM.**
  - **Mecanisme:** el gestor del senyal només l'anota (`sigaction` sense `SA_RESTART`, instal·lat després de `ui_init` perquè ncurses en posa uns de propis). Totes les lectures de tecles passen per `key_get_wch()` (`keys.c`, també les dels diàlegs de `render.c`): quan hi ha un senyal pendent, criden `key_quit_hook`, que desa amb `autosave_emergency()` (`autosave.c`), restaura el terminal, escriu on ha desat a stderr i acaba amb `_exit(128 + senyal)`. El desat es fa fora del gestor, en un punt on el document és coherent.
  - **On desa:** sobre el fitxer del document; si no té nom o no s'hi pot escriure, `rescat-AAAAMMDD-HHMMSS.md` (`.txt` en text pla) al directori de configuració, que es crea si cal; si tampoc, a `$HOME`.
  - **Límit conegut:** si el senyal arriba just entre la comprovació i el `read()` de ncurses, no es desa fins a la tecla següent (finestra de microsegons).
  - **BYOK (br2-phileas):** `apagada` ja enviava SIGTERM a Phileas i esperava 1 s; ara espera que acabi, com a molt 5 s, abans de desmuntar `/data`.
  - **Proves:** 1.652 comprovacions unitàries, també amb sanititzadors (sense canvis, amb nom, rescat al directori de configuració, a `$HOME`, i cap lloc possible). Interactives en un pseudoterminal: SIGTERM i tancament del terminal (SIGHUP), amb nom i sense, i SIGTERM amb el diàleg de cerca obert: tot desat (amb accents) i sortida immediata. **Pendent:** l'apagada a la Pi.
- **2026-10-26** — **Prova de l'usuari a la Pi (D2):** amb nom, correcte. Sense nom, «no ha funcionat»: el rescat s'havia desat, però a `/data/phileas` (el directori de configuració), on l'usuari no el veu i on no arriben el navegador de fitxers, `exporta` ni `envia`. **Canvi:** el rescat va primer al **directori actual** (a la BYOK, `/data/textos`; al PC, on s'ha obert Phileas), després al de configuració i després a `$HOME`. Proves: 1.656 comprovacions (directori actual, directori actual esborrat → configuració, → `$HOME`, enlloc) i prova interactiva amb SIGTERM.
- **2026-10-26** — **D1, primera prova:** Phileas a GNOME Terminal (Wayland, 96×34) amb un document de mostra i el corrector català. Prova de l'usuari: **tot correcte** («tot ok»; se li havien proposat els colors, els marges, F1, Alt+C, Ctrl+P, Ctrl+Maj+fletxes, el canvi de mida de la finestra i el desat en tancar-la). Queden Konsole, xterm i altres.
- **2026-10-26** — **D1, xterm:** amb la configuració per defecte (sense `.Xresources`; només `-fa Monospace -fs 12`), el mateix document i el corrector català. Prova de l'usuari: **tot correcte**, també les tecles Alt (es temia que xterm les enviés com a caràcters de 8 bits). Queden Konsole, kitty i altres, en un ordinador que els tingui.
- **2026-10-26** — **C1: autocompleció a la paleta.**
  - **Tab** allarga el nom fins al prefix comú més llarg de les ordres que hi coincideixen (també les externes del `phileasrc`). Si n'hi ha una de sola, la completa sencera amb un espai darrere, per escriure-hi arguments o prémer Retorn. Si n'hi ha diverses, les mostra atenuades darrere del text, tantes com hi càpiguen; si no n'hi ha cap, «(cap ordre comença així)». Els arguments no es completen.
  - Amb la paleta buida, «(Tab completa el nom)» atenuat; l'ajuda (F1) també ho diu.
  - **Codi:** `complete.c` (sense ncurses, a les proves), `prompt_complete()` a `render.c` (el `prompt()` de sempre hi passa sense autocompleció) i `palette_complete()` a `command.c`.
  - **Proves:** 1.663 comprovacions, també amb sanititzadors (prefix comú, únic, cap, buit, nom complet que és prefix d'un altre, sense lloc). Interactives: llista, completat únic i Retorn (executa `centrar`), cap coincidència, Tab amb la paleta buida, edició després del Tab.
- **2026-10-26** — Marges per defecte a **4 i 4** (eren 2 i 2), a petició de l'usuari; també al `phileasrc` de br2-phileas. C1 provada per l'usuari a GNOME Terminal: funciona.
- **2026-10-26** — **Error corregit (informat per l'usuari a la Pi):** `lletra +` i `lletra -` des de la paleta deien «no s'ha pogut executar», però la lletra sí que canviava.
  - **Causa:** `lletra` canvia la mida de la consola i el nucli envia SIGWINCH a Phileas mentre llegeix la sortida de l'ordre breu. El senyal interrompia el `fgets()` (EINTR), Phileas deixava de llegir i tancava la canonada, i l'ordre moria per SIGPIPE en escriure la darrera línia («Lletra … caràcters»). Una ordre acabada per un senyal es mostrava com «no s'ha pogut executar».
  - **Solució:** la lectura continua després d'un EINTR, fins al final de la sortida. Si una ordre acaba per un senyal, ara es diu: «l'ordre s'ha interromput (senyal)».
  - **Proves:** al PC, una ordre breu que escriu després d'un SIGWINCH (canvi de mida del pseudoterminal): abans «(error 1)», ara la darrera línia a la barra. 1.663 comprovacions. **Pendent:** a la Pi.
- **2026-10-27** — **Menús a l'estil WordGrinder** (ordre `menu`, a Esc), a petició de l'usuari.
  - **Comportament:** Esc obre el menú principal (Fitxer, Edita, Cerca, Ortografia, Vista, BYOK, Ajuda i configuració). ↑↓ i Enter, o la lletra de l'opció, hi entren; → obre un submenú; ← o Esc tornen un nivell enrere i, al principal, el tanquen. Cada opció mostra la seva drecera actual (la de l'usuari si l'ha canviada al `phileasrc`). Si hi ha selecció, Esc primer la cancel·la (l'ordre `cancellar` continua a la paleta).
  - **Disseny:** `src/menu.c`, una taula fixa en C. `menu_choose()` només retorna l'ordre triada; l'executa `c_menu()` amb `cmd_run()`, amb el menú ja tancat. Les opcions d'ordres que no existeixen no es mostren, ni els submenús buits.
  - **Submenú BYOK:** només hi és si hi ha ordres externes. Les conegudes tenen etiqueta i arguments propis (Lletra més gran = `lletra +`, Lletra més petita = `lletra -`, Gira la pantalla, Exporta al pendrive, Envia a un ordinador). Les altres externes s'hi afegeixen amb el seu nom. Es torna a construir cada vegada, perquè la configuració es pot recarregar.
  - **Pantalles petites:** les caixes es desplacen per cabre i, si no hi caben d'alt, la llista es desplaça amb el cursor (fletxes ↑↓ a la vora).
  - **Limitació:** com que Esc cancel·la la selecció, Talla i Copia del menú no tenen mai res seleccionat; cal Ctrl+X i Ctrl+C.
  - **Proves:** al PC amb `tools/screen.py` (80×24 i 40×10): menú, submenús, lletres, ←, Esc Esc, Esc amb selecció, BYOK amb externes de prova (`lletra -` arriba amb l'argument), l'ajuda. 1.663 comprovacions. **Pendent:** a la Pi (caràcters de les vores i fletxa → a la consola).
- **2026-10-27** — Menús gravats a la SD i provats a la Pi per l'usuari: correctes («milloren molt la usabilitat»). **Versió 2.0** (`PHILEAS_VERSION` 2.0.0, etiqueta `v2.0`): v1.0 + autocompleció a la paleta (C1), marges, desat en rebre SIGHUP/SIGTERM, rescat al directori actual, correcció d'EINTR a les ordres breus i els menús (Esc).
- **2026-10-28** — Repositori publicat a GitHub (`asolans/phileas`: `main`, `v1.0`, `v2.0`). `README.md` complet (descripció, instal·lació, ús, tecles, configuració, desenvolupament). **Empaquetat (D5 b):** `make deb` genera `dist/phileas_VERSIÓ_ARQ.deb` (`packaging/mkdeb.sh`: executable dinàmic, depèn de libncursesw6/libtinfo6 i recomana libhunspell-1.7-0, hunspell-ca i hunspell-es) i `make dist`, el codi font de HEAD en `.tar.gz`. Provat: el `.deb` s'extreu i l'executable funciona; el `.tar.gz` compila i passa les 1.663 comprovacions.
- **2026-10-28** — Llicència: **GPL-3.0-or-later**. Text complet a `LICENSE`; README i `packaging/copyright` actualitzats.
- **2026-10-28** — README: secció «Raspberry Pi» amb dos camins: (A) Raspberry Pi OS (Lite de 32 bits per a Pi 1/Zero), compilant a la Pi amb `make install` o `make deb` (armhf/arm64), configuració regional UTF-8, lletra de la consola i arrencada directa a Phileas a tty1; (B) la imatge BYOK. **Pendent:** el camí A no s'ha provat en un Raspberry Pi OS real.
- **2026-10-28** — Llançador per a l'escriptori: `packaging/phileas.desktop` (obre Phileas en un terminal; «Obre amb» per a `.md` i `.txt`) i icona `packaging/phileas.svg`, instal·lats per `make install-desktop` (fora d'`install`, perquè la imatge BYOK no té escriptori) i inclosos a `make deb`. Provat: instal·lat amb `apt` a GNOME 48. **Versió 2.0.1** (etiqueta `v2.0.1`).
- **2026-10-28** — **Recompte de paraules** a la barra d'estat, a petició de l'usuari: substitueix «L línia/total  C columna». Ara mostra, a la dreta, l'idioma del corrector (si està actiu), «N paraules» i les pàgines. Compta grups de caràcters sense espais que tinguin alguna lletra o xifra: «l'home», «i·l·l» o «**negreta**» són una paraula; els marcadors sols («#», «-», «—») no compten. `buf_words()` (`buffer.c`) desa el recompte de cada línia, com les pàgines, i només torna a comptar les línies modificades. Proves: 1.666 comprovacions; interactiva en un pseudoterminal (9 → 10 en escriure una paraula). **Pendent:** a la Pi.
- **2026-10-28** — **Temes de colors**, a petició de l'usuari (fase 1: papers i temes; fase 2: paleta de la consola).
  - **Papers:** a més dels colors de Markdown, `e->palette` té text, fons, barra, selecció, menús (primer pla i fons) i marcadors. Si la barra o la selecció no tenen cap color, van en vídeo invers, com abans; els menús, amb els colors del text. Les barres i la selecció del navegador de fitxers, l'ajuda i els menús fan servir `ui_attr()` (`tui.h`, a `render.c`) en lloc d'`A_REVERSE`. El fons s'aplica a tota la pantalla amb `bkgd()`.
  - **Temes** (`theme.c`): `terminal` (el d'abans, per defecte), `fosc`, `clar`, `sepia` i `fosfor`. Al `phileasrc`, `tema = NOM`; els `color-*` posteriors el retoquen (noves claus: `color-text`, `color-fons`, `color-barra`, `color-barra-fons`, `color-seleccio`, `color-seleccio-fons`, `color-menu`, `color-menu-fons`, `color-marcadors`). Colors 0–15: els 8 bàsics i els clars (`gris`, `vermell-clar`…); en un terminal de 8 colors, un color clar és el normal amb negreta. L'ordre `tema` (paleta o menú Vista → Tema) passa al següent, o `tema NOM`.
  - **Consola de Linux (fase 2):** amb `TERM=linux*` i `paleta-consola = si` (per defecte), els temes amb colors propis redefineixen els 16 colors de la consola (`ESC ] P`) i, en sortir o abans d'una ordre externa, els restauren (`ESC ] R`); en tornar d'una ordre externa, es tornen a aplicar. Els temes només fan servir fons de 0 a 7 (la consola no en té més) i les capçaleres, en negreta, surten dels colors 8–15 de la paleta del tema.
  - **Proves:** 1.759 comprovacions, també amb sanititzadors (tema, retocs posteriors, noms de colors, fons < 8 a tots els temes). Visuals en xterm amb 8 colors i la paleta rgb de cada tema aplicada amb OSC 4, que simula la consola de la Pi: text, Markdown, errors, selecció, barra i menús als cinc temes. Seqüències en un pseudoterminal amb `TERM=linux`: 16 colors en arrencar i `ESC ] R` en sortir; res amb `xterm`, amb el tema `terminal` ni amb `paleta-consola = no`; canvi en calent des del menú i des de la paleta. **Pendent:** a la Pi (consola real, també `lletra` i `gira` amb un tema actiu). Gravat a la SD i verificat per l'usuari a la Pi el mateix dia: correcte.
- **2026-10-28** — **El tema es desa** (observació de l'usuari a la Pi: el tema triat no quedava al `phileasrc`). L'ordre `tema` (menú Vista → Tema o paleta) ara el desa al fitxer de l'usuari (`$PHILEAS_CONFIG/phileasrc`; a la BYOK, `/data/phileas/phileasrc`) amb `config_store()`: canvia les línies `tema` que hi hagi o, si no n'hi ha cap, l'afegeix abans del primer `color-` (que el retoca) o al final; la resta del fitxer no es toca, i s'escriu en un fitxer nou que substitueix l'antic. Si no es pot desar, el missatge ho diu. Proves: 1.768 comprovacions, també amb sanititzadors (fitxer i directori nous, canvi d'una línia amb espais, comentaris intactes, abans dels colors, darrere dels colors); interactiva amb `TERM=linux`: menú → Sèpia, el fitxer queda amb `tema = sepia` i en tornar a obrir s'aplica la paleta; `tema fosc` des de la paleta el canvia.
- **2026-10-28** — **Ajuda (F1):** després del text inicial, on és la configuració pròpia (el camí real, segons `PHILEAS_CONFIG`; a la BYOK, `/data/phileas/phileasrc`) i l'ordre per crear-la amb totes les opcions, `phileas --config > CAMÍ`, amb l'avís que substitueix la que hi hagi. A la BYOK, el llançador `phileas` feia `clear` en acabar i n'hauria deixat els codis al final del fitxer: br2-phileas ara només fa `clear` si la sortida és un terminal.
- **2026-10-28** — Imatge gravada a la SD (BYOK Phileas 2.0, `phileas-v2.0-3-ge59efc1`) i provada per l'usuari a la Pi: tema desat al `phileasrc`, ajuda amb la configuració pròpia i `phileas --config`: **tot correcte**.
- **2026-10-28** — **La columna centrada no es conservava** (informat per l'usuari a la Pi). Causes: (1) Alt+C no s'havia desat mai, només durava la sessió; (2) la prova de `phileas --config > /data/phileas/phileasrc` havia substituït el fitxer de l'usuari pels valors de fàbrica de Phileas (`columnes = 72` en lloc de 80, `centrar = no`, i `idiomes = ca es`, que mana sobre el `es ca` de la BYOK). A més, amb aquesta redirecció el shell buida el fitxer abans que Phileas el llegeixi: mai no podria conservar res.
  - **Opcions de Vista desades:** `centrar` (Alt+C), `colors`, `maquina-escriure` i `tema` es desen al `phileasrc` de l'usuari amb `config_store()` en canviar-les (`remember()` a `command.c`).
  - **`desar-configuracio`** (paleta, i Esc → Ajuda i configuració → Desa la configuració): escriu el fitxer de l'usuari complet amb `config_template()` i els valors de `config_load_files()` (sistema + usuari, sense les opcions de la línia d'ordres ni l'estat de la sessió, com el diccionari triat amb F8), hi afegeix les línies `tecla.` i `externa` que tenia, i deixa l'anterior a `phileasrc.anterior`. L'ajuda (F1) ara recomana aquesta ordre en lloc de `--config`.
  - **`phileas --config`:** escriu els valors actuals (sistema + usuari) en lloc dels de fàbrica; els colors que difereixen del tema hi surten actius. L'ajuda d'ús avisa de no redirigir-lo al mateix fitxer.
  - **Proves:** 1.773 comprovacions, també amb sanititzadors (plantilla amb valors canviats que es torna a llegir igual, línies tecla./externa conservades, `.anterior`). Interactiva amb `TERM=linux` i `-a 0`: Alt+C i `tema fosc` queden al fitxer; `desar-configuracio` el deixa complet amb `autodesat = 30` (no el 0 de la línia d'ordres), sense `diccionari`, amb `tecla.f9` i la còpia anterior.
- **2026-10-29** — **Versió 2.0.2** (etiqueta `v2.0.2`): recompte de paraules, temes de colors, opcions de Vista desades, `desar-configuracio` i `--config` amb els valors actuals.
- **2026-10-29** — Provat per l'usuari a la Pi: opcions de Vista desades i `desar-configuracio`: **correcte**. Imatge BYOK etiquetada `phileas-v2.0.2`.
- **2026-10-29** — **Quant a Phileas** (Esc → Ajuda i configuració → Q, o `quant-a` a la paleta): finestra centrada amb la versió, la llicència i el repositori; a la BYOK hi afegeix la primera línia de `/etc/byok-version`. Es tanca amb qualsevol tecla, que no arriba al document. `PHILEAS_VERSION` passa a `src/version.h` (el Makefile la llegeix d'allà). Proves: 1.773 comprovacions; interactiva des del menú i des de la paleta, i amb un `/etc/byok-version` simulat amb bwrap.
- **2026-10-29** — **Tema WordPerfect** (`tema = wordperfect`, Vista → Tema → W): com el WordPerfect 5.1 per a DOS, text gris sobre blau amb la paleta CGA (a la consola de Linux, exacta); capçaleres en blanc, groc i cian brillants, codi verd clar, cites en cian, marques en gris fosc, errors en vermell clar; barra d'estat i menús grisos amb lletra negra i la selecció en blanc sobre negre. Proves: 1.790 comprovacions; captures en xterm amb la paleta CGA (text, capçaleres i menús oberts). Les lletres i els títols dels menús inactius surten en gris fosc (la negreta del negre), amb menys contrast.
- **2026-10-29** — **Cursor invisible amb els temes clar i sèpia a GNOME Terminal** (informat per l'usuari): el perfil té el cursor blanc fix i el color 7 (el fons d'aquests temes) també blanc. Ara, fora de la consola de Linux, el tema hi posa el seu color de text (`ESC ] 12`, p. ex. `#2A2A2A` al clar) i en sortir, suspendre o tornar al tema terminal es restaura el del terminal (`ESC ] 112`). La consola no ho rep (no ho entén i ja pinta el cursor amb el color de la cel·la). Proves: 1.790 comprovacions; seqüències comprovades en un pty (clar → sèpia → terminal, sortida, `TERM=linux` sense res); xterm amb cursor i color 7 blancs: abans invisible, ara fosc.
- **2026-10-29** — Cursor amb els temes clar i sèpia provat per l'usuari a GNOME Terminal: **correcte**.
- **2026-10-29** — **Versió 2.0.3** (etiqueta `v2.0.3`): Quant a Phileas, tema WordPerfect i color del cursor segons el tema.
- **2026-10-29** — **Innovacions per a l'escriptura creativa i acadèmica:** afegides com a millores candidates C7–C26 (esquema, moure seccions, projecte multifitxer, instantànies, pendents, enfocament, sprint, diari, ecos, tipografia de diàleg, calaix, notes al peu, citacions, YAML, límits per secció, llegibilitat, referències internes, sinònims i abreviatures), amb criteris de cost per a la Pi i una prioritat proposada. C4 marcada com a feta (recompte a la barra des de la 2.0.2).
- **2026-10-29** — **Prioritat de les innovacions** (decisió de l'usuari): C7 esquema, C8 moure seccions i paràgrafs, C12 mode enfocament, C17 tipografia de diàleg; la resta, a valorar. Anotades les implicacions de C17 per al Markdown desat i les condicions de la implementació.
- **2026-10-29** — **C7, esquema** (`esquema`, F2 o Alt+E, Esc → Cerca → Esquema): pantalla completa amb les capçaleres (`#` 1-6 i subratllades amb `===`/`---`; no les de dins del codi ni de les metadades), sagnades pel nivell, i a la dreta les paraules de cada secció, comptant-hi les subseccions però no la capçalera mateixa; si hi ha text abans de la primera, una entrada «(inici del document)». Comença a la secció del cursor; Enter hi salta (la capçalera queda a dalt o, amb la màquina d'escriure, a l'ancoratge); escrivint es filtra pel títol, i Esc buida el filtre o surt. En text pla o sense capçaleres, un missatge. Mòdul nou `outline.c` (sense ncurses, provat) i `outlineview.c` (diàleg); `buf_line_words()` exposa el recompte per línia amb cache. Es calcula només en obrir el diàleg: una passada pel document. Proves: 1.808 comprovacions, també amb sanititzadors (ATX amb tanca, setext, codi, títol buit, `#hashtag`, subseccions, `outline_at`, text pla); interactiva en xterm 80×24: F2, filtre, salt, Alt+E a la secció del cursor i menú Cerca, sense errors d'ASan. Al README, la fila «F2 barra d'ajuda inferior» (que no existia) ara és l'esquema.
- **2026-10-29** — Esquema (C7) provat per l'usuari a la Pi: **correcte**.
- **2026-10-29** — **C8, moure paràgrafs i seccions.** Al text, `pujar-paragraf` i `baixar-paragraf` (Alt+↑/↓, Esc → Edita → Puja/Baixa el paràgraf) intercanvien el paràgraf del cursor (una línia del buffer: paràgraf, element de llista, capçalera…) amb l'anterior o el següent; amb selecció, mouen totes les línies que toca (una selecció que acaba a l'inici d'una línia no la inclou) i la selecció passa a ser el bloc sencer, que es pot tornar a moure. A l'esquema, les mateixes tecles (les que tinguin assignades aquestes ordres) mouen la secció seleccionada amb les subseccions per sobre de l'anterior o per sota de la següent **del mateix nivell i dins de la mateixa secció pare**; el text d'abans de la primera capçalera i les línies buides del final no es mouen; en sortir amb Esc, el cursor queda a la secció moguda. Cada moviment és un pas de desfer (una esborrada i una inserció: `ed_swap_lines()` a `editor.c`; `outline_end()` i `outline_move()` a `outline.c`). A la consola de la BYOK, Alt+↑/↓ ja enviaven `\e[1;3A`/`B` (`seleccio.map`). Proves: 1.826 comprovacions, també amb sanititzadors (paràgraf amunt i al límit, desfer, línies seleccionades avall i amunt, seccions germanes, primera/última del nivell, primera dins el pare, secció amb subseccions, última secció amb línies buides al final); interactiva en xterm 80×24: Capítol 3 fins a dalt, Escena 2 per sobre d'Escena 1 i el límit, Esc, paràgraf avall i quatre Ctrl+Z fins al document original (sense [+]), sense errors d'ASan. Ajuda F1, menú i README al dia.
- **2026-10-29** — **C12, mode enfocament** (`enfocament = no | paragraf | frase`, ordre `enfocament` amb F4 o Esc → Vista → Enfocament, que passa al següent i es desa al phileasrc): fora del paràgraf (la línia del buffer) o de la frase del cursor, el text es pinta amb el color atenuat dels marcadors, sense estils ni errors ortogràfics; la selecció s'hi veu igual. La frase (`text_sentence()`, a `editor.c`) acaba darrere de `.`, `!`, `?` o `…`, amb els tancaments que els segueixin (`»`, `"`, `)`, `*`…), si després hi ha un espai o el final; «3.5» no la talla, però «Sr.» sí (no hi ha llista d'abreviatures). En escriure l'espai després del final d'una frase, la nova comença al cursor i tot el que hi ha abans s'atenua. Només afecta el pintat: un interval per pantalla.
- **2026-10-29** — **C17, tipografia en escriure** (`tipografia = si`, ordre `tipografia` a Esc → Edita → Tipografia automàtica; `apostrof-tipografic = si` per a l'apòstrof; totes dues desactivades per defecte). En escriure: `--` seguit d'un altre caràcter → «—» (no `---` ni `-->`), `<<` → «, `>>` → » (no a l'inici d'una cita, `>>`), `...` → «…» i, amb l'opció, `'` darrere d'una lletra → «’». Condicions de la decisió: (a) no actua al codi (blocs, i en línia: un nombre senar d'accents greus abans, perquè mentre s'escriu encara no hi ha el de tancament), metadades, HTML (excepte un paràgraf que comença amb `<<`), taules ni línies de guions; (b) cada substitució és un pas de desfer propi i el que s'escriu després va en un altre: Ctrl+Z just després torna a deixar el que s'havia escrit; (c) opció **única** i no per idioma: les regles del català i del castellà són les mateixes, i la rotació F8 no hi ha d'influir; (d) la cerca tracta `'` i `’` com el mateix caràcter, i el diccionari català (Softcatalà, també el de la BYOK) ja té `ICONV ’ '`: `l’home` és correcte (comprovat amb `hunspell -d ca`). Al fitxer queden els caràcters Unicode, que són Markdown normal.
  - **Proves:** 1.862 comprovacions, també amb sanititzadors (frases amb «!!», «.»», «3.5», «Sr.», cadena buida; interval d'enfocament en començar una frase; tipografia: diàleg, cometes, punts suspensius, apòstrof amb i sense l'opció, `---`, `a----b`, `<!-- -->`, cita niada, codi en línia, taula, bloc de codi, desfer/refer i el pas següent separat; cerca amb `'`/`’`; opcions i desar-configuracio). Interactiva en xterm 80×24: F4 paràgraf i frase, diàleg escrit amb tipografia, Ctrl+Z, cerca de «l'home» que troba «L’home», i també la de C8; sense errors d'ASan. br2-phileas: el phileasrc del sistema explica les opcions noves (comentades).
- **2026-10-29** — **Prova de l'usuari a la Pi (C8, C12, C17)**, dues observacions:
  - **L'enfocament depenia del tema** (bé amb terminal i sèpia; sense efecte amb fosc i clar; il·legible amb WordPerfect). Causa: a la consola (`TERM=linux`, `ncv` amb l'atenuat), ncurses descarta l'atenuat quan la cel·la té color, i els temes amb fons propi en tenen sempre; amb terminal, l'atenuat de la consola fa servir el color 8, i amb WordPerfect els marcadors són gris fosc sobre blau. Ara cada tema amb paleta de consola té un color per al text atenuat (`Theme.dim`): fosc, el 8 (més fosc, `#7a7c7a`); clar, el 3 (gris `#9a9a94`; el groc no el feia servir); sèpia i fòsfor, el 5; WordPerfect, el 5 (gris blavós `#7d7dd0` en lloc del magenta, que no feia servir). Fora de la consola continua l'atenuat del terminal (que sí que es combina amb els colors) i, si no n'hi ha, el color dels marcadors. Comprovat amb xterm i `TERM=linux` carregant la paleta de cada tema (OSC 4): els sis temes s'hi distingeixen i es llegeixen; i amb `TERM=xterm`, fosc i WordPerfect.
  - **`--` no feia el guió llarg** a l'inici d'un paràgraf seguit d'un espai («-- Hola»): mentre s'escriu, «-- » és un subratllat de capçalera per al Markdown (`MD_UNDERLINE`), on la tipografia no actuava. Ara hi actua: la regla del tercer guió ja protegeix `---`, i un subratllat `--` acaba amb Retorn, que no és un caràcter escrit. La lletra de la consola (Uni2-Terminus) té —, –, «, », … i ’.
  - Proves: 1.864 comprovacions, també amb sanititzadors (`-- Hola`, `--` i tabulador).
- **2026-10-29** — Provat per l'usuari a la Pi (imatge `5aa42044…`): esquema amb moviment de seccions, moure paràgrafs, enfocament amb tots els temes i tipografia: **tot correcte**.
- **2026-10-29** — **Versió 2.1** (etiqueta `v2.1`, `PHILEAS_VERSION` 2.1.0): esquema (C7), moure paràgrafs i seccions (C8), enfocament (C12) i tipografia en escriure (C17), provats a la Pi.
- **2026-10-29** — Release «Phileas 2.1» publicada a GitHub (Latest) amb el `.deb` i el `.tar.gz`; sha256 verificats a la pàgina de la release.
- **2026-10-29** — C13 (sprint) i C14 (mode «endavant»): **abandonades** per decisió de l'usuari.
- **2026-10-29** — **C2, posició del cursor, i C3, documents recents.**
  - **Sense rastres al document** (condició de l'usuari): la posició no s'escriu al fitxer que s'edita (ni comentaris, ni YAML, ni atributs estesos), sinó a `historial`, al directori de configuració (a la BYOK, `/data/phileas/historial`). Una línia per document, `LÍNIA BYTE CAMÍ-ABSOLUT`, el més recent primer, com a molt 100 (`HISTORY_MAX`). Es desa sencer en un `.nou` i es reanomena, amb `fsync`.
  - **Quan s'escriu:** en obrir un document (perquè passi al davant), en deixar-lo (obrir-ne un altre, `nou`, sortir) i en rebre SIGTERM/SIGHUP, després del desat d'emergència. No a cada tecla ni amb l'autodesat: poques escriptures a la SD. Només s'hi anoten els documents que existeixen al disc (el camí amb `realpath`); un fitxer nou sense desar no hi entra.
  - **C2:** en obrir un document que hi és, el cursor hi torna (l'ancoratge de la màquina d'escriure); si no hi és, al final, com abans. Si el fitxer s'ha escurçat fora de Phileas, la posició es retalla, i mai queda al mig d'un caràcter UTF-8. `recordar-posicio = no` torna al comportament anterior (la llista de recents continua).
  - **C3:** ordre `recents` (Alt+O, Esc → Fitxer → Recents…): pantalla completa amb el nom en negreta i el directori atenuat (amb `~`), sense el document obert ni els fitxers que ja no hi són (que també se'n van del disc). Escrivint es filtra pel camí; Enter l'obre (amb la confirmació de canvis sense desar de sempre); Supr el treu de la llista sense esborrar el fitxer; Esc buida el filtre o surt.
  - Mòdul nou `history.c` (sense ncurses, provat) i `recentview.c`; `open_path()` a `command.c` agrupa el que feia `c_open`. Ajuda F1, `--help`, plantilla del phileasrc i README al dia.
  - **Proves:** 1.889 comprovacions, també amb sanititzadors (touch sense duplicats, límit de 100, camins amb espais, línies mal formades o relatives, fitxers desapareguts, document sense nom o inexistent, restaurar, opció desactivada, posició dins d'un caràcter de dos bytes, fitxer escurçat, ordre de recents, `recordar-posicio` a la plantilla; el document no es modifica). Interactives en un pseudoterminal: obrir, moure's, sortir i tornar-hi (el text escrit va a la posició recordada), Alt+O amb la llista, Enter, filtre sense resultats, Supr, menú Fitxer, i SIGTERM (desa el document i l'historial, codi 143).
- **2026-10-29** — C2 i C3 provats per l'usuari a la Pi (imatge amb squashfs `a090f506…`): **tot correcte**.
- **2026-10-30** — Porta-retalls del sistema (D3, `694e42b`), fitxer nou sense extensió amb `.md` (`74bab18`) i notes al peu (C19, `91fc0bb`).
- **2026-10-30** — **Versió 2.1.1** (etiqueta `v2.1.1`, `PHILEAS_VERSION` 2.1.1): C2, C3, D3, `.md` per defecte i C19. C19, D3 i el `.md` encara no provats a la Pi.
- **2026-10-30** — Release «Phileas 2.1.1» publicada a GitHub (Latest) amb el `.deb` i el `.tar.gz`; sha256 verificats a la pàgina de la release.
- **2026-11-01** — **Línies de separació als menús**, a petició de l'usuari. A la taula de `src/menu.c`, `SEP` separa els grups; es dibuixa com una línia `├──┤` i les fletxes la salten (també en tornar a començar per l'altre extrem). Si un grup es queda buit (ordres que no existeixen), no surten línies al principi, al final ni dues de seguides. Grups: Fitxer (obrir/nou · desar · sortir), Edita (desfer · porta-retalls · selecció · moure paràgrafs · tipografia · notes al peu), Cerca (cerca i substitueix · esquema · desplaçament), Ortografia (marcar · revisar · idioma; «Alterna l'idioma» passa al grup de l'idioma, amb la mateixa lletra), Vista (opcions · tema · redibuixar), Tema (temes · el següent), Ajuda (ajuda i paleta · configuració · quant a), BYOK (lletra i gira · exporta i envia · externes desconegudes) i el principal (editor · BYOK · ajuda). **Proves:** 1.917 comprovacions; visuals en xterm amb tots els menús, BYOK amb totes les externes, amb una de desconeguda i amb només `gira`, i sense externes (el submenú no surt i no queden dues línies seguides). **Pendent:** a la Pi (caràcters `├ ┤` a la consola).
- **2026-11-03** — **Màquina d'escriure: un document nou comença a dalt**, a petició de l'usuari. Abans, en obrir un document buit (fitxer nou, «nou» o sense arguments), el cursor sortia a l'ancoratge (50 %) amb espai en blanc a sobre. Ara surt a la primera fila; s'escriu baixant i, en passar el límit (80 %), el text puja d'un salt i el cursor queda a l'ancoratge, com sempre. Un document amb text continua obrint amb el cursor a l'ancoratge. Canvi a `ed_init` i `ed_open` (`recenter` només si el document no és buit); en mode `fixa` no canvia res. **Proves:** 1.950 comprovacions (nova: document nou, 17 files sense moure's i salt a la 18a). **Pendent:** a la Pi.
- **2026-11-03** — Afinament, a petició de l'usuari: el document nou no comença enganxat a dalt, sinó amb **dues files en blanc** (`NEW_DOC_BLANK` a `editor.c`); el salt continua en passar el límit. **Proves:** 1.948 comprovacions. **Pendent:** a la Pi.
- **2026-11-03** — Les dues files en blanc del document nou també en mode **clàssic**, a petició de l'usuari (al PC el té en clàssic i no les veia). S'hi mantenen fins que el cursor arriba a baix, i llavors es gasten una a una abans que el text pugi; tornar a dalt no les recupera. **Proves:** 1.970 comprovacions i, amb el programa en un pty de 80×24, el cursor surt a la fila 3 en clàssic i en salt. Instal·lat al PC (`~/.local/bin`). **Pendent:** a la Pi.
