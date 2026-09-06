/**
 * @file classifier_registry.c
 * @brief The table of models this image carries.
 *
 * A plain static array rather than linker-section auto-registration: adding a
 * model should be a visible edit in a file called "registry", not a side effect
 * of a section attribute that is invisible in the source and awkward to debug
 * when the linker script changes.
 *
 * Deliberately a separate translation unit from the pipeline, so a test can
 * link its own registry with a stub model instead of the real weights.
 */
#include "classifier.h"

#include <string.h>

extern const classifier_t classifier_mlp_v6;
extern const classifier_t classifier_svm_v3;

/* First entry is the default. */
static const classifier_t *const s_models[] = {
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
