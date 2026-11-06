#!/usr/bin/env python3
"""Importa el calaix de Phileas (calaix.md, rebut amb `envia`) al vault d'Obsidian.

Sense paràmetres: crea una nota a l'Inbox del vault per cada idea nova i avisa
del resultat (al terminal, o amb una finestra si es crida des de l'entorn gràfic).
Què ja s'ha importat ho diu el vault mateix: cada nota porta `calaix: ID` i
es compta encara que l'hagis moguda o reanomenada; si l'esborres, es torna a crear.

Opcions (no cal cap): --simula (no escriu res), --reimporta (torna a crear
també les que ja hi són), --calaix FITXER, --config FITXER i --inicia VAULT (crea la configuració).
Sense --calaix, fa servir el calaix.md de la carpeta des d'on es crida.

Els vaults, la carpeta d'entrada i la resta de la configuració són en un fitxer
TOML propi de cada usuari, `importa-calaix.toml` al directori de configuració de
Phileas (~/.config/phileas); vegeu importa-calaix.exemple.toml.
"""
import argparse
import hashlib
import os
import re
import subprocess
import sys
import tomllib
import unicodedata
from pathlib import Path

TOML_EXEMPLE = "importa-calaix.exemple.toml"


def camí_config(arg=None):
    """--config, o importa-calaix.toml al directori de configuració de Phileas."""
    if arg:
        return Path(arg).expanduser()
    base = os.environ.get("PHILEAS_CONFIG") or os.path.join(
        os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "phileas")
    return Path(base) / "importa-calaix.toml"


def llegeix_config(camí):
    """Carrega la configuració (TOML) i la deixa en la forma que fa servir l'script."""
    with open(camí, "rb") as f:
        c = tomllib.load(f)
    vaults = c.get("vaults", {})
    if not vaults:
        raise ValueError("no hi ha cap [vaults.NOM]")
    for nom, v in vaults.items():
        if "vault" not in v:
            raise ValueError(f"[vaults.{nom}] no té «vault»")
    defecte = c.get("defecte") or next(iter(vaults))
    if defecte not in vaults:
        raise ValueError(f"«defecte» = «{defecte}» no és a [vaults]")
    return {"vaults": vaults, "inbox": c.get("inbox", "Inbox-byok"), "defecte": {"projecte": defecte},
            "textos": {"prefixos": c.get("textos", [])}, "fonts": c.get("fonts", {})}


TAG = re.compile(r"(?<!\S)#(\S+)")
TIPUS = {"idea": "Idea", "cita": "Cita", "observació": "Observació", "observacio": "Observació"}
cfg = {}


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


def treu_prefix(camí, cfg):
    """Camí relatiu a la carpeta de textos configurada, o None."""
    for pre in cfg.get("textos", {}).get("prefixos", []):
        pre = os.path.expanduser(pre).rstrip("/") + "/"
        if camí.startswith(pre):
            return camí[len(pre):]
    return None


def projecte_de(e, cfg):
    """(projecte, tags netes, tipus). Prioritat: #@projecte, la font, la carpeta del document, defecte."""
    tags, projecte, tipus = [], None, None
    for t in e["tags"]:
        if t.startswith("@"):
            projecte = t[1:]
        elif t.lower() in TIPUS:
            tipus = TIPUS[t.lower()]
        else:
            tags.append(t)
    if not projecte and e["font"]:
        projecte = cfg.get("fonts", {}).get(e["font"], {}).get("projecte")
    if not projecte:
        camí = re.sub(r":\d+$", "", e["origen"]) if re.search(r":\d+$", e["origen"]) else e["origen"].split(",")[0].strip()
        rel = treu_prefix(camí, cfg)
        if rel is None and not camí.startswith("/"):
            rel = camí
        if rel and "/" in rel:
            projecte = rel.split("/")[0]
    if not projecte:
        projecte = cfg.get("defecte", {}).get("projecte")
    return projecte, tags, tipus


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


def nota(e, tags, tipus, v, cfg):
    camp = v.get("camp_tipus", "tipus")
    font = e["font"]
    if font:
        # Enllaç al document de la font al vault («[[Prous, 1936]]»); `resum` el canvia
        enllac = cfg.get("fonts", {}).get(font, {}).get("resum") or neteja(font)
        font = f"\"[[{enllac}]]\"" if enllac else ""
    fm = ["---", f"{camp}: {tipus}"]
    if e["titol"]:
        fm.append("títol: \"" + e["titol"].replace('"', "'") + "\"")
    if font:
        fm.append(f"font: {font}")
    if e["pagina"]:
        fm.append(f"pàgina: \"{e['pagina']}\"")
    origen = e["origen"]
    m = re.match(r"^(.*?)(:\d+)?$", origen)
    rel = treu_prefix(m.group(1), cfg) if m else None
    if rel is not None:
        origen = rel + (m.group(2) or "")
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
    ap.add_argument("--calaix", default=None)
    ap.add_argument("--config", help="fitxer de configuració (per defecte ~/.config/phileas/importa-calaix.toml)")
    ap.add_argument("--inicia", metavar="VAULT", help="crea la configuració amb aquest vault d'Obsidian i surt")
    ap.add_argument("--simula", action="store_true", help="mostra què es faria, sense escriure res")
    ap.add_argument("--reimporta", action="store_true", help="torna a crear també les notes que ja hi són")
    a = ap.parse_args()
    if not a.calaix:
        a.calaix = str(Path.cwd() / "calaix.md")  # el que `envia` va deixar a la carpeta des d'on es crida

    camí = camí_config(a.config)
    if a.inicia:
        return inicia(camí, a.inicia)
    try:
        cfg.update(llegeix_config(camí))
    except FileNotFoundError:
        avisa("Calaix", f"No trobo la configuració {camí}.\nCrea-la amb el teu vault d'Obsidian:\n"
                        "  phileas-importa-calaix --inicia ~/Documents/ElTeuVault")
        return 1
    except (OSError, ValueError, tomllib.TOMLDecodeError) as exc:
        avisa("Calaix", f"Configuració no vàlida ({camí}):\n{exc}")
        return 1
    VAULTS, DEFECTE, INBOX = cfg["vaults"], cfg["defecte"]["projecte"], cfg["inbox"]
    if not Path(a.calaix).exists():
        avisa("Calaix", f"No trobo {a.calaix}.\nPrimer cal executar «envia» a la Pi.")
        return 1
    entrades = analitza(Path(a.calaix).read_text(encoding="utf-8"))
    existents = {}
    nous = ja = 0
    errors = []
    linies = []
    for e in entrades:
        projecte, tags, tipus = projecte_de(e, cfg)
        if projecte not in VAULTS:
            projecte = DEFECTE
        v = VAULTS.get(projecte) if projecte else None
        if v is None:
            errors.append(e["id"])
            continue
        vault = os.path.expanduser(v["vault"])
        if vault not in existents:
            existents[vault] = ja_importats(vault)
        if e["id"] in existents[vault] and not a.reimporta:
            ja += 1
            continue
        tipus = tipus or tipus_auto(e["cos"])
        inbox = Path(vault) / v.get("inbox", INBOX)
        dest = inbox / nom_fitxer(e)
        if dest.exists():
            dest = dest.with_name(dest.stem + " " + e["id"][-6:] + ".md")
        nous += 1
        linies.append(str(dest.relative_to(vault)))
        if not a.simula:
            inbox.mkdir(parents=True, exist_ok=True)
            dest.write_text(nota(e, tags, tipus, v, cfg), encoding="utf-8")
            base = inbox / "Inbox-byok.base"
            if not base.exists():
                base.write_text(BASE.format(inbox=v.get("inbox", INBOX)), encoding="utf-8")
    resum = (f"{'Es crearien' if a.simula else 'Importades'}: {nous} notes"
             f"{' (ja hi havia ' + str(ja) + ')' if ja else ''}."
             + (f"\nSense vault: {len(errors)} entrades." if errors else ""))
    if linies and len(linies) <= 12:
        resum += "\n\n" + "\n".join(linies)
    avisa("Calaix → Obsidian", resum)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
