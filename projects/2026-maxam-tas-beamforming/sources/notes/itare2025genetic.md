# Itare et al. 2025: Genetická optimalizace akustického pole pro DOA dronu (beamforming + TFR)

## Bibliografie
- Plná citace (podle PDF, s. 1): Nathan Itare, Jean-Hugh Thomas, Kosai Raoof. "Genetic Algorithm-Based Acoustic Array Optimization for Estimating UAV DOA Using Beamforming". Drones 2025, 9(2), 149 (article no. 149; 21 stran). DOI: 10.3390/drones9020149. Received 23 December 2024; revised 27 January 2025; accepted 14 February 2025; published 18 February 2025 [s. 1]. Academic Editors: Antonio J. Torija Martinez, Kotaro Hoshiba, Makoto Kumon.
- Afiliace: Laboratoire d'Acoustique de l'Université du Mans (LAUM), UMR 6613, CNRS, Le Mans Université (včetně Institut d'Acoustique-Graduate School, IA-GS), 72085 Le Mans, Francie [s. 1]. Korespondenční autor: Nathan Itare.
- Vydavatel: MDPI, Basel (Licensee MDPI). MDPI: ano. Open access: ano (CC BY 4.0) [s. 1]. Financování: DGA (grant 01D19024292 AID) a Région des Pays de Loire [s. 19].
- Číslování stran PDF = číslování "n of 21" v záhlaví. (PDF obsahuje i skrytou textovou vrstvu z verze "Version January 27, 2025 submitted to Drones" s jinou stránkovou sazbou; použit je text finální verze, čísla stran ověřena vykreslením stran 8 a 9.)
- DOI článku: 10.3390/drones9020149 (odkaz v PDF i v citaci na s. 1). DOI referencí jsou v textu jen jako "[CrossRef]"; doplněny z cílů hyperlinků v PDF (viz sekce Relevantní reference).
- Návrh BibTeX:
```bibtex
@article{itare2025genetic,
  author  = {Itare, Nathan and Thomas, Jean-Hugh and Raoof, Kosai},
  title   = {Genetic Algorithm-Based Acoustic Array Optimization for Estimating {UAV} {DOA} Using Beamforming},
  journal = {Drones},
  volume  = {9},
  number  = {2},
  pages   = {149},
  year    = {2025},
  publisher = {MDPI},
  doi     = {10.3390/drones9020149}
}
```

## Problém a přínos
- Problém: lokalizace dronů podle zvuku vyžaduje mikrofonní pole, jehož geometrie určuje směrovost (šířka hlavního laloku, postranní laloky) [s. 1-3].
- Přínos (3 body) [s. 3-4]: (1) rozšíření časově-frekvenční metody (TFR) z [19] o kritérium kontinuity trajektorie dronu; (2) genetický algoritmus (GA) optimalizuje 3D pole o 10 mikrofonech (struktura se 3 osami z [14]) podle kritéria šířka hlavního laloku / MSL; (3) experiment s maximální vzdáleností lokalizace, dosaženo 340 m.
- Výsledek: GA-pole dává podobný výkon DOA jako původní pole (původní mírně lepší), ale čistší energetickou mapu s užším hlavním lalokem (výhodné pro více dronů) [s. 14, 19]. Metoda dává střední chybu pod 10 stupňů v azimutu a pod 5 stupňů v elevaci; maximální vzdálenost 240 až 340 m [s. 1, Abstract].

## Mikrofonní pole a hardware
- Původní pole [19]/[14] (s. 4-5): 10 mikrofonů; jeden ve středu $x_0$ a tři na každé ze 3 os (větví); vzdálenosti od středu $l_1=5$ cm, $l_2=20$ cm, $l_3=110$ cm (Eq. 1); rozpětí $L\simeq155.6$ cm (shoduje se s $\sqrt2\,l_3=155.6$ cm, tj. vzdálenost koncových mikrofonů dvou větví; vlastní výpočet, interpretace); spodní mez $f_{min}=c/L=220.5$ Hz; horní mez $f_{max}=c/(2l_1)=3430$ Hz (Nyquist prostorově); tedy c = 343 m/s implikováno (interpretace) [s. 5].
- Optimalizované pole (200 generací, Tab. 3, s. 8): mikrofon 0 v počátku; osa x: 0.5, 0.7, 1.05 m; osa y: 0.5, 0.8, 1.25 m; osa z: 0.05, 0.7, 0.75 m (souřadnice čtené z tabulky, pořadí mikrofonů dle sloupců; interpretace). Omezení GA: "the smallest spacing of l1 and the largest spacing of 120 cm" [s. 6]; text nespecifikuje, zda jde o vzdálenost od středu, nebo mezi sousedními mikrofony na větvi. Tab. 3 uvádí y = 1.25 m (>120 cm); pokud jde o vzdálenost od středu, je to nesoulad ve zdroji, pokud o rozteč na větvi (1.25 - 0.8 = 0.45 m), není; z PDF nerozhodnutelné. Tab. 3 (s. 8): 10 mikrofonů, první v počátku; ostatní leží na osách (x: 0.5, 0.7, 1.05; y: 0.5, 0.8, 1.25; z: 0.05, 0.7, 0.75 m), poslední dva na ose z jsou od sebe jen 5 cm = $l_1$.
- Typ mikrofonu, fs, bit depth, akvizice, synchronizace, kalibrace, SNR, počet letů: neuvedeno v tomto článku (ověřeno prohledáním celého textu; podrobnosti případně v [19] Itare 2022 a [35] disertaci). GPS: palubní GPS dronu (typ a přesnost neuvedeny) [s. 1, 11].
- Pole je 3D a řídké (sparse, "antenna"); není UCA.

## Signálový model a rovnice
- (1) [s. 5]: $\|x_1\|=\|x_4\|=\|x_7\|=l_1,\ \|x_2\|=\|x_5\|=\|x_8\|=l_2,\ \|x_3\|=\|x_6\|=\|x_9\|=l_3$.
- (2) [s. 5] faktor směrovosti: $Q_d(f,\phi_r,\theta_r)=\dfrac{|H(f,\phi_r,\theta_r)|^2}{\frac1{4\pi}\int_0^{2\pi}\int_0^{\pi}|H(f,\phi,\theta)|^2\sin\theta\,d\theta\,d\phi}$, $H$ směrový diagram (ref [33], McCowan).
- (3) [s. 6] fitness GA: $fit=\dfrac{lobe\_width_{azi}}{MSL}$ (MSL = maximum sidelobe level).
- (4) [s. 9] model zdroje (akustický monopól): $y_i(t)=\dfrac{\rho_0 F_a(t-r_{iS}/c)}{4\pi r_{iS}}$ ($F_a$ = objemové zrychlení, "acceleration flow rate", $\rho_0$ hustota vzduchu); far-field, rovinné vlny (zdůvodněno tím, že vzdálenost dron-pole je větší než akustická vlnová délka).
- (5) [s. 9] zpoždění: $\tau_i(\phi_r,\theta_r)=(x_i-x_0)^t n_S(\phi_r,\theta_r)$ (ověřeno na vykreslené s. 9: Eq. 5 je v PDF skutečně bez dělení c, přestože $\tau_i$ je označeno jako čas a v Eq. 6 se přičítá k t; jde o chybu nebo zkratku zdroje, správně by bylo $\tau_i=(x_i-x_0)^t n_S/c$, znaménko podle konvence $n_S$; interpretace).
- (6) [s. 9] delay-and-sum (v časové oblasti, N = počet mikrofonů): $y(t,\phi_r,\theta_r)=\dfrac1N\sum_{i=0}^{N-1}y_i(t+\tau_i(\phi_r,\theta_r))$; DOA = směr maximální energie.
- (7) [s. 9] šířka pásma kolem k-té harmonické: $\Delta f_k=f_k/Q$ ($Q$ činitel jakosti).
- (8) [s. 10] SHC (spectral harmonic correlation, ref [34]): $SHC(t,f)=\sum_{f'=-L_w/2}^{L_w/2}\prod_{r=1}^{N_H+1}S(t,rf+f')$; $L_w$ šířka okna, $N_H$ počet harmonických, $S$ STFT.
- (9) [s. 11] kritérium kontinuity: $|\phi_t-\phi_{t-1}|\le20^\circ,\ |\theta_t-\theta_{t-1}|\le10^\circ$.
- Model signálu dronu: harmonická struktura s fundamentální frekvencí úměrnou otáčkám rotorů; slabé harmonické jsou liché násobky, dominantní sudé (první = blade passing frequency, BPF) [s. 4, podle [14]].

## Algoritmus
- Genetický algoritmus [s. 6-7]: gen = poloha mikrofonu; genom = 10 poloh; počáteční populace náhodná, 10 jedinců; řazení podle fitness (Eq. 3) při 350 Hz a směru (45 stupňů, 45 stupňů); křížení: nejlepší pole zachováno, ostatní děti složeny z prvních 5 mikrofonů otce a posledních 4 mikrofonů matky (kombinace v Tab. 2); mutace: jeden náhodný mikrofon nahrazen náhodnou polohou; 100 a 200 generací. Frekvence 350 Hz = možná harmonická BPF [s. 6-7].
- Zpracování TFR [s. 9-11]: (1) beamforming (DAS) do směru, (2) STFT fokusovaného signálu, (3) pitch tracking SHC (Eq. 8) pro hledání kandidátů fundamentální frekvence, (4) výběr TF binů kolem harmonických s šířkou $\Delta f_k=f_k/Q$ (Eq. 7), (5) energie vybraných binů pro směr, opakovat pro více směrů, DOA = argmax energie, (6) kritérium kontinuity (Eq. 9): kandidát fundamentály se testuje, dokud DOA nesplní podmínku vůči předchozímu odhadu [s. 11].
- Fitness GA a směr optimalizace (interpretace): článek neříká, zda se fit = lobe_width/MSL minimalizuje nebo maximalizuje. Protože Fig. 6 (škála MSL 0-12 dB) a text označují vyšší hodnoty MSL ("close to 6 dB") za "better", MSL je zřejmě odstup hlavního laloku od nejvyššího postranního (v dB) a fitness se minimalizuje; formulace "low MSL" v textu je nejednoznačná (u počátečního pole "four lobes and thus a low MSL", MSL = 2 dB a 1.2 dB v Tab. 1).
- Křížení (Tab. 2, s. 6): dítě 1 = nejlepší pole beze změny; děti 2-10 = prvních 5 mikrofonů otce + poslední 4 mikrofony matky; dvojice (otec, matka): (1,2), (1,3), (1,4), (1,5), (2,3), (2,4), (2,5), (3,4), (3,5).
- Parametry [s. 11]: úseky 5000 bodů s 50% překryvem; mřížka (4 stupně, 2 stupně) azimut/elevace; beamforming 4096 bodů; SHC 8092 bodů (jak je v textu; pravděpodobně překlep za 8192, interpretace; nelze ověřit); 10 harmonických; Q = 5 (tyto parametry jsou uvedeny u kruhového experimentu Sec. 4.2; pro Sec. 5 nejsou znovu specifikovány, interpretace). Úhlový rozměr mřížky: azimut krok 4 stupně, elevace krok 2 stupně ("A resolution of (4°, 2°) ... corresponding to azimuth and elevation"). Elevace je měřena od horizontu (v mapách Fig. 6 0 až 90 stupňů; elevace > 80 stupňů = zdroj nad polem). Hledání kandidátů od 150 Hz; u optimalizovaného pole "front" od 300 Hz (BPF kolem 175 Hz zamaskován šumem, přeskočena první harmonická, stále 10 harmonických) [s. 13-14].
- Počet zdrojů: jeden dron. Regularizace, váhování: neuvedeno. fs: neuvedeno (délka 5000 bodů v čase tedy nelze převést; interpretace).
- Výpočetní složitost: autoři ji nevyhodnotili: "Further work is needed to evaluate the computational complexity of this approach to determine its suitability for real-time applications." [s. 19].

## Experiment / simulace
- Dron: DJI Phantom 4 ("Phantom IV") [s. 4, 11]; v Altena 2026 je tento Phantom 4 uveden s hmotností 1380 g (interpretace, z jiného článku).
- Spektrogram (Fig. 1, s. 4): signál z 1. mikrofonu při vzletu na cca 2 m a přistání; BPF kolem 200 Hz; během letu fluktuuje mezi 100 a 170 Hz (kruhová trajektorie); u vyšších frekvencí kolem 800 Hz je vidět více stop (4 rotory) [s. 4, 11].
- Sec. 4.2 (s. 11-12): kruhová trajektorie (jeden kruh a menší kruh), porovnání klasického beamformingu, TFR s maximem SHC, TFR s kritériem kontinuity; GPS jako reference.
- Sec. 5.1 (s. 13-16): trajektorie startuje na zemi ~40 m od pole, výstup do 20 m, přímý přelet k poli, sestup, návrat; 4 měření (původní/optimalizované pole, dron před a za polem); rušení: ptáci, zrychlení aut.
- Sec. 5.2 (s. 17-18): dlouhá vzdálenost, optimalizované pole, otevřené pole u silnice, rušení: auta, ptáci, vítr (rychlost větru neuvedena); maximální vzdálenost dronu 600 m ("maximum distance traveled by the drone"); signál dronu není ve spektrogramu viditelný mezi 115 s a 216 s; šum pod 100 Hz; auta v časech 30-40 s, 118-135 s, 161-176 s, 191-216 s, 290-300 s [s. 17]. SNR neuvedeno; počet opakování neuvedeno.

## Výsledky (čísla)
- Tab. 1 [s. 6]: původní pole, faktor směrovosti (hodnoty z PDF)

| | 175 Hz, (-45, 20) stupňů | 350 Hz, (45, 45) stupňů |
|---|---|---|
| lobe_width_azi [stupně] | 52.5 | 32.3 |
| lobe_width_elev [stupně] | 80.3 | 47.6 |
| MSL [dB] | 2 | 1.2 |

- Fig. 4-6 [s. 7-8]: původní pole má při 350 Hz čtyři laloky a tedy nízký MSL; obě optimalizovaná pole mají "much better MSL"; GA 200 generací je nejlepší kompromis (blízké 6 dB MSL ve většině směrů); pro elevaci nad 80 stupňů zabírá azimutový lalok celou šířku u všech tří polí ("not very sensitive when sources are above them") [s. 7-8].
- Tab. 4 [s. 12]: RMSE vůči GPS, kruhová trajektorie

| Metoda | RMSE azimut [stupně] | RMSE elevace [stupně] |
|---|---|---|
| Classical beamforming | 111.2 | 35.9 |
| TFR maximum detection | 67.6 | 19.3 |
| TFR continuity criterion | 14.8 | 9.9 |

- Tab. 5 [s. 15]: průměr (µ), směrodatná odchylka (σ), RMSE chyb [stupně] (µ a σ a RMSE), trajektorie ze Sec. 5.1

| Konfigurace | Metoda | Azimut µ | Azimut σ | Azimut RMSE | Elevace µ | Elevace σ | Elevace RMSE |
|---|---|---|---|---|---|---|---|
| Initial, front | Classical beam | 79.8 | 76.3 | 110.4 | 15.3 | 15.0 | 21.4 |
| Initial, front | TFR | 4.2 | 3.7 | 5.6 | 2.7 | 2 | 3.4 |
| Optimized, front | Classical beam | 86.5 | 58.6 | 104.5 | 22.0 | 15.7 | 27.1 |
| Optimized, front | TFR | 8.1 | 6 | 9.7 | 3.8 | 2.3 | 4.4 |
| Initial, behind | Classical beam | 85.6 | 61.7 | 105.5 | 16.4 | 14.3 | 21.8 |
| Initial, behind | TFR | 6.7 | 3.8 | 7.7 | 2.7 | 2 | 3.3 |
| Optimized, behind | Classical beam | 63.2 | 73.6 | 96.9 | 12.8 | 13.2 | 18.5 |
| Optimized, behind | TFR | 7.6 | 5.5 | 9.4 | 4.7 | 3 | 5.5 |

- Dlouhá vzdálenost [s. 17]: dobré sledování DOA do 119.5 s = 341 m; azimut odpovídá GPS mezi 142.8 s a 162.1 s (vzdálenosti 594 m až 473 m), elevace v tomto úseku nepřesná; sledování se obnoví mezi 182.9 s a 189.3 s (244.5 m a 242.9 m); od 223 s (177.3 m) do konce správně; ztráty koincidují s přítomností auta. Závěr: lokalizace možná do 340 m při vzdalování a 240 m při návratu [s. 19]. Poznámka (interpretace): údaj 340 m vychází ze souvislého sledování do 119.5 s; azimut sám odpovídá GPS i na 473-594 m (142.8-162.1 s), tyto úseky autoři do "maximální vzdálenosti" nezapočítali, protože elevace byla nepřesná.
- Příklad Fig. 11 [s. 12]: při t = 9.25 s maximum SHC kolem 100 Hz dá nereálné DOA (-90 stupňů, 0 stupňů); druhý kandidát 180.7 Hz dá koherentní odhad.
- Klasický beamforming dává výrazně horší výsledek než TFR ve všech čtyřech konfiguracích [s. 14-15, Tab. 5].

## Omezení podle autorů
- Optimalizace GA provedena pro jednu frekvenci (350 Hz) a jeden směr (45, 45 stupňů); to může vysvětlovat, že původní pole je v DOA mírně lepší [s. 19].
- Výpočetní složitost TFR nebyla vyhodnocena; vhodnost pro real-time nejasná [s. 19].
- Testován jen jeden dron (Phantom 4); doporučeno testovat jiné drony [s. 19].
- Při šumu (auta, ptáci) a velké vzdálenosti dron "lost"; elevace v dlouhé vzdálenosti nepřesná [s. 17].
- Pole není citlivé na zdroje nad sebou (elevace > 80 stupňů) [s. 7-8].

## Relevance pro náš projekt
- SOTA: dobrý přehled kategorií metod (TDOA, maximální energie/beamforming, vysoké rozlišení/MUSIC) podle Lamotte [6] a srovnání GCC-PHAT vs. beamforming (ref [9] Altena 2023: GCC-PHAT rychlejší, ale méně přesný) [s. 2]; dosahy 160-250 m (Busset), 92-133 m vs. 104-152 m (SBL, Altena 2024), TFR do 340 m [s. 2, 19].
- Volba pásma: harmonická struktura dronu (BPF cca 100-200 Hz, 10 harmonických, tedy až do cca 1-2 kHz; interpretace/výpočet 10 x 175 Hz) potvrzuje smysluplnost pásma 300 Hz - 2 kHz; přesto jejich pole pracuje v pásmu 220.5-3430 Hz [s. 5].
- Metoda: harmonic-aware beamforming s kontinuitou trajektorie dramaticky zlepšuje klasický beamforming (RMSE azimutu 111.2 -> 14.8 stupňů, Tab. 4 [s. 12]). Pro naši lokalizaci na 100 ms po triggeru (jeden odhad) kritérium kontinuity nelze využít přímo (potřebuje historii); lze použít selektivní výběr harmonických (SHC), pokud trigger poskytne BPF. Poznámka (interpretace): detekční stupeň už může dodat odhad BPF.
- Geometrie: poznatek, že původní a GA pole dávají podobný výkon; GA optimalizace pro jedinou frekvenci a směr nemusí zlepšit DOA. Pro naše 2x8 UCA je relevantní jen metoda (fitness = šířka laloku / MSL, Eq. 3), ne geometrie (jejich pole je 3D řídké s rozpětím 1.556 m).
- Výpočetní nároky: autoři je neřeší (s. 19), takže je nelze citovat; STFT + SHC + mřížka (4 stupně x 2 stupně) je výpočetně náročnější než DAS (interpretace).
- Nelze přenést: velká rozpětí pole (1.556 m), dlouhé trajektorie, GPS reference, fs neuvedeno; metrika RMSE vůči GPS v pohybu vs. naše statické/krátké odhady.

## Citovatelná tvrzení
- Dosah: "The maximum distance between the antenna and the drone for which the method is able to give estimations is between 240 and 340 m." [s. 1].
- Přesnost: "Results show that the method used for the direction of arrival estimation can give a mean error below 10° in azimuth and 5° in elevation." [s. 1].
- Klasický beamforming je limitovaný: "Conventional beamforming is often used in source localization because of its robustness to noise, but this study shows that this method is very limited in terms of performance." [s. 19].
- Využití signatury: "taking into account the signature of the drone enables us to increase the signal-to-noise ratio and thus to enhance greatly the performance of the localization" [s. 19].
- Harmonická struktura: "Several studies showed that drones produce a harmonic signature with a fundamental frequency proportional to the rotation speed of the rotors" [s. 2].
- GCC-PHAT vs. beamforming (citace [9]): "GCC-PHAT is faster but less accurate than beamforming" [s. 2].
- Optimalizované pole není lepší v DOA: "In terms of localization performance, the two antennas are very close with the non-optimized performance being slightly better." [s. 19].
- Citlivost nad polem: "the arrays are not very sensitive when sources are above them" [s. 7].
- Výpočetní složitost: "Further work is needed to evaluate the computational complexity of this approach to determine its suitability for real-time applications." [s. 19].
- Ztráta při rušení: "It can be noted that the times when the drone is lost closely coincide with the presence of a car." [s. 17].

## Relevantní reference z článku
(přepsáno podle seznamu literatury, s. 19-21; DOI doplněny z cílů hyperlinků v PDF, v textu je jen "[CrossRef]"; [6], [9], [13], [18], [21], [24], [35] DOI nemají)
- [3] Yousaf, J.; Zia, H.; Alhalabi, M.; Yaghi, M.; Basmaji, T.; Shehhi, E.A.; Gad, A.; Alkhedher, M.; Ghazal, M. Drone and Controller Detection and Localization: Trends and Challenges. Appl. Sci. 2022, 12, 12612. DOI: 10.3390/app122412612
- [5] Ding, S.; Guo, X.; Peng, T.; Huang, X.; Hong, X. Drone Detection and Tracking System Based on Fused Acoustical and Optical Approaches. Adv. Intell. Syst. 2023, 5, 2300251. DOI: 10.1002/aisy.202300251
- [6] Lamotte, L.P.; Baron, V.; Bouley, S. UAV detection from acoustic signature: Requirements and state of the art. In Proceedings of the QUIET DRONES International e-Symposium on UAV/UAS Noise, Paris, France, 19-21 October 2020.
- [7] Van Veen, B.; Buckley, K. Beamforming: A versatile approach to spatial filtering. IEEE ASSP Mag. 1988, 5, 4-24. DOI: 10.1109/53.665
- [8] Schmidt, R. Multiple emitter location and signal parameter estimation. IEEE Trans. Antennas Propagat. 1986, 34, 276-280. DOI: 10.1109/TAP.1986.1143830
- [9] Altena, A.; Luesutthiviboon, S.; Croon, G.; Snellen, M.; Voskuijl, M. Comparison of Acoustic Localisation Techniques for Drone Position Estimation Using Real-World Experimental Data. In Proceedings of the 29th International Congress on Sound and Vibration, ICSV 2023, Prague, Czech Republic, 9-13 July 2023.
- [10] Shi, Z.; Chang, X.; Yang, C.; Wu, Z.; Wu, J. An Acoustic-Based Surveillance System for Amateur Drones Detection and Localization. IEEE Trans. Veh. Technol. 2020, 69, 2731-2739. DOI: 10.1109/TVT.2020.2964110
- [11] Wu, S.; Zheng, Y.; Ye, K.; Cao, H.; Zhang, X.; Sun, H. Sound Source Localization for Unmanned Aerial Vehicles in Low Signal-to-Noise Ratio Environments. Remote Sens. 2024, 16, 1847. DOI: 10.3390/rs16111847
- [12] Busset, J.; Perrodin, F.; Wellig, P.; Ott, B.; Heutschi, K.; Rühl, T.; Nussbaumer, T. Detection and tracking of drones using advanced acoustic cameras. In Proceedings of the SPIE Security + Defence, Toulouse, France, 21-24 September 2015; p. 96470F. DOI: 10.1117/12.2194309
- [13] Altena, A.; Snellen, M.; Luesutthiviboon, S.; de Croon, G.; Voskuijl, M. Sparse Bayesian Learning for Quadcopter Localisation. In Proceedings of the Quiet Drones, Manchester, UK, 8-11 September 2024.
- [14] Blanchard, T.; Thomas, J.H.; Raoof, K. Acoustic localization and tracking of a multi-rotor unmanned aerial vehicle using an array with few microphones. J. Acoust. Soc. Am. 2020, 148, 1456-1467. DOI: 10.1121/10.0001930
- [18] Blass, M.; Graz, F. A Real-Time System for Joint Acoustic Detection and Localization of UAVs. In Proceedings of the QUIET DRONES International Symposium on UAV/UAS Noise, Paris, France, 19-21 October 2020.
- [19] Itare, N.; Thomas, J.H.; Raoof, K.; Blanchard, T. Acoustic Estimation of the Direction of Arrival of an Unmanned Aerial Vehicle Based on Frequency Tracking in the Time-Frequency Plane. Sensors 2022, 22, 4021. DOI: 10.3390/s22114021
- [20] Yan, L.; Ma, W. Arrangements of phased microphone arrays for acoustic source localization based on deconvolution algorithms. J. Phys. Conf. Ser. 2018, 1065, 102002. DOI: 10.1088/1742-6596/1065/10/102002
- [21] Sarradj, E. A generic approach to synthesize optimal array microphone arrangements. In Proceedings of the 6th Berlin Beamforming Conference, Berlin, Germany, 29 February-1 March 2016.
- [24] Teng, P.; Lv, J. The optimization design of microphone array layout for wideband noise sources. In Proceedings of the 22nd International Congress on Acoustics, Buenos Aires, Argentina, 5-9 September 2016.
- [25] Liu, H.; Kirubarajan, T.; Xiao, Q. Arbitrary Microphone Array Optimization Method Based on TDOA for Specific Localization Scenarios. Sensors 2019, 19, 4326. DOI: 10.3390/s19194326
- [28] Yu, J.; Donohue, K. Optimal irregular microphone distributions with enhanced beamforming performance in immersive environments. J. Acoust. Soc. Am. 2013, 134, 2066-2077. DOI: 10.1121/1.4816540
- [29] Le Courtois, F.; Thomas, J.H.; Poisson, F.; Pascal, J.C. Genetic optimisation of a plane array geometry for beamforming. Application to source localisation in a high speed train. J. Sound Vib. 2016, 371, 78-93. DOI: 10.1016/j.jsv.2016.02.004
- [30] Lashi, D.; Quevy, Q.; Lemeire, J. Optimizing Microphone Arrays for Delay-and-Sum Beamforming using Genetic Algorithms. In Proceedings of the 2018 4th International Conference on Cloud Computing Technologies and Applications (Cloudtech), Brussels, Belgium, 26-28 November 2018; p. 5. DOI: 10.1109/CloudTech.2018.8713331
- [31] Khatami, I.; Abdollahzadeh Jamalabadi, M.Y. Optimal design of microphone array in a planar circular configuration by genetic algorithm enhanced beamforming. J. Therm. Anal. Calorim 2021, 145, 1817-1825. DOI: 10.1007/s10973-020-09994-0
- [34] Zahorian, S.A.; Hu, H. A spectral/temporal method for robust fundamental frequency tracking. J. Acoust. Soc. Am. 2008, 123, 4559-4571. DOI: 10.1121/1.2916590
- [35] Itare, N. Suivi Acoustique d'un Engin en vol en Environnement Bruité: Détection, Localisation et Caractérisation. Ph.D. Thesis, Le Mans Université, Le Mans, France, 2023.
- [15] Kloet, N.; Watkins, S.; Clothier, R. Acoustic signature measurement of small multi-rotor unmanned aircraft systems. Int. J. Micro Air Veh. 2017, 9, 3-14. DOI: 10.1177/1756829316681868
- [16] Djurek, I.; Petosic, A.; Grubesa, S.; Suhanek, M. Analysis of a Quadcopter's Acoustic Signature in Different Flight Regimes. IEEE Access 2020, 8, 10662-10670. DOI: 10.1109/ACCESS.2020.2965177
- [17] Kolamunna, H.; Dahanayaka, T.; Li, J.; Seneviratne, S.; Thilakaratne, K.; Zomaya, A.Y.; Seneviratne, A. DronePrint: Acoustic Signatures for Open-set Drone Detection and Identification with Online Data. Proc. ACM Interact. Mob. Wearable Ubiquitous Technol. 2021, 5, 20. DOI: 10.1145/3448115
- [22] Adhikari, K.; Vaccaro, R.J.; Sartori, D.D. Shift invariant sparse arrays and their optimal signal and noise subspaces. Signal Process. 2022, 198, 108579. DOI: 10.1016/j.sigpro.2022.108579
- [23] Fan, W.; Liang, J.; Fan, X.; So, H.C. A unified sparse array design framework for beampattern synthesis. Signal Process. 2021, 182, 107930. DOI: 10.1016/j.sigpro.2020.107930
- [26] Wielgus, A.; Szlachetko, B. A Simulation of Thinning of Microphone Array in Near-field Broadband Beamformers. Vib. Phys. Syst. 2021, 32, 2. DOI: 10.21008/J.0860-6897.2021.2.04
- [27] Bjelić, M.; Stanojević, M.; Šumarac Pavlović, D.; Mijić, M. Microphone array geometry optimization for traffic noise analysis. J. Acoust. Soc. Am. 2017, 141, 3101-3104. DOI: 10.1121/1.4982694
- [32] Bougaiov, N.; Danik, Y. Hough Transform for UAV's Acoustic Signals Detection. Adv. Sci. 2015, 6, 65-68. DOI: 10.15550/asj.2015.06.065 (základ syntetického signálu dronu v [14])
- [33] McCowan, I. Robust Speech Recognition Using Microphone Arrays. Ph.D. Thesis, Queensland University of Technology, Brisbane, Australia, 2001 (zdroj faktoru směrovosti, Eq. 2).
- Pozn.: [1] Abro et al. Drones 2022, 6, 284 (DOI 10.3390/drones6100284), [2] Pandey et al. IEEE Access 2022, 10, 112858-112897 (DOI 10.1109/ACCESS.2022.3215975) a [4] Svanström et al. Drones 2022, 6, 317 (DOI 10.3390/drones6110317) jsou přehledy hrozeb/fúze senzorů, pro beamforming nerelevantní.

## Kontrola (kolo 2)
- Opraveno: (1) s. 6: omezení GA "largest spacing of 120 cm" je nejednoznačné (od středu vs. mezi sousedními mikrofony), takže "nesoulad" s y = 1.25 m (Tab. 3, s. 8) nelze jednoznačně prohlásit za chybu; text upraven. (2) s. 9: Eq. 5 ověřena na vykreslené straně; skutečně je bez dělení c; upřesněno. (3) Poznámka o duplicitní textové vrstvě: jde o skrytou vrstvu verze "submitted" z 27. 1. 2025, čísla stran ověřena (Eq. 4-7 na s. 9, Tab. 3 na s. 8, Tab. 1 na s. 6, Tab. 4 na s. 12, Tab. 5 na s. 15). (4) Upřesněno, že parametry TFR (5000 bodů, 4096, 8092, Q = 5, 10 harmonických) jsou z Sec. 4.2 (s. 11) a pro Sec. 5 nejsou znovu uvedeny. Všechny citáty v "Citovatelná tvrzení", Tab. 1, Tab. 4, Tab. 5, Eq. 1-4, 6-9 a bibliografická data odpovídají PDF; reference [3]-[14], [18]-[21], [24], [25], [28]-[31], [34], [35] odpovídají seznamu literatury.
- Doplněno: DOI všech referencí s odkazem [CrossRef] (z cílů hyperlinků v PDF) a doplněny relevantní reference [15]-[17] v plném znění, [22], [23], [26], [27], [32], [33]; pozn. k jednotkám elevace (od horizontu); složení křížení z Tab. 2; interpretace směru fitness (minimalizace, MSL v dB roste = lepší); L = sqrt(2) l3; formulace fitness/mřížky/elevace; interval, kdy signál není viditelný (115-216 s); poznámka, že azimut sám sleduje dron i na 473-594 m.
- Nejistoty: fs, typ mikrofonu, bit depth, SNR, počet letů a rychlost větru nejsou v článku (parametry mohou být v [19]/[35], které tu nebyly ověřovány). "8092" u SHC je zřejmě překlep za 8192, ale z PDF nelze rozhodnout. Interpretace fitness (min/max) a významu "low MSL" je odvozená, ne uvedená. Význam "beamforming 4096 points" (délka FFT/okna?) není upřesněn. 120 cm vs. 1.25 m viz výše.
