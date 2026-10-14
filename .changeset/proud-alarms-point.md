---
"fw-bom-stm32node": minor
---

`detect` prints where the unit is whenever the alarm goes on: every `ALM ... ON`
is followed by

```
ALARM t=3.328 lat=50.102057 lon=14.426130 sats=7 hdop=1.2 age=0.4 utc=2026-10-14T11:24:43Z
```

with the last GPS fix in decimal degrees (north/east positive), satellites and
HDOP from GGA, how many seconds old the fix is and the UTC time the module took
it; every field is `-` while the board has had no fix since boot.

The board listens to the Teseo on UART4 for the whole `detect` run (about 20
bytes per mic block, parsed between blocks) and `gps` updates the same fix, so
a fix outlives the run that saw it. RMC and GGA parsing lives in
`Core/Src/nmea.c`, without HAL; only sentences with a valid checksum count.
