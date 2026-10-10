# Yoon, Kaplan, McClellan 2006: TOPS, DOA odhad pro širokopásmové signály

Pozn. k číslování stran: [s. X] = strana časopisu (1977 až 1989); PDF strana = X − 1976 (s. 1977 = PDF 1, s. 1989 = PDF 13). Rovnice ověřeny z vyrenderovaných stran (textová vrstva PDF rovnice neobsahuje).

## Bibliografie
- MDPI: ne. Open access: ne (kopie přes licenci ČVUT). BibTeX ověřen proti PDF (záhlaví s. 1977, DOI vytištěn na s. 1977, metadata PDF).
```bibtex
@article{yoon2006tops,
  author  = {Yoon, Yeo-Sun and Kaplan, Lance M. and McClellan, James H.},
  title   = {{TOPS}: New {DOA} Estimator for Wideband Signals},
  journal = {IEEE Transactions on Signal Processing},
  year    = {2006},
  volume  = {54},
  number  = {6},
  pages   = {1977--1989},
  month   = jun,
  doi     = {10.1109/TSP.2006.872581}
}
```

## Metoda
- Model: M senzorů, P zdrojů, K frekvenčních binů z DFT bloků; počet zdrojů P musí být znám nebo odhadnut [s. 1978]. Kovariance $\mathbf R_i$ (8), signálový podprostor $\mathbf F_i$ (P vektorů) a šumový $\mathbf W_i$ (M−P vektorů) z EVD (10), (11) [s. 1978]; odhad $\hat{\mathbf R}_i$ z J bloků (36) [s. 1982].
- Transformace: diagonální unitární $[\boldsymbol\Phi(\omega,\theta)]_{(k,k)}=\exp(-j\omega v_k\sin\theta)$ (17) převádí manifold na jinou frekvenci (Lemma 1, (18)-(20)); pro stejný úhel zachová DOA [s. 1980]. Lemma 2 (24), (25): $\mathcal R\{\boldsymbol\Phi(\Delta\omega,\phi)\mathbf F_i\}=\mathcal R\{\mathbf A(\omega_j,\hat{\boldsymbol\theta})\}$ [s. 1980].
- Test: $\mathbf U_i(\phi)=\boldsymbol\Phi(\Delta\omega_i,\phi)\mathbf F_0$, $\Delta\omega_i=\omega_i-\omega_0$, i = 1..K−1 (27); $\mathbf D(\phi)=[\mathbf U_1^H\mathbf W_1|\dots|\mathbf U_{K-1}^H\mathbf W_{K-1}]$, rozměr $P\times(K-1)(M-P)$ (28). Věta: pro $\phi=\theta_l$ ztrácí $\mathbf D$ hodnost; je-li $\mathbf D$ plné hodnosti, $\phi\ne\theta_l$. Předpoklady $2P\le M$, $K\ge P+1$ [s. 1981]. Tj. signálový podprostor referenčního binu $\omega_0$ se pro hypotetický směr φ přenese do ostatních binů a testuje se ortogonalita na jejich šumové podprostory.
- Projekce (37), (38): $\mathbf U_i'(\phi)=\mathbf P_i(\phi)\mathbf U_i(\phi)$, $\mathbf P_i$ projektor na nulový prostor $\mathbf a_i(\phi)$; snižuje únik signálu do odhadnutého šumového podprostoru; jen pro rozteč < λ/2 nejvyšší použité frekvence [s. 1982].
- Algoritmus (kroky 1-6) [s. 1982]: J bloků, DFT, výběr K binů, SVD $\hat{\mathbf R}_k$ ($\hat{\mathbf F}$ referenčního binu, $\hat{\mathbf W}_k$ ostatních), sestavení $\hat{\mathbf D}(\phi)$ pro každé φ, spektrum $\hat\theta=\arg\max_\phi 1/\sigma_{\min}(\phi)$ (39), hledá se P lokálních maxim 1D prohledáváním. Pozn.: krok 4 v článku píše $\hat{\mathbf F}_1$, v (27) je $\mathbf F_0$ (nekonzistence značení).
- Počáteční odhad ani fokusační úhly nejsou potřeba [s. 1977, 1978]. Plně korelované zdroje výkon zhoršují [s. 1981]. Funguje pro libovolná 1D a 2D pole, pro obecná 3D pole není zaručeno [s. 1981].
- Složitost (sek. III-E) [s. 1982]: přesný výpočet "není snadný"; nejmenší singulární hodnotu $\mathbf D$ lze získat SVD matice P×P, tedy $O(P^3)$ na každé hypotetické φ. CSSM/WAVES po sestavení koherentní matice potřebují jen jedno SVD, tj. méně výpočtů než TOPS, ale RSS fokusace vyžaduje SVD M×M v každém binu. (vlastní doplnění) Sestavení $\mathbf D(\phi)$ stojí řádově $O(K P M(M-P))$ na směr, článek to nekvantifikuje.
- Simulace [s. 1982-1986]: ULA 10 senzorů, 3 zdroje (8°, 33°, 37°), 200 Monte Carlo běhů, J = 100 bloků po 256 vzorcích, TOPS a IMUSIC 7 binů, CSSM a WAVES 22 binů, RSS fokusace; fokusační úhly buď perturbace skutečných DOA, nebo Capon.
- SNR: abstrakt tvrdí, že TOPS je nejlepší ve středních SNR, koherentní metody v nízkých a nekoherentní ve vysokých [s. 1977]. Výsledky jsou opatrnější: pod 0 dB TOPS nerozliší 33°/37° [s. 1983]; podle RMS (Obr. 3e,f) má TOPS "average performance" v celém rozsahu, IMUSIC je nejlepší ve středních až vysokých SNR a koherentní metody v nízkých [s. 1983]; závěr: výkon TOPS leží mezi koherentními a nekoherentními metodami v celém rozsahu SNR [s. 1986]. Výhoda vůči CSSM/WAVES při vysokém SNR: tyto metody zůstávají zkreslené (bias) [s. 1983].

## Citovatelná tvrzení
- "Unlike other coherent wideband methods, such as the coherent signal subspace method (CSSM) and WAVES, the new method does not require any preprocessing for initial values." [s. 1977]
- "The simulations show that this new technique performs better than others in mid signal-to-noise ratio (SNR) ranges, while coherent methods work best in low SNR and incoherent methods work best in high SNR." [s. 1977]
- "However, it uses the transformation matrix at each hypothesized DOA to perform an orthogonality test between the transformed signal subspace and the noise subspace. If the hypothesized DOA corresponds to a true DOA, then orthogonality is preserved; otherwise, it is not." [s. 1980]
- "The minimum nonzero singular values of the D matrix can be found via an SVD of a P × P matrix, so O(P^3) computations have to be done for each hypothesized φ." [s. 1982]
- "TOPS exhibited the average performance in the whole SNR in terms of rms error while IMUSIC was best in midto high SNR and the coherent methods were best in low SNR." [s. 1983]

## Relevance pro náš projekt
- Citace TOPS v sek. 2.D; v M3 jen kontrola přes pyroomacoustics. Cena: EVD/SVD M×M v každém binu jednou + pro každý směr sestavení $\mathbf D(\phi)$ a SVD P×P, tj. $O(P^3)$ na směr podle autorů [s. 1982].
- Podmínka $2P\le M$ [s. 1981]: se 4 mikrofony nejvýše 2 současné zdroje.
