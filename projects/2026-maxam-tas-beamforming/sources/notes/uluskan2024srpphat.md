# Uluskan 2024: SRP-PHAT kolem harmonických (SRP-Harmonics) pro lokalizaci a sledování dronu

Pozn. k číslování stran: [s. X] = strana časopisu (558 až 577); PDF strana = X − 557 (např. s. 561 = PDF 4). Titulní strana (s. 558) je PDF 1.

## Bibliografie
- Uluskan, S. "Precise acoustic drone localization and tracking via drone noise: Steered response power - phase transform around harmonics." *International Journal of Aeroacoustics* 2024, 23(5-6), 558-577. DOI: 10.1177/1475472X241259082. Received 15 Nov 2023, accepted 14 Apr 2024; 20 stran (558 až 577). Korespondenční adresa: Basın Şehitleri Cad. No: 152, Eskişehir 26140. K článku existuje Supplemental Material online a videa SRP map na YouTube (odkazy v titulcích Obr. 7-11). [s. 558]
- Pracoviště: Department of Motor Vehicles and Technologies, Vocational School of Transportation, Eskişehir Technical University, Turecko; jediný autor. [s. 558]
- Vydavatel: SAGE Publications (podle sagepub.com v záhlaví; název vydavatele výslovně neuveden). MDPI: ne (vydavatel není MDPI). Open access: neuvedeno (v PDF není licence CC; je tam jen "Article reuse guidelines: sagepub.com/journals-permissions"). Grant: Eskişehir Technical University BAP 24ADP051. [s. 558, 574]
- Návrh BibTeX:
```bibtex
@article{uluskan2024srpphat,
  author  = {Uluskan, Se{\c{c}}kin},
  title   = {Precise acoustic drone localization and tracking via drone noise: Steered response power - phase transform around harmonics},
  journal = {International Journal of Aeroacoustics},
  year    = {2024},
  volume  = {23},
  number  = {5-6},
  pages   = {558--577},
  doi     = {10.1177/1475472X241259082}
}
```

## Problém a přínos
- Problém: obyčejný SRP-PHAT, který sčítá GCC-PHAT přes celé spektrum, nelokalizuje dron spolehlivě (v experimentech neuspěl při žádné úrovni šumu). [s. 568]
- Přínos: (1) vlastní DFT "DFT-Harmonics" počítaná jen v okolí harmonických (včetně úzkopásmových náhodných složek kolem nich), (2) její integrace do SRP-PHAT = "SRP-Harmonics", (3) kombinace s Kaiserovým oknem (β_Kaiser = 10) odolná proti bílému šumu i větru, (4) sledování pohybujícího se dronu v 2D i 3D s odstraněním outlierů a LLS odhadem trajektorie. [s. 558-560, 568-573]

## Mikrofonní pole a hardware
- Audio rozhraní TASCAM US1800, mikrofony Audio-Technica AT2031, dron DJI Phantom 4. [s. 568, Obr. 6]
- 2D: mikrofony venku v řadě (lineárně), text říká "linearly with 3.30 m distance" (interpretace: rozteč sousedních mikrofonů; Obr. 7 to nepřímo potvrzuje, 4 mikrofony pokrývají přibližně od −5 do +5 m), výška 1,2 m (nastavitelné stojany); na Obr. 7 jsou označeny Mic#1 až Mic#4, tj. 4 mikrofony (z obrázku, interpretace; text počet 2D mikrofonů neuvádí). Počátek je střed mikrofonů, osa x je řada mikrofonů, dron visí ve stejné výšce 1,2 m. [s. 568, 570]
- 3D: "triangulární" geometrie, mikrofony na (−4,8; 0; 1,2), (4,8; 0; 1,2), (0; 0; 1,2) a (0; 3,2; 1,2) m. [s. 573]
- Záznam dronu s větrem: větrné ochrany (windscreens) záměrně sundány. [s. 569]
- Ground truth polohy stojícího dronu: změřeno 50m svinovacím metrem ("via 50 m measuring tape"); u pohybu trajektorie a rychlost z aplikace DJI GO. [s. 568, 570]
- Vzorkovací frekvence: pro záznam spektrogramu Phantomu 4 uvedeno 48 kHz [s. 566]; pro samotné pole explicitně neuvedeno. Bitová hloubka, synchronizace kanálů a kalibrace: neuvedeno.
- Omezení geometrie: dána počtem dostupných mikrofonů a délkou XLR kabelů. [s. 574]

## Signálový model a rovnice
- Obyčejná DFT (1), (2) a vztah frekvence k indexu $f=k\,f_s/N$ (3). [s. 560]
- DFT-Harmonics (4): $X(i)=\sum_{n=0}^{N-1}x(n)\,e^{-j2\pi h_i n/N}$, kde $h_i$ je posloupnost frekvenčních složek kolem harmonických. [s. 561]
- Kladná polovina (5): $\tilde h_i=H_{\lceil i/(2\beta/\Delta+1)\rceil}+\mathrm{mod}(i-1,\,2\beta/\Delta+1)\cdot\Delta-\beta,\ i=1,\dots,L$; počet složek kolem jedné harmonické $2\beta/\Delta+1$; celkem (6) $L=\left(\frac{2\beta}{\Delta}+1\right)M$; plná posloupnost včetně záporných (7). [s. 561]
- Zvolené parametry: $\Delta=0{,}1$, $\beta=3$ (rovnice 8: $(-H_M-3,-H_M-2{,}9,\dots,H_M+3)$); mohou se měnit podle kompromisu rychlost vs. přesnost; zvoleny, aby "comprehensively and densely include all the frequency components around harmonics" s ohledem na šířku harmonických dronu. $\Delta$ je rozlišení kolem harmonických, $\beta$ absolutní maximální odchylka od středu harmonické. Z rovnice (10) $f(i)=h_i f_s/N$ plyne, že $h_i$ je v jednotkách binů (interpretace: odchylka $\pm\beta=\pm3$ binů = $\pm3 f_s/N$ Hz). [s. 561]
- Inverze (9): $x(n)=\frac{\Delta}{N}\sum_{i=1}^{2L}X(i)\,e^{j2\pi h_i n/N}$; frekvence (10): $f(i)=h_i f_s/N$. [s. 561]
- GCC-PHAT (11), (12): $R_{x_1x_2}(k)=\frac{X_1(k)X_2^*(k)}{|X_1(k)X_2^*(k)|}$, $r_{x_1x_2}(n)=\mathcal{F}^{-1}\{R_{x_1x_2}(k)\}\approx\delta(n-n_d)$; zpoždění v sekundách $n_d/f_s$. [s. 562]
- Filtrace pomocí harmonických (13): $R'_{x_1x_2}(k)=R_{x_1x_2}(k)\cdot[W_\beta(k)*S_{H_1}(k)]$ ($W_\beta$ obdélníková funkce s mezí $\beta$, $S_{H_1}$ impulsní řada ve frekvenci s periodou $H_1$ = 1. harmonická, $*$ konvoluce). [s. 563-564]
- Časová korelace (14): $r'_{x_1x_2}(n)\approx\delta(n-n_d)*\left[\frac{2\beta}{H_1}\mathrm{sinc}\!\left(\frac{2\beta}{N}n\right)\cdot S_{(N/H_1)}(n)\right]$: zpožděná impulsní řada vynásobená sinc; vedlejší efekt: mřížka lokálních maxim (grid hyperbol). [s. 564, Obr. 2]
- Mapování bodu na TDOA (15): $n_{i,j}(\vec q)=\left\lfloor f_s\frac{\|\vec q-\vec b_i\|-\|\vec q-\vec b_j\|}{c}\right\rceil$ (zaokrouhlení na vzorek; $c$ rychlost šíření signálu, její hodnota v textu neuvedena). [s. 564]
- SRP-PHAT (16), (17): $P(\vec q)=\sum_{i=1}^{B-1}\sum_{j=i+1}^{B}r_{x_ix_j}(n_{i,j}(\vec q))$, $\hat q=\arg\max_{\vec q}P(\vec q)$ (B = počet mikrofonů). [s. 564]
- SRP-Harmonics (18): $P_{Harmonics}(\vec q)=\sum_{i=1}^{B-1}\sum_{j=i+1}^{B} r'_{x_ix_j}(n_{i,j}(\vec q))$, tedy stejný součet s $r'_{x_ix_j}$ z DFT-Harmonics. [s. 564]
- Kaiserovo okno: autor uvádí, že obdélníkové okno má vysoké postranní laloky (spektrální únik), takže nízkofrekvenční vítr s velkou amplitudou potlačí harmonické; $\beta_{Kaiser}=10$ postranní laloky větru silně potlačí. [s. 566-568, Obr. 5]

## Algoritmus
1. Okno signálu (0,5 s pro stacionární dron, 0,2 s v ukázkovém sledování, 0,1 s ve statistickém sledování); posun okna 0,1 s / 0,2 s / 0,05 s. [s. 568, 570-571]
2. Okno Kaiser s β_Kaiser = 10 (potlačuje postranní laloky větru, zvýrazňuje harmonické). [s. 566-568]
3. Odhad základní frekvence f0 (a tedy harmonických H_n) pro každé okno zvlášť: základní frekvence se "calculates and updates for each time window"; metodu v textu ilustruje autokorelace (pík 5,1 ms → 196 Hz). [s. 566, 568]
4. Počet harmonických M = 7 (uvedeno u ukázkového stacionárního experimentu s. 568; pro ostatní experimenty neuvedeno). [s. 568]
5. Kolem každé harmonické 2β/Δ+1 = 61 frekvenčních složek (Δ = 0,1, β = 3; interpretace: v binech; šířka oblasti v Hz je ±3·f_s/N, tedy při okně 0,5 s ±6 Hz a při okně 0,1 s ±30 Hz; vlastní dopočet). Celkem L = 61·7 = 427 složek na půlspektrum pro M = 7 (vlastní dopočet z rovnice 6). [s. 561]
6. GCC-PHAT pro všechny dvojice z DFT-Harmonics, součet přes dvojice na gridu hledání (SRP mapa), argmax. [s. 564]
7. Rozlišení SRP mapy: 0,05 m (stacionární experiment), 0,1 m (simulace). [s. 565, 568]
8. Tracking: při ≥ 5 odhadech shlukování (agglomerative hierarchical clustering) pro odstranění outlierů; LLS odhad trajektorie a rychlosti; průběžná aktualizace. [s. 571-572]
9. Zúžení oblasti výpočtu SRP-Harmonics na 4 × 4 m kolem bodu předpovězeného trajektorií. [s. 572]
- Předpočítané matice TDOA (rovnice 15) pro daný stálý geometrický setup. [s. 571, 574]
- Počet zdrojů: jeden (lokalizace více dronů je zmíněna jen jako možnost při různých f0). [s. 575]
- Regularizace/diagonal loading: neuvedeno (metoda není adaptivní beamformer).

## Experiment / simulace
- Simulace (Obr. 3): umělý periodický signál (ustálená samohláska /AA/, mužský hlas, 0,36 s), 4 mikrofony v (−10,−10), (−10,10), (10,−10), (10,10) m, AWGN, SNR bez šumu ("Inf dB"), −10 dB a −15 dB, cíl přibližně v (9; −3) m (odečet z obrázku), rozlišení mapy 0,1 m. [s. 565-566]
- Dron: DJI Phantom 4, dvoulistá vrtule; f0 = 196 Hz (autokorelační pík 5,1 ms), tj. 5880 ot/min; harmonické viditelné hlavně pod 1 kHz (spektrogram, záznam 48 kHz). [s. 566]
- Stacionární dron: vznáší se ve výšce 1,2 m (stejná jako mikrofony); ukázka v bodě (9; 14) m, záznam 11 s po vzletu. [s. 568]
- Statistika šumu: 55 pozic dronu v mřížce x od −12,5 do 12,5 m po 2,5 m (11 hodnot) a y od 10 do 18 m po 2 m (5 hodnot); dron visí ve výšce 1,2 m, ~10 s záznamu na pozici; okno 0,5 s, posun 0,1 s; metrika RMS (root mean squared) chyba lokalizace přes všechna okna. Osa chyby v Obr. 8 je popsána jen "RMS Localization Error" bez jednotky (interpretace: metry, v Obr. 8(c),(d) je "Localization Error (Distance)"). [s. 568, 571]
- Šum: (a) AWGN přidaný k datům mikrofonního pole, (b) uměle generovaný "větrný" šum jen pod 100 Hz, (c) přirozený vítr (bez větrné ochrany; asi 0 dB "wind noise" odhadnuto porovnáním PSD před a během větru; vítr po 7,5 s záznamu). [s. 568-569]
- Sledování: 96 lineárních letů s náhodným počátečním bodem a rychlostí 1,5 až 10 m/s (skupiny 1: 1,5-4 / 2: 4-7 / 3: 7-10 m/s; různá síla výchylky páky ovladače); trajektorie a rychlost z aplikace DJI GO (ground truth). Okno 0,1 s, posun 0,05 s. [s. 570-572]
- 3D sledování: 65 lineárních letů, výška 1,2 až 6 m, rychlosti 4 až 10 m/s (skupiny 2 a 3); ukázka 5,9 m/s. [s. 573]
- Vzdálenosti: dron v rozsahu přibližně 10 až 18 m (y) a ±12,5 m (x) od řady mikrofonů; explicitní "dosah" neuveden. [s. 568]

## Výsledky (čísla)
- Simulace /AA/ vzorek: SRP-PHAT lokalizuje při bez šumu a při −10 dB (maximum mapy u cíle), při −15 dB maximum na falešné poloze; SRP-Harmonics při −15 dB stále maximum u cíle. [s. 566, Obr. 3]
- Stacionární dron, AWGN ("up to X dB SNR" znamená podle Obr. 8, kde osa SNR klesá zprava doleva, že úspěch trvá, dokud SNR neklesne k X dB; interpretace): SRP-Harmonics úspěšný do 40 dB SNR; SRP-PHAT neúspěšný při jakékoli úrovni šumu; SRP-PHAT s Kaiserem (β = 10) lepší, ale stále ne efektivní; SRP-Harmonics s Kaiserem úspěšný do 30 dB SNR. [s. 568-569, Obr. 8(a)]
- Umělý vítr (< 100 Hz): SRP-Harmonics úspěšný do 10 dB SNR, SRP-Harmonics s Kaiserem do 0 dB SNR; SRP-PHAT (i s Kaiserem) nelokalizuje přesně. [s. 569, Obr. 8(b)]
- Přirozený vítr (kolem 0 dB): výkon SRP-Harmonics klesá, s Kaiserem se úspěšnost udrží. [s. 569-570, Obr. 8(c),(d)]
- Orientační odečet z Obr. 8 (grafy, interpretace, ±0,5): AWGN, plató při SNR ≥ 40 dB: SRP-PHAT přibližně 8,9, SRP-PHAT s Kaiserem přibližně 4,1, SRP-Harmonics i SRP-Harmonics s Kaiserem přibližně 0,3 až 0,4; při 20 dB SRP-Harmonics přibližně 5,4 a s Kaiserem přibližně 2,1; při 10 dB všechny přibližně 12,5. Umělý vítr: SRP-PHAT přibližně 8,9, s Kaiserem přibližně 4,0; SRP-Harmonics přibližně 0,3 při SNR ≥ 10 dB, přibližně 6,8 při 0 dB, přibližně 12,7 při −10 dB; s Kaiserem přibližně 0,3 při SNR ≥ 0 dB, přibližně 2,1 při −10 dB. Jednotka osy neuvedena. Číselné hodnoty RMS chyby nejsou v textu; "úspěch" je definován graficky, hranice chyby neuvedena. [s. 571, Obr. 8]
- Sledování pohybujícího se dronu (2,85 m/s, okno a posun 0,2 s): jen 2 odlehlé odhady ve vzorovém letu. [s. 570]
- Čas: průměrná doba lokalizace 0,034 s na Intel Core i7 (turbo 4,6 GHz) při okně 0,1 s a posunu 0,05 s, jestliže jsou TDOA matice předpočítané; autoři to označují za vhodné pro real-time. [s. 571]
- Podíl odlehlých odhadů roste s rychlostí (skupiny 1, 2, 3). Orientačně z boxplotů (interpretace, odečteno z grafu, ne z textu): při počítání v celém rozsahu mediány přibližně 18 %, 45 %, 52 %; při zúžení na 4 × 4 m přibližně 2 až 3 %, 5 %, 8 %. RMSE rychlosti (mediány, přibližně): celý rozsah 0,05 / 0,35 / 0,65 m/s, zúžení 0,02 / 0,1 / 0,15 m/s; RMS chyba vstupního/výstupního bodu: celý rozsah přibližně 0,35 / 0,65 / 0,95 m, zúžení přibližně 0,4 / 0,6 / 0,85 m. [s. 573, Obr. 10]
- Zúžení oblasti 4 × 4 m výrazně snižuje podíl outlierů a podstatně snižuje chyby rychlosti, mírně chybu vstupního/výstupního bodu. [s. 572-573]
- 3D sledování: odhad parametrů trajektorie "adequately" (box ploty Obr. 11(c)); chyby 3D jsou "slightly higher" než 2D. Orientačně z Obr. 11(c), 2D vs 3D: odlehlé odhady přibližně 6 % vs 11 %, RMSE rychlosti přibližně 0,13 vs 0,22 m/s, chyba vstupního bodu přibližně 0,7 vs 0,8 m. Ukázkový 3D let 5,9 m/s, isoplochy na 50 % globálního maxima. [s. 573-574, Obr. 11]

## Omezení podle autorů
- Měřicí geometrie omezena počtem mikrofonů a délkou XLR kabelů; dosah lze zvětšit více mikrofony a vhodnějšími venkovními mikrofony. [s. 574-575]
- Pohyb dronu uvnitř okna způsobuje nestabilitu TDOA a outliery; s rychlostí roste podíl outlierů a chyba. [s. 570, 572]
- Real-time proveditelnost podmíněna předpočítanými TDOA maticemi pro stálou geometrii. [s. 571, 574]
- Mřížka lokálních maxim (vedlejší efekt DFT-Harmonics) může podle autora na první pohled mást, ale "the local maxima will not create any significant issue". [s. 564]
- Autor konstatuje, že SRP-Harmonics je určena pro signály s výraznými harmonickými; pro vícenásobné drony: možné, když mají různé f0 (jen tvrzení, netestováno). [s. 565, 575]
- Pozn. (interpretace): testován jen jeden typ dronu (Phantom 4) se známými vlastnostmi harmonických; vyžaduje přítomnost zřetelných harmonických.

## Relevance pro náš projekt
- SOTA sekce: přímý důkaz, že pro SRP-PHAT je u dronu rozhodující výběr frekvencí (harmonické, ne celé spektrum) a okno (Kaiser); běžný SRP-PHAT přes celé pásmo selhal. Použitelné jako argument pro pásmové/harmonické omezení v našem SRP-PHAT.
- Volba pásma: harmonické Phantomu 4 jsou výrazné pod 1 kHz (s. 566), f0 = 196 Hz; pásmo 300 Hz až 2 kHz tedy pokrývá asi 2. až 10. harmonickou (interpretace: 2×196 = 392 Hz, 10×196 = 1960 Hz). Vítr je podle autora nízkofrekvenční (< 100 Hz); naše dolní mez 300 Hz ho proto z velké části vynechá (interpretace).
- Délka okna: 0,1 s okno s posunem 0,05 s je v článku použito a funguje při 0,034 s výpočtu na PC (s. 570-571), což odpovídá našemu 100 ms úseku po triggeru; frekvenční rozlišení okna 0,1 s je 10 Hz, tj. při f0 ≈ 200 Hz leží harmonické jen několik binů od sebe (interpretace); odhad f0 z 100 ms je třeba ověřit.
- Geometrie 2×8 UCA: nepřenositelné; 4 mikrofony v metrových rozestupech a hyperbolická lokalizace v hledací ploše (blízké pole, desítky metrů). Naše pole je kompaktní (160 až 250 mm), tedy úhlová lokalizace (DOA), ne triangulace.
- Volba metody: myšlenku "odebírat jen harmonické kolem f0" lze využít ve variantě SRP-PHAT (a pro vážení pásem v DAS/MVDR); pro MUSIC/CSSM/WAVES/TOPS neposkytuje porovnání.
- Výpočetní nároky na MCU: čísla (0,034 s) jsou z Intel i7 4,6 GHz s předpočítanými TDOA tabulkami v RAM; na STM32H563 by taková tabulka pro grid × 120 dvojic (16 mikrofonů) byla paměťově náročná (interpretace, nutno spočítat). DFT-Harmonics s M × 61 frekvencemi je vlastně neekvidistantní DFT (přímý výpočet, ne FFT).
- Nelze přenést: absolutní výsledky (chyby v metrech), jeden dron, 4 mikrofony, pozemní lineární řada, dron ve výšce 1,2 m (u 2D).

## Citovatelná tvrzení
- Harmonické souvisí s otáčkami a počtem listů: "The harmonic components are directly related to the rotational speed of the propeller and the number of blades." [s. 559]
- Okolí harmonických je důležité: Djurek et al. "stated that attempting to build acoustic signatures for drones based on only the harmonics and their ratios is not adequate." [s. 559-560]
- Princip SRP-Harmonics: "DFT-Harmonics only and densely concentrates on the vicinities of harmonics." [s. 560]
- Fundamentální frekvence Phantomu 4: "the fundamental frequency of the drone sound is 196 Hz (i.e. 5880 rpm of motor by keeping in mind that the propeller has two blades)". [s. 566]
- Obyčejný SRP-PHAT selhává: "the ordinary SRP-PHAT can not effectively localize the drone in any noise level." [s. 568]
- Odolnost proti AWGN: "SRP-Harmonics after Kaiser Window maintains its initial success up to 30 dB SNR." [s. 569]
- Vítr: "Random noise signals are created which include frequency components only below 100 Hz to simulate the wind noise." [s. 569]
- Výpočetní čas: "the average execution time of localization with these specifications is found to be 0.034 s" [s. 571] (při předpočítaných TDOA maticích, Intel i7, okno 0,1 s).
- Rušivý pohyb: "The relative distances of the moving drone to each microphone keep changing within a time window, therefore this issue can occasionally confuse the localization algorithm and produce outliers." [s. 570]
- Praktické omezení: "there were constraints on the measurement geometry in this study" kvůli počtu mikrofonů a délce XLR kabelů [s. 574].

## Relevantní reference z článku
- 20. Chang X, Yang C, Wu J, et al. A surveillance system for drone localization and tracking using acoustic arrays. In 10th sensor array and multichannel signal processing workshop (SAM), Sheffield, UK, 8–11 July 2018, pp. 573–577.
- 21. Guo J, Ahmad I and Chang K. Classification, positioning, and tracking of drones by HMM using acoustic circular microphone array beamforming. EURASIP J Wirel Commun Netw 2020; 2020: 9–19.
- 22. Famili A, Stavrou A, Wang H, et al. Rail: robust acoustic indoor localization for drones. In: IEEE 95th vehicular technology conference, Helsinki, Finland, 19–22 June 2022, pp. 1–6.
- 23. Fang J, Li Y, Ji PN, et al. Drone detection and localization using enhanced fiber-optic acoustic sensor and distributed acoustic sensing technology. J Lightwave Technol 2023; 41(3): 822–831.
- 27. Yang C, Sun LL, Guo H, et al. A fast 3D-MUSIC method for near-field sound source localization based on the bat algorithm. Int J Aeroacoustics 2022; 21(3–4): 98–114.
- 5. Taha B and Shoufan A. Machine learning-based drone detection and classification: state-of-the-art in research. IEEE Access 2019; 7: 138669–138682.
- 28. Magliozzi B, Hanson DB and Amiet RK. Propeller and propfan noise. In: Hubbard HH (ed) Aeroacoustics of flight vehicles: theory and practice. Volume 1: noise sources. Washington, DC: NASA, 1991, pp. 1–61.
- 29. Pham T, Sadler B, Fong M, et al. High-resolution acoustic direction-finding algorithm to detect and track ground vehicles. In 20th army science conference. Singapore: World Scientific, 1997, pp. 16–20.
- 30. Yang C, Wu Z, Chang X, et al. DOA estimation using amateur drones harmonic acoustic signals. In: IEEE 10th sensor array and multichannel signal processing workshop (SAM), Sheffield, UK, 8–11 July 2018, pp. 587–591.
- 31. Chang X, Yang C, Shi X, et al. Feature extracted DOA estimation algorithm using acoustic array for drone surveillance. In: 87th vehicular technology conference, Porto, Portugal, 3–6 June 2018, pp. 1–5.
- 34. Sedunov A, Haddad D, Salloum H, et al. Stevens drone detection acoustic system and experiments in acoustics UAV tracking. In IEEE international symposium on technologies for homeland security, Woburn, MA, 5–6 November 2019, pp. 1–7.
- 35. Shi Z, Chang X, Yang C, et al. An acoustic-based surveillance system for amateur drones detection and localization. IEEE Trans Veh Technol 2020; 69(3): 2731–2739.
- 36. Itare N, Thomas JH, Raoof K, et al. Acoustic estimation of the direction of arrival of an unmanned aerial vehicle based on frequency tracking in the time-frequency plane. Sensors 2022; 22(11): 4021.
- 37. Djurek I, Petosic A, Grubesa S, et al. Analysis of a quadcopter's acoustic signature in different flight regimes. IEEE Access 2020; 8: 10662–10670.
- 38. Proakis JG and Manolakis DG. Digital signal processing: principles, algorithms, and applications. Upper Saddle River, NJ: Prentice-Hall, Inc, 1996.
- 39. Knapp C and Carter G. The generalized correlation method for estimation of time delay. IEEE Trans Acoust 1976; 24(4): 320–327.
- 40. Pine KC, Pine S and Cheney M. The geometry of far-field passive source localization with TDOA and FDOA. IEEE Trans Aero Electron Syst 2021; 57(6): 3782–3790.
- 41. Vera-Diaz JM, Pizarro D and Macias-Guarasa J. Acoustic source localization with deep generalized cross correlations. Signal Process 2021; 187: 108169.
- 42. Cobos M, Marti A and Lopez JJ. A modified SRP-PHAT functional for robust real-time sound source localization with scalable spatial sampling. IEEE Signal Process Lett 2011; 18(1): 71–74.
- Pozn.: [20]-[23] jsou přepsány včetně původní interpunkce; číslo 21 má v seznamu stránkování "2020: 9–19" (tak je to v PDF). DOI v seznamu literatury nejsou uvedena u žádné z položek; reference [5] je zde navíc jako přehledová práce o detekci/klasifikaci dronů.

## Kontrola (kolo 2)
- Opraveno: (1) simulace Obr. 3 měla v poznámce SNR "0, −10, −15 dB"; v PDF je bez šumu ("Inf dB"), −10 dB a −15 dB [s. 565]; (2) "up to X dB SNR" doplněno výkladem směru podle osy Obr. 8 (úspěch do poklesu SNR k X dB) [s. 571]; (3) věta o neopsaných grafech nahrazena orientačním odečtem z Obr. 8, 10 a 11 [s. 571, 573, 574]; (4) odečet outlierů v Obr. 10(b) upřesněn na 2 až 3 %, 5 %, 8 %; (5) formulace "0,1 s v statistickém sledování" opravena; (6) MDPI/open access upřesněno; (7) M = 7 omezeno na ukázkový experiment (jinde nestanoveno); (8) rovnice (18), (13), (15) doplněny přesněji; (9) ověřeno: číslování stran (PDF = časopis − 557), všechny citáty (s. 559, 560, 566, 568, 569, 570, 571, 574) jsou doslovné, rovnice 1-18 odpovídají; reference [20]-[42] odpovídají seznamu.
- Doplněno: ground truth (50m pásmo, DJI GO); výška dronu 1,2 m; počátek souřadnic; definice Δ, β, rozsahu v binech a Hz pro naše okno (vlastní dopočet, označeno); L = 427 pro M = 7; Kaiserovo okno a vítr; Obr. 8 hodnoty; Obr. 10 a 11 hodnoty; reference [5], [38], [40]; kontext supplemental materials/videa.
- Nejistoty: fs pro pole, bitová hloubka, synchronizace, kalibrace a rychlost zvuku c nejsou uvedeny; jednotka chyby v Obr. 8 není uvedena; počet mikrofonů ve 2D (4) je jen z Obr. 7; "3,30 m distance" nemusí být rozteč sousedních mikrofonů (jen z obrázku konzistentní); všechna čísla z grafů jsou orientační odečty (±10 až 15 %); způsob odhadu f0 není popsán (jen autokorelace jako ilustrace); open access nelze v PDF ověřit.
