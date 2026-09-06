/**
 * @file selftest_tool.c
 * @brief Host executable that prints the same DST* lines the board's
 *        `detselftest` prints.
 *
 * The package's precedent is fw/common/boomlink's tests/codec_tool.c: a small
 * binary a Python harness can drive. Until it existed, boomdetect shipped three
 * self-contained test binaries and nothing anything else could call, so the
 * parity work the package is justified by had no surface to attach to, and the
 * checked-in fixture could not be reproduced without a board.
 *
 * Usage: boomdetect_selftest_tool [model]     (default mlp_v6)
 */
#include "boomdetect.h"
#include "boomdetect_selftest.h"
#include "classifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static boomdetect_t s_det;
static int16_t      s_block[BOOMDETECT_SELFTEST_BLOCK];

static void emit(void *ctx, const char *line)
{
    (void)ctx;
    puts(line);
}

int main(int argc, char **argv)
{
    const char *name = (argc > 1) ? argv[1] : "mlp_v6";

    const classifier_t *model = classifier_by_name(name);
    if (model == NULL)
    {
        fprintf(stderr, "no such model '%s'; available:\n", name);
        for (size_t i = 0u; i < classifier_count(); i++)
        {
            fprintf(stderr, "  %s\n", classifier_at(i)->name);
        }
        return 2;
    }

    if (!boomdetect_selftest(&s_det, model, s_block, BOOMDETECT_SELFTEST_BLOCK, emit, NULL))
    {
        return 1;
    }
    return 0;
}
