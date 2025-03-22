#include <stdlib.h>
#include "sreg.h"

#ifdef __AVR_TINY__
int main (void)
{
  return 0;
}
#else
   
#define bit(a, x) ((bool) ((a) & (1u << (x))))

#define ADDEND 10

flags_t flags_adiw (uint16_t d, uint16_t r)
{
  uint16_t res = d + r;
  bool R15  = bit (res, 15);
  bool Rdh7 = bit (d, 15);

  sreg_t s = { 0 };

  s.v = !Rdh7 & R15;
  s.n = R15;
  s.z = res == 0;
  s.c = !R15 & Rdh7;
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, S | V | N | Z | C, res };
}

flags_t flags_sbiw (uint16_t d, uint16_t r)
{
  uint16_t res = d - r;
  bool R15  = bit (res, 15);
  bool Rdh7 = bit (d, 15);

  sreg_t s = { 0 };

  s.v = !R15 & Rdh7;
  s.n = R15;
  s.z = res == 0;
  s.c = R15 & !Rdh7;
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, S | V | N | Z | C, res };
}


static inline
sreg_t sreg_adiw (uint16_t d, uint8_t sreg, uint16_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "adiw %[d],%[r]"        "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+w" (d)
         : [r] "n" (ADDEND));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_sbiw (uint16_t d, uint8_t sreg, uint16_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "sbiw %[d],%[r]"        "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+w" (d)
         : [r] "n" (ADDEND));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}


void test_adiw (uint16_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_adiw (d, sreg.val, d + ADDEND);
  flags_t f = flags_adiw (d, ADDEND);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_sbiw (uint16_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_sbiw (d, sreg.val, d - ADDEND);
  flags_t f = flags_sbiw (d, ADDEND);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_sreg (void)
{
  uint16_t d = 0;

  do
    {
      test_adiw (d, (sreg_t) { 0 });
      test_sbiw (d, (sreg_t) { 0 });
    } while (++d);
}

int main (void)
{
  test_sreg();
  return 0;
}

#endif /* ! AVRrc */
