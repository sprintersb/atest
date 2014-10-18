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
#include <stdlib.h>

// ---------------------------------------------------------------------------
//     configuration values (in bytes).

#define MAX_RAM_SIZE     (64 * 1024)
#define MAX_FLASH_SIZE  (256 * 1024)  // Must be at least 128KB
#define MAX_EEPROM_SIZE  (16 * 1024)  // .eeprom is read from ELF but unused

// PC is used as array index into decoded_flash[].
// If PC has bits outside the following mask, something went really wrong,
// e.g. in pop_PC.

#define PC_VALID_MASK (MAX_FLASH_SIZE/2 - 1)

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

typedef struct
{
  byte id;
  byte op1;
  word op2;
} decoded_t;

typedef struct
{
  // Program entry byte address as of ELF header or set
  // by -e ENTRY
  unsigned entry_point;

  // Size in bytes of program in flash (assuming it starts at 0x0).
  unsigned size;

  // Number of bytes read from file.
  unsigned n_bytes;

  // Range that covers executable code's byte addresses.  That range might
  // contain non-executable code like ELF headers that are part of PHDRs.
  unsigned code_start, code_end;

  // Maximum number of instructions to be executed,
  // used as a timeout.  Can be set my -m CYCLES
  dword max_insns;

  // Number of instructions simulated so far.
  dword n_insns;

  // Cycles consumed by the program so far.
  dword n_cycles;
} program_t;

extern program_t program;

extern unsigned cpu_PC;
extern const int is_xmega;
extern const int io_base;
extern const int is_avrtest_log;
extern const unsigned invalid_opcode;

#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define FASTCALL __attribute__((fastcall))

enum
  {
    EXIT_STATUS_EXIT,
    EXIT_STATUS_ABORTED,
    EXIT_STATUS_TIMEOUT,
    // Something went badly wrong
    EXIT_STATUS_USAGE,
    EXIT_STATUS_FILE,
    EXIT_STATUS_MEMORY,
    EXIT_STATUS_FATAL
  };

enum
  {
    AR_REG,
    AR_RAM,
    AR_FLASH,
    AR_EEPROM,
    AR_SP,
    AR_TICKS_PORT
  };

extern void NOINLINE NORETURN leave (int status, const char *reason, ...);
extern void qprintf (const char *fmt, ...);
extern byte* log_cpu_address (int, int);
extern void* get_mem (unsigned, size_t);

extern const int addr_SREG;

typedef struct
{
  int addr;
  int in, out;
  // False if PUSH, RET etc. are not supposed to change
  // this port.
  int any_id;
  const char *name;
} magic_t;

extern const magic_t named_port[];

#define OP_FUNC_TYPE void FASTCALL

typedef OP_FUNC_TYPE (*opcode_func)(int,int);

typedef struct
{
  opcode_func func;
  short size;
  short cycles;
} opcode_t;


#ifndef AVRTEST_LOG

// empty placeholders to keep the rest of the code clean

#define log_init(...)          (void) 0
#define log_append(...)        (void) 0
#define log_add_instr(...)     (void) 0
#define log_add_data_mov(...)  (void) 0
#define log_add_flag_read(...) (void) 0
#define log_dump_line(...)     (void) 0
#define do_syscall(...)        (void) 0
#define flush_ticks_port(...)  (void) 0
#define log_set_func_symbol(...) (void) 0

#else

extern void log_init (unsigned);
extern void log_append (const char *fmt, ...);
extern void log_add_instr (const decoded_t *op);
extern void log_add_data_mov (const char *format, int addr, int value);
extern void log_add_flag_read (int mask, int value);
extern void log_add_reg_mov (const char *format, int regno, int value);
extern void log_dump_line (int id);
extern void do_syscall (int x, int val);
extern void flush_ticks_port (int addr);
extern void log_set_func_symbol (int, const char*, int);

#endif  // AVRTEST_LOG

extern void load_to_flash (const char*, byte[], byte[], byte[]);
extern void decode_flash (decoded_t[], const byte[]);
extern void set_function_symbol (int, const char*, int);
extern void put_argv (int, byte*);

// ---------------------------------------------------------------------------
//     auxiliary lookup tables

extern const opcode_t opcodes[];

enum
  {
#define AVR_OPCODE(ID, N_WORDS, N_TICKS, NAME)    \
    ID_ ## ID,
#include "avr-opcode.def"
#undef AVR_OPCODE
  };

#endif // TESTAVR_H
