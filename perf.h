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

#ifndef PERF_H
#define PERF_H

#include <stdbool.h>

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

extern void perf_init (void);
extern void sys_perf_cmd (int x);
extern void sys_perf_tag_cmd (int x);
extern void perf_instruction (int id, int call_depth);

extern perf_t perf;

#endif // PERF_H

