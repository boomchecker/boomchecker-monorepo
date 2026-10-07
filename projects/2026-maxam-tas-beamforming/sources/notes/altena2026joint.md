# Altena et al. 2026: Společná lokalizace dronů a pozaďových zdrojů (beamforming po jednotlivých frekvencích)

## Bibliografie
- Plná citace (podle PDF, s. 1): Anique Altena, Mirjam Snellen, Salil Luesutthiviboon, Guido de Croon, Mark Voskuijl. "Joint sound source localisation of drones and other background sources". Applied Acoustics, vol. 254, article 111474, 2026. DOI: 10.1016/j.apacoust.2026.111474. Received 20 October 2025; revised 15 January 2026; accepted 12 July 2026; available online 22 July 2026 [s. 1]. PDF má 7 stran, číslování stran PDF odpovídá číslování 1-7 v záhlaví.
- Afiliace: Faculty of Aerospace Engineering, Delft University of Technology (Delft); Faculty of Military Sciences, Netherlands Defence Academy (Den Helder) [s. 1]. Korespondenční autorka: A. Altena. Financování: Dutch Ministry of Defence, projekt ACTION (ACoustic detecTION of class I (<20 kg) UAS supported by optical sensors), NTP N21/005 [s. 7]. Data: "available on request" [s. 7].
- Vydavatel: Elsevier Ltd. (©2026 The Authors). MDPI: ne. Open access: ano (CC BY 4.0, "This is an open access article under the CC BY license") [s. 1]. ISSN 0003-682X.
- Návrh BibTeX:
```bibtex
@article{altena2026joint,
  author  = {Altena, Anique and Snellen, Mirjam and Luesutthiviboon, Salil and de Croon, Guido and Voskuijl, Mark},
  title   = {Joint sound source localisation of drones and other background sources},
  journal = {Applied Acoustics},
  volume  = {254},
  pages   = {111474},
  year    = {2026},
  publisher = {Elsevier},
  doi     = {10.1016/j.apacoust.2026.111474}
}
```

## Související práce (Sec. 1, s. 1-2)
- Busset [2]: sférické pole, jednotlivé drony (3 typy), vícedronový provoz jen předpoklad; Blanchard [3]: časový delay-and-sum + hledání maxim, 2 zdroje v simulaci; Wu [4]: ResNet s attention modulem + beamforming, 2 identické drony, simulace s ego-noise z datasetu DREGON [5]; Herold [6]: functional beamforming + lokální maxima, anechoická komora, max. 4 drony; Blass [7]: SRP + GMM + delay-and-sum, venku 2 kvadrokoptéry na téže dráze, nerozlišeny kvůli podobnému zvuku. Autoři tvrdí, že dosud nebyla publikována studie lokalizující více dronů i pozaďové zdroje v reálném nekontrolovaném prostředí [s. 2].

## Problém a přínos
- Problém: akustická lokalizace dronů, zvláště při více zdrojích (více dronů, letadla, hluk závodní dráhy); rozlišit zdroje ve spektrogramu je obtížné, protože zdroje vyzařují na podobných frekvencích [s. 1].
- Přínos (novinka): frekvenčně doménový beamforming na jednotlivých frekvencích v pásmu 140 až 1200 Hz; pro každou frekvenci a časový krok se uloží maximum výstupu v azimutu a elevaci; trajektorie zdrojů se pak oddělí z "mraku" bodů a ke každé se dohledají dominantní frekvence [s. 2-3].
- Validace na reálném venkovním měření 64mikrofonovým polem se 7 drony a pozadím (letadlo, závodní dráha), GPS referencí [s. 2, 4-6].
- Dosah: kvadrokoptéry do 712 m (azimut) a 593 m (elevace), pevná křídla do 317 m (azimut) a 305 m (elevace) [s. 1, Highlights/Abstract]. Kontrola proti Tab. 3 [s. 6]: 712 m azimut je Mavic Pro v Meas. 2 (tentýž dron v Meas. 2 má elevaci jen 582 m); 593 m elevace je Mavic Pro v Meas. 1 (jiné měření, tam azimut 593 m), tedy abstrakt kombinuje maxima z různých měření; stejný dron/měření nikdy nedává dvojici 712/593. NESOULAD ZDROJE: 305 m "pro pevné křídlo v elevaci" nemá v Tab. 3 oporu; nejvyšší elevace pevných křídel v Tab. 3 je 290 m (Parrot Disco, Meas. 3), zatímco 305 m je DJI Phantom 4 (kvadrokoptéra) v Meas. 1. Při citaci pevných křídel uvádět 317 m (azimut) a 290 m (elevace, podle Tab. 3), případně obě čísla s poznámkou.

## Mikrofonní pole a hardware
- 64 mikrofonů PUI Audio 665-POM-2735P-R, planární pole v Underbrinkově rozložení (refs [13,14]), průměr 3.4 m, rozteče od 0.11 do 3.4 m [s. 2].
- Pole leží na zemi (tráva, vedle nepoužívané ranveje) na akustickém absorbéru (pěna) proti odrazům od země; mikrofony mají větrné kryty [s. 2].
- fs = 50 kHz [s. 2]. Bit depth, akvizice (typ DAQ), synchronizace, kalibrace, SNR, okno FFT, způsob odhadu CSM (počet průměrovaných bloků pro $\mathbb E[\cdot]$), přesnost GPS, počasí, rychlost větru, teplota: v článku neuvedeno (ověřeno hledáním v celém textu).
- Drony (Tab. 1, s. 2): viz tabulka níže. GPS: externí komerční GPS senzor na většině dronů, bez vylepšené GPS metody [s. 2].

| Dron | Typ | Vzletová hmotnost [g] | Průměr vrtule |
|---|---|---|---|
| DJI Matrice 300 | kvadrokoptéra | 6300 | 21 in |
| DJI Mavic 2 Enterprise | kvadrokoptéra | 899 | 8.7 in |
| DJI Mavic Pro | kvadrokoptéra | 734 | 8.3 in |
| DJI Phantom 4 | kvadrokoptéra | 1380 | 9.4 in |
| Believer UAV | pevné křídlo | 4200 | 11 in |
| Funjet | pevné křídlo | 875 | 6 in |
| Parrot Disco | pevné křídlo | 750 | 8 in |

## Signálový model a rovnice
- (1) [s. 3] výstup beamformeru (konvenční, frekvenční doména): $B_j(f)=\dfrac{\mathbf g_j^H \mathbf C\,\mathbf g_j}{\|\mathbf g_j\|^4}$, kde $j$ je bod mřížky (azimut $\phi$, elevace $\theta$), $\mathbf g$ steering vektor, $\mathbf C$ cross-spectral matrix (CSM).
- (2) [s. 3]: $\mathbf C=\mathbb E[\mathbf p\mathbf p^H]$, $\mathbf p$ je vektor tlaků po FFT.
- (3) [s. 3] steering vektor: $g_{n,j}(f)=e^{-2\pi i f r_{n,j}/c}$, $r_{n,j}$ vzdálenost mikrofonu $n$ od bodu mřížky $j$; předpoklad far-field (rovinná vlna), takže absolutní vzdálenost ke středu pole nehraje roli (odkaz na [12]); $c$ = rychlost zvuku (hodnota neuvedena). Beamformer je normován $\|\mathbf g\|^4$, tj. $\mathbf g^H\mathbf C\mathbf g/\|\mathbf g\|^4$ (v PDF skutečně 4. mocnina).
- (4) [s. 3] Rayleighovo kritérium pro šířku řízeného laloku (ref [17]): $\theta_{B,s}=1.22\,\dfrac{\lambda}{L\sin(\theta_s)}$, $L$ apertura pole, $\theta_s$ úhel řízení (0 stupňů = horizontála, 90 stupňů = nad polem); Eq. (4) je na s. 3 (na s. 5 se na ni autoři odvolávají). Efektivní apertura $L\sin\theta_s$ klesá u nízkých elevací, takže elevační odhady jsou horší pod 20 až 30 stupňů [s. 3].

## Algoritmus
- Krok 1, beamforming jednotlivých frekvencí [s. 3]: FFT blok 2500 vzorků, zero-padding na 8192, překryv 50 %; frekvenční rozlišení 6.1 Hz; 174 frekvencí v pásmu 140-1200 Hz vyhodnoceno v každém časovém kroku. Mřížka: azimut 0-360 stupňů, elevace 0 (horizontála) až 90 stupňů (nad polem), krok 1 stupeň obě osy, úhly vůči středu pole; jemná mřížka zvolena, aby šla srovnat s kontinuálním GPS. Pro každou frekvenci se uloží pouze globální maximum (lokální maxima se nepoužívají). Autoři uvádějí, že použití mnoha frekvencí kompenzuje případné přehlédnutí slabších zdrojů a vyhne se záměně postranních laloků za zdroj [s. 3].
- Odvozeno (interpretace, vlastní výpočet): blok 2500 vzorků při 50 kHz = 50 ms, krok bloku 1250 vzorků = 25 ms; 50000/8192 ≈ 6.1 Hz sedí.
- Zdůvodnění pásma [s. 2]: pod 140 Hz znečišťuje měření vítr; horní limit 1200 Hz = frekvence, do které je dominantní hluk dronu typicky měřitelný polem a atmosférická absorpce je relativně nízká (ref [12]). V předchozí práci [12]: 12 pásem po 200 Hz, v každém 2 frekvence [s. 2-3].
- Krok 2, separace trajektorií [s. 3]: (1) bod patří ke zdroji, pokud leží do 2 stupňů od azimutových bodů zdroje za posledních 0.5 s; (2) při nejednoznačnosti stejné kritérium v elevaci (2 stupně, 0.5 s); (3) při dalším nerozhodnutí se bod přidá k nejdelšímu zdroji; počet zdrojů není omezen. Post-processing: ponechány pouze zdroje s více než 200 body v azimutu. Azimut se vyhodnocuje první, protože elevace degraduje pod 20 až 30 stupňů [s. 3].
- Krok 3: frekvenční profil každého zdroje (dominantní frekvence v 140-1200 Hz) z frekvencí bodů zdroje [s. 3].
- Žádné filtrování mezi beamformingem a separací (zavrhnuto, protože odstraňuje i body skutečných zdrojů na začátku a konci detekce) [s. 4].
- Regularizace, váhování, MVDR, MUSIC, GCC-PHAT: v tomto článku nepoužito (jen konvenční beamformer).

## Experiment / simulace
- Místo: nizozemská vojenská základna "Luitenant-Generaal Bestkazerne", otevřená travnatá plocha vedle nepoužívané ranveje [s. 2]. Měření nebylo navrženo pro akustiku ("uncontrolled measurement campaign"), vždy létalo více dronů (kombinace pevných křídel a kvadrokoptér) v různých vzdálenostech [s. 2].
- Tři měření, každé 5 minut [s. 4]:

| Měření | Drony | Pozadí | Poznámka |
|---|---|---|---|
| 1 (clear) | Parrot Disco (FW), Funjet (FW), DJI Phantom 4 (QC) | hluk závodní dráhy; přelet letadla 0-80 s | [s. 4, Tab. 2] |
| 2 (clear) | Parrot Disco (FW), Funjet (FW), DJI Mavic Pro (QC), DJI Matrice 300 (QC) | hluk závodní dráhy; přelet letadla 150-250 s | [s. 4, Tab. 2] |
| 3 (challenging) | Believer UAV (FW), Parrot Disco (FW), DJI Mavic Pro (QC), DJI Phantom 4 (QC) | hluk závodní dráhy; přelet letadla 150-250 s | [s. 4, Tab. 2] |

- Hluk závodní dráhy lokalizován v azimutu 250 až 255 stupňů; v elevaci nejasně (podle autorů se předpokládá, že kvůli nízké elevaci vůči poli) [s. 4].
- Pozorované problémy [s. 4-5]: Meas. 1 (~90 s): tři zdroje (konec přeletu letadla, hluk závodní dráhy, přelet dronu) se spojí do jednoho; Meas. 2: překryv s hlukem závodní dráhy po 67 s; Meas. 3: rozptýlenější výstup beamformingu a falešné zdroje (řídké shluky bodů), autoři navrhují filtr mezi krokem 1 a 2.
- Ground truth: GPS na dronech [s. 2]. SNR, počasí, rychlost větru, teplota: neuvedeno. Data: "on request" [s. 7].

## Výsledky (čísla)
- Definice dosahu: vzdálenost, při které trajektorie zdroje začíná nebo končí (kontrola proti GPS na Fig. 3, 6, 8) [s. 5].
- Tab. 3 [s. 6]: maximální vzdálenost lokalizace

| Měření | Dron | Azimut [m] | Elevace [m] |
|---|---|---|---|
| 1 | Parrot Disco | 278 | 278 |
| 1 | DJI Mavic Pro | 593 | 593 |
| 1 | DJI Phantom 4 | 305 | 305 |
| 2 | Parrot Disco | 218 | 218 |
| 2 | DJI Mavic Pro | 712 | 582 |
| 2 | Funjet | 246 | 246 |
| 2 | DJI Matrice 300 | nelokalizován | nelokalizován |
| 3 | Believer | 270 | 254 |
| 3 | Parrot Disco | 317 | 290 |
| 3 | DJI Mavic Pro | nelokalizován | nelokalizován |
| 3 | DJI Phantom 4 | 294 | 258 |

- Nelokalizované drony letěly příliš daleko: Matrice 300 v Meas. 2 alespoň 844 m, Mavic Pro v Meas. 3 alespoň 1314 m [s. 5].
- Vliv pozadí: Mavic Pro v Meas. 1 593 m bez pozadí vs. 288 m při přeletu letadla; Phantom 4 305 m vs. 230 m (hodnoty 288 a 230 m jsou jen v textu, nejsou v Tab. 3); autoři uvádějí pokles "approximately by 30 to 50%" [s. 6]. (Vlastní výpočet, interpretace: z uvedených hodnot vychází pokles 51.4 % a 24.6 %, takže "30 to 50 %" je jen hrubé zaokrouhlení; pro citaci je bezpečnější uvést obě dvojice čísel.)
- Elevace je citlivější na hluk pozadí než azimut; při elevaci pod 20-30 stupňů je odhad elevace horší (Mavic Pro v Meas. 2: 712 m azimut vs. 582 m elevace) [s. 5-6].
- Srovnání s literaturou (Tab. 4, s. 6): tato práce pevná křídla 250-300 m (750-4200 g), typické kvadrokoptéry 300-700 m (734-1380 g); Altena 2025 [12]: quadplane 360 m (18250 g), typické kvadrokoptéry 130 m (895-1380 g), lehká kvadrokoptéra DJI Mini 2 90 m (242 g); Blass 2024 [18]: velký multikoptér 500 m (6000 g), typická kvadrokoptéra 200 m (907 g), lehká 100 m (249 g); Busset 2015 [2]: typické kvadrokoptéry 150-290 m (420-1200 g); Hengy 2024 [19]: velký oktokoptér 180 m (4000 g), typická kvadrokoptéra 120 m (734 g), lehká 90 m (300 g); Itare 2025 [11]: typická kvadrokoptéra (Phantom 4, 1380 g) 340 m [s. 6].
- Autoři: dosah typicky roste s hmotností dronu; metody neuronové sítě dávají nejnižší dosahy, beamforming (tato práce a Itare [11]) nejvyšší [s. 6]. Metody v Tab. 4: tato práce, [11], [12] beamforming; [18] Blass přímá metoda steered power response; [19] Hengy hluboká neuronová síť; [2] Busset metoda nespecifikována [s. 6]. Hmotnosti jsou buď převzaty z referencí, nebo odhadnuty podle typu dronu [s. 6]. Typy dronů v Tab. 4: tato práce Believer/Funjet/Parrot Disco a DJI Phantom 4/Mavic Pro; [12] Avy Aera (quadplane), DJI Phantom 3 a 4, Mavic 3, Autel EVO II, DJI Mini 2; [18] twinFOLD KAT, DJI Mavic 2 Pro, DJI Mini 2 SE; [2] DJI Phantom 2, Parrot AR Drone 2.0, DJI Flamewheel F450; [19] DJI S1000, Mavic Pro, Spark; [11] DJI Phantom 4. Tvrzení o typu dosahu závisí na poli, počasí a pozadí [s. 5].

## Omezení podle autorů
- Překrývající se nebo navazující trajektorie bývají považovány za jeden zdroj [s. 1, 3, 4, 6].
- Bez dominantního zdroje nebo při hlasitém širokopásmovém pozadí je výstup beamformingu rozptýlený a vznikají falešné zdroje (shluky šumových bodů) [s. 4, 5, 6].
- Elevace nespolehlivá pod 20 až 30 stupňů [s. 3, 5].
- Pouze globální maximum na frekvenci; slabší zdroje mohou být přehlédnuty [s. 3].
- Nekontrolované měření, GPS jen komerční (přesnost neuvedena) [s. 2].
- Bez filtru mezi beamformingem a separací; filtr by redukoval falešné zdroje [s. 4, 5].

## Relevance pro náš projekt
- SOTA a volba pásma: pásmo 140-1200 Hz s dolní mezí daným větrem a horní mezí daným aperturou pole a atmosférickou absorpcí [s. 2] je dobrý odkaz pro zdůvodnění našeho pásma 300 Hz - 2 kHz (naše pásmo je posunuté výš, protože pole je malé a trigger přichází až po detekci; interpretace).
- Metoda: beamforming na jednotlivých FFT binech s argmax na 1stupňové mřížce a následným shlukováním; velmi levná alternativa k MUSIC (jen $g^H C g$ na mřížce); 1 stupeň mřížka 360 x 91 bodů (≈ 32 760 bodů) na bin je ovšem pro MCU náročné (interpretace, vlastní výpočet); k tomu 174 binů/krok. Na MCU by bylo nutné omezit mřížku a biny.
- Rayleigh (Eq. 4): pro naši aperturu L = 0.16-0.25 m dává $1.22\lambda/L$ při 1 kHz (λ = 0.343 m při c = 343 m/s) šířku laloku ≈ 1.67 rad (≈ 96 stupňů) pro L = 0.25 m a ≈ 2.6 rad pro L = 0.16 m, tedy nutné použít vysokorozlišovací metody (MVDR, MUSIC) nebo vyšší frekvence (vlastní výpočet, interpretace). Eq. (4) a jev slábnoucí elevace pod 20-30 stupňů platí i pro náš 2x8 UCA (dvě kruhové roviny dávají vertikální aperturu, ale jen 40-100 mm; interpretace).
- Nelze přenést: pole 3.4 m s 64 mikrofony a fs = 50 kHz; far-field na stovkách metrů; dlouhodobé trajektorie (sekundy až minuty) vs. naše 100 ms; není žádné srovnání s MVDR/MUSIC/SRP-PHAT (srovnání je v předchozí práci Altena 2025 [12], tam v listině jako JASA Express Lett. 5, 24802); jen konvenční beamformer.
- Užitečné: Tab. 4 jako souhrn dosahů v literatuře (Altena, Blass, Busset, Hengy, Itare) pro SOTA sekci; závěr, že rozsah klesá o 30-50 % při hluku pozadí.

## Citovatelná tvrzení
- Pásmo a metoda: "a frequency-domain beamforming step, where a discrete number of frequencies in the range of 140 to 1200 Hz are beamformed individually" [s. 1].
- Dosah: "drones can be localised over long distances, up to 712 m and 593 m for a quadcopter drone in azimuth and elevation direction respectively, and 317 m and 305 m for a fixed-wing drone" [s. 1].
- Proč 140 Hz: "For frequencies below 140 Hz, wind noise pollutes the measurements." [s. 2].
- Proč 1200 Hz: "The upper limit of 1200 Hz is the frequency up to which the dominant drone noise is typically measurable by the array, and attenuation by atmospheric absorption is still relatively low" [s. 2].
- Frekvenční rozlišení: "a frequency resolution of 6.1 Hz and therefore 174 frequencies that are evaluated at each time stamp" [s. 3].
- Elevace: "the effective aperture of the microphone array reduces particularly for the elevation direction when a sound wave comes from low angles" [s. 3].
- Vliv pozadí: "The maximum distance thus drops approximately by 30 to 50% due to background noise." [s. 6].
- Srovnání metod v literatuře: "the neural network approach results in the lowest localisation distances. The beamforming approaches presented in this study and in Ref. [11] result in the highest distances." [s. 6].
- Překryv zdrojů: "overlapping sources are sometimes considered as only one source" [s. 1].
- Vztah dosahu k hmotnosti: "the achieved distances usually scale with the weight of the drone, i.e., the heavier the drone, the longer the distance" [s. 6].

## Relevantní reference z článku
(přepsáno podle seznamu literatury, s. 7)
- [2] Busset J, Perrodin F, Wellig P, Ott B, Heutschi K, Rühl T, Nussbaumer T. Detection and tracking of drones using advanced acoustic cameras. In: Unmanned/Unattended sensors and sensor networks XI; and advanced free-space optical communication techniques and applications, 96470F; 2015. p. 1-9. https://doi.org/10.1117/12.2194309
- [3] Blanchard T, Thomas J-H, Raoof K. UAV's localization from a microphone array by exploiting the harmonic structure of the sound produced. In: Quiet drones 2020 - international e-Symposium on UAV / UAS noise; 2020. p. 1-10.
- [4] Wu L, Fu Y, Yang X, Xu L, Chen S, Zhang Y, Zhang J. Research on the multi-signal DOA estimation based on ResNet with the attention module combined with beamforming (RAB-DOA). Appl Acoust 2025;231:110541. https://doi.org/10.1016/J.APACOUST.2025.110541
- [5] Strauss M, Mordel P, Miguet V, Deleforge A. DREGON: dataset and methods for UAV-embedded sound source localization. In: IEEE/RSJ international conference on intelligent robots and systems (IROS 2018). IEEE; 2018. p. 5735-42. https://doi.org/10.1109/IROS.2018.8593581
- [6] Herold G, Kujawski A, Strümpfel C, Huschbeck S, de Haag MU, Sarradj E. Flight path tracking and acoustic signature separation of swarm quadcopter drones using microphone array measurements. In: Quiet drones 2020 - international e-Symposium on UAV / UAS noise; 2020. p. 1-19.
- [7] Blass M, Maly A, Graf F, Wellig P, Ott B. Towards mobile microphone array based UAV tracking. In: Quiet drones 2022 - international e-Symposium on UAV / UAS noise; 2022. p. 1-18.
- [8] Merino-Martínez R, Sijtsma P, Snellen M, Ahlefeldt T, Antoni J, Bahr C, Blacodon D, Ernst D, Finez A, Funke S, Geyer T, Haxter S, Herold G, Huang X, Humphreys W, Leclère Q, Malgoezar A, Michel U, Padois T, Pereira A, Picard C, Sarradj E, Siller H, Simons D, Spehr C. A review of acoustic imaging methods using phased microphone arrays. CEAS Aeronaut J 2019;10:197-230. https://doi.org/10.1007/s13272-019-00383-4
- [9] Ding S, Guo X, Peng T, Huang X, Hong X. Drone detection and tracking system based on fused acoustical and optical approaches. Adv Intell Syst 2023;5:2300251. https://doi.org/10.1002/AISY.202300251
- [10] Blass M, Graf F. Design and analysis of mobile microphone arrays for acoustic drone tracking. In: Forum acousticum euronoise 2025; 2025. p. 1-8.
- [11] Itare N, Thomas J-H, Raoof K. Genetic algorithm-based acoustic array optimization for estimating UAV DOA using beamforming. Drones 2025;9:149. https://doi.org/10.3390/DRONES9020149
- [12] Altena A, Snellen M, Luesutthiviboon S, de Croon G, Voskuijl M. Frequency band analysis and comparison of localisation techniques for drones using microphone array measurements. JASA Express Lett 2025;5:24802. https://doi.org/10.1121/10.0035915
- [13] James R. Underbrink, circularly symmetric, zero redundancy, planar array having broad frequency range applications. U.S. Patent Number 6, 205, 224 B1 2001.
- [16] Pieter S. Phased array beamforming applied to wind tunnel and fly-over tests. Tech. Rep. NLR-TP-2010-549 National Aerospace Laboratory (NLR); 2010. (zdroj vzorce Eq. 1 spolu s [8])
- [15] Van Veen BD, Buckley KM. Beamforming: a versatile approach to spatial filtering. IEEE ASSP Mag 1988;5:4-24. https://doi.org/10.1109/53.665
- [17] Lord Rayleigh FRS. XXXI. investigations in optics, with special reference to the spectroscope. The London, Edinburgh, and Dublin Philosophical Magazine and Journal of Science 1879;8:261-74. https://doi.org/10.1080/14786447908639684
- [18] Blass M, Grebien S, Graf F. Experimental evaluation of acoustic drone tracking using mobile microphone arrays. In: Quiet drones 2024. University of Salford; 2024. https://doi.org/10.17866/RD.SALFORD.27697134.V1
- [19] Hengy S, Mezzo SD, Sangwa-Simba M, Matwyschuk A, Pujol H, Bavu E. Drone intrusion detection using a network of acoustic arrays running a neural network for real-time drone detection, localization and identification. In: 30th international congress on sound and vibration (ICSV); 2024.
- [1] CWA 18150:2024. Unmanned aircraft systems - counter UAS - testing methodology. Standard European Committee for Standardization; 2024.
- Pozn.: v seznamu je také [14] Vieira A, Snellen M, Simons DG. Experimental assessment of sound quality metrics for takeoff and landing aircraft. AIAA J 2021;59(1):240-9. https://doi.org/10.2514/1.J059633 (citováno pro Underbrinkovo rozložení, pro nás okrajové).

## Kontrola (kolo 2)
- Opraveno: (1) s. 1 vs. s. 6: údaj "305 m pro pevné křídlo v elevaci" v abstraktu a highlights nesouhlasí s Tab. 3 (nejvyšší elevace pevných křídel je 290 m, 305 m je Phantom 4 v Meas. 1); doplněno upozornění, abstrakt je tedy interně nekonzistentní. (2) 593 m (elevace) a 712 m (azimut) pocházejí z různých měření téhož typu dronu (Mavic Pro Meas. 1 a 2); upřesněno. (3) Rozmezí "30 to 50 %" (s. 6): z čísel autorů vychází 51.4 % (Mavic Pro 593 -> 288 m) a 24.6 % (Phantom 4 305 -> 230 m); hodnoty 288 a 230 m jsou jen v textu, ne v Tab. 3. (4) Stránky u omezení doplněny (překryv zdrojů: s. 1, 3, 4, 6). Všechny číselné hodnoty (Tab. 1, Tab. 2, Tab. 3, Tab. 4, Eq. 1-4, FFT 2500/8192/50 %, 6.1 Hz, 174 frekvencí, mřížka 1 stupeň, práh 2 stupně/0.5 s, 200 bodů), čísla stran (Tab. 3 a Tab. 4 na s. 6, Tab. 2 na s. 4) a 10 citátů odpovídají PDF; reference [1]-[19] odpovídají seznamu literatury.
- Doplněno: sekce Související práce (refs [2]-[7]); financování (projekt ACTION, NTP N21/005); typy dronů a metody v Tab. 4; příklady problémů v Meas. 1-3; ref. [16] (Pieter, NLR); upřesnění beamformeru ($\|\mathbf g\|^4$); seznam údajů, které článek vůbec neuvádí (SNR, bit depth, synchronizace, okno FFT, odhad CSM, přesnost GPS, počasí).
- Nejistoty: způsob odhadu CSM (počet průměrovaných bloků pro $\mathbb E[\cdot]$) a použité FFT okno nejsou uvedeny, takže nelze říct, zda je CSM odhad z jednoho bloku; přesnost komerčního GPS a rychlost větru jsou neznámé; "30 to 50 %" je autorské zaokrouhlení, které nelze potvrdit; definice dosahu ("path begins or ends", s. 5) je vizuální, nikoli statistická. Hmotnost 1380 g u Itare 2025 v Tab. 4 (s. 6) je podle autorů buď převzatá z reference, nebo odhadnutá; Itare 2025 sám hmotnost Phantomu 4 neuvádí (ověřeno v jeho PDF), takže jde o údaj Altenové (Tab. 1, s. 2).
