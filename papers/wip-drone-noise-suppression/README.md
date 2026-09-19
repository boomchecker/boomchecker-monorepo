# Drone Ego-Noise Suppression (work in progress, venue TBD)

Paper-writing scaffold for a contribution on suppressing drone (multirotor UAV)
ego-noise to improve detection of impulsive acoustic events.

**Status:** scaffold only; no article content drafted, no venue picked. The
blocking item is the measurement campaign: the real acoustic-path measurements
are not done yet, and everything the paper claims quantitatively depends on
them. Pick the venue once the data exists, then fill in
`.agents/memories/submission_guidelines.md` and size the text to its page limit.

The directory keeps the `wip-` prefix until then; rename it to
`<year>-<venue>-drone-noise-suppression` when the target is chosen.

## Layout

- `article/`: IEEE-style LaTeX source (`article_main.tex`, `references.bib`,
  `figs/`, `sources/`). Currently a section skeleton.
- `.agents/memories/`: persistent working memory for AI tools (vision/scope,
  submission guidelines, experiment plan, writing plan, literature map), the
  same structure as `papers/2026-bec-conference/.agents/memories/`.
- `Taskfile.yml`: `task papers:dronenoise:build` from the repository root.

The IEEE conference template is **not** copied in here; it lives once in
`papers/templates/IEEE-conference-template-062824/`. `article_main.tex` uses the
`IEEEtran.cls` installed by `task setup` (texlive-publishers), so the template
directory is reference material, not a build input.

## Related work in this monorepo

- `scripts/lms-filter/`: the FxLMS active-noise-control prototype (C core +
  Python harness + DADS drone-spectrum analysis) this paper builds on. The
  earlier Czech-language internal report is in `scripts/lms-filter/report/`.
- `papers/2026-bec-conference/` + `projects/2026-zelinjak-artillery-detection/`:
  the impulsive artillery-event detector/classifier, a candidate downstream
  task for a two-stage "noise suppression improves detection" narrative.

## Build

```bash
task papers:dronenoise:build     # from the repository root
```

Run `task latex:setup` once beforehand (TeX Live and the required packages).

## Next steps

1. Finish the measurement campaign (see `.agents/memories/experiment_plan.md`).
2. Pick a venue and record its CFP details in
   `.agents/memories/submission_guidelines.md`; the page limit constrains
   section-level scope planning, so do this before drafting prose.
3. Decide the paper's narrative scope (see open decisions in
   `.agents/memories/writing_plan.md`).
