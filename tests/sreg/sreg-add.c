#include <stdlib.h>
#include "sreg.h"

#define bit(a, x) ((bool) ((a) & (1u << (x))))

flags_t flags_add (uint8_t d, uint8_t r)
{
  uint8_t res = d + r;
  bool R7 = bit (res, 7);

  bool Rd7 = bit (d, 7);
  bool Rd3 = bit (d, 3);

  bool R3 = bit (res, 3);
  bool Rr7 = bit (r, 7);
  bool Rr3 = bit (r, 3);

  sreg_t s = { 0 };

  s.v = (Rd7 & Rr7 & !R7) | (!Rd7 & !Rr7 & R7);
  s.n = R7;
  s.z = res == 0;
  s.c = (Rd7 & Rr7) | (Rr7 & !R7) | (!R7 & Rd7);
  s.h = (Rd3 & Rr3) | (Rr3 & !R3) | (!R3 & Rd3);
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, H | S | V | N | Z | C, res };
}

flags_t flags_adc (uint8_t d, uint8_t r, sreg_t sreg)
{
  uint8_t res = d + r + sreg.c;
  bool R7 = bit (res, 7);

  bool Rd7 = bit (d, 7);
  bool Rd3 = bit (d, 3);

  bool R3 = bit (res, 3);
  bool Rr7 = bit (r, 7);
  bool Rr3 = bit (r, 3);

  sreg_t s = { 0 };

  s.v = (Rd7 & Rr7 & !R7) | (!Rd7 & !Rr7 & R7);
  s.n = R7;
  s.z = res == 0;
  s.c = (Rd7 & Rr7) | (Rr7 & !R7) | (!R7 & Rd7);
  s.h = (Rd3 & Rr3) | (Rr3 & !R3) | (!R3 & Rd3);
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, H | S | V | N | Z | C, res };
}

static inline
sreg_t sreg_add (uint8_t d, uint8_t r, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "add %[d],%[r]"         "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d)
         : [r] "r" (r));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_adc (uint8_t d, uint8_t r, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "adc %[d],%[r]"         "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d)
         : [r] "r" (r));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}


void test_add (uint8_t d, uint8_t r, sreg_t sreg)
{
  sreg_t s0 = sreg_add (d, r, sreg.val, d + r);
  flags_t f = flags_add (d, r);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_adc (uint8_t d, uint8_t r, sreg_t sreg)
{
  sreg_t s0 = sreg_adc (d, r, sreg.val, d + r + sreg.c);
  flags_t f = flags_adc (d, r, sreg);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}


void test_sreg (void)
{
  uint8_t d = 0;

  do
    {
      uint8_t r = 0;

      do
        {
          test_add (d, r, (sreg_t) { 0x00 });
          test_add (d, r, (sreg_t) { 0xff });

          test_adc (d, r, (sreg_t) { 0x00 });
          test_adc (d, r, (sreg_t) { 0xff });
        } while (++r);
    } while (++d);
}

int main (void)
{
  test_sreg();
  return 0;
}
