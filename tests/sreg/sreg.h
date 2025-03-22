#ifndef SREG_H
#define SREG_H

#include <stdint.h>
#include <stdbool.h>

#define BITNO_I  7
#define BITNO_T  6
#define BITNO_H  5
#define BITNO_S  4
#define BITNO_V  3
#define BITNO_N  2
#define BITNO_Z  1
#define BITNO_C  0

#define I (1u << BITNO_I)
#define T (1u << BITNO_T)
#define H (1u << BITNO_H)
#define S (1u << BITNO_S)
#define V (1u << BITNO_V)
#define N (1u << BITNO_N)
#define Z (1u << BITNO_Z)
#define C (1u << BITNO_C)

typedef union
{
  uint8_t val;
  struct
  {
    bool c:1;
    bool z:1;
    bool n:1;
    bool v:1;
    bool s:1;
    bool h:1;
    bool t:1;
    bool i:1;
  };
} sreg_t;


typedef struct
{
  sreg_t sreg;
  uint8_t mask;
  uint16_t result;
} flags_t;

#endif /* SREG_H */
