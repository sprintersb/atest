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
  "  usage: avrtest [-d] [-e ENTRY] [-m MAXCOUNT] [-mmcu=ARCH] [-runtime]\n"
  "                 [-no-log] [-no-stdin] [-no-stdout] [-q] [-graph[=FILE]]\n"
  "                 program [-args [...]]\n"
  "         avrtest --help\n"
  "Options:\n"
  "  -h            Show this help and exit.\n"
  "  -args ...     Pass all following parameters as argc and argv to main.\n"
  "  -d            Initialize SRAM from .data (for ELF program)\n"
  "  -e ENTRY      Byte address of program entry.  Default for ENTRY is\n"
  "                the entry point from the ELF program and 0 for non-ELF.\n"
  "  -m MAXCOUNT   Execute at most MAXCOUNT instructions.\n"
  "  -q            Quiet operation.  Only print messages explicitly\n"
  "                requested.  Pass exit status from the program.\n"
  "  -runtime      Print avrtest execution time.\n"
  "  -no-log       Disable logging in avrtest_log.  Useful when capturing\n"
  "                performance data.  Logging can still be controlled by\n"
  "                the running program, cf. README.\n"
  "  -no-stdin     Disable avrtest_getchar from avrtest.h.\n"
  "  -no-stdout    Disable avrtest_putchar from avrtest.h.\n"
  "  -graph[=FILE] Write a .dot FILE representing the dynamic call graph.\n"
  "                For the dot tool see  http://graphviz.org\n"
  "  -graph-help   Show more options to control graph generation and exit.\n"
  "  -mmcu=ARCH    Select instruction set for ARCH\n"
  "    ARCH is one of:\n";

static const char GRAPH_USAGE[] =
  "avrtest_log can generate dot files that show the dynamic call\n"
  "graph traversed during the simulation of the program.  The\n"
  "produced dot file can be converted to a graphic file using dot.\n"
  "dot is part of the graphviz package from  http://graphviz.org\n"
  "and is available for many platforms and OSes.\n"
  "\n"
  "To convert dot file \"file.dot\" to the PNG graphic \"file.png\"\n"
  "resp. to the SVG graphic \"file.svg\" run\n"
  "\n"
  "    dot file.dot -Tpng -o file.png\n"
  "    dot file.dot -Tsvg -o file.svg\n"
  "\n"
  "-graph-help   Show this help and exit.\n"
  "-graph[=FILE] Use FILE as file name for the dot call graph.\n"
  "              If FILE is \"\" or \"-\" standard output is used.\n"
  "-graph        Same as above, but compose the file name from\n"
  "              the program base name and the extension .dot.\n"
  "-graph-all    Show all nodes, even the ones that got no cycles\n"
  "              attributed to them.\n"
  "-graph-base=BASE   Account cycles to nodes only if BASE is in\n"
  "              their call chain.  Default for BASE is \"main\".\n"
  "-graph-reserved    Account cycles also to functions whose name\n"
  "              is a reserved identifier in C, e.g. to library\n"
  "              support functions like __mulsi3 from libgcc.\n"
  "-graph-leaf=CLIST  A comma separated list of functions to be\n"
  "              treated as leaf functions.  Costs of all sub-nodes\n"
  "              will be propagated to them.\n"
  "-graph-sub=CLIST   A comma separated list of functions to fully\n"
  "              expand, i.e. also assign costs to all reserved\n"
  "              functios they are using.\n"
  "-graph-skip=CLIST  A comma separated list of functions to ignore.\n"
  "              Propagate their costs up to the next appropriate\n"
  "              function in the call tree.\n"
  "\n"
  "Arguments BASE and elements of CLIST accept function names and\n"
  "numbers.  Numbers are treated as byte addresses except \"0\"\n"
  "which stands for the program entry point.  If you really mean\n"
  "address 0 then use \"0x0\" or \"00\".\n";

// List of supported archs with their features.

static const arch_t arch_desc[] =
  {
    { "avr51",     false, false, false, 0x01ffff }, // default if is_xmega = 0
    { "avrxmega6", true,  true,  true,  0x03ffff }, // default if is_xmega = 1
    { "avr6",      true,  true,  false, 0x03ffff },
    { NULL, false, false, false, 0}
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
  //  int deflt;
  // string after -foo=
  const char **psuffix;
} option_t;


static void
usage (const char *fmt, ...)
{
  static char reason[300];

  if (fmt == GRAPH_USAGE)
    {
      options.do_quiet = 0;
      qprintf ("%s\n", GRAPH_USAGE);
      exit (EXIT_SUCCESS);
    }

  if (!fmt)
    options.do_quiet = 0;

  qprintf ("%s", USAGE);
  for (const arch_t *d = arch_desc; d->name; d++)
    if (is_xmega == d->is_xmega)
      qprintf (" %s", d->name);

  if (!fmt)
    {
      qprintf ("\n");
      exit (EXIT_SUCCESS);
    }

  va_list args;
  va_start (args, fmt);
  vsnprintf (reason, sizeof (reason)-1, fmt, args);
  va_end (args);

  qprintf ("\n");
  leave (LEAVE_USAGE, "%s%s", reason,
         options.do_quiet ? ", use -h for help" : "");
}

static const option_t option_desc[] =
  {
#define AVRTEST_OPT(NAME, DEFLT, VAR)                                   \
    { OPT_##VAR, "-no-"#NAME, &options.do_##VAR, &options.s_##VAR },
#include "options.def"
#undef AVRTEST_OPT
    { OPT_unknown, NULL, NULL, NULL }
  };

options_t options =
  {
    "",   // .self
#define AVRTEST_OPT(NAME, DEFLT, VAR)   \
    DEFLT,                              \
    "",
#include "options.def"
#undef AVRTEST_OPT
  };


static unsigned long
get_valid_number (const char *str, const char *opt)
{
  char *end;
  unsigned long val = strtoul (str, &end, 0);
  if (*end && opt)
    usage ("invalid number %s in '%s'", str, opt);
  if (*end)
    usage ("invalid number %s", str);
  return val;
}


// parse command line arguments
void
parse_args (int argc, char *argv[])
{
  options.self = argv[0];
  arch = arch_desc[is_xmega];

  for (int i = 1; i < argc; i++)
    if (str_eq  (argv[i], "?")
        || str_eq (argv[i], "-?")
        || str_eq (argv[i], "/?")
        || str_eq (argv[i], "-h")
        || str_eq (argv[i], "-help")
        || str_eq (argv[i], "--help"))
      usage (NULL);
    else if (str_eq (argv[i], "-graph-help")
             || str_eq (argv[i], "-help-graph")
             || str_eq (argv[i], "--help=graph"))
      usage (GRAPH_USAGE);

  //  Use naive but very portable method to decode arguments.
  for (int i = 1; i < argc; i++)
    {
      const char *p;
      int on = 0;
      const option_t *o;
      for (o = option_desc; o->name; o++)
        {
          p = strchr (o->name, '=');
          if (p && str_prefix (o->name + strlen ("-no"), argv[i]))
            * o->pflag = on = 1, * o->psuffix = 1 + strchr (argv[i], '=');
          else if (p && str_prefix (o->name, argv[i]))
            * o->pflag = 0, * o->psuffix = "";
          else if (str_eq (argv[i], o->name + strlen ("-no")))
            * o->pflag = on = 1;
          else if (str_eq (argv[i], o->name))
            * o->pflag = 0;
          else
            continue;
          break;
        }

      switch (o->id)
        {
        case OPT_unknown:
          if (program.name != NULL)
            usage ("unknown option or duplicate program name '%s'", argv[i]);

          program.name = program.short_name = argv[i];
          // strip directories
          if ((p = strrchr (program.short_name, '/')))  program.short_name = p;
          if ((p = strrchr (program.short_name, '\\'))) program.short_name = p;
          break;

        case OPT_mmcu:
          if (!on)
            arch = arch_desc[is_xmega];
          else
            for (const arch_t *a = arch_desc; ; a++)
              if (a->name == NULL)
                usage ("unknown ARCH '%s'", options.s_mmcu);
              else if (is_xmega == a->is_xmega
                       && str_eq (options.s_mmcu, a->name))
                {
                  arch = *a;
                  break;
                }
          break; // -mmcu=

        case OPT_entry_point:
          if (++i >= argc)
            usage ("missing program ENTRY point after '%s'", argv[i-1]);
          if (on)
            {
              cpu_PC = get_valid_number (argv[i], "-e ENTRY");
              if (cpu_PC % 2 != 0)
                usage ("odd byte address as ENTRY point in '-e %s'", argv[i]);
              if (cpu_PC >= MAX_FLASH_SIZE)
                usage ("ENTRY point is too big in '-e %s'", argv[i]);
            }
          else
            cpu_PC = 0;
          program.entry_point = cpu_PC;
          cpu_PC /= 2;
          break; // -e

        case OPT_args:
          args.argc = on ? argc : i;
          args.argv = argv;
          args.i = i;
          i = argc;
          break; // -args

        case OPT_max_instr_count:
          if (++i >= argc)
            usage ("missing MAXCOUNT after '%s'", argv[i-1]);
          if (on)
            program.max_insns = get_valid_number (argv[i], "-m MAXCOUNT");
          break; // -m

        case OPT_graph:
          options.do_graph_filename &= on;
          break;

        case OPT_graph_filename:
          options.do_graph = on;
          break;
        }
    }

  if (program.name == NULL)
    usage ("missing program name");
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
  else if (c == '\"')  { putchar ('\\'); putchar ('"'); }
  else if (c == '\\')  { putchar ('\\'); putchar ('\\'); }
  else putchar (c);
}


// set argc and argv[] from -args
void
put_argv (int args_addr, byte *b)
{
  // put strings to args_addr 
  int argc = args.argc - args.i;
  int a = args_addr;

  for (int i = args.i; i < args.argc; i++)
    {
      const char *arg = i == args.i ? program.short_name : args.argv[i];
      int len = 1 + strlen (arg);
      if (is_avrtest_log)
        qprintf ("*** (%04x) <-- *argv[%d] = \"", a, i - args.i);
      strcpy ((char*) b, arg);
      a += len;
      b += len;
      if (is_avrtest_log)
        {
          for (int j = 0; j < len; j++)
            putchar_escaped (arg[j]);
          qprintf ("\"\n");
        }
    }

  // put their addresses to argv[]
  int argv = a;
  int aa = args_addr;
  for (int i = args.i; i < args.argc; i++)
    {
      const char *arg = i == args.i ? program.short_name : args.argv[i];
      int len = 1 + strlen (arg);
      if (is_avrtest_log)
        qprintf ("*** (%04x) <-- argv[%d] = %04x\n", a, i - args.i, aa);
      *b++ = aa;
      *b++ = aa >> 8;
      a += 2;
      aa += len;
    }
  if (is_avrtest_log)
    qprintf ("*** (%04x) <-- argv[%d] = NULL\n", a, argc);
  *b++ = 0;
  *b++ = 0;

  // set argc, argc: picked up by exit.c:avrtest_init_argc_argv() in .init8
  if (is_avrtest_log)
    qprintf ("*** -args: at=0x%04x, argc=R24=%d, argv=R22=0x%04x, "
             "env=R20=%d\n", args_addr, argc, argv, is_avrtest_log);

  args.avr_argv = argv;
  args.avr_argc = argc;
}


/* TOKENS is a string that is a comma-separated list of tokens
   "tok1,tok2,...,tokn".  Return a freshly allocated array of non-empty
   token strings.  Set *N to the number of strings.  */

char**
comma_list_to_array (const char *tokens, int *n)
{
  char *t, *toks = get_mem (2 + strlen (tokens), sizeof (char), "tok");

  // Prepend one ',' for convenience.  We must copy the string anyway.
  sprintf (toks, ",%s", tokens);

  // Count commas
  for (t = toks, *n = 0; *t; t++)
    *n += *t == ',';
  char **array = get_mem (*n, sizeof (const char*), "tok[]");

  // Find tokens from end to start of TOKS and replace each ',' by
  // a string terminating '\0'.
  *n = 0;
  while ((t = strrchr (toks, ',')))
    {
      if (t[1] != '\0')
        array[(*n)++] = t + 1;
      *t = '\0';
    }

  return array;
}
