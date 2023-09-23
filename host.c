/*
  This file is part of avrtest -- A simple simulator for the
  Atmel AVR family of microcontrollers designed to test the compiler.

  Copyright (C) 2019-2023 Free Software Foundation, Inc.

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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "host.h"

#define IN_AVRTEST
#include "avrtest.h"

#define LEN_LOG_STRING      500
#define LEN_LOG_XFMT        500

__attribute__((__format__(printf,1,2)))
static void log_add_ (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  log_va (fmt, args);
  va_end (args);
}


#define log_add(...)            \
  do {                          \
      if (is_avrtest_log)       \
        log_add_ (__VA_ARGS__); \
    } while (0)


const layout_t
layout[LOG_X_sentinel] =
  {
    [LOG_STR_CMD]   = { 2, "%s",       false, false },
    [LOG_PSTR_CMD]  = { 2, "%s",       false, true  },
    [LOG_ADDR_CMD]  = { 2, " 0x%04x ", false, false },

    [LOG_FLOAT_CMD] = { 4, " %.6f ", false, false },
    [LOG_D64_CMD]   = { 8, " %.6f ", false, false },

    [LOG_U8_CMD]  = { 1, " %u ", false, false },
    [LOG_U16_CMD] = { 2, " %u ", false, false },
    [LOG_U24_CMD] = { 3, " %u ", false, false },
    [LOG_U32_CMD] = { 4, " %u ", false, false },
    [LOG_U64_CMD] = { 8, " %llu ", false, false },

    [LOG_S8_CMD]  = { 1, " %d ", true,  false },
    [LOG_S16_CMD] = { 2, " %d ", true,  false },
    [LOG_S24_CMD] = { 3, " %d ", true,  false },
    [LOG_S32_CMD] = { 4, " %d ", true,  false },
    [LOG_S64_CMD] = { 8, " %lld ", true,  false },

    [LOG_X8_CMD]  = { 1, " 0x%02x ", false, false },
    [LOG_X16_CMD] = { 2, " 0x%04x ", false, false },
    [LOG_X24_CMD] = { 3, " 0x%06x ", false, false },
    [LOG_X32_CMD] = { 4, " 0x%08x ", false, false },
    [LOG_X64_CMD] = { 8, " 0x%016llx ", false, false },

    [LOG_UNSET_FMT_CMD]     = { 0, NULL, false, false },
    [LOG_SET_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_SET_PFMT_CMD]      = { 2, NULL, false, true  },
    [LOG_SET_FMT_ONCE_CMD]  = { 2, NULL, false, false },
    [LOG_SET_PFMT_ONCE_CMD] = { 2, NULL, false, true  },

    [LOG_TAG_FMT_CMD]       = { 2, NULL, false, false },
    [LOG_TAG_PFMT_CMD]      = { 2, NULL, false, true  },
  };

/* Copy a string from AVR target to the host, but not more than
   LEN_MAX characters.  */

char*
read_string (char *p, unsigned addr, bool flash_p, size_t len_max)
{
  char c;
  size_t n = 0;
  if (flash_p
      && arch.flash_pm_offset
      && addr >= arch.flash_pm_offset)
    {
      // README states that LOG_PSTR etc. can be used to print strings in
      // flash.  This is ambiguous for devices that see flash in RAM:
      // A variable can reside in flash due to __flash or PROGMEM and
      // hence in .progmem.data, or it can be located in .rodata.
      // If the address appears as a .rodata address, then access RAM
      // space (which holds a copy of flash at arch.flash_pm_offset).
      flash_p = false;
    }

  byte *p_avr = cpu_address (addr, flash_p ? AR_FLASH : AR_RAM);

  while (++n < len_max && (c = *p_avr++))
    if (c != '\r')
      *p++ = c;

  *p = '\0';
  return p;
}


// IEEE-754 single
avr_float_t
decode_avr_float (unsigned val)
{
  // float =  s  bbbbbbbb mmmmmmmmmmmmmmmmmmmmmmm
  //         31
  // s = sign (1)
  // b = biased exponent
  // m = mantissa

  int one;
  const int DIG_MANT = 23;
  const int DIG_EXP  = 8;
  const int EXP_BIAS = 127;
  avr_float_t af;

  int r = (1 << DIG_EXP) -1;
  unsigned mant = af.mant = val & ((1 << DIG_MANT) -1);
  val >>= DIG_MANT;
  af.exp_biased = val & r;
  af.exp = af.exp_biased - EXP_BIAS;

  val >>= DIG_EXP;
  af.sign_bit = val & 1;

  // Denorm?
  if (af.exp_biased == 0)
    af.fclass = FT_DENORM;
  else if (af.exp_biased < r)
    af.fclass = FT_NORM;
  else if (mant == 0)
    af.fclass = FT_INF;
  else
    af.fclass = FT_NAN;

  switch (af.fclass)
    {
    case FT_NORM:
    case FT_DENORM:
      one = af.fclass == FT_NORM;
      af.mant1 = mant | (one << DIG_MANT);
      af.x = ldexp ((double) af.mant1, af.exp + 1 - one - DIG_MANT);
      af.x = copysign (af.x, af.sign_bit ? -1.0 : 1.0);
      break;
    case FT_NAN:
      af.x = nan ("");
      break;
    case FT_INF:
      af.x = af.sign_bit ? -HUGE_VAL : HUGE_VAL;
      break;
    }

  return af;
}


// IEEE-754 double
avr_float_t
decode_avr_double (uint64_t val)
{
  // double = s bbbbbbbbbbb mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
  //         63
  // s = sign (1)
  // b = biased exponent
  // m = mantissa

  int one;
  const int DIG_MANT = 52;
  const int DIG_EXP  = 11;
  const int EXP_BIAS = 1023;
  avr_float_t af;

  int r = (1 << DIG_EXP) -1;
  uint64_t mant = af.mant = val & (((uint64_t)1 << DIG_MANT) -1);
  val >>= DIG_MANT;
  af.exp_biased = val & r;
  af.exp = af.exp_biased - EXP_BIAS;

  val >>= DIG_EXP;
  af.sign_bit = val & 1;

  // Denorm?
  if (af.exp_biased == 0)
    af.fclass = FT_DENORM;
  else if (af.exp_biased < r)
    af.fclass = FT_NORM;
  else if (mant == 0)
    af.fclass = FT_INF;
  else
    af.fclass = FT_NAN;

  switch (af.fclass)
    {
    case FT_NORM:
    case FT_DENORM:
      one = af.fclass == FT_NORM;
      af.mant1 = mant | ((uint64_t)one << DIG_MANT);
      af.x = ldexp ((double) af.mant1, af.exp + 1 - one - DIG_MANT);
      af.x = copysign (af.x, af.sign_bit ? -1.0 : 1.0);
      break;
    case FT_NAN:
      af.x = nan ("");
      break;
    case FT_INF:
      af.x = af.sign_bit ? -HUGE_VAL : HUGE_VAL;
      break;
    }

  return af;
}


/* Read a value as unsigned from R20.  Bytesize (1..4) and signedness are
   determined by respective layout[].  If the value is signed a cast to
   signed will do the conversion.  */

unsigned
get_r20_value (const layout_t *lay)
{
  byte *p = cpu_address (20, AR_REG);
  unsigned val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1U;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


/* Read a value as unsigned long long from R18.  Bytesize (1..8) and
   signedness are determined by respective layout[].  If the value is signed
   a cast to signed will do the conversion.  */

static unsigned long long
get_r18_value (const layout_t *lay)
{
  byte *p = cpu_address (18, AR_REG);
  unsigned long long val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1ull;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


/* Read a value as unsigned from ADDR.  Bytesize (1..4) and signedness
   are determined by respective layout[].  If the value is signed, then
   a cast to signed will do the conversion.  */

static unsigned
get_mem_value (unsigned addr, const layout_t *lay)
{
  bool flash_p = lay->in_rom;

  if (flash_p
      && arch.flash_pm_offset
      && addr >= arch.flash_pm_offset)
    {
      // "Flash" is ambiguous for devices that see flash in RAM:
      // A variable can reside in flash due to __flash or PRGMEM and
      // hence in .progmem.data, or it can be located in .rodata.
      // If the address appears as a .rodata address, then access RAM
      // (which holds a copy of flash at arch.flash_pm_offset).
      flash_p = false;
    }

  byte *p = cpu_address (addr, flash_p ? AR_FLASH : AR_RAM);
  unsigned val = 0;

  if (lay->signed_p && (0x80 & p[lay->size - 1]))
    val = -1U;

  for (int n = lay->size; n;)
    val = (val << 8) | p[--n];

  return val;
}


typedef struct
{
  // Offset set by RESET.
  uint64_t n_insns;
  uint64_t n_cycles;
  // Current value for PRAND mode
  uint32_t pvalue;
} ticks_port_t;


void
sys_ticks_cmd (int cfg)
{
  static ticks_port_t ticks_port;

  // a prime m
  const uint32_t prand_m = 0xfffffffb;
  // phi (m)
  // uint32_t prand_phi_m = m-1; // = 2 * 5 * 19 * 22605091
  // a primitive root of (Z/m*Z)^*
  const uint32_t prand_root = 0xcafebabe;

  cfg &= 0xff;
  ticks_port_t *tp = &ticks_port;

  if (cfg & TICKS_RESET_ALL_CMD)
    {
      log_add ("ticks reset:");
      if (cfg & TICKS_RESET_CYCLES_CMD)
        {
          log_add (" cycles");
          tp->n_cycles = program.n_cycles;
        }
      if (cfg & TICKS_RESET_INSNS_CMD)
        {
          log_add (" insns");
          tp->n_insns = program.n_insns;
        }
      if (cfg & TICKS_RESET_PRAND_CMD)
        {
          log_add (" prand");
          tp->pvalue = 0;
        }
      return;
    }

  const char *what = "???";
  uint32_t value = 0;

  switch (cfg)
    {
    case TICKS_GET_CYCLES_CMD:
      what = "cycles";
      value = (uint32_t) (program.n_cycles - tp->n_cycles);
      break;
    case TICKS_GET_INSNS_CMD:
      what = "insn";
      value = (uint32_t) (program.n_insns - tp->n_insns);
      break;
    case TICKS_GET_PRAND_CMD:
      what = "prand";
      value = tp->pvalue ? tp->pvalue : 1;
      value = ((uint64_t) value * prand_root) % prand_m;
      tp->pvalue = value;
      break;
    case TICKS_GET_RAND_CMD:
      what = "rand";
      value = rand();
      value ^= (unsigned) rand() << 11;
      value ^= (unsigned) rand() << 22;
      break;
    }

  log_add ("ticks get %s: R22<-(%08x) = %u", what, value, value);
  byte *p = cpu_address (22, AR_REG);

  *p++ = value;
  *p++ = value >> 8;
  *p++ = value >> 16;
  *p++ = value >> 24;
}


void
sys_log_dump (int what)
{
  what &= 0xff;
  if (what >= LOG_X_sentinel)
    {
      log_add ("log: invalid cmd %d\n", what);
      return;
    }

  static int fmt_once = 0;
  static char xfmt[LEN_LOG_XFMT];
  static char string[LEN_LOG_STRING];
  const layout_t *lay = & layout[what];
  unsigned val = get_r20_value (lay);
  const char *fmt = fmt_once ? xfmt : lay->fmt;

  if (fmt_once == 1)
    fmt_once = 0;

  switch (what)
    {
    default:
      log_add ("log %d-byte value", lay->size);
      printf (fmt, val);
      break;

    case LOG_S64_CMD:
    case LOG_U64_CMD:
    case LOG_X64_CMD:
      log_add ("log %d-byte value", lay->size);
      printf (fmt, get_r18_value (lay));
      break;

    case LOG_SET_FMT_ONCE_CMD:
    case LOG_SET_PFMT_ONCE_CMD:
      log_add ("log set format");
      fmt_once = 1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_SET_FMT_CMD:
    case LOG_SET_PFMT_CMD:
      log_add ("log set format");
      fmt_once = -1;
      read_string (xfmt, val, lay->in_rom, sizeof (xfmt));
      break;

    case LOG_UNSET_FMT_CMD:
      log_add ("log unset format");
      fmt_once = 0;
      break;

    case LOG_PSTR_CMD:
    case LOG_STR_CMD:
      log_add ("log string");
      read_string (string, val, lay->in_rom, sizeof (string));
      printf (fmt, string);
      break;

    case LOG_FLOAT_CMD:
      {
        log_add ("log float");
        avr_float_t af = decode_avr_float (val);
        printf (fmt, af.x);
      }
      break;

    case LOG_D64_CMD:
      {
        log_add ("log double");
        avr_float_t af = decode_avr_double (get_r18_value (lay));
        printf (fmt, af.x);
      }
      break;
    }
}


// Magic values from avr-libc::stdio.h

#define AVRLIBC_SEEK_SET 0
#define AVRLIBC_SEEK_CUR 1
#define AVRLIBC_SEEK_END 2

#define AVRLIBC_EOF (-1)

// Handles for special streams, e.g. to fflush() them.
#define N_STD_FILES 3
#define HANDLE_stdin  (-1)
#define HANDLE_stdout (-2)
#define HANDLE_stderr (-3)

#define AT "@"

#define FIND_UNUSED_FILE 0x1234


typedef int8_t handle_t;

typedef struct
{
  handle_t handle;
  bool binary_p;
  bool std_file_p;
  FILE *file;
  char name[10];
} file_t;


static INLINE file_t*
find_file (int handle)
{
  static file_t files[8 + N_STD_FILES];
  static const int n_files = sizeof (files) / sizeof (*files);

  static bool files_initialized_p;
  if (!files_initialized_p)
    {
      files_initialized_p = true;
      for (int i = N_STD_FILES; i < n_files; i++)
        {
          files[i].handle = i + 1 - N_STD_FILES;
          sprintf (files[i].name, AT "%d", files[i].handle);
        }

#define STD_INIT(f) \
      files[-1 - HANDLE_##f] = (file_t) { HANDLE_##f, true, true, f, AT #f }
      STD_INIT (stdin);
      STD_INIT (stdout);
      STD_INIT (stderr);
#undef STD_INIT
    }

  if (handle > 0 && handle <= n_files - N_STD_FILES)
    {
      // Return an already initialized / opened handle.

      file_t *file = & files[handle - 1 + N_STD_FILES];
      if (!file->file)
        leave (LEAVE_HOSTIO, "file handle %s not open", file->name);
      if (file->handle != handle)
        leave (LEAVE_FATAL, "file %s handle %d should be %d",
               file->name, file->handle, handle);
      return file;
    }
  else if (-handle <= N_STD_FILES && handle < 0)
    {
      return & files[-1 - handle];
    }
  else if (handle == FIND_UNUSED_FILE)
    {
      // Search for an unused files[] slot.

      file_t *file = files + N_STD_FILES;
      for (int i = N_STD_FILES; i < n_files; i++, file++)
        {
          if (!file->file)
            {
              if (file->handle != i + 1 - N_STD_FILES)
                leave (LEAVE_FATAL, "file %s handle %d should be %d",
                       file->name, file->handle, i + 1 - N_STD_FILES);
              return file;
            }
        }
    }
  else
    {
      // We are called with an invalid handle.
      leave (LEAVE_HOSTIO, "file handle %d out of range", handle);
    }

  leave (LEAVE_HOSTIO, "ran out of %d file handles", n_files - N_STD_FILES);
}


static const layout_t lay_1 = { 1, NULL, false, false }; // 1 byte  in RAM.
static const layout_t lay_2 = { 2, NULL, false, false }; // 2 bytes in RAM.
static const layout_t lay_4 = { 4, NULL, false, false }; // 4 bytes in RAM.


// FILE* fopen (const char *s_path, const char *s_mode)
static dword
host_fopen (dword args)
{
  if (!options.do_sandbox)
    leave (LEAVE_USAGE, "file i/o requires option '-sbox SANDBOX'");

  word p_file = (word) args;
  word p_mode = (word) (args >> 16);

  char s_file[40];
  char s_mode[5];
  read_string (s_file, p_file, AR_RAM, sizeof (s_file));
  read_string (s_mode, p_mode, AR_RAM, sizeof (s_mode));
  log_add (" (%04x)->\"%s\" (%04x)->\"%s\"", p_file, s_file, p_mode, s_mode);
  if (strstr (s_file, ".."))
    leave (LEAVE_HOSTIO, "bad file name in syscall open: \"%s\"", s_file);

  file_t *file = find_file (FIND_UNUSED_FILE);
  char s_path[1 + strlen (fileio_sandbox) + strlen (s_file)];
  strcpy (s_path, fileio_sandbox);
  strcat (s_path, s_file);
  file->file = fopen (s_path, s_mode);
  if (!file->file)
    {
      log_add ("\n*** cannot fopen \"%s\"", s_path);
      if (options.do_verbose)
        {
          char msg[16 + strlen (s_path)];
          sprintf (msg, "file i/o: %s", s_path);
          perror (msg);
        }
      return 0;
    }
  log_add ("\n*** %s <- fopen \"%s\" for \"%s\"",
           file->name, s_path, s_mode);
  file->binary_p = strchr (s_mode, 'b');
  return 0xff & (dword) file->handle;
}


// int fclose (FILE*);
static dword host_fclose (dword args)
{
  handle_t handle = (handle_t) args;
  file_t *file = find_file (handle);
  log_add (" %s", file->name);

  if (handle > 0)
    {
      fclose (file->file);
      file->file = NULL;
    }

  return 0;
}


// int fputc (char /* sic! */ c , FILE*);
// "char c" because that is what avr-libc's FDEV_SETUP_STREAM
// specifies for the put function.
static dword host_fputc (dword args)
{
  handle_t handle = (handle_t) (args >> 8);
  file_t *file = find_file (handle);

  byte c = (byte) args;
  if (!file->binary_p && c == '\r')
    return 0;

  log_add (" %s <- %02x", file->name, c);
  int r = fputc ((char) c, file->file);
  return r == EOF ? (0xffff & AVRLIBC_EOF) : (0xff & r);
}


// int fgetc (FILE*);
static dword host_fgetc (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  int c = fgetc (file->file);

  if (c == EOF)
    log_add (" -> EOF");
  else
    log_add (" -> %02x", (byte) c);

  return 0xffff & (c == EOF ? AVRLIBC_EOF : (0xff & c));
}


// int feof (FILE*);
static dword host_feof (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  int c = feof (file->file);
  log_add (" -> %d", c);
  return !!c;
}


// void clearerr (FILE*);
static dword host_clearerr (dword args)
{
  file_t *file = find_file ((handle_t) args);
  log_add (" %s", file->name);
  clearerr (file->file);
  return 0;
}


// int fflush (FILE*);
static dword host_fflush (dword args)
{
  int r = EOF;
  handle_t handle = (handle_t) args;

  if (0 == handle)
    {
      log_add (" %s", AT "all");
      r = fflush (NULL);
    }
  else
    {
      file_t *file = find_file (handle);
      log_add (" %s", file->name);
      r = fflush (file->file);
    }

  return r == EOF ? (0xffff & AVRLIBC_EOF) : 0;
}


typedef struct
{
  int wo;
  const char *text;
} avrlibc_stdio_seek_t;


// int fseek (FILE*, long pos, int whence);
static dword host_fseek (dword args)
{
  static const avrlibc_stdio_seek_t avrlibc_seek[] =
    {
      // Magic 0, 1, 2 are from avr-libc::stdio.h.
      [AVRLIBC_SEEK_SET] = { SEEK_SET, "SEEK_SET" },
      [AVRLIBC_SEEK_CUR] = { SEEK_CUR, "SEEK_CUR" },
      [AVRLIBC_SEEK_END] = { SEEK_END, "SEEK_END" },
    };

  word pargs = (word) args;
  handle_t hnd = (handle_t) get_mem_value (pargs, & lay_1);
  long pos     = get_mem_value (pargs + 1, & lay_4);
  byte wo      = get_mem_value (pargs + 5, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (pos)->%ld (whence)->%d=%s", file->name, pos, wo,
           wo <= 2 ? avrlibc_seek[wo].text : "?");

  if (wo > 2)
    leave (LEAVE_HOSTIO, "bad 3rd argument for fseek %s: %d", file->name, wo);

  if (hnd < 0)
    leave (LEAVE_HOSTIO, "cannot seek in %s", file->name);

  return 0xffff & fseek (file->file, (int32_t) pos, avrlibc_seek[wo].wo);
}


// size_t fread (void *ptr, size_t size, size_t nmemb, FILE *stream);
static dword host_fread (dword args)
{
  word pargs = (word) args;
  word ptr     = (word) get_mem_value (pargs + 0, & lay_2);
  size_t size  = (word) get_mem_value (pargs + 2, & lay_2);
  size_t nmemb = (word) get_mem_value (pargs + 4, & lay_2);
  handle_t hnd = (handle_t) get_mem_value (pargs + 6, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (ptr)->%04x (size)->%d (nmemb)->%d", file->name,
           ptr, (int) size, (int) nmemb);

  return fread (cpu_address (ptr, AR_RAM), size, nmemb, file->file);
}


// size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream);
static dword host_fwrite (dword args)
{
  word pargs = (word) args;
  word ptr     = (word) get_mem_value (pargs + 0, & lay_2);
  size_t size  = (word) get_mem_value (pargs + 2, & lay_2);
  size_t nmemb = (word) get_mem_value (pargs + 4, & lay_2);
  handle_t hnd = (handle_t) get_mem_value (pargs + 6, & lay_1);

  file_t *file = find_file (hnd);

  log_add (" %s (ptr)->%04x (size)->%d (nmemb)->%d", file->name,
           ptr, (int) size, (int) nmemb);

  return fwrite (cpu_address (ptr, AR_RAM), size, nmemb, file->file);
}


/* The worker function for file i/o (syscall 26).  R24 is passed as WHAT
   and specifies what file i/o operation to perform.  R20 are the input
   arguments of that syscall.  If all arguments together fit into 4 bytes,
   then they are passed in R20.  Otherwise, the lower 2 bytes of R20
   hold the memory address where to find the arguments for the specified
   function.  */

dword
host_fileio (byte what, dword r20)
{
  static const struct
  {
    // The handler that takes care of what R24 tells us to perform.
    dword (*hnd)(dword);

    // The name of the C function and the number of bytes used from args.
    // Only used for logging.
    const char *label;
    int n_bytes;
  } host_hnd[] =
      {
#define HANDLER(n, f)   [AVRTEST_##f] = { host_##f, #f, n }
        HANDLER (4, fopen),
        HANDLER (1, fclose),
        HANDLER (1, fgetc),
        HANDLER (2, fputc),
        HANDLER (1, feof),
        HANDLER (1, clearerr),
        HANDLER (2, fseek),     // -> 6 bytes
        HANDLER (1, fflush),
        HANDLER (2, fread),     // -> 7 bytes
        HANDLER (2, fwrite),    // -> 7 bytes
#undef HANDLER
      };

  if (what >= sizeof (host_hnd) / sizeof (*host_hnd))
    leave (LEAVE_HOSTIO,
           "not implemented: syscall 26 file i/o handler (R24)->%d", what);

  if (is_avrtest_log)
    {
      int n_bytes = host_hnd[what].n_bytes;
      unsigned mask = (2u << (8 * n_bytes - 1)) - 1;
      log_add ("file i/o #%d=%s (args)->%0*x", what, host_hnd[what].label,
               2 * n_bytes, mask & r20);
    }

  return host_hnd[what].hnd (r20);
}
