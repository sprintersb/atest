/*
 ****************************************************************************
 *
 * avrtest - A simple simulator for the Atmel AVR family of microcontrollers
 *           designed to test the compiler
 * Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph
 * Copyright (C) 2007 Paulo Marques
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------------
//     configuration values

#define MAX_RAM_SIZE	 64 * 1024
#define MAX_FLASH_SIZE  256 * 1024

// ---------------------------------------------------------------------------------
//     register and port definitions

#define SREG	0x5F
#define SPH	0x5E
#define SPL	0x5D
#define EIND	0x5C
#define RAMPZ	0x5B

#define IOBASE	0x20

// ports used for application <-> simulator interactions
#define STDIO_PORT	0x52
#define EXIT_PORT	0x4F
#define ABORT_PORT	0x49

#define REGX	26
#define REGY	28
#define REGZ	30

#define FLAG_I	0x80
#define FLAG_T	0x40
#define FLAG_H	0x20
#define FLAG_S	0x10
#define FLAG_V	0x08
#define FLAG_N	0x04
#define FLAG_Z	0x02
#define FLAG_C	0x01

enum decoder_operand_masks {
	/** 2 bit register id  ( R24, R26, R28, R30 ) */
	mask_Rd_2     = 0x0030,
	/** 3 bit register id  ( R16 - R23 ) */
	mask_Rd_3     = 0x0070,
	/** 4 bit register id  ( R16 - R31 ) */
	mask_Rd_4     = 0x00f0,
	/** 5 bit register id  ( R00 - R31 ) */
	mask_Rd_5     = 0x01f0,

	/** 3 bit register id  ( R16 - R23 ) */
	mask_Rr_3     = 0x0007,
	/** 4 bit register id  ( R16 - R31 ) */
	mask_Rr_4     = 0x000f,
	/** 5 bit register id  ( R00 - R31 ) */
	mask_Rr_5     = 0x020f,

	/** for 8 bit constant */
	mask_K_8      = 0x0F0F,
	/** for 6 bit constant */
	mask_K_6      = 0x00CF,

	/** for 7 bit relative address */
	mask_k_7      = 0x03F8,
	/** for 12 bit relative address */
	mask_k_12     = 0x0FFF,
	/** for 22 bit absolute address */
	mask_k_22     = 0x01F1,

	/** register bit select */
	mask_reg_bit  = 0x0007,
	/** status register bit select */
	mask_sreg_bit = 0x0070,
	/** address displacement (q) */
	mask_q_displ  = 0x2C07,

	/** 5 bit register id  ( R00 - R31 ) */
	mask_A_5      = 0x00F8,
	/** 6 bit IO port id */
	mask_A_6      = 0x060F
};
// ---------------------------------------------------------------------------------
//     some helpful types and constants

#define EXIT_STATUS_EXIT	0
#define EXIT_STATUS_ABORTED	1
#define EXIT_STATUS_TIMEOUT	2

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned long long ddword;

typedef struct {
	byte data_index;
	byte oper1;
	word oper2;
} decoded_op;

#define OP_FUNC_TYPE	void __attribute__((fastcall))

typedef OP_FUNC_TYPE (*opcode_func)(int,int);

typedef struct {
	opcode_func func;
	int size;
	int cycles;
	const char *hreadable;
} opcode_data;

// ---------------------------------------------------------------------------------
//     auxiliary lookup tables

enum {
	avr_op_index_dummy = 0,

	avr_op_index_ADC = 1,	avr_op_index_ADD,	avr_op_index_ADIW,	avr_op_index_SBIW,
	avr_op_index_AND,	avr_op_index_ANDI,	avr_op_index_ASR,
	avr_op_index_BCLR,	avr_op_index_BLD,	avr_op_index_BRBC,
	avr_op_index_BRBS,	avr_op_index_BSET,	avr_op_index_BST,
	avr_op_index_CALL,	avr_op_index_CBI,	avr_op_index_COM,
	avr_op_index_CP,	avr_op_index_CPC,	avr_op_index_CPI,
	avr_op_index_CPSE,	avr_op_index_DEC,	avr_op_index_EICALL,
	avr_op_index_EIJMP,	avr_op_index_ELPM,	avr_op_index_ELPM_Z,
	avr_op_index_ELPM_Z_incr,avr_op_index_EOR,	avr_op_index_ESPM,
	avr_op_index_FMUL,	avr_op_index_FMULS,	avr_op_index_FMULSU,
	avr_op_index_ICALL,	avr_op_index_IJMP,	avr_op_index_ILLEGAL,
	avr_op_index_IN,	avr_op_index_INC,	avr_op_index_JMP,
	avr_op_index_LDD_Y,	avr_op_index_LDD_Z,	avr_op_index_LDI,
	avr_op_index_LDS,	avr_op_index_LD_X,	avr_op_index_LD_X_decr,
	avr_op_index_LD_X_incr,	avr_op_index_LD_Y_decr,	avr_op_index_LD_Y_incr,
	avr_op_index_LD_Z_decr,	avr_op_index_LD_Z_incr,	avr_op_index_LPM,
	avr_op_index_LPM_Z,	avr_op_index_LPM_Z_incr,avr_op_index_LSR,
	avr_op_index_MOV,	avr_op_index_MOVW,	avr_op_index_MUL,
	avr_op_index_MULS,	avr_op_index_MULSU,	avr_op_index_NEG,
	avr_op_index_NOP,	avr_op_index_OR,	avr_op_index_ORI,
	avr_op_index_OUT,	avr_op_index_POP,	avr_op_index_PUSH,
	avr_op_index_RCALL,	avr_op_index_RET,	avr_op_index_RETI,
	avr_op_index_RJMP,	avr_op_index_ROR,	avr_op_index_SBC,
	avr_op_index_SBCI,	avr_op_index_SBI,	avr_op_index_SBIC,
	avr_op_index_SBIS,	avr_op_index_SBRC,	avr_op_index_SBRS,
	avr_op_index_SLEEP,	avr_op_index_SPM,	avr_op_index_STD_Y,
	avr_op_index_STD_Z,	avr_op_index_STS,	avr_op_index_ST_X,
	avr_op_index_ST_X_decr,	avr_op_index_ST_X_incr,	avr_op_index_ST_Y_decr,
	avr_op_index_ST_Y_incr,	avr_op_index_ST_Z_decr,	avr_op_index_ST_Z_incr,
	avr_op_index_SUB,	avr_op_index_SUBI,	avr_op_index_SWAP,
	avr_op_index_WDR,
};

extern opcode_data opcode_func_array[];

// ---------------------------------------------------------------------------------
// vars that hold core definitions

// configured flash size
static int flash_size;
static int PC_is_22_bits;

// maximum number of executed instructions (used as a timeout)
static dword max_instr_count;

// filename of the file being executed
static char program_name[256];
static int program_size;

// ---------------------------------------------------------------------------------
// vars that hold simulator state. These are kept as global vars for simplicity

// cycle counter
static dword program_cycles;

// cpu_data is used to store registers, ioport values and actual SRAM
static byte cpu_data[MAX_RAM_SIZE];
static int cpu_PC;

// flash
static byte cpu_flash[MAX_FLASH_SIZE];
static decoded_op decoded_flash[MAX_FLASH_SIZE/2];

// ---------------------------------------------------------------------------------
//     log functions

#ifdef LOG_DUMP

static char log_data[256];
char *cur_log_pos = log_data;

static void log_add_data(char *data)
{
	int len = strlen(data);
	memcpy(cur_log_pos, data, len);
	cur_log_pos += len;
}

static void log_add_instr(const char *instr)
{
	char buf[32];
	sprintf(buf, "%04x: %-5s ", cpu_PC * 2, instr);
	log_add_data(buf);
}

static void log_add_data_mov(const char *format, int addr, int value)
{
	char buf[32], adname[16];

	if (addr < 32) {
		sprintf(adname, "R%d", addr);
	} else {
		switch (addr) {
		case SREG: strcpy(adname, "SREG"); break;
		case SPH: strcpy(adname, "SPH"); break;
		case SPL: strcpy(adname, "SPL"); break;
		default:
			if (addr < 256)
				sprintf(adname, "%02x", addr);
			else
				sprintf(adname, "%04x", addr);
		}
	}

	sprintf(buf, format, adname, value);
	log_add_data(buf);
}

static void log_dump_line(void)
{
	*cur_log_pos = '\0';
	puts(log_data);
	cur_log_pos = log_data;
}

#else

// empty placeholders to keep the rest of the code clean while allowing the compiler to
// optimize these calls away

static void log_add_instr(const char *instr)
{}

static void log_add_data_mov(const char *format, int addr, int value)
{}

static void log_dump_line(void)
{}

#endif


static const char *exit_status_text[] = {
	[EXIT_STATUS_EXIT]    = "EXIT",
	[EXIT_STATUS_ABORTED] = "ABORTED",
	[EXIT_STATUS_TIMEOUT] = "TIMEOUT"
};

static void leave(int status, const char *reason)
{
	// make sure we print the last log line before leaving
	log_dump_line();

	printf("\n"
		" exit status: %s\n"
		"      reason: %s\n"
		"     program: %s\n"
		"exit address: %04x\n"
		"total cycles: %u\n\n",
		exit_status_text[status], reason, program_name,
		cpu_PC * 2, program_cycles);
	exit(0);
}

// ---------------------------------------------------------------------------------
//     ioport / ram / flash, read / write entry points

// lowest level memory accessors

static int data_read_byte_raw(int address)
{
	// add code here to handle special events
	if (address == STDIO_PORT)
		return getchar();

	return cpu_data[address];
}

static void data_write_byte_raw(int address, int value)
{
	// add code here to handle special events
	switch (address) {
		case STDIO_PORT:
			putchar(value);
			return;
		case EXIT_PORT:
			leave(value ? EXIT_STATUS_ABORTED : EXIT_STATUS_EXIT, "exit function called");
			break;
		case ABORT_PORT:
			leave(EXIT_STATUS_ABORTED, "abort function called");
			break;
	}

	// default action, just store the value
	cpu_data[address] = value;
}

static int flash_read_byte(int address)
{
	address %= flash_size;
	// add code here to handle special events
	return cpu_flash[address];
}


// mid-level memory accessors

// read a byte from memory / ioport / register

static int data_read_byte(int address)
{
	int ret = data_read_byte_raw(address);
	log_add_data_mov("(%s)->%02x ", address, ret);
	return ret;
}

// write a byte to memory / ioport / register

static void data_write_byte(int address, int value)
{
	log_add_data_mov("(%s)<-%02x ", address, value);
	data_write_byte_raw(address, value);
}

// get_reg / put_reg are just placeholders for read/write calls where we can
// be sure that the adress is < 32

static byte get_reg(int address)
{
	log_add_data_mov("(%s)->%02x ", address, cpu_data[address]);
	return cpu_data[address];
}

static void put_reg(int address, byte value)
{
	log_add_data_mov("(%s)<-%02x ", address, value);
	cpu_data[address] = value;
}

static int get_word_reg(int address)
{
	int ret = cpu_data[address] | (cpu_data[address + 1] << 8);
	log_add_data_mov("(%s)->%04x ", address, ret);
	return ret;
}

static void put_word_reg(int address, int value)
{
	log_add_data_mov("(%s)<-%04x ", address, value & 0xFFFF);
	cpu_data[address] = value;
	cpu_data[address + 1] = value >> 8;
}

// read a word from memory / ioport / register

static int data_read_word(int address)
{
	int ret = data_read_byte_raw(address) | (data_read_byte_raw(address + 1) << 8);
	log_add_data_mov("(%s)->%04x ", address, ret);
	return ret;
}

// write a word to memory / ioport / register

static void data_write_word(int address, int value)
{
	value &= 0xffff;
	log_add_data_mov("(%s)<-%04x ", address, value);
	data_write_byte_raw(address, value & 0xFF);
	data_write_byte_raw(address + 1, value >> 8);
}

// ---------------------------------------------------------------------------------
//     flag manipulation functions

static void update_flags(int flags, int new_values)
{
	int sreg = data_read_byte(SREG);
	sreg = (sreg & ~flags) | new_values;
	data_write_byte(SREG, sreg);
}

static int get_carry(void)
{
	return (data_read_byte(SREG) & FLAG_C) != 0;
}

// fast flag update tables to avoid conditional branches on frequently used operations

#define FUT_ADD8_RES	0x01FF
#define FUT_ADD8_V2_80	0x0200
#define FUT_ADD8_V1_80	0x0400
#define FUT_ADD8_V2_08	0x0800
#define FUT_ADD8_V1_08	0x1000

static byte flag_update_table_add8[8192];

#define FUT_ADD_SUB_INDEX(v1,v2,res)	\
	((((v1) & 0x08) << 9) | (((v2) & 0x08) << 8) | (((v1) & 0x80) << 3) | (((v2) & 0x80) << 2) | ((res) & 0x1FF))

#define FUT_ADDSUB16_INDEX(v1, res)	\
	( ((((v1) >> 8) & 0x80) << 3) | (((res) >> 8) & 0x1FF))

#define FUT_SUB8_RES	0x01FF
#define FUT_SUB8_V2_80	0x0200
#define FUT_SUB8_V1_80	0x0400
#define FUT_SUB8_V2_08	0x0800
#define FUT_SUB8_V1_08	0x1000

static byte flag_update_table_sub8[8192];

static byte flag_update_table_ror8[512];

static byte flag_update_table_inc[256];
static byte flag_update_table_dec[256];

static byte flag_update_table_logical[256];


static byte update_zns_flags(int result, byte sreg)
{
	if ((result & 0xFF) == 0x00)
		sreg |= FLAG_Z;
	if (result & 0x80)
		sreg |= FLAG_N;
	if (((sreg & FLAG_N) != 0) != ((sreg & FLAG_V) != 0))
		sreg |= FLAG_S;
	return sreg;
}

static void init_flag_update_tables(void)
{
	int i, result, sreg;

	// build flag update table for 8 bit addition
	for (i = 0; i < 8192; i++) {
		result = i & FUT_ADD8_RES;
		sreg = 0;
		if ((!(i & FUT_ADD8_V1_80) == !(i & FUT_ADD8_V2_80)) && (!(result & 0x80) != !(i & FUT_ADD8_V1_80)))
			sreg |= FLAG_V;
		if (result & 0x100)
			sreg |= FLAG_C;
		if (((i & 0x1800) == 0x1800) || ((result & 0x08) == 0 && ((i & 0x1800) != 0x0000)))
			sreg |= FLAG_H;
		sreg = update_zns_flags(result, sreg);
		flag_update_table_add8[i] = sreg;
	}

	// build flag update table for 8 bit subtraction
	for (i = 0; i < 8192; i++) {
		result = i & FUT_SUB8_RES;
		sreg = 0;
		if ((!(result & 0x80) == !(i & FUT_SUB8_V2_80)) && (!(i & FUT_SUB8_V1_80) != !(i & FUT_SUB8_V2_80)))
			sreg |= FLAG_V;
		if (result & 0x100)
			sreg |= FLAG_C;
		if (((i & 0x1800) == 0x1800) || ((result & 0x08) == 0 && ((i & 0x1800) != 0x0000)))
			sreg |= FLAG_H;
		sreg = update_zns_flags(result, sreg);
		flag_update_table_sub8[i] = sreg;
	}

	// build flag update table for 8 bit rotation
	for (i = 0; i < 512; i++) {
		result = i >> 1;
		sreg = 0;
		if (i & 0x01)
			sreg |= FLAG_C;
		if (((result & 0x80) != 0) != ((sreg & FLAG_C) != 0))
			sreg |= FLAG_V;
		sreg = update_zns_flags(result, sreg);
		flag_update_table_ror8[i] = sreg;
	}

	// build flag update table for increment
	for (i = 0; i < 256; i++) {
		sreg = (i == 0x80) ? FLAG_V : 0;
		sreg = update_zns_flags(i, sreg);
		flag_update_table_inc[i] = sreg;
	}

	// build flag update table for decrement
	for (i = 0; i < 256; i++) {
		sreg = (i == 0x7F) ? FLAG_V : 0;
		sreg = update_zns_flags(i, sreg);
		flag_update_table_dec[i] = sreg;
	}

	// build flag update table for logical operations
	for (i = 0; i < 256; i++) {
		sreg = update_zns_flags(i, 0);
		flag_update_table_logical[i] = sreg;
	}
}


// ---------------------------------------------------------------------------------
//     helper functions

static void add_program_cycles(dword cycles)
{
	program_cycles += cycles;
}


static void push_byte(int value)
{
	int sp = data_read_word(SPL);
	// temporary hack to disallow growing the stack over the reserved register area
	if (sp < 0x60)
		leave(EXIT_STATUS_ABORTED, "stack pointer overflow");
	data_write_byte(sp, value);
	data_write_word(SPL, sp - 1);
}

static int pop_byte(void)
{
	int sp = data_read_word(SPL);
	sp++;
	data_write_word(SPL, sp);
	return data_read_byte(sp);
}

static void push_PC(void)
{
	int sp = data_read_word(SPL);
	// temporary hack to disallow growing the stack over the reserved register area
	if (sp < 0x60)
		leave(EXIT_STATUS_ABORTED, "stack pointer overflow");
	data_write_byte(sp, cpu_PC & 0xFF);
	sp--;
	data_write_byte(sp, (cpu_PC >> 8) & 0xFF);
	sp--;
	if (PC_is_22_bits) {
	        data_write_byte(sp, (cpu_PC >> 16) & 0xFF);
		sp--;
        }
        data_write_word(SPL, sp);
}

static void pop_PC(void)
{
	int sp = data_read_word(SPL);
	if (PC_is_22_bits) {
		sp++;
		cpu_PC = data_read_byte(sp) << 16;
	} else
		cpu_PC = 0;
	sp++;
	cpu_PC |= data_read_byte(sp) << 8;
	sp++;
	cpu_PC |= data_read_byte(sp);
	data_write_word(SPL, sp);
}


// perform the addition and set the appropriate flags
static void do_addition_8(int rd, int rr, int carry)
{
	int value1, value2, sreg, result;

	value1 = get_reg(rd);
	value2 = get_reg(rr);

	result = value1 + value2 + carry;

	sreg = flag_update_table_add8[FUT_ADD_SUB_INDEX(value1, value2, result)];
	update_flags(FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);

	put_reg(rd, result & 0xFF);
}

// perform the subtraction and set the appropriate flags
static int do_subtraction_8(int value1, int value2, int carry, int use_carry)
{
	int sreg, result = value1 - value2 - carry;

	sreg = flag_update_table_sub8[FUT_ADD_SUB_INDEX(value1, value2, result)];
	if (use_carry && ((data_read_byte(SREG) & FLAG_Z) == 0))
		sreg &= ~FLAG_Z;
	update_flags(FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);

	return result & 0xFF;
}

static void store_logical_result(int rd, int result)
{
	put_reg(rd, result);
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z, flag_update_table_logical[result]);
}

/* 10q0 qq0d dddd 1qqq | LDD */

static void load_indirect(int rd, int ind_register, int adjust, int displacement_bits)
{
	//TODO: use RAMPx registers to address more than 64kb of RAM
	int ind_value = get_word_reg(ind_register);

	if (adjust < 0)
		ind_value += adjust;
	put_reg(rd, data_read_byte(ind_value + displacement_bits));
	if (adjust > 0)
		ind_value += adjust;
	if (adjust)
		put_word_reg(ind_register, ind_value);
}

static void store_indirect(int rd, int ind_register, int adjust, int displacement_bits)
{
	//TODO: use RAMPx registers to address more than 64kb of RAM
	int ind_value = get_word_reg(ind_register);

	if (adjust < 0)
		ind_value += adjust;
	data_write_byte(ind_value + displacement_bits, get_reg(rd));
	if (adjust > 0)
		ind_value += adjust;
	if (adjust)
		put_word_reg(ind_register, ind_value);
}

static void load_program_memory(int rd, int use_RAMPZ, int incr)
{
	int address = get_word_reg(REGZ);
	if (use_RAMPZ)
		address |= data_read_byte(RAMPZ) << 16;
	put_reg(rd, flash_read_byte(address));
	if (incr) {
		address++;
		put_word_reg(REGZ, address & 0xFFFF);
		if (use_RAMPZ)
			data_write_byte(RAMPZ, address >> 16);
	}
}

static void skip_instruction_on_condition(int condition)
{
	int size;
	if (condition) {
		size = opcode_func_array[decoded_flash[cpu_PC].data_index].size;
		cpu_PC += size;
		add_program_cycles(size);
	}
}

static void branch_on_sreg_condition(int rd, int rr, int flag_value)
{
	if (((data_read_byte(SREG) & rr) != 0) == flag_value) {
		int delta = rd;
		if (delta & 0x40) delta |= 0xFFFFFF80;
		cpu_PC += delta;
		add_program_cycles(1);
	}
}

static void rotate_right(int rd, int value, int top_bit)
{
	if (top_bit)
		value |= 0x100;
	put_reg(rd, value >> 1);

	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, flag_update_table_ror8[value]);
}

static void do_multiply(int rd, int rr, int signed1, int signed2, int shift)
{
	int v1, v2, result, sreg;

	v1 = signed1 ? ((signed char) get_reg(rd)) : get_reg(rd);
	v2 = signed2 ? ((signed char) get_reg(rr)) : get_reg(rr);
	result = (v1 * v2) & 0xFFFF;

	sreg = 0;
	if (result & 0x8000)
		sreg |= FLAG_C;
	if (shift)
		result <<= 1;
	if (result == 0)
		sreg |= FLAG_Z;
	update_flags(FLAG_Z | FLAG_C, sreg);
	put_word_reg(0, result);
}

// handle illegal instructions
static OP_FUNC_TYPE avr_op_ILLEGAL(int rd, int rr)
{
	char buf[128];
	sprintf(buf, "illegal opcode %04x", rr);
	leave(EXIT_STATUS_ABORTED, buf);
}

// ---------------------------------------------------------------------------------
//     opcode execution functions

/* opcodes with no operands */
/* 1001 0101 0001 1001 | EICALL */
static OP_FUNC_TYPE avr_op_EICALL(int rd, int rr)
{
	avr_op_ILLEGAL(0,0x9519);
	//TODO
}

/* 1001 0100 0001 1001 | EIJMP */
static OP_FUNC_TYPE avr_op_EIJMP(int rd, int rr)
{
	avr_op_ILLEGAL(0,0x9419);
	//TODO
}

/* 1001 0101 1101 1000 | ELPM */
static OP_FUNC_TYPE avr_op_ELPM(int rd, int rr)
{
	load_program_memory(0, 1, 0);
}

/* 1001 0101 1111 1000 | ESPM */
static OP_FUNC_TYPE avr_op_ESPM(int rd, int rr)
{
	avr_op_ILLEGAL(0, 0x95F8);
	//TODO
}

/* 1001 0101 0000 1001 | ICALL */
static OP_FUNC_TYPE avr_op_ICALL(int rd, int rr)
{
	push_PC();
	cpu_PC = get_word_reg(REGZ);
	if (PC_is_22_bits)
		add_program_cycles(1);
}

/* 1001 0100 0000 1001 | IJMP */
static OP_FUNC_TYPE avr_op_IJMP(int rd, int rr)
{
	cpu_PC = get_word_reg(REGZ);
}

/* 1001 0101 1100 1000 | LPM */
static OP_FUNC_TYPE avr_op_LPM(int rd, int rr)
{
	load_program_memory(0, 0, 0);
}

/* 0000 0000 0000 0000 | NOP */
static OP_FUNC_TYPE avr_op_NOP(int rd, int rr)
{
}

/* 1001 0101 0000 1000 | RET */
static OP_FUNC_TYPE avr_op_RET(int rd, int rr)
{
	pop_PC();
	if (PC_is_22_bits)
		add_program_cycles(1);
}

/* 1001 0101 0001 1000 | RETI */
static OP_FUNC_TYPE avr_op_RETI(int rd, int rr)
{
	avr_op_RET(rd,rr);
	update_flags(FLAG_I, FLAG_I);
}

/* 1001 0101 1000 1000 | SLEEP */
static OP_FUNC_TYPE avr_op_SLEEP(int rd, int rr)
{
	// we don't have anything to wake us up, so just pretend we wake up immediately
}

/* 1001 0101 1110 1000 | SPM */
static OP_FUNC_TYPE avr_op_SPM(int rd, int rr)
{
	avr_op_ILLEGAL(0,0x95E8);
	//TODO
}

/* 1001 0101 1010 1000 | WDR */
static OP_FUNC_TYPE avr_op_WDR(int rd, int rr)
{
	// we don't have a watchdog, so do nothing
}


/* opcodes with two 5-bit register (Rd and Rr) operands */
/* 0001 11rd dddd rrrr | ADC or ROL */
static OP_FUNC_TYPE avr_op_ADC(int rd, int rr)
{
	do_addition_8(rd,rr,get_carry());
}

/* 0000 11rd dddd rrrr | ADD or LSL */
static OP_FUNC_TYPE avr_op_ADD(int rd, int rr)
{
	do_addition_8(rd,rr,0);
}

/* 0010 00rd dddd rrrr | AND or TST */
static OP_FUNC_TYPE avr_op_AND(int rd, int rr)
{
	int result = get_reg(rd) & get_reg(rr);
	store_logical_result(rd,result);
}

/* 0001 01rd dddd rrrr | CP */
static OP_FUNC_TYPE avr_op_CP(int rd, int rr)
{
	do_subtraction_8(get_reg(rd), get_reg(rr), 0, 0);
}

/* 0000 01rd dddd rrrr | CPC */
static OP_FUNC_TYPE avr_op_CPC(int rd, int rr)
{
	do_subtraction_8(get_reg(rd), get_reg(rr), get_carry(), 1);
}

/* 0001 00rd dddd rrrr | CPSE */
static OP_FUNC_TYPE avr_op_CPSE(int rd, int rr)
{
	skip_instruction_on_condition(get_reg(rd) ==  get_reg(rr));
}

/* 0010 01rd dddd rrrr | EOR or CLR */
static OP_FUNC_TYPE avr_op_EOR(int rd, int rr)
{
	int result = get_reg(rd) ^ get_reg(rr);
	store_logical_result(rd,result);
}

/* 0010 11rd dddd rrrr | MOV */
static OP_FUNC_TYPE avr_op_MOV(int rd, int rr)
{
	put_reg(rd, get_reg(rr));
}

/* 1001 11rd dddd rrrr | MUL */
static OP_FUNC_TYPE avr_op_MUL(int rd, int rr)
{
	do_multiply(rd,rr,0,0,0);
}

/* 0010 10rd dddd rrrr | OR */
static OP_FUNC_TYPE avr_op_OR(int rd, int rr)
{
	int result = get_reg(rd) | get_reg(rr);
	store_logical_result(rd, result);
}

/* 0000 10rd dddd rrrr | SBC */
static OP_FUNC_TYPE avr_op_SBC(int rd, int rr)
{
	put_reg(rd, do_subtraction_8(get_reg(rd), get_reg(rr), get_carry(), 1));
}

/* 0001 10rd dddd rrrr | SUB */
static OP_FUNC_TYPE avr_op_SUB(int rd, int rr)
{
	put_reg(rd, do_subtraction_8(get_reg(rd), get_reg(rr), 0, 0));
}


/* opcode with a single register (Rd) as operand */
/* 1001 010d dddd 0101 | ASR */
static OP_FUNC_TYPE avr_op_ASR(int rd, int rr)
{
	int value = get_reg(rd);
	rotate_right(rd, value, value & 0x80);
}

/* 1001 010d dddd 0000 | COM */
static OP_FUNC_TYPE avr_op_COM(int rd, int rr)
{
	int result = (~get_reg(rd)) & 0xFF;
	put_reg(rd, result);
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, flag_update_table_logical[result] | FLAG_C);
}

/* 1001 010d dddd 1010 | DEC */
static OP_FUNC_TYPE avr_op_DEC(int rd, int rr)
{
	int result = (get_reg(rd) - 1) & 0xFF;
	put_reg(rd, result);
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z, flag_update_table_dec[result]);
}

/* 1001 000d dddd 0110 | ELPM */
static OP_FUNC_TYPE avr_op_ELPM_Z(int rd, int rr)
{
	load_program_memory(rd, 1, 0);
}

/* 1001 000d dddd 0111 | ELPM */
static OP_FUNC_TYPE avr_op_ELPM_Z_incr(int rd, int rr)
{
	load_program_memory(rd, 1, 1);
}

/* 1001 010d dddd 0011 | INC */
static OP_FUNC_TYPE avr_op_INC(int rd, int rr)
{
	int result = (get_reg(rd) + 1) & 0xFF;
	put_reg(rd, result);
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z, flag_update_table_inc[result]);
}

/* 1001 000d dddd 0000 | LDS */
static OP_FUNC_TYPE avr_op_LDS(int rd, int rr)
{
	//TODO:RAMPD
	put_reg(rd, data_read_byte(rr));
}

/* 1001 000d dddd 1100 | LD */
static OP_FUNC_TYPE avr_op_LD_X(int rd, int rr)
{
	load_indirect(rd, REGX, 0, 0);
}

/* 1001 000d dddd 1110 | LD */
static OP_FUNC_TYPE avr_op_LD_X_decr(int rd, int rr)
{
	load_indirect(rd, REGX, -1, 0);
}

/* 1001 000d dddd 1101 | LD */
static OP_FUNC_TYPE avr_op_LD_X_incr(int rd, int rr)
{
	load_indirect(rd, REGX, 1, 0);
}

/* 1001 000d dddd 1010 | LD */
static OP_FUNC_TYPE avr_op_LD_Y_decr(int rd, int rr)
{
	load_indirect(rd, REGY, -1, 0);
}

/* 1001 000d dddd 1001 | LD */
static OP_FUNC_TYPE avr_op_LD_Y_incr(int rd, int rr)
{
	load_indirect(rd, REGY, 1, 0);
}

/* 1001 000d dddd 0010 | LD */
static OP_FUNC_TYPE avr_op_LD_Z_decr(int rd, int rr)
{
	load_indirect(rd, REGZ, -1, 0);
}

/* 1001 000d dddd 0010 | LD */
static OP_FUNC_TYPE avr_op_LD_Z_incr(int rd, int rr)
{
	load_indirect(rd, REGZ, 1, 0);
}

/* 1001 000d dddd 0100 | LPM Z */
static OP_FUNC_TYPE avr_op_LPM_Z(int rd, int rr)
{
	load_program_memory(rd, 0, 0);
}

/* 1001 000d dddd 0101 | LPM Z+ */
static OP_FUNC_TYPE avr_op_LPM_Z_incr(int rd, int rr)
{
	load_program_memory(rd, 0, 1);
}

/* 1001 010d dddd 0110 | LSR */
static OP_FUNC_TYPE avr_op_LSR(int rd, int rr)
{
	rotate_right(rd, get_reg(rd), 0);
}

/* 1001 010d dddd 0001 | NEG */
static OP_FUNC_TYPE avr_op_NEG(int rd, int rr)
{
	put_reg(rd, do_subtraction_8(0, get_reg(rd), 0, 0));
}

/* 1001 000d dddd 1111 | POP */
static OP_FUNC_TYPE avr_op_POP(int rd, int rr)
{
	put_reg(rd, pop_byte());
}

/* 1001 001d dddd 1111 | PUSH */
static OP_FUNC_TYPE avr_op_PUSH(int rd, int rr)
{
	push_byte(get_reg(rd));
}

/* 1001 010d dddd 0111 | ROR */
static OP_FUNC_TYPE avr_op_ROR(int rd, int rr)
{
	rotate_right(rd, get_reg(rd), get_carry());
}

/* 1001 001d dddd 0000 | STS */
static OP_FUNC_TYPE avr_op_STS(int rd, int rr)
{
	//TODO:RAMPD
	data_write_byte(rr, get_reg(rd));
}

/* 1001 001d dddd 1100 | ST */
static OP_FUNC_TYPE avr_op_ST_X(int rd, int rr)
{
	store_indirect(rd, REGX, 0, 0);
}

/* 1001 001d dddd 1110 | ST */
static OP_FUNC_TYPE avr_op_ST_X_decr(int rd, int rr)
{
	store_indirect(rd, REGX, -1, 0);
}

/* 1001 001d dddd 1101 | ST */
static OP_FUNC_TYPE avr_op_ST_X_incr(int rd, int rr)
{
	store_indirect(rd, REGX, 1, 0);
}

/* 1001 001d dddd 1010 | ST */
static OP_FUNC_TYPE avr_op_ST_Y_decr(int rd, int rr)
{
	store_indirect(rd, REGY, -1, 0);
}

/* 1001 001d dddd 1001 | ST */
static OP_FUNC_TYPE avr_op_ST_Y_incr(int rd, int rr)
{
	store_indirect(rd, REGY, 1, 0);
}

/* 1001 001d dddd 0010 | ST */
static OP_FUNC_TYPE avr_op_ST_Z_decr(int rd, int rr)
{
	store_indirect(rd, REGZ, -1, 0);
}

/* 1001 001d dddd 0001 | ST */
static OP_FUNC_TYPE avr_op_ST_Z_incr(int rd, int rr)
{
	store_indirect(rd, REGZ, 1, 0);
}

/* 1001 010d dddd 0010 | SWAP */
static OP_FUNC_TYPE avr_op_SWAP(int rd, int rr)
{
	int value = get_reg(rd);
	put_reg(rd, ((value << 4) & 0xF0) | ((value >> 4) & 0x0F));
}

/* opcodes with a register (Rd) and a constant data (K) as operands */
/* 0111 KKKK dddd KKKK | CBR or ANDI */
static OP_FUNC_TYPE avr_op_ANDI(int rd, int rr)
{
	int result = get_reg(rd) & rr;
	store_logical_result(rd, result);
}

/* 0011 KKKK dddd KKKK | CPI */
static OP_FUNC_TYPE avr_op_CPI(int rd, int rr)
{
	do_subtraction_8(get_reg(rd), rr, 0, 0);
}

/* 1110 KKKK dddd KKKK | LDI or SER */
static OP_FUNC_TYPE avr_op_LDI(int rd, int rr)
{
	put_reg(rd, rr);
}

/* 0110 KKKK dddd KKKK | SBR or ORI */
static OP_FUNC_TYPE avr_op_ORI(int rd, int rr)
{
	int result = get_reg(rd) | rr;
	store_logical_result(rd, result);
}

/* 0100 KKKK dddd KKKK | SBCI */
/* 0101 KKKK dddd KKKK | SUBI */
static OP_FUNC_TYPE avr_op_SBCI_SUBI(int rd, int rr, int carry, int use_carry)
{
	put_reg(rd, do_subtraction_8(get_reg(rd), rr, carry, use_carry));
}

static OP_FUNC_TYPE avr_op_SBCI(int rd, int rr)
{
	avr_op_SBCI_SUBI(rd, rr, get_carry(), 1);
}

static OP_FUNC_TYPE avr_op_SUBI(int rd, int rr)
{
	avr_op_SBCI_SUBI(rd, rr, 0, 0);
}


/* opcodes with a register (Rd) and a register bit number (b) as operands */
/* 1111 100d dddd 0bbb | BLD */
static OP_FUNC_TYPE avr_op_BLD(int rd, int rr)
{
	int value = get_reg(rd);
	if (data_read_byte(SREG) & FLAG_T)
		value |= rr;
	else
		value &= ~rr;
	put_reg(rd, value);
}

/* 1111 101d dddd 0bbb | BST */
static OP_FUNC_TYPE avr_op_BST(int rd, int rr)
{
	int bit = get_reg(rd) & rr;
	update_flags(FLAG_T, bit ? FLAG_T : 0);
}

/* 1111 110d dddd 0bbb | SBRC */
static OP_FUNC_TYPE avr_op_SBRC(int rd, int rr)
{
	skip_instruction_on_condition(!(get_reg(rd) & rr));
}

/* 1111 111d dddd 0bbb | SBRS */
static OP_FUNC_TYPE avr_op_SBRS(int rd, int rr)
{
	skip_instruction_on_condition(get_reg(rd) & rr);
}

/* opcodes with a relative 7-bit address (k) and a register bit number (b) as operands */
/* 1111 01kk kkkk kbbb | BRBC */
static OP_FUNC_TYPE avr_op_BRBC(int rd, int rr)
{
	branch_on_sreg_condition(rd, rr, 0);
}

/* 1111 00kk kkkk kbbb | BRBS */
static OP_FUNC_TYPE avr_op_BRBS(int rd, int rr)
{
	branch_on_sreg_condition(rd, rr, 1);
}


/* opcodes with a 6-bit address displacement (q) and a register (Rd) as operands */
/* 10q0 qq0d dddd 1qqq | LDD */
static OP_FUNC_TYPE avr_op_LDD_Y(int rd, int rr)
{
	load_indirect(rd, REGY, 0, rr);
}

/* 10q0 qq0d dddd 0qqq | LDD */
static OP_FUNC_TYPE avr_op_LDD_Z(int rd, int rr)
{
	load_indirect(rd, REGZ, 0, rr);
}

/* 10q0 qq1d dddd 1qqq | STD */
static OP_FUNC_TYPE avr_op_STD_Y(int rd, int rr)
{
	store_indirect(rd, REGY, 0, rr);
}

/* 10q0 qq1d dddd 0qqq | STD */
static OP_FUNC_TYPE avr_op_STD_Z(int rd, int rr)
{
	store_indirect(rd, REGZ, 0, rr);
}


/* opcodes with a absolute 22-bit address (k) operand */
/* 1001 010k kkkk 110k | JMP */
static OP_FUNC_TYPE avr_op_JMP(int rd, int rr)
{
	cpu_PC = rr | (rd << 16);
}

/* 1001 010k kkkk 111k | CALL */
static OP_FUNC_TYPE avr_op_CALL(int rd, int rr)
{
	push_PC();
	cpu_PC = rr | (rd << 16);
	if (PC_is_22_bits)
		add_program_cycles(1);
}


/* opcode with a sreg bit select (s) operand */
/* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
/* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
/* 1001 0100 1sss 1000 | BCLR */
static OP_FUNC_TYPE avr_op_BCLR(int rd, int rr)
{
	update_flags(rd, 0);
}

/* 1001 0100 0sss 1000 | BSET */
static OP_FUNC_TYPE avr_op_BSET(int rd, int rr)
{
	update_flags(rd, rd);
}


/* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
/* 1001 0110 KKdd KKKK | ADIW */
static OP_FUNC_TYPE avr_op_ADIW(int rd, int rr)
{
	int svalue, evalue, sreg;

	svalue = get_word_reg(rd);
	evalue = svalue + rr;
	put_word_reg(rd, evalue);

	sreg = flag_update_table_add8[FUT_ADDSUB16_INDEX(svalue, evalue)];
	sreg &= ~FLAG_H;
	if ((evalue & 0xFFFF) != 0x0000)
		sreg &= ~FLAG_Z;

	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

/* 1001 0111 KKdd KKKK | SBIW */
static OP_FUNC_TYPE avr_op_SBIW(int rd, int rr)
{
	int svalue, evalue, sreg;

	svalue = get_word_reg(rd);
	evalue = svalue - rr;
	put_word_reg(rd, evalue);

	sreg = flag_update_table_sub8[FUT_ADDSUB16_INDEX(svalue, evalue)];
	sreg &= ~FLAG_H;
	if ((evalue & 0xFFFF) != 0x0000)
		sreg &= ~FLAG_Z;

	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}


/* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
/* 1001 1000 AAAA Abbb | CBI */
static OP_FUNC_TYPE avr_op_CBI(int rd, int rr)
{
	data_write_byte(rd, data_read_byte(rd) & ~(rr));
}

/* 1001 1010 AAAA Abbb | SBI */
static OP_FUNC_TYPE avr_op_SBI(int rd, int rr)
{
	data_write_byte(rd, data_read_byte(rd) | rr);
}

/* 1001 1001 AAAA Abbb | SBIC */
static OP_FUNC_TYPE avr_op_SBIC(int rd, int rr)
{
	skip_instruction_on_condition(!(data_read_byte(rd) & rr));
}

/* 1001 1011 AAAA Abbb | SBIS */
static OP_FUNC_TYPE avr_op_SBIS(int rd, int rr)
{
	skip_instruction_on_condition(data_read_byte(rd) & rr);
}


/* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
/* 1011 0AAd dddd AAAA | IN */
static OP_FUNC_TYPE avr_op_IN(int rd, int rr)
{
	put_reg(rd, data_read_byte(rr));
}

/* 1011 1AAd dddd AAAA | OUT */
static OP_FUNC_TYPE avr_op_OUT(int rd, int rr)
{
	data_write_byte(rr, get_reg(rd));
}


/* opcodes with a relative 12-bit address (k) operand */
/* 1100 kkkk kkkk kkkk | RJMP */
static OP_FUNC_TYPE avr_op_RJMP(int rd, int rr)
{
	int delta = rr;
	if (delta & 0x800) delta |= 0xFFFFF000;

	// special case: endless loop usually means that the program has ended
	if (delta == -1)
		leave(EXIT_STATUS_EXIT, "infinite loop detected (normal exit)");
	cpu_PC += delta;
}

/* 1101 kkkk kkkk kkkk | RCALL */
static OP_FUNC_TYPE avr_op_RCALL(int rd, int rr)
{
	int delta = rr;
	if (delta & 0x800) delta |= 0xFFFFF000;

	push_PC();
	cpu_PC += delta;
	if (PC_is_22_bits)
		add_program_cycles(1);
}


/* opcodes with two 4-bit register (Rd and Rr) operands */
/* 0000 0001 dddd rrrr | MOVW */
static OP_FUNC_TYPE avr_op_MOVW(int rd, int rr)
{
	put_word_reg(rd, get_word_reg(rr));
}

/* 0000 0010 dddd rrrr | MULS */
static OP_FUNC_TYPE avr_op_MULS(int rd, int rr)
{
	do_multiply(rd, rr, 1, 1, 0);
}

/* opcodes with two 3-bit register (Rd and Rr) operands */
/* 0000 0011 0ddd 0rrr | MULSU */
static OP_FUNC_TYPE avr_op_MULSU(int rd, int rr)
{
	do_multiply(rd, rr, 1, 0, 0);
}

/* 0000 0011 0ddd 1rrr | FMUL */
static OP_FUNC_TYPE avr_op_FMUL(int rd, int rr)
{
	do_multiply(rd, rr, 0, 0, 1);
}

/* 0000 0011 1ddd 0rrr | FMULS */
static OP_FUNC_TYPE avr_op_FMULS(int rd, int rr)
{
	do_multiply(rd, rr, 1, 1, 1);
}

/* 0000 0011 1ddd 1rrr | FMULSU */
static OP_FUNC_TYPE avr_op_FMULSU(int rd, int rr)
{
	do_multiply(rd, rr, 1, 0, 1);
}


// ---------------------------------------------------------------------------------
//     parse command line arguments

static int load_to_flash(const char *filename)
{
	FILE *fp;

	fp = fopen(filename, "rb");
	if (!fp) {
		perror(filename);
		return 1;
	}
	program_size = fread(cpu_flash, 1, MAX_FLASH_SIZE, fp);
	fclose(fp);

	// keep the program name, so that it can be used later in output messages
	strncpy(program_name, filename, sizeof(program_name));
	program_name[sizeof(program_name) - 1] = '\0';

	return 0;
}

static int parse_args(int argc, char *argv[])
{
	char buffer[600];
	int i;

	// setup default values
	flash_size = 128 * 1024;
	//max_instr_count = 1000000000;
	max_instr_count = 0;

	// parse command line arguments
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			if (load_to_flash(argv[i]))
				return 1;

			// handle elf files
			if (memcmp(cpu_flash, "\177ELF", 4) == 0 && strlen(argv[i]) < 256) {
				sprintf(buffer, "avr-objcopy -j .text -j .data -O binary %s %s.bin", argv[i], argv[i]);
				system(buffer);
				sprintf(buffer, "%s.bin", argv[i]);
				if (load_to_flash(buffer))
					return 1;
			}
			continue;
		}
	}

	// setup auxiliary flags
	PC_is_22_bits = (flash_size > 128 * 1024);

	return 0;
}


// ---------------------------------------------------------------------------------
//     flash pre-decoding functions

opcode_data opcode_func_array[] = {
	[avr_op_index_dummy] = {NULL,0,0,NULL},	// dummy entry to guarantee that "zero" is an invalid function

	[avr_op_index_ADC] =       { avr_op_ADC,         1, 1, "ADC"     },
	[avr_op_index_ADD] =       { avr_op_ADD,         1, 1, "ADD"     },
	[avr_op_index_ADIW] =      { avr_op_ADIW,        1, 2, "ADIW"    },
	[avr_op_index_SBIW] =      { avr_op_SBIW,        1, 2, "SBIW"    },
	[avr_op_index_AND] =       { avr_op_AND,         1, 1, "AND"     },
	[avr_op_index_ANDI] =      { avr_op_ANDI,        1, 1, "ANDI"    },
	[avr_op_index_ASR] =       { avr_op_ASR,         1, 1, "ASR"     },
	[avr_op_index_BCLR] =      { avr_op_BCLR,        1, 1, "BCLR"    },
	[avr_op_index_BLD] =       { avr_op_BLD,         1, 1, "BLD"     },
	[avr_op_index_BRBC] =      { avr_op_BRBC,        1, 1, "BRBC"    }, // may need extra cycles
	[avr_op_index_BRBS] =      { avr_op_BRBS,        1, 1, "BRBS"    }, // may need extra cycles
	[avr_op_index_BSET] =      { avr_op_BSET,        1, 1, "BSET"    },
	[avr_op_index_BST] =       { avr_op_BST,         1, 1, "BST"     },
	[avr_op_index_CALL] =      { avr_op_CALL,        2, 4, "CALL"    }, // may need extra cycles
	[avr_op_index_CBI] =       { avr_op_CBI,         1, 2, "CBI"     },
	[avr_op_index_COM] =       { avr_op_COM,         1, 1, "COM"     },
	[avr_op_index_CP] =        { avr_op_CP,          1, 1, "CP"      },
	[avr_op_index_CPC] =       { avr_op_CPC,         1, 1, "CPC"     },
	[avr_op_index_CPI] =       { avr_op_CPI,         1, 1, "CPI"     },
	[avr_op_index_CPSE] =      { avr_op_CPSE,        1, 1, "CPSE"    }, // may need extra cycles
	[avr_op_index_DEC] =       { avr_op_DEC,         1, 1, "DEC"     },
	[avr_op_index_EICALL] =    { avr_op_EICALL,      1, 4, "EICALL"  },
	[avr_op_index_EIJMP] =     { avr_op_EIJMP,       1, 2, "EIJMP"   },
	[avr_op_index_ELPM] =      { avr_op_ELPM,        1, 3, "ELPM"    },
	[avr_op_index_ELPM_Z] =    { avr_op_ELPM_Z,      1, 3, "ELPM Z"  },
	[avr_op_index_ELPM_Z_incr]={ avr_op_ELPM_Z_incr, 1, 3, "ELPM Z+" },
	[avr_op_index_EOR] =       { avr_op_EOR,         1, 1, "EOR"     },
	[avr_op_index_ESPM] =      { avr_op_ESPM,        1, 1, "ESPM"    },
	[avr_op_index_FMUL] =      { avr_op_FMUL,        1, 2, "FMUL"    },
	[avr_op_index_FMULS] =     { avr_op_FMULS,       1, 2, "FMULS"   },
	[avr_op_index_FMULSU] =    { avr_op_FMULSU,      1, 2, "FMULSU"  },
	[avr_op_index_ICALL] =     { avr_op_ICALL,       1, 3, "ICALL"   }, // may need extra cycles
	[avr_op_index_IJMP] =      { avr_op_IJMP,        1, 2, "IJMP"    },
	[avr_op_index_ILLEGAL] =   { avr_op_ILLEGAL,     1, 1, "ILLEGAL" },
	[avr_op_index_IN] =        { avr_op_IN,          1, 1, "IN"      },
	[avr_op_index_INC] =       { avr_op_INC,         1, 1, "INC"     },
	[avr_op_index_JMP] =       { avr_op_JMP,         2, 3, "JMP"     },
	[avr_op_index_LDD_Y] =     { avr_op_LDD_Y,       1, 2, "LDD r,Y+q" },
	[avr_op_index_LDD_Z] =     { avr_op_LDD_Z,       1, 2, "LDD r,Z+q" },
	[avr_op_index_LDI] =       { avr_op_LDI,         1, 1, "LDI"     },
	[avr_op_index_LDS] =       { avr_op_LDS,         2, 2, "LDS"     },
	[avr_op_index_LD_X] =      { avr_op_LD_X,        1, 2, "LD r,X"  },
	[avr_op_index_LD_X_decr] = { avr_op_LD_X_decr,   1, 2, "LD r,-X" },
	[avr_op_index_LD_X_incr] = { avr_op_LD_X_incr,   1, 2, "LD r,X+" },
	[avr_op_index_LD_Y_decr] = { avr_op_LD_Y_decr,   1, 2, "LD r,-Y" },
	[avr_op_index_LD_Y_incr] = { avr_op_LD_Y_incr,   1, 2, "LD r,Y+" },
	[avr_op_index_LD_Z_decr] = { avr_op_LD_Z_decr,   1, 2, "LD r,-Z" },
	[avr_op_index_LD_Z_incr] = { avr_op_LD_Z_incr,   1, 2, "LD r,Z+" },
	[avr_op_index_LPM] =       { avr_op_LPM,         1, 3, "LPM"     },
	[avr_op_index_LPM_Z] =     { avr_op_LPM_Z,       1, 3, "LPM Z"   },
	[avr_op_index_LPM_Z_incr]= { avr_op_LPM_Z_incr,  1, 3, "LPM Z+"  },
	[avr_op_index_LSR] =       { avr_op_LSR,         1, 1, "LSR"     },
	[avr_op_index_MOV] =       { avr_op_MOV,         1, 1, "MOV"     },
	[avr_op_index_MOVW] =      { avr_op_MOVW,        1, 1, "MOVW"    },
	[avr_op_index_MUL] =       { avr_op_MUL,         1, 2, "MUL"     },
	[avr_op_index_MULS] =      { avr_op_MULS,        1, 2, "MULS"    },
	[avr_op_index_MULSU] =     { avr_op_MULSU,       1, 2, "MULSU"   },
	[avr_op_index_NEG] =       { avr_op_NEG,         1, 1, "NEG"     },
	[avr_op_index_NOP] =       { avr_op_NOP,         1, 1, "NOP"     },
	[avr_op_index_OR] =        { avr_op_OR,          1, 1, "OR"      },
	[avr_op_index_ORI] =       { avr_op_ORI,         1, 1, "ORI"     },
	[avr_op_index_OUT] =       { avr_op_OUT,         1, 1, "OUT"     },
	[avr_op_index_POP] =       { avr_op_POP,         1, 2, "POP"     },
	[avr_op_index_PUSH] =      { avr_op_PUSH,        1, 2, "PUSH"    },
	[avr_op_index_RCALL] =     { avr_op_RCALL,       1, 3, "RCALL"   }, // may need extra cycles
	[avr_op_index_RET] =       { avr_op_RET,         1, 4, "RET"     }, // may need extra cycles
	[avr_op_index_RETI] =      { avr_op_RETI,        1, 4, "RETI"    }, // may need extra cycles
	[avr_op_index_RJMP] =      { avr_op_RJMP,        1, 2, "RJMP"    },
	[avr_op_index_ROR] =       { avr_op_ROR,         1, 1, "ROR"     },
	[avr_op_index_SBC] =       { avr_op_SBC,         1, 1, "SBC"     },
	[avr_op_index_SBCI] =      { avr_op_SBCI,        1, 1, "SBCI"    },
	[avr_op_index_SBI] =       { avr_op_SBI,         1, 2, "SBI"     },
	[avr_op_index_SBIC] =      { avr_op_SBIC,        1, 1, "SBIC"    }, // may need extra cycles
	[avr_op_index_SBIS] =      { avr_op_SBIS,        1, 1, "SBIS"    }, // may need extra cycles
	[avr_op_index_SBRC] =      { avr_op_SBRC,        1, 1, "SBRC"    }, // may need extra cycles
	[avr_op_index_SBRS] =      { avr_op_SBRS,        1, 1, "SBRS"    }, // may need extra cycles
	[avr_op_index_SLEEP] =     { avr_op_SLEEP,       1, 1, "SLEEP"   },
	[avr_op_index_SPM] =       { avr_op_SPM,         1, 1, "SPM"     },
	[avr_op_index_STD_Y] =     { avr_op_STD_Y,       1, 2, "STD Y+q,r" },
	[avr_op_index_STD_Z] =     { avr_op_STD_Z,       1, 2, "STD Z+q,r" },
	[avr_op_index_STS] =       { avr_op_STS,         2, 2, "STS"     },
	[avr_op_index_ST_X] =      { avr_op_ST_X,        1, 2, "ST X,r"  },
	[avr_op_index_ST_X_decr] = { avr_op_ST_X_decr,   1, 2, "ST -X,r" },
	[avr_op_index_ST_X_incr] = { avr_op_ST_X_incr,   1, 2, "ST X+,r" },
	[avr_op_index_ST_Y_decr] = { avr_op_ST_Y_decr,   1, 2, "ST -Y,r" },
	[avr_op_index_ST_Y_incr] = { avr_op_ST_Y_incr,   1, 2, "ST Y+,r" },
	[avr_op_index_ST_Z_decr] = { avr_op_ST_Z_decr,   1, 2, "ST -Z,r" },
	[avr_op_index_ST_Z_incr] = { avr_op_ST_Z_incr,   1, 2, "ST Z+,r" },
	[avr_op_index_SUB] =       { avr_op_SUB,         1, 1, "SUB"     },
	[avr_op_index_SUBI] =      { avr_op_SUBI,        1, 1, "SUBI"    },
	[avr_op_index_SWAP] =      { avr_op_SWAP,        1, 1, "SWAP"    },
	[avr_op_index_WDR] =       { avr_op_WDR,         1, 1, "WDR"     },
};

static int decode_opcode(decoded_op *op, word opcode1, word opcode2)
{
	int decode;

	// start with default arguments
	op->oper1 = 0;
	op->oper2 = 0;

	/* opcodes with no operands */
	switch (opcode1) {
		case 0x9519: return avr_op_index_EICALL;       /* 1001 0101 0001 1001 | EICALL */
		case 0x9419: return avr_op_index_EIJMP;        /* 1001 0100 0001 1001 | EIJMP */
		case 0x95D8: return avr_op_index_ELPM;         /* 1001 0101 1101 1000 | ELPM */
		case 0x95F8: return avr_op_index_ESPM;         /* 1001 0101 1111 1000 | ESPM */
		case 0x9509: return avr_op_index_ICALL;        /* 1001 0101 0000 1001 | ICALL */
		case 0x9409: return avr_op_index_IJMP;         /* 1001 0100 0000 1001 | IJMP */
		case 0x95C8: return avr_op_index_LPM;          /* 1001 0101 1100 1000 | LPM */
		case 0x0000: return avr_op_index_NOP;          /* 0000 0000 0000 0000 | NOP */
		case 0x9508: return avr_op_index_RET;          /* 1001 0101 0000 1000 | RET */
		case 0x9518: return avr_op_index_RETI;         /* 1001 0101 0001 1000 | RETI */
		case 0x9588: return avr_op_index_SLEEP;        /* 1001 0101 1000 1000 | SLEEP */
		case 0x95E8: return avr_op_index_SPM;          /* 1001 0101 1110 1000 | SPM */
		case 0x95A8: return avr_op_index_WDR;          /* 1001 0101 1010 1000 | WDR */
	}

	// since there are a lot of opcodes that use these parameters, we
	// extract them here to simplify each opcode decoding logic
	op->oper1 = ((opcode1 >> 4) & 0x1F);
	op->oper2 = (opcode1 & 0x0F) | ((opcode1 >> 5) & 0x0010);

	/* opcodes with two 5-bit register (Rd and Rr) operands */
	decode = opcode1 & ~(mask_Rd_5 | mask_Rr_5);
	switch ( decode ) {
		case 0x1C00: return avr_op_index_ADC;         /* 0001 11rd dddd rrrr | ADC or ROL */
		case 0x0C00: return avr_op_index_ADD;         /* 0000 11rd dddd rrrr | ADD or LSL */
		case 0x2000: return avr_op_index_AND;         /* 0010 00rd dddd rrrr | AND or TST */
		case 0x1400: return avr_op_index_CP;          /* 0001 01rd dddd rrrr | CP */
		case 0x0400: return avr_op_index_CPC;         /* 0000 01rd dddd rrrr | CPC */
		case 0x1000: return avr_op_index_CPSE;        /* 0001 00rd dddd rrrr | CPSE */
		case 0x2400: return avr_op_index_EOR;         /* 0010 01rd dddd rrrr | EOR or CLR */
		case 0x2C00: return avr_op_index_MOV;         /* 0010 11rd dddd rrrr | MOV */
		case 0x9C00: return avr_op_index_MUL;         /* 1001 11rd dddd rrrr | MUL */
		case 0x2800: return avr_op_index_OR;          /* 0010 10rd dddd rrrr | OR */
		case 0x0800: return avr_op_index_SBC;         /* 0000 10rd dddd rrrr | SBC */
		case 0x1800: return avr_op_index_SUB;         /* 0001 10rd dddd rrrr | SUB */
	}

	/* opcodes with a single register (Rd) as operand */
	decode = opcode1 & ~(mask_Rd_5);
	switch (decode) {
		case 0x9405: return avr_op_index_ASR;          /* 1001 010d dddd 0101 | ASR */
		case 0x9400: return avr_op_index_COM;          /* 1001 010d dddd 0000 | COM */
		case 0x940A: return avr_op_index_DEC;          /* 1001 010d dddd 1010 | DEC */
		case 0x9006: return avr_op_index_ELPM_Z;       /* 1001 000d dddd 0110 | ELPM */
		case 0x9007: return avr_op_index_ELPM_Z_incr;  /* 1001 000d dddd 0111 | ELPM */
		case 0x9403: return avr_op_index_INC;          /* 1001 010d dddd 0011 | INC */
		case 0x9000:                             /* 1001 000d dddd 0000 | LDS */
			op->oper2 = opcode2; return avr_op_index_LDS;
		case 0x900C: return avr_op_index_LD_X;         /* 1001 000d dddd 1100 | LD */
		case 0x900E: return avr_op_index_LD_X_decr;    /* 1001 000d dddd 1110 | LD */
		case 0x900D: return avr_op_index_LD_X_incr;    /* 1001 000d dddd 1101 | LD */
		case 0x900A: return avr_op_index_LD_Y_decr;    /* 1001 000d dddd 1010 | LD */
		case 0x9009: return avr_op_index_LD_Y_incr;    /* 1001 000d dddd 1001 | LD */
		case 0x9002: return avr_op_index_LD_Z_decr;    /* 1001 000d dddd 0010 | LD */
		case 0x9001: return avr_op_index_LD_Z_incr;    /* 1001 000d dddd 0001 | LD */
		case 0x9004: return avr_op_index_LPM_Z;        /* 1001 000d dddd 0100 | LPM */
		case 0x9005: return avr_op_index_LPM_Z_incr;   /* 1001 000d dddd 0101 | LPM */
		case 0x9406: return avr_op_index_LSR;          /* 1001 010d dddd 0110 | LSR */
		case 0x9401: return avr_op_index_NEG;          /* 1001 010d dddd 0001 | NEG */
		case 0x900F: return avr_op_index_POP;          /* 1001 000d dddd 1111 | POP */
		case 0x920F: return avr_op_index_PUSH;         /* 1001 001d dddd 1111 | PUSH */
		case 0x9407: return avr_op_index_ROR;          /* 1001 010d dddd 0111 | ROR */
		case 0x9200:                             /* 1001 001d dddd 0000 | STS */
			op->oper2 = opcode2; return avr_op_index_STS;
		case 0x920C: return avr_op_index_ST_X;         /* 1001 001d dddd 1100 | ST */
		case 0x920E: return avr_op_index_ST_X_decr;    /* 1001 001d dddd 1110 | ST */
		case 0x920D: return avr_op_index_ST_X_incr;    /* 1001 001d dddd 1101 | ST */
		case 0x920A: return avr_op_index_ST_Y_decr;    /* 1001 001d dddd 1010 | ST */
		case 0x9209: return avr_op_index_ST_Y_incr;    /* 1001 001d dddd 1001 | ST */
		case 0x9202: return avr_op_index_ST_Z_decr;    /* 1001 001d dddd 0010 | ST */
		case 0x9201: return avr_op_index_ST_Z_incr;    /* 1001 001d dddd 0001 | ST */
		case 0x9402: return avr_op_index_SWAP;         /* 1001 010d dddd 0010 | SWAP */
	}

	/* opcodes with a register (Rd) and a constant data (K) as operands */
	op->oper1 = ((opcode1 >> 4) & 0xF) | 0x10;
	op->oper2 = (opcode1 & 0x0F) | ((opcode1 >> 4) & 0x00F0);

	decode = opcode1 & ~(mask_Rd_4 | mask_K_8);
	switch ( decode ) {
		case 0x7000: return avr_op_index_ANDI;       /* 0111 KKKK dddd KKKK | CBR or ANDI */
		case 0x3000: return avr_op_index_CPI;        /* 0011 KKKK dddd KKKK | CPI */
		case 0xE000: return avr_op_index_LDI;        /* 1110 KKKK dddd KKKK | LDI or SER */
		case 0x6000: return avr_op_index_ORI;        /* 0110 KKKK dddd KKKK | SBR or ORI */
		case 0x4000: return avr_op_index_SBCI;       /* 0100 KKKK dddd KKKK | SBCI */
		case 0x5000: return avr_op_index_SUBI;       /* 0101 KKKK dddd KKKK | SUBI */
	}

	/* opcodes with a register (Rd) and a register bit number (b) as operands */
	op->oper1 = ((opcode1 >> 4) & 0x1F);
	op->oper2 = 1 << (opcode1 & 0x0007);
	decode = opcode1 & ~(mask_Rd_5 | mask_reg_bit);
	switch ( decode ) {
		case 0xF800: return avr_op_index_BLD;         /* 1111 100d dddd 0bbb | BLD */
		case 0xFA00: return avr_op_index_BST;         /* 1111 101d dddd 0bbb | BST */
		case 0xFC00: return avr_op_index_SBRC;        /* 1111 110d dddd 0bbb | SBRC */
		case 0xFE00: return avr_op_index_SBRS;        /* 1111 111d dddd 0bbb | SBRS */
	}

	/* opcodes with a relative 7-bit address (k) and a register bit number (b) as operands */
	op->oper1 = ((opcode1 >> 3) & 0x7F);
	decode = opcode1 & ~(mask_k_7 | mask_reg_bit);
	switch ( decode ) {
		case 0xF400: return avr_op_index_BRBC;         /* 1111 01kk kkkk kbbb | BRBC */
		case 0xF000: return avr_op_index_BRBS;         /* 1111 00kk kkkk kbbb | BRBS */
	}

	/* opcodes with a 6-bit address displacement (q) and a register (Rd) as operands */
	op->oper1 = ((opcode1 >> 4) & 0x1F);
	op->oper2 = (opcode1 & 0x7) | ((opcode1 >> 7) & 0x18) | ((opcode1 >> 8) & 0x20);
	decode = opcode1 & ~(mask_Rd_5 | mask_q_displ);
	switch ( decode ) {
		case 0x8008: return avr_op_index_LDD_Y;       /* 10q0 qq0d dddd 1qqq | LDD */
		case 0x8000: return avr_op_index_LDD_Z;       /* 10q0 qq0d dddd 0qqq | LDD */
		case 0x8208: return avr_op_index_STD_Y;       /* 10q0 qq1d dddd 1qqq | STD */
		case 0x8200: return avr_op_index_STD_Z;       /* 10q0 qq1d dddd 0qqq | STD */
	}

	/* opcodes with a absolute 22-bit address (k) operand */
	op->oper1 = (opcode1 & 1) | ((opcode1 >> 3) & 0x3E);
	op->oper2 = opcode2;
	decode = opcode1 & ~(mask_k_22);
	switch ( decode ) {
		case 0x940E: return avr_op_index_CALL;        /* 1001 010k kkkk 111k | CALL */
		case 0x940C: return avr_op_index_JMP;         /* 1001 010k kkkk 110k | JMP */
	}

	/* opcode1 with a sreg bit select (s) operand */
	op->oper1 = 1 << ((opcode1 >> 4) & 0x07);
	decode = opcode1 & ~(mask_sreg_bit);
	switch ( decode ) {
		/* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
		/* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
		case 0x9488: return avr_op_index_BCLR;        /* 1001 0100 1sss 1000 | BCLR */
		case 0x9408: return avr_op_index_BSET;        /* 1001 0100 0sss 1000 | BSET */
	}

	/* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
	op->oper1 = ((opcode1 >> 3) & 0x06) + 24;
	op->oper2 = ((opcode1 & 0xF) | ((opcode1 >> 2) & 0x30));
	decode = opcode1 & ~(mask_K_6 | mask_Rd_2);
	switch ( decode ) {
		case 0x9600: return avr_op_index_ADIW;        /* 1001 0110 KKdd KKKK | ADIW */
		case 0x9700: return avr_op_index_SBIW;        /* 1001 0111 KKdd KKKK | SBIW */
	}

	/* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
	op->oper1 = ((opcode1 >> 3) & 0x1F) + IOBASE;
	op->oper2 = 1 << (opcode1 & 0x0007);
	decode = opcode1 & ~(mask_A_5 | mask_reg_bit);
	switch ( decode ) {
		case 0x9800: return avr_op_index_CBI;         /* 1001 1000 AAAA Abbb | CBI */
		case 0x9A00: return avr_op_index_SBI;         /* 1001 1010 AAAA Abbb | SBI */
		case 0x9900: return avr_op_index_SBIC;        /* 1001 1001 AAAA Abbb | SBIC */
		case 0x9B00: return avr_op_index_SBIS;        /* 1001 1011 AAAA Abbb | SBIS */
	}

	/* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
	op->oper1 = ((opcode1 >> 4) & 0x1F);
	op->oper2 = ((opcode1 & 0x0F) | ((opcode1 >> 5) & 0x30)) + IOBASE;
	decode = opcode1 & ~(mask_A_6 | mask_Rd_5);
	switch ( decode ) {
		case 0xB000: return avr_op_index_IN;          /* 1011 0AAd dddd AAAA | IN */
		case 0xB800: return avr_op_index_OUT;         /* 1011 1AAd dddd AAAA | OUT */
	}

	/* opcodes with a relative 12-bit address (k) operand */
	op->oper2 = opcode1 & 0xFFF;
	decode = opcode1 & ~(mask_k_12);
	switch ( decode ) {
		case 0xD000: return avr_op_index_RCALL;       /* 1101 kkkk kkkk kkkk | RCALL */
		case 0xC000: return avr_op_index_RJMP;        /* 1100 kkkk kkkk kkkk | RJMP */
	}

	/* opcodes with two 4-bit register (Rd and Rr) operands */
	decode = opcode1 & ~(mask_Rd_4 | mask_Rr_4);
	switch ( decode ) {
		case 0x0100:
			op->oper1 = ((opcode1 >> 4) & 0x0F) << 1;
			op->oper2 = (opcode1 & 0x0F) << 1;
			return avr_op_index_MOVW;        /* 0000 0001 dddd rrrr | MOVW */
		case 0x0200:
			op->oper1 = ((opcode1 >> 4) & 0x0F) | 0x10;
			op->oper2 = (opcode1 & 0x0F) | 0x10;
			return avr_op_index_MULS;        /* 0000 0010 dddd rrrr | MULS */
	}

	/* opcodes with two 3-bit register (Rd and Rr) operands */
	op->oper1 = ((opcode1 >> 4) & 0x07) | 0x10;
	op->oper2 = (opcode1 & 0x07) | 0x10;
	decode = opcode1 & ~(mask_Rd_3 | mask_Rr_3);
	switch ( decode ) {
		case 0x0300: return avr_op_index_MULSU;       /* 0000 0011 0ddd 0rrr | MULSU */
		case 0x0308: return avr_op_index_FMUL;        /* 0000 0011 0ddd 1rrr | FMUL */
		case 0x0380: return avr_op_index_FMULS;       /* 0000 0011 1ddd 0rrr | FMULS */
		case 0x0388: return avr_op_index_FMULSU;      /* 0000 0011 1ddd 1rrr | FMULSU */
	}

	op->oper2 = opcode1;
	return avr_op_index_ILLEGAL;
}

static void decode_flash(void)
{
	word opcode1, opcode2;
	int i;

	for (i = 0; i < program_size; i += 2) {
		opcode1 = cpu_flash[i] | (cpu_flash[i + 1] << 8);
		opcode2 = cpu_flash[i + 2] | (cpu_flash[i + 3] << 8);
		decoded_flash[i / 2].data_index = decode_opcode(&decoded_flash[i / 2], opcode1, opcode2);
	}
}

// ---------------------------------------------------------------------------------
//     main execution loop

static void do_step(void)
{
	decoded_op dop;
	opcode_data *data;

	// fetch decoded instruction
	dop = decoded_flash[cpu_PC];
	if (!dop.data_index)
		leave(EXIT_STATUS_ABORTED, "program counter out of program space");

	// execute instruction
	data = &opcode_func_array[dop.data_index];
	log_add_instr(data->hreadable);
	cpu_PC += data->size;
	add_program_cycles(data->cycles);
	data->func(dop.oper1, dop.oper2);
	log_dump_line();
}

static void execute(void)
{
	dword count;

	program_cycles = 0;
	cpu_PC = 0;

	if (!max_instr_count) {
		for (;;)
			do_step();
	} else {
		for (count = 0; count < max_instr_count; count++)
			do_step();
		leave(EXIT_STATUS_TIMEOUT, "instruction count limit reached");
	}
}



// main: as simple as it gets
int main(int argc, char *argv[])
{
	if (parse_args(argc, argv))
		return 1;

	init_flag_update_tables();

	decode_flash();
	execute();

	return 0;
}
