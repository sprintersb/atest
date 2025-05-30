/*
  This file is part of AVRtest -- A simple simulator for the
  AVR family of 8-bit microcontrollers designed to test the compiler.

  Copyright (C) 2019-2025 Free Software Foundation, Inc.

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

#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

enum
  {
    FT_NORM,
    FT_DENORM,
    FT_INF,
    FT_NAN
  };

// Decomposed IEEE 754 floating point number.
typedef struct
{
  int sign_bit;
  // Mantissa without and with the leading (implicit) 1.
  uint64_t mant, mant1;
  int exp;
  int exp_biased;
  int fclass;
  double x;
} avr_float_t;

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

extern unsigned get_reg_value (int regno, const layout_t*);
extern uint8_t get_reg_u8 (int regno);
extern uint16_t get_reg_u16 (int regno);
extern uint32_t get_reg_u32 (int regno);
extern uint64_t get_reg_u64 (int regno);
extern int8_t get_reg_s8 (int regno);
extern int16_t get_reg_s16 (int regno);
extern int32_t get_reg_s32 (int regno);
extern int64_t get_reg_s64 (int regno);
extern uint8_t get_mem_u8 (int);
extern uint16_t get_mem_u16 (int);
extern uint32_t get_mem_u32 (int);
extern uint64_t get_mem_u64 (int);
extern int8_t get_mem_s8 (int);
extern int16_t get_mem_s16 (int);
extern int32_t get_mem_s32 (int);
extern int64_t get_mem_s64 (int);
extern char* read_string (char *dest, unsigned addr, bool flash_p, size_t limi);
extern void set_reg_value (int regno, int n_regs, uint64_t val);
extern void set_mem_value (int addr, int n_regs, uint64_t val);
extern avr_float_t decode_avr_float (unsigned);
extern avr_float_t decode_avr_double (uint64_t);
extern float get_reg_float (int regno);
extern void set_reg_float (int regno, float f);

typedef struct
{
  // Offset set by RESET.
  uint64_t n_insns;
  uint64_t n_cycles;
  // Current value for PRAND mode
  uint32_t pvalue;

  struct
  {
    // 0: Nothing to do.
    // 1: Pending next [R]CALL after avrtest_cycles_call().
    // 2: Pending respective RET.
    // 3: After RET, pending avrtest_cycles().
    int state;
    // Expected location where the associated RET returns to.
    unsigned pc_ret;
    // Expected SP after the associated RET.
    int sp_ret;
    uint64_t n_cycles_before_call;
    uint64_t n_cycles_after_ret;
    uint32_t n_cycles;
  } call;
} ticks_port_t;

extern ticks_port_t ticks_port;

extern void sys_ticks_cmd (int);
extern void sys_log_dump (int);
extern void sys_misc_emul (uint8_t);
extern void sys_emul_double (uint8_t);
extern void sys_emul_float (uint8_t);

extern dword host_fileio (byte, dword);
#endif // HOST_H
