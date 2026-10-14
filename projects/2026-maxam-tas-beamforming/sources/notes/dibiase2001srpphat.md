# DiBiase, Silverman, Brandstein 2001: robustní lokalizace v dozvukových místnostech (SRP-PHAT)

Stav: v `sources/978-3-662-04619-7_8.pdf` je jen náhled Springeru, 2 strany (s. 157 a 158 knihy = PDF 1 a 2): abstrakt, úvod 8.1 a začátek 8.2. Samotné odvození SRP-PHAT (sekce 8.3) v PDF není. Metadata ověřena přes Crossref API 2026-10-07 a souhlasí s PDF (kap. 8, © Springer-Verlag Berlin Heidelberg 2001). Rovnice v sekci Metoda je standardní formulace z literatury, ne citace z PDF.

## Bibliografie
- MDPI: ne. Open access: ne.
```bibtex
@incollection{dibiase2001srpphat,
  author    = {DiBiase, Joseph H. and Silverman, Harvey F. and Brandstein, Michael S.},
  title     = {Robust Localization in Reverberant Rooms},
  booktitle = {Microphone Arrays: Signal Processing Techniques and Applications},
  editor    = {Brandstein, Michael and Ward, Darren},
  publisher = {Springer},
  address   = {Berlin, Heidelberg},
  year      = {2001},
  pages     = {157--180},
  doi       = {10.1007/978-3-662-04619-7_8}
}
```

## Metoda (standardní formulace)
- SRP-PHAT: výkon řízené odezvy s PHAT váhou je (až na konstantu) součet GCC-PHAT všech párů mikrofonů vyhodnocených v TDOA odpovídajících kandidátnímu směru: $P(\Omega)=\sum_{m<n}R^{\mathrm{PHAT}}_{mn}(\tau_{mn}(\Omega))$.
- Kombinuje robustnost beamformingu (využití všech párů) a PHAT bělení (odolnost proti dozvuku).

## Relevance pro náš projekt
- Primární citace SRP-PHAT v sek. 2.C. Pro 16 mikrofonů 120 párů; ve frekvenční oblasti ekvivalentně $\sum_k \mathbf{a}^H\tilde{\mathbf{R}}(k)\mathbf{a}$ s PHAT-normalizovanou kovariancí (vlastní úvaha).

## Citovatelná tvrzení
- Přínos kapitoly: "A new localization method is then presented in detail. By utilizing key features of existing methods, this new algorithm is shown to be significantly more robust to acoustical conditions, particularly reverberation effects, than the traditional localization techniques in use today." [s. 157]
- Kombinace dvou přístupů: "a speech source localization algorithm designed specifically for reverberant enclosures which combines two of these general approaches" [s. 158]
- Tři kategorie lokalizace: "those based upon maximizing the steered response power (SRP) of a beamformer, techniques adopting high-resolution spectral estimation concepts, and approaches employing time-difference of arrival (TDOA) information." [s. 158]
- Požadavky na lokátor: "any such estimator would have to be computationally non-demanding and possess a short processing latency to make it practical for real-time systems." [s. 158]
- Pozn.: že kombinovanými přístupy jsou SRP a PHAT/TDOA, plyne z názvu metody (SRP-PHAT) a navazující literatury; v náhledu to výslovně není (interpretace).
