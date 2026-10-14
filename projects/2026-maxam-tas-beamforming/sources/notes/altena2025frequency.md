# Altena et al. 2025: analýza frekvenčních pásem a lokalizace dronů polem 64 mikrofonů

Pozn. k číslování stran: [s. 024802-N] = číslo strany článku v JASA-EL (patička "5, 024802-N"); odpovídající PDF strana = N + 1 (PDF 1 je titulní/"View Online" strana; ověřeno, článek má 7 stran = PDF 2 až 8). Obr. 1-4 jsou na -5 a -6, Tab. 1 na -2, Tab. 2 na -4.

## Bibliografie
- Altena, A.; Snellen, M.; Luesutthiviboon, S.; de Croon, G.; Voskuijl, M. "Frequency band analysis and comparison of localisation techniques for drones using microphone array measurements." *JASA Express Letters* 5(2), 024802 (2025). DOI: 10.1121/10.0035915. Received 18 Oct 2024, accepted 31 Jan 2025, published online 14 Feb 2025. Editor: Wen Xu. Citace v PDF: "JASA Express Lett. 5, 024802 (2025)", záhlaví "5 (2)" (číslo sešitu 2 je tedy v PDF uvedeno). © 2025 Author(s). Dostupnost dat: jen na rozumnou žádost u korespondenčního autora. [s. 024802-1, -7]
- Pracoviště: Faculty of Aerospace Engineering, Delft University of Technology, Delft; Faculty of Military Sciences, Netherlands Defence Academy, Den Helder (M. Voskuijl). [s. 024802-1]
- Vydavatel: Acoustical Society of America (podle záhlaví "asa.scitation.org/journal/jel"; AIP Publishing výslovně neuvedeno). MDPI: ne. Open access: ano, licence CC BY (https://creativecommons.org/licenses/by/4.0/). Financování: Dutch Ministry of Defence, projekt ACTION (NTP N21/005). [s. 024802-1, -7]
- Návrh BibTeX:
```bibtex
@article{altena2025frequency,
  author  = {Altena, Anique and Snellen, Mirjam and Luesutthiviboon, Salil and de Croon, Guido and Voskuijl, Mark},
  title   = {Frequency band analysis and comparison of localisation techniques for drones using microphone array measurements},
  journal = {JASA Express Letters},
  year    = {2025},
  volume  = {5},
  number  = {2},
  pages   = {024802},
  doi     = {10.1121/10.0035915}
}
```

## Problém a přínos
- Cíl 1: porovnat gridové frekvenční beamformování (konvenční, tedy DAS/Bartlett) s negridovým přístupem differential evolution (DE) pro odhad azimutu, elevace a vzdálenosti dronu. [s. 024802-2]
- Cíl 2: ověřit, že lokalizace v úzkých frekvenčních pásmech, jejichž souhrn pokrývá široký rozsah, zvětšuje dosah a odliší více zdrojů (dron vs. letadlo). [s. 024802-2]
- Cíl 3: reálný odhad limitu dosahu pro šest různých dronů. [s. 024802-2]
- Soustředění: pokročilé zpracování signálu jedním polem ("second strategy"), ne triangulace více poli ("first strategy"). [s. 024802-1]
- Autoři kritizují dřívější práce: Blanchard et al. [9] použili time-domain beamforming (nevyužívá frekvenční obsah dronu), některé práce volí široký frekvenční rozsah [6, 11], dvě práce netestují limity metody nebo měří jen uvnitř [9, 10], vícezdrojová lokalizace [5, 8] vyžaduje další algoritmus a předpoklad počtu zdrojů. [s. 024802-1]
- Abstrakt: DE (grid-free) umí odhadnout 3D polohu dronu v blízkém poli; ve větší vzdálenosti dává stále úhlovou polohu. [s. 024802-1]

## Mikrofonní pole a hardware
- 64 mikrofonů PUI Audio 665-POM-2735P-R, uspořádání Underbrink spiral, rozměr 4 × 4 m, rozteče mikrofonů 0,11 až 3,4 m. [s. 024802-2]
- Pole leží na zemi, rovina pole rovnoběžná se zemí; počátek gridu ve středu pole. [s. 024802-3]
- Absorpční pěna (proti odrazům) a větrné ochrany (proti větru). [s. 024802-2]
- Vzorkovací frekvence 50 000 Hz. Bitová hloubka, způsob akvizice, synchronizace a kalibrace: neuvedeno. [s. 024802-2]
- Průměrná hladina pozadí 52 dB (1. kampaň) a 51 dB (2. kampaň); referenční hodnota a vážení neuvedeno. Podmínky: větrné ochrany a pěna, vítr 1-2 / 5-7 / 6 m/s podle dronu (Tab. 1). [s. 024802-2]
- Dvě kampaně: vojenská základna "Luitenant-Generaal Bestkazerne" a "Unmanned Valley" (Nizozemsko). [s. 024802-2]

## Signálový model a rovnice
- CSM (1): $\mathbf{C}=E[\mathbf{p}\mathbf{p}^H]$, v práci počítána z jediného rámce (kvůli nestacionaritě dronu a nutnému frekvenčnímu rozlišení). [s. 024802-3]
- Steering vektor (2): $g_{n,j}(f)=e^{-2\pi i f r_{n,j}/c}$. [s. 024802-3]
- Vzdálenost mikrofon-bod gridu (3): $r_{n,j}=r_j\sqrt{1+\left(\frac{r_n}{r_j}\right)^2-\frac{2}{r_j}\left(x_n\cos\phi\cos\theta+y_n\sin\phi\cos\theta+z_n\sin\theta\right)}$. [s. 024802-3]
- Výstup beamformeru (4): $B_j(f)=\dfrac{\mathbf{g}_j^H\mathbf{C}\mathbf{g}_j}{\|\mathbf{g}_j\|^4}$ (normalizace podle Merino-Martínez et al. [12], nezávislost na počtu mikrofonů). [s. 024802-3]
- Cost funkce DE (5): $E_{Bartlett}(f)=-B_j(f)$ (Bartlettova cost funkce, DE minimalizuje). [s. 024802-4]
- Dalekopolní hranice: podle Fresnelovy vzdálenosti [20] (A. Williams, "The piston source at high frequencies", JASA 1951) je dron v dalekém poli od 40,4 m (vzorec ani parametry v textu neuvedeny). [s. 024802-6]
- Poznámka k Eq. (3): v PDF je člen 2/r_j před závorkou se souřadnicemi mikrofonu (tak je to přepsáno výše); r_n je vzdálenost počátku od mikrofonu n, r_j vzdálenost počátku od bodu gridu j. [s. 024802-3]

## Algoritmus
- Gridový beamforming: azimut φ 0° až 360° po 1°, elevace θ 0° až 90° nad zemí po 0,5°. [s. 024802-3]
- Frekvenční pásma: 12 pásem šířky 200 Hz: [0-200, 200-400, ..., 2000-2200, 2200-2400] Hz. [s. 024802-3]
- V každém pásmu se vyberou dvě frekvence s nejvyšší součtovou úrovní přes všechny mikrofony (po FFT); beamformování na obou, výstupy se průměrují nekoherentně; maximum gridu = poloha dronu pro dané pásmo. Počet dvou frekvencí zvolen po testování kvůli kompromisu výkon vs. doba běhu. [s. 024802-3]
- Blok signálu 2500 vzorků s překryvem 50 % → časový krok 25 ms (při 50 kHz: 50 ms blok, interpretace z fs). [s. 024802-3]
- Vzdálenost zdroje r_j v gridu není známa a nehraje roli (dalekopolní rovinné vlny): testovány hodnoty rj od 0 do 200 "ms" (v PDF doslova "ms", i ve vykresleném textu; interpretace: překlep za m), rozdíl ve výsledku "negligible". [s. 024802-3]
- DE: populace 12 kandidátů, 100 generací; prostor řešení azimut 0° až 360°, elevace 0° až 90°, vzdálenost 0 až 200 m (interpretace: odhady vzdálenosti se tedy v Obr. 1-4 shlukují u horní meze 200 m, kde nemají význam); dva problémy: (a) jen úhly (DEA), (b) úhly + vzdálenost (DEAR). [s. 024802-4]
- Vyšší rozlišení metody (MVDR, MUSIC aj.) záměrně nepoužita: "this study uses the conventional frequency-domain variant due to computational efficiency". Počet zdrojů: odhad jednoho (nejvyšší maximum); více zdrojů odlišeno jen přes pásma. [s. 024802-3, -6]
- Regularizace, váhování mikrofonů, okno (Hann apod.): neuvedeno.

## Experiment / simulace
- Šest dronů (pět quadkoptér, jeden elektrický VTOL quadplane), 7 letů (dron 1 má dva lety), pro každý 45 s segment přeletu nad polem. Quadkoptéry v 30 m výšce, quadplane v 60 m. Ground truth: GPS dronu ("drone global positioning system"); přesnost GPS neuvedena. [s. 024802-2, -4]
- Tab. 1 [s. 024802-2]:

| Dron | Typ | Hmotnost při vzletu (g) | BPF (Hz) | Rychlost (m/s) | Vítr (m/s) |
|---|---|---|---|---|---|
| Drone 1 (dva lety) | DJI Mavic 3 | 895 | 153 | 10 | 1-2 |
| Drone 2 | Autel EVO II | 1191 | 177 | 10 | 1-2 |
| Drone 3 | DJI Mini 2 | 242 | 354 | 5 | 1-2 |
| Drone 4 | DJI Phantom 3 | 1216 | 171 | 10 | 5-7 |
| Drone 5 | DJI Phantom 4 | 1380 | 159 | 5 | 5-7 |
| Drone 6 | Avy Aera (Quadplane) | 18 250 | 225 | 21 | 6 |

- BPF určena z PSD v okamžiku nejvyšší hladiny zvuku. [s. 024802-2]
- Druhý let dronu 1 a let dronu 6 obsahují přelet letadla (rušivý zdroj). [s. 024802-2]
- Obr. 1-4 [s. 024802-5, -6]: časový průběh odhadů azimutu φ, elevace θ a vzdálenosti r pomocí DE (tečky) proti GPS dronu (červená čárkovaná). Obr. 1/2: dron 1, let 2 (osa času 15 až 60 s, nejbližší bod přibližně 38 s, vzdálenost přibližně 35 až 40 m, elevace v maximu přibližně 85°); Obr. 3/4: dron 6 (120 až 165 s, nejbližší bod přibližně 143 s, vzdálenost přibližně 70 až 100 m, elevace v maximu přibližně 65°). Odečet z grafu, orientační. V pásmu 200-400 Hz (Obr. 1, 3) se odhady drží u letadla a dron je lokalizován jen blízko pole; v pásmech 1000-1200 Hz (Obr. 2) a 1800-2000 Hz (Obr. 4) odhady úhlů přesně sledují GPS a letadlo není vidět; odhady r jsou správné jen krátce u nejbližšího bodu (Obr. 2, přibližně 40 m). Lepší odhady zejména v intervalech 40 až 45 s (dron 1) a 145 až 155 s (dron 6); letadlo ruší u dronu 6 hlavně 120 až 140 s.
- SNR: nevyhodnocováno (SNR jako proměnná neuvedeno).
- Kritérium dosahu: největší vzdálenost, při které se azimut a elevace odhadnou "correctly" ve shodě s GPS dronu; číselná tolerance neuvedena. [s. 024802-4]
- Výpočetní stroj: stolní PC, 64 GB RAM, Intel Xeon E5-1620 v3. [s. 024802-4]

## Výsledky (čísla)
Tab. 2, maximální dosažený dosah [s. 024802-4] (v legendě "B" = beamforming, v tabulce je značeno "BF"; DEA = DE jen úhly, DEAR = DE úhly + vzdálenost, DE = obě varianty DE; "Max. distance" = největší vzdálenost, kde je úhel odhadnut správně vůči GPS, maximum přes všechny techniky a pásma):

| Let | Max. vzdálenost azimut (m) | Technika | Pásmo (Hz) | Max. vzdálenost elevace (m) | Technika | Pásmo (Hz) |
|---|---|---|---|---|---|---|
| Drone 1 Flight 1 | 93 | DEAR | 200-400 | 92 | BF | 200-400 |
| Drone 1 Flight 2 | 110 | DEA | 1000-1200 | 109 | DEA | 1400-1600 |
| Drone 2 | 126 | DEA | 1400-1600 | 126 | DEA | 1400-1600 |
| Drone 3 | 103 | DE | 1000-1200 | 103 | DE | 1000-1200 |
| Drone 4 | 133 | BF | 600-800 | 102 | BF | 400-600 |
| Drone 5 | 108 | BF | 1000-1200 | 107 | DEA | 800-1000 |
| Drone 6 | 360 | BF | 1800-2000 | 360 | BF | 1800-2000 |

- Beamforming dosáhl nejlepší výkon 6krát, DE 8krát (z 14 hodnot); "Not one technique outperforms the other". [s. 024802-4]
- Doba běhu: všechny tři přístupy přibližně 9 min na stolním PC ("basic desktop computer", rozdíl minimální; (interpretace) množství zpracovaných dat článek neuvádí, nejde tedy o dobu na jeden rámec); lze zkrátit optimalizací počtu generací (DE), optimalizací maticových operací a hrubším gridem (beamforming, na úkor přesnosti). [s. 024802-4]
- Rozsah dosahů: "from several tens of meters to hundreds of meters". [s. 024802-7]
- DE s vzdáleností neposkytuje horší úhlové odhady než DE bez vzdálenosti. [s. 024802-4]
- Odhad vzdálenosti (DE) správný jen v blízkém poli: v Obr. 2 krátce kolem 40 m (nejbližší místo přeletu); dalekopolní od 40,4 m (Fresnelova vzdálenost). [s. 024802-6]
- Frekvenční pásma: pásmo 200 až 400 Hz u druhého letu dronu 1 a letu dronu 6 je dominováno letadlem; pásma 1000 až 1200 Hz (dron 1, let 2) a 1800 až 2000 Hz (dron 6) letadlo zcela eliminují a dávají lepší odhady. [s. 024802-5]
- Pozn. (interpretace): pásma s maximálním dosahem často obsahují vyšší harmonické BPF (např. dron 3: BPF 354 Hz a pásmo 1000-1200 Hz obsahuje 3. harmonickou 1062 Hz; dron 2: 177 Hz a pásmo 1400-1600 Hz obsahuje 8. harmonickou 1416 Hz). Autoři to takto netvrdí; uvádějí jen, že není předem známo, v kterém pásmu je BPF nebo harmonická.

## Omezení podle autorů
- Akustická lokalizace má obecně menší dosah; dále klesá při nižším SNR. [s. 024802-1]
- Rychlost dronu předpokládána konstantní, ve skutečnosti ne, BPF se mění. [s. 024802-4, -5]
- Vzdálenost lze odhadnout jen v blízkém poli; v dalekém poli (rovinné vlny) nelze. [s. 024802-2, -6]
- Aproximace dalekého pole; doba běhu přibližně 9 min je neoptimalizovaná. [s. 024802-4]
- Pro mnoho aplikací bude potřeba distribuovaná síť polí pro včasnou/souvislou lokalizaci. [s. 024802-7]
- Jeden let na dron (kromě dronu 1), celkem 7 letů; data dostupná jen na žádost. [s. 024802-2, -7]

## Relevance pro náš projekt
- SOTA sekce: reálná venkovní data šesti dronů s GPS ground truth a konvenční (DAS) frekvenční beamformování jako výkonná baseline. Podporuje (interpretace) tvrzení, že DAS na úzkých pásmech je pro dron dostačující; autoři sami uvádějí jen, že DE a beamforming dávají srovnatelný dosah při srovnatelné době běhu (obě přibližně 9 min).
- Volba pásma: nejlepší pásma ležela mezi 200 a 2000 Hz (Tab. 2), u pěti ze sedmi letů v rozsahu 400 až 1600 Hz, u dronu 6 až 1800-2000 Hz. Horní mez analyzovaných pásem je 2,4 kHz; autoři očekávají, že na vyšších frekvencích je lokalizace omezena atmosférickým útlumem [s. 024802-3]. To podporuje naše pásmo 300 Hz až 2 kHz. Pásmo 200-400 Hz bylo citlivé na hluk letadla (rušení nízkými frekvencemi). Doporučení (interpretace): zvážit pásmově-selektivní výběr (např. dvě nejsilnější frekvence v několika 200 Hz pásmech) místo širokopásmového součtu.
- Jednosnímková CSM (jedno okno, bez průměrování): v článku se CSM počítá z jednoho bloku 2500 vzorků při 50 kHz (50 ms, vlastní dopočet z fs), tedy podobně krátký rámec jako náš 100 ms úsek. Pro MVDR/MUSIC to znamená singulární/špatně podmíněnou CSM (interpretace); DAS je vůči tomu robustní.
- Geometrie 2×8 UCA: nepřenositelné; 64 mikrofonů, 4 × 4 m, spirála, rozteče až 3,4 m (naše apertura 160-250 mm, tj. mnohem nižší rozlišení v azimutu, interpretace).
- Volba metody: grid azimut/elevace (1° a 0,5°, polokoule) lze převzít jako referenční parametrizaci pro DAS; DE (globální optimalizace) je zajímavé jako alternativa k hrubému gridu pro MCU (interpretace), ale doba 100 generací × 12 kandidátů není na MCU zadarmo.
- Výpočetní nároky: 9 min na PC (rozsah zpracovaných dat neuveden; interpretace: pravděpodobně offline zpracování 45 s přeletů), ne pro MCU. Nepřenositelné.
- Nelze přenést: dosahy 93 až 360 m jsou pro velkou apertuře a 64 mikrofonů; naše použití je blízké a s jiným hardwarem.

## Citovatelná tvrzení
- Dron je v dalekém poli: "A drone is usually flying in the far-field relative to the microphone array. Therefore, the sound waves arriving at the array are planar waves. Due to this, the distance cannot be estimated." [s. 024802-2]
- Zdůvodnění konvenčního beamformingu: "this study uses the conventional frequency-domain variant due to computational efficiency." [s. 024802-3]
- Pásmový přístup: "In each band, the two frequencies with the summed highest level over all microphones after the Fourier transform are selected for beamforming." [s. 024802-3]
- Parametry zpracování: "individual signal block of 2500 samples with overlap of 50%, leading to a time step of 25 ms." [s. 024802-3]
- Jedna CSM snímek: "due to the non-stationarity of the drones, as well as the requirement to have sufficient frequency resolution, in this work, the CSM is computed using a single frame." [s. 024802-3]
- Výsledek srovnání: "Table 2 clearly shows that beamforming and DE perform similar. Not one technique outperforms the other." [s. 024802-4]
- Hluk letadla v nízkých pásmech: "the aircraft flyover dominates the estimates." (pásmo 200-400 Hz) [s. 024802-5]
- Význam pásem: "it is essential to cut the frequency-domain into smaller bands while ensuring that these bands jointly span a large range of frequencies to discriminate between drones and background noise and to account for the variation in sound signatures among different drones." [s. 024802-7]
- Dosahy: "localisation ranges are found to range from several tens of meters to hundreds of meters." [s. 024802-7]
- Akustika jako doplněk: "Acoustic sensors, like microphone arrays, are lightweight, cheap, and are able to sense in all directions." [s. 024802-1]

## Relevantní reference z článku
- 1 I. Güvenc, F. Koohifar, S. Singh, M. L. Sichitiu, and D. Matolak, "Detection, tracking, and interdiction for amateur drones," IEEE Commun. Mag. 56, 75-81 (2018). (v PDF je znak ü zkomolen v textové vrstvě)
- 2 S. Al-Emadi, A. Al-Ali, and A. Al-Ali, "Audio-based drone detection and identification using deep learning techniques with dataset enhancement through generative adversarial networks," Sensors 21, 4953 (2021).
- 3 E. E. Case, A. M. Zelnio, and B. D. Rigling, "Low-cost acoustic array for small UAV detection and tracking," in 2008 IEEE National Aerospace and Electronics Conference (2008), pp. 110-113.
- 4 A. Wilson, A. Jha, A. Kumar, and L. R. Cenkeramaddi, "Estimation of number of unmanned aerial vehicles in a scene utilizing acoustic signatures and machine learning," J. Acoust. Soc. Am. 154, 533-546 (2023).
- 5 M. Blass, S. Grebien, and F. Graf, "Experimental evaluation of acoustic drone tracking using mobile microphone arrays," in Quiet Drones 2024 (2024).
- 6 S. Ding, X. Guo, T. Peng, X. Huang, and X. Hong, "Drone detection and tracking system based on fused acoustical and optical approaches," Adv. Intelligent Syst. 5, 2300251 (2023).
- 7 G. Herold and E. Sarradj, "Microphone array based trajectory reconstruction for small UAV fly-by measurements," in AIAA AVIATION 2023 Forum (2023).
- 8 M. Blass, A. Maly, F. Graf, P. Wellig, and B. Ott, "Towards mobile microphone array based UAV tracking," in Quiet Drones 2022 - International e-Symposium on UAV/UAS Noise (INCE/Europe, Paris, 2022), pp. 1-18.
- 9 T. Blanchard, J.-H. Thomas, and K. Raoof, "Acoustic localization and tracking of a multi-rotor unmanned aerial vehicle using an array with few microphones," J. Acoust. Soc. Am. 148, 1456-1467 (2020).
- 10 G. Herold, A. Kujawski, C. Strümpfel, S. Huschbeck, M. U. de Haag, and E. Sarradj, "Flight path tracking and acoustic signature separation of swarm quadcopter drones using microphone array measurements," in Quiet Drones 2020 - International e-Symposium on UAV/UAS Noise (INCE/Europe, Paris, 2020), pp. 1-19.
- 11 M. Blass and F. Graf, "A real-time system for joint acoustic detection and localization of uavs," in Quiet Drones 2020 - International e-Symposium on UAV/UAS Noise (INCE/Europe, Paris, 2020), pp. 1-18.
- 12 R. Merino-Martínez, P. Sijtsma, M. Snellen, T. Ahlefeldt, J. Antoni, C. Bahr, D. Blacodon, D. Ernst, A. Finez, S. Funke, T. Geyer, S. Haxter, G. Herold, X. Huang, W. Humphreys, Q. Leclère, A. Malgoezar, U. Michel, T. Padois, A. Pereira, C. Picard, E. Sarradj, H. Siller, D. Simons, and C. Spehr, "A review of acoustic imaging methods using phased microphone arrays," CEAS Aeronaut. J. 10, 197-230 (2019).
- 13 A. M. N. Malgoezar, M. Snellen, and R. Merino-Martinez, "On the use of global optimization methods for acoustic source mapping," J. Acoust. Soc. Am. 141, 453-465 (2017).
- 14 B. von den Hoff, R. Merino-Martínez, D. G. Simons, and M. Snellen, "Using global optimization methods for three-dimensional localization and quantification of incoherent acoustic sources," JASA Express Lett. 2, 054802 (2022).
- 15 A. Altena, S. Luesutthiviboon, G. D. Croon, M. Snellen, and M. Voskuijl, "Comparison of acoustic localisation techniques for drone position estimation using real-world experimental data," in Proceedings of the 29th International Congress on Sound and Vibration (IIAV, Prague, Czech Republic, 2023), pp. 1-8.
- 16 James R. Underbrink, "Circularly symmetric, zero redundancy, planar array having broad frequency range applications," U.S. patent 6,205,224 B1 (March 20, 2001).
- 17 A. Vieira, M. Snellen, and D. G. Simons, "Experimental assessment of sound quality metrics for takeoff and landing aircraft," AIAA J. 59(1), 240-249 (2021).
- 18 B. D. V. Veen and K. M. Buckley, "Beamforming: A versatile approach to spatial filtering," IEEE ASSP Mag. 5, 4-24 (1988).
- 19 R. Storn and K. Price, "Differential evolution - A simple and efficient heuristic for global optimization over continuous spaces," J. Global Optim. 11, 341-359 (1997).
- 20 A. Williams, "The piston source at high frequencies," J. Acoust. Soc. Am. 23, 1-6 (1951). (zdroj Fresnelovy vzdálenosti; autor [18] je v PDF zapsán "B. D. V. Veen", tak je to přepsáno)
- (V seznamu literatury nejsou uvedena DOI.)

## Kontrola (kolo 2)
- Opraveno: (1) Tab. 2 legenda: v legendě "B", v tabulce "BF" (poznámka doplněna) [s. -4]; (2) "rj od 0 do 200 ms": potvrzeno z vykresleného PDF, že tam skutečně stojí "ms" (překlep) [s. -3]; (3) tvrzení "DAS je dostačující a výpočetně levný" a "9 min je pro celé přelety" označena jako interpretace [s. -4]; (4) "hmotnost" v Tab. 1 přesněji "takeoff weight"; (5) číslo sešitu 2 je v PDF uvedeno v záhlaví ("5 (2)"), nikoli jen odvozeno; (6) ověřeno: Tab. 1 a 2 přepsány správně (všechna čísla), počty 6 vs 8 lepších výsledků (BF 3+3, DE 4+4), výběr pásem, eq. 1-5, parametry DE (12 kandidátů, 100 generací), citáty (s. -1, -2, -3, -4, -5, -7) jsou doslovné, strany odpovídají (PDF = N + 1).
- Doplněno: obsah Obr. 1-4 (orientační odečet z grafů); kritika předchozích prací z úvodu; abstrakt o blízkém poli; interpretace omezení vzdálenosti 200 m u DE; reference [2], [17], [20] (Fresnel); přesnější afiliace; poznámka o dostupnosti dat.
- Nejistoty: bitová hloubka, kalibrace mikrofonů, synchronizace, SNR, tolerance pro "correctly" odhadnutý úhel a přesnost GPS nejsou uvedeny; vzorec Fresnelovy vzdálenosti a parametry (40,4 m) nejsou v článku; "ms" vs "m" jen interpretace; číselné hodnoty z Obr. 1-4 jsou orientační; vydavatel (ASA/AIP) je odvozen jen ze záhlaví "asa.scitation.org/journal/jel".
