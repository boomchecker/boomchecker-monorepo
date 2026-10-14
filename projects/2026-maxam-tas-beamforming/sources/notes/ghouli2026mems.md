# Ghouli 2026: pasivní akustická detekce a lokalizace dronů pomocí MEMS mikrofonů a strojového učení (TDOA + Random Forest)

UPOZORNĚNÍ O SPOLEHLIVOSTI ZDROJE: článek obsahuje řadu vnitřních nekonzistencí (viz sekce "Omezení"; zbytky šablony v textu, rozporné metriky mezi textem a tabulkami, nesedící časy a počty rámců, nesouvislé reference). Doporučuji citovat jen opatrně, jako příklad levného embedded systému, ne jako zdroj kvantitativní přesnosti lokalizace.

Poznámka k číslování stran: PDF má 16 stran a číslování PDF se shoduje s časopiseckým (strana 1 = úvodní strana "Acta Acustica 2026, 10, 12"); [s. X] platí pro obojí.

## Bibliografie
- Ghouli, Z. "Passive acoustic detection and localization of drones using MEMS microphones and machine learning". *Acta Acustica* 2026, 10, 12. https://doi.org/10.1051/aacus/2026008 [s. 1, 16]
- Received 4 September 2025, Accepted 23 January 2026 [s. 1]. Autor: Royal Naval School, Casablanca, Maroko; Polydisciplinary Faculty of Taroudant, University Ibn Zohr, Agadir, Maroko [s. 1]. Typ: "SCIENTIFIC ARTICLE" [s. 1].
- Vydavatel: EDP Sciences (The Author(s), Published by EDP Sciences, 2026); MDPI ne; open access ano (Creative Commons Attribution License 4.0) [s. 1]. Číslo sešitu (issue) v PDF není uvedeno (v citaci "Acta Acustica, 10, 12" je 12 číslo článku), proto BibTeX bez pole number [s. 16]. Dostupnost dat: "Data are available on request from the author." [s. 15]. Střet zájmů: žádný [s. 15].
- Návrh BibTeX:

```bibtex
@article{ghouli2026mems,
  author    = {Ghouli, Zakaria},
  title     = {Passive acoustic detection and localization of drones using {MEMS} microphones and machine learning},
  journal   = {Acta Acustica},
  year      = {2026},
  volume    = {10},
  pages     = {12},
  publisher = {EDP Sciences},
  doi       = {10.1051/aacus/2026008}
}
```

## Problém a přínos
- Cíl: levný, pasivní, real-time systém pro detekci dronu (klasifikace Random Forest nad MFCC) a lokalizaci (TDOA z cross-correlation + least-squares multilaterace) s poli MEMS mikrofonů, s důrazem na námořní a bezpečnostní aplikace [s. 1, 2].
- Abstrakt mluví o "distributed array of MEMS microphones" a o určení "angular position" [s. 1], kdežto tělo článku popisuje jedno čtvercové pole 8 mikrofonů a 2D polohu (x, y) i vzdálenost [s. 5, 9, 14]; Obr. 1 zobrazuje dvě "Microphone array" [s. 6] (interpretace: nekonzistentní popis architektury).
- Příspěvky autora: (1) multikanálové pole MEMS, (2) Python pipeline pro akvizici a extrakci příznaků, (3) Random Forest klasifikátor drone vs. šum, (4) 2D lokalizace TDOA triangulací, (5) radarové rozhraní [s. 2].
- Pozn.: "RF" ve vývojovém diagramu (Obr. 3) znamená Random Forest, ne radiofrekvenci (interpretace z kontextu [s. 8]).
- Metoda lokalizace není beamforming ani subprostorová DOA metoda; je to párová cross-correlation (TDOA) + LS (rovnice (11), (12)) [s. 9]. Článek tedy nepřináší srovnání DAS/MVDR/MUSIC.

## Mikrofonní pole a hardware
- Mikrofony: 8 × INMP441 (MEMS, digitální I2S, všesměrové) [s. 5]. INMP441: "flat frequency response between 60 Hz and 15 kHz, with a signal-to-noise ratio (SNR) around 62 dB and high sensitivity (−26 dBFS)", I2S usnadňuje synchronizaci [s. 5]. Datasheet citován jako "Analog Devices: INMP441 MEMS Microphone Datasheet, 2023" [s. 15, ref. 14].
- Geometrie: "Eight omnidirectional MEMS microphones are arranged in a square planar array" [s. 5]; čtvercová konfigurace s mezi-elementovou vzdáleností d = 15 cm; plná poloha 8 mikrofonů (např. po stranách čtverce) v textu neuvedena [s. 5]. Obr. 2 je podle popisku "Geometry of the MEMS microphone array ... annotated elements indicate microphone positions, inter-element spacing (d = 15 cm), and reference coordinate axes", ale ve skutečnosti je to fotografie černé desky s propojenými moduly na stativu bez anotací [s. 6] (ověřeno na obrazu stránky). Odhad dosažitelné frekvence: "f_max = c/(2d), where c = 343 m/s and d = 0.15 m", tj. cca 1,1 kHz [s. 5]. (Vlastní kontrola: 343/0,30 = 1143 Hz, souhlasí.) Pokrytí 180° v horizontální rovině [s. 5].
- Mikrokontrolér: ESP32-S3 (I2S, vícevláknové zpracování), řídí akvizici a komunikaci; centrální uzel Raspberry Pi 4 (zpracování, ML, lokalizace) [s. 5]. Na Obr. 1 je ale v řetězci "ESP32 -> PC -> Radar interface" (dva bloky "Microphone array" na vstupu), ne Raspberry Pi [s. 6, Obr. 1] (nekonzistence potvrzena v PDF).
- Vzorkování: 48 kHz, I2S, "interrupt-driven I2S acquisition on the ESP32-S3 ensures low jitter and consistent timing" [s. 6]. Bit depth: neuvedeno. Přenos na RPi sériově nebo Wi-Fi; rámce 1024 až 2048 vzorků, Hammingovo okno [s. 6].
- Stativ 1,5 m, pole směřuje k očekávané trajektorii [s. 10].
- Napájení: přenosná baterie, regulace napětí a EMI stínění [s. 5]; spotřeba neuvedena.
- Synchronizace: deklarovaná (I2S, interrupt-driven rutina); kvantitativní jitter a metoda ověření: neuvedeno. Kalibrace mikrofonů (zisk/fáze/poloha): neuvedena.
- Výpočetní výkon: RPi 4 Model B, 1,5 GHz quad-core Cortex-A72, 4 GB RAM; průměrné využití CPU 65 až 70 %, paměť pod 1 GB; latence na cyklus detekce "approximately 150 ms" [s. 7]. Tvrzení, že systém může běžet na RPi 3B+ nebo i "ESP32 microcontroller with external DSP support, albeit at a reduced frame rate" [s. 7], není experimentálně doloženo (interpretace).

## Signálový model a rovnice
Frekvence průchodu listů (BPF), `N_b` počet listů, `f_r` otáčky v ot/s [s. 4]:

$$f_{BPF} = N_b \times f_r \quad (1)\ [s.\ 4]$$

Útlum amplitudy s vzdáleností (tak je vytištěno; správně by 1/d pro amplitudu a 1/d² pro intenzitu, interpretace) [s. 4]:

$$A(d) = \frac{A_0}{d^2} \quad (2)\ [s.\ 4]$$

Prostorový aliasing, rozteč mikrofonů [s. 4]:

$$\Delta d < \frac{c}{2 f_{max}} \quad (3)\ [s.\ 4]$$

TDOA pro zdroj S(x, y) a mikrofony M_i(x_i, y_i) [s. 4]:

$$\tau_{ij} = \frac{\lVert S - M_i\rVert - \lVert S - M_j\rVert}{c} \quad (4)\ [s.\ 4]$$

Odhad zpoždění křížovou korelací (rovnice (5) na s. 5 se signály s_i, s_j, opakuje se jako (11) na s. 9 se signály x_i, x_j; ve vzorci chybí meze integrálu) [s. 5, 9]:

$$\tau_{ij} = \arg\max_\tau \int x_i(t)\,x_j(t+\tau)\,dt \quad (5),(11)$$

(Použita prostá cross-correlation, žádné GCC-PHAT váhování; v seznamu literatury cituje Knapp a Carter [42] jen jako odkaz.)

STFT [s. 5]:

$$X(n,\omega) = \sum_{m=-\infty}^{+\infty} x(m)\,w(n-m)\,e^{-j\omega m} \quad (6)\ [s.\ 5]$$

(V PDF je okno značeno ω(n − m), tedy stejným symbolem jako frekvence ω; zde přepsáno jako w; s. 5.)

LS multilaterace [s. 9]:

$$(x,y) = \arg\min_{(x,y)} \sum_{i<j}\big(\tau_{ij}^{measured} - \tau_{ij}^{predicted}(x,y)\big)^2 \quad (12)\ [s.\ 9]$$

(V PDF je u předpovězeného zpoždění překlep "preadicted"; s. 9.)

Metriky klasifikace [s. 8]:

$$\mathrm{Accuracy}=\frac{TP+TN}{TP+TN+FP+FN}\ (7),\quad \mathrm{Precision}=\frac{TP}{TP+FP}\ (8),\quad \mathrm{Recall}=\frac{TP}{TP+FN}\ (9),\quad F1=\frac{2\,\mathrm{Precision}\,\mathrm{Recall}}{\mathrm{Precision}+\mathrm{Recall}}\ (10)$$

## Algoritmus
1. Akvizice: fs = 48 kHz, I2S, rámce 1024 vzorků (cca 21 ms) s 50% překryvem, Hammingovo okno [s. 7]; (v kap. 4.3 "typically 1024-2048 samples" [s. 6]).
2. Předzpracování: pásmová propust "typically 200 Hz to 10 kHz" (odstranění větru a elektronického rušení); v hlučném prostředí spektrální subtrakce a adaptivní Wienerův filtr [s. 7].
3. Příznaky: STFT (jen magnituda), spektrální centroid, šířka pásma, zero-crossing rate, MFCC (12 až 13 koeficientů, Mel filtry, DCT) [s. 6, 7]; normalizace vektoru [s. 7].
4. Klasifikace: Random Forest (hyperparametry: počet stromů, maximální hloubka, grid search + 5-fold cross-validation; konkrétní hodnoty neuvedeny) [s. 8]. Alternativy SVM, MLP "comparable accuracy" (bez čísel), CNN odložena na budoucí práci [s. 6, 8].
5. Lokalizace (podle Obr. 3: pokud je dron detekován, poloha z TDOA; jinak se TDOA použije jako pomoc při detekci) [s. 8]: cross-correlation pro všechny unikátní dvojice mikrofonů (11), pak LS minimalizace (12) → 2D poloha vzhledem ke středu pole [s. 9]. Pozn.: konkrétní vzorce pro azimut a elevaci "using geometric relationships" se neuvádějí [s. 9].
6. Zobrazení: radarové GUI (Python, Tkinter nebo PyQt), aktualizace 5 až 10 Hz [s. 7, 9].
- Frekvenční pásma pro TDOA, délka okna pro korelaci, interpolace, gridy úhlů, počet zdrojů (1 implicitně), regularizace: neuvedeno.
- Obr. 5 a 6 [s. 10]: Obr. 5 ukazuje geometrii TDOA a AOA měření (zelené kruhy mikrofony pro TDOA, modré trojúhelníky AOA, r_b,min minimální základna); Obr. 6 je podle popisku "Simulated 2D UAV trajectory and estimated position from TDOA", tedy simulace, nikoli měření; osa X má rozsah cca −0,4 až 0,6 m a mikrofony jsou nakresleny cca ±0,08 m od počátku, což neodpovídá rozteči 15 cm ani vzdálenostem desítek metrů z experimentu (interpretace). Text říká, že se TDOA a AOA "integrují", ale způsob fúze není popsán [s. 9, 10].
- Latence rozepsaná na PC (Intel i7): preprocessing cca 60 ms, TDOA lokalizace cca 40 ms, Random Forest cca 50 ms; celkem cca 150 ms [s. 12]. Pozn.: součet 60 + 40 + 50 = 150 ms; měřeno na PC, nikoli na RPi, přestože kap. 4.5 uvádí RPi 4 s 150 ms [s. 7, 12] (nekonzistence).

## Experiment / simulace
- Prostředí: (a) vnitřní hala 25 × 15 m ("multipurpose hall with moderate reverberation and minimal ambient noise"); (b) venkovní otevřené pobřežní pole s proměnným větrem, vlny, ptáci, vzdálená auta [s. 10].
- Dron: "a DJI Phantom quadcopter" (přesný model neuveden), zdůvodnění "well-documented and relatively stable acoustic emission profile" [s. 10]. Počet dronů: 1 typ; v rozporu s tím se ve Sec. 2.4 mluví o "multiple drone types" jen v šablonovém textu [s. 3]. BPF dronu: neuvedeno.
- Trajektorie: azimuty 0° až 180°, výšky 5, 10, 15, 20 m; statické visení, horizontální průlety různými rychlostmi, opakované vstupy a výstupy ze zorného pole, intervaly bez dronu pro test falešných poplachů [s. 10, 11].
- Ground truth: externí GPS modul + synchronizované video; časové značky telemetrie, akustických dat a výstupu systému zarovnány [s. 10, 11]. Přesnost GPS neuvedena.
- Rozsah: 40 běhů (20 vnitřních + 20 venkovních) po cca 2 min; "approximately 3 h of labeled recordings collected from 40 test runs" (viz nekonzistence), cca 18 000 rámců na trénink a 4500 na test [s. 11]; rozdělení cca 80 % / 20 %, záznamy ze stejného experimentu nejsou v obou podmnožinách [s. 11].
- SNR, vítr (m/s), teplota, vzdálenosti jiné než do 50 až 60 m: neuvedeno kvantitativně.

## Výsledky (čísla)
Klasifikace [s. 8]:
- Dataset A (5-fold CV): precision 92,3 % ± 1,1 %, recall 86,7 % ± 1,5 %.
- Dataset B: precision 90,1 % ± 1,4 %, recall 85,2 % ± 1,8 %.
- Venkovní hluk: precision 88,5 %, recall 83,0 %. ("Dataset A" a "Dataset B" nejsou nikde definovány.)

Souhrn hlavních metrik v textu [s. 11]:
- Detection accuracy 91,6 % ("average across all runs"), "for distances up to 50 m in outdoor conditions and up to 30 m indoors"; "Beyond 60 m" mírně horší [s. 11].
- Precision 94,2 %; recall 88,7 %; latence "within 150 ms of sound detection" [s. 11].
- Localization angular error: mean absolute error 6,3° (std 2,4°) [s. 11].
- 2D positional error: median 1,2 m, 90 % výsledků do poloměru 2 m [s. 11].
- Metodika měření chyb (interpretace): autor neuvádí, jak se 6,3° a 1,2 m určily (počet pozic, vzdálenosti, referenční GPS s nejasnou přesností) [s. 11].
- Detekce úspěšná do 50 až 60 m venku; horší při větších vzdálenostech nebo při průletu přímo nad polem ("potential geometric singularity in elevation") [s. 12].

Tab. 2 "Summary of key performance metrics" [s. 13]:

| Metrika | Hodnota | Poznámka |
|---|---|---|
| Detection accuracy | 92-95 % | Across various noise conditions |
| Latency | ~150 ms | From sound capture to classification |
| Localization error | 0.3-0.5 m | Varies with drone flight path |
| False positive rate | 5-8 % | Mainly due to ambient interference |

Tab. 3 "System performance under varied environmental conditions" [s. 13]:

| Podmínky | Detection accuracy | Localization error | Poznámka |
|---|---|---|---|
| Coastal wind | 90-92 % | 0.4-0.5 m | Low-frequency harmonics partially masked by wind gusts |
| Indoor reverberation | 91-94 % | 0.3-0.4 m | Multipath reflections affect TDOA-based localization |
| Ambient interference | 90-93 % | 0.3-0.5 m | False positives mainly from passing vehicles and birds |

Vlivy prostředí [s. 12]:
- Silný pobřežní vítr občas maskoval nízké harmonické; snížení citlivosti detekce o cca 10 až 15 %.
- Dozvuk uvnitř: chyby lokalizace až ~0,5 m.
- Dron přímo nad polem: chyby lokalizace vyšší o cca 20 až 25 % oproti bočním trajektoriím.
- Falešné poplachy z aut a ptáků: false positive rate 5 až 8 %.

Efektivní dosah: cca 60 m [s. 14].

## Omezení podle autorů
- Rušení hlukem (vítr, širokopásmový šum) občas maskuje podpis dronu [s. 14].
- Úhlová nejednoznačnost, když je dron přímo nad polem; plánuje se optimalizace geometrie (vícerovinná/objemová konfigurace, mikrofony směřující nahoru) a 3D TDOA [s. 14, 15].
- Dosah omezen na cca 60 m (radar až kilometry); návrh: více synchronizovaných polí, citlivější mikrofony, beamforming a odšumování [s. 14].
- Budoucí práce: CNN či transformery, 3D lokalizace, fúze s RF/optikou, více dronů [s. 14, 15].

Nekonzistence a slabiny zjištěné při čtení (vlastní kontrola, interpretace):
- Zbytky šablony/odpovědí recenzentům v textu (potvrzeno v PDF): kromě Sec. 2.4 také odstavec "Although the outdoor setup may appear similar to an indoor environment due to the camera angle and background, ..." [s. 10], popisky Obr. 9 "Irrelevant details have been removed" [s. 12] a Obr. 10 "Irrelevant space and extraneous annotations from the original screenshot have been removed" [s. 13] a závorkový odstavec "(One limitation observed during testing was ...)" v závěru [s. 14].
- Zbytek šablony v textu: "While their study focused on [controlled environments / a single drone type / short-duration recordings], the present work extends these results by [evaluating models under real-world noisy conditions, including multiple drone types, or performing long-term acoustic monitoring]" [s. 3, kap. 2.4].
- Rozpor mezi metrikami: přesnost 91,6 % a lokalizační chyba 6,3° s mediánem 1,2 m [s. 11] vs. 92 až 95 % a 0,3 až 0,5 m v Tab. 2 a 3 [s. 13]; precision 94,2 % [s. 11] vs. 92,3 %, 90,1 %, 88,5 % [s. 8]; text uvádí "within a 2 m radius" pro 90 % vs. chyby 0,3 až 0,5 m v tabulkách.
- Údaj "approximately 3 h of labeled recordings collected from 40 test runs" [s. 11] neodpovídá 40 × cca 2 min (cca 80 min) [s. 11]; 22 500 rámců (18 000 + 4500) při 1024 vzorcích s 50% překryvem při 48 kHz odpovídá jen cca 4 min (vlastní výpočet: 22 500 × 512/48 000 s = 240 s); naopak 80 min záznamu by dalo cca 450 000 rámců a 3 h cca 1 mil. rámců (vlastní výpočet; možné vysvětlení, že se použila jen část rámců, v textu není).
- Závěr tvrdí "A TDOA-based localization module offering sub-meter positional error" [s. 14], kdežto s. 11 uvádí medián 1,2 m a úhlovou chybu 6,3°; s. 12 až 13 pak 0,3 až 0,5 m (Tab. 2, 3).
- Kandeepan et al. [18] jsou v úvodu popsáni jako evaluace RF, SVM a MLP ("good baseline performance") [s. 2], v Sec. 2.4 jako RF, SVM a CNN ("high classification accuracy") [s. 3].
- Citace nesedí k tvrzením: [7] (Strohmeier, bezpečnost ADS-B) je uvedena u slabé detekce malých dronů radarem [s. 2]; [28] (Kopeika, omezení vizuální a IR detekce) je uvedena u výhod akustických systémů [s. 14].
- Deklarovaná chyba 0,3 až 0,5 m na vzdálenost do 50 m s aperturou čtverce cca 15 cm (pásmo do 1,1 kHz) je těžko slučitelná s fyzikální úhlovou rozlišovací schopností; metoda sloučení TDOA a AOA a způsob výpočtu vzdálenosti nejsou popsány (interpretace).
- Latence měřena na PC i7, přestože je systém popsán na RPi 4; Obr. 1 ukazuje PC místo RPi [s. 6, 7, 12].
- Reference: [9] a [29] jsou totožné (Kwon a Kim, Sensors 19, 23 (2019) 5178; potvrzeno v seznamu na s. 15); [21] (Ghouli, bistabilní energy harvesting) a [22], [23] (monitorování solenoidových ventilů, NodeMCU) s tématem nesouvisí; [18] je uvedena bez konkrétní konference ("2019 IEEE International Conference on Acoustics") a bez stran; u ostatních citací (např. [10]-[13], [15]-[17], [20], [24]-[27], [30]-[32], [39]) existenci neověřoval ani první autor, ani kolo 2 (PDF samotné nemůže existenci potvrdit). Před citací těchto referencí doporučuji nezávislé ověření.
- Rovnice (2) pro útlum amplitudy `A0/d²` je nesprávná pro amplitudu zvukového tlaku (interpretace).

## Relevance pro náš projekt
- SOTA sekce: příklad levného embedded přístupu s MEMS (INMP441 + ESP32-S3 + RPi 4) a ML detekcí; srovnání modalit radar/RF/optika/akustika v Tab. 1 [s. 4]. Vhodné jen jako ilustrace trendu; kvantitativní tvrzení je nespolehlivé (viz nekonzistence).
- Volba pásma: bandpass 200 Hz až 10 kHz pro detekci [s. 7] vs. naše pásmo 300 Hz až 2 kHz; autor tvrdí, že energie dronů se soustřeďuje do nízkých kHz [s. 4]. Pásmo prostorové jednoznačnosti jeho pole je jen do 1,1 kHz [s. 5] (rovnice (3)), což ilustruje nutnost vazby rozteč mikrofonů/horní frekvence i pro naše pole (pro naši 2×8 UCA viz poznámku o d = D·sin(π/8) v Itare).
- Geometrie 2×8 UCA: Ghouli používá jedinou rovinu 8 mikrofonů ve čtverci a sám uvádí problém s dronem přímo nad polem [s. 12, 14, 15]; dva kruhy 2×8 umožňují elevaci (3D), což je argument pro naši konfiguraci (interpretace).
- Volba metody: TDOA + LS (naše GCC-PHAT + LS) je zde jen v plné korelaci bez PHAT; není srovnání s DAS/MVDR/MUSIC. Může sloužit jako důkaz, že párová korelační metoda je dostupná i na levném HW, ale bez kvantitativní hodnoty.
- Výpočetní nároky: počítá se na RPi 4 (Cortex-A72 1,5 GHz) se 65 až 70 % CPU, latence cca 150 ms [s. 7]; to je řádově vyšší výkon než STM32H563 (Cortex-M33); tvrzení o ESP32 není doloženo. Použitelné jako referenční bod "latence 150 ms jako požadavek real-time" (podle citované Kandeepan et al. 200 ms [s. 11]).
- Nepřenositelné: fs 48 kHz vs. naše 16 kHz; klasifikace ML (MFCC + RF) mimo náš rozsah (my detekujeme triggerem a lokalizujeme 100 ms okno); vzdálenosti do 50 až 60 m; chybí metodika SNR a ground truth z přesných zdrojů.

## Citovatelná tvrzení
- Akustika jako pasivní metoda: "Acoustic detection has emerged as a promising passive method, especially suitable for low-altitude, small UAVs in short to medium range scenarios" [s. 1].
- Pásmo dronu: "drone propeller noise can be characterized by strong peaks in the frequency domain, particularly in the 100 Hz to 10 kHz range" [s. 1].
- BPF: "The BPF can be approximated by: f_BPF = N_b × f_r" [s. 4].
- Prostorový aliasing: "The array spacing must be selected to avoid spatial aliasing" [s. 4].
- Pole: "inter-element spacing of 15 cm ... allows spatially unambiguous detection of acoustic signals up to approximately 1.1 kHz" [s. 5].
- INMP441: "flat frequency response between 60 Hz and 15 kHz, with a signal-to-noise ratio (SNR) around 62 dB" [s. 5].
- Latence: "The total processing latency per detection cycle was approximately 150 ms" [s. 7].
- Úhlová chyba: "Mean absolute error of 6.3° (std: 2.4°)" [s. 11].
- Vítr: "Strong coastal winds occasionally masked low-frequency drone harmonics, reducing detection sensitivity by approximately 10–15%." [s. 12]
- Dron nad polem: "Drones flying directly above the array showed reduced angular resolution, increasing localization errors by roughly 20–25% compared to lateral flight paths." [s. 12]
- Dosah: "Effective detection was constrained to approximately 60 m" [s. 14].
- Přínosy akustiky: "Passive and covert; low cost; independent of lighting and RF environment; suitable for low-altitude detection." [s. 4, Tab. 1]
- Limitace akustiky: "Limited range; sensitive to environmental noise and wind; affected by reverberation." [s. 4, Tab. 1]

## Relevantní reference z článku
(Přesně podle seznamu [s. 15, 16]; DOI v seznamu neuvedeny. Kvůli výše uvedeným pochybnostem ověřit existenci před dalším citováním.)
- 9. O.J. Kwon, Y.H. Kim: Acoustic UAV detection using time-delay estimation and sound power analysis. Sensors 19, 23 (2019) 5178. (duplicitně jako 29.)
- 10. P.U. Alvarado, et al.: Evaluation of acoustic signatures for small UAV detection. Applied Acoustics 164 (2020) 107257.
- 11. C. Hanson, et al.: Acoustic signature analysis of small UAVs: detection and classification. Applied Acoustics 147 (2019) 123–133.
- 12. A. Habib, et al.: Passive acoustic drone detection using distributed microphone arrays. Sensors 21, 5 (2021) 1613.
- 13. M. López-Martín, et al.: Deep learning for UAV acoustic detection and localization. Journal of the Franklin Institute 358, 16 (2021) 8353–8373.
- 14. Analog Devices: INMP441 MEMS Microphone Datasheet, 2023.
- 15. S. Biedron, et al.: Design of a compact microphone array for low-altitude drone detection. Journal of Acoustical Engineering 59, 6 (2021) 421–432.
- 16. J. Palacios, et al.: Beamforming techniques for UAV localization using MEMS microphone arrays. IEEE Sensors Journal 21, 12 (2021) 14021–14030.
- 17. C. Park, et al.: DOA estimation using acoustic arrays for drone surveillance. Applied Sciences 12, 3 (2022) 1304.
- 18. S. Kandeepan, et al.: Deep learning approaches for acoustic UAV detection: a comparative study, in: 2019 IEEE International Conference on Acoustics, 2019.
- 30. J. Xiang, et al.: Adaptive filtering methods for drone noise in real-time detection. Signal Processing 181 (2021) 107889.
- 19. J. Salamon, et al.: Deep convolutional neural networks and data augmentation for environmental sound classification. IEEE Signal Processing Letters 24, 3 (2017) 279–283.
- 20. Q. Ma, et al.: RNN architectures for UAV sound classification in outdoor conditions. Sensors 22, 4 (2022) 1331.
- 31. J. Lee, et al.: Acoustic holography for UAV detection. Journal of Sound and Vibration 524 (2022) 116747.
- 32. Y.J. Tseng, et al.: UAV detection using hybrid AI models based on acoustic and visual features. Sensors 22, 1 (2022) 153.
- 34. S. Davis, P. Mermelstein: Comparison of parametric representations for monosyllabic word recognition in continuously spoken sentences. IEEE Transactions on Acoustics, Speech, and Signal Processing 28, 4 (1980): 357–366.
- 37. S.F. Boll: Suppression of acoustic noise in speech using spectral subtraction. IEEE Transactions on Acoustics, Speech, and Signal Processing 27, 2 (1979) 113–120.
- 38. J.S. Lim, A.V. Oppenheim: Enhancement and bandwidth compression of noisy speech. Proceedings of the IEEE 67, 12 (1979) 1586–1604.
- 39. J. Van der Merwe, A. Bekker: Passive acoustic detection of multirotor drones in outdoor environments. Sensors 22, 3 (2022) 915.
- 40. J. Chen, J. Benesty: Microphone Array Signal Processing. Springer, 2006.
- 41. M. Brandstein, D. Ward: Microphone Arrays: Signal Processing Techniques and Applications. Springer, 2001.
- 42. C.H. Knapp, G.C. Carter: The Generalized Correlation Method for Estimation of Time Delay. IEEE Transactions on Acoustics, Speech, Signal Processing 24, 4 (1976) 320–327.

## Kontrola (kolo 2)
- Opraveno: (1) rovnice (5) (s. 5) používá signály s_i, s_j, rovnice (11) (s. 9) x_i, x_j; obě bez mezí integrálu; (2) Obr. 2 (s. 6) není anotovaný nákres geometrie, ale neanotovaná fotografie desky na stativu, přestože popisek tvrdí opak; (3) Obr. 6 (s. 10) je podle popisku simulace a jeho osa (cca ±0,08 m mikrofony) neodpovídá d = 15 cm; (4) abstrakt (s. 1) mluví o "distributed array" a "angular position", tělo o jednom čtvercovém poli a 2D poloze; (5) potvrzeno: 91,6 % (s. 11) vs. 92 až 95 % (Tab. 2, s. 13); 6,3° a medián 1,2 m (s. 11) vs. 0,3 až 0,5 m (Tab. 2, 3, s. 13) a "sub-meter" v závěru (s. 14); 3 h vs. 40 × 2 min vs. 22 500 rámců (s. 11); šablona v Sec. 2.4 (s. 3); Obr. 1 ESP32 -> PC (s. 6) vs. Raspberry Pi (s. 5, 7); latence změřena na PC Intel i7 (s. 12) vs. popis RPi 4 (s. 7); Dataset A/B nedefinovány (s. 8); poloha mikrofonů ve čtverci neuvedena (s. 5); refs [9] = [29] (s. 15); [21], [22], [23] mimo téma (s. 2, 15); (6) doplněny další zbytky šablony/odpovědí recenzentům (s. 10, 12, 13, 14), rozpor popisu Kandeepan et al. (s. 2 vs. s. 3) a chybně přiřazené citace [7] a [28]; (7) BibTeX bez pole number (číslo sešitu není v PDF, 12 je číslo článku). Rovnice (1)-(12), Tab. 1-3, všechny parametry (fs 48 kHz, rámce 1024, 50 % překryv, Hamming, pásmo 200 Hz až 10 kHz, d = 15 cm, 8 × INMP441, výška stativu 1,5 m, 40 běhů, 80/20) i všechny přímé citace byly proti PDF ověřeny a odpovídají včetně stran.
- Doplněno: dostupnost dat a střet zájmů (s. 15); popis Obr. 5 a 6 (TDOA a AOA, s. 10); odůvodnění volby dronu a skutečnost, že fúze TDOA/AOA není popsána (s. 9, 10); reference [19], [20], [31], [32], [34], [37], [38] přepsané přesně z PDF (s. 15 až 16); výpočet očekávaného počtu rámců z 80 min a 3 h záznamu.
- Nejistoty: nelze rozhodnout, které metriky (6,3° / 1,2 m vs. 0,3 až 0,5 m) odpovídají skutečným měřením; co jsou Dataset A a B; jaký je skutečný počet rámců a doba záznamu; na jakém HW (PC i7 vs. RPi 4) a s jakým polem (jedno vs. více polí) byl systém ve skutečnosti měřen; existence většiny citovaných prací (kromě těch, které lze odvodit z PDF) nebyla ověřena; skutečná poloha mikrofonů a způsob výpočtu 2D polohy z TDOA a AOA nejsou v článku popsány.
