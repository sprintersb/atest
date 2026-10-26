#include <stdlib.h>
#include <stdint.h>

#include "avrtest.h"

#ifdef __AVR_TINY__
int main (void)
{
    return 0;
}
#else

#define ARRAY_SIZE(X) (sizeof(X) / sizeof (*X))

typedef long double T;

typedef struct
{
    int32_t i;
    T f;
} pair32_t;

typedef struct
{
    int64_t i;
    T f;
} pair64_t;

__flash const pair32_t vals32[] =
{
    { 0, 0.0 },
    { -1, -1.0 },
    { -1234, -1234.0 },
    { 2345, 2345.0 },
    { -(1L << 30), -0x1.0p30 },
    { +(1L << 30), +0x1.0p30 },
};

__flash const pair64_t vals64[] =
{
    { 0LL, 0.0 },
    { -1LL, -1.0 },
    { -1234LL, -1234.0 },
    { 2345LL, 2345.0 },
    { -(1LL << 62), -0x1.0p62 },
    { +(1LL << 62), +0x1.0p62 },
};

void test32 (void)
{
    for (size_t n = 0; n < ARRAY_SIZE (vals32); ++n)
    {
        int32_t i = vals32[n].i;
        T f = vals32[n].f;

        if (avrtest_s32tol (i) != f)
            exit (10 + n);

        if (i >= 0)
        {
            if (avrtest_u32tol ((uint32_t) i) != f)
                exit (20 + n);
        }
    }
}

void test64 (void)
{
    for (size_t n = 0; n < ARRAY_SIZE (vals64); ++n)
    {
        int64_t i = vals64[n].i;
        T f = vals64[n].f;

        if (avrtest_s64tol (i) != f)
            exit (30 + n);

        if (i >= 0)
        {
            if (avrtest_u64tol ((uint64_t) i) != f)
                exit (40 + n);
        }
    }
}


int main (void)
{
    test32 ();
    test64 ();

    return 0;
}
#endif
