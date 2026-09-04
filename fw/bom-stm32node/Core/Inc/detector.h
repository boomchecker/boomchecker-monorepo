/**
 ******************************************************************************
 * @file    detector.h
 * @brief   On-device drone detection: mic PCM -> 16 kHz -> MFCC -> linear SVM.
 *
 * Consumes 48 kHz PCM blocks from mic.c, decimates by 3 (alias-safe: the
 * pdm_pcm chain band-limits to 8 kHz), extracts MFCC frames (1024/512 @16 kHz)
 * and classifies 14-frame windows with the linear SVM from svm_model_data.h.
 * Results are streamed as text lines on the CDC console:
 *
 *   DET t=<s>.<ms> dec=<+d.ddd> <DRONE|noise>\n     one line per window
 *   DETEND windows=<n> drones=<n> overrun=<0|1> err=<0|1>\n
 *
 * Runs synchronously inside the CLI command (like pcm_stream_run); the USB
 * stack is serviced while waiting for microphone blocks.
 ******************************************************************************
 */
#ifndef DETECTOR_H
#define DETECTOR_H

#include <stdint.h>

/** Default operating point (units of 1/1000: 10 = RMS 0.010).
 * The threshold default belongs to the ACTIVE MODEL (see svm_classifier.c):
 * v6 MLP outputs raw logits and its champion operating point is +7.25
 * (pick_champion.py 2026-08-10: bebop/membo alarms with zero >=2-window
 * false alarms). For the legacy linear SVMs the natural default was 500.
 *
 * Raised to 15.0 after measuring the model on real hardware for the first
 * time. pick_champion.py's +7.25 was chosen on recordings, offline; on a node
 * with a working microphone it fires on ordinary room noise - 23 of 396
 * windows over three minutes of an office, decision values peaking at 12.05.
 * 15.0 clears that peak with about 25 % headroom and gave zero alarms on a
 * further three minutes of the same room.
 *
 * What that number is NOT: it was measured against ambient noise only, with no
 * drone present, so the sensitivity given up is unquantified. It is a field
 * default picked to stop crying wolf, not an operating point swept on labelled
 * data. Replacing it properly means recording drone and non-drone audio
 * THROUGH this hardware path and sweeping the threshold on that - see the
 * boomdetect package's skew report.
 *
 * Change this value and svm_classifier.c's model #include TOGETHER: nothing
 * enforces the pairing yet, and a linear SVM header left at a logit threshold
 * (its decision values live around +-3) is a detector that never fires. The
 * pairing moves into the classifier registry with the boomdetect package. */
#define DETECTOR_DEFAULT_SQUELCH_MILLI 10
#define DETECTOR_DEFAULT_THR_MILLI     15000

/**
 * @brief Run detection for `seconds` (clamped to 1..60) and stream results.
 * @param seconds       capture length
 * @param squelch_milli RMS squelch threshold in 1/1000 (0 disables the gate)
 * @param thr_milli     SVM decision threshold in 1/1000 (may be negative)
 * @param debug         non-zero: print an F=<frame> breadcrumb per frame
 */
void detector_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                  uint32_t debug);

#endif /* DETECTOR_H */
