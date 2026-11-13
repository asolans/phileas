# Phileas

Editor de text per a la consola de Linux, pensat per **escriure prosa en
Markdown**, no per programar. És petit (un executable de ~180 KB escrit en C
amb ncursesw), arrenca a l'instant i funciona igual en un terminal
d'escriptori que a la consola d'una Raspberry Pi 1B sense entorn gràfic.

Va néixer com l'editor del projecte **BYOK**: un dispositiu d'escriptura sense
distraccions fet amb una Raspberry Pi, al qual es connecta un teclat i que
només serveix per escriure.

```
    # Capítol 1

    Phileas Fogg era un home que no feia res sense calcular-ho. El text
    s'ajusta sol a 72 columnes i cada paràgraf és una sola línia al
    fitxer, de manera que el Markdown que es desa és net.

    - Les llistes continuen soles en prémer Retorn
    - i les paraules mal escrites es marquen en vermell (F7).

 cap1.md                                   ca 56 paraules  1 pàg.
```

## Característiques

- **Markdown sense previsualització:** només el marcatge, amb colors mínims
  (capçaleres, negreta, cursiva, codi i cites) que funcionen fins i tot en
  terminals de 8 colors.
- **Separador centrat:** una línia que només té `***` (tolera espais davant i darrere) es mostra centrada a la
  pantalla. És només una vista: el fitxer no canvia.
- **Temes de colors:** terminal (els colors del terminal, per defecte), fosc,
  clar, sèpia, fòsfor i WordPerfect (gris sobre blau, com el WordPerfect 5.1
  per a DOS), des del menú Vista o amb l'ordre `tema`, que el deixen desat
  al `phileasrc` de l'usuari per a la vegada següent. A la consola de Linux
  (com a la imatge BYOK), cada tema hi posa els seus propis colors i, en
  sortir, torna a deixar els de la consola.
- **Ajust de línia suau** a un nombre de columnes configurable (72 per
  defecte), amb sagnat penjant a les llistes i a les cites, marges i columna
  centrada opcional.
- **Model de paràgrafs:** Retorn separa paràgrafs; al fitxer s'escriu una
  línia en blanc entre blocs. Els textos tallats a mà s'ajunten en obrir-los.
  Els fitxers que no són `.md`, `.markdown` o `.txt` s'obren en mode text pla
  i no es modifiquen. Un fitxer nou sense extensió (en desar com, al
  navegador o a la línia d'ordres) es crea amb `.md`: `nota` → `nota.md`.
- **Dreceres a l'estil dels processadors de text** (Ctrl+S, Ctrl+Z, Ctrl+C,
  Ctrl+V, Ctrl+F…), amb alternatives Alt+lletra per a la consola de Linux,
  on moltes combinacions no arriben.
- **Porta-retalls compartit** a l'escriptori: Ctrl+C i Ctrl+X hi envien el
  text (amb la línia en blanc entre paràgrafs, com al fitxer) i Ctrl+V porta
  el que s'hi hagi copiat en una altra aplicació. Fa servir `wl-copy` i
  `wl-paste` (Wayland), `xclip` o `xsel` (X11). A més, hi ha un **porta-retalls
  comú** de Phileas, Fàbula i Recerca (el fitxer `retalls` del directori de
  configuració), que funciona entre programes també a la consola de la Pi.
- **Menús** a l'estil de WordGrinder (Esc) i **paleta d'ordres** amb
  autocompleció (Ctrl+P): tota ordre té un nom i s'hi pot arribar encara que
  no tingui tecla.
- **Correcció ortogràfica** amb hunspell, en català i castellà (o qualsevol
  altre diccionari instal·lat): marcatge instantani, suggeriments,
  revisió d'error en error i diccionari personal.
- **Desfer i refer** il·limitats (fins a 8 MB), **cerca i substitució**.
- **Desat atòmic i autodesat:** desa sol després de 30 segons sense escriure
  i, com a molt, cada 5 minuts. Si es tanca el terminal o el sistema s'apaga
  (SIGHUP, SIGTERM), desa abans de sortir.
- **Mode màquina d'escriure:** la línia on s'escriu es queda a una altura fixa
  de la pantalla. Un document nou comença a dalt, amb dues files en blanc; en
  arribar al límit (80 %), el text puja i la línia queda a l'ancoratge (50 %).
  En mode clàssic, les files en blanc es mantenen fins que s'arriba a baix.
  F10 (o Alt+M) passa d'un mode a l'altre: salt, línia fixa i clàssic.
- **Esquema** (F2): les capçaleres del document, sagnades pel nivell, amb
  les paraules de cada secció; Enter hi salta, escrivint es filtren i
  Alt+↑/↓ mou una secció sencera, amb les subseccions.
- **Pendents** (Alt+K): `TODO` (paraula sencera, en majúscules), `[?]` i
  `%%comentari%%` es ressalten al text (en vídeo invers); l'ordre `pendents`
  els llista amb el número de línia, es filtren escrivint i Enter hi salta.
  No compten dins del codi ni de les metadades YAML.
- **Enfocament** (F4): atenua tot el text menys el paràgraf o la frase on
  s'escriu.
- **Tipografia en escriure** (opcional): `--` → —, `<<` `>>` → « », `...` → …
  i, amb una altra opció, l'apòstrof ' → ’. Ctrl+Z la desfà just després.
  No actua dins del codi ni a les línies de guions (`---`). Al fitxer queden
  els caràcters Unicode, que són Markdown normal.
- **Moure paràgrafs:** Alt+↑/↓ puja o baixa el paràgraf del cursor (o les
  línies seleccionades).
- **Notes al peu** amb la sintaxi de pandoc: F5 posa la crida `[^1]` al
  cursor (amb el número següent) i la definició `[^1]: ` al final del
  document, on es queda el cursor per escriure-la; F6 va de la crida a la
  nota i de la nota a la crida. L'ordre `renumerar-notes` torna a numerar-les
  1, 2, 3… en l'ordre del text i n'ordena les definicions (les etiquetes amb
  nom, com `[^font]`, es queden). Les del codi no compten.
- **Navegador de fitxers** integrat (Ctrl+O) i **recompte de paraules** i
  **estimació de pàgines** a la barra d'estat, que també indica el mode de
  desplaçament (`salt`, `fixa` o `clàssic`).
- **Documents recents** (Alt+O, o Esc → Fitxer → Recents) i **posició del
  cursor**: en tornar a obrir un document, el cursor és on era. Tot es desa
  al fitxer `historial` del directori de configuració, fora dels documents,
  que no es modifiquen.
- **Calaix d'idees, orientat a la referència:** F9 afegeix una idea mentre
  s'escriu; amb un bloc seleccionat, F9 hi envia el bloc.
  Cada entrada és `## font · p. pàgina · #etiquetes`, un títol opcional
  (`### títol`), el text i un peu `∙ data · document:línia`; sense font, només
  `## #etiquetes`. La **font
  activa** (un llibre o article, p. ex. «Prous, 1935») es recorda entre
  sessions i es proposa a cada idea fins que es canvia: Esc → Edita → Font del
  calaix (o l'ordre `calaix-font`): un camp editable amb la font actual; escriure-hi
  la canvia i esborrar-la la treu. Amb font activa, F9 demana la pàgina (la de l'última idea, per
  defecte); després, el títol (es pot deixar en blanc) i les etiquetes. La font surt a la barra d'estat. Fitxer → Calaix d'idees (o
  `calaix`) les llista, filtrables, i porta a l'entrada triada. Es desa al
  fitxer de l'opció `calaix` (per defecte `~/calaix.md`) i la font activa a
  `calaix-font` del directori de configuració. Mentre el calaix és obert com a
  document, no s'hi afegeix res. El format antic (capçalera amb data i hora) ja no es llegeix.
  Totes les ordres del calaix són al menú **Idees** (Esc → Idees, després de Vista).
  **Obsidian:** «Vault d'Obsidian…» tria la carpeta del vault amb el navegador (Tab tria la
  carpeta on som) i pregunta si es vol definir també la carpeta d'entrada (`Inbox-byok`) i la
  propietat del tipus (`tipus`); «Calaix de la Pi…» diu on és el calaix que arriba amb
  `envia` (per defecte `~/BYOK/calaix.md`). Tot es desa al phileasrc (`obsidian-vault`,
  `obsidian-inbox`, `obsidian-tipus`, `calaix-pi`). «Importa a Obsidian» (`obsidian-importa`)
  crea una nota per idea a la carpeta d'entrada, amb el calaix local i el de la Pi; una idea
  que ja és al vault no es duplica. Fa servir l'ordre `phileas-importa-calaix` (el `.deb` la
  instal·la, amb `python3`), que també es pot cridar a mà: `--calaix`, `--vault`, `--inbox`,
  `--camp-tipus`, `--simula`, `--reimporta`; sense opcions llegeix el phileasrc.
- **Ordres externes:** es poden afegir a la paleta ordres del sistema
  (exportar amb pandoc, comptar paraules, enviar el text…).
- **UTF-8 complet:** accents, ç, l·l, cometes tipogràfiques i caràcters
  amples. Conserva els salts CRLF, la BOM i els bytes no UTF-8 del fitxer.
- **Ajuda integrada** (F1), amb les tecles que hi ha assignades de debò.

La interfície és en català.

## Instal·lació

### Paquet per a Debian i Ubuntu

A la pàgina de
[versions publicades](https://github.com/asolans/phileas/releases) hi ha un
paquet `.deb` per a x86-64 (per a la Raspberry Pi, vegeu
[més avall](#raspberry-pi)):

```bash
sudo apt install ./phileas_2.1.2_amd64.deb
```

`apt` hi afegeix les dependències (ncurses) i, com a recomanacions, hunspell,
els diccionaris de català i castellà i `wl-clipboard` (per compartir el
porta-retalls amb les altres aplicacions). El paquet també instal·la un
llançador per a l'escriptori (GNOME, KDE...): Phileas apareix entre les
aplicacions i s'obre en un terminal; també es pot triar a «Obre amb» per als
fitxers `.md` i `.txt`.

### Des del codi font

Cal un compilador de C11 i les capçaleres de ncursesw:

```bash
sudo apt install build-essential pkg-config libncurses-dev   # Debian, Ubuntu
sudo dnf install gcc make pkgconf ncurses-devel               # Fedora
sudo pacman -S base-devel ncurses                             # Arch
```

Compilar, provar i instal·lar:

```bash
git clone https://github.com/asolans/phileas.git
cd phileas
make
make test
sudo make install
```

`make install` copia l'executable a `/usr/bin/phileas`. Per instal·lar-lo en
un altre lloc: `make PREFIX=$HOME/.local install`.

### Ortografia

hunspell **no cal per compilar**: Phileas carrega `libhunspell` quan
s'executa (`dlopen`) i, si no la troba, funciona igual però sense
ortografia. Per tenir-la:

```bash
sudo apt install libhunspell-1.7-0 hunspell-ca hunspell-es   # Debian, Ubuntu
sudo dnf install hunspell hunspell-ca hunspell-es             # Fedora
sudo pacman -S hunspell hunspell-ca hunspell-es_es            # Arch
```

Els diccionaris es busquen a `/usr/share/hunspell`, `/usr/share/myspell`,
`/usr/local/share/hunspell`, als directoris de `$DICPATH` i a
`~/.local/share/hunspell`.

### Raspberry Pi

Phileas està pensat per funcionar en una Raspberry Pi petita, fins i tot una
Pi 1 o una Pi Zero, connectada a una pantalla i amb el teclat com a únic
perifèric. Hi ha dos camins.

#### A. Raspberry Pi OS

Funciona en qualsevol model amb Raspberry Pi OS. Per a una Pi 1 o una
Pi Zero, cal la versió **Lite de 32 bits** (sense escriptori). El paquet
`.deb` de les versions publicades és només per a x86-64; a la Pi, Phileas es
compila des del codi font (uns quants minuts en una Pi 1).

1. **Dependències** (per compilar i per a l'ortografia):

   ```bash
   sudo apt update
   sudo apt install build-essential pkg-config libncurses-dev \
                    libhunspell-1.7-0 hunspell-ca hunspell-es
   ```

2. **Compilar i instal·lar.** Baixeu `phileas-VERSIÓ.tar.gz` de les
   [versions publicades](https://github.com/asolans/phileas/releases) i
   copieu-lo a la Pi (per exemple amb `scp`), o bé cloneu el repositori amb
   `git clone`. Després:

   ```bash
   tar xzf phileas-2.1.2.tar.gz
   cd phileas-2.1.2
   make
   make test
   sudo make install
   ```

   Si ho preferiu en forma de paquet (més fàcil de desinstal·lar amb
   `sudo apt remove phileas`), en lloc de `sudo make install`:

   ```bash
   sudo apt install fakeroot
   make deb
   sudo apt install ./dist/phileas_2.1.2_armhf.deb
   ```

   El nom del paquet porta l'arquitectura de la Pi: `armhf` en 32 bits,
   `arm64` en 64 bits.

3. **Idioma i teclat.** Phileas necessita una configuració regional UTF-8
   perquè els accents i la ç es vegin i s'escriguin bé:

   ```bash
   sudo raspi-config
   ```

   A *Localisation Options*, trieu *Locale* `ca_ES.UTF-8` (o `es_ES.UTF-8`)
   i, a *Keyboard*, la disposició del teclat. La configuració regional
   també decideix el diccionari amb què arrenca l'ortografia.

4. **Lletra de la consola.** Amb `sudo dpkg-reconfigure console-setup` es
   tria el joc de caràcters (*Guess optimal character set*) i la mida de la
   lletra (per exemple Terminus 16×32, per escriure més còmodament).
   Phileas ajusta el text a l'amplada que hi càpiga.

5. **(Opcional) Un dispositiu només per escriure.** Perquè la Pi arrenqui
   directament a Phileas, activeu l'inici automàtic de sessió a la consola
   (`sudo raspi-config` → *System Options* → *Boot / Auto Login* →
   *Console Autologin*) i afegiu al final de `~/.profile`:

   ```bash
   if [ "$(tty)" = /dev/tty1 ]; then
       mkdir -p ~/textos && cd ~/textos
       phileas
   fi
   ```

   Si la Pi s'apaga de manera ordenada mentre escriviu, Phileas desa el
   document abans de sortir. L'autodesat limita el que es pot perdre si es
   talla el corrent.

**Tecles a la consola.** A la consola de Linux (sense entorn gràfic), les
combinacions de Ctrl o Maj amb les fletxes sovint no arriben al programa.
Totes les ordres tenen una alternativa amb Alt+lletra o una tecla F (vegeu
les [tecles principals](#tecles-principals)), i Esc obre els menús.

#### B. Imatge BYOK

El projecte BYOK té una imatge pròpia per a la **Raspberry Pi 1B**, feta
amb Buildroot. Arrenca en pocs segons directament a Phileas, amb els
diccionaris de català i castellà, i un sistema de només lectura que no es
fa malbé si es talla el corrent. Té ordres per canviar la mida de la lletra,
girar la pantalla, exportar al pendrive i sincronitzar-ho tot amb un ordinador,
i hi obre Fàbula i Recerca des del menú BYOK.
Aquesta imatge no forma part d'aquest repositori.

Per compilar Phileas per a una altra imatge de Buildroot o amb una cadena
de compilació creuada, vegeu [Desenvolupament](#desenvolupament).

## Fàbula (organitzar una novel·la)

El paquet inclou **Fàbula**, una eina de consola per organitzar un projecte
literari: capítols, escenes, personatges, llocs i recerca, tot com a fitxers
Markdown normals. Mostra l'arbre del projecte amb les paraules, l'estat de cada
capítol i el progrés cap a l'objectiu; Retorn obre el text a Phileas. Desa instantànies dels fitxers (`.versions/`) i en permet recuperar-les. Tab hi mostra una taula (outliner) amb el POV, el lloc, la data i l'estat de cada escena, que es pot ordenar, filtrar i editar. Entén els enllaços `[[Nom]]` d'Obsidian: mostra on surt cada personatge, els enllaços d'una escena i els que no apunten enlloc. Cerca i substitueix text a tot el projecte, amb llista de coincidències, confirmació i una instantània de cada fitxer abans de canviar-lo. Compila el manuscrit en un sol `.md` net (o `.docx`, `.epub`, `.pdf` amb pandoc). Porta un diari d'escriptura (`.fabula/diari.csv`) amb les paraules d'avui, la ratxa, la mitjana i la previsió d'acabar (`d`) i una línia temporal que ordena les escenes per data i avisa dels salts enrere (`T`).

```bash
fabula nou "La casa del pou"     # crea el projecte i l'obre
fabula la-casa-del-pou           # obre un projecte
fabula --resum la-casa-del-pou   # resum en text, sense pantalla
fabula --cerca anna la-casa-del-pou   # cerca a tot el projecte, en text
fabula compila -o llibre.docx la-casa-del-pou   # el manuscrit en un sol document
fabula --diari la-casa-del-pou   # avui, ratxa, mitjana i previsió, en text
```

Des de Phileas, Esc → Eines → Fàbula (l'ordre `fabula`). Fora d'un projecte,
`fabula` deixa triar-ne un o crear-ne un de nou a la carpeta de `projectes =`
del phileasrc (o, si no n'hi ha, a la carpeta actual).
La tecla `b` obre Recerca amb el vault de recerca del projecte (`recerca:` a
`projecte.md`), i en sortir es torna a Fàbula.

Les tecles i el format del projecte, al capítol 16 del [manual](MANUAL.md). Es
compila amb `make fabula` (executable a `build/fabula`) i les proves són a
`make test-fabula`.

## Recerca (fitxes de recerca)

**Recerca** és la tercera eina, per a fitxes de recerca en un vault d'Obsidian:
carpetes, propietats, etiquetes, enllaços `[[ ]]` i backlinks, amb un índex
SQLite (FTS5) a `VAULT/.recerca/index.db` que es posa al dia sol i es pot
esborrar sense perdre res.

La pantalla (`recerca VAULT`) té tres panells: la navegació per carpetes, per
etiquetes (`#tema/subtema` en arbre) o per tipus (`1` `2` `3`); la llista de
fitxes, que es filtra amb `/` i s'ordena amb `o`; i el visor, de només lectura,
amb les propietats a dalt. Retorn passa de la navegació a les fitxes i d'aquí al
visor; al visor, Tab i Maj+Tab trien un enllaç (també `]` i `[`), Retorn el segueix
i Retrocés torna enrere. `u` i `b` mostren les notes que hi enllacen, `l` els
enllaços de la fitxa, `t` els trencats de tot el vault i `z` amplia el visor. Per
sota de 110 columnes es veuen dos panells. Esc obre el menú i F1, l'ajuda.

`f` cerca al nom, a les propietats i al text de totes les fitxes (amb l'índex
FTS5; `"entre cometes"`, la frase exacta) i mostra la primera línia on surten.
`m` marca paràgrafs del visor (↑ ↓ n'amplien la marca) i `c` els copia (`C`, amb
`[[la fitxa]]` al final) al porta-retalls comú: a Phileas, Ctrl+V.

`e` obre la fitxa a Phileas (o a l'editor de `editor =` del phileasrc, o de `RECERCA_EDITOR`) a la línia que es
veu, i en tornar la torna a llegir. `n` crea una fitxa nova a la carpeta triada,
amb una plantilla de la carpeta de plantilles d'Obsidian si n'hi ha; `a` hi fa una
anotació (una fitxa nova amb `anota: "[[Fitxa]]"`), i `Supr` la mou a la
paperera d'Obsidian (`.trash/`). Les fitxes noves porten `id` i `created`.

Al phileasrc:

```
recerca-vault = /ruta/al/vault   # el vault per defecte
recerca-replica = si             # el vault és una còpia del PC (la Pi):
recerca-entrada = _pi            # només s'escriu en aquesta carpeta
```

```bash
recerca VAULT                    # la pantalla
recerca --indexa VAULT           # posa l'índex al dia
recerca --resum VAULT            # fitxes, tipus, etiquetes i enllaços
recerca --on-surt "Nom" VAULT    # notes que enllacen a «Nom»
recerca --trencats VAULT         # enllaços que no apunten enlloc
recerca --cerca "pau caspe" VAULT   # notes amb totes les paraules
```

A la Pi, `rep` (menú BYOK → Rep la recerca del PC, o la tecla `r` de Recerca)
porta una còpia d'un vault del PC a `/data/recerca/NOM` i torna al PC les fitxes
escrites a la Pi, que sempre van a `_pi/`. Des de Phileas, Esc → Eines → Recerca
(l'ordre `recerca`); des de Fàbula, la tecla `b` (amb `recerca: CAMÍ` a
`projecte.md`). En sortir, es torna a Phileas o a Fàbula.
Capítol 17 del [manual](MANUAL.md).

Es compila amb `make recerca` (executable a `build/recerca`), però abans cal
posar l'amalgamació de SQLite a `recerca/sqlite/` (vegeu el `LLEGEIX.md` d'aquella
carpeta). Les proves es passen amb `make test-recerca`.

## Ús

```
phileas [-c COLUMNES] [-a SEGONS] [-d IDIOMA] [+LÍNIA] [FITXER]
```

| Opció | Efecte |
|-------|--------|
| `-c N` | amplada del text en columnes (per defecte 72) |
| `-a N` | autodesat després de N segons sense escriure; `0` el desactiva |
| `-d ID` | activa l'ortografia amb aquest diccionari (`ca`, `es`, `en_US`…) |
| `+N` | obre el document amb el cursor a la línia (paràgraf) N, la primera és la 1; va davant de la posició recordada |
| `--config` | escriu la configuració completa amb els valors actuals |
| `-h`, `-v` | ajuda i versió (dins Phileas, Esc → Ajuda i configuració → Quant a Phileas) |

Sense fitxer, Phileas comença un document nou. Per començar, n'hi ha prou
de recordar tres tecles: **Esc** (menús), **F1** (ajuda) i **Ctrl+P**
(paleta d'ordres).

### Tecles principals

| Tecla | Ordre |
|-------|-------|
| Ctrl+S / Alt+S | desar / desar com |
| Ctrl+O / Ctrl+N | obrir (navegador de fitxers; un nom nou sense extensió pren `.md`) / document nou |
| Alt+O | documents recents (Supr en treu un de la llista) |
| Ctrl+Q | sortir (demana confirmació si hi ha canvis) |
| Ctrl+Z / Ctrl+Y | desfer / refer |
| Ctrl+X / Ctrl+C / Ctrl+V | tallar / copiar / enganxar (a l'escriptori, amb el porta-retalls del sistema) |
| Ctrl+A, Ctrl+Espai, Maj+fletxes | seleccionar tot / començar la selecció / ampliar-la |
| Ctrl+W / Alt+D | esborrar la paraula anterior / la següent |
| Alt+↑ / Alt+↓ | pujar / baixar el paràgraf (a l'esquema, la secció) |
| Ctrl+F, Ctrl+G, Alt+G | cercar, següent, anterior |
| Ctrl+R | substituir |
| Alt+B / Alt+F | paraula anterior / següent (també Ctrl+fletxes) |
| Alt+P / Alt+N | paràgraf anterior / següent (també Ctrl+fletxes) |
| Alt+< / Alt+> | inici / fi del document |
| Alt+L | anar a una línia |
| F7 / F8 | activar l'ortografia / canviar d'idioma |
| Maj+F7 / Alt+Retorn | revisar d'error en error / suggeriments |
| Alt+C | centrar la columna |
| F4 | enfocament: paràgraf, frase o cap |
| F10 / Alt+M | màquina d'escriure: salt, línia fixa o clàssic |
| Alt+T | tipografia automàtica: activa o desactiva |
| F2 / Alt+E | esquema del document |
| Alt+K | llista de pendents (TODO, [?], %%nota%%) |
| F5 / F6 | nota al peu nova / anar de la crida a la nota i tornar |
| F9 | al calaix d'idees: la selecció, o una idea nova si no n'hi ha; demana la pàgina (si hi ha font activa) i les etiquetes |
| Ctrl+L | redibuixar la pantalla |

Les alternatives amb Alt existeixen perquè a la consola de Linux (sense
entorn gràfic) les combinacions amb Ctrl+fletxes sovint no arriben.

## Configuració

Phileas llegeix, per aquest ordre, `/etc/phileas/phileasrc` i el fitxer de
l'usuari: `$PHILEAS_CONFIG/phileasrc` si hi ha aquesta variable, si no
`$XDG_CONFIG_HOME/phileas/phileasrc` o `~/.config/phileas/phileasrc`. Les
opcions de la línia d'ordres manen sobre tots dos.

Les opcions que es canvien des dels menús (columna centrada, colors,
enfocament, màquina d'escriure, tema i tipografia) es desen soles al fitxer
de l'usuari. Per tenir-lo complet, amb totes les
opcions i els valors actuals, dins Phileas: Ctrl+P `desar-configuracio` (o
Esc → Ajuda i configuració → Desa la configuració). Conserva les línies
`tecla.` i `externa` que hi hagués i les claus que Phileas no fa servir (les
de Recerca i Fàbula, `recerca-…` i `fabula-…`, que Phileas accepta sense
error), i deixa l'anterior a `phileasrc.anterior`.
`phileas --config` escriu el mateix a la sortida; no el redirigiu al mateix
`phileasrc`, que el shell buida abans que Phileas el llegeixi.

El `phileasrc` és el de les tres eines (`comu/rc.c` el llegeix per a Fàbula i
Recerca, amb les mateixes regles, també per als comentaris). La plantilla té
tres seccions: **Comú** (`editor`, que obren Fàbula i Recerca; `textos`,
`projectes` i `recerques`, els llocs que fan servir els triadors i les ordres
de la BYOK; el tema i els colors també són comuns), **Phileas** i
**Recerca** (`recerca-vault`, `recerca-replica`, `recerca-entrada`).
`phileas --valor CLAU` escriu el valor d'una opció (els dos fitxers, amb `~`
expandit; codi 1 si no hi és): és com el llegeixen els scripts.

Al mateix directori hi ha `historial`: una línia per document obert (`LÍNIA
BYTE CAMÍ`, el més recent primer, fins a 100), que es desa en canviar de
document, en sortir i en rebre SIGTERM o SIGHUP. D'aquí surten la llista de
recents i la posició del cursor en obrir un document.

Només cal deixar-hi les opcions que es volen canviar. Un exemple:

```ini
columnes = 66
marge-esquerre = 6
maquina-escriure = salt        # salt, fixa o classic
autodesat = 20
recordar-posicio = no          # en obrir un document, el cursor va al final
ortografia = si
idiomes = ca es                # F8 alterna; el primer és el de l'arrencada
tema = sepia                   # terminal, fosc, clar, sepia, fosfor o wordperfect
enfocament = frase             # no, paragraf o frase
tipografia = si                # -- → —, << >> → « », ... → …
apostrof-tipografic = si       # ' entre lletres → ’
color-capcalera-1 = blau       # retoca un color del tema (després de «tema»)

calaix = ~/calaix.md           # on es desa el calaix d'idees (F9)

# Dreceres: tecla.NOM = ordre (buit = treure-la)
tecla.f10 = colors

# Ordres externes, que apareixen a la paleta (Ctrl+P)
externa-breu.paraules = wc -w      # Ctrl+P, «paraules cap1.md»
externa.terminal = sh              # obre un shell; en sortir, torna a Phileas
```

Els colors que es poden retocar són `color-text`, `color-fons`, `color-barra`,
`color-barra-fons`, `color-seleccio`, `color-seleccio-fons`, `color-menu`,
`color-menu-fons`, `color-marcadors`, `color-capcalera-1` a `-3`, `color-codi`,
`color-cita` i `color-error`. Els valors són els 8 colors bàsics (`negre`,
`vermell`, `verd`, `groc`, `blau`, `magenta`, `cian`, `blanc`), els clars
(`gris`, `vermell-clar`… `blanc-clar`) o `cap`, el color del terminal. Sense
color, la barra i la selecció van en vídeo invers. `paleta-consola = no` fa que
a la consola de Linux es mantinguin els colors de la consola.

A la paleta, el que s'escriu després del nom d'una ordre externa se li passa
com a arguments. Abans d'executar-la, Phileas desa el document si té canvis.
Les ordres `externa-breu` mostren la darrera línia de la sortida a la barra
d'estat; les `externa` deixen el terminal a l'ordre fins que acaba (i Retorn
torna a Phileas); les `externa-pantalla` són per a programes de pantalla sencera,
com Recerca, i en acabar tornen a Phileas directament.

Una línia incorrecta no impedeix arrencar: s'ignora i la barra d'estat en
dona l'avís. Des de la paleta, `recarregar-configuracio` torna a llegir el
fitxer sense sortir.

## Desenvolupament

```bash
make                       # ./phileas
make test                  # proves unitàries (sense ncurses)
make clean && make debug   # amb AddressSanitizer i UBSan
make deb                   # paquet .deb a dist/
make dist                  # codi font en .tar.gz a dist/
```

**Compilació creuada** (p. ex. per a una Raspberry Pi amb Buildroot):

```bash
make CC=arm-linux-gcc PKG_CONFIG=/ruta/a/pkg-config
make DESTDIR=/ruta/arrel PREFIX=/usr install
```

`make install-desktop` hi afegeix el llançador (`.desktop`) i la icona;
`make deb` ja ho fa.

**Executable estàtic** (portable, sense dependències): un executable estàtic
no pot carregar hunspell amb `dlopen`, i per això cal enllaçar-l'hi:

```bash
make HUNSPELL_STATIC=1 LDFLAGS=-static
```

Funciona millor amb musl que amb glibc. El projecte BYOK el compila amb una
cadena musl de Buildroot.

### Estructura del codi

| Fitxer | Funció |
|--------|--------|
| `main.c` | arrencada, arguments i bucle principal |
| `buffer.c` | vector de línies (una línia = un paràgraf), càrrega i desat atòmic |
| `editor.c` | cursor, moviment, edició, selecció i porta-retalls |
| `clipboard.c` | porta-retalls del sistema amb `wl-copy`/`wl-paste`, `xclip` o `xsel` |
| `wrap.c` | ajust de línia suau amb sagnat penjant |
| `md.c` | tipus de bloc Markdown i estils |
| `render.c` | dibuix amb ncursesw, barra d'estat i diàlegs |
| `command.c`, `keys.c` | registre d'ordres amb nom, dreceres i lectura de tecles |
| `menu.c`, `complete.c`, `help.c` | menús, autocompleció de la paleta i ajuda |
| `search.c`, `undo.c`, `autosave.c` | cerca, desfer/refer i autodesat |
| `spell.c` | ortografia amb hunspell, carregat amb `dlopen` |
| `palette.h`, `theme.c` | colors i temes (compartits amb Fàbula) |
| `filebrowser.c`, `dirlist.c` | navegador de fitxers |
| `outline.c`, `outlineview.c` | esquema: capçaleres i paraules per secció, i el seu diàleg |
| `footnote.c` | notes al peu: inserir, anar i tornar, renumerar |
| `calaix.c`, `calaixview.c` | calaix d'idees: format, escriptura atòmica, font activa persistent i la llista filtrable |
| `history.c`, `recentview.c` | historial: posició del cursor per document i documents recents |
| `config.c` | fitxers de configuració |
| `utf8.c`, `util.c` | UTF-8 i utilitats |

El codi comú de Phileas, Fàbula i Recerca és a `comu/`: `rc.c` (lectura del phileasrc, l'aspecte —tema, paleta i colors— i la taula de les claus `color-…`, que també fa servir `config.c`), `fold.c` (minúscules i sense accents, per a les cerques i els enllaços), `yaml.c` (llegir i canviar claus de la capçalera), `front.c` (propietats YAML d'Obsidian, també les llistes en bloc), `mdscan.c` (enllaços, etiquetes i blocs `^id`), `fterm.c` (el terminal de Fàbula i Recerca), `ui.c` (menú d'Esc, línia d'entrada, confirmació i ajuda) i `launch.c` (obrir un fitxer a l'editor i tornar).

Fàbula és a `fabula/` (`project.c` el projecte i els fitxers, `flat.c` les files de l'arbre, `fview.c` la pantalla) i reutilitza `buffer`, `outline`, `md`, `utf8`, `keys` i `theme` de `src/`. Les proves són a `tests/test_fabula.c`.

Recerca és a `recerca/` (`vault.c` l'índex SQLite i la sincronització, `model.c` el que mostra la pantalla, `rview.c` la pantalla, `main.c` les ordres). Les proves són a `tests/test_recerca.c`.

Els mòduls que no depenen de ncurses es proven a `tests/test.c`. A `tools/`
hi ha eines per a les proves interactives: `ptyrun.c` (executa Phileas en un
pseudoterminal) i `screen.py` (emulador de pantalla mínim).

Les decisions de disseny (llenguatge, estructura del buffer, ajust de línia,
dreceres, model de paràgrafs, configuració…) i el registre complet del
desenvolupament són a [`SEGUIMENT.md`](SEGUIMENT.md).

## Versions

- **2.10.1**: des de Phileas, Fàbula (0.3.5) deixa triar o crear un projecte també sense `projectes =` al phileasrc (a la carpeta actual); Recerca (0.2.1) sense vault configurat ja no indexa la carpeta actual si no és un vault, i ho explica; si Fàbula o Recerca acaben amb error, Phileas espera Retorn perquè se'n pugui llegir el missatge.
- **2.10.0**: **Phileas és la casa.** **Porta-retalls comú** de les tres eines (fitxer `retalls` al directori de configuració): el que es copia en un programa s'enganxa a un altre, també a la Pi. **Recerca 0.2.0:** `f` cerca al nom, les propietats i el text de totes les fitxes (amb l'índex FTS5; `"frase exacta"`), i `m` marca paràgrafs del visor per copiar-los (`c`, o `C` amb `[[la fitxa]]`). Menú **Eines** (Fàbula i Recerca) i ordres `fabula` i `recerca` de Phileas (abans, ordres externes de la imatge BYOK); en tornar, si el document obert s'ha canviat fora, Phileas el torna a llegir. Fàbula 0.3.4 i Recerca 0.2.0: triador de projectes (amb «Projecte nou…») i de vaults dins dels programes (`projectes =`, `recerques =`), i Ctrl+P obre el menú, com a Phileas. A la imatge BYOK, un sol llançador (`/usr/lib/byok/executa`) per a `phileas`, `ph`, `fabula` i `recerca`, i fora els triadors en shell.
- **2.9.0**: a la imatge BYOK, **una ordre per a l'intercanvi amb l'ordinador**: `ordinador` (menú BYOK → Ordinador: sincronitza…) tria l'ordinador i fa «sincronitza-ho tot» (puja textos i projectes a la darrera carpeta i porta i torna els vaults de recerca d'aquell ordinador), només puja, o gestiona els vaults; també hi aparella ordinadors nous. `envia` i `rep` comparteixen la xarxa (`/usr/lib/byok/xarxa.sh`) i tenen modes sense menú (`envia [--darrera] NOM`, `envia --nou`, `rep --ordinador NOM`). Phileas: Ordinador al menú BYOK.
- **2.8.0**: **una configuració per a les tres eines.** Opcions comunes `editor` (l'editor de Fàbula i Recerca; `FABULA_EDITOR` i `RECERCA_EDITOR` hi manen), `textos`, `projectes` i `recerques`; la plantilla de `desar-configuracio` té les seccions Comú, Phileas i Recerca, amb els valors de totes tres; `phileas --valor CLAU`. Fàbula i Recerca llegeixen els comentaris del final de línia com Phileas, i `phileas-importa-calaix` també llegeix `/etc/phileas/phileasrc`. A la imatge BYOK, els llocs surten del phileasrc (`/usr/lib/byok/llocs.sh`). Fàbula 0.3.3, Recerca 0.1.2. També, codi comú de Phileas, Fàbula i Recerca a `comu/` (lector del phileasrc, colors, accents, menú, diàlegs, terminal), sense canvis visibles. L'ajuda (F1) de Fàbula i de Recerca es desplaça amb ↑ ↓ RePàg AvPàg quan no hi cap (abans es tallava). Fàbula treu els accents amb les mateixes regles que Recerca (ara també ý, ÿ i ŀ). `make clean` ja no esborra `dist/` (ho fa `make distclean`).
- **2.7.2**: `desar-configuracio` ja no esborra les claus de Recerca (`recerca-vault`…): conserva totes les línies que Phileas no escriu, i Phileas accepta sense error les claus `recerca-…` i `fabula-…` del phileasrc. A la imatge BYOK: Fàbula al menú BYOK (tria un projecte de `/data/projectes` o en crea un); `exporta` copia al pendrive `BYOK/textos`, `BYOK/projectes` i les fitxes `_pi/` de cada vault; el bàner de la consola mostra Fàbula, Recerca i `rep`.
- **2.7.1**: Fàbula 0.3.2 i Recerca 0.1.1: el text normal fa servir els colors del tema (amb els temes clars sortia amb els del terminal, blanc sobre negre). A la imatge BYOK: `envia` pregunta la carpeta de destinació a l'ordinador (per defecte, la darrera) i hi deixa `textos/` i `projectes/` (`/data/projectes`); `rep` pot treure un vault de la llista.
- **2.7.0**: **Recerca 0.1**, la tercera eina: fitxes de recerca en un vault d'Obsidian (carpetes, propietats, etiquetes niades, enllaços amb àlies, seccions i blocs `^id`, backlinks i relacions de propietat), amb un índex SQLite (FTS5) que es posa al dia sol; pantalla de navegació, llista i visor; editar a Phileas, fitxes noves amb plantilla, anotacions i paperera; a la Pi, `rep` porta un vault del PC i hi torna `_pi/`. Phileas: ordres `externa-pantalla.NOM` (programes de pantalla sencera, tornen sense Retorn) i Recerca i `rep` al menú BYOK. Fàbula 0.3.1: `b` obre Recerca (`recerca:` a `projecte.md`). Fàbula i Recerca fan servir els colors del tema com Phileas (el text atenuat a la consola i les línies `color-…` del phileasrc).
- **2.6.0**: **Escriure net.** Cerca sense majúscules ni accents (`cafe` troba `cafè`); estadístiques del document o de la selecció (`estadistiques`: paraules, caràcters, frases, pàgines de 1.800, temps de lectura); `col.legi` → `col·legi` amb la tipografia i el diccionari català, `Alt+.` (punt volat) i `unicode NOM|CODI`; diccionari de projecte (`.diccionari.txt`, `p` als suggeriments); `revisar-aparellaments` (« » ( ) [ ] “ ” cometes i acotacions de diàleg sense tancar).
- **2.5.0**: **Fàbula 0.3**. Diari d'escriptura (`.fabula/diari.csv`; `d` i `fabula --diari`: avui, setmana, mitjana, ratxa i previsió d'acabar segons l'objectiu i la data límit); línia temporal (`T`: escenes per data, amb marca ↶ als salts enrere); menú d'Esc agrupat per categories i amb desplaçament.
- **2.4.0**: **Fàbula 0.2**. Instantànies dels fitxers (`.versions/`: automàtiques abans d'obrir, canviar l'estat o el títol, i a mà; `v` en recupera una); metadades (`pov`, `lloc`, `data`, `personatges`, `sinopsi`, `estat`) als capítols i, en un comentari sota la capçalera, a les escenes; taula (outliner) amb Tab, que s'ordena, s'edita i es filtra per qualsevol camp; enllaços `[[Nom]]` d'Obsidian (`u` on surt, `l` enllaços d'aquí, `t` trencats); cerca i substitució a tot el projecte (`f`, `R`), amb instantània prèvia; compilar el manuscrit (`C`, `fabula compila`) en un `.md` net o, amb pandoc, `.docx`/`.epub`/`.pdf`; `fabula --cerca`.
- **2.3.3**: Fàbula 0.1.1: el menú d'Esc ja no parpelleja en moure la selecció.
- **2.3.2**: **Fàbula 0.1**, l'eina `fabula` per organitzar una novel·la (arbre del projecte amb paraules, estat i progrés; nou, reanomenar, reordenar i paperera; Retorn obre el text a Phileas; capítol 16 del manual); opció `+LÍNIA` per obrir un document a un paràgraf; el separador `***` es centra també amb espais als costats.
- **2.3.1**: manual d'ús (`MANUAL.md`, i `make manual` per al PDF); drecera Alt+T per activar o desactivar la tipografia automàtica; pendents (C11): ressaltat de `TODO`, `[?]` i `%%nota%%` i ordre `pendents` (Alt+K).
- **2.3.0**: menú **Idees** (després de Vista) amb les ordres del calaix; «Vault d'Obsidian…», «Calaix de la Pi…» i «Importa a Obsidian» des de Phileas; la configuració és al phileasrc i desapareix el fitxer TOML i `--inicia`.
- **2.2.7**: `phileas-importa-calaix --inicia VAULT` crea la configuració mínima (només cal el vault; la resta té valors per defecte); l'avís quan falta la configuració ho explica; model reordenat amb el mínim a dalt.
- **2.2.6**: `phileas-importa-calaix` instal·lat fa servir el `calaix.md` de la carpeta des d'on es crida (abans el buscava a `/usr/bin`); la 2.2.5 no es va publicar.
- **2.2.5**: arregla `phileas-importa-calaix` instal·lat: cerca `calaix.md` al costat de l'script, a la carpeta actual i a `~/BYOK` (abans només al costat de l'script, és a dir `/usr/bin`).
- **2.2.4**: el `.deb` instal·la l'importador com a ordre (`phileas-importa-calaix`), depèn de `python3` ≥ 3.11, i l'importador explica com copiar el model de configuració si falta.
- **2.2.3**: l'importador del calaix a Obsidian escriu la propietat `font` sempre com a enllaç `[[font]]` al document de la font (`resum` en canvia el nom).
- **2.2.2**: les idees del calaix tenen un títol opcional (`### títol`, després de la capçalera; F9 el demana després de la pàgina; la llista del calaix el mostra); es deixa de llegir el calaix en el format antic.
- **2.2.1**: «Font del calaix» és un camp editable amb la font actual (esborrar-la la treu; sense Tab ni `-`); s'elimina «Selecció al calaix», perquè F9 ja envia la selecció.
- **2.2.0**: calaix orientat a la referència: capçalera `font · p. pàgina · #etiquetes` i peu `∙ data · document:línia`; font activa persistent (`calaix-font`, F9 la proposa i la barra d'estat la mostra) i pàgina per defecte. El calaix antic es continua llegint.
- **2.1.7**: una línia que només conté `***` es mostra centrada a la pantalla (només vista; el fitxer no canvia).
- **2.1.6**: les llistes ja no pugen ni baixen en escriure després de dos espais al final d'un paràgraf.
- **2.1.5**: drecera F10 / Alt+M per canviar el mode de la màquina d'escriure (salt, fixa, clàssic).
- **2.1.4**: màquina d'escriure: un document nou comença a dalt, amb dues files en blanc (també en clàssic), i el mode de desplaçament surt a la barra d'estat.
- **2.1.3**: calaix d'idees (F9, Edita → Selecció al calaix, Fitxer → Calaix d'idees; opció `calaix`).
- **2.1.2**: separadors als menús; les notes al peu no perden el text que va després de dos espais al final d'una línia.
- **2.1.1** (2026-10-30): notes al peu (F5 nova, F6 va i torna,
  `renumerar-notes`); documents recents (Alt+O) i posició del cursor per
  document, a l'`historial` del directori de configuració; porta-retalls
  compartit amb les altres aplicacions de l'escriptori; un fitxer nou sense
  extensió es crea amb `.md`.
- **2.1** (2026-10-29): esquema del document (F2 o Alt+E), amb les
  capçaleres i les paraules de cada secció; moure paràgrafs i, des de
  l'esquema, seccions senceres (Alt+↑/↓); enfocament de paràgraf o de frase
  (F4), amb un color atenuat propi de cada tema a la consola; tipografia en
  escriure (— « » … ’); la cerca no distingeix ' de ’.
- **2.0.3** (2026-10-29): «Quant a Phileas» al menú Ajuda i configuració
  (versió de Phileas i, a la BYOK, de la imatge); tema WordPerfect (gris
  sobre blau, com el WordPerfect 5.1 per a DOS); fora de la consola, el
  cursor pren el color del tema (es veu amb els temes clar i sèpia).
- **2.0.2** (2026-10-29): recompte de paraules a la barra d'estat; temes de
  colors (terminal, fosc, clar, sèpia, fòsfor), també a la consola de Linux;
  les opcions de Vista (tema, centrar, colors, màquina d'escriure) es desen
  soles; ordre `desar-configuracio`; `--config` amb els valors actuals.
- **2.0.1** (2026-10-28): llançador per a l'escriptori (GNOME, KDE...) al
  paquet `.deb`.
- **2.0** (2026-10-27): menús a l'estil WordGrinder amb Esc, amb un submenú
  BYOK per a les ordres externes.
- **1.0** (2026-10-26): primera versió completa, integrada a la imatge BYOK
  per a Raspberry Pi 1B.

## Llicència

Copyright © 2026 Àngel Solans.

Phileas és programari lliure: el podeu redistribuir i modificar d'acord amb
els termes de la [Llicència Pública General de GNU](LICENSE), tal com la
publica la Free Software Foundation, versió 3 o (a la vostra elecció)
qualsevol de posterior (GPL-3.0-or-later). Es distribueix sense cap
garantia.
