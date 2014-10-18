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

#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct
{
  // Name of the architecture
  const char *name;
  // True if PC is 3 bytes, false if only 2 bytes
  unsigned char pc_3bytes;
  // True if the architecture has EIND related insns (EICALL/EIJMP)
  unsigned char has_eind;
  // True if this is XMEGA
  unsigned char is_xmega;
  // Mask to detect whether cpu_PC is out of bounds
  unsigned int flash_addr_mask;
} arch_t;

extern arch_t arch;

enum
  {
#define AVRTEST_OPT(NAME, DEFLT, VAR)           \
    OPT_##NAME,
#include "options.def"
#undef AVRTEST_OPT
    OPT_unknown
  };

typedef struct
{
  // program name of avrtest
  const char *self;

  // filename of the file being executed
  const char *program_name;

#define AVRTEST_OPT(NAME, DEFLT, VAR)           \
  int do_##VAR;
#include "options.def"
#undef AVRTEST_OPT

} options_t;

typedef struct
{
  int argc, i;
  char **argv;
  int avr_argc, avr_argv;
} args_t;

extern options_t options;
extern void parse_args (int argc, char *argv[]);
extern args_t args;
extern arch_t arch;

#endif // OPTIONS_H
