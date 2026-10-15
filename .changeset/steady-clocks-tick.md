---
"fw-bom-stm32node": patch
---

The clock tree runs from the 24 MHz crystal the board carries.

The firmware was set up for a 25 MHz HSE (the Nucleo's crystal: HSE_VALUE
25000000, PLL1 M=2 N=40), while bch-stmnode has a 24 MHz one. The core ran at
240 MHz instead of 250 and everything on its PLL 4 % slow: the GNSS UART at
about 9216 Bd instead of 9600 (board A flagged 41 % of the received bytes with
a noise error), SPI1 to the radio, HAL_Delay and the timeouts. HSE_VALUE is now
24000000 and PLL1 runs M=6 N=125 (a 4 MHz input, VCO 500 MHz): SYSCLK 250 MHz
and PLL1Q 125 MHz as before, the .ioc carries the same values. USB and the
microphone never ran from the HSE. A board with a 25 MHz crystal needs the old
values.
