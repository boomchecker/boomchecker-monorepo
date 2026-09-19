# Detailní report zátěžového testu detekčního algoritmu

Tento report podrobně popisuje úspěšnost Lineárního SVM modelu na jednotlivých nahrávkách. Porovnáváme **dvě metody extrakce příznaků**:
1. **Unnormalized (MCU Match)**: Příznaky bez globální normalizace signálu. Přesně odpovídá tomu, co počítá firmware v C (obsahuje frame-level RMS stabilizaci, ale zachovává absolutní měřítko spektrální energie).
2. **Global RMS Normalized**: Příznaky, u kterých je celý signál předem podělen globální RMS hodnotou (použito v dřívějších skriptech v Pythonu).

## Výsledky pro metodu: Unnormalized (MCU Match)

### 1. Souhrnná tabulka dle kategorií

| Kategorie | Popis | Celkem vzorků | Správně | Úspěšnost | Průměrná jistota (Decision Value) |
|:---|:---|:---:|:---:|:---:|:---:|
| **Bebop Drone (Clean)** | Testovací vzorky z této skupiny | 331 | 330 | **99.70%** | 1.4766 |
| **Local Ambient Noise (ESC-50)** | Testovací vzorky z této skupiny | 10371 | 9537 | **91.96%** | -4.0388 |
| **Membo Drone (Clean)** | Testovací vzorky z této skupiny | 331 | 314 | **94.86%** | 2.2803 |
| **Mixed Bebop (Noise+Drone)** | Testovací vzorky z této skupiny | 335 | 280 | **83.58%** | 0.7896 |
| **Mixed Membo (Noise+Drone)** | Testovací vzorky z této skupiny | 335 | 218 | **65.07%** | 0.3326 |
| **Parquet Drone** | Testovací vzorky z této skupiny | 200 | 199 | **99.50%** | 1.7669 |
| **Parquet Noise** | Testovací vzorky z této skupiny | 200 | 195 | **97.50%** | -3.1174 |

### 2. Analýza falešných poplachů v ambientním šumu (ESC-50)
Tabulka ukazuje, které konkrétní typy okolních zvuků způsobují falešné detekce (Falešné poplachy, False Positives). U ideálního chování by měly mít všechny zvuky úspěšnost 100% (což znamená 0% FP) a záporný Decision Value.

| Třída zvuku | Třída ID | Celkem testováno | Správně odmítnuto | Falešné poplachy (FP Rate) | Průměrný Decision Value |
|:---|:---:|:---:|:---:|:---:|:---:|
| dog | 0 | 200 | 200 | 0.00% | -8.4304 |
| rooster | 1 | 200 | 199 | 0.50% | -6.9128 |
| pig | 2 | 200 | 195 | 2.50% | -4.5282 |
| cow | 3 | 200 | 199 | 0.50% | -5.0914 |
| frog | 4 | 200 | 199 | 0.50% | -4.8485 |
| cat | 5 | 200 | 199 | 0.50% | -5.5868 |
| hen | 6 | 200 | 200 | 0.00% | -5.7671 |
| insects | 7 | 200 | 194 | 3.00% | -2.3884 |
| sheep | 8 | 200 | 197 | 1.50% | -3.5547 |
| crow | 9 | 200 | 199 | 0.50% | -4.6373 |
| rain | 10 | 200 | 80 | **60.00%** ⚠️ | 0.0735 |
| sea_waves | 11 | 200 | 185 | 7.50% | -1.4822 |
| crackling_fire | 12 | 200 | 166 | **17.00%** ⚠️ | -1.6471 |
| crickets | 13 | 200 | 104 | **48.00%** ⚠️ | -0.4826 |
| chirping_birds | 14 | 200 | 168 | **16.00%** ⚠️ | -2.1884 |
| water_drops | 15 | 200 | 199 | 0.50% | -5.1036 |
| wind | 16 | 200 | 183 | 8.50% | -1.7584 |
| pouring_water | 17 | 200 | 200 | 0.00% | -4.5513 |
| toilet_flush | 18 | 200 | 169 | **15.50%** ⚠️ | -2.1021 |
| thunderstorm | 19 | 200 | 200 | 0.00% | -3.3233 |
| crying_baby | 20 | 200 | 198 | 1.00% | -6.8683 |
| sneezing | 21 | 200 | 200 | 0.00% | -7.1344 |
| clapping | 22 | 200 | 200 | 0.00% | -4.2770 |
| breathing | 23 | 200 | 200 | 0.00% | -5.0899 |
| coughing | 24 | 200 | 200 | 0.00% | -7.2455 |
| footsteps | 25 | 200 | 200 | 0.00% | -4.1550 |
| laughing | 26 | 200 | 200 | 0.00% | -6.3944 |
| brushing_teeth | 27 | 200 | 193 | 3.50% | -2.7120 |
| snoring | 28 | 200 | 178 | **11.00%** ⚠️ | -2.6560 |
| drinking_sipping | 29 | 200 | 200 | 0.00% | -6.2097 |
| door_wood_creak | 30 | 200 | 199 | 0.50% | -6.1299 |
| dustbin | 31 | 200 | 199 | 0.50% | -5.5009 |
| gas_on_stove | 32 | 200 | 200 | 0.00% | -4.5851 |
| washing_machine | 33 | 200 | 195 | 2.50% | -5.2238 |
| vacuum_cleaner | 34 | 200 | 200 | 0.00% | -5.3185 |
| clock_tick | 35 | 200 | 131 | **34.50%** ⚠️ | -0.5894 |
| clock_alarm | 36 | 200 | 88 | **56.00%** ⚠️ | -0.2202 |
| keyboard_typing | 37 | 200 | 176 | **12.00%** ⚠️ | -4.8962 |
| door_wood_knock | 38 | 200 | 200 | 0.00% | -6.8098 |
| glass_breaking | 39 | 200 | 197 | 1.50% | -5.8380 |
| helicopter | 40 | 200 | 165 | **17.50%** ⚠️ | -1.6684 |
| chainsaw | 41 | 200 | 142 | **29.00%** ⚠️ | -1.2515 |
| siren | 42 | 200 | 193 | 3.50% | -4.5922 |
| car_horn | 43 | 200 | 191 | 4.50% | -4.2123 |
| engine | 44 | 200 | 166 | **17.00%** ⚠️ | -1.4134 |
| train | 45 | 200 | 182 | 9.00% | -1.7493 |
| church_bells | 46 | 200 | 200 | 0.00% | -4.5961 |
| airplane | 47 | 200 | 182 | 9.00% | -2.5475 |
| fireworks | 48 | 200 | 200 | 0.00% | -5.5194 |
| hand_saw | 49 | 200 | 200 | 0.00% | -4.0988 |
| unknown | 7100 | 1 | 1 | 0.00% | -3.2625 |

### 3. Analýza detekce dronů dle relací (Sessions)
Drony jsou seskupeny podle jednotlivých nahrávacích relací. Zde vidíme, zda model spolehlivě detekuje dron v průběhu celé nahrávky.

| Název relace | Celkem segmentů | Detekováno | Úspěšnost (Sensitivity) | Průměrná jistota (Decision Value) |
|:---|:---:|:---:|:---:|:---:|
| B_S2_D1_067 | 5 | 5 | 100.00% | 1.1775 |
| B_S2_D1_068 | 5 | 5 | 100.00% | 1.4718 |
| B_S2_D1_069 | 5 | 5 | 100.00% | 1.2057 |
| B_S2_D1_070 | 5 | 5 | 100.00% | 0.7654 |
| B_S2_D1_071 | 5 | 5 | 100.00% | 1.2883 |
| B_S2_D1_072 | 5 | 5 | 100.00% | 1.1329 |
| B_S2_D1_073 | 5 | 5 | 100.00% | 1.4172 |
| B_S2_D1_074 | 5 | 5 | 100.00% | 0.9769 |
| B_S2_D1_075 | 5 | 5 | 100.00% | 0.8662 |
| B_S2_D1_076 | 5 | 5 | 100.00% | 1.2536 |
| B_S2_D1_077 | 5 | 5 | 100.00% | 1.2234 |
| B_S2_D1_078 | 5 | 5 | 100.00% | 1.1049 |
| B_S2_D1_079 | 5 | 5 | 100.00% | 1.6366 |
| B_S2_D1_080 | 5 | 5 | 100.00% | 1.7224 |
| B_S2_D1_081 | 5 | 5 | 100.00% | 2.1653 |
| B_S2_D1_082 | 5 | 5 | 100.00% | 1.8841 |
| B_S2_D1_083 | 5 | 5 | 100.00% | 2.2353 |
| B_S2_D1_084 | 5 | 5 | 100.00% | 2.0294 |
| B_S2_D1_085 | 5 | 5 | 100.00% | 1.6189 |
| B_S2_D1_086 | 5 | 5 | 100.00% | 1.5512 |
| B_S2_D1_087 | 5 | 5 | 100.00% | 1.9564 |
| B_S2_D1_088 | 5 | 5 | 100.00% | 1.9401 |
| B_S2_D1_089 | 5 | 5 | 100.00% | 1.4020 |
| B_S2_D1_090 | 5 | 5 | 100.00% | 1.2443 |
| B_S2_D1_091 | 5 | 5 | 100.00% | 1.7066 |
| B_S2_D1_092 | 5 | 5 | 100.00% | 1.6639 |
| B_S2_D1_093 | 5 | 5 | 100.00% | 0.9492 |
| B_S2_D1_094 | 5 | 5 | 100.00% | 1.0065 |
| B_S2_D1_095 | 5 | 5 | 100.00% | 1.7060 |
| B_S2_D1_096 | 5 | 5 | 100.00% | 1.7616 |
| B_S2_D1_097 | 5 | 5 | 100.00% | 1.6785 |
| B_S2_D1_098 | 5 | 5 | 100.00% | 1.9904 |
| B_S2_D1_099 | 5 | 5 | 100.00% | 1.7465 |
| B_S2_D1_100 | 5 | 5 | 100.00% | 1.7091 |
| B_S2_D1_101 | 5 | 5 | 100.00% | 1.7848 |
| B_S2_D1_102 | 5 | 5 | 100.00% | 1.6849 |
| B_S2_D1_103 | 5 | 5 | 100.00% | 1.2165 |
| B_S2_D1_104 | 5 | 5 | 100.00% | 1.3164 |
| B_S2_D1_105 | 5 | 5 | 100.00% | 1.4472 |
| B_S2_D1_106 | 5 | 5 | 100.00% | 0.8304 |
| B_S2_D1_107 | 5 | 5 | 100.00% | 1.1157 |
| B_S2_D1_108 | 5 | 5 | 100.00% | 0.9264 |
| B_S2_D1_109 | 5 | 5 | 100.00% | 0.9383 |
| B_S2_D1_110 | 5 | 5 | 100.00% | 1.3297 |
| B_S2_D1_111 | 5 | 5 | 100.00% | 1.4587 |
| B_S2_D1_112 | 5 | 5 | 100.00% | 1.6249 |
| B_S2_D1_113 | 5 | 5 | 100.00% | 1.5106 |
| B_S2_D1_114 | 5 | 4 | **80.00%** ❌ | 0.4888 |
| B_S2_D1_115 | 5 | 5 | 100.00% | 0.8204 |
| B_S2_D1_116 | 5 | 5 | 100.00% | 2.0389 |
| B_S2_D1_117 | 5 | 5 | 100.00% | 1.8970 |
| B_S2_D1_118 | 5 | 5 | 100.00% | 1.4116 |
| B_S2_D1_119 | 5 | 5 | 100.00% | 1.7774 |
| B_S2_D1_120 | 5 | 5 | 100.00% | 2.0110 |
| B_S2_D1_121 | 5 | 5 | 100.00% | 1.5258 |
| B_S2_D1_122 | 5 | 5 | 100.00% | 1.4049 |
| B_S2_D1_123 | 5 | 5 | 100.00% | 1.7161 |
| B_S2_D1_124 | 5 | 5 | 100.00% | 1.6168 |
| B_S2_D1_125 | 5 | 5 | 100.00% | 1.8826 |
| B_S2_D1_126 | 5 | 5 | 100.00% | 1.6264 |
| B_S2_D1_127 | 5 | 5 | 100.00% | 1.4163 |
| B_S2_D1_128 | 5 | 5 | 100.00% | 1.1004 |
| B_S2_D1_129 | 5 | 5 | 100.00% | 1.8067 |
| B_S2_D1_130 | 5 | 5 | 100.00% | 1.4761 |
| B_S2_D1_131 | 5 | 5 | 100.00% | 1.5328 |
| B_S2_D1_132 | 5 | 5 | 100.00% | 1.5662 |
| B_S2_D1_133 | 1 | 1 | 100.00% | 1.3176 |
| Membo_0_000 | 5 | 4 | **80.00%** ❌ | 1.6242 |
| Membo_0_001 | 5 | 5 | 100.00% | 2.6469 |
| Membo_0_002 | 5 | 5 | 100.00% | 3.1190 |
| Membo_0_003 | 5 | 5 | 100.00% | 2.9007 |
| Membo_0_004 | 5 | 5 | 100.00% | 3.2229 |
| Membo_0_005 | 5 | 5 | 100.00% | 2.8330 |
| Membo_0_006 | 5 | 5 | 100.00% | 2.7437 |
| Membo_0_007 | 5 | 5 | 100.00% | 2.3849 |
| Membo_0_008 | 5 | 5 | 100.00% | 2.6033 |
| Membo_0_009 | 5 | 5 | 100.00% | 2.7340 |
| Membo_0_010 | 5 | 5 | 100.00% | 2.2925 |
| Membo_0_011 | 5 | 5 | 100.00% | 2.5009 |
| Membo_0_012 | 5 | 5 | 100.00% | 2.6488 |
| Membo_0_013 | 5 | 5 | 100.00% | 2.6147 |
| Membo_0_014 | 5 | 5 | 100.00% | 2.0671 |
| Membo_0_015 | 5 | 5 | 100.00% | 2.4388 |
| Membo_0_016 | 5 | 5 | 100.00% | 2.6923 |
| Membo_0_017 | 5 | 5 | 100.00% | 2.3769 |
| Membo_0_034 | 5 | 5 | 100.00% | 3.0490 |
| Membo_0_036 | 5 | 5 | 100.00% | 2.6988 |
| Membo_0_037 | 5 | 5 | 100.00% | 3.0623 |
| Membo_0_038 | 5 | 5 | 100.00% | 2.6872 |
| Membo_0_039 | 5 | 5 | 100.00% | 2.4696 |
| Membo_0_040 | 5 | 5 | 100.00% | 3.5775 |
| Membo_0_041 | 5 | 5 | 100.00% | 2.3917 |
| Membo_1_000 | 5 | 4 | **80.00%** ❌ | 1.6271 |
| Membo_1_001 | 5 | 5 | 100.00% | 2.5754 |
| Membo_1_002 | 5 | 5 | 100.00% | 3.3481 |
| Membo_1_004 | 5 | 5 | 100.00% | 3.0985 |
| Membo_1_029 | 5 | 5 | 100.00% | 3.1603 |
| Membo_1_030 | 5 | 5 | 100.00% | 3.4817 |
| Membo_1_031 | 5 | 5 | 100.00% | 3.3518 |
| Membo_1_032 | 5 | 5 | 100.00% | 3.2202 |
| Membo_1_033 | 5 | 5 | 100.00% | 2.8474 |
| Membo_1_034 | 5 | 5 | 100.00% | 2.9143 |
| Membo_1_041 | 5 | 5 | 100.00% | 2.3590 |
| Membo_1_042 | 5 | 5 | 100.00% | 2.8124 |
| Membo_1_043 | 5 | 5 | 100.00% | 2.9854 |
| Membo_1_044 | 5 | 5 | 100.00% | 2.5422 |
| Membo_1_045 | 5 | 5 | 100.00% | 2.4297 |
| Membo_1_046 | 5 | 5 | 100.00% | 2.2776 |
| Membo_1_047 | 5 | 5 | 100.00% | 2.3285 |
| Membo_1_048 | 5 | 5 | 100.00% | 2.2683 |
| Membo_2_010 | 5 | 5 | 100.00% | 3.1137 |
| Membo_2_011 | 5 | 5 | 100.00% | 2.3743 |
| Membo_2_013 | 5 | 5 | 100.00% | 3.3131 |
| Membo_2_014 | 5 | 5 | 100.00% | 3.2079 |
| Membo_2_015 | 5 | 5 | 100.00% | 2.7804 |
| Membo_2_016 | 5 | 5 | 100.00% | 2.3103 |
| Membo_2_017 | 5 | 5 | 100.00% | 2.8977 |
| Membo_2_018 | 5 | 5 | 100.00% | 2.7906 |
| Membo_2_019 | 5 | 5 | 100.00% | 3.3132 |
| Membo_2_020 | 5 | 5 | 100.00% | 3.7930 |
| Membo_2_027 | 5 | 4 | **80.00%** ❌ | 1.2734 |
| Membo_2_028 | 5 | 4 | **80.00%** ❌ | 0.5092 |
| extra_membo_D2_2000.wav | 1 | 0 | **0.00%** ❌ | -3.0367 |
| extra_membo_D2_2001.wav | 1 | 0 | **0.00%** ❌ | -1.0946 |
| extra_membo_D2_2002.wav | 1 | 0 | **0.00%** ❌ | -0.0535 |
| extra_membo_D2_2003.wav | 1 | 1 | 100.00% | 0.2169 |
| extra_membo_D2_2004.wav | 1 | 1 | 100.00% | 0.0753 |
| extra_membo_D2_2005.wav | 1 | 1 | 100.00% | 0.9698 |
| extra_membo_D2_2006.wav | 1 | 1 | 100.00% | 0.7616 |
| extra_membo_D2_2007.wav | 1 | 0 | **0.00%** ❌ | -1.0554 |
| extra_membo_D2_2008.wav | 1 | 0 | **0.00%** ❌ | -1.0865 |
| extra_membo_D2_2009.wav | 1 | 1 | 100.00% | 0.6177 |
| extra_membo_D2_2010.wav | 1 | 1 | 100.00% | 0.2665 |
| extra_membo_D2_2011.wav | 1 | 1 | 100.00% | 1.0262 |
| extra_membo_D2_2012.wav | 1 | 1 | 100.00% | 0.6757 |
| extra_membo_D2_2013.wav | 1 | 1 | 100.00% | 0.6246 |
| extra_membo_D2_2014.wav | 1 | 1 | 100.00% | 0.3921 |
| extra_membo_D2_2015.wav | 1 | 1 | 100.00% | 1.0000 |
| extra_membo_D2_2016.wav | 1 | 1 | 100.00% | 1.0533 |
| extra_membo_D2_2017.wav | 1 | 0 | **0.00%** ❌ | -0.1661 |
| extra_membo_D2_2018.wav | 1 | 1 | 100.00% | 0.1280 |
| extra_membo_D2_2019.wav | 1 | 1 | 100.00% | 1.0628 |
| extra_membo_D2_2020.wav | 1 | 1 | 100.00% | 1.2701 |
| extra_membo_D2_2021.wav | 1 | 1 | 100.00% | 0.8962 |
| extra_membo_D2_2022.wav | 1 | 1 | 100.00% | 0.2902 |
| extra_membo_D2_2023.wav | 1 | 1 | 100.00% | 0.2373 |
| extra_membo_D2_2024.wav | 1 | 1 | 100.00% | 0.3185 |
| extra_membo_D2_2025.wav | 1 | 1 | 100.00% | 1.0143 |
| extra_membo_D2_2026.wav | 1 | 1 | 100.00% | 1.3879 |
| extra_membo_D2_2027.wav | 1 | 1 | 100.00% | 1.1845 |
| extra_membo_D2_2028.wav | 1 | 1 | 100.00% | 0.6156 |
| extra_membo_D2_2029.wav | 1 | 1 | 100.00% | 0.4894 |
| extra_membo_D2_2030.wav | 1 | 0 | **0.00%** ❌ | -0.0682 |
| extra_membo_D2_2031.wav | 1 | 1 | 100.00% | 0.7933 |
| extra_membo_D2_2032.wav | 1 | 1 | 100.00% | 0.0557 |
| extra_membo_D2_2033.wav | 1 | 1 | 100.00% | 0.4733 |
| extra_membo_D2_2034.wav | 1 | 1 | 100.00% | 0.4349 |
| extra_membo_D2_2035.wav | 1 | 1 | 100.00% | 0.2210 |
| extra_membo_D2_2036.wav | 1 | 1 | 100.00% | 0.4803 |
| extra_membo_D2_2037.wav | 1 | 1 | 100.00% | 0.4095 |
| extra_membo_D2_2038.wav | 1 | 1 | 100.00% | 0.1276 |
| extra_membo_D2_2039.wav | 1 | 1 | 100.00% | 0.2145 |
| extra_membo_D2_2040.wav | 1 | 1 | 100.00% | 0.3455 |
| extra_membo_D2_2041.wav | 1 | 1 | 100.00% | 0.7575 |
| extra_membo_D2_2042.wav | 1 | 1 | 100.00% | 0.1257 |
| extra_membo_D2_2043.wav | 1 | 1 | 100.00% | 0.3287 |
| extra_membo_D2_2044.wav | 1 | 0 | **0.00%** ❌ | -0.1697 |
| extra_membo_D2_2045.wav | 1 | 0 | **0.00%** ❌ | -0.4296 |
| extra_membo_D2_2046.wav | 1 | 0 | **0.00%** ❌ | -0.2131 |
| extra_membo_D2_2047.wav | 1 | 0 | **0.00%** ❌ | -0.0247 |
| extra_membo_D2_2048.wav | 1 | 1 | 100.00% | 0.0498 |
| extra_membo_D2_2049.wav | 1 | 1 | 100.00% | 0.5514 |
| extra_membo_D2_2050.wav | 1 | 1 | 100.00% | 0.2674 |
| extra_membo_D2_2051.wav | 1 | 0 | **0.00%** ❌ | -0.4667 |
| extra_membo_D2_2052.wav | 1 | 0 | **0.00%** ❌ | -0.0158 |
| extra_membo_D2_2053.wav | 1 | 1 | 100.00% | 0.4430 |
| extra_membo_D2_2054.wav | 1 | 1 | 100.00% | 0.7742 |
| extra_membo_D2_2055.wav | 1 | 1 | 100.00% | 0.8137 |
| mixed_0 | 5 | 4 | **80.00%** ❌ | 0.8430 |
| mixed_1 | 5 | 1 | **20.00%** ❌ | -0.5143 |
| mixed_10 | 5 | 5 | 100.00% | 0.4558 |
| mixed_11 | 5 | 5 | 100.00% | 1.4449 |
| mixed_12 | 5 | 5 | 100.00% | 1.7464 |
| mixed_13 | 5 | 5 | 100.00% | 1.0186 |
| mixed_14 | 5 | 5 | 100.00% | 1.6513 |
| mixed_15 | 5 | 5 | 100.00% | 1.4963 |
| mixed_16 | 5 | 4 | **80.00%** ❌ | 0.6243 |
| mixed_17 | 5 | 5 | 100.00% | 1.3188 |
| mixed_18 | 5 | 5 | 100.00% | 1.2317 |
| mixed_19 | 5 | 3 | **60.00%** ❌ | 0.7906 |
| mixed_2 | 5 | 5 | 100.00% | 1.1766 |
| mixed_20 | 5 | 5 | 100.00% | 1.4050 |
| mixed_21 | 5 | 5 | 100.00% | 0.9828 |
| mixed_22 | 5 | 2 | **40.00%** ❌ | -0.1749 |
| mixed_23 | 5 | 5 | 100.00% | 1.7351 |
| mixed_24 | 5 | 5 | 100.00% | 0.9478 |
| mixed_25 | 5 | 5 | 100.00% | 1.4762 |
| mixed_26 | 5 | 5 | 100.00% | 1.9724 |
| mixed_27 | 5 | 5 | 100.00% | 0.9487 |
| mixed_28 | 5 | 5 | 100.00% | 1.2203 |
| mixed_29 | 5 | 2 | **40.00%** ❌ | -0.0076 |
| mixed_3 | 5 | 4 | **80.00%** ❌ | 0.0512 |
| mixed_30 | 5 | 1 | **20.00%** ❌ | -0.2973 |
| mixed_31 | 5 | 3 | **60.00%** ❌ | 0.2409 |
| mixed_32 | 5 | 0 | **0.00%** ❌ | -0.6411 |
| mixed_33 | 5 | 5 | 100.00% | 1.0676 |
| mixed_34 | 5 | 3 | **60.00%** ❌ | 0.0894 |
| mixed_35 | 5 | 1 | **20.00%** ❌ | -0.5439 |
| mixed_36 | 5 | 4 | **80.00%** ❌ | 0.3368 |
| mixed_37 | 5 | 5 | 100.00% | 0.5902 |
| mixed_38 | 5 | 5 | 100.00% | 0.8177 |
| mixed_39 | 5 | 5 | 100.00% | 0.7936 |
| mixed_4 | 5 | 5 | 100.00% | 0.9369 |
| mixed_40 | 5 | 5 | 100.00% | 0.9180 |
| mixed_41 | 5 | 4 | **80.00%** ❌ | 0.3108 |
| mixed_42 | 5 | 5 | 100.00% | 1.0937 |
| mixed_43 | 5 | 1 | **20.00%** ❌ | 0.0963 |
| mixed_44 | 5 | 5 | 100.00% | 0.7477 |
| mixed_45 | 5 | 5 | 100.00% | 1.0201 |
| mixed_46 | 5 | 5 | 100.00% | 1.7421 |
| mixed_47 | 5 | 4 | **80.00%** ❌ | 0.1846 |
| mixed_48 | 5 | 5 | 100.00% | 1.2002 |
| mixed_49 | 5 | 2 | **40.00%** ❌ | -0.1465 |
| mixed_5 | 5 | 4 | **80.00%** ❌ | 0.1880 |
| mixed_50 | 5 | 4 | **80.00%** ❌ | 0.5581 |
| mixed_51 | 5 | 5 | 100.00% | 0.9438 |
| mixed_52 | 5 | 5 | 100.00% | 0.9894 |
| mixed_53 | 5 | 5 | 100.00% | 1.1609 |
| mixed_54 | 5 | 5 | 100.00% | 0.7921 |
| mixed_55 | 5 | 3 | **60.00%** ❌ | 0.1689 |
| mixed_56 | 5 | 5 | 100.00% | 0.8862 |
| mixed_57 | 5 | 5 | 100.00% | 1.4980 |
| mixed_58 | 5 | 5 | 100.00% | 1.1216 |
| mixed_59 | 5 | 5 | 100.00% | 1.2461 |
| mixed_6 | 5 | 5 | 100.00% | 1.2190 |
| mixed_60 | 5 | 2 | **40.00%** ❌ | 0.0967 |
| mixed_61 | 5 | 2 | **40.00%** ❌ | -0.3163 |
| mixed_62 | 5 | 5 | 100.00% | 1.6536 |
| mixed_63 | 5 | 5 | 100.00% | 1.3428 |
| mixed_64 | 5 | 4 | **80.00%** ❌ | 0.3967 |
| mixed_65 | 5 | 5 | 100.00% | 0.7672 |
| mixed_66 | 5 | 5 | 100.00% | 1.1300 |
| mixed_7 | 5 | 5 | 100.00% | 1.1619 |
| mixed_8 | 5 | 5 | 100.00% | 1.4672 |
| mixed_9 | 5 | 3 | **60.00%** ❌ | 0.0333 |
| mixed_membo_0 | 5 | 3 | **60.00%** ❌ | 0.7904 |
| mixed_membo_1 | 5 | 0 | **0.00%** ❌ | -1.4711 |
| mixed_membo_10 | 5 | 0 | **0.00%** ❌ | -0.7188 |
| mixed_membo_11 | 5 | 5 | 100.00% | 1.0806 |
| mixed_membo_12 | 5 | 5 | 100.00% | 1.2684 |
| mixed_membo_13 | 5 | 5 | 100.00% | 0.5758 |
| mixed_membo_14 | 5 | 5 | 100.00% | 1.0263 |
| mixed_membo_15 | 5 | 3 | **60.00%** ❌ | 0.3511 |
| mixed_membo_16 | 5 | 2 | **40.00%** ❌ | 0.4657 |
| mixed_membo_17 | 5 | 2 | **40.00%** ❌ | -2.3610 |
| mixed_membo_18 | 5 | 5 | 100.00% | 1.0508 |
| mixed_membo_19 | 5 | 3 | **60.00%** ❌ | 0.9474 |
| mixed_membo_2 | 5 | 4 | **80.00%** ❌ | 0.3085 |
| mixed_membo_20 | 5 | 5 | 100.00% | 2.0312 |
| mixed_membo_21 | 5 | 4 | **80.00%** ❌ | 0.0407 |
| mixed_membo_22 | 5 | 2 | **40.00%** ❌ | 0.3060 |
| mixed_membo_23 | 5 | 5 | 100.00% | 1.3253 |
| mixed_membo_24 | 5 | 5 | 100.00% | 0.7862 |
| mixed_membo_25 | 5 | 5 | 100.00% | 1.0355 |
| mixed_membo_26 | 5 | 5 | 100.00% | 1.4012 |
| mixed_membo_27 | 5 | 5 | 100.00% | 0.6135 |
| mixed_membo_28 | 5 | 5 | 100.00% | 0.6437 |
| mixed_membo_29 | 5 | 1 | **20.00%** ❌ | -0.4529 |
| mixed_membo_3 | 5 | 4 | **80.00%** ❌ | 1.3080 |
| mixed_membo_30 | 5 | 0 | **0.00%** ❌ | -1.3341 |
| mixed_membo_31 | 5 | 1 | **20.00%** ❌ | -0.5517 |
| mixed_membo_32 | 5 | 0 | **0.00%** ❌ | -1.5315 |
| mixed_membo_33 | 5 | 5 | 100.00% | 1.0449 |
| mixed_membo_34 | 5 | 0 | **0.00%** ❌ | -0.7219 |
| mixed_membo_35 | 5 | 0 | **0.00%** ❌ | -1.2639 |
| mixed_membo_36 | 5 | 1 | **20.00%** ❌ | -0.5093 |
| mixed_membo_37 | 5 | 5 | 100.00% | 1.7266 |
| mixed_membo_38 | 5 | 5 | 100.00% | 2.2628 |
| mixed_membo_39 | 5 | 5 | 100.00% | 1.4871 |
| mixed_membo_4 | 5 | 5 | 100.00% | 0.7660 |
| mixed_membo_40 | 5 | 5 | 100.00% | 0.8056 |
| mixed_membo_41 | 5 | 0 | **0.00%** ❌ | -0.7222 |
| mixed_membo_42 | 5 | 5 | 100.00% | 2.1442 |
| mixed_membo_43 | 5 | 0 | **0.00%** ❌ | -1.0667 |
| mixed_membo_44 | 5 | 4 | **80.00%** ❌ | 0.5237 |
| mixed_membo_45 | 5 | 5 | 100.00% | 1.6537 |
| mixed_membo_46 | 5 | 5 | 100.00% | 1.0323 |
| mixed_membo_47 | 5 | 1 | **20.00%** ❌ | -0.2814 |
| mixed_membo_48 | 5 | 5 | 100.00% | 2.0922 |
| mixed_membo_49 | 5 | 0 | **0.00%** ❌ | -9.0604 |
| mixed_membo_5 | 5 | 3 | **60.00%** ❌ | 0.2587 |
| mixed_membo_50 | 5 | 3 | **60.00%** ❌ | 0.3507 |
| mixed_membo_51 | 5 | 3 | **60.00%** ❌ | 0.2066 |
| mixed_membo_52 | 5 | 4 | **80.00%** ❌ | 0.5102 |
| mixed_membo_53 | 5 | 1 | **20.00%** ❌ | -0.1579 |
| mixed_membo_54 | 5 | 0 | **0.00%** ❌ | -0.8544 |
| mixed_membo_55 | 5 | 1 | **20.00%** ❌ | -0.3712 |
| mixed_membo_56 | 5 | 0 | **0.00%** ❌ | -0.4101 |
| mixed_membo_57 | 5 | 5 | 100.00% | 1.1792 |
| mixed_membo_58 | 5 | 5 | 100.00% | 2.3032 |
| mixed_membo_59 | 5 | 5 | 100.00% | 1.4152 |
| mixed_membo_6 | 5 | 5 | 100.00% | 1.4283 |
| mixed_membo_60 | 5 | 1 | **20.00%** ❌ | -0.9893 |
| mixed_membo_61 | 5 | 1 | **20.00%** ❌ | -0.8840 |
| mixed_membo_62 | 5 | 4 | **80.00%** ❌ | 0.6713 |
| mixed_membo_63 | 5 | 5 | 100.00% | 0.9213 |
| mixed_membo_64 | 5 | 5 | 100.00% | 1.9003 |
| mixed_membo_65 | 5 | 4 | **80.00%** ❌ | 1.5139 |
| mixed_membo_66 | 5 | 5 | 100.00% | 0.5968 |
| mixed_membo_7 | 5 | 5 | 100.00% | 0.6849 |
| mixed_membo_8 | 5 | 5 | 100.00% | 1.0622 |
| mixed_membo_9 | 5 | 3 | **60.00%** ❌ | 0.1008 |

### 4. Seznam nezachycených dronů (False Negatives)
Seznam konkrétních nahrávek s drony, které model vyhodnotil jako šum pozadí.

| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |
|:---|:---|:---:|:---:|:---:|
| Parquet_Drone_1068 | Parquet Drone | DRONE (1) | NOISE (0) | -0.1695 |
| B_S2_D1_114-bebop_004_.wav | Bebop Drone (Clean) | DRONE (1) | NOISE (0) | -0.3519 |
| extra_membo_D2_2000.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -3.0367 |
| extra_membo_D2_2001.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -1.0946 |
| extra_membo_D2_2002.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.0535 |
| extra_membo_D2_2007.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -1.0554 |
| extra_membo_D2_2008.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -1.0865 |
| extra_membo_D2_2017.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.1661 |
| extra_membo_D2_2030.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.0682 |
| extra_membo_D2_2044.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.1697 |
| extra_membo_D2_2045.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.4296 |
| extra_membo_D2_2046.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.2131 |
| extra_membo_D2_2047.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.0247 |
| extra_membo_D2_2051.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.4667 |
| extra_membo_D2_2052.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.0158 |
| Membo_0_000-membo_000_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -1.1210 |
| Membo_1_000-membo_000_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -3.8348 |
| Membo_2_027-membo_004_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -1.4443 |
| Membo_2_028-membo_001_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.3067 |
| mixed_0-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0018 |
| mixed_1-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2948 |
| mixed_1-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6778 |
| mixed_1-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6093 |
| mixed_1-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1905 |
| mixed_16-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1814 |
| mixed_19-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3820 |
| mixed_19-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4243 |
| mixed_22-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1998 |
| mixed_22-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3197 |
| mixed_22-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9247 |
| mixed_29-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2766 |
| mixed_29-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5394 |
| mixed_29-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2337 |
| mixed_3-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.6342 |
| mixed_30-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2611 |
| mixed_30-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0421 |
| mixed_30-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7443 |
| mixed_30-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4969 |
| mixed_31-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0529 |
| mixed_31-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2858 |
| mixed_32-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5501 |
| mixed_32-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2605 |
| mixed_32-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9162 |
| mixed_32-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9207 |
| mixed_32-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5578 |
| mixed_34-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2226 |
| mixed_34-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5800 |
| mixed_35-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7056 |
| mixed_35-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8316 |
| mixed_35-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5627 |
| mixed_35-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8878 |
| mixed_36-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5991 |
| mixed_41-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1270 |
| mixed_43-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2032 |
| mixed_43-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0283 |
| mixed_43-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1848 |
| mixed_43-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0754 |
| mixed_47-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0890 |
| mixed_49-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2512 |
| mixed_49-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6637 |
| mixed_49-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2236 |
| mixed_5-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8913 |
| mixed_50-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1942 |
| mixed_55-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4874 |
| mixed_55-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2301 |
| mixed_60-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5047 |
| mixed_60-bebop_003_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2288 |
| mixed_60-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9053 |
| mixed_61-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1233 |
| mixed_61-bebop_002_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2398 |
| mixed_61-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7984 |
| mixed_64-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1695 |
| mixed_9-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.5812 |
| mixed_9-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -2.7982 |
| mixed_membo_0-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2084 |
| mixed_membo_0-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4679 |
| mixed_membo_1-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3974 |
| mixed_membo_1-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0327 |
| mixed_membo_1-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.8548 |
| mixed_membo_1-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0167 |
| mixed_membo_1-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -3.0541 |
| mixed_membo_10-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3888 |
| mixed_membo_10-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8074 |
| mixed_membo_10-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5659 |
| mixed_membo_10-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9055 |
| mixed_membo_10-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9267 |
| mixed_membo_15-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.4312 |
| mixed_membo_15-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3868 |
| mixed_membo_16-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8929 |
| mixed_membo_16-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3766 |
| mixed_membo_16-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1272 |
| mixed_membo_17-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -7.7994 |
| mixed_membo_17-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -3.1516 |
| mixed_membo_17-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -3.0515 |
| mixed_membo_19-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.8155 |
| mixed_membo_19-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.9130 |
| mixed_membo_2-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8513 |
| mixed_membo_21-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0222 |
| mixed_membo_22-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6665 |
| mixed_membo_22-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.5199 |
| mixed_membo_22-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1978 |
| mixed_membo_29-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0906 |
| mixed_membo_29-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8166 |
| mixed_membo_29-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1168 |
| mixed_membo_29-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7428 |
| mixed_membo_3-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -2.4338 |
| mixed_membo_30-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0348 |
| mixed_membo_30-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0059 |
| mixed_membo_30-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8240 |
| mixed_membo_30-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.6702 |
| mixed_membo_30-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -2.1355 |
| mixed_membo_31-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3172 |
| mixed_membo_31-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1139 |
| mixed_membo_31-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7855 |
| mixed_membo_31-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0035 |
| mixed_membo_32-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3711 |
| mixed_membo_32-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3205 |
| mixed_membo_32-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.9580 |
| mixed_membo_32-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.7722 |
| mixed_membo_32-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.2354 |
| mixed_membo_34-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2648 |
| mixed_membo_34-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1010 |
| mixed_membo_34-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5430 |
| mixed_membo_34-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1444 |
| mixed_membo_34-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.5563 |
| mixed_membo_35-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.4705 |
| mixed_membo_35-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.5697 |
| mixed_membo_35-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.2994 |
| mixed_membo_35-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.7004 |
| mixed_membo_35-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2796 |
| mixed_membo_36-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1981 |
| mixed_membo_36-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4751 |
| mixed_membo_36-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1854 |
| mixed_membo_36-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7011 |
| mixed_membo_41-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2583 |
| mixed_membo_41-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1093 |
| mixed_membo_41-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.4346 |
| mixed_membo_41-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3462 |
| mixed_membo_41-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4625 |
| mixed_membo_43-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.8537 |
| mixed_membo_43-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0037 |
| mixed_membo_43-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0579 |
| mixed_membo_43-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3737 |
| mixed_membo_43-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0442 |
| mixed_membo_44-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0045 |
| mixed_membo_47-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1211 |
| mixed_membo_47-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4635 |
| mixed_membo_47-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5313 |
| mixed_membo_47-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4437 |
| mixed_membo_49-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -8.8960 |
| mixed_membo_49-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -7.3883 |
| mixed_membo_49-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -10.0146 |
| mixed_membo_49-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -9.4078 |
| mixed_membo_49-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -9.5951 |
| mixed_membo_5-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2359 |
| mixed_membo_5-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4187 |
| mixed_membo_50-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8945 |
| mixed_membo_50-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6145 |
| mixed_membo_51-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2326 |
| mixed_membo_51-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2126 |
| mixed_membo_52-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2632 |
| mixed_membo_53-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0437 |
| mixed_membo_53-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0433 |
| mixed_membo_53-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3179 |
| mixed_membo_53-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4731 |
| mixed_membo_54-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8806 |
| mixed_membo_54-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5105 |
| mixed_membo_54-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8374 |
| mixed_membo_54-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1354 |
| mixed_membo_54-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9080 |
| mixed_membo_55-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3600 |
| mixed_membo_55-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4280 |
| mixed_membo_55-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5046 |
| mixed_membo_55-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6619 |
| mixed_membo_56-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0078 |
| mixed_membo_56-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1356 |
| mixed_membo_56-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8694 |
| mixed_membo_56-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2810 |
| mixed_membo_56-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7568 |
| mixed_membo_60-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7156 |
| mixed_membo_60-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3404 |
| mixed_membo_60-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3146 |
| mixed_membo_60-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -2.3764 |
| mixed_membo_61-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.9049 |
| mixed_membo_61-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5258 |
| mixed_membo_61-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.1122 |
| mixed_membo_61-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.9348 |
| mixed_membo_62-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1456 |
| mixed_membo_65-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3278 |
| mixed_membo_9-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -2.5676 |
| mixed_membo_9-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -3.4229 |

### 5. Seznam falešných poplachů (False Positives)
Seznam konkrétních nahrávek šumu, které model vyhodnotil jako dron.

| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |
|:---|:---|:---:|:---:|:---:|
| Parquet_Noise_999 | Parquet Noise | NOISE (0) | DRONE (1) | 0.4972 |
| Parquet_Noise_1419 | Parquet Noise | NOISE (0) | DRONE (1) | 1.4004 |
| Parquet_Noise_1231 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5451 |
| Parquet_Noise_1023 | Parquet Noise | NOISE (0) | DRONE (1) | 2.3410 |
| Parquet_Noise_1228 | Parquet Noise | NOISE (0) | DRONE (1) | 0.7934 |
| 1-103298-A-92.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1000 |
| 1-11687-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6340 |
| 1-11687-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1909 |
| 1-11687-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7329 |
| 1-119125-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1917 |
| 1-137296-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6659 |
| 1-137296-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4420 |
| 1-137296-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3512 |
| 1-137296-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9577 |
| 1-137296-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3240 |
| 1-172649-A-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3434 |
| 1-172649-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6969 |
| 1-172649-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9442 |
| 1-172649-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5762 |
| 1-172649-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5724 |
| 1-172649-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9178 |
| 1-172649-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6047 |
| 1-172649-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0011 |
| 1-172649-B-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9597 |
| 1-172649-B-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7560 |
| 1-172649-C-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9799 |
| 1-172649-C-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8543 |
| 1-172649-C-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3833 |
| 1-172649-C-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9177 |
| 1-172649-C-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3250 |
| 1-172649-D-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5612 |
| 1-172649-D-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7750 |
| 1-172649-D-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8087 |
| 1-172649-D-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8316 |
| 1-172649-D-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9208 |
| 1-172649-E-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1329 |
| 1-172649-E-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1143 |
| 1-172649-E-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2642 |
| 1-172649-E-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0943 |
| 1-17367-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7867 |
| 1-17367-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7658 |
| 1-17367-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0886 |
| 1-17367-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9506 |
| 1-17367-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7201 |
| 1-19840-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6513 |
| 1-19840-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4033 |
| 1-19840-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6196 |
| 1-19840-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6296 |
| 1-19840-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3530 |
| 1-19872-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5873 |
| 1-19872-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7547 |
| 1-19872-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4374 |
| 1-19872-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6593 |
| 1-19872-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6446 |
| 1-19872-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8297 |
| 1-19872-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6587 |
| 1-19872-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9160 |
| 1-19872-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0112 |
| 1-19872-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8305 |
| 1-20545-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9479 |
| 1-21189-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0390 |
| 1-21189-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2341 |
| 1-21189-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3030 |
| 1-21896-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3308 |
| 1-21896-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7625 |
| 1-21896-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4263 |
| 1-21896-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0100 |
| 1-21896-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0455 |
| 1-23996-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0875 |
| 1-23996-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2369 |
| 1-23996-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1115 |
| 1-23996-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6589 |
| 1-23996-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5242 |
| 1-23996-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3842 |
| 1-23996-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2766 |
| 1-24796-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4516 |
| 1-24796-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6965 |
| 1-24796-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1675 |
| 1-24796-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4196 |
| 1-260640-A-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0135 |
| 1-260640-C-23.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3501 |
| 1-260640-C-24.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5011 |
| 1-26222-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0450 |
| 1-26222-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5537 |
| 1-26222-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6893 |
| 1-26222-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8043 |
| 1-26222-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7475 |
| 1-26806-A-11.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3019 |
| 1-27165-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0728 |
| 1-27165-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1398 |
| 1-27165-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2498 |
| 1-27165-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4249 |
| 1-27166-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8195 |
| 1-27166-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1137 |
| 1-27166-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3968 |
| 1-27166-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9228 |
| 1-27166-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8494 |
| 1-29532-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1274 |
| 1-29532-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3742 |
| 1-29532-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1749 |
| 1-29532-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3260 |
| 1-29532-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0958 |
| 1-29561-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7444 |
| 1-29561-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1860 |
| 1-29561-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2484 |
| 1-32373-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1296 |
| 1-32373-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9593 |
| 1-32373-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2244 |
| 1-32373-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4499 |
| 1-32373-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3756 |
| 1-32373-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5118 |
| 1-32373-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1946 |
| 1-32373-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2704 |
| 1-32373-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2179 |
| 1-32373-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0982 |
| 1-34495-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1395 |
| 1-38559-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6204 |
| 1-38560-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3774 |
| 1-39937-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3192 |
| 1-39937-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2852 |
| 1-40621-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3025 |
| 1-46272-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1622 |
| 1-46272-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4293 |
| 1-46272-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9124 |
| 1-46272-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7812 |
| 1-46272-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4104 |
| 1-46274-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1338 |
| 1-46744-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6901 |
| 1-46744-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5741 |
| 1-46744-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5284 |
| 1-46744-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3365 |
| 1-47709-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3046 |
| 1-47709-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2197 |
| 1-50060-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0027 |
| 1-50060-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0905 |
| 1-50060-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2636 |
| 1-50060-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3001 |
| 1-50060-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0719 |
| 1-50661-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1905 |
| 1-50661-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1924 |
| 1-50661-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2495 |
| 1-50661-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2103 |
| 1-51805-G-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7803 |
| 1-51805-H-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0089 |
| 1-53444-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4241 |
| 1-53444-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4294 |
| 1-53670-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2432 |
| 1-53670-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2267 |
| 1-54918-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7239 |
| 1-54918-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8914 |
| 1-54918-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2392 |
| 1-54918-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1238 |
| 1-54918-B-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0282 |
| 1-54918-B-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1637 |
| 1-54918-B-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3663 |
| 1-54958-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8432 |
| 1-54958-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7957 |
| 1-54958-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7753 |
| 1-54958-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8811 |
| 1-54958-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7162 |
| 1-56311-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2371 |
| 1-56311-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4166 |
| 1-56311-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0518 |
| 1-57318-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5196 |
| 1-57318-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4868 |
| 1-57318-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2965 |
| 1-57318-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1497 |
| 1-57318-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5047 |
| 1-59102-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2597 |
| 1-59102-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0543 |
| 1-59102-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4311 |
| 1-59102-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0435 |
| 1-60460-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4181 |
| 1-60460-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2354 |
| 1-60460-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3733 |
| 1-60460-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1795 |
| 1-60460-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1547 |
| 1-61252-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4730 |
| 1-63871-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0612 |
| 1-63871-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0778 |
| 1-64398-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2173 |
| 1-64398-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7506 |
| 1-64398-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3797 |
| 1-64398-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1530 |
| 1-64473-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4528 |
| 1-64473-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2772 |
| 1-64473-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8704 |
| 1-64473-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2484 |
| 1-64473-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1926 |
| 1-67432-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7166 |
| 1-67432-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0445 |
| 1-68628-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4096 |
| 1-69760-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3774 |
| 1-69760-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0724 |
| 1-69760-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5216 |
| 1-69760-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6930 |
| 1-69760-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7651 |
| 1-73585-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1702 |
| 1-7456-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3719 |
| 1-7456-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4284 |
| 1-7456-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4269 |
| 1-7456-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4074 |
| 1-7456-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0964 |
| 1-79236-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4460 |
| 1-79236-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0944 |
| 1-79236-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6796 |
| 1-79236-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3008 |
| 1-80840-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1210 |
| 1-80840-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8551 |
| 1-80840-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1696 |
| 1-80840-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3267 |
| 1-80840-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3194 |
| 1-80841-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3739 |
| 1-80841-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5885 |
| 1-80841-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3411 |
| 1-80841-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2839 |
| 1-80841-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4216 |
| 1-85123-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0660 |
| 1-91359-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0311 |
| 1-9841-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1505 |
| 1-9841-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2471 |
| 1-9841-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5861 |
| 1-9841-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3283 |
| 1-9841-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0612 |
| 2-100648-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1393 |
| 2-100648-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7729 |
| 2-100648-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6196 |
| 2-101676-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9572 |
| 2-101676-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6257 |
| 2-101676-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2298 |
| 2-101676-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1782 |
| 2-101676-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1008 |
| 2-102567-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6687 |
| 2-102567-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7982 |
| 2-102567-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4699 |
| 2-102567-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6022 |
| 2-102567-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4118 |
| 2-102567-C-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5981 |
| 2-102567-C-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8465 |
| 2-102567-C-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3670 |
| 2-102567-C-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3363 |
| 2-102567-C-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2009 |
| 2-102567-D-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8669 |
| 2-102567-D-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6824 |
| 2-102567-D-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4476 |
| 2-102567-D-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3163 |
| 2-102567-D-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6872 |
| 2-106019-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9619 |
| 2-106019-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2146 |
| 2-106019-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0054 |
| 2-106019-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9965 |
| 2-106019-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0699 |
| 2-106072-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2875 |
| 2-106072-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3525 |
| 2-106072-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3166 |
| 2-106072-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4616 |
| 2-106073-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2006 |
| 2-106073-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2919 |
| 2-106487-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2173 |
| 2-106487-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1807 |
| 2-106487-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0843 |
| 2-112213-A-393.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2171 |
| 2-112213-B-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8209 |
| 2-117116-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4977 |
| 2-117116-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7707 |
| 2-117116-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2743 |
| 2-117116-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9688 |
| 2-117116-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2052 |
| 2-117625-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2097 |
| 2-117625-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3533 |
| 2-122066-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7058 |
| 2-122067-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4075 |
| 2-122616-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1796 |
| 2-122616-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6322 |
| 2-122616-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3077 |
| 2-122820-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3972 |
| 2-122820-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3142 |
| 2-122820-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3290 |
| 2-122820-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1653 |
| 2-122820-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4688 |
| 2-122820-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1080 |
| 2-122820-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1051 |
| 2-122820-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0750 |
| 2-122820-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8260 |
| 2-122820-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5776 |
| 2-135649-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3349 |
| 2-135649-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1400 |
| 2-135649-B-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0125 |
| 2-135649-B-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4054 |
| 2-135649-B-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2381 |
| 2-135649-B-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0807 |
| 2-135649-C-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0356 |
| 2-135649-C-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0887 |
| 2-135649-C-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1606 |
| 2-137162-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1244 |
| 2-138465-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0538 |
| 2-141682-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1854 |
| 2-141682-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4414 |
| 2-141682-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2603 |
| 2-141682-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2355 |
| 2-141682-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4956 |
| 2-160888-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5164 |
| 2-173618-A-390.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7257 |
| 2-18766-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4334 |
| 2-18766-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1786 |
| 2-18766-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1832 |
| 2-188822-D-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5574 |
| 2-188822-D-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3605 |
| 2-188822-D-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5560 |
| 2-188822-D-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9560 |
| 2-32515-B-44.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6134 |
| 2-37806-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4432 |
| 2-37806-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4506 |
| 2-37806-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1267 |
| 2-37806-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2186 |
| 2-50668-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0326 |
| 2-50668-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0303 |
| 2-50668-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8352 |
| 2-50668-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3655 |
| 2-50668-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5515 |
| 2-50668-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5872 |
| 2-50668-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8298 |
| 2-50668-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5926 |
| 2-52001-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1250 |
| 2-52001-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3953 |
| 2-52001-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3783 |
| 2-52001-B-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0936 |
| 2-59241-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8680 |
| 2-59241-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9077 |
| 2-59241-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0077 |
| 2-59241-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1582 |
| 2-59241-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8515 |
| 2-64332-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7476 |
| 2-64332-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4466 |
| 2-65484-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4650 |
| 2-65484-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2796 |
| 2-65484-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2102 |
| 2-65484-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0490 |
| 2-70367-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6291 |
| 2-72547-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4372 |
| 2-72547-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1254 |
| 2-72970-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3503 |
| 2-72970-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2416 |
| 2-72970-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2064 |
| 2-72970-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1672 |
| 2-73027-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2114 |
| 2-73027-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4074 |
| 2-73027-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4176 |
| 2-73027-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1640 |
| 2-73027-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3239 |
| 2-74977-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2822 |
| 2-74977-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3342 |
| 2-77945-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2318 |
| 2-77945-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2194 |
| 2-77945-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2939 |
| 2-77945-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3047 |
| 2-77945-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7763 |
| 2-77945-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4812 |
| 2-77945-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0433 |
| 2-77945-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0376 |
| 2-77945-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0201 |
| 2-78562-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4570 |
| 2-80313-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0059 |
| 2-80844-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8670 |
| 2-82367-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0333 |
| 2-83270-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0567 |
| 2-83270-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8440 |
| 2-83270-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3597 |
| 2-83270-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9960 |
| 2-83934-B-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1967 |
| 2-85139-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5920 |
| 2-85139-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2330 |
| 2-85139-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5472 |
| 2-85139-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3655 |
| 2-85139-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2486 |
| 2-87781-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4236 |
| 2-87781-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0463 |
| 2-87781-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4170 |
| 2-96033-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7716 |
| 2-96033-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7504 |
| 2-96033-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0716 |
| 2-96033-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2420 |
| 2-96033-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0712 |
| 3-100018-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5819 |
| 3-100018-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2686 |
| 3-104761-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1441 |
| 3-105235-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5469 |
| 3-108677-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2560 |
| 3-110913-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1973 |
| 3-115382-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1752 |
| 3-115382-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9377 |
| 3-115382-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2710 |
| 3-115382-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0841 |
| 3-115382-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4763 |
| 3-118656-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6265 |
| 3-118657-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0172 |
| 3-118657-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1258 |
| 3-118657-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4011 |
| 3-118657-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8208 |
| 3-118658-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8822 |
| 3-118658-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8774 |
| 3-118658-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7627 |
| 3-118972-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7107 |
| 3-118972-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7301 |
| 3-118972-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7343 |
| 3-118972-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0520 |
| 3-118972-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5568 |
| 3-119455-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5198 |
| 3-119455-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6027 |
| 3-119455-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1798 |
| 3-119455-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5862 |
| 3-119455-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6301 |
| 3-123086-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7775 |
| 3-124958-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2897 |
| 3-129338-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7305 |
| 3-129338-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1828 |
| 3-129338-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6165 |
| 3-129338-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3265 |
| 3-129678-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.3707 |
| 3-129678-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.6170 |
| 3-129678-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.2995 |
| 3-129678-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.6179 |
| 3-129678-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.4671 |
| 3-130998-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6143 |
| 3-130998-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2527 |
| 3-130998-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4414 |
| 3-130998-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1883 |
| 3-130998-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0646 |
| 3-132340-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4417 |
| 3-132340-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6457 |
| 3-132340-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9598 |
| 3-132340-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4862 |
| 3-132340-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4386 |
| 3-139958-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3427 |
| 3-139958-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5898 |
| 3-142005-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3361 |
| 3-142005-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3580 |
| 3-143929-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9143 |
| 3-143929-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0204 |
| 3-143929-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9616 |
| 3-143929-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3561 |
| 3-143929-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4390 |
| 3-147965-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2983 |
| 3-147965-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3052 |
| 3-147965-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1461 |
| 3-147965-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3670 |
| 3-151081-A-203.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3642 |
| 3-151557-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0729 |
| 3-152007-A-202.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7445 |
| 3-152020-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9544 |
| 3-152020-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5083 |
| 3-152020-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8780 |
| 3-152020-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5616 |
| 3-152020-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8578 |
| 3-152020-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6479 |
| 3-152020-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9412 |
| 3-152020-C-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1630 |
| 3-152020-C-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1579 |
| 3-152039-B-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6430 |
| 3-155578-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8223 |
| 3-155579-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1541 |
| 3-155579-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8770 |
| 3-155579-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4077 |
| 3-156391-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1420 |
| 3-156393-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1602 |
| 3-156393-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1959 |
| 3-156581-B-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1887 |
| 3-156581-B-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2811 |
| 3-156581-B-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0863 |
| 3-157149-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0389 |
| 3-157149-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1174 |
| 3-157187-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3749 |
| 3-157615-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7233 |
| 3-157615-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6849 |
| 3-157615-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4614 |
| 3-157615-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6711 |
| 3-157615-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7692 |
| 3-159346-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5810 |
| 3-159346-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6863 |
| 3-159346-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1740 |
| 3-159346-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7252 |
| 3-159346-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7814 |
| 3-159347-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4065 |
| 3-159347-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2087 |
| 3-159347-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2799 |
| 3-159347-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9152 |
| 3-159347-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8320 |
| 3-159347-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9193 |
| 3-161010-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8794 |
| 3-161010-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4645 |
| 3-161010-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6531 |
| 3-161010-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1803 |
| 3-164120-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1589 |
| 3-166422-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1483 |
| 3-166422-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2988 |
| 3-182023-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0952 |
| 3-259622-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0072 |
| 3-259622-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1654 |
| 3-259622-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3123 |
| 3-65748-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4664 |
| 3-65748-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3465 |
| 3-93010-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6480 |
| 3-93010-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1526 |
| 3-94355-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3263 |
| 4-119720-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4254 |
| 4-126532-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4190 |
| 4-126532-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9989 |
| 4-126532-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0806 |
| 4-126532-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6204 |
| 4-135439-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2643 |
| 4-135439-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6263 |
| 4-135439-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0398 |
| 4-141365-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3193 |
| 4-145006-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7080 |
| 4-145006-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3757 |
| 4-145006-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9801 |
| 4-146200-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1783 |
| 4-146200-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6964 |
| 4-146200-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8253 |
| 4-146200-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9411 |
| 4-146200-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0846 |
| 4-149294-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0091 |
| 4-149294-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2489 |
| 4-161100-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1990 |
| 4-161100-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0516 |
| 4-161127-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7927 |
| 4-161127-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9168 |
| 4-161127-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6074 |
| 4-161127-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9728 |
| 4-161127-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1002 |
| 4-163264-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3057 |
| 4-163264-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6182 |
| 4-163264-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9481 |
| 4-163264-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1609 |
| 4-163264-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2663 |
| 4-163697-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3359 |
| 4-163697-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0779 |
| 4-163697-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3212 |
| 4-163697-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9348 |
| 4-163697-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8214 |
| 4-164206-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1976 |
| 4-164206-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3619 |
| 4-164206-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0588 |
| 4-164661-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0563 |
| 4-164661-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0544 |
| 4-164661-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5470 |
| 4-164661-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0939 |
| 4-169127-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5196 |
| 4-169127-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3020 |
| 4-169127-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3525 |
| 4-169127-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5720 |
| 4-171652-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0828 |
| 4-172733-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1565 |
| 4-172733-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2384 |
| 4-172733-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1084 |
| 4-172733-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1125 |
| 4-172733-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1560 |
| 4-177250-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7967 |
| 4-177250-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0528 |
| 4-177250-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3947 |
| 4-177250-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6308 |
| 4-177250-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6034 |
| 4-181286-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3800 |
| 4-181286-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7851 |
| 4-181286-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6042 |
| 4-181286-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6730 |
| 4-181286-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7072 |
| 4-181999-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5215 |
| 4-181999-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3231 |
| 4-181999-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5194 |
| 4-181999-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9028 |
| 4-181999-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8171 |
| 4-182613-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1909 |
| 4-182613-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0344 |
| 4-182613-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2275 |
| 4-182613-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2323 |
| 4-183882-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8006 |
| 4-183882-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2529 |
| 4-183882-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3399 |
| 4-186940-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0403 |
| 4-186940-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0840 |
| 4-186962-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1106 |
| 4-186962-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1190 |
| 4-186962-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6970 |
| 4-186962-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1900 |
| 4-186962-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8504 |
| 4-191015-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5679 |
| 4-195451-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3043 |
| 4-195451-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2994 |
| 4-195451-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6520 |
| 4-195451-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2065 |
| 4-195707-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2913 |
| 4-195707-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0668 |
| 4-195707-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8579 |
| 4-195707-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7930 |
| 4-195707-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4649 |
| 4-195805-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1231 |
| 4-195805-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0934 |
| 4-195805-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7154 |
| 4-195805-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0497 |
| 4-195805-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4529 |
| 4-196672-A-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5248 |
| 4-196672-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8485 |
| 4-196672-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4611 |
| 4-198841-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9511 |
| 4-201988-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2250 |
| 4-201988-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3550 |
| 4-202749-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0710 |
| 4-202749-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9029 |
| 4-202749-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1348 |
| 4-202749-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2744 |
| 4-202749-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6475 |
| 4-209536-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3521 |
| 4-209536-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1717 |
| 4-209536-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1527 |
| 4-209536-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2670 |
| 4-216211-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1479 |
| 4-218199-H-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2148 |
| 4-218199-H-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5039 |
| 4-218199-H-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9779 |
| 4-218199-H-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3393 |
| 4-218199-H-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8455 |
| 4-223125-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6640 |
| 4-223125-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4193 |
| 4-223125-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5868 |
| 4-223125-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4780 |
| 4-223125-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0314 |
| 4-250869-A-24.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1564 |
| 4-250869-C-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7100 |
| 4-251959-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5071 |
| 5-117120-A-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6034 |
| 5-133989-A-422.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3538 |
| 5-133989-A-423.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7084 |
| 5-133989-A-424.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8821 |
| 5-133989-B-420.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5610 |
| 5-133989-B-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8688 |
| 5-133989-B-422.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2200 |
| 5-161270-B-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7178 |
| 5-170338-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7628 |
| 5-170338-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4390 |
| 5-170338-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5248 |
| 5-170338-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0082 |
| 5-170338-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3715 |
| 5-170338-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1724 |
| 5-171653-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4471 |
| 5-171653-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2508 |
| 5-177957-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3262 |
| 5-177957-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1927 |
| 5-177957-B-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1694 |
| 5-182007-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0473 |
| 5-182007-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1392 |
| 5-182007-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0773 |
| 5-182007-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0018 |
| 5-182007-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1419 |
| 5-182010-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6587 |
| 5-182010-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6254 |
| 5-182010-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7072 |
| 5-182010-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4394 |
| 5-182010-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7228 |
| 5-182012-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6689 |
| 5-182012-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0328 |
| 5-182012-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2537 |
| 5-182012-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2319 |
| 5-182012-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3129 |
| 5-185516-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3557 |
| 5-185516-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2109 |
| 5-185516-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7287 |
| 5-185516-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1858 |
| 5-185579-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8905 |
| 5-185579-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5264 |
| 5-185579-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5132 |
| 5-185579-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7300 |
| 5-185579-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3351 |
| 5-185579-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0476 |
| 5-186924-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3223 |
| 5-186924-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8436 |
| 5-186924-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7802 |
| 5-186924-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7894 |
| 5-186924-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8546 |
| 5-188365-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0355 |
| 5-188365-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0792 |
| 5-188365-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0605 |
| 5-188365-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1082 |
| 5-188655-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3194 |
| 5-188655-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2432 |
| 5-188655-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7385 |
| 5-188655-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7709 |
| 5-188655-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3278 |
| 5-193339-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9873 |
| 5-193339-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4299 |
| 5-193339-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5509 |
| 5-193339-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4542 |
| 5-193339-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3373 |
| 5-194892-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0902 |
| 5-194892-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1537 |
| 5-194892-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1098 |
| 5-194892-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4171 |
| 5-194892-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1968 |
| 5-195518-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0967 |
| 5-195518-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4738 |
| 5-195710-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0285 |
| 5-195710-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2722 |
| 5-198321-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3353 |
| 5-198321-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3800 |
| 5-198321-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2871 |
| 5-198321-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0528 |
| 5-198321-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4068 |
| 5-199284-B-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0719 |
| 5-201664-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8837 |
| 5-201664-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1242 |
| 5-201664-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3383 |
| 5-202898-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7506 |
| 5-202898-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5315 |
| 5-202898-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6008 |
| 5-202898-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1770 |
| 5-202898-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0961 |
| 5-203739-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7494 |
| 5-203739-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3481 |
| 5-203739-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2680 |
| 5-203739-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2667 |
| 5-203739-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0935 |
| 5-204352-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.3188 |
| 5-204352-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.0769 |
| 5-204352-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.1930 |
| 5-204352-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.3758 |
| 5-204352-B-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.7375 |
| 5-204352-B-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0271 |
| 5-204352-B-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.1209 |
| 5-204352-B-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.0250 |
| 5-207811-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9670 |
| 5-207811-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7933 |
| 5-207811-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6702 |
| 5-207811-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1434 |
| 5-207811-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2205 |
| 5-210540-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1360 |
| 5-210540-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5428 |
| 5-210540-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9952 |
| 5-210540-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2787 |
| 5-210540-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9977 |
| 5-211408-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2747 |
| 5-211408-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2117 |
| 5-211408-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2378 |
| 5-211408-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2217 |
| 5-211408-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0415 |
| 5-212059-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3297 |
| 5-212059-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4771 |
| 5-212059-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1845 |
| 5-212059-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2712 |
| 5-213077-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5163 |
| 5-213077-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0226 |
| 5-213077-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7586 |
| 5-213077-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0995 |
| 5-213077-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5979 |
| 5-213802-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9040 |
| 5-213802-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3212 |
| 5-213802-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8183 |
| 5-213802-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5336 |
| 5-213802-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2408 |
| 5-215172-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.7784 |
| 5-215172-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.6979 |
| 5-215172-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.9981 |
| 5-215172-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.4385 |
| 5-215172-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.5121 |
| 5-215658-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9438 |
| 5-215658-B-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7417 |
| 5-215658-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1983 |
| 5-215658-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4989 |
| 5-215658-B-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8202 |
| 5-216370-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2769 |
| 5-216370-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7510 |
| 5-216370-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6316 |
| 5-216370-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9826 |
| 5-219242-B-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.0106 |
| 5-238021-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2739 |
| 5-238021-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7660 |
| 5-238021-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8913 |
| 5-238021-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9964 |
| 5-238021-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7478 |
| 5-241846-A-150.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4941 |
| 5-243459-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0769 |
| 5-251957-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1753 |
| 5-251962-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5635 |
| 5-251963-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0654 |
| 5-251963-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1861 |
| 5-251971-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2035 |
| 5-251971-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2977 |
| 5-251971-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6079 |
| 5-257839-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4050 |
| 5-257839-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2282 |
| 5-257839-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1157 |
| 5-263902-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1405 |
| 5-263902-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7377 |
| 5-263902-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8308 |
| 5-263902-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5428 |
| 5-263902-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6403 |
| exercise_bike002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0447 |
| exercise_bike003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2063 |
| exercise_bike004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0409 |
| exercise_bike005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0039 |
| exercise_bike007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1323 |
| exercise_bike009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1707 |
| exercise_bike012.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2379 |
| pink_noise000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6533 |
| pink_noise001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5408 |
| pink_noise002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6248 |
| pink_noise003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5401 |
| pink_noise004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5147 |
| pink_noise005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5874 |
| pink_noise006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6621 |
| pink_noise007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5379 |
| pink_noise008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5781 |
| pink_noise009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5782 |
| pink_noise010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7253 |
| pink_noise011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5076 |
| running_tap000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5241 |
| running_tap001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7256 |
| running_tap002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0968 |
| running_tap003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7407 |
| running_tap004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1309 |
| running_tap005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5459 |
| running_tap006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8612 |
| running_tap007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9498 |
| running_tap008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1506 |
| running_tap009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9023 |
| running_tap010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0493 |
| running_tap011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5495 |
| running_tap012.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6461 |
| white_noise000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2160 |
| white_noise001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1755 |
| white_noise002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1762 |
| white_noise003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2638 |
| white_noise004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2506 |
| white_noise005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3040 |
| white_noise006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2409 |
| white_noise007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2990 |
| white_noise008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2712 |
| white_noise009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3957 |
| white_noise010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2124 |
| white_noise011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2766 |

## Výsledky pro metodu: Global RMS Normalized

### 1. Souhrnná tabulka dle kategorií

| Kategorie | Popis | Celkem vzorků | Správně | Úspěšnost | Průměrná jistota (Decision Value) |
|:---|:---|:---:|:---:|:---:|:---:|
| **Bebop Drone (Clean)** | Testovací vzorky z této skupiny | 331 | 331 | **100.00%** | 2.7022 |
| **Local Ambient Noise (ESC-50)** | Testovací vzorky z této skupiny | 10371 | 7875 | **75.93%** | -2.8618 |
| **Membo Drone (Clean)** | Testovací vzorky z této skupiny | 331 | 328 | **99.09%** | 4.2357 |
| **Mixed Bebop (Noise+Drone)** | Testovací vzorky z této skupiny | 335 | 331 | **98.81%** | 2.2457 |
| **Mixed Membo (Noise+Drone)** | Testovací vzorky z této skupiny | 335 | 304 | **90.75%** | 2.0024 |
| **Parquet Drone** | Testovací vzorky z této skupiny | 200 | 200 | **100.00%** | 2.4857 |
| **Parquet Noise** | Testovací vzorky z této skupiny | 200 | 137 | **68.50%** | -0.8417 |

### 2. Analýza falešných poplachů v ambientním šumu (ESC-50)
Tabulka ukazuje, které konkrétní typy okolních zvuků způsobují falešné detekce (Falešné poplachy, False Positives). U ideálního chování by měly mít všechny zvuky úspěšnost 100% (což znamená 0% FP) a záporný Decision Value.

| Třída zvuku | Třída ID | Celkem testováno | Správně odmítnuto | Falešné poplachy (FP Rate) | Průměrný Decision Value |
|:---|:---:|:---:|:---:|:---:|:---:|
| dog | 0 | 200 | 199 | 0.50% | -7.7314 |
| rooster | 1 | 200 | 191 | 4.50% | -7.3639 |
| pig | 2 | 200 | 189 | 5.50% | -3.5544 |
| cow | 3 | 200 | 183 | 8.50% | -4.2168 |
| frog | 4 | 200 | 192 | 4.00% | -3.4284 |
| cat | 5 | 200 | 167 | **16.50%** ⚠️ | -4.5320 |
| hen | 6 | 200 | 192 | 4.00% | -4.5173 |
| insects | 7 | 200 | 111 | **44.50%** ⚠️ | -0.7305 |
| sheep | 8 | 200 | 173 | **13.50%** ⚠️ | -1.9412 |
| crow | 9 | 200 | 182 | 9.00% | -3.3963 |
| rain | 10 | 200 | 14 | **93.00%** ⚠️ | 1.4703 |
| sea_waves | 11 | 200 | 106 | **47.00%** ⚠️ | -0.1644 |
| crackling_fire | 12 | 200 | 65 | **67.50%** ⚠️ | 0.2166 |
| crickets | 13 | 200 | 45 | **77.50%** ⚠️ | 1.7089 |
| chirping_birds | 14 | 200 | 87 | **56.50%** ⚠️ | -0.2178 |
| water_drops | 15 | 200 | 173 | **13.50%** ⚠️ | -3.5632 |
| wind | 16 | 200 | 135 | **32.50%** ⚠️ | -0.5219 |
| pouring_water | 17 | 200 | 166 | **17.00%** ⚠️ | -2.7577 |
| toilet_flush | 18 | 200 | 100 | **50.00%** ⚠️ | -0.7306 |
| thunderstorm | 19 | 200 | 165 | **17.50%** ⚠️ | -1.7671 |
| crying_baby | 20 | 200 | 196 | 2.00% | -5.6829 |
| sneezing | 21 | 200 | 192 | 4.00% | -7.5464 |
| clapping | 22 | 200 | 187 | 6.50% | -3.1095 |
| breathing | 23 | 200 | 187 | 6.50% | -3.7357 |
| coughing | 24 | 200 | 195 | 2.50% | -7.0955 |
| footsteps | 25 | 200 | 182 | 9.00% | -2.4355 |
| laughing | 26 | 200 | 193 | 3.50% | -5.5263 |
| brushing_teeth | 27 | 200 | 113 | **43.50%** ⚠️ | -0.7885 |
| snoring | 28 | 200 | 99 | **50.50%** ⚠️ | -0.5044 |
| drinking_sipping | 29 | 200 | 193 | 3.50% | -4.9741 |
| door_wood_creak | 30 | 200 | 187 | 6.50% | -5.8980 |
| dustbin | 31 | 200 | 169 | **15.50%** ⚠️ | -4.0495 |
| gas_on_stove | 32 | 200 | 186 | 7.00% | -2.4828 |
| washing_machine | 33 | 200 | 166 | **17.00%** ⚠️ | -3.6655 |
| vacuum_cleaner | 34 | 200 | 160 | **20.00%** ⚠️ | -4.6598 |
| clock_tick | 35 | 200 | 54 | **73.00%** ⚠️ | 0.6894 |
| clock_alarm | 36 | 200 | 41 | **79.50%** ⚠️ | 0.8213 |
| keyboard_typing | 37 | 200 | 137 | **31.50%** ⚠️ | -3.9378 |
| door_wood_knock | 38 | 200 | 191 | 4.50% | -4.5824 |
| glass_breaking | 39 | 200 | 189 | 5.50% | -6.1823 |
| helicopter | 40 | 200 | 125 | **37.50%** ⚠️ | -0.7527 |
| chainsaw | 41 | 200 | 88 | **56.00%** ⚠️ | -0.1725 |
| siren | 42 | 200 | 186 | 7.00% | -3.7769 |
| car_horn | 43 | 200 | 163 | **18.50%** ⚠️ | -3.5455 |
| engine | 44 | 200 | 84 | **58.00%** ⚠️ | -0.2623 |
| train | 45 | 200 | 146 | **27.00%** ⚠️ | -0.6385 |
| church_bells | 46 | 200 | 198 | 1.00% | -3.3846 |
| airplane | 47 | 200 | 156 | **22.00%** ⚠️ | -1.3386 |
| fireworks | 48 | 200 | 188 | 6.00% | -3.4737 |
| hand_saw | 49 | 200 | 180 | 10.00% | -2.8458 |
| unknown | 7100 | 1 | 1 | 0.00% | -1.1471 |

### 3. Analýza detekce dronů dle relací (Sessions)
Drony jsou seskupeny podle jednotlivých nahrávacích relací. Zde vidíme, zda model spolehlivě detekuje dron v průběhu celé nahrávky.

| Název relace | Celkem segmentů | Detekováno | Úspěšnost (Sensitivity) | Průměrná jistota (Decision Value) |
|:---|:---:|:---:|:---:|:---:|
| B_S2_D1_067 | 5 | 5 | 100.00% | 2.5026 |
| B_S2_D1_068 | 5 | 5 | 100.00% | 2.7200 |
| B_S2_D1_069 | 5 | 5 | 100.00% | 2.6656 |
| B_S2_D1_070 | 5 | 5 | 100.00% | 2.2555 |
| B_S2_D1_071 | 5 | 5 | 100.00% | 2.6471 |
| B_S2_D1_072 | 5 | 5 | 100.00% | 2.4083 |
| B_S2_D1_073 | 5 | 5 | 100.00% | 2.7201 |
| B_S2_D1_074 | 5 | 5 | 100.00% | 2.3697 |
| B_S2_D1_075 | 5 | 5 | 100.00% | 2.3172 |
| B_S2_D1_076 | 5 | 5 | 100.00% | 2.7326 |
| B_S2_D1_077 | 5 | 5 | 100.00% | 2.6437 |
| B_S2_D1_078 | 5 | 5 | 100.00% | 2.3671 |
| B_S2_D1_079 | 5 | 5 | 100.00% | 2.5445 |
| B_S2_D1_080 | 5 | 5 | 100.00% | 2.8737 |
| B_S2_D1_081 | 5 | 5 | 100.00% | 3.4138 |
| B_S2_D1_082 | 5 | 5 | 100.00% | 3.1838 |
| B_S2_D1_083 | 5 | 5 | 100.00% | 3.3238 |
| B_S2_D1_084 | 5 | 5 | 100.00% | 3.1057 |
| B_S2_D1_085 | 5 | 5 | 100.00% | 2.8791 |
| B_S2_D1_086 | 5 | 5 | 100.00% | 2.8104 |
| B_S2_D1_087 | 5 | 5 | 100.00% | 3.1344 |
| B_S2_D1_088 | 5 | 5 | 100.00% | 3.1165 |
| B_S2_D1_089 | 5 | 5 | 100.00% | 2.5934 |
| B_S2_D1_090 | 5 | 5 | 100.00% | 2.4866 |
| B_S2_D1_091 | 5 | 5 | 100.00% | 2.8392 |
| B_S2_D1_092 | 5 | 5 | 100.00% | 2.8620 |
| B_S2_D1_093 | 5 | 5 | 100.00% | 2.2503 |
| B_S2_D1_094 | 5 | 5 | 100.00% | 2.3028 |
| B_S2_D1_095 | 5 | 5 | 100.00% | 2.7762 |
| B_S2_D1_096 | 5 | 5 | 100.00% | 2.6851 |
| B_S2_D1_097 | 5 | 5 | 100.00% | 2.6400 |
| B_S2_D1_098 | 5 | 5 | 100.00% | 2.8697 |
| B_S2_D1_099 | 5 | 5 | 100.00% | 2.6257 |
| B_S2_D1_100 | 5 | 5 | 100.00% | 2.8701 |
| B_S2_D1_101 | 5 | 5 | 100.00% | 2.9186 |
| B_S2_D1_102 | 5 | 5 | 100.00% | 2.7816 |
| B_S2_D1_103 | 5 | 5 | 100.00% | 2.4139 |
| B_S2_D1_104 | 5 | 5 | 100.00% | 2.4640 |
| B_S2_D1_105 | 5 | 5 | 100.00% | 2.6032 |
| B_S2_D1_106 | 5 | 5 | 100.00% | 1.9903 |
| B_S2_D1_107 | 5 | 5 | 100.00% | 2.4444 |
| B_S2_D1_108 | 5 | 5 | 100.00% | 2.4636 |
| B_S2_D1_109 | 5 | 5 | 100.00% | 2.4569 |
| B_S2_D1_110 | 5 | 5 | 100.00% | 2.5729 |
| B_S2_D1_111 | 5 | 5 | 100.00% | 2.7653 |
| B_S2_D1_112 | 5 | 5 | 100.00% | 2.8922 |
| B_S2_D1_113 | 5 | 5 | 100.00% | 2.7626 |
| B_S2_D1_114 | 5 | 5 | 100.00% | 2.1143 |
| B_S2_D1_115 | 5 | 5 | 100.00% | 2.3049 |
| B_S2_D1_116 | 5 | 5 | 100.00% | 3.1963 |
| B_S2_D1_117 | 5 | 5 | 100.00% | 3.0992 |
| B_S2_D1_118 | 5 | 5 | 100.00% | 2.6814 |
| B_S2_D1_119 | 5 | 5 | 100.00% | 2.9087 |
| B_S2_D1_120 | 5 | 5 | 100.00% | 3.0903 |
| B_S2_D1_121 | 5 | 5 | 100.00% | 2.7284 |
| B_S2_D1_122 | 5 | 5 | 100.00% | 2.7245 |
| B_S2_D1_123 | 5 | 5 | 100.00% | 2.9667 |
| B_S2_D1_124 | 5 | 5 | 100.00% | 2.8687 |
| B_S2_D1_125 | 5 | 5 | 100.00% | 3.0253 |
| B_S2_D1_126 | 5 | 5 | 100.00% | 2.7504 |
| B_S2_D1_127 | 5 | 5 | 100.00% | 2.5782 |
| B_S2_D1_128 | 5 | 5 | 100.00% | 2.3975 |
| B_S2_D1_129 | 5 | 5 | 100.00% | 3.0164 |
| B_S2_D1_130 | 5 | 5 | 100.00% | 2.7245 |
| B_S2_D1_131 | 5 | 5 | 100.00% | 2.6184 |
| B_S2_D1_132 | 5 | 5 | 100.00% | 2.5731 |
| B_S2_D1_133 | 1 | 1 | 100.00% | 2.2740 |
| Membo_0_000 | 5 | 5 | 100.00% | 3.5807 |
| Membo_0_001 | 5 | 5 | 100.00% | 4.5759 |
| Membo_0_002 | 5 | 5 | 100.00% | 5.0598 |
| Membo_0_003 | 5 | 5 | 100.00% | 4.7325 |
| Membo_0_004 | 5 | 5 | 100.00% | 5.0443 |
| Membo_0_005 | 5 | 5 | 100.00% | 4.6769 |
| Membo_0_006 | 5 | 5 | 100.00% | 4.5491 |
| Membo_0_007 | 5 | 5 | 100.00% | 4.2802 |
| Membo_0_008 | 5 | 5 | 100.00% | 4.5446 |
| Membo_0_009 | 5 | 5 | 100.00% | 4.6764 |
| Membo_0_010 | 5 | 5 | 100.00% | 4.2640 |
| Membo_0_011 | 5 | 5 | 100.00% | 4.4509 |
| Membo_0_012 | 5 | 5 | 100.00% | 4.3502 |
| Membo_0_013 | 5 | 5 | 100.00% | 4.5605 |
| Membo_0_014 | 5 | 5 | 100.00% | 3.9827 |
| Membo_0_015 | 5 | 5 | 100.00% | 4.3040 |
| Membo_0_016 | 5 | 5 | 100.00% | 4.5404 |
| Membo_0_017 | 5 | 5 | 100.00% | 4.3535 |
| Membo_0_034 | 5 | 5 | 100.00% | 4.8284 |
| Membo_0_036 | 5 | 5 | 100.00% | 4.5819 |
| Membo_0_037 | 5 | 5 | 100.00% | 5.0107 |
| Membo_0_038 | 5 | 5 | 100.00% | 4.6264 |
| Membo_0_039 | 5 | 5 | 100.00% | 4.4330 |
| Membo_0_040 | 5 | 5 | 100.00% | 5.4118 |
| Membo_0_041 | 5 | 5 | 100.00% | 4.3170 |
| Membo_1_000 | 5 | 4 | **80.00%** ❌ | 2.8974 |
| Membo_1_001 | 5 | 5 | 100.00% | 4.3942 |
| Membo_1_002 | 5 | 5 | 100.00% | 5.1115 |
| Membo_1_004 | 5 | 5 | 100.00% | 4.8722 |
| Membo_1_029 | 5 | 5 | 100.00% | 4.8611 |
| Membo_1_030 | 5 | 5 | 100.00% | 5.3749 |
| Membo_1_031 | 5 | 5 | 100.00% | 5.2970 |
| Membo_1_032 | 5 | 5 | 100.00% | 5.0763 |
| Membo_1_033 | 5 | 5 | 100.00% | 4.7942 |
| Membo_1_034 | 5 | 5 | 100.00% | 4.8458 |
| Membo_1_041 | 5 | 5 | 100.00% | 4.2431 |
| Membo_1_042 | 5 | 5 | 100.00% | 4.7936 |
| Membo_1_043 | 5 | 5 | 100.00% | 4.9538 |
| Membo_1_044 | 5 | 5 | 100.00% | 4.4987 |
| Membo_1_045 | 5 | 5 | 100.00% | 4.4484 |
| Membo_1_046 | 5 | 5 | 100.00% | 3.9522 |
| Membo_1_047 | 5 | 5 | 100.00% | 3.9762 |
| Membo_1_048 | 5 | 5 | 100.00% | 4.0569 |
| Membo_2_010 | 5 | 5 | 100.00% | 5.0083 |
| Membo_2_011 | 5 | 5 | 100.00% | 4.3190 |
| Membo_2_013 | 5 | 5 | 100.00% | 4.9899 |
| Membo_2_014 | 5 | 5 | 100.00% | 4.8435 |
| Membo_2_015 | 5 | 5 | 100.00% | 4.4959 |
| Membo_2_016 | 5 | 5 | 100.00% | 4.2262 |
| Membo_2_017 | 5 | 5 | 100.00% | 4.8626 |
| Membo_2_018 | 5 | 5 | 100.00% | 4.6661 |
| Membo_2_019 | 5 | 5 | 100.00% | 4.8469 |
| Membo_2_020 | 5 | 5 | 100.00% | 5.3258 |
| Membo_2_027 | 5 | 4 | **80.00%** ❌ | 2.4227 |
| Membo_2_028 | 5 | 5 | 100.00% | 1.4786 |
| extra_membo_D2_2000.wav | 1 | 0 | **0.00%** ❌ | -0.4689 |
| extra_membo_D2_2001.wav | 1 | 1 | 100.00% | 1.5176 |
| extra_membo_D2_2002.wav | 1 | 1 | 100.00% | 2.5359 |
| extra_membo_D2_2003.wav | 1 | 1 | 100.00% | 2.8384 |
| extra_membo_D2_2004.wav | 1 | 1 | 100.00% | 2.7017 |
| extra_membo_D2_2005.wav | 1 | 1 | 100.00% | 3.5805 |
| extra_membo_D2_2006.wav | 1 | 1 | 100.00% | 3.3344 |
| extra_membo_D2_2007.wav | 1 | 1 | 100.00% | 1.5200 |
| extra_membo_D2_2008.wav | 1 | 1 | 100.00% | 1.5581 |
| extra_membo_D2_2009.wav | 1 | 1 | 100.00% | 3.2639 |
| extra_membo_D2_2010.wav | 1 | 1 | 100.00% | 2.8644 |
| extra_membo_D2_2011.wav | 1 | 1 | 100.00% | 3.5824 |
| extra_membo_D2_2012.wav | 1 | 1 | 100.00% | 3.3345 |
| extra_membo_D2_2013.wav | 1 | 1 | 100.00% | 3.2418 |
| extra_membo_D2_2014.wav | 1 | 1 | 100.00% | 2.9868 |
| extra_membo_D2_2015.wav | 1 | 1 | 100.00% | 3.5354 |
| extra_membo_D2_2016.wav | 1 | 1 | 100.00% | 3.7225 |
| extra_membo_D2_2017.wav | 1 | 1 | 100.00% | 2.4404 |
| extra_membo_D2_2018.wav | 1 | 1 | 100.00% | 2.6717 |
| extra_membo_D2_2019.wav | 1 | 1 | 100.00% | 3.6044 |
| extra_membo_D2_2020.wav | 1 | 1 | 100.00% | 3.9265 |
| extra_membo_D2_2021.wav | 1 | 1 | 100.00% | 3.4811 |
| extra_membo_D2_2022.wav | 1 | 1 | 100.00% | 2.8655 |
| extra_membo_D2_2023.wav | 1 | 1 | 100.00% | 2.8419 |
| extra_membo_D2_2024.wav | 1 | 1 | 100.00% | 3.0112 |
| extra_membo_D2_2025.wav | 1 | 1 | 100.00% | 3.5774 |
| extra_membo_D2_2026.wav | 1 | 1 | 100.00% | 3.9184 |
| extra_membo_D2_2027.wav | 1 | 1 | 100.00% | 3.7336 |
| extra_membo_D2_2028.wav | 1 | 1 | 100.00% | 3.2067 |
| extra_membo_D2_2029.wav | 1 | 1 | 100.00% | 3.0893 |
| extra_membo_D2_2030.wav | 1 | 1 | 100.00% | 2.5426 |
| extra_membo_D2_2031.wav | 1 | 1 | 100.00% | 3.4516 |
| extra_membo_D2_2032.wav | 1 | 1 | 100.00% | 2.7091 |
| extra_membo_D2_2033.wav | 1 | 1 | 100.00% | 3.1236 |
| extra_membo_D2_2034.wav | 1 | 1 | 100.00% | 3.1281 |
| extra_membo_D2_2035.wav | 1 | 1 | 100.00% | 2.9378 |
| extra_membo_D2_2036.wav | 1 | 1 | 100.00% | 3.1611 |
| extra_membo_D2_2037.wav | 1 | 1 | 100.00% | 3.0878 |
| extra_membo_D2_2038.wav | 1 | 1 | 100.00% | 2.8539 |
| extra_membo_D2_2039.wav | 1 | 1 | 100.00% | 2.8669 |
| extra_membo_D2_2040.wav | 1 | 1 | 100.00% | 2.9456 |
| extra_membo_D2_2041.wav | 1 | 1 | 100.00% | 3.4297 |
| extra_membo_D2_2042.wav | 1 | 1 | 100.00% | 2.7943 |
| extra_membo_D2_2043.wav | 1 | 1 | 100.00% | 2.9806 |
| extra_membo_D2_2044.wav | 1 | 1 | 100.00% | 2.4932 |
| extra_membo_D2_2045.wav | 1 | 1 | 100.00% | 2.2870 |
| extra_membo_D2_2046.wav | 1 | 1 | 100.00% | 2.4669 |
| extra_membo_D2_2047.wav | 1 | 1 | 100.00% | 2.6895 |
| extra_membo_D2_2048.wav | 1 | 1 | 100.00% | 2.7442 |
| extra_membo_D2_2049.wav | 1 | 1 | 100.00% | 3.2352 |
| extra_membo_D2_2050.wav | 1 | 1 | 100.00% | 2.9203 |
| extra_membo_D2_2051.wav | 1 | 1 | 100.00% | 2.2163 |
| extra_membo_D2_2052.wav | 1 | 1 | 100.00% | 2.6705 |
| extra_membo_D2_2053.wav | 1 | 1 | 100.00% | 3.0783 |
| extra_membo_D2_2054.wav | 1 | 1 | 100.00% | 3.3984 |
| extra_membo_D2_2055.wav | 1 | 1 | 100.00% | 3.4379 |
| mixed_0 | 5 | 5 | 100.00% | 2.1598 |
| mixed_1 | 5 | 4 | **80.00%** ❌ | 0.8278 |
| mixed_10 | 5 | 5 | 100.00% | 1.8723 |
| mixed_11 | 5 | 5 | 100.00% | 2.6439 |
| mixed_12 | 5 | 5 | 100.00% | 2.9847 |
| mixed_13 | 5 | 5 | 100.00% | 2.6728 |
| mixed_14 | 5 | 5 | 100.00% | 3.2207 |
| mixed_15 | 5 | 5 | 100.00% | 2.6646 |
| mixed_16 | 5 | 5 | 100.00% | 2.0986 |
| mixed_17 | 5 | 5 | 100.00% | 2.9494 |
| mixed_18 | 5 | 5 | 100.00% | 2.8164 |
| mixed_19 | 5 | 5 | 100.00% | 2.3189 |
| mixed_2 | 5 | 5 | 100.00% | 2.7399 |
| mixed_20 | 5 | 5 | 100.00% | 3.0026 |
| mixed_21 | 5 | 5 | 100.00% | 2.2836 |
| mixed_22 | 5 | 5 | 100.00% | 1.3941 |
| mixed_23 | 5 | 5 | 100.00% | 2.8457 |
| mixed_24 | 5 | 5 | 100.00% | 2.5315 |
| mixed_25 | 5 | 5 | 100.00% | 2.8039 |
| mixed_26 | 5 | 5 | 100.00% | 3.0435 |
| mixed_27 | 5 | 5 | 100.00% | 2.7087 |
| mixed_28 | 5 | 5 | 100.00% | 2.4920 |
| mixed_29 | 5 | 5 | 100.00% | 1.1332 |
| mixed_3 | 5 | 4 | **80.00%** ❌ | 1.8060 |
| mixed_30 | 5 | 5 | 100.00% | 1.0997 |
| mixed_31 | 5 | 5 | 100.00% | 1.4673 |
| mixed_32 | 5 | 5 | 100.00% | 0.6552 |
| mixed_33 | 5 | 5 | 100.00% | 2.4246 |
| mixed_34 | 5 | 5 | 100.00% | 1.5570 |
| mixed_35 | 5 | 5 | 100.00% | 0.7254 |
| mixed_36 | 5 | 5 | 100.00% | 1.6509 |
| mixed_37 | 5 | 5 | 100.00% | 2.3247 |
| mixed_38 | 5 | 5 | 100.00% | 2.4206 |
| mixed_39 | 5 | 5 | 100.00% | 2.3885 |
| mixed_4 | 5 | 5 | 100.00% | 2.4687 |
| mixed_40 | 5 | 5 | 100.00% | 2.0471 |
| mixed_41 | 5 | 5 | 100.00% | 1.7751 |
| mixed_42 | 5 | 5 | 100.00% | 2.6292 |
| mixed_43 | 5 | 5 | 100.00% | 1.4377 |
| mixed_44 | 5 | 5 | 100.00% | 2.3377 |
| mixed_45 | 5 | 5 | 100.00% | 2.6240 |
| mixed_46 | 5 | 5 | 100.00% | 2.6276 |
| mixed_47 | 5 | 5 | 100.00% | 1.4652 |
| mixed_48 | 5 | 5 | 100.00% | 2.8899 |
| mixed_49 | 5 | 5 | 100.00% | 1.3943 |
| mixed_5 | 5 | 5 | 100.00% | 1.7556 |
| mixed_50 | 5 | 5 | 100.00% | 2.2853 |
| mixed_51 | 5 | 5 | 100.00% | 2.5310 |
| mixed_52 | 5 | 5 | 100.00% | 2.5553 |
| mixed_53 | 5 | 5 | 100.00% | 2.5997 |
| mixed_54 | 5 | 5 | 100.00% | 2.0277 |
| mixed_55 | 5 | 5 | 100.00% | 1.7738 |
| mixed_56 | 5 | 5 | 100.00% | 2.4146 |
| mixed_57 | 5 | 5 | 100.00% | 3.1451 |
| mixed_58 | 5 | 5 | 100.00% | 2.9347 |
| mixed_59 | 5 | 5 | 100.00% | 3.0987 |
| mixed_6 | 5 | 5 | 100.00% | 2.7484 |
| mixed_60 | 5 | 5 | 100.00% | 1.3404 |
| mixed_61 | 5 | 5 | 100.00% | 1.0200 |
| mixed_62 | 5 | 5 | 100.00% | 3.0988 |
| mixed_63 | 5 | 5 | 100.00% | 2.5789 |
| mixed_64 | 5 | 5 | 100.00% | 2.2524 |
| mixed_65 | 5 | 5 | 100.00% | 2.5438 |
| mixed_66 | 5 | 5 | 100.00% | 2.4399 |
| mixed_7 | 5 | 5 | 100.00% | 2.5546 |
| mixed_8 | 5 | 5 | 100.00% | 2.9146 |
| mixed_9 | 5 | 3 | **60.00%** ❌ | 1.4242 |
| mixed_membo_0 | 5 | 5 | 100.00% | 2.1668 |
| mixed_membo_1 | 5 | 3 | **60.00%** ❌ | -0.0982 |
| mixed_membo_10 | 5 | 5 | 100.00% | 0.9571 |
| mixed_membo_11 | 5 | 5 | 100.00% | 2.3366 |
| mixed_membo_12 | 5 | 5 | 100.00% | 2.5658 |
| mixed_membo_13 | 5 | 5 | 100.00% | 2.7512 |
| mixed_membo_14 | 5 | 5 | 100.00% | 2.9552 |
| mixed_membo_15 | 5 | 4 | **80.00%** ❌ | 1.6473 |
| mixed_membo_16 | 5 | 4 | **80.00%** ❌ | 2.2320 |
| mixed_membo_17 | 5 | 4 | **80.00%** ❌ | 0.9982 |
| mixed_membo_18 | 5 | 5 | 100.00% | 2.9965 |
| mixed_membo_19 | 5 | 4 | **80.00%** ❌ | 3.0435 |
| mixed_membo_2 | 5 | 5 | 100.00% | 1.9885 |
| mixed_membo_20 | 5 | 5 | 100.00% | 4.2735 |
| mixed_membo_21 | 5 | 5 | 100.00% | 1.4883 |
| mixed_membo_22 | 5 | 4 | **80.00%** ❌ | 2.1791 |
| mixed_membo_23 | 5 | 5 | 100.00% | 2.4787 |
| mixed_membo_24 | 5 | 5 | 100.00% | 2.7877 |
| mixed_membo_25 | 5 | 5 | 100.00% | 2.3814 |
| mixed_membo_26 | 5 | 5 | 100.00% | 2.4507 |
| mixed_membo_27 | 5 | 5 | 100.00% | 1.9147 |
| mixed_membo_28 | 5 | 5 | 100.00% | 1.8158 |
| mixed_membo_29 | 5 | 5 | 100.00% | 0.6692 |
| mixed_membo_3 | 5 | 4 | **80.00%** ❌ | 3.4586 |
| mixed_membo_30 | 5 | 3 | **60.00%** ❌ | 0.0065 |
| mixed_membo_31 | 5 | 5 | 100.00% | 0.7257 |
| mixed_membo_32 | 5 | 2 | **40.00%** ❌ | -0.2121 |
| mixed_membo_33 | 5 | 5 | 100.00% | 2.4204 |
| mixed_membo_34 | 5 | 4 | **80.00%** ❌ | 0.7917 |
| mixed_membo_35 | 5 | 1 | **20.00%** ❌ | 0.0689 |
| mixed_membo_36 | 5 | 4 | **80.00%** ❌ | 0.8204 |
| mixed_membo_37 | 5 | 5 | 100.00% | 3.8020 |
| mixed_membo_38 | 5 | 5 | 100.00% | 4.3129 |
| mixed_membo_39 | 5 | 5 | 100.00% | 3.4863 |
| mixed_membo_4 | 5 | 5 | 100.00% | 2.4287 |
| mixed_membo_40 | 5 | 5 | 100.00% | 1.9682 |
| mixed_membo_41 | 5 | 5 | 100.00% | 0.7686 |
| mixed_membo_42 | 5 | 5 | 100.00% | 4.0452 |
| mixed_membo_43 | 5 | 4 | **80.00%** ❌ | 0.4584 |
| mixed_membo_44 | 5 | 5 | 100.00% | 2.6330 |
| mixed_membo_45 | 5 | 5 | 100.00% | 3.4688 |
| mixed_membo_46 | 5 | 5 | 100.00% | 1.9333 |
| mixed_membo_47 | 5 | 5 | 100.00% | 1.0515 |
| mixed_membo_48 | 5 | 5 | 100.00% | 4.1796 |
| mixed_membo_49 | 5 | 0 | **0.00%** ❌ | -7.3133 |
| mixed_membo_5 | 5 | 5 | 100.00% | 2.3680 |
| mixed_membo_50 | 5 | 5 | 100.00% | 2.0459 |
| mixed_membo_51 | 5 | 5 | 100.00% | 2.0951 |
| mixed_membo_52 | 5 | 5 | 100.00% | 2.2198 |
| mixed_membo_53 | 5 | 5 | 100.00% | 1.4586 |
| mixed_membo_54 | 5 | 5 | 100.00% | 0.4994 |
| mixed_membo_55 | 5 | 5 | 100.00% | 1.6853 |
| mixed_membo_56 | 5 | 5 | 100.00% | 1.5297 |
| mixed_membo_57 | 5 | 5 | 100.00% | 3.2410 |
| mixed_membo_58 | 5 | 5 | 100.00% | 4.3432 |
| mixed_membo_59 | 5 | 5 | 100.00% | 3.3831 |
| mixed_membo_6 | 5 | 5 | 100.00% | 3.3319 |
| mixed_membo_60 | 5 | 2 | **40.00%** ❌ | 0.2688 |
| mixed_membo_61 | 5 | 4 | **80.00%** ❌ | 0.7054 |
| mixed_membo_62 | 5 | 5 | 100.00% | 2.1565 |
| mixed_membo_63 | 5 | 5 | 100.00% | 2.3100 |
| mixed_membo_64 | 5 | 5 | 100.00% | 3.8246 |
| mixed_membo_65 | 5 | 5 | 100.00% | 3.3559 |
| mixed_membo_66 | 5 | 5 | 100.00% | 2.0827 |
| mixed_membo_7 | 5 | 5 | 100.00% | 2.2392 |
| mixed_membo_8 | 5 | 5 | 100.00% | 2.7574 |
| mixed_membo_9 | 5 | 3 | **60.00%** ❌ | 1.9796 |

### 4. Seznam nezachycených dronů (False Negatives)
Seznam konkrétních nahrávek s drony, které model vyhodnotil jako šum pozadí.

| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |
|:---|:---|:---:|:---:|:---:|
| extra_membo_D2_2000.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.4689 |
| Membo_1_000-membo_000_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -2.3952 |
| Membo_2_027-membo_004_.wav | Membo Drone (Clean) | DRONE (1) | NOISE (0) | -0.6998 |
| mixed_1-bebop_004_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0522 |
| mixed_3-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1164 |
| mixed_9-bebop_000_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4114 |
| mixed_9-bebop_001_.wav | Mixed Bebop (Noise+Drone) | DRONE (1) | NOISE (0) | -1.5026 |
| mixed_membo_1-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5715 |
| mixed_membo_1-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.9045 |
| mixed_membo_15-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3455 |
| mixed_membo_16-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0348 |
| mixed_membo_17-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -6.0523 |
| mixed_membo_19-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0714 |
| mixed_membo_22-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0704 |
| mixed_membo_3-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.8594 |
| mixed_membo_30-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3879 |
| mixed_membo_30-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.7249 |
| mixed_membo_32-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0566 |
| mixed_membo_32-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6798 |
| mixed_membo_32-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.5163 |
| mixed_membo_34-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2358 |
| mixed_membo_35-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3420 |
| mixed_membo_35-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.4093 |
| mixed_membo_35-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.0968 |
| mixed_membo_35-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3131 |
| mixed_membo_36-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.2281 |
| mixed_membo_43-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.6535 |
| mixed_membo_49-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -7.1514 |
| mixed_membo_49-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -5.5739 |
| mixed_membo_49-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -8.3223 |
| mixed_membo_49-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -7.5994 |
| mixed_membo_49-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -7.9198 |
| mixed_membo_60-membo_002_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1581 |
| mixed_membo_60-membo_003_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.1479 |
| mixed_membo_60-membo_004_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.0789 |
| mixed_membo_61-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -0.3500 |
| mixed_membo_9-membo_000_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -1.3242 |
| mixed_membo_9-membo_001_.wav | Mixed Membo (Noise+Drone) | DRONE (1) | NOISE (0) | -2.0480 |

### 5. Seznam falešných poplachů (False Positives)
Seznam konkrétních nahrávek šumu, které model vyhodnotil jako dron.

| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |
|:---|:---|:---:|:---:|:---:|
| Parquet_Noise_772 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0715 |
| Parquet_Noise_2753 | Parquet Noise | NOISE (0) | DRONE (1) | 1.0733 |
| Parquet_Noise_2595 | Parquet Noise | NOISE (0) | DRONE (1) | 1.3030 |
| Parquet_Noise_2328 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6733 |
| Parquet_Noise_2196 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6833 |
| Parquet_Noise_1547 | Parquet Noise | NOISE (0) | DRONE (1) | 0.2172 |
| Parquet_Noise_368 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0812 |
| Parquet_Noise_2570 | Parquet Noise | NOISE (0) | DRONE (1) | 0.1381 |
| Parquet_Noise_678 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0966 |
| Parquet_Noise_1422 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6961 |
| Parquet_Noise_1088 | Parquet Noise | NOISE (0) | DRONE (1) | 0.8205 |
| Parquet_Noise_677 | Parquet Noise | NOISE (0) | DRONE (1) | 0.3246 |
| Parquet_Noise_1833 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5775 |
| Parquet_Noise_999 | Parquet Noise | NOISE (0) | DRONE (1) | 1.5343 |
| Parquet_Noise_1338 | Parquet Noise | NOISE (0) | DRONE (1) | 0.1933 |
| Parquet_Noise_2060 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6463 |
| Parquet_Noise_1334 | Parquet Noise | NOISE (0) | DRONE (1) | 1.0827 |
| Parquet_Noise_2649 | Parquet Noise | NOISE (0) | DRONE (1) | 1.2109 |
| Parquet_Noise_907 | Parquet Noise | NOISE (0) | DRONE (1) | 1.3658 |
| Parquet_Noise_141 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0485 |
| Parquet_Noise_2298 | Parquet Noise | NOISE (0) | DRONE (1) | 0.1997 |
| Parquet_Noise_535 | Parquet Noise | NOISE (0) | DRONE (1) | 0.8977 |
| Parquet_Noise_521 | Parquet Noise | NOISE (0) | DRONE (1) | 0.3576 |
| Parquet_Noise_544 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5742 |
| Parquet_Noise_1419 | Parquet Noise | NOISE (0) | DRONE (1) | 2.7600 |
| Parquet_Noise_2437 | Parquet Noise | NOISE (0) | DRONE (1) | 0.4279 |
| Parquet_Noise_834 | Parquet Noise | NOISE (0) | DRONE (1) | 1.1852 |
| Parquet_Noise_1231 | Parquet Noise | NOISE (0) | DRONE (1) | 1.4471 |
| Parquet_Noise_2582 | Parquet Noise | NOISE (0) | DRONE (1) | 1.2557 |
| Parquet_Noise_1882 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0917 |
| Parquet_Noise_2121 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5092 |
| Parquet_Noise_2309 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0585 |
| Parquet_Noise_1023 | Parquet Noise | NOISE (0) | DRONE (1) | 3.7676 |
| Parquet_Noise_1801 | Parquet Noise | NOISE (0) | DRONE (1) | 1.5875 |
| Parquet_Noise_1820 | Parquet Noise | NOISE (0) | DRONE (1) | 0.8063 |
| Parquet_Noise_2538 | Parquet Noise | NOISE (0) | DRONE (1) | 0.3925 |
| Parquet_Noise_2334 | Parquet Noise | NOISE (0) | DRONE (1) | 0.7531 |
| Parquet_Noise_2127 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0678 |
| Parquet_Noise_518 | Parquet Noise | NOISE (0) | DRONE (1) | 0.3632 |
| Parquet_Noise_2058 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6293 |
| Parquet_Noise_1228 | Parquet Noise | NOISE (0) | DRONE (1) | 1.7251 |
| Parquet_Noise_2659 | Parquet Noise | NOISE (0) | DRONE (1) | 0.4792 |
| Parquet_Noise_1992 | Parquet Noise | NOISE (0) | DRONE (1) | 1.7021 |
| Parquet_Noise_707 | Parquet Noise | NOISE (0) | DRONE (1) | 0.9238 |
| Parquet_Noise_1080 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0441 |
| Parquet_Noise_2469 | Parquet Noise | NOISE (0) | DRONE (1) | 0.3239 |
| Parquet_Noise_1592 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5836 |
| Parquet_Noise_298 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0196 |
| Parquet_Noise_1543 | Parquet Noise | NOISE (0) | DRONE (1) | 1.2255 |
| Parquet_Noise_685 | Parquet Noise | NOISE (0) | DRONE (1) | 1.0284 |
| Parquet_Noise_2314 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6959 |
| Parquet_Noise_460 | Parquet Noise | NOISE (0) | DRONE (1) | 0.5365 |
| Parquet_Noise_2494 | Parquet Noise | NOISE (0) | DRONE (1) | 0.8879 |
| Parquet_Noise_239 | Parquet Noise | NOISE (0) | DRONE (1) | 0.8007 |
| Parquet_Noise_2303 | Parquet Noise | NOISE (0) | DRONE (1) | 0.7285 |
| Parquet_Noise_1566 | Parquet Noise | NOISE (0) | DRONE (1) | 0.1407 |
| Parquet_Noise_1177 | Parquet Noise | NOISE (0) | DRONE (1) | 0.0132 |
| Parquet_Noise_44 | Parquet Noise | NOISE (0) | DRONE (1) | 0.1140 |
| Parquet_Noise_1681 | Parquet Noise | NOISE (0) | DRONE (1) | 1.0028 |
| Parquet_Noise_2005 | Parquet Noise | NOISE (0) | DRONE (1) | 1.4067 |
| Parquet_Noise_2817 | Parquet Noise | NOISE (0) | DRONE (1) | 0.4571 |
| Parquet_Noise_1854 | Parquet Noise | NOISE (0) | DRONE (1) | 1.1846 |
| Parquet_Noise_533 | Parquet Noise | NOISE (0) | DRONE (1) | 0.6539 |
| 1-100038-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2191 |
| 1-100038-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1617 |
| 1-100038-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0125 |
| 1-100038-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4657 |
| 1-100210-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0009 |
| 1-100210-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0936 |
| 1-100210-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2034 |
| 1-101296-A-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4201 |
| 1-101296-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0537 |
| 1-101296-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4554 |
| 1-101296-B-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4400 |
| 1-101404-A-341.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4899 |
| 1-101404-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7778 |
| 1-101404-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9611 |
| 1-101404-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2650 |
| 1-103298-A-91.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0134 |
| 1-103298-A-92.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4367 |
| 1-103298-A-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6488 |
| 1-116765-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1077 |
| 1-116765-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1551 |
| 1-116765-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6208 |
| 1-11687-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3212 |
| 1-11687-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8112 |
| 1-11687-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6062 |
| 1-11687-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0642 |
| 1-11687-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3734 |
| 1-118206-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3527 |
| 1-119125-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2174 |
| 1-119125-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7934 |
| 1-119125-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6164 |
| 1-119125-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8251 |
| 1-121951-A-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1839 |
| 1-12654-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7426 |
| 1-12654-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4560 |
| 1-12654-A-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9973 |
| 1-12654-B-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5880 |
| 1-12654-B-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3327 |
| 1-12654-B-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5747 |
| 1-13572-A-461.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6934 |
| 1-13613-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0405 |
| 1-13613-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4803 |
| 1-13613-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3076 |
| 1-13613-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6002 |
| 1-137296-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7994 |
| 1-137296-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5441 |
| 1-137296-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2432 |
| 1-137296-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9678 |
| 1-137296-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2138 |
| 1-14262-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8511 |
| 1-14262-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8993 |
| 1-14262-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9560 |
| 1-14262-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8740 |
| 1-14262-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5148 |
| 1-15689-A-41.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3285 |
| 1-15689-A-43.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2781 |
| 1-15689-B-41.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4858 |
| 1-15689-B-43.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3617 |
| 1-160563-A-480.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2104 |
| 1-16568-A-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0493 |
| 1-16568-A-33.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4371 |
| 1-16568-A-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6485 |
| 1-17092-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4701 |
| 1-17092-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3516 |
| 1-17092-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8315 |
| 1-17092-B-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1399 |
| 1-172649-A-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3333 |
| 1-172649-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6595 |
| 1-172649-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9284 |
| 1-172649-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5540 |
| 1-172649-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5581 |
| 1-172649-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8936 |
| 1-172649-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6063 |
| 1-172649-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0213 |
| 1-172649-B-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9283 |
| 1-172649-B-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7147 |
| 1-172649-C-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0470 |
| 1-172649-C-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9354 |
| 1-172649-C-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4233 |
| 1-172649-C-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9343 |
| 1-172649-C-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3497 |
| 1-172649-D-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5478 |
| 1-172649-D-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7520 |
| 1-172649-D-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7771 |
| 1-172649-D-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7708 |
| 1-172649-D-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8675 |
| 1-172649-E-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8052 |
| 1-172649-E-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1493 |
| 1-172649-E-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0280 |
| 1-172649-E-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1519 |
| 1-172649-E-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9632 |
| 1-172649-F-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1273 |
| 1-17367-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1118 |
| 1-17367-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0326 |
| 1-17367-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3934 |
| 1-17367-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3064 |
| 1-17367-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0382 |
| 1-17585-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1946 |
| 1-17585-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6886 |
| 1-17742-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7257 |
| 1-17742-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9746 |
| 1-17742-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2769 |
| 1-17742-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1309 |
| 1-17742-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0818 |
| 1-17808-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4492 |
| 1-17808-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2750 |
| 1-17808-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1752 |
| 1-17808-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3121 |
| 1-17808-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6095 |
| 1-17808-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7874 |
| 1-17808-B-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8998 |
| 1-18527-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2433 |
| 1-18527-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0196 |
| 1-19840-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6308 |
| 1-19840-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3946 |
| 1-19840-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5746 |
| 1-19840-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5793 |
| 1-19840-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3030 |
| 1-19872-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7589 |
| 1-19872-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9274 |
| 1-19872-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6314 |
| 1-19872-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9556 |
| 1-19872-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9859 |
| 1-19872-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2437 |
| 1-19872-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0248 |
| 1-19872-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2932 |
| 1-19872-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3939 |
| 1-19872-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2027 |
| 1-202111-A-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0234 |
| 1-202111-A-33.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1086 |
| 1-20545-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1160 |
| 1-20545-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7420 |
| 1-20545-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7180 |
| 1-20736-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0797 |
| 1-20736-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3126 |
| 1-208757-B-24.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1528 |
| 1-21189-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2366 |
| 1-21189-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3790 |
| 1-21189-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9122 |
| 1-21189-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5332 |
| 1-21189-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5659 |
| 1-21896-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3631 |
| 1-21896-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7989 |
| 1-21896-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4576 |
| 1-21896-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0347 |
| 1-21896-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0709 |
| 1-23222-B-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2322 |
| 1-23222-B-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2368 |
| 1-23996-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7168 |
| 1-23996-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6524 |
| 1-23996-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8699 |
| 1-23996-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1034 |
| 1-23996-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3327 |
| 1-23996-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9626 |
| 1-23996-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5064 |
| 1-23996-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3633 |
| 1-23996-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1874 |
| 1-23996-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0543 |
| 1-24524-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1115 |
| 1-24796-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6961 |
| 1-24796-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4687 |
| 1-24796-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5025 |
| 1-24796-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8126 |
| 1-24796-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8732 |
| 1-25781-A-483.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6696 |
| 1-25781-A-484.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3608 |
| 1-260640-A-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7205 |
| 1-260640-C-23.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0445 |
| 1-260640-C-24.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0493 |
| 1-26222-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6680 |
| 1-26222-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1941 |
| 1-26222-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3003 |
| 1-26222-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4356 |
| 1-26222-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4085 |
| 1-26806-A-11.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0933 |
| 1-27165-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3644 |
| 1-27165-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4139 |
| 1-27165-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5110 |
| 1-27165-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2482 |
| 1-27165-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6890 |
| 1-27166-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7086 |
| 1-27166-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9036 |
| 1-27166-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2871 |
| 1-27166-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8414 |
| 1-27166-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8635 |
| 1-27403-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2442 |
| 1-27403-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0714 |
| 1-27403-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2158 |
| 1-27403-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0394 |
| 1-27405-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0536 |
| 1-27405-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1504 |
| 1-27405-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2278 |
| 1-28005-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7012 |
| 1-28135-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7050 |
| 1-28135-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4962 |
| 1-28135-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6682 |
| 1-28135-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6653 |
| 1-28135-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5485 |
| 1-28135-B-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0178 |
| 1-28135-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2257 |
| 1-28135-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1146 |
| 1-28135-B-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5650 |
| 1-28808-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2056 |
| 1-29532-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7911 |
| 1-29532-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0021 |
| 1-29532-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7737 |
| 1-29532-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7524 |
| 1-29532-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5415 |
| 1-29561-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2551 |
| 1-29561-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0852 |
| 1-29561-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5372 |
| 1-29561-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5830 |
| 1-29561-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4667 |
| 1-29680-A-211.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6752 |
| 1-30214-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4700 |
| 1-30226-A-04.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0852 |
| 1-31251-B-64.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0935 |
| 1-32373-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0261 |
| 1-32373-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8544 |
| 1-32373-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1168 |
| 1-32373-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3358 |
| 1-32373-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2626 |
| 1-32373-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4423 |
| 1-32373-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1313 |
| 1-32373-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2025 |
| 1-32373-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1535 |
| 1-32373-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0236 |
| 1-33658-A-264.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6534 |
| 1-34495-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3230 |
| 1-34495-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4368 |
| 1-34495-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2880 |
| 1-34495-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5182 |
| 1-34497-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2651 |
| 1-34497-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8566 |
| 1-34497-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9304 |
| 1-34497-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1911 |
| 1-34497-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9086 |
| 1-36397-A-231.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6723 |
| 1-36929-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1783 |
| 1-36929-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0737 |
| 1-37226-A-290.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0728 |
| 1-38559-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2096 |
| 1-38559-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5753 |
| 1-38559-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4489 |
| 1-38559-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6366 |
| 1-38559-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7902 |
| 1-38560-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0238 |
| 1-38560-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6463 |
| 1-38560-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2265 |
| 1-38560-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2666 |
| 1-38560-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8875 |
| 1-39835-B-91.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1950 |
| 1-39901-B-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5632 |
| 1-39937-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6339 |
| 1-39937-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2400 |
| 1-39937-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2011 |
| 1-39937-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5569 |
| 1-39937-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2329 |
| 1-40621-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0031 |
| 1-40621-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3430 |
| 1-40730-A-11.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1478 |
| 1-40967-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0616 |
| 1-40967-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4632 |
| 1-4211-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0137 |
| 1-4211-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7319 |
| 1-45641-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4239 |
| 1-45645-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3250 |
| 1-45645-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6712 |
| 1-45645-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2882 |
| 1-46040-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6963 |
| 1-46040-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6036 |
| 1-46040-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3019 |
| 1-46040-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0878 |
| 1-46272-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5510 |
| 1-46272-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7607 |
| 1-46272-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3276 |
| 1-46272-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2455 |
| 1-46272-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8207 |
| 1-46274-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5768 |
| 1-46274-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7679 |
| 1-46274-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7328 |
| 1-46274-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4053 |
| 1-46744-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2004 |
| 1-46744-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0650 |
| 1-46744-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9787 |
| 1-46744-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6080 |
| 1-46744-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3459 |
| 1-46938-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5612 |
| 1-46938-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8476 |
| 1-46938-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4076 |
| 1-46938-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9430 |
| 1-46938-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3937 |
| 1-47250-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2093 |
| 1-47709-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6414 |
| 1-47709-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5284 |
| 1-47714-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4852 |
| 1-47714-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2550 |
| 1-47714-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5541 |
| 1-47714-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0042 |
| 1-47714-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1480 |
| 1-47819-A-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7957 |
| 1-47819-A-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4576 |
| 1-47819-A-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3311 |
| 1-47819-A-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4854 |
| 1-47819-B-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3387 |
| 1-47819-B-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8026 |
| 1-47819-B-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7032 |
| 1-47819-B-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0296 |
| 1-47819-B-54.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5012 |
| 1-47819-C-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3111 |
| 1-47819-C-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1336 |
| 1-47819-C-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9756 |
| 1-47819-C-54.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9945 |
| 1-47923-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4043 |
| 1-49409-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5587 |
| 1-49409-B-80.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7417 |
| 1-50060-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3117 |
| 1-50060-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3892 |
| 1-50060-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5381 |
| 1-50060-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5707 |
| 1-50060-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3375 |
| 1-50455-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3452 |
| 1-50455-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2333 |
| 1-50623-A-150.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9877 |
| 1-50623-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8700 |
| 1-50661-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6001 |
| 1-50661-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6125 |
| 1-50661-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4082 |
| 1-50661-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6690 |
| 1-50661-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6264 |
| 1-50688-A-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7331 |
| 1-50688-A-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4810 |
| 1-51037-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2730 |
| 1-51037-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0103 |
| 1-51037-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2160 |
| 1-51037-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0011 |
| 1-51170-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4797 |
| 1-51170-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2162 |
| 1-51433-A-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6371 |
| 1-51433-A-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5951 |
| 1-51805-A-333.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0020 |
| 1-51805-C-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6506 |
| 1-51805-C-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2858 |
| 1-51805-E-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7665 |
| 1-51805-G-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2570 |
| 1-51805-H-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6031 |
| 1-51805-H-332.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7525 |
| 1-53444-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0834 |
| 1-53444-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9796 |
| 1-53467-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1781 |
| 1-53670-A-180.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4385 |
| 1-53670-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3883 |
| 1-53670-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4942 |
| 1-53670-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7540 |
| 1-53670-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1178 |
| 1-54065-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5261 |
| 1-54065-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3581 |
| 1-54065-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8507 |
| 1-54065-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1785 |
| 1-54065-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0364 |
| 1-54752-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5196 |
| 1-54752-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8371 |
| 1-54752-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6794 |
| 1-54752-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9149 |
| 1-54918-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5185 |
| 1-54918-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5024 |
| 1-54918-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8191 |
| 1-54918-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8838 |
| 1-54918-B-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8771 |
| 1-54918-B-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9999 |
| 1-54918-B-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2125 |
| 1-54958-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8191 |
| 1-54958-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7228 |
| 1-54958-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8082 |
| 1-54958-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9463 |
| 1-54958-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7791 |
| 1-56269-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1629 |
| 1-56269-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0845 |
| 1-56269-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5863 |
| 1-56311-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6591 |
| 1-56311-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4015 |
| 1-56311-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5154 |
| 1-56311-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3516 |
| 1-56311-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3173 |
| 1-57316-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7129 |
| 1-57316-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5000 |
| 1-57316-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3080 |
| 1-57316-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1202 |
| 1-57316-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4814 |
| 1-57318-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2271 |
| 1-57318-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2215 |
| 1-57318-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0704 |
| 1-57318-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9704 |
| 1-57318-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3265 |
| 1-58923-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2004 |
| 1-58923-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5217 |
| 1-58923-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1990 |
| 1-58923-B-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0490 |
| 1-59102-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3130 |
| 1-59102-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7079 |
| 1-59102-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8921 |
| 1-59102-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1561 |
| 1-59102-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6787 |
| 1-60460-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8243 |
| 1-60460-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6324 |
| 1-60460-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8051 |
| 1-60460-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5828 |
| 1-60460-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5453 |
| 1-60676-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7160 |
| 1-61252-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7451 |
| 1-61252-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4933 |
| 1-61252-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3549 |
| 1-61252-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7593 |
| 1-61252-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4918 |
| 1-61261-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6371 |
| 1-61261-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6314 |
| 1-61261-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7884 |
| 1-61261-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3182 |
| 1-61534-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4652 |
| 1-61534-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4743 |
| 1-61534-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3552 |
| 1-62565-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8030 |
| 1-62565-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5715 |
| 1-62565-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5189 |
| 1-62565-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6703 |
| 1-62565-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6442 |
| 1-63871-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1682 |
| 1-63871-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9575 |
| 1-63871-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0670 |
| 1-63871-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3899 |
| 1-63871-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4225 |
| 1-64398-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8482 |
| 1-64398-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5100 |
| 1-64398-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3066 |
| 1-64398-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8836 |
| 1-64398-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4585 |
| 1-64398-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0794 |
| 1-64398-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9074 |
| 1-64473-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9910 |
| 1-64473-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6251 |
| 1-64473-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2314 |
| 1-64473-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7218 |
| 1-64473-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6289 |
| 1-67152-A-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8870 |
| 1-67152-A-171.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6171 |
| 1-67432-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6878 |
| 1-67432-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0553 |
| 1-67432-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5104 |
| 1-67432-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4958 |
| 1-67432-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4979 |
| 1-68628-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7582 |
| 1-68670-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6483 |
| 1-69760-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3704 |
| 1-69760-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9039 |
| 1-69760-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1023 |
| 1-69760-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3054 |
| 1-69760-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3763 |
| 1-7057-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7246 |
| 1-7057-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2085 |
| 1-7057-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3481 |
| 1-7057-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1249 |
| 1-7057-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0658 |
| 1-72195-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0676 |
| 1-72195-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9426 |
| 1-72195-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9108 |
| 1-72695-A-264.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9071 |
| 1-73585-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4211 |
| 1-73585-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5870 |
| 1-73585-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2360 |
| 1-73585-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3751 |
| 1-73585-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9626 |
| 1-74517-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2657 |
| 1-74517-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6954 |
| 1-74517-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3705 |
| 1-7456-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0698 |
| 1-7456-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1705 |
| 1-7456-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1884 |
| 1-7456-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1283 |
| 1-7456-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8209 |
| 1-75189-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0105 |
| 1-75190-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7064 |
| 1-77160-A-33.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5408 |
| 1-77160-A-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7314 |
| 1-79113-A-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6903 |
| 1-79236-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3204 |
| 1-79236-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9747 |
| 1-79236-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5523 |
| 1-79236-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1806 |
| 1-7973-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9152 |
| 1-80785-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2033 |
| 1-80785-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1625 |
| 1-80785-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1090 |
| 1-80785-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2172 |
| 1-80785-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4316 |
| 1-80840-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.9665 |
| 1-80840-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6044 |
| 1-80840-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.8304 |
| 1-80840-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.9538 |
| 1-80840-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.0001 |
| 1-80841-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.4623 |
| 1-80841-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.7058 |
| 1-80841-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.4548 |
| 1-80841-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.3984 |
| 1-80841-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.5405 |
| 1-85123-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5319 |
| 1-85123-A-311.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2003 |
| 1-85123-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6641 |
| 1-85123-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9774 |
| 1-85123-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0980 |
| 1-85168-A-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9812 |
| 1-85168-A-392.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8134 |
| 1-87565-A-293.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3097 |
| 1-91359-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9098 |
| 1-91359-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1285 |
| 1-91359-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8891 |
| 1-91359-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3480 |
| 1-91359-B-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4994 |
| 1-91359-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9650 |
| 1-91359-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8339 |
| 1-91359-B-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0174 |
| 1-94231-A-320.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1918 |
| 1-94231-A-321.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8814 |
| 1-94231-A-322.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6960 |
| 1-94231-B-323.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1558 |
| 1-94231-B-324.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3334 |
| 1-97793-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1460 |
| 1-9841-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8103 |
| 1-9841-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9090 |
| 1-9841-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2838 |
| 1-9841-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9469 |
| 1-9841-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5000 |
| 1-99958-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1907 |
| 2-100648-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1205 |
| 2-100648-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4434 |
| 2-100648-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0298 |
| 2-100648-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9190 |
| 2-101676-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6170 |
| 2-101676-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3118 |
| 2-101676-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9315 |
| 2-101676-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9230 |
| 2-101676-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8077 |
| 2-102414-A-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7087 |
| 2-102414-B-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4063 |
| 2-102414-B-171.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5069 |
| 2-102414-B-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6850 |
| 2-102414-C-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7628 |
| 2-102414-C-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6007 |
| 2-102414-D-171.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6599 |
| 2-102414-D-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8331 |
| 2-102414-E-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5507 |
| 2-102414-F-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7272 |
| 2-102414-F-171.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8103 |
| 2-102414-F-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2831 |
| 2-102414-G-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0469 |
| 2-102567-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2843 |
| 2-102567-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4968 |
| 2-102567-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2711 |
| 2-102567-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5217 |
| 2-102567-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3081 |
| 2-102567-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5021 |
| 2-102567-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1762 |
| 2-102567-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4643 |
| 2-102567-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5939 |
| 2-102567-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4807 |
| 2-102567-C-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5988 |
| 2-102567-C-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8486 |
| 2-102567-C-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4337 |
| 2-102567-C-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3759 |
| 2-102567-C-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2195 |
| 2-102567-D-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7832 |
| 2-102567-D-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5139 |
| 2-102567-D-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1671 |
| 2-102567-D-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9521 |
| 2-102567-D-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2205 |
| 2-104877-A-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1986 |
| 2-104952-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2305 |
| 2-104952-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0835 |
| 2-104952-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0430 |
| 2-104952-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0942 |
| 2-104952-B-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4171 |
| 2-104952-B-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6252 |
| 2-104952-B-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2783 |
| 2-104952-B-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3945 |
| 2-104952-B-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9056 |
| 2-105270-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7057 |
| 2-106014-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4814 |
| 2-106014-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5958 |
| 2-106014-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3465 |
| 2-106014-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5304 |
| 2-106014-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4501 |
| 2-106015-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1098 |
| 2-106015-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1232 |
| 2-106015-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0289 |
| 2-106015-B-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1781 |
| 2-106015-B-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1748 |
| 2-106015-B-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2317 |
| 2-106019-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5606 |
| 2-106019-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8204 |
| 2-106019-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6936 |
| 2-106019-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6132 |
| 2-106019-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6851 |
| 2-106072-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3507 |
| 2-106072-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8579 |
| 2-106072-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8991 |
| 2-106072-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8757 |
| 2-106072-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0357 |
| 2-106073-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9696 |
| 2-106073-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7161 |
| 2-106073-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6625 |
| 2-106073-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7254 |
| 2-106073-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0599 |
| 2-106486-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3509 |
| 2-106486-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3909 |
| 2-106486-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4135 |
| 2-106486-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3133 |
| 2-106486-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4935 |
| 2-106487-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9782 |
| 2-106487-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0364 |
| 2-106487-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4728 |
| 2-106487-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3723 |
| 2-106487-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3049 |
| 2-108761-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8763 |
| 2-108761-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1227 |
| 2-108761-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1601 |
| 2-108761-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5830 |
| 2-108766-A-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0086 |
| 2-109231-A-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5066 |
| 2-109231-A-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5633 |
| 2-109231-C-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0887 |
| 2-109231-C-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1680 |
| 2-109374-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1778 |
| 2-109374-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0786 |
| 2-109374-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1910 |
| 2-109759-B-262.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0692 |
| 2-110417-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2782 |
| 2-110417-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4706 |
| 2-110417-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4787 |
| 2-110417-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5364 |
| 2-110417-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5297 |
| 2-110417-B-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5820 |
| 2-110613-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9239 |
| 2-110613-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4548 |
| 2-110613-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4084 |
| 2-110614-A-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3930 |
| 2-112213-A-393.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4570 |
| 2-112213-B-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8584 |
| 2-114609-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7454 |
| 2-117116-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9138 |
| 2-117116-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1826 |
| 2-117116-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6681 |
| 2-117116-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3700 |
| 2-117116-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5817 |
| 2-117330-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4020 |
| 2-117330-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7552 |
| 2-117330-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8961 |
| 2-117330-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2851 |
| 2-117330-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7423 |
| 2-117615-C-481.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0189 |
| 2-117625-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3300 |
| 2-117625-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5289 |
| 2-117625-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2874 |
| 2-117625-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9297 |
| 2-117625-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1734 |
| 2-118817-A-320.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9313 |
| 2-118817-A-321.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9629 |
| 2-118817-A-322.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9715 |
| 2-118817-A-323.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3103 |
| 2-118817-A-324.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4718 |
| 2-120218-A-303.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4246 |
| 2-121909-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0781 |
| 2-121909-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2344 |
| 2-121909-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1987 |
| 2-121909-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0707 |
| 2-122066-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1754 |
| 2-122066-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0692 |
| 2-122067-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2037 |
| 2-122067-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0086 |
| 2-122616-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5929 |
| 2-122616-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8009 |
| 2-122616-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9138 |
| 2-122616-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3270 |
| 2-122763-A-291.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3535 |
| 2-122763-A-292.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7739 |
| 2-122820-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3454 |
| 2-122820-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2975 |
| 2-122820-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2848 |
| 2-122820-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1736 |
| 2-122820-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6241 |
| 2-122820-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1703 |
| 2-122820-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1986 |
| 2-122820-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0940 |
| 2-122820-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8617 |
| 2-122820-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6176 |
| 2-124662-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6244 |
| 2-124662-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8087 |
| 2-125821-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7411 |
| 2-125821-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7855 |
| 2-125821-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8574 |
| 2-125821-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3264 |
| 2-125821-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9303 |
| 2-128465-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1007 |
| 2-128465-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3371 |
| 2-128465-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6349 |
| 2-130245-A-340.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1271 |
| 2-130245-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3458 |
| 2-130979-A-212.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0317 |
| 2-132157-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0268 |
| 2-132157-B-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0891 |
| 2-133863-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3112 |
| 2-134700-A-381.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0496 |
| 2-135649-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6455 |
| 2-135649-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5541 |
| 2-135649-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7967 |
| 2-135649-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9917 |
| 2-135649-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0925 |
| 2-135649-B-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8523 |
| 2-135649-B-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0258 |
| 2-135649-B-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9336 |
| 2-135649-B-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7620 |
| 2-135649-B-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5385 |
| 2-135649-C-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6521 |
| 2-135649-C-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8066 |
| 2-135649-C-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4122 |
| 2-135649-C-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3244 |
| 2-135649-C-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0034 |
| 2-135728-A-382.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9141 |
| 2-135728-A-384.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8732 |
| 2-137162-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8102 |
| 2-137162-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7261 |
| 2-137162-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9764 |
| 2-137162-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0721 |
| 2-138257-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2678 |
| 2-138257-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2380 |
| 2-138465-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3114 |
| 2-138465-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4906 |
| 2-138465-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0331 |
| 2-139749-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4190 |
| 2-141584-A-381.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9641 |
| 2-141584-A-382.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1000 |
| 2-141584-A-383.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1211 |
| 2-141584-A-384.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2464 |
| 2-141681-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3071 |
| 2-141681-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9106 |
| 2-141681-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2587 |
| 2-141681-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3702 |
| 2-141681-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8017 |
| 2-141681-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0412 |
| 2-141681-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5328 |
| 2-141681-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5143 |
| 2-141681-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2601 |
| 2-141681-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6606 |
| 2-141682-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9073 |
| 2-141682-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1728 |
| 2-141682-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9925 |
| 2-141682-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9626 |
| 2-141682-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2127 |
| 2-153388-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2145 |
| 2-155801-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8342 |
| 2-155801-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2639 |
| 2-155801-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5536 |
| 2-155801-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7771 |
| 2-155801-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6364 |
| 2-160128-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1337 |
| 2-160888-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3178 |
| 2-160888-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0853 |
| 2-173559-A-394.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1940 |
| 2-173607-A-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1098 |
| 2-173618-A-390.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5356 |
| 2-173618-A-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1692 |
| 2-173618-A-393.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6606 |
| 2-18766-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0227 |
| 2-18766-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2428 |
| 2-18766-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7888 |
| 2-18766-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6753 |
| 2-18766-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2717 |
| 2-18766-B-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1580 |
| 2-18766-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4817 |
| 2-18766-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3187 |
| 2-18766-B-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7825 |
| 2-188822-C-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8167 |
| 2-188822-C-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2566 |
| 2-188822-C-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9015 |
| 2-188822-C-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4110 |
| 2-188822-D-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5144 |
| 2-188822-D-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2919 |
| 2-188822-D-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5512 |
| 2-188822-D-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9190 |
| 2-188822-D-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9300 |
| 2-196688-A-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2028 |
| 2-196688-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8986 |
| 2-196688-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4481 |
| 2-250710-A-392.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3903 |
| 2-25292-A-221.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0176 |
| 2-25292-A-224.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3791 |
| 2-28314-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0013 |
| 2-28314-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6663 |
| 2-28314-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1702 |
| 2-28314-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7510 |
| 2-28314-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5962 |
| 2-28314-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6005 |
| 2-30322-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2291 |
| 2-30322-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1575 |
| 2-30322-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8827 |
| 2-30322-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5306 |
| 2-30322-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9360 |
| 2-30322-B-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5392 |
| 2-30322-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7293 |
| 2-30322-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3990 |
| 2-32515-B-44.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5366 |
| 2-37806-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2703 |
| 2-37806-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2625 |
| 2-37806-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9714 |
| 2-37806-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0664 |
| 2-37870-A-22.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2274 |
| 2-39441-A-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0414 |
| 2-39441-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0889 |
| 2-39441-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1817 |
| 2-39443-A-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5302 |
| 2-39443-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3074 |
| 2-39443-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4972 |
| 2-39945-B-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0571 |
| 2-42101-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6651 |
| 2-42101-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2528 |
| 2-43802-A-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4367 |
| 2-43802-A-423.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5957 |
| 2-50666-A-203.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2718 |
| 2-50667-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1011 |
| 2-50667-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4604 |
| 2-50667-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3652 |
| 2-50667-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4449 |
| 2-50667-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6788 |
| 2-50668-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2713 |
| 2-50668-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0624 |
| 2-50668-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0533 |
| 2-50668-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1117 |
| 2-50668-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0011 |
| 2-50668-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3164 |
| 2-50668-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6736 |
| 2-50668-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3662 |
| 2-50668-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7819 |
| 2-50668-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4319 |
| 2-51630-A-490.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9811 |
| 2-51630-A-491.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4051 |
| 2-51630-A-493.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1880 |
| 2-51630-B-490.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4283 |
| 2-51630-B-491.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1013 |
| 2-51630-B-492.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0589 |
| 2-51630-B-493.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2522 |
| 2-52001-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3497 |
| 2-52001-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9777 |
| 2-52001-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7312 |
| 2-52001-B-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0480 |
| 2-52001-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7836 |
| 2-52001-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2233 |
| 2-52001-B-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0172 |
| 2-54086-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3368 |
| 2-59241-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9342 |
| 2-59241-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0849 |
| 2-59241-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3129 |
| 2-59241-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4842 |
| 2-59241-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1873 |
| 2-64332-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9550 |
| 2-64332-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8148 |
| 2-65484-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3737 |
| 2-65484-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3620 |
| 2-65484-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3657 |
| 2-65484-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2457 |
| 2-65747-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7722 |
| 2-65747-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2613 |
| 2-65747-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0382 |
| 2-65747-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4259 |
| 2-65747-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1290 |
| 2-67422-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9683 |
| 2-67422-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6814 |
| 2-67422-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5025 |
| 2-68391-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9815 |
| 2-68391-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9950 |
| 2-68391-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3606 |
| 2-68391-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2988 |
| 2-68595-A-150.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3302 |
| 2-70366-A-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1097 |
| 2-70367-A-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4784 |
| 2-70367-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7799 |
| 2-70936-A-423.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0210 |
| 2-72547-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7509 |
| 2-72547-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2957 |
| 2-72547-B-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4720 |
| 2-72547-B-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2311 |
| 2-72547-C-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1063 |
| 2-72547-C-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9678 |
| 2-72547-C-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9190 |
| 2-72547-C-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9153 |
| 2-72547-D-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2052 |
| 2-72677-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8559 |
| 2-72970-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8649 |
| 2-72970-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7389 |
| 2-72970-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6183 |
| 2-72970-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2094 |
| 2-72970-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6105 |
| 2-73027-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6593 |
| 2-73027-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7605 |
| 2-73027-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7951 |
| 2-73027-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5150 |
| 2-73027-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5397 |
| 2-7321-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7776 |
| 2-7321-A-332.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0685 |
| 2-7321-A-333.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1536 |
| 2-7321-A-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1903 |
| 2-73260-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2509 |
| 2-73260-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0679 |
| 2-73260-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4126 |
| 2-74977-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9274 |
| 2-74977-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3556 |
| 2-74977-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0421 |
| 2-74977-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1690 |
| 2-77945-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4966 |
| 2-77945-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3359 |
| 2-77945-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1240 |
| 2-77945-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1592 |
| 2-77945-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7202 |
| 2-77945-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2469 |
| 2-77945-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8502 |
| 2-77945-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7649 |
| 2-77945-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9638 |
| 2-78562-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0756 |
| 2-78562-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0403 |
| 2-78562-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0100 |
| 2-78562-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0317 |
| 2-78562-B-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0929 |
| 2-78562-B-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0936 |
| 2-78651-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5255 |
| 2-78651-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1103 |
| 2-80313-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4400 |
| 2-80313-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8421 |
| 2-80313-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7518 |
| 2-80844-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8211 |
| 2-80844-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2310 |
| 2-80844-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9141 |
| 2-80844-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5748 |
| 2-80844-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3722 |
| 2-81112-A-340.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2412 |
| 2-81112-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7345 |
| 2-81112-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1889 |
| 2-81270-A-10.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1126 |
| 2-81731-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0891 |
| 2-81731-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5933 |
| 2-81731-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0544 |
| 2-81731-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1232 |
| 2-81731-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4687 |
| 2-81970-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3503 |
| 2-81970-C-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0899 |
| 2-82071-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1970 |
| 2-82071-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8760 |
| 2-82071-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2859 |
| 2-82071-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2799 |
| 2-82071-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9434 |
| 2-82274-A-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7358 |
| 2-82274-A-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9503 |
| 2-82274-A-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2422 |
| 2-82274-B-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4191 |
| 2-82367-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5486 |
| 2-82367-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7957 |
| 2-82367-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6882 |
| 2-82367-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9209 |
| 2-82367-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5952 |
| 2-83270-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.1368 |
| 2-83270-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.9290 |
| 2-83270-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4460 |
| 2-83270-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2109 |
| 2-83270-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0687 |
| 2-83536-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4772 |
| 2-83536-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0264 |
| 2-83667-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6211 |
| 2-83667-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1161 |
| 2-83688-A-341.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5645 |
| 2-83688-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0971 |
| 2-83688-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1291 |
| 2-83688-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3191 |
| 2-83934-B-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7471 |
| 2-84943-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1343 |
| 2-84943-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5634 |
| 2-84943-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7792 |
| 2-84965-A-231.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0701 |
| 2-85139-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4326 |
| 2-85139-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0036 |
| 2-85139-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2535 |
| 2-85139-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0668 |
| 2-85139-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8031 |
| 2-85292-A-243.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0397 |
| 2-85292-A-244.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5955 |
| 2-85434-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7649 |
| 2-85434-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3970 |
| 2-85471-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2011 |
| 2-85471-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4886 |
| 2-85471-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3779 |
| 2-85945-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7826 |
| 2-85945-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3670 |
| 2-86160-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2454 |
| 2-87282-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8596 |
| 2-87282-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4169 |
| 2-87282-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4627 |
| 2-87781-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6635 |
| 2-87781-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0968 |
| 2-87781-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0389 |
| 2-87781-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9470 |
| 2-87781-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7245 |
| 2-91912-B-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4576 |
| 2-94230-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4509 |
| 2-95258-B-13.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1955 |
| 2-95567-A-234.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2426 |
| 2-96033-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1735 |
| 2-96033-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1485 |
| 2-96033-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4474 |
| 2-96033-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6009 |
| 2-96033-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5049 |
| 2-96654-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1343 |
| 2-96654-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2232 |
| 2-96654-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3084 |
| 2-96654-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7278 |
| 2-96904-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6455 |
| 2-96904-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3083 |
| 2-96904-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3688 |
| 2-96904-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0636 |
| 2-98866-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0637 |
| 2-98866-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4902 |
| 2-99955-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1205 |
| 2-99955-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4110 |
| 2-99955-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5671 |
| 2-99955-C-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9436 |
| 2-99955-C-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4018 |
| 3-100018-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6468 |
| 3-100018-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2505 |
| 3-100018-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5341 |
| 3-100024-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3402 |
| 3-100024-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3977 |
| 3-100024-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4419 |
| 3-100024-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2994 |
| 3-100024-B-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3378 |
| 3-101381-A-332.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8928 |
| 3-101381-B-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5351 |
| 3-102908-A-43.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4889 |
| 3-103050-A-191.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0797 |
| 3-103050-A-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2008 |
| 3-103050-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1118 |
| 3-103051-A-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0223 |
| 3-103051-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5504 |
| 3-103051-B-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8987 |
| 3-103051-B-191.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2616 |
| 3-103051-B-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2067 |
| 3-103051-B-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1572 |
| 3-103597-A-250.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7238 |
| 3-103597-A-251.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5870 |
| 3-103597-A-252.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5545 |
| 3-103597-A-253.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4151 |
| 3-103598-A-252.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0109 |
| 3-103598-A-253.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0472 |
| 3-103599-B-254.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2746 |
| 3-104761-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8667 |
| 3-104761-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2825 |
| 3-104761-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3084 |
| 3-104761-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1534 |
| 3-104761-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8969 |
| 3-104761-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8402 |
| 3-104761-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2234 |
| 3-104958-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0754 |
| 3-104958-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0484 |
| 3-104958-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1182 |
| 3-105235-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7599 |
| 3-105235-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4794 |
| 3-105236-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0028 |
| 3-105236-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6431 |
| 3-105236-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3872 |
| 3-105236-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4464 |
| 3-108451-A-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2607 |
| 3-108451-B-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5276 |
| 3-108451-B-171.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3236 |
| 3-108451-B-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9723 |
| 3-108677-A-180.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3771 |
| 3-108677-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8178 |
| 3-108677-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3331 |
| 3-108677-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5469 |
| 3-108791-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8616 |
| 3-108791-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3240 |
| 3-108791-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1322 |
| 3-110913-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5197 |
| 3-110913-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0687 |
| 3-110913-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4267 |
| 3-110913-B-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1711 |
| 3-110913-B-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6886 |
| 3-110913-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8583 |
| 3-110913-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2760 |
| 3-110913-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8953 |
| 3-110913-C-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0764 |
| 3-110913-C-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1488 |
| 3-110913-C-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6435 |
| 3-110913-C-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7159 |
| 3-110913-C-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1825 |
| 3-110913-D-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4840 |
| 3-110913-D-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9396 |
| 3-110913-D-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9754 |
| 3-112356-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5264 |
| 3-112522-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1969 |
| 3-112522-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3970 |
| 3-112522-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2569 |
| 3-112522-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8985 |
| 3-115382-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0952 |
| 3-115382-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6287 |
| 3-115382-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7725 |
| 3-115382-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5429 |
| 3-115382-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0104 |
| 3-117293-A-92.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4170 |
| 3-118059-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2165 |
| 3-118069-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0075 |
| 3-118069-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3723 |
| 3-118069-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8730 |
| 3-118069-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2080 |
| 3-118069-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8427 |
| 3-118069-B-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4122 |
| 3-118069-B-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0564 |
| 3-118487-A-263.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6447 |
| 3-118487-A-264.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5591 |
| 3-118656-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4860 |
| 3-118656-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4626 |
| 3-118656-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3670 |
| 3-118657-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8354 |
| 3-118657-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8988 |
| 3-118657-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1808 |
| 3-118657-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6052 |
| 3-118658-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5740 |
| 3-118658-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5974 |
| 3-118658-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5723 |
| 3-118658-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4615 |
| 3-118658-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4498 |
| 3-118972-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3769 |
| 3-118972-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6859 |
| 3-118972-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2464 |
| 3-118972-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2190 |
| 3-118972-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5340 |
| 3-118972-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2935 |
| 3-118972-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4199 |
| 3-119120-A-484.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3379 |
| 3-119120-B-483.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9399 |
| 3-119120-B-484.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3725 |
| 3-119455-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8218 |
| 3-119455-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9032 |
| 3-119455-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4915 |
| 3-119455-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8929 |
| 3-119455-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9462 |
| 3-120644-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5122 |
| 3-120644-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2206 |
| 3-120644-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9407 |
| 3-120644-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7251 |
| 3-123086-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6365 |
| 3-123086-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7873 |
| 3-123086-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2680 |
| 3-123086-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8773 |
| 3-123224-A-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3105 |
| 3-124600-A-190.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1681 |
| 3-124600-A-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1190 |
| 3-124600-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0703 |
| 3-124600-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5205 |
| 3-124958-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6853 |
| 3-124958-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7220 |
| 3-124958-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5068 |
| 3-124958-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5933 |
| 3-124958-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9706 |
| 3-126391-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0275 |
| 3-128512-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3581 |
| 3-128512-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3458 |
| 3-128512-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3839 |
| 3-128512-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7876 |
| 3-129338-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3961 |
| 3-129338-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2300 |
| 3-129338-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7148 |
| 3-129338-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0772 |
| 3-129338-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8627 |
| 3-129678-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.1176 |
| 3-129678-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.3688 |
| 3-129678-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.0646 |
| 3-129678-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.3993 |
| 3-129678-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.2388 |
| 3-130998-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9851 |
| 3-130998-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0524 |
| 3-130998-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0071 |
| 3-130998-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3777 |
| 3-130998-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2786 |
| 3-130998-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4278 |
| 3-130998-B-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6071 |
| 3-130998-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8751 |
| 3-130998-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6897 |
| 3-130998-B-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1931 |
| 3-131943-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6469 |
| 3-131943-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6889 |
| 3-131943-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3667 |
| 3-131943-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5604 |
| 3-131943-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3752 |
| 3-132340-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3442 |
| 3-132340-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5186 |
| 3-132340-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8192 |
| 3-132340-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3869 |
| 3-132340-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2810 |
| 3-132747-A-263.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5539 |
| 3-134699-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0634 |
| 3-134699-B-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6764 |
| 3-134699-B-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3078 |
| 3-134699-B-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5486 |
| 3-134699-B-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7879 |
| 3-134699-C-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2544 |
| 3-134699-C-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0542 |
| 3-134699-C-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2555 |
| 3-134802-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3389 |
| 3-134802-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1296 |
| 3-134802-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5977 |
| 3-135650-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5188 |
| 3-135650-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1897 |
| 3-135650-B-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2204 |
| 3-135650-B-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4755 |
| 3-135650-B-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3774 |
| 3-136608-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9893 |
| 3-136608-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7868 |
| 3-136608-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3814 |
| 3-136608-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1901 |
| 3-139958-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2804 |
| 3-139958-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5515 |
| 3-139958-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5108 |
| 3-139958-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6134 |
| 3-140199-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6290 |
| 3-141240-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4258 |
| 3-141240-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6675 |
| 3-141240-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7594 |
| 3-141240-B-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5492 |
| 3-141240-B-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7423 |
| 3-141240-B-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9540 |
| 3-141240-B-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4405 |
| 3-142005-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1895 |
| 3-142005-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7210 |
| 3-142005-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2239 |
| 3-142005-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5604 |
| 3-142006-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1722 |
| 3-142006-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8455 |
| 3-142006-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7547 |
| 3-142006-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9481 |
| 3-142006-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0345 |
| 3-142349-A-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2488 |
| 3-142349-A-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8695 |
| 3-142349-A-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7291 |
| 3-142601-A-211.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6149 |
| 3-142605-A-210.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9968 |
| 3-142605-A-212.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5415 |
| 3-142605-A-213.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9154 |
| 3-143119-A-212.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8924 |
| 3-143562-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1212 |
| 3-143929-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8491 |
| 3-143929-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9154 |
| 3-143929-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8500 |
| 3-143929-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2696 |
| 3-143929-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3062 |
| 3-144120-A-320.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2439 |
| 3-144120-A-324.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4203 |
| 3-144827-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4772 |
| 3-144827-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4891 |
| 3-144827-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6863 |
| 3-144827-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0968 |
| 3-144827-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4580 |
| 3-145387-A-292.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1657 |
| 3-145719-A-170.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1386 |
| 3-145719-A-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7367 |
| 3-145774-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0665 |
| 3-145774-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4085 |
| 3-145774-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5299 |
| 3-145774-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4263 |
| 3-145774-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4501 |
| 3-146964-A-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3256 |
| 3-147965-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8785 |
| 3-147965-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2671 |
| 3-147965-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2974 |
| 3-147965-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1763 |
| 3-147965-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5828 |
| 3-151081-A-203.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3222 |
| 3-151269-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2006 |
| 3-151269-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1133 |
| 3-151269-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2974 |
| 3-151269-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2900 |
| 3-151273-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2861 |
| 3-151273-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2710 |
| 3-151273-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6399 |
| 3-151273-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1090 |
| 3-151273-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2200 |
| 3-151557-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9201 |
| 3-151557-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8105 |
| 3-151557-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3444 |
| 3-151557-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7971 |
| 3-151557-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6223 |
| 3-151557-B-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6425 |
| 3-151557-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3785 |
| 3-151557-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4350 |
| 3-151557-B-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7198 |
| 3-152007-A-202.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4977 |
| 3-152020-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3023 |
| 3-152020-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8759 |
| 3-152020-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2147 |
| 3-152020-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9031 |
| 3-152020-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2065 |
| 3-152020-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9901 |
| 3-152020-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2887 |
| 3-152020-C-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8007 |
| 3-152020-C-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8347 |
| 3-152039-B-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9499 |
| 3-155130-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5644 |
| 3-155130-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4379 |
| 3-155130-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1227 |
| 3-155234-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0835 |
| 3-155234-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1334 |
| 3-155234-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0792 |
| 3-155234-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0546 |
| 3-155577-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1992 |
| 3-155577-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3932 |
| 3-155577-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9425 |
| 3-155577-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3914 |
| 3-155577-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7660 |
| 3-155578-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1150 |
| 3-155578-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8611 |
| 3-155578-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0927 |
| 3-155578-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1443 |
| 3-155579-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9454 |
| 3-155579-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6712 |
| 3-155579-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0736 |
| 3-155579-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5015 |
| 3-155583-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5371 |
| 3-155583-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2947 |
| 3-155583-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0475 |
| 3-155584-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1514 |
| 3-155584-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5053 |
| 3-155584-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0002 |
| 3-155584-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7530 |
| 3-155642-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2393 |
| 3-155642-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0086 |
| 3-155642-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2873 |
| 3-155642-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3659 |
| 3-155642-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3756 |
| 3-156391-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9532 |
| 3-156391-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4314 |
| 3-156391-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8560 |
| 3-156391-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7668 |
| 3-156391-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8572 |
| 3-156393-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2820 |
| 3-156393-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8125 |
| 3-156393-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7710 |
| 3-156393-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4187 |
| 3-156393-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3452 |
| 3-156581-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0824 |
| 3-156581-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0939 |
| 3-156581-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9863 |
| 3-156581-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9625 |
| 3-156581-B-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5241 |
| 3-156581-B-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.4922 |
| 3-156581-B-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0820 |
| 3-156581-B-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0402 |
| 3-156581-B-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4487 |
| 3-157149-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4431 |
| 3-157149-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5027 |
| 3-157149-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3701 |
| 3-157149-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5769 |
| 3-157149-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2845 |
| 3-157187-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5806 |
| 3-157187-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0769 |
| 3-157487-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9998 |
| 3-157487-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9281 |
| 3-157487-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6369 |
| 3-157487-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1076 |
| 3-157487-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0175 |
| 3-157615-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2502 |
| 3-157615-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2265 |
| 3-157615-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0306 |
| 3-157615-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2528 |
| 3-157615-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3627 |
| 3-158056-B-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2973 |
| 3-158476-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6758 |
| 3-158476-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1423 |
| 3-159346-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2953 |
| 3-159346-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4016 |
| 3-159346-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3072 |
| 3-159346-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4303 |
| 3-159346-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7642 |
| 3-159346-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6644 |
| 3-159346-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.7519 |
| 3-159346-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2299 |
| 3-159346-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9436 |
| 3-159346-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9803 |
| 3-159347-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4651 |
| 3-159347-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2938 |
| 3-159347-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4240 |
| 3-159347-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9992 |
| 3-159347-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9170 |
| 3-159347-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1040 |
| 3-159347-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1723 |
| 3-159348-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8626 |
| 3-159348-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2703 |
| 3-161010-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6586 |
| 3-161010-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6397 |
| 3-161010-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1670 |
| 3-161010-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3542 |
| 3-161010-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7734 |
| 3-161500-A-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2091 |
| 3-163607-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0557 |
| 3-163607-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4549 |
| 3-163607-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0065 |
| 3-163607-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5485 |
| 3-163607-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4321 |
| 3-163607-B-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3197 |
| 3-163607-B-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1828 |
| 3-163607-B-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9615 |
| 3-163607-B-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8807 |
| 3-163607-B-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1011 |
| 3-164120-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5593 |
| 3-164120-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7280 |
| 3-164120-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0969 |
| 3-164120-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1922 |
| 3-164120-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5903 |
| 3-164592-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8366 |
| 3-164593-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7070 |
| 3-164630-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1431 |
| 3-164630-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3281 |
| 3-164630-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0644 |
| 3-164630-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0117 |
| 3-165856-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3203 |
| 3-166125-A-234.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9837 |
| 3-166125-B-232.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4135 |
| 3-166324-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6916 |
| 3-166324-A-153.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0333 |
| 3-166422-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2711 |
| 3-166422-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0792 |
| 3-166422-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8707 |
| 3-166422-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5199 |
| 3-166422-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1084 |
| 3-166546-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7807 |
| 3-170002-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3541 |
| 3-170312-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3967 |
| 3-170851-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2608 |
| 3-171281-A-63.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5448 |
| 3-171281-A-64.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5796 |
| 3-171937-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7621 |
| 3-171937-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7611 |
| 3-171937-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2190 |
| 3-180147-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2109 |
| 3-181132-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5306 |
| 3-182023-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8528 |
| 3-182710-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3505 |
| 3-182710-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3374 |
| 3-182710-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3760 |
| 3-182710-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6513 |
| 3-182710-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4563 |
| 3-182710-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4445 |
| 3-182710-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9677 |
| 3-187549-B-63.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3386 |
| 3-187710-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9023 |
| 3-188726-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0912 |
| 3-188726-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1896 |
| 3-188726-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3278 |
| 3-188726-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5098 |
| 3-197408-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3511 |
| 3-197408-C-81.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3824 |
| 3-197408-C-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0041 |
| 3-197408-C-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3987 |
| 3-197408-C-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6283 |
| 3-208820-A-490.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5502 |
| 3-208820-A-491.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3039 |
| 3-208820-A-492.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2855 |
| 3-208820-A-493.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0512 |
| 3-208820-A-494.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1133 |
| 3-233151-A-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0442 |
| 3-259622-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1430 |
| 3-259622-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0496 |
| 3-259622-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3355 |
| 3-259622-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0047 |
| 3-259622-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4639 |
| 3-65748-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8232 |
| 3-65748-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3737 |
| 3-65748-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8394 |
| 3-65748-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3300 |
| 3-65748-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6301 |
| 3-68630-A-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2074 |
| 3-68630-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1929 |
| 3-68630-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1364 |
| 3-68630-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1163 |
| 3-83527-A-42.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4598 |
| 3-92637-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1616 |
| 3-93010-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5173 |
| 3-93010-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7312 |
| 3-93010-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3107 |
| 3-93010-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2270 |
| 3-94342-A-250.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1692 |
| 3-94342-A-253.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0662 |
| 3-94342-A-254.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0787 |
| 3-94344-A-250.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3749 |
| 3-94344-A-251.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3234 |
| 3-94344-A-252.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6399 |
| 3-94355-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5363 |
| 3-94355-A-333.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2513 |
| 3-94355-A-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4893 |
| 3-95694-A-50.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8028 |
| 3-95694-A-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4737 |
| 3-95695-A-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2442 |
| 3-95695-A-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4304 |
| 3-95695-B-54.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9776 |
| 3-95698-A-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3445 |
| 3-96606-A-490.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1505 |
| 3-96606-A-491.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1809 |
| 3-96606-A-492.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2090 |
| 3-96606-A-493.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2911 |
| 3-96606-B-493.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0158 |
| 3-96606-B-494.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3223 |
| 3-98771-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1463 |
| 3-98771-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1632 |
| 3-98771-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1561 |
| 4-102844-A-490.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0077 |
| 4-102844-A-494.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0903 |
| 4-107117-A-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4285 |
| 4-107117-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2238 |
| 4-107117-A-332.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9158 |
| 4-107117-A-333.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6457 |
| 4-107117-A-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6686 |
| 4-107120-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0001 |
| 4-107122-A-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0778 |
| 4-107122-A-331.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2885 |
| 4-108352-A-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3221 |
| 4-119647-A-482.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2754 |
| 4-119647-A-483.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7933 |
| 4-119647-B-483.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5748 |
| 4-119647-B-484.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4786 |
| 4-119720-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1725 |
| 4-119720-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1358 |
| 4-119720-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2704 |
| 4-119720-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1551 |
| 4-125070-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3344 |
| 4-125070-A-194.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9818 |
| 4-125929-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2591 |
| 4-126046-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3992 |
| 4-126046-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3505 |
| 4-126046-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5796 |
| 4-126532-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2402 |
| 4-126532-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9285 |
| 4-126532-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0880 |
| 4-126532-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6075 |
| 4-132803-A-263.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7497 |
| 4-135439-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1789 |
| 4-135439-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7334 |
| 4-135439-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2686 |
| 4-135439-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0670 |
| 4-141365-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3965 |
| 4-141365-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0937 |
| 4-141365-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6036 |
| 4-141365-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7111 |
| 4-144468-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1560 |
| 4-144468-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1038 |
| 4-144468-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1423 |
| 4-144468-B-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6310 |
| 4-145006-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7110 |
| 4-145006-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7104 |
| 4-145006-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5102 |
| 4-145006-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2078 |
| 4-145081-A-91.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4234 |
| 4-146200-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9208 |
| 4-146200-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4178 |
| 4-146200-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5388 |
| 4-146200-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5615 |
| 4-146200-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7018 |
| 4-149294-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5901 |
| 4-149294-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2060 |
| 4-149294-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9799 |
| 4-149294-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7823 |
| 4-149294-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0038 |
| 4-149294-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2832 |
| 4-149294-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1631 |
| 4-152958-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8728 |
| 4-154405-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6476 |
| 4-154405-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4544 |
| 4-154405-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8631 |
| 4-156227-A-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1587 |
| 4-156993-A-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1052 |
| 4-157611-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2062 |
| 4-157611-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2522 |
| 4-157611-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0750 |
| 4-157611-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0876 |
| 4-157611-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0648 |
| 4-159609-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5082 |
| 4-159609-B-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2345 |
| 4-160999-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8696 |
| 4-160999-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2663 |
| 4-160999-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7372 |
| 4-160999-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3130 |
| 4-161100-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5330 |
| 4-161100-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3820 |
| 4-161127-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9122 |
| 4-161127-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0293 |
| 4-161127-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6814 |
| 4-161127-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8971 |
| 4-161127-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9586 |
| 4-161303-A-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3338 |
| 4-161579-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0205 |
| 4-161579-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1023 |
| 4-161579-B-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1583 |
| 4-161579-B-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0588 |
| 4-163264-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9053 |
| 4-163264-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1540 |
| 4-163264-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0005 |
| 4-163264-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6413 |
| 4-163264-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5044 |
| 4-163608-B-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1726 |
| 4-163608-B-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3403 |
| 4-163608-B-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0032 |
| 4-163608-B-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8595 |
| 4-163697-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3548 |
| 4-163697-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0956 |
| 4-163697-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3335 |
| 4-163697-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9412 |
| 4-163697-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8203 |
| 4-164064-C-11.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0533 |
| 4-164206-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6379 |
| 4-164206-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8382 |
| 4-164206-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3151 |
| 4-164206-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4844 |
| 4-164206-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4220 |
| 4-164661-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0220 |
| 4-164661-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0810 |
| 4-164661-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3664 |
| 4-164661-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6284 |
| 4-164661-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8520 |
| 4-164661-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6872 |
| 4-164661-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0432 |
| 4-165606-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5924 |
| 4-165606-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3003 |
| 4-165606-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6754 |
| 4-165606-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3014 |
| 4-165813-B-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0095 |
| 4-165823-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1680 |
| 4-165823-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1864 |
| 4-165823-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8184 |
| 4-165823-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9322 |
| 4-166661-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5805 |
| 4-166661-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4737 |
| 4-166661-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7818 |
| 4-166661-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5181 |
| 4-166661-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6288 |
| 4-167063-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0750 |
| 4-167063-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1343 |
| 4-167063-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5538 |
| 4-167063-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1808 |
| 4-167063-B-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5984 |
| 4-167063-C-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3966 |
| 4-169127-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4165 |
| 4-169127-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3391 |
| 4-169127-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2050 |
| 4-169127-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2467 |
| 4-169127-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4673 |
| 4-169508-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7954 |
| 4-169508-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1002 |
| 4-170247-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0471 |
| 4-170247-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0433 |
| 4-170247-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4676 |
| 4-170247-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2738 |
| 4-171207-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3638 |
| 4-171396-A-242.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4600 |
| 4-171652-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9475 |
| 4-171652-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7355 |
| 4-171652-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7197 |
| 4-171652-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7490 |
| 4-171652-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8709 |
| 4-172180-A-322.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0572 |
| 4-172500-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3121 |
| 4-172500-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6805 |
| 4-172500-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6012 |
| 4-172500-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2801 |
| 4-172500-B-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3620 |
| 4-172500-B-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9854 |
| 4-172500-C-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1230 |
| 4-172500-C-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4792 |
| 4-172500-C-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5980 |
| 4-172500-C-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8885 |
| 4-172500-C-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3265 |
| 4-172500-D-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1483 |
| 4-172500-D-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3669 |
| 4-172500-D-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1802 |
| 4-172500-D-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1149 |
| 4-172732-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4000 |
| 4-172732-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2778 |
| 4-172732-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3284 |
| 4-172732-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2311 |
| 4-172732-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2900 |
| 4-172733-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7708 |
| 4-172733-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8652 |
| 4-172733-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7178 |
| 4-172733-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7360 |
| 4-172733-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7535 |
| 4-172734-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6419 |
| 4-172734-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5503 |
| 4-172736-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7630 |
| 4-172736-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3887 |
| 4-172736-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0700 |
| 4-172736-B-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5113 |
| 4-172736-B-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0165 |
| 4-172736-B-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6350 |
| 4-172736-B-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3143 |
| 4-172736-B-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1531 |
| 4-173865-B-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3239 |
| 4-174797-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7307 |
| 4-174860-A-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9817 |
| 4-174860-B-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1583 |
| 4-174860-B-33.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8470 |
| 4-174860-B-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3360 |
| 4-175025-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1983 |
| 4-175025-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9709 |
| 4-175025-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1280 |
| 4-176638-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3722 |
| 4-176638-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2822 |
| 4-177250-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2179 |
| 4-177250-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5141 |
| 4-177250-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9316 |
| 4-177250-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1298 |
| 4-177250-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0513 |
| 4-178402-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5346 |
| 4-178402-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4017 |
| 4-178402-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3817 |
| 4-180337-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4582 |
| 4-180380-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2854 |
| 4-180380-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3799 |
| 4-180380-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4037 |
| 4-180380-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3947 |
| 4-180380-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2409 |
| 4-180453-A-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0551 |
| 4-181286-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9287 |
| 4-181286-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3882 |
| 4-181286-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2019 |
| 4-181286-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3171 |
| 4-181286-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3459 |
| 4-181362-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5663 |
| 4-181362-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5929 |
| 4-181362-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8241 |
| 4-181362-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5447 |
| 4-181362-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7006 |
| 4-181563-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0705 |
| 4-181563-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2822 |
| 4-181563-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3474 |
| 4-181563-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7757 |
| 4-181563-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5012 |
| 4-181628-A-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3681 |
| 4-181999-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3217 |
| 4-181999-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1054 |
| 4-181999-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2664 |
| 4-181999-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6249 |
| 4-181999-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5412 |
| 4-182034-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6118 |
| 4-182034-A-303.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9164 |
| 4-182039-A-301.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5815 |
| 4-182314-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6440 |
| 4-182314-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7658 |
| 4-182314-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3039 |
| 4-182314-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6670 |
| 4-182314-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8415 |
| 4-182314-B-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0643 |
| 4-182314-B-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2483 |
| 4-182314-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0927 |
| 4-182314-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0784 |
| 4-182368-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4897 |
| 4-182368-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1976 |
| 4-182368-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4037 |
| 4-182369-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4416 |
| 4-182369-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6695 |
| 4-182369-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3255 |
| 4-182369-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1480 |
| 4-182613-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1952 |
| 4-182613-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8937 |
| 4-182613-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5120 |
| 4-182613-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6874 |
| 4-182613-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4535 |
| 4-182613-B-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9638 |
| 4-182613-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1198 |
| 4-182613-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1854 |
| 4-182613-B-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7475 |
| 4-182795-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8204 |
| 4-182795-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0501 |
| 4-182795-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7187 |
| 4-182795-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8169 |
| 4-182795-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2164 |
| 4-183882-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2404 |
| 4-183882-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0794 |
| 4-183882-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6591 |
| 4-183882-B-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4899 |
| 4-183882-B-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6052 |
| 4-184235-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0493 |
| 4-184235-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2177 |
| 4-184235-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2292 |
| 4-184237-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4786 |
| 4-184434-A-210.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0676 |
| 4-185575-A-203.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7811 |
| 4-186518-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8973 |
| 4-186693-A-174.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2440 |
| 4-186935-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3376 |
| 4-186935-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1398 |
| 4-186935-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2139 |
| 4-186936-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8218 |
| 4-186936-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4936 |
| 4-186936-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8582 |
| 4-186936-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0536 |
| 4-186936-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0736 |
| 4-186938-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4704 |
| 4-186938-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5216 |
| 4-186940-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6900 |
| 4-186940-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4824 |
| 4-186940-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7337 |
| 4-186962-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6943 |
| 4-186962-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6208 |
| 4-186962-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1457 |
| 4-186962-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6423 |
| 4-186962-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2285 |
| 4-188003-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4651 |
| 4-188003-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2018 |
| 4-188003-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5163 |
| 4-188293-B-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9489 |
| 4-188878-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7288 |
| 4-188878-A-303.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8239 |
| 4-188878-A-304.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0212 |
| 4-191015-A-432.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6992 |
| 4-191015-A-433.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0713 |
| 4-191015-A-434.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1703 |
| 4-191246-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2342 |
| 4-191297-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0669 |
| 4-193480-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2955 |
| 4-193480-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7167 |
| 4-193480-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3672 |
| 4-193480-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4735 |
| 4-193480-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8388 |
| 4-193480-B-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4127 |
| 4-194246-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8569 |
| 4-194246-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5509 |
| 4-194246-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4787 |
| 4-194246-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5944 |
| 4-194246-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4676 |
| 4-194680-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2850 |
| 4-194680-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7367 |
| 4-194680-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5159 |
| 4-194680-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6684 |
| 4-194680-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3690 |
| 4-194711-A-383.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0525 |
| 4-194711-A-384.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0351 |
| 4-194979-A-251.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0462 |
| 4-194979-A-252.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1434 |
| 4-195451-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8502 |
| 4-195451-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3871 |
| 4-195451-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3209 |
| 4-195451-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5262 |
| 4-195451-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3049 |
| 4-195497-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0844 |
| 4-195497-B-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1731 |
| 4-195707-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.8874 |
| 4-195707-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6690 |
| 4-195707-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4650 |
| 4-195707-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0473 |
| 4-195707-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3828 |
| 4-195805-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7489 |
| 4-195805-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4754 |
| 4-195805-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3358 |
| 4-195805-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6231 |
| 4-195805-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8899 |
| 4-196672-A-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6284 |
| 4-196672-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0232 |
| 4-196672-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4637 |
| 4-197454-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0339 |
| 4-198841-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0820 |
| 4-198841-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9467 |
| 4-198841-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9052 |
| 4-198841-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6162 |
| 4-201988-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5096 |
| 4-201988-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4562 |
| 4-201988-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7167 |
| 4-201988-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3618 |
| 4-201988-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5619 |
| 4-202749-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.7593 |
| 4-202749-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2691 |
| 4-202749-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.7365 |
| 4-202749-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9524 |
| 4-202749-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.3423 |
| 4-204121-A-391.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3961 |
| 4-204618-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1487 |
| 4-204683-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1573 |
| 4-204684-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8201 |
| 4-204684-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3876 |
| 4-204684-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6460 |
| 4-204684-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9024 |
| 4-204684-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3545 |
| 4-209536-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2963 |
| 4-209536-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1639 |
| 4-209536-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1281 |
| 4-209536-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4195 |
| 4-210000-A-232.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3894 |
| 4-210000-A-233.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5978 |
| 4-210000-B-233.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4700 |
| 4-210593-A-291.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0548 |
| 4-210593-A-293.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8979 |
| 4-212604-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7737 |
| 4-212604-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7935 |
| 4-212604-A-153.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9566 |
| 4-212604-A-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2369 |
| 4-212604-B-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2250 |
| 4-216211-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5547 |
| 4-216211-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3621 |
| 4-216211-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4261 |
| 4-216211-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0020 |
| 4-216211-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7939 |
| 4-218199-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6787 |
| 4-218199-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8250 |
| 4-218199-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6620 |
| 4-218199-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5132 |
| 4-218199-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2130 |
| 4-218199-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3025 |
| 4-218199-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1074 |
| 4-218199-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2859 |
| 4-218199-C-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6196 |
| 4-218199-C-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6578 |
| 4-218199-C-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7801 |
| 4-218199-C-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8609 |
| 4-218199-C-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7878 |
| 4-218199-F-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2459 |
| 4-218199-F-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1403 |
| 4-218199-G-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5907 |
| 4-218199-G-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8162 |
| 4-218199-G-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0907 |
| 4-218199-G-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7383 |
| 4-218199-H-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2744 |
| 4-218199-H-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5738 |
| 4-218199-H-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0189 |
| 4-218199-H-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3888 |
| 4-218199-H-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.9057 |
| 4-223125-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.7855 |
| 4-223125-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4833 |
| 4-223125-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5984 |
| 4-223125-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6459 |
| 4-223125-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3401 |
| 4-223127-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0248 |
| 4-223127-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1481 |
| 4-223127-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1127 |
| 4-223127-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7522 |
| 4-223127-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3883 |
| 4-234879-A-61.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0156 |
| 4-234879-A-62.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8077 |
| 4-250864-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5917 |
| 4-250869-A-21.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2163 |
| 4-250869-A-22.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4307 |
| 4-250869-A-24.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9253 |
| 4-250869-B-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6955 |
| 4-250869-C-20.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3139 |
| 4-251959-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0198 |
| 4-251959-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4945 |
| 4-251959-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2417 |
| 4-261068-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9024 |
| 4-264453-A-341.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8324 |
| 4-90014-B-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1389 |
| 4-90014-B-424.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6534 |
| 5-117120-A-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2263 |
| 5-117773-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9551 |
| 5-117773-A-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5730 |
| 5-117773-A-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2004 |
| 5-117773-A-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9618 |
| 5-117773-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8930 |
| 5-133989-A-420.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5488 |
| 5-133989-A-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0189 |
| 5-133989-A-422.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3497 |
| 5-133989-A-423.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7005 |
| 5-133989-A-424.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.8557 |
| 5-133989-B-420.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5642 |
| 5-133989-B-421.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9428 |
| 5-133989-B-422.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1377 |
| 5-141683-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4726 |
| 5-141683-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3639 |
| 5-141683-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2713 |
| 5-141683-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0508 |
| 5-141683-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5175 |
| 5-147297-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7754 |
| 5-156698-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2332 |
| 5-157204-A-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4927 |
| 5-157204-B-160.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3169 |
| 5-157204-B-161.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0922 |
| 5-157204-B-162.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9439 |
| 5-157204-B-163.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5631 |
| 5-160614-H-480.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6998 |
| 5-161270-B-330.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3879 |
| 5-161270-B-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0761 |
| 5-169983-A-54.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0524 |
| 5-170338-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9945 |
| 5-170338-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3901 |
| 5-170338-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3306 |
| 5-170338-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6034 |
| 5-170338-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6776 |
| 5-170338-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5915 |
| 5-170338-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3534 |
| 5-170338-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0438 |
| 5-170338-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3715 |
| 5-170338-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2960 |
| 5-171653-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1547 |
| 5-171653-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3361 |
| 5-177034-A-180.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9063 |
| 5-177034-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1201 |
| 5-177034-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3523 |
| 5-177779-A-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3640 |
| 5-177957-A-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4739 |
| 5-177957-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0418 |
| 5-177957-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2083 |
| 5-177957-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1852 |
| 5-177957-B-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7837 |
| 5-177957-B-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9721 |
| 5-177957-B-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9961 |
| 5-177957-B-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9005 |
| 5-177957-B-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6696 |
| 5-177957-C-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1690 |
| 5-177957-E-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0385 |
| 5-177957-E-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3404 |
| 5-177957-E-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1471 |
| 5-177957-E-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1984 |
| 5-177957-E-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4676 |
| 5-178997-A-242.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5280 |
| 5-179496-A-164.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0051 |
| 5-180156-A-430.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7928 |
| 5-180156-A-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7910 |
| 5-180156-C-431.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0011 |
| 5-180229-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6682 |
| 5-180229-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5058 |
| 5-181458-A-332.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2452 |
| 5-181766-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2949 |
| 5-181766-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2666 |
| 5-181766-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9967 |
| 5-181766-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8860 |
| 5-181766-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1010 |
| 5-182007-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9169 |
| 5-182007-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0110 |
| 5-182007-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9210 |
| 5-182007-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8667 |
| 5-182007-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0155 |
| 5-182010-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4736 |
| 5-182010-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4137 |
| 5-182010-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4789 |
| 5-182010-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2648 |
| 5-182010-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6199 |
| 5-182012-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6760 |
| 5-182012-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8686 |
| 5-182012-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0475 |
| 5-182012-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0040 |
| 5-182012-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0959 |
| 5-182404-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5727 |
| 5-185516-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5911 |
| 5-185516-A-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.2358 |
| 5-185516-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7258 |
| 5-185516-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2097 |
| 5-185516-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4648 |
| 5-185579-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8004 |
| 5-185579-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4715 |
| 5-185579-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0883 |
| 5-185579-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3615 |
| 5-185579-B-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5175 |
| 5-185579-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1426 |
| 5-185579-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8412 |
| 5-185908-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4845 |
| 5-185908-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6486 |
| 5-185908-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7107 |
| 5-186924-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6570 |
| 5-186924-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2055 |
| 5-186924-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1403 |
| 5-186924-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1308 |
| 5-186924-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2006 |
| 5-187201-B-44.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0556 |
| 5-187444-A-334.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7531 |
| 5-188365-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6703 |
| 5-188365-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7093 |
| 5-188365-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5234 |
| 5-188365-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7028 |
| 5-188365-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7516 |
| 5-188495-A-192.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3378 |
| 5-188495-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2123 |
| 5-188655-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0054 |
| 5-188655-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8761 |
| 5-188655-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3816 |
| 5-188655-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3561 |
| 5-188655-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9249 |
| 5-189212-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3088 |
| 5-189212-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3685 |
| 5-189212-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4909 |
| 5-189212-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0149 |
| 5-191131-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0339 |
| 5-192191-A-193.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2560 |
| 5-193339-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4534 |
| 5-193339-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8690 |
| 5-193339-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9858 |
| 5-193339-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9702 |
| 5-193339-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7987 |
| 5-193473-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4791 |
| 5-193473-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0761 |
| 5-193473-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3180 |
| 5-193473-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7952 |
| 5-193473-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6553 |
| 5-193473-B-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7938 |
| 5-193473-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4161 |
| 5-193473-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1479 |
| 5-193473-B-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5700 |
| 5-194892-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1349 |
| 5-194892-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2346 |
| 5-194892-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1735 |
| 5-194892-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4879 |
| 5-194892-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2701 |
| 5-194899-C-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4383 |
| 5-194930-A-13.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2766 |
| 5-194930-A-14.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6188 |
| 5-194930-B-13.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4509 |
| 5-194932-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6456 |
| 5-195517-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0962 |
| 5-195517-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3052 |
| 5-195517-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1799 |
| 5-195518-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4942 |
| 5-195518-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6429 |
| 5-195518-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4953 |
| 5-195710-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8298 |
| 5-195710-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5151 |
| 5-195710-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7566 |
| 5-195710-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3319 |
| 5-195710-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0022 |
| 5-197118-A-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8573 |
| 5-197118-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3340 |
| 5-197118-A-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2019 |
| 5-197121-A-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0964 |
| 5-197121-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2826 |
| 5-198278-A-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7143 |
| 5-198278-A-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6520 |
| 5-198278-A-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0521 |
| 5-198278-A-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6717 |
| 5-198278-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6978 |
| 5-198278-B-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2131 |
| 5-198278-B-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8915 |
| 5-198278-B-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5559 |
| 5-198278-B-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8851 |
| 5-198278-B-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7588 |
| 5-198278-C-70.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4263 |
| 5-198278-C-71.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7753 |
| 5-198278-C-72.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7077 |
| 5-198278-C-73.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7170 |
| 5-198278-C-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7993 |
| 5-198321-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7644 |
| 5-198321-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8068 |
| 5-198321-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7265 |
| 5-198321-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5006 |
| 5-198321-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8586 |
| 5-198600-A-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0695 |
| 5-198600-A-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3024 |
| 5-198891-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6709 |
| 5-198891-D-81.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9495 |
| 5-198891-D-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3333 |
| 5-199284-B-450.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4709 |
| 5-199284-B-451.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0467 |
| 5-199284-B-452.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7698 |
| 5-199284-B-453.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3202 |
| 5-199284-B-454.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4445 |
| 5-200329-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8479 |
| 5-200329-B-81.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3617 |
| 5-200329-B-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5245 |
| 5-200329-C-82.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7339 |
| 5-200339-A-10.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3335 |
| 5-200461-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1162 |
| 5-200461-B-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0969 |
| 5-201664-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0805 |
| 5-201664-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1421 |
| 5-201664-A-183.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4190 |
| 5-201664-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7859 |
| 5-202020-A-184.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8754 |
| 5-202540-A-181.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2656 |
| 5-202540-A-182.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8585 |
| 5-202898-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0889 |
| 5-202898-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9773 |
| 5-202898-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1526 |
| 5-202898-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3680 |
| 5-202898-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3782 |
| 5-203739-A-100.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0880 |
| 5-203739-A-101.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7549 |
| 5-203739-A-102.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7359 |
| 5-203739-A-103.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7556 |
| 5-203739-A-104.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5236 |
| 5-204352-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.5208 |
| 5-204352-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.2877 |
| 5-204352-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.4047 |
| 5-204352-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.5893 |
| 5-204352-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8573 |
| 5-204352-B-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 5.9718 |
| 5-204352-B-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4299 |
| 5-204352-B-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.3433 |
| 5-204352-B-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.2414 |
| 5-204352-B-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0500 |
| 5-204741-A-461.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1946 |
| 5-205898-A-400.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6033 |
| 5-205898-A-401.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4305 |
| 5-205898-A-402.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3902 |
| 5-205898-A-403.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3638 |
| 5-205898-A-404.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0276 |
| 5-207811-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2218 |
| 5-207811-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9547 |
| 5-207811-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2505 |
| 5-207811-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1813 |
| 5-207811-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3426 |
| 5-207811-B-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8883 |
| 5-207811-B-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6838 |
| 5-207811-B-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5730 |
| 5-207811-B-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0522 |
| 5-207811-B-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0403 |
| 5-208810-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0620 |
| 5-208810-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0649 |
| 5-208810-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0640 |
| 5-208810-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0049 |
| 5-208810-B-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2554 |
| 5-210540-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1531 |
| 5-210540-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.6543 |
| 5-210540-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1135 |
| 5-210540-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3923 |
| 5-210540-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1359 |
| 5-210612-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3099 |
| 5-211197-A-244.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7633 |
| 5-211408-A-370.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.7918 |
| 5-211408-A-371.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.8194 |
| 5-211408-A-372.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.8658 |
| 5-211408-A-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.8342 |
| 5-211408-A-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 4.6758 |
| 5-212059-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1449 |
| 5-212059-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2891 |
| 5-212059-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0183 |
| 5-212059-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1159 |
| 5-213077-A-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3906 |
| 5-213077-A-111.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9830 |
| 5-213077-A-112.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9172 |
| 5-213077-A-113.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3801 |
| 5-213077-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8119 |
| 5-213802-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9414 |
| 5-213802-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3106 |
| 5-213802-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7897 |
| 5-213802-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4805 |
| 5-213802-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1287 |
| 5-213836-A-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1051 |
| 5-214759-A-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1628 |
| 5-214759-A-52.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2377 |
| 5-214759-A-53.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8244 |
| 5-214759-B-51.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9725 |
| 5-214759-B-54.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4688 |
| 5-214869-A-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1363 |
| 5-215172-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 7.2382 |
| 5-215172-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 7.1568 |
| 5-215172-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.4805 |
| 5-215172-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.9078 |
| 5-215172-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 6.9764 |
| 5-215179-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2627 |
| 5-215179-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1038 |
| 5-215179-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0234 |
| 5-215179-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2389 |
| 5-215179-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0110 |
| 5-215634-A-172.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7806 |
| 5-215634-A-173.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9187 |
| 5-215658-A-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0619 |
| 5-215658-A-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2411 |
| 5-215658-A-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0111 |
| 5-215658-A-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1076 |
| 5-215658-A-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3438 |
| 5-215658-B-120.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0641 |
| 5-215658-B-121.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8986 |
| 5-215658-B-122.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3279 |
| 5-215658-B-123.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5603 |
| 5-215658-B-124.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8825 |
| 5-216213-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7016 |
| 5-216213-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0146 |
| 5-216213-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9220 |
| 5-216213-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5312 |
| 5-216214-A-130.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1203 |
| 5-216214-A-131.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0433 |
| 5-216214-A-132.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6684 |
| 5-216214-A-133.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0246 |
| 5-216214-A-134.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3960 |
| 5-216368-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5360 |
| 5-216368-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0992 |
| 5-216370-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2577 |
| 5-216370-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2308 |
| 5-216370-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2061 |
| 5-216370-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2854 |
| 5-216370-B-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4626 |
| 5-216370-B-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8703 |
| 5-216370-B-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7636 |
| 5-216370-B-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8710 |
| 5-218196-A-270.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5773 |
| 5-218196-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3201 |
| 5-218196-B-271.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1263 |
| 5-218196-B-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5225 |
| 5-219242-B-373.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1373 |
| 5-219242-B-374.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 7.5052 |
| 5-219379-A-114.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6183 |
| 5-219379-B-110.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2336 |
| 5-221567-A-221.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2988 |
| 5-221567-A-223.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6132 |
| 5-221568-A-220.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3706 |
| 5-221568-A-221.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1927 |
| 5-221568-A-222.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1657 |
| 5-221568-A-223.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6674 |
| 5-221878-A-342.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3589 |
| 5-221878-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0807 |
| 5-222524-A-410.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5766 |
| 5-222524-A-411.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3111 |
| 5-222524-A-412.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5063 |
| 5-222524-A-413.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6362 |
| 5-222524-A-414.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5363 |
| 5-232272-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9587 |
| 5-232272-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4158 |
| 5-232272-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0547 |
| 5-232272-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0802 |
| 5-232272-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2665 |
| 5-232802-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5289 |
| 5-233019-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0231 |
| 5-233019-A-311.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3787 |
| 5-233019-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5144 |
| 5-233019-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9769 |
| 5-233019-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0602 |
| 5-233260-A-230.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2914 |
| 5-233312-A-281.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1575 |
| 5-233312-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3029 |
| 5-233312-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3101 |
| 5-233787-A-74.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6883 |
| 5-234145-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0153 |
| 5-234145-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5380 |
| 5-234145-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6015 |
| 5-234855-A-251.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3705 |
| 5-234923-A-321.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3363 |
| 5-235507-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2542 |
| 5-235507-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3018 |
| 5-235507-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2600 |
| 5-235507-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2454 |
| 5-235507-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0612 |
| 5-235874-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7906 |
| 5-235874-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7961 |
| 5-235893-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0465 |
| 5-235893-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4653 |
| 5-236288-A-282.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6938 |
| 5-236288-A-284.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6609 |
| 5-237315-A-310.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2195 |
| 5-237315-A-311.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5391 |
| 5-237315-A-312.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1397 |
| 5-237315-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0052 |
| 5-237315-A-314.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5346 |
| 5-238021-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3789 |
| 5-238021-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8780 |
| 5-238021-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0260 |
| 5-238021-A-353.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1429 |
| 5-238021-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9218 |
| 5-238492-A-231.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2693 |
| 5-238938-A-272.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3508 |
| 5-238938-A-273.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3084 |
| 5-238938-A-274.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.7107 |
| 5-241846-A-150.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.6502 |
| 5-241846-A-151.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4839 |
| 5-241846-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9020 |
| 5-241846-A-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7509 |
| 5-242711-A-92.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5800 |
| 5-242711-A-93.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2488 |
| 5-242711-A-94.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3763 |
| 5-243450-A-140.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2725 |
| 5-243450-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4439 |
| 5-243450-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6172 |
| 5-243450-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0983 |
| 5-243450-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5641 |
| 5-243459-A-144.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5629 |
| 5-243459-B-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5319 |
| 5-243773-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2876 |
| 5-243773-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0449 |
| 5-243773-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0003 |
| 5-243773-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1071 |
| 5-243773-B-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0510 |
| 5-243783-A-440.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6456 |
| 5-243783-A-441.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6135 |
| 5-243783-A-442.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4494 |
| 5-243783-A-443.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6060 |
| 5-243783-A-444.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.5437 |
| 5-244310-A-253.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8501 |
| 5-244310-A-254.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6117 |
| 5-244315-C-63.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2330 |
| 5-244315-C-64.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9543 |
| 5-244459-A-280.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9212 |
| 5-244459-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4822 |
| 5-244651-A-311.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.5168 |
| 5-244651-A-313.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.4621 |
| 5-244933-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4241 |
| 5-244933-A-344.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5407 |
| 5-245040-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1229 |
| 5-245040-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4332 |
| 5-245040-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6324 |
| 5-245040-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3863 |
| 5-249748-A-283.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4494 |
| 5-250753-A-343.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4708 |
| 5-251426-A-302.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8414 |
| 5-251426-B-303.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5397 |
| 5-251957-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0629 |
| 5-251957-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4110 |
| 5-251957-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1477 |
| 5-251962-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6039 |
| 5-251962-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4708 |
| 5-251963-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2267 |
| 5-251963-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1318 |
| 5-251971-A-470.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2842 |
| 5-251971-A-471.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1118 |
| 5-251971-A-472.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7526 |
| 5-251971-A-473.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.6752 |
| 5-251971-A-474.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7074 |
| 5-253085-A-33.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.6502 |
| 5-253085-A-34.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3063 |
| 5-253085-B-32.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.2797 |
| 5-257839-A-141.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9984 |
| 5-257839-A-142.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7678 |
| 5-257839-A-143.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.5760 |
| 5-260164-A-234.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.2755 |
| 5-260875-A-350.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3674 |
| 5-260875-A-351.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3295 |
| 5-260875-A-352.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3418 |
| 5-260875-A-354.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0304 |
| 5-261439-A-152.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0999 |
| 5-261439-A-154.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4700 |
| 5-261464-A-233.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.4103 |
| 5-261464-A-234.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0579 |
| 5-262957-A-220.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3662 |
| 5-262957-A-221.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4801 |
| 5-262957-A-222.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4895 |
| 5-262957-A-223.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.4370 |
| 5-262957-A-224.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3227 |
| 5-263902-A-360.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3348 |
| 5-263902-A-361.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9208 |
| 5-263902-A-362.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0145 |
| 5-263902-A-363.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7152 |
| 5-263902-A-364.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.8124 |
| 5-61635-A-83.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9733 |
| 5-61635-A-84.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8218 |
| doing_the_dishes000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0204 |
| doing_the_dishes001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.1683 |
| doing_the_dishes002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.8000 |
| doing_the_dishes003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.0266 |
| doing_the_dishes004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9179 |
| doing_the_dishes008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1371 |
| doing_the_dishes012.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.3616 |
| doing_the_dishes015.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.1287 |
| doing_the_dishes016.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.9505 |
| doing_the_dishes017.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3419 |
| doing_the_dishes018.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 0.0136 |
| doing_the_dishes019.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.3513 |
| exercise_bike000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7186 |
| exercise_bike001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0043 |
| exercise_bike002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1237 |
| exercise_bike003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2156 |
| exercise_bike004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0320 |
| exercise_bike005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9997 |
| exercise_bike006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9880 |
| exercise_bike007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1805 |
| exercise_bike008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.9033 |
| exercise_bike009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1066 |
| exercise_bike010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7711 |
| exercise_bike011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 1.7609 |
| exercise_bike012.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.3047 |
| pink_noise000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2634 |
| pink_noise001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1386 |
| pink_noise002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2886 |
| pink_noise003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1475 |
| pink_noise004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1650 |
| pink_noise005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1349 |
| pink_noise006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2933 |
| pink_noise007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1102 |
| pink_noise008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1491 |
| pink_noise009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.1446 |
| pink_noise010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.2794 |
| pink_noise011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.0741 |
| running_tap000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0059 |
| running_tap001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2078 |
| running_tap002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5412 |
| running_tap003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.1878 |
| running_tap004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5923 |
| running_tap005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9908 |
| running_tap006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.2872 |
| running_tap007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3606 |
| running_tap008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.5897 |
| running_tap009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.3217 |
| running_tap010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.4914 |
| running_tap011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9904 |
| running_tap012.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 3.0640 |
| white_noise000.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8071 |
| white_noise001.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7674 |
| white_noise002.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.7680 |
| white_noise003.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8561 |
| white_noise004.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8425 |
| white_noise005.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8967 |
| white_noise006.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8354 |
| white_noise007.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8894 |
| white_noise008.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8644 |
| white_noise009.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.9886 |
| white_noise010.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8029 |
| white_noise011.wav | Local Ambient Noise (ESC-50) | NOISE (0) | DRONE (1) | 2.8686 |

## Shrnutí a doporučení

1. **Rozdíl v normalizacích**:
   - Unnormalized (MCU Match) zachovává absolutní energii nahrávky. To může pomoci snížit falešné poplachy u tichých ambientních zvuků, protože drony mají obvykle výrazně vyšší celkovou energii.
   - Global RMS Normalized odstraňuje celkovou úroveň hlasitosti, což činí model citlivějším na akustické signatury v tichých nahrávkách, ale zvyšuje míru falešných poplachů u tichého šumu, který má podobný spektrální tvar jako dron.

2. **Typy šumů náchylné k chybám**:
   - Prozkoumejte sekce ESC-50 výše. Zvuky jako **vysavač (vacuum_cleaner)**, **motor (engine)**, **chainsaw (motorová pila)**, **helicopter (vrtulník)** nebo **chainsaw** mají frekvenční spektra velmi podobná rotujícím vrtulím dronů (harmonické složky a širokopásmový šum) a mohou mít vysokou chybovost.

3. **Threshold Tuning**:
   - Pokud je míra falešných poplachů příliš vysoká, doporučujeme zvýšit rozhodovací práh (decision threshold) v C kódu (v `svm_classifier.c`) z `0.0f` na vyšší hodnotu (např. `1.0f` nebo `1.5f`). Tím se odfiltruje většina hraničních případů falešných poplachů za cenu mírného poklesu citlivosti u velmi vzdálených dronů.
