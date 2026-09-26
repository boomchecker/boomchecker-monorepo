/**
 * @file boomdetect_alarm.h
 * @brief K-of-N alarm with hysteresis over the per-window decisions.
 *
 * One window is 448 ms and one logit; an alarm should be a property of
 * seconds. The alarm turns ON when at least `k_on` of the last `n` classified
 * windows were called drone, and OFF again when fewer than `k_off` of the last
 * `n` were. With k_off < k_on a decision hovering around the threshold does
 * not flicker the state. Squelched frames produce no window and so do not
 * move the history: a window that never closes cannot raise an alarm.
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

/** Longest history the bit mask below can hold. */
#define BOOMDETECT_ALARM_MAX_N 32u

typedef struct
{
    uint8_t n;     /**< windows remembered, 1..BOOMDETECT_ALARM_MAX_N */
    uint8_t k_on;  /**< hits among the last n that turn the alarm on */
    uint8_t k_off; /**< below this many hits the alarm turns off; k_off <= k_on */
} boomdetect_alarm_rule_t;

typedef struct
{
    boomdetect_alarm_rule_t rule;
    uint32_t                history; /**< bit i set: window i back was a drone call */
    uint8_t                 count;   /**< windows seen so far, saturating at n */
    bool                    on;
    uint32_t                onsets;  /**< OFF -> ON transitions since init */
} boomdetect_alarm_t;

/** @brief Adopt a rule and clear the history. false if the rule is inconsistent. */
bool boomdetect_alarm_init(boomdetect_alarm_t *a, const boomdetect_alarm_rule_t *rule);

/**
 * @brief Record one window's verdict.
 * @return true if the alarm state CHANGED on this window.
 */
bool boomdetect_alarm_push(boomdetect_alarm_t *a, bool is_drone);

/** @brief Current state. */
bool boomdetect_alarm_on(const boomdetect_alarm_t *a);

/** @brief Drone calls among the last n windows. */
uint8_t boomdetect_alarm_hits(const boomdetect_alarm_t *a);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_ALARM_H */
