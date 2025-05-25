#define AVRTEST_DEFF(F)  GENF1 (F, S)
#define GENF1(F, S)  GENF2 (F, S)
#define GENF2(F, S)  GENF (F##S)

#define GENF(F)                                    \
  float fun_##F (float x)                          \
  {                                                \
    return avrtest_##F (x);                        \
  }

AVRTEST_DEFF(sin) AVRTEST_DEFF(asin) AVRTEST_DEFF(sinh) AVRTEST_DEFF(asinh)
AVRTEST_DEFF(cos) AVRTEST_DEFF(acos) AVRTEST_DEFF(cosh) AVRTEST_DEFF(acosh)
AVRTEST_DEFF(tan) AVRTEST_DEFF(atan) AVRTEST_DEFF(tanh) AVRTEST_DEFF(atanh)
AVRTEST_DEFF(exp) AVRTEST_DEFF(log)  AVRTEST_DEFF(sqrt) AVRTEST_DEFF(cbrt)
AVRTEST_DEFF(trunc) AVRTEST_DEFF(ceil)  AVRTEST_DEFF(floor) AVRTEST_DEFF(round)
AVRTEST_DEFF(log2) AVRTEST_DEFF(log10) AVRTEST_DEFF(fabs)
