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
#include "config.h"

#ifndef FLOAT_SIZE
#error FLOAT_SIZE not defined in config.h
#endif

#define NI __attribute((noipa))

enum { OUT_FLOAT, OUT_ULP };

// Command Line Args
uint32_t NX = 0; // Number of pixels.
float Lo = 0.5, Hi = 1.5;
float Step = 1.0;
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
    long double y0 = AFUNC (avrtest_ftol (x));
#if FLOAT_SIZE == 4
    float y = FUNC (x);
    long double yl = avrtest_ftol (y);
    float ulp = avrtest_ulpf (y, avrtest_ltof (y0));
#else
    long double y = FUNC (avrtest_ftol (x));
    long double yl = y;
    float ulp = avrtest_ltof (avrtest_ulpl (y, y0));
#endif

    if (avrtest_cmpf (ulp, 0) == 0)
        return 0;

    if (OutFormat == OUT_ULP)
        return ulp;

    long double d = avrtest_subl (yl, y0);
    long double r = avrtest_divl (d, y0);

    return avrtest_ltof (r);
}

//////////////////////////////////////////////////////////////////////
// Write the Data File for Gnuplot.

void print_deltas (void)
{
    printf ("== \"x\" \"%s{/symbol D} " stringy (FUNC) "\"\n",
            OutFormat == OUT_ULP ? "ulp " : "");

    for (uint32_t i = 0; i < NX; ++i)
    {
        float off = avrtest_mulf (Step, avrtest_utof (i));
        float x = avrtest_addf (Lo, off);
        x = avrtest_fmaxf (x, Lo);
        x = avrtest_fminf (x, Hi);
        float d = get_delta (x);

        printf ("== %e %e\n", x, d);
    }
}

/////////////////////////////////////////////////////////////////////
// Parameters to main are passed qua `avrtest ... -args <args>` and
// are injected into the program by a syscall called in .init8 by
// dejagnuboards/exit.c.

int main (int argc, char *argv[])
{
    info ("FUNC = %s\n", stringy(FUNC));

    for (int i = 1; i < argc; ++i)
    {
        info ("argv[%d] = '%s'\n", i, argv[i]);

        if (! get_u32 (argv[i], "-nx=", &NX)
            && ! get_float (argv[i], "-lo=", &Lo)
            && ! get_float (argv[i], "-hi=", &Hi)
            && ! get_out_format (argv[i], "-out=", &OutFormat))
        {
            error ("unknown option %s\n", argv[i]);
        }
    }

    if (NX < 2 || NX > 1000000)
        error ("-n=%lu not in [2, num=%lu)", NX, 1000000);
    if (Lo > Hi)
        error ("lo=%e > hi=%e\n", Lo, Hi);
    Step = avrtest_divf (Hi - Lo, avrtest_utof (NX - 1));

    info ("N=%lu: [%e, %e] += %e\n", NX, Lo, Hi, Step);

    print_deltas ();

    return 0;
}
