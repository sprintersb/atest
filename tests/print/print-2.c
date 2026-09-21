#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include "avrtest.h"

char buf0[30];

#define sprint(res, f, ...)                                         \
  do {                                                              \
    avrtest_sprintf (buf0, f, ##__VA_ARGS__);                       \
    if (strcmp_P (buf0, PSTR (res)))                                \
      exit (2);                                                     \
    avrtest_sprintf_P (buf0, PSTR (f), ##__VA_ARGS__);              \
    if (strcmp_P (buf0, PSTR (res)))                                \
      exit (3);                                                     \
  } while (0)

int main (void)
{
  sprint ("\ttext%TEXT", "\t%s%%%S", "text", PSTR ("TEXT"));

  return 0;
}
