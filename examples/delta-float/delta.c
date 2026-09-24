#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <avr/pgmspace.h>

#include "avrtest.h"

#define NI __attribute((noipa))

enum { OUT_FLOAT, OUT_ULP };

// Command Line Args
int N = 0;   // In [0, Num).
int Num = 0; // Size of the cohort.
float Lo = 0.5, Hi = 1.5;
uint32_t Step = 1; // Stride in ULPs
int OutFormat = OUT_FLOAT;

#ifndef FUNC
#define FUNC logf
#endif

#ifndef AFUNC
#define AFUNC avrtest_logl
#endif

#define stringy_(X) #X
#define stringy(X) stringy_(X)

/////////////////////////////////////////////////////////////////////////////
// For convenience, introduce PSTR and avrtest_* for printing and diagnosing.
// Notice that avrtest_*printf is more capable than printf since it can
// print IEEE double.
#define error(msg, ...) error_P (PSTR (msg), ##__VA_ARGS__)
#define info(msg, ...)  info_P  (PSTR (msg), ##__VA_ARGS__)
#define printf(msg, ...)      avrtest_printf_P  (PSTR (msg), ##__VA_ARGS__)
#define printf_P(msg, ...)    avrtest_printf_P  (msg, ##__VA_ARGS__)
#define fprintf(f, msg, ...)  avrtest_fprintf_P (f, PSTR (msg), ##__VA_ARGS__)
#define vfprintf_P(a, b, c)   avrtest_vfprintf_P (a, b, c);

//////////////////////////////////////////////////////////////////////
// Diagnostics and Information

// noipa due to PR127482.
NI void error_P (const char *msg, ...)
{
    va_list args;
    va_start (args, msg);
    fprintf (stderr, "\nerror: ");
    vfprintf_P (stderr, msg, args);
    fprintf (stderr, "\n");
    va_end (args);

    exit (1);
}

NI void info_P (const char *msg, ...)
{
    va_list args;
    va_start (args, msg);
    vfprintf_P (stderr, msg, args);
    va_end (args);
}

//////////////////////////////////////////////////////////////////////
// Parsing Command Line Arguments

static inline float utof (uint32_t u)
{
    float f;
    __builtin_memcpy (&f, &u, 4);
    return f;
}

static inline uint32_t ftou (float f)
{
    uint32_t u;
    __builtin_memcpy (&u, &f, 4);
    return u;
}

// Keeps order.
static inline int32_t ftoi (float f)
{
    uint32_t u = ftou (f);
    int32_t i;
    if (u & 0x80000000)
        u ^= 0x7fffffff;
    __builtin_memcpy (&i, &u, 4);
    return i;
}

// Keeps order.
static inline float itof (int32_t i)
{
    uint32_t u = (uint32_t) i;
    return utof ((u & 0x80000000) ? u ^ 0x7fffffff : u);
}

static float f_incr (float f, uint32_t inc)
{
    uint32_t base = (uint32_t) ftoi (f);
    uint32_t next = base + inc;
    int32_t ibase, inext, iinc;
    // Saturate.
    __builtin_memcpy (&ibase, &base, 4);
    __builtin_memcpy (&inext, &next, 4);
    __builtin_memcpy (&iinc, &inc, 4);
    if (iinc > 0 && inext < ibase)
        return utof (0x7fffffff);
    if (iinc < 0 && inext > ibase)
        return utof (0x80000000);
    return itof (next);
}


static inline bool is_prefix (const char *pre, const char *s)
{
    return 0 == strncmp (pre, s, strlen (pre));
}

static inline bool is_last (void)
{
    return N == Num - 1;
}

static bool get_int (const char *arg, const char *prefix, int *pi)
{
    if (! is_prefix (prefix, arg))
        return false;
    *pi = atoi (arg + strlen (prefix));
    return true;
}

static bool get_u32 (const char *arg, const char *prefix, uint32_t *pi)
{
    if (! is_prefix (prefix, arg))
        return false;
    char *end;
    *pi = strtoul (arg + strlen (prefix), &end, 0);
    return *end == '\0';
}

/* Recognize sum of 2 terms: 1st is float, 2nd is ulong (float as bits).
   For example, the smallest float > 0 can be written as "0+1" or "0+0x1". */

static bool get_float (const char *arg, const char *prefix, float *pf)
{
    char *pend, *pend2;
    if (! is_prefix (prefix, arg))
        return false;
    *pf = avrtest_strtof (arg + strlen (prefix), &pend);

    if (*pend)
    {
        while (isspace (*pend))
            ++pend;

        if (*pend != '+' && *pend != '-')
            error ("unrecognized float in %s\n", arg);

        uint32_t add = strtoul (1 + pend, &pend2, 0);
        if (*pend2)
            error ("unrecognized float in %s\n", arg);

        *pf = f_incr (*pf, *pend == '+' ? add : -add);
    }

    return true;
}

static bool get_out_format (const char *arg, const char *prefix, int *ofmt)
{
    if (! is_prefix (prefix, arg))
        return false;

    if (! strcasecmp (arg + strlen (prefix), "float"))
    {
        *ofmt = OUT_FLOAT;
        return true;
    }
    if (! strcasecmp (arg + strlen (prefix), "ulp"))
    {
        *ofmt = OUT_ULP;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// The very Routine to determine the relative Error at x.

static float get_delta (float x)
{
    float y = FUNC (x);
    long double y0 = AFUNC (avrtest_ftol (x));
    long double yl = avrtest_ftol (y);
    float ulp = avrtest_ulpf (y, avrtest_ltof (y0));
    if (avrtest_cmpf (ulp, 0) == 0)
        return 0;

    if (OutFormat == OUT_ULP)
        return ulp;

    long double d = avrtest_subl (yl, y0);
    return avrtest_ltof (avrtest_divl (d, y0));
}

//////////////////////////////////////////////////////////////////////
// Shows the expected Run Time.

void show_expected_runtime (int n_loops)
{
    uint32_t cyc = avrtest_cycles ();

    uint32_t n_xs = (uint32_t) ftoi (Hi) - (uint32_t) ftoi (Lo) + 1;
    n_xs = n_xs / Step + 1;
    info ("%lu values = ", n_xs);
    n_xs = 1 + n_xs / Num;
    if (n_xs > 500000)
        info ("%.2fM", n_xs / 1e6f);
    else if (n_xs > 500)
        info ("%.2fk", n_xs / 1e3f);
    else
        info ("%lu", n_xs);
    info ("/run = %.2f min expected execution time\n",
          // Assume 90MHz AVRtest performance.
          n_xs / (90e6f * 60.0f * n_loops) * cyc);
}


//////////////////////////////////////////////////////////////////////
// Return the maximal relative Error over the Interval as specified by
// command-line Arguments.  Set *PX to the associated x value.

float get_minmax (float *px)
{
    uint32_t inc = Step * Num;
    float d_mi = 0;
    float d_ma = 0;
    float mami = 0;
    uint32_t cnt = 0;

    avrtest_reset_cycles ();

    for (float x = f_incr (Lo, Step * N);
         avrtest_cmpf (x, Hi) <= 0;
         x = f_incr (x, inc))
    {
        if (cnt++ == 100 && is_last())
            show_expected_runtime (100);

        float d = get_delta (x);
        d_ma = avrtest_fmaxf (d, d_ma);
        d_mi = avrtest_fminf (d, d_mi);

        d = avrtest_fmaxf (d_ma, -d_mi);
        if (avrtest_cmpf (d, mami) > 0 || cnt == 1)
        {
            mami = d;
            *px = x;
        }
        if (ftou (x) == 0x7fffffff)
            break;
    }

    return mami;
}

/////////////////////////////////////////////////////////////////////
// Parameters to main are passed qua `avrtest ... -args <args>` and
// are injected into the program by a syscall called in .init8 by
// dejagnuboards/exit.c.

int main (int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
        if (get_int (argv[i], "-n=", &N))
            break;

    if (N == 0)
        info ("FUNC = %s\n", stringy(FUNC));

    for (int i = 1; i < argc; ++i)
    {
        if (N == 0)
            info ("argv[%d] = '%s'\n", i, argv[i]);

        if (! get_int (argv[i], "-n=", &N)
            && ! get_int (argv[i], "-num=", &Num)
            && ! get_float (argv[i], "-lo=", &Lo)
            && ! get_float (argv[i], "-hi=", &Hi)
            && ! get_u32 (argv[i], "-step=", &Step)
            && ! get_out_format (argv[i], "-out=", &OutFormat))
        {
            error ("unknown option %s\n", argv[i]);
        }
    }

    if (Num < 1 || Num > 1000)
        error ("-num=%d must be in [1, 1000]", Num);
    if (N < 0 || N >= Num)
        error ("-n=%d not in [0, num=%d)", N, Num);
    if (Lo > Hi)
        error ("lo=%e > hi=%e\n", Lo, Hi);
    const uint32_t max_step = avrtest_divu32 (UINT32_MAX, Num);
    if ((int32_t) Step <= 0)
        error ("-step=%ld must be > 0", Step);
    if (Step >= max_step)
        error ("-step=%lu must be < %lu", max_step);

    if (N == 0)
        info ("NUM=%d: [%e, %e] += 0x%lx\n", Num, Lo, Hi, Step);

    float x = __builtin_nanf("");
    float mami = get_minmax (&x);
    const char *fmt = OutFormat == OUT_ULP
        ? PSTR ("== %d/%d: 0x%08lx: %e -> %.1f\n")
        : PSTR ("== %d/%d: 0x%08lx: %e -> %e\n");
    printf_P (fmt, N, Num, ftou(x), x, mami);

    return 0;
}
