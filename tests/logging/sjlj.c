// at:flags { -Og -std=gnu99 }
// at:filter { grep -- '+++' }

#include <setjmp.h>
#include "avrtest.h"

#define NI __attribute((noinline, noclone))

#define HERE(x) __asm (x)

jmp_buf buf;

NI void fun2 (char c)
{
    HERE ("seh");
    if (c)
        longjmp (buf, 1);
}

NI void fun1 (char c)
{
    HERE ("clh");
    fun2 (c);
}

int main (void)
{
    LOG_ON;
    if (setjmp (buf))
    {
        HERE ("clz");
        goto done;
    }

    fun1 (0);
    fun1 (1);
done:
    HERE ("sez");
    LOG_OFF;
    return 0;
}
