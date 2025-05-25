#define AVRTEST_DEFF(F)  GENF1 (F, S)
#define GENF1(F, S)  GENF2 (F, S)
#define GENF2(F, S)  GENF (F##S)

#define GENF(F)                                    \
  T fun_##F (T x, float y)                         \
  {                                                \
    return avrtest_##F (x, y);                     \
  }

AVRTEST_DEFF(pow)  AVRTEST_DEFF(atan2) AVRTEST_DEFF(hypot) AVRTEST_DEFF(fdim)
AVRTEST_DEFF(fmin) AVRTEST_DEFF(fmax)  AVRTEST_DEFF(fmod)
AVRTEST_DEFF(mul) AVRTEST_DEFF(div) AVRTEST_DEFF(add) AVRTEST_DEFF(sub)
AVRTEST_DEFF(ulp) AVRTEST_DEFF(prand)
