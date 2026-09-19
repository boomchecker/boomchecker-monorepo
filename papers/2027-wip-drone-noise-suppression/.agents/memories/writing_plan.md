# Writing Plan Memory

## Drafting Strategy
- `confirmed`: Conference paper, 5 pages max. `article/article_main.tex` holds the background (Sections I to IV: intro, multirotor propulsion and ESCs, noise sources, motor-derived references, FxLMS, evaluation methodology), about 2.5 pages, condensed from `projects/2026-maxam-fxlms-filter/report/main.tex` and `motory-pruzkum.md`. The background must stay within 3 pages; Results and Conclusion are empty placeholders for the real-drone measurement.
- `to-verify`: Target length depends on the venue's page limit (see `submission_guidelines.md`), unknown until a venue is picked; assume something tighter than BEC's 4-6 pages and keep the story tight from the start.
- `confirmed`: The simulation results of the Czech report (SISO 24.5 dB, sum-first 6.7 dB, MISO 4x4 21.3 dB) are not in the paper at all, not even commented out. The paper's claims come from the real-drone measurement. Do not bring them back without asking.
- `to-verify`: Decide narrative shape with the user: (a) pure ANC/noise-suppression paper evaluated by attenuation metrics only, vs. (b) two-stage paper connecting suppression to downstream impulsive-event detection performance (reusing the `papers/2026-bec-conference/` classifier). Option (b) is more novel but needs the new end-to-end experiment noted in `experiment_plan.md`.

## Section-Level Build Order (draft)
1. `confirmed`: Introduction -> UAV ego-noise problem, relevance to acoustic sensing/impulsive-event detection, contribution list.
2. `confirmed`: Related Work -> classical/adaptive ANC (FxLMS family), drone ego-noise literature, impulsive/acoustic event detection, embedded deployment constraints.
3. `confirmed`: Method -> signal model, reference source, FxLMS variant used, integration point with downstream detector (if option b).
4. `confirmed`: Experimental Setup -> DADS corpus, synthetic mixture protocol, (optional) impulsive-event corpus, metrics.
5. `confirmed`: Results -> attenuation results, (optional) downstream detection results.
6. `confirmed`: Discussion -> scope boundaries (simulated vs. measured acoustic path, single- vs multi-channel, embedded feasibility).
7. `confirmed`: Conclusion -> summary + future work (real-hardware validation, embedded real-time implementation).

## Open Decisions (ask user before drafting prose)
- `to-verify`: Paper title and exact framing/contribution claims.
- `to-verify`: Author list and affiliations for the actual submission.
- `to-verify`: Whether to include the downstream detector experiment or keep this a focused ANC-only paper.
