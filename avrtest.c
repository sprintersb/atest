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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <sys/time.h>

#include "testavr.h"
#include "options.h"
#include "flag-tables.h"
#include "sreg.h"

// ---------------------------------------------------------------------------
// register and port definitions

#define REGX    26
#define REGY    28
#define REGZ    30

#define SREG    (0x3F + IOBASE)
#define SPH     (0x3E + IOBASE)
#define SPL     (0x3D + IOBASE)
#define EIND    (0x3C + IOBASE)
#define RAMPZ   (0x3B + IOBASE)

// ----------------------------------------------------------------------------
// information about program incarnation (avrtest_log, avrtest-xmega, ...)
// use the global constants only at places where performance does not
// matter e.g. in option parsing, so that these modules are independent
// of ISA_XMEGA and AVRTEST_LOG.

// io_base :        load-flash.c:decode_opcode()   map I/O -> RAM
// is_avrtest_log : load-flash.c:load_elf()        load ELF symbols
// is_xmega :       options.c:parse_args()         legal -mmcu=MCU

#ifdef ISA_XMEGA
#define IOBASE  0
#define CX 1
#else
#define IOBASE  0x20
#define CX 0
#endif

#ifdef AVRTEST_LOG
#define IS_AVRTEST_LOG 1
#else
#define IS_AVRTEST_LOG 0
#endif

const int is_xmega = CX;
const int is_avrtest_log = IS_AVRTEST_LOG;
const int io_base = IOBASE;

// ----------------------------------------------------------------------------
// ports like EXIT_PORT used for application <-> simulator interactions 

#define IN_AVRTEST
#include "avrtest.h"


// ----------------------------------------------------------------------------
// holds simulator state and program information.

program_t program;


// ---------------------------------------------------------------------------
// vars that hold AVR states: PC, RAM and Flash

// Word address of current PC and offset into decoded_flash[].
unsigned cpu_PC;

#ifdef ISA_XMEGA
static byte cpu_reg[0x20];
#else
#define cpu_reg cpu_data
#endif /* XMEGA */

// cpu_data is used to store registers (non-xmega), ioport values
// and actual SRAM
static byte cpu_data[MAX_RAM_SIZE];
static byte cpu_eeprom[MAX_EEPROM_SIZE];

// flash
static byte cpu_flash[MAX_FLASH_SIZE];
static decoded_t decoded_flash[MAX_FLASH_SIZE/2];


// ---------------------------------------------------------------------------
// Exit stati as used with leave()

typedef struct
{
  // Holding some keyword that is scanned by board descriptions
  const char *text;
  // Specify kind of failure (not from target program) error
  const char *kind;
  // Whether we exit because of avrtest or usage problems rather than
  // problems in the target program
  int failure;
  // Exit value with -q quiet operation, cf. README
  int quiet_value;
} exit_status_t;

static byte exit_value;

static const exit_status_t exit_status[] = 
  {
    // "EXIT" and "ABORTED" are keywords scanned by board descriptions.
    // -1 : Use exit_value (only set to != 0 with EXIT_STATUS_EXIT)
    [EXIT_STATUS_EXIT]    = { "EXIT",    NULL, EXIT_SUCCESS, -1 },
    [EXIT_STATUS_ABORTED] = { "ABORTED", NULL, EXIT_SUCCESS, EXIT_FAILURE },
    [EXIT_STATUS_TIMEOUT] = { "TIMEOUT", NULL, EXIT_SUCCESS, 10 },
    // Something went badly wrong
    [EXIT_STATUS_FILE]    = { "ABORTED", "file",        EXIT_FAILURE, 11 },
    [EXIT_STATUS_MEMORY]  = { "ABORTED", "memory",      EXIT_FAILURE, 12 },
    [EXIT_STATUS_USAGE]   = { "ABORTED", "usage",       EXIT_FAILURE, 13 },
    [EXIT_STATUS_FATAL]   = { "FATAL ABORTED", "fatal", EXIT_FAILURE, 42 },
  };


// ---------------------------------------------------------------------------
// vars used with -runtime to measure avrtest performance

static struct timeval t_start, t_decode, t_execute, t_load;


static void
time_sub (unsigned long *s, unsigned long *us, double *ms,
          const struct timeval *t1, const struct timeval *t0)
{
  unsigned long s0 = (unsigned long) t0->tv_sec;
  unsigned long s1 = (unsigned long) t1->tv_sec;
  unsigned long u0 = (unsigned long) t0->tv_usec;
  unsigned long u1 = (unsigned long) t1->tv_usec;

  *s = s1 - s0;
  *us = u1 - u0;
  if (u1 < u0)
    {
      (*s)--;
      *us += 1000000UL;
    }
  *ms = 1000. * (*s) + 0.001 * (*us);
}


static void
print_runtime (void)
{
  const program_t *p = &program;
  struct timeval t_end;
  unsigned long r_sec, e_sec, d_sec, l_sec, r_us, e_us, d_us, l_us;
  double r_ms, e_ms, d_ms, l_ms;

  gettimeofday (&t_end, NULL);
  time_sub (&r_sec, &r_us, &r_ms, &t_end, &t_start);
  time_sub (&e_sec, &e_us, &e_ms, &t_end, &t_execute);
  time_sub (&d_sec, &d_us, &d_ms, &t_execute, &t_decode);
  time_sub (&l_sec, &l_us, &l_ms, &t_decode, &t_load);

  printf ("        load: %lu:%02lu.%06lu  = %3lu.%03lu sec  ="
          " %6.2f%%,  %10.3f        bytes/ms, 0x%05x = %u bytes\n",
          l_sec/60, l_sec%60, l_us, l_sec, l_us/1000,
          r_ms > 0.01 ? 100.*l_ms/r_ms : 0.0,
          l_ms > 0.01 ? p->n_bytes/l_ms : 0.0, p->n_bytes, p->n_bytes);

  unsigned n_decoded = p->code_end - p->code_start + 1;
  printf ("      decode: %lu:%02lu.%06lu  = %3lu.%03lu sec  ="
          " %6.2f%%,  %10.3f        bytes/ms, 0x%05x = %u bytes\n",
          d_sec/60, d_sec%60, d_us, d_sec, d_us/1000,
          r_ms > 0.01 ? 100.*d_ms/r_ms : 0.0,
          d_ms > 0.01 ? n_decoded/d_ms : 0.0, n_decoded, n_decoded);

  printf ("     execute: %lu:%02lu.%06lu  = %3lu.%03lu sec  ="
          " %6.2f%%,  %10.3f instructions/ms\n",
          e_sec/60, e_sec%60, e_us, e_sec, e_us/1000,
          r_ms > 0.01 ? 100.*e_ms/r_ms : 0.0,
          e_ms > 0.01 ? p->n_insns/e_ms : 0.0);

  printf (" avrtest run: %lu:%02lu.%06lu  = %3lu.%03lu sec  ="
          " %6.2f%%,  %10.3f instructions/ms\n",
          r_sec/60, r_sec%60, r_us, r_sec, r_us/1000, 100.,
          r_ms > 0.01 ? p->n_insns/r_ms : 0.0);
}


// Skip any output if -q (quiet) is on
void qprintf (const char *fmt, ...)
{
  if (!options.do_quiet)
    {
      va_list args;
      va_start (args, fmt);
      vprintf (fmt, args);
      va_end (args);
    }
}


void NOINLINE NORETURN
leave (int n, const char *reason, ...)
{
  const exit_status_t *status = & exit_status[n];
  va_list args;

  // make sure we print the last log line before leaving
  log_dump_line (0);

  qprintf ("\n");

  if (options.do_runtime
      && EXIT_SUCCESS == status->failure)
    print_runtime();

  if (!options.do_quiet)
    {
      va_start (args, reason);

      printf (" exit status: %s\n"
              "      reason: ", exit_value
              ? exit_status[EXIT_STATUS_ABORTED].text
              : status->text);
      vprintf (reason, args);
      printf ("\n"
              "     program: %s\n",
              options.program_name ? options.program_name : "-not set-");
      if (program.entry_point != 0)
        printf (" entry point: %06x\n", program.entry_point);
      printf ("exit address: %06x\n"
              "total cycles: %u\n"
              "total instr.: %u\n\n", cpu_PC * 2, program.n_cycles,
              program.n_insns);

      va_end (args);
      fflush (stdout);

      exit (status->failure);
    }

  fflush (stdout);

  if (status->failure != EXIT_SUCCESS)
    {
      FILE *out = stderr;
      va_start (args, reason);

      fprintf (out, "\n%s: %s error: ", options.self, status->kind);
      vfprintf (out, reason, args);
      fprintf (out, "\n");
      fflush (out);

      va_end (args);
    }

  exit (n == EXIT_STATUS_EXIT ? exit_value : status->quiet_value);
}

// ----------------------------------------------------------------------------
//     ioport / ram / flash, read / write entry points

// Adding magic to some I/O ports as we read from or write to them

static NOINLINE void
data_write_magic_port (int address, int value)
{
  // add code here to handle special events
  switch (address)
    {
    case STDOUT_PORT:
      if (options.do_stdout)
        putchar (0xff & value);
      else
        // default action, just store the value
        cpu_data[address] = value;
      break;
    case EXIT_PORT:
      log_add_data_mov ("(%s)<-%02x ", address, value & 0xff);
      leave (EXIT_STATUS_EXIT, "exit(%d) function called", exit_value = value);
      break;
    case ABORT_PORT:
      log_add_data_mov ("(%s)<-%02x ", address, value & 0xff);
      leave (EXIT_STATUS_ABORTED, "abort function called");
      break;
    case LOG_PORT:
      if (IS_AVRTEST_LOG)
        do_log_port_cmd (value);
      else
        cpu_data[address] = value;
      break;
    default:
      cpu_data[address] = value;
      break;
    }
}

static NOINLINE int
data_read_magic_port (int address)
{
  // add code here to handle special events

  if (address == STDIN_PORT && options.do_stdin)
    return getchar();

  if (address == TICKS_PORT && options.do_ticks)
    // update TICKS_PORT only on access of first byte to avoid glitches
    flush_ticks_port (address);

  // default action, just read the value
  return cpu_data[address];
}

// Lowest level vanilla memory accessors, no magic, no logging.

static INLINE int
data_read_byte_raw (int address)
{
  return cpu_data[address];
}

static INLINE void
data_write_byte_raw (int address, int value)
{
  cpu_data[address] = value;
}

static INLINE int
flash_read_byte (int address)
{
  address &= arch.flash_addr_mask;
  // add code here to handle special events
  return cpu_flash[address];
}

// Memory accessors: with logging and with magic
// Used by memory accessing instructions like IN, OUT,
// LD*, ST* ... that might trigger magic.

static INLINE int
data_read_magic_byte (int address)
{
  int ret;
  if (address == STDIN_PORT
      || (IS_AVRTEST_LOG && address == TICKS_PORT))
    ret = data_read_magic_port (address);
  else
    // default action, just read the value
    ret = cpu_data[address];

  log_add_data_mov (address == SREG ? "(SREG)->'%s'" : "(%s)->%02x ",
                    address, ret);
  return ret;
}

static INLINE void
data_write_magic_byte (int address, int value)
{
  if (address <= LAST_MAGIC_OUT_PORT && address >= FIRST_MAGIC_OUT_PORT)
    data_write_magic_port (address, value);
  else
    // default action, just store the value
    cpu_data[address] = value;

  log_add_data_mov (address == SREG ? "(SREG)<-'%s' " : "(%s)<-%02x ",
                    address, value & 0xff);
}


// Memory accessors: with logging but no magic
// Used by PUSH, POP, CALL, RET ...  These instrucion should
// never ever need or do magic actions.

static INLINE int
data_read_byte (int address)
{
  int ret = cpu_data[address];
  log_add_data_mov (address == SREG ? "(SREG)->'%s' " : "(%s)->%02x ",
                    address, ret);
  return ret;
}

// write a byte to memory / ioport / register

static INLINE void
data_write_byte (int address, int value)
{
  log_add_data_mov (address == SREG ? "(SREG)<-'%s' " : "(%s)<-%02x ",
                    address, value & 0xff);
  cpu_data[address] = value;
}

// get_reg / put_reg are just placeholders for read/write calls where we can
// be sure that the adress is < 32

static INLINE byte
get_reg (int regno)
{
  log_append ("(R%d)->%02x ", regno, cpu_reg[regno]);
  return cpu_reg[regno];
}

static INLINE void
put_reg (int regno, byte value)
{
  log_append ("(R%d)<-%02x ", regno, value);
  cpu_reg[regno] = value;
}

static INLINE int
get_word_reg (int regno)
{
  int ret = cpu_reg[regno] | (cpu_reg[regno + 1] << 8);
  log_append ("(R%d)->%04x ", regno, ret);
  return ret;
}

static INLINE void
put_word_reg (int regno, int value)
{
  log_append ("(R%d)<-%04x ", regno, value & 0xFFFF);
  cpu_reg[regno] = value;
  cpu_reg[regno + 1] = value >> 8;
}

// read a word from memory / ioport / register

static INLINE int
data_read_word (int address)
{
  int ret = (data_read_byte_raw (address)
             | (data_read_byte_raw (address + 1) << 8));
  log_add_data_mov ("(%s)->%04x ", address, ret);
  return ret;
}

// write a word to memory / ioport / register

static INLINE void
data_write_word (int address, int value)
{
  value &= 0xffff;
  log_add_data_mov ("(%s)<-%04x ", address, value);
  data_write_byte_raw (address, value & 0xFF);
  data_write_byte_raw (address + 1, value >> 8);
}

// ----------------------------------------------------------------------------
// extern functions to make logging.c independent of ISA_XMEGA

const int addr_SREG       = SREG;
const int addr_TICKS_PORT = TICKS_PORT;

const magic_t named_port[] =
  {
    { SPL,   1, 1, 1, "SPL"   },
    { SPH,   1, 1, 0, "SPH"   },
    { EIND,  1, 1, 0, "EIND"  },
    { RAMPZ, 1, 1, 0, "RAMPZ" },

    { LOG_PORT,       0, 1, 0, "LOG_PORT" },
    { TICKS_PORT,     1, 1, 0, "TICKS_PORT"   },
    { TICKS_PORT + 1, 1, 1, 0, "TICKS_PORT+1" },
    { TICKS_PORT + 2, 1, 1, 0, "TICKS_PORT+2" },
    { TICKS_PORT + 3, 1, 1, 0, "TICKS_PORT+3" },

    { STDOUT_PORT, 0, 1, 0, "STDOUT_PORT" },
    { STDIN_PORT,  1, 0, 0, "STDIN_PORT" },
    { EXIT_PORT,   0, 1, 0, "EXIT_PORT" },
    { ABORT_PORT,  0, 1, 0, "ABORT_PORT" },
    { 0, 0, 0, 0, NULL }
  };
  
int log_data_read_SP (void)
{
  return data_read_byte_raw (SPL) | (data_read_byte_raw (SPH) << 8);
}

byte* log_cpu_address (int address, int flash)
{
  return flash
    ? cpu_flash + address
    : cpu_data + address;
}

byte log_data_read_byte (int address, int nraw)
{
  return (nraw)
    ? data_read_byte (address)
    : data_read_byte_raw (address);
}

void log_data_write_byte (int address, int value, int nraw)
{
  if (nraw)
    data_write_byte (address, value);
  else
    data_write_byte_raw (address, value);
}

void log_put_word_reg (int regno, int value, int nraw)
{
  if (nraw)
    put_word_reg (regno, value);
  else
    {
      cpu_reg[regno] = value;
      cpu_reg[regno + 1] = value >> 8;
    }
}

void log_data_write_word (int address, int value, int nraw)
{
  if (nraw)
    data_write_word (address, value);
  else
    {
      value &= 0xffff;
      data_write_byte_raw (address, value & 0xFF);
      data_write_byte_raw (address + 1, value >> 8);
    }
}

void set_function_symbol (int addr, const char *fname, int is_func)
{
  log_set_func_symbol (addr, fname, is_func);
}

// Memory allocation that never fails (returns NULL).

void* get_mem (unsigned n, size_t size)
{
#ifdef AVRTEST_LOG
  void *p = calloc (n, size);
  if (p == NULL)
    leave (EXIT_STATUS_MEMORY, "out of memory allocating %u bytes",
           (unsigned) (n * size));
  return p;
#else
  leave (EXIT_STATUS_FATAL, "alloc_mem must be unreachable");
  return NULL;
#endif
}


// ----------------------------------------------------------------------------
//     flag manipulation functions

static INLINE void
update_flags (int flags, int new_values)
{
  int sreg = data_read_byte (SREG);
  sreg = (sreg & ~flags) | new_values;
  data_write_byte (SREG, sreg);
}

static INLINE int
get_carry (void)
{
  return (data_read_byte_raw (SREG) & FLAG_C) != 0;
}

// fast flag update tables to avoid conditional branches on frequently
// used operations
static INLINE unsigned
FUT_ADD_SUB_INDEX (unsigned v1, unsigned v2, unsigned res)
{
  /*   ((v1 & 0x08) << 9)
     | ((v2 & 0x08) << 8)
     | ((v1 & 0x80) << 3)
     | ((v2 & 0x80) << 2)
     | (res & 0x1FF) */
  unsigned v = 2 * (v1 & 0x88) + (v2 & 0x88);
  v *= 0x104;
  return (res & 0x1ff) | (v & 0x1e00);
}

#define FUT_ADDSUB16_INDEX(v1, res)                             \
  (((((v1) >> 8) & 0x80) << 3) | (((res) >> 8) & 0x1FF))

// ----------------------------------------------------------------------------
//     helper functions

static INLINE void
add_program_cycles (dword cycles)
{
  program.n_cycles += cycles;
}


static INLINE void
push_byte (int value)
{
  int sp = data_read_word (SPL);
  // temporary hack to disallow growing the stack over the reserved
  // register area
  if (sp < 0x40 + IOBASE)
    leave (EXIT_STATUS_ABORTED, "stack pointer overflow (SP = 0x%04x)", sp);
  data_write_byte (sp--, value);
  data_write_word (SPL, sp);
}

static int
pop_byte(void)
{
  int sp = data_read_word (SPL);
  data_write_word (SPL, ++sp);
  return data_read_byte (sp);
}

static INLINE void
push_PC (void)
{
  int sp = data_read_word (SPL);
  // temporary hack to disallow growing the stack over the reserved
  // register area
  if (sp < 0x40 + IOBASE)
    leave (EXIT_STATUS_ABORTED, "stack pointer overflow (SP = 0x%04x)", sp);
  data_write_byte (sp--, cpu_PC);
  data_write_byte (sp--, cpu_PC >> 8);
  if (arch.pc_3bytes)
    data_write_byte (sp--, cpu_PC >> 16);
  data_write_word (SPL, sp);
}

static NOINLINE NORETURN void
bad_PC (unsigned pc)
{
  leave (EXIT_STATUS_ABORTED, "program counter 0x%x out of bounds "
         "(0x%x--0x%x)", 2 * pc, program.code_start, program.code_end - 1);
}

static INLINE void
pop_PC (void)
{
  unsigned pc = 0;
  int sp = data_read_word (SPL);
  if (arch.pc_3bytes)
    {
      pc = data_read_byte (++sp) << 16;
      if (pc >= MAX_FLASH_SIZE / 2)
        {
          pc |= data_read_byte (++sp) << 8;
          pc |= data_read_byte (++sp);
          bad_PC (pc);
        }
    }
  pc |= data_read_byte (++sp) << 8;
  pc |= data_read_byte (++sp);
  data_write_word (SPL, sp);
  cpu_PC = pc;
}


// perform the addition and set the appropriate flags
static INLINE void
do_addition_8 (int rd, int rr, int carry)
{
  int value1 = get_reg (rd);
  int value2 = get_reg (rr);
  int result = value1 + value2 + carry;
  put_reg (rd, result);

  int sreg = flag_update_table_add8[FUT_ADD_SUB_INDEX (value1, value2, result)];
  update_flags (FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

// perform the left shift and set the appropriate flags
static INLINE void
do_shift_8 (int rd, int carry)
{
  int value = get_reg (rd);
  int result = value + value + carry;
  put_reg (rd, result);

  int sreg = flag_update_table_add8[FUT_ADD_SUB_INDEX (value, value, result)];
  update_flags (FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

// perform the subtraction and set the appropriate flags
static INLINE void
do_subtraction_8 (int rd, int value1, int value2, int carry,
                  int use_carry, int writeback)
{
  int result = value1 - value2 - carry;
  if (writeback)
    put_reg (rd, result);
  int sreg = flag_update_table_sub8[FUT_ADD_SUB_INDEX (value1, value2, result)];
  unsigned flag = (data_read_byte_raw (SREG) | ~FLAG_Z) | (use_carry-1);
  sreg &= flag;
  update_flags (FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

static INLINE void
store_logical_result (int rd, int result)
{
  put_reg (rd, result);
  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z,
                flag_update_table_logical[result]);
}

/* 10q0 qq0d dddd 1qqq | LDD */

static INLINE void
load_indirect (int rd, int ind_register, int adjust, int displacement_bits)
{
  //TODO: use RAMPx registers to address more than 64kb of RAM
  int ind_value = get_word_reg (ind_register);

  if (adjust < 0)
    ind_value += adjust;
  put_reg (rd, data_read_magic_byte (ind_value + displacement_bits));
  if (adjust > 0)
    ind_value += adjust;
  if (adjust)
    put_word_reg (ind_register, ind_value);
}

static INLINE void 
store_indirect (int rd, int ind_register, int adjust, int displacement_bits)
{
  //TODO: use RAMPx registers to address more than 64kb of RAM
  int ind_value = get_word_reg (ind_register);

  if (adjust < 0)
    ind_value += adjust;
  data_write_magic_byte (ind_value + displacement_bits, get_reg (rd));
  if (adjust > 0)
    ind_value += adjust;
  if (adjust)
    put_word_reg (ind_register, ind_value);
}

static INLINE void
load_program_memory (int rd, int use_RAMPZ, int incr)
{
  int address = get_word_reg (REGZ);
  if (use_RAMPZ)
    address |= data_read_byte (RAMPZ) << 16;
  put_reg (rd, flash_read_byte (address));
  if (incr)
    {
      address++;
      put_word_reg (REGZ, address & 0xFFFF);
      if (use_RAMPZ)
        data_write_byte (RAMPZ, address >> 16);
    }
}

static INLINE void
skip_instruction_on_condition (int condition, int words_to_skip)
{
  if (condition)
    {
      cpu_PC = (cpu_PC + words_to_skip) & PC_VALID_MASK;
      add_program_cycles (words_to_skip);
    }
}

static INLINE void
branch_on_sreg_condition (int rd, int rr, int flag_value)
{
  int flag = data_read_byte (SREG) & rr;
  log_add_flag_read (rr, flag);
  if ((flag != 0) == flag_value)
    {
      int8_t delta = rd;
      cpu_PC = (cpu_PC + delta) & PC_VALID_MASK;
      add_program_cycles (1);
    }
}

static INLINE void
rotate_right (int rd, int value, int top_bit /* 0 or 0x100 */)
{
  value |= top_bit;
  put_reg (rd, value >> 1);

  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C,
                flag_update_table_ror8[value]);
}

static INLINE void
do_multiply (int rd, int rr, int signed1, int signed2, int shift)
{
  int v1 = signed1 ? ((signed char) get_reg (rd)) : get_reg (rd);
  int v2 = signed2 ? ((signed char) get_reg (rr)) : get_reg (rr);
  int result = (v1 * v2) & 0xFFFF;

  int sreg = (result & 0x8000) >> (15 - FLAG_C_BIT);
  result <<= shift;
  sreg |= FLAG_Z & - (result == 0);
  update_flags (FLAG_Z | FLAG_C, sreg);
  put_word_reg (0, result);
}

// handle illegal instructions
static OP_FUNC_TYPE func_ILLEGAL (int rd, int rr)
{
  log_append (".word 0x%04x", rr);
  leave (EXIT_STATUS_ABORTED, "illegal opcode 0x%04x", rr);
}

// ----------------------------------------------------------------------------
//     opcode execution functions

/* opcodes with no operands */

/* 1001 0101 1001 1000 | BREAK */
static OP_FUNC_TYPE func_BREAK (int rd, int rr)
{
  // FIXME: Acts currently like NOP.  Do helpful feature here
}

/* 1001 0101 0001 1001 | EICALL */
static OP_FUNC_TYPE func_EICALL (int rd, int rr)
{
  if (arch.has_eind)
    {
      push_PC();
      cpu_PC = get_word_reg (REGZ) | (data_read_byte (EIND) << 16);
      if (cpu_PC >= MAX_FLASH_SIZE / 2)
        bad_PC (cpu_PC);
    }
  else
    {
      func_ILLEGAL (0, 0x9519);
    }
}

/* 1001 0100 0001 1001 | EIJMP */
static OP_FUNC_TYPE func_EIJMP (int rd, int rr)
{
  if (arch.has_eind)
    {
      cpu_PC = get_word_reg (REGZ) | (data_read_byte (EIND) << 16);
      if (cpu_PC >= MAX_FLASH_SIZE / 2)
        bad_PC (cpu_PC);
    }
  else
    {
      func_ILLEGAL (0, 0x9419);
    }
}

/* 1001 0101 1101 1000 | ELPM */
static OP_FUNC_TYPE func_ELPM (int rd, int rr)
{
  load_program_memory (0, 1, 0);
}

/* 1001 0101 1111 1000 | ESPM */
static OP_FUNC_TYPE func_ESPM (int rd, int rr)
{
  func_ILLEGAL (0, 0x95F8);
  //TODO
}

/* 1001 0101 0000 1001 | ICALL */
static OP_FUNC_TYPE func_ICALL (int rd, int rr)
{
  push_PC();
  cpu_PC = get_word_reg (REGZ);
  add_program_cycles (arch.pc_3bytes);
}

/* 1001 0100 0000 1001 | IJMP */
static OP_FUNC_TYPE func_IJMP (int rd, int rr)
{
  cpu_PC = get_word_reg (REGZ);
}

/* 1001 0101 1100 1000 | LPM */
static OP_FUNC_TYPE func_LPM (int rd, int rr)
{
  load_program_memory (0, 0, 0);
}

/* 0000 0000 0000 0000 | NOP */
static OP_FUNC_TYPE func_NOP (int rd, int rr)
{
}

/* 1001 0101 0000 1000 | RET */
static OP_FUNC_TYPE func_RET (int rd, int rr)
{
  pop_PC();
  add_program_cycles (arch.pc_3bytes);
}

/* 1001 0101 0001 1000 | RETI */
static OP_FUNC_TYPE func_RETI (int rd, int rr)
{
  func_RET (rd, rr);
  update_flags (FLAG_I, FLAG_I);
}

/* 1001 0101 1000 1000 | SLEEP */
static OP_FUNC_TYPE func_SLEEP (int rd, int rr)
{
  // we don't have anything to wake us up, so just pretend we wake
  // up immediately
}

/* 1001 0101 1110 1000 | SPM */
static OP_FUNC_TYPE func_SPM (int rd, int rr)
{
  func_ILLEGAL (0, 0x95E8);
  //TODO
}

/* 1001 0101 1010 1000 | WDR */
static OP_FUNC_TYPE func_WDR (int rd, int rr)
{
  // we don't have a watchdog, so do nothing
}


/* opcodes with two 5-bit register (Rd and Rr) operands */
/* 0001 11rd dddd rrrr | ADC or ROL */
static OP_FUNC_TYPE func_ADC (int rd, int rr)
{
  do_addition_8 (rd, rr, get_carry());
}

/* 0001 11rd dddd dddd | ROL */
static OP_FUNC_TYPE func_ROL (int rd, int rr)
{
  do_shift_8 (rd, get_carry());
}

/* 0000 11rd dddd rrrr | ADD or LSL */
static OP_FUNC_TYPE func_ADD (int rd, int rr)
{
  do_addition_8 (rd, rr, 0);
}

/* 0000 11rd dddd dddd | LSL */
static OP_FUNC_TYPE func_LSL (int rd, int rr)
{
  do_shift_8 (rd, 0);
}

/* 0010 00rd dddd rrrr | AND or TST */
static OP_FUNC_TYPE func_AND (int rd, int rr)
{
  int result = get_reg (rd) & get_reg (rr);
  store_logical_result (rd, result);
}

/* 0010 00rd dddd dddd | TST */
static OP_FUNC_TYPE func_TST (int rd, int rr)
{
  int result = get_reg (rd);
  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z,
                flag_update_table_logical[result]);
}

/* 0001 01rd dddd rrrr | CP */
static OP_FUNC_TYPE func_CP (int rd, int rr)
{
  do_subtraction_8 (0, get_reg (rd), get_reg (rr), 0, 0, 0);
}

/* 0000 01rd dddd rrrr | CPC */
static OP_FUNC_TYPE func_CPC (int rd, int rr)
{
  do_subtraction_8 (0, get_reg (rd), get_reg (rr), get_carry(), 1, 0);
}

/* 0001 00rd dddd rrrr | CPSE skipping 1 word */
static OP_FUNC_TYPE func_CPSE (int rd, int rr)
{
  skip_instruction_on_condition (get_reg (rd) == get_reg (rr), 1);
}

/* 0001 00rd dddd rrrr | CPSE skipping 2 words */
static OP_FUNC_TYPE func_CPSE2 (int rd, int rr)
{
  skip_instruction_on_condition (get_reg (rd) == get_reg (rr), 2);
}

/* 0010 01rd dddd rrrr | EOR or CLR */
static OP_FUNC_TYPE func_EOR (int rd, int rr)
{
  int result = get_reg (rd) ^ get_reg (rr);
  store_logical_result (rd, result);
}

/* 0010 01rd dddd rrrr | CLR */
static OP_FUNC_TYPE func_CLR (int rd, int rr)
{
  store_logical_result (rd, 0);
}

/* 0010 11rd dddd rrrr | MOV */
static INLINE OP_FUNC_TYPE func_MOV (int rd, int rr)
{
  put_reg (rd, get_reg (rr));
}

/* 1001 11rd dddd rrrr | MUL */
static OP_FUNC_TYPE func_MUL (int rd, int rr)
{
  do_multiply (rd, rr, 0, 0, 0);
}

/* 0010 10rd dddd rrrr | OR */
static OP_FUNC_TYPE func_OR (int rd, int rr)
{
  int result = get_reg (rd) | get_reg (rr);
  store_logical_result (rd, result);
}

/* 0000 10rd dddd rrrr | SBC */
static OP_FUNC_TYPE func_SBC (int rd, int rr)
{
  do_subtraction_8 (rd, get_reg (rd), get_reg (rr), get_carry(), 1, 1);
}

/* 0001 10rd dddd rrrr | SUB */
static OP_FUNC_TYPE func_SUB (int rd, int rr)
{
  do_subtraction_8 (rd, get_reg (rd), get_reg (rr), 0, 0, 1);
}


/* opcode with a single register (Rd) as operand */
/* 1001 010d dddd 0101 | ASR */
static OP_FUNC_TYPE func_ASR (int rd, int rr)
{
  int value = (int8_t) get_reg (rd);
  rotate_right (rd, value & 0x1ff, 0);
}

/* 1001 010d dddd 0000 | COM */
static OP_FUNC_TYPE func_COM (int rd, int rr)
{
  int result = 0xFF & ~get_reg (rd);
  put_reg (rd, result);
  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C,
                flag_update_table_logical[result] | FLAG_C);
}

/* 1001 010d dddd 1010 | DEC */
static OP_FUNC_TYPE func_DEC (int rd, int rr)
{
  int result = (get_reg (rd) - 1) & 0xFF;
  put_reg (rd, result);
  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z,
                flag_update_table_dec[result]);
}

/* 1001 000d dddd 0110 | ELPM */
static OP_FUNC_TYPE func_ELPM_Z (int rd, int rr)
{
  load_program_memory (rd, 1, 0);
}

/* 1001 000d dddd 0111 | ELPM */
static OP_FUNC_TYPE func_ELPM_Z_incr (int rd, int rr)
{
  load_program_memory (rd, 1, 1);
}

/* 1001 010d dddd 0011 | INC */
static OP_FUNC_TYPE func_INC (int rd, int rr)
{
  int result = (get_reg (rd) + 1) & 0xFF;
  put_reg (rd, result);
  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z,
                flag_update_table_inc[result]);
}

/* 1001 000d dddd 0000 | LDS */
static OP_FUNC_TYPE func_LDS (int rd, int rr)
{
  //TODO:RAMPD
  put_reg (rd, data_read_magic_byte (rr));
}

/* 1001 000d dddd 1100 | LD */
static OP_FUNC_TYPE func_LD_X (int rd, int rr)
{
  load_indirect (rd, REGX, 0, 0);
}

/* 1001 000d dddd 1110 | LD */
static OP_FUNC_TYPE func_LD_X_decr (int rd, int rr)
{
  load_indirect (rd, REGX, -1, 0);
}

/* 1001 000d dddd 1101 | LD */
static OP_FUNC_TYPE func_LD_X_incr (int rd, int rr)
{
  load_indirect (rd, REGX, 1, 0);
}

/* 1001 000d dddd 1010 | LD */
static OP_FUNC_TYPE func_LD_Y_decr (int rd, int rr)
{
  load_indirect (rd, REGY, -1, 0);
}

/* 1001 000d dddd 1001 | LD */
static OP_FUNC_TYPE func_LD_Y_incr (int rd, int rr)
{
  load_indirect (rd, REGY, 1, 0);
}

/* 1001 000d dddd 0010 | LD */
static OP_FUNC_TYPE func_LD_Z_decr (int rd, int rr)
{
  load_indirect (rd, REGZ, -1, 0);
}

/* 1001 000d dddd 0010 | LD */
static OP_FUNC_TYPE func_LD_Z_incr (int rd, int rr)
{
  load_indirect (rd, REGZ, 1, 0);
}

/* 1001 000d dddd 0100 | LPM Z */
static OP_FUNC_TYPE func_LPM_Z (int rd, int rr)
{
  load_program_memory (rd, 0, 0);
}

/* 1001 000d dddd 0101 | LPM Z+ */
static OP_FUNC_TYPE func_LPM_Z_incr (int rd, int rr)
{
  load_program_memory (rd, 0, 1);
}

/* 1001 010d dddd 0110 | LSR */
static OP_FUNC_TYPE func_LSR (int rd, int rr)
{
  rotate_right (rd, get_reg (rd), 0);
}

/* 1001 010d dddd 0001 | NEG */
static OP_FUNC_TYPE func_NEG (int rd, int rr)
{
  do_subtraction_8 (rd, 0, get_reg (rd), 0, 0, 1);
}

/* 1001 000d dddd 1111 | POP */
static OP_FUNC_TYPE func_POP (int rd, int rr)
{
  put_reg (rd, pop_byte());
}

static INLINE void
xmega_atomic (int regno, int op, const char *mnemo, int magic_p)
{
#ifndef ISA_XMEGA
  leave (EXIT_STATUS_ABORTED, "illegal instruction %s", mnemo);
#endif

  int mask = get_reg (regno);
  int address = get_word_reg (REGZ);
  int val = (magic_p
             ? data_read_magic_byte (address)
             : data_read_byte (address));

  put_reg (regno, val);

  switch (op)
    {
    case ID_XCH: val = mask; break;
    case ID_LAS: val |=  mask; break;
    case ID_LAC: val &= ~mask; break;
    case ID_LAT: val ^=  mask; break;
    }

  if (magic_p)
    data_write_magic_byte (address, val);
  else
    data_write_byte (address, val);
}

/* 1001 001d dddd 0100 | XCH */
static OP_FUNC_TYPE func_XCH (int rd, int rr)
{
  xmega_atomic (rd, ID_XCH, "XCH", 1);
}

/* 1001 001d dddd 0101 | LAS */
static OP_FUNC_TYPE func_LAS (int rd, int rr)
{
  xmega_atomic (rd, ID_LAS, "LAS", 0);
}

/* 1001 001d dddd 0110 | LAC */
static OP_FUNC_TYPE func_LAC (int rd, int rr)
{
  xmega_atomic (rd, ID_LAC, "LAC", 0);
}

/* 1001 001d dddd 0111 | LAT */
static OP_FUNC_TYPE func_LAT (int rd, int rr)
{
  xmega_atomic (rd, ID_LAT, "LAT", 0);
}

/* 1001 001d dddd 1111 | PUSH */
static OP_FUNC_TYPE func_PUSH (int rd, int rr)
{
  push_byte (get_reg (rd));
}

/* 1001 010d dddd 0111 | ROR */
static OP_FUNC_TYPE func_ROR (int rd, int rr)
{
  rotate_right (rd, get_reg (rd), get_carry() << 8);
}

/* 1001 001d dddd 0000 | STS */
static OP_FUNC_TYPE func_STS (int rd, int rr)
{
  //TODO:RAMPD
  data_write_magic_byte (rr, get_reg (rd));
}

/* 1001 001d dddd 1100 | ST */
static OP_FUNC_TYPE func_ST_X (int rd, int rr)
{
  store_indirect (rd, REGX, 0, 0);
}

/* 1001 001d dddd 1110 | ST */
static OP_FUNC_TYPE func_ST_X_decr (int rd, int rr)
{
  store_indirect (rd, REGX, -1, 0);
}

/* 1001 001d dddd 1101 | ST */
static OP_FUNC_TYPE func_ST_X_incr (int rd, int rr)
{
  store_indirect (rd, REGX, 1, 0);
}

/* 1001 001d dddd 1010 | ST */
static OP_FUNC_TYPE func_ST_Y_decr (int rd, int rr)
{
  store_indirect (rd, REGY, -1, 0);
}

/* 1001 001d dddd 1001 | ST */
static OP_FUNC_TYPE func_ST_Y_incr (int rd, int rr)
{
  store_indirect (rd, REGY, 1, 0);
}

/* 1001 001d dddd 0010 | ST */
static OP_FUNC_TYPE func_ST_Z_decr (int rd, int rr)
{
  store_indirect (rd, REGZ, -1, 0);
}

/* 1001 001d dddd 0001 | ST */
static OP_FUNC_TYPE func_ST_Z_incr (int rd, int rr)
{
  store_indirect (rd, REGZ, 1, 0);
}

/* 1001 010d dddd 0010 | SWAP */
static OP_FUNC_TYPE func_SWAP (int rd, int rr)
{
  int value = get_reg (rd);
  put_reg (rd, ((value << 4) & 0xF0) | ((value >> 4) & 0x0F));
}

/* opcodes with a register (Rd) and a constant data (K) as operands */
/* 0111 KKKK dddd KKKK | CBR or ANDI */
static OP_FUNC_TYPE func_ANDI (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  int result = get_reg (rd) & rr;
  store_logical_result (rd, result);
}

/* 0011 KKKK dddd KKKK | CPI */
static OP_FUNC_TYPE func_CPI (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  do_subtraction_8 (0, get_reg (rd), rr, 0, 0, 0);
}

/* 1110 KKKK dddd KKKK | LDI or SER */
static INLINE OP_FUNC_TYPE func_LDI (int rd, int rr)
{
  put_reg (rd, rr);
}

/* 0110 KKKK dddd KKKK | SBR or ORI */
static OP_FUNC_TYPE func_ORI (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  int result = get_reg (rd) | rr;
  store_logical_result (rd, result);
}

/* 0100 KKKK dddd KKKK | SBCI */
static OP_FUNC_TYPE func_SBCI (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  do_subtraction_8 (rd, get_reg (rd), rr, get_carry(), 1, 1);
}

/* 0101 KKKK dddd KKKK | SUBI */
static OP_FUNC_TYPE func_SUBI (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  do_subtraction_8 (rd, get_reg (rd), rr, 0, 0, 1);
}


/* opcodes with a register (Rd) and a register bit number (b) as operands */
/* 1111 100d dddd 0bbb | BLD */
static OP_FUNC_TYPE func_BLD (int rd, int rr)
{
  int value = get_reg (rd) | rr;
  unsigned flag = (data_read_byte (SREG) >> FLAG_T_BIT) & 1;
  value &= ~rr | -flag;
  put_reg (rd, value);
}

/* 1111 101d dddd 0bbb | BST */
static OP_FUNC_TYPE func_BST (int rd, int rr)
{
  int bit = get_reg (rd) & rr;
  update_flags (FLAG_T, bit ? FLAG_T : 0);
}

/* 1111 110d dddd 0bbb | SBRC skipping 1 word */
static OP_FUNC_TYPE func_SBRC (int rd, int rr)
{
  skip_instruction_on_condition (!(get_reg (rd) & rr), 1);
}

/* 1111 110d dddd 0bbb | SBRC skipping 2 words */
static OP_FUNC_TYPE func_SBRC2 (int rd, int rr)
{
  skip_instruction_on_condition (!(get_reg (rd) & rr), 2);
}

/* 1111 111d dddd 0bbb | SBRS skipping 1 word */
static OP_FUNC_TYPE func_SBRS (int rd, int rr)
{
  skip_instruction_on_condition (get_reg (rd) & rr, 1);
}

/* 1111 111d dddd 0bbb | SBRS skipping 2 words */
static OP_FUNC_TYPE func_SBRS2 (int rd, int rr)
{
  skip_instruction_on_condition (get_reg (rd) & rr, 2);
}

/* opcodes with a relative 7-bit address (k) and a register bit number (b)
   as operands */
/* 1111 01kk kkkk kbbb | BRBC */
static OP_FUNC_TYPE func_BRBC (int rd, int rr)
{
  branch_on_sreg_condition (rd, rr, 0);
}

/* 1111 00kk kkkk kbbb | BRBS */
static OP_FUNC_TYPE func_BRBS (int rd, int rr)
{
  branch_on_sreg_condition (rd, rr, 1);
}


/* opcodes with a 6-bit address displacement (q) and a register (Rd)
   as operands */
/* 10q0 qq0d dddd 1qqq | LDD */
static OP_FUNC_TYPE func_LDD_Y (int rd, int rr)
{
  load_indirect (rd, REGY, 0, rr);
}

/* 10q0 qq0d dddd 0qqq | LDD */
static OP_FUNC_TYPE func_LDD_Z (int rd, int rr)
{
  load_indirect (rd, REGZ, 0, rr);
}

/* 10q0 qq1d dddd 1qqq | STD */
static OP_FUNC_TYPE func_STD_Y (int rd, int rr)
{
  store_indirect (rd, REGY, 0, rr);
}

/* 10q0 qq1d dddd 0qqq | STD */
static OP_FUNC_TYPE func_STD_Z (int rd, int rr)
{
  store_indirect (rd, REGZ, 0, rr);
}


/* opcodes with a absolute 22-bit address (k) operand */
/* 1001 010k kkkk 110k | JMP */
static OP_FUNC_TYPE func_JMP (int rd, int rr)
{
  cpu_PC = rr | (rd << 16);
}

/* 1001 010k kkkk 111k | CALL */
static OP_FUNC_TYPE func_CALL (int rd, int rr)
{
  push_PC();
  cpu_PC = rr | (rd << 16);
  add_program_cycles (arch.pc_3bytes);
}


/* opcode with a sreg bit select (s) operand */
/* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
/* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
/* 1001 0100 1sss 1000 | BCLR */
static OP_FUNC_TYPE func_BCLR (int rd, int rr)
{
  update_flags (rd, 0);
}

/* 1001 0100 0sss 1000 | BSET */
static OP_FUNC_TYPE func_BSET (int rd, int rr)
{
  update_flags (rd, rd);
}


/* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
/* 1001 0110 KKdd KKKK | ADIW */
static OP_FUNC_TYPE func_ADIW (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  int svalue = get_word_reg (rd);
  int evalue = svalue + rr;
  put_word_reg (rd, evalue);

  int sreg = flag_update_table_add8[FUT_ADDSUB16_INDEX (svalue, evalue)];
  sreg &= ~FLAG_H;
  unsigned flag = (evalue & 0xFFFF) != 0;
  sreg &= ~(flag << FLAG_Z_BIT);

  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

/* 1001 0111 KKdd KKKK | SBIW */
static OP_FUNC_TYPE func_SBIW (int rd, int rr)
{
  log_append ("(###)->%02x ", rr);
  int svalue = get_word_reg (rd);
  int evalue = svalue - rr;
  put_word_reg (rd, evalue);

  int sreg = flag_update_table_sub8[FUT_ADDSUB16_INDEX (svalue, evalue)];
  sreg &= ~FLAG_H;
  unsigned flag = (evalue & 0xFFFF) != 0;
  sreg &= ~(flag << FLAG_Z_BIT);

  update_flags (FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}


/* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
/* 1001 1000 AAAA Abbb | CBI */
static OP_FUNC_TYPE func_CBI (int rd, int rr)
{
  data_write_magic_byte (rd, data_read_magic_byte (rd) & ~(rr));
}

/* 1001 1010 AAAA Abbb | SBI */
static OP_FUNC_TYPE func_SBI (int rd, int rr)
{
  data_write_magic_byte (rd, data_read_magic_byte (rd) | rr);
}

/* 1001 1001 AAAA Abbb | SBIC skipping 1 word */
static OP_FUNC_TYPE func_SBIC (int rd, int rr)
{
  skip_instruction_on_condition (!(data_read_magic_byte (rd) & rr), 1);
}

/* 1001 1001 AAAA Abbb | SBIC skipping 2 words */
static OP_FUNC_TYPE func_SBIC2 (int rd, int rr)
{
  skip_instruction_on_condition (!(data_read_magic_byte (rd) & rr), 2);
}

/* 1001 1011 AAAA Abbb | SBIS skipping 1 word */
static OP_FUNC_TYPE func_SBIS (int rd, int rr)
{
  skip_instruction_on_condition (data_read_magic_byte (rd) & rr, 1);
}

/* 1001 1011 AAAA Abbb | SBIS skipping 2 words */
static OP_FUNC_TYPE func_SBIS2 (int rd, int rr)
{
  skip_instruction_on_condition (data_read_magic_byte (rd) & rr, 2);
}


/* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
/* 1011 0AAd dddd AAAA | IN */
static OP_FUNC_TYPE func_IN (int rd, int rr)
{
  put_reg (rd, data_read_magic_byte (rr));
}

/* 1011 1AAd dddd AAAA | OUT */
static OP_FUNC_TYPE func_OUT (int rd, int rr)
{
  data_write_magic_byte (rr, get_reg (rd));
}


/* opcodes with a relative 12-bit address (k) operand */
/* 1100 kkkk kkkk kkkk | RJMP */
static OP_FUNC_TYPE func_RJMP (int rd, int rr)
{
  int delta = (int16_t) rr;
  // special case: endless loop usually means that the program has ended
  if (delta == -1)
    leave (EXIT_STATUS_EXIT, "infinite loop detected (normal exit)");
  cpu_PC = (cpu_PC + delta) & PC_VALID_MASK;
}

/* 1101 kkkk kkkk kkkk | RCALL */
static OP_FUNC_TYPE func_RCALL (int rd, int rr)
{
  int delta = (int16_t) rr;
  push_PC();
  cpu_PC = (cpu_PC + delta) & PC_VALID_MASK;
  add_program_cycles (arch.pc_3bytes);
}


/* opcodes with two 4-bit register (Rd and Rr) operands */
/* 0000 0001 dddd rrrr | MOVW */
static INLINE OP_FUNC_TYPE func_MOVW (int rd, int rr)
{
#ifdef AVRTEST_LOG
  put_word_reg (rd, get_word_reg (rr));
#else
  put_reg (rd,     get_reg (rr));
  put_reg (rd + 1, get_reg (rr + 1));
#endif
}

/* 0000 0010 dddd rrrr | MULS */
static OP_FUNC_TYPE func_MULS (int rd, int rr)
{
  do_multiply (rd, rr, 1, 1, 0);
}

/* opcodes with two 3-bit register (Rd and Rr) operands */
/* 0000 0011 0ddd 0rrr | MULSU */
static OP_FUNC_TYPE func_MULSU (int rd, int rr)
{
  do_multiply (rd, rr, 1, 0, 0);
}

/* 0000 0011 0ddd 1rrr | FMUL */
static OP_FUNC_TYPE func_FMUL (int rd, int rr)
{
  do_multiply (rd, rr, 0, 0, 1);
}

/* 0000 0011 1ddd 0rrr | FMULS */
static OP_FUNC_TYPE func_FMULS (int rd, int rr)
{
  do_multiply (rd, rr, 1, 1, 1);
}

/* 0000 0011 1ddd 1rrr | FMULSU */
static OP_FUNC_TYPE func_FMULSU (int rd, int rr)
{
  do_multiply (rd, rr, 1, 0, 1);
}


static OP_FUNC_TYPE func__null (int rd, int rr)
{
  leave (EXIT_STATUS_FATAL, "function must be unreachable");
}

#define func__last_fast  func__null

// ----------------------------------------------------------------------------
// AVR opcodes
// depends on CX and thus on ISA_XMEGA, hence this table lives in avrtest.c

const opcode_t opcodes[] =
  {
#define AVR_OPCODE(ID, N_WORDS, N_TICKS, NAME)            \
    [ID_ ## ID] = { func_ ## ID, N_WORDS, N_TICKS },
#include "avr-opcode.def"
#undef AVR_OPCODE
  };

// ----------------------------------------------------------------------------
//     main execution loop

static INLINE void
do_fast (byte id, int op1, int op2)
{
  // Some instructions are used so frequently and are so easy to simulate
  // (not more than 4 instructions) that they are not worth a function call.
  switch (id)
    {
    case ID_LDI:  func_LDI  (op1, op2); break;
    case ID_MOV:  func_MOV  (op1, op2); break;
    case ID_MOVW: func_MOVW (op1, op2); break;
    }
}
    
static INLINE void
do_step (void)
{
  // fetch decoded instruction
  decoded_t d = decoded_flash[cpu_PC];
  byte id = d.id;
  if (!id)
    bad_PC (cpu_PC);

  // execute instruction
  const opcode_t *insn = &opcodes[id];
  log_add_instr (&d);
  cpu_PC += insn->size;
  add_program_cycles (insn->cycles);
  int op1 = d.op1;
  int op2 = d.op2;
  if (id >= ID__last_fast)
    insn->func (op1, op2);
  else
    do_fast (id, op1, op2);
  log_dump_line (id);
}

static INLINE void
execute (void)
{
  dword max_insns = program.max_insns;
  program.n_cycles = 0;

  for (;;)
    {
      do_step();

      program.n_insns++;
      if (max_insns && program.n_insns >= max_insns)
        leave (EXIT_STATUS_TIMEOUT, "instruction count limit reached");
    }
}

// main: as simple as it gets
int
main (int argc, char *argv[])
{
  gettimeofday (&t_start, NULL);
  log_init (t_start.tv_usec + t_start.tv_sec);

  parse_args (argc, argv);

  if (options.do_runtime)
    gettimeofday (&t_load, NULL);

  load_to_flash (options.program_name, cpu_flash, cpu_data, cpu_eeprom);

  if (options.do_runtime)
    gettimeofday (&t_decode, NULL);

  decode_flash (decoded_flash, cpu_flash);

  if (options.do_runtime)
    gettimeofday (&t_execute, NULL);

  execute();

  return EXIT_SUCCESS;
}
