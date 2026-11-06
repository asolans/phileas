#!/usr/bin/env python3
"""Importa el calaix de Phileas (calaix.md) al vault d'Obsidian.

Normalment s'executa des de Phileas (Idees → Importa a Obsidian). Fora
de Phileas: phileas-importa-calaix [--calaix FITXER]... [--vault CARPETA]
[--inbox NOM] [--camp-tipus NOM] [--simula] [--reimporta] [--quiet].

Sense opcions, el vault, la carpeta d'entrada i la propietat del tipus són els
de Phileas (opcions obsidian-vault, obsidian-inbox i obsidian-tipus del
phileasrc), i els calaixos, els de les opcions «calaix» i «calaix-pi» (el que
arriba de la Pi amb «envia»); els que no existeixen, es deixen. Cada nota
porta `calaix: ID` i què ja s'ha importat ho diu el vault mateix: es compta
encara que l'hagis moguda o reanomenada; si l'esborres, es torna a crear.
"""
import argparse
import hashlib
import os
import re
import subprocess
import sys
import unicodedata
from pathlib import Path


def dir_config():
    return Path(os.environ.get("PHILEAS_CONFIG") or os.path.join(
        os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "phileas"))


def llegeix_phileasrc():
    """Les opcions «clau = valor» del phileasrc de l'usuari (les de Phileas)."""
    opcions = {}
    try:
        for linia in (dir_config() / "phileasrc").read_text(encoding="utf-8").splitlines():
            linia = linia.strip()
            if not linia or linia.startswith("#") or "=" not in linia:
                continue
            k, v = linia.split("=", 1)
            opcions[k.strip()] = v.split("  #")[0].strip()
    except OSError:
        pass
    return opcions


TAG = re.compile(r"(?<!\S)#(\S+)")
TIPUS = {"idea": "Idea", "cita": "Cita", "observació": "Observació", "observacio": "Observació"}


FOOT = re.compile(r"^∙ (\d{4}-\d{2}-\d{2})(?: · (.*))?$")


def entrada_nova(linia):
    e = {"cos": [], "titol": "", "font": "", "pagina": "", "tags": [], "origen": "", "data": ""}
    parts = [p.strip() for p in linia[3:].split(" · ")]
    if parts == ["(idea)"]:
        parts = []
    if parts and not parts[0].startswith("#"):
        e["font"] = parts.pop(0)
        if parts and not parts[0].startswith("#"):
            e["pagina"] = re.sub(r"^p\. ?", "", parts.pop(0))
    for p in parts:
        e["tags"] += TAG.findall(p)
    return e


def analitza(text):
    """Retorna les entrades [{data, font, pagina, titol, origen, tags, cos, id}].

    Format: «## font · p. pàgina · #etiquetes», un títol opcional («### títol»,
    primera línia després de la capçalera; «###» sola = sense títol), el text i
    el peu «∙ data · origen». Una entrada sense peu no està tancada i s'ignora.
    """
    entrades, actual, dins = [], None, False
    for linia in text.splitlines():
        if not dins:
            if linia.startswith("## "):
                actual = entrada_nova(linia)
                entrades.append(actual)
                dins = True
            continue
        f = FOOT.match(linia)
        if f:
            actual["data"] = f.group(1)
            actual["origen"] = f.group(2) or ""
            dins = False
        else:
            actual["cos"].append(linia)
    entrades = [e for e in entrades if e["data"]]
    for e in entrades:
        cos = e["cos"]
        while cos and not cos[0].strip():
            cos.pop(0)
        if cos and (cos[0] == "###" or cos[0].startswith("### ")):
            e["titol"] = cos.pop(0)[3:].strip()
        e["cos"] = "\n".join(cos).strip("\n")
        clau = "\n".join([e["font"], e["pagina"], e["cos"]])
        e["id"] = e["data"] + "-" + hashlib.sha1(clau.encode()).hexdigest()[:8]
    return entrades


PREFIX_PI = "/data/textos/"   # el document d'origen, relatiu a la carpeta de textos de la Pi


def tags_i_tipus(e):
    """(tags netes, tipus). Les etiquetes @... eren de l'antic selector de projecte: s'ignoren."""
    tags, tipus = [], None
    for t in e["tags"]:
        if t.startswith("@"):
            continue
        if t.lower() in TIPUS:
            tipus = TIPUS[t.lower()]
        else:
            tags.append(t)
    return tags, tipus


def tipus_auto(cos):
    c = cos.strip()
    if c.startswith((">", '"', "«", "“")):
        return "Cita"
    return "Idea"


def neteja(t):
    return unicodedata.normalize("NFC", re.sub(r'[\\/:*?"<>|#^\[\]]', "", t)).strip()


def nom_fitxer(e):
    """«font títol». Sense títol, les primeres paraules de la idea."""
    titol = e["titol"]
    if not titol:
        titol = " ".join(re.sub(r"^[>\s\"«“]+", "", e["cos"]).split()[:6])[:50]
    nom = " ".join(x for x in (neteja(e["font"]), neteja(titol)) if x)
    return (nom.rstrip(". ") or f"{e['data']} idea") + ".md"


def nota(e, tags, tipus, camp):
    font = e["font"]
    if font:
        # Enllaç al document de la font al vault («[[Prous, 1936]]»)
        enllac = neteja(font)
        font = f"\"[[{enllac}]]\"" if enllac else ""
    fm = ["---", f"{camp}: {tipus}"]
    if e["titol"]:
        fm.append("títol: \"" + e["titol"].replace('"', "'") + "\"")
    if font:
        fm.append(f"font: {font}")
    if e["pagina"]:
        fm.append(f"pàgina: \"{e['pagina']}\"")
    origen = e["origen"]
    if origen.startswith(PREFIX_PI):
        origen = origen[len(PREFIX_PI):]
    fm += [f"creat: {e['data']}",
           f"origen: \"{origen}\"" if origen else "origen:",
           "tags: [" + ", ".join(tags) + "]",
           "estat: nova",
           f"calaix: {e['id']}",
           "---", ""]
    cos = e["cos"]
    if tipus == "Cita" and not cos.lstrip().startswith(">"):
        cos = "\n".join("> " + l if l else ">" for l in cos.splitlines())
    return "\n".join(fm) + cos + "\n"


BASE = """views:
  - type: table
    name: Pendents
    filters:
      and:
        - file.inFolder("{inbox}")
        - 'estat == "nova"'
    order:
      - file.name
      - títol
      - font
      - tags
      - origen
    sort:
      - property: creat
        direction: DESC
properties:
  file.name:
    displayName: Nom
  títol:
    displayName: Títol
"""


def ja_importats(vault):
    """IDs de les notes del vault que venen del calaix (estiguin on estiguin)."""
    ids = set()
    pat = re.compile(r"^calaix: (\S+)\s*$", re.M)
    for r, dirs, fitxers in os.walk(vault):
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        for f in fitxers:
            if f.endswith(".md"):
                try:
                    with open(os.path.join(r, f), encoding="utf-8") as fh:
                        m = pat.search(fh.read(1500))
                except (OSError, UnicodeDecodeError):
                    continue
                if m:
                    ids.add(m.group(1))
    return ids


def avisa(titol, text):
    """Terminal si n'hi ha; si no (llançador gràfic), una finestra."""
    print(text)
    if sys.stdout.isatty():
        return
    try:
        subprocess.run(["zenity", "--info", "--title", titol, "--no-markup", "--text", text], timeout=600)
    except (OSError, subprocess.SubprocessError):
        try:
            subprocess.run(["notify-send", titol, text])
        except OSError:
            pass


def inicia(camí, vault):
    """Crea la configuració mínima amb un vault; no toca una configuració existent."""
    v = Path(vault).expanduser()
    if not v.is_dir():
        avisa("Calaix", f"{v} no és una carpeta. Indica la carpeta del vault d'Obsidian.")
        return 1
    if camí.exists():
        avisa("Calaix", f"Ja existeix {camí}; no la toco. Edita-la a mà.")
        return 1
    nom = re.sub(r"[^a-z0-9_-]+", "-", v.resolve().name.lower()).strip("-") or "vault"
    camí.parent.mkdir(parents=True, exist_ok=True)
    camí.write_text(f"""# Configuració de phileas-importa-calaix. Només cal el vault.
[vaults.{nom}]
vault = "{vault}"

# Avançat (opcional); vegeu importa-calaix.exemple.toml:
#   inbox = "Inbox-byok"      carpeta d'entrada del vault
#   camp_tipus = "tipus"      nom de la propietat del tipus (Idea, Cita, Observació)
#   [fonts."Prous, 1935"]     fonts conegudes, i `textos` / `defecte` si hi ha diversos vaults
""", encoding="utf-8")
    avisa("Calaix", f"Configuració creada: {camí}\nVault «{nom}» = {vault}\nAra ja pots executar phileas-importa-calaix des de la carpeta del calaix.md.")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--calaix", action="append", metavar="FITXER",
                    help="calaix a importar (es pot repetir); per defecte, els del phileasrc")
    ap.add_argument("--vault", help="carpeta del vault d'Obsidian (per defecte, obsidian-vault)")
    ap.add_argument("--inbox", help="carpeta d'entrada del vault (per defecte, obsidian-inbox)")
    ap.add_argument("--camp-tipus", help="propietat del tipus (per defecte, obsidian-tipus)")
    ap.add_argument("--simula", action="store_true", help="mostra què es faria, sense escriure res")
    ap.add_argument("--reimporta", action="store_true", help="torna a crear també les notes que ja hi són")
    ap.add_argument("--quiet", action="store_true", help="només escriu al terminal, sense finestres")
    a = ap.parse_args()
    rc = llegeix_phileasrc()
    avis = (lambda t, x: print(x)) if a.quiet else avisa
    vault = a.vault or rc.get("obsidian-vault")
    if not vault:
        avis("Calaix", "No hi ha cap vault d'Obsidian.\nA Phileas: Idees → Vault d'Obsidian…  (o --vault CARPETA)")
        return 1
    vault = os.path.expanduser(vault)
    if not os.path.isdir(vault):
        avis("Calaix", f"El vault {vault} no és una carpeta.")
        return 1
    nom_inbox = a.inbox or rc.get("obsidian-inbox", "Inbox-byok")
    camp = a.camp_tipus or rc.get("obsidian-tipus", "tipus")
    if a.calaix:
        calaixos = [Path(c).expanduser() for c in a.calaix]
    else:
        calaixos = [Path(os.path.expanduser(rc.get(k, d))) for k, d in
                    (("calaix", "~/calaix.md"), ("calaix-pi", "~/BYOK/calaix.md"))]
    calaixos = [c for i, c in enumerate(calaixos) if c.exists() and c not in calaixos[:i]]
    if not calaixos:
        avis("Calaix", "No hi ha cap calaix.md. Escriu idees amb F9 o executa «envia» a la Pi.")
        return 1
    entrades, vistos = [], set()
    for c in calaixos:
        for e in analitza(c.read_text(encoding="utf-8")):
            if e["id"] not in vistos:   # la mateixa idea en dos calaixos, només un cop
                vistos.add(e["id"])
                entrades.append(e)
    existents = ja_importats(vault)
    inbox = Path(vault) / nom_inbox
    nous = ja = 0
    linies = []
    for e in entrades:
        if e["id"] in existents and not a.reimporta:
            ja += 1
            continue
        tags, tipus = tags_i_tipus(e)
        tipus = tipus or tipus_auto(e["cos"])
        dest = inbox / nom_fitxer(e)
        if dest.exists():
            dest = dest.with_name(dest.stem + " " + e["id"][-6:] + ".md")
        nous += 1
        linies.append(str(dest.relative_to(vault)))
        if not a.simula:
            inbox.mkdir(parents=True, exist_ok=True)
            dest.write_text(nota(e, tags, tipus, camp), encoding="utf-8")
            base = inbox / "Inbox-byok.base"
            if not base.exists():
                base.write_text(BASE.format(inbox=nom_inbox), encoding="utf-8")
    resum = (f"{'Es crearien' if a.simula else 'Importades'}: {nous} notes"
             f"{' (ja hi havia ' + str(ja) + ')' if ja else ''}.")
    if linies and len(linies) <= 12:
        resum += "\n\n" + "\n".join(linies)
    avis("Calaix → Obsidian", resum)
    return 0


if __name__ == "__main__":
    sys.exit(main())
