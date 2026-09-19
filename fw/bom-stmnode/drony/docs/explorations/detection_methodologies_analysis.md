# Analýza metod detekce dronů (Brainstorming)

Tento dokument shrnuje možnosti implementace algoritmů pro akustickou detekci dronů na základě poskytnutých vědeckých článků. Cílem je vybrat nejvhodnější metodu pro systém postavený na **STM32H5** a **MEMS mikrofonech**.

---

## 📋 Porozumění problému
- **Cíl:** Detekce přítomnosti dronu a identifikace jeho typu v reálném čase.
- **Vstup:** Proud surových numerických dat (pole/buffer) z digitálních MEMS mikrofonů (získaný přes DMA).
- **Omezení:** 
    - Běh na MCU (ARM Cortex-M33).
    - Limitovaná RAM a Flash paměť.
    - Zpracování v reálném čase nad pevnou délkou okna (windowing).
- **Předpoklad:** Prvotní vývoj probíhá v Pythonu (simulace pole ze souborů), následně přímá práce s buffery v C.

---

## 🔄 Analýza přístupů

### 1. Akustický otisk (Acoustic Fingerprinting)
*Inspirováno článkem: Balakin et al. (Real-time drone detection and recognition by acoustic fingerprint)*

| Aspekt | Analýza |
|--------|----------|
| **Popis** | Využívá FFT k analýze PSD nad příchozím polem vzorků. Hledá harmonické frekvence motorů a porovnává je s databází. |
| **Plusy** | • Extrémně nízké nároky na CPU/RAM • Přímá operace nad polem (CMSIS-DSP FFT) |
| **Mínusy** | • Velmi vysoká citlivost na hluk • Selhává při změně otáček dronu nebo v dálce |
| **Náročnost** | Nízká |
| **Riziko** | Vysoké (nízká robustnost v reálném světě) |

### 2. Klasické Strojové Učení (SVM + MFCC)
*Inspirováno článkem: Anwar et al. (Machine Learning Inspired Sound-Based Amateur Drone Detection)*

| Aspekt | Analýza |
|--------|----------|
| **Popis** | Transformace příchozího pole vzorků na koeficienty **MFCC**. Klasifikace vektoru příznaků pomocí **SVM**. |
| **Plusy** | • Výborný poměr přesnost/výkon • MFCC transformace je efektivní nad buffery • Malý paměťový footprint modelu |
| **Mínusy** | • Vyžaduje trénování na datasetu • Nutnost implementovat MFCC extrakci (v CMSIS-DSP dostupná `arm_mfcc_f32`) |
| **Náročnost** | Střední |
| **Riziko** | Nízké (osvědčená metoda pro Edge AI) |

### 3. Deep Learning (CNN na Spektrogramech)
*Inspirováno článkem: Al-Emadi et al. (Audio Based Drone Detection using Deep Learning)*

| Aspekt | Analýza |
|--------|----------|
| **Popis** | Audio se převede na Mel-spektrogram (obrázek) a klasifikuje se pomocí konvoluční neuronové sítě (CNN). |
| **Plusy** | • Nejvyšší možná přesnost • Automatické učení příznaků |
| **Mínusy** | • Obrovské nároky na RAM/Flash • Nutnost kvantizace (TinyML) pro STM32 |
| **Náročnost** | Vysoká |
| **Riziko** | Střední (riziko, že se model nevejde do paměti MCU) |

### 4. Hybridní přístup (FFT Peak + ML)
*Kombinace metod pro zvýšení spolehlivosti*

| Aspekt | Analýza |
|--------|----------|
| **Popis** | Rychlá detekce energie v kritických pásmech (FFT) spustí komplexnější analýzu (SVM/CNN) pouze při podezření. |
| **Plusy** | • Úspora energie • Snížení falešných poplachů |
| **Mínusy** | • Komplexnější architektura firmwaru |
| **Náročnost** | Střední až Vysoká |

---

## 📊 Matice rozhodování

| Kritérium | Fingerprinting | **SVM + MFCC** | CNN (Deep Learning) |
|----------|:---:|:---:|:---:|
| **Přesnost** | ⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **Nízká latence** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐ |
| **Paměťová úspora** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐ |
| **Vhodnost pro STM32H5** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |

---

## ✅ Kritéria úspěchu
- [ ] **Latence:** Zpracování rámce audia pod 200 ms.
- [ ] **Přesnost:** Úspěšnost detekce > 90 % v testovacím datasetu.
- [ ] **Paměť:** Model a buffering nesmí překročit 70 % dostupné RAM STM32H5.

---

## ⚠️ YAGNI Assessment (Kontrola overengineeringu)
- **Nepotřebujeme:** Komplexní RNN/LSTM sítě. Audio je sice časová řada, ale spektrogramy/MFCC v kombinaci s CNN nebo SVM jsou pro detekci motorů dronů dostačující a mnohem efektivnější.
- **Verdikt:** Začít s SVM + MFCC. Je to dostatečně silné, ale ne zbytečně složité.

---

## 🎯 Doporučení
**Doporučený přístup: Klasické Strojové Učení (SVM + MFCC)**

Tato metoda představuje "Sweet Spot" pro Cortex-M33. Nabízí robustnost vůči okolnímu hluku (díky MFCC) a zároveň je velmi nenáročná na paměť, což je kritické pro firmware v C.

---

## 🚀 Další kroky
1. Příprava Python prostředí (`librosa`, `scikit-learn`).
2. Analýza frekvenčního spektra prvních `.wav` nahrávek.
3. Výpočet MFCC a návrh klasifikátoru.
