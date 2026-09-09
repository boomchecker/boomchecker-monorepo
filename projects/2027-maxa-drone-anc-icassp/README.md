# Drone Ego-Noise Suppression — ICASSP 2027 (working title)

Paper-writing scaffold for a submission to the **2027 IEEE International
Conference on Acoustics, Speech, and Signal Processing (ICASSP 2027)** on
suppressing drone (multirotor UAV) ego-noise to improve detection of
impulsive acoustic events.

Status: scaffold only — no article content drafted yet, no CFP details
verified. See `.agents/memories/` for the working plan and open decisions.

## Layout

- `article/` — IEEE-style LaTeX source (`article_main.tex`, `references.bib`,
  `figs/`, `sources/`). Currently a section skeleton.
- `templates/IEEE-conference-template/` — unmodified copy of the root
  `templates/IEEE-conference-template/` IEEE conference template
  (`IEEEtran.cls` + example `.tex`), kept local to this project for
  self-containment.
- `.agents/memories/` — persistent working memory for AI tools (vision/scope,
  submission guidelines, experiment plan, writing plan, literature map),
  mirroring the structure used in
  `projects/2026-zelinjak-artillery-detection/BEC/.agents/memories/`.
- `Taskfile.yml` — `task build` compiles `article/article_main.tex` to PDF.

## Related work in this monorepo

- `scripts/lms-filter/` — the FxLMS active-noise-control prototype (C core +
  Python harness + DADS drone-spectrum analysis) this paper builds on.
- `projects/2026-zelinjak-artillery-detection/BEC/` — the impulsive
  artillery-event detector/classifier, a candidate downstream task for a
  two-stage "noise suppression improves detection" narrative.

## Next steps

1. Confirm ICASSP 2027 CFP details (page limit, deadline, track, blind-review
   policy) and fill in `.agents/memories/submission_guidelines.md`.
2. Decide the paper's narrative scope with the user (see open decisions in
   `.agents/memories/writing_plan.md`).
3. Plan and run the new experiments listed in
   `.agents/memories/experiment_plan.md`.
