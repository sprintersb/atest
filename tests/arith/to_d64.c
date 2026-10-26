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

typedef uint64_t T;

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
    { 0, 0 },
    { -1,    0xbff0000000000000 },
    { -1234, 0xc093480000000000 },
    { 2345,  0x40a2520000000000 },
    { -(1L << 30), 0xc1d0000000000000 },
    { +(1L << 30), 0x41d0000000000000 },
};

__flash const pair64_t vals64[] =
{
    { 0LL, 0 },
    { -1LL,    0xbff0000000000000 },
    { -1234LL, 0xc093480000000000 },
    { 2345LL,  0x40a2520000000000 },
    { -(1LL << 62), 0xc3d0000000000000 },
    { +(1LL << 62), 0x43d0000000000000 },
};

void test32 (void)
{
    for (size_t n = 0; n < ARRAY_SIZE (vals32); ++n)
    {
        int32_t i = vals32[n].i;
        T f = vals32[n].f;

        if (avrtest_s32to_d64 (i) != f)
            exit (10 + n);

        if (i >= 0)
        {
            if (avrtest_u32to_d64 ((uint32_t) i) != f)
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

        if (avrtest_s64to_d64 (i) != f)
            exit (30 + n);

        if (i >= 0)
        {
            if (avrtest_u64to_d64 ((uint64_t) i) != f)
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
