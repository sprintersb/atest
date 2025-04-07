/* Make sure that 32-bit arith from syscall 21 "misc" corner cases
   like 0 / 0 yield the same results like libgcc.  */

#include <stdlib.h>
#include <stdint.h>

#include "avrtest.h"

#ifdef __FLASH
__flash
#endif
const uint32_t vals[] =
{
    0, 1, 2, -2ul, -1ul, 0x7fffffff, 0x80000000, 0x80000001, 0xff, 0xffff,
    0x100, 0x10000, 0xff00ff00, 0x00ff00ff, 0x80008000, 0x8000ffff,
    1234, 0xcafebabe, 0xbabeface, 0x12345678
};

#define ARRAY_SIZE(X) (sizeof(X) / sizeof (*X))

int main (void)
{
    for (size_t i = 0; i < ARRAY_SIZE (vals); ++i)
    {
        uint32_t a = vals[i];
        __asm ("" : "+r" (a));
        int32_t sa = a;

        for (size_t j = 0; j < ARRAY_SIZE (vals); ++j)
        {
            uint32_t b = vals[j];
            __asm ("" : "+r" (b));
            int32_t sb = b;

            if (a / b != avrtest_divu32 (a, b)) exit (__LINE__);
            if (a % b != avrtest_modu32 (a, b)) exit (__LINE__);
            if (a * b != avrtest_mulu32 (a, b)) exit (__LINE__);

            if (sa / sb != avrtest_divs32 (sa, sb)) exit (__LINE__);
            if (sa % sb != avrtest_mods32 (sa, sb)) exit (__LINE__);
            if (sa * sb != avrtest_muls32 (sa, sb)) exit (__LINE__);
        }
    }

    return 0;
}
