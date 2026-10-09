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
 *   DET t=<s>.<ms> span=<n> dec=<+d.ddd> <DRONE|noise>
 *                                                   one per classified window;
 *                                                   t is when the window CLOSED
 *                                                   and span is how many frames
 *                                                   it covered, which is not a
 *                                                   constant: the gate resets
 *                                                   accumulation, so a window
 *                                                   can straddle silence
 *   ALM t=<s>.<ms> <ON|OFF> hits=<k>/<n>            the alarm changed state on
 *   ALM t=<s>.<ms> <ON|OFF> mean=<+d.ddd>/<n>       the window that closed at t
 *                                                   (vote rule / mean rule)
 *   F=<n> a=<n> r=<n> h=<us> m=<us>                 per frame, only with dbg=1
 *   DETEND windows=<n> drones=<n> alarms=<n> first_drone=<s.mmm|->
 *          first_alarm=<s.mmm|-> overrun=<0|1> err=<0|1>
 *   DETERR <reason>                                 followed by DETEND, always
 *
 * detect_service_selftest() prints a separate DST* family; see
 * fw/common/boomdetect/include/boomdetect_selftest.h for its grammar.
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

#include "boomdetect_alarm.h"
#include "classifier.h"

/** Default RMS gate, in 1/1000 of full scale: outdoors the background sat at
    RMS 0.004 and a drone at 20 m and beyond at 0.004-0.009. A quiet room
    (0.0026) still yields no window - one needs 14 frames in a row above it. */
#define DETECT_DEFAULT_SQUELCH_MILLI 3

/* Longest timed run `detect` accepts, one day (64-bit timestamps, 32-bit
   counters). What a long run costs is the radio, which is not serviced while
   `detect` runs. `detect 0` has no limit and ends on the first console byte. */
#define DETECT_MAX_SECONDS 86400

/* The default alarm rule (boomdetect_alarm.h): ON when at least K_ON of the
   last N classified windows were called drone, OFF when fewer than K_OFF were -
   the rule the training package judges clip verdicts with. `detect` takes
   another per run as its fifth argument: `<k>of<n>` or `mean<n>`. */
#define DETECT_ALARM_N     4
#define DETECT_ALARM_K_ON  2
#define DETECT_ALARM_K_OFF 1
#define DETECT_ALARM_RULE_DEFAULT "2of4"

/* The decision threshold is NOT here. It belongs to the model - a linear SVM's
   decisions live around +-3 while an MLP's are unbounded logits - so it is
   classifier_t::default_thr_milli, set in each model's models/model_*.c. */

/**
 * @brief Run detection for `seconds` and stream results.
 * @param seconds       capture length, clamped to 1..DETECT_MAX_SECONDS; 0 runs
 *                      until the console receives any byte (usb_cli_key_pressed)
 * @param squelch_milli RMS squelch threshold in 1/1000 (0 disables the gate)
 * @param thr_milli     decision threshold in 1/1000 (may be negative)
 * @param debug         non-zero: print an F=<frame> breadcrumb per frame
 * @param rule          alarm rule for this run; NULL = the DETECT_ALARM_* default
 */
void detect_service_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                        uint32_t debug, const boomdetect_alarm_rule_t *rule);

/**
 * @brief Parse an alarm rule token: `<k>of<n>` (vote, off below k-1, at least
 *        1) or `mean<n>` (mean of the last n relative decisions), n 1..32.
 * @return false if the token is not one of those or the rule is inconsistent.
 */
bool detect_service_parse_rule(const char *token, boomdetect_alarm_rule_t *out);

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
