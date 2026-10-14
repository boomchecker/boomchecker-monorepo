# Capon 1969: spektrální analýza frekvence-vlnové číslo s vysokým rozlišením (MVDR)

Pozn. k číslování stran: [s. X] = strana časopisu (1408 až 1418); PDF strana = X − 1407 (s. 1408 = PDF 1, s. 1418 = PDF 11). Sken s OCR, textová vrstva rovnic je poškozená; rovnice (12), (18) až (20), (26) až (33), (44) a (45) ověřeny z vyrenderovaných stran.

## Bibliografie
- Capon, J. (M.I.T. Lincoln Laboratory). Proceedings of the IEEE, vol. 57, no. 8, pp. 1408-1418, August 1969. Manuscript received December 13, 1968; revised June 17, 1969. [s. 1408] DOI z metadat PDF (pole Subject). MDPI: ne. Open access: ne (licence ČVUT).
```bibtex
@article{capon1969mvdr,
  author  = {Capon, J.},
  title   = {High-resolution frequency-wavenumber spectrum analysis},
  journal = {Proceedings of the IEEE},
  year    = {1969},
  volume  = {57},
  number  = {8},
  pages   = {1408--1418},
  month   = aug,
  doi     = {10.1109/PROC.1969.7278}
}
```

## Metoda
- Kontext: seismické pole LASA (Montana, 21 subpolí), odhad spektra frekvence-vlnové číslo $P(\lambda,\mathbf{k})$, tj. rychlosti a azimutu šířících se vln; $\lambda=2\pi fT$ je normovaná frekvence. [s. 1408-1409]
- Spektrální matice: data každého z $K$ senzorů rozdělena na $M$ bloků po $N$ vzorcích, $S_{jn}(\lambda)$ je DFT bloku (10), odhad $\hat f_{jl}(\lambda)=\frac{1}{M}\sum_n S_{jn}S^*_{ln}$ (11). Odpovídá našemu $\hat{\mathbf{R}}(k)$ z $L$ rámců STFT. [s. 1409]
- Konvenční odhad (12): $\hat P(\lambda,\mathbf{k})=\frac{1}{K^2}\sum_{j,l} w_j w_l^* \hat f_{jl}(\lambda)\,e^{i\mathbf{k}\cdot(\mathbf{x}_j-\mathbf{x}_l)}$, pevné okno ve vlnovém čísle dané beam patternem pole $|B(\mathbf{k})|^2$ (15). [s. 1409-1410]
- Odhad s vysokým rozlišením (18): $P'(\lambda,\mathbf{k})=\big[\sum_{j,l}\hat q_{jl}(\lambda)\,e^{i\mathbf{k}\cdot(\mathbf{x}_j-\mathbf{x}_l)}\big]^{-1}$, kde $\{\hat q_{jl}\}$ je inverze spektrální matice $\{\hat f_{jl}\}$. [s. 1410]
- Převod na naši notaci: se směrovým vektorem $a_j=e^{-i\mathbf{k}\cdot\mathbf{x}_j}$ je součet v (18) roven $\mathbf{a}^H\hat{\mathbf{R}}^{-1}\mathbf{a}$, tedy $P'=1/(\mathbf{a}^H\hat{\mathbf{R}}^{-1}\mathbf{a})=P_{\mathrm{MVDR}}$ pro jeden frekvenční bin. (vlastní převod)
- Interpretace (19), (20): $P'$ je výstupní výkon procesoru s váhami $A_j(\lambda,\mathbf{k})$ (20) navrženými z dat zvlášť pro každé $\mathbf{k}_0$, který propustí rovinnou vlnu z $\mathbf{k}_0$ nezkreslenou a ostatní vlny potlačí v optimálním smyslu nejmenších čtverců (Capon jej nazývá "maximum-likelihood filter"; odvození odkazuje na [3, (122), (123)]). Explicitní úloha $\min \mathbf{w}^H\mathbf{R}\mathbf{w}$ s podmínkou $\mathbf{w}^H\mathbf{a}=1$ ani název MVDR v článku nejsou. [s. 1410]
- Rozlišení, jedna vlna + nekoherentní šum (26) až (33): v bodě, kde $\hat P$ klesne jen na $1-R$ (téměř maximum $1-R+R/K$), je $P'$ už o 3 dB níž. [s. 1411] Dvě vlny (34) až (43): metoda je rozliší i tam, kde je beam pattern nerozliší, pokud je $R/K$ malé. [s. 1411-1412]
- Singularita a loading (44), (45): spektrální matice má hodnost nejvýš $M$ (počet bloků), pro $M<K$ je singulární; nutná podmínka $M\ge K$. Řešení: přidat malé množství nekoherentního šumu, $F'=(1-R)F+RI$, $F'$ je pozitivně definitní (46), (47). [s. 1412] V Tab. I použito $R=0{,}05$ při $M=2$ blocích a $K=21$. [s. 1413]

## Citovatelná tvrzení
- "It is shown that the wavenumber resolution of this method is considerably better than that of the conventional method." [s. 1408, abstrakt]
- "Thus, P'(λ, k0) is the power output of an array processor, known as a maximum-likelihood filter, whose design is determined by the sensor data and is different for each wavenumber k0, which passes undistorted any monochromatic plane wave traveling at a velocity corresponding to the wavenumber k0 and suppresses in an optimum least-squares sense the power of those waves traveling at velocities corresponding to wavenumbers other than k0" [s. 1410]
- "the amount of computation required to obtain P' is almost the same as that to get P̂, since only an additional Hermitian matrix inversion is required." [s. 1410]
- "if the number of blocks M is less than the number of sensors K, then the spectral matrix is of order K, but only of rank M at most and is thus singular." ... "In order to make the spectral matrix nonsingular a small amount of incoherent noise is added." [s. 1412]
- "the wavenumber resolution of this method is determined primarily by the amount of incoherent noise which is present in the array of sensors, and, to a lesser extent, by the natural beam pattern of the array." Pro šum LPZ "an improvement of about a factor of four". [s. 1418]

## Relevance pro náš projekt
- Primární citace MVDR (Capon) v sek. 2.B; vzorec (18) je náš $P_{\mathrm{MVDR}}$ pro jeden bin. Nekoherentní součet přes biny $\sum_k$ v článku není (Capon pracuje na jedné frekvenci).
- Rovnice (45) je diagonální loading v Caponově podobě a s. 1412 přímo podkládá tvrzení, že $16\times16$ matice z 5 rámců STFT má hodnost nejvýš 5 a je singulární.
- Výhoda rozlišení závisí na malém podílu nekoherentního šumu $R$; u dronu s vlastním hlukem rotorů ji nelze předpokládat (vlastní úvaha).
