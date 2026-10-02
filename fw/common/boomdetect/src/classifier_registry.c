/**
 * @file classifier_registry.c
 * @brief The table of models this image carries.
 *
 * A plain static array rather than linker-section auto-registration: adding a
 * model should be a visible edit in a file called "registry", not a side effect
 * of a section attribute that is invisible in the source and awkward to debug
 * when the linker script changes.
 *
 * A separate translation unit from the pipeline so that a consumer can supply
 * its own: nothing in src/boomdetect.c references s_models, only the four
 * accessors below, so linking a different object that defines them replaces the
 * whole table. tests/stub_registry.c does exactly that, which is what keeps the
 * claim from being decorative.
 */
#include "classifier.h"
#include "models.h"

#include <string.h>

/* First entry is the default. */
static const classifier_t *const s_models[] = {
    /* mlp_f2 and gbt_f2 (2026-10-01) add the second outdoor session - DJI at
       10-70 m, Runner 250 at 20-40 m, wind on the microphone - and a day of
       office confusers to the training set. On those 30.9 recordings mlp_f1
       alarmed on 2 of 16 (nothing beyond 10 m); mlp_f2, judged out of fold at
       its shipped threshold (1 false-alarm window per hour on public val),
       alarms on 10 of 16 and on 25 of all 33 drone recordings, with one false
       alarm in 22 own negatives (mouth buzz) and none per hour on 11.6 h of
       held-out public negatives. gbt_f2 is the second opinion from another
       family (23 of 33, 0 of 22, 1.9 public alarms per hour). Everything
       older stays in the image for `model`; moving mlp_f1 back to the front
       is the whole rollback.

       2026-10-02: the DJI straight overhead at 20-90 m and 30 min of traffic
       showed mlp_f2's raw logit positive out to 90 m while the 1 FA/h
       threshold cut everything past 30 m, so it ships at the usual 5 FA/h
       point (7.656) like every other model: all 7 heights, 13 of 16 of the
       30.9 takes, no alarm on 37 min of outdoor background and traffic,
       three office confusers. gbt_f3 is the forest retrained with those
       recordings: all 7 heights, none of 27 negatives. Not yet checked on
       the board. */
    &classifier_mlp_f2,
    &classifier_gbt_f3,
    &classifier_gbt_f2,
    &classifier_mlp_f1,
    &classifier_gbt_f1,
    &classifier_mlp_v6,
    &classifier_svm_v3,
    &classifier_mlp_l2,
    &classifier_gbt_l2,
    &classifier_gbt_reg_l2,
    &classifier_cnn_small,
};

size_t classifier_count(void)
{
    return sizeof(s_models) / sizeof(s_models[0]);
}

const classifier_t *classifier_at(size_t idx)
{
    return (idx < classifier_count()) ? s_models[idx] : NULL;
}

const classifier_t *classifier_default(void)
{
    return s_models[0];
}

const classifier_t *classifier_by_name(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    for (size_t i = 0u; i < classifier_count(); i++)
    {
        if (strcmp(s_models[i]->name, name) == 0)
        {
            return s_models[i];
        }
    }
    return NULL;
}
