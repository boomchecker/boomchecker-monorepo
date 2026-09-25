"""Filter-bank thumbnails for the feature-extraction slide.

Four small, axis-less panels in the deck's colours: mel, linear and inverted-mel
triangular banks (CTU blue) and a gammatone bank (accent red). Shapes are
illustrative -- 10 filters over 0..fs/2 at fs = 44.1 kHz -- not the 26-band
configuration of the study.

    python3 scripts/generate_filterbank_icons.py   # writes slides/figs/fb_*.pdf
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

OUT = Path(__file__).resolve().parents[1] / "slides" / "figs"
FS = 44_100.0
NYQ = FS / 2
NFILT = 10
BLUE = "#0065BD"
ACCENT = "#C8102E"
BASE = "#B8BCC4"

f = np.linspace(0.0, NYQ, 4000)


def hz2mel(x):
    return 2595.0 * np.log10(1.0 + x / 700.0)


def mel2hz(m):
    return 700.0 * (10.0 ** (m / 2595.0) - 1.0)


def triangles(edges):
    """Triangular filters from a sorted array of NFILT+2 edge frequencies."""
    out = []
    for lo, c, hi in zip(edges[:-2], edges[1:-1], edges[2:]):
        h = np.zeros_like(f)
        up = (f >= lo) & (f <= c)
        dn = (f > c) & (f <= hi)
        h[up] = (f[up] - lo) / (c - lo)
        h[dn] = (hi - f[dn]) / (hi - c)
        out.append(h)
    return out


def mel_bank():
    return triangles(mel2hz(np.linspace(hz2mel(0), hz2mel(NYQ), NFILT + 2)))


def linear_bank():
    return triangles(np.linspace(0, NYQ, NFILT + 2))


def inverse_mel_bank():
    # mirror the mel edges around the band centre: dense at high frequencies
    return triangles(np.sort(NYQ - mel2hz(np.linspace(hz2mel(0), hz2mel(NYQ), NFILT + 2))))


def erb(fc):
    return 24.7 * (4.37 * fc / 1000.0 + 1.0)


def gammatone_bank(order=4, fmin=400.0, fmax=NYQ * 0.9):
    # ERB-rate spaced centres (Glasberg & Moore), magnitude response of a
    # 4th-order gammatone, normalised to unit peak.
    def erbrate(x):
        return 21.4 * np.log10(4.37e-3 * x + 1.0)

    def inv(r):
        return (10.0 ** (r / 21.4) - 1.0) / 4.37e-3

    centres = inv(np.linspace(erbrate(fmin), erbrate(fmax), NFILT))
    out = []
    for fc in centres:
        b = 1.019 * erb(fc)
        h = (1.0 + ((f - fc) / b) ** 2) ** (-order / 2.0)
        out.append(h / h.max())
    return out


def draw(filters, colour, name):
    fig, ax = plt.subplots(figsize=(1.25, 0.38), dpi=300)
    for h in filters:
        ax.plot(f, h, color=colour, lw=0.7, alpha=0.9, solid_capstyle="round")
    ax.axhline(0, color=BASE, lw=0.5, zorder=0)
    ax.set_xlim(0, NYQ)
    ax.set_ylim(-0.04, 1.08)
    ax.axis("off")
    fig.subplots_adjust(0, 0, 1, 1)
    fig.savefig(OUT / f"fb_{name}.pdf", transparent=True)
    plt.close(fig)


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    draw(mel_bank(), BLUE, "mel")
    draw(linear_bank(), BLUE, "lin")
    draw(inverse_mel_bank(), BLUE, "imel")
    draw(gammatone_bank(), ACCENT, "gtcc")
    print("written:", sorted(p.name for p in OUT.glob("fb_*.pdf")))
