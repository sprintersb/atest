#define AVRTEST_DEFF(F, V)  GENF1 (F, S, V)
#define GENF1(F, S, V)  GENF2 (F, S, V)
#define GENF2(F, S, V)  GENF (F##S, V)

#define GENF(F, V)                                 \
  T fun_##F (T x, V y)                             \
  {                                                \
    return avrtest_##F (x, y);                     \
  }

AVRTEST_DEFF(ldexp, int)
AVRTEST_DEFF(frexp, int*)
AVRTEST_DEFF(modf, T*)
