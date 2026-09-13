# ARS review současného rukopisu

Datum review: 2026-05-16  
Rukopis: `article/article_main.tex`  
Rozsah hodnocení: název, abstrakt, úvod, state of the art, metody a navazující náčrty experimental setup/results/discussion, protože už teď ovlivňují důvěryhodnost abstraktu a metod.  
Poznámka k ověření: Review vychází z lokálního rukopisu, `references.bib` a dostupných lokálních source souborů. Externí DOI/metadata jsem v tomto kole neověřoval webem; tam, kde jde o bibliografickou existenci nebo aktuálnost SOTA, berte komentáře jako metodické a argumentační, ne jako kompletní citation audit.

## Editorské rozhodnutí

**Rozhodnutí: Major Revision před odesláním.**

Rukopis má dobrý základ: jasný aplikační problém, smysluplný embedded-oriented pipeline, vlastní terénní data a velmi silný výsledkový kontrast mezi clean-only a augmented tréninkem. V aktuální podobě by ale reálný reviewer pravděpodobně napadl hlavně metodickou průkaznost. Největší problém není samotná myšlenka, ale to, že claim „robust field deployment“ stojí na malém datasetu, feature-space augmentaci a feature-space SNR testu, přičemž není dost přesně popsáno, jak bylo zabráněno leakage mezi eventy, augmentacemi a splitty. Další velké riziko je embedded claim: host počítá MFCC a embedded target dělá jen TFLite inference, takže nejde o end-to-end embedded acoustic detector.

POZNÁMKA AUTORA: Klidně bych ten claim trošku zmírnil. To že zařízení dělá jen TFLite inference a není to end-to-end není problém, protože jiné části jsme již vyzkoušli někde jindy a nebyl to problém jako například ten mediánový filtr a tak. Teď je to zamýšleno vyloženě na tu druhou fázi detekce, což je ten binární klasifikátor. Možná pokud by to bylo pro článek přínosné bychom to mohli tlačit více jako dvoufázová detekce, kde ta přvní je online streamovací a většinu věcí pustí a ta druhá je pomalejší přes neuronku, kde pak mámě nějakou časovou necitlivost než to proběhne.

Nejsilnější doporučení: **zúžit hlavní claim z „field robustness“ na „robustnost vůči řízeným perturbacím v MFCC prostoru“ a doplnit metodickou transparentnost tak, aby byly výsledky reprodukovatelné.** Pokud máte waveform-noise nebo cross-environment testy, přidejte je; pokud ne, explicitně je označte jako future work a nepoužívejte je jako důkaz field robustness.

## Analýza zaměření článku

| Dimenze | Hodnocení |
|---|---|
| Primární disciplína | Akustické snímání / machine learning pro detekci impulzních událostí |
| Sekundární disciplíny | TinyML / embedded inference, obranné senzorové systémy, audio signal processing |
| Výzkumné paradigma | Kvantitativní experimentální ML evaluace |
| Typ metodologie | Supervised binary classification, robustness ablation, embedded inference validation |
| Zralost draftu | Rozpracovaný první draft: text má strukturu a srozumitelnou linku, ale metody/výsledky potřebují výrazně víc detailu a důkazní disciplíny |
| Pravděpodobná úroveň venue | Specializovaná Q2/Q3 konference nebo journal bez další validace; se silnější validací potenciálně IEEE Sensors / Measurement / Applied Acoustics typ venue |

### Použitá konfigurace reviewerů

1. **EIC / editor-in-chief**: Editor aplikačního sensing / embedded AI venue. Fokus: journal fit, přínos, zda článek nepřesahuje důkazy.
2. **Metodologický reviewer**: Výzkumník v ML-for-audio experimentech. Fokus: split design, leakage, augmentace, metriky, nejistota, reprodukovatelnost.
3. **Oborový reviewer**: Výzkumník v akustické detekci weapon-event / impulzních událostí. Fokus: state of the art, akustika dělostřeleckých událostí, hranice trigger/classifier, terminologie.
4. **Cross-disciplinary / deployment reviewer**: Embedded systems reviewer. Fokus: realističnost end-to-end nasazení, latence/energie, limity host-device protokolu.
5. **Devil’s advocate**: Stresový test hlavního argumentu a nejsilnějších protiargumentů.

## Nezávislé reviewer reporty

### 1. EIC review

**Doporučení: Major Revision**  
**Jistota hodnocení: 4/5**

**Silné stránky**

- Název, abstrakt a úvod míří na koherentní aplikační přínos: akustickou detekci artillery launch s PC-to-TFLite deploymentem.
- Článek má čistou narativní linku: impulse segmentation -> MFCC -> compact CNN -> robustness test -> embedded inference.
- Abstrakt je konkrétní a uvádí numerické výsledky, takže čtenář rychle pochopí deklarovaný přínos.
- Sekce limitations už správně přiznává nejdůležitější hranici: Gaussian perturbation v MFCC prostoru není fyzický waveform-level noise model.

**Zásadní slabiny**

- **Přínos je zarámován příliš široce.** Název a abstrakt naznačují field robustness a embedded deployment, ale evidence aktuálně podporuje užší tvrzení: robustnost při řízené perturbaci v MFCC prostoru a embedded inference na předpočítaných features. Viz `article_main.tex` řádky 48, 58, 118, 128, 169.
- **Článek nemá jasnou výzkumnou otázku nebo hypotézu.** Úvod říká, že hlavní přínos je „low-SNR robustness evaluation“, ale neformuluje testovatelné claimy ani kritéria úspěchu. Viz řádky 55-58.
- **Výsledky jsou příliš stručné vzhledem k síle abstraktu.** Abstrakt uvádí přesné accuracy, ale results section zatím obsahuje jen jednu accuracy tabulku a kvalitativní tvrzení o PC vs embedded, ablation, class weighting a quantization. Viz řádky 139-166.
- **Journal fit závisí na chybějících deployment metrikách.** Embedded venue bude čekat latenci, paměť, velikost modelu, typ quantization, cílový hardware a často i energy/power. Aktuální protokol uvádí jen host-device feature transfer. Viz řádek 128.

POZNÁMKA AUTORA: Pojdmě vymyslet lepší a menší scope, kde jasně vymezíme to co chceme tvrdit. Upravíme název, abstrak a poté metody. Zbytek článku ještě není ready. Musíme vymyslet jasnou výzkumnou otázku a hypotézu. Výsledky v článku zatím žádné nejsou, to je správně, zatím jsem pracoval jen na abstraktu, state-of-the-art a metodách. Do paměti mi ulož jako TODO, že musím zjistit latenci, pamět, velikost modelu, typ kvantizace pro článek. Možná bych i jako jednu z dalších zajímavých výzkumných věcí tam přidal trénování na malém datasetu, z nevýhody udělal tu výhodu a na tom se obracel, například to váhování datasetu, ale ško tam udělat taky více s tím, tak to zmínit jen okrajově a nedělt z toho rozhodně hlavní myšlenku.

**Otázky editora pro autory**

1. Co přesně je hlavní claim: robustnost launch-event classifieru, robustnost celého acoustic detectoru, nebo jen robustnost MFCC-feature classifieru?
2. Které části pipeline skutečně běží na embedded hardware?
3. Jaké je zamýšlené venue a page limit? Podle toho se musí rozhodnout, jak agresivně rozšířit methods/results.

POZNÁMKA AUTORA: chceme robusnost celého acoustic detektoru, na embedded teď běželo jen ta neuronovka, ale zároveň máme i kódy, kde nám funguje mediánový filtr, který při akustické události záznam pošle dále na zpracování. Pak by tam byla ta neuronka co již funguje. Funguje nám na našem HW i akvizice dat. Možná bych tam dal zmíňku, že to běželo na ESP32-S3, který už máme otestovaný s tou akvizicí a mediánovým filtrem a teď jsme právě přidali ještě ten klasifikátor pro arilerii, který slouží k odfiltrování jen střel artillerie. Celkem bych se chtěl dostat do 5-6 stran, ale přesné restriction jsou uloženy v paměti.

### 2. Metodologické review

**Doporučení: Major Revision**  
**Jistota hodnocení: 5/5**

**Silné stránky**

- Experimentální setup je na vysoké úrovni pochopitelný a používá vhodné srovnání baseline-vs-augmented.
- Class imbalance je rozpoznán a jsou pojmenované alespoň tři mitigační mechanismy: augmentation, rozšíření non-artillery class pool a class weighting.
- Discussion správně omezuje MFCC-space perturbation jako proxy, ne jako plnohodnotný acoustic noise model.

**Zásadní slabiny**

- **Možný data leakage je centrální riziko.** Rukopis říká, že trénink začíná ze 152 eventů a expanduje na 760 MFCC samples pomocí perturbace; později říká, že používá stratified 80/20 splitting. Není jasné, zda splitting probíhá před augmentací a zda všechny varianty stejného původního eventu zůstávají ve stejné partition. Pokud se augmented siblings objeví současně v train i test, robustness result není validní. Viz řádky 48, 87, 118, 131.

POZNÁMKA AUTORA: Pôvodných 152 čistých vzoriek bolo najskôr rozdelených na trénovaciu a testovaciu množinu (80% trénovacie dáta a 20% testovacie). Následne trénovacia množina bola augmentovaná tak, že boli algoritmicky vygenerované štyri nové, zašumené verzie s rôznou úrovňou intenzity šumu (reprezentované koeficientmi zosilnenia 0.1, 0.2, 0.3 a 0.5 voči smerodajnej odchýlke signálu). Týmto krokom sa zväčšil počet dát – z pôvodných 152 na celkových 608.

- **Denominátory accuracy jsou nejasné a možná nekonzistentní.** Při 152 eventech a 80/20 splitu má test set přibližně 30-31 eventů. Accuracy jako 96.72 %, 94.74 % a 59.02 % naznačují jiný denominátor nebo opakované/augmentované test samples. Článek musí uvést přesný počet evaluačních samples pro každý SNR a každý model. Viz řádky 48, 131, 141-155.

POZNÁMKA AUTORA: Dal bych tam tenhle experiment: "Pre účely testovania klasifikátora na PC aj na zariadení ESP32-S3 boli na základe odvodenej rovnice algoritmicky vygenerované štyri zašumené varianty pre každú zo 706 pôvodných audionahrávok. Testované boli nasledujúce úrovne degradácie signálu: 
SNR 30 dB (k ≈ 0, 0316): Predstavuje takmer čistý signál, kde je šum akusticky len veľmi slabo badateľný.
SNR 20 dB (k = 0, 1000): Predstavuje mierny hluk prostredia, amplitúda šumu dosahuje 10% smerodajnej odchýlky signálu.
SNR 10 dB (k ≈ 0, 3162): Simuluje výrazný hluk bojiska alebo silný vietor pri mikrofóne.
SNR 5 dB (k ≈ 0, 5623): Predstavuje extrémne nepriaznivé podmienky (limitný stav), kde energia šumu začína maskovať užitočné spektrálne zložky úsťovej vlny výstrelu.
Každý takto synteticky znehodnotený zvukový súbor bol následne prehnaný kompletným extrakčným reťazcom, čím sa zabezpečilo, že vplyv šumu prirodzene prenikol cez nelineárne operácie (najmä logaritmickú transformáciu) až do finálnej 58 × 12 matice kepstrálnych koeficientov."

Výsledky PC: 
SNR 30 dB 84,41 %
SNR 20 dB 82,86 %
SNR 10 dB 81,12 %
SNR 05 dB 80,44 %

Výsledky embedded ESP32-S3: 
SNR 30 dB 99,86 %
SNR 20 dB 99,73 %
SNR 10 dB 97,78 %
SNR 05 dB 97,18 %

KONEC POZNAMEK AUTORA

- **Feature-space SNR není definované.** Článek používá „SNR“ pro Gaussian perturbations v MFCC prostoru, ale nedefinuje signal power, noise power, scaling rule, pořadí normalizace ani to, zda je SNR počítané per-sample, per-coefficient, nebo globálně. Viz řádky 117-123 a 131.

POZNÁMKA AUTORA: M F C C_{\text {noisy }}(t, f)=M F C C_{\text {clean }}(t, f)+\mathcal{N}\left(0, \sigma^2\right)

- **Preprocessing je nedostatečně specifikovaný.** Chybí sampling rate, bit depth, microphones/sensors, channel selection, filtering, peak detector, řešení peak ties, segment padding, MFCC parameters, normalization, train-only fitting of normalization statistics, random seed, optimizer, learning rate, epochs, batch size, early stopping, threshold a class-weight formula.

POZNÁMKA AUTORA: Vzorkovací frekvence byla 22.05 kHz, 16 bit. Z jiného článku mám toto: "The recording system utilized PreSonus PRM1 electret microphones, with good sensitivity and flat frequency response. The microphones have been placed on a rotating platform to ensure the precise angle configuration. These microphones were connected to a Roland Rubix 44 USB audio interface, which features a 24-bit Analog-to-Digital Converter (ADC) and a sampling frequency of fs = 48 kHz.
For sound reproduction, a GENELEC 8030C studio monitor was employed. This professional speaker delivers a Sound Pressure Level (SPL) of up to 104 dB and covers a frequency range of 47 Hz to 25 kHz, ensuring good reproduction of gunshot signals across the required frequency spectrum." To jestli se použije v textu, tak to musíš přpsat. Zároveň tohle byla ta nahrávací frekvence, ale ono se to pak převzorkovalo na těch 22.05 kHz a 16 bit.

- **Evaluace příliš spoléhá na accuracy.** Při 62 positive a 90 negative eventech accuracy nestačí. Reportujte confusion matrices, precision, recall/sensitivity, specificity, F1, balanced accuracy, ROC-AUC nebo PR-AUC, false alarm rate, miss rate a confidence intervals.

POZNÁMKA AUTORA: Jo, tady bych to té zdrojové diplomky klidně převzal i ty další míry, už je tam vypočteno.

Pro PC: 
\begin{array}{|c|c|c|c|c|c|}
\hline \text { Úroveň šumu (SNR) } & \text { Accuracy } & \text { Precision } & \text { Recall } & \text { F1-score } & \text { MCC } \\
\hline \hline 30 \mathrm{~dB} & 84.41 \% & 0.35 & 1.00 & 0.52 & 0.5377 \\
\hline 20 \mathrm{~dB} & 82.86 \% & 0.33 & 1.00 & 0.49 & 0.5367 \\
\hline 10 \mathrm{~dB} & 81.12 \% & 0.31 & 0.99 & 0.47 & 0.4893 \\
\hline 5 \mathrm{~dB} & 80.44 \% & 0.29 & 0.93 & 0.45 & 0.4581 \\
\hline
\end{array}


Pro ESP32-S3:
\begin{array}{|c|c|c|c|c|c|}
\hline \text { Úroveň šumu (SNR) } & \text { Accuracy } & \text { Precision } & \text { Recall } & \text { F1-score } & \text { MCC } \\
\hline \hline 30 \mathrm{~dB} & 99,86 \% & 1,00 & 0,98 & 0,99 & 0,9912 \\
\hline 20 \mathrm{~dB} & 99.73 \% & 0.98 & 0.98 & 0.98 & 0.9824 \\
\hline 10 \mathrm{~dB} & 97.78 \% & 0.86 & 0.87 & 0.87 & 0.8554 \\
\hline 5 \mathrm{~dB} & 97.18 \% & 0.95 & 0.70 & 0.81 & 0.8009 \\
\hline
\end{array}

KONEC POZNÁMKA AUTORA


- **Chybí odhad variability.** Jeden stratified split na 152 eventech je křehký. Použijte repeated stratified splits, grouped cross-validation nebo bootstrap confidence intervals. Pokud má dataset strukturu recording sessions/scenarios, splitujte podle session/scenario, ne jen podle eventu.

POZNÁMKA AUTORA: Tohle bych rád pro brainstormoval.

- **Ablation claims nejsou podložené.** Řádky 162-166 tvrdí efekty augmentation, class weighting a quantization, ale zatím k nim nejsou žádné numerické tabulky.

POZNÁMKA AUTORA: Taky probrainstromovat.

**Minimální metodická doplnění**

- Nejdřív event-level split, až potom augmentace.
- Přesné train/validation/test počty podle class a podle augmented/non-augmented status.
- Tabulka s evaluačními denominátory: model, SNR, počet samples/eventů, TP, FP, TN, FN, accuracy, balanced accuracy, recall, precision, F1.
- Repeat-split nebo confidence interval reporting.
- Explicitní odstavec o prevenci leakage.
- Jasná definice MFCC-space SNR a upozornění, že nejde o acoustic SNR.

### 3. Oborové review

**Doporučení: Major Revision**  
**Jistota hodnocení: 4/5**

**Silné stránky**

- SOTA je logicky seřazené: classical detection, acoustic signature, post-trigger classification, robustness/domain shift, embedded constraints.
- Rozlišení mezi trigger stage a post-trigger classifierem je užitečné a sedí k paper pipeline.
- Draft cituje relevantní starší práce o artillery/acoustic event a novější práce o gunshot/audio classification.

**Zásadní slabiny**

- **Gap statement existuje, ale je pořád obecný.** Poslední SOTA věta říká, že otevřený problém je stability across the operational chain, ale článek pak vyhodnocuje jen část tohoto chainu. Gap by měl být přeformulován kolem toho, co skutečně testujete: robustnost post-trigger MFCC classifieru a TFLite inference drift.

POZNÁMKA AUTORA: Jo to bych přepsal, ale zároveň se výše ještě řešila otázka vědecké hypotézy a směřování článku, tak se to podle toho taky ještě může změnit. 

- **Terminologie artillery launch vs. gunshot potřebuje zpřesnit.** Článek střídá artillery launch, artillery gunshot, weapon identification, gun audio, launch/impact events. Definujte positive class přesně a používejte jeden termín konzistentně.

POZNÁMKA AUTORA: S tímto plně souhlasím a měli bychom to zpřesnit na artillery launch a artillery gunshot, zkrátka výstřel artilérie

- **Triggering je prezentován jako centrální, ale není vyhodnocen.** Řádky 62 a 87 dělají z trigger quality důležitý faktor, ale není uveden trigger-detection performance, false trigger rate ani continuous-stream evaluation. Pokud se peak detekuje uvnitř už segmentovaných labeled events, řekněte to explicitně.

POZNÁMKA AUTORA: tady si nejsem jistý jak je to myšleno a jak je to v článku. Doplnil bych tam tu detekci pomocí mediánového filtru, který je online a prostě dokud netrigne tak jede stream dat, který se v round bufferu zahazuje. Pokud triggne, tak se dojede ještě čas, aby v tom bufferu bylo 60 ms dat s tím, že 40 % je před peakem a 60 % po peaku.

- **Zdůvodnění MFCC by mohlo být silnější.** Davis 1980 je v bibliografii, ale není citován v textu. Rukopis by měl vysvětlit, proč je MFCC vhodnější než log-mel spectrograms, CQT, wavelet features nebo raw waveform models pro small-data embedded setting.

POZNÁMKA AUTORA: TDOD jako rozšíření literatury do paměti

- **Současné SOTA by nemělo naznačovat, že citované studie dokazují stejný operating regime.** Lokální reference mohou být relevantní, ale text musí odlišit artillery localization, gunshot forensics, embedded urban sound recognition a acoustic emission TinyML od přesné úlohy v tomto článku.

POZNÁMKA AUTORA: Pojďme na to udělat brainstorm

**Navržená restrukturalizace SOTA**

1. Předchozí artillery/mortar acoustic sensing: launch/impact classification a localization.
2. Impulsive audio classification: cepstral/wavelet/time-frequency features a CNNs.
3. Problém robustnosti: device/environment/SNR/domain shift; rozdíl mezi waveform noise a feature perturbation.
4. Embedded constraint: co už bylo a nebylo ukázáno pro full audio front ends na low-power devices.
5. Váš gap: small-data post-trigger launch-event classifier s TFLite inference consistency pod řízenými MFCC perturbacemi.

### 4. Cross-disciplinary / embedded deployment review

**Doporučení: Major Revision**  
**Jistota hodnocení: 4/5**

**Silné stránky**

- Článek správně uznává, že samotná model accuracy nestačí pro edge použití.
- Host-device validation je rozumný mezikrok v engineering testu.
- Zvolená CNN je dost jednoduchá na to, aby realisticky zapadla do embedded inference constraints.

**Zásadní slabiny**

- **Embedded claim je aktuálně příliš široký.** Protože host počítá MFCC features a posílá tensors, embedded target neběží front end. Článek by měl říkat „embedded TFLite classifier inference on host-computed MFCC tensors“, pokud nebude později portována celá pipeline. Viz řádek 128.

POZNÁMKA AUTORA: tady bych si byl jistý, že to esp32-s3 to utáhne celé, otázka je jak jiné embedded zařízení, asi by stálo za to v článku mít paměťové nároky řešení

- **Chybí specifikace targetu.** Chybí target board/MCU/SBC, CPU architecture, RAM, accelerator, TFLite runtime version, quantization scheme, input/output tensor dtype, model size, RAM use a latency.

POZNÁMKA AUTORA: ESP32-S3, kontrétně ESP32-S3 N16R8 Využívá modul ESP32-S3-WROOM s dvoujádrovým procesorem Xtensa 32bit LX7 s frekvencí až 240 MHz. Obsahuje 512 KB statické RAM (TCM), paměť Flash 16 MB a PSRAM 8 MB. Díky čipům CH343 a CH334 umožňuje vývoj přes USB a UART rozhraní skrze USB-C konektor. Pinově je kompatibilní s deskou SP32-S3-DevKitC-1, což usnadňuje rozšíření o další periferie. Podporuje prostředí ESP-IDF, Arduino, MicroPython a další. Začít s vývojem je snadné a rychlé. Společně s esp-nn. Kvantizácia modelu na 8-bitové celé čísla smeruje k univerzálnosti riešenia.

Štandardným výstupom trénovania v prostredí Keras/TensorFlow je model
uložený vo formáte HDF5 (.h5). Tento formát však vyžaduje plnohodnotný
súborový systém a operačnú pamäť na úrovni osobných počítačov, čo ho robí
pre mikrokontroléry nepoužiteľným. Z tohto dôvodu bolo nevyhnutné model
pred nasadením transformovať.
Pre tento účel bol využitý nástroj TensorFlow Lite Converter. Model bol
najprv skonvertovaný do optimalizovaného formátu .tflite, pričom bola
aplikovaná celočíselná kvantizácia po trénovaní (Post-Training Integer Quantization).
Tento proces transformoval váhy a aktivácie modelu z 32-bitovej
plávajúcej čiarky (float32) na 8-bitové celé čísla (int8)[40]. Tým sa dosiahla
štvornásobná redukcia pamäťovej stopy modelu, čo je kľúčové pre
zariadenia s obmedzenou pamäťou Flash.
Riešením bolo transformovať binárny .tflite súbor do podoby hlavičkového
súboru v jazyku C/C++ (tzv. C-array). Pomocou na mieru vytvoreného
skriptu v jazyku Python bol model prevedený do obrovského poľa hexadecimálnych
hodnôt (súbor model_data.h). Takýto prístup umožňuje kompilátoru
vložiť celú neurónovú sieť priamo do Flash pamäte mikrokontroléra určenú
len na čítanie (Read-Only Memory), čím sa šetrí dynamická pamäť (RAM).

KONEC POZNÁMKA AUTORA

- **Chybí real-time criterion.** Článek nedefinuje přijatelnou latenci vzhledem k 60 ms windows, event rate nebo warning use case.

POZNÁMKA AUTORA: Priemerný čas inferencie jedného MFCC segmentu na zariadení ESP32-S3 dosahoval stabilnú hodnotu približne 32 ms. Vzhledem k tomu, že je to až za tím mediánovým filtrem, tak je to v pohodě délka, ale rozhodně to chce přidat do článku.

- **Chybí communication overhead.** Host-device transfer je součástí validačního protokolu, ale není součástí deklarované operational pipeline. Buď ho vylučte z deployment performance claims, nebo ho měřte odděleně.

POZNÁMKA AUTORA: ten host-device transfer byl jen pro testování, v normálním provozu té jednotky není. Ale nebyla šance jednotku na střelbě artillerie znovu otestovat.

- **Energy/power je zmíněné v SOTA, ale není měřené.** Pokud to neměříte, odstraňte energy claims nebo je nechte jen jako motivaci/future work.

POZNÁMKA AUTORA: Nijak to nebylo měřeno, odstranit.

### 5. Devil’s advocate review

**Nejsilnější protiargument**

Skeptický reviewer může tvrdit, že článek zatím nedemonstruje robustní detekci artillery launch ve field conditions. Demonstruje, že CNN trénovaná s additive Gaussian perturbations v MFCC prostoru dobře funguje při testu s podobným MFCC-space perturbation procesem. To je užitečný regularization result, ale nemusí se přenést na skutečnou terénní degradaci, kde waveform noise, reverberation, propagation, microphone clipping, wind, sensor mounting a unseen acoustic backgrounds ovlivňují signál ještě před MFCC extraction. Malý dataset problém zesiluje: pokud augmented variants nebo correlated events uniknou mezi train a test partitions, reportovaná low-SNR accuracy může z velké části odrážet memorization event-specific signatures. Embedded výsledek je také užší, než článek naznačuje, protože embedded device dostává jen předpočítané features. Proto by core claim měl být zúžen, pokud nebudou přidány waveform-domain, session-level a full-pipeline embedded testy.

**Závažné problémy**

| # | Dimenze | Problém | Umístění |
|---|---|---|---|
| DA1 | Evidence gap | Field robustness claim stojí jen na MFCC-space perturbation | řádky 48, 78, 118, 169 |
| DA2 | Leakage risk | Není uvedeno pořadí augmentation/split | řádky 48, 118, 131 |
| DA3 | Nesoulad data-conclusion | Formulace „embedded detector“ přeceňuje host-computed MFCC protocol | řádek 128 |
| DA4 | Overgeneralization | Clean accuracy a feature-space low-SNR accuracy jsou zobecněné směrem k field-relevant regimes | řádky 48, 141, 169 |
| DA5 | Reproducibility | Chybějící preprocessing/training/evaluation parameters brání nezávislé replikaci | řádky 85-136 |

**Menší problémy**

- Citace „TensorFlow Lite deployment practice“ je webová dokumentace s rokem 2026; měla by být formátována jako accessed documentation, ne jako research source.
- `goodfellow2016dl`, `hershey2017cnn_audio` a `davis1980mfcc` jsou v bibliografii, ale rukopis je aktuálně nepoužívá.
- Results subsections pojmenované „Block A/B/C/D“ působí jako interní poznámky; přejmenujte je na paper-style headings.

## Komentáře po sekcích

### Abstrakt

Abstrakt je silný, ale aktuálně příliš asertivní. Měl by zachovat numerické výsledky, ale přidat dvě omezení:

- SNR degradation je **feature-space** degradation, ne acoustic waveform SNR.
- Embedded inference je **TFLite classifier inference on precomputed MFCC tensors**, pokud celý feature extraction chain neběží na zařízení.

Frázi „field-relevant regimes“ bych změkčil, pokud nepřidáte waveform-level nebo cross-session field testy. Bezpečnější závěr by byl ve smyslu: „clean-condition accuracy alone does not predict robustness to feature-space perturbations that may occur in deployment-oriented acoustic pipelines.“

### Úvod

Úvod je stručný, ale potřebuje:

- jednu explicitní výzkumnou otázku nebo objective;
- contribution list s přesným vymezením, co je nové;
- jasnou hranici: jde o post-trigger classifier, ne o plný continuous acoustic surveillance system.

Navržené rámování přínosů:

1. Event-centered MFCC/CNN pipeline pro binary launch-event classification z malého field-recorded corpus.
2. Leakage-controlled comparison clean-only a MFCC-perturbed training.
3. Robustness evaluation při definovaných MFCC-space SNR levels.
4. PC vs TFLite classifier inference comparison na precomputed features.

### State of the Art

Struktura je dobrá, ale gap má být ostřejší a méně široký. Teď SOTA směřuje ke „stability across the operational chain“, zatímco metody testují jen subset. Upravte finální odstavec tak, aby evaluated gap odpovídal metodě.

Doplňte nebo zpřesněte:

- rozdíl mezi launch, impact, muzzle blast, projectile shock a generic gunshot;
- proč je 60 ms vhodné post-trigger window;
- proč jsou MFCC stále obhajitelné pro tuto úlohu;
- rozdíl mezi physical acoustic noise a feature-space perturbation;
- embedded audio front-end cost vs classifier-only cost.

### Metody

Tohle je sekce, která potřebuje nejvíc práce. Doplňte dost detailů, aby čtenář mohl experiment replikovat bez dotazů na kód.

Potřebné detaily:

- recording conditions: sensor/microphone, sample rate, bit depth, distance/range, pokud je publikovatelné, number of sessions, environment, labels, negative-class sources;
- event preprocessing: peak detector, 60 ms window in samples, boundary handling, filtering;
- MFCC extraction: frame length, hop, FFT size, mel bands, number of coefficients, deltas/no deltas, log energy, normalization;
- dataset splitting: event-level split, class counts per split, random seed, whether validation and test are separate;
- augmentation: jak vzniká 760 samples, kolik variants per original, train-only nebo i test perturbation, sigma/SNR formula;
- CNN training: optimizer, loss, LR, epochs, batch size, early stopping, dropout, class weights, threshold;
- TFLite: conversion parameters, quantization, target, runtime, dtype, model size, latency.

## Roadmapa revizí

| Priorita | Úkol | Proč na tom záleží | Odhad práce |
|---|---|---|---|
| P1 | Prokázat, že nevzniká leakage: split původních eventů před augmentací a reportovat počty | Bez toho může reviewer shodit hlavní výsledek | 0.5-1 den, pokud to kód už dělá |
| P1 | Matematicky definovat MFCC-space SNR a podle toho přejmenovat claimy | Zabrání útoku na „SNR“ a field robustness | 1-2 hodiny |
| P1 | Nahradit/změkčit field robustness claims v title/abstract/discussion, pokud neexistují waveform testy | Srovná závěry s důkazy | 1-2 hodiny |
| P1 | Doplnit plné methods detaily pro MFCC, split, augmentation, CNN, class weights, TFLite | Reproducibility gate | 0.5-1 den |
| P1 | Přidat confusion matrices a class-wise metrics pro každé SNR | Accuracy samotná je slabá při imbalance | 0.5 dne |
| P2 | Přidat repeated splits nebo bootstrap confidence intervals | Stabilizuje small-data claims | 0.5-1 den |
| P2 | Přidat waveform-domain noise test nebo ho jasně odložit | Z limitation udělá kontrolovanou hranici | 1-2 dny, pokud jsou data/kód dostupné |
| P2 | Přidat embedded target table: hardware, model size, RAM, latency, quantization | Udělá embedded contribution důvěryhodným | 0.5 dne |
| P2 | Přepsat SOTA gap tak, aby odpovídal skutečně evaluovanému přínosu | Zpřesní narativ | 2-4 hodiny |
| P3 | Vyčistit bibliografii: citovat Davis/Hershey/Goodfellow nebo odstranit; upravit TFLite docs | Sníží compile/reviewer noise | 1 hodina |
| P3 | Přejmenovat „Block A/B/C/D“ results headings | Draft bude působit submission-ready | 15 min |

## Doporučený směr pro abstrakt

Nepřepisujte ještě celý abstrakt; nejdřív rozhodněte, zda máte waveform-domain a skutečné embedded front-end results. Pokud ne, abstrakt by měl říkat:

- „controlled MFCC-space perturbation“ místo širokého „low-SNR field regime“;
- „TFLite inference on precomputed MFCC features“ místo širokého embedded deployment;
- „small field-recorded dataset“ a „event-level split“, pokud je to pravda;
- přidat class-wise metrics nebo aspoň balanced accuracy, pokud hraje roli imbalance.

## Závěr review

Článek je slibný a po metodicky silné revizi publikovatelný, ale současný draft je zranitelný vůči jedné rozhodující reviewer námitce: **evidence zatím neospravedlňuje šířku robustness a embedded-deployment claims.** Nejrychlejší cesta vpřed není přidávat víc obecného textu; je potřeba zpřesnit claim, rozšířit metody a udělat evaluační tabulky obhajitelné na event-level úrovni.
