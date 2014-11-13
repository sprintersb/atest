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
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "sreg.h"
#include "graph.h"
#include "perf.h"
#include "logging.h"

// ports used for application <-> simulator interactions
#define IN_AVRTEST
#include "avrtest.h"

#ifndef AVRTEST_LOG
#error no function herein is needed without AVRTEST_LOG
#endif // AVRTEST_LOG

typedef struct
{
  // Buffer filled by log_append() and flushed to stdout after the
  // instruction has been executed (provided logging is on).
  char data[256];
  // Write position in .data[].
  char *pos;
  // LOG_SET(N): Log the next f(N) instructions
  unsigned count_val;
  // LOG_SET(N): Value count down to 0 and then stop logging.
  unsigned countdown;
  // ID of current instruction.
  int id;
  // Instruction might turn on logging, thus log it even if logging is
  // (still) off.  Only SYSCALLs can start logging.
  bool log_this;
  // No log needed for the current instruction; dont' add stuff to .data[]
  // in order to speed up matters if logging is (temporarily) off.
  //int unused;
  bool maybe_log;
  // Whether this instruction has been logged
  // LOG_PERF: Just log when at least one perf-meter is on
  bool perf_only;
} alog_t;

const char s_SREG[] = "CZNVSHTI";


#define LEN_LOG_STRING      500
#define LEN_LOG_XFMT        500

unsigned old_PC, old_old_PC;
bool log_unused;
need_t need;

string_table_t string_table;

static alog_t alog;

void
log_append (const char *fmt, ...)
{
  if (log_unused)
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


/* Add a symbol; called by ELF loader.

   ADDR  is the value of the symbol.
   STOFF is the offset into the string table string_table.data[].
   According to ELF, STOFF is non-null as the first char in
   the string table is always '\0'.
   IS_FUNC is 1 if the symbol table type is STT_FUNC,
   0 otherwise. */

void
log_set_func_symbol (int addr, size_t stoff, bool is_func)
{
  string_table_t *stab = & string_table;

  if (!stab->data)
    leave (LEAVE_FATAL, "symbol table is NULL");

  const char *name = stab->data + stoff;

  if (is_func
      && (addr % 2 != 0
          || addr >= MAX_FLASH_SIZE))
    leave (LEAVE_ABORTED, "'%s': bad symbol at 0x%x", name, addr);

  if (addr % 2 != 0
      // Something weird, maybe orphan etc.
      || addr >= MAX_FLASH_SIZE
      // Ignore internal labels
      || name[0] == '.')
    {
      stab->n_bad++;
      return;
    }

  graph_elf_symbol (name, stoff, addr / 2, is_func);

  stab->n_funcs += is_func;
  stab->n_vec += !is_func && str_prefix ("__vector_", name);
  stab->n_strings ++;
}


void
log_set_string_table (char *stab, size_t size, int n_entries)
{
  string_table_t *s = & string_table;

  s->data = stab;
  s->size = size;
  s->n_entries = n_entries;

  graph_set_string_table (stab, size, n_entries);
}


void
log_finish_string_table (void)
{
  string_table_t *s = & string_table;

  if (options.do_verbose)
    printf (">>> strtab[%u] %d entries, %d usable, %d functions, %d other, "
            "%d bad, %d unused vectors\n", 0U + s->size, s->n_entries,
            s->n_strings, s->n_funcs, s->n_strings - s->n_funcs, s->n_bad,
            s->n_vec);

  graph_finish_string_table ();
}


void
log_add_instr (const decoded_t *d)
{
  alog.id = d->id;
  old_old_PC = old_PC;
  old_PC = cpu_PC;

  char mnemo_[16];
  const char *fmt, *mnemo = opcodes[alog.id].mnemonic;

  // SYSCALL 0..3 might turn on logging: always log them to alog.data[].

  bool maybe_used = (alog.maybe_log
                     || (alog.id == ID_SYSCALL && d->op1 <= 3));

  if ((log_unused = !maybe_used || !need.logging))
    return;

  if (alog.id == ID_UNDEF)
    {
      log_append (arch.pc_3bytes ? "%06x: " : "%04x: ", cpu_PC * 2);
      return;
    }
  
  strcpy (mnemo_, mnemo);
  log_patch_mnemo (d, mnemo_ + strlen (mnemo));
  fmt = arch.pc_3bytes ? "%06x: %-7s " : "%04x: %-7s ";
  log_append (fmt, cpu_PC * 2, mnemo_);
}


void
log_add_flag_read (int mask, int value)
{
  if (log_unused)
    return;

  int bit = mask_to_bit (mask);
  log_append (" %c->%c", s_SREG[bit], '0' + !!value);
}


void
log_add_data_mov (const char *format, int addr, int value)
{
  if (log_unused)
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

  for (const sfr_t *sfr = named_sfr; ; sfr++)
    {
      if (addr == sfr->addr
          && (sfr->pon == NULL || *sfr->pon))
        s_name = sfr->name;
      else if (sfr->name == NULL)
        sprintf (name, addr < 256 ? "%02x" : "%04x", addr);
      else
        continue;
      break;
    }

  log_append (format, s_name, value);
}


// IEEE 754 single
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


/* Copy a string from AVR target to the host, but not more than
   LEN_MAX characters.  */

char*
read_string (char *p, unsigned addr, bool flash_p, size_t len_max)
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


/* Read a value as unsigned from R20.  Bytesize (1..4) and signedness are
   determined by respective layout[].  If the value is signed a cast to
   signed will do the conversion.  */

unsigned
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


typedef struct
{
  // Offset set by RESET.
  dword n_insns;
  dword n_cycles;
  // Current value for PRAND mode
  uint32_t pvalue;
} ticks_port_t;


static void
sys_ticks_cmd (int cfg)
{
  static ticks_port_t ticks_port;

  // a prime m
  const uint32_t prand_m = 0xfffffffb;
  // phi (m)
  // uint32_t prand_phi_m = m-1; // = 2 * 5 * 19 * 22605091
  // a primitive root of (Z/m*Z)^*
  const uint32_t prand_root = 0xcafebabe;

  cfg &= 0xff;
  ticks_port_t *tp = &ticks_port;

  if (cfg & TICKS_RESET_ALL_CMD)
    {
      log_append ("ticks reset:");
      if (cfg & TICKS_RESET_CYCLES_CMD)
        {
          log_append (" cycles");
          tp->n_cycles = program.n_cycles;
        }
      if (cfg & TICKS_RESET_INSNS_CMD)
        {
          log_append (" insns");
          tp->n_insns = program.n_insns;
        }
      if (cfg & TICKS_RESET_PRAND_CMD)
        {
          log_append (" prand");
          tp->pvalue = 0;
        }
      return;
    }

  const char *what = "???";
  uint32_t value = 0;

  switch (cfg)
    {
    case TICKS_GET_CYCLES_CMD:
      what = "cycles";
      value = program.n_cycles - tp->n_cycles;
      break;
    case TICKS_GET_INSNS_CMD:
      what = "insn";
      value = program.n_insns - tp->n_insns;
      break;
    case TICKS_GET_PRAND_CMD:
      what = "prand";
      value = tp->pvalue ? tp->pvalue : 1;
      value = ((uint64_t) value * prand_root) % prand_m;
      tp->pvalue = value;
      break;
    case TICKS_GET_RAND_CMD:
      what = "rand";
      value = rand();
      value ^= (unsigned) rand() << 11;
      value ^= (unsigned) rand() << 22;
      break;
    }

  log_append ("ticks get %s: R22<-(%08x) = %u", what, value, value);
  byte *p = log_cpu_address (22, AR_REG);

  *p++ = value;
  *p++ = value >> 8;
  *p++ = value >> 16;
  *p++ = value >> 24;
}


static void
sys_log_dump (int what)
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
sys_log_config (int cmd, int val)
{
#define SET_LOGGING(F, P, C)                                            \
  do { options.do_log=(F); alog.perf_only=(P); alog.countdown=(C); } while(0)

  // Turning logging on / off
  switch (cmd)
    {
    case LOG_ON_CMD:
      log_append ("log On");
      SET_LOGGING (1, false, 0);
      break;
    case LOG_OFF_CMD:
      log_append ("log Off");
      SET_LOGGING (0, false, 0);
      break;
    case LOG_PERF_CMD:
      log_append ("performance log");
      SET_LOGGING (0, true, 0);
      break;
    case LOG_SET_CMD:
      alog.count_val = val ? val : 0x10000;
      log_append ("start log %u", alog.count_val);
      SET_LOGGING (1, false, 1 + alog.count_val);
      break;
    }

#undef SET_LOGGING
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

    case 0: sys_log_config (LOG_OFF_CMD, 0);    break;
    case 1: sys_log_config (LOG_ON_CMD, 0);     break;
    case 2: sys_log_config (LOG_PERF_CMD, 0);   break;
    case 3: sys_log_config (LOG_SET_CMD, val);  break;
    case 4: sys_ticks_cmd (val);    break;
    case 5: sys_perf_cmd (val);     break;
    case 6: sys_perf_tag_cmd (val); break;
    case 7: sys_log_dump (val);     break;
    }
}


void
log_init (unsigned val)
{
  perf_init();

  alog.pos = alog.data;
  alog.maybe_log = true;
  srand (val);

  /**/

  need.perf = have_syscall[5] || have_syscall[6];
  
  need.logging = (is_avrtest_log
                  && (options.do_log
                      || have_syscall[1]
                      || (have_syscall[2] && need.perf)
                      || have_syscall[3]));

  need.graph_cost = options.do_graph || options.do_debug_tree;

  need.call_depth = need.graph_cost || need.logging || need.perf;
  need.graph = need.call_depth;
}


void
log_dump_line (const decoded_t *d)
{
  if (d && alog.countdown && --alog.countdown == 0)
    {
      options.do_log = 0;
      qprintf ("*** done log %u\n", alog.count_val);
    }

  bool log_this = (options.do_log
                   || (alog.perf_only
                       && (perf.on || perf.will_be_on)));
  if (log_this || (log_this != alog.log_this))
    {
      alog.maybe_log = true;
      puts (alog.data);
      if (log_this && log_unused)
        leave (LEAVE_FATAL, "problem in log_dump_line");
    }
  else
    alog.maybe_log = false;

  alog.log_this = log_this;

  alog.pos = alog.data;
  *alog.pos = '\0';

  int call_depth = (d && need.call_depth
                    ? graph_update_call_depth (d)
                    : 0);

  if (!d && options.do_graph)
    graph_write_dot();

  if (need.perf)
    perf_instruction (d ? d->id : 0, call_depth);
}


const layout_t
layout[LOG_X_sentinel] =
  {
    [LOG_STR_CMD]   = { 2, "%s",       false, false },
    [LOG_PSTR_CMD]  = { 2, "%s",       false, true  },
    [LOG_ADDR_CMD]  = { 2, " 0x%04x ", false, false },

    [LOG_FLOAT_CMD] = { 4, " %.6f ", false, false },
    
    [LOG_U8_CMD]  = { 1, " %u ", false, false },
    [LOG_U16_CMD] = { 2, " %u ", false, false },
    [LOG_U24_CMD] = { 3, " %u ", false, false },
    [LOG_U32_CMD] = { 4, " %u ", false, false },
    
    [LOG_S8_CMD]  = { 1, " %d ", true,  false },
    [LOG_S16_CMD] = { 2, " %d ", true,  false },
    [LOG_S24_CMD] = { 3, " %d ", true,  false },
    [LOG_S32_CMD] = { 4, " %d ", true,  false },

    [LOG_X8_CMD]  = { 1, " 0x%02x ", false, false },
    [LOG_X16_CMD] = { 2, " 0x%04x ", false, false },
    [LOG_X24_CMD] = { 3, " 0x%06x ", false, false },
    [LOG_X32_CMD] = { 4, " 0x%08x ", false, false },

    [LOG_UNSET_FMT_CMD]     = { 0, NULL, false, false },
    [LOG_SET_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_SET_PFMT_CMD]      = { 2, NULL, false, true  },
    [LOG_SET_FMT_ONCE_CMD]  = { 2, NULL, false, false },
    [LOG_SET_PFMT_ONCE_CMD] = { 2, NULL, false, true  },

    [LOG_TAG_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_TAG_PFMT_CMD]      = { 2, NULL, false, true  },
  };
