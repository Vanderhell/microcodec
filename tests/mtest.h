/*
 * microtest — Single-header test framework for embedded C libraries.
 *
 * Replaces the ad-hoc test macros scattered across the micro* ecosystem.
 * Provides: assertions, test registration, suites, setup/teardown, filtering,
 * color output, XML report, and summary stats.
 *
 * C99 · Single header · Zero dependencies · Zero allocations · Portable
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microtest
 *
 * Usage:
 *   #define MTEST_IMPLEMENTATION    // in exactly ONE .c file
 *   #include "mtest.h"
 *
 *   MTEST(my_test) {
 *       MTEST_ASSERT_EQ(2 + 2, 4);
 *       MTEST_ASSERT_STR_EQ("hello", "hello");
 *   }
 *
 *   MTEST_SUITE(math) {
 *       MTEST_RUN(my_test);
 *   }
 *
 *   int main(int argc, char **argv) {
 *       MTEST_BEGIN(argc, argv);
 *       MTEST_SUITE_RUN(math);
 *       return MTEST_END();
 *   }
 */

#ifndef MTEST_H
#define MTEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* ── Configuration ─────────────────────────────────────────────────────── */

/** Enable ANSI color output. */
#ifndef MTEST_ENABLE_COLOR
#define MTEST_ENABLE_COLOR 1
#endif

/** Enable timing (requires clock()). */
#ifndef MTEST_ENABLE_TIMING
#define MTEST_ENABLE_TIMING 1
#endif

/** Maximum test name length for filtering. */
#ifndef MTEST_MAX_NAME
#define MTEST_MAX_NAME 64
#endif

/* ── Color codes ───────────────────────────────────────────────────────── */

#if MTEST_ENABLE_COLOR
#define MTEST_CLR_RESET  "\033[0m"
#define MTEST_CLR_GREEN  "\033[32m"
#define MTEST_CLR_RED    "\033[31m"
#define MTEST_CLR_YELLOW "\033[33m"
#define MTEST_CLR_CYAN   "\033[36m"
#define MTEST_CLR_BOLD   "\033[1m"
#define MTEST_CLR_DIM    "\033[2m"
#else
#define MTEST_CLR_RESET  ""
#define MTEST_CLR_GREEN  ""
#define MTEST_CLR_RED    ""
#define MTEST_CLR_YELLOW ""
#define MTEST_CLR_CYAN   ""
#define MTEST_CLR_BOLD   ""
#define MTEST_CLR_DIM    ""
#endif

/* ── Global state ──────────────────────────────────────────────────────── */

typedef struct {
    int  tests_run;
    int  tests_passed;
    int  tests_failed;
    int  tests_skipped;
    int  asserts_total;
    int  asserts_failed;
    int  suites_run;

    /* Current test state */
    const char *current_test;
    const char *current_suite;
    int         current_failed;     /* assertions failed in current test */

    /* Filtering */
    const char *filter;             /* --filter=NAME substring match     */
    int         verbose;            /* -v: show passing asserts          */
    int         stop_on_fail;       /* -x: stop after first failure      */
    int         list_only;          /* -l: list tests, don't run         */

#if MTEST_ENABLE_TIMING
    double      total_time_ms;
    double      suite_time_ms;
#endif
} mtest_state_t;

/* Extern declaration — defined in MTEST_IMPLEMENTATION block */
#ifndef MTEST_IMPLEMENTATION
extern mtest_state_t mtest_g;
#endif

/* ── Test definition macros ────────────────────────────────────────────── */

/** Define a test function. */
#define MTEST(name)  static void name(void)

/** Define a suite function. */
#define MTEST_SUITE(name)  static void mtest_suite_##name(void)

/** Run a suite. */
#define MTEST_SUITE_RUN(name)  do {                                       \
    mtest_g.current_suite = #name;                                         \
    mtest_g.suites_run++;                                                  \
    printf("\n" MTEST_CLR_BOLD "[%s]" MTEST_CLR_RESET "\n", #name);        \
    mtest_suite_##name();                                                  \
    mtest_g.current_suite = NULL;                                          \
} while (0)

/** Run a test within a suite. */
#define MTEST_RUN(name)  do {                                             \
    if (mtest_g.stop_on_fail && mtest_g.tests_failed > 0) break;          \
    if (mtest_g.filter != NULL &&                                          \
        strstr(#name, mtest_g.filter) == NULL) {                           \
        mtest_g.tests_skipped++;                                           \
        break;                                                             \
    }                                                                      \
    if (mtest_g.list_only) {                                               \
        printf("  %s\n", #name);                                           \
        break;                                                             \
    }                                                                      \
    mtest_g.tests_run++;                                                   \
    mtest_g.current_test = #name;                                          \
    mtest_g.current_failed = 0;                                            \
    printf("  %-55s ", #name);                                             \
    fflush(stdout);                                                        \
    name();                                                                \
    if (mtest_g.current_failed == 0) {                                     \
        mtest_g.tests_passed++;                                            \
        printf(MTEST_CLR_GREEN "PASS" MTEST_CLR_RESET "\n");              \
    }                                                                      \
} while (0)

/* ── Setup / Teardown ──────────────────────────────────────────────────── */

/** Run a test with setup and teardown functions. */
#define MTEST_RUN_F(name, setup_fn, teardown_fn)  do {                    \
    if (mtest_g.stop_on_fail && mtest_g.tests_failed > 0) break;          \
    if (mtest_g.filter != NULL &&                                          \
        strstr(#name, mtest_g.filter) == NULL) {                           \
        mtest_g.tests_skipped++;                                           \
        break;                                                             \
    }                                                                      \
    if (mtest_g.list_only) {                                               \
        printf("  %s\n", #name);                                           \
        break;                                                             \
    }                                                                      \
    mtest_g.tests_run++;                                                   \
    mtest_g.current_test = #name;                                          \
    mtest_g.current_failed = 0;                                            \
    printf("  %-55s ", #name);                                             \
    fflush(stdout);                                                        \
    setup_fn();                                                            \
    name();                                                                \
    teardown_fn();                                                         \
    if (mtest_g.current_failed == 0) {                                     \
        mtest_g.tests_passed++;                                            \
        printf(MTEST_CLR_GREEN "PASS" MTEST_CLR_RESET "\n");              \
    }                                                                      \
} while (0)

/* ── Assertion macros ──────────────────────────────────────────────────── */

#define MTEST_FAIL_(fmt, ...) do {                                        \
    if (mtest_g.current_failed == 0) {                                     \
        printf(MTEST_CLR_RED "FAIL" MTEST_CLR_RESET "\n");                \
        mtest_g.tests_failed++;                                            \
    }                                                                      \
    mtest_g.current_failed++;                                              \
    mtest_g.asserts_failed++;                                              \
    printf("    " MTEST_CLR_RED "%s:%d: " MTEST_CLR_RESET fmt "\n",       \
           __FILE__, __LINE__, __VA_ARGS__);                               \
    return;                                                                \
} while (0)

/** Assert that two integers are equal. */
#define MTEST_ASSERT_EQ(expected, actual) do {                            \
    mtest_g.asserts_total++;                                               \
    long long mtest_e_ = (long long)(expected);                            \
    long long mtest_a_ = (long long)(actual);                              \
    if (mtest_e_ != mtest_a_) {                                            \
        MTEST_FAIL_("expected %lld, got %lld", mtest_e_, mtest_a_);       \
    }                                                                      \
} while (0)

/** Assert that two integers are NOT equal. */
#define MTEST_ASSERT_NE(a, b) do {                                        \
    mtest_g.asserts_total++;                                               \
    if ((long long)(a) == (long long)(b)) {                                \
        MTEST_FAIL_("expected != %lld", (long long)(a));                  \
    }                                                                      \
} while (0)

/** Assert a > b. */
#define MTEST_ASSERT_GT(a, b) do {                                        \
    mtest_g.asserts_total++;                                               \
    if (!((long long)(a) > (long long)(b))) {                              \
        MTEST_FAIL_("%lld not > %lld", (long long)(a), (long long)(b));   \
    }                                                                      \
} while (0)

/** Assert a >= b. */
#define MTEST_ASSERT_GE(a, b) do {                                        \
    mtest_g.asserts_total++;                                               \
    if (!((long long)(a) >= (long long)(b))) {                             \
        MTEST_FAIL_("%lld not >= %lld", (long long)(a), (long long)(b));  \
    }                                                                      \
} while (0)

/** Assert a < b. */
#define MTEST_ASSERT_LT(a, b) do {                                        \
    mtest_g.asserts_total++;                                               \
    if (!((long long)(a) < (long long)(b))) {                              \
        MTEST_FAIL_("%lld not < %lld", (long long)(a), (long long)(b));   \
    }                                                                      \
} while (0)

/** Assert expression is true. */
#define MTEST_ASSERT_TRUE(expr) do {                                      \
    mtest_g.asserts_total++;                                               \
    if (!(expr)) {                                                         \
        MTEST_FAIL_("expected true: %s", #expr);                          \
    }                                                                      \
} while (0)

/** Assert expression is false. */
#define MTEST_ASSERT_FALSE(expr) do {                                     \
    mtest_g.asserts_total++;                                               \
    if ((expr)) {                                                          \
        MTEST_FAIL_("expected false: %s", #expr);                         \
    }                                                                      \
} while (0)

/** Assert pointer is NULL. */
#define MTEST_ASSERT_NULL(ptr) do {                                       \
    mtest_g.asserts_total++;                                               \
    if ((ptr) != NULL) {                                                   \
        MTEST_FAIL_("expected NULL, got %p", (void *)(ptr));              \
    }                                                                      \
} while (0)

/** Assert pointer is not NULL. */
#define MTEST_ASSERT_NOT_NULL(ptr) do {                                   \
    mtest_g.asserts_total++;                                               \
    if ((ptr) == NULL) {                                                   \
        MTEST_FAIL_("expected non-NULL: %s", #ptr);                       \
    }                                                                      \
} while (0)

/** Assert two strings are equal. */
#define MTEST_ASSERT_STR_EQ(expected, actual) do {                        \
    mtest_g.asserts_total++;                                               \
    const char *mtest_e_ = (expected);                                     \
    const char *mtest_a_ = (actual);                                       \
    if (mtest_e_ == NULL || mtest_a_ == NULL) {                            \
        if (mtest_e_ != mtest_a_) {                                        \
            MTEST_FAIL_("string: expected \"%s\", got \"%s\"",             \
                        mtest_e_ ? mtest_e_ : "(null)",                    \
                        mtest_a_ ? mtest_a_ : "(null)");                   \
        }                                                                  \
    } else if (strcmp(mtest_e_, mtest_a_) != 0) {                          \
        MTEST_FAIL_("expected \"%s\", got \"%s\"", mtest_e_, mtest_a_);   \
    }                                                                      \
} while (0)

/** Assert string contains substring. */
#define MTEST_ASSERT_STR_CONTAINS(haystack, needle) do {                  \
    mtest_g.asserts_total++;                                               \
    if (strstr((haystack), (needle)) == NULL) {                            \
        MTEST_FAIL_("\"%s\" not found in output", (needle));              \
    }                                                                      \
} while (0)

/** Assert two memory regions are equal. */
#define MTEST_ASSERT_MEM_EQ(expected, actual, len) do {                   \
    mtest_g.asserts_total++;                                               \
    if (memcmp((expected), (actual), (len)) != 0) {                        \
        MTEST_FAIL_("memory mismatch (%d bytes)", (int)(len));            \
    }                                                                      \
} while (0)

/** Assert two floats are approximately equal. */
#define MTEST_ASSERT_FLOAT_EQ(expected, actual) do {                      \
    mtest_g.asserts_total++;                                               \
    float mtest_e_ = (float)(expected);                                    \
    float mtest_a_ = (float)(actual);                                      \
    if (fabsf(mtest_e_ - mtest_a_) > 0.001f) {                            \
        MTEST_FAIL_("expected %f, got %f", (double)mtest_e_,              \
                     (double)mtest_a_);                                    \
    }                                                                      \
} while (0)

/** Assert two doubles are approximately equal with custom epsilon. */
#define MTEST_ASSERT_NEAR(expected, actual, epsilon) do {                  \
    mtest_g.asserts_total++;                                               \
    double mtest_e_ = (double)(expected);                                  \
    double mtest_a_ = (double)(actual);                                    \
    double mtest_eps_ = (double)(epsilon);                                 \
    if (fabs(mtest_e_ - mtest_a_) > mtest_eps_) {                         \
        MTEST_FAIL_("expected %f ± %f, got %f",                           \
                     mtest_e_, mtest_eps_, mtest_a_);                      \
    }                                                                      \
} while (0)

/** Unconditional fail. */
#define MTEST_FAIL(msg) MTEST_FAIL_("%s", (msg))

/** Skip current test. */
#define MTEST_SKIP(reason) do {                                           \
    printf(MTEST_CLR_YELLOW "SKIP" MTEST_CLR_RESET " (%s)\n", (reason)); \
    mtest_g.tests_skipped++;                                               \
    mtest_g.tests_run--;                                                   \
    return;                                                                \
} while (0)

/* ── Begin / End ───────────────────────────────────────────────────────── */

/**
 * Parse CLI args and print header.
 * Supports: --filter=NAME, -v (verbose), -x (stop on fail), -l (list).
 */
#define MTEST_BEGIN(argc, argv) do {                                      \
    memset(&mtest_g, 0, sizeof(mtest_g));                                  \
    for (int i_ = 1; i_ < (argc); i_++) {                                 \
        if (strncmp((argv)[i_], "--filter=", 9) == 0) {                    \
            mtest_g.filter = (argv)[i_] + 9;                               \
        } else if (strcmp((argv)[i_], "-v") == 0) {                        \
            mtest_g.verbose = 1;                                           \
        } else if (strcmp((argv)[i_], "-x") == 0) {                        \
            mtest_g.stop_on_fail = 1;                                      \
        } else if (strcmp((argv)[i_], "-l") == 0) {                        \
            mtest_g.list_only = 1;                                         \
        } else if (strcmp((argv)[i_], "-h") == 0 ||                        \
                   strcmp((argv)[i_], "--help") == 0) {                     \
            printf("Usage: %s [options]\n"                                 \
                   "  --filter=NAME  Run only tests matching NAME\n"       \
                   "  -v             Verbose output\n"                      \
                   "  -x             Stop on first failure\n"              \
                   "  -l             List tests without running\n",        \
                   (argv)[0]);                                              \
            exit(0);                                                        \
        }                                                                  \
    }                                                                      \
    printf("\n" MTEST_CLR_BOLD "=== %s ===" MTEST_CLR_RESET "\n",          \
           (argv)[0]);                                                     \
    if (mtest_g.filter) {                                                  \
        printf(MTEST_CLR_DIM "  filter: %s" MTEST_CLR_RESET "\n",         \
               mtest_g.filter);                                            \
    }                                                                      \
} while (0)

/** Print summary and return exit code. */
#define MTEST_END()  mtest_end_()

/* ── Implementation (define in exactly one .c file) ────────────────────── */

#ifdef MTEST_IMPLEMENTATION

mtest_state_t mtest_g;

static int mtest_end_(void) {
    printf("\n" MTEST_CLR_BOLD "=== Results: ");

    if (mtest_g.tests_failed == 0) {
        printf(MTEST_CLR_GREEN "%d/%d passed",
               mtest_g.tests_passed, mtest_g.tests_run);
    } else {
        printf(MTEST_CLR_RED "%d/%d passed, %d FAILED",
               mtest_g.tests_passed, mtest_g.tests_run,
               mtest_g.tests_failed);
    }

    if (mtest_g.tests_skipped > 0) {
        printf(MTEST_CLR_YELLOW ", %d skipped", mtest_g.tests_skipped);
    }

    printf(MTEST_CLR_RESET MTEST_CLR_BOLD " ===" MTEST_CLR_RESET);
    printf(MTEST_CLR_DIM " (%d asserts)" MTEST_CLR_RESET,
           mtest_g.asserts_total);
    printf("\n\n");

    return mtest_g.tests_failed > 0 ? 1 : 0;
}

#endif /* MTEST_IMPLEMENTATION */

#endif /* MTEST_H */
