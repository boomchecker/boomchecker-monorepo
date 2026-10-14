# Madokoro et al. 2021: křížové pole 32 mikrofonů, DAS beamforming pro lokalizaci dronu v GNSS-denied oblastech

Poznámka k číslování stran: PDF má 19 stran a čísla stran PDF se shodují s číslováním časopisu ("N of 19"); všechny odkazy [s. X] jsou tedy zároveň PDF i časopisecké strany.

## Bibliografie
- Madokoro, H.; Yamamoto, S.; Watanabe, K.; Nishiguchi, M.; Nix, S.; Woo, H.; Sato, K. "Prototype Development of Cross-Shaped Microphone Array System for Drone Localization Based on Delay-and-Sum Beamforming in GNSS-Denied Areas". *Drones* 2021, 5, 123. https://doi.org/10.3390/drones5040123 [s. 1]
- Received 19 August 2021, Accepted 19 October 2021, Published 23 October 2021; Academic Editors: Francesco Nex, Diego González-Aguilera [s. 1].
- Vydavatel: MDPI, Basel; MDPI ano; open access ano (licence CC BY 4.0) [s. 1].
- Číslo sešitu (issue) není v PDF přímo uvedeno; "4" plyne z DOI (drones5**04**0123) (interpretace).
- Návrh BibTeX:

```bibtex
@article{madokoro2021cross,
  author    = {Madokoro, Hirokazu and Yamamoto, Satoshi and Watanabe, Kanji and Nishiguchi, Masayuki and Nix, Stephanie and Woo, Hanwool and Sato, Kazuhito},
  title     = {Prototype Development of Cross-Shaped Microphone Array System for Drone Localization Based on Delay-and-Sum Beamforming in {GNSS}-Denied Areas},
  journal   = {Drones},
  year      = {2021},
  volume    = {5},
  number    = {4},
  pages     = {123},
  publisher = {MDPI},
  doi       = {10.3390/drones5040123}
}
```

## Problém a přínos
- Cíl: určit polohu dronu ve world coordinates pouze akusticky v oblastech bez GNSS (mezi výškovými budovami, pod mosty, v tunelech) [s. 1, 2].
- Přístup: vlastní křížové pole 32 všesměrových mikrofonů (16 horizontálně, 16 vertikálně), DAS beamforming v časové oblasti zvlášť pro horizontální a vertikální rameno (azimut a elevace), následně geometrický převod na (x, y) pomocí výšky dronu `h` přenášené z letového řadiče [s. 4, 5, 6].
- Přínos podle autorů: první (podle jejich znalostí) demonstrace a vyhodnocení lokalizace dronu pomocí DAS v GNSS-denied oblastech; vlastní venkovní benchmark dataset na 24 pozicích se dvěma drony [s. 4, 1].
- Hlavní výsledek: chyba polohy E 0,4 až 4,4 m pro P1-P20 ve vzdálenostech 20 až 50 m (výjimka P22.1 ve výšce 5 m: 7,5 m; P24 na cca 500 m: 13,0 m) [s. 14, Tab. 4]; přesnost 71,4 % při toleranci 2,0 m [s. 16, Tab. 6]. Autoři v úvodu uvádějí, že identifikace polohy je "within the accuracy range of GPS" [s. 2]. Dataset je dostupný jen na vyžádání u korespondenčního autora [s. 17].
- Pozor (interpretace): metoda není DOA z pole pro 3D lokalizaci, vyžaduje znalost výšky dronu `h` z telemetrie; to je u nás (cizí dron) nesplnitelné.

## Mikrofonní pole a hardware
- Počet mikrofonů: 32 (16 horizontálně + 16 vertikálně), křížová konfigurace [s. 6].
- Geometrie: kříž, rozměry nosné konstrukce 1200 mm (délka, šířka, výška); horizontální rameno ve výšce 900 mm nad zemí; hliníkové čtvercové trubky 20 × 20 mm; dva hliníkové profily rovnoběžně, mikrofony upevněny vpředu a vzadu [s. 6].
- Rozteče: 16 mikrofonů v konstantních intervalech na šířce 600 mm horizontálně; 16 mikrofonů vertikálně v rozsahu výšek 600 mm až 1200 mm; "We installed microphones on the mount at 40 mm intervals horizontally and vertically." [s. 6, 7]. Několik mikrofonů má mírně nerovnoměrné rozestupy (popisek Obr. 4: "Several microphones are installed with slightly unequal intervals. Omnidirectional microphones allow this installation.") [s. 7]. Konzistence: 16 mikrofonů × 40 mm = 15 intervalů = 600 mm (vlastní kontrola, interpretace); přesné pozice mikrofonů nejsou uvedeny.
- Výška středu pole `h_m` = 900 mm [s. 5].
- Typ mikrofonů: Behringer ECM8000 (omnidirectional, Music Tribe), vstupní frekvenční rozsah 15 až 20 000 Hz, průměr hrotu ("tip diameter") 12 mm [s. 7]. Označení "measurement condenser" v PDF není (doplnil první autor z vlastní znalosti; interpretace). Nejde o MEMS.
- Akvizice: 4 × 8kanálový audio interface Behringer ADA8200 (předzesilovač + A/D), vzorkovací frekvence "44.1/48 kHz with synchronization across the channels"; signály z interfaců sloučeny hubem a ukládány laptopem [s. 6]. Skutečně použitá fs: neuvedeno. Bit depth: neuvedeno.
- Napájení: 15 Wh na interface (uvedeno jako "Wh", pravděpodobně míněno W; interpretace), laptop 9 Wh, celkem cca 69 Wh; baterie Anker PowerHouse 200, 213 Wh, provoz cca 3 h [s. 6].
- Synchronizace: pouze deklarována synchronizace kanálů uvnitř jednoho ADA8200; synchronizace mezi čtyřmi interfacy neuvedena.
- Kalibrace mikrofonů (zisk/fáze) nebo pozic: neuvedena.
- Rozteč 40 mm odpovídá prostorovému Nyquistu c/(2d) = 4,29 kHz při c = 343 m/s (vlastní výpočet, interpretace; článek aliasing nediskutuje).
- Cíle: DJI Matrice 200 a Matrice 600 Pro (viz níže) [s. 7].
- Zpracování po měření: laptop ukládá digitální signály (hub); zda a jak se zpracovává v reálném čase, není uvedeno; "real-time drone tracking" je až future work [s. 17].

## Signálový model a rovnice
Geometrie převodu úhlů na polohu (O = střed pole, `h` výška dronu, `h_m` výška středu pole nad zemí) [s. 4, 5]:

$$h_d = h - h_m \quad (1)\ [s.\ 4]$$

$$r = \frac{h_d}{\tan\theta_e} \quad (2)\ [s.\ 4]$$

$$x = r\cos\theta_h \quad (3)\ [s.\ 5]$$

$$y = x\tan\theta_h \quad (4)\ [s.\ 5]$$

Poznámka (interpretace): rovnice (3), (4) tak, jak jsou vytištěny, neodpovídají hodnotám v Tab. 4. Např. P1: `h_d` = 50 − 0,9 = 49,1 m, `θ_e` = 44°, `r` = 49,1/tan 44° = 50,85 m; vytištěné (3), (4) by pro `θ_h` = −86° dávaly malé x, kdežto Tab. 4 uvádí x_m = −50,7 m, y_m = 3,5 m. Tab. 4 odpovídá `x = r sin θ_h`, `y = r cos θ_h` (vlastní přepočet: 50,85·sin(−86°) = −50,73; 50,85·cos(−86°) = 3,55). Úhel `θ_h` je tedy měřen od osy y (kolmice k horizontálnímu ramenu), ne od osy x. Před převzetím vzorců ověřit.

DAS v časové oblasti [s. 5]:

$$y(t) = \sum_{m=1}^{M} w_m(t) \otimes z_m(t) \quad (5)\ [s.\ 5]$$

$$z_m(t) = s_m(t-\tau_m) \quad (6)\ [s.\ 5]$$

$$w_m(t) = \frac{1}{M}\,\delta(t+\tau_m) \quad (7)\ [s.\ 5]$$

Relativní střední výkon a odhad úhlu [s. 5, 6]:

$$G(\theta) = \frac{1}{T}\sum_{t=0}^{T} y^2(t) \quad (8)\ [s.\ 5]$$

$$\theta_{\{h,e\}} = \arg\max_{-90^\circ\le\theta\le 90^\circ} P_{\{h,e\}}(\theta) \quad (9)\ [s.\ 6]$$

kde `P_h(θ)` a `P_e(θ)` jsou `G(θ)` z horizontálního a vertikálního pole. Předpoklad: jediná rovinná vlna, jediný zdroj [s. 5].

Chyba polohy [s. 13]:

$$E = \sqrt{x_e^2 + y_e^2} \quad (10)$$

$$\begin{pmatrix}x_e\\ y_e\end{pmatrix} = \begin{pmatrix}x_m - x_g\\ y_m - y_g\end{pmatrix} \quad (11)$$

## Algoritmus
1. Z 10 s záznamu při visení dronu (hovering) se náhodně vybere 1 s dat `s(t)` jako vstup do DAS [s. 10].
2. Pro horizontální i vertikální rameno zvlášť: zpoždění `τ_m` pro rovinnou vlnu z úhlu `θ`, DAS v časové oblasti (5)-(7), výkon G(θ) (8). Detaily interpolace zlomkových zpoždění: neuvedeno.
3. Grid úhlů: θ od −90° do 90° po 1° [s. 6].
4. Úhly θ_h a θ_e jako argmax (9). Počet zdrojů: 1. Váhování/apodizace: jednotné 1/M. Frekvenční pásmo, filtrace, délka okna jiná než 1 s, překryv, regularizace: neuvedeno (žádné pásmo/pre-filtr se nezmiňuje).
5. Převod na (x, y) pomocí (1)-(4) s výškou `h` dronu z řadiče letu (letové parametry z IMU, barometru a teploty) [s. 5].
6. Vyhodnocení E podle (10), (11) vůči ground truth [s. 13].

Výpočetní nároky, implementace (jazyk, platforma), čas výpočtu: neuvedeno.

## Experiment / simulace
- Místo: dráha (track) na kampusu Honjo, Akita Prefectural University, Yurihonjo, Japonsko (39°39′35″ N, 140°7′33″ E); okolí rýžová pole, dálnice na východě a silnice na jihu; experiment ve dne, málo provozu, ale zvuk projíždějících aut je v datech [s. 8].
- Drony [s. 7, 8, Tab. 1]:

| Parametr | Matrice 200 | Matrice 600 Pro |
|---|---|---|
| Diagonal wheelbase | 643 mm | 1133 mm |
| Rozměry (L × W × H) | 887 × 880 × 378 mm | 1668 × 1518 × 727 mm |
| Počet rotorů | 4 | 6 |
| Hmotnost (se standardními bateriemi) | 6,2 kg | 9,5 kg |
| Payload | 2,3 kg | 6,0 kg |
| Max. rychlost stoupání | 5 m/s (jedna hodnota pro oba) | 5 m/s (jedna hodnota pro oba) |
| Max. rychlost klesání | 3 m/s (jedna hodnota pro oba) | 3 m/s (jedna hodnota pro oba) |
| Max. odolnost proti větru | 12 m/s | 8 m/s |
| Max. letová výška | 3000 m | 2500 m |
| Provozní teplota | −20 až 45 °C | −10 až 40 °C |
| GNSS | GPS + GLONASS | GPS + GLONASS |

  (V PDF jsou řádky max. rychlost stoupání, klesání a GNSS uvedeny jednou přes oba sloupce, zde proto zopakovány.)
- Úroveň akustického tlaku dronů: 50 až 80 dB podle zátěže a letových podmínek [s. 7]. BPF/rpm: neuvedeno.
- Matrice 200 na P1 až P20; Matrice 600 Pro na P21 až P24; P1-P20, P21-P22, P23-P24 měřeny v různých dnech [s. 9].
- Ground truth: 2D poloha na zemi změřena pásmem (Tape measure USR-100, minimální rozsah 2 mm), dron vyzvednut do libovolné výšky svisle; 3D poloha potvrzena vizuálně a z FOV obrazu z palubní kamery [s. 9]. Výška `h` z telemetrie (barometr + IMU). Přesnost výšky nebyla ověřena (viz Omezení).
- Počet pozic: 24 (P1-P20; P21 a P22 každá ve čtyřech výškách 5, 50, 100, 150 m; P23, P24); celkem 28 výsledků pro P1-P22 plus P23, P24 [s. 9, 10, 15].
- Meteorologie [s. 10, Tab. 3]:

| Parametr | P1-P20 | P21-P22 | P23-P24 |
|---|---|---|---|
| Datum | 17. 7. 2020 | 27. 8. 2020 | 16. 10. 2020 |
| Čas (JST) | 14:00-15:00 | 14:00-15:00 | 14:00-15:00 |
| Počasí | slunečno | slunečno | oblačno |
| Tlak [hPa] | 1006,8 | 1007,7 | 1019,4 |
| Teplota [°C] | 28,0 | 33,1 | 14,8 |
| Vlhkost [%] | 60 | 50 | 48 |
| Rychlost větru [m/s] | 1,8 | 5,3 | 1,1 |
| Směr větru | W | W | ENE |

- Pole umístěno v (0, 0); pozice P1-P20 v x ±20 až ±50 m po 10 m, y 20 až 50 m, úhly 0° až 180° po 45°, výšky 20 až 70 m po 10 m (text) [s. 9]. Pro P23 a P24 bylo pole přemístěno a dron zůstal; vzdálenost 70 m a 355 m v obou osách (cca 100 m a 500 m v přímce), výšky 100 m a 150 m; u P23 a P24 byl zvuk dronu slyšet subjektivně slabě ("At all locations except for P23 and P24, we were able to hear the drone propeller rotation sound.") [s. 10, 13].
- Jedno měření (single trial) na pozici; P23 a P24 uvedeny jako referenční hodnoty, "conducted only in a single trial" [s. 16].
- SNR, BPF, vliv hluku: nekvantifikováno. Autoři uvádějí "further investigation of the magnitude and effects of noise" jako future work [s. 17].

### Ground truth (Tab. 2) [s. 9]
Group = skupina podle vzdálenosti; `h` = výška dronu; θ_gh, θ_gv = azimut a elevace GT.

| Pozice | Group | x_g [m] | y_g [m] | h [m] | θ_gh [°] | θ_gv [°] |
|---|---|---|---|---|---|---|
| P1 | 4 | −50 | 0 | 50 | −90 | 45 |
| P2 | 3 | −40 | 0 | 40 | −90 | 45 |
| P3 | 2 | −30 | 0 | 30 | −90 | 45 |
| P4 | 1 | −20 | 0 | 20 | −90 | 45 |
| P5 | 1 | 20 | 0 | 20 | 90 | 45 |
| P6 | 2 | 30 | 0 | 30 | 90 | 45 |
| P7 | 3 | 40 | 0 | 40 | 90 | 45 |
| P8 | 4 | 50 | 0 | 50 | 90 | 45 |
| P9 | 1 | −20 | 20 | 28 | −45 | 45 |
| P10 | 1 | 0 | 20 | 20 | 0 | 45 |
| P11 | 1 | 20 | 20 | 28 | 45 | 45 |
| P12 | 2 | −30 | 30 | 42 | −45 | 45 |
| P13 | 2 | 0 | 30 | 30 | 0 | 45 |
| P14 | 2 | 30 | 30 | 42 | 45 | 45 |
| P15 | 3 | −40 | 40 | 57 | −45 | 45 |
| P16 | 3 | 0 | 40 | 40 | 0 | 45 |
| P17 | 3 | 40 | 40 | 57 | 45 | 45 |
| P18 | 4 | −50 | 50 | 71 | −45 | 45 |
| P19 | 4 | 0 | 50 | 50 | 0 | 45 |
| P20 | 4 | 50 | 50 | 71 | 45 | 45 |
| P21.1 | - | 20 | 20 | 5 | 45 | 8 |
| P21.2 | - | 20 | 20 | 50 | 45 | 60 |
| P21.3 | - | 20 | 20 | 100 | 45 | 74 |
| P21.4 | - | 20 | 20 | 150 | 45 | 79 |
| P22.1 | - | 50 | 50 | 5 | 45 | 3 |
| P22.2 | - | 50 | 50 | 50 | 45 | 35 |
| P22.3 | - | 50 | 50 | 100 | 45 | 54 |
| P22.4 | - | 50 | 50 | 150 | 45 | 65 |
| P23 | - | 70 | 70 | 100 | 45 | 45 |
| P24 | - | 355 | 355 | 150 | 45 | 17 |

(Sloupce přečteny z textové extrakce tabulky: pořadí Position, Group, xg, yg, h, θgh, θgv; kontrola: pro P1-P8 je `h` = |x_g| a θ_gv = 45°, pro P21.1 atan((5−0,9)/28,28) = 8,2° ≈ 8°, konzistentní.)

## Výsledky (čísla)
Úhly a poloha pro všechny pozice [s. 14, Tab. 4]. Sloupce přesně jak jsou vytištěny (θ_hm, θ_vm = odhadnutý azimut a elevace; x_m, y_m = odhadnutá poloha; x_e, y_e, E = chyby v m):

| Pozice | θ_hm [°] | θ_vm [°] | x_m [m] | y_m [m] | x_e [m] | y_e [m] | E [m] |
|---|---|---|---|---|---|---|---|
| P1 | −86 | 44 | −50.7 | 3.5 | 3.5 | 0.7 | 3.6 |
| P2 | −84 | 44 | −40.3 | 4.2 | 4.2 | 0.3 | 4.2 |
| P3 | −84 | 44 | −30.0 | 3.1 | 3.1 | 0.0 | 3.1 |
| P4 | −87 | 45 | −19.1 | 1.0 | 1.0 | −0.9 | 1.3 |
| P5 | 85 | 45 | 19.0 | 1.7 | 1.7 | 1.0 | 2.0 |
| P6 | 85 | 45 | 29.0 | 2.5 | 2.5 | 1.0 | 2.7 |
| P7 | 85 | 45 | 39.0 | 3.4 | 3.4 | 1.0 | 3.5 |
| P8 | 85 | 45 | 48.9 | 4.3 | 4.3 | 1.1 | 4.4 |
| P9 | −45 | 45 | −19.2 | 19.2 | −0.8 | −0.8 | 1.1 |
| P10 | 0 | 45 | 0.0 | 19.1 | −0.9 | 0.0 | 0.9 |
| P11 | 45 | 45 | 19.2 | 19.2 | −0.8 | 0.8 | 1.1 |
| P12 | −45 | 45 | −29.1 | 29.1 | −0.9 | −0.9 | 1.3 |
| P13 | 0 | 45 | 0.0 | 29.1 | −0.9 | 0.0 | 0.9 |
| P14 | 45 | 45 | 29.1 | 29.1 | 0.9 | 0.9 | 1.3 |
| P15 | −45 | 45 | −39.7 | 39.7 | −0.3 | −0.3 | 0.4 |
| P16 | 0 | 45 | 0.0 | 39.1 | −0.9 | 0.0 | 0.9 |
| P17 | 45 | 45 | 39.7 | 39.7 | −0.3 | 0.3 | 0.4 |
| P18 | −45 | 45 | −49.6 | 49.6 | −0.4 | −0.4 | 0.6 |
| P19 | 0 | 45 | 0.0 | 49.1 | −0.9 | 0.0 | 0.9 |
| P20 | 44 | 45 | 48.7 | 50.4 | 0.4 | 1.3 | 1.4 |
| P21.1 | 45 | 8 | 20.6 | 20.6 | 0.6 | −0.6 | 0.8 |
| P21.2 | 45 | 60 | 20.0 | 20.0 | 0.0 | 0.0 | 0.0 |
| P21.3 | 44 | 73 | 21.0 | 21.8 | 1.8 | −1.0 | 2.1 |
| P21.4 | 45 | 79 | 20.5 | 20.5 | 0.5 | −0.5 | 0.7 |
| P22.1 | 45 | 3 | 55.3 | 55.3 | 5.3 | −5.3 | 7.5 |
| P22.2 | 45 | 35 | 49.6 | 49.6 | −0.4 | 0.4 | 0.6 |
| P22.3 | 45 | 54 | 50.9 | 50.9 | 0.9 | −0.9 | 1.3 |
| P22.4 | 45 | 65 | 49.2 | 49.2 | −0.8 | 0.8 | 1.1 |
| P23 | 45 | 45 | 70.1 | 70.1 | −0.6 | 0.6 | 0.8 |
| P24 | 45 | 17 | 344.8 | 344.8 | −9.2 | 9.2 | 13.0 |

Tabulka je přepsána správně podle PDF (zkontrolováno proti vizuálnímu vzhledu s. 14). Poznámka (vlastní přepočet, interpretace): sloupce x_e a y_e nejsou konzistentní s (11) a se sloupci x_m, y_m. U P1-P8 a P10, P13, P16, P19, P21.3 je ve sloupci x_e hodnota y_m − y_g a ve sloupci y_e hodnota x_m − x_g (sloupce jsou tedy zaměněné), znaménka nejsou jednotná (např. P9: x_m − x_g = +0,8, vytištěno −0,8; P20: x_m − x_g = −1,3, vytištěno +1,3). Hodnota E odpovídá √((x_m−x_g)² + (y_m−y_g)²) u P1-P22 (např. P1: 3,57; P22.1: 7,50), ale NE u P23 a P24: z x_m, y_m a x_g, y_g vychází pro P23 E = 0,14 m (vytištěno 0,8) a pro P24 E = 14,4 m (vytištěno 13,0; vytištěné x_e = −9,2 vs. x_m − x_g = −10,2). Příčinu nelze z textu určit. Pro citaci používat E (P1-P22), x_m, y_m; hodnoty P23/P24 označit jako nepřesně reprodukovatelné.

Souhrn podle skupin vzdálenosti [s. 16, Tab. 5] (součet E přes 5 pozic ve skupině):

| E | Group 1 | Group 2 | Group 3 | Group 4 |
|---|---|---|---|---|
| Součet [m] | 6.48 | 9.24 | 9.50 | 10.83 |
| Průměr [m] | 1.30 | 1.85 | 1.90 | 2.17 |

Skupiny: Group 1 = P4, P5, P9, P10, P11 (±20 m v x, 20 m v y); Skupiny 2 až 4 po 10 m [s. 10]. Průměr E "shows a slight trend for the error to increase slightly according to the growing distance" [s. 16].

Přesnost při toleranci pro 28 výsledků P1-P22 [s. 16, Tab. 6]:

| Tolerance | 3.0 m | 2.5 m | 2.0 m | 1.5 m | 1.0 m |
|---|---|---|---|---|---|
| Přesnost [%] | 78.6 | 75.0 | 71.4 | 67.9 | 39.3 |

(Vlastní kontrola z Tab. 4: počet E ≤ 2,0 m je 20 z 28 = 71,4 %, shoduje se.)

Další kvantitativní tvrzení:
- Chyba na ±90° je větší než na 0° a ±45°: P1-P8 (±90°) E = 1,3 až 4,4 m, kdežto P9-P19 (0°, ±45°) E = 0,4 až 1,3 m [s. 14, Tab. 4]. Autoři to vysvětlují: "The cumulative error increased because of the higher delay ratio, defined theoretically as τm." [s. 14]. (Interpretace: ±90° je endfire horizontálního ramene, kde je citlivost úhlu na zpoždění nejhorší a svazek nejširší.)
- Rozptyl chyb na ose y (pravý panel Obr. 20) je větší než na ose x [s. 14, Obr. 20].
- Výška dronu (P21, P22): se zvyšující se výškou klesá střední výkon, kromě 5 m ("mean power level at the 5 m altitude was low because of the effects sound waves reflected from the ground") [s. 11]; velké chyby u P21 ve výšce 100 m (E = 2,1 m) a P22 ve výšce 5 m (E = 7,5 m); výšky 50 a 150 m stabilní [s. 14, Tab. 4].
- P23: E = 0,8 m (menší než na P1-P22); P24 (cca 500 m): E = 13,0 m, "it corresponds to 3.4%" [s. 15, 16]. Základ procenta není v textu uveden. Vlastní přepočet (interpretace): 13,0 / 487,6 m (horizontální vzdálenost 344,8·√2) = 2,7 %; 13,0 / 502 m (GT 355·√2) = 2,6 %; 13,0 / 544 m (3D vzdálenost) = 2,4 %; 13,0 / 355 = 3,7 %; hodnotu 3,4 % se nepodařilo reprodukovat žádným z těchto základů. Navíc samotné E = 13,0 se nepodařilo reprodukovat z x_m, y_m (viz poznámka u Tab. 4).
- Výstupní křivky G(θ) jsou unimodální s výrazným vrcholem, i pro P23 a P24 [s. 11, 13].
- Porovnání s GPS: typická chyba GPS cca 2 m [s. 16]; Modsching et al. [49] naměřili 2,52 m ve středně velkém městě [s. 16].
- Sedunov et al. [37] (cit. v článku): 3 uzly po 15 mikrofonech s rozestupy 100 ± 20 m, SRP-PHAT založený na součtu GCC-PHAT, pět dronů různých velikostí; detekce dronu do 350 m s průměrnou přesností 4°, sledování do cca 250 m [s. 3].
- Další související práce podle článku [s. 3, 4]: Chang et al. [40] (dvě pole po 4 mikrofonech s odstupem 14 m, TDOA s Gaussovým apriorním rozdělením, jediný experiment v oblasti 50 × 50 m); Dumitrescu et al. [41] (30 spirálově uspořádaných MEMS mikrofonů, CoNN klasifikace, 6 dronů); Blanchard et al. [42] (10 mikrofonů ve 3 ortogonálních osách, časově-frekvenční DAS + Kalman, oblast cca 10 × 10 m horizontálně a 5 m vertikálně); Zunino et al. [43] (128 MEMS mikrofonů + kamera); Izquierdo et al. [47] (64 MEMS mikrofonů + kamera, 2D beamforming, malá místnost).
- Rozpor text vs. Tab. 6: text uvádí tolerance "from 3.0 m to 0.5 m step by 0.5 m" (i v závěru), ale Tab. 6 obsahuje jen sloupce 3,0 až 1,0 m; hodnota pro 0,5 m v PDF chybí [s. 16].

## Omezení podle autorů
- Dron musí přesně vysílat výšku letu systému; "Another limitation is that we do not currently consider applications in environments where sound reflection occurs, such as in tunnels or under bridges." [s. 4].
- P23 a P24 jsou jednotlivá měření (single trial), brána jako referenční hodnoty [s. 16].
- Future work: zlepšit úhlové rozlišení elevace, srovnání s jinými metodami lokalizace zdroje, vliv a velikost hluku, současná lokalizace více dronů, sledování v reálném čase, rozšíření datasetu, ověření chyby výšky totální stanicí (total station theodolite) [s. 17].
- Další omezení (interpretace): jediná metoda (DAS), žádné srovnání; žádná frekvenční filtrace ani pásmo; bez SNR; 1 s okno; drony jen ve visení (hovering), ne v pohybu.

## Relevance pro náš projekt
- SOTA sekce: příklad akustické lokalizace dronu s velkým lineárním (křížovým) polem a DAS; užitečné jako reference pro DAS jako základní metodu a jako příklad praktické chyby v řádu metrů do 50 m; dále přehled souvisejících prací (Sedunov SRP-PHAT, Chang TDOA, Blanchard DAS + Kalman, Dumitrescu MEMS spirála, Izquierdo 64 MEMS) [s. 2-4].
- Volba pásma: nepřispívá (žádné pásmo ani filtr nejsou popsány; autoři jen uvádějí 15 až 20 000 Hz mikrofonu).
- Geometrie 2×8 UCA: nepřenosné; jde o dvě lineární ramena 16 × 40 mm (600 mm apertura) s oddělenou azimutální/elevační estimací, kdežto my máme dva kruhy po 8 mikrofonech. Odhad úhlů ve dvou rovinách nezávisle (každé rameno vlastní 1D DAS) nahrazuje plné 2D prohledávání; pro nás irelevantní.
- Metoda: potvrzuje, že DAS funguje, ale chyba roste na endfire (±90°); pro nás argument, proč u UCA očekávat horší výsledky u okrajů a proč zkoumat MVDR/MUSIC (interpretace).
- Výpočetní nároky na MCU: neuvedeny; DAS v časové oblasti s 1 s oknem (cca 44 až 48 ksample při deklarovaných 44,1/48 kHz) je mimo rozpočet 100 ms. Nelze přenést.
- Nepřenositelné: poloha ve world coordinates závisí na telemetrické výšce `h` dronu; to u detekce cizích dronů není k dispozici (u nás jde o odhad úhlů, ne polohy). Dále: 32 laboratorních mikrofonů ECM8000 + 4 interface + laptop vs. 16 MEMS na STM32H563; vzdálenosti 20 až 500 m vs. náš scénář; drony DJI Matrice 6 až 9,5 kg (větší než typické cíle); pouze 1 s statické záznamy.
- Použitelné: metodika vyhodnocení přesnosti jako podíl odhadů v toleranci (Tab. 6), srovnání s přesností GPS, skupinování podle vzdálenosti, ground truth pomocí tape measure + telemetrie.

## Citovatelná tvrzení
- Akustika funguje při omezené viditelnosti: "acoustic information performs robustly in environments with limited visibility and occlusion between objects." [s. 2]
- Nedostatek akustických benchmarků: Taha et al. "indicated a shortage of acoustic benchmark datasets because of difficulties related to annotation compared with other modalities" [s. 2].
- Sedunov et al.: "their system detected a drone at up to 350 m with average precision of 4◦." [s. 3]
- Novost práce: "To the best of our knowledge, this is the first study to demonstrate and evaluate drone localization based on DAS beamforming used in GNSS-denied areas." [s. 4]
- Omezení: "the target drone must accurately transmit its flight altitude information to our proposed system." [s. 4]
- Metodika: "we recorded 10 s sound data while the drone was hovering. We extracted 1 s sound data randomly at time t for input to DAS beamforming as s(t)." [s. 10]
- Úhlový rozsah: "We changed θ from −90◦ to 90◦ with 1◦ intervals." [s. 6]
- Zdroje zvuku dronu 50 až 80 dB: "The sound pressure level range from the respective drones was 50–80 dB depending on the payload and flight conditions." [s. 7]
- Odraz od země při nízké výšce: "We consider that the mean power level at the 5 m altitude was low because of the effects sound waves reflected from the ground." [s. 11]
- Úhlová závislost chyby: "the error values at ±90◦ tend to be larger than those at 0◦ or ±45◦." [s. 14]
- Srovnání s GPS: "Generally, GPS localization error values are approximated at 2 m" [s. 16].
- Přesnost: "The localization accuracy of our proposed method is 71.4% with 2.0 m tolerance." [s. 16]
- Vzdálený případ: "The localization error of P24 was 13.0, which was higher than those of P1–P23, it corresponds to 3.4%." [s. 15-16]

## Relevantní reference z článku
(Přesně podle seznamu literatury [s. 18-19]; DOI nejsou v PDF vypsány, jen odkaz [CrossRef] u některých; DOI: neuvedeno.)
- [14] Chen, C.; Tian, Y.; Lin, L.; Chen, S.; Li, H.; Wang, Y.; Su, K. Obtaining World Coordinate Information of UAV in GNSS Denied Environments. Sensors 2020, 20, 2241.
- [15] Taha, B.; Shoufan, A. Machine Learning-Based Drone Detection and Classification: State-of-the-Art in Research. IEEE Access 2019, 7, 138669–138682. [CrossRef]
- [16] Lykou, G.; Moustakas, D.; Gritzalis, D. Defending Airports from UAS: A Survey on Cyber-Attacks and Counter-Drone Sensing Technologies. Sensors 2020, 20, 3537. [CrossRef] [PubMed]
- [17] Park, S.; Kim, H.T.; Lee, S.; Joo, H.; Kim, H. Survey on Anti-Drone Systems: Components, Designs, and Challenges. IEEE Access 2021, 9, 42635–42659. [CrossRef]
- [30] Nijim, M.; Mantrawadi, N. Drone Classification and Identification System by Phenome Analysis Using Data Mining Techniques. In Proceedings of the IEEE Symposium on Technologies for Homeland Security Waltham, MA, USA, 10–11 May 2016; pp. 1–5.
- [31] Jeon, S.; Shin, J.W.; Lee, Y.J.; Kim, W.H.; Kwon, Y.; Yang, H.Y. Empirical Study of Drone Sound Detection in Real-Life Environment with Deep Neural Networks. In Proceedings of the 25th European Signal Processing Conference, Kos, Greece, 28 August–2 September 2017; pp. 1858–1862.
- [32] Bernardini, A.; Mangiatordi, F.; Pallotti, E.; Capodiferro, L. Drone detection by acoustic signature identification. Electron. Imaging 2017, 10, 60–64. [CrossRef]
- [33] Kim, J.; Park, C.; Ahn, J.; Ko, Y.; Park, J.; Gallagher, J.C. Real-Time UAV Sound Detection and Analysis System. In Proceedings of the IEEE Sensors Applications Symposium, Glassboro, NJ, USA, 13–15 March 2017; pp. 1–5.
- [34] Yue, X.; Liu, Y.; Wang, J.; Song, H.; Cao, H. Software Defined Radio and Wireless Acoustic Networking for Amateur Drone Surveillance. IEEE Commun. Mag. 2018, 56, 90–97. [CrossRef]
- [35] Seo, Y.; Jang, B.; Im, S. Drone Detection Using Convolutional Neural Networks with Acoustic STFT Features. In Proceedings of the 15th IEEE International Conference on Advanced Video Signal Based Surveillance, Auckland, New Zealand, 27–30 November 2018; pp. 1–6.
- [36] Matson, E.; Yang, B.; Smith, A.; Dietz, E.; Gallagher, J. UAV Detection System with Multiple Acoustic Nodes Using Machine Learning Models. In Proceedings of the third IEEE International Conference on Robotic Computing, Naples, Italy, 25–27 February 2019; pp. 493–498.
- [37] Sedunov, A.; Haddad, D.; Salloum, H.; Sutin, A.; Sedunov, N.; Yakubovskiy, A. Stevens Drone Detection Acoustic System and Experiments in Acoustics UAV Tracking. In Proceedings of the IEEE International Symposium on Technologies for Homeland Security, Woburn, MA, USA, 5–6 November 2019; pp. 1–7.
- [38] Cobos, M.; Marti, A.; Lopez, J.J. A Modified SRP-PHAT Functional for Robust Real-Time Sound Source Localization With Scalable Spatial Sampling. IEEE Signal Process. Lett. 2011 18, 71–74. [CrossRef]
- [39] Knapp, C.; Carter, G. The Generalized Correlation Method for Estimation of Time Delay. IEEE Trans. Acoust. Speech Signal Process. 1976, 24, 320–327. [CrossRef]
- [40] Chang, X.; Yang, C.; Wu, J.; Shi, X.; Shi, Z. A Surveillance System for Drone Localization and Tracking Using Acoustic Arrays. In Proceedings of the IEEE 10th Sensor Array and Multichannel Signal Processing Workshop, Sheffield, UK, 8–11 July 2018; pp. 573–577.
- [41] Dumitrescu, C.; Minea, M.; Costea, I.M.; Cosmin Chiva, I.; Semenescu, A. Development of an Acoustic System for UAV Detection. Sensors 2020, 20, 4870. [CrossRef]
- [42] Blanchard, T.; Thomas, J.H.; Raoof, K. Acoustic Localization and Tracking of a Multi-Rotor Unmanned Aerial Vehicle Using an Array with Few Microphones. J. Acoust. Soc. Am. 2020, 148, 1456. [CrossRef] [PubMed]
- [43] Zunino, A.; Crocco, M.; Martelli, S.; Trucco, A.; Bue, A.D.; Murino, V. Seeing the Sound: A New Multimodal Imaging Device for Computer Vision. In Proceedings of the IEEE International Conference on Computer Vision, Santiago, Chile, 13–16 December 2015; pp. 6–14.
- [44] Liu, H.; Wei, Z.; Chen, Y.; Pan, J.; Lin, L.; Ren, Y. Drone Detection Based on an Audio-Assisted Camera Array. In Proceedings of the IEEE Third International Conference on Multimedia Big Data, Laguna Hills, CA, USA, 19–21 April 2017; pp. 402–406.
- [45] Svanstr´’om, F.; Englund, C.; Alonso-Fernandez, F. Real-Time Drone Detection and Tracking with Visible, Thermal and Acoustic Sensors. In Proceedings of the 25th International Conference on Pattern Recognition, Milan, Italy, 10–15 January 2021; pp. 7265–7272. (zápis jména tak, jak je v PDF; správně Svanström)
- [47] Izquierdo, A.; del Val, L.; Villacorta, J.J.; Zhen, W.; Scherer, S.; Fang, Z. Feasibility of Discriminating UAV Propellers Noise from Distress Signals to Locate People in Enclosed Environments Using MEMS Microphone Arrays. Sensors 2020, 20, 597. [CrossRef]
- [48] Van Veen, B.D.; Buckley, K.M. Beamforming: A Versatile Approach to Spatial Filtering. IEEE ASSP Mag. 1988, 5, 4–24. [CrossRef]
- [49] Modsching, M.; Kramer, R.; ten Hagen, K. Field Trial on GPS Accuracy in a Medium Size City: The Influence of Built-up. In Proceedings of the Third Workshop on Positioning, Navigation and Communication, Hannover, Germany, 16 March 2006.

## Kontrola (kolo 2)
- Opraveno: (1) citace Taha et al. "shortage of acoustic benchmark datasets" je na s. 2, ne s. 3; (2) citace "it corresponds to 3.4%" začíná větou na s. 15 a končí na s. 16 (uvedeno s. 15-16); (3) seznam literatury je na s. 18-19, ne 17-19; (4) "16 až 48 ksample" u 1 s okna nahrazeno 44 až 48 ksample (podle deklarovaných 44,1/48 kHz, s. 6); (5) "measurement condenser" u ECM8000 není v PDF, označeno jako doplněk první autorky/autora (s. 7); (6) "typicky 0,4 až 4,4 m" zpřesněno o odlehlé hodnoty P22.1 (7,5 m) a P24 (13,0 m) (s. 14); (7) poznámka k Tab. 4 přepsána podle přepočtu: sloupce x_e a y_e jsou zaměněné a mají nejednotná znaménka u P1-P22; E odpovídá x_m, y_m a x_g, y_g u P1-P22, ale NEODPOVÍDÁ u P23 (přepočet 0,14 m vs. vytištěno 0,8) a P24 (14,4 m vs. 13,0) (s. 14); (8) poznámka k "3.4 %" rozšířena o více možných základů (2,4 až 3,7 %), žádný nedává 3,4 % (s. 16). Tab. 1-6, rovnice (1)-(11), Tab. 4 a všechny ostatní přímé citace byly porovnány s PDF a odpovídají; Tab. 4 i Tab. 2 přepsány bez chyb.
- Doplněno: Sedunov (3 uzly × 15 mikrofonů, rozestupy 100 ± 20 m, SRP-PHAT, 5 dronů) a stručné parametry prací Chang, Dumitrescu, Blanchard, Zunino, Izquierdo (s. 3-4); tvrzení "within the accuracy range of GPS" (s. 2); dataset jen na vyžádání (s. 17); kontrola konzistence rozteče 16 × 40 mm = 600 mm (s. 6-7); rozpor text vs. Tab. 6 (text uvádí tolerance do 0,5 m, tabulka končí na 1,0 m; s. 16); reference [14] a [30]-[36] (akustická detekce dronů) přepsány přesně z PDF (s. 18-19); poznámka, že "100 m" u P23 je horizontální vzdálenost (3D vzdálenost cca 140 m, interpretace).
- Nejistoty: číslo sešitu (4) není v PDF, plyne jen z DOI a data publikace (23. 10. 2021); skutečně použitá fs (44,1 nebo 48 kHz), bit depth, kalibrace mikrofonů a synchronizace mezi čtyřmi ADA8200 nejsou uvedeny; příčina nesouladu Eqs. (3), (4) s Tab. 4 (odpovídá x = r sin θ_h, y = r cos θ_h, vlastní přepočet) není vysvětlena; text na s. 9 říká, že úhly se měří od kladné osy x (0° až 180° po 45°), kdežto Tab. 2 používá −90° až 90° a x_g = ±20 až ±50 m při θ_gh = ∓90°, tedy odlišnou konvenci (nevysvětleno); P23/P24 E a "3.4 %" nelze reprodukovat; věta "For θhm, the flight altitudes and estimated angles were found to have positive correlation" (s. 11) je pravděpodobně překlep za θvm (interpretace).
