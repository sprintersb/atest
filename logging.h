/*
  This file is part of avrtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2019 Free Software Foundation, Inc.

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

#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

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

extern const layout_t layout[];
extern int maybe_SP_glitch;

#endif // LOGGING_H
