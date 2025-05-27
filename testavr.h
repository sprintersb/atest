/*
  This file is part of AVRtest -- A simple simulator for the
  AVR family of 8-bit microcontrollers designed to test the compiler.

  Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
  Copyright (C) 2007 Paulo Marques
  Copyright (C) 2008-2025 Free Software Foundation, Inc.

  AVRtest is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  AVRtest is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AVRtest; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.  */

#ifndef TESTAVR_H
#define TESTAVR_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>

// ---------------------------------------------------------------------------
//     configuration values (in bytes).

#ifdef ISA_XMEGA
#define MAX_RAM_SIZE    (0x1000000)     // 3-byte addresses due to RAMPx.
#else
#define MAX_RAM_SIZE    (0x10000)
#endif // ISA_XMEGA

#define MAX_FLASH_SIZE  (0x40000)       // Must be at least 128KiB
#define MAX_EEPROM_SIZE (16 * 1024)     // .eeprom is read from ELF but unused

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

  // Max word address the PC can ever have.  Anything bigger is bad_PC().
  unsigned max_pc;

  // A word mask to implement PC wrap-around for relative jumps.
  unsigned pc_mask;

  // Maximum number of instructions to be executed,
  // used as a timeout.  Can be set by -m CYCLES
  uint64_t max_insns;

  // Number of instructions simulated so far.
  uint64_t n_insns;

  // Cycles consumed by the program so far.
  uint64_t n_cycles;

  //
  int leave_status, exit_value;

  // filename of the file being executed
  const char *name;

  // ...and with directories stripped off
  const char *short_name;

  // from -stdout=<filename> etc.
  FILE *f_stdin, *f_stdout, *f_stderr;

  // From -log=<filename>.
  FILE *log_stream;
} program_t;

extern program_t program;

extern unsigned cpu_PC;
extern const int io_base;
extern const bool is_xmega;
extern const bool is_tiny;
extern const bool is_avrtest_log;
extern const unsigned invalid_opcode;

extern bool have_syscall[32];

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(*X))

#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))

#if defined (__i386__) || defined (__i868__)
#define FASTCALL __attribute__((fastcall))
#else
#define FASTCALL /* empty */
#endif

#if defined (__MINGW_PRINTF_FORMAT)
#define ATTR_PRINTF(N1, N2)                                     \
  __attribute__((__format__(__MINGW_PRINTF_FORMAT,N1,N2)))
#else
#define ATTR_PRINTF(N1, N2)                                     \
  __attribute__((__format__(printf,N1,N2)))
#endif

enum
  {
    LEAVE_EXIT,
    LEAVE_ABORTED,
    LEAVE_TIMEOUT,
    LEAVE_ELF,
    LEAVE_CODE,
    LEAVE_SYMBOL,
    LEAVE_HOSTIO,
    // Something went badly wrong
    LEAVE_USAGE,
    LEAVE_MEMORY,
    LEAVE_FOPEN,
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

ATTR_PRINTF(2,3)
extern void NOINLINE NORETURN leave (int status, const char *reason, ...);
ATTR_PRINTF(1,2)
extern void qprintf (const char *fmt, ...);
extern byte* cpu_address (int, int);
extern void* get_mem (unsigned, size_t, const char*);

extern const int addr_SREG;
extern const int addr_SPL;
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

extern void log_va (const char*, va_list);
extern bool log_unused;

#ifndef AVRTEST_LOG

// empty placeholders to keep the rest of the code clean

#define log_init(...)          (void) 0
#define log_append(...)        (void) 0
#define log_append_va(...)     (void) 0
#define log_add_instr(...)     (void) 0
#define log_add_data_mov(...)  (void) 0
#define log_add_flag_read(...) (void) 0
#define log_dump_line(...)     (void) 0
#define log_do_syscall(...)    (void) 0
#define log_set_func_symbol(...)      (void) 0
#define log_set_string_table(...)     (void) 0
#define log_finish_string_table(...)  (void) 0
#define log_maybe_change_SP(...)  (void) 0

#else

extern unsigned old_PC, old_old_PC;
extern void log_init (unsigned);
ATTR_PRINTF(1,2)
extern void log_append (const char *fmt, ...);
extern void log_append_va (const char *fmt, va_list);
extern void log_add_instr (const decoded_t *op);
extern void log_add_data_mov (const char *format, int addr, int value);
extern void log_add_flag_read (int mask, int value);
extern void log_add_reg_mov (const char *format, int regno, int value);
extern void log_dump_line (const decoded_t*);
extern void log_do_syscall (int x, int val);
extern void log_set_func_symbol (int, size_t, bool);
extern void log_set_string_table (char*, size_t, int);
extern void log_finish_string_table (void);
extern void log_maybe_change_SP (int);

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

extern int get_nonglitch_SP (void);

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
str_suffix (const char *suffix, const char *str)
{
  size_t lx = strlen (suffix);
  size_t lstr = strlen (str);
  return lx <= lstr && 0 == strcmp (str + lstr - lx, suffix);
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


// Returns n if x = 2^n, and -1 otherwise.
static INLINE int
exact_log2 (unsigned x)
{
    if (x == 0 || (x & (x - 1)))
        return -1;

    int n = 0;
    for (; x != 1; x >>= 1)
        n = n + 1;
    return n;
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
