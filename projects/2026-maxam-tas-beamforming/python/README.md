# Simulace DOA pro pole 2×8 mikrofonů

Balíček `beamforming` simuluje, jak by pole 16 MEMS mikrofonů (dva kruhy po osmi)
lokalizovalo dron v úseku 100 ms po triggeru. Slouží k volbě geometrie a metody pro report
(sekce 4) a pro pozdější PCB a firmware. Instalace a spouštění přes `task` jsou v
[README projektu](../README.md); tady je popis modelu, konvencí a kódu.

## Rychlý přehled

```bash
task setup           # venv v python/venv, pinované závislosti, pip install -e python/
task sim:fetch       # 100 dronových klipů z DADS do data/dads (jednou, ~40 s)
task sim:test        # pytest (198 testů, ~20 s)
task sim:lint        # ruff + mypy
task sim:demo        # report/figures/doa_demo.pdf + report/generated/demo.tex
task sim:validate    # report/generated/validation.tex, 30/10/0 dB (~9 min na 4 jádrech)
task sim:ablate      # ablace voleb zpracování -> python/results/summary.csv
task sim:sweep       # screening, potvrzení, SNR sweep a citlivost -> python/results/summary.csv
task sim:figures     # report/figures/{sweep,beampattern}.pdf a report/generated/*.tex ze summary.csv
```

Z Pythonu (po `task setup`):

```python
import numpy as np
from beamforming import dads, doa, geometry as g, signals as sg

mic = g.make_array("2x8_rot", diameter=0.20, height=0.07)  # (16, 3) metry
clip = dads.load_clip(dads.list_clips("data/dads")[0])
u = g.unit_vector(np.deg2rad(120), np.deg2rad(60))  # skutečný směr
x = sg.observe(clip, mic, u, offset=2000)  # (16, 1600) signály pole
x = sg.add_noise(x, snr_db=10, rng=np.random.default_rng(0))
az, el = doa.localize("music", x, mic)  # radiány
```

## Konvence

Shodné se vzorci v `report/main.tex` (sekce 2, signálový model).

| Veličina | Definice |
|---|---|
| azimut φ | od osy x proti směru hodinových ručiček, 0 až 355° |
| elevace θ | od vodorovné roviny, 0° horizont, 90° zenit; prohledává se jen horní polokoule |
| směr | `u = [cosθ cosφ, cosθ sinφ, sinθ]` |
| počátek | střed pole (u 2×8 uprostřed mezi deskami) |
| úhly v API | radiány (`*_deg` jen v názvech funkcí s výsledkem ve stupních) |
| chyba | úhel po velké kružnici mezi odhadem a skutečným směrem (`angular_error_deg`) |

pyroomacoustics používá colatitude (0° = zenit). Převod `colatitude = 90° − θ` je jen v
`pra_check.py`.

### Geometrie (`geometry.make_array`)

`diameter` je průměr kružnice, na které leží MEMS mikrofony (ne obrys desky), aby podle něj
šla navrhnout PCB. Mikrofon `k` leží na úhlu `45°·k` (u horního kruhu `+22,5°` v topologii
`2x8_rot`).

| Topologie | Mikrofony | Kanály |
|---|---|---|
| `1x8` | 8 v rovině z = 0 | 0..7 |
| `2x8` | dolní kruh z = −h/2, horní kruh z = +h/2, stejné azimuty | 0..7 dolní (SD_A), 8..15 horní (SD_B) |
| `2x8_rot` | jako `2x8`, horní kruh pootočený o 22,5° | stejně |

## Signálový model

Rovinná vlna (far-field) ze směru `u`. Mikrofon na `r_m` ji zachytí s předstihem
`τ_m = r_mᵀu / c` (c = 343 m/s):

```
x_m(t) = s(t + τ_m)         X_m(f) = S(f) · exp(+j2π f τ_m)
```

`signals.steering()` počítá `a_m(f, u) = exp(+j2π f τ_m)` a používá ji jak `propagate()`
(generování signálů), tak každá metoda, takže znaménko nemůže být nekonzistentní (hlídá to
`test_steering_sign_matches_model`).

1. **Zdroj**: klip DADS (mono, 16 kHz). Simulace běží celá na 16 kHz; DADS nemá nad 8 kHz
   obsah, takže převzorkování na 48 kHz by nic nezměnilo.
2. **Zpoždění** (`propagate`, `observe`): frakcionální posun násobením spektra. FFT je kruhová,
   proto se zpožďuje úsek s okrajem `MARGIN = 1024` vzorků na obou stranách a ten se pak ořízne.
3. **Šum** (`add_noise`): nezávislý bílý gaussovský šum v každém kanálu, škálovaný na změřený
   výkon, takže SNR sedí přesně. Výchozí SNR je širokopásmové (střední výkon signálu přes pole
   dělený výkonem šumu); `snr_band=(300, 2000)` ho počítá jen v pásmu lokalizace.
4. **STFT** (`stft`): Hann, `NFFT = 512`, `hop = 256`, bez paddingu, tedy z 1600 vzorků
   **5 rámců**. Pásmo 300 až 2000 Hz jsou biny 10 až 64 (55 binů).
5. **Metody** pracují s `X` tvaru `(M, K, T)` (mikrofony, biny, rámce).

`decimate_48k_to_16k` (Kaiser FIR, 3:1) se v simulaci nepoužívá. Je to testovaná reference
decimace, kterou bude dělat firmware (útlum > 55 dB od 8 kHz, zpoždění `(N−1)/2` vzorků).

## Grid a vyhledávání

* **Hrubý grid** (`geometry.coarse_grid`): krok 5°, azimut 0 až 355°, elevace 0 až 90°,
  72 × 19 = 1368 směrů. V zenitu se směr opakuje pro každý azimut, což argmax nevadí.
* **Jemný grid** (`geometry.fine_cap`): 11 × 11 bodů s krokem 1° v tečné rovině kolem hrubého
  maxima (121 směrů, gnomonická projekce), body pod horizontem se zahodí. Rozteč je 1° při
  jakékoli elevaci včetně zenitu a přechod azimutu přes 0° nevyžaduje zvláštní případ.
* `doa.search()` spojí obojí. `doa.power_map()` vrací mapu hrubého gridu pro vykreslení.

## Metody (`doa.py`)

| Název | Funkce | Výkon pro bin `k` a směr `Ω` | Sčítání binů |
|---|---|---|---|
| `das` | `das_bins` | `aᴴ R a` | součet |
| `mvdr` | `mvdr_bins` | `1 / aᴴ (R + ε·tr(R)/M·I)⁻¹ a`, ε = 10⁻² | normalizovaný součet |
| `srp_phat` | `srp_phat_bins` | součet GCC-PHAT přes 120 dvojic ve zpožděních `τ_mn(Ω)` | součet |
| `music` | `music_bins` | `1 / aᴴ Eₙ Eₙᴴ a`, `n_src = 1` | normalizovaný součet |
| `gcc_phat_ls` | `gcc_phat_ls` | bez gridu, viz níže | bez sčítání |

* `R` je výběrová kovariance přes 5 rámců, má hodnost nejvýš 5. MVDR proto bez loadingu
  nejde invertovat, u MUSIC stačí jedna dominantní složka (`n_src = 1`), šumový podprostor má
  15 rozměrů.
* **Normalizovaný součet**: u MVDR a MUSIC se výkon každého binu vydělí svým maximem na
  hrubém gridu, aby několik silných harmonických nepřehlušilo ostatní biny. Jemné hledání
  přebírá konstanty z hrubého (`bin_weights`), jinak by obě fáze vážily biny jinak.
* **SRP-PHAT** má PHAT váhu na každém kanálu a rámci (`X / |X|`) jako pyroomacoustics, a
  součet přes dvojice se vyjádří jako `(|aᴴp|² − M) / 2`, takže se nesmyčkuje přes 120 dvojic.
* **MUSIC** používá `aᴴ Eₙ Eₙᴴ a = M − |Eₛᴴ a|²` (`‖a‖² = M`), stačí tedy dominantní
  vlastní vektory.
* **GCC-PHAT + LS**: celý úsek 1600 vzorků, FFT s dvojnásobným zero-paddingem, PHAT jen
  v pásmu 300 až 2000 Hz, korelace 16× jemněji zero-padded IFFT, maximum jen v oknu
  `|τ| ≤ |rᵢ − rⱼ|/c`. `IFFT[Xᵢ Xⱼ*]` má maximum v `−τᵢⱼ`. Soustava
  `(rᵢ − rⱼ)ᵀ u = c·τᵢⱼ` ze 120 dvojic se řeší nejmenšími čtverci. U rovinného pole (`1x8`)
  se `u_z` dopočítá z `|u| = 1`. Výsledek se ořízne na horní polokouli.

Nová metoda: napsat `xxx_bins(X, mic_pos, freqs, dirs, cfg) -> (K, D)` a zapsat ji do
`GRID_METHODS` (s příznakem `normalize`). `doa.METHODS` se z něj odvozuje, takže ji testy
přesnosti v `test_doa.py` zahrnou automaticky.

## Data

`dads.fetch()` (`task sim:fetch`) stáhne N klipů ze shardu 20 datasetu
`geronimobasso/drone-audio-detection-samples` (shardy jsou čisté podle labelu, 20 a 38 jsou
dronové). Čte se 20 rovnoměrně rozložených row groups (~1,6 MB každá), takže výběr je pestrý
a stahování trvá desítky sekund, ne gigabajty. Klipy mají typicky 0,5 s (8000 vzorků); filtr
vyřadí klipy kratší než 3648 vzorků (úsek 100 ms plus okraje) a klipy s méně než 20 % energie
v pásmu 300 až 2000 Hz. Výsledek je v `data/dads/*.wav` a `manifest.json` (mimo git).
Parquet se čte přes `HfFileSystem` a `pyarrow`, bajty WAV dekóduje `soundfile`. Stažení je
atomické: klipy se ukládají do `.dads.tmp` vedle `data/dads` a po dokončení manifestu se
adresář přejmenuje, takže přerušený nebo neúspěšný fetch nechá původní cache a po změně N
nezůstanou osiřelé soubory. `task sim:fetch` kontroluje i to, že všechny soubory z manifestu
existují. `HF_TOKEN` z `.env` je volitelný (zástupnou hodnotu z `env.example` ignoruje).

## Experimenty (M4)

Všechny běhy jsou **párové a deterministické** (`experiment.py`): trial je klip DADS s náhodným
úsekem 100 ms a náhodným směrem na horní polokouli. Směry `d`-tého běhu mají vlastní generátor
(`d = 0` je stejný jako v `validate.py`), úsek, šum při daném SNR a perturbace mikrofonů mají
generátor klíčovaný `(SEED, klip, d, účel)`, takže výsledek nezávisí na počtu procesů, pořadí
dokončení ani na tom, které SNR a metody se počítají společně. Procesy se spouštějí
(`spawn`), ne forkují, a skripty nastavují jedno BLAS vlákno na proces.

| Krok | Skript | Co dělá |
|---|---|---|
| ablace | `ablate.py` | volby `guard_bins`, `bin_weighting="snr"`, `freq_smooth`, `hop=128` na 2x8_rot Ø200/h70 při 30, 10, 0 dB, 5 směrů na klip; přijme se volba, která při 0 dB sníží RMSE o ≥ 5 % (geometrický průměr přes metody) a při 30 dB ji nezvýší o víc než 5 % |
| screening | `sweep.py screening` | 21 geometrií × 5 metod, SNR 10 a 0 dB, 1 směr na klip |
| potvrzení | `sweep.py confirm` | 3 nejlepší geometrie podle geometrického průměru RMSE přes metody při 0 dB, 5 směrů na klip |
| SNR sweep | `sweep.py snr` | vybraná geometrie (`python/results/selected.json`, zapisuje se ručně po potvrzení), SNR −10 až 30 dB po 5, s CRB |
| citlivost | `sweep.py sensitivity` | vybraná geometrie a 1x8 stejného Ø: c = 331 a 355 m/s v simulaci (metody počítají s 343), σ polohy 0,5, 1, 2 mm, 10 dB |
| grafy | `figures.py` | čte `summary.csv`, nic nesimuluje |

`python/results/summary.csv` obsahuje jeden řádek na stage, geometrii, variantu, metodu a SNR
(`n`, RMSE, medián, RMSE azimutu vážené cos(el) a elevace, podíl nad 5°, RMSE a podíl po třech
elevačních pásmech stejné plochy); řádky `method == crb` nesou Cramérovu-Raovu mez.
Každá stage nahradí jen své řádky, soubor je stabilní na bajty. `beampattern.csv` má šířky
laloku a PSL všech geometrií.

Doplňující moduly: `metrics.py` (chyby, pásma elevace, souhrn), `crb.py` (CRB deterministického
signálu v tečných souřadnicích, uzavřený tvar přes rozptyl poloh mikrofonů v tečné rovině),
`beampattern.py` (DAS: šířka −3 dB v řezu azimutem a elevací, PSL), `cost.py` (analytický počet
MAC metod pro 16 kanálů, 5 rámců, 55 binů; odhad času při 2 cyklech na MAC a 250 MHz je
předpoklad, ne měření), `results.py` (`summary.csv`, pořadí geometrií).

## Testy (`task sim:test`)

| Soubor | Co hlídá |
|---|---|
| `test_geometry.py` | poloměr, rotace 22,5°, rozteč desek, konvence úhlů, grid 1368 směrů a odmítnutí kroku, který nedělí 90° a 360°, symetrická jemná čepička (i pro krok nedělící rozpětí), zenit a horizont, rovnoměrnost náhodných směrů |
| `test_signals.py` | znaménko steeringu, přesnost frakčního zpoždění, žádný přetok FFT, absolutní výkon šumu a SNR nezávislým měřením, 5 rámců, biny 10..64 a okraje pásma, Hannovo okno (únik), útlum decimace |
| `test_dads.py` | podíl energie v pásmu, okraje segmentu, klipy v cache podle `manifest.json` (bez dat test **selže**), atomický fetch nad lokálním parquet, bez osiřelých souborů, ignorování zástupného tokenu |
| `test_doa.py` | každá metoda: chyba ≤ 2° při SNR 30 dB na 8 směrech mimo uzly gridu (včetně přechodu 360°, horizontu a zenitu); `1x8` azimut; `2x8` bez rotace; nízké SNR; invariance k měřítku vstupu; tichý segment; každý binový výkon proti explicitnímu vzorci |
| `test_gcc.py` | zpoždění dvojic proti přesným hodnotám (i end-fire), fyzikální mez zpoždění, pásmo proti celému spektru, PHAT proti hlasitému koherentnímu rušiteli, řešení směru ze zpoždění (3D, rovinné pole, ořez pod horizont), elevace u `1x8` |
| `test_search.py` | jemné hledání přebírá normalizační váhy z hrubého gridu, dosáhne rohu hrubé buňky, `power_map` je součet binů podle rovnic, shoda dat binů a frekvencí (čistý tón) |
| `test_options.py` | volby `Config`: výchozí chování, odmítnutí neplatných hodnot, vyhlazování binů a hodnost kovariance, `snr_gain`, `guard_bins`, každá volba lokalizuje při 30 dB |
| `test_metrics.py`, `test_crb.py`, `test_beampattern.py`, `test_cost.py` | pásma stejné plochy, vážený azimut, outliery; CRB proti numerickému Fisherovu informačnímu číslu a tvaru pro dva mikrofony; šířka laloku proti uzavřenému tvaru, PSL; počty MAC na ručně spočítaném případu |
| `test_experiment.py`, `test_results.py`, `test_sweep_smoke.py` | determinismus a vnořenost trialů, nezávislost šumu na ostatních SNR a metodách, `evaluate`, `summary.csv`, pravidlo přijetí volby, smoke test skriptů na třech klipech |
| `test_pra_check.py` | vlastní hrubé maximum SRP-PHAT a MUSIC je stejný uzel jako pyroomacoustics SRP a NormMUSIC; všechny pra metody běží |

Při SNR 30 dB jsou chyby 0,3 až 0,7°. Bez šumu je medián chyby gridových metod 0,39° (krok jemného
gridu 1°), takže 30 dB měří hlavně kvantizaci.

Testy se kontrolovaly mutacemi: do kopie balíčku se vnesla chyba (převrácené znaménko steeringu,
vypnutý PHAT, špatné okno lagů, posun binu o 1, ztracené normalizační váhy, ...) a pytest musel
selhat. Po opravách po review přežila jen ekvivalentní změna.

## Kontrola proti pyroomacoustics

`pra_check.locate()` předá pyroomacoustics stejné biny (`freq_bins=np.arange(10, 65)`, ne
`freq_range`, která poslední bin vynechá) a stejný grid 5°. Výsledkem je maximum
`grid.values`; pyroomacoustics nemá jemné dohledání, proto se s ním porovnává hrubé maximum
vlastních metod (shodují se na stejný uzel). Srovnává se `SRP` se `srp_phat` a `NormMUSIC`
s `music` (pra `MUSIC` normalizaci binů standardně nepoužívá).

**TOPS v pyroomacoustics 0.10.1 má chybu indexace**: matice `Phi` se staví pro absolutní
čísla binů (`Phi[b]` odpovídá binu `b`), ale čte se `Phi[k]` pro `k` jako pozici v seznamu
použitých binů, zatímco odpovídající šumový podprostor `W[k]` patří binu `freq_bins[k]`.
Pásmo, které nezačíná od binu 0, tak dostane špatné frekvence. Ověřeno kopií s opravenou
indexací na 5 směrech při 30 dB: chyba klesla z 5,5 až 12° na 0,6 až 3,1° (zbytek je
kvantizace gridu). TOPS proto v tabulce reportu není a jeho selhání nelze přičítat 3D poli.

## Omezení modelu

* Rovinná vlna a bílý šum nezávislý mezi kanály; bez odrazů, větru, vlastního šumu rámu
  a difuzního pole. Tabulky ukazují spodní mez chyby, ne chování venku.
* Jeden zdroj. Počet zdrojů se neodhaduje (`n_src = 1`).
* Pozice mikrofonů jsou ideální a rychlost zvuku pevně 343 m/s; citlivost na jejich chybu se
  nesimuluje.
* SNR je nominální širokopásmové. Dron má v pásmu 300 až 2000 Hz zhruba 80 % energie, bílý šum
  21 %, takže SNR uvnitř pásma je o ~6 dB vyšší (`validate.py` vypíše přesnou hodnotu a zapíše
  `\validBandSnrGain`).
* **DAS má malý systematický posun elevace** (bez šumu medián chyby 0,58°, průměrná chyba
  elevace +0,49°; ostatní metody ~0 a 0,39°). Příčinou je únik Hannova okna ze silné složky těsně
  za okrajem pásma do binů 10 a 64: normované metody ho zprůměrují, výkonově vážený DAS ne.
  Řešení (ochranné biny, váhy podle SNR v binu) je záležitost M4.
* Normalizace binů u MVDR a MUSIC dává šumovým binům stejnou váhu jako binům s harmonickou
  složkou; u čistého tónu v šumu to metody zhoršuje.
* `localize` škáluje vstup na jednotkové RMS (absolutní prahy v metodách pak nezávisejí na
  jednotkách, např. int32 ze SAI); tichý segment vyvolá `ValueError`.
* Kovariance z 5 rámců je silně podurčená (hodnost ≤ 5 ze 16); MVDR ji řeší diagonálním
  loadingem, MUSIC a DAS ne.
* 1×8 určí elevaci jen slabě (zpoždění závisí na `cosθ`); GCC-PHAT s LS ji dopočítá z `|u| = 1`
  (v simulaci s chybou do 2° při 30 dB); u mřížkových metod se pro `1x8` testuje jen azimut.
