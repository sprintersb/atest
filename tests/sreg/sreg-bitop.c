#include <stdlib.h>
#include "sreg.h"

#define bit(a, x) ((bool) ((a) & (1u << (x))))

flags_t flags_bitop (uint8_t res)
{
  bool R7 = bit (res, 7);

  sreg_t s = { 0 };

  s.v = 0;
  s.n = R7;
  s.z = res == 0;
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, S | V | N | Z, res };
}

static inline
sreg_t sreg_and (uint8_t d, uint8_t r, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "and %[d],%[r]"         "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d)
         : [r] "r" (r));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_eor (uint8_t d, uint8_t r, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "eor %[d],%[r]"         "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d)
         : [r] "r" (r));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_ior (uint8_t d, uint8_t r, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "or  %[d],%[r]"         "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d)
         : [r] "r" (r));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}


void test_and (uint8_t d, uint8_t r, sreg_t sreg)
{
  sreg_t s0 = sreg_and (d, r, sreg.val, d & r);
  flags_t f = flags_bitop (d & r);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_eor (uint8_t d, uint8_t r, sreg_t sreg)
{
  sreg_t s0 = sreg_eor (d, r, sreg.val, d ^ r);
  flags_t f = flags_bitop (d ^ r);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_ior (uint8_t d, uint8_t r, sreg_t sreg)
{
  sreg_t s0 = sreg_ior (d, r, sreg.val, d | r);
  flags_t f = flags_bitop (d | r);
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
          test_and (d, r, (sreg_t) { 0x00 });
          test_and (d, r, (sreg_t) { 0xff });

          test_eor (d, r, (sreg_t) { 0x00 });
          test_eor (d, r, (sreg_t) { 0xff });

          test_ior (d, r, (sreg_t) { 0x00 });
          test_ior (d, r, (sreg_t) { 0xff });
        } while (++r);
    } while (++d);
}

int main (void)
{
  test_sreg();
  return 0;
}
