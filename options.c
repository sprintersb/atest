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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "testavr.h"
#include "options.h"

// ----------------------------------------------------------------------------
//     parse command line arguments

static const char USAGE[] =
  "  usage: avrtest [-d] [-e entry-point] [-m MAXCOUNT] [-mmcu=ARCH]\n"
  "                 [-runtime] [-ticks] [-no-log] program\n"
  "Options:\n"
  "  -d           Initialize SRAM from .data (for ELF program)\n"
  "  -e ADDRESS   Byte address of program entry point (defaults to 0).\n"
  "  -m MAXCOUNT  Execute at most MAXCOUNT instructions.\n"
  "  -runtime     Print avrtest execution time.\n"
  "  -ticks       Enable the 32-bit cycle counter TICKS_PORT that can be used\n"
  "               to measure performance.\n"
  "  -no-log      Disable logging in avrtest_log.  Useful when capturing\n"
  "               performance data.  Logging can be turned on / off by\n"
  "               writing to LOG_PORT.  For avrtest this option and writing\n"
  "               to LOG_PORT have no effect.\n"
  "  -no-stdin    Disable stdin, i.e. reading from STDIN_PORT will not wait\n"
  "               for user input.\n"
  "  -no-stdout   Disable stdout, i.e. writing to STDOUT_PORT will not print\n"
  "               to stdout.\n"
  "  -mmcu=ARCH   Select instruction set for ARCH\n"
  "    ARCH is one of:\n";

// List of supported archs with their features.

static const arch_t arch_desc[] =
  {
    { "avr51",     0, 0, 0, 0x01ffff }, // default for is_xmega = 0
    { "avrxmega6", 1, 1, 1, 0x03ffff }, // default for is_xmega = 1
    { "avr6",      1, 1, 0, 0x03ffff },
    { NULL, 0, 0, 0, 0}
  };

// args from -args ... to pass to the target program (*_log only)

args_t args;

static void
usage (void)
{
  printf (USAGE);
  for (const arch_t *d = arch_desc; d->name; d++)
    if (is_xmega == d->is_xmega)
      printf (" %s", d->name);

  printf ("\n");
  leave (EXIT_STATUS_ABORTED, "command line error");
}

typedef struct
{
  // unique id
  int id;
  // name as known to the command line abd prefixed "-no-"
  const char *name;
  // address of target variable
  int *pflag;
  // default value
  int deflt;
} option_t;


static option_t option_desc[] =
  {
#define AVRTEST_OPT(NAME, DEFLT, VAR)           \
    { OPT_##NAME, "-no-"#NAME, &options.do_##VAR, DEFLT  },
#include "options.def"
#undef AVRTEST_OPT
    { OPT_unknown, NULL, NULL, 0 }
  };

options_t options =
  {
    NULL, // .program_name
#define AVRTEST_OPT(NAME, DEFLT, VAR)           \
    DEFLT,
#include "options.def"
#undef AVRTEST_OPT
  };

arch_t arch;

// parse command line arguments
void
parse_args (int argc, char *argv[])
{
  arch = arch_desc[is_xmega];

  for (int i = 1; i < argc; i++)
    {
      //  Use naive but very portable method to decode arguments.
      int on = 0;
      option_t *o;
      for (o = option_desc; o->name; o++)
        {
          if (strcmp (argv[i], o->name + strlen ("-no")) == 0)
            * o->pflag = on = 1;
          else if (strcmp (argv[i], o->name) == 0)
            * o->pflag = 0;
          else
            continue;
          break;
        }

      switch (o->id)
        {
        case OPT_unknown:
          if (strncmp (argv[i], "-mmcu=", 6) == 0)
            {
              for (const arch_t *d = arch_desc; d->name; d++)
                if (d->name == NULL)
                  usage();
                else if (strcmp (argv[i] + 6, d->name) == 0)
                  arch = *d;
              break;
            } // -mmcu=
          
          if (options.program_name != NULL)
            usage();
          options.program_name = argv[i];
          break;

        case OPT_e:
          if (++i >= argc)
            usage();
          // FIXME: get from avr-ld --entry-point if !on
          cpu_PC = on ? strtoul (argv[i], NULL, 0) : 0UL;
          if (cpu_PC % 2 != 0)
            {
              fprintf (stderr, "program entry point must be an even"
                       " byte address\n");
              leave (EXIT_STATUS_ABORTED, "command line error");
            }
          cpu_PC /= 2;
          break; // -e

        case OPT_args:
          args.argc = on ? argc : i;
          args.argv = argv;
          args.i = i;
          i = argc;
          break; // -args

        case OPT_m:
          if (++i >= argc)
            usage();
          max_instr_count = on ? strtoul (argv[i], NULL, 0) : 0UL;
          break; // -m
        }
    }

  if (options.program_name == NULL)
    usage();
}
