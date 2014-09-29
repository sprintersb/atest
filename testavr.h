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

#ifndef TESTAVR_H
#define TESTAVR_H

#include <stdint.h>

// ---------------------------------------------------------------------------
//     configuration values (in bytes).

#define MAX_RAM_SIZE     (64 * 1024)
#define MAX_FLASH_SIZE  (256 * 1024)  // Must be at least 128KB

// PC is used as array index into decoded_flash[].
// If PC has bits outside the following mask, something went really wrong,
// e.g. in pop_PC.

#define PC_VALID_MASK (MAX_FLASH_SIZE/2 - 1)

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

typedef struct
{
  byte data_index;
  byte oper1;
  word oper2;
} decoded_op;

extern unsigned cpu_PC;
extern dword max_instr_count;
extern dword instr_count;
extern dword program_cycles;
extern const char *program_name;
extern const int is_xmega;

extern const int addr_SREG;
extern const int addr_SPH;
extern const int addr_SPL;
extern const int addr_EIND;
extern const int addr_RAMPZ;

#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define FASTCALL __attribute__((fastcall))

#define EXIT_STATUS_EXIT    0
#define EXIT_STATUS_ABORTED 1
#define EXIT_STATUS_TIMEOUT 2

extern void NOINLINE NORETURN leave (int status, const char *reason);

extern int log_data_read_SP (void);
extern void log_put_word_reg (int, int, int);
extern void log_data_write_byte (int, int, int);
extern void log_data_write_word (int, int, int);

#define OP_FUNC_TYPE void FASTCALL

typedef OP_FUNC_TYPE (*opcode_func)(int,int);

typedef struct
{
  opcode_func func;
#ifdef LOG_DUMP
  const char *hreadable;
#endif
  short size;
  short cycles;
} opcode_t;


#ifndef LOG_DUMP

// empty placeholders to keep the rest of the code clean

#define log_add_instr(...)    (void) 0
#define log_add_bit(...)      (void) 0
#define log_add_immed(...)    (void) 0
#define log_add_data_mov(...) (void) 0
#define log_add_reg_mov(...)  (void) 0
#define log_dump_line(...)    (void) 0
#define do_log_cmd(...)       (void) 0

#else

extern void log_add_data (const char *data);
extern void log_add_instr (const decoded_op *op);
extern void log_add_immed (int value);
extern void log_add_data_mov (const char *format, int addr, int value);
extern void log_dump_line (int id);
extern void log_add_reg_mov (const char *format, int regno, int value);
extern void do_log_cmd (int x);

#endif  // LOG_DUMP

// ---------------------------------------------------------------------------
//     auxiliary lookup tables

extern const opcode_t opcode_func_array[];

enum
  {
#define AVR_INSN(ID, N_WORDS, N_TICKS, NAME)    \
    ID_ ## ID,
    // dummy entry to guarantee that "zero" is an invalid function
    AVR_INSN(NULL, 0, 0, NULL)
#include "avr-insn.def"
#undef AVR_INSN
  };

#endif // TESTAVR_H
