/* at:flags { -Os } */

#include "avrtest.h"

#ifndef __AVR_TINY__

#if __SIZEOF_LONG_DOUBLE__ == 8
#define T long double
#define S l
#else
#define T float
#define S f
#endif

#include "fun-1.h"

#endif

int main (void)
{
  return 0;
}
