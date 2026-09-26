# Modely a citlivost – manuál do terénu

Krátký návod, jak na desce přepínat modely a ladit citlivost detekce, když
jdeme ven testovat s dronem. Technický popis řetězce je v [index.md](index.md).
Čísla platí pro firmware z větve `hermakam/newdetection` s výchozím modelem
`mlp_f1` (26. 9. 2026), prvním natrénovaným i na našich terénních nahrávkách.

## 1. Než začneš

Připoj se na USB konzoli desky (embedded-cli, prompt `> `). Buď terminálem,
nebo pomocníkem `boomdetect-data/tools/bdcli.py`:

```
bdcli.py --port COM7 cmd "version" "model"
```

Port COM se mezi relacemi mění, hledej VID 0483 / PID 5710.

Po každém zapnutí desky udělej dvě věci, obě se po resetu zapomenou:

```
micslot a        # živý mikrofon je na slotu A, deska startuje na B
model            # zkontroluj, že je aktivní mlp_f1 (hvězdička) s thr=8466
```

## 2. Příkazy

| příkaz | co dělá |
|---|---|
| `model` | vypíše modely v image, `*` označuje aktivní, u každého výchozí práh |
| `model <jméno>` | přepne model, např. `model gbt_f1` |
| `detect <s> [squelch] [thr] [dbg]` | běží `<s>` sekund (až 86400), vypisuje okna a alarmy |
| `detect 0 ...` | běží bez limitu, dokud v konzoli nestiskneš libovolnou klávesu; pak přijde `DETEND` |
| `micslot a` | přepne na živý mikrofon |

Argumenty `detect` jsou poziční. Když chceš zadat práh, musíš zadat i squelch
před ním. Obě čísla jsou v tisícinách: `8466` znamená 8.466, `3` znamená RMS 0.003.

```
detect 30                # výchozí squelch 3, práh aktivního modelu
detect 30 3 5800         # squelch 3, práh 5.8 (citlivější, dál dosáhne)
detect 30 3 8466 1       # výchozí práh mlp_f1 a ladicí výpis po snímcích
detect 600               # deset minut, výchozí squelch a práh
detect 0 3 8466          # bez limitu, zastaví libovolná klávesa v konzoli
```

Během `detect` deska neobsluhuje rádio, takže dlouhý běh znamená dlouhý výpadek
LoRa. Pomocník `bdcli.py` čeká na `DETEND` a klávesu poslat neumí, `detect 0`
proto pouštěj z ručního terminálu (PuTTY, Tera Term), ne přes skript.

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

Vyhodnoceno na terénních nahrávkách z 23. a 25. 9. 2026 (6× DJI Phantom 4,
11× Runner 250, 11× negativa) při squelchi 3, alarm 2 ze 4. Nové modely jsou
hodnocené „mimo fold“: každou nahrávku posuzuje model, který ji při tréninku
neslyšel. Poslední sloupec jsou falešné alarmy za hodinu na 11,6 h veřejných
negativních nahrávek, které nikdo netrénoval.

| model | výchozí práh | DJI | Runner | negativa | fal. alarmy/h | poznámka |
|---|---|---|---|---|---|---|
| **mlp_f1** (výchozí) | 8.466 | 6/6 | 8/11 | 0/11 | 0,26 | MLP 68→16→1, trénováno i na terénu a na „oddálených“ kopiích |
| gbt_f1 | 2.646 | 5/6 | 7/11 | 0/11 | 0,6 | 120 stromů, stejná data |
| mlp_v6 | 3.0 | 3/6 | 7/11 | 0/11 | 16,7 | předchozí výchozí; práh 3.0 byl laděn na Runneru |
| svm_v3 | 0.5 | – | – | – | – | pálí na čtvrtinu negativních oken, nepoužívat |
| mlp_l2 | 6.072 | 5/6 | 0/11 | 0/11 | – | jen veřejná data, Runner nezná |
| gbt_l2, gbt_reg_l2 | 4.81 / 2.28 | 0/6 | 0/11 | 0/11 | – | jen veřejná data, při svých prazích skoro nic |
| cnn_small | 5.127 | – | – | – | – | překračuje časový rozpočet (9.9 ms na okno) |

Co z toho plyne pro test venku:

- `mlp_f1` zachytí i Runner, který dřív neznal nikdo kromě `mlp_v6`. Nahrávky,
  které nechytil, jsou nejtišší (`runner_daleko`, `runner_zboku`, -48 dBFS).
- Dosah: nahrávky uměle „oddálené“ na 200 m (útlum vzduchu, o 10 dB tišší, šum
  pozadí) zachytí `mlp_f1` ve 2 ze 6 DJI a 5 z 11 Runnerů, s prahem 5.8 v 5 ze
  6 a 5 z 11. Model bez oddálených kopií v tréninku dal 0 a 1.
- **Známá slabina:** když se ze vstupu odřízne všechno nad 6 kHz, nepozná
  nic. Vzduch výšky takhle ostře neusekne, ale pásmově omezený záznam (telefon)
  ano – na přehrávky z mobilu se proto nespoléhej.
- Typ dronu, který v trénovacích datech není, zatím spolehlivě nepozná. Každý
  nový dron (FPV, jiná DJI) potřebuje vlastní nahrávky.
- `mlp_v6` a ostatní zůstávají v image pro srovnání na stejném zvuku.

## 5. Citlivost mlp_f1: jak si s prahem hrát

Vyšší práh = méně citlivé, méně falešných poplachů. Nižší práh = citlivější,
víc falešných. Tři body z vyhodnocení (stejné nahrávky jako v kapitole 4):

| práh | DJI | Runner | terénní negativa | fal. alarmy/h (veřejná negativa) |
|---|---|---|---|---|
| **8.466** (výchozí) | 6/6 | 8/11 | 0/11 | 0,26 |
| 5.8 | 6/6 | 9/11 | 0/11 | 3,1 |
| 3.1 | 6/6 | 10/11 | 2/11 | 12,0 |

Doporučený žebřík na venkovní test, každý krok 30 s se stejným manévrem dronu:

```
detect 30 3 8466
detect 30 3 5800
detect 30 3 3100
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
měnil firmware. Loguj proto celé běhy (`bdcli.py --log soubor.txt`).

## 7. Postup venku, ve zkratce

1. `micslot a`, `model`, ověř `mlp_f1 * thr=8466`.
2. Klid bez dronu: `detect 30` a sleduj `LVL rms=`, podle toho nastav squelch.
3. Rušiče bez dronu (řeč, auto, vítr) při prahu 5.8 a 8.466. Musí být `alarms=0`.
4. Dron: hover blízko, hover daleko, přelet. Žebřík prahů z kapitoly 5.
5. Stejné manévry s `model gbt_f1` a `model mlp_v6` pro srovnání.
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

Vše se loguje, nic se na desce neukládá. Po resetu jsi zpátky na mlp_f1,
prahu 8.466, squelchi 3 a slotu B.

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
4. První chunk streamu (náběh PDM) smazat můžeš, ale nemusíš, trénink z něj
   stejně zahodí 0,2 s.
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
