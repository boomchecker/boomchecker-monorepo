"""Training material for the talk, generated from the speaker notes in slides.tex.

The spoken script lives only in the ``\\note{}`` blocks of ``slides/slides.tex``. This
script reads those blocks and writes, into ``slides/training/``:

* ``booklet.tex`` -- an A4 booklet in three parts, one page per slide:
  A. the full script, one sentence per line (every ``//`` pause = a line break),
  B. the same text reduced to first letters (retrieval practice with a skeleton),
  C. only the opening and closing sentence of each slide (recall from anchors);
  plus a first page with the method and a last page with every number in the talk.
* ``anki.csv`` -- flashcards for Anki (tab separated, HTML allowed): one card per
  slide, one "bridge" card per slide (its last sentence), and the number cards.

Slide thumbnails are taken straight from ``slides/slides.pdf`` by page number
(``\\includegraphics[page=...]``); the frame -> page mapping comes from ``slides.nav``,
so build the deck first::

    task build && python3 scripts/build_training.py && (cd slides/training && pdflatex booklet.tex)

or simply ``task training``.
"""
from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLIDES_TEX = ROOT / "slides" / "slides.tex"
SLIDES_NAV = ROOT / "slides" / "slides.nav"
OUT_DIR = ROOT / "slides" / "training"

# Every number in the talk, in one place (front, back, tag). Mirrors the cue-card footer.
NUMBERS = [
    ("Best configuration (GTCC + NN, 50 ms) – accuracy?", "99.15 %", "numbers results"),
    ("Best configuration (GTCC + NN, 50 ms) – recall?", "96.77 %", "numbers results"),
    ("Best configuration (GTCC + NN, 50 ms) – MCC?", "0.98", "numbers results"),
    ("Test partition – how many events, how many 9 mm?", "118 events, 31 of them 9 mm", "numbers results"),
    ("99.15 % accuracy = how many correct decisions?", "117 of 118", "numbers results"),
    ("96.77 % recall = how many 9 mm shots recalled?", "30 of 31", "numbers results"),
    ("Corpus – training / test samples (five classes)?", "371 / 118", "numbers dataset"),
    ("Features per extractor?", "26 coefficients per frame", "numbers features"),
    ("Frame lengths tested?", "15 / 30 / 50 ms", "numbers features"),
    ("Bearing accuracy (cross-correlation + parabolic interpolation)?", "MAE 0.77°", "numbers localization"),
    ("Bearing evaluation – how many events, how many reference angles?", "70 events, 9 angles (−45° to +60°)", "numbers localization"),
    ("Microphone spacing and sampling rate?", "d = 0.186 m (≈ 20 cm), f_s = 44.1 kHz", "numbers localization"),
    ("Maximum TDoA for this geometry?", "≈ 24 samples", "numbers localization"),
    ("Shock wave / muzzle blast timing in the recording shown?", "shock wave ≈ 2.5 ms, muzzle blast ≈ 45 ms later, lasting 1–3 ms", "numbers signal"),
    ("Talk length – target?", "10 min slot, script written to 9:30", "numbers talk"),
]

# --------------------------------------------------------------------------- parsing


@dataclass
class Slide:
    number: int
    title: str
    window: str = ""
    lines: list[str] = field(default_factory=list)   # sentences and markers, in order
    page: int = 0

    @property
    def sentences(self) -> list[str]:
        return [l for l in self.lines if not l.startswith("[")]


def frames(tex: str):
    """Yield (title, note_source) for each frame before \\appendix, in order."""
    body = tex.split("\\appendix")[0]
    pos = 0
    while True:
        a = body.find("\\begin{frame}", pos)
        if a < 0:
            return
        e = body.index("\\end{frame}", a)
        head = body[a + len("\\begin{frame}"):a + 200]
        m = re.match(r"(\[[^\]]*\])?\{((?:[^{}]|\{[^{}]*\})*)\}", head)
        title = m.group(2) if m and m.group(2) else "Title"
        notes = _note_blocks(body[a:e])
        # continuation note right after \end{frame}
        tail = body[e + len("\\end{frame}"):]
        m2 = re.match(r"\s*\\note\{", tail)
        if m2:
            notes += _note_blocks(tail[: _block_end(tail, m2.end() - 1) + 1])
        yield title, "\n".join(notes)
        pos = e + len("\\end{frame}")


def _block_end(s: str, open_brace: int) -> int:
    depth = 0
    for i in range(open_brace, len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    raise ValueError("unbalanced braces")


def _note_blocks(s: str) -> list[str]:
    out = []
    for m in re.finditer(r"\\note(?:<[^>]*>)?\{", s):
        end = _block_end(s, m.end() - 1)
        out.append(s[m.end():end])
    return out


def clean(note: str) -> tuple[str, list[str]]:
    """Return (time window, list of lines). Lines are sentences or [CLICK] markers."""
    win = ""
    m = re.search(r"\\textbf\{\[(\d+:\d\d)--(\d+:\d\d)\]\}", note)
    if m:
        win = f"{m.group(1)}–{m.group(2)}"
        note = note.replace(m.group(0), "")
    note = re.sub(r"~?\{\\color\{accent\}\((?:1/2|2/2)\)\}", "", note)
    note = re.sub(r"\\textit\{\(// = pause.*?\)\}", "", note, flags=re.S)
    note = note.replace(r"{\color{accent}[CUT IF LATE]}", " [CUT] ")
    note = note.replace(r"{\color{accent}[/CUT]}", " [/CUT] ")
    note = note.replace(r"\textbf{[CLICK]}", " // [CLICK] // ")
    note = re.sub(r"\\par\\medskip", " ", note)
    note = re.sub(r"\\(?:textbf|emph|textit)\{([^{}]*)\}", r"\1", note)
    note = note.replace("\\,", " ").replace("~", " ").replace("---", "—").replace("--", "–")
    note = note.replace("\\%", "%")
    note = re.sub(r"%.*", "", note)                # LaTeX comments
    note = re.sub(r"\s+", " ", note).strip()
    lines = []
    for part in note.split("//"):
        part = part.strip()
        if part:
            lines.append(part)
    return win, lines


def pages_from_nav(nav: str) -> list[int]:
    return [int(m.group(2)) for m in re.finditer(r"\\beamer@framepages \{(\d+)\}\{(\d+)\}", nav)]


def load() -> list[Slide]:
    tex = SLIDES_TEX.read_text(encoding="utf-8")
    slides = []
    for i, (title, note) in enumerate(frames(tex), start=1):
        win, lines = clean(note)
        slides.append(Slide(i, title.replace("--", "–").replace("\\,", " "), win, lines))
    pages = pages_from_nav(SLIDES_NAV.read_text(encoding="utf-8"))
    if len(pages) < len(slides):
        sys.exit(f"slides.nav lists {len(pages)} frames, slides.tex has {len(slides)} main frames – run `task build` first")
    for s, p in zip(slides, pages):
        s.page = p
    return slides


# --------------------------------------------------------------------------- transforms


def first_letters(sentence: str) -> str:
    """'Good morning. My name is Martin' -> 'G m. M n i M'. Digits and acronyms kept."""
    out = []
    for tok in sentence.split():
        if any(ch.isdigit() for ch in tok) or tok.startswith("["):
            out.append(tok)
            continue
        parts = tok.split("-")
        red = []
        for p in parts:
            m = re.match(r"^([^\w]*)(\w+)([^\w]*)$", p, flags=re.U)
            if not m:
                red.append(p)
                continue
            pre, word, post = m.groups()
            keep = word if (word.isupper() and len(word) > 1) else word[0]
            red.append(pre + keep + post)
        out.append("-".join(red))
    return " ".join(out)


def tex_escape(s: str) -> str:
    s = (s.replace("\\", r"\textbackslash{}").replace("&", r"\&").replace("%", r"\%")
          .replace("$", r"\$").replace("#", r"\#").replace("_", r"\_"))
    # symbols the T1 text fonts do not have
    return s.replace("−", "$-$").replace("≈", r"$\approx$").replace("°", r"$^\circ$")


# --------------------------------------------------------------------------- booklet

PREAMBLE = r"""\documentclass[11pt,a4paper]{article}
\usepackage[T1]{fontenc}
\usepackage[utf8]{inputenc}
\usepackage[english]{babel}
\usepackage[sfdefault,lining]{FiraSans}
\usepackage{FiraMono}
\usepackage[margin=18mm,top=16mm,bottom=16mm]{geometry}
\usepackage{graphicx}
\usepackage{xcolor}
\usepackage{fancyhdr}
\usepackage{enumitem}
\usepackage{booktabs}
\usepackage{parskip}
\definecolor{ctublue}{RGB}{0,101,189}
\definecolor{accent}{RGB}{200,16,46}
\definecolor{ink}{RGB}{35,38,45}
\color{ink}
\pagestyle{fancy}\fancyhf{}
\renewcommand{\headrulewidth}{0pt}
\fancyfoot[L]{\scriptsize\color{ink!55}Acoustic Detection of Impulsive Sounds -- IMEKO TC4 2026 -- training booklet, generated from slides.tex}
\fancyfoot[R]{\scriptsize\color{ink!55}\thepage}
\setlength{\fboxsep}{0pt}\setlength{\fboxrule}{0.4pt}
\newcommand{\slidehead}[4]{% number, title, window, part
  {\color{ctublue}\Large\bfseries #1\quad #2}\hfill{\large\color{ink!60}#3}\\[-1.5mm]
  {\color{ctublue}\rule{\textwidth}{1pt}}\\[-1mm]
  {\scriptsize\color{ink!55}#4}\par\medskip}
\newcommand{\thumb}[1]{\begin{center}\fcolorbox{ink!30}{white}{\includegraphics[page=#1,width=0.72\textwidth]{../slides.pdf}}\end{center}}
\newcommand{\click}{\par\medskip{\color{accent}\bfseries\small [CLICK]}\ {\color{accent}\rule[0.5ex]{0.6\textwidth}{0.6pt}}\par\medskip}
\newcommand{\cutopen}{{\upshape\scriptsize\bfseries [CUT IF LATE]}\ }
\newcommand{\cutclose}{\ {\upshape\scriptsize\bfseries [/CUT]}}
\begin{document}
"""

METHOD = r"""
{\color{ctublue}\LARGE\bfseries How to learn the talk with this booklet}

{\color{ctublue}\rule{\textwidth}{1pt}}

\medskip
The booklet has three parts, each with one page per slide. Work through them in order,
slide by slide, always \textbf{aloud}, standing if you can.

\begin{enumerate}[leftmargin=*,itemsep=4pt]
  \item \textbf{Learn the skeleton, not the words.} Per slide, learn verbatim only the
        \emph{opening sentence}, the \emph{last sentence} (it is the bridge to the next
        slide) and the \emph{numbers}. Everything in between is a list of points you say
        in your own words. A talk recited word for word sounds recited and collapses on
        the first slip; anchor sentences give you a safe place to land.
  \item \textbf{Part A -- full text.} One sentence per line; every line break is a pause.
        Read the slide twice, then cover the text and say it while looking at the thumbnail.
  \item \textbf{Part B -- first letters.} Say the slide from the skeleton. Peek only when
        stuck. Move on once you manage two clean runs.
  \item \textbf{Part C -- anchors only.} Say the whole slide from the opening and closing
        sentence alone. This is the level you need on stage.
  \item \textbf{Time it.} Each page carries its time window. The pauses are part of the
        timing. Target for the whole talk: \textbf{9:30}.
  \item \textbf{Space it out.} Day 1: slides 1--5 to Part C. Day 2: slides 6--11, then re-test
        1--5. Day 3: two full runs with the PDF full-screen and a clicker, one of them recorded
        on your phone -- listen back. Day 4: open the booklet on a random page and start
        from there (this trains recovery from a blank), plus the questions in \texttt{QA.md}.
        Last evening: one relaxed run. \textbf{Freeze the script two days before the talk};
        every edit resets what you have learnt.
  \item \textbf{Anki.} \texttt{anki.csv} holds one card per slide, one card per bridge
        sentence and one per number -- for the minutes between sessions.
\end{enumerate}

\medskip
\textbf{Legend.}\ \ {\color{accent}\bfseries [CLICK]} -- advance the slide.\quad
{\color{ink!55}\itshape grey italics} -- \textbf{[CUT IF LATE]}: can be dropped without
breaking the argument.\quad Numbers table on the last page.
\newpage
"""


def page_full(s: Slide) -> str:
    out = [r"\slidehead{%d}{%s}{%s}{Part A -- full text}" % (s.number, tex_escape(s.title), s.window),
           r"\thumb{%d}" % s.page, r"{\Large\raggedright\sloppy\setlength{\parskip}{6pt}"]
    in_cut = False
    for line in s.lines:
        if line == "[CLICK]":
            out.append(r"\click")
            continue
        text = line
        opening = text.startswith("[CUT]")
        if opening:
            in_cut = True
            text = text[len("[CUT]"):].strip()
        closing = text.endswith("[/CUT]")
        if closing:
            text = text[: -len("[/CUT]")].strip()
        if text:
            t = tex_escape(text)
            if in_cut:
                t = (r"\cutopen{}" if opening else "") + t + (r"\cutclose{}" if closing else "")
                t = r"{\color{ink!55}\itshape " + t + "}"
            out.append(t + r"\par")
        if closing:
            in_cut = False
    out.append("}")
    out.append(r"\newpage")
    return "\n".join(out)


def page_letters(s: Slide) -> str:
    out = [r"\slidehead{%d}{%s}{%s}{Part B -- first letters}" % (s.number, tex_escape(s.title), s.window),
           r"\thumb{%d}" % s.page, r"{\large\ttfamily\raggedright\linespread{1.5}\selectfont\setlength{\parskip}{7pt}"]
    for line in s.lines:
        if line == "[CLICK]":
            out.append(r"\click")
            continue
        text = line.replace("[CUT]", "").replace("[/CUT]", "").strip()
        cut = line.startswith("[CUT]")
        red = tex_escape(first_letters(text))
        out.append((r"{\color{ink!55}%s}\par" if cut else r"%s\par") % red)
    out.append("}")
    out.append(r"\newpage")
    return "\n".join(out)


def page_anchors(s: Slide) -> str:
    sents = [l.replace("[CUT]", "").replace("[/CUT]", "").strip() for l in s.sentences]
    opener, closer = sents[0], sents[-1]
    out = [r"\slidehead{%d}{%s}{%s}{Part C -- anchors only}" % (s.number, tex_escape(s.title), s.window),
           r"\thumb{%d}" % s.page,
           r"\vspace{4mm}{\Large\raggedright\bfseries " + tex_escape(opener) + r"\par}",
           r"\vspace{6mm}{\color{ink!40}\Huge\ldots}\par",
           r"\vfill",
           r"{\Large\raggedright\bfseries " + tex_escape(closer) + r"\par}",
           r"\vspace{8mm}",
           r"\newpage"]
    return "\n".join(out)


def page_numbers() -> str:
    rows = "\n".join(r"    %s & \textbf{%s}\\" % (tex_escape(q), tex_escape(a)) for q, a, _ in NUMBERS)
    return r"""
{\color{ctublue}\LARGE\bfseries Every number in the talk}

{\color{ctublue}\rule{\textwidth}{1pt}}

\medskip
\begin{tabular}{@{}p{0.6\textwidth} p{0.36\textwidth}@{}}
  \toprule
""" + rows + r"""
  \bottomrule
\end{tabular}
"""


def write_booklet(slides: list[Slide]) -> Path:
    parts = [PREAMBLE, METHOD]
    parts += [page_full(s) for s in slides]
    parts += [page_letters(s) for s in slides]
    parts += [page_anchors(s) for s in slides]
    parts.append(page_numbers())
    parts.append(r"\end{document}" + "\n")
    out = OUT_DIR / "booklet.tex"
    out.write_text("\n".join(parts), encoding="utf-8")
    return out


# --------------------------------------------------------------------------- anki


def html(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def write_anki(slides: list[Slide]) -> tuple[Path, int]:
    rows = ["#separator:tab", "#html:true", "#tags column:3"]
    for s in slides:
        sents = s.sentences
        front = f"<b>Slide {s.number} – {html(s.title)}</b> [{s.window}]<br><i>Say the whole slide.</i>"
        body = []
        for line in s.lines:
            if line == "[CLICK]":
                body.append('<span style="color:#C8102E"><b>[CLICK]</b></span>')
            else:
                t = html(line).replace("[CUT]", '<i style="color:#777">[CUT]').replace("[/CUT]", "[/CUT]</i>")
                body.append(t)
        body[0] = "<b>" + body[0] + "</b>" if not body[0].startswith("<span") else body[0]
        rows.append("\t".join([front, "<br>".join(body), f"talk slide{s.number}"]))
        closer = sents[-1].replace("[CUT]", "").replace("[/CUT]", "").strip()
        rows.append("\t".join([f"<b>Slide {s.number} – {html(s.title)}</b><br>Last sentence (the bridge to the next slide)?",
                               html(closer), f"talk bridge slide{s.number}"]))
    for q, a, tags in NUMBERS:
        rows.append("\t".join([html(q), html(a), "talk " + tags]))
    out = OUT_DIR / "anki.csv"
    out.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return out, len(rows) - 3


# --------------------------------------------------------------------------- main

if __name__ == "__main__":
    OUT_DIR.mkdir(exist_ok=True)
    slides = load()
    for s in slides:
        if not s.lines:
            sys.exit(f"slide {s.number} ({s.title}) has no note text")
        if not s.window:
            print(f"warning: slide {s.number} ({s.title}) has no time window", file=sys.stderr)
    booklet = write_booklet(slides)
    anki, ncards = write_anki(slides)
    nsent = sum(len(s.sentences) for s in slides)
    print(f"{len(slides)} slides, {nsent} sentences -> {booklet.relative_to(ROOT)} "
          f"({1 + 3 * len(slides) + 1} pages), {anki.relative_to(ROOT)} ({ncards} cards)")
    for s in slides:
        print(f"  {s.number:2d}  p.{s.page:2d}  [{s.window}]  {s.title}  ({len(s.sentences)} sentences)")
