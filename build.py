#!/usr/bin/env python3
"""
Build script for HFT_QD_Master_Guide.pdf

  1. Compiles every C++ file under code/ with g++ -O2 -std=c++20 -Wall -Wextra -Werror,
     runs it, and captures stdout (cached by source hash).
  2. Parses chapters/*.md (a small Markdown dialect, see README.md) and typesets the
     book with reportlab: title page, clickable TOC, PDF outline, numbered headings,
     syntax-highlighted code blocks that split across pages and never overflow.

Usage:  python3 build.py [--rerun] [--sanitize] [--only-code]
"""
import hashlib, json, os, re, subprocess, sys, datetime
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, Flowable, Frame, KeepTogether,
                                NextPageTemplate, PageBreak, PageTemplate, Paragraph,
                                Spacer, Table, TableStyle, CondPageBreak)
from reportlab.platypus.tableofcontents import TableOfContents
from pygments.lexers import get_lexer_by_name
from pygments.token import Token

ROOT = Path(__file__).resolve().parent
CODE, CHAP, SOLN, BUILD = ROOT / "code", ROOT / "chapters", ROOT / "solutions", ROOT / "build"
OUT_PDF = ROOT / "HFT_QD_Master_Guide.pdf"
MAX_COLS = 92
CXX = ["g++", "-O2", "-std=c++20", "-Wall", "-Wextra", "-Werror"]

# Book manifest: discovered automatically so parallel-written chapters merge by copying files.
#   chapters/00_front.md, chapters/chNN.md (NN = 01..21), chapters/app_b.md, app_c.md, app_d.md
#   solutions/chNN.md -> Appendix A sections, in chapter order
MANIFEST = [("00_front.md", "front")] + [(p.name, "chapter") for p in sorted(CHAP.glob("ch[0-9][0-9].md"))]
SOLUTIONS = [p.name for p in sorted(SOLN.glob("ch[0-9][0-9].md"))]
EXTRA_APPENDICES = [p.name for p in sorted(CHAP.glob("app_*.md"))]   # "# B. Title" etc.

# --------------------------------------------------------------------------- fonts
FD = "/usr/share/fonts/truetype/dejavu/"
for name, f in [("Serif", "DejaVuSerif.ttf"), ("Serif-B", "DejaVuSerif-Bold.ttf"),
                ("Serif-I", "DejaVuSerif-Italic.ttf"), ("Serif-BI", "DejaVuSerif-BoldItalic.ttf"),
                ("Sans", "DejaVuSans.ttf"), ("Sans-B", "DejaVuSans-Bold.ttf"),
                ("Sans-I", "DejaVuSans-Oblique.ttf"), ("Sans-BI", "DejaVuSans-BoldOblique.ttf"),
                ("Mono", "DejaVuSansMono.ttf"), ("Mono-B", "DejaVuSansMono-Bold.ttf"),
                ("Mono-I", "DejaVuSansMono-Oblique.ttf"), ("Mono-BI", "DejaVuSansMono-BoldOblique.ttf")]:
    pdfmetrics.registerFont(TTFont(name, FD + f))
pdfmetrics.registerFontFamily("Serif", normal="Serif", bold="Serif-B", italic="Serif-I", boldItalic="Serif-BI")
pdfmetrics.registerFontFamily("Sans", normal="Sans", bold="Sans-B", italic="Sans-I", boldItalic="Sans-BI")
pdfmetrics.registerFontFamily("Mono", normal="Mono", bold="Mono-B", italic="Mono-I", boldItalic="Mono-BI")

# --------------------------------------------------------------------------- styles
INK = colors.HexColor("#1b1f24")
ACCENT = colors.HexColor("#0b4f8a")
MUTED = colors.HexColor("#5b6670")
RULE = colors.HexColor("#c9d1d9")
CODE_BG = colors.HexColor("#f6f8fa")
OUT_BG = colors.HexColor("#f2f5ef")
NOTE_BG = colors.HexColor("#eef4fb")

S = {}
S["body"] = ParagraphStyle("body", fontName="Serif", fontSize=9.6, leading=13.6, textColor=INK,
                           alignment=TA_JUSTIFY, spaceAfter=5.5)
S["bullet"] = ParagraphStyle("bullet", parent=S["body"], leftIndent=14, bulletIndent=4,
                             spaceAfter=2.5, alignment=TA_LEFT)
S["bullet2"] = ParagraphStyle("bullet2", parent=S["bullet"], leftIndent=28, bulletIndent=18)
S["h1"] = ParagraphStyle("h1", fontName="Sans-B", fontSize=21, leading=26, textColor=INK,
                         spaceAfter=14)
S["chapnum"] = ParagraphStyle("chapnum", fontName="Sans", fontSize=11, leading=14,
                              textColor=ACCENT, spaceBefore=40, spaceAfter=4)
S["h2"] = ParagraphStyle("h2", fontName="Sans-B", fontSize=13.2, leading=17, textColor=ACCENT,
                         spaceBefore=13, spaceAfter=6, keepWithNext=1)
S["h3"] = ParagraphStyle("h3", fontName="Sans-B", fontSize=10.6, leading=14, textColor=INK,
                         spaceBefore=9, spaceAfter=4, keepWithNext=1)
S["h4"] = ParagraphStyle("h4", fontName="Sans-BI", fontSize=9.6, leading=13, textColor=INK,
                         spaceBefore=6, spaceAfter=3, keepWithNext=1)
S["part"] = ParagraphStyle("part", fontName="Sans-B", fontSize=28, leading=34, textColor=ACCENT,
                           alignment=TA_CENTER, spaceBefore=200)
S["partsub"] = ParagraphStyle("partsub", fontName="Serif-I", fontSize=12, leading=16,
                              textColor=MUTED, alignment=TA_CENTER, spaceBefore=12)
S["caption"] = ParagraphStyle("caption", fontName="Sans", fontSize=7.8, leading=10,
                              textColor=MUTED, spaceBefore=4, spaceAfter=2, keepWithNext=1)
S["cell"] = ParagraphStyle("cell", fontName="Serif", fontSize=8.3, leading=10.6, textColor=INK)
S["cellh"] = ParagraphStyle("cellh", fontName="Sans-B", fontSize=8.3, leading=10.6,
                            textColor=colors.white)
S["note"] = ParagraphStyle("note", parent=S["body"], fontSize=9.2, leading=13, spaceAfter=0,
                           alignment=TA_LEFT)
S["toctitle"] = ParagraphStyle("toctitle", fontName="Sans-B", fontSize=20, leading=26,
                               spaceAfter=16, textColor=INK)
TOC_STYLES = [
    ParagraphStyle("toc0", fontName="Sans-B", fontSize=10.5, leading=14, spaceBefore=9,
                   textColor=ACCENT),
    ParagraphStyle("toc1", fontName="Sans-B", fontSize=9.6, leading=12.5, leftIndent=10,
                   spaceBefore=4),
    ParagraphStyle("toc2", fontName="Serif", fontSize=8.8, leading=11.2, leftIndent=26),
]

PAGE_W, PAGE_H = A4
LM = RM = 20 * mm
TM, BM = 22 * mm, 20 * mm
TEXT_W = PAGE_W - LM - RM

# --------------------------------------------------------------------------- compile
def directives(src):
    d = {"flags": [], "run": True, "args": [], "timeout": 120}
    for line in src.splitlines()[:6]:
        m = re.match(r"\s*//\s*book:\s*(.*)", line)
        if not m:
            continue
        for kv in m.group(1).split(";"):
            kv = kv.strip()
            if kv == "norun":
                d["run"] = False
            elif kv.startswith("flags="):
                d["flags"] = kv[6:].split()
            elif kv.startswith("args="):
                d["args"] = kv[5:].split()
            elif kv.startswith("timeout="):
                d["timeout"] = int(kv[8:])
    return d


def compile_and_run_all(rerun=False, sanitize=False):
    cache_f = BUILD / "cache.json"
    cache = json.loads(cache_f.read_text()) if cache_f.exists() else {}
    (BUILD / "bin").mkdir(parents=True, exist_ok=True)
    (BUILD / "out").mkdir(parents=True, exist_ok=True)
    ok = True
    for cpp in sorted(CODE.rglob("*.cpp")):
        rel = cpp.relative_to(CODE).as_posix()
        src = cpp.read_text()
        for i, line in enumerate(src.splitlines(), 1):
            if len(line) > MAX_COLS:
                print(f"[cols] {rel}:{i} has {len(line)} > {MAX_COLS} columns"); ok = False
        d = directives(src)
        if sanitize:
            cmd = ["g++", "-O1", "-g", "-std=c++20", "-Wall", "-Wextra",
                   "-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
            if "-pthread" in d["flags"] and "tsan" in src[:400]:
                cmd = ["g++", "-O1", "-g", "-std=c++20", "-fsanitize=thread"]
        else:
            cmd = list(CXX)
        exe = BUILD / "bin" / (rel.replace("/", "__")[:-4] + (".san" if sanitize else ""))
        key = hashlib.sha256((src + " ".join(cmd + d["flags"])).encode()).hexdigest()
        outf = BUILD / "out" / (rel.replace("/", "__")[:-4] + ".txt")
        if not sanitize and cache.get(rel) == key and exe.exists() and outf.exists() and not rerun:
            continue
        r = subprocess.run(cmd + [str(cpp), "-o", str(exe)] + d["flags"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[compile FAIL] {rel}\n{r.stderr}"); ok = False; continue
        print(f"[compiled] {rel}")
        if d["run"]:
            try:
                rr = subprocess.run([str(exe)] + d["args"], capture_output=True, text=True,
                                    timeout=d["timeout"])
            except subprocess.TimeoutExpired:
                print(f"[run TIMEOUT] {rel}"); ok = False; continue
            if rr.returncode != 0:
                print(f"[run FAIL rc={rr.returncode}] {rel}\n{rr.stdout[-2000:]}\n{rr.stderr[-2000:]}")
                ok = False; continue
            if not sanitize:
                outf.write_text(rr.stdout)
            print(f"[ran] {rel}")
        elif not sanitize:
            outf.write_text("")
        if not sanitize:
            cache[rel] = key
    if not sanitize:
        cache_f.write_text(json.dumps(cache, indent=1))
    return ok

# --------------------------------------------------------------------------- inline markup
def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def inline(t):
    slots = []

    def keep(s):
        slots.append(s); return f"\x00{len(slots)-1}\x00"
    t = re.sub(r"`([^`]+)`", lambda m: keep(
        f'<font name="Mono" size="{0.92:.2f}em" color="#7a1f3d">{esc(m.group(1))}</font>'), t)
    t = re.sub(r"\[([^\]]+)\]\((https?://[^)]+)\)", lambda m: keep(
        f'<link href="{esc(m.group(2))}" color="#0b4f8a"><u>{esc(m.group(1))}</u></link>'), t)
    t = esc(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", t)
    t = re.sub(r"\^\{([^}]*)\}", r"<super>\1</super>", t)
    t = re.sub(r"_\{([^}]*)\}", r"<sub>\1</sub>", t)
    t = re.sub(r"\x00(\d+)\x00", lambda m: slots[int(m.group(1))], t)
    # em-size font hack: reportlab doesn't know 'em'; translate to absolute size later
    return t


def fix_em(t, base):
    return re.sub(r'size="([\d.]+)em"', lambda m: f'size="{float(m.group(1))*base:.2f}"', t)


def P(text, style):
    return Paragraph(fix_em(inline(text), style.fontSize), style)

# --------------------------------------------------------------------------- code flowable
TOKEN_COLORS = [
    (Token.Comment, "#5b7083", "I"), (Token.Keyword.Type, "#6f42c1", ""),
    (Token.Keyword, "#a0115e", "B"), (Token.Name.Builtin, "#6f42c1", ""),
    (Token.Literal.String, "#0a6640", ""), (Token.Literal.Number, "#005cc5", ""),
    (Token.Comment.Preproc, "#a35200", ""), (Token.Comment.PreprocFile, "#0a6640", ""),
    (Token.Name.Function, "#1f4e9c", ""), (Token.Operator, "#24292f", ""),
]


def tok_style(tt):
    best = None
    for base, col, fl in TOKEN_COLORS:
        if tt in base and (best is None or len(base) > len(best[0])):
            best = (base, col, fl)
    if best is None:
        return ("#24292f", "")
    return best[1], best[2]


def tokenize(code, lang):
    lines = [[]]
    if lang in ("cpp", "c++", "c", "asm", "python", "bash", "nasm", "gas"):
        lex = get_lexer_by_name({"c++": "cpp", "asm": "gas"}.get(lang, lang), stripnl=False,
                                ensurenl=False)
        toks = lex.get_tokens(code)
    else:
        toks = [(Token.Text, code)]
    for tt, val in toks:
        col, fl = tok_style(tt)
        parts = val.split("\n")
        for i, p in enumerate(parts):
            if i > 0:
                lines.append([])
            if p:
                lines[-1].append((p.replace("\t", "    "), col, fl))
    while lines and not lines[-1]:
        lines.pop()
    return lines


class CodeBlock(Flowable):
    FS, LEAD, PAD = 7.55, 9.45, 5.0

    def __init__(self, lines, bg=CODE_BG, numbers=True, start=1, cont_top=False):
        super().__init__()
        self.lines, self.bg, self.numbers, self.start, self.cont_top = lines, bg, numbers, start, cont_top
        self.cw = pdfmetrics.stringWidth("M", "Mono", self.FS)
        self.gutter = (len(str(start + len(lines))) + 1) * self.cw if numbers else 0
        self._wrapped = None

    def _wrap_lines(self, avail):
        maxc = int((avail - 2 * self.PAD - self.gutter - 4) / self.cw)
        out = []   # (lineno or None, frags)
        for i, frags in enumerate(self.lines):
            n = self.start + i
            cur, cl = [], 0
            first = True
            for text, col, fl in frags:
                while text:
                    room = maxc - cl - (0 if first else 2)
                    if len(text) <= room:
                        cur.append((text, col, fl)); cl += len(text); text = ""
                    else:
                        cur.append((text[:room], col, fl)); text = text[room:]
                        out.append((n if first else None, cur, True))
                        first, cur, cl = False, [("↪ ", "#9aa5b1", "")], 2
            out.append((n if first else None, cur, False))
        return out

    def wrap(self, aw, ah):
        self.aw = aw
        self._wrapped = self._wrap_lines(aw)
        self.h = len(self._wrapped) * self.LEAD + 2 * self.PAD
        return aw, self.h

    def split(self, aw, ah):
        rows = int((ah - 2 * self.PAD) / self.LEAD)
        if rows < 4 or rows >= len(self.lines):
            return []
        a = CodeBlock(self.lines[:rows - 1], self.bg, self.numbers, self.start, self.cont_top)
        b = CodeBlock(self.lines[rows - 1:], self.bg, self.numbers, self.start + rows - 1, True)
        return [a, b]

    def draw(self):
        c = self.canv
        c.setFillColor(self.bg); c.setStrokeColor(RULE); c.setLineWidth(0.5)
        c.rect(0, 0, self.aw, self.h, stroke=1, fill=1)
        c.setFillColor(ACCENT if self.bg == CODE_BG else colors.HexColor("#5a7d3a"))
        c.rect(0, 0, 2.2, self.h, stroke=0, fill=1)
        y = self.h - self.PAD - self.FS
        for n, frags, _ in self._wrapped:
            x = self.PAD + 2
            if self.numbers and n is not None:
                c.setFont("Mono", self.FS - 1); c.setFillColor(colors.HexColor("#9aa5b1"))
                s = str(n)
                c.drawRightString(x + self.gutter - self.cw, y, s)
            x += self.gutter
            for text, col, fl in frags:
                font = {"I": "Mono-I", "B": "Mono-B"}.get(fl, "Mono")
                c.setFont(font, self.FS); c.setFillColor(colors.HexColor(col))
                c.drawString(x, y, text)
                x += len(text) * self.cw
            y -= self.LEAD

# --------------------------------------------------------------------------- headings
class Heading(Paragraph):
    def __init__(self, text, style, level, key, toc_text, outline_level):
        super().__init__(text, style)
        self.toc_level, self.key, self.toc_text, self.outline_level = level, key, toc_text, outline_level


class ChapterMark(Flowable):
    """Zero-size flowable telling the page template the current chapter title."""
    def __init__(self, title):
        super().__init__(); self.title = title
    def wrap(self, aw, ah): return 0, 0
    def draw(self): pass

# --------------------------------------------------------------------------- parser
class State:
    def __init__(self):
        self.chapter = 0; self.section = 0; self.sub = 0; self.listing = 0
        self.label = ""; self.key = 0; self.appendix = False; self.off = 0


def hkey(st):
    st.key += 1; return f"k{st.key}"


def make_table(rows):
    ncol = max(len(r) for r in rows)
    rows = [r + [""] * (ncol - len(r)) for r in rows]
    lens = [max(len(re.sub(r"[`*]", "", r[i])) for r in rows) for i in range(ncol)]
    weights = [min(max(l, 4), 60) ** 0.8 for l in lens]
    tot = sum(weights)
    widths = [TEXT_W * w / tot for w in weights]
    data = [[P(c, S["cellh"]) for c in rows[0]]] + [[P(c, S["cell"]) for c in r] for r in rows[1:]]
    t = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), ACCENT),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f7fa")]),
        ("GRID", (0, 0), (-1, -1), 0.4, RULE),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 4), ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 2.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return t


def note_box(text_lines):
    paras = []
    for para in "\n".join(text_lines).split("\n\n"):
        paras.append(P(" ".join(para.split("\n")), S["note"]))
    inner = []
    for p in paras:
        inner.append(p)
    t = Table([[inner]], colWidths=[TEXT_W])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), NOTE_BG),
        ("LINEBEFORE", (0, 0), (0, -1), 2.2, ACCENT),
        ("LEFTPADDING", (0, 0), (-1, -1), 9), ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return [Spacer(1, 3), t, Spacer(1, 7)]


def code_flow(code, lang, caption, bg=CODE_BG, numbers=True):
    out = []
    for i, line in enumerate(code.splitlines(), 1):
        if len(line) > MAX_COLS and lang != "text":
            raise SystemExit(f"[cols] inline block line {i} too long: {line!r}")
    if caption:
        out.append(P(caption, S["caption"]))
    out.append(CodeBlock(tokenize(code, lang), bg=bg, numbers=numbers))
    out.append(Spacer(1, 7))
    return out


def parse(text, st, kind, num=None):
    flow = []
    lines = text.splitlines()
    i = 0
    para = []

    def flush():
        nonlocal para
        if para:
            flow.append(P(" ".join(s.strip() for s in para), S["body"])); para = []

    while i < len(lines):
        line = lines[i]
        s = line.strip()
        if s.startswith("```"):
            flush()
            lang = s[3:].strip() or "text"
            j = i + 1; buf = []
            while not lines[j].strip().startswith("```"):
                buf.append(lines[j]); j += 1
            flow += code_flow("\n".join(buf), lang, None, numbers=(lang not in ("text",)))
            i = j + 1; continue
        if s.startswith("#! "):           # Part divider
            flush()
            title, _, sub = s[3:].partition("|")
            k = hkey(st)
            flow += [NextPageTemplate("plain"), PageBreak(), ChapterMark(""),
                     Heading(esc(title.strip()), S["part"], 0, k, title.strip() + (" · " + sub.strip() if sub.strip() else ""), 0)]
            if sub.strip():
                flow.append(P(sub.strip(), S["partsub"]))
            flow += [NextPageTemplate("main")]
            i += 1; continue
        if s.startswith("# "):            # Chapter
            flush()
            title = s[2:].strip()
            st.section = 0; st.sub = 0; st.listing = 0
            if kind == "chapter":
                st.chapter = num if num else st.chapter + 1; st.label = str(st.chapter)
                pre = f"Chapter {st.label}"
            elif kind == "appendix":
                st.label = title.split(".")[0].strip(); pre = f"Appendix {st.label}"
                title = title.split(".", 1)[1].strip()
            else:
                st.label = ""; pre = ""
            k = hkey(st)
            st.off = -1 if kind == "front" else 0
            flow += [NextPageTemplate("chapfirst"), PageBreak(), NextPageTemplate("main"),
                     ChapterMark((pre + " · " if pre else "") + title)]
            if pre:
                flow.append(Paragraph(pre.upper(), S["chapnum"]))
            else:
                flow.append(Spacer(1, 40))
            toc = (f"{st.label}  " if pre and kind == "chapter" else (f"{pre}. " if pre else "")) + title
            flow.append(Heading(esc(title), S["h1"], 1, k, toc, 1 + st.off))
            flow.append(HRule())
            i += 1; continue
        if s.startswith("## "):
            flush()
            title = s[3:].strip()
            if title.startswith("*"):     # unnumbered
                title = title[1:].strip(); num = ""
            else:
                st.section += 1; st.sub = 0
                num = f"{st.label}.{st.section}" if st.label else f"{st.section}"
            k = hkey(st)
            disp = f"{num}  {title}" if num else title
            flow.append(Heading(fix_em(inline(disp), S["h2"].fontSize), S["h2"], 2, k, disp, 2 + st.off))
            i += 1; continue
        if s.startswith("### "):
            flush()
            title = s[4:].strip()
            if title.startswith("*"):
                disp = title[1:].strip()
            else:
                st.sub += 1
                disp = f"{st.label}.{st.section}.{st.sub}  {title}" if st.label else title
            k = hkey(st)
            flow.append(Heading(fix_em(inline(disp), S["h3"].fontSize), S["h3"], 3, k, disp, 3 + st.off))
            i += 1; continue
        if s.startswith("#### "):
            flush(); flow.append(P(s[5:], S["h4"])); i += 1; continue
        if s.startswith("@pagebreak"):
            flush(); flow.append(PageBreak()); i += 1; continue
        if s.startswith("@condbreak"):
            flush(); flow.append(CondPageBreak(float(s.split()[1]) * mm)); i += 1; continue
        if s.startswith("@code ") or s.startswith("@output "):
            flush()
            cmd, path = s.split()[0], s.split()[1]
            if cmd == "@code":
                st.listing += 1
                src = (CODE / path).read_text().rstrip("\n")
                flow += code_flow(src, "cpp",
                                  f"**Listing {st.label}.{st.listing}**  ·  code/{path}  ·  "
                                  f"compiled with g++ 13.3 -O2 -std=c++20 -Wall -Wextra -Werror")
            else:
                outf = BUILD / "out" / (path.replace("/", "__")[:-4] + ".txt")
                txt = outf.read_text().rstrip("\n") if outf.exists() else "(not run)"
                note = " ".join(s.split()[2:])
                flow += code_flow(txt, "text", f"**Output** of code/{path}" +
                                  (f"  ·  {note}" if note else ""), bg=OUT_BG, numbers=False)
            i += 1; continue
        if s.startswith(">"):
            flush(); buf = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip()); i += 1
            flow += note_box(buf); continue
        if s.startswith("|"):
            flush(); rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                r = lines[i].strip().strip("|")
                cells = [c.strip() for c in re.split(r"(?<!\\)\|", r)]
                cells = [c.replace("\\|", "|") for c in cells]
                if not all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
                    rows.append(cells)
                i += 1
            flow += [make_table(rows), Spacer(1, 8)]; continue
        m = re.match(r"^(\s*)([-•]|\d+\.)\s+(.*)", line)
        if m and (not para):
            flush()
            ind, mark, body = m.groups()
            buf = [body]; i += 1
            while i < len(lines) and lines[i].strip() and not re.match(r"^\s*([-•]|\d+\.)\s+", lines[i]) \
                    and not lines[i].strip().startswith(("|", ">", "```", "#", "@")):
                buf.append(lines[i].strip()); i += 1
            style = S["bullet2"] if len(ind) >= 2 else S["bullet"]
            bt = "•" if mark in "-•" else mark
            flow.append(Paragraph(fix_em(inline(" ".join(buf)), style.fontSize), style,
                                  bulletText=bt))
            continue
        if not s:
            flush(); i += 1; continue
        para.append(line); i += 1
    flush()
    return flow


class HRule(Flowable):
    def wrap(self, aw, ah):
        self.aw = aw; return aw, 10
    def draw(self):
        self.canv.setStrokeColor(ACCENT); self.canv.setLineWidth(1.2)
        self.canv.line(0, 8, 60, 8)
        self.canv.setStrokeColor(RULE); self.canv.setLineWidth(0.5)
        self.canv.line(62, 8, self.aw, 8)

# --------------------------------------------------------------------------- document
class BookDoc(BaseDocTemplate):
    def __init__(self, fn, **kw):
        super().__init__(fn, pagesize=A4, leftMargin=LM, rightMargin=RM, topMargin=TM,
                         bottomMargin=BM, title="HFT Quant Developer Master Guide",
                         author="Prepared with Claude", subject="QD interview preparation", **kw)
        fr = Frame(LM, BM, TEXT_W, PAGE_H - TM - BM, id="f", leftPadding=0, rightPadding=0,
                   topPadding=0, bottomPadding=0)
        self.cur_title = ""
        self.addPageTemplates([
            PageTemplate("title", [fr], onPage=lambda c, d: None),
            PageTemplate("main", [fr], onPageEnd=self.decorate),
            PageTemplate("chapfirst", [fr], onPageEnd=self.footer_only),
            PageTemplate("plain", [fr], onPageEnd=lambda c, d: None),
        ])

    def footer_only(self, c, d):
        c.saveState(); c.setFont("Sans", 8); c.setFillColor(MUTED)
        c.drawCentredString(PAGE_W / 2, 11 * mm, str(c.getPageNumber())); c.restoreState()

    def decorate(self, c, d):
        c.saveState()
        c.setFont("Sans", 7.8); c.setFillColor(MUTED)
        c.drawString(LM, PAGE_H - 13 * mm, "HFT Quant Developer Master Guide")
        c.drawRightString(PAGE_W - RM, PAGE_H - 13 * mm, self.cur_title)
        c.setStrokeColor(RULE); c.setLineWidth(0.4)
        c.line(LM, PAGE_H - 14.6 * mm, PAGE_W - RM, PAGE_H - 14.6 * mm)
        c.setFont("Sans", 8)
        c.drawCentredString(PAGE_W / 2, 11 * mm, str(c.getPageNumber()))
        c.restoreState()

    def afterFlowable(self, f):
        if isinstance(f, ChapterMark):
            self.cur_title = f.title
        if isinstance(f, Heading):
            c = self.canv
            c.bookmarkPage(f.key)
            plain = re.sub(r"<[^>]+>", "", f.toc_text)
            c.addOutlineEntry(plain, f.key, level=f.outline_level, closed=f.outline_level >= 1)
            if f.toc_level <= 2:
                self.notify("TOCEntry", (f.toc_level, esc(plain) if "<" not in f.toc_text else plain,
                                         self.page, f.key))


class TitlePage(Flowable):
    def wrap(self, aw, ah):
        return aw, ah - 1
    def draw(self):
        c = self.canv
        c.saveState()
        c.translate(-LM, -BM)  # absolute coordinates
        c.setFillColor(colors.HexColor("#0d2238")); c.rect(0, PAGE_H * 0.42, PAGE_W, PAGE_H * 0.58, 0, 1)
        c.setFillColor(colors.HexColor("#1e6fb3")); c.rect(0, PAGE_H * 0.42 - 4, PAGE_W, 4, 0, 1)
        c.setFillColor(colors.white)
        c.setFont("Sans", 12); c.drawString(LM, PAGE_H - 50 * mm, "A PREPARATION BOOK FOR QUANT DEVELOPER INTERNS")
        c.setFont("Sans-B", 38); c.drawString(LM, PAGE_H - 75 * mm, "HFT Quant Developer")
        c.drawString(LM, PAGE_H - 91 * mm, "Master Guide")
        c.setFont("Serif-I", 13); c.setFillColor(colors.HexColor("#c7d9ee"))
        y = PAGE_H - 108 * mm
        for ln in ["From basic C++ to interview-ready: probability, algorithms,",
                   "C++ internals, systems, concurrency, and trading-system engineering"]:
            c.drawString(LM, y, ln); y -= 17
        c.setFillColor(INK); c.setFont("Sans-B", 10.5)
        y = PAGE_H * 0.42 - 22 * mm
        c.drawString(LM, y, "What is inside"); y -= 16
        c.setFont("Serif", 9.8)
        for ln in ["Part A  Getting through the screen: hiring, probability, algorithms",
                   "Part B  C++ depth: object model, memory, modern C++, templates, STL, UB",
                   "Part C  Systems: architecture, Linux, concurrency, networking, profiling",
                   "Part D  Trading-domain engineering: microstructure, components, capstone",
                   "Part E  Interviews and converting the internship",
                   "Appendices: full solutions, cheat sheets, toolchain, reading list"]:
            c.drawString(LM, y, ln); y -= 14
        c.setFont("Sans", 8.5); c.setFillColor(MUTED)
        c.drawString(LM, 25 * mm, f"Working draft · built {datetime.date.today():%d %B %Y} · "
                                  "all listings compiled and run with g++ 13.3, -O2 -std=c++20")
        c.restoreState()


def build_pdf():
    st = State()
    story = [TitlePage(), NextPageTemplate("plain"), PageBreak()]
    toc = TableOfContents(levelStyles=TOC_STYLES, dotsMinLevel=1)
    story += [Paragraph("Contents", S["toctitle"]), toc]
    for fn, kind in MANIFEST:
        story += parse((CHAP / fn).read_text(), st, kind,
                       int(fn[2:4]) if kind == "chapter" else None)
    # Appendix A: solutions
    if SOLUTIONS:
        k = hkey(st)
        story += [NextPageTemplate("plain"), PageBreak(), ChapterMark(""),
                  Heading("Appendices", S["part"], 0, k, "Appendices", 0),
                  P("Solutions, cheat sheets, toolchain, reading list", S["partsub"]),
                  NextPageTemplate("main")]
        txt = "# A. Solutions to Practice Problems\n\n"
        txt += ("Solutions are grouped by chapter. Numeric answers marked *verified* were "
                "checked by the simulation program in the relevant chapter's solutions section.\n\n")
        for fn in SOLUTIONS:
            txt += (SOLN / fn).read_text() + "\n\n"
        story += parse(txt, st, "appendix")
        for fn in EXTRA_APPENDICES:
            story += parse((CHAP / fn).read_text(), st, "appendix")
    doc = BookDoc(str(OUT_PDF))
    doc.multiBuild(story, maxPasses=6)
    print(f"[pdf] {OUT_PDF} pages={doc.page}")


if __name__ == "__main__":
    args = sys.argv[1:]
    ok = compile_and_run_all(rerun="--rerun" in args, sanitize="--sanitize" in args)
    if not ok:
        sys.exit("code checks failed")
    if "--only-code" in args or "--sanitize" in args:
        sys.exit(0)
    build_pdf()
