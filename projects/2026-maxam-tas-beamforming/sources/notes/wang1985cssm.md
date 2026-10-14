# Wang & Kaveh 1985: koherentní zpracování v signálovém podprostoru (CSSM)

Pozn. k číslování stran: [s. X] = strana časopisu (823 až 831); PDF strana = X − 822 (s. 823 = PDF 1, s. 831 = PDF 9). PDF je sken s OCR (Acrobat Capture, licence ČVUT, nejde o OA); rovnice ověřeny z vyrenderovaných stran 3 až 6.

## Bibliografie
- MDPI: ne. Open access: ne. BibTeX ověřen proti PDF (záhlaví "VOL. ASSP-33, NO. 4, AUGUST 1985", s. 823-831; DOI z metadat PDF), shoduje se s `report/references.bib`.
```bibtex
@article{wang1985cssm,
  author  = {Wang, H. and Kaveh, M.},
  title   = {Coherent signal-subspace processing for the detection and estimation of angles of arrival of multiple wide-band sources},
  journal = {IEEE Transactions on Acoustics, Speech, and Signal Processing},
  year    = {1985},
  volume  = {33},
  number  = {4},
  pages   = {823--831},
  month   = aug,
  doi     = {10.1109/TASSP.1985.1164667}
}
```

## Metoda
- Model: M senzorů, d < M širokopásmových zdrojů (mohou být i zcela korelované, např. vícecestné šíření), libovolná známá geometrie; výstup rozložen DFT na J nepřekrývajících se úzkých pásem $f_j$, K snapshotů délky $\Delta T$. [s. 823-824, rov. (5)]
- Transformační (v pozdější literatuře "fokusační") matice $\mathbf{T}(f_j)$, M×M, nesingulární: $\mathbf{T}(f_j)\mathbf{A}(f_j)=\mathbf{A}(f_0)$ (6); jedna volba $\mathbf{T}(f_j)=[\mathbf{A}(f_0)|\mathbf{B}(f_0)][\mathbf{A}(f_j)|\mathbf{B}(f_j)]^{-1}$ (7). Matice nejsou jednoznačné. [s. 825] Pojem "focusing" článek nepoužívá.
- Koherentní kovariance na referenční frekvenci $f_0$: $\mathbf{R}=\sum_j w_j\,\mathrm{cov}(\mathbf{Y}(f_j))=\mathbf{A}(f_0)\mathbf{R}_s\mathbf{A}^H(f_0)+\sigma_n^2\mathbf{R}_n$ (9)-(11), $\mathbf{R}_s=\sum_j w_j\mathbf{P}_s(f_j)$ (12), $\mathbf{R}_n=\sum_j w_j\mathbf{T}(f_j)\mathbf{P}_n(f_j)\mathbf{T}^H(f_j)$ (13); dále $w_j=1$. [s. 825] Odhad: $\hat{\mathbf{R}}=\Delta T\sum_j\hat{\mathbf{T}}(f_j)\hat{\mathbf{C}}(\mathbf{X}(f_j))\hat{\mathbf{T}}^H(f_j)$ (30), $\hat{\mathbf{R}}_n$ (31). [s. 827-828]
- Věta: M − d nejmenších vlastních čísel svazku $(\mathbf{R},\mathbf{R}_n)$ je rovno $\sigma_n^2$ a $\mathbf{A}^H(f_0)\mathbf{E}_n=\mathbf{0}$ (14). [s. 825-826] Frekvenční průměrování odstraní singularitu $\mathbf{P}_s$ u zcela korelovaných zdrojů, pokud $\int\mathbf{P}_s(f)\,df$ je nesingulární. [s. 824]
- Počet zdrojů: koherentní MAICE (25) nad vlastními čísly $(\hat{\mathbf{R}},\hat{\mathbf{R}}_n)$; věrohodnostní funkce (20) počítá s K snapshoty. [s. 826-827] Směry: MUSIC na $f_0$ (32). [s. 828]
- Předběžné odhady: konstrukce $\mathbf{T}(f_j)$ vyžaduje neznámé úhly, proto se používají předběžné odhady $\beta_1..\beta_{d_1}$ (např. prostorový periodogram), (27)-(28); pro zdroje v okolí jednoho úhlu diagonální $\hat{\mathbf{T}}$ (29). Že stačí znát okolí úhlů, je jen hypotéza ověřená simulacemi; citlivost na počáteční odhad autoři teoreticky neanalyzují ("work in progress"). Kroky 3)-7) lze iterovat. [s. 827-828]
- Simulace: ULA M = 16, $f_0$ = 100 Hz, BW = 40 Hz, J = 33, K = 64. Dva zcela korelované zdroje 9° a 12° (pod polovinou Rayleighovy meze 7,4°), 0 dB: koherentní metoda je rozliší s jediným předběžným odhadem 10,4°, nekoherentní ne. [s. 828-829, obr. 1-3]
- Dva nekorelované zdroje: při nízkém SNR lepší detekce, menší rozptyl (blízko CRLB) i menší bias než nekoherentní metoda; bias v obr. 6 je funkcí SNR, ne chyby předběžného odhadu. [s. 829-830, obr. 4-6]

## Citovatelná tvrzení
- "The technique relies on an approximately coherent combination of the spatial signal spaces of the temporally narrow-band decomposition of the received signal vector from an array of sensors." [s. 823]
- "Still another problem with the incoherent signal-subspace processing is its inability to handle completely correlated sources even if the SNR is infinitely high and the observation time is infinitely long" [s. 823]
- "It is clear from its definition that construction of T(f_j) requires a knowledge of the unknown angles of arrival. A natural estimator of T, therefore, uses preliminary estimates of the angles in its formulation." [s. 827]
- "We hypothesize that a knowledge of the neighborhoods of these angles is sufficient to effect the advantages of coherent processing." [s. 827]
- "The coherently constructed signal space results in an appropriately frequency-averaged estimate of the spatial covariance matrix that is statistically more accurate and that is, to a large extent, immune to the degree of correlation between the sources." [s. 830]

## Relevance pro náš projekt
- Citace CSSM v sek. 2.D jako koherentní alternativa k nekoherentnímu součtu přes biny. Případný přínos pro krátký 100 ms úsek (L = 5 rámců) je vlastní úvaha: článek mluví o "statisticky přesnějším" odhadu kovariance [s. 830], ne o zvýšení efektivního počtu snapshotů. V M3 jen přes pyroomacoustics.
