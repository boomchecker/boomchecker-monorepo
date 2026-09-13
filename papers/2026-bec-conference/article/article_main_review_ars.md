# ARS Review: `article/article_main.tex`

- Datum review: 2026-05-17
- Workflow: `academic-research-suite` -> `academic-paper-reviewer` -> `full`
- Reviewed file: `article/article_main.tex`
- Celkovy verdikt: `Major Revision`
- Confidence: `4/5`

## Reviewer Configuration Card

- EIC: editor zamereny na embedded sensing / applied acoustic ML pro obranne a bezpecnostni aplikace
- R1 Methodology: reviewer pro experimentalni ML, robustnostni evaluaci a TinyML inference
- R2 Domain: reviewer pro akustickou detekci impulznich udalosti a gunshot/artillery audio classification
- R3 Perspective: reviewer z pohledu deploymentu, operational validation a field reliability
- Devil's Advocate: adversarialni reviewer zamereny na logiku argumentu, reprodukovatelnost a nekonzistence mezi protokolem a vysledky

## Executive Summary

Rukopis je citelny, dobre ohranicuje scope a rozumne priznava cast limitaci. Silnou strankou je disciplinovane vymezeni, ze experiment se tyka pouze post-trigger klasifikatoru, nikoli celeho kontinualniho detekcniho retezce. Text take pouziva vhodnejsi metriky nez samotnou accuracy, zejmena MCC.

Hlavni problem neni stylisticky, ale validacni. Nejzasadnejsi tvrzeni clanku je, ze kvantizovany model si zachovava nebo dokonce zlepsuje robustnost a ze ESP32-S3 deployment nevykazuje skodlivy drift vuci PC-side int8 referenci. V aktualni podobe tomu experimentalni protokol ani tabulky nedavaji dostatecne verohodnou oporu. Rozdil mezi `PC int8` a `ESP32-S3` ve stejne tabulce je prilis velky na to, aby slo o bezne nasazovaci odchylky stejneho kvantizovaneho modelu pri stejnych hostem generovanych MFCC vstupech. Soucasne clanek nepopisuje split, opakovani, thresholding ani uncertainty tak, aby bylo mozne vysledky reprodukovat a posoudit, zda nejde o artefakt protokolu, variance maleho test setu nebo reporting chybu.

Proto doporucuji `Major Revision`, nikoli kvuli absenci potencialne zajimaveho vysledku, ale protoze klicova empiricka opora pro hlavni claim clanku zatim neni dostatecne uzavrena.

## Key Findings

### CRITICAL

1. **Nekonzistence mezi PC int8 a ESP32-S3 vysledky pro tentyz kvantizovany model**
   - V textu se pise, ze host pocita MFCC, aplikuje stejne perturbace a posila stejne tensory na embedded target, ktery bezi se "the same quantized classifier" (`r. 132-133`).
   - Presto tabulka ukazuje dramaticky lepsi vysledky na ESP32-S3 nez na `PC int8`, napr. pri `5 dB` precision `0.53 -> 0.95` a MCC `0.6511 -> 0.8009` (`r. 171-188`).
   - Pokud je model skutecne stejny a vstupy jsou stejne, takovy rozdil je bez dalsiho vysvetleni nepravdepodobny. To spise ukazuje na neekvivalentni inference path, jiny threshold/post-processing, jinou kvantizaci vstupu, chybu v reportingu nebo chybu evaluace.
   - Tento bod oslabuje hlavni claim clanku.

2. **Tabulkove metriky nejsou zjevne slucitelne s deklarovanym jednim 80/20 split protokolem**
   - Clanek deklaruje `706` eventu a jeden stratifikovany `80/20` hold-out split (`r. 135-140`), tedy test set zhruba `141-142` eventu.
   - Presnosti typu `99.86 %` a `99.73 %` (`r. 185-186`) nejsou pro tak maly jednorazovy test set prirozene, pokud neni efektivni pocet hodnocenych pripadu vyrazne vetsi.
   - Stejne tak recall hodnoty jako `0.98`, `0.87` nebo precision `0.95` pri velmi malem poctu pozitiv implikuji bud agregaci pres opakovane perturbace, nebo jiny evaluacni rezim, ktery ale neni popsany.
   - Pokud jsou vysledky prumery pres vice stochastickych behu nebo vice korupcnich realizaci na event, musi to byt explicitne popsano.

### MAJOR

3. **Chybi kontrola na session-level leakage nebo correlated-event leakage**
   - Split je pouze `event-level` (`r. 136`), ale clanek nerika, zda train/test neobsahuji udalosti ze stejneho dne mereni, stejne lokality, stejne zbrane nebo stejne akusticke sceny (`r. 102-105`, `r. 136`).
   - U field acoustic dat je to zasadni. Event-level split muze byt prilis optimisticky, pokud sdileny recording context pronika do obou casti.

4. **Chybi uncertainty estimation pro velmi maly pozitivni class count**
   - Cely korpus ma jen `62` pozitiv (`r. 47-48`, `r. 105`, `r. 136`).
   - Jeden hold-out split bez confidence intervals, repeated splits nebo grouped cross-validation nestaci pro silnejsi zaver o robustnosti a deployment stability.
   - U tak maleho poctu pozitiv muze jeden az dva eventy vyrazne zmenit precision, recall i MCC.

5. **Kauzalni interpretace "quantization helps robustness" neni uzavrena**
   - Text spravne rika, ze mechanismus neni izolovan (`r. 48`, `r. 166`, `r. 197`), ale zaroven celkova argumentace stale vede ctenare k tomu, ze kvantizace je pravdepodobne vysvetleni robustness gain.
   - Bez ablace a bez plneho srovnani preprocessing/inference path jde jen o observacni korelaci.

6. **Chybi baseline a ablace**
   - Neni srovnani proti jednodussim baseline modelum ani proti alternativnimu featuringu ci treninkovym variantam.
   - Neni oddelen efekt expanded negatives, class weighting a augmentation (`r. 127-130`).
   - Bez ablace je tezke rict, ktera cast navrhu skutecne nese vykon.

7. **Thresholding a calibration nejsou popsany**
   - U binarniho sigmoid classifieru je threshold klicovy, zvlast kdyz float32 model vykazuje vysoky recall a slabou precision (`r. 145-159`).
   - Clanek nerika, zda byl threshold fixni `0.5`, kalibrovany na validacni sade, nebo odlisny mezi float32/int8/ESP32.
   - Bez teto informace nelze interpretovat presun mezi precision a recall.

8. **Claim o deployability je jen castecne end-to-end**
   - Clanek spravne priznava, ze host pocita MFCC a posila je do zarizeni (`r. 133`), ale v title, abstractu a zaveru je deployment framing silnejsi nez skutecny evaluacni scope (`r. 17`, `r. 47-48`, `r. 199-200`).
   - Pokud front-end feature extraction nebezi na targetu, inference latency `32 ms` nepopisuje plne on-device zpracovani.

### MINOR

9. **Title lehce overpromisuje oproti vyhodnocenemu scope**
   - Nazev mluvi o "Two-Stage Acoustic Pipeline" (`r. 17`), ale vlastni experiment hodnoti jen druhou fazi (`r. 58`, `r. 91`, `r. 197`).
   - Doporucuji title zpresnit, nebo jeste explicitneji uvest "post-trigger classification stage".

10. **Dataset popis je pro reprodukovatelnost prilis strucny**
   - Chybi rozpad non-launch negatives, pocet recording sessions, variabilita zbrani/scen, distance bands, environment conditions a label protocol (`r. 102-105`).

11. **MFCC parametrizace je neobvykla a zaslouzi zdovodneni**
   - `2.5 ms` frame, `1.0 ms` hop a `NFFT=512` pro ~55sample frame (`r. 125`) je velmi silne zero-padded setup.
   - Neni nutne spatne, ale je vhodne vysvetlit, proc je tato volba lepsi nez beznejsi parametrizace.

12. **Vysledky by mely obsahovat confusion matrices nebo absolutni pocty**
   - Pri silne nevyvazenosti jsou absolutni `TP/FP/FN/TN` dulezite pro interpretaci prakticke pouzitelnosti.

## EIC Review Report

### Overall Recommendation

`Major Revision`

### Summary Assessment

Clanek resi prakticky relevantni problem a ma srozumitelny narativ: od field acoustic sensing pres robustni post-trigger klasifikaci az k embedded deploymentu. Prinos je potencialne zajimavy zejmena tam, kde je potreba velmi lehka inference na low-power platforme. Text ma navic dobrou disciplinu ve vymezovani scope a neprodava continuous-stream trigger performance, kterou nemeri.

Soucasne ale hlavni claim clanku stoji na vysledku, ktery je v aktualnim provedeni nedostatecne vysvetleny: stejny kvantizovany model udajne funguje vyrazne lepe na ESP32-S3 nez v `PC int8` rezimu. Bez vysvetleni evaluacniho protokolu a bez odstraneni numerickych nejasnosti to nevypada jako presvedcivy deployment validation bridge, ale spise jako signal, ze srovnavate neekvivalentni experimentalni podminky.

### Strengths

1. **Dobre ohraniceny scope**: Clanek opakovane priznava, ze hodnoti jen post-trigger klasifikaci, nikoli cely trigger pipeline (`r. 58-62`, `r. 91`, `r. 197`).
2. **Prakticky relevantni metriky**: Pouziti MCC je spravne rozhodnuti vzhledem k nevyvazenym tridam (`r. 136`).
3. **Deployment-aware framing**: Autori propojuji ML vykon s embedded constrainty, nikoli jen s desktop accuracy (`r. 84-87`, `r. 132-133`, `r. 169`).

### Weaknesses

1. **Neuzavrena interni konzistence hlavniho vysledku**: `PC int8` a `ESP32-S3` by nemely divergovat tak vyrazne bez jasneho vysvetleni.
2. **Slabsi reprodukovatelnost experimentu**: Chybi detaily splitu, validacniho protokolu, thresholdingu a uncertainty.
3. **Prinos vuci baseline neni dostatecne ukotveny**: Neni zrejme, zda vykon plyne z architektury, dataset designu, augmentace nebo pouze z konfigurace thresholdu.

## Methodology Review Report

### Overall Recommendation

`Major Revision`

### Hlavni metodicke pripominky

1. **Neni popsano model selection protocol**
   - "selected model" (`r. 133`) implikuje nejake rozhodovani, ale neni receno, zda existovala validacni sada, early stopping, nebo tuning mimo test set.

2. **Jeden hold-out split je pro tento typ claimu slaby**
   - Pro robustnost a deployment paper by bylo vhodnejsi repeated stratified splits nebo jeste lepe grouped split podle recording session / gun / day.

3. **Ablace chybi v nejdulezitejsich mistech**
   - Expanded negatives, augmentation a class weighting jsou smichany do jednoho baliku (`r. 127-130`).
   - Bez ablace nelze dolozit, ktera komponenta skutecne prispiva.

4. **Neni jasne, zda jsou perturbace deterministicke nebo opakovane**
   - To je zasadni pro interpretaci jemnych desetinnych rozdilu v tabulkach.

5. **Chybi calibration / threshold protocol**
   - Pri slabe precision float32 modelu je volba decision thresholdu kriticka.

### Co doplnit

- presny pocet test samples a positives pro kazdy reported condition
- absolutni confusion matrices
- threshold selection rule
- detail representative dataset / quantization pipeline
- repeated-split nebo grouped-split evaluaci
- CI nebo bootstrap intervaly pro MCC, precision a recall

## Domain Review Report

### Overall Recommendation

`Major Revision`

### Domain Assessment

Domenove zasazeni je slusne a reference pokryvaji klasicke artillery/gunshot acoustic prace i obecnejsi audio-classification zdroje. Silne je zejmena to, ze text neignoruje akustickou variabilitu pole ani rozdil mezi triggeringem a naslednou klasifikaci (`r. 66-87`).

Slabsi je pozicovani prispevku vuci existujicim baseline pristupum v samotne experimentalni casti. Clanek tvrdi, ze lightweight post-trigger classifier je "feasible", ale neukazuje, zda je lepsi nez jednodussi klasifikator na stejnych MFCC ani zda tato konkretni CNN architektura prinasi neco vic nez standardni baseline.

### Specific Issues

1. **Contribution framing je spise validacni nez metodologicke**
   - To je v poradku, ale musi to byt explicitne priznano: prinos neni nova architektura, spise empiricka embedded validacni studie.

2. **Chybi silnejsi propojeni mezi datasetem a operational scenario**
   - Non-launch class je zminen, ale neni rozebrano, jake typy false alarms reprezentuje (`r. 103-105`).

3. **Research gap by mel byt formulovan ostreji**
   - Ted je gap castecne mezi robustnosti a deployability, ale neni plne ukazano, co presne chybi ve stavajici literature a jak to clanek uzavira lepe nez pouze "small embedded field dataset".

## Perspective Review Report

### Overall Recommendation

`Major Revision`

### Broader Impact and Deployment Perspective

Z praktickeho pohledu je nejcennejsi, ze clanek miri na nizkoprikonove cile a neskryva limity continuous-stream deploymentu. Soucasne ale skutecna operational value zavisi prave na tom, co zde zatim zustava mimo scope: false trigger rate, end-to-end latency vcetne featuringu, session-shift generalization a chovani v novych acoustic scenes.

Pro ctenare z deployment praxe tak text zatim odpovida spise na otazku "lze embedded targetu poslat predpocitane MFCC a dostat predikci?" nez na otazku "je cely system pripraven k realistickemu nasazeni?" To samo o sobe neni problem, ale musi to byt komunikovano ostreji a konzistentneji v title, abstractu a conclusion.

## Devil's Advocate Review

### Strongest Counter-Argument

Nejsilnejsi protiargument vuci clanku zni takto: prezentovany vysledek mozna neukazuje skutecnou robustnost embedded artillery-launch klasifikatoru, ale spise nekonzistenci evaluacniho protokolu. Autori tvrdi, ze stejny kvantizovany model bezi v `PC int8` i na `ESP32-S3`, pricemz host generuje stejne MFCC tensory. Pokud je to pravda, hardware deployment by nemel dramaticky zlepsit precision a MCC. Kdyz ale embedded target v tabulce vychazi podstatne lepe nez PC int8 reference, nejpravdepodobnejsi vysvetleni neni "embedded deployment is more robust", ale ze se ve skutecnosti porovnavaji dva ruzne pipeline rezimy, dva ruzne thresholdy, odlisna vstupni kvantizace, jiny scoring, nebo ze doslo k chybe v agregaci vysledku. V takovem pripade hlavni claim clanku neprokazuje robustnost deploymentu, ale dokumentuje nedostatecne uzavrenou experimentalni metodiku. Dokud autori nedolozi, proc se stejne modelove jadro na stejnych vstupech chova tak odlisne, zustava jadro clanku napadnutelne.

### Issue List

#### CRITICAL

| # | Dimension | Issue Description | Location |
|---|-----------|-------------------|----------|
| 1 | Logic chain validation | Same quantized model + same host-generated MFCC inputs should not yield a large embedded advantage without further explanation. | `r. 132-133`, `r. 168-188` |
| 2 | Data-conclusion mismatch | Text says embedded results do not show harmful drift relative to `PC int8`, but the table shows large differences, not small drift. | `r. 168-169`, `r. 180-188` |

#### MAJOR

| # | Dimension | Issue Description | Location |
|---|-----------|-------------------|----------|
| 1 | Overgeneralization | Title and conclusion still suggest a validated two-stage pipeline although only post-trigger classification is measured. | `r. 17`, `r. 58-62`, `r. 199-200` |
| 2 | Alternative paths | No baseline or ablation evidence excludes simpler explanations such as thresholding, split variance, or leakage. | `r. 127-130`, `r. 136`, `r. 145-188` |
| 3 | Confirmation bias risk | The discussion acknowledges limitations, but the overall narrative still leans toward a positive causal reading of quantization gains. | `r. 48`, `r. 166`, `r. 195-197` |

#### MINOR

| # | Dimension | Issue Description | Location |
|---|-----------|-------------------|----------|
| 1 | "So what?" test | Practical deployment significance would be stronger with end-to-end on-device feature extraction timing. | `r. 84-85`, `r. 133`, `r. 169` |

## Editorial Decision Package

## Part 1: Decision Letter

Decision: `Major Revision`

Rukopis ma potencial, ale pred dalsi verzi musi autori uzavrit validacni logiku hlavniho vysledku. Nejvyssi prioritu ma vysvetlit a pripadne opravit vztah mezi `PC int8` a `ESP32-S3` vysledky, protoze prave ten dnes nese hlavni argument clanku. Dale je nutne zpresnit evaluacni protokol, doplnit uncertainty a vyjasnit, zda reported procenta odpovidaji jednomu hold-out splitu nebo agregaci pres vice perturbovanych realizaci. Bez techto oprav bude vetsina ctenaru opravnene pochybovat, zda jde o robustni embedded vysledek, nebo o artefakt experimentu.

## Part 2: Revision Roadmap

### Required Revisions (Must Fix)

| # | Revision Item | Source | Priority |
|---|--------------|--------|----------|
| R1 | Vysvetlit, proc `ESP32-S3` vyrazne prekonava `PC int8` pro tentyz kvantizovany model a stejne host-generated MFCC vstupy; pokud je to reporting chyba, opravit tabulku i text. | DA, R1, EIC | P1 |
| R2 | Explicitne popsat evaluacni protokol pro kazde SNR: velikost effective test setu, pocet opakovani, zda jsou perturbace stochasticke, jak se agreguji vysledky. | R1, DA | P1 |
| R3 | Doplnit thresholding/calibration rule a presny model-selection protocol. | R1 | P1 |
| R4 | Dodat uncertainty estimation: repeated splits, grouped split podle session/gun/day, nebo bootstrap CI minimalne pro MCC/precision/recall. | R1, EIC | P1 |
| R5 | Doplnit confusion matrices nebo absolutni TP/FP/FN/TN counts pro vsechny hlavni podminky. | R1 | P1 |

### Suggested Revisions (Should Fix)

| # | Revision Item | Source | Priority |
|---|--------------|--------|----------|
| S1 | Pridat ablacni tabulku pro expanded negatives, augmentation a class weighting. | R1, R2 | P2 |
| S2 | Pridat alespon jeden jednoduchy baseline classifier na stejnych MFCC. | R2 | P2 |
| S3 | Rozsirit dataset description o session structure, negative subclasses a recording variability. | R2, R3 | P2 |
| S4 | Zpresnit title/abstract/conclusion tak, aby nebylo mozne clanek cist jako plnou validaci cele two-stage pipeline. | EIC, R3 | P2 |
| S5 | Zdovodnit MFCC parametrizaci a oddelit inference latency od full on-device pipeline latency. | R1, R3 | P2 |

### Minor Revisions (Nice to Fix)

| # | Revision Item | Source | Priority |
|---|--------------|--------|----------|
| M1 | Pridat kratke vysvetleni, proc je `MCC` hlavni interpretacni metrika. | EIC | P3 |
| M2 | Drobne zjemnit formulace o "robustness benefit of quantization", aby zustaly plne observacni. | EIC, DA | P3 |

## Questions for Authors

1. Jak presne vznikaji hodnoty v Tab. 2 a Tab. 3: z jednoho pruchodu test setem, nebo z prumeru pres vice stochastickych perturbaci?
2. Jaky je decision threshold pro float32, `PC int8` a `ESP32-S3`, a je ve vsech trech rezimech totozny?
3. Pokud host generuje stejne MFCC tensory a model je tentyz, cim si vysvetlujete vyrazne lepsi embedded vysledky?
4. Jsou train/test eventy rozdeleny i podle recording session / firing instance / weapon setup, nebo jen event-level?
5. Kolik pozitivnich eventu skutecne konci v test setu a jake jsou absolutni confusion counts pro kazdou SNR podminku?

## Technical Notes

- `pdflatex` build prosel pri pouziti alternativniho `jobname` `article_main_review`.
- Puvodni zapis do `article_main.pdf` byl blokovan lockem na vystupnim PDF, nikoli chybou v LaTeXu.
- Po `bibtex` + opakovanem `pdflatex` zustavaji jen bezne `Underfull \\hbox` warningy.
- Vysledny zkusebni build vygeneroval 5strankove PDF, takze dokument je technicky kompilovatelny.
