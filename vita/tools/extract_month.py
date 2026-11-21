"""Extrai dados de um jogo "Rise of the Half Moon" a partir do JavaScript original (já formatado
com jsbeautifier) e grava um JSON usado pela app nativa.

Uso: python -I extract_month.py <pretty.js> <month_id> <out.json> <pylib_dir>
"""
import json
import re
import sys

PRETTY, MONTH, OUT, PYLIB = sys.argv[1:5]
sys.path.insert(0, PYLIB)
from py_mini_racer import MiniRacer  # noqa: E402

src = open(PRETTY, encoding="utf-8").read()
js = MiniRacer()

SHEETS = [
    "play-sprite.png", "background-sprite.png", "moon-sprite-dark-moon-eye-open-in.png",
    "moon-sprite-dark-moon-eye-open-out.png", "moon-sprite-dark-moon-fade-in.png",
    "moon-sprite-light-moon-fade-in.png", "moon-sprite-light-moon-fade-out.png",
    "moon-sprite-light-moon-happy.png", "moon-sprite-light-moon-sad.png",
    "moon-sprite-light-moon-wildcard-gift.png", "moon-sprite-light-moon-win.png",
    "moon-sprite-light-moon-win-fade-out.png", "cards-sprite.png", "spinner-sprite.png",
    "progress-empty-sprite.png", "progress-star1-empty-to-full-sprite.png",
    "progress-star1-full-sprite.png", "progress-star1-full-to-empty-sprite.png",
    "progress-star2-empty-to-full-sprite.png", "progress-star2-full-sprite.png",
    "progress-star2-full-to-empty-sprite.png", "progress-star3-empty-to-full-sprite.png",
    "progress-star3-full-sprite.png", "rules-sprite.png",
]


def match_close(text, start):
    """Devolve o índice logo após o fecho do bloco que abre em text[start] ('{' ou '[')."""
    pairs = {"{": "}", "[": "]", "(": ")"}
    stack = []
    i = start
    in_str = None
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in "\"'`":
            in_str = c
        elif c in pairs:
            stack.append(pairs[c])
        elif c in "}])":
            stack.pop()
            if not stack:
                return i + 1
        i += 1
    raise ValueError("unbalanced")


def open_before(text, pos, ch="{"):
    """Índice do caracter de abertura que contém a posição pos."""
    depth = 0
    i = pos
    close = {"{": "}", "[": "]"}[ch]
    while i >= 0:
        c = text[i]
        if c == close:
            depth += 1
        elif c == ch:
            if depth == 0:
                return i
            depth -= 1
        i -= 1
    raise ValueError("no open")


def ev(expr, ctx=""):
    return json.loads(js.eval(f"(function(){{ {ctx}; return JSON.stringify({expr}); }})()"))


# ---- variáveis de frames simples: NOME = [n, n, n, n, n(, n)]
frame_vars = {k: re.sub(r"\s+", " ", v) for k, v in re.findall(
    r"(?<![\w$.])([A-Za-z_$][\w$]*) = (\[\d+,\s*\d+,\s*\d+,\s*\d+,\s*\d+(?:,\s*[\d.]+)?\s*\])", src)}
ctx = "".join(f"var {k} = {v};" for k, v in frame_vars.items())

out = {"month": MONTH}

# ---- lista de folhas de sprites
p = src.index('filename: "play-sprite.png"')
s = open_before(src, p, "[")
sheets = ev(src[s:match_close(src, s)])
out["sheets"] = [{"file": x["filename"], "w": x["size"][0], "h": x["size"][1]} for x in sheets]
sheet_names = [x["filename"] for x in sheets]


def sheet_of(frame):
    return sheet_names[frame[0]]


# ---- frames estáticos e animações (classe com BACK: ...)
if ", 1980, 1280]" in src:
    p = src.index(", 1980, 1280]")
    s = open_before(src, p, "{")
    frames_raw = ev(src[s:match_close(src, s)], ctx)
    p2 = re.compile(r"this\.[\w$]+ = \{\n").search(src, p).start()
    s2 = src.index("{", p2)
    anims_raw = ev(src[s2:match_close(src, s2)], ctx)
else:
    # Motor antigo (outubro/novembro): frames e animações são variáveis soltas a seguir à lista de folhas.
    p = src.index('filename: "rules-sprite.png"')
    end = src.index("\n var ", p)
    block = src[p:end]
    frames_raw = {k: json.loads(v) for k, v in frame_vars.items() if src.find(f"  {k} = [", p, end) >= 0}
    # As animações estão espalhadas (variáveis e listas dentro dos Maps da Lua e do progresso): procura todas
    # as listas de frames do ficheiro e fica com a mais longa de cada folha.
    best = {}
    for m in re.finditer(r"\[\s*\n\s*\[\d+,\s*\d+,\s*\d+,\s*\d+,\s*\d+\s*\]", src):
        st = m.start()
        try:
            seq = json.loads(re.sub(r"\s+", "", src[st:match_close(src, st)]))
        except ValueError:
            continue
        if len(seq) < 2 or not all(isinstance(f, list) and len(f) == 5 and f[0] == seq[0][0] for f in seq):
            continue
        if len(seq) > len(best.get(seq[0][0], [])):
            best[seq[0][0]] = seq
    anims_raw = {}
    for sheet_i in sorted(best):
        anims_raw[f"s{sheet_i}"] = best[sheet_i]
        if sheet_names[sheet_i] == "progress-empty-sprite.png":  # usada pelas três estrelas
            anims_raw[f"s{sheet_i}b"] = best[sheet_i]
            anims_raw[f"s{sheet_i}c"] = best[sheet_i]

frames = {}
for k, f in frames_raw.items():
    sh = sheet_of(f)
    if sh == "cards-sprite.png" and f[3] == 100 and f[1] == 0 and f[2] == 203:
        frames["card_back"] = f
    elif sh == "cards-sprite.png" and f[3] == 50:
        frames["tile"] = f
    elif sh == "cards-sprite.png" and f[3] == 200 and f[1] == 0 and f[2] == 0:
        frames["wildcard_back"] = f
    elif sh == "background-sprite.png" and f[3] >= 1900:
        frames["stars"] = f
    elif sh == "background-sprite.png":
        frames.setdefault("bg_moons", []).append(f)
    elif sh == "rules-sprite.png":
        frames.setdefault("rules", []).append(f)
    elif sh == "progress-empty-sprite.png":
        frames.setdefault("progress_empty", []).append(f)
frames.get("bg_moons", []).sort(key=lambda f: f[1])
# Motor antigo: regras, céu estrelado e estrelas vazias do progresso estão no CSS / em listas, com as mesmas
# coordenadas dos meses novos (as folhas têm o mesmo tamanho).
_si = {n: i for i, n in enumerate(sheet_names)}
if "rules" not in frames and "rules-sprite.png" in _si:
    r = _si["rules-sprite.png"]
    frames["rules"] = [[r, 390, 122, 111, 58], [r, 273, 61, 131, 58], [r, 273, 122, 114, 58], [r, 0, 0, 270, 131],
                       [r, 273, 0, 133, 58]]
if "stars" not in frames:
    frames["stars"] = [_si["background-sprite.png"], 0, 0, 1980, 1280]
if "progress_empty" not in frames and "progress-empty-sprite.png" in _si:
    e = _si["progress-empty-sprite.png"]
    frames["progress_empty"] = [[e, 1910, 0, 113, 113], [e, 0, 0, 188, 188], [e, 1528, 191, 188, 188]]
if "rules" in frames and len(frames["rules"]) == 5:
    r = frames["rules"]
    frames["rules"] = {"bonus_dark": r[0], "fullmoon": r[1], "bonus_light": r[2], "cycle": r[3], "phase": r[4]}
out["frames"] = frames

anims = {}
for k, seq in anims_raw.items():
    if not seq:
        continue
    sh = sheet_of(seq[0]).replace(".png", "").replace("-sprite", "").replace("moon-sprite-", "")
    name = sh
    n = 2
    while name in anims:
        name = f"{sh}#{n}"
        n += 1
    anims[name] = {"key": k, "frames": seq}
out["anims"] = anims

# ---- cartas de lua (face, verso, clara=jogador, escura=lua)
m = re.search(r"new_moon: \{\s*\n\s*[\w$]+: [\w$]+,\s*\n\s*back: ", src)
if not m:  # motor antigo: { type: "moon", id, face, back, light, dark, value }
    m = re.search(r'new_moon: \{\s*\n\s*type: "moon",', src)
s = open_before(src, m.start(), "{")
cards_obj = src[s:match_close(src, s)]
cards = ev(cards_obj, ctx)
out["cards"] = {}
for phase, d in cards.items():
    vals = [v for v in d.values() if isinstance(v, list)]
    out["cards"][phase] = {"face": vals[0], "back": vals[1], "light": vals[2], "dark": vals[3]}

# ---- wildcards
wild = []
for m in re.finditer(r'type: "(legendary_wildcard|wildcard)",\s*\n\s*id: "([\w]+)"', src):
    s = open_before(src, m.start(), "{")
    body = src[s:match_close(src, s)]
    arrays = re.findall(r"\b\w+: (\[\d+, \d+, \d+, \d+, \d+\]|[A-Za-z_$][\w$]*),", body)
    texts = dict(re.findall(r'\b(\w+): "(wc_[\w]+)"', body))
    emoji = re.findall(r'\b\w+: "((?:\\u[0-9a-f]{4})+)"', body)
    fr = []
    for a in arrays:
        if a.startswith("["):
            fr.append(json.loads(a))
        elif a in frame_vars:
            fr.append(json.loads(frame_vars[a]))
    if any(w["id"] == m.group(2) for w in wild):
        continue
    wild.append({
        "id": m.group(2), "type": m.group(1),
        "front": fr[0] if fr else None, "back": fr[1] if len(fr) > 1 else None,
        "texts": list(texts.values()),
        "emoji": json.loads('"' + emoji[0] + '"') if emoji else "",
    })
# Motor antigo: os textos estão num objeto à parte ({ id: { title, description, popup, info, query } }).
for w in wild:
    if len(w["texts"]) < 5:
        m = re.search(r"\b" + w["id"] + r": \{\s*\n\s*title: ", src)
        if m:
            s = src.index("{", m.start())
            w["texts"] = re.findall(r'\b\w+: "([\w]+)"', src[s:match_close(src, s)])
out["wildcards"] = wild

# ---- tabuleiros
boards = {}
board_var = {}  # variável JS -> id do tabuleiro (formato de dezembro)
for m in re.finditer(r'id: "([\w]+)",\s*\n\s*tiles: \[\{', src):
    s = open_before(src, m.start(), "{")
    b = ev(src[s:match_close(src, s)])
    if "neighbors" in b:
        boards[b["id"]] = b
        mv = re.search(r"([\w$]+) = $", src[max(0, s - 40):s])
        if mv:
            board_var[mv.group(1)] = b["id"]
out["boards"] = boards

# listas de tabuleiros por nível: ["a", "b"].map(x => W[x])  e  X = W.nome
lists = re.findall(r'\[\s*("\w+"(?:,\s*"\w+")*)\s*\]\.map\(\w+ =>\s*[\w$]+\[\w+\]\)', src)
out["board_lists"] = [json.loads("[" + l + "]") for l in lists]
out["board_refs"] = re.findall(r"(?<![\w$])([\w$]+) = [\w$]+\.([a-z][\w]+),?\n", src)
# Configuração da campanha (mesma ordem em todos os meses novos): debug, tutorial, 1.º nível,
# 1.º nível se já viu outro mês, 2.º, 3.º e as três listas para os níveis seguintes.
_refs = [r[1] for r in out["board_refs"] if r[1] in boards]
if len(_refs) >= 6 and len(out["board_lists"]) >= 3:
    out["campaign"] = {"tutorial": _refs[1], "first": _refs[2], "firstIfSeen": _refs[3], "second": _refs[4],
                       "third": _refs[5], "lists": out["board_lists"][-3:]}
else:
    # Dezembro: c = seen > 0 ? A : B; c = sem wildcards ? [c, C, D] : [c, R(l2), R(l3)]; senão [R(l1), R(l2), R(l3)]
    m = re.search(r"[\w$]+ > 0 \? ([\w$]+) : ([\w$]+);\s*c = [^;]*?\? \[c, ([\w$]+), ([\w$]+)\] : "
                  r"\[c, [\w$]+\(([\w$]+)\), [\w$]+\(([\w$]+)\)\]\s*\} else c = \[[\w$]+\(([\w$]+)\)", src)
    m2 = None
    if not m:  # outubro: c = vitórias === 0 ? (sem wildcards ? [A, B, C] : [A, R(l2), R(l3)]) : [R(l1), R(l2), R(l3)]
        m2 = re.search(r"\? \[([\w$]+), ([\w$]+), ([\w$]+)\] : \[[\w$]+, [\w$]+\(([\w$]+)\), [\w$]+\(([\w$]+)\)\] : "
                       r"\[[\w$]+\(([\w$]+)\)", src)

    def board_list(v):
        # Elementos da lista: variáveis de tabuleiros ou objetos { id: "...", ... } escritos em linha.
        mm = re.search(r"(?<![\w$.])" + re.escape(v) + r" = \[", src)
        st = mm.end() - 1
        en = match_close(src, st)
        res, i, depth, item_start = [], st + 1, 0, st + 1
        while i < en:
            c = src[i]
            if c in "[{(":
                depth += 1
            elif c in "]})":
                depth -= 1
            if (c == "," and depth == 0) or i == en - 1:
                item = src[item_start:i].strip()
                mid = re.match(r'\{\s*id: "(\w+)"', item)
                if item in board_var:
                    res.append(board_var[item])
                elif mid:
                    res.append(mid.group(1))
                item_start = i + 1
            elif c in "\"'":
                i = src.index(c, i + 1)
            i += 1
        return res
    if m2:
        first, second, third, l2, l3, l1 = m2.groups()
        out["campaign"] = {"tutorial": board_var[first], "first": board_var[first], "firstIfSeen": board_var[first],
                           "second": board_var[second], "third": board_var[third],
                           "lists": [board_list(l1), board_list(l2), board_list(l3)]}
    if m:
        seen, first, second, third, l2, l3, l1 = m.groups()
        out["campaign"] = {"tutorial": board_var[first], "first": board_var[first], "firstIfSeen": board_var[seen],
                           "second": board_var[second], "third": board_var[third],
                           "lists": [board_list(l1), board_list(l2), board_list(l3)]}
print("campaign", out.get("campaign"))

# ---- sons: V.nome = new C(inicio, duracao)
snd = re.findall(r"^ ([A-Za-z$]+)\.([A-Za-z$]+) = new [A-Za-z$]+\(([0-9.Ee+-]+), ([0-9.Ee+-]+)\);$", src, re.M)
from collections import Counter  # noqa: E402
obj = Counter(x[0] for x in snd).most_common(1)[0][0]
out["sounds"] = [{"key": x[1], "start": float(x[2]), "dur": float(x[3])} for x in snd if x[0] == obj]

# ---- textos da introdução (por cima do vídeo)
m = re.search(r"startTime: \d+", src)
if m:
    s = open_before(src, m.start(), "[")
    out["intro"] = ev(src[s:match_close(src, s)])

# ---- CSS (para cores do tema)
i = src.find(".ddl-background-inner{")
start = src.rfind("'>\\n", 0, i) + 4
out["css"] = src[start:src.find("</style>", i)]

json.dump(out, open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print(MONTH, "sheets", len(out["sheets"]), "anims", len(anims), "boards", len(boards),
      "lists", out["board_lists"], "wild", [w["id"] for w in wild], "sounds", len(out["sounds"]),
      "frames", list(frames.keys()))
