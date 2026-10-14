# Knapp & Carter 1976: zobecněná korelační metoda pro odhad časového zpoždění (GCC)

Pozn. k číslování stran: [s. X] = strana časopisu (320 až 327); PDF strana = X − 319 (s. 320 = PDF 1). Konvence ověřena (PDF 1 = s. 320, PDF 8 = s. 327). PDF strana 1 (s. 320) začíná na vrchu koncem seznamu literatury předchozího článku (reference [7]-[10]) a PDF strana 8 (s. 327) obsahuje na začátku zbytek seznamu literatury Knappa a Cartera ([20]-[24]) a pod ním začátek jiného článku (Oppenheim, Kopec, Tribolet, "Signal Analysis by Homomorphic Prediction"), ten se nepoužívá. Text v PDF má v rovnicích poškozenou textovou vrstvu (sken s OCR); všechny rovnice byly ověřeny z vyrenderovaných stran.

## Bibliografie
- Knapp, C.H.; Carter, G.C. "The Generalized Correlation Method for Estimation of Time Delay." *IEEE Transactions on Acoustics, Speech, and Signal Processing*, vol. ASSP-24, no. 4, pp. 320-327, August 1976. Manuscript received July 24, 1975; revised November 21, 1975 and February 23, 1976. [s. 320]
- Pracoviště: C. H. Knapp, Dept. of Electrical Engineering and Computer Science, University of Connecticut, Storrs; G. C. Carter, Naval Underwater Systems Center, New London Laboratory. [s. 320]
- DOI: 10.1109/TASSP.1976.1162830 (ověřeno: Crossref DOI rozlišuje a metadata PDF obsahují "IEEE Article ID 1162830" a v poli Subject "IEEE Transactions on Acoustics, Speech, and Signal Processing;1976;24;4;10.1109/TASSP.1976.1162830"; DOI není vytištěn na stránkách PDF). Vydavatel: IEEE (časopis IEEE Trans. ASSP; záhlaví "VOL. ASSP-24, NO. 4, AUGUST 1976"). MDPI: ne. Open access: neuvedeno (kopie stažena přes licenci ČVUT, "Authorized licensed use limited to: CZECH TECHNICAL UNIVERSITY", tj. nejde o OA).
- Pozn.: Uluskan 2024 cituje tento článek jako "IEEE Trans Acoust 1976; 24(4): 320–327".
- Návrh BibTeX:
```bibtex
@article{knapp1976gcc,
  author  = {Knapp, Charles H. and Carter, G. Clifford},
  title   = {The Generalized Correlation Method for Estimation of Time Delay},
  journal = {IEEE Transactions on Acoustics, Speech, and Signal Processing},
  year    = {1976},
  volume  = {24},
  number  = {4},
  pages   = {320--327},
  month   = aug,
  doi     = {10.1109/TASSP.1976.1162830}
}
```
(volume: v záhlaví "ASSP-24", Crossref a Uluskan [39] uvádějí 24; pole `doi` ověřeno.)

## Problém a přínos
- Problém: odhad zpoždění D mezi signály na dvou prostorově oddělených senzorech v přítomnosti nekorelovaného šumu, při neznámém nebo jen přibližně známém spektru zdroje a konečné době pozorování T. [s. 320]
- Přínos: (1) zobecněná křížová korelace s frekvenčním váhováním ψ_g(f) = H1(f)H2*(f), (2) odvození ML odhadu zpoždění (Hannan-Thomson procesor, HT), (3) srovnání šesti procesorů (cross-correlation, Roth, SCOT, PHAT, Eckart, ML/HT) v Tab. I, (4) vztah ML k Eckartově filtru při nízkém SNR, (5) rozptyl odhadu zpoždění a Cramér-Rao mez (Appendix). [s. 320-326]
- Pozn.: PHAT je zde jen jeden z procesorů, označen jako "ad hoc"; váha PHAT je v článku připsána [16] (Carter, Nuttall, Cable, tech. memo SCOT, 1972; Tab. I, s. 322-323). (interpretace) Článek je původním zdrojem zobecněné korelační metody (rámec GCC s prefiltry), PHAT jako samostatná technika pochází podle autorů z [16].

## Mikrofonní pole a hardware
- Neuvedeno. Článek je čistě teoretický, dva senzory, žádná geometrie pole, hardware, fs, kvantizace, kalibrace ani synchronizace. Zmínka: zpoždění D a z něj úhel příchodu vůči ose senzorů [1]. [s. 320]
- Jedna numerická zkouška (simulace, "one such trial with T large") bez popisu parametrů. [s. 326]

## Signálový model a rovnice
- Model (1a, 1b): $x_1(t)=s_1(t)+n_1(t)$, $x_2(t)=\alpha s_1(t+D)+n_2(t)$; $s_1,n_1,n_2$ reálné, spolu stacionární náhodné procesy; $s_1$ nekorelovaný s šumem. [s. 320]
- Korelace (2), (3): $R_{x_1x_2}(\tau)=E[x_1(t)x_2(t-\tau)]$; odhad $\hat R_{x_1x_2}(\tau)=\frac{1}{T-\tau}\int_\tau^T x_1(t)x_2(t-\tau)\,dt$. [s. 320]
- Vztah ke křížovému spektru (4): $R_{x_1x_2}(\tau)=\int_{-\infty}^{\infty}G_{x_1x_2}(f)e^{j2\pi f\tau}df$. [s. 321]
- Spektrum po filtraci (5): $G_{y_1y_2}(f)=H_1(f)H_2^*(f)G_{x_1x_2}(f)$. [s. 321]
- Roth (12): $\hat R^{(R)}_{y_1y_2}(\tau)=\int\frac{\hat G_{x_1x_2}(f)}{G_{x_1x_1}(f)}e^{j2\pi f\tau}df$ (odhad impulzní odezvy Wienerova-Hopfova filtru $H_m=G_{x_1x_2}/G_{x_1x_1}$ (13)); pro $n_1\ne0$ je $G_{x_1x_1}=G_{s_1s_1}+G_{n_1n_1}$ (14) a (15) $R^{(R)}_{y_1y_2}(\tau)=\delta(\tau-D)\circledast\int\frac{\alpha G_{s_1s_1}}{G_{s_1s_1}+G_{n_1n_1}}e^{j2\pi f\tau}df$, tj. delta funkce je rozmazána, pokud $G_{n_1n_1}$ není konstanta krát $G_{s_1s_1}$. [s. 322, poznámka pod čarou 1: ve výrazech musí být $G$ nahrazeno odhadem $\hat G$]
- SCOT (17): $\hat R^{(s)}_{y_1y_2}(\tau)=\int\hat\gamma_{x_1x_2}(f)e^{j2\pi f\tau}df$; pro $G_{x_1x_1}=G_{x_2x_2}$ je SCOT ekvivalentní Rothově procesoru; jde o prewhitening filtry $H_i=1/\sqrt{G_{x_ix_i}}$ následované křížovou korelací. [s. 322]
- Eckart: kritérium deflection (24) $d^2$, optimální $H_1H_2^*=\psi_E(f)e^{+j2\pi fD}$ (25), $\psi_E(f)=\frac{\alpha G_{s_1s_1}(f)}{G_{n_1n_1}(f)G_{n_2n_2}(f)}$ (26) (v Tab. I je bez $\alpha$); odhad pro $\alpha=1$ (27): $\psi_E=|\hat G_{x_1x_2}|\{[\hat G_{x_1x_1}-|\hat G_{x_1x_2}|]\cdot[\hat G_{x_2x_2}-|\hat G_{x_1x_2}|]\}$ (v PDF je to vytištěno jako součin v kompaktním zápisu bez zlomku/exponentu -1; (interpretace) správně má být $|\hat G_{x_1x_2}|/\{\cdots\}$, což plyne z (26) pro $\alpha=1$). Eckartův filtr, na rozdíl od PHAT, dává nulovou váhu pásmům, kde $G_{s_1s_1}=0$. [s. 323]
- Zobecněná korelace (6a-6c): $R^{(g)}_{y_1y_2}(\tau)=\int_{-\infty}^{\infty}\psi_g(f)G_{x_1x_2}(f)e^{j2\pi f\tau}df$, $\psi_g(f)=H_1(f)H_2^*(f)$; v praxi $\hat R^{(g)}_{y_1y_2}(\tau)=\int\psi_g(f)\hat G_{x_1x_2}(f)e^{j2\pi f\tau}df$. [s. 321]
- Křížová korelace a spektrum modelu (7), (8): $R_{x_1x_2}(\tau)=\alpha R_{s_1s_1}(\tau-D)+R_{n_1n_2}(\tau)$, $G_{x_1x_2}(f)=\alpha G_{s_1s_1}(f)e^{-j2\pi fD}+G_{n_1n_2}(f)$. Pro $G_{n_1n_2}=0$: $R_{x_1x_2}(\tau)=\alpha R_{s_1s_1}(\tau)\circledast\delta(t-D)$ (9) a pro více zpoždění (10) $R_{x_1x_2}(\tau)=R_{s_1s_1}(\tau)\circledast\sum_i\alpha_i\delta(\tau-D_i)$. [s. 321]
- Váhy (Tab. I, [s. 323]):

| Procesor | $\psi(f)$ | Rovnice |
|---|---|---|
| Cross Correlation | 1 | |
| Roth Impulse Response | $1/G_{x_1x_1}(f)$ | (11) [s. 321] |
| SCOT | $1/\sqrt{G_{x_1x_1}(f)G_{x_2x_2}(f)}$ | (16) [s. 322] |
| PHAT | $1/|G_{x_1x_2}(f)|$ | (19) [s. 322] |
| Eckart | $G_{s_1s_1}(f)/[G_{n_1n_1}(f)G_{n_2n_2}(f)]$ | (26) [s. 323] |
| ML nebo HT | $\dfrac{|\gamma_{12}(f)|^2}{|G_{x_1x_2}(f)|\,[1-|\gamma_{12}(f)|^2]}$ | (45b) [s. 324] |

- PHAT korelace (20), (21), (22), (23): $\hat R^{(p)}_{y_1y_2}(\tau)=\int\frac{\hat G_{x_1x_2}(f)}{|G_{x_1x_2}(f)|}e^{j2\pi f\tau}df$ (v PDF je v čitateli odhad $\hat G$ a ve jmenovateli skutečné $|G|$ bez stříšky; Uluskan a Lim zapisují praktickou formu s odhadem v obou); pro model s $G_{n_1n_2}=0$ je $|G_{x_1x_2}|=\alpha G_{s_1s_1}$ (21); ideálně, když $\hat G=G$: $\frac{\hat G_{x_1x_2}}{|G_{x_1x_2}|}=e^{j\theta(f)}=e^{j2\pi fD}$ (jednotkový modul) a $R^{(p)}_{y_1y_2}(\tau)=\delta(t-D)$ (v PDF zapsáno $\delta(t-D)$). [s. 322]
- Koherence (18): $\hat\gamma_{x_1x_2}(f)\triangleq\frac{\hat G_{x_1x_2}(f)}{\sqrt{G_{x_1x_1}(f)G_{x_2x_2}(f)}}$. [s. 322]
- ML (HT) zpoždění (45a): $R^{(HT)}_{y_1y_2}(\tau)=\int\hat G_{x_1x_2}(f)\frac{1}{|G_{x_1x_2}(f)|}\frac{|\gamma_{12}(f)|^2}{[1-|\gamma_{12}(f)|^2]}e^{j2\pi f\tau}df$ [s. 324]; vztah k PHAT (46a, 46b): $\mathrm{var}[\hat\theta(f)]\approx\frac{1-|\gamma|^2}{|\gamma|^2}\frac{1}{L_1}$ ($L_1$ konstanta úměrná způsobu zpracování dat), $R^{(HT)}\approx\frac{1}{L_1}\int e^{j\hat\theta(f)}\frac{1}{\mathrm{var}[\hat\theta(f)]}e^{j2\pi f\tau}df$, tj. ML = PHAT váženo inverzně podle variability fáze. [s. 325]
- Nízké SNR ($\alpha=1$; $G_{s_1s_1}/G_{n_ii}\ll1$): (48) $\psi_{HT}(f)\approx\frac{G_{s_1s_1}(f)}{G_{n_1n_1}(f)G_{n_2n_2}(f)}=\psi_E(f)$; (49) $\psi_s\approx1/\sqrt{G_{n_1n_1}G_{n_2n_2}}$; (50a) $\psi_{HT}\approx\frac{G_{s_1s_1}}{\sqrt{G_{n_1n_1}G_{n_2n_2}}}\psi_s$; pro $G_{n_1n_1}=G_{n_2n_2}=G_{nn}$ (50b): $\psi_{HT}\approx\frac{G_{s_1s_1}}{G_{nn}}\psi_s=\left[\frac{G_{s_1s_1}}{G_{nn}}\right]^2\psi_p(f)$ (tedy PHAT se SNR² váhou; Eckart a HT lze interpretovat jako SCOT se SNR vahou). [s. 325]
- Rozptyl odhadu zpoždění (51): $\mathrm{var}[\hat D]=\dfrac{\int|\psi(f)|^2(2\pi f)^2G_{x_1x_1}(f)G_{x_2x_2}(f)[1-|\gamma(f)|^2]df}{T\left[\int(2\pi f)^2|G_{x_1x_2}(f)|\psi(f)df\right]^2}$ (lokální rozptyl v okolí pravého zpoždění). [s. 325]
- Rozptyl HT (52): $\mathrm{var}^{HT}[\hat D]=\left\{2T\int_0^\infty(2\pi f)^2\frac{|\gamma(f)|^2}{1-|\gamma(f)|^2}df\right\}^{-1}$; dosahuje Cramér-Rao meze: (A4) minimum var$(\hat D)=\left[T\int_{-\infty}^{\infty}(2\pi f)^2\frac{|\gamma_{12}(f)|^2}{1-|\gamma_{12}(f)|^2}df\right]^{-1}$ (Appendix, (A1)-(A4), (A3) věrohodnostní střední hodnota) je totéž co (52). [s. 325-326]
- Odvození ML: Gaussovský model, Fourierovy koeficienty $X_i(k)$ s $\omega_\Delta=2\pi/T$ (28a-c), nekorelované (tedy nezávislé) Gaussovy veličiny (29), matice výkonových spekter $Q_x$ (31), věrohodnost (32) s kvadratickou formou $J_1$ (33)-(34); $|Q_x|$ (35) nezávisí na $D$ pro $G_{n_1n_2}=0$; $Q_x^{-1}$ (37); $J_1=J_2+J_3$ (41), $D$ maximalizuje $-J_3$ (42b, 44). Předpoklad: $G_{n_1n_2}=0$, $|\gamma_{12}|^2\ne1$. [s. 323-324]

## Algoritmus
- Schéma (Obr. 1): $x_1\to H_1\to y_1$, $x_2\to H_2\to y_2$ se zpožděním; součin, integrace přes $[0,T]$, umocnění na druhou, detekce píku → $\hat D$. [s. 321]
- Postup pro odhad zpoždění: spočítat/odhadnout křížové spektrum $\hat G_{x_1x_2}(f)$ z konečného pozorování, vynásobit váhou $\psi_g(f)$, zpětná transformace, argmax přes $\tau$. [s. 321]
- Pro PHAT: váha je $1/|G_{x_1x_2}(f)|$ (bez nutnosti znát spektra zdroje a šumu; v praxi je $G$ ve jmenovateli nahrazeno odhadem, viz poznámka pod čarou 1, s. 322). Pro SCOT, Eckart a ML/HT je nutné znát nebo odhadovat spektra a koherenci (odhad např. technikami [22]; "entirely a heuristic procedure"). [s. 322, 324]
- Parametry (délka okna, FFT, překryv, pásmo, regularizace, počet zdrojů, grid): neuvedeno; jde o jediné zpoždění (v diskuzi i více zpoždění, kap. Processor Interpretation, rovnice 10). [s. 321]

## Experiment / simulace
- Neuvedeno (žádné drony, vzdálenosti, SNR experimenty ani ground truth). Jediná zmínka: "We have conducted one such trial (with T large) and verified that useful delay estimates can be obtained by inserting estimates |Ĝx1x2(f)| and |γ̂12(f)|² in place of the true values." [s. 326]
- Empirické ověření vzorců pro rozptyl simulací nebylo provedeno, protože by bylo výpočetně příliš náročné bez speciálního korelátorového hardwaru. [s. 326]

## Výsledky (čísla)
- Žádné kvantitativní experimentální výsledky. Závěr (s. 326): HT procesor je ML odhad zpoždění za obvyklých podmínek, při nízkém SNR je ekvivalentní Eckartovu prefiltrování a křížové korelaci; porovnány všechny "six estimation techniques". Jde o analytické výsledky: ML (HT) procesor je identický s Hannan-Thomsonovým, za určitých podmínek s MacDonald-Schultheissovým; při nízkém SNR ekvivalentní Eckartovu filtru; dosahuje Cramér-Rao meze (52, A4). [s. 320, 325-326]
- Při $G_{n_1n_1}=G_{n_2n_2}=G_{nn}\propto G_{s_1s_1}$ jsou posledních pět procesorů v Tab. I stejných až na konstantu; klasická korelace ($\psi=1$) zůstává "smeared" Fourierovou transformací výkonového spektra signálu. [s. 325]

## Omezení podle autorů
- Model předpokládá stacionaritu a nekorelovaný šum; v praxi stacionární jen po konečnou dobu T; D a α se pomalu mění. [s. 320]
- PHAT: vyvinut "purely as an ad hoc technique"; váží křížové spektrum inverzně vůči výkonu signálu, tj. chyby jsou zesíleny tam, kde je signál nejslabší; v praxi, kdy $\hat G\ne G$, není $\theta(f)=2\pi fD$ a odhad $R^{(P)}$ nebude delta funkce; v pásmu, kde $G_{x_1x_2}(f)=0$, je fáze nedefinovaná a odhad fáze je "erratic, being uniformly distributed in the interval [−π, π] rad". [s. 322]
- Vzorce (51), (52) předpovídají jen lokální variaci odhadu zpoždění v okolí skutečného zpoždění; nezahrnují nejednoznačné píky při nedostatečném T a nezahrnují chybu odhadu spekter; (52) může být příliš optimistické při neznámých spektrech. [s. 325-326]
- ML váhy vyžadují znát (nebo odhadovat) koherenci a spektra; nahrazení odhady je heuristické. [s. 324]
- Volba ψ_g(f) je kompromis mezi rozlišením (ostrý pík) a stabilitou (citlivost na konečné T při nízkém SNR). [s. 321]

## Relevance pro náš projekt
- SOTA sekce: definice GCC (rovnice 6a-6c) a PHAT váhy (rovnice 19, 20; PHAT připsán [16]) a odkaz na nízké SNR a rozptyl; PHAT jako speciální případ zobecněné korelace. Hodí se pro odstavec o GCC-PHAT+LS: TDOA z dvojic mikrofonů, pak LS řešení úhlu.
- Volba pásma: článek o pásmu nehovoří; ale rovnice (6a) a (45b) dovolují omezit váhu na pásmo (např. 300 Hz až 2 kHz) vynulováním ψ mimo pásmo (interpretace). PHAT váží všechny frekvence stejně a zesiluje chyby v pásmech s nízkým signálem (s. 322), což podporuje omezení na pásmo s harmonickými dronu (interpretace).
- Geometrie 2×8 UCA: nelze, jde o dva senzory; ale TDOA mezi dvojicemi z UCA lze získat touto metodou a rozlišení závisí na šířce píku (viz kompromis rozlišení/stabilita).
- Volba metody: ML/HT váha dává nižší rozptyl než PHAT, pokud je znám/odhadnut γ12(f); pro krátké okno 100 ms (malé T) je odhad koherence nespolehlivý a PHAT je robustnější volba (interpretace). Rozptyl odhadu zpoždění je podle (51) a (52) nepřímo úměrný době pozorování T, takže krátké 100 ms okno znamená vyšší rozptyl (interpretace).
- Výpočetní nároky: GCC-PHAT na dvojici = FFT dvou signálů, součin, normalizace, IFFT; žádná inverze matice. Pro 16 mikrofonů 120 dvojic (interpretace); nároky na MCU nejsou v článku, nutno odhadnout samostatně.
- Nelze přenést: předpoklad nekorelovaného šumu (u nás hluk vrtulí dronu a vítr jsou prostorově korelované), jeden zdroj a stacionarita (dron se pohybuje).

## Citovatelná tvrzení
- Zdroj nutně neznáme: "In many problems, this information is negligible. For example, in passive detection, unlike the usual communications problems, the source spectrum is unknown or only known approximately." [s. 320]
- Podstata metody: "The time argument at which the correlator achieves a maximum is the delay estimate." [s. 320]
- Úloha prefiltrů: "the role of the prefilters is to accentuate the signal passed to the correlator at frequencies for which the signal-to-noise (S/N) ratio is highest and, simultaneously, to suppress the noise power." [s. 320]
- Kompromis rozlišení vs. stabilita: "the choice of ψg(f) is a compromise between good resolution and stability." [s. 321]
- PHAT je ad hoc: "The PHAT was developed purely as an ad hoc technique." [s. 322]
- Slabina PHAT: "Thus, errors are accentuated where signal power is smallest." [s. 322]
- ML vs PHAT: "the ML estimator is the PHAT inversely weighted according to the variability of the phase estimates." [s. 325]
- Optimalita: "In particular, the HT processor achieves the Cramér-Rao lower bound" [s. 325]
- Limit rozptylu: "(51) and (52) evaluate the local variation of the time-delay estimate and thus do not account for ambiguous peaks which may arise when the averaging time is not large enough for the given signal and noise characteristics." [s. 325-326]
- Zahrnutí reálných odhadů: "Substituting estimated weighting for true weighting is entirely a heuristic procedure whereby the ML estimator can approximately be achieved in practice." [s. 324]

## Relevantní reference z článku
(Číslování podle seznamu na s. 326-327; zapsáno podle PDF.)
- [1] A. H. Nuttall, G. C. Carter, and E. M. Montavon, "Estimation of the two-dimensional spectrum of the space-time noise field for a sparse line array," J. Acoust. Soc. Amer., vol. 55, pp. 1034-1041, 1974.
- [4] G. M. Jenkins and D. G. Watts, Spectral Analysis and Its Applications. San Francisco, CA: Holden-Day, 1968.
- [5] C. Eckart, "Optimal rectifier systems for the detection of steady signals," Univ. California, Scripps Inst. Oceanography, Marine Physical Lab. Rep SIO 12692, SIO Ref 52-11, 1952.
- [7] C. H. Knapp, "Optimum linear filtering for multi-element arrays," Electric Boat Division, Groton, CT, Rep. U417-66-031, Nov. 1966.
- [8] A. H. Nuttall and D. W. Hyde, "A unified approach to optimum and suboptimum processing for arrays," Naval Underwater Systems Center, New London Lab., New London, CT, Rep. 992, Apr. 1969.
- [9] P. R. Roth, "Effective measurements using digital signal analysis," IEEE Spectrum, vol. 8, pp. 62-70, Apr. 1971.
- [10] E. J. Hannan and P. J. Thomson, "The estimation of coherence and group delay," Biometrika, vol. 58, pp. 469-481, 1971.
- [11] G. C. Carter, A. H. Nuttall, and P. G. Cable, "The smoothed coherence transform," Proc. IEEE (Lett.), vol. 61, pp. 1497-1498, Oct. 1973.
- [12] E. J. Hannan and P. J. Thomson, "Estimating group delay," Biometrika, vol. 60, pp. 241-253, 1973.
- [13] A. V. Oppenheim and R. W. Schafer, Digital Signal Processing. Englewood Cliffs, NJ: Prentice-Hall, 1975.
- [14] G. C. Carter and C. H. Knapp, "Coherence and its estimation via the partitioned modified chirp-Z-transform," IEEE Trans. Acoust., Speech, Signal Processing (Special Issue on 1974 Arden House Workshop on Digital Signal Processing), vol. ASSP-23, pp. 257-264, June 1975.
- [15] H. L. Van Trees, Detection, Estimation and Modulation Theory, Part I. New York: Wiley, 1968.
- [16] G. C. Carter, A. H. Nuttall, and P. G. Cable, "The smoothed coherence transform," Naval Underwater Systems Center, New London Lab., New London, CT, Tech. Memo TC-159-72, Aug. 8, 1972. (podle Tab. I zdroj SCOT i PHAT; v PDF je název "The smoothed coherence transform" bez "(SCOT)")
- [20] R. K. Otnes and L. Enochson, Digital Time Series Analysis. New York: Wiley, 1972.
- [21] V. H. MacDonald and P. M. Schultheiss, "Optimum passive bearing estimation," J. Acoust. Soc. Amer., vol. 46, pp. 37-43, 1969.
- [22] G. C. Carter, C. H. Knapp, and A. H. Nuttall, "Estimation of the magnitude-squared coherence function via overlapped fast Fourier transfrom processing," IEEE Trans. Audio Electroacoust., vol. AU-21, pp. 337-344, Aug. 1973. ("transfrom" je překlep v PDF; autoři Carter, Knapp, Nuttall)
- [23] D. Signori, J. L. Freeh, and C. Stradling, personal communication.
- [24] B. V. Hamon and E. J. Hannan, "Spectral estimation of time delay for dispersive and non-dispersive systems," J. Royal Stat. Soc. Ser. C (Appl. Statist.), vol. 23, pp. 134-142, 1974.
- Poznámka k seznamu: [20]-[24] jsou na s. 327 (PDF 8) v hlavičce stránky nad začátkem jiného článku; ověřeno z vykreslené stránky. [11] je v PDF "Proc. IEEE (Lett.), vol. 61, pp. 1497-1498, Oct. 1973". V seznamu nejsou uvedena DOI.

## Kontrola (kolo 2)
- Opraveno: (1) DOI 10.1109/TASSP.1976.1162830 označen jako ověřený (Crossref + metadata PDF), odstraněno "z paměti, neověřeno" včetně poznámky pod BibTeX; volume změněno na 24 (v záhlaví "ASSP-24") [s. 320]; (2) rovnice (20) a (22): v PDF je ve jmenovateli skutečné |G_{x1x2}(f)| bez stříšky (čitatel Ĝ), původně přepsáno s odhadem v obou [s. 322]; (3) stránkování (45a) je s. 324 (ne 325), (46) s. 325; (4) tvrzení "článek je původním zdrojem PHAT váhy" opraveno: PHAT připsán [16], článek je zdrojem rámce GCC a srovnání procesorů (Tab. I, s. 322-323); (5) ref [16] v PDF nemá "(SCOT)" v názvu; (6) [22] má v PDF překlep "transfrom"; (7) ověřeno: všechny citáty (s. 320, 321, 322, 324, 325, 326) jsou doslovné, Tab. I včetně referencí, refs [20]-[24] z vyrenderované s. 327, rovnice (1)-(52), (A1)-(A4).
- Doplněno: rovnice (12)-(15), (17), (24)-(27), (28)-(37), (41), (46b), (47)-(50) a (A4); poznámky pod čarou 1 a 2 (s. 322); závěr; reference [4], [7], [8], [13], [15], [20], [23]; popis stránek s cizím textem (PDF 1 nahoře, PDF 8).
- Nejistoty: rovnice (27) je v PDF vytištěna bez zlomku/exponentu -1 (pravděpodobně chyba sazby, náš výklad je odvozen z (26)); (51) odpovídá vytištěné podobě (|G| ψ ve jmenovateli bez |ψ|); číslování stran (320 až 327) vychází z tištěných hlaviček stránek v PDF; PDF je sken s OCR, text vrstvu nelze použít pro rovnice.
