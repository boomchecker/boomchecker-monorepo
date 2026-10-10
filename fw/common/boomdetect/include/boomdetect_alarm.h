/**
 * @file boomdetect_alarm.h
 * @brief The alarm over the per-window decisions: K-of-N votes, or a mean.
 *
 * One window is 448 ms and one logit; an alarm should be a property of
 * seconds. Two rules, selected per run:
 *
 * - VOTE (K of N with hysteresis): the alarm turns ON when at least `k_on` of
 *   the last `n` classified windows were called drone, and OFF again when
 *   fewer than `k_off` of the last `n` were. With k_off < k_on a decision
 *   hovering around the threshold does not flicker the state.
 * - MEAN (soft integration): the alarm is ON while the mean of the last `n`
 *   relative decisions - decision minus threshold - is >= 0, so a run of
 *   windows just below the threshold does not alarm while one window well
 *   above it can carry three weak ones. Windows before the first count as 0,
 *   exactly at the threshold.
 *
 * Squelched frames produce no window and so do not move the history: a
 * window that never closes cannot raise an alarm.
 *
 * Mirrored by training/boomdetect_train/decision.py, whose tests are the
 * specification the C tests repeat.
 */
#ifndef BOOMDETECT_ALARM_H
#define BOOMDETECT_ALARM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest history the bit mask and the ring below can hold. */
#define BOOMDETECT_ALARM_MAX_N 32u

/** How the last n windows combine. VOTE is 0 so that an initialiser that
    names only n/k_on/k_off keeps meaning what it did. */
#define BOOMDETECT_ALARM_VOTE 0u
#define BOOMDETECT_ALARM_MEAN 1u

typedef struct
{
    uint8_t n;     /**< windows remembered, 1..BOOMDETECT_ALARM_MAX_N */
    uint8_t k_on;  /**< VOTE: hits among the last n that turn the alarm on */
    uint8_t k_off; /**< VOTE: below this many hits the alarm turns off; k_off <= k_on */
    uint8_t mode;  /**< BOOMDETECT_ALARM_VOTE or BOOMDETECT_ALARM_MEAN */
} boomdetect_alarm_rule_t;

typedef struct
{
    boomdetect_alarm_rule_t rule;
    uint32_t                history; /**< bit i set: window i back was a drone call */
    float                   ring[BOOMDETECT_ALARM_MAX_N]; /**< last n relative decisions */
    uint8_t                 head;    /**< next ring slot to overwrite */
    bool                    on;
    uint32_t                onsets;  /**< OFF -> ON transitions since init */
} boomdetect_alarm_t;

/** @brief Adopt a rule and clear the history. false if the rule is inconsistent. */
bool boomdetect_alarm_init(boomdetect_alarm_t *a, const boomdetect_alarm_rule_t *rule);

/**
 * @brief Record one window's relative decision (decision minus threshold).
 *
 * Under VOTE a value >= 0 is a drone call; under MEAN the value itself enters
 * the mean. This is the entry point detect_service uses.
 * @return true if the alarm state CHANGED on this window.
 */
bool boomdetect_alarm_push_decision(boomdetect_alarm_t *a, float relative);

/** @brief Current state. */
bool boomdetect_alarm_on(const boomdetect_alarm_t *a);

/** @brief Drone calls (relative decision >= 0) among the last n windows. */
uint8_t boomdetect_alarm_hits(const boomdetect_alarm_t *a);

/** @brief Mean of the last n relative decisions, windows before the first as 0. */
float boomdetect_alarm_mean(const boomdetect_alarm_t *a);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_ALARM_H */
