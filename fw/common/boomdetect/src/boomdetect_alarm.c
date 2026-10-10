/**
 * @file boomdetect_alarm.c
 * @brief K-of-N with hysteresis, or the mean of the last n; see boomdetect_alarm.h.
 */
#include "boomdetect_alarm.h"

#include <string.h>

bool boomdetect_alarm_init(boomdetect_alarm_t *a, const boomdetect_alarm_rule_t *rule)
{
    if (a == NULL || rule == NULL)
    {
        return false;
    }
    if (rule->n == 0u || rule->n > BOOMDETECT_ALARM_MAX_N)
    {
        return false;
    }
    if (rule->mode == BOOMDETECT_ALARM_VOTE)
    {
        if (rule->k_off == 0u || rule->k_off > rule->k_on || rule->k_on > rule->n)
        {
            return false;
        }
    }
    else if (rule->mode != BOOMDETECT_ALARM_MEAN)
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

float boomdetect_alarm_mean(const boomdetect_alarm_t *a)
{
    /* Summed afresh each time: n is at most 32 and a running sum would drift. */
    float sum = 0.0f;
    for (uint32_t i = 0u; i < a->rule.n; i++)
    {
        sum += a->ring[i];
    }
    return sum / (float)a->rule.n;
}

bool boomdetect_alarm_push_decision(boomdetect_alarm_t *a, float relative)
{
    const bool was = a->on;
    a->history         = (a->history << 1) | ((relative >= 0.0f) ? 1u : 0u);
    a->ring[a->head]   = relative;
    a->head            = (uint8_t)((a->head + 1u) % a->rule.n);
    if (a->rule.mode == BOOMDETECT_ALARM_MEAN)
    {
        a->on = boomdetect_alarm_mean(a) >= 0.0f;
    }
    else
    {
        const uint8_t hits = boomdetect_alarm_hits(a);
        if (!a->on && hits >= a->rule.k_on)
        {
            a->on = true;
        }
        else if (a->on && hits < a->rule.k_off)
        {
            a->on = false;
        }
    }
    if (a->on && !was)
    {
        a->onsets++;
    }
    return a->on != was;
}

bool boomdetect_alarm_on(const boomdetect_alarm_t *a)
{
    return a->on;
}
