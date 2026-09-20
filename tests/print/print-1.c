#ifdef __AVR_TINY__
int main (void)
{
  return 0;
}
#else

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include "avrtest.h"

char buf0[40];
char buf1[40];

#define sprint(n, f, ...)                                           \
  do {                                                              \
    int n0 = sprintf_P (buf0, PSTR (f), ##__VA_ARGS__);             \
    int n1 = avrtest_sprintf_P (buf1, PSTR (f), ##__VA_ARGS__);     \
    if (n0 != n1)                                                   \
      exit (2);                                                     \
    if (strcmp (buf0, buf1))                                        \
      exit (3);                                                     \
    int n2 = snprintf_P (buf0, n, PSTR (f), ##__VA_ARGS__);         \
    int n3 = avrtest_snprintf_P (buf1, n, PSTR (f), ##__VA_ARGS__); \
    if (n2 != n3)                                                   \
      exit (4);                                                     \
    if (n && strcmp (buf0, buf1))                                   \
      exit (5);                                                     \
  } while (0)

int main (void)
{
  sprint (4, "%d %+ld %8u", 123, -456l, 128);
  sprint (6, "%+2.10x  % 8ld %.8u", 123, 456l, 128);
  sprint (5, "%s%% %+.8s %-.8S", "abcde", "fghij", PSTR ("lmnop"));
  sprint (0, "%s%% %+4s %-3S %c", "abcde", "fghij", PSTR ("lmnop"), '!');

  return 0;
}
#endif /* AVR_TINY */
