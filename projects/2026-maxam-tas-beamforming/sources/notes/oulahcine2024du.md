# Oulahcine et al. 2024: Diagonal Unloading pro lokalizaci mini-dronu kruhovým mikrofonním polem

## Bibliografie
- Plná citace (podle PDF, s. 1): Dhiya Eddine Rabia Oulahcine, Nacerredine Lassami, Mustapha Benssalah, Khalil Azzoune, Ahcen Mohamed Nassim Si Salah. "Diagonal Unloading Algorithm for Mini-Drone Localization Using Circular Microphone Array". 2024 8th International Conference on Image and Signal Processing and their Applications (ISPA), IEEE, 2024. ISBN/ISSN řádek: 979-8-3503-0924-9/24/$31.00. DOI: 10.1109/ISPA59904.2024.10536780. Strany v sborníku: neuvedeno (PDF má 5 stran, číslované 1-5). Měsíc konání: neuvedeno. Klíčová slova (Index Terms): Mini-drone, Diagonal unloading algorithm, Acoustic localisation, Circular microphone array [s. 1]. Všech pět autorů: Lab. Traitement du signal, Ecole militaire polytechnique, BP 17 Bordj El Bahri, Alžírsko.
- Afiliace: Lab. Traitement du signal, Ecole militaire polytechnique, BP 17 Bordj El Bahri, Algeria [s. 1].
- Vydavatel: IEEE (©2024 IEEE) [s. 1]. MDPI: ne. Open access: neuvedeno (PDF je stažený přes IEEE Xplore s licencí "Authorized licensed use limited to: CZECH TECHNICAL UNIVERSITY", pravděpodobně není OA; interpretace).
- Návrh BibTeX:
```bibtex
@inproceedings{oulahcine2024du,
  author    = {Oulahcine, Dhiya Eddine Rabia and Lassami, Nacerredine and Benssalah, Mustapha and Azzoune, Khalil and Si Salah, Ahcen Mohamed Nassim},
  title     = {Diagonal Unloading Algorithm for Mini-Drone Localization Using Circular Microphone Array},
  booktitle = {2024 8th International Conference on Image and Signal Processing and their Applications (ISPA)},
  publisher = {IEEE},
  year      = {2024},
  doi       = {10.1109/ISPA59904.2024.10536780}
}
```

## Problém a přínos
- Cíl: "operationalize and validate an acoustic platform designed for mini drones localization" pomocí kruhového mikrofonního pole a algoritmu diagonal unloading (DU) pro DOA [s. 1, Abstract].
- Přínos: DU (Salvati et al., refs [6]-[8]) se vyhodnocuje na dronu (simulace s RIR image-source metodou + experiment s reálným dronem) a srovnává se s MUSIC, MVDR (s diagonal loading), SRP a SRP-PHAT; tvrzení: DU dává výsledky srovnatelné s MUSIC při nižší výpočetní složitosti [s. 1, Abstract; s. 4, Conclusion].
- Výpočetní složitost však není kvantifikována (žádné FLOPs ani časy), jen tvrzena (interpretace: nutno brát jako kvalitativní).

## Mikrofonní pole a hardware
- Simulace: UCA, M = 8 stejnoměrně rozložených mikrofonů, FFT okno 1024 bodů; poloměr pole v simulaci neuvedeno [s. 3].
- Experiment: UCA s 8 mikrofony, poloměr r = 5 cm (tj. průměr 10 cm), mikrofony Gras 46BE, dvě akviziční karty Data Translation 9837B (každá 4 analogové vstupy), fs = 44.1 kHz, c = 343 m/s, T = 5 snapshotů [s. 4, Sec. IV].
- Bit depth, synchronizace obou karet, kalibrace mikrofonů, předzesilovače: neuvedeno.
- Vlastní výpočet (interpretace): vzdálenost sousedních mikrofonů u r = 5 cm a M = 8 je 2 r sin(pi/8) ≈ 3.83 cm; pole je tedy výrazně menší než naše (160-250 mm).

## Signálový model a rovnice
- (1) [s. 2]: $x_m(t,f)=\sum_{k=1}^{K} e^{-j2\pi f \tau_m(\theta_k,\phi_k)} s_k(t,f)+n_m(t,f)$; šum prostorově bílý Gaussovský, rozptyl $\sigma^2$.
- (2) [s. 2] steering vektor UCA: $a(f,\theta_k,\phi_k)=\left[e^{-j2\pi r f\cos(\theta_k-\Phi_i)\cos(\phi_k)/c}\right]_{i=1..M}$, $\Phi_i=2\pi i/M$; azimut $\theta\in[0,2\pi]$, elevace $\phi\in[0,\pi/2]$ (text s. 2: úhly jsou měřeny "from the x to y axes and from x to z axes", tj. elevace od horizontály, což potvrzuje cos(phi) ve fázi).
- (3) [s. 2]: $X(t,f)=A(f,\Omega)S(t,f)+N(t,f)$.
- (4) [s. 2]: $R_{xx}(f)=E[X X^H]=A R_{ss} A^H+\sigma_n^2 I$, nekorelované zdroje, $R_{ss}=\mathrm{diag}(P_1^2,\dots,P_K^2)$.
- (5) [s. 2]: $\hat R_{xx}(f)=\frac1T\sum_{i=1}^{T} X(i,f)X^H(i,f)$, T = počet snapshotů (podle textu odhad "maximum likelihood").
- (6) [s. 2] DU spektrum: $P_{DU}(f,\Omega)=\dfrac{1}{a(f,\Omega)^H(\lambda I-R_{xx}(f))a(f,\Omega)}$, kde $\lambda$ je největší vlastní číslo (podprostor nejsilnějšího zdroje), určené power method (Algorithm 1, s. 3); podle textu je výraz převzat z [8].
- (7) [s. 3] širokopásmová fúze (norm transform frequency fusion, ref [9]): $P_{DU}(\Omega)=\sum_{f=f_{min}}^{f_{max}}\dfrac{P_{DU}(f,\Omega)}{\max|P_{DU}(f,:)|}$.
- (8) [s. 3]: $\Omega_{estimate}=\arg\max_\Omega P_{DU}(\Omega)$ (DOA nejsilnějšího zdroje).
- (9) [s. 3] RMSE: $\mathrm{RMSE}=\sqrt{\frac1N\sum_{i=1}^{N}(\theta_i-\hat\theta_i)^2+(\phi_i-\hat\phi_i)^2}$ (společná chyba azimutu a elevace; ověřeno na vykreslené s. 3: odmocnina zahrnuje celý výraz, tj. $\sqrt{\frac1N\sum_i[(\theta_i-\hat\theta_i)^2+(\phi_i-\hat\phi_i)^2]}$, společná úhlová RMSE; že součet pokrývá oba členy, plyne jen z typografie (pravděpodobné); průměrování přes N = 200 pokusů potvrzuje text).

## Algoritmus
- Kroky (interpretace sestavená z s. 2-3): (1) STFT/FFT 1024 bodů; (2) pro každý frekvenční bin odhad $\hat R_{xx}(f)$ z T snapshotů (Eq. 5); (3) power method pro $\lambda$; (4) DU pseudospektrum (Eq. 6) na úhlové mřížce; (5) normalizace a součet přes pásmo (Eq. 7); (6) argmax (Eq. 8).
- Power method (Algorithm 1, s. 3): iterace $V\leftarrow R V_{old}$, $\lambda\leftarrow\|V\|$, $V_{new}\leftarrow V/\|V\|$, konec při $\|V_{new}-V_{old}\|<\epsilon$; vstup R, parametry $N_t$, $\epsilon$, inicializace $V_{old}$ (způsob inicializace není uveden); $\epsilon=10^{-3}$, max. počet iterací $N_t=200$ [s. 3].
- MVDR: diagonal loading s parametrem 0.1 [s. 3]. Srovnávané metody: SRP, SRP-PHAT [10] (Cobos), MVDR s diagonal loading [11] (Fertig), MUSIC [12] (Li et al., spherical harmonics MUSIC vs. conventional MUSIC); širokopásmová fúze podle [9]. V závěru (s. 4) autoři zmiňují porovnání MVDR, MUSIC a DU, v textu i SRP a SRP-PHAT.
- Počet zdrojů: DU hledá jen nejsilnější zdroj [s. 3]; počet zdrojů K v modelu obecně.
- Pásmo $f_{min}$-$f_{max}$: neuvedeno. Úhlová mřížka: neuvedeno (osy Fig. 6 [s. 4] jdou přibližně 0-350 stupňů v azimutu a 0-85 stupňů v elevaci; krok neznámý).
- Okno a překryv: FFT 1024 bodů [s. 3]; překryv neuvedeno. Při fs = 44.1 kHz odpovídá 1024 bodů ≈ 23.2 ms (vlastní výpočet, interpretace); T = 5 snapshotů tedy dle uvedeného nastavení je krátký úsek.

## Experiment / simulace
- Simulace: Monte Carlo, N = 200 pokusů, v každém akustická místnost simulovaná image-source metodou (ref [13] Lehmann a Johansson) a signál dronu z datasetu [14] (Al-Emadi et al., 2019) v náhodném směru ("in a random direction in elevation and azimuth"; rozsah úhlů se předpokládá z modelu, azimut 0-2π a elevace 0-π/2, s. 2) [s. 3]. Rozměry místnosti, poloha zdroje (vzdálenost), vzorkovací frekvence simulace, typ dronu: neuvedeno.
- Parametry: Fig. 2 (RMSE vs. SNR) při T = 5 snapshotů a RT60 = 0.0 s; Fig. 3 (RMSE vs. počet snapshotů) při SNR = 5 dB; Fig. 4 (RMSE vs. RT60) [s. 3, 4]. Rozsahy os: SNR od -10 do 20 dB (Fig. 2), T od 1 do 10 (Fig. 3), RT60 od 0 do 1 s (Fig. 4).
- Experiment: vnitřní prostředí, létající dron jako zdroj, 8 mikrofonů, bez ground truth (jen mapa Fig. 6) [s. 4]. Fig. 5 (s. 4) ukazuje laboratoř, malý kvadrokoptérový dron ve vzduchu a pole na stojanu s notebookem. Model dronu, vzdálenost, výška, BPF, pásmo, počet měření, rozměry místnosti a RT60: neuvedeno (z textu ani z Fig. 5 nelze odečíst).

## Výsledky (čísla)
- Pro SNR > 0 dB zůstává RMSE metod DU a MUSIC pod 1 stupněm ("consistently remains below 1 degree") [s. 3]; v grafu Fig. 2 je to při SNR = 5 dB těsně kolem 1 stupně. Pro SNR < 0 dB nárůst RMSE, vysvětlení autorů: "the alignment of the noise subspace with larger eigenvalues" [s. 3].
- Hodnoty odečtené z grafů jsou přibližné (interpretace, z obrázků vykreslených při 200 dpi, nikoliv z textu; nejistota odečtu ≈ ±0.1 stupně u Fig. 3-4, ≈ ±1 stupeň u Fig. 2 při -10 dB):

| Obrázek | Podmínka | DU | MUSIC | SRP | SRP-PHAT | MVDR |
|---|---|---|---|---|---|---|
| Fig. 2 [s. 3] | SNR = -10 dB, T = 5, RT60 = 0 | ≈ 16.5 stupňů | ≈ 17.3 | ≈ 27 | ≈ 22.5 | ≈ 30.5 |
| Fig. 2 [s. 3] | SNR = 0 dB | ≈ 2 | ≈ 2 | ≈ 2.5 | ≈ 2 | ≈ 2.7 |
| Fig. 3 [s. 3] | T = 1, SNR = 5 dB | ≈ 1.87 | ≈ 1.63 | ≈ 2.07 | ≈ 2.07 | ≈ 2.2 |
| Fig. 3 [s. 3] | T = 10, SNR = 5 dB | ≈ 0.4 | ≈ 0.25 | ≈ 0.55 | ≈ 0.5 | ≈ 0.6 |
| Fig. 4 [s. 4] | RT60 = 1 s | ≈ 1.26 | ≈ 1.33 | ≈ 2.63 | ≈ 2.3 | ≈ 2.3 |

  Pozorování z grafů (interpretace): ve Fig. 3 je MUSIC pro všechna T pod DU (T = 10: ≈ 0.25 vs. ≈ 0.4 stupně), "comparable" tedy platí s odstupem 0.1-0.25 stupně; ve Fig. 4 je při RT60 ≥ 0.9 s DU zhruba stejné nebo mírně lepší než MUSIC (≈ 1.26 vs. 1.33 při 1 s); při SNR = -5 dB jsou DU/MUSIC/SRP/SRP-PHAT kolem 4-5 stupňů, MVDR ≈ 7.8. RMSE je společná chyba azimutu a elevace (Eq. 9), ne chyba jednoho úhlu.

- RT60: DU má srovnatelný výkon s MUSIC a "superior performance compared to the MVDR and SRP PHAT techniques" [s. 3-4, Fig. 4]. Poznámka (interpretace): při RT60 = 0 až 0.3 s jsou rozdíly mezi metodami v grafu malé (≈ 0.3 až 0.8 stupně).
- Experiment: akustická mapa (Fig. 6) ukazuje úzký hlavní lalok bez postranních laloků ("without the presence of secondary lobes"; "the localization peak is narrow") [s. 4]; maximum vizuálně kolem azimutu ≈ 115 stupňů a elevace ≈ 50 stupňů (rozmezí asi 45-55; odečet z obrázku, interpretace; opravena původní hodnota 55-60). Číselná chyba vůči referenci neuvedena.

## Omezení podle autorů
- Autoři explicitní sekci omezení nemají. Výsledek v experimentu je popsán pouze jako "satisfactory results" [s. 4].
- Z textu plyne (interpretace): metoda lokalizuje jen nejsilnější zdroj; při SNR < 0 dB se chyba prudce zvyšuje [s. 3]; výpočetní složitost není měřena.

## Relevance pro náš projekt
- SOTA sekce: ukázka, že pro UCA s 8 mikrofony na dronu lze srovnávat DAS (SRP), SRP-PHAT, MVDR, MUSIC a DU; je to jedno z mála srovnání přímo s 8mikrofonovým UCA a stejnou sadou metod jako my [s. 1, 3].
- Volba metody a MCU: DU = jedna power-method iterace místo plného eigendekompozice (vs. MUSIC) a žádná inverze matice (vs. MVDR); pro STM32H563 (8x8 kovarianční matice na bin) je to potenciálně zajímavé (interpretace). Autoři ale neposkytují měření složitosti, tedy nelze citovat kvantitativně.
- Geometrie: r = 5 cm (průměr 10 cm) je menší než naše 160-250 mm; s 1024 FFT a fs 44.1 kHz. Pásmo neuvedeno, takže nelze přímo převzít pro 300 Hz - 2 kHz.
- Nelze přenést: simulační scénář bez specifikace pásma, vzdálenosti, ani geometrie; fs 44.1 kHz oproti našim 16 kHz; T = 5 snapshotů při FFT 1024 (náš úsek 100 ms při 16 kHz dává 1600 vzorků, tj. jen cca 1 okno 1024 bez překryvu; interpretace); experiment bez ground truth; jen elevační/azimutální společná RMSE (Eq. 9).
- Pozor: Eq. (7) normalizuje spektrum po frekvencích; vhodný vzor pro naši širokopásmovou fúzi (interpretace).

## Citovatelná tvrzení
- DU dává srovnatelné výsledky jako MUSIC při nižší složitosti: "the diagonal unloading algorithm achieved comparable outcomes to the MUSIC algorithm while requiring less computational complexity" [s. 1].
- Princip DU: "The idea of this algorithm is to unload the covariance matrix from the signal subspace, then exploit the orthogonal property between the signal and noise subspaces" [s. 1].
- Pro SNR > 0 dB: "the RMSE for both the DU and MUSIC methods consistently remains below 1 degree" [s. 3].
- Chování při nízkém SNR: "When considering an SNR below 0 dB, it becomes evident that the algorithms exhibit a noticible increase in RMSE" [s. 3].
- Reverberace: DU "exhibits superior performance compared to the MVDR and SRP PHAT techniques" [s. 3-4].
- Experimentální uspořádání: "circular microphone array 8 microphones in indoor environments" s "Gras 46BE microphones" a fs 44.1 kHz, r = 5 cm [s. 4]; plný text: "The sampling rate is set to 44.1kHz, the number of snapshots is T = 5, the sound speed is c = 343m/s and radius of the array is fixed to r = 5cm".
- Závěr: "we achieved satisfactory results" [s. 4].
- Experimentální mapa: "the localization peak is narrow, indicating fine spatial resolution" [s. 4].
- Motivace: "Due to their modest dimensions and limited endurance, typically ranging from 10 to 30 minutes" (mini-drony) [s. 1].

## Relevantní reference z článku
(přepsáno podle seznamu literatury; [1]-[5] na s. 4, [6]-[14] na s. 5; "Ieee Access" a roky jak v PDF)
- [1] Honggu Kang, Jingon Joung, Jinyoung Kim, Joonhyuk Kang, and Yong Soo Cho. Protect your sky: A survey of counter unmanned aerial vehicle systems. Ieee Access, 8:168671-168710, 2020.
- [2] Muhammad Asif Khan, Hamid Menouar, Aisha Eldeeb, Adnan Abu-Dayya, and Flora D Salim. On the detection of unauthorized drones-techniques and future perspectives: A review. IEEE Sensors Journal, 2022.
- [3] Jian Fang, Anthony Finn, Ron Wyber, and Russell SA Brinkworth. Acoustic detection of unmanned aerial vehicles using biologically inspired vision processing. The Journal of the Acoustical Society of America, 151(2):968-981, 2022.
- [4] Torea Blanchard, J-H Thomas, and Kosai Raoof. Acoustic localization and tracking of a multi-rotor unmanned aerial vehicle using an array with few microphones. The Journal of the Acoustical Society of America, 148(3):1456-1467, 2020.
- [5] Cătălin Dumitrescu, Marius Minea, Ilona Mădălina Costea, Ionut Cosmin Chiva, and Augustin Semenescu. Development of an acoustic system for uav detection. Sensors, 20(17):4870, 2020.
- [6] Daniele Salvati, Carlo Drioli, and Gian Luca Foresti. Diagonal unloading beamforming for source localization. (v PDF je ref. [6] na začátku s. 5 uvedena jen s autory a názvem, bez venue a roku; neúplné ve zdroji, nedoplňováno)
- [7] Daniele Salvati, Carlo Drioli, and Gian Luca Foresti. A low-complexity robust beamforming using diagonal unloading for acoustic source localization. IEEE/ACM Transactions on Audio, Speech, and Language Processing, 26(3):609-622, 2018.
- [8] Daniele Salvati, Carlo Drioli, and Gian Luca Foresti. Power method for robust diagonal unloading localization beamforming. IEEE Signal Processing Letters, 26(5):725-729, 2019.
- [9] Daniele Salvati, Carlo Drioli, Giovanni Ferrin, and Gian Luca Foresti. Acoustic source localization from multirotor uavs. IEEE Transactions on Industrial Electronics, 67(10):8618-8628, 2019.
- [10] Maximo Cobos, Amparo Marti, and Jose J Lopez. A modified srp-phat functional for robust real-time sound source localization with scalable spatial sampling. IEEE Signal Processing Letters, 18(1):71-74, 2010.
- [11] Louis B Fertig. Statistical performance of the mvdr beamformer in the presence of diagonal loading. In Proceedings of the 2000 IEEE Sensor Array and Multichannel Signal Processing Workshop. SAM 2000 (Cat. No. 00EX410), pages 77-81. IEEE, 2000.
- [12] Xuan Li, Shefeng Yan, Xiaochuan Ma, and Chaohuan Hou. Spherical harmonics music versus conventional music. Applied Acoustics, 72(9):646-652, 2011.
- [13] Eric A Lehmann and Anders M Johansson. Diffuse reverberation model for efficient image-source simulation of room impulse responses. IEEE Transactions on Audio, Speech, and Language Processing, 18(6):1429-1439, 2009.
- [14] Sara Al-Emadi, Abdulla Al-Ali, Amr Mohammad, and Abdulaziz Al-Ali. Audio based drone detection and identification using deep learning. In 2019 15th International Wireless Communications & Mobile Computing Conference (IWCMC), pages 459-464. IEEE, 2019.
- DOI u referencí: v seznamu neuvedeny a PDF neobsahuje hyperlinky (ověřeno).
- Úvod (s. 1) shrnuje: [3] Fang et al. (bio-inspired zpracování, mikrofonní pole), [4] Blanchard et al. (harmonické + Kalmanův filtr, málo mikrofonů), [5] Dumitrescu et al. (konvoluční neuronové sítě + spirální pole); DU z [6], [7] (Salvati), vylepšení pomocí power method v [8].

## Kontrola (kolo 2)
- Opraveno: (1) s. 3: citát v sekci Výsledky nebyl verbatim ("consistently below 1 degree"); správně "consistently remains below 1 degree". (2) Fig. 6 (s. 4): maximum akustické mapy je při elevaci ≈ 50 stupňů, ne 55-60; osy Fig. 6 jsou 0-350 (azimut) a 0-85 (elevace). (3) Eq. 9 (s. 3): na vykreslené straně ověřeno, že odmocnina zahrnuje celý výraz (společná RMSE azimutu a elevace). (4) Hodnoty z Fig. 2-4 zpřesněny po vykreslení při 200 dpi (DU při -10 dB ≈ 16.5, MUSIC ≈ 17.3, MVDR ≈ 30.5; MUSIC při RT60 = 1 s ≈ 1.33). (5) Elevace od horizontály není jen interpretace; text s. 2 to říká ("from x to z axes"). Ostatní citáty, parametry (T = 5, N = 200, FFT 1024, loading 0.1, ε = 1e-3, Nt = 200, 44.1 kHz, 343 m/s, 5 cm, Gras 46BE, 2x Data Translation 9837B), rovnice (1)-(8), čísla stran a reference [1]-[14] odpovídají PDF.
- Doplněno: R_ss = diag(P_k^2); inicializace V_old neuvedena; odkazy [9]-[12] pro srovnávané metody; shrnutí úvodu ([3]-[5]); pozorování z grafů (MUSIC lepší než DU ve Fig. 3, DU srovnatelné/lepší při velké reverberaci); popis Fig. 5; diakritika v ref. [5]; upřesnění ref. [6].
- Nejistoty: pásmo f_min-f_max, poloměr pole v simulaci, rozměry místnosti, poloha zdroje, úhlová mřížka, model dronu a vzdálenost v experimentu nejsou v článku (ověřeno úplným čtením); ref. [6] je v PDF neúplná; hodnoty z grafů jsou odečtené a přibližné; zda součet v Eq. 9 pokrývá oba členy, plyne jen z typografie; tvrzení o nižší složitosti DU není kvantifikováno.
