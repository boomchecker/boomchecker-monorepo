/**
 ******************************************************************************
 * @file    detect_service.h
 * @brief   The detection pipeline's one call site on this board.
 *
 * fw/common/boomdetect owns the arithmetic - decimation, framing, squelch,
 * MFCC, aggregation, classification - and knows nothing about this hardware.
 * This file is the target-specific remainder: it pulls PCM out of mic.h, paces
 * the pipeline so the superloop keeps its real-time budget, and renders the
 * results as console lines. Same split, and the same reason for it, as
 * App/link/link_service.c has against fw/common/boomlink.
 *
 * Console output, all lines CRLF-terminated:
 *
 *   LVL t=<s>.<ms> rms=<+d.ddd>                     input level, ~1/s
 *   DET t=<s>.<ms> dec=<+d.ddd> <DRONE|noise>       one per classified window
 *   DETEND windows=<n> drones=<n> overrun=<0|1> err=<0|1>
 *   DETERR <reason>                                 followed by DETEND, always
 *
 * Runs synchronously inside the CLI command, servicing USB while it waits for
 * microphone blocks. The radio is NOT serviced meanwhile - see
 * docs/firmware/bom-stm32node/boomlink.md section 6.2.
 ******************************************************************************
 */
#ifndef DETECT_SERVICE_H
#define DETECT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "classifier.h"

/** Default RMS gate, in 1/1000 of full scale. */
#define DETECT_DEFAULT_SQUELCH_MILLI 10

/**
 * Default decision threshold, in 1/1000. Belongs to the model compiled into
 * fw/common/boomdetect (currently the v6 MLP, whose decision is a raw logit
 * rather than a probability - hence a value nowhere near 0.5).
 *
 * 15.0 was measured on hardware: pick_champion.py's offline +7.25 fired on 23
 * of 396 windows of ordinary room noise, peaking at 12.05, and a second
 * three-minute campaign at 15.0 saw none. That was ambient noise with no drone
 * present, so the sensitivity it gives up is unquantified - it is a field
 * default chosen to stop crying wolf, not an operating point swept on labelled
 * data.
 *
 * Change this together with the model include in boomdetect: a linear SVM
 * header left at a logit threshold (its decisions live around +-3) is a
 * detector that never fires. The classifier registry makes the pairing
 * mechanical.
 */
#define DETECT_DEFAULT_THR_MILLI 15000

/**
 * @brief Run detection for `seconds` (clamped to 1..60) and stream results.
 * @param seconds       capture length
 * @param squelch_milli RMS squelch threshold in 1/1000 (0 disables the gate)
 * @param thr_milli     decision threshold in 1/1000 (may be negative)
 * @param debug         non-zero: print an F=<frame> breadcrumb per frame
 */
void detect_service_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                        uint32_t debug);

/**
 * @brief Run the pipeline over a deterministic synthetic signal and print every
 *        stage as raw IEEE-754 bit patterns.
 *
 * Exists to make a numerically sensitive refactor checkable: the live
 * microphone never repeats an input, so two `detect` runs can never be
 * compared. This one feeds an integer LCG (bit-identical on any platform, no
 * flash cost) through the same pipeline and prints hex bit patterns rather than
 * decimal, so "unchanged" means unchanged rather than "agrees to six places".
 *
 * Its reference output is checked in at
 * fw/common/boomdetect/tests/vectors/selftest_expected.txt.
 */
void detect_service_selftest(void);

/**
 * @brief The model `detect` will use, and its default threshold.
 *
 * Not persisted: a reset returns to the deployed model, the same way `micslot`
 * behaves. This is a bring-up and comparison knob, not configuration.
 */
const classifier_t *detect_service_model(void);

/** @brief Select a model by name. false if there is no such model. */
bool detect_service_set_model(const char *name);

#endif /* DETECT_SERVICE_H */
