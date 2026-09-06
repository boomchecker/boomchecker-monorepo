/**
 * @file boomdetect_alarm.c
 * @brief K-of-N with hysteresis; see boomdetect_alarm.h.
 */
#include "boomdetect_alarm.h"

#include <string.h>

bool boomdetect_alarm_init(boomdetect_alarm_t *a, const boomdetect_alarm_rule_t *rule)
{
    if (a == NULL || rule == NULL)
    {
        return false;
    }
    if (rule->n == 0u || rule->n > BOOMDETECT_ALARM_MAX_N || rule->k_off == 0u ||
        rule->k_off > rule->k_on || rule->k_on > rule->n)
    {
        return false;
    }
    memset(a, 0, sizeof(*a));
    a->rule = *rule;
    return true;
}

uint8_t boomdetect_alarm_hits(const boomdetect_alarm_t *a)
{
    /* Only the last n bits count; the mask below never holds more, but say so. */
    const uint32_t mask = (a->rule.n >= 32u) ? 0xFFFFFFFFu : ((1u << a->rule.n) - 1u);
    uint32_t       bits = a->history & mask;
    uint8_t        hits = 0u;
    while (bits != 0u)
    {
        hits += (uint8_t)(bits & 1u);
        bits >>= 1;
    }
    return hits;
}

bool boomdetect_alarm_push(boomdetect_alarm_t *a, bool is_drone)
{
    const bool was = a->on;
    a->history = (a->history << 1) | (is_drone ? 1u : 0u);
    if (a->count < a->rule.n)
    {
        a->count++;
    }
    const uint8_t hits = boomdetect_alarm_hits(a);
    if (!a->on && hits >= a->rule.k_on)
    {
        a->on = true;
        a->onsets++;
    }
    else if (a->on && hits < a->rule.k_off)
    {
        a->on = false;
    }
    return a->on != was;
}

bool boomdetect_alarm_on(const boomdetect_alarm_t *a)
{
    return a->on;
}
