/*
  This file is part of AVRtest -- A simple simulator for the
  AVR family of 8-bit microcontrollers designed to test the compiler.

  Copyright (C) 2019-2025 Free Software Foundation, Inc.

  AVRtest is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  AVRtest is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AVRtest; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.  */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "host.h"

#define IN_AVRTEST
#include "avrtest.h"

#define LEN_LOG_STRING      500
#define LEN_LOG_XFMT        500

// LOG_XXX logs to -log=FILE.
#define LOGPRINT(FMT, ...)                                      \
  do {                                                          \
    fprintf (program.log_stream, FMT, __VA_ARGS__);             \
    if (!log_unused && program.log_stream != stdout)            \
      printf (FMT, __VA_ARGS__);                                \
  } while (0)


ATTR_PRINTF(1,2)
static void log_add_ (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  log_va (fmt, args);
  va_end (args);
}


#define log_add(...)            \
  do {                          \
      if (is_avrtest_log)       \
        log_add_ (__VA_ARGS__); \
    } while (0)

#define str_append(str, ...)                    \
  sprintf ((str) + strlen (str), __VA_ARGS__)

const layout_t
layout[LOG_X_sentinel] =
  {
    [LOG_STR_CMD]   = { 2, "%s",       false, false },
    [LOG_PSTR_CMD]  = { 2, "%s",       false, true  },
    [LOG_ADDR_CMD]  = { 2, " 0x%04x ", false, false },

    [LOG_FLOAT_CMD] = { 4, " %.6f ", false, false },
    [LOG_D64_CMD]   = { 8, " %.6f ", false, false },
    [LOG_F7T_CMD]   = { 2, " %s ",   false, false },

    [LOG_U8_CMD]  = { 1, " %u ", false, false },
    [LOG_U16_CMD] = { 2, " %u ", false, false },
    [LOG_U24_CMD] = { 3, " %u ", false, false },
    [LOG_U32_CMD] = { 4, " %u ", false, false },
    [LOG_U64_CMD] = { 8, " %llu ", false, false },

    [LOG_S8_CMD]  = { 1, " %d ", true,  false },
    [LOG_S16_CMD] = { 2, " %d ", true,  false },
    [LOG_S24_CMD] = { 3, " %d ", true,  false },
    [LOG_S32_CMD] = { 4, " %d ", true,  false },
    [LOG_S64_CMD] = { 8, " %lld ", true,  false },

    [LOG_X8_CMD]  = { 1, " 0x%02x ", false, false },
    [LOG_X16_CMD] = { 2, " 0x%04x ", false, false },
    [LOG_X24_CMD] = { 3, " 0x%06x ", false, false },
    [LOG_X32_CMD] = { 4, " 0x%08x ", false, false },
    [LOG_X64_CMD] = { 8, " 0x%016llx ", false, false },

    [LOG_UNSET_FMT_CMD]     = { 0, NULL, false, false },
    [LOG_SET_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_SET_PFMT_CMD]      = { 2, NULL, false, true  },
    [LOG_SET_FMT_ONCE_CMD]  = { 2, NULL, false, false },
    [LOG_SET_PFMT_ONCE_CMD] = { 2, NULL, false, true  },

    [LOG_TAG_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_TAG_PFMT_CMD]      = { 2, NULL, false, true  },
  };

static const layout_t lay_1 = { 1, NULL, false, false }; // 1 byte  in RAM.
static const layout_t lay_2 = { 2, NULL, false, false }; // 2 bytes in RAM.
static const layout_t lay_4 = { 4, NULL, false, false }; // 4 bytes in RAM.

/* Copy a string from AVR target to the host, but not more than
   LEN_MAX characters.  */

char*
read_string (char *p, unsigned addr, bool flash_p, size_t len_max)
{
  char c;
  size_t n = 0;
  if (flash_p
      && arch.flash_pm_offset
      && addr >= arch.flash_pm_offset)
    {
      // README states that LOG_PSTR etc. can be used to print strings in
      // flash.  This is ambiguous for devices that see flash in RAM:
      // A variable can reside in flash due to __flash or PROGMEM and
      // hence in .progmem.data, or it can be located in .rodata.
      // If the address appears as a .rodata address, then access RAM
      // space (which holds a copy of flash at arch.flash_pm_offset).
      flash_p = false;
    }

  byte *p_avr = cpu_address (addr, flash_p ? AR_FLASH : AR_RAM);

  while (++n < len_max && (c = *p_avr++))
    if (c != '\r')
      *p++ = c;

  *p = '\0';
  return p;
}


// IEEE-754 single
avr_float_t
decode_avr_float (unsigned val)
{
  // float =  s  bbbbbbbb mmmmmmmmmmmmmmmmmmmmmmm
  //         31
  // s = sign (1)
  // b = biased exponent
  // m = mantissa

  int one;
  const int DIG_MANT = 23;
  const int DIG_EXP  = 8;
  const int EXP_BIAS = 127;
  avr_float_t af;

  int r = (1 << DIG_EXP) -1;
  unsigned mant = af.mant = val & ((1 << DIG_MANT) -1);
  val >>= DIG_MANT;
  af.exp_biased = val & r;
  af.exp = af.exp_biased - EXP_BIAS;

  val >>= DIG_EXP;
  af.sign_bit = val & 1;

  // Denorm?
  if (af.exp_biased == 0)
    af.fclass = FT_DENORM;
  else if (af.exp_biased < r)
    af.fclass = FT_NORM;
  else if (mant == 0)
    af.fclass = FT_INF;
  else
    af.fclass = FT_NAN;

  switch (af.fclass)
    {
    case FT_NORM:
    case FT_DENORM:
      one = af.fclass == FT_NORM;
      af.mant1 = mant | (one << DIG_MANT);
      af.x = ldexp ((double) af.mant1, af.exp + 1 - one - DIG_MANT);
      af.x = copysign (af.x, af.sign_bit ? -1.0 : 1.0);
      break;
    case FT_NAN:
      af.x = nan ("");
      break;
    case FT_INF:
      af.x = af.sign_bit ? -HUGE_VAL : HUGE_VAL;
      break;
    }

  return af;
}


// IEEE-754 double
avr_float_t
decode_avr_double (uint64_t val)
{
  // double = s bbbbbbbbbbb mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
  //         63
  // s = sign (1)
  // b = biased exponent
  // m = mantissa

  int one;
  const int DIG_MANT = 52;
  const int DIG_EXP  = 11;
  const int EXP_BIAS = 1023;
  avr_float_t af;

  int r = (1 << DIG_EXP) -1;
  uint64_t mant = af.mant = val & (((uint64_t)1 << DIG_MANT) -1);
  val >>= DIG_MANT;
  af.exp_biased = val & r;
  af.exp = af.exp_biased - EXP_BIAS;

  val >>= DIG_EXP;
  af.sign_bit = val & 1;

  // Denorm?
  if (af.exp_biased == 0)
    af.fclass = FT_DENORM;
  else if (af.exp_biased < r)
    af.fclass = FT_NORM;
  else if (mant == 0)
    af.fclass = FT_INF;
  else
    af.fclass = FT_NAN;

  switch (af.fclass)
    {
    case FT_NORM:
    case FT_DENORM:
      one = af.fclass == FT_NORM;
      af.mant1 = mant | ((uint64_t)one << DIG_MANT);
      af.x = ldexp ((double) af.mant1, af.exp + 1 - one - DIG_MANT);
      af.x = copysign (af.x, af.sign_bit ? -1.0 : 1.0);
      break;
    case FT_NAN:
      af.x = nan ("");
      break;
    case FT_INF:
      af.x = af.sign_bit ? -HUGE_VAL : HUGE_VAL;
      break;
    }

  return af;
}


/* Read a value as unsigned from REGNO.  Bytesize (1..4) and signedness are
   determined by respective layout[].  If the value is signed a cast to
   signed will do the conversion.  */

unsigned
get_reg_value (int regno, const layout_t *lay)
{
  byte *p = cpu_address (regno, AR_REG);
  unsigned val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1U;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


/* Read a value as unsigned long long from R18.  Bytesize (1..8) and
   signedness are determined by respective layout[].  If the value is signed
   a cast to signed will do the conversion.  */

static unsigned long long
get_r18_value (const layout_t *lay)
{
  byte *p = cpu_address (18, AR_REG);
  unsigned long long val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1ull;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


/* Write value VAL to register REGNO.  */

static void
put_reg_value (int regno, int n_regs, uint64_t val)
{
  byte *p = cpu_address (regno, AR_REG);
  for (int i = 0; i < n_regs; ++i)
    {
      *p++ = val & 0xff;
      val >>= 8;
    }
}

/* Read a value as unsigned from ADDR.  Bytesize (1..4) and signedness
   are determined by respective layout[].  If the value is signed, then
   a cast to signed will do the conversion.  */

static unsigned
get_mem_value (unsigned addr, const layout_t *lay)
{
  bool flash_p = lay->in_rom;

  if (flash_p
      && arch.flash_pm_offset
      && addr >= arch.flash_pm_offset)
    {
      // "Flash" is ambiguous for devices that see flash in RAM:
      // A variable can reside in flash due to __flash or PRGMEM and
      // hence in .progmem.data, or it can be located in .rodata.
      // If the address appears as a .rodata address, then access RAM
      // (which holds a copy of flash at arch.flash_pm_offset).
      flash_p = false;
    }

  byte *p = cpu_address (addr, flash_p ? AR_FLASH : AR_RAM);
  unsigned val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1U;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


static uint8_t
get_mem_byte (unsigned addr)
{
  return get_mem_value (addr, & lay_1);
}

ticks_port_t ticks_port;

static uint32_t
get_next_prand (void)
{
  // a prime m
  const uint32_t prand_m = 0xfffffffb;
  // phi (m)
  // uint32_t prand_phi_m = m-1; // = 2 * 5 * 19 * 22605091
  // a primitive root of (Z/m*Z)^*
  const uint32_t prand_root = 0xcafebabe;

  ticks_port_t *tp = &ticks_port;
  uint32_t value = tp->pvalue ? tp->pvalue : 1;
  return tp->pvalue = ((uint64_t) value * prand_root) % prand_m;
}

void
sys_ticks_cmd (int cfg)
{
  cfg &= 0xff;
  ticks_port_t *tp = &ticks_port;

  if (cfg & TICKS_RESET_ALL_CMD)
    {
      log_add ("ticks reset:");
      if (cfg & TICKS_RESET_CYCLES_CMD)
        {
          log_add (" cycles");
          tp->call.state = 0;
          tp->n_cycles = program.n_cycles;
        }
      if (cfg & TICKS_RESET_INSNS_CMD)
        {
          log_add (" insns");
          tp->n_insns = program.n_insns;
        }
      if (cfg & TICKS_RESET_PRAND_CMD)
        {
          log_add (" prand");
          tp->pvalue = 0;
        }
      return;
    }

  if (cfg == TICKS_CYCLES_CALL_CMD)
    {
      tp->call.state = 1;
      log_add ("ticks cycles call");
      return;
    }

  const char *what = "???";
  uint32_t value = 0;

  switch (cfg)
    {
    case TICKS_GET_CYCLES_CMD:
      if (tp->call.state == 3)
        {
          tp->call.state = 0;
          what = "cycles.call";
          value = tp->call.n_cycles;
        }
      else
        {
          what = "cycles";
          value = (uint32_t) (program.n_cycles - tp->n_cycles);
        }
      break;
    case TICKS_GET_INSNS_CMD:
      what = "insn";
      value = (uint32_t) (program.n_insns - tp->n_insns);
      break;
    case TICKS_GET_PRAND_CMD:
      what = "prand";
      value = get_next_prand ();
      break;
    case TICKS_GET_RAND_CMD:
      what = "rand";
      value = rand();
      value ^= (unsigned) rand() << 11;
      value ^= (unsigned) rand() << 22;
      break;
    }

  log_add ("ticks get %s: R22<-(%08x) = %u", what, value, value);

  put_reg_value (22, 4, value);
}


static void
sys_misc_u32 (uint8_t what)
{
  uint32_t a = (uint32_t) get_reg_value (22, & layout[LOG_U32_CMD]);
  uint32_t b = (uint32_t) get_reg_value (18, & layout[LOG_U32_CMD]);
  uint32_t c = 0;
  const char *op = "???";
  const char *name = "???";

  switch (what)
    {
    default:
      leave (LEAVE_USAGE, "unknown misc 32-bit arith function %u\n", what);

    case AVRTEST_MISC_mulu32: op = "*"; name = "mul";
      c = a * b;
      break;

    case AVRTEST_MISC_divu32: op = "/"; name = "div";
      c =  b == 0 ? UINT32_MAX : a / b;
      break;

    case AVRTEST_MISC_modu32: op = "%"; name = "mod";
      c = b == 0 ? a : a % b;
      break;
    }

  put_reg_value (22, 4, c);

  log_add (" arith %su32: %u=0x%x %s %u=0x%x = %u=0x%x", name,
           (unsigned) a, (unsigned) a, op, (unsigned) b, (unsigned) b,
           (unsigned) c, (unsigned) c);
}


static void
sys_misc_s32 (uint8_t what)
{
  int32_t a = (int32_t) get_reg_value (22, & layout[LOG_S32_CMD]);
  int32_t b = (int32_t) get_reg_value (18, & layout[LOG_S32_CMD]);
  int32_t c = 0;
  bool sign;
  const char *op = "???";
  const char *name = "???";

  switch (what)
    {
    default:
      leave (LEAVE_USAGE, "unknown misc 32-bit arith function %u\n", what);

    case AVRTEST_MISC_muls32: op = "*"; name = "mul";
      c = a * b;
      break;

    case AVRTEST_MISC_divs32: op = "/"; name = "div";
      sign = (a < 0) ^ (b < 0);
      c = 0?0
        : b == 0                    ? sign ? 1 : -1
        : a == INT32_MIN && b == -1 ? INT32_MIN
        : a / b;
      break;

    case AVRTEST_MISC_mods32: op = "%"; name = "mod";
      c = 0?0
        : b == 0                    ? a
        : a == INT32_MIN && b == -1 ? 0
        : a % b;
      break;
    }

  put_reg_value (22, 4, c);

  log_add (" arith %ss32: %d=0x%x %s %d=0x%x = %d=0x%x", name,
           (signed) a, (unsigned) (uint32_t) a, op, (signed) b,
           (unsigned) (uint32_t) b, (signed) c, (unsigned) (uint32_t) c);
}

void
sys_log_dump (int what)
{
  what &= 0xff;
  if (what >= LOG_X_sentinel)
    {
      log_add ("log: invalid cmd %d\n", what);
      return;
    }

  static int fmt_once = 0;
  static char xfmt[LEN_LOG_XFMT];
  static char string[LEN_LOG_STRING];
  const layout_t *lay = & layout[what];
  unsigned val = get_reg_value (20, lay);
  const char *fmt = fmt_once ? xfmt : lay->fmt;

  if (fmt_once == 1)
    fmt_once = 0;

  switch (what)
    {
    default:
      log_add ("log %d-byte value", lay->size);
      LOGPRINT (fmt, val);
      break;

    case LOG_S64_CMD:
    case LOG_U64_CMD:
    case LOG_X64_CMD:
      log_add ("log %d-byte value", lay->size);
      LOGPRINT (fmt, get_r18_value (lay));
      break;

    case LOG_SET_FMT_ONCE_CMD:
    case LOG_SET_PFMT_ONCE_CMD:
      log_add ("log set format");
      fmt_once = 1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_SET_FMT_CMD:
    case LOG_SET_PFMT_CMD:
      log_add ("log set format");
      fmt_once = -1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_UNSET_FMT_CMD:
      log_add ("log unset format");
      fmt_once = 0;
      break;

    case LOG_PSTR_CMD:
    case LOG_STR_CMD:
      log_add ("log string");
      read_string (string, val, lay->in_rom, sizeof (string));
      LOGPRINT (fmt, string);
      break;

    case LOG_FLOAT_CMD:
      {
        log_add ("log float");
        avr_float_t af = decode_avr_float (val);
        LOGPRINT (fmt, af.x);
      }
      break;

    case LOG_D64_CMD:
      {
        log_add ("log double");
        avr_float_t af = decode_avr_double (get_r18_value (lay));
        LOGPRINT (fmt, af.x);
      }
      break;

    case LOG_F7T_CMD:
      {
        log_add ("log f7_t");

        char txt[200];
        const int n_mant = 7;
        const unsigned addr = val;
        const uint8_t flags = get_mem_byte (addr);
        const uint8_t m_sign  = 1 << 0;
        const uint8_t m_zero  = 1 << 1;
        const uint8_t m_nan   = 1 << 2;
        const uint8_t m_plusx = 1 << 3;
        const uint8_t m_inf   = 1 << 7;
        const uint8_t m_all = m_sign | m_inf | m_nan | m_zero | m_plusx;

        sprintf (txt, "{ flags = ");
        str_append (txt, flags <= 1 ? "%d" : "0x%02x", flags);
        str_append (txt, " [%c", (flags & m_sign) ? '-' : '+');
        if (flags & m_inf)   str_append (txt, ",Inf");
        if (flags & m_nan)   str_append (txt, ",NaN");
        if (flags & m_zero)  str_append (txt, ",Zero");
        if (flags & m_plusx) str_append (txt, ",PlusX");
        if (flags & ~m_all)  str_append (txt, ",???");

        uint64_t mant = 0;
        str_append (txt, "], mant = { 0x");
        for (int i = n_mant - 1; i >= 0; --i)
          {
            uint8_t b = get_mem_byte (addr + 1 + i);
            str_append (txt, "%02x ", b);
            mant = (mant << 8) | b;
          }
        // mant_bits = 56 = 1 + IEEE_mant_bits + 3
        const uint8_t msb = mant >> (8 * n_mant - 1);
        // Make mant such that it has 13 + 1 nibbles at the low end.
        mant <<= 1;
        const uint8_t lsn = mant & 0xf;
        // Make mant such that it has 13 nibbles at the low end.
        mant >>= 4;
        // mant = 0x1.[IEEE_mant_bits] | 3 extra f7 bits
        mant &= (UINT64_C(1) << 52) - 1;
        str_append (txt, "} = 0x%u.%013" PRIx64 "|%u, expo = %d }",
                    msb, mant, lsn,
                    get_mem_value (addr + 1 + n_mant, & layout[LOG_S16_CMD]));
        LOGPRINT (fmt, txt);
      }
      break;
    }
}


static int
is_special_ulp (const avr_float_t *x, const avr_float_t *y)
{
  if (x->fclass == FT_NAN && y->fclass == FT_NAN)
    return 0;
  else if (x->fclass == FT_INF && y->fclass == FT_INF
           && x->sign_bit == y->sign_bit)
    return 0;
  else if (x->fclass >= FT_INF || y->fclass >= FT_INF)
    return 12345;

  if (x->mant1 == 0 && y->mant1 == 0)
    return 0;

  return -1;
}

// Emulating IEEE single functions.

#if __SIZEOF_FLOAT__ != 4
#define NO_FEMUL "host has no 32-bit IEEE single"
#endif

#if !defined NO_FEMUL && __FLT_MANT_DIG__ != 24
#define NO_FEMUL "host IEEE single mantissa != 24 bits"
#endif

#if !defined NO_FEMUL && __FLT_MAX_EXP__ != 128
#define NO_FEMUL "host IEEE single max exponent != 128"
#endif

#if !defined NO_FEMUL                                   \
  && defined __FLOAT_WORD_ORDER__                       \
  && defined __ORDER_LITTLE_ENDIAN__                    \
  && __FLOAT_WORD_ORDER__ != __ORDER_LITTLE_ENDIAN__
#define NO_FEMUL "host IEEE single is not little endian"
#endif

#define PRIF "%e=%.6a"

#if defined NO_FEMUL
void sys_emul_float (uint8_t fid)
{
  log_add ("not supported: %s", NO_FEMUL);
  leave (LEAVE_FATAL, "IEEE single emulation failed: %s", NO_FEMUL);
}

static void sys_misc_fxtof (uint8_t fid)
{
  log_add ("not supported: %s", NO_FEMUL);
  leave (LEAVE_FATAL, "IEEE single emulation failed: %s", NO_FEMUL);
}
#else // float emulation is supported

static float
get_reg_float (int regno)
{
  float f;
  memcpy (&f, cpu_address (regno, AR_REG), sizeof (float));
  return f;
}

static void
set_reg_float (int regno, float f)
{
  memcpy (cpu_address (regno, AR_REG), &f, sizeof (float));
}

static avr_float_t
get_reg_avr_float (int regno)
{
  uint32_t rval;
  memcpy (&rval, cpu_address (regno, AR_REG), sizeof (rval));
  return decode_avr_float ((unsigned) rval);
}

/* Error of X in units of ULPs (unit in the last place / unit of least
   precision).  Y is the reference / assumed correct value.  */

static float
get_fulp (const avr_float_t *x, const avr_float_t *y)
{
  int iulp = is_special_ulp (x, y);
  if (iulp >= 0)
    return iulp;
  float sx = ldexpf (x->mant1, x->exp) * (x->sign_bit ? -1 : 1);
  float sy = ldexpf (y->mant1, y->exp) * (y->sign_bit ? -1 : 1);
  float ulp = ldexpf (1, y->exp);

  return (sx - sy) / ulp;
}

static float
get_fprand (float lo, float hi)
{
  uint32_t u = get_next_prand() & 0x7fffffff;
  float x = lo + (float) u / 0x7fffffff * (hi - lo);
  return fminf (fmaxf (x, lo), hi);
}


static void
sys_misc_strtof (void)
{
  char s_float[100], *tail;
  const uint16_t addr = get_reg_value (24, & layout[LOG_U16_CMD]);
  const uint16_t pend = get_reg_value (22, & layout[LOG_U16_CMD]);

  read_string (s_float, addr, false, sizeof(s_float) - 1);
  const float f = strtof (s_float, &tail);
  const size_t n_chars = tail - s_float;

  log_add (" strtof 0x%04x:\"%s\" -> " PRIF, addr, s_float, f, f);
  if (*tail)
    {
      log_add (", pend+=%d", (int) n_chars);
      log_add (", *pend=0x%x", *tail);
      if (isprint (*tail))
        log_add ("='%c'", *tail);
    }

  set_reg_float (22, f);

  if (pend)
    {
      byte *p = cpu_address (pend, AR_RAM);
      uint16_t end = addr + n_chars;
      p[0] = end;
      p[1] = end / 256;
    }
}


typedef struct
{
  float (*fun) (float);
  const char *name;
} func1f_t;

typedef struct
{
  float (*fun) (float, float);
  const char *name;
} func2f_t;


#define _(X) [AVRTEST_##X] = { X ## f, #X },
static const func1f_t func1f[] =
  {
    _(sin) _(asin) _(sinh) _(asinh)
    _(cos) _(acos) _(cosh) _(acosh)
    _(tan) _(atan) _(tanh) _(atanh)
    _(exp) _(log)  _(sqrt) _(cbrt)
    _(trunc) _(ceil) _(floor) _(round)
    _(log2) _(log10) _(fabs)
  };
#undef _

#define _(X) [AVRTEST_##X - AVRTEST_EMUL_2args] = { X ## f, #X },
static const func2f_t func2f[] =
  {
    _(pow)  _(atan2) _(hypot)
    _(fmin) _(fmax)  _(fmod)
  };
#undef _

static void
emul_float_misc (uint8_t fid)
{
  switch (fid)
    {
    default:
      leave (LEAVE_USAGE, "unknown IEEE single misc function %u\n", fid);

    case AVRTEST_ldexp:
      {
        float x = get_reg_float (22);
        int y = (int16_t) get_reg_value (20, & layout[LOG_S16_CMD]);
        float z = ldexpf (x, y);
        const char *name = "ldexp";
        log_add ("emulate %sf(" PRIF ", %d) = " PRIF "", name, x,x, y, z,z);
        set_reg_float (22, z);
        break;
      }

    case AVRTEST_u32to:
      {
        uint32_t u32 = (uint32_t) get_reg_value (22, & layout[LOG_U32_CMD]);
        float z = (float) u32;
        log_add ("utof(%u=0x%x) = " PRIF, (unsigned) u32, (unsigned) u32, z,z);
        set_reg_float (22, z);
        break;
      }

    case AVRTEST_s32to:
      {
        int32_t s32 = (int32_t) get_reg_value (22, & layout[LOG_S32_CMD]);
        float z = (float) s32;
        log_add ("stof(%d=0x%x) = " PRIF, (signed) s32, (signed) s32, z,z);
        set_reg_float (22, z);
        break;
      }

    case AVRTEST_cmp:
      {
        float x = get_reg_float (22);
        float y = get_reg_float (18);
        int8_t z = 0?0
          : x < y  ? -1
          : x > y  ? +1
          : x == y ? 0
          : -128;
        log_add ("cmpf(" PRIF ", " PRIF ") = %d", x,x, y,y, (signed) z);
        put_reg_value (24, 1, z);
        break;
      }
    } // switch
}

static void
emul_float2 (uint8_t fid)
{
  float x = get_reg_float (22);
  float y = get_reg_float (18);
  float z = 0;
  const char *name = "???";

  switch (fid)
    {
    case AVRTEST_mul: name = "mul"; z = x * y; break;
    case AVRTEST_div: name = "div"; z = x / y; break;
    case AVRTEST_add: name = "add"; z = x + y; break;
    case AVRTEST_sub: name = "sub"; z = x - y; break;
    case AVRTEST_prand: name = "prand"; z = get_fprand (x, y); break;
    case AVRTEST_ulp: name = "ulp";
      {
        avr_float_t ax = get_reg_avr_float (22);
        avr_float_t ay = get_reg_avr_float (18);
        z = get_fulp (& ax, & ay);
        break;
      }
    default:
      fid -= AVRTEST_EMUL_2args;
      if (fid >= ARRAY_SIZE (func2f) || ! func2f[fid].fun)
        leave (LEAVE_FATAL, "unexpected func2f %u", fid + AVRTEST_EMUL_2args);
      z = func2f[fid].fun (x, y);
      name = func2f[fid].name;
      break;
    }

  log_add ("emulate %sf(" PRIF ", " PRIF ") = " PRIF "", name, x,x, y,y, z,z);

  set_reg_float (22, z);
}

// Emulate IEEE single functions like avrtest_sinf and avrtest_mulf.
void sys_emul_float (uint8_t fid)
{
  if (fid >= AVRTEST_EMUL_sentinel)
    leave (LEAVE_USAGE, "unknown IEEE single emulate function %u\n", fid);

  if (fid >= AVRTEST_EMUL_misc)
    {
      emul_float_misc (fid);
      return;
    }

  if (fid >= AVRTEST_EMUL_2args)
    {
      emul_float2 (fid);
      return;
    }

  if (fid >= ARRAY_SIZE (func1f) || ! func1f[fid].fun)
    leave (LEAVE_FATAL, "unexpected func1 %u", fid);

  float x = get_reg_float (22);
  float z = func1f[fid].fun (x);
  log_add ("emulate %sf(" PRIF ") = " PRIF "", func1f[fid].name, x,x, z,z);

  set_reg_float (22, z);
}


static void
sys_misc_fxtof (uint8_t fid)
{
  // avrtest.h requires <stdfix.h> in order to make sure that types
  // like _Accum are available and can be used in syscall prototypes.
  // The conditional makes sure that avrtest.h works with C++ for example.
  if (fid == AVRTEST_MISC_nofxtof)
    leave (LEAVE_USAGE, "include <stdfix.h> prior to \"avrtest.h\" before"
           " using fixed-point to/from float conversions");

  bool sign = 0;
  int size = 0;
  int fbit = 0;
  bool fxtof = 0;
  const char *name = "???";

  switch (fid)
    {
#define CASE_FX(id, s, sz, fb)                                  \
      case AVRTEST_MISC_##id##tof:                              \
        sign = s; size = sz; fbit = fb; name = #id; fxtof = 1;  \
        break;                                                  \
      case AVRTEST_MISC_fto##id:                                \
        sign = s; size = sz; fbit = fb; name = #id; fxtof = 0;  \
        break

      CASE_FX (r,   1, 2, 15);
      CASE_FX (k,   1, 4, 15);
      CASE_FX (hr,  1, 1,  7);
      CASE_FX (hk,  1, 2,  7);
      CASE_FX (ur,  0, 2, 16);
      CASE_FX (uk,  0, 4, 16);
      CASE_FX (uhr, 0, 1,  8);
      CASE_FX (uhk, 0, 2,  8);
#undef CASE_FX
    }

  int regno = size <= 2 ? 24 : 22;
  int ndigs = 3 + fbit / log2 (10);

  if (fxtof)
    {
      byte *p = cpu_address (regno, AR_REG);
      unsigned val = 0;
      for (int n = size; n; )
        val = (val << 8) | p[--n];

      float f = val;
      if (sign)
        {
          unsigned smask = 1u << (8 * size - 1);
          if (val & smask)
            {
              val |= -smask;
              f = (signed) val;
            }
        }

      f = ldexpf (f, -fbit);
      set_reg_float (22, f);

      log_add (" %stof(0x%0*x) = %.*f", name, 2 * size, val, ndigs, f);
    }
  else
    {
      // float -> fixed
      const float f = get_reg_float (22);
      float v = ldexpf (f, fbit);
      int64_t v64 = v < 0 ? v - 0.5f : v + 0.5f;

      const int64_t mask = (1ull << (8 * size)) - 1;
      const int64_t ma = mask >> sign;
      const int64_t mi = sign ? -ma - 1 : 0;
      if (v64 < mi) v64 = mi;
      if (v64 > ma) v64 = ma;

      unsigned val = v64 & mask;

      put_reg_value (regno, size, v64);

      log_add (" fto%s(%.*f) = 0x%0*x", name, ndigs, f, 2 * size, val);
    }
}

#endif // NO_FEMUL


// Emulating IEEE double functions.

#if __SIZEOF_DOUBLE__ == 8
typedef double host_double_t;
#   define HOST_DOUBLE
#   define DOUBLE_MANT_DIG __DBL_MANT_DIG__
#   define DOUBLE_MAX_EXP  __DBL_MAX_EXP__
#   define PRID "%e=%.13a"
#elif __SIZEOF_LONG_DOUBLE__ == 8
typedef long double host_double_t;
#   define HOST_LONG_DOUBLE
#   define DOUBLE_MANT_DIG __LDBL_MANT_DIG__
#   define DOUBLE_MAX_EXP  __LDBL_MAX_EXP__
#   define PRID "%Le=%.13La"
#else
#define NO_DEMUL "host has no 64-bit IEEE double"
#endif

#if !defined NO_DEMUL && DOUBLE_MANT_DIG != 53
#define NO_DEMUL "host IEEE double mantissa != 53 bits"
#endif

#if !defined NO_DEMUL && DOUBLE_MAX_EXP != 1024
#define NO_DEMUL "host IEEE double max exponent != 1024"
#endif

#if !defined NO_DEMUL                                   \
  && defined __FLOAT_WORD_ORDER__                       \
  && defined __ORDER_LITTLE_ENDIAN__                    \
  && __FLOAT_WORD_ORDER__ != __ORDER_LITTLE_ENDIAN__
#define NO_DEMUL "host IEEE double is not little endian"
#endif


#if defined NO_DEMUL
void sys_emul_double (uint8_t fid)
{
  log_add ("not supported: %s", NO_DEMUL);
  leave (LEAVE_FATAL, "IEEE double emulation failed: %s", NO_DEMUL);
}
#else // double emulation is supported

static host_double_t
get_reg_double (int regno)
{
  host_double_t d;
  memcpy (&d, cpu_address (regno, AR_REG), sizeof (host_double_t));
  return d;
}

static void
set_reg_double (int regno, host_double_t d)
{
  memcpy (cpu_address (regno, AR_REG), &d, sizeof (host_double_t));
}

static avr_float_t
get_reg_avr_double (int regno)
{
  uint64_t rval;
  memcpy (&rval, cpu_address (regno, AR_REG), sizeof (rval));
  return decode_avr_double (rval);
}

/* Error of X in units of ULPs (unit in the last place / unit of least
   precision).  Y is the reference / assumed correct value.  */

static host_double_t
get_dulp (const avr_float_t *x, const avr_float_t *y)
{
  int iulp = is_special_ulp (x, y);
  if (iulp >= 0)
    return iulp;
  host_double_t sx = ldexp (x->mant1, x->exp) * (x->sign_bit ? -1 : 1);
  host_double_t sy = ldexp (y->mant1, y->exp) * (y->sign_bit ? -1 : 1);
  host_double_t ulp = ldexp (1, y->exp);

  return (sx - sy) / ulp;
}

static host_double_t
get_dprand (host_double_t lo, host_double_t hi)
{
  uint64_t u1 = get_next_prand();
  uint64_t u2 = get_next_prand();
  uint64_t mask = UINT64_MAX >> 1;
  uint64_t u = (u1 | (u2 << 31)) & mask;
  host_double_t x = lo + (host_double_t) u / mask * (hi - lo);
  if (x > hi) x = hi;
  if (x < lo) x = lo;
  return x;
}

typedef struct
{
  host_double_t (*fun) (host_double_t);
  const char *name;
} func1l_t;

typedef struct
{
  host_double_t (*fun) (host_double_t, host_double_t);
  const char *name;
} func2l_t;


#if defined HOST_DOUBLE
#define _(X) [AVRTEST_##X] = { X, #X },
#elif defined HOST_LONG_DOUBLE
#define _(X) [AVRTEST_##X] = { X ## l, #X },
#endif
static const func1l_t func1l[] =
  {
    _(sin) _(asin) _(sinh) _(asinh)
    _(cos) _(acos) _(cosh) _(acosh)
    _(tan) _(atan) _(tanh) _(atanh)
    _(exp) _(log)  _(sqrt) _(cbrt)
    _(trunc) _(ceil) _(floor) _(round)
    _(log2) _(log10) _(fabs)
  };
#undef _

#if defined HOST_DOUBLE
#define _(X) [AVRTEST_##X - AVRTEST_EMUL_2args] = { X, #X },
#elif defined HOST_LONG_DOUBLE
#define _(X) [AVRTEST_##X - AVRTEST_EMUL_2args] = { X ## l, #X },
#endif
static const func2l_t func2l[] =
  {
    _(pow)  _(atan2) _(hypot)
    _(fmin) _(fmax)  _(fmod)
  };
#undef _

static void
emul_double2 (uint8_t fid)
{
  host_double_t x = get_reg_double (18);
  host_double_t y = get_reg_double (10);
  host_double_t z = 0;
  const char *name = "???";

  switch (fid)
    {
    case AVRTEST_mul: name = "mul"; z = x * y; break;
    case AVRTEST_div: name = "div"; z = x / y; break;
    case AVRTEST_add: name = "add"; z = x + y; break;
    case AVRTEST_sub: name = "sub"; z = x - y; break;
    case AVRTEST_prand: name = "prand"; z = get_dprand (x, y); break;
    case AVRTEST_ulp: name = "ulp";
      {
        avr_float_t ax = get_reg_avr_double (18);
        avr_float_t ay = get_reg_avr_double (10);
        z = get_dulp (& ax, & ay);
        break;
      }
    default:
      fid -= AVRTEST_EMUL_2args;
      if (fid >= ARRAY_SIZE (func2l) || ! func2l[fid].fun)
        leave (LEAVE_FATAL, "unexpected func2l %u", fid + AVRTEST_EMUL_2args);
      z = func2l[fid].fun (x, y);
      name = func2l[fid].name;
      break;
    }

  log_add ("emulate %sl(" PRID ", " PRID ") = " PRID, name, x,x, y,y, z,z);

  set_reg_double (18, z);
}

static void
emul_double_misc (uint8_t fid)
{
  switch (fid)
    {
    default:
      leave (LEAVE_USAGE, "unknown IEEE double misc function %u\n", fid);

    case AVRTEST_ldexp:
      {
        host_double_t x = get_reg_double (18);
        int y = (int16_t) get_reg_value (16, & layout[LOG_S16_CMD]);
        host_double_t z = -1;
#if defined HOST_DOUBLE
        z = ldexp (x, y);
#elif defined HOST_LONG_DOUBLE
        z = ldexpl (x, y);
#endif
        const char *name = "ldexp";
        log_add ("emulate %sl(" PRID ", %d) = " PRID "", name, x,x, y, z,z);
        set_reg_double (18, z);
        break;
      }
    } // switch
}

// Emulate IEEE double functions like avrtest_sin and avrtest_mul.
void sys_emul_double (uint8_t fid)
{
  if (fid >= AVRTEST_EMUL_sentinel)
    leave (LEAVE_USAGE, "unknown IEEE double emulate function %u\n", fid);

  switch (fid)
    {
    case AVRTEST_ldexp:
      emul_double_misc (fid);
      return;
    }

  if (fid >= AVRTEST_EMUL_2args)
    {
      emul_double2 (fid);
      return;
    }

  if (fid >= ARRAY_SIZE (func1l) || ! func1l[fid].fun)
    leave (LEAVE_FATAL, "unexpected func1l %u", fid);

  host_double_t x = get_reg_double (18);
  host_double_t z = func1l[fid].fun (x);
  log_add ("emulate %sl(" PRID ") = " PRID, func1l[fid].name, x,x, z,z);

  set_reg_double (18, z);
}
#endif // NO_DEMUL


void sys_misc_emul (uint8_t what)
{
  switch (what)
    {
    case AVRTEST_MISC_nofxtof:
    case AVRTEST_MISC_rtof:   case AVRTEST_MISC_urtof:
    case AVRTEST_MISC_ktof:   case AVRTEST_MISC_uktof:
    case AVRTEST_MISC_hrtof:  case AVRTEST_MISC_uhrtof:
    case AVRTEST_MISC_hktof:  case AVRTEST_MISC_uhktof:

    case AVRTEST_MISC_ftor:   case AVRTEST_MISC_ftour:
    case AVRTEST_MISC_ftok:   case AVRTEST_MISC_ftouk:
    case AVRTEST_MISC_ftohr:  case AVRTEST_MISC_ftouhr:
    case AVRTEST_MISC_ftohk:  case AVRTEST_MISC_ftouhk:
      sys_misc_fxtof (what);
      break;

    case AVRTEST_MISC_mulu32:
    case AVRTEST_MISC_divu32:
    case AVRTEST_MISC_modu32:
      sys_misc_u32 (what);
      break;

    case AVRTEST_MISC_muls32:
    case AVRTEST_MISC_divs32:
    case AVRTEST_MISC_mods32:
      sys_misc_s32 (what);
      break;

    case AVRTEST_MISC_strtof:
      sys_misc_strtof ();
      break;

    default:
      leave (LEAVE_FATAL, "syscall 21 misc R26=%d not implemented", what);
    }
}


// Magic values from avr-libc::stdio.h

#define AVRLIBC_SEEK_SET 0
#define AVRLIBC_SEEK_CUR 1
#define AVRLIBC_SEEK_END 2

#define AVRLIBC_EOF (-1)

// Handles for special streams, e.g. to fflush() them.
#define N_STD_FILES 3
#define HANDLE_stdin  (-1)
#define HANDLE_stdout (-2)
#define HANDLE_stderr (-3)

#define AT "@"

#define FIND_UNUSED_FILE 0x1234


typedef int8_t handle_t;

typedef struct
{
  handle_t handle;
  bool binary_p;
  bool std_file_p;
  FILE *file;
  char name[10];
} file_t;


static INLINE file_t*
find_file (int handle)
{
  static file_t files[8 + N_STD_FILES];
  static const int n_files = sizeof (files) / sizeof (*files);

  static bool files_initialized_p;
  if (!files_initialized_p)
    {
      files_initialized_p = true;
      for (int i = N_STD_FILES; i < n_files; i++)
        {
          files[i].handle = i + 1 - N_STD_FILES;
          sprintf (files[i].name, AT "%d", files[i].handle);
        }

#define STD_INIT(f) \
      files[-1 - HANDLE_##f] = (file_t) { HANDLE_##f, true, true, f, AT #f }
      STD_INIT (stdin);
      STD_INIT (stdout);
      STD_INIT (stderr);
#undef STD_INIT
    }

  if (handle > 0 && handle <= n_files - N_STD_FILES)
    {
      // Return an already initialized / opened handle.

      file_t *file = & files[handle - 1 + N_STD_FILES];
      if (!file->file)
        leave (LEAVE_HOSTIO, "file handle %s not open", file->name);
      if (file->handle != handle)
        leave (LEAVE_FATAL, "file %s handle %d should be %d",
               file->name, file->handle, handle);
      return file;
    }
  else if (-handle <= N_STD_FILES && handle < 0)
    {
      return & files[-1 - handle];
    }
  else if (handle == FIND_UNUSED_FILE)
    {
      // Search for an unused files[] slot.

      file_t *file = files + N_STD_FILES;
      for (int i = N_STD_FILES; i < n_files; i++, file++)
        {
          if (!file->file)
            {
              if (file->handle != i + 1 - N_STD_FILES)
                leave (LEAVE_FATAL, "file %s handle %d should be %d",
                       file->name, file->handle, i + 1 - N_STD_FILES);
              return file;
            }
        }
    }
  else
    {
      // We are called with an invalid handle.
      leave (LEAVE_HOSTIO, "file handle %d out of range", handle);
    }

  leave (LEAVE_HOSTIO, "ran out of %d file handles", n_files - N_STD_FILES);
}


// FILE* fopen (const char *s_path, const char *s_mode)
static dword
host_fopen (dword args)
{
  if (!options.do_sandbox)
    leave (LEAVE_USAGE, "file i/o requires option '-sbox SANDBOX'");

  word p_file = (word) args;
  word p_mode = (word) (args >> 16);

  char s_file[40];
  char s_mode[5];
  read_string (s_file, p_file, AR_RAM, sizeof (s_file));
  read_string (s_mode, p_mode, AR_RAM, sizeof (s_mode));
  log_add (" (%04x)->\"%s\" (%04x)->\"%s\"", p_file, s_file, p_mode, s_mode);
  if (strstr (s_file, ".."))
    leave (LEAVE_HOSTIO, "bad file name in syscall open: \"%s\"", s_file);

  file_t *file = find_file (FIND_UNUSED_FILE);
  char s_path[1 + strlen (fileio_sandbox) + strlen (s_file)];
  strcpy (s_path, fileio_sandbox);
  strcat (s_path, s_file);
  file->file = fopen (s_path, s_mode);
  if (!file->file)
    {
      log_add ("\n*** cannot fopen \"%s\"", s_path);
      if (options.do_verbose)
        {
          char msg[16 + strlen (s_path)];
          sprintf (msg, "file i/o: %s", s_path);
          perror (msg);
        }
      return 0;
    }
  log_add ("\n*** %s <- fopen \"%s\" for \"%s\"",
           file->name, s_path, s_mode);
  file->binary_p = strchr (s_mode, 'b');
  return 0xff & (dword) file->handle;
}


// int fclose (FILE*);
static dword host_fclose (dword args)
{
  handle_t handle = (handle_t) args;
  file_t *file = find_file (handle);
  log_add (" %s", file->name);

  if (handle > 0)
    {
      fclose (file->file);
      file->file = NULL;
    }

  return 0;
}


// int fputc (char /* sic! */ c , FILE*);
// "char c" because that is what avr-libc's FDEV_SETUP_STREAM
// specifies for the put function.
static dword host_fputc (dword args)
{
  handle_t handle = (handle_t) (args >> 8);
  file_t *file = find_file (handle);

  byte c = (byte) args;
  if (!file->binary_p && c == '\r')
    return 0;

  log_add (" %s <- %02x", file->name, c);
  int r = fputc ((char) c, file->file);
  return r == EOF ? (0xffff & AVRLIBC_EOF) : (0xff & r);
}


// int fgetc (FILE*);
static dword host_fgetc (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  int c = fgetc (file->file);

  if (c == EOF)
    log_add (" -> EOF");
  else
    log_add (" -> %02x", (byte) c);

  return 0xffff & (c == EOF ? AVRLIBC_EOF : (0xff & c));
}


// int feof (FILE*);
static dword host_feof (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  int c = feof (file->file);
  log_add (" -> %d", c);
  return !!c;
}


// void clearerr (FILE*);
static dword host_clearerr (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  clearerr (file->file);
  return 0;
}


// int fflush (FILE*);
static dword host_fflush (dword args)
{
  int r = EOF;
  handle_t handle = (handle_t) args;

  if (0 == handle)
    {
      log_add (" %s", AT "all");
      r = fflush (NULL);
    }
  else
    {
      file_t *file = find_file (handle);
      log_add (" %s", file->name);
      r = fflush (file->file);
    }

  return r == EOF ? (0xffff & AVRLIBC_EOF) : 0;
}


typedef struct
{
  int wo;
  const char *text;
} avrlibc_stdio_seek_t;


// int fseek (FILE*, long pos, int whence);
static dword host_fseek (dword args)
{
  static const avrlibc_stdio_seek_t avrlibc_seek[] =
    {
      // Magic 0, 1, 2 are from avr-libc::stdio.h.
      [AVRLIBC_SEEK_SET] = { SEEK_SET, "SEEK_SET" },
      [AVRLIBC_SEEK_CUR] = { SEEK_CUR, "SEEK_CUR" },
      [AVRLIBC_SEEK_END] = { SEEK_END, "SEEK_END" },
    };

  word pargs = (word) args;
  handle_t hnd = (handle_t) get_mem_value (pargs, & lay_1);
  long pos     = get_mem_value (pargs + 1, & lay_4);
  byte wo      = get_mem_value (pargs + 5, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (pos)->%ld (whence)->%d=%s", file->name, pos, wo,
           wo <= 2 ? avrlibc_seek[wo].text : "?");

  if (wo > 2)
    leave (LEAVE_HOSTIO, "bad 3rd argument for fseek %s: %d", file->name, wo);

  if (hnd < 0)
    leave (LEAVE_HOSTIO, "cannot seek in %s", file->name);

  return 0xffff & fseek (file->file, (int32_t) pos, avrlibc_seek[wo].wo);
}


// size_t fread (void *ptr, size_t size, size_t nmemb, FILE *stream);
static dword host_fread (dword args)
{
  word pargs = (word) args;
  word ptr     = (word) get_mem_value (pargs + 0, & lay_2);
  size_t size  = (word) get_mem_value (pargs + 2, & lay_2);
  size_t nmemb = (word) get_mem_value (pargs + 4, & lay_2);
  handle_t hnd = (handle_t) get_mem_value (pargs + 6, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (ptr)->%04x (size)->%d (nmemb)->%d", file->name,
           ptr, (int) size, (int) nmemb);

  return fread (cpu_address (ptr, AR_RAM), size, nmemb, file->file);
}


// size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream);
static dword host_fwrite (dword args)
{
  word pargs = (word) args;
  word ptr     = (word) get_mem_value (pargs + 0, & lay_2);
  size_t size  = (word) get_mem_value (pargs + 2, & lay_2);
  size_t nmemb = (word) get_mem_value (pargs + 4, & lay_2);
  handle_t hnd = (handle_t) get_mem_value (pargs + 6, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (ptr)->%04x (size)->%d (nmemb)->%d", file->name,
           ptr, (int) size, (int) nmemb);

  return fwrite (cpu_address (ptr, AR_RAM), size, nmemb, file->file);
}


/* The worker function for file i/o (syscall 26).  R24 is passed as WHAT
   and specifies what file i/o operation to perform.  R20 are the input
   arguments of that syscall.  If all arguments together fit into 4 bytes,
   then they are passed in R20.  Otherwise, the lower 2 bytes of R20
   hold the memory address where to find the arguments for the specified
   function.  */

dword
host_fileio (byte what, dword r20)
{
  static const struct
  {
    // The handler that takes care of what R24 tells us to perform.
    dword (*hnd)(dword);

    // The name of the C function and the number of bytes used from args.
    // Only used for logging.
    const char *label;
    int n_bytes;
  } host_hnd[] =
      {
#define HANDLER(n, f)   [AVRTEST_##f] = { host_##f, #f, n }
        HANDLER (4, fopen),
        HANDLER (1, fclose),
        HANDLER (1, fgetc),
        HANDLER (2, fputc),
        HANDLER (1, feof),
        HANDLER (1, clearerr),
        HANDLER (2, fseek),     // -> 6 bytes
        HANDLER (1, fflush),
        HANDLER (2, fread),     // -> 7 bytes
        HANDLER (2, fwrite),    // -> 7 bytes
#undef HANDLER
      };

  if (what >= sizeof (host_hnd) / sizeof (*host_hnd))
    leave (LEAVE_HOSTIO,
           "not implemented: syscall 26 file i/o handler (R24)->%d", what);

  if (is_avrtest_log)
    {
      int n_bytes = host_hnd[what].n_bytes;
      unsigned mask = (2u << (8 * n_bytes - 1)) - 1;
      log_add ("file i/o #%d=%s (args)->%0*x", what, host_hnd[what].label,
               2 * n_bytes, mask & r20);
    }

  return host_hnd[what].hnd (r20);
}
