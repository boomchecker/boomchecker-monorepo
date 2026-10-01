# Modely a citlivost – manuál do terénu

Krátký návod, jak na desce přepínat modely a ladit citlivost detekce, když
jdeme ven testovat s dronem. Technický popis řetězce je v [index.md](index.md).
Čísla platí pro firmware z větve `hermakam/newdetection` s výchozím modelem
`mlp_f2` (1. 10. 2026), natrénovaným na veřejných datech a na všech našich
terénních nahrávkách včetně dronů na 10–70 m z 30. 9.

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
stm32node-cli model --port COM7       # ověř, že je aktivní mlp_f2 (hvězdička) thr=15855
```

Výběr modelu i slotu přežije mezi jednotlivými voláními `stm32node-cli` (otevření
portu desku neresetuje), takže je nastavíš jednou a pak jen pouštíš `detect`.
Totéž jde napsat i do ruční konzole jako `micslot a` a `model`.

## 2. Příkazy

| příkaz | co dělá |
|---|---|
| `model` | vypíše modely v image, `*` označuje aktivní, u každého výchozí práh |
| `model <jméno>` | přepne model, např. `model gbt_f2` (druhý názor) nebo `model mlp_f1` (předchozí výchozí) |
| `detect <s> [squelch] [thr] [dbg]` | běží `<s>` sekund (až 86400), vypisuje okna a alarmy |
| `detect 0 ...` | běží bez limitu, dokud v konzoli nestiskneš libovolnou klávesu; pak přijde `DETEND` |
| `micslot a` | přepne na živý mikrofon |

Argumenty `detect` jsou poziční. Když chceš zadat práh, musíš zadat i squelch
před ním. Obě čísla jsou v tisícinách: `15855` znamená 15.855, `3` znamená RMS 0.003.

```
detect 30                # výchozí squelch 3, práh aktivního modelu
detect 30 3 5800         # squelch 3, práh 5.8 (citlivější, dál dosáhne)
detect 30 3 15855 1      # výchozí práh mlp_f2 a ladicí výpis po snímcích
detect 600               # deset minut, výchozí squelch a práh
detect 0 3 15855         # bez limitu, zastaví libovolná klávesa v konzoli
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
DETEND windows=64 drones=27 alarms=1 overrun=0 err=0
```

Pro hodnocení citlivosti sleduj dvě čísla z `DETEND`:

- `drones` / `windows` = podíl oken nad prahem. To je hrubá citlivost.
- `alarms` = kolikrát alarm přešel z OFF na ON. To je, co by šlo rádiem.

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
| **mlp_f2** (výchozí) | 15.855 | 10/14 | 15/19 | 1/22 | 0,00 | MLP 68→32→16→1; práh je přísný bod 1 FA/h, viz kap. 5 |
| gbt_f2 | 3.263 | 9/14 | 14/19 | 0/22 | 1,9 | 200 stromů, stejná data; druhý názor jiné rodiny |
| mlp_f1 | 8.466 | 6/14 | 10/19 | 2/22 | 0,26 | předchozí výchozí (26. 9.); z nahrávek 30. 9. chytil 2 z 16, nic nad 10 m |
| gbt_f1 | 2.646 | 5/14 | 9/19 | 0/22 | 0,6 | 120 stromů, data do 25. 9. |
| mlp_v6 | 3.0 | 7/14 | 10/19 | 5/22 | 16,7 | jen veřejná data; práh 3.0 byl laděn na Runneru |
| svm_v3 | 0.5 | – | – | – | – | pálí na čtvrtinu negativních oken, nepoužívat |
| mlp_l2, gbt_l2, gbt_reg_l2 | 6.07 / 4.81 / 2.28 | – | – | – | – | jen veřejná data, při svých prazích skoro nic |
| cnn_small | 5.127 | – | – | – | – | překračuje časový rozpočet (9.9 ms na okno) |

Co z toho plyne pro test venku:

- **Dosah je teď hlavní limit, ne typ dronu.** Nahrávky z 30. 9. podle
  vzdálenosti, `mlp_f2` při výchozím prahu, hodnoceno mimo fold (podíl oken
  nad prahem, `*` = alarm): DJI 10 m 19 %\*, 15 m 56 %\*, 20 m 38 %\*, 30 m 2 %,
  40 m 21 %\*, 50 m 47 %\*, 50 m přes korunu stromu 0 %, 70 m 0 %; Runner 20 m
  71–94 %\* při všech výkonech motorů, 40 m 0–1 %. Při chůzi s Runnerem od
  mikrofonu detekce odezní kolem 25–30 m. Žádný z modelů nedal nic na DJI
  od 50 m přes strom a na 70 m: tam je nad 6 kHz už jen úroveň pozadí.
- Venku 30. 9. tvořil vítr a dunění 85–90 % energie (pod 300 Hz) a DJI na
  10 m byla třikrát tišší než „blízko“ z 25. 9. – přesně tohle staré modely
  neznaly, proto `mlp_f1` z těch nahrávek nechytil skoro nic. Nový trénink to
  zná, ale jen z jednoho dne. **Příště nahraj i pozadí bez dronu** na stejném
  místě (aspoň 2–3 min), to v datech chybí.
- `gbt_f2` je na vlastních negativech úplně čistý (0/22), na veřejných pálí
  častěji (1,9/h). Hodí se jako druhý názor: když hlásí oba, je to dron.
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

## 5. Citlivost mlp_f2: jak si s prahem hrát

Vyšší práh = méně citlivé, méně falešných poplachů. Nižší práh = citlivější,
víc falešných. Výchozí práh `mlp_f2` je schválně přísný (1 falešné okno za
hodinu na veřejných negativech); ostatní modely mají 5. Tři body z vyhodnocení
(stejné nahrávky jako v kapitole 4, mimo fold):

| práh | DJI | Runner | terénní negativa | fal. alarmy/h (veřejná negativa) |
|---|---|---|---|---|
| **15.855** (výchozí, 1 FA/h) | 10/14 | 15/19 | 1/22 (bzučení pusou) | 0,00 |
| 7.66 (5 FA/h) | 11/14 | 18/19 | 4/22 (vrtačka, bzučení, skartovačka, větrák) | 2,0 |
| 5.57 (20 FA/h) | 12/14 | 18/19 | 5/22 | 3,9 |

Na nahrávkách z 30. 9. přidá práh 7.66 k výchozímu bodu DJI 40 m (48 % oken) a
50 m (87 %) a Runner na 40 m (3–10 %, těsně nad alarmem); 30 m DJI a 70 m
nedá ani ten. Pro `gbt_f2` je žebřík 4.12 (1 FA/h: 6/14, 8/19, 0/22) –
**3.263** (5 FA/h, výchozí) – 2.46 (20 FA/h: 11/14, 17/19, 2/22).

Doporučený žebřík na venkovní test, každý krok 30 s se stejným manévrem dronu:

```
detect 30 3 15855
detect 30 3 7660
detect 30 3 5570
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

### Alarm (K z N)

Alarm se zapne, když jsou alespoň **2 ze 4** posledních oken DRONE, a vypne,
když jich je méně než 1. Okno je 448 ms, takže alarm znamená zhruba sekundu
dronu. Tahle pravidla jsou zatím konstanty při kompilaci:

```
fw/bom-stm32node/App/detect/detect_service.h
#define DETECT_ALARM_N     4
#define DETECT_ALARM_K_ON  2
#define DETECT_ALARM_K_OFF 1
```

Přísnější brána (méně falešných): 3 ze 4 nebo 3 ze 6. Volnější: 1 ze 4 nebo
2 ze 8. Změna znamená přeflashovat. Venku se ale dá vyhodnotit zpětně: z
řádků `DET` v logu spočítáš, kolik alarmů by dala jiná kombinace, aniž bys
měnil firmware. Loguj proto celé běhy (`stm32node-cli detect ... > beh.txt`,
nebo `bdcli.py --log soubor.txt`).

## 7. Postup venku, ve zkratce

1. `micslot a`, `model`, ověř `mlp_f2 * thr=15855`.
2. Klid bez dronu: `detect 30` a sleduj `LVL rms=`, podle toho nastav squelch.
   Zároveň **nahraj pozadí bez dronu** (`record 1 120`), v tréninku chybí.
3. Rušiče bez dronu (řeč, auto, vítr) při prahu 15.855 a 7.66. Musí být `alarms=0`.
4. Dron: hover blízko, hover daleko, přelet; k tomu známé vzdálenosti
   (20, 40, 60, 80 m), protože dosah je teď hlavní otázka. Žebřík prahů z kapitoly 5.
5. Stejné manévry s `model gbt_f2` a `model mlp_f1` pro srovnání.
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
prahu 15.855, squelchi 3 a slotu B.

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
