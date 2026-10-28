#define AVRTEST_DEFF(F, V)  GENF1 (F, S, V)
#define GENF1(F, S, V)  GENF2 (F, S, V)
#define GENF2(F, S, V)  GENF (F##S, V)

#define GENF(F, V)                                 \
  T fun_##F (T x, V y)                             \
  {                                                \
    return avrtest_##F (x, y);                     \
  }                                                \
  T (*var_##F)(T, V) = avrtest_##F;

AVRTEST_DEFF(ldexp, int)
AVRTEST_DEFF(frexp, int*)
AVRTEST_DEFF(modf, T*)

#define AVRTEST_DEFFb(F, V)  GENF1b (F, S, V)
#define GENF1b(F, S, V)  GENF2b (F, S, V)
#define GENF2b(F, S, V)  GENFb (F##S, V)

#define GENFb(F, V)                                \
  V fun_##F (T x)                                  \
  {                                                \
    return avrtest_##F (x);                        \
  }                                                \
  V (*var_##F)(T) = avrtest_##F;

AVRTEST_DEFFb (lrint, long)
AVRTEST_DEFFb (lround, long)
AVRTEST_DEFFb (llrint, long long)
AVRTEST_DEFFb (llround, long long)
