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
#include <stdarg.h>
#include <string.h>

#include "testavr.h"
#include "options.h"

// ----------------------------------------------------------------------------
//     parse command line arguments

static const char USAGE[] =
  "  usage: avrtest [-d] [-e ENTRY] [-m MAXCOUNT] [-mmcu=ARCH] [-q]\n"
  "                 [-runtime] [-ticks] [-no-log] [-no-stdin] [-no-stdout]\n"
  "                 program [-args [...]]\n"
  "Options:\n"
  "  -args ...    Pass all following parameters as argc and argv to main.\n"
  "  -d           Initialize SRAM from .data (for ELF program)\n"
  "  -e ENTRY     Byte address of program entry point (defaults to 0).\n"
  "  -m MAXCOUNT  Execute at most MAXCOUNT instructions.\n"
  "  -q           Quiet operation.  Only print messages explicitly requested,\n"
  "               e.g. by LOG_U8(42).  Pass exit status from the program.\n"
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
    { "avr51",     0, 0, 0, 0x01ffff }, // default if is_xmega = 0
    { "avrxmega6", 1, 1, 1, 0x03ffff }, // default if is_xmega = 1
    { "avr6",      1, 1, 0, 0x03ffff },
    { NULL, 0, 0, 0, 0}
  };

arch_t arch;

// args from -args ... to pass to the target program (*_log only)
args_t args;

typedef struct
{
  // unique id
  int id;
  // name as known to the command line and prefixed "-no-"
  const char *name;
  // address of target variable
  int *pflag;
  // default value
  int deflt;
} option_t;


static void
usage (const char *fmt, ...)
{
  static char reason[300];
  
  options.do_quiet = 0;

  printf (USAGE);
  for (const arch_t *d = arch_desc; d->name; d++)
    if (is_xmega == d->is_xmega)
      printf (" %s", d->name);

  va_list args;
  va_start (args, fmt);
  vsnprintf (reason, sizeof (reason)-1, fmt, args);
  va_end (args);

  printf ("\n");
  leave (EXIT_STATUS_USAGE, "command line error: %s", reason);
}

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
    "", // .self
    NULL, // .program_name
#define AVRTEST_OPT(NAME, DEFLT, VAR)           \
    DEFLT,
#include "options.def"
#undef AVRTEST_OPT
  };


static unsigned long
get_valid_number (const char *str, const char *opt)
{
  char *end;
  unsigned long val = strtoul (str, &end, 0);
  if (*end)
    usage ("invalid number in '%s %s'", opt, str);
  return val;
}


// parse command line arguments
void
parse_args (int argc, char *argv[])
{
  options.self = argv[0];
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
              for (const arch_t *d = arch_desc; ; d++)
                if (d->name == NULL)
                  usage ("unknown ARCH '%s'", argv[i] + 6);
                else if (is_xmega == d->is_xmega
                         && strcmp (argv[i] + 6, d->name) == 0)
                  {
                    arch = *d;
                    break;
                  }
              break;
            } // -mmcu=
          
          if (options.program_name != NULL)
            usage ("unknown option or second program name '%s'", argv[i]);
          options.program_name = argv[i];
          break;

        case OPT_e:
          if (++i >= argc)
            usage ("missing program ENTRY point after '%s'", argv[i]);
          // FIXME: get from avr-ld --entry-point if !on
          if (on)
            {
              cpu_PC = get_valid_number (argv[i], "-e");
              if (cpu_PC % 2 != 0)
                usage ("odd byte address as ENTRY point in '-e %s'", argv[i]);
              if (cpu_PC >= MAX_FLASH_SIZE)
                usage ("ENTRY point is too big in '-e %s'", argv[i]);
            }
          else
            cpu_PC = 0;
          program_entry_point = cpu_PC;
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
            usage ("missing MAXCOUNT after '%s'", argv[i]);
          if (on)
            max_instr_count = get_valid_number (argv[i], "-m");
          break; // -m
        }
    }

  if (options.program_name == NULL)
    usage ("missing program name");
}
