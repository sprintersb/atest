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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sreg.h"

#define FUT_ADD8_RES    0x01FF
#define FUT_ADD8_V2_80  0x0200
#define FUT_ADD8_V1_80  0x0400
#define FUT_ADD8_V2_08  0x0800
#define FUT_ADD8_V1_08  0x1000

#define FUT_SUB8_RES    0x01FF
#define FUT_SUB8_V2_80  0x0200
#define FUT_SUB8_V1_80  0x0400
#define FUT_SUB8_V2_08  0x0800
#define FUT_SUB8_V1_08  0x1000

static int theSize;
static int theNum;

enum
  {
    TABLE_C, TABLE_H,
    TB_EXTERN, TB_STATIC, TB_GLOBAL, TB_NOTHING
  };

int what, tb;

static void
table_start (const char *name, int size)
{
  theSize = size;
  theNum = 0;
  if (tb == TB_EXTERN)
    printf ("\nextern const byte %s[%d];", name, size);
  else if (tb == TB_STATIC)
    printf ("\nstatic const byte %s[%d] =\n  {", name, size);
  else if (tb == TB_GLOBAL)
    printf ("\nconst byte %s[%d] =\n  {", name, size);
  else if (tb == TB_NOTHING)
    (void) 0;
  else
    abort();
}


static void
table_end (void)
{
  if (tb == TB_NOTHING || tb == TB_EXTERN)
    return;
  printf ("\n  };\n");
}


static void
table_add (uint8_t x)
{
  if (tb == TB_NOTHING || tb == TB_EXTERN)
    return;

  if (theNum % 16 == 0)
    printf ("\n    ");

  printf ("0x%02x%s", x,
          theNum == theSize - 1 ? "" : theNum % 16 == 15 ? "," : ", ");
  theNum++;
}


static uint8_t
update_zns_flags (int result, uint8_t sreg)
{
  if ((result & 0xFF) == 0x00)
    sreg |= FLAG_Z;
  if (result & 0x80)
    sreg |= FLAG_N;
  if (((sreg & FLAG_N) != 0) != ((sreg & FLAG_V) != 0))
    sreg |= FLAG_S;
  return sreg;
}


static void
gen_flag_update_tables (void)
{
  printf ("// Auto-generated file. Do not edit.\n"
          "// Generated by : gen-flag-tables\n");
  if (what == TABLE_C)
    printf ("// Used by      : avrtest.c\n\n"
            "#include <stdint.h>\n\n"
            "typedef uint8_t byte;\n");
  if (what == TABLE_H)
    printf ("// Included by  : avrtest.c\n");

  tb = what == TABLE_C ? TB_GLOBAL : TB_EXTERN;
  // build flag update table for 8 bit addition
  table_start ("flag_update_table_add8", 8192);
  for (int i = 0; i < 8192; i++)
    {
      int result = i & FUT_ADD8_RES;
      uint8_t sreg = 0;
      if ((!(i & FUT_ADD8_V1_80) == !(i & FUT_ADD8_V2_80))
          && (!(result & 0x80) != !(i & FUT_ADD8_V1_80)))
        sreg |= FLAG_V;
      if (result & 0x100)
        sreg |= FLAG_C;
      if (((i & 0x1800) == 0x1800)
          || ((result & 0x08) == 0
              && ((i & 0x1800) != 0x0000)))
        sreg |= FLAG_H;
      sreg = update_zns_flags (result, sreg);
      table_add (sreg);
    }
  table_end ();

  // build flag update table for 8 bit subtraction
  table_start ("flag_update_table_sub8", 8192);
  for (int i = 0; i < 8192; i++)
    {
      int result = i & FUT_SUB8_RES;
      uint8_t sreg = 0;
      if ((!(result & 0x80) == !(i & FUT_SUB8_V2_80))
          && (!(i & FUT_SUB8_V1_80) != !(i & FUT_SUB8_V2_80)))
        sreg |= FLAG_V;
      if (result & 0x100)
        sreg |= FLAG_C;
      if (((i & 0x1800) == 0x1800)
          || ((result & 0x08) == 0
              && ((i & 0x1800) != 0x0000)))
        sreg |= FLAG_H;
      sreg = update_zns_flags (result, sreg);
      table_add (sreg);
    }
  table_end ();

  // build flag update table for 8 bit rotation
  table_start ("flag_update_table_ror8", 512);
  for (int i = 0; i < 512; i++)
    {
      int result = i >> 1;
      uint8_t sreg = 0;
      if (i & 0x01)
        sreg |= FLAG_C;
      if (((result & 0x80) != 0) != ((sreg & FLAG_C) != 0))
        sreg |= FLAG_V;
      sreg = update_zns_flags (result, sreg);
      table_add (sreg);
    }
  table_end ();

  // build flag update table for increment
  table_start ("flag_update_table_inc", 256);
  for (int i = 0; i < 256; i++)
    {
      uint8_t sreg = (i == 0x80) ? FLAG_V : 0;
      sreg = update_zns_flags (i, sreg);
      table_add (sreg);
    }
  table_end ();

  // build flag update table for decrement
  table_start ("flag_update_table_dec", 256);
  for (int i = 0; i < 256; i++)
    {
      uint8_t sreg = (i == 0x7F) ? FLAG_V : 0;
      sreg = update_zns_flags (i, sreg);
      table_add (sreg);
    }
  table_end ();

  // Implement the following table in flag-tanles.h so that
  // GCC can look up values at compile time (CLR).
  tb = what == TABLE_C ? TB_NOTHING : TB_STATIC;
  // build flag update table for logical operations
  table_start ("flag_update_table_logical", 256);
  for (int i = 0; i < 256; i++)
    {
      uint8_t sreg = update_zns_flags (i, 0);
      table_add (sreg);
    }
  table_end ();
}

// argv[1] == "0": Make flag-tables.c (extern tables)
// argv[1] == "1": Make flag-tables.h (static tables)
int main (int argc, char *argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;

  if (0 == strcmp (argv[1], "0"))
    what = TABLE_C;
  else if (0 == strcmp (argv[1], "1"))
    what = TABLE_H;
  else
    return EXIT_FAILURE;

  FILE *self = fopen ("gen-flag-tables.c", "r");

  if (!self)
    return EXIT_FAILURE;

  for (int c = fgetc (self); c != '#' ; c = fgetc (self))
    {
      if (c == EOF)
        {
          fclose (self);
          return EXIT_FAILURE;
        }
      if (c == '\r')
        continue;
      fputc (c, stdout);
    }
  fclose (self);

  gen_flag_update_tables ();
  return EXIT_SUCCESS;
}
