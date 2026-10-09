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
    /* gbt_m1 (layout 4, the modulation spectrum of the 1-4 kHz envelope added
       to layout 2; see models.h) is the default since 2026-10-09. It ran on the
       board from 2026-10-04 and outdoors on 2026-10-05 kept the DJI hovering at
       60-100 m where mlp_f2 faded, with no alarm on the background; across five
       training seeds its operating point barely moves, where mlp_m1's does.
       The first two seconds of a run produce no decision while the envelope
       ring fills. mlp_f2 (layout 2) was the default before and stays for
       rollback - moving it back to the front is the whole change. mlp_v6 and
       svm_v3 are the public-data models the selftest and the parity harness
       are anchored to. The other field-trained models of 2026-09/10 (gbt_f1-3,
       mlp_f1, the public-only mlp_l2/gbt_l2/gbt_reg_l2 and cnn_small) left the
       image on 2026-10-09: none was ahead of these on the field recordings. */
    &classifier_gbt_m1,
    &classifier_mlp_f2,
    &classifier_mlp_m1,
    &classifier_mlp_v6,
    &classifier_svm_v3,
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
