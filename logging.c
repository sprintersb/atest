/*
  This file is part of avrtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2014 Free Software Foundation, Inc.
   
  avrtest is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  avrtest is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with avrtest; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "sreg.h"

#ifndef AVRTEST_LOG
#error no function herein is needed without AVRTEST_LOG
#endif // AVRTEST_LOG

const char s_SREG[] = "CZNVSHTI";

static const char *func_symbol[MAX_FLASH_SIZE/2];

static const char* const mnemonic[] =
  {
#define AVR_OPCODE(ID, N_WORDS, N_TICKS, NAME)  \
    [ID_ ## ID] = NAME,
#include "avr-opcode.def"
#undef AVR_OPCODE
  };

// ports used for application <-> simulator interactions
#define IN_AVRTEST
#include "avrtest.h"

#define LEN_PERF_TAG_STRING  50
#define LEN_PERF_TAG_FMT    200
#define LEN_PERF_LABEL      100
#define LEN_LOG_STRING      500
#define LEN_LOG_XFMT        500
#define LEN_SYMBOL_STACK    100

#define NUM_PERFS 8
#define NUM_PERF_CMDS 8

#include "logging.h"

static alog_t alog;
static perf_t perf;
static perfs_t perfs[NUM_PERFS];

void
log_append (const char *fmt, ...)
{
  if (alog.unused)
    return;

  va_list args;
  va_start (args, fmt);
  alog.pos += vsprintf (alog.pos, fmt, args);
  va_end (args);
}

static INLINE int
mask_to_bit (int val)
{
  switch (val)
    {
    default: return -1;
    case 1<<0 : return 0;
    case 1<<1 : return 1;
    case 1<<2 : return 2;
    case 1<<3 : return 3;
    case 1<<4 : return 4;
    case 1<<5 : return 5;
    case 1<<6 : return 6;
    case 1<<7 : return 7;
    }
}


// Patch the instruction mnemonic to be more familiar
// and more specific about bits
static void
log_patch_mnemo (const decoded_t *d, char *buf)
{
  int id, mask, style = 0;

  switch (id = d->id)
    {
    default:
      return;
    case ID_BLD:  case ID_SBI:
    case ID_BST:  case ID_CBI:
    case ID_SBIS:  case ID_SBIS2:  case ID_SBRS:  case ID_SBRS2:
    case ID_SBIC:  case ID_SBIC2:  case ID_SBRC:  case ID_SBRC2:
      mask = d->op2;
      style = 1;
      break;
    case ID_BRBS:  case ID_BRBC:
      mask = d->op2;
      style = 2;
      break;
    case ID_BSET: case ID_BCLR:
      mask = d->op1;
      style = 3;
      break;
    }

  int val = mask_to_bit (mask);

  switch (style)
    {
    case 1:
      // CBI.* --> CBI.4 etc.
      buf[-1] = "01234567"[val];
      return;
    case 2:
      {
        const char *s = NULL;
        switch (mask)
          {
          // "BR*S" --> "BREQ" etc.
          // "BR*C" --> "BRNE" etc.
          case FLAG_Z: s = id == ID_BRBS ? "EQ" : "NE"; break;
          case FLAG_N: s = id == ID_BRBS ? "MI" : "PL"; break;
          case FLAG_S: s = id == ID_BRBS ? "LT" : "GE"; break;
          // "BR*C" --> "BRVC" etc.
          // "BR*S" --> "BRVS" etc.
          default: buf[-2] = s_SREG[val]; return;
          }
        buf[-2] = s[0];
        buf[-1] = s[1];
        return;
      }
    case 3:
      // SE* --> SEI  etc.
      // CL* --> CLI  etc.
      buf[-1] = s_SREG[val];
      return;
    }
}


void
log_set_func_symbol (int addr, const char *name, int is_func)
{
  if (is_func)
    {
      if (addr % 2 != 0
          || addr >= MAX_FLASH_SIZE)
        leave (EXIT_STATUS_ABORTED, "'%s': bad symbol at 0x%x", name, addr);
    }
  else if (addr >= MAX_FLASH_SIZE
           || addr % 2 != 0)
    // Something weird, maybe orphan etc.
    return;

  func_symbol[addr/2] = name;
  
  if (0 == strcmp ("__prologue_saves__", name))
    alog.prologue_save.pc = addr/2;
  if (0 == strcmp ("__epilogue_restores__", name))
    alog.epilogue_restore.pc = addr/2;
}


static inline
const char *func_name (int i)
{
  return i >= 0 && i < LEN_SYMBOL_STACK && alog.symbol_stack[i]
    ? alog.symbol_stack[i]
    : "?";
}


/* Track the current call depth for performance metering and to display
   during instruction logging as functions are entered / left.
   Tracking setjmp / longjmp is too painful and would bring hardly
   any benefit -- for now we are fine with a note in README.  */

static void
set_call_depth (const decoded_t *d)
{
  int call = 0;
  int id = d->id;

  switch (id)
    {
    case ID_RCALL:
      // avr-gcc uses "rcall ." to allocate stack, but an assembler
      // program might use that instruction just as well for an
      // ordinary call.  We cannot decide what's going on and take
      // the case that's more likely: Offset == 0 is allocating stack.
      call = d->op2 != 0;
      break;
    case ID_ICALL: case ID_CALL: case ID_EICALL:
      call = 1;
      break;
    case ID_RET:
      // GCC might use push/push/ret for indirect jump,
      // don't account these for call depth
      if (perf.id != ID_PUSH)
        call = -1;
      break;
    }

  perf.calls += call;

  if (!options.do_symbols)
    return;

  int calls = alog.calls = alog.calls + alog.calls_changed;
  const char *name = func_symbol[cpu_PC], *old_name = NULL;

  // Special handling for __prologue_saves__ and __epilogue_restores__
  // from libgcc.
  const char *const prologue_saves = "__prologue_saves__";

  if (alog.prologue_save.func
      // __prologue_saves__ ends with [E]IJMP
      && (alog.id == ID_IJMP || alog.id == ID_EIJMP))
    {
      name = alog.prologue_save.func;
      alog.prologue_save.func = NULL;
      if (calls >= 0 && calls < LEN_SYMBOL_STACK)
        old_name = prologue_saves;
    }

  // insn call_prologue_saves: %~jmp __prologue_saves__
  // insn epilogue_restores:   %~jmp __epilogue_restores__
  if (alog.id == ID_RJMP || alog.id == ID_JMP)
    {
      unsigned off;
      static char buf[30];
      // __prologue_saves__ has 18 PUSH entry points
      if ((off = (unsigned) (cpu_PC - alog.prologue_save.pc)) < 18
          && alog.prologue_save.pc)
        {
          sprintf (buf, "__prologue_saves__ + 2*%u", off);
          alog.prologue_save.func = func_name (calls);
          name = buf;
        }
      // __epilogue_restores__ has 18 LDD *, Y+q entry points
      if ((off = (unsigned) (cpu_PC - alog.epilogue_restore.pc)) < 18
          && alog.epilogue_restore.pc)
        {
          sprintf (buf, "__epilogue_restores__ + 2*%u", off);
          alog.epilogue_restore.func = func_name (calls);
          name = buf;
        }
    }

  // Update symbol stack according to call stack
  if (calls >= 0 && calls < LEN_SYMBOL_STACK)
    {
      if (name)
        {
          if (!old_name)
            old_name = alog.symbol_stack[calls];
          alog.symbol_stack[calls] = name;
        }
      else if (alog.calls_changed == 1)
        alog.symbol_stack[calls] = "?";
    }

  if (name || alog.calls_changed)
    {
      const char *s0  = func_name (calls);
      const char *s1  = func_name (calls + 1);
      const char *s_1 = func_name (calls - 1);

      switch (alog.calls_changed)
        {
        case 0:
          if (!old_name || 0 == strcmp (old_name, s0))
            log_append ("::: [%d] %s \n", calls, s0);
          else if (old_name == prologue_saves)
            log_append ("::: [%d] %s <-- %s \n", calls, s0, old_name);
          else if (*s0 != '.' || *old_name != '.')
            log_append ("::: [%d] %s --> %s \n", calls, old_name, s0);
          break;

        case 1:
          log_append ("::: [%d]-->[%d] %s --> %s \n", calls-1, calls, s_1, s0);
          break;

        case -1:
          if (alog.epilogue_restore.func)
            log_append ("::: [%d]<--[%d] %s <-- %s <-- __epilogue_restores__"
                        "\n", calls, calls+1, s0, alog.epilogue_restore.func);
          else
            log_append ("::: [%d]<--[%d] %s <-- %s \n", calls, calls+1, s0, s1);
          alog.epilogue_restore.func = NULL;
          break;

        default:
          leave (EXIT_STATUS_FATAL, "problem in set_call_depth");
        }
    }

  alog.calls_changed = call;
}


void
log_add_instr (const decoded_t *d)
{
  set_call_depth (d);
  alog.id = d->id;

  char mnemo_[16];
  const char *fmt, *mnemo = mnemonic[alog.id];

  // SYSCALL 0..3 might turn on logging: always log them to alog.data[].

  int maybe_used = alog.maybe_log
                   || (alog.id == ID_SYSCALL && d->op1 <= 3);

  if ((alog.unused = !maybe_used))
    return;

  strcpy (mnemo_, mnemo);
  log_patch_mnemo (d, mnemo_ + strlen (mnemo));
  fmt = arch.pc_3bytes ? "%06x: %-7s " : "%04x: %-7s ";
  log_append (fmt, cpu_PC * 2, mnemo_);
}


void
log_add_flag_read (int mask, int value)
{
  if (alog.unused)
    return;

  int bit = mask_to_bit (mask);
  log_append (" %c->%c", s_SREG[bit], '0' + !!value);
}

void
log_add_data_mov (const char *format, int addr, int value)
{
  if (alog.unused)
    return;

  char name[16];
  const char *s_name = name;

  if (addr_SREG == addr)
    {
      char *s = name;
      for (const char *f = s_SREG; *f; f++, value >>= 1)
        if (value & 1)
          *s++ = *f;
      *s++ = '\0';
      log_append (format, s_name);
      return;
    }

  for (const magic_t *p = named_port; ; p++)
    {
      if (addr == p->addr
          && (p->pon == NULL || *p->pon))
        s_name = p->name;
      else if (p->name == NULL)
        sprintf (name, addr < 256 ? "%02x" : "%04x", addr);
      else
        continue;
      break;
    }

  log_append (format, s_name, value);
}


// IEEE 754 single
static avr_float_t
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
      af.x = ldexp ((double) af.mant1, af.exp - DIG_MANT);
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

// Copy a string from AVR target to the host, but not more than
// LEN_MAX characters.
static char*
read_string (char *p, unsigned addr, int flash_p, size_t len_max)
{
  char c;
  size_t n = 0;
  byte *p_avr = log_cpu_address (addr, flash_p ? AR_FLASH : AR_RAM);

  while (++n < len_max && (c = *p_avr++))
    if (c != '\r')
      *p++ = c;

  *p = '\0';
  return p;
}


// Read a value as unsigned from R20.  Bytesize (1..4) and
// signedness are determined by respective layout[].
// If the value is signed a cast to signed will do the conversion.
static unsigned
get_r20_value (const layout_t *lay)
{
  byte *p = log_cpu_address (20, AR_REG);
  unsigned val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1U;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}

enum
{
  TI_CYCLES,
  TI_INSNS,
  TI_RAND,
  TI_PRAND
};

// a prime m
static uint32_t prand_m = 0xfffffffb;
// phi (m)
// uint32_t prand_phi_m = m-1; // = 2 * 5 * 19 * 22605091
// a primitive root of (Z/m*Z)^*
static uint32_t prand_root = 0xcafebabe;

typedef struct
{
  // What value will be read from TICKS_PORT.
  int config;
  // Offset set by RESET.
  dword n_insns;
  dword n_cycles;
  // Current value for PRAND mode
  uint32_t pvalue;
} ticks_port_t;

static ticks_port_t ticks_port;


static const char*
s_ti (int mode)
{
  switch (mode)
    {
    case TI_CYCLES: return ("cycles");
    case TI_INSNS:  return ("isns");
    case TI_PRAND:  return ("prand");
    case TI_RAND:   return ("rand");
    }
  return "";
}


unsigned
log_get_ticks (byte *p)
{
  uint32_t value = 0;

  switch (ticks_port.config)
    {
    case TI_CYCLES:
      value = program.n_cycles - ticks_port.n_cycles;
      break;
    case TI_INSNS:
      value = program.n_insns - ticks_port.n_insns;
      break;
    case TI_RAND:
      value = rand();
      value ^= (unsigned) rand() << 11;
      value ^= (unsigned) rand() << 22;
      break;
    case TI_PRAND:
      value = ticks_port.pvalue ? ticks_port.pvalue : 1;
      value = ((uint64_t) value * prand_root) % prand_m;
      ticks_port.pvalue = value;
      break;
    }

  *p++ = value;
  *p++ = value >> 8;
  *p++ = value >> 16;
  *p++ = value >> 24;

  return value;
}


static void
do_get_ticks (void)
{
  unsigned value = log_get_ticks (log_cpu_address (22, AR_REG));
  log_append ("ticks: %s: R22<-(%u)", s_ti (ticks_port.config), value);
}


static void
do_ticks_cmd (int cfg)
{
  const char *r = NULL;
  const char *s = NULL;
  ticks_port_t *tp = &ticks_port;
  cfg &= 0xff;

  switch (cfg)
    {
    case TICKS_RESET_CMD:
      r = s_ti (tp->config);
      switch (tp->config)
        {
        case TI_CYCLES: tp->n_cycles = program.n_cycles; break;
        case TI_INSNS:  tp->n_insns  = program.n_insns;  break;
        case TI_PRAND:  tp->pvalue = 0; break;
        default: r = NULL; break;
        }
      break;

    case TICKS_RESET_ALL_CMD:
      r = "all";
      tp->n_cycles = program.n_cycles;
      tp->n_insns  = program.n_insns;
      tp->pvalue = 0;
      break;

    case TICKS_IS_CYCLES_CMD: s = s_ti (tp->config = TI_CYCLES); break;
    case TICKS_IS_INSNS_CMD:  s = s_ti (tp->config = TI_INSNS);  break;
    case TICKS_IS_PRAND_CMD:  s = s_ti (tp->config = TI_PRAND);  break;
    case TICKS_IS_RAND_CMD:   s = s_ti (tp->config = TI_RAND);   break;
    }

  if (r) log_append ("ticks: reset %s", r);
  if (s) log_append ("ticks: config as %s", s);
}


static void
do_log_dump (int what)
{
  what &= 0xff;
  if (what >= LOG_X_sentinel)
    {
      log_append ("log: invalid cmd %d\n", what);
      return;
    }

  static int fmt_once = 0;
  static char xfmt[LEN_LOG_XFMT];
  static char string[LEN_LOG_STRING];
  const layout_t *lay = & layout[what];
  unsigned val = get_r20_value (lay);
  const char *fmt = fmt_once ? xfmt : lay->fmt;

  if (fmt_once == 1)
    fmt_once = 0;

  switch (what)
    {
    default:
      log_append ("log %d-byte value", lay->size);
      printf (fmt, val);
      break;

    case LOG_SET_FMT_ONCE_CMD:
    case LOG_SET_PFMT_ONCE_CMD:
      log_append ("log set format");
      fmt_once = 1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_SET_FMT_CMD:
    case LOG_SET_PFMT_CMD:
      log_append ("log set format");
      fmt_once = -1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_UNSET_FMT_CMD:
      log_append ("log unset format");
      fmt_once = 0;
      break;

    case LOG_PSTR_CMD:
    case LOG_STR_CMD:
      log_append ("log string");
      read_string (string, val, lay->in_rom, sizeof (string));
      printf (fmt, string);
      break;

    case LOG_FLOAT_CMD:
      {
        log_append ("log float");
        avr_float_t af = decode_avr_float (val);
        printf (fmt, af.x);
      }
      break;
    }
}


enum
{
  LOG_ON_CMD,
  LOG_OFF_CMD,
  LOG_SET_CMD,
  LOG_PERF_CMD,
};

static void
do_log_config (int cmd, int val)
{
#define SET_LOGGING(F, P, C)                                            \
  do { options.do_log=(F); alog.perf_only=(P); alog.countdown=(C); } while(0)

  // Turning logging on / off
  switch (cmd)
    {
    case LOG_ON_CMD:
      log_append ("log On");
      SET_LOGGING (1, 0, 0);
      break;
    case LOG_OFF_CMD:
      log_append ("log Off");
      SET_LOGGING (0, 0, 0);
      break;
    case LOG_PERF_CMD:
      log_append ("performance log");
      SET_LOGGING (0, 1, 0);
      break;
    case LOG_SET_CMD:
      alog.count_val = val ? val : 0x10000;
      log_append ("start log %u", alog.count_val);
      SET_LOGGING (1, 0, 1 + alog.count_val);
      break;
    }

#undef SET_LOGGING
}


static void
do_perf_cmd (int x)
{
  const char *s = "";
  int n = PERF_N (x);
  int cmd = PERF_CMD(x);

  switch (cmd)
    {
    case PERF_START_CMD: s = "start"; break;
    case PERF_STOP_CMD:  s = "stop"; break;
    case PERF_DUMP_CMD:  s = "dump"; break;
    case PERF_STAT_U32_CMD:   s = "stat u32"; break;
    case PERF_STAT_S32_CMD:   s = "stat s32"; break;
    case PERF_STAT_FLOAT_CMD: s = "stat float"; break;
    case PERF_START_CALL_CMD: s = "start on call"; break;
    }

  if (n) log_append ("PERF %d %s", n, s);
  else   log_append ("PERF all %s", s);

  // Do perf-meter stuff only in avrtest*_log in order
  // to avoid impact on execution speed.
  perf.cmd[cmd] = n ? 1 << n : PERF_ALL;
  perf.will_be_on = perf.cmd[PERF_START_CMD]
                    || perf.cmd[PERF_START_CALL_CMD];
}


static void
do_perf_tag_cmd (int x)
{
  // PERF_TAG_[P]FMT gets the format string, then
  // PERF_TAG_XXX uses that format on a specific perf-meter

  perfs_t *p = & perfs[PERF_N (x)];
  int cmd = 0, tag_cmd = PERF_TAG_CMD (x);
  const char *s = "";

  switch (tag_cmd)
    {
    case PERF_TAG_STR_CMD: s = "_TAG string";  cmd = LOG_STR_CMD;   break;
    case PERF_TAG_U16_CMD:   s = "_TAG u16";   cmd = LOG_U16_CMD;   break;
    case PERF_TAG_U32_CMD:   s = "_TAG u32";   cmd = LOG_U32_CMD;   break;
    case PERF_TAG_FLOAT_CMD: s = "_TAG float"; cmd = LOG_FLOAT_CMD; break;
    case PERF_LABEL_CMD:     s = " label";     cmd = LOG_STR_CMD;   break;
    case PERF_PLABEL_CMD:    s = " plabel";    cmd = LOG_PSTR_CMD;  break;
    case PERF_TAG_FMT_CMD:   s = " fmt";       cmd = LOG_STR_CMD;   break;
    case PERF_TAG_PFMT_CMD:  s = " pfmt";      cmd = LOG_PSTR_CMD;  break;
    }
  log_append ("PERF%s %d", s, PERF_N (x));

  const layout_t *lay = & layout[cmd];
  unsigned raw = get_r20_value (lay);

  switch (tag_cmd)
    {
    case PERF_TAG_FMT_CMD:
    case PERF_TAG_PFMT_CMD:
      perf.pending_LOG_TAG_FMT = 1;
      read_string (perfs[0].tag.fmt, raw, lay->in_rom,
                   sizeof (perfs[0].tag.fmt));
      return;

    case PERF_LABEL_CMD:
    case PERF_PLABEL_CMD:
      if (raw)
        read_string (p->label, raw, lay->in_rom, sizeof (p->label));
      else
        * p->label = '\0';
      return;
    }

  perf_tag_t *t = & p->tag_for_start;

  t->cmd = cmd;
  t->val = raw;

  if (cmd == LOG_STR_CMD)
    read_string (t->string, t->val, 0, sizeof (t->string));
  else if (cmd == LOG_FLOAT_CMD)
    t->dval = decode_avr_float (t->val).x;

  if (perf.pending_LOG_TAG_FMT)
    strcpy (t->fmt, perfs[0].tag.fmt);
  else
    * t->fmt = '\0';

  perf.pending_LOG_TAG_FMT = 0;
}


void
do_syscall (int sysno, int val)
{
  switch (sysno)
    {
    default:
      log_append ("void ");
      qprintf ("*** syscall %d: void\n", sysno);
      break;

    case 0: do_log_config (LOG_OFF_CMD, 0);    break;
    case 1: do_log_config (LOG_ON_CMD, 0);     break;
    case 2: do_log_config (LOG_PERF_CMD, 0);   break;
    case 3: do_log_config (LOG_SET_CMD, val);  break;
    case 4: do_ticks_cmd (val);    break;
    case 5: do_perf_cmd (val);     break;
    case 6: do_perf_tag_cmd (val); break;
    case 7: do_log_dump (val);     break;
    case 8: do_get_ticks (); break;
    }
}


static int
print_tag (const perf_tag_t *t, const char *no_tag, const char *tag_prefix)
{
  if (t->cmd < 0)
    return printf (no_tag);

  printf ("%s", tag_prefix);

  const char *fmt = *t->fmt ? t->fmt : layout[t->cmd].fmt;

  if (t->cmd == LOG_STR_CMD)
    return printf (fmt, t->string);
  else if (t->cmd == LOG_FLOAT_CMD)
    return printf (fmt, t->dval);
  else
    return printf (fmt, t->val);
}

static int
print_tags (const minmax_t *mm, const char *text)
{
  int pos;
  printf ("%s", text);
  if (mm->r_min == mm->r_max)
    return printf ("     -all-same-                          /\n");

  printf ("%9d %9d", mm->r_min, mm->r_max);
  pos = print_tag (& mm->tag_min, "    -no-tag-         ", "    ");
  printf ("%*s", pos >= 20 ? 0 : 20 - pos, " / ");
  print_tag (& mm->tag_max, "  -no-tag-", "   ");
  return printf ("\n");
}


static INLINE void
minmax_update (minmax_t *mm, long x, const perfs_t *p)
{
  if (x < mm->min && p->tag.cmd >= 0) mm->tag_min = p->tag;
  if (x > mm->max && p->tag.cmd >= 0) mm->tag_max = p->tag;
  if (x < mm->min) { mm->min = x; mm->min_at = perf.pc; mm->r_min = p->n; }
  if (x > mm->max) { mm->max = x; mm->max_at = perf.pc; mm->r_max = p->n; }
}

static INLINE void
minmax_update_double (minmax_t *mm, double x, const perfs_t *p)
{
  if (x < mm->dmin && p->tag.cmd >= 0) mm->tag_min = p->tag;
  if (x > mm->dmax && p->tag.cmd >= 0) mm->tag_max = p->tag;
  if (x < mm->dmin) { mm->dmin = x; mm->min_at = perf.pc; mm->r_min = p->n; }
  if (x > mm->dmax) { mm->dmax = x; mm->max_at = perf.pc; mm->r_max = p->n; }
}

static INLINE void
minmax_init (minmax_t *mm, long at_start)
{
  mm->min = LONG_MAX;
  mm->max = LONG_MIN;
  mm->at_start = at_start;
  mm->tag_min.cmd = mm->tag_max.cmd = -1;
  mm->dmin = HUGE_VAL;
  mm->dmax = -HUGE_VAL;
  mm->ev2 = 0.0;
}

static int
perf_verbose_start (perfs_t *p, int i, int mode)
{
  qprintf ("\n--- ");

  if (!p->valid)
    {
      if (PERF_START_CMD == mode)
        qprintf ("Start T%d (round 1", i);
    }
  else if (PERF_START_CMD == mode)
    {
      if (PERF_STAT_CMD == p->valid)
        qprintf ("Start T%d ignored: T%d in Stat mode (%d values",
                 i, i, p->n);
      else if (p->on)
        qprintf ("Start T%d ignored: T%d already started (round %d",
                 i, i, p->n);
      else
        qprintf ("reStart T%d (round %d", i, 1 + p->n);
    }
  else if (PERF_START_CMD == p->valid)
    qprintf ("Stat T%d ignored: T%d is in Start/Stop mode (%s "
             "round %d", i, i, p->on ? "in" : "after", p->n);

  // Finishing --- message is perf_stat() resp. perf_start()

  return (!p->valid
          || (mode >= PERF_STAT_CMD
              && p->valid == PERF_STAT_CMD)
          || (mode == PERF_START_CMD
              && p->valid == PERF_START_CMD && !p->on));
}

// PERF_STAT_XXX (i, x)
static void
perf_stat (perfs_t *p, int i, int stat)
{
  if (p->tag_for_start.cmd >= 0)
    p->tag = p->tag_for_start;
  else
    p->tag.cmd = -1;
  p->tag_for_start.cmd = -1;

  if (!p->valid)
    {
      // First value
      p->valid = PERF_STAT_CMD;
      p->on = p->n = 0;
      p->val_ev = 0.0;
      minmax_init (& p->val, 0);
    }

  double dval;
  signed sraw = get_r20_value (& layout[LOG_S32_CMD]);
  unsigned uraw = (unsigned) sraw & 0xffffffff;
  if (PERF_STAT_U32_CMD == stat)
    dval = (double) uraw;
  else if (PERF_STAT_S32_CMD == stat)
    dval = (double) sraw;
  else
    dval = decode_avr_float (uraw).x;

  p->n++;
  minmax_update_double (& p->val, dval, p);
  p->val.ev2 += dval * dval;
  p->val_ev += dval;

  if (!options.do_quiet)
    {
      qprintf ("Stat T%d (value %d = %e", i, p->n, dval);
      print_tag (& p->tag, "", ", ");
      qprintf (")\n");
    }
}

// PERF_START (i)
static void
perf_start (perfs_t *p, int i)
{
  {
    if (p->tag_for_start.cmd >= 0)
      p->tag = p->tag_for_start;
    else
      p->tag.cmd = -1;
    p->tag_for_start.cmd = -1;
  }

  if (!p->valid)
    {
      // First round begins
      p->valid = PERF_START_CMD;
      p->n = 0;
      p->insns = p->ticks = 0;
      minmax_init (& p->insn,  program.n_insns);
      minmax_init (& p->tick,  program.n_cycles);
      minmax_init (& p->calls, perf.calls);
      minmax_init (& p->sp,    perf.sp);
      minmax_init (& p->pc,    p->pc_start = cpu_PC);
    }

  // (Re)start
  p->on = 1;
  // Overridden by caller for PERF_START_CALL
  p->call_only.sp = INT_MAX;
  p->call_only.insns = 0;
  p->call_only.ticks = 0;
  p->n++;
  p->insn.at_start = program.n_insns;
  p->tick.at_start = program.n_cycles;

  if (!options.do_quiet)
    {
      print_tag (& p->tag, "", ", ");
      qprintf (")\n");
    }
}


// PERF_STOP (i)
static void
perf_stop (perfs_t *p, int i, int dumps, int dump, int sp)
{
  if (!dump)
    {
      int ret = 1;
      if (!p->valid)
        qprintf ("\n--- Stop T%d ignored: -unused-\n", i);
      else if (p->valid == PERF_START_CMD && !p->on)
        qprintf ("\n--- Stop T%d ignored: T%d already stopped (after "
                 "round %d)\n", i, i, p->n);
      else if (p->valid == PERF_STAT_CMD)
        qprintf ("\n--- Stop T%d ignored: T%d used for Stat (%d Values)\n",
                 i, i, p->n);
      else
        ret = 0;

      if (ret)
        return;
    }

  if (p->valid == PERF_START_CMD && p->on)
    {
      long ticks;
      int insns;
      p->on = 0;
      p->pc.at_end = p->pc_end = perf.pc2;
      p->insn.at_end = program.n_insns -1;
      p->tick.at_end = perf.tick;
      p->calls.at_end = perf.calls;
      p->sp.at_end = sp;
      if (p->call_only.sp == LONG_MAX)
        {
          ticks = p->tick.at_end - p->tick.at_start;
          insns = p->insn.at_end - p->insn.at_start;
        }
      else
        {
          ticks = p->call_only.ticks;
          insns = p->call_only.insns;
        }
      p->tick.ev2 += (double) ticks * ticks;
      p->insn.ev2 += (double) insns * insns;
      p->ticks += ticks;
      p->insns += insns;
      minmax_update (& p->insn, insns, p);
      minmax_update (& p->tick, ticks, p);

      qprintf ("%sStop T%d (round %d",
               dumps == PERF_ALL ? "  " : "\n--- ", i, p->n);
      if (!options.do_quiet)
        print_tag (& p->tag, "", ", ");

      qprintf (", %04lx--%04lx, %ld Ticks)\n",
               2 * p->pc.at_start, 2 * p->pc.at_end, ticks);
    }
}


// PERF_DUMP (i)
static void
perf_dump (perfs_t *p, int i, int dumps)
{
  if (!p->valid)
    {
      if (dumps != PERF_ALL)
        printf (" Timer T%d \"%s\": -unused-\n\n", i, p->label);
      return;
    }

  long c = p->calls.at_start;
  long s = p->sp.at_start;
  if (p->valid == PERF_START_CMD)
    printf (" Timer T%d \"%s\" (%d round%s):  %04x--%04x\n"
            "              Instructions        Ticks\n"
            "    Total:      %7u"  "         %7u\n",
            i, p->label, p->n, p->n == 1 ? "" : "s",
            2 * p->pc_start, 2 * p->pc_end, p->insns, p->ticks);
  else
    printf (" Stat  T%d \"%s\" (%d Value%s)\n",
            i, p->label, p->n, p->n == 1 ? "" : "s");

  double e_x2, e_x;

  if (p->valid == PERF_START_CMD)
    {
      if (p->n > 1)
        {
          // Var(X) = E(X^2) - E^2(X)
          e_x2 = p->tick.ev2 / p->n; e_x = (double) p->ticks / p->n;
          double tick_sigma = sqrt (e_x2 - e_x*e_x);
          e_x2 = p->insn.ev2 / p->n; e_x = (double) p->insns / p->n;
          double insn_sigma = sqrt (e_x2 - e_x*e_x);

          printf ("    Mean:       %7d"  "         %7d\n"
                  "    Stand.Dev:  %7.1f""         %7.1f\n"
                  "    Min:        %7ld" "         %7ld\n"
                  "    Max:        %7ld" "         %7ld\n",
                  p->insns / p->n, p->ticks / p->n, insn_sigma, tick_sigma,
                  p->insn.min, p->tick.min, p->insn.max, p->tick.max);
        }

      printf ("    Calls (abs) in [%4ld,%4ld] was:%4ld now:%4ld\n"
              "    Calls (rel) in [%4ld,%4ld] was:%4ld now:%4ld\n"
              "    Stack (abs) in [%04lx,%04lx] was:%04lx now:%04lx\n"
              "    Stack (rel) in [%4ld,%4ld] was:%4ld now:%4ld\n",
              p->calls.min,   p->calls.max,     c, p->calls.at_end,
              p->calls.min-c, p->calls.max-c, c-c, p->calls.at_end-c,
              p->sp.max,      p->sp.min,        s, p->sp.at_end,
              s-p->sp.max,    s-p->sp.min,    s-s, s-p->sp.at_end);
      if (p->n > 1)
        {
          printf ("\n           Min round Max round    "
                  "Min tag           /   Max tag\n");
          print_tags (& p->calls, "    Calls  ");
          print_tags (& p->sp,    "    Stack  ");
          print_tags (& p->insn,  "    Instr. ");
          print_tags (& p->tick,  "    Ticks  ");
        }
    }
  else /* PERF_STAT */
    {
      e_x2 = p->val.ev2 / p->n;
      e_x =  p->val_ev  / p->n;
      double val_sigma = sqrt (e_x2 - e_x*e_x);
      printf ("    Mean:       %e     round    tag\n"
              "    Stand.Dev:  %e\n", e_x, val_sigma);
      printf ("    Min:        %e  %8d", p->val.dmin, p->val.r_min);
      print_tag (& p->val.tag_min, " -no-tag-", "    ");
      printf ("\n"
              "    Max:        %e  %8d", p->val.dmax, p->val.r_max);
      print_tag (& p->val.tag_max, " -no-tag-", "    ");
      printf ("\n");
    }

  printf ("\n");

  p->valid = 0;
  * p->label = '\0';
}


static void
perf_instruction (int id)
{
  perf.id = id;
  perf.will_be_on = 0;

  // actions requested by perf SYSCALLs 5..6

  int dumps  = perf.cmd[PERF_DUMP_CMD];
  int starts = perf.cmd[PERF_START_CMD];
  int stops  = perf.cmd[PERF_STOP_CMD];
  int starts_call = perf.cmd[PERF_START_CALL_CMD];
  int stats_u32   = perf.cmd[PERF_STAT_U32_CMD];
  int stats_s32   = perf.cmd[PERF_STAT_S32_CMD];
  int stats_float = perf.cmd[PERF_STAT_FLOAT_CMD];
  int stats = stats_u32 | stats_s32 | stats_float;

  byte *psp = log_cpu_address (0, AR_SP);
  int sp = psp[0] | (psp[1] << 8);
  int cmd = starts || starts_call || stops || dumps || stats;

  if (!perf.on && !cmd)
    goto done;

  perf.on = 0;

  if (dumps)
    printf ("\n--- Dump # %d:\n", ++perf.n_dumps);

  for (int i = 1; i < NUM_PERFS; i++)
    {
      perfs_t *p = &perfs[i];
      int start = ((starts | starts_call) & (1 << i)) ? PERF_START_CMD : 0;
      int stop  = stops & (1 << i);
      int dump  = dumps & (1 << i);
      int start_call = starts_call  & (1 << i);
      int stat_u32   = (stats_u32   & (1 << i)) ? PERF_STAT_U32_CMD : 0;
      int stat_s32   = (stats_s32   & (1 << i)) ? PERF_STAT_S32_CMD : 0;
      int stat_float = (stats_float & (1 << i)) ? PERF_STAT_FLOAT_CMD : 0;
      int stat_val = stat_u32 | stat_s32 | stat_float;

      if (p->on
          // PERF_START_CALL:  Only account costs (including CALL+RET)
          // if we have a call depth > 0 relative to the starting point.
          && p->call_only.sp < INT_MAX
          && (sp < p->call_only.sp || perf.sp < p->call_only.sp))
        {
          p->call_only.insns += 1;
          p->call_only.ticks += program.n_cycles - perf.tick;
        }

      if (stop | dump)
        perf_stop (p, i, dumps, dump, sp);

      if (dump)
        perf_dump (p, i, dumps);

      if (p->on)
        {
          minmax_update (& p->sp, sp, p);
          minmax_update (& p->calls, perf.calls, p);
        }

      if (start && perf_verbose_start (p, i, start))
        {
          perf_start (p, i);
          if (start_call)
            p->call_only.sp = sp;
        }
      else if (stat_val && perf_verbose_start (p, i, stat_val))
        perf_stat (p, i, stat_val);

      perf.on |= p->on;
    }

  memset (perf.cmd, 0, sizeof (perf.cmd));
 done:;
  // Store for the next call of ours.  Needed because log_dump_line()
  // must run after the instruction has performed and we might need
  // the values from before the instruction.
  perf.sp  = sp;
  perf.pc2 = perf.pc;
  perf.pc  = cpu_PC;
  perf.tick = program.n_cycles;
}


void
log_init (unsigned val)
{
  alog.pos = alog.data;
  alog.maybe_log = 1;

  for (int i = 1; i < NUM_PERFS; i++)
    perfs[i].tag_for_start.cmd = -1;

  srand (val);
}


void
log_dump_line (int id)
{
  if (id && alog.countdown && --alog.countdown == 0)
    {
      options.do_log = 0;
      qprintf ("*** done log %u\n", alog.count_val);
    }

  int log_this = (options.do_log
                  || (alog.perf_only
                      && (perf.on || perf.will_be_on)));
  if (log_this || (log_this != alog.log_this))
    {
      alog.maybe_log = 1;
      puts (alog.data);
      if (log_this && alog.unused)
        leave (EXIT_STATUS_FATAL, "problem in log_dump_line");
    }
  else
    alog.maybe_log = 0;

  alog.log_this = log_this;

  alog.pos = alog.data;
  *alog.pos = '\0';
  perf_instruction (id);
}
