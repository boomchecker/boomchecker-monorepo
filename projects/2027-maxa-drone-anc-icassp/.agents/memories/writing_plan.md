# Writing Plan Memory

## Drafting Strategy
- `confirmed`: Use `article/article_main.tex` (currently a section skeleton) as scaffold.
- `to-verify`: Target length depends on the real ICASSP 2027 page limit (see `submission_guidelines.md`) — ICASSP is historically stricter (4+1 pages) than BEC's 4-6 pages, so keep the story tight from the start.
- `confirmed`: Reuse the FxLMS/ANC framing and DADS spectral-analysis material already written (in Czech) in `scripts/lms-filter/report/main.tex` as source material, but rewrite in English and re-verify all numbers.
- `to-verify`: Decide narrative shape with the user: (a) pure ANC/noise-suppression paper evaluated by attenuation metrics only, vs. (b) two-stage paper connecting suppression to downstream impulsive-event detection performance (reusing `projects/2026-zelinjak-artillery-detection/BEC/` classifier). Option (b) is more novel but needs the new end-to-end experiment noted in `experiment_plan.md`.

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
