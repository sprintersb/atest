/*
  This file is part of avrtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2024 Free Software Foundation, Inc.

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

#include "testavr.h"
#include "options.h"
#include "sreg.h"
#include "graph.h"
#include "perf.h"
#include "logging.h"
#include "host.h"

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


unsigned old_PC, old_old_PC;
bool log_unused;
need_t need;
static int maybe_SP_glitch;

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

void
log_append_va (const char *fmt, va_list args)
{
  if (log_unused)
    return;

  alog.pos += vsprintf (alog.pos, fmt, args);
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
    case ID_LDD_Y:  case ID_STD_Y:
    case ID_LDD_Z:  case ID_STD_Z:
      if (is_tiny)
        {
          buf[-4] = buf[-3];
          buf[-5] = buf[-3] = buf[-2] = buf[-1] = ' ';
        }
      return;
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


/* When measuring performance and track min / max SP values,
   then changing SP by OUT may lead to a glitch just like when
   an IRQ occurred in the middle of the SP adjustment.
   Therefore, flag that SP might contain garbage.  */

void log_maybe_change_SP (int address)
{
  if (address == addr_SPL || address == addr_SPL + 1)
    maybe_SP_glitch = 4;
}


int get_nonglitch_SP (void)
{
  static int nonglitch_SP;

  if (!maybe_SP_glitch)
    nonglitch_SP = pSP[0] | (pSP[1] << 8);

  return nonglitch_SP;
}


/* Add a symbol; called by ELF loader.

   ADDR  is the value of the symbol.
   STOFF is the offset into the string table string_table.data[].
   According to ELF, STOFF is non-null as the first char in
   the string table is always '\0'.
   IS_FUNC is 1 if the symbol table type is STT_FUNC, 0 otherwise.  */

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
    leave (LEAVE_SYMBOL, "'%s': bad symbol at 0x%x", name, addr);

  // ??? Some newer Binutils GAS version introduce an arificial
  // label after "RCALL .+0" that contains non-printable character
  // like "L0^A".  According to Binutils documentation "Local Labels" at
  // https://sourceware.org/binutils/docs-2.40/as/Symbol-Names.html
  // such a label would result from "L0$".  Ignore them.
  bool nonprint = name[0] && name[1] && name[2] < 0x20;

  if (addr % 2 != 0
      // Something weird, maybe orphan etc.
      || addr >= MAX_FLASH_SIZE
      // Ignore internal labels
      || name[0] == '.'
      || nonprint)
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
    printf (">>> strtab[%zu] %d entries, %d usable, %d functions, %d other, "
            "%d bad, %d unused vectors\n", s->size, s->n_entries,
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

  // We are called by do_step() for each instruction.  Decrement our
  // SP "atomicy" device.
  if (maybe_SP_glitch)
    {
      maybe_SP_glitch--;

      // Some instructions immediately end a glitch because they won't be
      // used during an explicit SP adjustment.  IJMP is usually from longjmp
      // or from __prologue_saves__; RET is from __epilogue_restores__.
      if (ID_RET == d->id || ID_IJMP == d->id || ID_EIJMP == d->id
          || ID_RCALL == d->id || ID_CALL == d->id
          || ID_PUSH == d->id || ID_POP == d->id)
        maybe_SP_glitch = 0;
    }

  char mnemo_[16];
  const char *mnemo = opcodes[alog.id].mnemonic;

  // SYSCALL 0..3, 5, 10..11 might turn on logging:
  // always log them to alog.data[].

  unsigned sysmask = 0xf | (1 << 5) | (1 << 10) | (1 << 11);
  bool maybe_used = (alog.maybe_log
                     || (alog.id == ID_SYSCALL && (sysmask & (1u << d->op1))));
  int pc_strlen = arch.flash_addr_mask > 0xffff ? 6 : 4;

  if ((log_unused = !maybe_used || !need.logging))
    return;

  if (alog.id == ID_UNDEF)
    {
      log_append ("%0*x: ", pc_strlen, cpu_PC * 2);
      return;
    }

  strcpy (mnemo_, mnemo);
  log_patch_mnemo (d, mnemo_ + strlen (mnemo));
  log_append ("%0*x: %-7s ", pc_strlen, cpu_PC * 2, mnemo_);
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
        if (addr >= 0x10000 && arch.has_rampd)
          sprintf (name, "%02x:%04x", addr >> 16, addr & 0xffff);
        else
          sprintf (name, addr < 256 ? "%02x" : "%04x", addr);
      else
        continue;
      break;
    }

  log_append (format, s_name, value);
}


enum
  {
    LOG_ON_CMD,
    LOG_OFF_CMD,
    LOG_SET_CMD,
    LOG_PERF_CMD,
  };

static void log_set_logging (bool on, bool on_perf, unsigned cntdwn)
{
  options.do_log = on;
  alog.perf_only = on_perf;
  alog.countdown = cntdwn;
}

static void
sys_log_config (int cmd, int val)
{
  // Turning logging on / off
  switch (cmd)
    {
    case LOG_ON_CMD:
      log_append ("log On");
      log_set_logging (1, false, 0);
      break;
    case LOG_OFF_CMD:
      log_append ("log Off");
      log_set_logging (0, false, 0);
      break;
    case LOG_PERF_CMD:
      log_append ("performance log");
      log_set_logging (0, true, 0);
      break;
    case LOG_SET_CMD:
      alog.count_val = val ? val : 0x10000;
      log_append ("start log %u", alog.count_val);
      log_set_logging (1, false, 1 + alog.count_val);
      break;
    }
}

static const char*
pc_string (void)
{
  static char str[20];
  sprintf (str, arch.pc_3bytes ? "%06x" : "%04x", cpu_PC * 2);
  return str;
}

typedef struct
{
  bool on;
  bool perf;
  unsigned count_val;
  unsigned countdown;
} log_stack_t;


static void
sys_log_pushpop (int sysno, int what)
{
  static log_stack_t stack[100];
  static log_stack_t *sp = stack;

  size_t n_slots = sizeof (stack) / sizeof (*stack);

  if (what == 0 || what == 1)
    {
      log_append ("log push %s", what ? "On" : "Off");

      if (sp < stack  + n_slots - 1)
        {
          log_stack_t slot = { !!options.do_log, alog.perf_only,
                               alog.count_val, alog.countdown };
          *sp++ = slot;

          log_append (" #%d", (int) (sp - stack));

          if (slot.perf)
            log_append (" (perf)");

          if (slot.on && slot.countdown)
            log_append (" (%u / %u) ", slot.countdown, slot.count_val);

          log_set_logging (what, 0 /* perf */, 0 /* countdown */);
          alog.count_val = 0;
        }
      else
        {
          log_append (" (stack #%u overflow)", (unsigned) n_slots);
          if (! options.do_log)
            qprintf ("*** syscall #%d 0x%s: log push (stack #%u overflow)\n",
                     sysno, pc_string(), (unsigned) n_slots);
        }
    }
  else if (what == -1)
    {
      log_append ("log pop ");

      if (sp > stack)
        {
          log_stack_t slot = * --sp;
          log_append ("%s #%d", slot.on ? "On" : "Off", (int) (sp + 1 - stack));

          alog.count_val = slot.count_val;
          log_set_logging (slot.on, slot.perf, slot.countdown);

          if (slot.perf)
            log_append (" (perf)");

          if (slot.on && slot.countdown)
            log_append (" (%u / %u)", slot.countdown, slot.count_val);
        }
      else
        {
          log_append ("(stack underflow)");
          if (! options.do_log)
            qprintf ("*** syscall #%d 0x%s: log pop (stack underflow)\n",
                     sysno, pc_string());
        }
    }
  else
    leave (LEAVE_FATAL, "code must be unreachable");
}


void
do_syscall (int sysno, int val)
{
  switch (sysno)
    {
    default:
      log_append ("void ");
      qprintf ("*** syscall #%d: void\n", sysno);
      break;

    case 0: sys_log_config (LOG_OFF_CMD, 0);    break;
    case 1: sys_log_config (LOG_ON_CMD, 0);     break;
    case 2: sys_log_config (LOG_PERF_CMD, 0);   break;
    case 3: sys_log_config (LOG_SET_CMD, val);  break;
    case 5: sys_perf_cmd (val);     break;
    case 6: sys_perf_tag_cmd (val); break;
    case  9: sys_log_pushpop (sysno, 0);    break;
    case 10: sys_log_pushpop (sysno, 1);    break;
    case 11: sys_log_pushpop (sysno, -1);   break;
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
                      || have_syscall[10] || have_syscall[11]
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
