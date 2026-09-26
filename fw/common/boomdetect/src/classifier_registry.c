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
    /* mlp_f1 is the first model trained on the node's own field recordings
       (run fw_aug, 2026-09-26; see models/models.h). Out of fold it alarmed on
       14 of 17 drone recordings and none of 11 negatives with 0.26 false
       alarms per hour on held-out public negatives; mlp_v6 at the same budget
       found 2 of 17, and its hand-set 3.0 buys 10 of 17 with 16.7 false alarms
       per hour. mlp_v6 and the rest stay in the image so the same audio can be
       scored by each, and moving mlp_v6 back to the front is the whole
       rollback. */
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
