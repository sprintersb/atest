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
#include <stdbool.h>

// ---------------------------------------------------------------------------
//     configuration values (in bytes).

#define MAX_RAM_SIZE     (64 * 1024)
#define MAX_FLASH_SIZE  (256 * 1024)  // Must be at least 128KB
#define MAX_EEPROM_SIZE  (16 * 1024)  // .eeprom is read from ELF but unused

#define REGX    26
#define REGY    28
#define REGZ    30

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

  //
  int leave_status, exit_value;

  // filename of the file being executed
  const char *name;

  // ...and with directories stripped off
  const char *short_name;
} program_t;

extern program_t program;

extern unsigned cpu_PC;
extern const int io_base;
extern const bool is_xmega;
extern const bool is_avrtest_log;
extern const unsigned invalid_opcode;

extern bool have_syscall[32];

#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define FASTCALL __attribute__((fastcall))

enum
  {
    LEAVE_EXIT,
    LEAVE_ABORTED,
    LEAVE_TIMEOUT,
    LEAVE_FILE,
    // Something went badly wrong
    LEAVE_USAGE,
    LEAVE_MEMORY,
    LEAVE_IO,
    LEAVE_FATAL
  };

enum
  {
    AR_REG,
    AR_RAM,
    AR_FLASH,
    AR_EEPROM
  };

enum
  {
    IL_ILL,
    IL_ARCH,
    IL_TODO
  };

extern void NOINLINE NORETURN leave (int status, const char *reason, ...);
extern void qprintf (const char *fmt, ...);
extern byte* log_cpu_address (int, int);
extern void* get_mem (unsigned, size_t, const char*);

extern const int addr_SREG;
extern byte* const pSP;

typedef struct
{
  int addr;
  const char *name;
  // Points to a bool telling whether this address is special.
  // NULL means "yes".
  bool *pon;
} sfr_t;

extern const sfr_t named_sfr[];

#define OP_FUNC_TYPE void FASTCALL

typedef OP_FUNC_TYPE (*opcode_func)(int,int);

typedef struct
{
  opcode_func func;
  const char *mnemonic;
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
#define log_set_func_symbol(...)      (void) 0
#define log_set_string_table(...)     (void) 0
#define log_finish_string_table(...)  (void) 0

#else

extern unsigned old_PC, old_old_PC;
extern bool log_unused;
extern void log_init (unsigned);
extern void log_append (const char *fmt, ...);
extern void log_add_instr (const decoded_t *op);
extern void log_add_data_mov (const char *format, int addr, int value);
extern void log_add_flag_read (int mask, int value);
extern void log_add_reg_mov (const char *format, int regno, int value);
extern void log_dump_line (const decoded_t*);
extern void do_syscall (int x, int val);
extern void log_set_func_symbol (int, size_t, bool);
extern void log_set_string_table (char*, size_t, int);
extern void log_finish_string_table (void);

typedef struct
{
  bool perf;
  bool logging;
  bool graph, graph_cost;
  bool call_depth;
} need_t;

typedef struct
{
  // The ELF string table associated to the ELF symbol table
  const char *data;
  size_t size;
  int n_entries;

  // Number of ...
  int n_strings;    // ... usable items and indices (inclusive) in strings[]
  int n_funcs;      // ... that have STT_FUNC as symbol table type
  int n_bad;        // ... that are of no use for us, e.g. orphans
  int n_vec;        // ... unused vectors slots
  bool *have;
} string_table_t;

// Some data shared by logging modules logging.c, perf.c, graph.c.
// Objects hosted by logging.c.
extern need_t need;
extern string_table_t string_table;

#endif  // AVRTEST_LOG

extern void load_to_flash (const char*, byte[], byte[], byte[]);
extern void decode_flash (decoded_t[], const byte[]);
extern void set_elf_string_table (char*, size_t, int);
extern void finish_elf_string_table (void);
extern void set_elf_function_symbol (int, size_t, bool);
extern void put_argv (int, byte*);

#include <string.h>

static INLINE bool
str_prefix (const char *prefix, const char *str)
{
  return 0 == strncmp (prefix, str, strlen (prefix));
}

static INLINE bool
str_eq (const char *s1, const char *s2)
{
  return 0 == strcmp (s1, s2);
}

static INLINE bool
str_in (const char *s, const char * const *arr)
{
  for (; *arr; arr++)
    if (str_eq (s, *arr))
      return true;
  return false;
}


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
