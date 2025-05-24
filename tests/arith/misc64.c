/* Make sure that 64-bit arith from syscall 21 "misc" corner cases
   like 0 / 0 yield the same results like libgcc.  */

#ifdef __AVR_TINY__
int main (void)
{
    return 0;
}
#else

#include <stdlib.h>
#include <stdint.h>

#include "avrtest.h"

#ifdef __FLASH
__flash
#endif
const uint64_t vals[] =
{
    0, 1, 2, -2ull, -1ull, INT64_MAX, INT64_MIN, INT64_MIN+1, 0xffff, 0x10000,
    0x100000000ull,  0xffffffffull, 0xff00ff00ff00ff00, 0x00ff00ff00ff00ff,
    0x8000000080000000, 0x80000000ffffffff,
    1234, 0xcafebabecafebabe, 0xbabefacebabeface, 0x123456789abcdef
};

#define ARRAY_SIZE(X) (sizeof(X) / sizeof (*X))

int main (void)
{
    for (size_t i = 0; i < ARRAY_SIZE (vals); ++i)
    {
        uint64_t a = vals[i];
        __asm ("" : "+r" (a));
        int64_t sa = a;

        for (size_t j = 0; j < ARRAY_SIZE (vals); ++j)
        {
            uint64_t b = vals[j];
            __asm ("" : "+r" (b));
            int64_t sb = b;

            if (a / b != avrtest_divu64 (a, b)) exit (__LINE__);
            if (a % b != avrtest_modu64 (a, b)) exit (__LINE__);
            if (a * b != avrtest_mulu64 (a, b)) exit (__LINE__);

            if (sa / sb != avrtest_divs64 (sa, sb)) exit (__LINE__);
            if (sa % sb != avrtest_mods64 (sa, sb)) exit (__LINE__);
            if (sa * sb != avrtest_muls64 (sa, sb)) exit (__LINE__);
        }
    }

    return 0;
}
#endif /* AVR TINY */
