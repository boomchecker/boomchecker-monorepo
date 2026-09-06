/**
 * @file models.h
 * @brief Every classifier_t this package's models/ directory exports.
 *
 * The registry used to carry these as bare `extern` declarations inside its own
 * .c file, which means nothing ever cross-checked them against the definitions:
 * a model file renaming its entry, or giving it a different type, would fail at
 * link time with a symbol name rather than at compile time with the mismatch.
 * Both sides include this instead.
 *
 * Adding a model is still two edits and no more: a file here, and a line in
 * src/classifier_registry.c - plus this declaration, which is what makes the
 * compiler check the pair.
 */
#ifndef BOOMDETECT_MODELS_H
#define BOOMDETECT_MODELS_H

#include "classifier.h"

extern const classifier_t classifier_mlp_v6;
extern const classifier_t classifier_svm_v3;

#endif /* BOOMDETECT_MODELS_H */
