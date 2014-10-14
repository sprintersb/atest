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

typedef struct
{
  // Buffer filled by log_append() and flushed to stdout after the
  // instruction has been executed (provided logging is on).
  char data[256];
  // Write position in .data[].
  char *pos;
  // LOG_PERF: Just log when at least one perf-meter is on
  int perf_only;
  // LOG_SET(N): Log the next f(N) instructions
  unsigned count_val;
  // LOG_SET(N): Value count down to 0 and then stop logging.
  unsigned countdown;
  // Instruction might turn on logging, thus log it even if logging is
  // (still) off.  Only ST* and OUT can start logging.
  int maybe_log;
  // This instruction is OUT or ST*, always log as is might be LOG_ON.
  int maybe_OUT;
  // Whether this instruction has been logged
  int log_this;
  // ID of current instruction.
  int id;
  // No log needed for the current instruction; dont' add stuff to .data[]
  // in order to speed up matters if logging is (temporarily) off.
  int unused;
  // Call depth after the insn completes
  int calls;
  // Change of call depth from the last instruction.
  int calls_changed;
  // __prologue_saves__ / __epilogue_restores__
  struct
  {
    // Word address of __prologue_saves__ / __epilogue_restores__ or 0
    int pc;
    // Function that is using __prologue_saves__ / __epilogue_restores__
    const char *func;
  } prologue_save, epilogue_restore;
  
  // Symbol stack for function names from ELF.
  const char *symbol_stack[LEN_SYMBOL_STACK];
} alog_t;

typedef struct
{
  // Whether any perf-meter is curremtly on;
  int on;
  // This instruction issued PERF_START and we must log if LOG_PERF is on.
  // However log_add_instr() must run before perf_instruction().
  int will_be_on;
  // PC and SP and program_cycles before the current instruction executed.
  dword tick;
  int pc, sp;
  // Yet one step older instruction ID (for call depth) and PC.
  int id, pc2;
  // Call depth
  int calls;
  // Enumerates PERF_DUMP
  int n_dumps;
  
  // Commands as sent to LOG_PORT
  unsigned cmd[NUM_PERF_CMDS];
  int pending_LOG_TAG_FMT;
  
  // From PERF_STOP_XXX()
  //int dval_is_set;
  double dval;
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
  int on;
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
  // # Bytes to read from TICKS_PORT
  int size;
  // Whether the value is signed / loacted in flash (LOG_PSTR etc.)
  int signed_p, in_rom;
  // Default printf format string
  const char *fmt;
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
    [LOG_STR_CMD]  = { 2, 0, 0, "%s" },
    [LOG_PSTR_CMD] = { 2, 0, 1, "%s"},
    [LOG_ADDR_CMD] = { 2, 0, 0, " 0x%04x " },

    [LOG_FLOAT_CMD] = { 4, 0, 0, " %.6f " },
    
    [LOG_U8_CMD]  = { 1, 0, 0, " %u " },
    [LOG_U16_CMD] = { 2, 0, 0, " %u " },
    [LOG_U24_CMD] = { 3, 0, 0, " %u " },
    [LOG_U32_CMD] = { 4, 0, 0, " %u " },
    
    [LOG_S8_CMD]  = { 1, 1, 0, " %d " },
    [LOG_S16_CMD] = { 2, 1, 0, " %d " },
    [LOG_S24_CMD] = { 3, 1, 0, " %d " },
    [LOG_S32_CMD] = { 4, 1, 0, " %d " },

    [LOG_X8_CMD]  = { 1, 0, 0, " 0x%02x " },
    [LOG_X16_CMD] = { 2, 0, 0, " 0x%04x " },
    [LOG_X24_CMD] = { 3, 0, 0, " 0x%06x " },
    [LOG_X32_CMD] = { 4, 0, 0, " 0x%08x " },

    [LOG_UNSET_FMT_CMD]     = { 0, 0, 0, NULL },
    [LOG_SET_FMT_CMD]       = { 2, 0, 0, NULL },
    [LOG_SET_PFMT_CMD]      = { 2, 0, 1, NULL },
    [LOG_SET_FMT_ONCE_CMD]  = { 2, 0, 0, NULL },
    [LOG_SET_PFMT_ONCE_CMD] = { 2, 0, 1, NULL },

    [LOG_TAG_FMT_CMD]       = { 2, 0, 0, NULL },
    [LOG_TAG_PFMT_CMD]      = { 2, 0, 1, NULL },
  };
