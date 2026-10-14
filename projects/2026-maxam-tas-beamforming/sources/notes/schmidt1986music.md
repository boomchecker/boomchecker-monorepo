# Schmidt 1986: MUSIC, lokalizace více zdrojů a odhad parametrů signálu

Pozn. k číslování stran: [s. X] = strana časopisu (276 až 280); PDF strana = X − 275 (s. 276 = PDF 1, s. 280 = PDF 5). Rovnice ověřeny z vyrenderovaných stran (sken s OCR, textová vrstva rovnic poškozená). Podle poznámky editora jde o přetisk příspěvku z Proc. RADC Spectrum Estimation Workshop (říjen 1979). [s. 276]

## Bibliografie
- MDPI: ne. Open access: ne (kopie přes licenci ČVUT). Záhlaví "VOL. AP-34, NO. 3, MARCH 1986", s. 276-280; DOI 10.1109/TAP.1986.1143830 z metadat PDF (na stránkách nevytištěn). Autor: Ralph O. Schmidt. [s. 276]
```bibtex
@article{schmidt1986music,
  author  = {Schmidt, Ralph O.},
  title   = {Multiple emitter location and signal parameter estimation},
  journal = {IEEE Transactions on Antennas and Propagation},
  year    = {1986},
  volume  = {34},
  number  = {3},
  pages   = {276--280},
  month   = mar,
  doi     = {10.1109/TAP.1986.1143830}
}
```

## Metoda
- Datový model (1): $X=AF+W$; $X$ je komplexní $M$-vektor, $A=[a(\theta_1)\dots a(\theta_D)]$ jsou "mode vectors" (steering vektory), $F$ komplexní amplitudy $D$ zdrojů, $W$ šum. Pole libovolné geometrie i směrových charakteristik. [s. 276]
- Kovariance (2): $S=\overline{XX^*}=APA^*+\lambda S_0$, za předpokladu nekorelovaných signálů a šumu; $P$ může být i singulární (plně korelované páry). Pro $D<M$ je $APA^*$ singulární (3) a $S=APA^*+\lambda_{\min}S_0$ (4); pro bílý šum $\lambda_{\min}S_0=\sigma^2 I$. [s. 277]
- Vlastní struktura: zobecněný vlastní problém "S in the metric of $S_0$", $Se_i=\lambda_i S_0 e_i$; $\lambda_{\min}$ má násobnost $N=M-D$. [s. 277]
- Počet zdrojů se odhaduje (5): $\hat D=M-\hat N$, $\hat N$ = násobnost $\lambda_{\min}$; v praxi shluk blízkých hodnot, jehož rozptyl klesá s množstvím dat. [s. 277]
- Šumový podprostor: vlastní vektory příslušné $\lambda_{\min}$ splňují $A^*e_i=0$, tj. jsou kolmé na sloupce $A$; $E_N$ je $M\times N$ matice těchto vektorů. [s. 277]
- Pseudospektrum (6): $P_{MU}(\theta)=1/\big(a^*(\theta)E_NE_N^*a(\theta)\big)$, převrácená kvadratická vzdálenost $a(\theta)$ od signálového podprostoru. Varianta s polarizací (8), odhad $P$ (7). [s. 277-278]
- Kroky (Fig. 2): 0 tvorba $S$; 1 vlastní rozklad; 2 volba $D$ dle (5); 3 výpočet $P_{MU}$; 4 výběr $D$ maxim; 5 ostatní parametry (7). [s. 278]
- Srovnání s BF, ML (Capon) a ME na trojúhelníkovém poli (Fig. 3, 4): MUSIC bez biasu a s ostrým maximem. [s. 278-279]
- Model je úzkopásmový (jedna frekvence, $a$ závisí jen na $\theta$); slovo "narrowband" ani širokopásmové rozšíření v článku nejsou. Složitost výpočtu článek neuvádí.
- Mapování na naši notaci: $S\to\hat{\mathbf R}(k)$, $E_N\to\mathbf E_n(k)$, $a(\theta)\to\mathbf a(k,\Omega)$, $^*\to{}^H$, $D\to K$, $S_0\to\mathbf I$. Součet $\sum_k$ přes biny je naše nekoherentní rozšíření, ne Schmidtovo.

## Citovatelná tvrzení
- "the eigenvectors associated with λmin(S, S0) are orthogonal to the space spanned by the columns of A; the incident signal mode vectors!" [s. 277]
- "Therefore, the number of incident signals estimator is D̂ = M − N̂ (5) where N̂ = the multiplicity of λmin(S, S0)" [s. 277]
- "It is clear from the expression that MUSIC is asymptotically unbiased even for multiple incident wavefronts because S is asymptotically perfectly measured so that EN is also." [s. 278]
- "NO BIAS ERROR; PEAK IS VERY SHARP" (popisek MUSIC ve Fig. 4) [s. 279]
- "No assumptions have been made about array geometry." [s. 280]

## Relevance pro náš projekt
- Primární citace MUSIC v sek. 2 (podprostorové metody). Cena (vlastní odhad, článek ji neuvádí): EVD $M\times M$ na bin $O(M^3)$, pak $O(M^2)$ na směr s předpočteným projektorem $\mathbf E_n\mathbf E_n^H$.
- (interpretace) Odhad (5) je u nás málo vypovídající: při $L=5$ rámcích má $\hat{\mathbf R}$ ($16\times16$) hodnost nejvýš 5, nejmenší vlastní číslo má násobnost aspoň 11 bez ohledu na skutečný počet zdrojů, diagonální loading ji jen posune o $\varepsilon$. Schmidtův předpoklad "asymptotically perfectly measured" S zde neplatí.
