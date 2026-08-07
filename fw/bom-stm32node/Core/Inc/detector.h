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

/** Default operating point (units of 1/1000: 10 = RMS 0.010, 500 = 0.5). */
#define DETECTOR_DEFAULT_SQUELCH_MILLI 10
#define DETECTOR_DEFAULT_THR_MILLI     500

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
