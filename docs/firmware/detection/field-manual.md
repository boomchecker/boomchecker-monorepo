# Modely a citlivost – manuál do terénu

Krátký návod, jak na desce přepínat modely a ladit citlivost detekce, když
jdeme ven testovat s dronem. Technický popis řetězce je v [index.md](index.md).
Výchozí model je `gbt_m1` (vrstva 4 s modulací obálky, práh 3.211), ověřený
s dronem venku; `mlp_f2` (předchozí výchozí) a `mlp_m1` zůstávají v image pro
srovnání. Nově jsou v image i sítě na gammatonovém spektrogramu (`crnn_h`,
`crnn_hl`) a jejich dvojice s `gbt_m1` (`or_h`, `or_hl`, `or_l`), viz
kapitolu 4b; venku zatím ověřené nejsou.

## 1. Než začneš

Dvě cesty, jak s deskou mluvit: pomocník `stm32node-cli`
(`fw/apps/stm32node-cli`), který umí `model`, `micslot`, `detect` i `record`,
takže celý test odbavíš z něj bez terminálu; nebo ruční terminál na USB konzoli
desky (embedded-cli, prompt `> `; PuTTY, Tera Term).

```
stm32node-cli model --port COM7      # co je v image a co je aktivní
```

Port COM se mezi relacemi mění, hledej VID 0483 / PID 5710.

Po každém zapnutí desky udělej dvě věci, obě se po resetu zapomenou:

```
stm32node-cli micslot a --port COM7   # živý mik je na slotu A, deska startuje na B
stm32node-cli model --port COM7       # ověř, že je aktivní gbt_m1 (hvězdička) thr=3211
```

Výběr modelu i slotu přežije mezi jednotlivými voláními `stm32node-cli` (otevření
portu desku neresetuje), takže je nastavíš jednou a pak jen pouštíš `detect`.
Totéž jde napsat i do ruční konzole jako `micslot a` a `model`.

## 2. Příkazy

| příkaz | co dělá |
|---|---|
| `model` | vypíše modely v image, `*` označuje aktivní, u každého výchozí práh |
| `model <jméno>` | přepne model, např. `model mlp_f2` (předchozí výchozí, vrstva 2), `model mlp_m1` (MLP na vrstvě 4) nebo `model or_h` (nové sítě, kap. 4b) |
| `detect <s> [squelch] [thr] [dbg] [rule]` | běží `<s>` sekund (až 86400), vypisuje okna a alarmy; `rule` = pravidlo alarmu, `2of4` (výchozí) nebo `mean4`, viz kap. 6 |
| `detect 0 ...` | běží bez limitu, dokud v konzoli nestiskneš libovolnou klávesu; pak přijde `DETEND` |
| `micslot a` | přepne na živý mikrofon |

Argumenty `detect` jsou poziční. Když chceš zadat práh, musíš zadat i squelch
před ním. Obě čísla jsou v tisícinách: `3211` znamená 3.211, `3` znamená RMS 0.003.

```
detect 30                # výchozí squelch 3, práh aktivního modelu
detect 30 3 2500         # squelch 3, práh 2.5 (citlivější, dál dosáhne)
detect 30 3 3211 1       # výchozí práh gbt_m1 a ladicí výpis po snímcích
detect 600               # deset minut, výchozí squelch a práh
detect 0 3 3211          # bez limitu, zastaví libovolná klávesa v konzoli
```

Práh platí pro aktivní model: po `model mlp_f2` je jeho výchozí 7656.

Během `detect` deska neobsluhuje rádio, takže dlouhý běh znamená dlouhý výpadek
LoRa. Běh bez limitu (`detect 0`) zastavíš v `stm32node-cli detect 0` libovolnou
klávesou, v jeho TUI konzoli `q` a Enter, nebo v ručním terminálu libovolným
bajtem. Deska se tak čistě zastaví a uvolní rádio; `stm32node-cli` vypíše
`stopped`, v ručním terminálu navíc uvidíš `DETEND`. Běh s limitem vypíše
`DETEND` vždy.

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
  `gbt_m1` a `mlp_m1` odečti 2,2 s zahřívání (plní se kruh obálky, totéž po
  výpadku vzorků), dřív okno přijít nemůže.
- `overrun=1` znamená ztracené vzorky, zapiš si to k běhu.
- U sítí (`crnn_*`, `or_*`) se síť počítá po kouscích v dalších blocích
  mikrofonu, takže řádek `DET` přijde asi o 0,1–0,3 s později, než okno
  skončilo; čas `t` v něm je pořád čas okna. První rozhodnutí: `crnn_h` a
  `or_h` asi 0,5 s od startu, `crnn_hl`, `or_hl` a `or_l` asi 1,4 s (čekají
  na sekundu spektrogramu); modulační čísla se přidají po 2 a 4 s, na ně
  nečekají.
- S `dbg` = 1 přijde před `DETEND` řádek `DETDBG h_max= m_max= w_max=
  loop_max= skipped=`: nejpomalejší blok PDM, snímek, kousek sítě a celý
  průchod smyčkou v µs (blok má 21 333 µs) a počet oken zahozených, protože
  síť ještě počítala to předchozí (má být 0).

Hodnota `dec` je rozhodnutí modelu (logit). Práh se porovnává přímo s ním,
takže z výpisu vidíš, o kolik drony a rušiče přelétají nebo podlétají práh.
Zapiš si při každém běhu rozsah `dec` pro dron a pro rušiče, tím se práh ladí.

## 4. Modely v image

Vyhodnoceno „mimo fold“: každou terénní nahrávku posuzuje model, který ji při
tréninku neslyšel; squelch 3, alarm 2 ze 4. `gbt_m1` a `mlp_m1` na nahrávkách do
2. 10. (21× DJI Phantom 4, 19× Runner 250, 27 negativ včetně venkovního pozadí,
klimatizace, lidí a 30 min dopravy), `mlp_f2` a `mlp_v6` na starším základu do
1. 10. (14 DJI, 19 Runner, 22 negativ). Sloupec fal. alarmy/h = falešné alarmy
za hodinu na 11,6 h veřejných negativních nahrávek, které nikdo netrénoval.

| model | výchozí práh | DJI | Runner | negativa | fal. alarmy/h | poznámka |
|---|---|---|---|---|---|---|
| **gbt_m1** (výchozí) | 3.211 | 21/21 | 14/19 | 1/27 | 1,7 | vrstva 4 (modulace obálky), 200 stromů; první 2 s běhu bez rozhodnutí; venku 5. 10. ve 100 m 35/35 oken |
| mlp_f2 | 7.656 | 11/14 | 18/19 | 4/22 | 2,0 | vrstva 2, MLP 68→32→16→1; předchozí výchozí, rollback; venku slábne od 60 m |
| mlp_m1 | 8.400 | 21/21 | 14/19 | 2/27 | 1,9 | vrstva 4, MLP 78→32→16→1; nejvyšší polní AUC, ale jeho práh se mezi tréninky hodně liší |
| mlp_v6 | 3.0 | 7/14 | 10/19 | 5/22 | 16,7 | jen veřejná data; práh 3.0 byl laděn na Runneru |
| svm_v3 | 0.5 | – | – | – | – | pálí na čtvrtinu negativních oken, nepoužívat |

Co z toho plyne pro test venku:

- **Dosah je teď hlavní limit, ne typ dronu.** DJI visící nad mikrofonem (mimo
  fold, výchozí práh, podíl oken nad prahem): `gbt_m1` 60/80/90 m 86/85/71 %
  (`mlp_m1` podobně), vrstva 2 (`mlp_f2`) jen 38/44/17 %. **Venku na desce**,
  dron visí: 60 m 100 % oken proti 58 % u `mlp_f2`, 80 m 65 % proti 21 %, 100 m
  `gbt_m1` 35 z 35 oken, `mlp_f2` 1 z 31; pozadí s lidmi bez falešného alarmu.
  Ve 120 m nechytil nikdo, nejspíš brána (squelch 3): už na 80–100 m prošlo jen
  20–43 oken ze 60. V RMS je DJI nad hlavou od 60 m nerozlišitelná od pozadí
  (0,004).
- **Proč vrstva 4:** k příznakům spektra přibylo modulační spektrum obálky
  pásma 1–4 kHz za poslední dvě sekundy. Visící dron „seká“ svůj šum frekvencí
  průchodu listů (Phantom 4 asi 170–185 Hz, s harmonickou na dvojnásobku) a
  tahle čára drží i tam, kde je spektrum už na úrovni pozadí. Je to podpis
  stálých otáček: při přeletu a klesání se čára rozmaže a detekce je o dost
  horší než při visení. Runner na 40 m nechytí ani vrstva 4.
- Druhý názor na stejném zvuku: `model mlp_m1` (stejné příznaky, jiný druh
  modelu) nebo `model mlp_f2` (bez modulace). `model mlp_f2` je i rollback.
- **Známá slabina trvá:** když se ze vstupu odřízne všechno nad 6 kHz, model
  nepozná nic. Na přehrávky z mobilu se nespoléhej.
- Typ dronu, který v trénovacích datech není, zatím spolehlivě nepozná: na
  cizích dronech (sada Halmstad) chytí modely jen menšinu klipů. Každý nový
  dron (FPV, jiná DJI) potřebuje vlastní nahrávky.

## 4b. Nové modely: sítě na gammatonovém spektrogramu a dvojice s gbt_m1

Sítě (CRNN: dvě konvoluce, LSTM a hlava) čtou spektrogram v 64 gammatonových
pásmech, buď okno 0,45 s (`crnn_h`), nebo sekundu, která oknem končí
(`crnn_hl`), a vedle něj modulační čísla vrstvy 8 (vrstva 4 a totéž přes 4 s).
Dvojice `or_*` pustí na stejné okno `gbt_m1` i síť a dron hlásí, když zabere
kterýkoli z nich. Pocházejí ze srovnání ze 7.–9. 10. (souhrn
`boomdetect-data/analysis/2026-10-09-souhrn/vysledky.md`).

Mimo fold, podíl oken označených „dron“ při výchozím prahu. Nové modely mají
výchozí práh na 1 falešném okně za hodinu veřejných val negativ, staré na 5,
proto je `gbt_m1` pro srovnání při svém bodu 1 FA/h (4.06). „V pohybu“ =
nahrávky s pohybem: Runner přílet a odlet ze 40 m, přiblížení, náklony; DJI
vzlet do 100 m, nahoru a dolů, let z boku. Falešné alarmy = polní negativa
s alarmem (z 27) a alarmy za hodinu na 30 min dopravy při 5 FA/h.

| model | výchozí práh | DJI 60–90 m | DJI blízko | Runner | Runner v pohybu | DJI v pohybu | falešné alarmy | první rozhodnutí |
|---|---|---|---|---|---|---|---|---|
| `gbt_m1` (srovnání) | 4.06 | 71 | 68 | 21 | 11 | 64 | 0 / 0 | 2,2 s |
| `crnn_h` | 7.196 | 82 | 89 | 81 | 88 | 87 | 1 (lidský ruch) / 4 | 0,5 s |
| `or_h` | z 3.831 | 79 | 86 | 77 | 81 | 80 | 0 / 0 | 0,5 s |
| `crnn_hl` | 8.774 | 88 | 94 | 77 | 90 | 90 | 1 (doprava) / 6 | 1,4 s |
| `or_hl` | z 3.760 | ≈ 89 | ≈ 94 | ≈ 77 | ≈ 88 | ≈ 91 | ≈ 1 / ≈ 6 | 1,4 s |
| `or_l` | z 3.760 | ≈ 78 | ≈ 91 | ≈ 74 | ≈ 75 | ≈ 76 | 0 / 0 | 1,4 s |

Čísla bez ≈ patří přesně modelům v image. U `or_hl` a `or_l` analýza párovala
síť se stromy z jiného tréninku než `gbt_m1`, čísla jsou proto jen přibližná.
`or_l` používá síť na samotném sekundovém spektrogramu (`crnn_l`), která sama
v seznamu není, protože na dálku je slabá.

Jak je použít venku:

- **`or_h`** je nejbezpečnější nový model: v analýze bez falešného alarmu,
  oproti `gbt_m1` výrazně lepší na Runnera a na dron v pohybu, rozhoduje už po
  0,5 s.
- **`crnn_hl` / `or_hl`** dosáhnou nejdál (DJI v simulované vzdálenosti
  ~106 m: `crnn_hl` 50 % oken, `gbt_m1` 21 %), ale v analýze alarmovaly na
  aku vrtačce, venkovním pozadí, lidech nebo dopravě. Venku proto u nich
  pohlídej pozadí bez dronu a při falešném alarmu zvedni práh (kap. 5).
- **`crnn_h`** je nejlepší na Runnera a na rychlé změny (pohyb, plyn).
- **`or_l`** je opatrná varianta: v analýze žádný falešný alarm, na Runnera
  a v pohybu slabší než ostatní nové.
- Práh dvojice není logit, ale **z**: −log10 podílu val negativních oken,
  která dopadla aspoň tak vysoko. 3.8 ≈ 1 falešné okno za hodinu, vyšší
  = přísnější. Nad zhruba 4.2–4.3 je okno silnější než cokoli ve val a z se
  jen protahuje. `dec` v řádku `DET` je u dvojice tohle z.
- Sítě na desce zatím neběžely venku ani se nezměřilo, kolik času zaberou;
  první běh pusť s `dbg` = 1 a podívej se na `DETDBG` (`loop_max` musí být pod
  21 333 µs a `skipped` 0).

## 5. Citlivost: jak si s prahem hrát

Vyšší práh = méně citlivé, méně falešných poplachů. Nižší práh = citlivější,
víc falešných. Pro výchozí `gbt_m1` (mimo fold, nahrávky do 2. 10.: 21 DJI,
19 Runner, 27 negativ; poslední sloupec na 11,6 h veřejných negativ):

| práh | DJI | Runner | terénní negativa | fal. alarmy/h |
|---|---|---|---|---|
| 4.06 (1 FA/h) | 20/21 | 12/19 | 0/27 | 0,26 |
| **3.211** (5 FA/h, výchozí) | 21/21 | 14/19 | 1/27 | 1,7 |
| 2.35 (20 FA/h) | 21/21 | 16/19 | 4/27 | 4,0 |

A po nahrávkách (DJI nad mikrofonem 2. 10., drony 30. 9., venkovní negativa
2. 10., kancelář 1. 10., 30 min dopravy), s pravidlem alarmu z kapitoly 6:

| práh + pravidlo | DJI 2. 10. 20–90 m (/7) | drony 30. 9. (/16) | venkovní negativa (/4) | kancelář (/11) | doprava (30 min) |
|---|---|---|---|---|---|
| 2.0 + 2of4 | 7 | 14 | 2 | 1 | 8 alarmů/h |
| 2.5 + 2of4 | 7 | 14 | 2 | 1 | 0 |
| **3.211 + 2of4** (výchozí) | **7** | **13** | 1 | 0 | 0 |
| **3.5 + mean4** | 7 | 13 | **0** | **0** | 0 |
| 4.0 + 2of4 | 7 | 12 | 0 | 0 | 0 |
| 5.0 + 2of4 | 7 | 7 | 0 | 0 | 0 |

Pod 2.5 začne pálit doprava. `3500 + mean4` má stejné detekce jako výchozí a
žádný falešný alarm na ničem, co jsme zatím nahráli.

Doporučený žebřík na venkovní test, každý krok 30 s se stejným manévrem dronu:

```
detect 30 3 3211              # výchozí
detect 30 3 3500 0 mean4      # nula falešných
detect 30 3 2500              # citlivější, pro měření dosahu
```

A ke každému kroku stejnou sekvenci bez dronu (mluvení, chůze, auto, vítr),
aby byl vidět odstup. Práh volíme jako nejvyšší hodnotu, při které dron stále
dává alespoň třetinu oken, a nejnižší, při které rušiče nedají žádný alarm.

### mlp_f2 (rollback)

Výchozí práh `mlp_f2` je 7.656 (5 FA/h). Pod 7 začne pálit doprava, od 7.656
výš je doprava i venkovní pozadí čisté a `9000 + mean4` nedal falešný alarm na
ničem, co jsme zatím nahráli. Žebřík pro `mlp_f2`:

```
model mlp_f2
detect 30 3 7656              # výchozí mlp_f2
detect 30 3 9000 0 mean4      # nula falešných
detect 30 3 5570              # jen pro měření dosahu, pálí doprava
```

### Nové modely (sítě a dvojice)

Prahy při 1 / 5 / 20 falešných oknech za hodinu na val, v tisícinách, jak se
zadávají do `detect` (u dvojic je to z × 1000):

| model | 1 FA/h (výchozí) | 5 FA/h | 20 FA/h |
|---|---|---|---|
| `crnn_h` | 7196 | 5519 | 1988 |
| `crnn_hl` | 8774 | 6962 | 3619 |
| `or_h` | 3831 | 3267 | 2696 |
| `or_hl`, `or_l` | 3760 | 3196 | 2646 |

```
model or_h
detect 30 3                   # výchozí, 1 FA/h
detect 30 3 3267              # 5 FA/h, citlivější
detect 30 3 4100              # přísnější, než val umí změřit
```

Pod 5 FA/h ještě nikdo nic venku nezkoušel; nejdřív ověř pozadí bez dronu.

## 6. Brány

### Squelch (RMS brána)

Druhý argument `detect`. Snímek s RMS pod squelchem se nezpracuje vůbec:
nevznikne okno, nehne se alarm. Výchozí je **3** (RMS 0.003): venku bylo pozadí
kolem RMS 0.004 a dron ve střední výšce nebo dál 0.004–0.009. Tichá místnost
(0.0026) ani při 3 okno nedá, okno potřebuje 14 snímků v řadě nad branou.

- **Nikdy netestuj verdikty se squelchem 0.** Příznaky jsou nezávislé na
  hlasitosti, takže i šum na úrovni floor má „tvar“ a model na něm pálí.
- Když bude dron sotva slyšet, uvidíš v `DETEND` málo oken, to znamená, že
  brána maže moc. Pod 3 ale nechoď, tam už začíná vlastní šum mikrofonu.
- Ve větru nebo u silnice: zkus **10–20**, aby se hluk pozadí nedostal ke
  klasifikátoru vůbec. Sleduj `LVL rms=`, brána má sedět těsně nad klidem.
- Celopásmové RMS je dané hlavně hukotem pod 200 Hz, dron má energii výš.

Kontrola nastavení: `windows` v `DETEND` proti délce běhu. 30 s dává maximálně
asi 66 oken. Když je `windows` výrazně méně, brána ořezává.

### Alarm: hlasování K z N, nebo průměr

Pátý argument `detect` (za `dbg`), při každém běhu znovu, nic se neukládá:

```
detect 30 3 3211 0 2of4     # výchozí: zapne při 2 DRONE ze 4 posledních oken, vypne pod 1
detect 30 3 3211 0 mean4    # průměr posledních 4 rozhodnutí vůči prahu >= 0
detect 30 3 3211 0 1of4     # volnější hlasování: stačí 1 okno ze 4
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

Co dělá `mean4` s `gbt_m1`, ukazuje tabulka v kapitole 5 (`3500 + mean4`).
Venku vyzkoušej obě pravidla na stejném manévru. Z řádků `DET` v logu spočítáš,
co by dalo jiné pravidlo, aniž bys běh opakoval, loguj proto celé běhy
(`stm32node-cli detect ... > beh.txt`).

## 7. Postup venku, ve zkratce

1. `micslot a`, `model`, ověř `gbt_m1 * thr=3211`.
2. Klid bez dronu: `detect 30` a sleduj `LVL rms=`, podle toho nastav squelch.
   Zároveň **nahraj pozadí bez dronu** (`record 1 120`), v tréninku chybí.
3. Rušiče bez dronu (řeč, auto, vítr) při výchozím prahu 3.211 a při 3.5
   (`mean4`). Musí být `alarms=0`.
4. Dron: hover blízko, hover daleko, přelet; k tomu známé vzdálenosti
   (20, 40, 60, 80, 100 m a dál), protože dosah je teď hlavní otázka. Žebřík
   prahů z kapitoly 5; od ~100 m zkus i squelch 1 (`detect 30 1`).
5. Stejné manévry s `model mlp_m1` (stejné příznaky, jiný model) a
   `model mlp_f2` (vrstva 2, bez modulace) pro srovnání.
6. Stejné manévry s novými modely (kap. 4b): `model or_h` (bezpečná volba),
   `model crnn_hl` (nejdál) a `model crnn_h` (pohyb, Runner), u každého i
   pozadí bez dronu; poprvé s `dbg` = 1 kvůli `DETDBG`.
7. Nahraj i syrový zvuk jako WAV, doma ho pak přehraje
   `bdtrain score nahravka.wav` přes všechny modely bez dalšího létání:

   ```
   stm32node-cli record 10 20 --port COM7
   ```

   To nahraje 20 souborů po 10 s (`chunk-001.wav` až `chunk-020.wav`, každý
   hned, jak doběhne) do `recordings/batch-<datum-čas>/` a k nim `index.csv`
   (stream a overrun každého kusu); Ctrl-C nechá hotové. `record 30` je jeden
   soubor; totéž jde i v TUI konzoli. Spouští se přes
   `.venv\Scripts\stm32node-cli.exe` ve `fw/apps/stm32node-cli`.

   Nahrávej souběžně s manévry dronu a piš si k číslům kusů, co se dělo
   (hover 2 m: 003–005, přelet: 006, ...). Doma pak práh doladíš přesně na
   to, co bylo slyšet, bez dalšího létání.

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
4. Pak `bdtrain manifest`, `bdtrain features field --force` a trénink s `--field`.

Co nejvíc chybí, v tomhle pořadí:

- **dron dál než 100 m a v pohybu** (přelet, klesání), s poznámkou, jak daleko
  byl;
- **dlouhá pozadí** (desítky minut: park, silnice, vítr, déšť), jinak nejde
  změřit falešné alarmy za hodinu;
- **skutečné rušiče, ne z telefonu** – fén a včely z mobilu jsou ořezané nad
  5 kHz a model se na tom učí zkratku „bez výšek = není dron“;
- **další typy dronů**, hlavně FPV, a kdyby šlo, i jiné DJI;
- delší souvislé záběry (1–3 min na manévr) místo 9–19 s.
