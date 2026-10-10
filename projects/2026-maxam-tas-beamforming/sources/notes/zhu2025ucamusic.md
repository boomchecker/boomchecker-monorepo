# Zhu et al. 2025: 2D směrové hledání RF signálů UAV pomocí UCA a MUSIC-WAA (RF, ne akustika)

## Bibliografie
- Plná citace (podle PDF, s. 1): Jizan Zhu, Kuangang Fan, Qing He, Jingzhen Ye, Aigen Fan. "Two-Dimensional Real-Time Direction-Finding System for UAV RF Signals Based on Uniform Circular Array and MUSIC-WAA". Drones 2025, 9(4), 278 (article no. 278; 22 stran). DOI: 10.3390/drones9040278. Received 3 March 2025; revised 3 April 2025; accepted 4 April 2025; published 7 April 2025 [s. 1]. Academic Editor: Pablo Rodríguez-Gonzálvez.
- Afiliace: School of Electrical Engineering and Automation, Jiangxi University of Science and Technology, Ganzhou, China; Jiangxi Province Key Laboratory of Multidimensional Intelligent Perception and Control; Shenzhen Institute of Radio Testing & Tech [s. 1].
- Vydavatel: MDPI, Basel. MDPI: ano. Open access: ano (CC BY 4.0) [s. 1]. Data: "available upon request" [s. 21].
- Číslování stran PDF = "n of 22" v záhlaví.
- DŮLEŽITÉ: článek je o rádiových (RF) signálech UAV, ne o akustice. Pro náš projekt je relevantní jen jako analogie pro UCA + MUSIC a pro omezení elevačního odhadu u planárního UCA.
- Návrh BibTeX:
```bibtex
@article{zhu2025ucamusic,
  author  = {Zhu, Jizan and Fan, Kuangang and He, Qing and Ye, Jingzhen and Fan, Aigen},
  title   = {Two-Dimensional Real-Time Direction-Finding System for {UAV} {RF} Signals Based on Uniform Circular Array and {MUSIC-WAA}},
  journal = {Drones},
  volume  = {9},
  number  = {4},
  pages   = {278},
  year    = {2025},
  publisher = {MDPI},
  doi     = {10.3390/drones9040278}
}
```

## Problém a přínos
- Problém: UCA-MUSIC pro 2D DOA (azimut + elevace) vyžaduje vyčerpávající prohledávání spektra; při rozlišení 0.1 stupně "approximately 3.24 million spectral function computations" [s. 2-3]. Autoři tvrdí, že to znemožňuje real-time.
- Přínos [s. 3]: (1) hybrid UCA + MUSIC + WAA (weighted average algorithm, metaheuristika [20] Jun a De 2024) snižuje počet výpočtů spektrální funkce z 3 240 000 na 1200 ("more than 99.9%"); (2) levný hardware: 6 SDR HackRF One R10 se sdíleným hodinovým signálem a spouštěním FPGA, dvoupásmový provoz 2.4 a 5.8 GHz je podle autorů možný výměnou antén (postavená anténní sada je jen 2.4 GHz) [s. 3, 10]; (3) hovering experiment: průměrná chyba 7.0 stupňů v azimutu a 7.7 stupňů v elevaci pro vzdálenost 30-200 m a výšku 20-90 m [s. 1, 3]. Tyto headline hodnoty platí až po vyřazení měřicího bodu 11 (s outlierem je průměr 10.9/8.9 stupňů), což abstrakt ani úvod neuvádějí; vyřazení je zmíněno až v Sec. 4.3 [s. 16-17].
- Poznámka k odvození (interpretace): 3 240 000 = 900 (elevace 0-90 stupňů po 0.1) x 3600 (azimut 0-360 po 0.1); 1200 = 30 (populace) x 40 (iterací) podle Sec. 2.3; v Sec. 4.1 se používá 30 iterací, což by dalo 900 vyhodnocení (neuvedeno autory).

## Mikrofonní pole a hardware
- Pole: 6 prvků UCA (složené dipóly 2.4 GHz, zisk 6 dBi), poloměr R = 6.25 cm (λ = 12.5 cm při 2.4 GHz; autoři píšou "corresponding to half-wavelength spacing"; pro M = 6 je strana pravidelného šestiúhelníku rovna poloměru, tedy vzdálenost sousedních prvků = R = 6.25 = λ/2, takže tvrzení autorů sedí; předchozí verze poznámky to chybně označila za záměnu poloměru a rozteče), na stativu ve výšce 120 cm [s. 10-11].
- Příjem: 6 x HackRF One R10 (1 MHz - 6 GHz, ADC max 20 MSPS), stejně dlouhé RF kabely [s. 10].
- Synchronizace: hodiny 10 MHz, 3.3 V, 8kanálový generátor BG7TBL; spouštění: upravený firmware HackRF (čeká na náběžnou hranu), FPGA Altera EP4CE10E22 generuje spouštěcí pulz ("5-pulse-per-second (PPS) rising-edge signal", jak je v textu) [s. 10-11].
- Kalibrace: po každém startu nutná (zesílení, fáze, skupinové zpoždění kanálů se liší po každém restartu); kalibrační signál CW 2.407 GHz ve vzdálenosti 6 m východně, zarovnaný s anténou 0; očekávané fázové rozdíly kanálů 1-5 vůči kanálu 0: 90, 270, 360, 270, 90 stupňů; kompenzace v hostiteli [s. 13].
- Host: GNU Radio v3.10.7.0, GUI; fs HackRF = 2 MHz, střední frekvence 2407 MHz, IF a VGA zisky 30 dB [s. 11-13]. Bit depth: neuvedeno (HackRF je 8bitový, ale v článku se neuvádí). Autoři zdůvodňují fs = 2 MHz a 10 240 snapshotů omezeným výpočetním výkonem hostitele a tvrdí, že MUSIC závisí hlavně na počtu snapshotů, ne na fs [s. 12]. Při fs = 2 MHz odpovídá 10 240 snapshotů 5.12 ms signálu (vlastní výpočet, interpretace). Tab. 2 [s. 13] uvádí u snapshotů chybnou jednotku "10,240 MHz"; z textu (s. 12) je zřejmé, že jde o 10 240 snapshotů (překlep ve zdroji).

## Signálový model a rovnice
- (1)-(2) [s. 4]: úzkopásmový signál $\tilde s(t)=s(t)e^{j2\pi f_0 t}$; $\tilde x_m(t)\approx s(t)e^{j2\pi f_0(t-\tau_m)}$.
- (3) [s. 4] poloha prvku: $p_m=[R\cos\gamma_m,\ R\sin\gamma_m,\ 0]$, $\gamma_m=2\pi m/M$; (4) jednotkový vektor směru: $r=[-\sin\theta\cos\varphi,\ -\sin\theta\sin\varphi,\ \cos\theta]$.
- (5)-(6) [s. 4]: zpoždění $\tau_m=-\frac Rc\sin\theta\cos(\varphi-\gamma_m)$; fáze $\phi_m(\theta,\varphi)=-\beta R\sin\theta\cos(\varphi-\gamma_m)$, $\beta=2\pi/\lambda$ (v textu "c is the speed of light").
- (7) [s. 4] steering vektor: $a(\theta,\varphi)=[e^{-j\phi_0}\ e^{-j\phi_1}\cdots e^{-j\phi_{M-1}}]^T$.
- (8)-(9) [s. 4-5]: $x(t)=As(t)+v(t)$; $x(k)=As(k)+n(k)$ (A = matice odezvy pole; v textu k Eq. 8 je šum označen "n" místo $v$, drobná nekonzistence zdroje; AWGN).
- (10)-(12) [s. 5]: $R=\frac1N\sum_{k=1}^N x(k)x^H(k)$; $R=E\Sigma E^H$; $R=U_S\Sigma_S U_S^H+U_N\Sigma_N U_N^H$.
- (13) [s. 5] MUSIC: $P_{MUSIC}(\theta,\varphi)=\dfrac{1}{a^H(\theta,\varphi)U_NU_N^Ha(\theta,\varphi)}$.
- (14)-(15) [s. 5] WAA inicializace: $x_{ij}=rand\cdot(UB_j-LB_j)+LB_j$.
- (16) [s. 6]: $N_C=(nP-4)\dfrac{iter-1}{1-Max_I}+nP$; (17) $SumF=\sum_{i=1}^{N_C}F(X_i)$; (18) STB: $X_{Miu}=\dfrac{\sum_{i=1}^{N_C}X_i\,(SumF-F(X_i))}{SumF\,(N_C-1)}$; (19) LTB: $X_{Miu}=\dfrac{\sum_{i=1}^{N_C}X_i\,F(X_i)}{SumF}$.
- (20) [s. 6]: $k_1=(\alpha\cdot rand-1)\sin\!\left(\dfrac{\pi\,iter}{Max_{Iter}}\right)$; $k_1<0.5$ exploration, jinak exploitation.
- (21)-(23) [s. 6] exploitation: (21) $X_i=w_{11}(X_{Miu}-X_{GBest})+w_{12}(X_{Miu}-X_{PBest})+w_{13}X_{Miu}$; (22) $X_i=w_{21}(X_{Miu}-X_{PBest})+w_{22}X_{PBest}$; (23) $X_i=w_{31}(X_{Miu}-X_{GBest})+w_{32}X_{GBest}$ ($w_{ij}$ náhodná v 0-1); (24)-(28) [s. 7] Lévyho let ($S=U/|V|^{1/\beta}$, $X_{i,j}(iter+1)=X_{GBest_j}(iter)+S$), (29) [s. 7] náhodné restartování v mezích; volba pomocí $k_2\in\{1,2,3\}$ a $k_3$ (>0.5: Eq. 28, jinak Eq. 29).
- (30)-(31) [s. 8] optimalizační formulace: $\min f(x_1,x_2)=-P_{MUSIC}(x_1,x_2)$, $0^\circ\le x_1\le90^\circ$ (elevace), $0^\circ\le x_2<360^\circ$ (azimut).
- Konvence úhlů (vyřešeno vlastním přepočtem z Tab. 4, interpretace): v Eq. (4)-(6) je $\theta$ polární úhel od osy z (kolmice k rovině pole). Přepočtem z Tab. 4 (horizontální vzdálenost d a relativní výška h) vychází "elevace" ve sloupci Flight Log v Tab. 5 jako $\theta = 90^\circ - \arctan(h/d)$, tj. úhel od svislice, ne od horizontu (např. bod 2: arctan(49.4/60.5) = 39.2 stupňů nad horizontem, v Tab. 5 je 50.7; bod 6: 41.9 vs 48.1). Hodnoty ve sloupci Flight Log v Tab. 5 (37.0 až 70.7 stupně) jsou tedy polární úhly, což je v souladu s Eq. (31) (0-90) i s Eq. (4). Samotný text je nekonzistentní: "elevation angle is 0° in the normal direction of the horizontal plane" [s. 15] a Sec. 4.5(1) [s. 20] "broadside direction (θ → 0°) ... sin θ → 0" (zenit, ztráta citlivosti) a "near the array plane (θ → 90°)" (azimutální nejednoznačnost), zatímco Sec. 4.4 [s. 19] říká, že citlivost klesá "as it nears the plane of the UCA antenna". Sec. 4.5 a 4.4 si tedy odporují v tom, kde citlivost elevace klesá; neřešitelné z PDF.

## Algoritmus
- MUSIC-WAA: (1) kovarianční matice z N = 10 240 snapshotů, (2) eigendekompozice, šumový podprostor $U_N$ (K = 1 zdroj, M = 6) [s. 12], (3) WAA hledá maximum $P_{MUSIC}$ (minimum $-P_{MUSIC}$) v prostoru (elevace, azimut) místo úplného prohledávání; populace 30; iterace 40 (simulace, Sec. 2.3) nebo 30 (systém, Sec. 4.1); $\alpha=10$ [s. 8, 12].
- WAA fáze [s. 5-7]: inicializace, vážená průměrná pozice, výběr explorace/exploitace (k1), 3 exploitační pravidla, 2 explorační pravidla (Lévy a náhodné restartování). Srovnání s WOA, SSA, PSO, GWO [s. 9].
- Počet zdrojů: 1 [s. 12]. Frekvenční pásmo: signál drona má šířku 10 MHz, střed 2407 MHz [s. 14]; model je úzkopásmový (Sec. 2.1), dron vysílá nespojitý nepravidelný signál (Fig. 8, s. 14), zpracovává se fs = 2 MHz; váhování a regularizace: neuvedeno.
- Postprocessing výsledků: průměr přes 1 s po odstranění outlierů kritériem MAD ("more than three times the median deviation") [s. 16, 18].

## Experiment / simulace
- Simulace (MATLAB 2022b) [s. 8-9]: 4 příchozí signály s různými úhly (v Tab. 1 a Fig. 3 vyhodnocené každý zvlášť, interpretace) na 2.4 GHz, SNR 5 dB (počet snapshotů v simulaci neuveden), 6prvkový UCA R = 6.25 cm, populace 30, 40 iterací, $\alpha=10$. Srovnání WAA s WOA, SSA, PSO, GWO (Fig. 3).
- Terénní měření [s. 13-15]: lokalita na předměstí s nízkým RF rušením; dron DJI Air 2S (ref [29]), HD režim, pásmo 2.4 GHz, manuální kanál, šířka pásma 10 MHz, střed 2407 MHz (Tab. 3). Dva lety, 26 míst visení (15 + 11), min. 10 s visení na místě; vzdálenost 30-200 m, výška 30-90 m (v textu na s. 14 "altitudes between 20 m and 90 m", na s. 15 "altitude varied between 30 m and 90 m", v Tab. 4 min. 29.7 m, max 90.0 m; dolní mez 20 m na s. 1, 3, 14 nemá oporu v tabulce, nejde o platný údaj z měření; ponecháno jako nesoulad ve zdroji). "Distance" v Tab. 4 je horizontální vzdálenost od místa vzletu (ověřeno přepočtem ze souřadnic, shoda do 0.5 m), "Height" je rozdíl výšky vůči vzletu, "Altitude" absolutní výška z logu. DJI Air 2S: downlink 2.4/5.8 GHz, udávaný dosah 12 km, doba letu cca 30 min [s. 14]. Pole považováno za kolokované se vzletovým bodem (0.5 m od něj). Ground truth: letové logy (zeměpisná šířka, délka, výška), azimut vypočten Vincentyho vzorci [30] (0 stupňů = východ, kladný směr proti hodinám), elevace arctan z rozdílu výšek [s. 14-16].
- Dron je RF vysílač (downlink video), BPF a akustika se neřeší.

## Výsledky (čísla)
- WAA simulace, Tab. 1 [s. 9]:

| Index | Skutečný úhel (elev., azim.) | Výsledek WAA | Absolutní chyba |
|---|---|---|---|
| a | (20.0, 60.0) | (19.9, 60.6) | (0.1, 0.6) |
| b | (40.0, 100.0) | (40.1, 100.2) | (0.1, 0.2) |
| c | (50.0, 160.0) | (50.1, 159.4) | (0.1, 0.6) |
| d | (70.0, 240.0) | (70.1, 240.4) | (0.1, 0.4) |

  (úhly ve stupních; chyby do 1 stupně [s. 8-9])
- Výpočetní zisk: WAA 40 iterací = 1200 vyhodnocení spektra vs. 3 240 000 při průběžném prohledávání po 0.1 stupně; úspora "approximately 99.9%" [s. 9]. Fakticky 1 - 1200/3 240 000 ≈ 99.96 % (vlastní výpočet, interpretace).
- Tab. 4 [s. 16] (míst visení; Alt. = absolutní výška z logu, Dist. = horizontální vzdálenost od startu, Height = relativní výška; zeměpisné souřadnice ve stupních):

| Idx | Lat | Lon | Alt. [m] | Dist. [m] | Height [m] |
|---|---|---|---|---|---|
| 0 (start) | 25.76984011 | 114.7491364 | 140.4 | 0.0 | 0.0 |
| 1 | 25.76990561 | 114.7487381 | 180.8 | 40.6 | 40.4 |
| 2 | 25.77024192 | 114.7487277 | 189.8 | 60.5 | 49.4 |
| 3 | 25.77027657 | 114.7485007 | 200.1 | 80.0 | 59.7 |
| 4 | 25.77001880 | 114.7484583 | 210.6 | 70.8 | 70.2 |
| 5 | 25.77002978 | 114.7482601 | 220.7 | 90.3 | 80.3 |
| 6 | 25.77030159 | 114.7482745 | 230.4 | 100.4 | 90.0 |
| 7 | 25.77032455 | 114.7480587 | 220.5 | 120.6 | 80.1 |
| 8 | 25.76992283 | 114.7480253 | 210.0 | 111.7 | 69.6 |
| 9 | 25.76990013 | 114.7477439 | 200.5 | 139.7 | 60.1 |
| 10 | 25.77032015 | 114.7472073 | 210.5 | 200.5 | 70.1 |
| 11 | 25.77051998 | 114.7474993 | 220.7 | 180.5 | 80.3 |
| 12 | 25.77051499 | 114.7477191 | 230.4 | 160.5 | 90.0 |
| 13 | 25.76982094 | 114.7477251 | 220.5 | 141.5 | 80.1 |
| 14 | 25.77007673 | 114.7479557 | 210.4 | 121.2 | 70.0 |
| 15 | 25.77013401 | 114.7489039 | 180.5 | 40.0 | 40.1 |
| 16 | 25.77011233 | 114.7491332 | 180.4 | 30.2 | 40.0 |
| 17 | 25.77024124 | 114.7487190 | 190.6 | 60.9 | 50.2 |
| 18 | 25.77056632 | 114.7487316 | 200.1 | 90.1 | 59.7 |
| 19 | 25.77055245 | 114.7491622 | 210.3 | 79.1 | 69.9 |
| 20 | 25.77082168 | 114.7491817 | 220.7 | 109.0 | 80.3 |
| 21 | 25.77094563 | 114.7486809 | 230.3 | 130.7 | 89.9 |
| 22 | 25.77132218 | 114.7486960 | 220.2 | 170.0 | 79.8 |
| 23 | 25.77126209 | 114.7482753 | 220.1 | 179.6 | 79.7 |
| 24 | 25.77081717 | 114.7486373 | 200.4 | 119.2 | 60.0 |
| 25 | 25.77054422 | 114.7490243 | 190.4 | 78.9 | 50.0 |
| 26 | 25.77018674 | 114.7492656 | 170.1 | 40.7 | 29.7 |

  (v textu s. 15 chybně "Index 0 in Table 1", míněno Tab. 4). Vlastní kontrola (interpretace): ze souřadnic Tab. 4 jsem přepočetl azimut a polární úhel; shoda s Tab. 5 "Flight Log" je do 0.1 stupně u všech bodů kromě dvou zjevných nesrovnalostí ve zdroji: bod 12 (Tab. 5 az. 155.2, přepočet 152.2) a bod 23 (Tab. 5 el. 68.7, přepočet 66.1). Chybové sloupce Tab. 5 jsou s hodnotami v téže tabulce vnitřně konzistentní.
- Tab. 5 [s. 17] (stupně; Count = počet odhadů, Out. = podíl outlierů v %):

| Idx | Log az. | Log el. | DF az. | DF el. | Count | Out. [%] | Chyba az. | Chyba el. |
|---|---|---|---|---|---|---|---|---|
| 1 | 169.7 | 45.1 | 162.6 | 36.5 | 6973 | 11.09 | 7.1 | 8.6 |
| 2 | 132.6 | 50.7 | 142.3 | 44.2 | 6034 | 4.21 | 9.7 | 6.5 |
| 3 | 142.8 | 53.3 | 142.8 | 57.4 | 3759 | 4.52 | 0.1 | 4.1 |
| 4 | 163.8 | 45.2 | 166.5 | 42.9 | 6837 | 25.82 | 2.7 | 2.3 |
| 5 | 166.5 | 48.4 | 168.1 | 35.9 | 3085 | 3.95 | 1.6 | 12.5 |
| 6 | 149.4 | 48.1 | 134.9 | 24.6 | 5096 | 9.52 | 14.5 | 23.5 |
| 7 | 153.6 | 56.4 | 154.6 | 60.5 | 5508 | 5.52 | 1.0 | 4.1 |
| 8 | 175.3 | 58.1 | 167.4 | 60.0 | 2818 | 4.05 | 7.9 | 1.9 |
| 9 | 177.3 | 66.7 | 176.9 | 72.6 | 3753 | 18.55 | 0.4 | 5.9 |
| 10 | 164.6 | 70.7 | 157.3 | 81.1 | 4296 | 2.89 | 7.3 | 10.4 |
| 11 | 155.3 | 66.0 | 46.5 | 28.9 | 3748 | 1.70 | 108.8 | 37.1 |
| 12 | 155.2 | 60.7 | 153.9 | 77.2 | 3631 | 5.48 | 1.3 | 16.5 |
| 13 | 180.9 | 60.5 | 181.4 | 57.7 | 4030 | 11.34 | 0.5 | 2.8 |
| 14 | 167.5 | 60.0 | 170.6 | 79.6 | 3618 | 12.24 | 3.1 | 19.6 |
| 15 | 125.5 | 45.0 | 124.8 | 61.9 | 4022 | 14.00 | 0.7 | 16.9 |
| 16 | 90.1 | 37.0 | 100.5 | 30.7 | 1745 | 3.04 | 10.4 | 6.3 |
| 17 | 133.1 | 50.5 | 143.3 | 50.3 | 4820 | 13.86 | 10.2 | 0.2 |
| 18 | 116.5 | 56.5 | 124.4 | 55.8 | 2810 | 11.10 | 7.9 | 0.7 |
| 19 | 88.0 | 48.5 | 107.7 | 38.1 | 2814 | 9.70 | 19.7 | 10.4 |
| 20 | 87.5 | 53.6 | 98.1 | 51.1 | 2143 | 6.07 | 10.6 | 2.5 |
| 21 | 110.3 | 55.5 | 118.9 | 65.5 | 2954 | 9.31 | 8.6 | 10.0 |
| 22 | 105.0 | 64.9 | 115.4 | 69.0 | 3754 | 4.05 | 10.4 | 4.1 |
| 23 | 118.7 | 68.7 | 128.3 | 66.8 | 3350 | 5.04 | 9.6 | 1.9 |
| 24 | 114.7 | 63.2 | 123.0 | 64.0 | 2682 | 4.88 | 8.3 | 0.8 |
| 25 | 98.1 | 57.6 | 108.2 | 50.5 | 3887 | 9.67 | 10.1 | 7.1 |
| 26 | 71.2 | 53.8 | 82.3 | 39.5 | 2548 | 10.68 | 11.1 | 14.3 |
| Průměr (všech 26) | | | | | | | 10.9 | 8.9 |
| Průměr bez bodu 11 | | | | | | | 7.0 | 7.7 |

  (kontrola: průměr ze zaokrouhlených chyb v tabulce vychází bez bodu 11 na 6.99 / 7.76, s bodem 11 na 10.9 / 8.9; autoři uvádějí 7.7, ne 7.8, tj. nejspíš průměr z nezaokrouhlených hodnot; interpretace; Tab. 5 přepsána a strojově porovnána s PDF, všech 26 řádků sedí)

- Souhrn [s. 16-17, 20]: s outlierem (bod 11): max. chyba azimutu 108.8, min. 0.1, průměr 10.9; max. chyba elevace 37.1, min. 0.2, průměr 8.9. Bez bodu 11: max. chyba azimutu 19.7, max. chyba elevace 23.5, průměr azimut 7.0 a elevace 7.7 stupňů; min. chyba elevace 0.2 [s. 20]. Bod 11 vysvětlen multipath od křoví a zařízení ("severe multipath interference"), nízkým SNR a útlumem [s. 16-17]. (Poznámka: Autoři vyřadili bod 11 z hodnocení, což zlepšuje headline hodnotu 7.0/7.7 stupňů; interpretace.)
- Real-time sledování (Fig. 10-11) [s. 18-19]: azimut je dobře sledován; elevace má větší chyby a fluktuace; při vzletu/přistání (blízké pole) větší chyby; druhý let má v azimutu mírně vyšší hodnoty kvůli odchylce kalibrace (chyba "about 10°"); chyba v elevaci v druhém letu menší po kalibraci.

## Omezení podle autorů
- Citlivost elevace UCA: při přibližování k rovině antén ztráta citlivosti; $\sin\theta\to0$ rozšiřuje hlavní lalok; symetrie pole způsobuje azimutální nejednoznačnosti ("main lobe splitting") [s. 20, Sec. 4.5(1)].
- Multipath a RF rušení (zvláště v dynamických úsecích) [s. 20].
- Heuristická povaha WAA: občas lokální optimum místo globálního maxima [s. 20].
- Blízké pole (vzlet/přistání) zhoršuje DOA [s. 19].
- Manuální kalibrace po každém startu; budoucí práce: lepší kalibrace, optimalizace geometrie pole, vylepšení exploration-exploitation WAA [s. 20-21].

## Relevance pro náš projekt
- SOTA: nepřímá; použít max. jako příklad UCA + MUSIC pro 2D DOA UAV v RF doméně a jako ilustraci, že plná 2D mřížka je výpočetně drahá (3.24M vyhodnocení při 0.1 stupně) a že metaheuristické prohledávání může počet vyhodnocení snížit (1200).
- Volba metody a MCU (interpretace): náš úkol (100 ms, hrubší mřížka např. 1-5 stupňů) má řádově méně bodů (např. 360 x 90 = 32 400 při 1 stupni, 1.0 % z 3.24M); heuristiky typu WAA (nedeterministické, nutnost RNG, Lévyho let) jsou pro MCU zbytečné; hierarchické/hrubé-jemné prohledávání je deterministická alternativa. Autoři uvádějí jen počet vyhodnocení, ne čas ani FLOPs na konkrétním hardwaru.
- Geometrie UCA: potvrzení omezení elevace u planárního UCA (citlivost klesá u roviny pole, azimutální ambiguity); to podporuje naši volbu dvou kruhů (2x8, vertikální rozteč 40-100 mm), které přidávají vertikální aperturu (interpretace; článek sám 2-kruhy neuvažuje).
- Kalibrace: autoři zdůrazňují nutnost fázové kalibrace po každém spuštění (fázové a gain rozdíly kanálů); pro MEMS mikrofony je analogická nutná kalibrace citlivosti/fáze (interpretace).
- Nelze přenést: RF, f0 = 2.4 GHz, R = 6.25 cm = λ/2, rozteč sousedních prvků rovněž λ/2 (u nás λ_akustické 0.17-1.1 m pro 300 Hz-2 kHz, takže R ≪ λ na nízkých frekvencích), 6 prvků, 10 240 snapshotů při 2 MHz (u nás ~1600 vzorků při 16 kHz), 1 zdroj, dlouhodobě vyhlazované výsledky (průměr 1 s), vyřazení outlieru.

## Citovatelná tvrzení
- Výpočetní náročnost MUSIC 2D: "employing a UCA for two-dimensional angle estimation using the MUSIC algorithm necessitates approximately 3.24 million spectral function computations" při rozlišení 0.1 stupně [s. 2-3].
- Zisk WAA: "the WAA completed the optimization within just 40 iterations (where the MUSIC spectrum function performed 1200 evaluations during the 40 iterations)" [s. 9].
- Výsledek: "an average azimuth error of 7.0° and elevation error of 7.7° for UAV hovering distances of 30-200 m and heights of 20-90 m" [s. 1].
- ULA nemá elevaci: "ULA-based systems are fundamentally limited to the azimuth range and lack elevation estimation capabilities" [s. 2].
- UCA a elevace: "UCA configurations theoretically enable omnidirectional coverage, practical implementations struggle with sensitivity to the elevation angle and computational inefficiency" [s. 2].
- Elevace slabší: "the elevation angle measurements exhibited larger errors and fluctuations ... reduced sensitivity in its elevation angle measurements as it nears the plane of the UCA antenna" [s. 19].
- Potřeba kalibrace: "channel calibration is required after each system startup to synchronize all of the receiver channels" [s. 13].
- Multipath: "the existence of obstacles in this direction that leads to serious multipath effects" [s. 18].
- Heuristika: "its heuristic nature occasionally prioritized local optima over global peaks" [s. 20].

## Relevantní reference z článku
(přepsáno podle seznamu literatury, s. 21-22; DOI nejsou vypsány v textu, jen jako odkazy "[CrossRef]"; doplněny z cílových URL hyperlinků v PDF, kolo 2)
- [9] Fernandes, R.P.; Apolinário, J.A., Jr.; de Seixas, J.M. A Reduced Complexity Acoustic-Based 3D DoA Estimation with Zero Cyclic Sum. Sensors 2024, 24, 2344. DOI: 10.3390/s24072344
- [16] Batuhan, K.; İbrahim, K.; Alı, R.E.; Serhan, Y.; ALı, G.; M, K.Ö.; Çirpan, H.A. Detection, Identification, and Direction of Arrival Estimation of Drone FHSS Signals with Uniform Linear Antenna Array. IEEE Access 2021, 9, 152057-152069. DOI: 10.1109/ACCESS.2021.3127199
- [17] Alexandru, M.; Cosmin, P.; Ioana-Manuela, M.; Calin, V. Direction-finding for unmanned aerial vehicles using radio frequency methods. Meas. J. Int. Meas. Confed. 2024, 235, 114883. DOI: 10.1016/j.measurement.2024.114883
- [19] Codău, C.; Buta, R.-C.; Păstrav, A.; Dolea, P.; Palade, T.; Puschita, E. Experimental Evaluation of an SDR-Based UAV Localization System. Sensors 2024, 24, 2789. DOI: 10.3390/s24092789 (breve v diakritice jmen "Codău", "Păstrav" je artefakt extrakce; jména jsou správně)
- [20] Jun, C.; De, W, D.W. Weighted average algorithm: A novel meta-heuristic optimization algorithm based on the weighted average position concept. Knowl.-Based Syst. 2024, 305, 112564. DOI: 10.1016/j.knosys.2024.112564
- [21] Schmidt, R. Multiple emitter location and signal parameter estimation. IEEE Trans. Antennas Propag. 1986, 34, 276-280. DOI: 10.1109/TAP.1986.1143830
- [18] Oliveira, M.T.; Miranda, R.K.; Costa, J.P.C.L.; Almeida, A.L.F.; Sousa, R.T., Jr. Low Cost Antenna Array Based Drone Tracking Device for Outdoor Environments. Wirel. Commun. Mob. Comput. 2019, 1, 5437908. DOI: 10.1155/2019/5437908
- [13] Yan, X.; Fu, T.; Lin, H.; Xuan, F.; Huang, Y.; Cao, Y.; Hu, H.; Liu, P. UAV Detection and Tracking in Urban Environments Using Passive Sensors: A Survey. Appl. Sci. 2023, 13, 11320. DOI: 10.3390/app132011320
- [15] Samith, A.; Lahiru, J.; Hua, F.; Subashini, N.; Chau, Y. RF-based Direction Finding of UAVs Using DNN. In Proceedings of the 2018 IEEE International Conference on Communication Systems (ICCS), Chengdu, China, 19-21 December 2018; pp. 157-161. DOI: 10.1109/ICCS.2018.8689177
- [24] Kennedy, J.; Eberhart, R. Particle swarm optimization. ICNN'95, Perth, 1995, 4, 1942-1948. DOI: 10.1109/ICNN.1995.488968 (pouze jako zdroj PSO srovnávaného s WAA)
- Ostatní reference (zemědělské drony, optimalizační algoritmy WOA/SSA/PSO/GWO [22]-[25], HackRF atd.) pro beamforming/DOA relevantní nejsou.

## Kontrola (kolo 2)
- Opraveno: (1) s. 10-11: tvrzení první verze, že "half-wavelength spacing" je záměna poloměru, bylo chybné; pro M = 6 je rozteč sousedních prvků rovna R = λ/2, autoři mají pravdu. (2) Relevance: 32 400 bodů je 1.0 % z 3.24 M, ne 1.2 %. (3) Konvence úhlů (Doubt 1): vlastním přepočtem z Tab. 4 jsem zjistil, že "elevace" v Tab. 5 je polární úhel od svislice (90 stupňů minus úhel nad horizontem), viz sekci Konvence úhlů. (4) Headline 7.0/7.7 stupňů platí jen po vyřazení bodu 11; doplněno upozornění k s. 1 a 3. (5) Dvoupásmový provoz 2.4/5.8 GHz je jen možnost výměnou antén (s. 3), postavená sada je 2.4 GHz. (6) Výškový rozsah: s. 15 uvádí 30-90 m, Tab. 4 min. 29.7 m; 20 m na s. 1, 3, 14 nemá oporu v tabulce. (7) Tab. 2 [s. 13]: "10,240 MHz" je překlep, jde o 10 240 snapshotů. (8) Iterace: 40 (Sec. 2.3, s. 8-9, 1200 vyhodnocení) vs. 30 (Sec. 4.1 a Tab. 2, s. 12-13); potvrzeno jako nesoulad zdroje. (9) Ref. [19]: autoři nejsou poškozeni, jde o Codău, Buta, Păstrav, Dolea, Palade, Puschita (breve ve výstupu extrakce); přepis v sekci referencí je správný. Citace v "Citovatelná tvrzení", Tab. 1 a Tab. 5 (všech 26 řádků, strojově) a reference [9], [13], [15]-[21] odpovídají PDF.
- Doplněno: kompletní Tab. 4 (27 řádků včetně souřadnic a absolutní výšky, s. 16) s významem sloupců (Dist. = horizontální vzdálenost); rovnice (21)-(23) WAA; poznámka k Eq. 8 (n vs. v); odhad doby signálu 5.12 ms pro 10 240 snapshotů při 2 MHz (interpretace); parametry DJI Air 2S (dosah 12 km, 30 min); počet snapshotů v simulaci neuveden; kontrola průměru z tabulky (6.99/7.76); DOI všech relevantních referencí [9], [13], [15]-[21] (z cílů hyperlinků [CrossRef] v PDF) a ref. [24]; dvě zjevné nesrovnalosti ve zdroji Tab. 5 oproti Tab. 4 (bod 12 azimut, bod 23 elevace).
- Nejistoty: Sec. 4.4 (s. 19; ztráta citlivosti elevace "near the plane of the UCA") a Sec. 4.5 (s. 20; sin θ → 0 pro θ → 0, tj. zenit) si odporují; z PDF nelze rozhodnout, který popis je správný. Počet vyhodnocení WAA na iteraci (30 x 40 = 1200) je odvozený, autoři jej explicitně nerozepisují pro 30 iterací (900). Jednotka "Altitude" v Tab. 4 (absolutní/MSL) není v textu definována. Zda vyřazený bod 11 byl vybrán před analýzou nebo až podle chyby, nelze z PDF posoudit (text říká jen, že jde o multipath).
