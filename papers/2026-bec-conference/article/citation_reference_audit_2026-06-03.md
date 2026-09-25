# Citation and reference audit, 2026-06-03

Scope: `article/article_main.tex` and `article/references.bib`.
Workflow: ARS `academic-paper` / `citation-check`, with semantic source-to-claim verification where local source text or authoritative metadata was available.
Citation style detected: IEEE numeric style via `IEEEtran`.

## Executive summary

The manuscript has no obvious missing in-text citation keys in the compiled bibliography: the generated `article_main.bbl` contains 29 cited references, and the current log search did not show unresolved citation warnings.

The main risk is not missing references. The main risk is over-broad citation support in several state-of-the-art sentences, where a mixed citation cluster is asked to support a whole methodological generalization. Most cited sources support nearby parts of the claim, but some sentences should be narrowed or split so that each cited source supports the specific clause next to it.

Highest-priority fixes:

1. Lines 58 and 68: split the broad "triggering often relies on thresholding, energy-envelope analysis, time-of-arrival logic, or multi-sensor consistency checks" claim. The current cited artillery sources support threshold snippets, acoustic sensor layouts, launch/impact classification, wavelet/DWT features, TOA/localization, and multi-sensor/distributed sensing, but not all items equally as a general pipeline taxonomy.
2. Line 83: `banbury2021micronets` supports commodity-microcontroller TinyML deployment logic, but not directly audio MFCC-CNN design. Keep it for deployment-cost framing, not as evidence for audio architecture choice. Consider adding or moving an audio TinyML source if the claim remains audio-specific.
3. Line 86: `ye2019multi_sound` plausibly supports multi-sound robustness, but local full text was not available. Mark as externally verified only if a PDF/source is added; otherwise soften or replace.
4. Line 91: `tflite` is a live documentation source with year `2026` and access date 2026-05-16. Google now brands TensorFlow Lite as LiteRT in official docs. The citation is acceptable if you mean legacy TensorFlow Lite/TFLite practice, but the reference metadata should be reviewed for naming and access-date consistency.
5. Add missing DOIs where available for cited classic sources: `davis1980mfcc`, `lecun1998gradient`, `hershey2017cnn_audio`. `piczak2015esc_cnn` already has DOI and is correct.

## Mechanical citation audit

### In-text citation sites

There are 13 lines containing citation commands:

| Line | Citation keys |
|---:|---|
| 58 | `desai2005he_cb`, `hohil2007variants`, `piczak2015esc_cnn`, `svatos2024fcc` |
| 68 | `desai2005he_cb`, `desai2006launch_impact`, `hohil2007variants`, `dagallier2019long_range` |
| 71 | `desai2005he_cb`, `desai2006launch_impact`, `dagallier2019long_range` |
| 81 | `hohil2007variants`, `khan2010embedding`, `raponi2022sound_of_guns`, `piczak2015esc_cnn`, `salamon2017esc_aug`, `davis1980mfcc`, `chu2009env_sound_tf`, `svatos2024fcc` |
| 83 | `lecun1998gradient`, `piczak2015esc_cnn`, `hershey2017cnn_audio`, `banbury2021micronets` |
| 86 | `khan2010embedding`, `raponi2022sound_of_guns`, `svatos2024fcc`, `ye2019multi_sound`, `salamon2017esc_aug` |
| 88 | `park2019specaugment`, `braun2017accan`, `rehr2015cepstral_noise`, `delatorre2002nonlinear_feature_space` |
| 91 | `muthumala2024tinyml_ae`, `lamrini2023embedded_esr`, `lin2020mcunet`, `banbury2021micronets`, `tflite`, `david2021tflm`, `jacob2018quantization`, `benkhalifa2024stm32_quantization` |
| 97 | `svatos2019smart_acoustic_sensor` |
| 137 | `lecun1998gradient` |
| 149 | `salamon2017esc_aug`, `bishop1995training_noise`, `braun2017accan`, `delatorre2002nonlinear_feature_space` |
| 152 | `he2009imbalanced` |
| 155 | `tflite`, `david2021tflm`, `jacob2018quantization`, `david2021tflm` |

### Cited references

The compiled `.bbl` contains 29 cited entries:

`desai2005he_cb`, `hohil2007variants`, `piczak2015esc_cnn`, `svatos2024fcc`, `desai2006launch_impact`, `dagallier2019long_range`, `khan2010embedding`, `raponi2022sound_of_guns`, `salamon2017esc_aug`, `davis1980mfcc`, `chu2009env_sound_tf`, `lecun1998gradient`, `hershey2017cnn_audio`, `banbury2021micronets`, `ye2019multi_sound`, `park2019specaugment`, `braun2017accan`, `rehr2015cepstral_noise`, `delatorre2002nonlinear_feature_space`, `muthumala2024tinyml_ae`, `lamrini2023embedded_esr`, `lin2020mcunet`, `tflite`, `david2021tflm`, `jacob2018quantization`, `benkhalifa2024stm32_quantization`, `svatos2019smart_acoustic_sensor`, `bishop1995training_noise`, `he2009imbalanced`.

### Unused BibTeX entries

These entries exist in `references.bib` but are not cited in the current manuscript:

`goodfellow2016dl`, `crocco2014audio_surveillance`, `gemmeke2017audioset`, `kong2019weakly_audioset`, `kong2020panns`, `novac2021quantization_mcu`, `gong2021psla`.

This is not a compiled-reference-list problem under BibTeX, because uncited entries are not printed. It is still worth cleaning the `.bib` if the project should contain only active references.

## DOI and metadata checks

### Missing DOI in cited entries

| Key | Current issue | Verification | Recommended BibTeX change |
|---|---|---|---|
| `davis1980mfcc` | DOI missing | DOI found in bibliographic metadata: `10.1109/TASSP.1980.1163420` | Add `doi = {10.1109/TASSP.1980.1163420}` |
| `lecun1998gradient` | DOI missing | DOI found in bibliographic metadata: `10.1109/5.726791` | Add `doi = {10.1109/5.726791}` |
| `hershey2017cnn_audio` | DOI missing | DBLP/metadata list DOI: `10.1109/ICASSP.2017.7952132` | Add `doi = {10.1109/ICASSP.2017.7952132}` |
| `delatorre2002nonlinear_feature_space` | DOI missing | Not conclusively verified from available local/web metadata in this pass | Leave missing unless IEEE metadata is checked directly |
| `rehr2015cepstral_noise` | DOI missing | Local/open PDF confirms paper content, but DOI not conclusively verified in this pass | Leave missing unless IEEE metadata is checked directly |
| `lin2020mcunet` | DOI missing | NeurIPS/Proceedings-style entry may not use DOI | No action required unless venue metadata provides DOI |
| `david2021tflm` | DOI missing | DBLP lists no DOI for the MLSys paper | No DOI action required |

### Live documentation source

`tflite` points to `https://www.tensorflow.org/lite` with year 2026 and access date 2026-05-16. Current Google AI Edge documentation presents LiteRT as the successor/renaming of TensorFlow Lite and provides a separate LiteRT for Microcontrollers page. If the paper is submitted in 2026, consider citing the official LiteRT for Microcontrollers documentation instead of a generic TensorFlow Lite page, or explicitly write "TensorFlow Lite/LiteRT".

## Semantic claim-to-source audit

Legend:

- OK: the citation supports the claim.
- PARTIAL: source supports only part of the sentence or requires narrower wording.
- WEAK: source is adjacent but not sufficient for the written claim.
- UNVERIFIED: no local full text was available and web metadata did not fully verify the specific claim.

| Manuscript location | Claim being supported | Cited source(s) | Audit result | Notes / recommended action |
|---|---|---|---|---|
| Line 58 | Weapon-detection pipelines split triggering from post-trigger interpretation; triggering uses threshold/energy/TOA/multi-sensor checks; classification uses engineered descriptors and CNNs. | `desai2005he_cb`, `hohil2007variants`, `piczak2015esc_cnn`, `svatos2024fcc` | PARTIAL | `desai2005` and `hohil2007` support artillery acoustic classification using wavelet/statistical features and sensors. `piczak2015` supports CNNs for environmental sound, not weapons. `svatos2024` supports cepstral gunshot classification. The trigger taxonomy is broader than this citation cluster. Split into two sentences or add a source specifically reviewing acoustic triggering/localization pipelines. |
| Line 68 | Classical weapon-related acoustic surveillance uses thresholding, energy-envelope analysis, TOA logic, multi-sensor checks; battlefield studies show dependence on triggering robust to propagation, distance, layouts. | `desai2005he_cb`, `desai2006launch_impact`, `hohil2007variants`, `dagallier2019long_range` | PARTIAL | `desai2006` explicitly includes dynamic threshold snippets, DWT features, launch/impact classification, range/propagation effects. `dagallier2019` strongly supports TOA, distributed sensors, atmosphere/terrain sensitivity. `hohil2007` supports sensor layouts, launch/impact/variant acoustic features. But the opening taxonomy is still broader than any one cited source. |
| Line 71 | Artillery/gunfire waveforms vary with source type, firing geometry, propagation, reflections; mixtures include muzzle blast, shock wave, delayed arrivals; long-range studies model arrivals and sensitivity to atmosphere/terrain/sensor geometry. | `desai2005he_cb`, `desai2006launch_impact`, `dagallier2019long_range` | OK | `dagallier2019` directly supports muzzle/projectile/impact waves, TOA matching, atmospheric and terrain effects. `desai2006` supports launch/impact signatures, range propagation, DWT features. This is one of the better-supported citation clusters. |
| Line 81 | Post-trigger windows use wavelet, cepstral, temporal-envelope features and shallow classifiers; later CNNs over spectro-temporal inputs; MFCC/cepstral representations are useful and competitive. | `hohil2007variants`, `khan2010embedding`, `raponi2022sound_of_guns`, `piczak2015esc_cnn`, `salamon2017esc_aug`, `davis1980mfcc`, `chu2009env_sound_tf`, `svatos2024fcc` | OK with minor split recommended | The evidence base is appropriate. `khan2010` uses MFCC/exemplar embedding for gunshots. `raponi2022` supports CNN over spectrogram/time-frequency gunshot inputs. `davis1980` supports MFCC origin/use in speech; `svatos2024` supports FCC/MFCC/GFCC comparison for gunshot classification. The sentence is long; consider splitting to keep each source near its claim. |
| Line 83 | Compact CNN topologies preserve local time-frequency structure and are deployable on constrained hardware; LeNet-like local conv/pooling/dense pattern; audio CNNs learn local spectro-temporal patterns; MicroNets supports small-footprint deployment logic. | `lecun1998gradient`, `piczak2015esc_cnn`, `hershey2017cnn_audio`, `banbury2021micronets` | PARTIAL | `lecun1998` supports LeNet/convolution/pooling pattern; `piczak2015` and `hershey2017` support audio CNNs. `banbury2021` supports commodity microcontroller TinyML deployment but not audio-specific MFCC-CNN design. Keep `banbury2021` for deployment constraint wording only. |
| Line 86 | Strong results under uncontrolled recording are dataset/prior dependent; performance degrades under perturbation, device mismatch, scene change, overlapping sources; augmentation and calibration matter. | `khan2010embedding`, `raponi2022sound_of_guns`, `svatos2024fcc`, `ye2019multi_sound`, `salamon2017esc_aug` | PARTIAL / UNVERIFIED | `khan2010` directly addresses varying recording conditions. `raponi2022` and `svatos2024` support dataset/task-specific gunshot classification evidence. `salamon2017` supports augmentation for environmental sound. `ye2019` was not locally available; if retained, add source PDF or verify externally. Device mismatch is better supported by acoustic-scene/DCASE sources than by the listed weapon papers. |
| Line 88 | SpecAugment uses feature-domain warping/masking; feature-level Gaussian noise has ASR precedent; cepstral-domain processing used for robust ASR; waveform additive noise is not equivalent to MFCC-domain perturbation because feature distortion is nonlinear. | `park2019specaugment`, `braun2017accan`, `rehr2015cepstral_noise`, `delatorre2002nonlinear_feature_space` | OK | Local source text strongly supports this chain. `delatorre2002` is especially apt for nonlinear cepstral effects of additive noise. `rehr2015` supports cepstral noise subtraction/feature compensation. |
| Line 91 | TinyML acoustic comparisons show feature extraction can dominate runtime; embedded/TPU studies show accuracy-resource tradeoffs; MCU TinyML memory/latency/energy overhead matters; TFLite and quantization motivate separate float/int8/embedded validation. | `muthumala2024tinyml_ae`, `lamrini2023embedded_esr`, `lin2020mcunet`, `banbury2021micronets`, `tflite`, `david2021tflm`, `jacob2018quantization`, `benkhalifa2024stm32_quantization` | OK with wording caveat | The sources support the broad deployment argument. Caveat: `lamrini2023` is Raspberry Pi/TPU-class, not MCU. `lin2020mcunet` and `banbury2021` are MCU/TinyML but not audio-specific. Phrase as "related embedded/TinyML evidence" rather than direct artillery-audio evidence. |
| Line 97 | First stage follows a median-filter impulse-detection approach; streaming trigger estimates background and forwards buffered segments. | `svatos2019smart_acoustic_sensor` | OK / local PDF not deeply checked | The title and local availability indicate relevance. If this is a key system-scope claim, add a page/section note in your working notes after checking the paper text. |
| Line 137 | CNN layout follows standard convolutional design practice. | `lecun1998gradient` | OK | Proper support for local convolution, pooling/subsampling, dense head pattern. |
| Line 149 | Audio augmentation under limited data; training with input noise relates to Tikhonov regularization; feature-level Gaussian noise used in ASR; waveform noise induces nonlinear cepstral distortion. | `salamon2017esc_aug`, `bishop1995training_noise`, `braun2017accan`, `delatorre2002nonlinear_feature_space` | OK | This is well-supported. `bishop1995` is exactly relevant for input-noise regularization. |
| Line 152 | Imbalanced corpus justifies class-weighted optimization / reducing majority-class bias. | `he2009imbalanced` | OK with wording caveat | `he2009` supports imbalanced learning and cost-sensitive learning broadly. It does not prescribe the exact implementation used here, but supports the methodological rationale. |
| Line 155 | Export to TensorFlow Lite Micro; static tensor arena and selective op resolver; int8 inference reduces memory and enables efficient embedded execution. | `tflite`, `david2021tflm`, `jacob2018quantization` | OK | `david2021tflm` supports memory arena/operator resolver/TFLM design. Official LiteRT docs support microcontroller deployment and ESP32 support. `jacob2018` supports integer-only quantized inference. |

## Source-specific findings

### Strongly aligned sources

- `dagallier2019long_range`: Strong support for long-range artillery acoustics, multiple arrivals, distributed sensors, TOA matching, and atmospheric/terrain sensitivity.
- `desai2006launch_impact`: Strong support for artillery launch/impact classification with DWT/wavelet-derived features, thresholded snippets around maxima, propagation/range issues, and neural-network classification.
- `hohil2007variants`: Strong support for mortar/artillery variant classification, launch/impact acoustic signatures, sensor deployment, temporal/power/energy features, and neural-network classification.
- `svatos2024fcc`: Strong support for gunshot/false-alarm cepstral-feature comparison and MFCC/FCC-type representations in impulse acoustic classification.
- `delatorre2002nonlinear_feature_space`: Strong support for the nonlinear effect of additive noise on cepstral/log-energy feature representations.
- `david2021tflm`: Strong support for TensorFlow Lite Micro memory arena and operator resolver concepts.

### Sources that need careful wording

- `piczak2015esc_cnn`: Good source for environmental-sound CNNs over spectrogram-like features; not a weapon/artillery source.
- `salamon2017esc_aug`: Good source for environmental-sound CNN augmentation; not a weapon/artillery source.
- `banbury2021micronets`: Good source for microcontroller TinyML deployment; not audio-specific.
- `lin2020mcunet`: Good source for memory-limited MCU model/inference design; not audio-specific.
- `lamrini2023embedded_esr`: Good source for embedded sound recognition on Raspberry Pi/TPU-class platforms; not MCU-class and not artillery-specific.
- `tflite`: Live documentation; use with access date and current naming caution.

### Sources needing verification or cleanup

- `ye2019multi_sound`: No local full text found in `article/sources`. Keep only if you verify the paper supports the device/perturbation/overlap claim or add the PDF/TXT.
- `delatorre2002nonlinear_feature_space` and `rehr2015cepstral_noise`: Content support is good, but DOI fields were not verified in this pass.
- `tflite`: Consider replacing generic `TensorFlow Lite` page with official LiteRT for Microcontrollers documentation if this is intended to support MCU deployment details.

## Recommended manuscript edits

These are wording-level recommendations, not applied automatically.

### Line 58

Current wording asks four sources to support both triggering and classification. Suggested split:

> Existing acoustic weapon-detection work commonly treats impulsive-event detection and post-event interpretation as separate tasks. Artillery-focused studies have used acoustic sensors, thresholded or windowed event snippets, wavelet/time-frequency features, and neural classifiers for launch/impact or variant discrimination. Broader audio-classification work later established CNN-style classifiers over spectro-temporal inputs.

Then cite artillery sources after the first two clauses and `piczak2015esc_cnn` after the CNN clause.

### Line 68

Current wording is plausible but broad. Suggested narrower version:

> Battlefield-oriented acoustic studies show that artillery sensing depends on robust event windowing, propagation-tolerant features, sensor placement, and time-of-arrival or multi-sensor information.

This better matches `desai2006`, `hohil2007`, and `dagallier2019`.

### Line 83

Suggested tightening:

> In audio classification, CNNs over time-frequency representations learn local spectro-temporal patterns. For constrained hardware, related TinyML work shows that architecture and memory/runtime budgets must be considered jointly.

This keeps `piczak2015`/`hershey2017` tied to audio and `banbury2021` tied to constrained deployment.

### Line 86

Suggested tightening:

> Related gunshot and environmental-sound studies report strong results, but performance is often tied to dataset conditions, recording setup, label granularity, and augmentation strategy.

This removes the under-supported "device mismatch / overlapping sources" unless `ye2019multi_sound` or a DCASE/generalization source is explicitly verified and cited.

## Recommended BibTeX maintenance

Add DOI fields:

```bibtex
doi = {10.1109/TASSP.1980.1163420}
doi = {10.1109/5.726791}
doi = {10.1109/ICASSP.2017.7952132}
```

Candidate cleanup:

- Remove unused entries from `references.bib` if the bibliography file is manuscript-specific.
- Or keep unused entries if `references.bib` is a working project database, but do not treat them as part of the submitted reference list.

## Retraction / integrity notes

No fabricated in-text keys were detected. No cited source was identified as obviously nonexistent from local files and metadata checks.

Retraction screening was not fully completed against the Retraction Watch database, because no direct database access was performed in this pass. The cited sources are mostly IEEE/SPIE/ACM/MLSys/Sensors/Springer/IOP proceedings or articles. Before submission, run a final DOI-level check for retractions or expressions of concern for all journal articles, especially `raponi2022sound_of_guns`, `lamrini2023embedded_esr`, `svatos2024fcc`, `chu2009env_sound_tf`, and `he2009imbalanced`.

