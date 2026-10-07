# Van Veen & Buckley 1988: beamforming jako prostorová filtrace (tutoriál)

Pozn. k číslování stran: [s. X] = strana časopisu (4 až 24); PDF strana = X − 3 (s. 4 = PDF 1, s. 24 = PDF 21; ověřeno na tištěných číslech stran). PDF je sken s OCR (Acrobat Capture), rovnice ověřeny z vyrenderovaných stran.

## Bibliografie
- Van Veen, B.D.; Buckley, K.M. "Beamforming: A Versatile Approach to Spatial Filtering." *IEEE ASSP Magazine*, vol. 5, no. 2, pp. 4-24, April 1988. [s. 4, záhlaví "APRIL 1988"]
- DOI 10.1109/53.665 (z metadat PDF: "IEEE ASSP Magazine;1988;5;2;10.1109/53.665"; na stránkách nevytištěn). MDPI: ne. Open access: ne (licence ČVUT přes IEEE Xplore).
- BibTeX ověřen proti PDF, beze změny (titul v PDF v title case, v `.bib` sentence case; IEEEtran tiskne tak, jak je zadán):
```bibtex
@article{vanveen1988beamforming,
  author  = {Van Veen, Barry D. and Buckley, Kevin M.},
  title   = {Beamforming: a versatile approach to spatial filtering},
  journal = {IEEE ASSP Magazine},
  year    = {1988},
  volume  = {5},
  number  = {2},
  pages   = {4--24},
  month   = apr,
  doi     = {10.1109/53.665}
}
```

## Obsah
- Tutoriál: terminologie a statistiky 2. řádu (sek. II), data-independent beamforming (III), statisticky optimální (IV: MSC, referenční signál, max SNR, LCMV/MVDR, GSC), adaptivní algoritmy (V), částečně adaptivní (VI), implementace (VII). [s. 5-22]
- Článek neřeší odhad DOA skenováním výkonu přes směry (SRP/prostorové spektrum) ani výpočetní složitost konvenčního beamformeru.

## Metoda (notace článku)
- Výstup: $y = \mathbf{w}^H\mathbf{x}$ (2.3), $N$ vah; úzkopásmově $N=J$ senzorů, širokopásmově s FIR $N=KJ$. [s. 7]
- Odezva: $r(\theta,\omega)=\mathbf{w}^H\mathbf{d}(\theta,\omega)$ (2.6); $\mathbf{d}(\theta,\omega)=[1\ e^{j\omega\tau_2(\theta)}\cdots e^{j\omega\tau_N(\theta)}]^H$ (2.7) je "array response vector", též steering vector. Beampattern $=|r(\theta,\omega)|^2$. [s. 7-8]
- Výstupní výkon: $E\{|y|^2\}=\mathbf{w}^H\mathbf{R}_x\mathbf{w}$, $\mathbf{R}_x=E\{\mathbf{x}\mathbf{x}^H\}$; úzkopásmový zdroj $\mathbf{R}_x=\sigma_s^2\mathbf{d}\mathbf{d}^H$ (2.10). [s. 9]
- Frekvenční (DFT) beamforming: data každého binu zpracuje vlastní beamformer (Fig. 2.4). [s. 9]
- Klasický (phased array) beamformer: $\mathbf{w}=\mathbf{d}(\theta_0,\omega_0)$, volitelně s taperem $\mathbf{T}\mathbf{d}$. [s. 11] Širokopásmově "delay sum beamforming": FIR filtry aproximují propagační zpoždění (Fig. 3.2). [s. 12]
- White noise gain $=\mathbf{w}^H\mathbf{w}$. [s. 12]
- Max SNR: $\mathbf{w}=\alpha\mathbf{R}_n^{-1}\mathbf{d}(\theta,\omega)$ (4.1). [s. 14] (interpretace) Pro prostorově bílý šum $\mathbf{R}_n=\sigma^2\mathbf{I}$ vychází $\mathbf{w}\propto\mathbf{d}$, tj. konvenční beamformer.
- LCMV/MVDR: $\min_\mathbf{w}\mathbf{w}^H\mathbf{R}_x\mathbf{w}$ s.t. $\mathbf{d}^H\mathbf{w}=g^*$ (4.2); $\mathbf{w}=g^*\mathbf{R}_x^{-1}\mathbf{d}/(\mathbf{d}^H\mathbf{R}_x^{-1}\mathbf{d})$ (4.3), pro $g=1$ MVDR. [s. 15] Obecně $\mathbf{C}^H\mathbf{w}=\mathbf{f}$, $\mathbf{w}=\mathbf{R}_x^{-1}\mathbf{C}[\mathbf{C}^H\mathbf{R}_x^{-1}\mathbf{C}]^{-1}\mathbf{f}$ (Tab. 4.1). [s. 13]
- Mapování na naši notaci: $\mathbf{d}(\theta,\omega)\to\mathbf{a}(k,\Omega)$, $\mathbf{R}_x\to\hat{\mathbf{R}}(k)$. Dosazením $\mathbf{w}=\mathbf{a}$ do $\mathbf{w}^H\mathbf{R}_x\mathbf{w}$ (s. 9 + s. 11) dostaneme $P_{\mathrm{DAS}}=\mathbf{a}^H\hat{\mathbf{R}}\mathbf{a}$ (bez normalizace, prvky $\mathbf{d}$ mají jednotkovou velikost). Součet přes biny (SRP) v článku není, je to naše konstrukce.

## Citovatelná tvrzení
- "A beamformer performs spatial filtering to separate signals that have overlapping frequency content but originate from different spatial locations." [s. 4]
- "Spatial discrimination capability depends on the size of the spatial aperture; as the aperture increases, discrimination improves. The absolute aperture size is not important, rather its size in wavelengths is the critical parameter." [s. 5]
- "The resulting array and beamformer is termed a phased array since the output of each sensor is phase shifted prior to summation." [s. 11]
- "One example of where the structure of Fig. 3.2 is used is delay sum beamforming. Here the FIR filters approximate the propagation delays (linear phase over the frequency band of interest) ..." [s. 12]
- "If the required knowledge is inaccurate, the optimum beamformer will attenuate the desired signal as if it were interference." [s. 17]; k tomu GSC: "the data independent beamformer is useful in situations where adaptive signal cancellation occurs" [s. 16]

## Relevance pro náš projekt
- Přehledová citace pro DAS (phased array / delay sum), steering vektor $\mathbf{d}\leftrightarrow\mathbf{a}$, výkon $\mathbf{w}^H\mathbf{R}\mathbf{w}$ a MVDR/LCMV v sek. 2.A a 2.B. SRP přes biny citovat spíš DiBiase 2001; robustnost DAS vůči chybám polohy mikrofonů článek netvrdí.
