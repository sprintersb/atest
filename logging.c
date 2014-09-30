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
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "sreg.h"

#ifndef LOG_DUMP
#error no function herein is needed without LOG_DUMP
#endif // LOG_DUMP

const char s_SREG[] = "CZNVSHTI";

// ports used for application <-> simulator interactions
#define IN_AVRTEST
#include "avrtest.h"

static char log_data[256];
char *cur_log_pos = log_data;
static int log_during_perf;
static unsigned log_count, log_count_val;

typedef struct
{
  int on, will_be_on, pc, pc2, id, calls, n_dumps, sp;
  dword tick;
  unsigned cmd[4];
} perf_t;
static perf_t perf;

void
log_add_data (const char *data)
{
  int len = strlen (data);
  memcpy (cur_log_pos, data, len);
  cur_log_pos += len;
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

static void
log_put_bit (const decoded_op *op, char *buf)
{
  int id, mask, style = 0;
  
  switch (id = op->data_index)
    {
    default:
      return;
    case ID_BLD:  case ID_SBI:  case ID_SBIS:  case ID_SBRS:
    case ID_BST:  case ID_CBI:  case ID_SBIC:  case ID_SBRC:
      mask = op->oper2;
      style = 1;
      break;
    case ID_BRBS:  case ID_BRBC:
      mask = op->oper2;
      style = 2;
      break;
    case ID_BSET: case ID_BCLR:
      mask = op->oper1;
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

static char buf[32];

void
log_add_instr (const decoded_op *op)
{
  byte id = op->data_index;
  const char *instr = opcode_func_array[id].hreadable;
  if (arch.pc_3bytes)
    sprintf (buf, "%06x: %-7s ", cpu_PC * 2, instr);
  else
    sprintf (buf, "%04x: %-7s ", cpu_PC * 2, instr);
    
  log_put_bit (op, buf + 6 + 2*arch.pc_3bytes + strlen (instr));
  log_add_data (buf);
}

void
log_add_immed (int value)
{
  sprintf (buf, "(###)->%02x ", value);
  log_add_data (buf);
}


void
log_add_flag_read (int mask, int value)
{
  int bit = mask_to_bit (mask);

  if (bit < 0 || bit > 7)
    leave (EXIT_STATUS_FATAL, "not a power of 2: %02x", mask);

  sprintf (buf, " %c->%c", s_SREG[bit], '0' + !!value);
  log_add_data (buf);
}


void
log_add_data_mov (const char *format, int addr, int value)
{
  char ad[16], *adname = ad;

  if (addr_SREG == addr)
    {
      for (const char *f = s_SREG; *f; f++, value >>= 1)
        if (value & 1)
          *adname++ = *f;
      *adname++ = '\0';
      sprintf (buf, format, ad);
      log_add_data (buf);
      return;
    }

  if (addr_EIND == addr && arch.has_eind) adname = "EIND";
  else if (addr_SPL == addr)              adname = "SPL";
  else if (addr_SPH == addr)              adname = "SPH";
  else if (addr_RAMPZ == addr)            adname = "RAMPZ";
  else
    sprintf (adname, addr < 256 ? "%02x" : "%04x", addr);

  sprintf (buf, format, adname, value);
  log_add_data (buf);
}

void
log_add_reg_mov (const char *format, int regno, int value)
{
  sprintf (buf, format, regno, value);
  log_add_data (buf);
}

static void
putchar_escaped (char c)
{
  if (options.do_quiet)
    return;

  if (c == '\0') {}
  else if (c == '\n')  { putchar ('\\'); putchar ('n'); }
  else if (c == '\t')  { putchar ('\\'); putchar ('t'); }
  else if (c == '\r')  { putchar ('\\'); putchar ('r'); }
  else if (c == '\"')  { putchar ('\\'); putchar ('\"'); }
  else if (c == '\\')  { putchar ('\\'); putchar ('\\'); }
  else putchar (c);
}

// set argc and argv[] from -args
static void
do_put_args (byte x)
{
  // We have been asked to transfer command line arguments.
  if (--args.request == 1)
    {
      args.addr = (byte) x;
      return;
    }

  args.addr |= (byte) x << 8;

  // strip directory to save space
  const char *p, *program = options.program_name;
  if ((p = strrchr (program, '/')))    program = p;
  if ((p = strrchr (program, '\\')))   program = p;

  // put strings to args.addr 
  int argc = args.argc - args.i;
  int a = args.addr;
  for (int i = args.i; i < args.argc; i++)
    {
      const char *arg = i == args.i ? program : args.argv[i];
      int len = 1 + strlen (arg);
      qprintf ("*** (%04x) <-- *argv[%d] = \"", a, i - args.i);
      for (int j = 0; j < len; j++)
        {
          char c = arg[j];
          putchar_escaped (c);
          log_data_write_byte (a++, c, 0);
        }
      qprintf ("\"\n");
    }

  // put their addresses to argv[]
  int argv = a;
  int aa = args.addr;
  for (int i = args.i; i < args.argc; i++)
    {
      const char *arg = i == args.i ? program : args.argv[i];
      int len = 1 + strlen (arg);
      qprintf ("*** (%04x) <-- argv[%d] = %04x\n", a, i - args.i, aa);
      log_data_write_word (a, aa, 0);
      a += 2;
      aa += len;
    }
  qprintf ("*** (%04x) <-- argv[%d] = NULL\n", a, argc);
  log_data_write_word (a, aa, 0);

  // set argc, argc: picked up by exit.c:init_args() in .init8
  qprintf ("*** -args: at=%04x, argc=%d, argv=%04x\n", args.addr, argc, argv);
  qprintf ("*** R24 = %04x\n", argc);
  qprintf ("*** R22 = %04x\n", argv);

  log_put_word_reg (24, argc, 0);
  log_put_word_reg (22, argv, 0);
}


NOINLINE void
do_log_cmd (int x)
{
#define SET_LOGGING(F, P, C) \
  do { options.do_log = (F); log_during_perf = (P); log_count = (C); } while(0)

  if (args.request)
    {
      do_put_args (x);
      return;
    }

  switch (LOG_CMD (x))
    {
    case 0:
      // Do perf-meter stuff only in avrtest*_log in order
      // to avoid impact on execution speed.
      perf.cmd[PERF_CMD(x)] = PERF_N (x) ? 1 << PERF_N (x) : PERF_ALL;
      perf.will_be_on = 0 != perf.cmd[PERF_START];
      break;

    case LOG_SET:
      switch (LOG_NUM (x))
        {
        case LOG_GET_ARGS_NUM:
          args.request = 2;
          qprintf ("*** transfer %s-args\n", options.do_args ? "" : "-no");
          break;
        case LOG_START_NUM:
          qprintf ("*** start log\n");
          SET_LOGGING (1, 0, 0);
          break;
        case LOG_STOP_NUM:
          qprintf ("*** stop log\n");
          SET_LOGGING (0, 0, 0);
          break;
        case LOG_PERF_NUM:
          qprintf ("*** performance log\n");
          SET_LOGGING (0, 1, 0);
          break;
        default:
          log_count_val = LOG_NUM (x);
          qprintf ("*** start log %u\n", log_count_val);
          SET_LOGGING (1, 0, 1 + log_count_val);
          break;
        } // LOG_NUM
      break; // LOG_SET
    } // LOG_CMD
#undef SET_LOGGING
}

typedef struct
{
  // Extremal values and where they occurred (code word address)
  long min, min_at, at_start;
  long max, max_at, at_end;
} minmax_t;

typedef struct
{
  int n, on, valid;
  unsigned ticks; double ticks_ev2;
  unsigned insns; double insns_ev2;
  unsigned pc_start, pc_end;

  // Values for current Start/Stop round
  minmax_t pc, tick, insn;
  // Extremal values vor stack pointer and call depth
  minmax_t sp, calls;
} perfs_t;

static perfs_t perfs[16];

static INLINE void
minmax_update (minmax_t *mm, long x)
{
  if (x < mm->min)  mm->min = x, mm->min_at = perf.pc;
  if (x > mm->max)  mm->max = x, mm->max_at = perf.pc;
}

static INLINE void
minmax_init (minmax_t *mm, long at_start)
{
  mm->min = LONG_MAX;
  mm->max = LONG_MIN;
  mm->at_start = at_start;
}

static void
perf_instruction (int id)
{
  // call depth
  switch (id)
    {
    case ID_RCALL: case ID_ICALL: case ID_CALL: case ID_EICALL:
      perf.calls++;
      break;
    case ID_RET:
      // GCC might use push/push/ret for indirect jump,
      // don't account these for call depth
      if (perf.id != ID_PUSH)
        perf.calls--;
      break;
    }
  perf.id = id;
  perf.will_be_on = 0;

  // actions requested by LOG_PORT

  int dumps  = perf.cmd[PERF_DUMP];
  int starts = perf.cmd[PERF_START];
  int means  = perf.cmd[PERF_MEAN];
  int stops  = perf.cmd[PERF_STOP];

  int sp = log_data_read_SP();

  if (!perf.on && !starts && !stops && !dumps && !means)
    goto done;

  perf.on = 0;

  if (dumps)
    printf ("\n--- Dump # %d:\n", ++perf.n_dumps);

  for (int i = 1; i <= 15; i++)
    {
      perfs_t *p = &perfs[i];
      int start = starts & (1 << i) ? PERF_START : 0;
      int mean  = means  & (1 << i) ? PERF_MEAN : 0;
      int stop  = stops  & (1 << i);
      int dump  = dumps  & (1 << i);
      int sm = start | mean;

      if (stop && !dump && !p->on)
        {
          if (p->valid)
            qprintf ("\n--- Stop T%d %signored: T%d already stopped (after "
                    "round %d)\n", i, sm == PERF_MEAN ? "Mean " : "", i, p->n);
          else
            qprintf ("\n--- Stop T%d ignored: -unused-\n", i);
        }
      if ((stop | dump) && p->on)
        {
          p->on = 0;
          p->pc.at_end = p->pc_end = perf.pc2;
          p->insn.at_end = instr_count -1;
          p->tick.at_end = perf.tick;
          p->calls.at_end = perf.calls;
          p->sp.at_end = sp;
          int ticks = p->tick.at_end - p->tick.at_start;
          int insns = p->insn.at_end - p->insn.at_start;
          p->ticks_ev2 += (double) ticks * ticks;
          p->insns_ev2 += (double) insns * insns;
          p->ticks += ticks;
          p->insns += insns;
          minmax_update (& p->insn, insns);
          minmax_update (& p->tick, ticks);

          qprintf ("%sStop T%d %s(round %d,   %04lx--%04lx, %ld Ticks)\n",
                  dumps == PERF_ALL ? "  " : "\n--- ", i,
                  p->valid == PERF_MEAN ? "Mean " : "", p->n,
                  2*p->pc.at_start, 2*p->pc.at_end,
                  p->tick.at_end - p->tick.at_start);
        }

      if (dump && !p->valid && dumps != PERF_ALL)
        printf (" Timer T%d: -unused-\n\n", i);
      if (dump && p->valid)
        {
          long c = p->calls.at_start;
          long s = p->sp.at_start;
          printf (" Timer T%d (%d round%s):  %04x--%04x\n"
                  "    Total:      %6u Instructions  %6u Ticks\n", i, p->n,
                  p->n == 1 ? "" : "s", 2*p->pc_start, 2*p->pc_end,
                  p->insns, p->ticks);
          if (p->valid == PERF_MEAN)
            {
              // Var(X) = E(X^2) - E^2(X)
              double e_x2, e_x;
              e_x2 = p->ticks_ev2 / p->n; e_x = (double) p->ticks / p->n;
              double tick_sigma = sqrt (e_x2 - e_x*e_x);
              e_x2 = p->insns_ev2 / p->n; e_x = (double) p->insns / p->n;
              double insn_sigma = sqrt (e_x2 - e_x*e_x);
              printf ("    Mean:       %6d Instructions  %6d Ticks\n"
                      "    Stand.Dev:  %6.1f Instructions  %6.1f Ticks\n"
                      "    Min:        %6ld Instructions  %6ld Ticks\n"
                      "    Max:        %6ld Instructions  %6ld Ticks\n",
                      p->insns / p->n, p->ticks / p->n,
                      insn_sigma, tick_sigma,
                      p->insn.min, p->tick.min,
                      p->insn.max, p->tick.max); 
            }
          printf ("    Calls (abs) in [% 3ld,% 3ld] was:% 4ld now:% 4ld"
                  "    Stack (abs) in [%04lx,%04lx] was:%04lx now:%04lx\n"
                  "    Calls (rel) in [% 3ld,% 3ld] was:% 4ld now:% 4ld"
                  "    Stack (rel) in [% 4ld,% 4ld] was:% 4ld now:% 4ld\n\n",
                  p->calls.min,   p->calls.max,     c, p->calls.at_end,
                  p->sp.max,      p->sp.min,        s, p->sp.at_end,
                  p->calls.min-c, p->calls.max-c, c-c, p->calls.at_end-c,
                  s-p->sp.max,    s-p->sp.min,    s-s, s-p->sp.at_end);
          p->valid = 0;
        }

      if (p->on)
        {
          minmax_update (& p->sp, sp);
          minmax_update (& p->calls, perf.calls);
        }

      const char *s_start = start ? "Start T" : "start Mean T";
      const char *s_mode  = p->valid == PERF_START ? "Start" : "Mean";
      
      if (sm)
        {
          if (!p->valid)
            qprintf ("\n--- %s%d (round 1)\n", s_start, i);
          else if (p->on && sm == p->valid)
            qprintf ("\n--- %s%d ignored: T%d already started (round %d)\n",
                    s_start, i, i, p->n);
          else if (!p->on && sm == p->valid)
            qprintf ("\n--- re%s%d (round %d)\n", s_start, i, 1 + p->n);
          else
            qprintf ("\n--- %s%d ignored: T%d is in %s/Stop mode (%s "
                    "round %d)\n", s_start, i, i, s_mode,
                    p->on ? "started and in" : "stopped and after", p->n);
          if (!p->on && sm == p->valid)
            {
              p->on = 1;
              p->n ++;
              p->insn.at_start = instr_count;
              p->tick.at_start = program_cycles;
            }
          if (!p->valid)
            {
              p->valid = sm;
              p->n = p->on = 1;
              p->insns = p->ticks = 0;
              p->insns_ev2 = p->ticks_ev2 = 0.0;
              minmax_init (& p->insn,  instr_count);
              minmax_init (& p->tick,  program_cycles);
              minmax_init (& p->calls, perf.calls);
              minmax_init (& p->sp,    perf.sp);
              minmax_init (& p->pc,    p->pc_start = cpu_PC);
            }
        }
      perf.on |= p->on;
    }

  perf.cmd[0] = perf.cmd[1] = perf.cmd[2] = perf.cmd[3] = 0;
done:;
  // Store for the next call of ours.  Needed because log_dump_line()
  // must run after the instruction has performed and we might need
  // the values from before the instruction.
  perf.sp  = sp;
  perf.pc2 = perf.pc;
  perf.pc  = cpu_PC;
  perf.tick = program_cycles;
}

void
log_dump_line (int id)
{
  if (id && log_count && --log_count == 0)
    {
      options.do_log = 0;
      qprintf ("*** done log %u", log_count_val);
    }
  *cur_log_pos = '\0';
  if (options.do_log
      || (log_during_perf && (perf.on || perf.will_be_on)))
    puts (log_data);
  cur_log_pos = log_data;
  perf_instruction (id);
}
