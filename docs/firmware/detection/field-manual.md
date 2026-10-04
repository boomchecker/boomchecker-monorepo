# Modely a citlivost – manuál do terénu

Krátký návod, jak na desce přepínat modely a ladit citlivost detekce, když
jdeme ven testovat s dronem. Technický popis řetězce je v [index.md](index.md).
Čísla platí pro firmware z větve `hermakam/newdetection` s výchozím modelem
`mlp_f2` (1. 10. 2026, od 2. 10. s prahem 7.656), natrénovaným na veřejných
datech a na terénních nahrávkách do 1. 10., s `gbt_f3` (2. 10.) jako
druhým názorem a od 3. 10. s `gbt_m1` a `mlp_m1` (modulační příznaky, viz
kap. 4), které jsou na desce zatím jen změřené, s dronem venku ne.

## 1. Než začneš

Dvě cesty, jak s deskou mluvit: pomocník `stm32node-cli`
(`fw/apps/stm32node-cli`), který umí `model`, `micslot`, `detect` i `record`,
takže celý test odbavíš z něj bez terminálu; nebo ruční terminál na USB konzoli
desky (embedded-cli, prompt `> `; PuTTY, Tera Term). Starší
`boomdetect-data/tools/bdcli.py` mluví stejnou konzolí.

```
stm32node-cli model --port COM7      # co je v image a co je aktivní
```

Port COM se mezi relacemi mění, hledej VID 0483 / PID 5710.

Po každém zapnutí desky udělej dvě věci, obě se po resetu zapomenou:

```
stm32node-cli micslot a --port COM7   # živý mik je na slotu A, deska startuje na B
stm32node-cli model --port COM7       # ověř, že je aktivní mlp_f2 (hvězdička) thr=7656
```

Výběr modelu i slotu přežije mezi jednotlivými voláními `stm32node-cli` (otevření
portu desku neresetuje), takže je nastavíš jednou a pak jen pouštíš `detect`.
Totéž jde napsat i do ruční konzole jako `micslot a` a `model`.

## 2. Příkazy

| příkaz | co dělá |
|---|---|
| `model` | vypíše modely v image, `*` označuje aktivní, u každého výchozí práh |
| `model <jméno>` | přepne model, např. `model gbt_f3` (druhý názor) nebo `model mlp_f1` (předchozí výchozí) |
| `detect <s> [squelch] [thr] [dbg] [rule]` | běží `<s>` sekund (až 86400), vypisuje okna a alarmy; `rule` = pravidlo alarmu, `2of4` (výchozí) nebo `mean4`, viz kap. 6 |
| `detect 0 ...` | běží bez limitu, dokud v konzoli nestiskneš libovolnou klávesu; pak přijde `DETEND` |
| `micslot a` | přepne na živý mikrofon |

Argumenty `detect` jsou poziční. Když chceš zadat práh, musíš zadat i squelch
před ním. Obě čísla jsou v tisícinách: `7656` znamená 7.656, `3` znamená RMS 0.003.

```
detect 30                # výchozí squelch 3, práh aktivního modelu
detect 30 3 5800         # squelch 3, práh 5.8 (citlivější, dál dosáhne)
detect 30 3 7656 1       # výchozí práh mlp_f2 a ladicí výpis po snímcích
detect 600               # deset minut, výchozí squelch a práh
detect 0 3 7656          # bez limitu, zastaví libovolná klávesa v konzoli
```

Během `detect` deska neobsluhuje rádio, takže dlouhý běh znamená dlouhý výpadek
LoRa. Běh bez limitu (`detect 0`) zastavíš v `stm32node-cli detect 0` libovolnou
klávesou, v jeho TUI konzoli `q` a Enter, nebo v ručním terminálu libovolným
bajtem. Deska se tak čistě zastaví a uvolní rádio; `stm32node-cli` vypíše
`stopped`, v ručním terminálu navíc uvidíš `DETEND`. Běh s limitem vypíše
`DETEND` vždy. (Starší `bdcli.py` klávesu poslat neumí, odtud `detect 0`
nepouštěj.)

## 3. Co deska vypisuje

```
LVL t=1.984 rms=+0.031            úroveň vstupu, asi 1x za sekundu
DET t=2.432 span=14 dec=+4.120 DRONE   jedno klasifikované okno (~448 ms)
DET t=2.880 span=14 dec=-12.500 noise
ALM t=3.328 ON hits=2/4           alarm změnil stav
DETEND windows=64 drones=27 alarms=1 first_drone=2.432 first_alarm=3.328 overrun=0 err=0
```

Pro hodnocení citlivosti sleduj čísla z `DETEND`:

- `drones` / `windows` = podíl oken nad prahem. To je hrubá citlivost.
- `alarms` = kolikrát alarm přešel z OFF na ON. To je, co by šlo rádiem.
- `first_drone` a `first_alarm` = čas (s od startu běhu) prvního okna
  označeného DRONE a prvního zapnutí alarmu; `-` když se to nestalo. U
  `gbt_m1` a `mlp_m1` odečti 2,2 s zahřívání, dřív okno přijít nemůže.
- `overrun=1` znamená ztracené vzorky, zapiš si to k běhu.

Hodnota `dec` je rozhodnutí modelu (logit). Práh se porovnává přímo s ním,
takže z výpisu vidíš, o kolik drony a rušiče přelétají nebo podlétají práh.
Zapiš si při každém běhu rozsah `dec` pro dron a pro rušiče, tím se práh ladí.

## 4. Modely v image

Vyhodnoceno na všech terénních nahrávkách z 23. 9., 25. 9. a 30. 9. 2026
(14× DJI Phantom 4, 19× Runner 250) a na 22 negativech (25. 9. venku a
1. 10. kancelář: vrtačka, skartovačka, větrák, vítr z větráku, tleskání,
luskání, bzučení pusou, klíče, sklenička, řeč) při squelchi 3, alarm 2 ze 4.
Modely z terénních dat jsou hodnocené „mimo fold“: každou nahrávku posuzuje
model, který ji při tréninku neslyšel. Poslední sloupec jsou falešné alarmy za
hodinu na 11,6 h veřejných negativních nahrávek, které nikdo netrénoval.

| model | výchozí práh | DJI | Runner | negativa | fal. alarmy/h | poznámka |
|---|---|---|---|---|---|---|
| **mlp_f2** (výchozí) | 7.656 | 11/14 | 18/19 | 4/22 | 2,0 | MLP 68→32→16→1; od 2. 10. na bodu 5 FA/h (dřív 15.855), viz kap. 5 |
| **gbt_f3** | 3.438 | 16/21 | 13/19 | 0/27 | 0,9 | les přetrénovaný i s nahrávkami z 2. 10.; jiný základ (21 DJI, 27 negativ) |
| gbt_m1 | 3.211 | 21/21 | 14/19 | 1/27 | 1,7 | 3. 10., vrstva 4 (modulace obálky); stejný základ jako gbt_f3; první 2 s běhu bez rozhodnutí; na desce změřeno 4. 10., venku zatím ne |
| mlp_m1 | 8.400 | 21/21 | 14/19 | 2/27 | 1,9 | 3. 10., vrstva 4, MLP 78→32→16→1; nejvyšší polní AUC, slabší nad mikrofonem 20–30 m z 30. 9.; na desce změřeno 4. 10., venku zatím ne |
| gbt_f2 | 3.263 | 9/14 | 14/19 | 0/22 | 1,9 | 200 stromů, data do 1. 10.; nahrazen gbt_f3 |
| mlp_f1 | 8.466 | 6/14 | 10/19 | 2/22 | 0,26 | předchozí výchozí (26. 9.); z nahrávek 30. 9. chytil 2 z 16, nic nad 10 m |
| gbt_f1 | 2.646 | 5/14 | 9/19 | 0/22 | 0,6 | 120 stromů, data do 25. 9. |
| mlp_v6 | 3.0 | 7/14 | 10/19 | 5/22 | 16,7 | jen veřejná data; práh 3.0 byl laděn na Runneru |
| svm_v3 | 0.5 | – | – | – | – | pálí na čtvrtinu negativních oken, nepoužívat |
| mlp_l2, gbt_l2, gbt_reg_l2 | 6.07 / 4.81 / 2.28 | – | – | – | – | jen veřejná data, při svých prazích skoro nic |
| cnn_small | 5.127 | – | – | – | – | překračuje časový rozpočet (9.9 ms na okno) |

Co z toho plyne pro test venku:

- **Dosah je teď hlavní limit, ne typ dronu.** DJI přímo nad mikrofonem
  2. 10. (`mlp_f2` při výchozím prahu 7.656, model nahrávky neviděl; podíl
  oken nad prahem, `*` = alarm): 20 m 58 %\*, 30 m 77 %\*, 40 m 23 %\*, 60 m
  31 %\*, 80 m 9 %\*, 90 m 16 %\*; vzlet z 10 m do 100 m: 64 / 36 / 25 / 7 / 0 / 0 %
  po desetisekundách, tedy odeznění kolem 60–70 m. `gbt_f3` chytí všech sedm
  výšek také. Z 30. 9.: DJI 10–50 m ano, 50 m přes korunu stromu a 70 m 0 %
  (tam je nad 6 kHz už jen úroveň pozadí); Runner 20 m ano, 40 m jen těsně.
  V RMS je DJI nad hlavou od 60 m nerozlišitelná od pozadí (0,004).
- Venku 30. 9. tvořil vítr a dunění 85–90 % energie (pod 300 Hz) a DJI na
  10 m byla třikrát tišší než „blízko“ z 25. 9. – přesně tohle staré modely
  neznaly, proto `mlp_f1` z těch nahrávek nechytil skoro nic. Od 2. 10. jsou v
  datech i venkovní negativa (pozadí, klimatizace, lidé, 30 min dopravy u
  kruhového objezdu): na nich nasazené modely nepálí vůbec, obava z
  „vítr = dron“ se nepotvrdila.
- `gbt_f3` je na vlastních negativech úplně čistý (0/27 včetně 30 min
  dopravy) a chytí všech sedm výšek z 2. 10. Hodí se jako druhý názor: když
  hlásí oba, je to dron. Přetrénovaný MLP naopak lepší nebyl, proto zůstává
  `mlp_f2` z 1. 10.
- Umělé „oddálení“ nahrávek na 50–500 m v tréninku (běh `fw2_far`) nepomohlo
  vůbec, 50–70 m zůstalo na nule. Dosah posunou jen další skutečné nahrávky
  z větších vzdáleností.
- **Známá slabina trvá:** když se ze vstupu odřízne všechno nad 6 kHz, model
  nepozná nic. Na přehrávky z mobilu se nespoléhej.
- Typ dronu, který v trénovacích datech není, zatím spolehlivě nepozná. Na
  cizích dronech (sada Halmstad) jsou `mlp_f2` i `gbt_f2` na úrovni `mlp_f1`
  (5–7 z 21 klipů). Každý nový dron (FPV, jiná DJI) potřebuje vlastní nahrávky.
- `mlp_f1` a ostatní zůstávají v image pro srovnání na stejném zvuku;
  `model mlp_f1` je rollback.
- **Nové od 3. 10.: `gbt_m1` a `mlp_m1` (vrstva 4).** K dosavadním příznakům
  přibylo modulační spektrum obálky pásma 1–4 kHz za poslední dvě sekundy:
  visící dron „seká“ svůj šum frekvencí průchodu listů (Phantom 4 asi
  170–185 Hz, s harmonickou na dvojnásobku) a tahle čára drží i tam, kde je
  spektrum už na úrovni pozadí. Mimo fold, stejné foldy jako `gbt_f3`, práh
  5 FA/h: DJI z 2. 10. na 60/80/90 m 86/85/71 % oken (`gbt_m1`) a 87/83/72 %
  (`mlp_m1`) proti 38/44/17 % u rodiny `mlp_f2`; 70 m z 30. 9. 78/89 % místo
  0 %; poslední úsek stoupání do 100 m 64/57 % místo 0 %. Negativa: `gbt_m1`
  1/27, `mlp_m1` 2/27, 30 min dopravy 0 alarmů u obou. Runner na 40 m ani
  tady nic. Modulace je podpis stálých otáček: při manévrech se čára rozmaže
  (30. 9. na 30 m jen 6 dB, jinde 9–11 dB). Dvě věci navíc: první dvě
  sekundy po startu `detect` tyto modely nic nevypíšou (plní se kruh obálky,
  totéž po výpadku vzorků), první rozhodnutí přijde v čase 2,2 s; a venku s
  dronem zatím neběžely. Na desce B ověřeno 4. 10.: `selftest` sedí, snímek
  uzavírající okno trvá 4,7 ms (u `mlp_f2` 1,2 ms) z rozpočtu 21 ms, běžný
  snímek 1,2 ms, žádný overrun, v kanceláři bez alarmu.

## 5. Citlivost mlp_f2: jak si s prahem hrát

Vyšší práh = méně citlivé, méně falešných poplachů. Nižší práh = citlivější,
víc falešných. Do 2. 10. byl výchozí práh `mlp_f2` přísný bod 1 FA/h
(15.855); měření s DJI přímo nad mikrofonem ukázalo, že surový logit modelu je
kladný až do 90 m a ten práh zahazoval všechno nad 30 m, tak je teď výchozí
běžný bod 5 FA/h jako u ostatních modelů. Body z vyhodnocení na nahrávkách do
1. 10. (mimo fold, základ 14 DJI / 19 Runner / 22 negativ):

| práh | DJI | Runner | terénní negativa | fal. alarmy/h (veřejná negativa) |
|---|---|---|---|---|
| 15.855 (1 FA/h, dřívější výchozí) | 10/14 | 15/19 | 1/22 (bzučení pusou) | 0,00 |
| **7.656** (5 FA/h, výchozí) | 11/14 | 18/19 | 4/22 (vrtačka, bzučení, skartovačka, větrák) | 2,0 |
| 5.57 (20 FA/h) | 12/14 | 18/19 | 5/22 | 3,9 |

A totéž na nahrávkách z 2. 10. a 30. 9. (model je neviděl), s pravidlem alarmu
z kapitoly 6:

| práh + pravidlo | DJI 2. 10. 20–90 m (/7) | drony 30. 9. (/16) | venkovní negativa (/4) | kancelář (/11) | doprava (30 min) |
|---|---|---|---|---|---|
| 5.57 + 2of4 | 7 | 15 | 2 | 4 | 24 alarmů/h |
| **7.656 + 2of4** (výchozí) | **7** | **13** | 0 | 3 | 0 |
| 7.656 + mean4 | 7 | 11 | 0 | 2 | 0 |
| **9.0 + mean4** | 6 | 10 | 0 | **0** | 0 |
| 15.855 + 2of4 | 3 | 6 | 0 | 0 | 0 |

Pod 7 začne pálit doprava; od 7.656 výš je doprava i venkovní pozadí čisté.
Výchozí bod dává největší dosah (DJI nad hlavou do 90 m, vzlet sleduje do
~60–70 m), `9000 + mean4` nulu falešných alarmů na všem, co jsme zatím
nahráli. Pro `gbt_f3` je žebřík 4.20 (1 FA/h: 11/21, 7/19, 0/27) – **3.438**
(5 FA/h, výchozí: 16/21, 13/19, 0/27, doprava 0) – 2.52 (20 FA/h: 17/21,
15/19, 5/27).

Doporučený žebřík na venkovní test, každý krok 30 s se stejným manévrem dronu:

```
detect 30 3 7656              # výchozí
detect 30 3 9000 0 mean4      # nula falešných
detect 30 3 5570              # jen pro měření dosahu, pálí doprava
```

A ke každému kroku stejnou sekvenci bez dronu (mluvení, chůze, auto, vítr),
aby byl vidět odstup. Práh volíme jako nejvyšší hodnotu, při které dron stále
dává alespoň třetinu oken, a nejnižší, při které rušiče nedají žádný alarm.

## 6. Brány

### Squelch (RMS brána)

Druhý argument `detect`. Snímek s RMS pod squelchem se nezpracuje vůbec:
nevznikne okno, nehne se alarm. Výchozí je **3** (RMS 0.003), do 26. 9. to bylo
10. Proč: venku bylo pozadí kolem RMS 0.004 a dron ve střední výšce nebo dál
0.004–0.009, takže při 10 většina letu nedala ani jedno okno. Tichá místnost
(0.0026) ani při 3 okno nedá, okno potřebuje 14 snímků v řadě nad branou.

- **Nikdy netestuj verdikty se squelchem 0.** Příznaky jsou nezávislé na
  hlasitosti, takže i šum na úrovni floor má „tvar“ a model na něm pálí.
- Když bude dron sotva slyšet, uvidíš v `DETEND` málo oken, to znamená, že
  brána maže moc. Pod 3 ale nechoď, tam už začíná vlastní šum mikrofonu.
- Ve větru nebo u silnice: zkus **10–20**, aby se hluk pozadí nedostal ke
  klasifikátoru vůbec. Sleduj `LVL rms=`, brána má sedět těsně nad klidem.
- Celopásmové RMS je dané hlavně hukotem pod 200 Hz, dron má energii výš.
  Brána počítaná jen z pásma nad ~300 Hz by drony od ticha oddělila o 10 dB
  líp. Je to v plánu, zatím jen RMS.

Kontrola nastavení: `windows` v `DETEND` proti délce běhu. 30 s dává maximálně
asi 66 oken. Když je `windows` výrazně méně, brána ořezává.

### Alarm: hlasování K z N, nebo průměr

Pátý argument `detect` (za `dbg`), při každém běhu znovu, nic se neukládá:

```
detect 30 3 7656 0 2of4     # výchozí: zapne při 2 DRONE ze 4 posledních oken, vypne pod 1
detect 30 3 7656 0 mean4    # průměr posledních 4 rozhodnutí vůči prahu >= 0
detect 30 3 7656 0 1of4     # volnější hlasování: stačí 1 okno ze 4
stm32node-cli detect 30 --rule mean4 --port COM7
```

Okno je 448 ms, takže alarm znamená zhruba sekundu dronu.

- **`<k>of<n>`** (hlasování, výchozí `2of4`): počítá okna nad prahem. Zapne
  při `k` z posledních `n`, vypne, když jich je méně než `k−1` (nejméně 1), aby
  rozhodnutí kolem prahu neblikalo. Přísnější: `3of4`, `3of8`. Volnější:
  `1of4`, `2of8`.
- **`mean<n>`** (měkká integrace): průměruje samotná rozhodnutí (`dec` minus
  práh) za posledních `n` oken a hlásí, dokud je průměr ≥ 0. Řada oken těsně
  pod prahem nikdy nezahlásí, jedno okno vysoko nad prahem utáhne tři slabá.
  V řádku `ALM` je pak místo `hits=k/n` hodnota `mean=+d.ddd/n`.
- `n` je 1 až 32.

Co to dělá s `mlp_f2` při výchozím prahu, spočítáno zpětně z rozhodnutí na
všech terénních nahrávkách (mimo fold):

| pravidlo | DJI /14 | Runner /19 | negativa /22 | veřejné fal. alarmy/h |
|---|---|---|---|---|
| `2of4` (výchozí) | 10 | 15 | 1 (bzučení) | 0,00 |
| **`mean4`** | 10 | 15 | **0** | 0,00 |
| `1of4` | 12 | 16 | 2 | 0,09 |
| `3of8` | 9 | 13 | 0 | 0,00 |

`mean4` tedy odstraní jediný falešný alarm beze ztráty detekce; `1of4` přidá
dvě DJI a jednu Runner nahrávku za jeden falešný alarm navíc. Venku vyzkoušej
obojí na stejném manévru. Hodnocení zpětně funguje dál: z řádků `DET` v logu
spočítáš, co by dalo jiné pravidlo, aniž bys běh opakoval
(`analysis/2026-10-01-retrain/decision_rules.py` to dělá přes všechny
nahrávky). Loguj proto celé běhy (`stm32node-cli detect ... > beh.txt`).

Výchozí pravidlo zůstává konstantou při kompilaci (`DETECT_ALARM_*` v
`fw/bom-stm32node/App/detect/detect_service.h`); změnit ho natrvalo znamená
přeflashovat.

## 7. Postup venku, ve zkratce

1. `micslot a`, `model`, ověř `mlp_f2 * thr=7656`.
2. Klid bez dronu: `detect 30` a sleduj `LVL rms=`, podle toho nastav squelch.
   Zároveň **nahraj pozadí bez dronu** (`record 1 120`), v tréninku chybí.
3. Rušiče bez dronu (řeč, auto, vítr) při prahu 7.656 a 9.0 (`mean4`). Musí být `alarms=0`.
4. Dron: hover blízko, hover daleko, přelet; k tomu známé vzdálenosti
   (20, 40, 60, 80 m), protože dosah je teď hlavní otázka. Žebřík prahů z kapitoly 5.
5. Stejné manévry s `model gbt_f3` (druhý názor), `model gbt_m1` a `model mlp_m1`
   (modulace, vrstva 4) a `model mlp_f1` pro srovnání.
6. Nahraj i syrový zvuk jako WAV, doma ho pak přehraje
   `bdtrain score nahravka.wav` přes všechny modely bez dalšího létání:

   ```
   stm32node-cli record 10 20 --port COM7
   ```

   To nahraje 20 souborů po 10 s do složky `recordings/batch-<datum-čas>/`
   jako `chunk-001.wav` až `chunk-020.wav`, každý se uloží hned, jak doběhne,
   a `index.csv` vedle nich říká, který kus je z kterého streamu a zda měl
   overrun. Ctrl-C zastaví a nechá, co už je hotové. Bez druhého čísla je to
   jeden souvislý záběr: `stm32node-cli record 30`. Totéž funguje i v TUI
   konzoli aplikace (`record 10 20`). Nástroj je v `fw/apps/stm32node-cli`,
   spouští se přes `.venv\Scripts\stm32node-cli.exe`.

   Nahrávej souběžně s manévry dronu a piš si k číslům kusů, co se dělo
   (hover 2 m: 003–005, přelet: 006, ...). Doma pak práh doladíš přesně na
   to, co bylo slyšet, bez dalšího létání.

Vše se loguje, nic se na desce neukládá. Po resetu jsi zpátky na mlp_f2,
prahu 7.656, squelchi 3 a slotu B.

## 8. Nahrávky pro trénink

Každá nahrávka z terénu je pro trénink cennější než hodina veřejných dat.
Aby šla rovnou použít (`bdtrain` ji najde sám):

1. Složku z `record` přejmenuj podle obsahu a roztřiď do sezení podle data:

   ```
   boomdetect-data/raw/field/2026-10-02/Positive/dji_hover_50m/chunk-*.wav + index.csv
   boomdetect-data/raw/field/2026-10-02/Negative/silnice_kamiony/...
   ```

2. Pozitiva začínají jménem dronu: `dji_...` (Phantom 4), `runner_...`
   (Runner 250). Nový dron = nová předpona, stačí ji doplnit do
   `DRONE_PREFIXES` v `fw/common/boomdetect/training/boomdetect_train/datasets/field.py`.
3. Jméno složky negativ je jejich kategorie a objeví se v reportu (co mate model).
4. Nahrávky ze staršího firmwaru (do 30. 9. 2026 včetně) začínají každý stream
   lupem: skok DC, ~0,12 s na plné škále, doznívá do ~0,4 s. První chunk
   streamu proto smaž; když zůstane, trénink z jeho začátku zahodí 0,5 s.
   Firmware s opravou náběhu mikrofonu (zahodí prvních 107 ms) lup nemá.
5. Pak `bdtrain manifest`, `bdtrain features field --force` a trénink s `--field`.

Co nejvíc chybí, v tomhle pořadí:

- **dron v různých vzdálenostech** (10 / 20 / 50 / 100 m) a výškách, s poznámkou,
  jak daleko byl – dnes nevíme, kde dosah končí;
- **dlouhá pozadí** (desítky minut: park, silnice, vítr, déšť), jinak nejde
  změřit falešné alarmy za hodinu;
- **skutečné rušiče, ne z telefonu** – fén a včely z mobilu jsou ořezané nad
  5 kHz a model se na tom učí zkratku „bez výšek = není dron“;
- **další typy dronů**, hlavně FPV, a kdyby šlo, i jiné DJI;
- delší souvislé záběry (1–3 min na manévr) místo 9–19 s.
