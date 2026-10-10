# Itare et al. 2022: DOA dronu pomocí DSB a sledování harmonických ve čas-frekvenční rovině

Poznámka k číslování stran: PDF má 18 stran a číslování PDF se shoduje s časopiseckým ("N of 18"); [s. X] platí pro obojí.

## Bibliografie
- Itare, N.; Thomas, J.-H.; Raoof, K.; Blanchard, T. "Acoustic Estimation of the Direction of Arrival of an Unmanned Aerial Vehicle Based on Frequency Tracking in the Time-Frequency Plane". *Sensors* 2022, 22, 4021. https://doi.org/10.3390/s22114021 [s. 1]
- Received 15 April 2022, Accepted 24 May 2022, Published 26 May 2022; Academic Editors: Fangqing Wen, Wei Liu, Jin He, Veerendra Dakulagi [s. 1]. Pracoviště: Laboratoire d'Acoustique de l'Université du Mans (LAUM), CNRS, Le Mans Université, Francie [s. 1].
- Vydavatel: MDPI, Basel; MDPI ano; open access ano (CC BY 4.0) [s. 1].
- Číslo sešitu 11 plyne z DOI (s22114021) a z data publikace (26. 5. 2022), v PDF přímo neuvedeno (interpretace; nelze ověřit z PDF).
- Financování: Direction Générale de l'Armement (DGA) grant 01D19024292 AID a Région des Pays de Loire, grant cofi_DGA_U72_LAUM_49258; APC hradilo LAUM [s. 17]. Dostupnost dat: v PDF není uvedena.
- Návrh BibTeX:

```bibtex
@article{itare2022frequency,
  author    = {Itare, Nathan and Thomas, Jean-Hugh and Raoof, Kosai and Blanchard, Torea},
  title     = {Acoustic Estimation of the Direction of Arrival of an Unmanned Aerial Vehicle Based on Frequency Tracking in the Time-Frequency Plane},
  journal   = {Sensors},
  year      = {2022},
  volume    = {22},
  number    = {11},
  pages     = {4021},
  publisher = {MDPI},
  doi       = {10.3390/s22114021}
}
```

## Problém a přínos
- Cíl: odhadnout DOA (azimut, elevace) dronu z mikrofonního pole tak, aby se zvýšil SNR využitím akustického podpisu dronu (harmonická struktura BPF) [s. 1, 2].
- Východisko: Blanchard et al. [19] použili DSB v časové oblasti s předfiltrovanými signály (zero-phase filtr, forward + reverse), což "is not easily implementable in real time" [s. 2].
- Přínos: DSB v časové oblasti + STFT výstupu beamformeru; energie se počítá jen z čas-frekvenčních binů kolem harmonických frekvence zjištěné pitch trackingem (HPS nebo SHC). Odpadá předfiltrování signálu každého mikrofonu; metoda umí separovat dva drony s odlišným spektrem a snáší nízké SNR [s. 1, 2, 4, 8].
- Dále: srovnání dvou pitch trackerů (HPS vs. SHC) a volba šířky pásma kolem harmonických (konstantní vs. s činitelem jakosti Q) [s. 9-12].

## Mikrofonní pole a hardware
- Pole navržené Blanchardem et al. [19]: 3D, 10 mikrofonů na 3 navzájem kolmých osách, každá osa se 4 mikrofony, jeden referenční mikrofon x0 společný všem třem větvím [s. 3, Obr. 1].
- Vzdálenosti od reference: ‖x1‖ = ‖x4‖ = ‖x7‖ = l1, ‖x2‖ = ‖x5‖ = ‖x8‖ = l2, ‖x3‖ = ‖x6‖ = ‖x9‖ = l3, s l1 = 5 cm, l2 = 20 cm, l3 = 110 cm (rovnice (1)) [s. 3].
- Pásmo pole: l1 určuje horní frekvenci podle prostorového Shannonova kritéria, l3 dolní; "This arrangement gives the antenna a bandwidth of [220.5, 3430] Hz" [s. 3]. (Kontrola: c/(2 l1) = 343/0,1 = 3430 Hz, shoduje se; vlastní výpočet.)
- Kritéria návrhu geometrie: (1) 3D lokalizace, (2) citlivost na rozsah frekvencí dronů (více harmonických BPF), (3) málo senzorů a kanálů, (4) snadná přeprava [s. 3].
- Mikrofony v experimentu: 10 × BSWA Technology MPA 416, 1/4 in., 20 Hz až 20 kHz [s. 13]. Nejde o MEMS (interpretace, PDF typ kapsle neuvádí).
- Jiné návrhy geometrie citované autory [s. 3]: 72 až 96 MEMS na polokouli optimalizované genetickým algoritmem [28]; rovinné pole pro vysokorychlostní vlak [29]; spirálové pole [30]; pole se 3 senzory (diferenciální pole 1. řádu) [31]; optimalizace roje částic s TDOA ve fitness funkci [32].
- Akvizice: PXI-1036 chassis (National Instruments) + laptop; 10 kanálů simultánně; fs = 20 kHz [s. 13]. Bit depth: neuvedeno. Synchronizace: simultánní záznam 10 kanálů na jednom chassis; detaily neuvedeny. Kalibrace: neuvedena.
- Simulace: zdroj monopól, volné pole [s. 4]; fs = 20 kHz je výslovně uvedena u hodnocení vs. SNR [s. 9] a v Tab. 1 [s. 12]; u statických simulací v kap. 3.2 a 3.3 fs není výslovně uvedena (interpretace: stejná).
- Experiment: DJI Phantom IV, pole na hřišti Le Mans University ("a field of Le Mans University") [s. 13].

## Signálový model a rovnice
Zpoždění pro zaostření (virtuální zdroj M, vzdálenost ‖x_i M‖ od mikrofonu i, `c` rychlost zvuku) [s. 4]:

$$\tau_i = \frac{\left|\,\lVert x_0 M\rVert - \lVert x_i M\rVert\,\right|}{c} \quad (2)\ [s.\ 4]$$

(Absolutní hodnota je tak vytištěna v článku; pro zaostření s kladnými i zápornými zpožděními by se použil rozdíl se znaménkem; interpretace.)

Vztah BPF a otáček [s. 5]:

$$f_{bp} = N_b f_0 \quad (3)\ [s.\ 5]$$

Model signálu na i-tém mikrofonu (sudé harmonické silné, liché slabé; dvoulistá vrtule, `N_h` počet harmonických) [s. 5]:

$$p_i(t)=\sum_{n=1}^{N_h}\frac{\beta\cos\!\big(2\pi[2n-1]f_0\,(t-\lVert x_iM\rVert(t)/c)\big)}{4\pi\lVert x_iM\rVert(t)}+\frac{\alpha_{f_0}(n)\cos\!\big(2\pi[2n]f_0\,(t-\lVert x_iM\rVert(t)/c)\big)}{4\pi\lVert x_iM\rVert(t)} \quad (4)\ [s.\ 5]$$

s parametry β = 10^{−1,5} a α_{f0}(n) = 10^{(1/20)(−11,6 log10(2n f0) + 65,4)} [s. 5]. Příklad: f0 = 80,6 Hz, f_bp = 161,2 Hz [s. 5, Obr. 3]. Model nezahrnuje čtyři motory (šířka okolo harmonických je v modelu konstantní, ve skutečnosti ne) [s. 5].

Harmonic Product Spectrum (HPS) [s. 10]:

$$f_0 = \arg\max_f \sum_{k\in\mathbb{N}^*}\log_{10}|X(kf)| \quad (5)\ [s.\ 10]$$

Spectral Harmonic Correlation (SHC) [s. 10] (L_w délka okna ve frekvenční oblasti, N_H počet harmonických, S(t, f) čas-frekvenční reprezentace):

$$SHC(t,f)=\sum_{f'=-L_w/2}^{L_w/2}\ \prod_{r=1}^{N_H+1} S(t,\,rf+f') \quad (6)\ [s.\ 10]$$

Šířka pásma kolem harmonické s činitelem jakosti [s. 12]:

$$Q = \frac{f_{detect}}{\Delta f} \quad (7)\ [s.\ 12]$$

Vybírají se biny v pásmu [f_detect·i·(1 − 1/(2Q)); f_detect·i·(1 + 1/(2Q))], i = 1, ..., n_h [s. 12].

Výkon: energie E(φ, θ) zaostřeného signálu se počítá pouze z vybraných TF binů (STFT zaostřeného signálu); DOA je směr s maximální energií [s. 4].

## Algoritmus
(Obr. 2 synopse, [s. 4])
1. Pro každý časový úsek: pitch tracking (HPS nebo SHC) na signálu referenčního (středového) mikrofonu před zaostřením, výsledek f_detect (typicky f_bp) [s. 4].
2. Pro každý směr Ω = (φ, θ) z mřížky: DSB v časové oblasti se zpožděními (2), STFT zaostřeného signálu, výběr TF binů kolem harmonických f_detect·i, výpočet energie E(φ, θ) [s. 4].
3. DOA = argmax E(φ, θ) [s. 4].
- Mřížka: azimut [−180°, 180°], elevace [0°, 90°] [s. 4]; rozlišení (4°, 2°) v simulaci [s. 7] a pro měření výpočetního času [s. 14]; (1°, 1°) při vyhodnocení experimentálního letu [s. 14].
- Délka signálu pro energii: 2048 vzorků (simulace statická) [s. 7]; 4096 vzorků pro BF a SHC (pohyblivý zdroj, simulace i experiment), simulovaný signál 5000 vzorků "to be sure to process the 4096 delays" [s. 12, 14].
- STFT (statický případ): FFT 4096 bodů, Hanningova okna 2048 bodů, překryv 512 bodů [s. 7].
- Harmonické: pro statický případ 5 sudých harmonických 2·f0, 4·f0, 6·f0, 8·f0, 10·f0 (tj. i·f_bp, i = 1..5); biny v intervalech [2 i f0 − f0/10; 2 i f0 + f0/10], šířka Δf = f_bp/10 = 2·f0/10 = 16,12 Hz (f0 = 80,6 Hz); lichá harmonická se nepoužívají, protože při SNR ≤ 40 dB jsou ve spektru jen silné (sudé) [s. 7].
- Počet zdrojů: 1 (hlavní scénář), 2 s odlišnými f0 (f01 = 150 Hz, f02 = 175 Hz) vybírá se maska pro jeden zdroj [s. 8].
- HPS: součin komprimovaných spekter, 3 nebo 4 komprese [s. 9-11]. SHC: L_w = 10 Hz, N_H = 4 (nebo 3), hledání mezi 100 a 500 Hz (BPF DJI Phantom IV je v tomto intervalu) [s. 10, 11].
- Šířka pásma: konstantní Δf, nebo s Q; výběr Q a Δf v TFR: Q = 10 (Δf = 17,5 Hz pro první harmonickou), Q = 5, Q = 17,5 [s. 14].
- Váhování mikrofonů, regularizace, kalibrace polohy: neuvedeno (DSB bez adaptivního váhování; adaptivní beamforming zvažován a zamítnut kvůli inverzi kovarianční matice a citlivosti na chyby pozic mikrofonů, [s. 2]).

## Experiment / simulace
- Dron: DJI Phantom IV (kvadrokoptéra, dvoulistá vrtule: silné sudé a slabé liché harmonické) [s. 4, 5]; BPF f_bp = 161,2 Hz až 175 Hz (v simulaci), v experimentu f_bp = 175 Hz [s. 5, 14]; f0 = 80,6 Hz (4836 rpm) pro statické simulace [s. 7].
- Skutečný signál pro simulaci: z měření v bezodrazové komoře, dron držen nylonovými lany, vzdálenost 1,5 m od pole, zaostřeno beamformingem a pak numericky propagováno na virtuální pole [s. 4].
- Statická simulace, 1 zdroj: SNR 20 dB (AWGN), zdroje v (0°, 45°) a (100°, 70°), vzdálenost 10 m, mřížka (4°, 2°), energie z 2048 vzorků [s. 7].
- Statická simulace, 2 zdroje: f01 = 150 Hz v (0°, 45°), f02 = 175 Hz v (100°, 70°), 5 harmonických každého [s. 8].
- Vyhodnocení vs. SNR: 50 virtuálních zdrojů po 4096 vzorcích, fs = 20 kHz, beamforming na 2048 vzorcích, zdroj (0°, 45°) ve vzdálenosti 1 m, výsledek průměr [s. 9]. Porovnání: klasické DSB, TFR s 1 a 5 harmonickými, BTPFS (Beamforming with Temporally Pre-Filtered Signals) s 5 harmonickými [s. 9].
- Pitch tracking vs. SNR: 100 virtuálních zdrojů na každou úroveň šumu; f0 = 87,5 Hz (f_bp = 175 Hz); FFT 2048, 4096, 8192 bodů [s. 11].
- Simulace trajektorie: start na zemi cca 2 m od pole, maximální výška 3,1 m, sestup na zem ve vzdálenosti 4 m; SNR 5 dB; frekvenční modulace f_bp kolem 175 Hz (f0 cca 87,5 Hz); TFR s 5 harmonickými, Q = 10; SHC; okna 4096 vzorků [s. 12].
- Reálný let: start na zemi 3 m od pole, výstup mírnou rychlostí do výšky 3,2 m, pak sestup; azimut a elevace odvozeny z GPS na dronu; "Note that the GPS has an incertitude around 3 m" [s. 13, 14]. Vítr, venkovní SNR: neuvedeno. Počet letů: 1 trajektorie (interpretace z popisu).

## Výsledky (čísla)
- Pole: pásmo [220,5; 3430] Hz [s. 3].
- Směrovost pole (monopól na 1 m): šířka hlavního laloku v azimutu na 350 Hz [38°, 60°] ("everywhere else and especially around the trajectory"), na 525 Hz [29°, 39°]; pro elevace zdroje > 80° je azimutální šířka 360°; šířka v elevaci [35°, 40°] pro 350 Hz (fokus [20°, 70°]) a cca 25° pro 525 Hz; MSL kolem 0 dB pro azimut ±180° na 350 Hz, jinde 1 až 3 dB; na trajektorii MSL cca 1 dB (obtížné odlišit postranní lalok od druhého zdroje) [s. 6, Obr. 4]. Frekvence 175, 350, 525 Hz = BPF a 2 harmonické (4·f0, 6·f0) pro dron z kap. 5 [s. 6].
- 1 zdroj, SNR 20 dB: klasické DSB i TFR dávají maximum blízko skutečné polohy; TFR "a little more energy where the source is located" [s. 7].
- 2 zdroje: TFR zlepšuje přesnost zdroje v (0°, 45°) (3° na Obr. 6a, 1° na Obr. 6b) a umí oddělit zdroje podle jejich spektrálního obsahu [s. 8].
- Chyba vs. SNR (Obr. 7) [s. 9; hodnoty z textu; Obr. 7 je bodový graf bez tabulky, vizuálně je text konzistentní: chyba klasického DSB klesá k 0° kolem −6 až −10 dB, azimutální chyba při −30 dB dosahuje desítek stupňů až cca 100°]: klasické DSB má chybu 0° v azimutu do −6 dB SNR; TFR s 1 a 5 harmonickými začíná růst při 10 dB a −14 dB; BTPFS (5 harmonických) roste jako klasické DSB, ale rychleji. Elevace: chyba cca 1°, začíná růst od −6 dB (klasické DSB), od 10 dB (TFR 1 harmonická), od −14 dB (TFR 5 harmonických), od −2 dB (BTPFS 5 harm.). TFR s 5 harmonickými: chyba blízká 0° do −16 dB (azimut) a 1° do −14 dB (elevace). Závěr: "it is better to use more harmonics in the TFR to have a better performance with lower SNR." [s. 9].
- Pitch tracking (2048 bodů FFT): přesný odhad f_bp do −14 dB (HPS) a do −16 dB (SHC); zdvojnásobení počtu bodů FFT umožní o 2 dB horší SNR; SHC je lepší než HPS při silném šumu [s. 11]. Spodní hranice SNR pro metody podle autorů: kombinace Obr. 7 a Obr. 10 [s. 11].
- Tabulka maximálních Q [s. 12, Tab. 1] (fs = 20 kHz; N_FFT body FFT; ΔFFT rozestup čar; Δf_min minimální šířka pásma):

| N_FFT | Doba [s] | ΔFFT [Hz] | Δf_min [Hz] | Q_max (f = 175 Hz) | Q_max (f = 350 Hz) |
|---|---|---|---|---|---|
| 16 384 | 0.8 | 1.22 | 3 | 58 | 116 |
| 8192 | 0.4 | 2.44 | 5 | 35 | 70 |
| 4096 | 0.2 | 4.88 | 10 | 17.5 | 35 |
| 2048 | 0.1 | 9.76 | 20 | 8.75 | 17.5 |
| 1024 | 0.05 | 19.53 | 40 | 4.37 | 8.75 |

- Simulovaná trajektorie, SNR 5 dB: chyba "around 1.7° in azimuth and 1.1° in elevation"; TFR mírně lepší než klasické DSB [s. 12].
- Chyba řízení (steering) [s. 13, Tab. 2], střední hodnota μ a směrodatná odchylka σ v °:

| Metoda | Azimut μ | Azimut σ | Elevace μ | Elevace σ |
|---|---|---|---|---|
| Simulovaná trajektorie: klasické DSB | 1.8 | 0.8 | 1.1 | 0.7 |
| Simulovaná: TFR 5 harmonických | 1.7 | 0.8 | 1.1 | 0.7 |
| Experimentální trajektorie: klasické DSB | 2.9 | 2.6 | 2 | 1.5 |
| Experimentální: TFR 5 harm. (Q = 10) | 2.3 | 2.5 | 2.8 | 1.9 |
| Experimentální: TFR 5 harm. (Δf = 17.5 Hz) | 2.3 | 2.5 | 2.6 | 1.9 |
| Experimentální: BTPFS | 2.5 | 2.4 | 4.7 | 6.1 |

- Experiment: TFR v azimutu o něco lepší než klasické DSB (cca 2,3° vs. 2,9°); v elevaci klasické DSB lepší (2 vs. 2,6 až 2,8°), TFR "still close" [s. 14]. Konstantní šířka pásma dává méně chyb na konci trajektorie a v elevaci více odhadů blízko reference pro elevace > 40°; rozdíly mezi Q a konstantním pásmem "are not very marked" [s. 14]. Spektrogram ukazuje stopy čtyř rotorů s různými otáčkami, kompaktní na nízkých a rozprostřené na vyšších frekvencích [s. 14].
- Počet harmonických v experimentu (3, 4, 5, 10, 15, 20): chyby se mění málo, "from 2.7° to 4.2° in azimuth and 12.8° to 15.6° in elevation", nejlepší jsou 4 nebo 5 harmonických jako kompromis přesnost/výpočet [s. 14]. Pozor: text s. 14 uvádí rozsah 2,7° až 4,2° v azimutu a 12,8° až 15,6° v elevaci, kdežto Tab. 2 (s. 13) uvádí pro experimentální trajektorii střední chyby 2,3° až 2,9° v azimutu a 2,0° až 4,7° v elevaci. Rozpor je v PDF (potvrzeno); v textu není vysvětlen (jiné nastavení, jiná reference nebo překlep; interpretace). Citovat opatrně, přednostně Tab. 2.
- Výpočetní čas: "takes 10.7 s to provide a DOA estimate on a 5000-point slot with a (4°, 2°) resolution", Matlab, AMD Ryzen 7 5800H 3,20 GHz; lze zkrátit paralelizací a zúžením mřížky okolo předchozího odhadu [s. 14].

## Omezení podle autorů
- Fundamentální frekvence se v simulacích zprvu předpokládá známá; v praxi nutný pitch tracking, jehož dolní mez SNR omezuje metodu (−14 dB HPS, −16 dB SHC při 2048 bodech) [s. 9, 11].
- Model signálu nezahrnuje čtyři motory (konstantní šířka harmonických) [s. 5].
- Při nízkém SNR (≤ 40 dB) jsou ve spektru jen silné sudé harmonické, proto se používají pouze ty [s. 7].
- Směrovost pole: postranní laloky blízko 0 dB u některých směrů; obtížné rozlišit sekundární lalok od druhého zdroje [s. 6].
- Metoda by měla být testována na složitějších trajektoriích s více drony; geometrii pole lze optimalizovat [s. 16, 17].
- Výpočetní čas je vysoký (10,7 s na slot v Matlabu); ground truth z GPS s nejistotou cca 3 m [s. 14].

## Relevance pro náš projekt
- SOTA sekce: referenční práce pro "harmonic-aware" DSB; kontrast k našim metodám (DAS, MVDR, MUSIC): autoři zvažovali robustní adaptivní beamforming [20] (MVDR není jmenováno; Capon [8] je v úvodu uveden jen jako kategorie metod, interpretace že jde o MVDR), ale zamítli kvůli inverzi kovarianční matice (regularizace, krátké snímky) a citlivosti na chyby pozic mikrofonů [s. 2]. Dobrý citační zdroj pro zdůvodnění, proč se MVDR/MUSIC na MCU nepoužívá (nebo kdy ano).
- Volba pásma: BPF DJI Phantom IV v 100 až 500 Hz (SHC) [s. 10], f_bp = 175 Hz, harmonické do 10·f0 ≈ 875 Hz pro f0 = 87,5 Hz; využitelné harmonické 4 až 5 (do cca 1 kHz) stačí [s. 14]. To podporuje naše pásmo cca 300 Hz až 2 kHz; velká část informace je ale v nižších harmonických (pod 1 kHz) (interpretace).
- Okno: 2048 vzorků při 20 kHz = 0,1 s (Tab. 1) odpovídá našemu oknu 100 ms (1600 vzorků při 16 kHz, ΔFFT = 10 Hz, vlastní výpočet); Q_max pro 2048 bodů je 8,75 pro 175 Hz, tzn. úzké pásmo kolem harmonické vyžaduje delší okno.
- Geometrie: pole 10 mikrofonů na 3 osách s l = 5, 20, 110 cm (pásmo 220,5 až 3430 Hz) je zcela odlišné od naší 2×8 UCA (160 až 250 mm průměr); přenositelné jen principiálně: horní frekvence z rozteče (c/(2d)), dolní z apertury. U naší UCA: pro tětivu mezi sousedními mikrofony kruhu o 8 mikrofonech d = D·sin(π/8) je f_max = c/(2d); D = 160 mm dává d ≈ 61 mm a f_max ≈ 2,8 kHz, D = 250 mm dává d ≈ 96 mm a f_max ≈ 1,8 kHz (vlastní výpočet, c = 343 m/s, interpretace).
- Výpočet na MCU: algoritmus je výpočetně náročný (pro každý směr DSB + STFT; 10,7 s na slot v Matlabu na Ryzen 7). Pro STM32H563 nepřímo použitelný; lze přenést myšlenku (výběr harmonických binů) do frekvenční oblasti (interpretace): beamforming ve frekvenční oblasti jen na binech harmonických, tj. několik desítek binů místo plného signálu. Zjednodušení: hledání kolem předchozího odhadu [s. 14].
- Nepřenositelné: jediný dron (Phantom IV, 2 listy), vzdálenost 3 až 4 m od pole (příliš blízko, ne dálkové pole), reference GPS s nejistotou 3 m, fs 20 kHz, 10 mikrofonů na 3D ose s dlouhým ramenem 1,1 m.

## Citovatelná tvrzení
- Dominantní zvuk dronů: "the noise produced by the UAVs is still predominant, so it gives a good opening for the development of acoustic methods." [s. 1]
- Harmonická struktura: "multi-rotor drones and Radio Controlled airplanes produce harmonic structured acoustic signals" [s. 2].
- Proč DSB v časové oblasti: "using DSB in the time domain seems relevant to us since it allows us to envisage a real-time implementation while presenting a good robustness to noise." [s. 2]
- Proč ne MVDR: "the technique requires inverting the interference-plus-noise covariance matrix, which may also include a regularisation step and the consideration of shorter snapshots to be averaged." [s. 2]
- Citlivost adaptivních metod na polohu: "The method is also imprecise when the positions of the antenna microphones are affected by errors" [s. 2].
- Geometrie a pásmo: "This arrangement gives the antenna a bandwidth of [220.5, 3430] Hz" [s. 3].
- BPF: f_bp = N_b · f0; dron se dvěma listy má "strong even harmonics with predominant amplitudes ... weak odd harmonics" [s. 5].
- Víc harmonických je lepší při nízkém SNR: "it is better to use more harmonics in the TFR to have a better performance with lower SNR." [s. 9]
- SHC vs. HPS: "The SHC appears to perform better than the HPS in the presence of strong noise." [s. 11]
- Kompromis počtu harmonických: "the results are better when considering 4 or 5 harmonics, which is a reasonable compromise between localization accuracy and computation cost." [s. 14]
- Výpočetní náročnost: "takes 10.7 s to provide a DOA estimate on a 5000-point slot with a (4°, 2°) resolution." [s. 14]
- Separace zdrojů: "it is a useful tool for separating multiple sources if each of them has a different spectral content." [s. 8]
- Střední chyba v letu: azimut 2,3° (TFR) vs. 2,9° (klasické DSB) [s. 13, Tab. 2].
- Metoda je použitelná v reálném čase: "the processing is based on the time-frequency representation of the focused signal ... and could be carried out in real time." [s. 17]

## Relevantní reference z článku
(Přesně podle seznamu [s. 17, 18]; DOI v PDF jen jako [CrossRef], čísla DOI neuvedena.)
- [3] Lykou, G.; Moustakas, D.; Gritzalis, D. Defending Airports from UAS: A Survey on Cyber-Attacks and Counter-Drone Sensing Technologies. Sensors 2020, 20, 3537. [CrossRef]
- [4] Ramamonjy, A. Développement de Nouvelles Méthodes de Classification/Localisation de Signaux Acoustiques Appliquées aux Véhicules Aériens. (Development of New Classification/Localization Methods Applied to Aerian Vehicle Acoustical Signals). Ph.D. Thesis, Conservatoire National des Arts et Métiers, Paris, France, 2019.
- [5] Baron, V.; Bouley, S.; Muschinowski, M.; Mars, J.; Nicolas, B. Localisation et identification acoustique de drones par mesures d'antennerie et apprentissage supervisé (Acoustic Localization and identification with antennas and supervised learning). In Proceedings of the GRETSI 2019 - XXVIIème Colloque Francophone de Traitement du Signal et des Images, Lille, France, 26–29 August 2019.
- [6] Sedunov, A.; Haddad, D.; Salloum, H.; Sutin, A.; Sedunov, N.; Yakubovskiy, A. Stevens Drone Detection Acoustic System and Experiments in Acoustics UAV Tracking. In Proceedings of the 2019 IEEE International Symposium on Technologies for Homeland Security (HST), Woburn, MA, USA, 5–6 November 2019; pp. 1–7. [CrossRef]
- [7] Van Veen, B.; Buckley, K. Beamforming: A versatile approach to spatial filtering. IEEE ASSP Mag. 1988, 5, 4–24. [CrossRef]
- [8] Capon, J. High-resolution frequency-wavenumber spectrum analysis. Proc. IEEE 1969, 57, 1408–1418. [CrossRef]
- [9] Frikel, M.; Bourennane, S. High-resolution methods without eigendecomposition for locating the acoustic sources. Appl. Acoust. 1997, 52, 139–154. [CrossRef]
- [10] Schmidt, R. Multiple emitter location and signal parameter estimation. IEEE Trans. Antennas Propagat. 1986, 34, 276–280. [CrossRef]
- [11] Suzuki, T. L1 generalized inverse beam-forming algorithm resolving coherent/incoherent, distributed and multipole sources. J. Sound Vib. 2011, 330, 5835–5851. [CrossRef]
- [12] Van Lancker, E. Acoustic Goniometry: A Spatio-Temporal Approach. Ph.D. Thesis, Ecole Polytechnique Fédérale de Lausanne, Lausanne, Switzerland, 2001.
- [13] Lardies, J.; Ma, H.; Berthillier, M. Source localization using a sparse representation of sensor measurements. In Proceedings of the Acoustics 2012 Conference, Nantes, France, 23 April 2012.
- [14] Zou, Y.X.; Li, B.; Ritz, C.H. Multi-Source DOA Estimation Using an Acoustic Vector Sensor Array Under a Spatial Sparse Representation Framework. Circuits Syst. Signal Process. 2016, 35, 993–1020. [CrossRef]
- [15] Malioutov, D.; Cetin, M.; Willsky, A. A sparse signal reconstruction perspective for source localization with sensor arrays. IEEE Trans. Signal Process. 2005, 53, 3010–3022. [CrossRef]
- [16] Cabell, R.; McSwain, R.; Grosveld, F. Measured Noise from Small Unmanned Aerial Vehicles. In Inter-Noise and Noise-Con Congress and Conference Proceedings; Institute of Noise Control Engineering: West Lafayette, IN, USA, 2016; Volume 252, pp. 345–354.
- [17] Kloet, N.; Watkins, S.; Clothier, R. Acoustic signature measurement of small multi-rotor unmanned aircraft systems. Int. J. Micro Air Veh. 2017, 9, 3–14. [CrossRef]
- [18] Djurek, I.; Petosic, A.; Grubesa, S.; Suhanek, M. Analysis of a Quadcopter's Acoustic Signature in Different Flight Regimes. IEEE Access 2020, 8, 10662–10670. [CrossRef]
- [19] Blanchard, T. Acoustic localization and tracking of a multi-rotor unmanned aerial vehicle using an array with few microphones. J. Acoust. Soc. Am. 2020, 148, 1456–1467. [CrossRef] [PubMed] (v seznamu uveden jen autor T. Blanchard; v textu citováno jako Blanchard et al.)
- [20] Yujie Gu.; Leshem, A. Robust Adaptive Beamforming Based on Interference Covariance Matrix Reconstruction and Steering Vector Estimation. IEEE Trans. Signal Process. 2012, 60, 3881–3885. [CrossRef]
- [21] Chen, P.; Yang, Y.; Wang, Y.; Ma, Y. Robust Adaptive Beamforming with Sensor Position Errors Using Weighted Subspace Fitting-Based Covariance Matrix Reconstruction. Sensors 2018, 18, 1476. [CrossRef] [PubMed]
- [22] Schroeder, M.R. Period Histogram and Product Spectrum: New Methods for Fundamental-Frequency Measurement. J. Acoust. Soc. Am. 1968, 43, 829–834. [CrossRef]
- [23] Shi, W.; Arabadjis, G.; Bishop, B.; Hill, P.; Plasse, R.; Yoder, J. Detecting, tracking, and identifying airborne threats with netted sensor fence. In Sensor Fusion; Thomas, C., Ed.; IntechOpen: Rijeka, Croatia, 2011; Chapter 8. [CrossRef]
- [24] Srour, N.; James, R. Remote Netted Acoustic Detection System: Final Report; Technical Report ARL-TR-706; US Army Research Laboratory: Adelphi, MD, USA, 1995.
- [25] Pham, T.; Sadler, B.; Fong, M.; Messer, D. High-resolution acoustic direction-finding algorithm to detect and track ground vehicles. In Ward Winning Papers, Proceedings of the Twentieth Army Science Conference, Norfolk, VI, USA, 24–27 June 1996; World Scientific: Singapore; Hackensack, NJ, USA; London, UK; Hong Kong, China, 1997; p. 16.
- [26] Zahorian, S.A.; Hu, H. A spectral/temporal method for robust fundamental frequency tracking. J. Acoust. Soc. Am. 2008, 123, 4559–4571. [CrossRef]
- [27] Goto, M. A robust predominant-F0 estimation method for real-time detection of melody and bass lines in CD recordings. In Proceedings of the 2000 IEEE International Conference on Acoustics, Speech, and Signal Processing (Cat. No.00CH37100), Istanbul, Turkey, 5–9 June 2000; Volume 2, pp. II757–II760. [CrossRef]
- [28] Grubeša, S.; Stamać, J.; Suhanek, M.; Petošić, A. Use of Genetic Algorithms for Design an FPGA-Integrated Acoustic Camera. Sensors 2022, 22, 2851. [CrossRef]
- [29] Le Courtois, F.; Thomas, J.H.; Poisson, F.; Pascal, J.C. Genetic optimisation of a plane array geometry for beamforming. Application to source localisation in a high speed train. J. Sound Vib. 2016, 371, 78–93. [CrossRef]
- [30] Aldeman, M.R. A hybrid spiral microphone array design for performance and portability. Appl. Acoust. 2020, 170, 107512. [CrossRef]
- [31] Tu, Q.; Chen, H. Array configuration optimization of first-order steerable differential arrays with minimum number of microphones. J. Acoust. Soc. Am. 2020, 148, 1732–1747. [CrossRef] [PubMed]
- [32] Liu, H.; Kirubarajan, T.; Xiao, Q. Arbitrary Microphone Array Optimization Method Based on TDOA for Specific Localization Scenarios. Sensors 2019, 19, 4326. [CrossRef] [PubMed]
- [33] Oppenheim, A.V.; Schafer, R.W. Discrete-Time Signal Processing; Prentice Hall Signal Processing Series; Prentice-Hall: Englewood Cliffs, NJ, USA, 1989.
- [34] Blanchard, T. Caractérisation de Drones en vue de leur Localisation et de leur Suivi à Partir d'une Antenne de Microphones (Characterization of Drones for Their Localization and Their Tracking from a Microphone Array). Ph.D. Thesis, Le Mans Université, Le Mans, France, 2019.
- [35] Bougaiov, N.; Danik, Y. Hough Transform for UAV's Acoustic Signals Detection. Adv. Sci. 2015, 6, 65–68. [CrossRef]
- [36] McCowan, I. Robust Speech Recognition Using Microphone Arrays. Ph.D. Thesis, Queensland University of Technology, Brisbane City, Australia, 2001. (zdroj metrik směrovosti: šířka hlavního laloku, MSL; s. 5)

## Kontrola (kolo 2)
- Opraveno: (1) fs = 20 kHz nebyla výslovně uvedena na s. 4 (jak stálo u simulací), je uvedena na s. 9 a v Tab. 1 na s. 12; u statických simulací (s. 7, 8) není uvedena; (2) "autoři explicitně zvažovali MVDR": PDF jmenuje jen "robust adaptive beamforming" [20] (s. 2), MVDR/Capon jako zamítnutá varianta jmenován není; (3) reference [5] doplněna o úplný název sborníku ("GRETSI 2019 - XXVIIème Colloque Francophone de Traitement du Signal et des Images", s. 18); (4) rozpor mezi textem s. 14 (azimut 2,7 až 4,2°, elevace 12,8 až 15,6°) a Tab. 2 s. 13 (azimut 2,3 až 2,9°, elevace 2,0 až 4,7°) potvrzen v PDF a popsán přesně, včetně toho, že neodpovídá ani azimut; (5) "šířka laloku v [38°, 60°] kolem trajektorie" zpřesněno podle citace "everywhere else and especially around the trajectory" (s. 6); (6) typ mikrofonu "není MEMS" označen jako interpretace (s. 13). Tab. 1, Tab. 2, rovnice (1)-(7), všechny parametry simulací a letu a všechny přímé citace byly proti PDF ověřeny a odpovídají (včetně stran).
- Doplněno: odkazy na jiné návrhy geometrie pole [28]-[32] (s. 3); detaily experimentu (spektrogram se čtyřmi rotory, konstantní vs. Q pásmo, s. 14); reference [13], [14], [23]-[25], [27], [29]-[31], [33], [35], [36] přepsané přesně z PDF (s. 17-18); financování včetně grantu Région; ověření Obr. 7 vizuálně (s. 9).
- Nejistoty: číslo sešitu (11) není v PDF, plyne z DOI; důvod rozporu text vs. Tab. 2 (s. 13, 14) není v článku vysvětlen; Obr. 7 je bodový graf bez čísel, prahy −6/10/−14/−16 dB lze brát jen z textu (text s. 9 je navíc nejasný: "from −14 dB for the TFR" bez uvedení počtu harmonických, interpretováno jako 5 harmonických); SNR v reálném letu a vítr/venkovní podmínky v PDF nejsou uvedeny (SNR 5 dB je jen v simulaci); nejistota GPS reference cca 3 m (s. 14) je srovnatelná s krátkou trajektorií (3 až 4 m od pole), proto absolutní chyby letu mají omezenou vypovídací hodnotu (interpretace); v seznamu literatury je u [19] uveden jen autor T. Blanchard, v textu citováno jako Blanchard et al.
