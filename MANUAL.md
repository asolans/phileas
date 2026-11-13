# Manual d'ús de Phileas

*Versió 2.7.0*

Aquest manual t'acompanya pas a pas: des d'obrir Phileas per primera vegada
fins a organitzar un llibre llarg. No cal llegir-lo de dalt a baix. Si només
vols escriure ja, llegeix el capítol 1 i torna aquí quan et faci falta alguna
cosa. L'ajuda de dins del programa (**F1**) és un llistat curt de tecles; això
és la guia amb explicacions i exemples.

## Índex

1. [Començar](#1-començar)
2. [Escriure: paràgrafs, llistes i capçaleres](#2-escriure-paràgrafs-llistes-i-capçaleres)
3. [Moure's pel text](#3-moures-pel-text)
4. [Seleccionar, copiar, tallar i desfer](#4-seleccionar-copiar-tallar-i-desfer)
5. [Obrir i desar fitxers](#5-obrir-i-desar-fitxers)
6. [Cercar i substituir](#6-cercar-i-substituir)
7. [Ortografia](#7-ortografia)
8. [Organitzar un text llarg: esquema i pendents](#8-organitzar-un-text-llarg-esquema-i-pendents)
9. [Notes al peu](#9-notes-al-peu)
10. [Tipografia en escriure](#10-tipografia-en-escriure)
11. [L'aspecte: temes, màquina d'escriure i enfocament](#11-laspecte-temes-màquina-descriure-i-enfocament)
12. [El calaix d'idees i Obsidian](#12-el-calaix-didees-i-obsidian)
13. [Configurar Phileas](#13-configurar-phileas)
14. [A la Raspberry Pi (BYOK)](#14-a-la-raspberry-pi-byok)
15. [Resolució de problemes](#15-resolució-de-problemes)
16. [Fàbula: organitzar un llibre](#16-fàbula-organitzar-un-llibre)
17. [Recerca: fitxes de recerca](#17-recerca-fitxes-de-recerca)
18. [Xuleta de dreceres](#18-xuleta-de-dreceres)

---

## 1. Començar

### Què és Phileas

Phileas és un editor de text per a la consola, pensat per **escriure prosa**
en Markdown: novel·la, articles, apunts, cartes. No és un editor de codi ni
un processador amb tipus de lletra i imatges. Només mostra el text, amb uns
pocs colors per distingir les capçaleres, la negreta i les cites, i el
desa com un fitxer de text normal que pots obrir amb qualsevol altre
programa.

### Obrir-lo

Des d'un terminal:

```bash
phileas                 # un document nou
phileas capitol1.md     # obre (o crea) un fitxer
phileas -c 66 nota.md   # amb la columna de text a 66 caràcters
phileas +40 capitol1.md # amb el cursor al paràgraf 40 (la primera línia és la 1)
```

Si l'has instal·lat amb el paquet `.deb`, també apareix entre les aplicacions
de l'escriptori.

### Les tres tecles que cal recordar

| Tecla | Què fa |
|-------|--------|
| **Esc** | obre els menús (tot el que Phileas sap fer hi és) |
| **F1** | l'ajuda, amb les tecles que hi ha assignades de debò |
| **Ctrl+P** | la *paleta d'ordres*: escrius el nom d'una ordre i l'executes |

Si et perds, prem **Esc** i navega pels menús: la lletra subratllada o
Retorn tria, **←** o **Esc** torna enrere. Cada ordre té un nom (per exemple
`esquema` o `pendents`) i, si no recordes la tecla, sempre pots escriure'n el
nom a la paleta; amb **Tab** es completa.

### Primer document, en un minut

1. Obre Phileas: `phileas dietari.md`.
2. Escriu: `# Dietari de viatge` i prem **Retorn**.
3. Escriu un parell de línies. No cal que premis Retorn al final de cada
   línia: el text s'ajusta sol a l'amplada de la columna.
4. Prem **Ctrl+S** per desar.
5. Prem **Ctrl+Q** per sortir.

Si oblides desar, no passa res greu: Phileas **desa sol** (vegeu el
[capítol 5](#5-obrir-i-desar-fitxers)).

### La barra d'estat

A la part de baix de la pantalla hi ha la barra d'estat. A l'esquerra hi surt
el nom del fitxer; a la dreta, de dreta a esquerra, el nombre de pàgines
(una estimació), el de paraules, l'idioma del corrector (si està actiu) i el
mode de desplaçament (`salt`, `fixa` o `clàssic`, vegeu el
[capítol 11](#11-laspecte-temes-màquina-descriure-i-enfocament)). Els
missatges de Phileas («1 substitució», «Desat»…) hi surten un moment.

---

## 2. Escriure: paràgrafs, llistes i capçaleres

### Un paràgraf és una línia

A Phileas, **Retorn separa paràgrafs**, no línies. Escrius sense preocupar-te
de les vores de la pantalla; el text s'ajusta només. Al fitxer, cada paràgraf
queda com una sola línia llarga, i entre un paràgraf i el següent hi ha una
línia en blanc. Aquest és el Markdown net que entén qualsevol programa.

Per separar visualment dos paràgrafs, Phileas deixa una fila buida a la
pantalla; tu no has d'escriure cap línia en blanc.

> **Si obres un text tallat a mà** (cada línia acabada amb un salt, com els
> correus antics), Phileas n'ajunta les línies en obrir-lo perquè siguin
> paràgrafs de debò.

### Capçaleres

Una capçalera comença amb `#`. Quants més `#`, més petita:

```markdown
# Títol del llibre
## Capítol 1
### Una escena
```

Les capçaleres es mostren amb color. A més, són la base de l'**esquema**
(F2, [capítol 8](#8-organitzar-un-text-llarg-esquema-i-pendents)): si les
fas servir, podràs saltar d'un capítol a un altre i reordenar-los.

### Negreta, cursiva i codi

```markdown
Aquesta paraula és *cursiva* i aquesta altra **negreta**.
Una ordre dins del text: `make test`.
```

Phileas no les previsualitza: veus els asteriscs, atenuats, i el text amb
l'estil corresponent.

### Llistes

Escriu `- ` (guió i espai) i el text. En prémer **Retorn**, Phileas continua
la llista tot sol amb un guió nou. Per acabar-la, prem Retorn una altra vegada
en una línia buida.

```markdown
- Comprar paper
- Revisar el capítol 3
- Trucar a l'editor
```

Les llistes numerades (`1. `) funcionen igual i s'hi incrementa el número.
També les llistes de tasques (`- [ ] `, `- [x] `) i les cites (`> `).

### Separador centrat

Una línia que només conté `***` (s'hi toleren espais davant i darrere) es mostra **centrada** a la pantalla, com els
separadors d'escenes d'un llibre. És només una vista: al fitxer continua sent
`***`.

---

## 3. Moure's pel text

Les fletxes, **Inici**, **Fi**, **Re Pàg** i **Av Pàg** fan el que esperes.
Per anar més lluny:

| Vols anar… | Tecla |
|------------|-------|
| a la paraula anterior / següent | **Ctrl+←** / **Ctrl+→** (o **Alt+B** / **Alt+F**) |
| al paràgraf anterior / següent | **Ctrl+↑** / **Ctrl+↓** (o **Alt+P** / **Alt+N**) |
| a l'inici / al final del document | **Ctrl+Inici** / **Ctrl+Fi** (o **Alt+<** / **Alt+>**) |
| a una línia concreta | **Alt+L**, escrius el número |
| a un capítol | **F2** (esquema) |

> A la consola de Linux (la pantalla negra de la Raspberry Pi, sense escriptori)
> moltes combinacions amb Ctrl i les fletxes no arriben al programa. Per això
> gairebé totes les ordres tenen una alternativa amb **Alt+lletra**.

### Esborrar

| Tecla | Efecte |
|-------|--------|
| **Esborrar** (retrocés) | el caràcter anterior |
| **Supr** | el caràcter següent |
| **Ctrl+W** | la paraula anterior |
| **Alt+D** | la paraula següent |

---

## 4. Seleccionar, copiar, tallar i desfer

### Seleccionar

- **Maj+fletxes** estén la selecció caràcter a caràcter o línia a línia.
- **Maj+Ctrl+fletxes** la fa per paraules o paràgrafs.
- **Ctrl+A** selecciona tot el document.
- **Ctrl+Espai** comença una selecció sense mantenir res premut: mous el
  cursor amb les fletxes i el text queda seleccionat. Va molt bé a la consola
  on Maj+Ctrl+fletxes no arriben. **Esc** cancel·la la selecció.

### Copiar, tallar i enganxar

**Ctrl+C** copia, **Ctrl+X** talla i **Ctrl+V** enganxa. A l'escriptori, el
porta-retalls és **el del sistema**: pots copiar un paràgraf des del navegador i
enganxar-lo a Phileas, o a l'inrevés. (Cal `wl-clipboard`, `xclip` o `xsel`; el
paquet `.deb` ja ho recomana.) A la consola de la Pi, Phileas fa servir el seu
porta-retalls intern.

### Desfer i refer

**Ctrl+Z** desfà l'última acció i **Ctrl+Y** la refà. L'historial és llarg
(fins a 8 MB): pots desfer molt enrere. Les pulsacions consecutives s'agrupen,
de manera que un Ctrl+Z desfà una paraula o una frase, no una lletra.

### Moure un paràgraf

Poses el cursor dins d'un paràgraf i prems **Alt+↑** o **Alt+↓**: el paràgraf
puja o baixa per sobre de l'anterior o el següent. També funciona amb diversos
paràgrafs seleccionats.

*Exemple:* tens els punts d'una llista de motius en un ordre que no et
convenç. Poses el cursor al tercer, prems **Alt+↑** dues vegades i ja és el
primer. Si no t'agrada, **Ctrl+Z**.

---

## 5. Obrir i desar fitxers

### Obrir

**Ctrl+O** (o Esc → Fitxer → Obre) mostra el navegador de fitxers:

- **↑ ↓** per triar, **Retorn** per obrir un fitxer o entrar a una carpeta.
- **←** puja a la carpeta de dalt.
- Escriu unes lletres i la llista es **filtra**.
- Per crear un fitxer nou, escriu-ne el nom. Si no hi poses extensió,
  Phileas hi afegeix `.md`: `nota` es converteix en `nota.md`.

Phileas edita **un sol document alhora**. Per canviar de document, torna a
obrir-ne un altre (si hi ha canvis, desa'ls primer).

### Documents recents

**Alt+O** mostra els últims documents que has obert. **Retorn** n'obre un, i
**Supr** el treu de la llista (no esborra el fitxer). Quan tornes a obrir un
document, **el cursor és on era**: no cal buscar el lloc on havies deixat
d'escriure.

Aquesta memòria es guarda a un fitxer a part (`historial`, al directori de
configuració), mai dins del teu document. Si no la vols: `recordar-posicio =
no` al `phileasrc`.

### Desar

| Tecla | Efecte |
|-------|--------|
| **Ctrl+S** | desa |
| **Alt+S** | desa amb un altre nom (*Desa com*) |
| **Ctrl+Q** | surt; si hi ha canvis sense desar, pregunta |

**Autodesat.** Si deixes d'escriure 30 segons, Phileas desa sol; i, amb canvis
pendents, desa com a molt cada 5 minuts. A més, si es tanca el terminal o
s'apaga l'ordinador de manera ordenada, desa abans de sortir. Si el
document no té nom i s'ha de desar d'emergència, el deixa en un fitxer
`rescat-DATA-HORA.md` (al directori actual o al de configuració).

El desat és **atòmic**: Phileas escriu un fitxer temporal i el canvia pel
definitiu, de manera que un tall de corrent no et deixa el document a mitges.

### Fitxers que no són Markdown

Phileas només interpreta com a Markdown els fitxers `.md`, `.markdown` i
`.txt`. Els altres (un `.conf`, un `.csv`…) s'obren com a **text pla** i no
es modifiquen: ni s'hi ajunten línies ni s'hi apliquen colors. Així pots
obrir-ne qualsevol per mirar-lo sense fer-hi malbé res.

---

## 6. Cercar i substituir

### Cercar

**Ctrl+F** obre el camp de cerca. Escrius el text i Phileas va saltant a la
coincidència **mentre escrius**. Retorn l'accepta.

- **Ctrl+G** (o **F3**) salta a la coincidència següent.
- **Alt+G** (o **Maj+F3**) a l'anterior.
- La cerca **no distingeix majúscules** (si no n'escrius cap) **ni accents**:
  `cafe` troba «cafè» i «Cafè», i `ca` troba també «ÇA». Si escrius l'accent
  (`cafè`) o una majúscula (`Cafe`), la cerca és exacta.

### Substituir

**Ctrl+R** demana primer què cerques i després per què ho vols canviar.
Després, a cada coincidència et pregunta:

```
Substituir? (s)í, (n)o, (t)ots, Esc: acabar
```

- **s** canvia aquesta i continua;
- **n** la deixa i continua;
- **t** canvia **totes** les que queden;
- **Esc** acaba.

Al final, la barra diu «3 substitucions» (o les que siguin). Si t'has
equivocat, **Ctrl+Z**.

*Exemple:* el personatge s'ha de dir «Elisenda» i no «Elisabet». **Ctrl+R**,
escrius `Elisabet`, Retorn, escrius `Elisenda`, Retorn, i després **t**.

---

## 7. Ortografia

Phileas té **correcció ortogràfica instantània** en català i castellà (amb
hunspell). Si no està instal·lat el motor, funciona igual, però sense
correcció (vegeu el [capítol 15](#15-resolució-de-problemes)).

### Activar-la

**F7** activa o desactiva la correcció. Les paraules que no coneix es marquen
**en vermell i subratllades** (la que estàs escrivint no es marca fins que
l'acabes). Per tenir-la sempre activa: `ortografia = si` al `phileasrc`.

### Canviar d'idioma

**F8** alterna entre els idiomes de la llista (per defecte, català i
castellà). L'idioma actual surt a la barra d'estat (`ca` o `es`). Per triar
un diccionari concret (per exemple el valencià), usa Esc → Ortografia → Tria
el diccionari.

### Revisar errors i suggeriments

- **Maj+F7** salta a **l'error següent**.
- **Alt+Retorn** (o Esc → Ortografia → Suggeriments) mostra els suggeriments
  per a la paraula del cursor, numerats.

En el quadre de suggeriments:

- el **número** substitueix la paraula pel suggeriment triat;
- **a** afegeix la paraula al teu diccionari personal (no tornarà a marcar-se);
- **p** l'afegeix al **diccionari del projecte** (vegeu més avall);
- **i** l'ignora fins que tanquis Phileas;
- **Esc** surt sense fer res.

*Exemple:* has escrit «presiciò». Poses el cursor a sobre, prems
**Alt+Retorn**, veus «1 precisió, 2 pressió…» i prems **1**.

### Diccionari del projecte

Els noms propis d'una novel·la (personatges, llocs inventats) no han de
contaminar el teu diccionari personal. Amb **p** al quadre de suggeriments, la
paraula es desa a un fitxer **`.diccionari.txt`** (una paraula per línia) que
serveix per a **tots els idiomes** i només per als documents d'aquell
projecte: Phileas el busca a la carpeta del document i a les de sobre (fins a
sis nivells). Si encara no n'hi ha, el crea a l'arrel del projecte de Fàbula
(la carpeta amb `projecte.md`) o, si no és un projecte, a la carpeta del
document. És un fitxer de text normal: es pot editar, copiar i `envia` el porta.

### Aparellaments (Esc → Ortografia → Aparellaments)

L'ordre **`revisar-aparellaments`** va al **paràgraf següent** amb un signe
sense tancar o mal tancat: `«` `»`, `(` `)`, `[` `]`, `“` `”`, les cometes
rectes, i el **guió de diàleg** (`—Hola —va dir.` té una acotació oberta i no
tancada). Cada paràgraf s'ha de tancar tot sol. No revisa el codi, ni els
numerals de llista (`1)`), ni els emoticones (`:)`). Quan no en queda cap, ho
diu. Es repeteix per anar al problema següent.

---

## 8. Organitzar un text llarg: esquema i pendents

### L'esquema (F2)

Quan un document passa de les poques pàgines, saber on ets és el més difícil.
Prem **F2** (o **Alt+E**) i veuràs totes les **capçaleres** del document,
sagnades segons el nivell, cadascuna amb el **nombre de paraules** de la
secció:

```
Esquema: novella.md · 41.230 paraules

(inici del document)                         120
Part primera                                9.870
  Capítol 1                                 3.120
  Capítol 2                                 2.900
Part segona                               11.400
```

Què hi pots fer:

- **↑ ↓** per moure't, **Retorn** per anar-hi: la capçalera queda a dalt de la
  pantalla.
- **Escriure** per filtrar (només surten les capçaleres que contenen el text).
  **Esc** esborra el filtre i, una segona vegada, tanca l'esquema.
- **Alt+↑ / Alt+↓** **mou la secció sencera**, amb totes les subseccions,
  per sobre o per sota de la germana del seu nivell. No sortiràs del capítol
  pare. És la manera ràpida de reordenar un llibre; **Ctrl+Z** ho desfà.

*Exemple:* el capítol 2 hauria de ser abans que el 1. **F2**, et poses sobre
«Capítol 2», **Alt+↑**, **Esc**. Fet: el text sencer del capítol 2 ha pujat.

Si el document no té cap capçalera, Phileas t'ho diu. Els fitxers de text
pla no tenen esquema.

### Els pendents (Alt+K)

Mentre escrius, moltes vegades et falta una dada o saps que has de tornar
a una part. En lloc d'aturar-te, deixa un **marcador** i continua. Phileas en
reconeix tres:

| Marcador | Per a què serveix |
|----------|-------------------|
| `TODO` | una cosa per fer |
| `[?]` | un dubte, una dada per comprovar |
| `%%comentari%%` | una nota per a tu (entre dos `%%`) |

Es ressalten **en vídeo invers** (lletres fosques sobre fons clar), de
manera que els veus de lluny. Exemples:

```markdown
La batalla va ser el 1714 [?] i va acabar a la tardor.

TODO: començar el capítol amb una escena més viva.

Va arribar a la ciutat %%comprovar el nom del carrer%% de nit.
```

Quan vulguis revisar-los, prem **Alt+K** (o Esc → Cerca → Pendents, o
Ctrl+P i `pendents`). Surt una llista amb el **número de línia** i el text:

```
Pendents: novella.md · 3

     2  La batalla va ser el 1714 [?] i va acabar a la tardor.
     4  TODO: començar el capítol amb una escena més viva.
     6  Va arribar a la ciutat %%comprovar el nom del carrer%% de nit.
```

**↑ ↓** per triar, **Retorn** per anar-hi, **escriure** per filtrar (només
surten les línies que contenen el text) i **Esc** per tancar. La llista
comença al primer pendent des d'on tens el cursor.

Coses que cal saber:

- `TODO` ha de ser una **paraula sola i en majúscules**: `TODO:` i `(TODO)`
  compten; `TODOS`, `MYTODO` i `todo` no.
- `%%comentari%%` ha de tancar-se a la mateixa línia.
- Dins del **codi** (en línia amb accents greus, en blocs o sagnat) i de les
  metadades YAML no compten, perquè hi surten amb un altre sentit.
- Una barra invertida davant (`\[?]`) fa que no es consideri marcador.
- Els marcadors són **text normal** del fitxer. Per treure'ls, esborra'ls.
  Cap programa extern els ha d'entendre.

*Truc:* abans de lliurar un text, fes **Alt+K**. Si diu «No hi ha pendents»,
pots enviar-lo tranquil.

---

## 9. Notes al peu

Phileas fa servir la sintaxi de notes al peu de *pandoc*.

1. Poses el cursor on vols la crida i prems **F5**. Phileas escriu `[^1]` al
   text i afegeix al final del document la definició `[^1]: `, deixant-hi el
   cursor perquè escriguis la nota.
2. Escrius la nota. Quan acabis, **F6** et **torna** al lloc de la crida.
3. **F6** sobre una crida `[^1]` **va** a la seva nota.

```markdown
El tractat es va signar l'any següent.[^1]

[^1]: Segons l'acta conservada a l'arxiu municipal.
```

Si afegeixes o treus notes pel mig, els números queden desordenats.
**Ctrl+P** → `renumerar-notes` les torna a numerar 1, 2, 3… en l'ordre del
text i ordena les definicions. Les notes amb nom (com `[^font]`) es queden
tal com són. Les marques dins del codi no compten.

---

## 10. Tipografia en escriure

Si ho actives, Phileas converteix mentre escrius:

| Escrius | Obtens |
|---------|--------|
| `--` seguit d'un altre caràcter | **—** (guió llarg de diàleg) |
| `<<` i `>>` | **«** i **»** |
| `...` | **…** |
| `'` entre lletres (opció a part) | **’** |

*Exemple de diàleg:* escrius `-- On vas? -- va preguntar.` i obtens
`— On vas? — va preguntar.`

- **Català:** amb la tipografia activa i el diccionari català, `col.legi` es
  converteix en **`col·legi`** en escriure la segona `l` (també `il.lusió` i
  `IL.LUSIÓ`; no en castellà, ni dins del codi, ni si el punt va després d'un
  espai). Si no vols activar la tipografia, **Alt+.** insereix el punt volat `·`.
- Activa-la o desactiva-la amb **Alt+T** (o Esc → Edita → Tipografia
  automàtica). La primera opció és `tipografia = si` al `phileasrc`; l'apòstrof
  tipogràfic és `apostrof-tipografic = si`.
- **Ctrl+Z** just després d'una conversió **la desfà** i et deixa el `--`.
- No actua dins del codi ni a les línies fetes només de guions (`---`).
- Al fitxer queden els caràcters Unicode, que són text normal i es veuen igual
  a tot arreu.

### Inserir un caràcter pel nom (Esc → Edita → Insereix un caràcter)

Per als teclats que no tenen un caràcter, **`unicode`** (Ctrl+P, o el menú)
l'insereix pel nom curt o pel codi hexadecimal:

| Nom | Caràcter | Nom | Caràcter |
|-----|----------|-----|----------|
| `punt-volat` | · | `guio-llarg` | — |
| `guio-mitja` | – | `punts` | … |
| `obre` / `tanca` | « » | `cometa-obre` / `cometa-tanca` | “ ” |
| `apostrof` | ’ | `c-majuscula` / `c-minuscula` | Ç ç |
| `ela-geminada` | ŀ | `grau`, `euro`, `lliura` | ° € £ |
| `copyright`, `seccio`, `paragraf` | © § ¶ | `fletxa`, `multiplica` | → × |

També `unicode 2014`, `unicode U+00B7` o `unicode 0xab`. Sense argument, en
demana el nom. **Alt+.** és la drecera del punt volat.

### Estadístiques (Esc → Cerca → Estadístiques)

L'ordre **`estadistiques`** mostra, del document sencer o de la **selecció**:
paraules, caràcters (amb i sense espais), frases, paràgrafs, **pàgines
estàndard** (1.800 caràcters amb espais) i **temps de lectura** (200 paraules
per minut). Les frases són aproximades (un títol no compta com a frase).

---

## 11. L'aspecte: temes, màquina d'escriure i enfocament

### Temes de colors

Esc → **Vista** → **Tema** (o l'ordre `tema`):

| Tema | Aspecte |
|------|---------|
| Terminal | els colors del teu terminal (el per defecte) |
| Fosc | text clar sobre fons negre |
| Clar | text fosc sobre fons blanc |
| Sèpia | color de paper antic |
| Fòsfor | verd sobre negre, com els monitors d'abans |
| WordPerfect | gris sobre blau, com el WordPerfect 5.1 per a DOS |

El tema triat **es desa** i es manté la vegada següent. A la consola de Linux,
cada tema canvia la paleta de la consola i, en sortir, torna a deixar els
colors originals. Per mantenir els de la consola: `paleta-consola = no`.

### Màquina d'escriure (F10)

Perquè els ulls no vagin sempre cap a baix de la pantalla, Phileas pot fer que
**la línia on escrius es quedi a una alçada fixa**. Hi ha tres modes; **F10**
(o **Alt+M**) passa de l'un a l'altre i la barra d'estat diu quin tens:

- **salt:** escrius baixant fins al 80 % de la pantalla; llavors el text puja
  d'un salt i el cursor torna al 50 %.
- **fixa:** el cursor es queda sempre al 50 % i és el text el que es mou.
- **clàssic:** el cursor baixa fins al final de la pantalla, com en un
  editor de sempre.

Un document nou comença amb dues files en blanc a dalt, perquè no escriguis
enganxat a la vora. Es pot ajustar amb `maquina-escriure-limit` i
`maquina-escriure-ancoratge`.

### Enfocament (F4)

Atenua tot el text **menys el paràgraf o la frase** on ets. És per concentrar-te
en el que escrius ara, sense tenir els paràgrafs de dalt al davant. **F4**
passa per tres estats: paràgraf, frase i desactivat. No canvia el fitxer, només
com es veu.

### Columna i marges

La columna de text fa 80 caràcters per defecte (`-c` o `columnes =` per
canviar-ho), amb marges a esquerra i dreta i centrada a la pantalla. **Alt+C**
centra o no la columna.

---

## 12. El calaix d'idees i Obsidian

### Què és

Mentre escrius pot venir-te una idea que no encaixa en el que fas, o trobes una
cita bona en un llibre. El **calaix** és un fitxer de text (`calaix.md`) on
guardes aquestes idees **sense deixar el que estàs fent**.

### Afegir una idea (F9)

1. Prem **F9**. Si tens un bloc **seleccionat**, aquesta és la idea; si no,
   Phileas et demana el text d'una idea nova.
2. Si hi ha una **font activa** (un llibre o un article), et demana la pàgina.
3. Et demana un **títol** (opcional, pots deixar-lo en blanc) i les
   **etiquetes**.

El resultat al calaix té aquest format:

```markdown
## Prous, 1935 · p. 142 · #personatge #final
### Una fi amarga

El bloc de text que has seleccionat o escrit.

∙ 2026-11-10 · capitol3.md:42
```

El peu et diu **de quin document i línia** ve la idea.

### La font activa

La *font* és el llibre o article d'on treus les cites. Quan en tries una, es
**recorda** entre sessions i es proposa a cada idea fins que la canvies:

- Esc → **Idees** → **Font del calaix** (o `calaix-font`): un camp amb la font
  actual. Escriu-hi per canviar-la; esborra-la per treure-la.
- La font activa surt a la barra d'estat.

### Consultar el calaix

Esc → **Idees** → **Calaix d'idees** (o `calaix`): llista totes les idees, es
poden **filtrar** escrivint (una etiqueta, una paraula…) i Retorn t'hi porta.

### Obsidian

Si fas servir *Obsidian*, Phileas pot portar-hi les idees:

1. Esc → Idees → **Vault d'Obsidian…**: tries la carpeta del teu vault (amb
   **Tab** tries la carpeta on ets).
2. Esc → Idees → **Importa a Obsidian**: crea una nota per idea a la carpeta
   d'entrada (`Inbox-byok`). Una idea que ja hi és no es duplica.

Si tens una Pi amb Phileas, **Calaix de la Pi…** diu on és el calaix que arriba
amb l'ordre `envia`. Així el calaix de la Pi també s'importa al vault.

---

## 13. Configurar Phileas

### El fitxer phileasrc

Totes les opcions són en un fitxer de text, `~/.config/phileas/phileasrc`.
Hi ha una opció per línia (`clau = valor`); les línies amb `#` són comentaris.
Només cal deixar-hi **el que vols canviar**. Un exemple:

```ini
columnes = 66
marge-esquerre = 6
ortografia = si
idiomes = ca es
tema = sepia
tipografia = si
autodesat = 20
```

Les opcions que canvies des dels menús (tema, màquina d'escriure, enfocament,
tipografia, columna centrada) **es desen soles**. Per tenir el fitxer complet,
amb comentaris i totes les opcions: **Ctrl+P** → `desar-configuracio`. L'anterior
es guarda a `phileasrc.anterior`. Si edites el fitxer a mà amb Phileas
obert, **Ctrl+P** → `recarregar-configuracio` el torna a llegir.

### Canviar una tecla

```ini
tecla.f10 = enfocament        # F10 passa a fer l'enfocament
tecla.ctrl-t = maquina-escriure
```

Els noms de tecla són `ctrl-a`…`ctrl-z`, `alt-LLETRA`, `f1`…`f12`,
amb `shift-`, `ctrl-` o `alt-` davant. Els noms de les ordres els veus a la
paleta (Ctrl+P) i a l'ajuda (F1).

### Colors

Pots retocar els del tema:

```ini
tema = fosc
color-capcalera-1 = groc
color-cita = cian
```

Valors: `negre`, `vermell`, `verd`, `groc`, `blau`, `magenta`, `cian`, `blanc`,
les versions clares (`vermell-clar`…), `gris` o `cap` (el del terminal).

### Ordres externes

Phileas pot cridar programes del sistema des de la paleta:

```ini
externa-breu.paraules = wc -w
externa.terminal = sh
```

- `externa-breu.NOM`: executa l'ordre i mostra la **darrera línia** de la
  sortida a la barra d'estat. A la paleta: `paraules capitol1.md`.
- `externa.NOM`: deixa la pantalla a l'ordre fins que acaba; després torna
  a Phileas quan prems Retorn. Va bé per obrir un shell o executar `pandoc`.
- `externa-pantalla.NOM`: per a un programa de **pantalla sencera** (com
  Recerca o Fàbula): en sortir-ne, torna directament a Phileas, sense Retorn.

```ini
externa-pantalla.recerca = recerca ~/Documents/Recerca
```

Abans d'executar-la, Phileas **desa el document** si té canvis.

---

## 14. A la Raspberry Pi (BYOK)

La imatge **BYOK Phileas** converteix una Raspberry Pi 1B en un dispositiu
d'escriptura: **arrenca directament a Phileas**, en uns 8 segons, i només hi
connectes un teclat. Els textos es desen a una partició a part (`/data/textos`).
El sistema és de només lectura: si es talla el corrent, no es fa malbé.

### Els textos

Phileas s'obre al directori `/data/textos`. Hi crees carpetes i fitxers
amb el navegador (Ctrl+O). En sortir de Phileas, el dispositiu fa `sync` i
neteja la pantalla.

### El menú BYOK

Esc → **BYOK** (només a la Pi):

| Opció | Què fa |
|-------|--------|
| Lletra més gran / més petita | canvia la mida de la lletra de la pantalla |
| Gira la pantalla | gira la imatge (útil si el monitor està en vertical) |
| Exporta al pendrive | copia els textos a un pendrive USB |
| Envia a un ordinador | copia els textos, per la xarxa, a un ordinador Linux |
| Recerca | obre Recerca amb els vaults de recerca de la Pi ([capítol 17](#17-recerca-fitxes-de-recerca)); en sortir, torna a Phileas |
| Rep la recerca del PC | porta un vault d'Obsidian del PC a la Pi i hi torna les fitxes escrites a la Pi |

També són ordres de la paleta: `lletra`, `gira`, `exporta`, `envia`, `recerca`, `rep`.

### Passar els textos a l'ordinador

- **Amb un pendrive:** endolla'l, Esc → BYOK → Exporta al pendrive, i
  desendolla'l quan acabi.
- **Per la xarxa:** Esc → BYOK → Envia a un ordinador. Es mostra un menú
  amb els ordinadors aparellats; els textos arriben a `~/BYOK` de l'ordinador.

### Apagar el dispositiu

Surt de Phileas amb **Ctrl+Q** (desa abans). El dispositiu fa `sync`. Després,
ja pots desendollar-lo: el sistema de només lectura no pateix.

Si la Pi s'apaga de manera ordenada amb Phileas obert, Phileas **desa el
document abans de sortir**. Un tall brusc de corrent es limita a perdre el
que has escrit des de l'últim autodesat.

---

## 15. Resolució de problemes

**No es veuen bé els accents o la ç.**
Phileas necessita una configuració regional UTF-8. Comprova-ho amb
`locale`; hauria de dir `…UTF-8`. A la Raspberry Pi OS, `sudo raspi-config` →
Localisation Options → Locale.

**L'ortografia no funciona.**
Falta `libhunspell` o els diccionaris. A Debian i Ubuntu:

```bash
sudo apt install libhunspell-1.7-0 hunspell-ca hunspell-es
```

**Una tecla no fa res a la consola de Linux.**
Les combinacions amb Ctrl o Maj i fletxes no arriben a la consola. Fes servir
l'alternativa amb Alt (a la taula del [capítol 18](#18-xuleta-de-dreceres)) o
**Esc** per als menús.

**Ctrl+C o Ctrl+S no fan el que dic.**
Phileas treballa en mode *raw*: Ctrl+S és «desa», Ctrl+Q és «surt», Ctrl+C és
«copia». Si el teu terminal captura alguna de les combinacions, fes servir la
paleta (Ctrl+P) o els menús (Esc).

**No puc copiar des d'altres aplicacions.**
A l'escriptori, instal·la `wl-clipboard` (Wayland) o `xclip` (X11).

**Vull recuperar un text que he esborrat per error.**
**Ctrl+Z** repetidament. L'historial és llarg i només es perd en tancar el
document.

**La barra d'estat avisa d'un error de configuració.**
Una línia del `phileasrc` no s'entén. S'ignora i Phileas arrenca igualment;
revisa-la. A més, **Ctrl+P** →
`recarregar-configuracio` la torna a llegir.

**S'ha tancat el terminal amb un document obert.**
No es perd: Phileas desa abans de sortir quan rep el tancament. Si el
document no tenia nom, busca un fitxer `rescat-…md` al directori actual.

**Un text no s'obre com a Markdown.**
Només `.md`, `.markdown` i `.txt` s'interpreten; la resta és text pla. Desa-lo
amb l'extensió `.md` per tenir-hi tots els avantatges.

---

## 16. Fàbula: organitzar un llibre

Phileas serveix per escriure. **Fàbula** és l'eina companya per tenir a la vista
tota la novel·la: els capítols i les escenes, els personatges, els llocs i la
recerca. No és un editor: quan vols escriure, obre el text a Phileas i, en
sortir-ne, torna a Fàbula. També és de consola i corre a la Raspberry Pi.

### Un projecte és una carpeta

Cada projecte és una carpeta de fitxers Markdown normals, que pots obrir amb
qualsevol editor o amb Obsidian:

```
la-casa-del-pou/
  projecte.md        títol, objectiu de paraules, data límit i estats
  manuscrit/         un fitxer per capítol: 01-comencament.md, 02-el-pou.md…
  personatges/       una fitxa per personatge
  llocs/  recerca/  notes/
  plantilles/        (opcional) les teves plantilles de fitxa
  .paperera/         el que retires; mai no s'esborra
```

Dins d'un capítol, **cada escena és una secció `###`**. Per això Phileas ja la
mou (Alt+↑/↓ a l'esquema, F2) i la compta. Un capítol i una fitxa poden
començar amb una capçalera YAML:

```yaml
---
estat: revisat
sinopsi: L'Anna arriba al mas i troba el pou tapat.
---
```

`projecte.md` té aquests camps:

| Camp | Què és |
|------|--------|
| `titol` | el nom del projecte |
| `objectiu` | paraules que ha de tenir el manuscrit (0 = sense objectiu) |
| `data-limit` | `AAAA-MM-DD`; Fàbula en treu les paraules diàries que et calen |
| `estats` | la llista d'estats, p. ex. `[esborrany, revisat, final]` |

### Començar

```bash
fabula nou "La casa del pou"      # crea la carpeta i l'obre
fabula la-casa-del-pou            # obre un projecte existent
fabula                            # obre el de la carpeta actual
fabula --resum la-casa-del-pou    # el resum en text, sense pantalla
```

Fàbula agafa el tema (`tema =` al `phileasrc`) de Phileas. Si vols un altre
editor, defineix `FABULA_EDITOR` (ordre i arguments separats per espais).

### La pantalla

A dalt, el títol i el progrés: «42.310 / 80.000 (53 %) · 118/dia fins 30-06».
A sota, l'arbre: els grups, els capítols amb les paraules i l'estat, i les
escenes quan desplegues un capítol. Amb la pantalla vertical de la Pi (75
columnes) hi cap tot; en una pantalla de 30 files o més, a baix hi ha la
sinopsi de l'element seleccionat.

Les paraules de Fàbula són de **prosa**: no hi compten la capçalera YAML ni els
títols, així que el total d'un capítol és la suma de les seves escenes. La
barra d'estat de Phileas compta tot el fitxer, per això el mateix capítol hi pot
sortir una mica més alt.

### Tecles

| Tecla | Acció |
|-------|-------|
| ↑ ↓ RePàg AvPàg Inici Fi | moure's |
| → / Espai, ← | desplegar i plegar |
| **Retorn** | obre el capítol, l'escena (amb el cursor a la seva capçalera) o la fitxa a Phileas; en un grup, el plega o desplega |
| `n` | **nou**: capítol (al manuscrit) o fitxa (a un altre grup) |
| `r` | canvia el títol i el nom del fitxer |
| `e` | canvia l'estat al següent de la llista |
| Alt+↑ / Alt+↓ (o `<` `>`) | puja o baixa el capítol |
| Tab | canvia entre l'arbre i la **taula** (outliner) |
| `u` | **on surt** el capítol o la fitxa: els `[[enllaços]]` d'altres fitxers que hi apunten |
| `l` | els **enllaços d'aquí** (de l'escena, el capítol o la fitxa) i on van |
| `t` | els enllaços **trencats** de tot el projecte |
| `f` | **cerca** un text a tot el projecte |
| `R` | **substitueix** un text a tot el projecte (amb llista i confirmació) |
| `C` | **compila** el manuscrit en un sol fitxer (`.md`, `.docx`, `.epub`…) |
| `T` | **línia temporal**: les escenes per data, amb els salts enrere |
| `d` | **diari** d'escriptura: avui, la setmana, la ratxa i la previsió d'acabar |
| `s` | desa una **instantània** del capítol o la fitxa |
| `v` | **versions** anteriors: llista amb data, paraules i diferència; Retorn en recupera una |
| Supr | mou a la paperera (pregunta abans) |
| `/` | filtra pel títol o la sinopsi, sense majúscules ni accents |
| `b` | obre **Recerca** amb el vault de recerca del projecte (vegeu més avall); en sortir, torna a Fàbula |
| Esc | esborra el filtre; sense filtre, obre el menú |
| F1, Ctrl+Q | ajuda, sortir |

Al retornar de Phileas, Fàbula torna a llegir només els fitxers que han canviat.

Per lligar el projecte amb un vault de recerca ([capítol 17](#17-recerca-fitxes-de-recerca)),
posa'l a `projecte.md`: `recerca: ~/Documents/Recerca` (també pot ser un camí
relatiu a la carpeta del projecte). Sense aquesta línia, `b` obre el vault per
defecte de Recerca.

### L'outliner: la taula del manuscrit

Amb **Tab** l'arbre passa a una taula: una fila per capítol i, a sota, les
seves escenes, amb una columna per cada metadada: **Títol, POV, Lloc, Data,
Estat i Paraules**. Serveix per veure d'un cop d'ull qui explica cada escena,
on passa, quan i en quin estat és.

Les metadades d'un **capítol** són a la capçalera YAML:

```yaml
---
estat: revisat
pov: Anna
lloc: Mas Roig
data: 1936-07-18
personatges: [Anna, Neus]
---
```

A una **escena** (una secció `###`) van en un comentari d'una línia **just sota
la capçalera**. És Markdown normal: no es veu i no compta com a paraules.

```markdown
### L'arribada
<!-- pov: Neus; lloc: El pou; data: 1936-07-19; estat: esborrany -->
```

Els camps són `pov`, `lloc`, `data` (`AAAA`, `AAAA-MM` o `AAAA-MM-DD`),
`personatges`, `sinopsi` i `estat`, separats per `;`. **Una escena hereta del
seu capítol** tot el que no diu (a la taula es veu atenuat). Un comentari sota
la capçalera que no sigui `clau: valor` es respecta i no es pren com a
metadades.

| Tecla (a la taula) | Acció |
|--------------------|-------|
| ← → | columna actual |
| `o` | **ordena** per la columna: ascendent ▲, descendent ▼ i, un tercer cop, torna a l'ordre del llibre. Ordenada, la taula és plana (només escenes, amb el capítol al títol) i els buits van sempre al final |
| `m` | **edita** la metadada de la columna (buit la treu); l'estat canvia al següent de la llista |
| `/` | filtra per qualsevol camp: `neus` troba el POV Neus, el lloc, el títol o la sinopsi; `revisat` mostra el que té aquest estat |
| Retorn | obre l'escena a Phileas, a la seva capçalera |

Quan edites una escena, Fàbula escriu el comentari al fitxer (després d'una
instantània, vegeu més avall); si el fitxer ha canviat des que el va llegir, no
hi toca res i t'ho diu.

### Enllaços: on surt cada personatge

Al text, i a les fitxes, escriu `[[Anna Puig]]` per referir-te a una fitxa o a
un capítol; és la sintaxi d'Obsidian, i també valen `[[Anna Puig|l'Anna]]` (amb
el text que vulguis) i `[[Anna Puig#Aspecte]]` (a una secció). L'enllaç apunta al
fitxer que **té aquest nom**: pel títol, pel nom del fitxer (`anna-puig`) o per un
**àlies** de la capçalera de la fitxa:

```yaml
---
alies: [Anneta, Anna P.]
---
```

No cal distingir majúscules ni accents: `[[anna puig]]`, `[[Cafe]]` i `[[Cafè]]`
són el mateix. Els enllaços dins d'un bloc de codi, de la capçalera YAML o d'un
comentari de metadades no compten.

- **`u`, on surt:** posa el cursor en una fitxa (o un capítol) i prem `u`. Veus
  cada lloc on s'hi fa referència, amb el capítol i l'escena i un tros del
  paràgraf. Retorn obre el text a Phileas, justament en aquell paràgraf.
- **`l`, enllaços d'aquí:** els `[[ ]]` de l'escena (o del capítol o de la fitxa)
  seleccionats i on van. Retorn et porta a la fitxa; si l'enllaç no en té, obre el
  text on és.
- **`t`, trencats:** els enllaços que no apunten a res (un nom mal escrit, o un
  personatge nou que encara no té fitxa), de tot el projecte. Retorn obre el text on
  és, per corregir-lo; o crea la fitxa amb `n` i desapareix de la llista.

Fàbula no canvia mai els enllaços: només els llegeix, així que Obsidian pot obrir
el mateix projecte i veure'n el graf i els retroenllaços sense convertir res.

### Cerca i substitució a tot el projecte

- **`f`, cerca:** escriu el text i veus cada coincidència de tots els capítols
  i fitxes, amb el capítol, l'escena i un tros del paràgraf (la coincidència va
  entre `«` i `»`). Retorn obre el text a Phileas, en aquell paràgraf. Per
  defecte no distingeix majúscules ni accents (`cafe` troba `Cafè`).
- **`R`, substitueix:** escriu el text a canviar i el text nou (buit = esborrar).
  Surt la llista amb totes les coincidències **marcades**; amb **Espai**
  desmarques les que no vols canviar, `a` marca o desmarca totes, i **Retorn** les
  canvia, després de confirmar. Serveix, per exemple, per canviar el nom d'un
  personatge a tot el llibre.

Dins de la llista, tres tecles canvien com es busca (i tornen a fer la cerca):

| Tecla | Opció |
|-------|-------|
| `c` | majúscules i accents: ignorar o comptar (a la cerca s'ignoren; a la substitució, compten) |
| `w` | només paraules senceres (a la substitució és activada: «Anna» no toca «Annabel») |
| `m` | incloure les **metadades** (capçalera YAML i comentaris de sota els títols): per canviar també el `pov: Anna` |

La substitució és literal i només toca els caràcters de cada coincidència: no
canvia res més del fitxer. **Abans de tocar un fitxer, Fàbula en desa una
instantània** (tecla `v` per recuperar-la), i si un fitxer ha canviat des de la
cerca, no el toca i t'ho diu. També es cerquen els enllaços `[[ ]]`, així que
canviar «Anna Puig» per «Marta Puig» corregeix també els enllaços; un canvi de
nom de fitxa i els seus enllaços van junts si els marques tots. Des de la línia
d'ordres, `fabula --cerca TEXT [CARPETA]` mostra les coincidències en text.

### Compilar el manuscrit

Amb **`C`** (o `fabula compila`) el manuscrit se't converteix en **un sol
document**, amb els capítols en ordre. Et demana el nom del fitxer, i l'extensió
tria el format:

- **`.md`**: un Markdown ben net, que no necessita res més.
- **`.docx`, `.odt`, `.epub`, `.pdf`, `.html`…**: Fàbula prepara el Markdown i
  el passa per **pandoc** (cal tenir-lo instal·lat; a la Raspberry Pi, passa el
  `.md` a l'ordinador amb `envia` i converteix-lo allà).

El document compilat comença amb el títol, l'autor i l'idioma del projecte, i
després porta cada capítol **sense** el que és de treball:

- la capçalera YAML del capítol i els comentaris de metadades de sota els títols;
- les notes `%%així%%`;
- els claudàtors dels enllaços: `[[Anna Puig|l'Anna]]` surt com a «l'Anna» i
  `[[Neus]]`, com a «Neus».

Els `TODO` i els `[?]` **no** s'esborren (és text teu), però en acabar Fàbula
t'avisa de quants en queden. Les opcions de la línia d'ordres:

```bash
fabula compila -o llibre.docx         # al format de l'extensió
fabula compila -o llibre.md --puja    # ## → # (els capítols passen a nivell 1)
fabula compila --separa-escenes       # cada ### es converteix en «* * *»
fabula compila --amb-notes --amb-enllacos   # conserva %%notes%% i [[enllaços]]
```

Sense `-o`, el document surt per la sortida estàndard. El fitxer es crea a la
carpeta del projecte si només en dius el nom, i **mai** a dins de `manuscrit/`,
de les fitxes ni sobre `projecte.md`: Fàbula ho refusa. No canvia res del projecte.

### Línia temporal

Amb **`T`** veus les escenes ordenades per la seva **data** (la metadada `data`:
`AAAA`, `AAAA-MM` o `AAAA-MM-DD`, que una escena hereta del capítol) i no per
l'ordre del text. Un capítol sense escenes hi surt com un sol element. Les
escenes **sense data** (o amb una data que no s'entén) van al final.

Si el text hi arriba **després** d'una escena posterior, hi surt marcada amb
**↶ salt enrere**: és un flaix enrere volgut, o una contradicció que convé
revisar. Dues dates parcials que es toquen (`1936-07` i `1936-07-18`) no
compten com a salt. El títol resumeix quantes escenes tenen data, quantes no i
quants salts hi ha. Retorn obre l'escena a Phileas. No es desa res.

### Diari d'escriptura

Fàbula apunta cada dia quantes paraules té el manuscrit a `.fabula/diari.csv`
(`AAAA-MM-DD,inici,final`, una línia per dia amb feina). Ho fa en obrir el
projecte, en tornar de Phileas i en prémer `d`, així que no cal fer res.
L'*inici* d'un dia és el final de l'últim dia apuntat: també compta el que has
escrit fora de Fàbula. Si esborres text, el dia pot sortir en negatiu.

Amb **`d`** (o `fabula --diari`, sense pantalla) veus:

- les paraules netes d'**avui** i de la **setmana**, i un gràfic dels últims set dies;
- la **mitjana diària** dels darrers 30 dies (només el que s'ha escrit), el **millor dia** i la **ratxa** de dies seguits escrivint;
- la **previsió**: si el `projecte.md` té `objectiu`, quan s'acabaria al ritme actual, i, si també té `data-limit`, quantes paraules al dia calen i si s'hi arriba a temps.

El diari és un fitxer de text normal: es pot copiar, editar i `envia` el porta amb el projecte.

### Instantànies: marxa enrere

Fàbula desa còpies dels teus capítols i fitxes a `.versions/` (una carpeta
per grup). Se'n fa una **sola vegada cada hora** per fitxer, **abans** d'obrir-lo
a Phileas, de canviar-ne l'estat o el títol, i a mà amb `s`. Així, quan acabes
una sessió d'escriptura, la versió més recent és la que tenies abans de començar.

Amb `v` veus les versions d'un fitxer, de la més nova a la més antiga, amb les
paraules que tenia i la diferència respecte a ara. Retorn en recupera una: abans
de posar-la, l'estat actual es desa també com a instantània, així que sempre
pots desfer la recuperació. Se'n conserven 20 per fitxer; les més antigues passen
a `.paperera/`, mai no s'esborren. Si dues sessions queden dins la mateixa hora,
només la primera té instantània prèvia.

### Coses que cal saber

- **Reanomenar** (`r`) posa `titol:` a la capçalera i canvia el nom del fitxer;
  **no toca el text**, així que el `## títol` de dins del capítol continua com
  estava. Si vols que coincideixi, canvia'l a Phileas.
- **Reordenar** intercanvia els números dels noms amb el capítol veí
  (`02-b` ↔ `10-c`); si els números són iguals o falten, renumera tots en passos
  de 10. No cal que els renumeris tu.
- Les **escenes** i els grups es canvien a Phileas, no a Fàbula.
- Les **plantilles** de fitxa són al projecte, a `plantilles/` (`capitol.md`,
  `personatge.md`, `lloc.md`, `recerca.md`, `nota.md`); admeten `{{titol}}` i
  `{{data}}`.
- **A la Raspberry Pi** l'ordre és la mateixa (`fabula`); els projectes viuen a
  `/data/textos` i `envia` els copia a l'ordinador amb la resta de textos.

---

## 17. Recerca: fitxes de recerca

**Recerca** és la tercera eina, al costat de Phileas i Fàbula. Serveix per a
la documentació d'una recerca: fitxes de fonts, de bibliografia, de persones i
de llocs, notes de camp i transcripcions de treball. Les fitxes viuen en un
**vault d'Obsidian** normal, i Recerca les llegeix i les escriu sense canviar
res del format: el mateix vault s'obre a Obsidian al PC i a Recerca a la Pi.

```bash
recerca ~/Documents/Recerca      # obre el vault
recerca --resum ~/Documents/Recerca
```

### Què entén

- **Carpetes**: l'arbre del vault és el de les carpetes.
- **Propietats** a la capçalera (YAML), també les llistes que escriu Obsidian:

  ```yaml
  ---
  tipus: fons
  arxiu: ACA
  signatura: "ACA, Cancelleria, reg. 1234, f. 56v"
  data: "1363"
  tags: [fiscalitat, notarial]
  aliases:
    - Registre 1234
  complementa: "[[Batlle 1988]]"
  ---
  ```

  `tipus` dona una lletra a la llista (F fons, N nota de camp, B
  bibliografia…). Una propietat amb un enllaç, com `complementa`,
  `contradiu` o `cita`, és una **relació amb nom** i surt així a les notes que
  hi enllacen.
- **Etiquetes**: les de `tags` i les del text (`#fiscalitat`), també niades
  (`#tema/subtema`), que es mostren en arbre.
- **Enllaços**: `[[Nota]]`, `[[Nota|text]]`, `[[Nota#Secció]]`, el bloc
  `[[Nota#^f56v]]` (un passatge marcat amb `^f56v` al final de la línia),
  `[[Carpeta/Nota]]`, els àlies i els enllaços Markdown `[text](../Nota.md)`.
  `![[imatge.png]]` és un adjunt, no un enllaç trencat.
- Res del que hi ha dins de blocs de codi o de comentaris (`%% %%`) no compta.

### L'índex

La primera vegada, Recerca fa un índex a `.recerca/` dins del vault (unes
quantes desenes de segons a la Pi per a 2.000 fitxes). Després, en obrir-lo,
només llegeix el que ha canviat. L'índex es pot esborrar sense perdre res:
els fitxers són la veritat. Obsidian no veu les carpetes que comencen amb punt.

### La pantalla

A partir de 110 columnes hi ha tres panells: la **navegació**, la **llista de
fitxes** i el **visor**. Per sota, se'n veuen dos, i el de l'esquerra fa de
navegació o de llista. Retorn passa d'un panell al següent, i ← torna enrere.

| Tecla | Acció |
|-------|-------|
| `1` `2` `3` | navega per carpetes, per etiquetes o per tipus |
| ↑ ↓ RePàg AvPàg Inici Fi | moure's |
| Retorn / → | de la navegació a les fitxes, i de les fitxes al visor |
| `/` | filtra les fitxes pel nom (sense majúscules ni accents) |
| `o` | ordena per nom, data, modificació o paraules |
| Tab / Maj+Tab | al visor, l'enllaç següent o l'anterior (també `]` i `[`) |
| Retorn | al visor, segueix l'enllaç triat (si no n'hi ha cap, tria el primer) |
| Retrocés | torna a la fitxa d'abans, a la mateixa posició |
| `u` / `b` | les notes que hi enllacen (backlinks), amb el context |
| `l` | els enllaços d'aquesta fitxa i on van a parar |
| `t` | els enllaços trencats de tot el vault |
| `z` | el visor a tota la pantalla |
| `e` | edita la fitxa a Phileas, a la línia que es veu; en tornar, es torna a llegir |
| `n` | fitxa nova a la carpeta triada, amb plantilla si n'hi ha |
| `a` | anotació: una fitxa nova amb `anota: "[[Fitxa]]"` |
| Supr | mou la fitxa a la paperera d'Obsidian (`.trash/`) |
| `r` | a la Pi: rep el vault del PC (vegeu més avall) |
| Ctrl+R | torna a llegir el vault |
| Esc, F1, Ctrl+Q | menú, ajuda, sortir |

Les fitxes noves porten `id` i `created` a la capçalera. Les plantilles són
les de la carpeta de plantilles d'Obsidian (`.obsidian/templates.json`) o,
si no, les de `plantilles/`, amb `{{title}}`, `{{date}}` i `{{time}}`.

### Recerca a la Pi: rep

La recerca es fa al PC, amb Obsidian, i la Pi en porta una **còpia**:

1. Esc → BYOK → **Rep la recerca del PC** (o `rep` a la paleta). La primera
   vegada tria **vault nou**, l'ordinador (el mateix d'«Envia a un
   ordinador») i la carpeta del vault al PC. Demana la contrasenya del PC
   una sola vegada.
2. El vault arriba a `/data/recerca/NOM`. Esc → BYOK → **Recerca** l'obre.
3. A la Pi només s'escriu a la carpeta **`_pi/`** del vault: les fitxes
   noves (`n`) i les anotacions (`a`) hi van. Les altres fitxes, que són del
   PC, no s'editen aquí.
4. Cada `rep` (també la tecla **`r`** de Recerca) torna el vault al dia i
   envia `_pi/` al PC. Al PC, a Obsidian, mou les fitxes de `_pi/` al seu
   lloc: la vegada següent, desapareixen de `_pi/` a la Pi i hi arriben al
   lloc nou.

Les fonts (PDF, imatges) no viatgen: queden al PC. A les fitxes, la propietat
`font` hi apunta.

### Configuració

Al phileasrc, com Phileas (també el tema):

```ini
recerca-vault = ~/Documents/Recerca   # el vault per defecte
```

`RECERCA_EDITOR` canvia l'editor de `e` (per defecte, Phileas).

---

## 18. Xuleta de dreceres

### Fitxer

| Tecla | Ordre |
|-------|-------|
| Ctrl+O | obrir |
| Ctrl+N | document nou |
| Alt+O | documents recents |
| Ctrl+S | desar |
| Alt+S | desar com |
| Ctrl+Q | sortir |

### Edició

| Tecla | Ordre |
|-------|-------|
| Ctrl+Z / Ctrl+Y | desfer / refer |
| Ctrl+X / Ctrl+C / Ctrl+V | tallar / copiar / enganxar |
| Ctrl+A | seleccionar tot |
| Ctrl+Espai | començar una selecció |
| Maj+fletxes | ampliar la selecció |
| Ctrl+W / Alt+D | esborrar paraula anterior / següent |
| Alt+↑ / Alt+↓ | pujar / baixar paràgraf (o secció, a l'esquema) |
| Alt+T | tipografia automàtica |
| Alt+. | punt volat · |
| F5 / F6 | nota al peu / anar a la nota o tornar |

### Moviment i cerca

| Tecla | Ordre |
|-------|-------|
| Ctrl+←/→ (Alt+B / Alt+F) | paraula anterior / següent |
| Ctrl+↑/↓ (Alt+P / Alt+N) | paràgraf anterior / següent |
| Ctrl+Inici / Ctrl+Fi (Alt+< / Alt+>) | inici / final del document |
| Alt+L | anar a una línia |
| Ctrl+F | cercar |
| Ctrl+G / F3 | cerca següent |
| Alt+G / Maj+F3 | cerca anterior |
| Ctrl+R | substituir |
| F2 / Alt+E | esquema |
| Alt+K | pendents |

### Ortografia

| Tecla | Ordre |
|-------|-------|
| F7 | activar o desactivar |
| F8 | canviar d'idioma |
| Maj+F7 | error següent |
| Alt+Retorn | suggeriments |

### Vista i altres

| Tecla | Ordre |
|-------|-------|
| F4 | enfocament |
| F10 / Alt+M | màquina d'escriure |
| Alt+C | centrar la columna |
| F9 | afegir una idea al calaix |
| Ctrl+L | redibuixar la pantalla |
| Esc | menús |
| Ctrl+P | paleta d'ordres (Tab completa el nom) |
| F1 | ajuda |

---

*Phileas és programari lliure (GPL-3.0). El codi i les notes de cada versió són a
<https://github.com/asolans/phileas>.*
