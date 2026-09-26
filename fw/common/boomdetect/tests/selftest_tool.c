/**
 * @file selftest_tool.c
 * @brief Host executable that prints the same DST* lines the board's
 *        `detselftest` prints, or compares them against a recorded fixture.
 *
 * The package's precedent is fw/common/boomlink's tests/codec_tool.c: a small
 * binary a Python harness can drive. Until it existed, boomdetect shipped three
 * self-contained test binaries and nothing anything else could call, so the
 * parity work the package is justified by had no surface to attach to, and the
 * checked-in fixture could not be reproduced without a board.
 *
 * Usage:
 *   boomdetect_selftest_tool [model]
 *       print the DST* lines (default model mlp_v6)
 *   boomdetect_selftest_tool --compare FILE [--rel TOL] [model]
 *       run, then compare against the DST* lines in FILE. Every hex token is
 *       decoded as an IEEE-754 float and must satisfy
 *       |a - b| <= TOL * max(|a|, |b|) + 1e-9; every other token must match
 *       exactly. TOL 0 (the default) is therefore bit-exactness. Exit 0 when
 *       everything is within tolerance, 1 otherwise.
 *
 * Why a tolerance exists at all: tests/vectors/selftest_host.txt is the
 * baseline of ONE toolchain (the Linux CI compiler). The same C on MinGW-w64
 * -O2 differs in the last places - measured 4.1e-7 relative on the worst
 * feature and 1.1e-6 on a decision, the same scale as the board-versus-host
 * gap recorded in that file's header. Bit-exactness is a same-binary claim, so
 * the CMake side passes --rel only where the binary is known to differ.
 */
#include "boomdetect.h"
#include "boomdetect_selftest.h"
#include "classifier.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static boomdetect_t s_det;
static int16_t      s_block[BOOMDETECT_SELFTEST_BLOCK];

/* The run is 21 lines today; leave room for a longer fixture without silently
   truncating it, which would make a shortened run compare equal. */
#define MAX_LINES 256
#define MAX_LINE  512

typedef struct
{
    char   lines[MAX_LINES][MAX_LINE];
    size_t n;
    bool   overflow;
} capture_t;

static void emit_stdout(void *ctx, const char *line)
{
    (void)ctx;
    puts(line);
}

static void emit_capture(void *ctx, const char *line)
{
    capture_t *c = (capture_t *)ctx;
    if (c->n >= MAX_LINES)
    {
        c->overflow = true;
        return;
    }
    snprintf(c->lines[c->n], MAX_LINE, "%s", line);
    c->n++;
}

/* DST* lines of a fixture file; comments and blanks are documentation. */
static bool read_fixture(const char *path, capture_t *out)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        fprintf(stderr, "cannot open fixture %s\n", path);
        return false;
    }
    char buf[MAX_LINE];
    out->n = 0u;
    out->overflow = false;
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        size_t len = strlen(buf);
        while (len > 0u && (buf[len - 1u] == '\n' || buf[len - 1u] == '\r'))
        {
            buf[--len] = '\0';
        }
        if (strncmp(buf, "DST", 3) != 0)
        {
            continue;
        }
        if (out->n >= MAX_LINES)
        {
            out->overflow = true;
            break;
        }
        snprintf(out->lines[out->n], MAX_LINE, "%s", buf);
        out->n++;
    }
    fclose(f);
    return true;
}

/* Exactly eight hex digits: the emitter prints %08lX and nothing else looks
   like that (frame counters are decimal). */
static bool hex_token(const char *tok, float *v)
{
    if (strlen(tok) != 8u)
    {
        return false;
    }
    for (size_t i = 0u; i < 8u; i++)
    {
        const char c = tok[i];
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        if (!ok)
        {
            return false;
        }
    }
    uint32_t u = (uint32_t)strtoul(tok, NULL, 16);
    memcpy(v, &u, sizeof(*v));
    return true;
}

/* "logit=C11D44B8" style: the value follows the last '='. */
static const char *value_part(const char *tok)
{
    const char *eq = strrchr(tok, '=');
    return (eq != NULL) ? eq + 1 : tok;
}

typedef struct
{
    unsigned long values;
    unsigned long differing;
    double        worst_rel;
    char          worst_where[64];
} stats_t;

/* Compare one line token by token. Returns false on a mismatch the tolerance
   cannot excuse (a non-numeric token, a token count, a value out of tolerance). */
static bool compare_line(const char *want, const char *got, double tol, stats_t *st, size_t idx)
{
    char  w[MAX_LINE], g[MAX_LINE];
    char *wt, *gt, *ws = NULL, *gs = NULL;
    bool  ok = true;

    snprintf(w, sizeof(w), "%s", want);
    snprintf(g, sizeof(g), "%s", got);

    wt = strtok_r(w, " ", &ws);
    gt = strtok_r(g, " ", &gs);
    while (wt != NULL && gt != NULL)
    {
        float a, b;
        if (hex_token(value_part(wt), &a) && hex_token(value_part(gt), &b) &&
            strncmp(wt, gt, (size_t)(value_part(wt) - wt)) == 0)
        {
            st->values++;
            if (memcmp(&a, &b, sizeof(a)) != 0)
            {
                const double da = fabs((double)a), db = fabs((double)b);
                const double mx = (da > db) ? da : db;
                const double diff = fabs((double)a - (double)b);
                const double rel = diff / ((mx > 0.0) ? mx : 1.0);
                st->differing++;
                if (rel > st->worst_rel)
                {
                    st->worst_rel = rel;
                    snprintf(st->worst_where, sizeof(st->worst_where), "line %lu (%.9g vs %.9g)",
                             (unsigned long)idx, (double)a, (double)b);
                }
                if (diff > tol * mx + 1e-9)
                {
                    ok = false;
                }
            }
        }
        else if (strcmp(wt, gt) != 0)
        {
            ok = false;
        }
        wt = strtok_r(NULL, " ", &ws);
        gt = strtok_r(NULL, " ", &gs);
    }
    if (wt != NULL || gt != NULL)
    {
        ok = false; /* token counts differ */
    }
    return ok;
}

static const char *find_sig(const capture_t *c)
{
    for (size_t i = 0u; i < c->n; i++)
    {
        if (strncmp(c->lines[i], "DSTSIG", 6) == 0)
        {
            return c->lines[i];
        }
    }
    return NULL;
}

static int run_compare(const classifier_t *model, const char *fixture, double tol)
{
    static capture_t want, got;
    if (!read_fixture(fixture, &want))
    {
        return 2;
    }
    if (want.overflow || want.n == 0u)
    {
        fprintf(stderr, "fixture %s has %s DST lines\n", fixture,
                want.overflow ? "too many" : "no");
        return 2;
    }

    got.n = 0u;
    got.overflow = false;
    if (!boomdetect_selftest(&s_det, model, s_block, BOOMDETECT_SELFTEST_BLOCK, emit_capture,
                             &got))
    {
        fprintf(stderr, "selftest run failed\n");
        return 1;
    }
    if (got.overflow)
    {
        fprintf(stderr, "selftest produced more than %d lines\n", MAX_LINES);
        return 1;
    }

    /* DSTSIG first: it is the checksum of the generated INPUT, so a mismatch
       there means the two sides did not score the same signal and every
       downstream difference is a consequence rather than a finding of its own. */
    const char *sw = find_sig(&want), *sg = find_sig(&got);
    if (sw == NULL || sg == NULL || strcmp(sw, sg) != 0)
    {
        fprintf(stderr, "input signature differs, so nothing downstream is comparable:\n"
                        "  expected %s\n  got      %s\n",
                (sw != NULL) ? sw : "<missing>", (sg != NULL) ? sg : "<missing>");
        return 1;
    }
    if (want.n != got.n)
    {
        fprintf(stderr, "expected %lu DST lines, got %lu\n", (unsigned long)want.n,
                (unsigned long)got.n);
        return 1;
    }

    stats_t st = { 0u, 0u, 0.0, "" };
    int     bad = 0;
    for (size_t i = 0u; i < want.n; i++)
    {
        if (!compare_line(want.lines[i], got.lines[i], tol, &st, i))
        {
            if (bad == 0)
            {
                fprintf(stderr, "line %lu differs beyond tolerance %g:\n  expected %s\n  got      %s\n",
                        (unsigned long)i, tol, want.lines[i], got.lines[i]);
            }
            bad++;
        }
    }
    printf("selftest: %lu lines, %lu values compared, %lu differ, worst %.3g relative%s%s "
           "(tolerance %g)\n",
           (unsigned long)want.n, st.values, st.differing, st.worst_rel,
           (st.differing != 0u) ? " at " : "", (st.differing != 0u) ? st.worst_where : "", tol);
    if (bad != 0)
    {
        fprintf(stderr, "%d of %lu lines outside tolerance; see the header of %s for what a "
                        "moved number means\n",
                bad, (unsigned long)want.n, fixture);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *name    = "mlp_v6";
    const char *fixture = NULL;
    double      tol     = 0.0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--compare") == 0 && i + 1 < argc)
        {
            fixture = argv[++i];
        }
        else if (strcmp(argv[i], "--rel") == 0 && i + 1 < argc)
        {
            tol = strtod(argv[++i], NULL);
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "usage: %s [--compare FILE [--rel TOL]] [model]\n", argv[0]);
            return 2;
        }
        else
        {
            name = argv[i];
        }
    }

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

    if (fixture != NULL)
    {
        return run_compare(model, fixture, tol);
    }

    if (!boomdetect_selftest(&s_det, model, s_block, BOOMDETECT_SELFTEST_BLOCK, emit_stdout, NULL))
    {
        return 1;
    }
    return 0;
}
