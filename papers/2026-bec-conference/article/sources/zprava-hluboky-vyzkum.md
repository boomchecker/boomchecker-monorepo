# Doplňkové zdroje k článku o embedded akustické detekci dělostřeleckého odpalu

## Předpoklady a identifikace vstupního článku

**Vstupní článek:** Maxa, Martin; Svatos, Jakub; Zelinka, Jakub. *A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection*. Z dostupného PDF je zřejmé, že jde o „state of the art“ k tématům akustické detekce/klasifikace impulsních událostí, robustnosti vůči šumu a doménovému posunu, MFCC/log-mel reprezentací, CNN a nasazení na ESP32-S3/TinyML; DOI ani finální venue nejsou v dostupném PDF jednoznačně uvedeny. Pro ještě přesnější dohledání by pomohly zejména: finální abstrakt, rok/venue publikace a DOI finální verze. fileciteturn0file0

## Primární výzkum

1. **Raponi, Simone; Ali, Isra; Oligeri, Gabriele.** *Sound of Guns: Digital Forensics of Gun Audio Samples meets Artificial Intelligence*. arXiv preprint arXiv:2004.07948, 2020. DOI/URL: `10.48550/arXiv.2004.07948`. Open access: **ano**. Relevance: Přímý doplněk k akustické klasifikaci zbraní z jedné nahrávky bez speciální senzorové sítě; vhodné pro srovnání s pipeline založenou na embedded klasifikaci impulsních událostí. Klíčová slova: gunshot classification, CNN, muzzle blast, digital forensics, single-recording. citeturn8view0

2. **Park, Junwoo; Cho, Youngwoo; Sim, Gyuhyeon; Lee, Hojoon; Choo, Jaegul.** *Enemy Spotted: in-game gun sound dataset for gunshot classification and localization*. In: *IEEE Conference on Games*, 2022. DOI/URL: `10.1109/CoG51982.2022.9893670`. Open access: **ano**. Relevance: Dává použitelný dataset i baseline pro klasifikaci a lokalizaci výstřelů, zejména když je sběr reálných dat obtížný nebo nebezpečný. Klíčová slova: gunshot dataset, localization, classification, BGG, synthetic data. citeturn47view1

3. **Shah, Ankit; Singh, Rita; Raj, Bhiksha; Hauptmann, Alexander.** *Deciphering GunType Hierarchy through Acoustic Analysis of Gunshot Recordings*. arXiv preprint arXiv:2506.20609, 2025. DOI/URL: `10.48550/arXiv.2506.20609`. Open access: **ano**. Relevance: Aktuální práce k detekci a hierarchické klasifikaci typů střelných zbraní; užitečná jako novější srovnání pro modely klasifikující impulsní akustické události. Klíčová slova: firearm type classification, hierarchy, gunshot acoustics, deep learning, noisy web data. citeturn49view5

4. **Salamon, Justin; Bello, Juan Pablo.** *Deep Convolutional Neural Networks and Data Augmentation for Environmental Sound Classification*. arXiv preprint arXiv:1608.04363, 2016. DOI/URL: `10.48550/arXiv.1608.04363`. Open access: **ano**. Relevance: Praktický zdroj k otázce „jak se to dělá“ pro augmentaci audio dat a trénink CNN nad environmentálními zvuky; přímo přenositelný do robustní detekce výstřelových zvuků. Klíčová slova: data augmentation, environmental sound classification, CNN, robustness, spectrogram. citeturn9academia0

5. **Kong, Qiuqiang; Cao, Yin; Iqbal, Turab; Wang, Yuxuan; Wang, Wenwu; Plumbley, Mark D.** *PANNs: Large-Scale Pretrained Audio Neural Networks for Audio Pattern Recognition*. arXiv preprint arXiv:1912.10211, 2019. DOI/URL: `10.48550/arXiv.1912.10211`. Open access: **ano**. Relevance: Silný referenční bod pro transfer learning v audio klasifikaci; zvlášť relevantní tam, kde je doménových dělostřeleckých dat málo a je třeba přenášet reprezentace z velkých audio kolekcí. Klíčová slova: transfer learning, pretrained audio models, AudioSet, CNN14, Wavegram-Logmel-CNN. citeturn50view1

6. **Kong, Qiuqiang; Yu, Changsong; Iqbal, Turab; Xu, Yong; Wang, Wenwu; Plumbley, Mark D.** *Weakly Labelled AudioSet Tagging with Attention Neural Networks*. *IEEE/ACM Transactions on Audio, Speech, and Language Processing*, 27(11), 1791–1802, 2019. DOI/URL: `10.1109/TASLP.2019.2930913`. Open access: **ano**. Relevance: Důležité pro práci se slabě anotovanými audio daty a pro attention-based tagování, které se hodí při rozšiřování trénovacích kolekcí vojenských nebo forenzních zvuků. Klíčová slova: weak labels, attention, AudioSet, audio tagging, large-scale audio. citeturn48view3turn49view3

7. **Gong, Yuan; Chung, Yu-An; Glass, James.** *PSLA: Improving Audio Tagging with Pretraining, Sampling, Labeling, and Aggregation*. *IEEE/ACM Transactions on Audio, Speech, and Language Processing*, 29, 3292–3306, 2021. DOI/URL: `10.1109/TASLP.2021.3120633`. Open access: **ano**. Relevance: Velmi užitečné pro „training recipe“ stránku problému: balanced sampling, label enhancement a agregace modelů jsou přímo přenositelné do nevyvážených akustických datasetů s málo výstřely. Klíčová slova: audio tagging, pretraining, balanced sampling, label enhancement, aggregation. citeturn45view1

8. **Morsali, M. Mehrdad; Mohammadzade, Hoda; Bagheri Shouraki, Saeed.** *Face: Fast, Accurate and Context-Aware Audio Annotation and Classification*. arXiv preprint arXiv:2303.03666, 2023. DOI/URL: `10.48550/arXiv.2303.03666`. Open access: **ano**. Relevance: Užitečné pro aktivní anotaci a výběr příznaků, tedy pro fázi budování vlastního datasetu a snižování nákladů na ruční štítkování akustických událostí. Klíčová slova: active learning, annotation, feature selection, UrbanSound8K, audio classification. citeturn45view0

9. **Abdoli, Sajjad; Cardinal, Patrick; Koerich, Alessandro Lameiras.** *End-to-End Environmental Sound Classification using a 1D Convolutional Neural Network*. arXiv preprint arXiv:1904.08990, 2019. DOI/URL: `10.48550/arXiv.1904.08990`. Open access: **ano**. Relevance: Nabízí alternativu k MFCC/log-mel vstupům přes učení přímo z waveformu; vhodné jako metodická protiváha k feature-based pipeline. Klíčová slova: raw waveform, 1D CNN, end-to-end learning, environmental sound classification. citeturn49view4

10. **Walden, Finley; Dasgupta, Sagar; Rahman, Mizanur; Islam, Mhafuzul.** *Improving the Environmental Perception of Autonomous Vehicles using Deep Learning-based Audio Classification*. arXiv preprint arXiv:2209.04075, 2022. DOI/URL: `10.48550/arXiv.2209.04075`. Open access: **ano**. Relevance: Přestože je aplikační doména jiná, práce explicitně používá i třídu „gunshot“ a dobře ukazuje robustní audio klasifikaci v provozně šumových scénářích. Klíčová slova: gunshot class, UrbanSound8K, robust audio classification, CNN, real-world noise. citeturn43academia2

## Přehledové články a survey práce

1. **Crocco, Marco; Cristani, Marco; Trucco, Andrea; Murino, Vittorio.** *Audio Surveillance: a Systematic Review*. arXiv preprint arXiv:1409.7787, 2014. DOI/URL: `10.48550/arXiv.1409.7787`. Open access: **ano**. Relevance: Široký přehled audio surveillance metod včetně detekce událostí, klasifikace a použitých příznaků; dobrý rámec pro zasazení dělostřelecké akustiky do širší oblasti akustického dohledu. Klíčová slova: audio surveillance, systematic review, event classification, acoustic features, monitoring. citeturn15academia1

2. **Grumiaux, Pierre-Amaury; Kitić, Srđan; Girin, Laurent; Guérin, Alexandre.** *A Survey of Sound Source Localization with Deep Learning Methods*. arXiv preprint arXiv:2109.03465, 2021. DOI/URL: `10.48550/arXiv.2109.03465`. Open access: **ano**. Relevance: Doplňuje část kolem lokalizace akustického zdroje a je zvlášť užitečný tam, kde se pipeline rozšiřuje z detekce/klasifikace na odhad směru nebo polohy. Klíčová slova: sound source localization, deep learning, microphone arrays, survey, TDOA alternatives. citeturn15academia0

3. **Capogrosso, Luigi; Cunico, Federico; Cheng, Dong Seon; Fummi, Franco; Cristani, Marco.** *A Machine Learning-oriented Survey on Tiny Machine Learning*. arXiv preprint arXiv:2309.11932, 2023. DOI/URL: `10.48550/arXiv.2309.11932`. Open access: **ano**. Relevance: Systematický přehled optimalizací, workflow a toolchainů TinyML; velmi vhodné pro část nasazení na mikrokontrolérech. Klíčová slova: TinyML, survey, model optimization, edge AI, resource constraints. citeturn38academia0

4. **Somvanshi, Shriyank; Islam, Md Monzurul; Chhetri, Gaurab; Chakraborty, Rohit; Mimi, Mahmuda Sultana; Shuvo, Sawgat Ahmed; Islam, Kazi Sifatul; Javed, Syed Aaqib; Rafat, Sharif Ahmed; Dutta, Anandi; Das, Subasish.** *From Tiny Machine Learning to Tiny Deep Learning: A Survey*. arXiv preprint arXiv:2506.18927, 2025. DOI/URL: `10.48550/arXiv.2506.18927`. Open access: **ano**. Relevance: Novější přehled pro orientaci v posunu od klasického TinyML k hlubším modelům a novým toolchainům; užitečné pro aktualizaci „state of the art“ části. Klíčová slova: TinyDL, TinyML, survey, edge deployment, compression, hardware-software co-design. citeturn38academia1

## Knihy a knižní zdroje

1. **Warden, Pete; Situnayake, Daniel.** *TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers*. O’Reilly Media, 2020. DOI/URL: `ISBN 9781492052043`. Open access: **ne**. Relevance: Praktický „how-to“ zdroj k implementaci a nasazení modelů na mikrokontrolérech; vhodný zejména pro převod experimentální pipeline do embedded prototypu. Klíčová slova: TinyML, TensorFlow Lite, Arduino, embedded deployment, practical guide. citeturn44search2

2. **Deller, John R.; Proakis, John G.; Hansen, John H. L.** *Discrete-Time Processing of Speech Signals*. Wiley-IEEE Press, klasické vydání 1999. DOI/URL: `ISBN 9780780310940`. Open access: **ne**. Relevance: Základní referenční text pro analýzu diskrétních audio/speech signálů, ze kterého lze odvodit i postupy pro extrakci příznaků v impulsní akustice. Klíčová slova: digital signal processing, speech signals, feature extraction, time-frequency analysis. citeturn32search1

3. **Wang, DeLiang; Brown, Guy J., eds.** *Computational Auditory Scene Analysis: Principles, Algorithms, and Applications*. Wiley-IEEE Press, 2006. DOI/URL: `ISBN 9780471749967`. Open access: **ne**. Relevance: Silný teoretický základ pro oddělování, segmentaci a interpretaci více zdrojů zvuku; užitečné při úvahách o složitějších akustických scénách a rušení. Klíčová slova: CASA, auditory scene analysis, source separation, auditory modeling, machine listening. citeturn44search0

4. **Bregman, Albert S.** *Auditory Scene Analysis: The Perceptual Organization of Sound*. MIT Press, 1990. DOI/URL: `ISBN 9780262521956`. Open access: **ne**. Relevance: Klasická teoretická kniha pro porozumění tomu, jak se zvukové scény segmentují a skládají; vhodné jako konceptuální rámec pro akustickou detekci událostí. Klíčová slova: auditory scene analysis, perception, sound organization, acoustic streams. citeturn44search1

## Technické zprávy, systémové práce a white-paper typ zdrojů

1. **David, Robert; Duke, Jared; Jain, Advait; Reddi, Vijay Janapa; Jeffries, Nat; Li, Jian; Kreeger, Nick; Nappier, Ian; Natraj, Meghna; Regev, Shlomi; Rhodes, Rocky; Wang, Tiezhen; Warden, Pete.** *TensorFlow Lite Micro: Embedded Machine Learning on TinyML Systems*. arXiv preprint arXiv:2010.08678, 2021. DOI/URL: `10.48550/arXiv.2010.08678`. Open access: **ano**. Relevance: Zásadní práce k běhu inference v řádu kilobajtů paměti a k návrhu embedded inference frameworku; metodicky přímo relevantní pro nasazení na ESP32-S3. Klíčová slova: TensorFlow Lite Micro, embedded inference, TinyML systems, interpreter, microcontrollers. citeturn50view4

2. **Novac, Pierre-Emmanuel; Boukli Hacene, Ghouthi; Pegatoquet, Alain; Miramond, Benoît; Gripon, Vincent.** *Quantization and Deployment of Deep Neural Networks on Microcontrollers*. *Sensors*, 21(9):2984, 2021. DOI/URL: `10.3390/s21092984`. Open access: **ano**. Relevance: Velmi praktický zdroj k postupu kvantizace, měření paměti, času a energie a ke srovnání embedded inference enginů; přesně odpovídá otázce „jak to dostat na MCU“. Klíčová slova: quantization, deployment, microcontrollers, energy, inference engines. citeturn48view2turn49view2

3. **Lin, Ji; Chen, Wei-Ming; Lin, Yujun; Cohn, John; Gan, Chuang; Han, Song.** *MCUNet: Tiny Deep Learning on IoT Devices*. arXiv preprint arXiv:2007.10319, 2020. DOI/URL: `10.48550/arXiv.2007.10319`. Open access: **ano**. Relevance: Důležitý zdroj pro co-design architektury a inference enginu na MCU; vhodné pro optimalizaci modelu, když je cílem co nejnižší RAM/Flash footprint. Klíčová slova: TinyNAS, TinyEngine, MCU deployment, memory-efficient inference, IoT. citeturn49view0

4. **Banbury, Colby; Zhou, Chuteng; Fedorov, Igor; Matas Navarro, Ramon; Thakker, Urmish; Gope, Dibakar; Reddi, Vijay Janapa; Mattina, Matthew; Whatmough, Paul N.** *MicroNets: Neural Network Architectures for Deploying TinyML Applications on Commodity Microcontrollers*. arXiv preprint arXiv:2010.11267, 2020. DOI/URL: `10.48550/arXiv.2010.11267`. Open access: **ano**. Relevance: Užitečné pro návrh architektur s ohledem na latenci, spotřebu a paměť; vhodné jako benchmarkový a architektonický doplněk k embedded audio pipeline. Klíčová slova: MicroNets, TinyMLperf, NAS, microcontrollers, latency-energy tradeoff. citeturn49view1

## Softwarové nástroje, datasety a repozitáře

1. **Kong, Qiuqiang.** *qiuqiangkong/audioset_tagging_cnn*. GitHub repozitář k práci PANNs, aktivní open-source implementace. DOI/URL: `https://github.com/qiuqiangkong/audioset_tagging_cnn`. Rok: 2020. Open access: **ano**. Stručná relevance: Přímá implementace pretrained audio modelů a trénovacích skriptů; velmi vhodné pro replikaci a transfer learning nad vlastními akustickými daty. Klíčová slova: PANNs, GitHub, transfer learning, pretrained models, audio tagging. citeturn42view0

2. **Gong, Yuan.** *YuanGongND/psla*. GitHub repozitář k práci PSLA. DOI/URL: `https://github.com/YuanGongND/psla`. Rok: 2021. Open access: **ano**. Stručná relevance: Praktická implementace balanced sampling, label enhancement a aggregation pipeline; užitečné pro trénink na nevyvážených datech. Klíčová slova: PSLA, GitHub, training recipe, balanced sampling, audio tagging. citeturn45view1turn46view1

3. **Morsali, M. Mehrdad; Mohammadzade, Hoda; Bagheri Shouraki, Saeed.** *gitmehrdad/FACE: Urban Sound Annotation and Classification*. GitHub repozitář. DOI/URL: `https://github.com/gitmehrdad/FACE`. Rok: 2023. Open access: **ano**. Stručná relevance: Obsahuje notebooky pro anotaci, extrakci příznaků a klasifikaci; praktické pro tvorbu vlastního workflow sběru a štítkování dat. Klíčová slova: annotation, active learning, feature extraction, GitHub, UrbanSound8K. citeturn46view0

4. **Gemmeke, Jort F.; Ellis, Daniel P. W.; Freedman, Dylan; Jansen, Aren; Lawrence, Wade; Moore, R. Channing; Plakal, Manoj; Ritter, Marvin.** *Audio Set: An Ontology and Human-Labeled Dataset for Audio Events*. IEEE ICASSP, 2017; oficiální datasetová stránka Google AudioSet. DOI/URL: `https://research.google.com/audioset/`. Rok: 2017. Open access: **ano**. Stručná relevance: Velký slabě anotovaný audio korpus pro pretraining a transfer learning; vhodný jako zdroj obecných akustických reprezentací. Klíčová slova: AudioSet, large-scale dataset, weak labels, ontology, pretraining. citeturn28search0turn35search4

5. **Fonseca, Eduardo; Favory, Xavier; Pons, Jordi; Font, Frederic; Serra, Xavier.** *FSD50K: An Open Dataset of Human-Labeled Sound Events*. arXiv preprint arXiv:2010.00475, 2020. DOI/URL: `10.48550/arXiv.2010.00475`. Open access: **ano**. Stručná relevance: Otevřený dataset pro sound event recognition s ručními anotacemi; vhodný pro benchmarky i transfer learning tam, kde je AudioSet limitován dostupností původního audia. Klíčová slova: FSD50K, open dataset, sound events, benchmark, transfer learning. citeturn41view2

6. **Park, Junwoo; Cho, Youngwoo; Sim, Gyuhyeon; Lee, Hojoon; Choo, Jaegul.** *BGG dataset* popsaný v práci *Enemy Spotted: in-game gun sound dataset for gunshot classification and localization*. DOI/URL: `10.1109/CoG51982.2022.9893670`. Rok: 2022. Open access: **ano**. Stručná relevance: Specializovaný dataset pro zvuky střelby, vzdálenosti a směry; z pohledu tématu článku jde o nejbližší dostupný datasetový doplněk. Klíčová slova: BGG, gunshot dataset, direction, distance, firearm classes. citeturn47view1

## Srovnávací tabulka

| kategorie | název | autoři | rok | DOI/URL | open access | stručná relevance |
|---|---|---|---:|---|---|---|
| primární výzkum | Sound of Guns | Raponi; Ali; Oligeri | 2020 | `10.48550/arXiv.2004.07948` | ano | Akustická klasifikace zbraní z jediné nahrávky. |
| primární výzkum | Enemy Spotted | Park et al. | 2022 | `10.1109/CoG51982.2022.9893670` | ano | Dataset a baseline pro klasifikaci/lokalizaci výstřelů. |
| primární výzkum | Deciphering GunType Hierarchy | Shah et al. | 2025 | `10.48550/arXiv.2506.20609` | ano | Novější hierarchická klasifikace typů zbraní. |
| primární výzkum | Deep CNNs and Data Augmentation for ESC | Salamon; Bello | 2016 | `10.48550/arXiv.1608.04363` | ano | Praktická augmentace a CNN pro robustní audio klasifikaci. |
| primární výzkum | PANNs | Kong et al. | 2019 | `10.48550/arXiv.1912.10211` | ano | Pretrained audio modely pro transfer learning. |
| primární výzkum | Weakly Labelled AudioSet Tagging with Attention NNs | Kong et al. | 2019 | `10.1109/TASLP.2019.2930913` | ano | Slabě anotovaná velká data a attention. |
| primární výzkum | PSLA | Gong; Chung; Glass | 2021 | `10.1109/TASLP.2021.3120633` | ano | Balanced sampling a training recipe pro audio tagging. |
| primární výzkum | FACE | Morsali; Mohammadzade; Bagheri Shouraki | 2023 | `10.48550/arXiv.2303.03666` | ano | Aktivní anotace a feature selection. |
| primární výzkum | End-to-End ESC using a 1D CNN | Abdoli; Cardinal; Koerich | 2019 | `10.48550/arXiv.1904.08990` | ano | Alternativa k MFCC/log-mel přes raw waveform. |
| primární výzkum | Improving the Environmental Perception of Autonomous Vehicles using Audio Classification | Walden et al. | 2022 | `10.48550/arXiv.2209.04075` | ano | Obsahuje i třídu gunshot v robustním provozním scénáři. |
| přehledové články a meta-analýzy | Audio Surveillance: a Systematic Review | Crocco et al. | 2014 | `10.48550/arXiv.1409.7787` | ano | Systematický rámec pro audio surveillance. |
| přehledové články a meta-analýzy | A Survey of Sound Source Localization with Deep Learning Methods | Grumiaux et al. | 2021 | `10.48550/arXiv.2109.03465` | ano | Přehled lokalizace zvukových zdrojů. |
| přehledové články a meta-analýzy | A Machine Learning-oriented Survey on Tiny Machine Learning | Capogrosso et al. | 2023 | `10.48550/arXiv.2309.11932` | ano | Přehled TinyML workflow a optimalizací. |
| přehledové články a meta-analýzy | From Tiny Machine Learning to Tiny Deep Learning: A Survey | Somvanshi et al. | 2025 | `10.48550/arXiv.2506.18927` | ano | Aktualizace stavu edge AI/TinyDL. |
| knihy a kapitoly | TinyML | Warden; Situnayake | 2020 | `ISBN 9781492052043` | ne | Praktická implementace modelů na MCU. |
| knihy a kapitoly | Discrete-Time Processing of Speech Signals | Deller; Proakis; Hansen | 1999 | `ISBN 9780780310940` | ne | DSP základ pro extrakci audio příznaků. |
| knihy a kapitoly | Computational Auditory Scene Analysis | Wang; Brown (eds.) | 2006 | `ISBN 9780471749967` | ne | Teoretický základ pro segmentaci a separaci zvuků. |
| knihy a kapitoly | Auditory Scene Analysis | Bregman | 1990 | `ISBN 9780262521956` | ne | Klasický konceptuální rámec percepční organizace zvuku. |
| technické zprávy, standardy a white papers | TensorFlow Lite Micro | David et al. | 2021 | `10.48550/arXiv.2010.08678` | ano | Systémový popis embedded inference frameworku. |
| technické zprávy, standardy a white papers | Quantization and Deployment of Deep Neural Networks on Microcontrollers | Novac et al. | 2021 | `10.3390/s21092984` | ano | Kvantizace, latency, energie a deployment na MCU. |
| technické zprávy, standardy a white papers | MCUNet | Lin et al. | 2020 | `10.48550/arXiv.2007.10319` | ano | Co-design architektury a inference enginu. |
| technické zprávy, standardy a white papers | MicroNets | Banbury et al. | 2020 | `10.48550/arXiv.2010.11267` | ano | TinyML architektury s ohledem na latenci/energii. |
| softwarové nástroje/datasets a repozitáře | audioset_tagging_cnn | Kong | 2020 | `https://github.com/qiuqiangkong/audioset_tagging_cnn` | ano | Open-source implementace PANNs a trénovacích skriptů. |
| softwarové nástroje/datasets a repozitáře | psla | Gong | 2021 | `https://github.com/YuanGongND/psla` | ano | Implementace PSLA recipe pro nevyvážená audio data. |
| softwarové nástroje/datasets a repozitáře | FACE repository | Morsali et al. | 2023 | `https://github.com/gitmehrdad/FACE` | ano | Notebooky pro anotaci, features a klasifikaci. |
| softwarové nástroje/datasets a repozitáře | AudioSet | Gemmeke et al. | 2017 | `https://research.google.com/audioset/` | ano | Velký dataset pro pretraining obecných audio reprezentací. |
| softwarové nástroje/datasets a repozitáře | FSD50K | Fonseca et al. | 2020 | `10.48550/arXiv.2010.00475` | ano | Otevřený benchmark sound event recognition. |
| softwarové nástroje/datasets a repozitáře | BGG dataset | Park et al. | 2022 | `10.1109/CoG51982.2022.9893670` | ano | Specializovaný dataset pro zvuky střelby, směry a vzdálenosti. |

## Vyhledávací dotazy a databáze

**Doporučené vyhledávací dotazy:**
- `embedded acoustic artillery launch detection CNN MFCC ESP32-S3`
- `gunshot classification muzzle blast acoustic dataset localization`
- `audio tagging weakly labeled gunshot classification attention`
- `TinyML audio classification TensorFlow Lite Micro ESP32-S3`
- `robust environmental sound classification domain shift augmentation`
- `artillery acoustic localization distributed sensors`
- `gunshot dataset firearm classification localization`
- `kvantizace neuronových sítí mikrořadiče audio klasifikace`
- `akustická detekce impulsních událostí embedded systémy`
- `robustní klasifikace zvuku šum doménový posun`

**Databáze a portály k prohledání:**
- IEEE Xplore
- ACM Digital Library
- Scopus
- Web of Science
- SpringerLink
- ScienceDirect
- Google Scholar
- arXiv
- Zenodo
- GitHub