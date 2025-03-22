#include <stdlib.h>
#include "sreg.h"

#define bit(a, x) ((bool) ((a) & (1u << (x))))

flags_t flags_neg (uint8_t d)
{
  uint8_t res = -d;
  bool R7 = bit (res, 7);
  bool R3 = bit (res, 3);

  bool Rd3 = bit (d, 3);

  sreg_t s = { 0 };

  s.v = res == 0x80;
  s.n = R7;
  s.z = res == 0;
  s.c = res != 0;
  s.h = R3 | Rd3;
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, H | S | V | N | Z | C, res };
}

flags_t flags_com (uint8_t d)
{
  uint8_t res = ~d;
  bool R7 = bit (res, 7);

  sreg_t s = { 0 };

  s.v = 0;
  s.n = R7;
  s.z = res == 0;
  s.c = 1;
  s.s = s.n ^ s.v;
  
  return (flags_t) { s, S | V | N | Z | C, res };
}

flags_t flags_inc (uint8_t d)
{
  uint8_t res = 0xff & (d + 1);

  sreg_t s = { 0 };

  s.z = res == 0;
  s.n = bit (res, 7);
  s.v = d == 0x7f;
  s.s = s.n ^ s.v;

  return (flags_t) { s, S | V | N | Z, res };
}

flags_t flags_dec (uint8_t d)
{
  uint8_t res = 0xff & (d - 1);

  sreg_t s = { 0 };

  s.z = res == 0;
  s.n = bit (res, 7);
  s.v = d == 0x80;
  s.s = s.n ^ s.v;

  return (flags_t) { s, S | V | N | Z, res };
}

flags_t flags_asr (uint8_t d)
{
  uint8_t res = (d >> 1) | (d & 0x80);

  sreg_t s = { 0 };

  s.c = bit (d, 0);
  s.z = res == 0;
  s.n = bit (res, 7);
  s.v = s.n ^ s.c;
  s.s = s.n ^ s.v;

  return (flags_t) { s, S | V | N | Z | C, res };
}

flags_t flags_ror (uint8_t d, sreg_t sreg)
{
  uint8_t res = (d + 0x100 * sreg.c) >> 1;

  sreg_t s = { 0 };

  s.c = bit (d, 0);
  s.z = res == 0;
  s.n = bit (res, 7);
  s.v = s.n ^ s.c;
  s.s = s.n ^ s.v;

  return (flags_t) { s, S | V | N | Z | C, res };
}

static inline
sreg_t sreg_inc (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "inc %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_dec (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "dec %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_neg (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "neg %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_com (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "com %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_asr (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "asr %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

static inline
sreg_t sreg_ror (uint8_t d, uint8_t sreg, uint8_t result)
{
  __asm ("out __SREG__,%[sreg]"  "\n\t"
         "ror %[d]"              "\n\t"
         "in %[sreg],__SREG__"
         : [sreg] "+r" (sreg), [d] "+r" (d));
  if (d != result)
    exit (__LINE__);
  return (sreg_t) sreg;
}

void test_inc (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_inc (d, sreg.val, d + 1);
  flags_t f = flags_inc (d);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_dec (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_dec (d, sreg.val, d - 1);
  flags_t f = flags_dec (d);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_neg (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_neg (d, sreg.val, -d);
  flags_t f = flags_neg (d);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_com (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_com (d, sreg.val, ~d);
  flags_t f = flags_com (d);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_ror (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_ror (d, sreg.val, (d + 0x100 * sreg.c) >> 1);
  flags_t f = flags_ror (d, sreg);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_asr (uint8_t d, sreg_t sreg)
{
  sreg_t s0 = sreg_asr (d, sreg.val, (d >> 1) | (d & 0x80));
  flags_t f = flags_asr (d);
  if ((f.sreg.val & f.mask) != (s0.val & f.mask))
    exit (__LINE__);
}

void test_sreg (void)
{
  uint8_t d = 0;

  do
    {
      test_inc (d, (sreg_t) { 0x00 });
      test_inc (d, (sreg_t) { 0xff });

      test_dec (d, (sreg_t) { 0x00 });
      test_dec (d, (sreg_t) { 0xff });

      test_neg (d, (sreg_t) { 0x00 });
      test_neg (d, (sreg_t) { 0xff });

      test_com (d, (sreg_t) { 0x00 });
      test_com (d, (sreg_t) { 0xff });

      test_ror (d, (sreg_t) { 0 });
      test_ror (d, (sreg_t) { C });

      test_asr (d, (sreg_t) { 0x00 });
      test_asr (d, (sreg_t) { 0xff });
} while (++d);
}

int main (void)
{
  test_sreg();
  return 0;
}
