/**
 * @file boomdetect_selftest.h
 * @brief The deterministic regression fixture generator, portable.
 *
 * The detector's only real input is a live microphone, which never repeats, so
 * two `detect` runs can never be compared. This drives the whole chain from an
 * integer LCG instead and reports every stage as raw IEEE-754 bit patterns, so
 * "unchanged" means unchanged rather than "unchanged to six decimals".
 *
 * It lives in the package rather than in the firmware port because the fixture
 * it produces lives here too: tests/vectors/. A package that ships reference
 * data it cannot regenerate has the dependency pointing the wrong way, and it
 * is what kept the host build from being able to check a single line of it.
 *
 * Emission is a callback so the same generator serves both consumers: the
 * firmware appends CRLF and writes to the USB console, the host tool appends LF
 * and writes to stdout. Lines arrive WITHOUT a terminator for that reason.
 */
#ifndef BOOMDETECT_SELFTEST_H
#define BOOMDETECT_SELFTEST_H

#include "boomdetect.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Input length in 48 kHz samples: enough for THREE full windows (42 frames of
 * 1024/512 at 16 kHz, tripled). One window would leave the accumulator reset
 * between windows untested, which is exactly the kind of state bug a refactor
 * introduces.
 */
#define BOOMDETECT_SELFTEST_INPUT_LEN 66048u

/** LCG seed. Any change here invalidates every checked-in fixture. */
#define BOOMDETECT_SELFTEST_SEED 1u

/** How many frames' MFCC coefficients are printed. */
#define BOOMDETECT_SELFTEST_MFCC_FRAMES 3u

/**
 * Block size the signal is fed in, in 48 kHz samples.
 *
 * Not one push: the FIFO holds 4096 decimated samples and the signal decimates
 * to 22016, so a single push would overflow it and start dropping. Must stay a
 * multiple of the decimation factor or the kept samples shift and every fixture
 * moves; boomdetect_selftest() static-asserts nothing about the caller's buffer
 * beyond its length, so that invariant is checked where the constant is used.
 */
#define BOOMDETECT_SELFTEST_BLOCK 3072u

/** One line of output, without a terminator; the caller appends its own. */
typedef void (*boomdetect_selftest_emit_fn)(void *ctx, const char *line);

/**
 * @brief Run the fixture through @p d and emit the DST* line protocol.
 *
 * @param d          detector to drive; re-initialised here, so any previous
 *                   state is discarded.
 * @param model      classifier to pin the run to. Must not be NULL: passing
 *                   NULL to boomdetect_init() is not an error there, it
 *                   silently means "use the default", which would measure a
 *                   different model while the reference file sent the reader
 *                   hunting for a lost -O2.
 * @param block      caller-owned scratch, at least @p block_len samples. The
 *                   package allocates nothing; on the firmware this is 6 KB
 *                   that must not be a stack frame.
 * @param block_len  samples in @p block; must be a multiple of the decimation
 *                   factor and no larger than BOOMDETECT_SELFTEST_BLOCK.
 * @param emit       receives each line; must not be NULL.
 * @param ctx        passed through to @p emit.
 *
 * @return false if an argument was rejected or init failed. The DSTERR and
 *         DSTEND lines are emitted either way, so a reader always sees a
 *         terminator.
 */
bool boomdetect_selftest(boomdetect_t *d, const classifier_t *model, int16_t *block,
                         size_t block_len, boomdetect_selftest_emit_fn emit, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_SELFTEST_H */
