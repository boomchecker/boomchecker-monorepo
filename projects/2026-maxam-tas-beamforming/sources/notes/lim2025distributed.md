# Lim, Joo, Kim 2025: lokalizace dronu pomocí dvou distribuovaných mikrofonních polí a GCC-PHAT

Pozn. k číslování stran: [s. X] = PDF strana = strana časopisu ("X of 25").

## Bibliografie
- Lim, J.; Joo, J.; Kim, S.C. "Performance Enhancement of Drone Acoustic Source Localization Through Distributed Microphone Arrays." *Sensors* 2025, 25, 1928 (článek č. 1928; číslo sešitu 6 je odvozeno z DOI `s25061928`, v PDF není explicitně uvedeno). DOI: 10.3390/s25061928. Received 20 Dec 2024, revised 21 Feb 2025, accepted 13 Mar 2025, published 20 Mar 2025. Academic Editor: Udo Frese. [s. 1]
- Vydavatel: MDPI, Basel, Švýcarsko ("Licensee MDPI, Basel, Switzerland"). MDPI: ano. Open access: ano, licence CC BY 4.0. [s. 1]
- Pracoviště autorů: Department of Electrical and Electronic Engineering, Pusan National University, Busan 46241, Republic of Korea. Korespondenční autor: Suk Chan Kim. [s. 1]
- Návrh BibTeX:
```bibtex
@article{lim2025distributed,
  author  = {Lim, Jaejun and Joo, Jaehan and Kim, Suk Chan},
  title   = {Performance Enhancement of Drone Acoustic Source Localization Through Distributed Microphone Arrays},
  journal = {Sensors},
  year    = {2025},
  volume  = {25},
  number  = {6},
  pages   = {1928},
  doi     = {10.3390/s25061928},
  publisher = {MDPI}
}
```

## Problém a přínos
- Problém: dřívější TDOA/GCC-PHAT lokalizace malým polem (4 mikrofony, Pourmohammad & Ahadi [12]) bere úhel příchodu (AOA) jako azimut; u dronu ve velké výšce se AOA a azimut výrazně liší a vznikají chyby. [s. 2]
- Přínos: (1) rozšíření GCC-PHAT TDOA rámce na výpočet azimutu i elevace, (2) nízkonákladový algoritmus "hyperbolic intersection" pro 3D lokalizaci, (3) srovnání s baseline metodou v simulaci a v terénu, (4) dvě distribuované čtyřmikrofonní pole, jejichž úhlové odhady se protínají. [s. 4]
- Pozn. (interpretace): jde o čistě geometrickou/TDOA metodu; neporovnává se s DAS, MVDR, MUSIC ani SRP-PHAT.

## Mikrofonní pole a hardware
- Jedno pole = 4 mikrofony (Mic1 (R,0,0), Mic2 (−R,0,0), Mic3 (0,R,0), Mic4 (0,R,R)); dvojice 1-2 pro AOA, dvojice 3-4 pro elevaci. [s. 4, Obr. 1; s. 12-13]
- Dvě pole (celkem 8 mikrofonů), symetricky vůči počátku, každé ve vzdálenosti 2 m od počátku; vzdálenost mikrofonů od počátku pole v rovině xy je 1 m (explicitně v simulaci s. 16 a v terénu s. 19; ztotožnění s R z Obr. 1 je interpretace). Obě pole mají stejné Darr (s. 15). [s. 14-16, s. 18-19]
- Mikrofony v terénu: PCB Piezotronics 130F22, 0,25" membrána, citlivost 45 mV/Pa, frekvenční odezva 10 Hz až 20 kHz. [s. 18]
- Akvizice: "high-speed USB audio interface", zpracování v reálném čase na NVIDIA Jetson AGX Xavier (8jádrové ARM CPU, 512jádrové GPU Volta). [s. 19]
- Vzorkovací frekvence, bitová hloubka, synchronizace mezi poli a kalibrace: neuvedeno. (Úvod zmiňuje "inter-array calibration" jen jako motivaci, s. 3.)
- Terénní test používá osm mikrofonů (dvě pole po čtyřech) v rozmístění podle Obr. 8. [s. 18-19]
- Rychlost zvuku: konstantně 340 m/s; vzorec Vsound = 331,3 + 0,606·T (rovnice 45) uveden, ale nepoužit. [s. 11]

## Signálový model a rovnice
- Cross-correlation a TDOA (rovnice 3, 4): $R_{xy}(\tau)=\int x(t)\,y(t+\tau)\,dt$, $\tau_{12}=\arg\max_\tau R_{12}(\tau)$. [s. 5]
- GCC-PHAT (rovnice 5-7): $R^{PHAT}_{xy}(\tau)=\frac{1}{2\pi}\int H^{PHAT}(f)X(\omega)Y(\omega)^*\,d\omega$ (v textu uvedeno "dt", interpretace: překlep), $H^{PHAT}(f)=\frac{1}{|X(f)Y^*(f)|}$, $R^{PHAT}_{xy}(\tau)=\mathcal{F}^{-1}\!\left\{\frac{X(f)Y^*(f)}{|X(f)Y^*(f)|}\right\}$. [s. 6]
- Úhel z TDOA (rovnice 8): $\cos\phi=\frac{d_2-d_1}{D}=\frac{\tau_{21}v_{sound}}{D}$. [s. 6]
- Hyperbola (rovnice 14): $y^2=ax^2+b$, $a=\frac{4R^2}{v^2\tau_{21}^2}-1$, $b=\frac{v^2\tau_{21}^2}{4}-R^2$. [s. 7]
- Přímka z TDOA tří mikrofonů (rovnice 21, 24, 27): $y=a_ix+b_i$, např. $a_1=\frac{\tau_{23}+\tau_{13}}{\tau_{23}-\tau_{13}}$, $b_1=-\frac{v^2}{2R}\tau_{23}\tau_{13}$. [s. 8-9]
- AOA v počátku (rovnice 29/59): $\phi=\frac{\pi}{2}-\cos^{-1}\sqrt{1-\left(\frac{v_{sound}\tau_{12}}{2R}\right)^2}$; vzdálenost $r=\frac{b}{\sin\Phi-a\cos\Phi}$ (rovnice 32). [s. 9, 13]
- Elevace (rovnice 60-61): $\theta=\frac{\pi}{2}-\cos^{-1}\sqrt{1-\left(\frac{v_{sound}\tau_{34}}{R}\right)^2}$. [s. 13]
- Podmínka dalekého pole (rovnice 52): $d_{FF}\gg D_{FF}^2/\lambda$. [s. 12]
- AOA vs. azimut (rovnice 62-67): $\phi_r=\cos^{-1}(x_s/r)$, $d=r\cos\theta$, $\phi_d=\cos^{-1}\!\left(\frac{\cos\phi_r}{\cos\theta}\right)$ (rovnice 67). [s. 13-14]
- 3D souřadnice jednoho pole (rovnice 43): $x=r\cos\Phi\sin\theta,\ y=r\sin\Phi\sin\theta,\ z=r\cos\theta$. [s. 11]
- Distribuovaná lokalizace (rovnice 68-70): $y_s=\tan\phi_1(x_s-D_{arr1})$, $y_s=\tan\phi_2(x_s+D_{arr2})$, $x_s=D_{arr}\frac{\tan\phi_1+\tan\phi_2}{\tan\phi_1-\tan\phi_2}$. [s. 15]
- Průměrování z-odhadů (rovnice 71-75): $z_s=\frac{z_{s1}+z_{s2}}{2}$, kde $z_{si}=(\tan\theta_i-n_i)\sqrt{(x_s\mp D_{arr})^2+y_s^2}$. [s. 15-16]
- Rovnice (42) v PDF (s. 11): "cos(θ) = 90 − cos⁻¹(√(1 − (v_sound τ34 / R)²))"; (interpretace) vypadá jako překlep (levá strana má být θ, úhel v radiánech jako v rovnici 61, kde je správně θ = π/2 − cos⁻¹(...)); text říká "using Equation (29)", ale argument je τ34/R, ne τ12/(2R). Rovnice (14): a = 4R²/(v²τ21²) − 1 (ověřeno z vykresleného PDF, s. 7); rovnice (28) a (29) jsou ve stupních (90°), rovnice (57)-(61) v radiánech.
- Pozn.: Eq. (53) cos(b) ≈ (C/2)/(D/2) a (54) C = D cos(b) jsou geometrie dalekého pole; (55)-(58) odvozují φ z rozdílu vzdáleností d1−d2. [s. 13]

## Algoritmus
1. TDOA každé dvojice mikrofonů z GCC-PHAT (argmax PHAT-vážené korelace). [s. 5-6, 12]
2. Z TDOA dvojice 1-2 se v každém poli spočte AOA φ (rov. 59), z dvojice 3-4 elevace θ (rov. 61). [s. 13]
3. Azimut φ_d z AOA a elevace (rov. 67). [s. 14]
4. Průsečík dvou přímek ze dvou polí dává x_s, pak y_s (rov. 68-70). [s. 15]
5. z_s z elevací obou polí, průměrování (rov. 71-75). [s. 15-16]
- Parametry: délka okna, FFT, překryv, frekvenční pásmo, váhování, počet zdrojů, regularizace, grid úhlů: neuvedeno (metoda je čistě analytická, bez gridu). Předpoklad dalekého pole a jednoho zdroje. [s. 12, 23]
- V terénní části autoři tvrdí, že aproximace rovinné vlny "remains applicable", protože vlnová délka zvuku dronu je "relatively short compared to the microphone array's spatial configuration" [s. 19]. (interpretace) Podmínka (52) d_FF ≫ D²/λ ale vyžaduje velkou vzdálenost vůči D²/λ; při D = 2R = 2 m a λ = 0,34 m (1 kHz) je D²/λ ≈ 12 m, tedy dron do 10 m od pole podmínku formálně nesplňuje. Autoři to neřeší.
- Konvenční korelace podle autorů zhoršuje odhad TDOA u nízkých frekvencí [21], což je důvod volby GCC-PHAT. [s. 5]
- Zmíněný bandpass filtr pro oddělení zvuku dronu od šumu, parametry neuvedeny; spektrální subtrakce a adaptivní potlačení šumu nezkoumány. [s. 2]

## Experiment / simulace
- Simulace: dron 10 m nad počátkem na kruhové dráze o poloměru 10 m, kroky přibližně 10°, rozsah 0° až 180°; 1000 pokusů; chyba = vzdálenost odhad-pravda (osy y v Obr. 6 a 7 jsou popsány "RMSE(m)", text říká "mean error"; (interpretace) není jasné, zda jde o průměr vzdáleností nebo RMSE); kanál LOS s Free Space Path Loss; porovnání AWGN a ideálního kanálu; SNR od −20 do 30 dB po 5 dB. Šum je AWGN; SNR se nevztahuje k žádnému konkrétnímu pásmu ani signálu dronu (simulovaný signál není popsán). [s. 16-17]
- Obr. 6 (ideální kanál, odečet z grafu, orientační): chyba baseline je nejvyšší u azimutu 0° a 180° (celková přibližně 6 až 7 m) a klesá na minimum kolem 40° a 135° (přibližně 0,3 m); navržená metoda je ve všech azimutech blízko nuly. [s. 17]
- Baseline: metoda neuvažující azimut, jen elevace a TDOA (Pourmohammad & Ahadi). [s. 16]
- Terénní test: dron(y) neuvedeného typu (typ dronu, BPF, vítr neuvedeno); pozice podle Obr. 9 (mřížka 6 × 6 = 36 poloh, x i y ∈ {±4, ±6, ±8} m, čtyři kvadranty; s. 20), výška 4 m a 6 m (text s. 19; úvod kap. 4 s. 16 také "4 m and 6 m"), ale titulek Obr. 9 uvádí fixní z = 5 m (s. 20) a s. 21 se říká, že dron byl "5 m lower than in the simulations" (tj. 10 m − 5 m = 5 m); rozpor nevyřešen. Autoři výšky "slightly reduced" kvůli slabému signálu dronů. Maximální vzdálenost 10 m kvůli nízké akustické intenzitě; po každém umístění 10 měření, použit průměr; ground truth = skutečné souřadnice při umístění (způsob měření neuveden). [s. 18-21]
- Prostředí: venku; vítr, teplota, vlhkost neuvedeno; rychlost zvuku 340 m/s. [s. 11, 18]

## Výsledky (čísla)
Simulace, ideální kanál [s. 18, Tab. 1-2] (průměr / rozptyl / 95% CI):

| Osa | Baseline mean (m) | Baseline var (m²) | Baseline 95% CI (m) | Navržená mean (m) | Navržená var (m²) | Navržená 95% CI (m) |
|---|---|---|---|---|---|---|
| Celkem | 2,4906 | 3,3434 | [1,3567; 3,6244] | 0,1569 | 0,0035 | [0,1202; 0,1936] |
| X | 0,0108 | 0,0001 | [0,0046; 0,0170] | 0,0108 | 0,0001 | [0,0046; 0,0170] |
| Y | 2,1477 | 3,2090 | [1,0368; 3,2586] | 0,1339 | 0,0052 | [0,0892; 0,1786] |
| Z | 1,1592 | 0,3813 | [0,7763; 1,5421] | 0,0594 | 0,0013 | [0,0370; 0,0818] |

Simulace, závislost na SNR [s. 17, Obr. 7]:
- Navržená metoda: průměrná chyba pod 3 m pro SNR > 0 dB, pod 0,3 m pro SNR > 15 dB, přibližně 0,16 m pro SNR > 20 dB. [s. 17]
- Baseline se ustálí na přibližně 2,5 m i pro SNR > 15 dB. Pro SNR < 0 dB je rozdíl mezi metodami pod 1 m; nad 0 dB rozdíl od 1 m do maximálně 2,34 m. [s. 17]
- Nesoulad: abstrakt a závěr uvádějí zlepšení "up to 2.13 m" resp. "peak localization improvement of 2.13 m" (SNR > 0 dB) [s. 1, 24], hlavní text uvádí maximum 2,34 m [s. 17]. Odečet z vykresleného Obr. 7 (orientační): rozdíl baseline a navržené metody je kolem 1,4 m při 0 dB, kolem 2,0 m při 10 dB a přibližně 2,1 m při SNR ≥ 15 dB (baseline plató přibližně 2,3 m, navržená přibližně 0,16 m), tedy graf podporuje spíše 2,13 m než 2,34 m; plató baseline v grafu (přibližně 2,3 m) je též o něco níže než text "about 2.5 m". Použít 2,13 m (abstrakt, závěr, graf) a uvádět s výhradou.
- Pozn. (interpretace): konstatování "performance gap 1 m až 2,34 m" a "under 3 m for SNR > 0 dB" ukazují, že absolutní chyba v šumu je v metrech, tedy ne přesnost srovnatelná s ideálním kanálem.

Terénní test [s. 20, Tab. 3-4]:

| Osa | Baseline mean (m) | Baseline var (m²) | Baseline 95% CI (m) | Navržená mean (m) | Navržená var (m²) | Navržená 95% CI (m) |
|---|---|---|---|---|---|---|
| Celkem | 2,4526 | 1,0463 | [1,8182; 3,0870] | 0,7668 | 0,0888 | [0,5820; 0,9516] |
| X | 0,418 | 0,1302 | [0,1942; 0,6418] | 0,3458 | 0,0653 | [0,1873; 0,5043] |
| Y | 2,1931 | 1,1345 | [1,5325; 2,8537] | 0,4194 | 0,0782 | [0,2460; 0,5928] |
| Z | 0,6819 | 0,3556 | [0,3120; 1,0518] | 0,4125 | 0,0693 | [0,2492; 0,5758] |

- Průměrné zlepšení v terénu přibližně 1,7 m (2,4526 − 0,7668 = 1,6858 m), snížení rozptylu přibližně 0,9575 m² (1,0463 − 0,0888). [s. 20]
- Pozn.: baseline v terénu (celkem 2,4526 m) je o málo lepší než v simulaci (2,4906 m), v ose Z výrazně (0,6819 vs 1,1592 m), v ose X naopak horší (0,418 vs 0,0108 m); autoři to vysvětlují nižší výškou dronu (o 5 m nižší, menší rozdíl AOA/azimut) a navržená metoda byla v terénu horší než v simulaci (0,7668 vs 0,1569 m) kvůli hluku pozadí/nižšímu SNR. [s. 20-21]
- Intervaly spolehlivosti v Obr. 10-11: σ_x=0,1302, σ_y=1,1345, σ_z=0,3556 (baseline); σ_x=0,2854, σ_y=0,1934, σ_z=0,2081 (navržená); n=10, t=2,26 (Obr. 10 a 11 ukazují odhady a 95% intervaly pro čtyři kvadranty). [s. 21-22]
- Pozn. (interpretace): hodnoty "σ" pro baseline jsou číselně totožné se sloupcem "Variance (m²)" v Tab. 3; pro navrženou metodu se liší od Tab. 4 (variance 0,0653 / 0,0782 / 0,0693 m², jejich odmocniny 0,2555 / 0,2796 / 0,2633 m se neshodují ani s uvedenými σ 0,2854 / 0,1934 / 0,2081; vlastní dopočet). Označení rozptyl/směrodatná odchylka v článku tedy není konzistentní.

## Omezení podle autorů
- Jen jeden dron najednou; roje a více zdrojů jako budoucí práce. [s. 23]
- Experimentální dosah omezen na 10 m kvůli nízké intenzitě zvuku dronu; reálné scénáře podle autorů často vyžadují lokalizaci "at distances exceeding 100 m". [s. 19, 23]
- Jen jeden typ/konfigurace měření; různé typy dronů a další metody se mají teprve porovnat ("Further comparisons with alternative localization techniques will be conducted"). [s. 24]
- Metoda počítá s dalekým polem; rychlost zvuku konstantní 340 m/s, vliv vlhkosti a výšky nezohledněn. [s. 11, 12]
- Spektrální subtrakce a adaptivní potlačení šumu nezkoumány. [s. 2]
- Data jen částečně dostupná (GitHub LimJJ23/Drone_Localization). [s. 24]

## Relevance pro náš projekt
- SOTA sekce: příklad GCC-PHAT + geometrické (hyperbolické) řešení, tj. kategorie "GCC-PHAT+LS" z našeho srovnání; ukazuje i problém záměny AOA a azimutu u dronu ve velké elevaci. Pro nás platí, že 2×8 pole s 3D steering vektorem (azimut a elevace zvlášť) tento problém nemá; ale pro 2D-UCA bez rozlišení elevace by vznikl (interpretace).
- Volba pásma: nic; bandpass zmíněn, ale bez parametrů. Nelze použít pro volbu pásma 300 Hz až 2 kHz.
- Geometrie 2×8 UCA: nepřenositelné, pole jsou 4 mikrofony v L-uspořádání s metrovými rozměry (R = 1 m) a dvě pole vzdálená 4 m; naše apertura je 160 až 250 mm.
- Volba metody: GCC-PHAT pro dvojice je levný (IFFT na dvojici), vhodný jako baseline pro MCU; proti DAS/MVDR/MUSIC v článku žádné srovnání (interpretace: nelze z něj usuzovat na pořadí metod).
- Výpočetní nároky: běží na Jetson AGX Xavier s GPU, ne na MCU; časování/latence neuvedena. Nelze přenést jako důkaz proveditelnosti na STM32H563.
- Nelze přenést: jednotlivé výsledky (metry chyby při vzdálenosti do 10 m, neznámý dron, neznámé fs).
- Kvalita zdroje (interpretace): nesrovnalosti (2,13 vs 2,34 m; z = 4/6 vs 5 m; σ vs variance) a nejasné terénní podmínky snižují důvěryhodnost kvantitativních čísel; citovat opatrně.

## Citovatelná tvrzení
- Existující TDOA lokalizace čtyřmi mikrofony odhaduje AOA jako azimut, což je u vysoko položeného zdroje chybné: "Calculating AOA as azimuth results in angular errors since the AOA does not accurately represent the true azimuth angle." [s. 2]
- Akustické metody jsou odolné vůči změnám prostředí: "acoustic source-based localization methods are particularly effective for drone detection as they are relatively resilient to environmental changes and provide high localization accuracy." [s. 2]
- PHAT: "The application of the PHAT weighting function further refines this process by eliminating amplitude information, which enhances robustness against noise, and calculating the correlation solely based on phase." [s. 5]
- Nelineární soustava TDOA hyperbol je výpočetně náročná: "Since these are nonlinear equations, solving them requires a hyperbolic intersection approach, which typically involves numerical methods." [s. 7]
- Podmínka dalekého pole (rovnice, nikoli citát): d_FF ≫ D_FF²/λ, kde d_FF je vzdálenost zdroje od počátku pole, D_FF maximální rozestup mikrofonů, λ vlnová délka. [s. 12-13]
- Simulace v šumu: "For the proposed method, the average error decreased to under 3 m when the SNR exceeded 0 dB." [s. 17]
- Terénní test: "the proposed method showed an average error reduction of approximately 1.7 m in all configurations of the real-world test environment." [s. 20]
- Omezený dosah: "The experimental range was limited to 10 m due to the low acoustic intensity of the drone's emitted sound" [s. 23]
- Hardware: signály zpracovány "in real time by the NVIDIA Jetson AGX Xavier". [s. 19]
- Dosah: "real-world drone detection scenarios often require localization at distances exceeding 100 m". [s. 23]
- GCC-PHAT vs. nízké frekvence: "for low-frequency signals, such as those emitted by acoustic sources, the performance of the conventional cross-correlation approach tends to deteriorate". [s. 5]

## Relevantní reference z článku
- [4] Park, S.; Kim, H.T.; Lee, S.; Joo, H.; Kim, H. Survey on Anti-Drone Systems: Components, Designs, and Challenges. IEEE Access 2021, 9, 42635–42659. [CrossRef]
- [5] Manamperi, W.; Abhayapala, T.D.; Zhang, J.; Samarasinghe, P.N. Drone Audition: Sound Source Localization Using On-Board Microphones. IEEE/ACM Trans. Audio Speech Lang. Process. 2022, 30, 508–519. [CrossRef]
- [6] Birch, G.C.; Griffin, J.C.; Erdman, M.K. UAS Detection Classification and Neutralization: Market Survey 2015; Sandia National Laboratories: Albuquerque, NM, USA, 2015. [CrossRef]
- [12] Pourmohammad, A.; Ahadi, S.M. Real Time High Accuracy 3-D PHAT-Based Sound Source Localization Using a Simple 4-Microphone Arrangement. IEEE Syst. J. 2012, 6, 455–468. [CrossRef]
- [15] Benesty, J.; Chen, J.; Huang, Y. Direction-of-Arrival and Time-Difference-of-Arrival Estimation. In Microphone Array Signal Processing; Springer: Heidelberg, Germany, 2008; Volume 1, pp. 181–215.
- [21] Lee, R.; Kang, M.-S.; Kim, B.-H.; Park, K.-H.; Lee, S.Q.; Park, H.-M. Sound Source Localization Based on GCC-PHAT with Diffuseness Mask in Noisy and Reverberant Environments. IEEE Access 2020, 8, 7373–7382. [CrossRef]
- [12] viz výše; [13] Zhang, C.; Florencio, D.; Ba, D.E.; Zhang, Z. Maximum Likelihood Sound Source Localization and Beamforming for Directional Microphone Arrays in Distributed Meetings. IEEE Trans. Multimed. 2008, 10, 538–548. [CrossRef]
- [14] Brandstein, M.S.; Griebel, S. Explicit Speech Modeling for Microphone Array Applications. In Microphone Arrays: Signal Processing Techniques and Applications; Brandstein, M.S., Ward, D.B., Eds.; Springer: New York, NY, USA, 2001; pp. 133–153.
- [17] Lee, H.; Park, J. An Acoustic Source Localization Method Using a Drone-Mounted Phased Microphone Array. Drones 2021, 5, 75. [CrossRef] (v PDF je u [CrossRef] jen odkaz bez vytištěného DOI; existenci a údaje ověřit před citací, interpretace)
- [18] Kim, D.; Choi, S. Detection of Nearby UAVs Using a Multi-Microphone Array Onboard a Drone. J. Acoust. Signal Process. 2022, 17, 102–115.
- [19] Wang, Y.; Chen, L. Drone Detection and Localization Using Enhanced Fiber-Optic Acoustic Sensors. IEEE Trans. Instrum. Meas. 2023, 72, 145–158.
- [20] Smith, J.; Brown, A. Deep Learning-Based 3D Sound Source Localization in Urban Environments. Neural Netw. Mach. Learn. 2022, 35, 204–220.
- [22] Kay, S.M. Fundamentals of Statistical Signal Processing: Estimation Theory; Prentice-Hall, Inc.: Upper Saddle River, NJ, USA, 1993. (autoři cituji pro tvrzení, že průměrování odhadů zlepšuje odolnost vůči šumu, s. 15)
- Pozn. (interpretace): reference [18]-[20] nemají [CrossRef] ani DOI, mají neověřitelně vyhlížející časopisy ("J. Acoust. Signal Process.", "Neural Netw. Mach. Learn.") a obecná jména autorů; před převzetí do naší rešerše ověřit jejich existenci, jinak necitovat.

## Kontrola (kolo 2)
- Opraveno: (1) "stovky metrů" nahrazeno přesným tvrzením autorů "distances exceeding 100 m" [s. 23]; (2) Pracoviště doplněno o PSČ a zemi dle záhlaví [s. 1]; (3) popis rozporu 2,13 vs 2,34 m doplněn odečtem z Obr. 7 [s. 18 PDF], graf podporuje 2,13 m, plató baseline v grafu je kolem 2,3 m, ne 2,5 m z textu; (4) strany u terénního testu zpřesněny (text o 4/6 m a hardware s. 18-19, mřížka Obr. 9 a Tab. 3-4 s. 20, vysvětlení rozdílu výšek s. 21); (5) "po každé" na "po každém"; (6) „Podmínka dalekého pole“ v citovatelných tvrzeních přeznačena na rovnici, ne citát; (7) rovnice (42): upřesněno, co přesně je v PDF špatně; (8) rozptyly vs σ v Tab. 3-4 doplněny číselným dopočtem; (9) pořadí a úplnost referencí (doplněno [13], [18]-[20] doslovně, [22]); (10) výsledek v terénu: baseline celkem 2,4526 vs sim 2,4906 (nejen osa Z).
- Doplněno: mřížka poloh dronu z Obr. 9 (36 poloh, ±4/±6/±8 m); 8 mikrofonů v terénu; kritika tvrzení o rovinné vlně vs. podmínka (52) (označeno interpretace); definice chyby a nejasnost "RMSE" vs "mean error"; odečet z Obr. 6; omezení (jen jeden dron, další metody a typy dronů v budoucí práci); citáty o dosahu 100 m a o selhání korelace na nízkých frekvencích; zmínka, že Darr1 = Darr2.
- Nejistoty: výška dronu v terénu (4/6 m vs 5 m) a 2,13 vs 2,34 m zůstávají v článku vnitřně nekonzistentní; hodnoty σ pro navrženou metodu nelze sladit s Tab. 4; číslo sešitu (6) není v PDF, jen z DOI; existence ref. [17]-[20] neověřena (bez DOI); typ dronu, fs, bitová hloubka, synchronizace polí, kalibrace, pásmo BPF, okno/FFT nejsou v článku uvedeny.
