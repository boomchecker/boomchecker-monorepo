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

## Timing (50-minute slot)

24 main frames plus two live demos; the four frames after `\appendix` are backup
(Taskfile, CI, AI in the thesis text, Our stance) and are shown only if a question comes up.

| Block                                              | Frames | Budget |
| -------------------------------------------------- | ------ | ------ |
| Title, Today                                       | 2      | 1 min  |
| What we build                                      | 3      | 6 min  |
| The five theses + Independent, but connected       | 6      | 12 min |
| How we develop (repo … GitHub demo), incl. 3-min demo | 7   | 12 min |
| AI (CTU rules, practice, Claude seat)              | 3      | 5 min  |
| How we plan (Agile, Kanban + live demo, milestones) | 3     | 9 min  |
| Next steps, questions                              | 2      | 5 min  |

That is 47 minutes of talk and 3 minutes for questions.

Each main frame has `\budget{n}` (its minutes) in `slides.tex`; the footer then shows
"⏱ 13:18": the wall-clock time at which you should arrive at that frame, counted from
`\talkstart{13}{00}`. If the room clock is already past it, you are behind. Change
`\talkstart` when the meeting starts at another time.

Speaker badges (initials next to the frame number, set with `\speaker{..}` in `slides.tex`):
JS on 1 to 4 and 10, MM on 11 to 17 and 22 to 24, KH on 18 to 21, the thesis frames 5 to 9
and the backup have none.

## Screenshots

The only screenshot is `slides/figs/docs-screen.png` (docs.boomchecker.cz, Get Started
page with the left nav). The "What it looks like on GitHub" slide is a live demo:
branch protection rule and a merged PR are shown in the browser, not as images.
