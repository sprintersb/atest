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

#include <stdbool.h>
#include <stdlib.h>

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


typedef struct
{
  // The ELF string table associated to the ELF symbol table
  const char *data;
  size_t size;
  int n_entries;

  // Number of ...
  int n_strings;    // ... usable items and indices (inclusive) in strings[]
  int n_funcs;      // ... that have STT_FUNC as symbol table type
  int n_bad;        // ... that are of no use for us, e.g. orphans
  int n_vec;        // ... unused vectors slots
} string_table_t;


typedef struct
{
  // Whether any perf-meter is curremtly on;
  bool on;
  // This instruction issued PERF_START and we must log if LOG_PERF is on.
  // However log_add_instr() must run before perf_instruction().
  bool will_be_on;
  // PC and SP and program_cycles before the current instruction executed.
  dword tick;
  int sp;
  // Enumerates PERF_DUMP
  int n_dumps;
  // Commands as sent by SYSCALL 5..6
  unsigned cmd;
  unsigned pmask;
  // From PERF_STOP_XXX()
  double dval;
  bool pending_LOG_TAG_FMT;
} perf_t;

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

// Decomposed IEEE 754 single
typedef struct
{
  int sign_bit;
  // Mantissa without (23 bits) and with the leading (implicit) 1 (24 bits)
  unsigned mant, mant1;
  int exp;
  int exp_biased;
  int fclass;
  double x;
} avr_float_t;

// Information for LOG_<data>
typedef struct
{
  // # Bytes to read starting at R20
  int size;
  // Default printf format string
  const char *fmt;
  // Whether the value is signed / loacted in flash (LOG_PSTR etc.)
  bool signed_p, in_rom;
} layout_t;

enum
  {
    FT_NORM,
    FT_DENORM,
    FT_INF,
    FT_NAN
  };


static const layout_t
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
