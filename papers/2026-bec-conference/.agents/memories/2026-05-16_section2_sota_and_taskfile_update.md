# Memory Update (2026-05-16)

## Scope Completed
- `confirmed`: Reworked Section II in `article/article_main.tex` from **Related Work** to a fuller **State of the Art** section.
- `confirmed`: Expanded Section II to multi-subsection structure with descriptive emphasis and only light critique.
- `confirmed`: Integrated user-requested framing: classical acoustic detection, artillery/gunfire signal characteristics, post-detection classification, and embedded deployment constraints.

## Section II Structure Now
1. `confirmed`: `Classical Acoustic Detection of Impulsive Events`
2. `confirmed`: `Acoustic Signature of Artillery and Gunfire`
3. `confirmed`: `From Detection to Post-Detection Classification`
4. `confirmed`: `Embedded Deployment and Runtime Constraints`
- `confirmed`: Added explicit bridge sentence to Method section, including mention that median-filter detector details are described later in Methods.

## Sources/Citations Added
- `confirmed`: Added and used new citation for prior Svatos/Holub work:
  - `svatos2024fcc` (Measurement Science and Technology, 2024, DOI: `10.1088/1361-6501/ad3c5d`)
- `confirmed`: Section II now cites previously added artillery acoustics, ML, and embedded references consistently.

## Build System Update
- `confirmed`: Updated `Taskfile.yml` `build` task to compile from `article/` and output `article/article_main.pdf`.
- `confirmed`: Build pipeline now runs:
  1. `pdflatex article_main.tex`
  2. `bibtex article_main`
  3. `pdflatex article_main.tex`
  4. `pdflatex article_main.tex`
- `confirmed`: This ensures bibliography generation plus two final LaTeX passes for citation/reference linking.

## Practical Note
- `confirmed`: Local TeX tooling in this environment has MiKTeX profile/permission issues in some runs; document content and build commands were updated regardless.
