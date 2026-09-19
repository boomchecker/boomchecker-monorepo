# Literature Map Memory

## Usage Rules
- `confirmed`: Use sources to support claim framing, method credibility, and limitation discussion — not citation padding.
- `confirmed`: Keep citations tied to explicit argument roles.

## Carried Over from `projects/2026-maxam-fxlms-filter/report/references.bib`
- `elliott1993active`, `kuo1996anc`, `widrow1985adaptive`, `burgess1981duct`, `ramos2021modified` — classical/adaptive ANC and FxLMS foundations. Imported into `article/references.bib` and cited.
- `basso2024dads`, `alemadi2019drone`, `ramos2024dronenoise` — DADS drone-audio dataset and drone-noise characterization. Imported and cited.
- `xiao2006narrowband`, `betaflightDshotRpm`, `ardupilotEscTelemetry`, `bi2025directional`, `steiner2026drone` — narrowband ANC with tachometric references, ESC telemetry limits, and the two published drone-ANC experiments. Imported and cited.
- `confirmed`: `article/references.bib` is now a full copy of `projects/2026-maxam-fxlms-filter/report/references.bib`; the two files will drift, so re-check before submission.

## To Add
- `to-verify`: Two claims in Section II-B come from `projects/2026-maxam-fxlms-filter/report/motory-pruzkum.md` without a bib entry yet, marked `% TODO cite:` in the .tex: the NASA measurement of small quadcopter motors/propellers (tones dominant, BPF harmonics to ~4 kHz, propeller loading amplifies motor tones by 5 to 15 dB) and the analytical rotor tonal-noise work (loading noise, unsteady loading). Find the originals; the survey's own citation markers do not resolve to references.
- `to-verify`: UAV/drone ego-noise suppression papers specific to acoustic sensing payloads (as opposed to general ANC).
- `to-verify`: Impulsive acoustic event detection literature (gunshot/explosion detection) — can reuse the source list already collected in `papers/2026-bec-conference/article/sources/` if the downstream-detector narrative (option b in `writing_plan.md`) is chosen.
- `to-verify`: Recent work on drone-noise removal / bird-drone bioacoustics interference (adjacent, reviewer-relevant field).
