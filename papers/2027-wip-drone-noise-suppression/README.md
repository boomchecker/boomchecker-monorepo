# Drone Ego-Noise Suppression (work in progress, venue TBD)

Paper-writing scaffold for a contribution on suppressing drone (multirotor UAV)
ego-noise to improve detection of impulsive acoustic events.

**Status:** background drafted in English, no results, no venue picked. The
paper is a conference contribution of 5 pages at most; the background
(Sections I to IV: abstract, problem, multirotor propulsion and ESCs, noise
sources, motor-derived references, FxLMS, evaluation metric) takes about 2.5
pages and must stay within 3, so that the real-drone results and the
conclusion get the rest.

Sources: `projects/2026-maxam-fxlms-filter/report/main.tex` for the FxLMS and
DADS material, `projects/2026-maxam-fxlms-filter/report/motory-pruzkum.md` for
the propulsion, ESC, protocol, and reference-signal material. The simulation
results of that report are **not** in this paper and are not parked in it
either; they stay in the report. `% TODO:` comments in the .tex mark what the
measurement campaign has to fill in, and two `% TODO cite:` items mark claims
from the motor survey that still lack a bib entry.

Pick the venue once the data exists, then fill in
`.agents/memories/submission_guidelines.md` and size the text to its page
limit.

The directory keeps the `wip-` prefix until then; rename it to
`<year>-<venue>-drone-noise-suppression` when the target is chosen.

## Layout

- `article/`: IEEE-style LaTeX source (`article_main.tex`, `references.bib`,
  `figs/`, `sources/`).
- `article/figs/`: `dads_average_spectrum.png` (the DADS spectral
  characterization) copied from
  `projects/2026-maxam-fxlms-filter/report/figures/`, plus the standalone TikZ
  block diagram `fxlms_schema.tex` copied from that report's `generated/`,
  compiled by `task papers:dronenoise:figures` (PDF git-ignored). The report's
  other figures (multichannel diagram, simulation result plots) are not used.
- `.agents/memories/`: persistent working memory for AI tools (vision/scope,
  submission guidelines, experiment plan, writing plan, literature map), the
  same structure as `papers/2026-bec-conference/.agents/memories/`.
- `Taskfile.yml`: `task papers:dronenoise:build` from the repository root.

The IEEE conference template is **not** copied in here; it lives once in
`papers/templates/IEEE-conference-template-062824/`. `article_main.tex` uses the
`IEEEtran.cls` installed by `task setup` (texlive-publishers), so the template
directory is reference material, not a build input.

## Related work in this monorepo

- `projects/2026-maxam-fxlms-filter/`: the FxLMS active-noise-control prototype (C core +
  Python harness + DADS drone-spectrum analysis) this paper builds on. The
  earlier Czech-language internal report is in `projects/2026-maxam-fxlms-filter/report/`.
- `papers/2026-bec-conference/` + `projects/2026-zelinjak-artillery-detection/`:
  the impulsive artillery-event detector/classifier, a candidate downstream
  task for a two-stage "noise suppression improves detection" narrative.

## Build

```bash
task papers:dronenoise:build     # from the repository root
task papers:dronenoise:figures   # TikZ block diagram only
task papers:dronenoise:clean
```

Run `task latex:setup` once beforehand (TeX Live and the required packages).

## Next steps

1. Run the measurement campaign (see `.agents/memories/experiment_plan.md`)
   and write the Results section from it.
2. Pick a venue and record its CFP details in
   `.agents/memories/submission_guidelines.md`.
3. Find bib entries for the two `% TODO cite:` claims in Section II-B (small
   quadcopter motor acoustics, rotor tonal-noise analysis).
4. Decide the paper's narrative scope (see open decisions in
   `.agents/memories/writing_plan.md`), in particular whether the downstream
   detector experiment is in.
