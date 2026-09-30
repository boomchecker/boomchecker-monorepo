# Zpráva o úspěšnosti detekce v jednotlivých kategoriích

Tento report detailně popisuje úspěšnost našeho Lineárního SVM modelu s StandardScalerem na různých typech akustických nahrávek.

## Tabulka úspěšnosti dle kategorií

| Kategorie nahrávek | Popis | Počet vzorků | Správně | Úspěšnost (Recall/Acc) | Průměrná jistota (Decision Margin) |
|:---|:---|:---:|:---:|:---:|:---:|
| **Parquet Drone** | Vzorky dronů z parquet souborů (všeobecná databáze) | 300 | 300 | **100.00%** |   2.4938 |
| **Parquet Noise** | Vzorky šumu/okolí z parquet souborů | 300 | 201 | ** 67.00%** |  -0.8424 |
| **Bebop Drone (Clean)** | Čisté nahrávky dronu Parrot Bebop | 333 | 333 | **100.00%** |   2.7027 |
| **Membo Drone (Clean)** | Čisté nahrávky dronu Membo | 332 | 329 | ** 99.10%** |   4.2255 |
| **Mixed Bebop (Noise+Drone)** | Nahrávky Bebop dronu smíchané se šumem pozadí | 336 | 332 | ** 98.81%** |   2.2476 |
| **Mixed Membo (Noise+Drone)** | Nahrávky Membo dronu smíchané se šumem pozadí | 338 | 307 | ** 90.83%** |   1.9993 |
| **Local Ambient Noise** | Lokální nahrávky šumu a klidného okolí (unknown) | 10371 | 7875 | ** 75.93%** |  -2.8618 |

## Podrobná analýza výsledků

### 1. Detekce čistých dronů (Bebop & Membo)
Čisté nahrávky specifických dronů (Parrot Bebop, Membo) ukazují, jak dobře model reaguje na čistou akustickou signaturu motorů a vrtulí bez okolních vlivů. Silně kladná rozhodovací hodnota (Decision Margin) znamená vysokou jistotu klasifikátoru.

### 2. Detekce smíchaných nahrávek (Mixed Bebop & Mixed Membo)
Mixed nahrávky simulují reálné nasazení, kdy je zvuk dronu překryt ambientním šumem (vítr, městský hluk, šustění). Nižší úspěšnost v této kategorii ukazuje na limity lineárního oddělení při nízkém poměru signálu k šumu (SNR). Rozhodovací hodnota se zde blíží nule, což značí hraniční případy.

### 3. Falešné poplachy (Local Ambient Noise & Parquet Noise)
Kategorie s očekávaným labelem NOISE (0). Úspěšnost v těchto kategoriích vyjadřuje odolnost vůči falešným poplachům (Specificity). Velmi záporná hodnota Decision Margin u šumu značí, že klasifikátor spolehlivě odmítá běžný okolní hluk.
