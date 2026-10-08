"""Figures and LaTeX tables of section 4 from ``summary.csv`` (no simulation is run here).

Reads the stages written by ``ablate.py`` and ``sweep.py`` and writes

* ``report/figures/sweep.pdf``: azimuth and elevation RMSE vs SNR with the CRB, RMSE over the
  geometry sweep and RMSE per elevation band,
* ``report/figures/beampattern.pdf``: DAS beampattern cuts at 1 kHz for the three topologies,
* ``report/generated/{ablation,confirm,beampattern,cost}.tex``: table bodies and numbers,
* ``python/results/beampattern.csv``: widths and sidelobe levels of every sweep geometry.

The PDFs carry no timestamp, so equal data give equal files.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.layout_engine import ConstrainedLayoutEngine  # noqa: E402

from beamforming import beampattern as bp  # noqa: E402
from beamforming import cost as cs  # noqa: E402
from beamforming import experiment as ex  # noqa: E402
from beamforming import geometry as g  # noqa: E402
from beamforming import metrics as mt  # noqa: E402
from beamforming import results as rs  # noqa: E402
from beamforming.texfmt import czech  # noqa: E402

LABEL = {
    "das": "DAS",
    "mvdr": "MVDR",
    "srp_phat": "SRP-PHAT",
    "gcc_phat_ls": "GCC-PHAT + LS",
    "music": "MUSIC",
}
# categorical slots 1, 2, 3, 7, 8 of the reference palette; markers repeat the identity in print
COLOR = {
    "das": "#2a78d6",
    "mvdr": "#eb6834",
    "srp_phat": "#1baf7a",
    "gcc_phat_ls": "#4a3aa7",
    "music": "#e34948",
}
MARKER = {"das": "o", "mvdr": "s", "srp_phat": "^", "gcc_phat_ls": "D", "music": "v"}
HEIGHT_COLOR = {0.04: "#2a78d6", 0.07: "#eb6834", 0.10: "#1baf7a"}
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#dcdbd5"
PDF_META = {"CreationDate": None, "ModDate": None, "Creator": None, "Producer": None}
TABLE_FREQS = bp.FREQS_HZ
BEAM_CUT_GEOMETRY = {t: ex.Geometry(t, 0.20, 0.0 if t == "1x8" else 0.10) for t in g.TOPOLOGIES}
TOPOLOGY_LABEL = {"1x8": "1$\\times$8", "2x8": "2$\\times$8", "2x8_rot": "2$\\times$8 pootočené"}


def style() -> None:
    plt.rcParams.update(
        {
            "font.size": 7,
            "axes.labelsize": 7,
            "axes.titlesize": 7,
            "legend.fontsize": 6,
            "xtick.labelsize": 6.5,
            "ytick.labelsize": 6.5,
            "axes.edgecolor": MUTED,
            "axes.labelcolor": INK,
            "text.color": INK,
            "xtick.color": MUTED,
            "ytick.color": MUTED,
            "axes.grid": True,
            "grid.color": GRID,
            "grid.linewidth": 0.5,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "lines.linewidth": 1.2,
            "lines.markersize": 3.5,
            "pdf.fonttype": 42,
        }
    )


def need(rows: list[rs.Row], stage: str) -> list[rs.Row]:
    out = rs.stage_rows(rows, stage)
    if not out:
        raise SystemExit(f"no '{stage}' rows in the summary: run the {stage} stage first")
    return out


def series(rows: list[rs.Row], method: str, column: str) -> tuple[np.ndarray, np.ndarray]:
    sel = sorted((r for r in rows if r["method"] == method), key=lambda r: float(r["snr_db"]))
    return np.array([float(r["snr_db"]) for r in sel]), np.array([float(r[column]) for r in sel])


def geometry_label(geo: ex.Geometry) -> str:
    return geo.label.replace("d", "Ø", 1).replace("_rot", " rot")


def plot_vs_snr(ax, rows: list[rs.Row], column: str, ylabel: str) -> None:
    for m in ex.METHODS:
        x, y = series(rows, m, column)
        ax.plot(x, y, color=COLOR[m], marker=MARKER[m], label=LABEL[m])
    x, y = series(rows, "crb", column)
    ax.plot(x, y, color=MUTED, linestyle="--", marker=None, label="CRB")
    ax.set_yscale("log")
    ax.set_xlabel("SNR [dB]")
    ax.set_ylabel(ylabel)
    ax.set_xticks(np.arange(-10, 31, 10))


def plot_geometry(ax, rows: list[rs.Row]) -> None:
    scores = rs.geometry_scores(rows, "screening", 0.0)
    diam = np.array(ex.DIAMETERS) * 1000
    topologies: list[tuple[g.Topology, str]] = [("2x8", "-"), ("2x8_rot", ":")]
    for topo, style_ in topologies:
        for h in ex.HEIGHTS:
            y = [scores[ex.Geometry(topo, d, h)] for d in ex.DIAMETERS]
            ax.plot(diam, y, style_, color=HEIGHT_COLOR[h], marker="o" if topo == "2x8" else "x")
    y1 = [scores[ex.Geometry("1x8", d)] for d in ex.DIAMETERS]
    ax.plot(diam, y1, "-", color=INK, marker="s")
    top = max(max(scores.values()), 1.0)
    ax.set_ylim(0.0, top * 1.5)  # the top band is left free for the legend
    ax.set_xticks(diam)
    ax.set_xlabel("Ø [mm]")
    ax.set_ylabel("RMSE při 0 dB [°]")
    handles = [
        plt.Line2D([], [], color=HEIGHT_COLOR[h], label=f"h = {h * 1000:.0f} mm")
        for h in ex.HEIGHTS
    ]
    handles += [
        plt.Line2D([], [], color=INK, marker="s", label="1×8"),
        plt.Line2D([], [], color=MUTED, linestyle="-", marker="o", label="2×8"),
        plt.Line2D([], [], color=MUTED, linestyle=":", marker="x", label="2×8 rot"),
    ]
    ax.legend(
        handles=handles,
        ncol=2,
        frameon=False,
        loc="upper right",
        columnspacing=0.8,
        handlelength=1.6,
    )


def plot_bands(ax, rows: list[rs.Row]) -> None:
    at0 = [r for r in rows if r["snr_db"] == "0"]
    width = 0.8 / len(ex.METHODS)
    for i, m in enumerate(ex.METHODS):
        row = next(r for r in at0 if r["method"] == m)
        vals = [float(row[f"rmse_band{b}"]) for b in range(mt.N_BANDS)]
        ax.bar(
            np.arange(mt.N_BANDS) + (i - 2) * width,
            vals,
            width * 0.9,
            color=COLOR[m],
            label=LABEL[m],
        )
    edges = mt.BAND_EDGES_DEG
    ax.set_xticks(range(mt.N_BANDS))
    ax.set_xticklabels([f"{edges[b]:.0f} až {edges[b + 1]:.0f}" for b in range(mt.N_BANDS)])
    ax.set_xlabel("elevace [°]")
    ax.set_ylabel("RMSE při 0 dB [°]")
    ax.grid(axis="x", visible=False)


def sweep_figure(rows: list[rs.Row], out: Path) -> None:
    snr_rows = need(rows, "snr")
    need(rows, "screening")
    fig, axes = plt.subplots(
        1, 4, figsize=(7.16, 2.4), layout=ConstrainedLayoutEngine(w_pad=0.04, h_pad=0.04)
    )
    plot_vs_snr(axes[0], snr_rows, "rmse_az", "RMSE azimutu [°]")
    plot_vs_snr(axes[1], snr_rows, "rmse_el", "RMSE elevace [°]")
    plot_geometry(axes[2], rows)
    plot_bands(axes[3], snr_rows)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, frameon=False, loc="outside upper center", ncol=len(labels))
    for ax, tag in zip(axes, "abcd", strict=True):
        ax.set_title(f"({tag})", loc="left", color=MUTED)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, metadata=PDF_META)
    plt.close(fig)


def cut(mic: np.ndarray, freq: float, u0: np.ndarray, axis: str) -> tuple[np.ndarray, np.ndarray]:
    east, north = g.tangent_basis(u0)
    t = {"east": east, "north": north}[axis]
    phi = np.deg2rad(np.arange(-90.0, 90.01, 0.5))
    dirs = np.cos(phi)[:, None] * u0[None, :] + np.sin(phi)[:, None] * t[None, :]
    return np.rad2deg(phi), 10 * np.log10(np.maximum(bp.power(mic, freq, u0, dirs), 1e-4))


def beampattern_figure(out: Path, freq: float = 1000.0) -> None:
    u0 = g.unit_vector(0.0, np.deg2rad(bp.STEER_EL_DEG))
    fig, axes = plt.subplots(
        1, 2, figsize=(3.4, 1.8), sharey=True, layout=ConstrainedLayoutEngine(w_pad=0.04)
    )
    for ax, axis, title in zip(
        axes, ("east", "north"), ("řez azimutem", "řez elevací"), strict=True
    ):
        for topo, geo in BEAM_CUT_GEOMETRY.items():
            phi, db = cut(geo.mics(), freq, u0, axis)
            ax.plot(
                phi,
                db,
                label=TOPOLOGY_LABEL[topo].replace("$\\times$", "×"),
                linestyle={"1x8": "-", "2x8": "--", "2x8_rot": ":"}[topo],
                color={"1x8": "#2a78d6", "2x8": "#eb6834", "2x8_rot": "#1baf7a"}[topo],
            )
        ax.axhline(-3.0, color=MUTED, linewidth=0.6)
        if axis == "north":  # below the horizon a planar array mirrors the pattern
            ax.axvline(-bp.STEER_EL_DEG, color=MUTED, linewidth=0.6, linestyle=":")
            ax.text(-bp.STEER_EL_DEG + 3, -28, "horizont", color=MUTED, fontsize=6)
        ax.set_title(title, loc="left", color=MUTED)
        ax.set_xlabel("úhel od směru navádění [°]")
        ax.set_xticks([-90, -45, 0, 45, 90])
        ax.set_ylim(-30, 1)
    axes[0].set_ylabel("DAS [dB]")
    axes[0].legend(frameon=False, loc="lower center", handlelength=1.6)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, metadata=PDF_META)
    plt.close(fig)


def beampattern_results() -> list[dict[str, str]]:
    """Widths and PSL for every sweep geometry and table frequency (steering el = 45 deg)."""
    out = []
    for geo in ex.sweep_geometries():
        mic = geo.mics()
        for f in TABLE_FREQS:
            r = bp.evaluate(mic, f)
            out.append(
                {
                    "topology": geo.topology,
                    "diameter_mm": f"{geo.diameter * 1000:.0f}",
                    "height_mm": f"{geo.height * 1000:.0f}",
                    "freq_hz": f"{f:.0f}",
                    "width_az_deg": f"{r.width_az:.1f}",
                    "width_up_deg": f"{r.width_up:.1f}",
                    "width_down_deg": f"{r.width_down:.1f}",
                    "psl_db": "nan" if np.isnan(r.psl_db) else f"{r.psl_db:.1f}",
                }
            )
    return out


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def width_cell(w: float, full: float = 360.0) -> str:
    """A width in degrees, ``--`` when the lobe covers the whole (half) circle."""
    return "--" if w >= full else czech(w, 0)


def beampattern_tex(rows: list[dict[str, str]], out: Path) -> None:
    """Rows of the beampattern table: one block of four lines per topology at Ø200 / h100."""
    lines = ["% generated by python/scripts/figures.py"]
    body: list[str] = []
    for topo, geo in BEAM_CUT_GEOMETRY.items():
        sel = [
            r
            for r in rows
            if r["topology"] == topo
            and r["diameter_mm"] == f"{geo.diameter * 1000:.0f}"
            and r["height_mm"] == f"{geo.height * 1000:.0f}"
        ]
        by_f = {int(r["freq_hz"]): r for r in sel}
        if body:
            body.append("\\midrule")
        for name, key, fmt in (
            ("$\\Delta\\phi$", "width_az_deg", lambda v: width_cell(float(v))),
            ("$\\Delta\\theta_\\uparrow$", "width_up_deg", lambda v: width_cell(float(v), 180.0)),
            (
                "$\\Delta\\theta_\\downarrow$",
                "width_down_deg",
                lambda v: width_cell(float(v), 180.0),
            ),
            ("PSL", "psl_db", lambda v: "--" if v == "nan" else czech(float(v), 0)),
        ):
            first = (
                f"\\multirow{{4}}{{*}}{{{TOPOLOGY_LABEL[topo]}}}"
                if name.startswith("$\\Delta\\phi")
                else ""
            )
            cells = " & ".join(fmt(by_f[int(f)][key]) for f in TABLE_FREQS)
            body.append(f"{first} & {name} & {cells} \\\\")
    lines.append("\\newcommand{\\bprows}{%\n" + "\n".join(body) + "}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n")


ABLATION_LABEL = {
    rs.ABLATION_BASELINE: "bez úprav (M3)",
    "guard1": "1 ochranný bin",
    "guard2": "2 ochranné biny",
    "hop128": "překryv 75\\,\\%",
    "smooth1": "vyhlazení $\\pm1$ bin",
    "smooth2": "vyhlazení $\\pm2$ biny",
    "snr": "váha $g_k$",
    "snr+guard1": "$g_k$ + 1 ochranný bin",
}
ABLATION_ORDER = (
    "baseline",
    "snr",
    "guard1",
    "guard2",
    "smooth1",
    "smooth2",
    "hop128",
    "snr+guard1",
)


def ablation_tex(rows: list[rs.Row], out: Path) -> None:
    """Rows variant x method with the RMSE at 0 dB and the 0 dB and 30 dB ratio to the baseline."""
    ab = need(rows, "ablation")
    variants = [v for v in ABLATION_ORDER if any(r["variant"] == v for r in ab)]
    methods = [m for m in ("das", "mvdr", "srp_phat", "music") if any(r["method"] == m for r in ab)]
    body = []
    for v in variants:
        cells = []
        for m in methods:
            hit = rs.stage_rows(ab, "ablation", variant=v, method=m, snr_db="0")
            cells.append(czech(float(hit[0]["rmse"]), 2) if hit else "--")
        if v == rs.ABLATION_BASELINE:
            tail = "-- & --"
        else:
            r0, r30 = rs.variant_ratio(ab, v, 0.0), rs.variant_ratio(ab, v, 30.0)
            tail = f"{czech(r0, 2)} & {czech(r30, 2)}"
        body.append(f"{ABLATION_LABEL[v]} & " + " & ".join(cells) + f" & {tail} \\\\")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "% generated by python/scripts/figures.py\n"
        + "\\newcommand{\\ablrows}{%\n"
        + "\n".join(body)
        + "}\n"
        + f"\\newcommand{{\\ablN}}{{{ab[0]['n']}}}\n"
    )


def confirm_tex(rows: list[rs.Row], out: Path) -> None:
    """Rows of the confirmation: geometry x (RMSE per method at 0 dB, outliers, low band)."""
    conf = need(rows, "confirm")
    scores = rs.geometry_scores(conf, "confirm", 0.0)
    body = []
    for geo in sorted(scores, key=lambda x: (scores[x], x.size_key())):
        sel = {
            r["method"]: r
            for r in rs.stage_rows(
                conf,
                "confirm",
                topology=geo.topology,
                diameter_mm=f"{geo.diameter * 1000:.0f}",
                height_mm=f"{geo.height * 1000:.0f}",
                snr_db="0",
            )
        }
        cells = " & ".join(czech(float(sel[m]["rmse"]), 2) for m in ex.METHODS)
        score = czech(scores[geo], 2)
        name = f"{TOPOLOGY_LABEL[geo.topology]}, {geo.diameter * 1000:.0f}/{geo.height * 1000:.0f}"
        body.append(f"{name} & {cells} & {score} \\\\")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "% generated by python/scripts/figures.py\n"
        + "\\newcommand{\\confrows}{%\n"
        + "\n".join(body)
        + "}\n"
        + f"\\newcommand{{\\confN}}{{{conf[0]['n']}}}\n"
    )


def cost_tex(out: Path) -> None:
    """Million MAC of every method for the default workload and the assumed time on the MCU."""
    w = cs.Workload.from_config()
    macro = {
        "das": "DAS",
        "mvdr": "MVDR",
        "srp_phat": "SRP",
        "music": "MUSIC",
        "gcc_phat_ls": "GCC",
    }
    lines = ["% generated by python/scripts/figures.py"]
    for m, name in macro.items():
        c = cs.cost(m, w)
        lines.append(f"\\newcommand{{\\mac{name}}}{{{czech(c.mac / 1e6, 0)}}}")
        lines.append(f"\\newcommand{{\\time{name}}}{{{czech(c.time_s(), 2)}}}")
    lines.append(f"\\newcommand{{\\macDirections}}{{{w.D}}}")
    lines.append(f"\\newcommand{{\\costCyclesPerMac}}{{{czech(cs.CYCLES_PER_MAC, 0)}}}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--summary", type=Path, default=Path("python/results/summary.csv"))
    parser.add_argument("--figures", type=Path, default=Path("report/figures"))
    parser.add_argument("--generated", type=Path, default=Path("report/generated"))
    parser.add_argument(
        "--beampattern-csv", type=Path, default=Path("python/results/beampattern.csv")
    )
    args = parser.parse_args()

    style()
    rows = rs.read_summary(args.summary)
    sweep_figure(rows, args.figures / "sweep.pdf")
    beampattern_figure(args.figures / "beampattern.pdf")
    bp_rows = beampattern_results()
    write_csv(args.beampattern_csv, bp_rows)
    beampattern_tex(bp_rows, args.generated / "beampattern.tex")
    ablation_tex(rows, args.generated / "ablation.tex")
    confirm_tex(rows, args.generated / "confirm.tex")
    cost_tex(args.generated / "cost.tex")
    print(f"wrote figures to {args.figures} and tables to {args.generated}")


if __name__ == "__main__":
    main()
