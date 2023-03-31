/*
  This file is part of avrtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2023 Free Software Foundation, Inc.

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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct
{
  // Name of the architecture
  const char *name;
  // True if PC is 3 bytes, false if only 2 bytes
  bool pc_3bytes;
  // True if the architecture has EIND related insns (EICALL/EIJMP)
  bool has_eind;
  // True if this is XMEGA
  bool is_xmega;
  // True if the architecture has the RAMPD special function register.
  bool has_rampd;
  // True if this is reduced TINY
  bool is_tiny;
  // Mask to detect whether cpu_PC is out of bounds
  unsigned int flash_addr_mask;
  // Offset where flash is seen in RAM address space, or 0.
  unsigned int flash_pm_offset;
} arch_t;

extern arch_t arch;

enum
  {
#define AVRTEST_OPT(NAME, DEFLT, VAR)   \
    OPT_ ## VAR,
#include "options.def"
#undef AVRTEST_OPT
    OPT_unknown
  };

typedef struct
{
  // program name of avrtest
  const char *self;

#define AVRTEST_OPT(NAME, DEFLT, VAR)   \
  int do_ ## VAR;                       \
  const char *s_ ## VAR;
#include "options.def"
#undef AVRTEST_OPT

} options_t;

typedef struct
{
  int argc, i;
  char **argv;
  int avr_argc, avr_argv;
} args_t;

extern void parse_args (int argc, char *argv[]);
extern char** comma_list_to_array (const char *tokens, int *n);

extern options_t options;
extern args_t args;
extern arch_t arch;
extern const char *fileio_sandbox;

#endif // OPTIONS_H
