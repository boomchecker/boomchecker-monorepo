# Modely a citlivost – manuál do terénu

Krátký návod, jak na desce přepínat modely a ladit citlivost detekce, když
jdeme ven testovat s dronem. Technický popis řetězce je v [index.md](index.md).
Čísla platí pro firmware z větve `hermakam/newdetection` (commit `47e57e6c`).

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
model            # zkontroluj, že je aktivní mlp_v6 (hvězdička) s thr=3000
```

## 2. Příkazy

| příkaz | co dělá |
|---|---|
| `model` | vypíše modely v image, `*` označuje aktivní, u každého výchozí práh |
| `model <jméno>` | přepne model, např. `model mlp_l2` |
| `detect <s> [squelch] [thr] [dbg]` | běží `<s>` sekund (1..60), vypisuje okna a alarmy |
| `micslot a` | přepne na živý mikrofon |

Argumenty `detect` jsou poziční. Když chceš zadat práh, musíš zadat i squelch
před ním. Obě čísla jsou v tisícinách: `3000` znamená 3.0, `10` znamená RMS 0.010.

```
detect 30                # výchozí squelch 10, práh modelu
detect 30 10 1000        # squelch 10, práh 1.0
detect 30 10 3000 1      # totéž s prahem 3.0 a ladicím výpisem po snímcích
```

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

| model | výchozí práh | na mikrofonu uzlu (test 23. 9. 2026) |
|---|---|---|
| **mlp_v6** (výchozí) | 3.0 | jediný, který funguje: 33–47 % oken za letu, 0 % na rušičích |
| svm_v3 | 0.5 | dron +0.8, rušiče -0.8: neodděluje, 27 % falešných na rušičích |
| mlp_l2 | 6.072 | nejlepší na veřejných datech (95 %), na našem mikrofonu dron hodnotí jako šum |
| gbt_l2 | 4.808 | dron pod prahem |
| gbt_reg_l2 | 2.275 | dron -1.0, rušiče -3.6: skoro neodděluje |
| cnn_small | 5.127 | dron -2.2; navíc překračuje časový rozpočet (9.9 ms na okno) |

Rušiče = řeč, tleskání, klávesnice, spotřebiče. Motory na zemi bez vzletu
nezachytil žádný model (0 %), to je známé.

Ostatní modely jsou v image pro srovnání na stejném zvuku. Venku má smysl je
pustit na stejný manévr hned po mlp_v6, ale očekávej, že budou horší. Důvod:
spektrální příznaky layoutu 2 (podíl energie nad 4 kHz, harmonicita) ukazují
na našem mikrofonu opačným směrem, než jak je naučila veřejná data.

## 5. Citlivost mlp_v6: jak si s prahem hrát

Vyšší práh = méně citlivé, méně falešných poplachů. Nižší práh = citlivější,
víc falešných. Změřeno na jednom dronu v jedné místnosti, 2,5 minuty, takže
jde o kalibraci, ne validaci:

| práh | dron ve vzduchu (% oken) | rušiče (% oken) | alarmy na rušiče | poznámka |
|---|---|---|---|---|
| 15.0 | 0–5 % | 0 % | 0 | starý default, prakticky slepý |
| **3.0** | 32–47 % | 0 % | 0 | současný default |
| 1.0 | 52–70 % | 2 % | 0 | rušiče vrcholily na 1.48 |
| < 1.0 | více | roste | hrozí | pod 1.5 začíná pálit na řeč a tlesknutí |

Doporučený žebřík na venkovní test, každý krok 30 s se stejným manévrem dronu:

```
detect 30 10 1000
detect 30 10 2000
detect 30 10 3000
detect 30 10 5000
detect 30 10 8000
```

A ke každému kroku stejnou sekvenci bez dronu (mluvení, chůze, auto, vítr),
aby byl vidět odstup. Práh volíme jako nejvyšší hodnotu, při které dron stále
dává alespoň třetinu oken, a nejnižší, při které rušiče nedají žádný alarm.

Pozor na vzdálenost: hover přes místnost dal 33 % oken proti 41 % na 2 m.
Venku bude dron dál a tišší, takže se možná posuneme spíš k 2.0 než k 5.0.

## 6. Brány

### Squelch (RMS brána)

Druhý argument `detect`. Snímek s RMS pod squelchem se nezpracuje vůbec:
nevznikne okno, nehne se alarm. Výchozí je 10 (RMS 0.010), tichá místnost byla
celá pod ním.

- **Nikdy netestuj verdikty se squelchem 0.** Příznaky jsou nezávislé na
  hlasitosti, takže i šum na úrovni floor má „tvar“ a model na něm pálí.
- Venku v klidu s dronem daleko: zkus **5**. Když bude dron sotva slyšet, uvidíš
  v `DETEND` málo oken, to znamená, že brána maže moc.
- Ve větru nebo u silnice: zkus **20–30**, aby se hluk pozadí nedostal ke
  klasifikátoru vůbec. Sleduj `LVL rms=`, brána má sedět těsně nad klidem.

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

1. `micslot a`, `model`, ověř `mlp_v6 * thr=3000`.
2. Klid bez dronu: `detect 30` a sleduj `LVL rms=`, podle toho nastav squelch.
3. Rušiče bez dronu (řeč, auto, vítr) při prahu 1.0 a 3.0. Musí být `alarms=0`.
4. Dron: hover blízko, hover daleko, přelet. Žebřík prahů z kapitoly 5.
5. Stejné manévry s `model mlp_l2` a `model gbt_reg_l2` pro srovnání.
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

Vše se loguje, nic se na desce neukládá. Po resetu jsi zpátky na mlp_v6,
prahu 3.0, squelchi 10 a slotu B.
