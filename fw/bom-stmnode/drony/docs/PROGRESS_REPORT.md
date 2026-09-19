# Progress Report: Acoustic Drone Detection System

**Date:** 2026-06-19  
**Status:** Parity Validation, Threshold Optimization, and Firmware Verification Completed (Tasks 002 & 003 Done)  

---

## 🚀 Accomplishments & Milestones

1.  **DSP Pipeline & Parity Validation (Task 002):**
    *   Pre-calculated Mel filterbank and DCT-II coefficients generated matching Librosa's frame partitioning.
    *   Achieved **100% mathematical parity** between Python and C implementations. Parity tests show maximum difference of **4.61e-07** (average **9.88e-08**), which is well below the target limit.
    *   Host PC GCC compilation fully configured for CMSIS-DSP target-agnostic testing using custom alignment and rotation mocks in `cmsis_compiler.h`.

2.  **Optimized SVM & Squelch Gate (Task 003):**
    *   Trained a highly-efficient **Linear SVM** that compresses coefficients into a single 108-byte weight vector, avoiding the 220+ KB Flash overhead of non-linear RBF kernels.
    *   Conducted a large-scale grid search optimization (12,103 samples) to balance Sensitivity (drones) and Specificity (false alarms).
    *   **Squelch RMS Gate (`0.010f`)** and **SVM Decision Threshold (`0.5f`)** implemented directly in C source files ([svm_classifier.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/svm_classifier.c) and [main_dsp_test.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/main_dsp_test.c)).
    *   **Optimization Results:** 
        *   Overall system accuracy increased from **91.49% to 93.21%**.
        *   False Positives (alarms) on the entire ESC-50 database dropped by nearly half (from **7.94% to 4.66%**).
        *   Drastically reduced false alarms from continuous high-frequency noises (rain FP rate fell by 27.50%, crickets by 22.50%).

3.  **Host Testing & Verification:**
    *   Both verification executables, `dsp_test.exe` (full pipeline) and `svm_test.exe` (SVM logic test on 10/10 vectors), are fully compiled and verified passing on the host machine.
4.  **SAI PDM Microphone Integration Ready:**
    *   Designed and implemented the downsampling handler ([audio_sai_handler.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/audio_sai_handler.c) / [audio_sai_handler.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/audio_sai_handler.h)) based on the pinout in `bom-stm32node.ioc` (SAI1 configured for PDM protocols at 48 kHz).
    *   Uses CMSIS-DSP FIR Decimation (`arm_fir_decimate_f32`) with a pre-calculated 15-tap low-pass filter to downsample the 48 kHz PCM stream to the required 16 kHz in real-time.
    *   Implements double-buffered DMA callbacks and processing queues to feed the verified MFCC + SVM classification chain. Compiled successfully on the host.

---

## 📂 Current Firmware & Src File Structure

-   [svm_classifier.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/svm_classifier.c) / [svm_classifier.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/svm_classifier.h): Core SVM math.
-   [mfcc_processor.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/mfcc_processor.c) / [mfcc_processor.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/mfcc_processor.h): CMSIS-DSP MFCC wrapper.
-   [audio_sai_handler.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/audio_sai_handler.c) / [audio_sai_handler.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/audio_sai_handler.h): Real-time SAI PDM microphone downsampler and pipeline driver.
-   [main_dsp_test.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/main_dsp_test.c): Full pipeline testing executable including Squelch Gate.
-   [test_svm_logic.c](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Src/test_svm_logic.c): Logic test vector runner.
-   [svm_model_data.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/svm_model_data.h): StandardScaler means/std parameters and weights vector.
-   [mfcc_tables.h](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/src/firmware/Inc/mfcc_tables.h): Static DCT matrix and sparse Mel filters.
-   [detailed_test_report.md](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/docs/explorations/detailed_test_report.md): Large-scale evaluation results.
-   [optimized_report.md](file:///C:/Users/Kamil/Documents/boomchecker-monorepo/fw/bom-stmnode/drony/docs/explorations/optimized_report.md): Before-after comparison of optimal threshold sweeps.

---

## ⏭️ Next Steps: Target Hardware Nasazení (STM32H5)

Když začíná nová vývojová seance, zaměřte se na nasazení na mikrokontrolér:

1.  **STM32CubeMX Inicializace:**
    *   Generování kódu z `bom-stm32node.ioc` pro vytvoření projektu (vytvoří ovladače periferií, inicializaci hodin, atd.).
    *   Přidání našich souborů z `src/firmware` a knihovny CMSIS-DSP do sestavovacího řetězce MCU.
2.  **Ping-Pong Buffer Integration:**
    *   V callbackech `HAL_SAI_RxHalfCpltCallback` a `HAL_SAI_RxCpltCallback` stačí přímo volat `audio_sai_process_half_callback(buf)`.
3.  **Spuštění Pipeline:**
    *   V hlavní smyčce `main.c` pravidelně volejte `audio_sai_pipeline_poll()`, která se postará o vyhodnocování RMS Squelche, MFCC analýzu a spouštění klasifikátoru SVM při detekci dronu.
