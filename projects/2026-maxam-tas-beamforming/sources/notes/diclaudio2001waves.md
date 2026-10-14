# Di Claudio & Parisi 2001: WAVES, vážený průměr signálových podprostorů

Pozn. k číslování stran: [s. X] = strana časopisu (2179 až 2191); PDF strana = X − 2178 (s. 2179 = PDF 1, s. 2191 = PDF 13). Rovnice ověřeny z vyrenderovaných stran (textová vrstva rovnice neobsahuje). Kopie přes licenci ČVUT.

## Bibliografie
- MDPI: ne. Open access: ne. Metadata PDF: "IEEE Transactions on Signal Processing;2001;49;10;10.1109/78.950774"; záhlaví "VOL. 49, NO. 10, OCTOBER 2001". [s. 2179]
```bibtex
@article{diclaudio2001waves,
  author  = {Di Claudio, Elio D. and Parisi, Raffaele},
  title   = {{WAVES}: weighted average of signal subspaces for robust wideband direction finding},
  journal = {IEEE Transactions on Signal Processing},
  year    = {2001},
  volume  = {49},
  number  = {10},
  pages   = {2179--2191},
  month   = oct,
  doi     = {10.1109/78.950774}
}
```
(ověřeno proti PDF, beze změny.)

## Metoda
- Výchozí bod: CSSM fokusuje kovariance binů $\mathbf{R}_{xx}(f_i)$ maticemi $\mathbf{T}(f_i)$ na referenční frekvenci $f_0$ a průměruje je se skalárními vahami $\alpha_i$ do USCM $\mathbf{R}_F$ (5); nad ní MUSIC (7). Slabiny CSSM: fokusace přes celé zorné pole má chyby, chyby fokusace „obarví" šum a zkazí odhad počtu zdrojů, proto se fokusuje jen do úzkých sektorů po předběžném beamformingu, a několik špatných binů zkazí celou USCM. [s. 2179-2181]
- WAVES místo kovariancí fokusuje signálové podprostory: v každém binu EVD odhadu $\hat{\mathbf{R}}_{xx}(f_i)$, vlastní vektory signálového podprostoru $\tilde{\mathbf{E}}_s(f_i)$ se váží diagonální maticí $\mathbf{P}(f_i)$ (výchozí váhy podle WSF, (10): $(\lambda_k-\sigma_n^2)/\sqrt{\lambda_k\sigma_n^2}$), fokusují $\mathbf{T}(f_i)$ a skládají vedle sebe do pseudodatové matice $\tilde{\mathbf{Z}}=\eta^{-1/2}[\mathbf{T}(f_1)\tilde{\mathbf{E}}_s(f_1)\mathbf{P}(f_1),\dots,\mathbf{T}(f_J)\tilde{\mathbf{E}}_s(f_J)\mathbf{P}(f_J)]$ (12). [s. 2181-2182]
- Bázi univerzálního signálového podprostoru („WAVES") dává $d$ hlavních levých singulárních vektorů $\tilde{\mathbf{U}}_S$ z R-SVD (13) (TLS krok); $\tilde{\mathbf{U}}_N$ nahradí $\tilde{\mathbf{E}}_N$ v libovolném podprostorovém odhadu, v článku MUSIC (7), v simulacích ROOT-MUSIC, resp. beam-space ROOT-MUSIC. [s. 2182, 2186-2187]
- Robustní WAVES: váhy sloupců $\tilde{\mathbf{Z}}$ se adaptivně optimalizují (M-odhad pseudokovariance, (31), iterativně převážené R-SVD), funkce $s(x)=(x+a\sqrt{M/\eta})^{-1}$ (32), $a=0{,}05$, potlačí vlastní vektory, které nesedí na průměrnou prostorovou strukturu (chyba fokusace, neodhalený úzkopásmový zdroj, omylem vybraný šumový vektor). [s. 2184-2185]
- Navíc nový vážený LS návrh fokusačních matic BI-CSSM (33), (34), který snižuje bias. [s. 2186]
- Co potřebuje: fokusační matice (u BI-CSSM přes široký sektor bez předběžného beamformingu, tj. bez počátečních odhadů směru) [s. 2181, 2186-2187]; v každém binu výběr signálových vlastních vektorů (práh z mediánu a MAD nejmenších vlastních čísel) [s. 2185-2186]; počet zdrojů $\hat D$ se odhadne z R-SVD $\tilde{\mathbf{Z}}$ (Step 4), v simulacích byl ale pro ROOT-MUSIC zadán přesně. [s. 2182, 2186]
- Výsledky (ULA 8 senzorů, 4 zdroje, 33 binů, 250 Monte Carlo běhů): s RSS fokusací (nízké chyby) jsou WAVES a CSSM velmi podobné, CSSM je lepší při 0 dB SNR, ale má horší bias při vyšším SNR [s. 2186]; nad širokým sektorem WAVES snižuje rozptyl oproti BI-CSSM [s. 2187]; úzkopásmové rušení potlačí asi o 10 dB [s. 2188].
- (interpretace) Robustnost WAVES je vůči chybám modelu (fokusace, kalibrace, výběr vektorů, úzkopásmové rušení), ne vůči nízkému SNR; při nízkém SNR převažují chyby konečného vzorku a CSSM je v ideálním případě i mírně lepší. [s. 2184, 2186, 2188-2189]

## Citovatelná tvrzení
- "Signal subspace eigenvectors estimated from the narrowband decomposition of array outputs and properly focused generate a pseudodata matrix, which approximately obeys the narrowband array model." [s. 2180]
- "Moreover, it is less sensitive than the USCM against actual focusing errors, mistakes in the selection of signal subspace eigenvectors, and the presence of weak narrowband sources." [s. 2180]
- "Results were indeed very similar. CSSM scored a better performance at 0 dB SNR but a generally worse bias at higher SNR, except for the single source at 33°." [s. 2186]
- "A very small performance loss in ideal cases is traded off with a strong reduction of the actual risk of getting unacceptable DOA estimates in the presence of unbalanced data sets originated by highly varying source spectra, narrowband interference and, unmodeled focusing errors." [s. 2188]
- "The computational cost of WAVES is indeed comparable with that of the standard CSSM and strongly depends on the particular implementation." [s. 2189]

## Relevance pro náš projekt
- Citace WAVES v sek. 2.D vedle CSSM; v M3 jen přes pyroomacoustics. (interpretace) Pro naše krátké úseky ($L=5$ rámců) je podstatné, že WAVES potřebuje odhad signálového podprostoru v každém binu, tedy stejně jako CSSM dost snímků na bin.
