---
"fw-bom-stm32node": minor
---

The `detect` trailer stamps when the run first called a window DRONE and first raised the alarm.

`DETEND windows=<n> drones=<n> alarms=<n> first_drone=<s.mmm|-> first_alarm=<s.mmm|-> overrun=<0|1> err=<0|1>`:
the two new fields are seconds into the run, `-` when it never happened. A field
log of a flight then needs nothing but the trailer of each run - how many windows,
how many called drone, how many alarms, and how long the drone had to be audible
before the first window and the first alarm said so - which is what the outdoor
comparison of `mlp_f2`, `gbt_m1` and `mlp_m1` records per height. The host tool
parses them into `DetectTrailer.first_drone_s` / `first_alarm_s` (None for `-` or
for an older firmware) and prints them after its window and alarm counts.
