/*
  This file is part of AVRtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2024 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "logging.h"
#include "host.h"
#include "perf.h"

#define IN_AVRTEST
#include "avrtest.h"

#ifndef AVRTEST_LOG
#error no function herein is needed without AVRTEST_LOG
#endif // AVRTEST_LOG

#define LEN_PERF_TAG_STRING  50
#define LEN_PERF_TAG_FMT    200
#define LEN_PERF_LABEL      100

#define NUM_PERFS 8

// Min / Max values can be tagged to show the tag in LOG_DUMP
typedef struct
{
  // The kind of the tag, index in layout[].
  int cmd;
  // The very tag: Integer or double or string.
  unsigned val;
  double dval;
  char string [LEN_PERF_TAG_STRING];
  // A custom printf format string to display the tag.
  // !!! Be careful with this!  Non-matching %-codes in the
  // !!! format string migh crash avrtest!
  char fmt[LEN_PERF_TAG_FMT];
} perf_tag_t;


// Extremal values and where they occurred (code word address)
typedef struct
{
  long min, min_at, at_start; perf_tag_t tag_min; double dmin; int r_min;
  long max, max_at, at_end;   perf_tag_t tag_max; double dmax; int r_max;
  double ev2;
} minmax_t;


// We have 7 perf-meters perfs[1] ... perfs[7]
// Special index 0 stands for ALL
typedef struct
{
  // We are in / after the n-th START / STOP round resp. have n values
  // form PERF_STAT_xxx()
  int n;
  // 1 after PERF_START
  // 0 after PERF_STOP
  bool on;
  // Whether this perf holds information, i.e. the perf is after
  // PERF_START but not directly after PERF_DUMP.
  int valid;
  // Cumulated Ticks and Instructions over all START / STOP rounds
  unsigned ticks, insns;
  // Program counter from START of round 1 and STOP of last round
  unsigned pc_start, pc_end;
  // Sum over all PERF_STAT_X to compute expectation value
  double val_ev;
  // Tag that was valid at START and tag from PERF_TAG_X.
  perf_tag_t tag, tag_for_start;

  // Values for current Start/Stop round
  minmax_t pc, tick, insn, val;
  // Extremal values for stack pointer and call depth
  minmax_t sp, calls;
  struct
  {
    // Only instructions with SP smaller than this matter (PERF_START_CALL).
    int sp;
    dword ticks, insns;
  } call_only;
  // PERF_LABEL
  char label[LEN_PERF_LABEL];
} perfs_t;


perf_t perf;
static perfs_t perfs[NUM_PERFS];


void
sys_perf_cmd (int x)
{
  const char *s = "???";
  int n = PERF_N (x);
  int cmd = PERF_CMD(x);

  if (!log_unused)
    {
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
    }

  // Do perf-meter stuff only in avrtest*_log in order
  // to avoid impact on execution speed.
  perf.pmask = n ? 1 << n : PERF_ALL;
  perf.will_be_on = cmd == PERF_START_CMD || cmd == PERF_START_CALL_CMD;
  perf.cmd = cmd;
}


void
sys_perf_tag_cmd (int x)
{
  // PERF_TAG_[P]FMT gets the format string, then
  // PERF_TAG_XXX uses that format on a specific perf-meter

  perfs_t *p = & perfs[PERF_N (x)];
  int cmd = 0, tag_cmd = PERF_TAG_CMD (x);
  const char *s = "";

  switch (tag_cmd)
    {
    case PERF_TAG_STR_CMD: s = "_TAG string";  cmd = LOG_STR_CMD;   break;
    case PERF_TAG_S16_CMD:   s = "_TAG s16";   cmd = LOG_S16_CMD;   break;
    case PERF_TAG_S32_CMD:   s = "_TAG s32";   cmd = LOG_S32_CMD;   break;
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
      perf.pending_LOG_TAG_FMT = true;
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

  perf.pending_LOG_TAG_FMT = false;
}


static int
print_tag (const perf_tag_t *t, const char *no_tag, const char *tag_prefix)
{
  if (t->cmd < 0)
    return printf ("%s", no_tag);

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
  if (x < mm->min) { mm->min = x; mm->min_at = old_PC; mm->r_min = p->n; }
  if (x > mm->max) { mm->max = x; mm->max_at = old_PC; mm->r_max = p->n; }
}

static INLINE void
minmax_update_double (minmax_t *mm, double x, const perfs_t *p)
{
  if (x < mm->dmin && p->tag.cmd >= 0) mm->tag_min = p->tag;
  if (x > mm->dmax && p->tag.cmd >= 0) mm->tag_max = p->tag;
  if (x < mm->dmin) { mm->dmin = x; mm->min_at = old_PC; mm->r_min = p->n; }
  if (x > mm->dmax) { mm->dmax = x; mm->max_at = old_PC; mm->r_max = p->n; }
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
perf_verbose_start (perfs_t *p, int i, int cmd)
{
  qprintf ("\n--- ");

  if (!p->valid)
    {
      if (PERF_START_CMD == cmd)
        qprintf ("Start T%d (round 1", i);
    }
  else if (PERF_START_CMD == cmd)
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
          || (cmd >= PERF_STAT_CMD
              && p->valid == PERF_STAT_CMD)
          || (cmd == PERF_START_CMD
              && p->valid == PERF_START_CMD && !p->on));
}

// PERF_STAT_XXX (i, x)
static void
perf_stat (perfs_t *p, int i, int cmd)
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
      p->on = false;
      p->n = 0;
      p->val_ev = 0.0;
      minmax_init (& p->val, 0);
    }

  double dval;
  signed sraw = get_r20_value (& layout[LOG_S32_CMD]);
  unsigned uraw = (unsigned) sraw & 0xffffffff;
  if (PERF_STAT_U32_CMD == cmd)
    dval = (double) uraw;
  else if (PERF_STAT_S32_CMD == cmd)
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
perf_start (perfs_t *p, int i, int call_depth)
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
      minmax_init (& p->insn,  (long) program.n_insns);
      minmax_init (& p->tick,  (long) program.n_cycles);
      minmax_init (& p->calls, call_depth);
      minmax_init (& p->sp,    perf.sp);
      minmax_init (& p->pc,    p->pc_start = cpu_PC);
    }

  // (Re)start
  p->on = true;
  // Overridden by caller for PERF_START_CALL
  p->call_only.sp = INT_MAX;
  p->call_only.insns = 0;
  p->call_only.ticks = 0;
  p->n++;
  p->insn.at_start = (long) program.n_insns;
  p->tick.at_start = (long) program.n_cycles;

  if (!options.do_quiet)
    {
      print_tag (& p->tag, "", ", ");
      qprintf (")\n");
    }
}


// PERF_STOP (i)
static void
perf_stop (perfs_t *p, int i, bool dump_all, int cmd, int call_depth, int sp)
{
  if (cmd != PERF_DUMP_CMD)
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
      p->on = false;
      p->pc.at_end = p->pc_end = old_old_PC;
      p->insn.at_end = (long) program.n_insns - 1;
      p->tick.at_end = perf.tick;
      p->calls.at_end = call_depth;
      p->sp.at_end = sp;
      if (p->call_only.sp == INT_MAX)
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
               dump_all ? "  " : "\n--- ", i, p->n);
      if (!options.do_quiet)
        print_tag (& p->tag, "", ", ");

      qprintf (", %04lx--%04lx, %ld Ticks)\n",
               2 * p->pc.at_start, 2 * p->pc.at_end, ticks);
    }
}


// PERF_DUMP (i)
static void
perf_dump (perfs_t *p, int i, bool dump_all)
{
  if (!p->valid)
    {
      if (!dump_all)
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


void
perf_instruction (int id, int call_depth)
{
  perf.will_be_on = false;

  int sp = get_nonglitch_SP();

  // actions requested by perf SYSCALLs 5..6

  int pmask = perf.pmask;

  if (!perf.on && !pmask)
    goto done;

  perf.pmask = 0;
  perf.on = false;

  int cmd = perf.cmd;
  if (cmd == PERF_DUMP_CMD)
    printf ("\n--- Dump # %d:\n", ++perf.n_dumps);

  bool dump_all = cmd == PERF_DUMP_CMD  &&  pmask == PERF_ALL;

  for (int i = 1; i < NUM_PERFS; i++)
    {
      int imask = (1 << i) & pmask;
      int start = 0, stat = 0, stop = 0, dump = 0;

      if (imask)
        switch (cmd)
          {
          case PERF_START_CMD:
          case PERF_START_CALL_CMD: start = imask; break;
          case PERF_STOP_CMD:       stop  = imask; break;
          case PERF_DUMP_CMD:       dump  = imask; break;
          case PERF_STAT_U32_CMD:
          case PERF_STAT_S32_CMD:
          case PERF_STAT_FLOAT_CMD: stat  = imask; break;
          }

      perfs_t *p = &perfs[i];

      if (p->on
          // PERF_START_CALL:  Only account costs (including CALL+RET)
          // if we have a call depth > 0 relative to the starting point.
          && p->call_only.sp < INT_MAX
          && (sp < p->call_only.sp || perf.sp < p->call_only.sp))
        {
          p->call_only.insns += 1;
          p->call_only.ticks += program.n_cycles - perf.tick;
        }

      if (stop || dump)
        perf_stop (p, i, dump_all, cmd, call_depth, sp);

      if (dump)
        perf_dump (p, i, dump_all);

      if (p->on)
        {
          minmax_update (& p->sp, sp, p);
          minmax_update (& p->calls, call_depth, p);
        }

      if (start && perf_verbose_start (p, i, PERF_START_CMD))
        {
          perf_start (p, i, call_depth);
          if (cmd == PERF_START_CALL_CMD)
            p->call_only.sp = sp;
        }
      else if (stat && perf_verbose_start (p, i, cmd))
        perf_stat (p, i, cmd);

      perf.on |= p->on;
    }

 done:;
  // Store for the next call of ours.  Needed because log_dump_line()
  // must run after the instruction has performed and we might need
  // the values from before the instruction.
  perf.sp  = sp;
  perf.tick = (dword) program.n_cycles;
}


void
perf_init (void)
{
  for (int i = 1; i < NUM_PERFS; i++)
    perfs[i].tag_for_start.cmd = -1;
}
