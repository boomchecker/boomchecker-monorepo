# 2026 student kickoff

Slides for the first joint meeting with the bachelor students working on drone
detection and localization (intro, how we develop in the monorepo, Kanban on GitHub
Projects). English slides, Czech talk.

Build from this folder:

```bash
task setup   # once: installs texlive-fonts-extra (Fira fonts, newtxsf, fontawesome5)
task build   # -> slides/slides.pdf
```

The folder is excluded from the MkDocs site (`exclude_docs` in `mkdocs.yml`).

## Timing (60-minute slot)

26 main frames plus two live demos; the two frames after `\appendix` are backup
(Taskfile, AI in the thesis text) and are shown only if a question comes up.

| Block                                              | Frames | Budget |
| -------------------------------------------------- | ------ | ------ |
| Title, Today                                       | 2      | 2 min  |
| What we build                                      | 3      | 6 min  |
| The five theses + Independent, but connected       | 6      | 10 min |
| How we develop (repo … CI), incl. 3-min GitHub demo | 7      | 13 min |
| AI (CTU rules, practice, our stance, Claude seat)  | 4      | 7 min  |
| How we plan (Agile, Kanban + live demo, milestones) | 3      | 10 min |
| Next steps, questions                              | 2      | 4 min  |

That is about 52 minutes of talk and 8 minutes of buffer for questions.

## Screenshots

The only screenshot is `slides/figs/docs-screen.png` (docs.boomchecker.cz, Get Started
page with the left nav). The "What it looks like on GitHub" slide is a live demo:
branch protection rule and a merged PR are shown in the browser, not as images.
