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
// some helpful types

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned long long ddword;

// ---------------------------------------------------------------------------------
// vars that hold core definitions

// configured flash size
static int flash_size;
static int PC_is_22_bits;

// maximum number of execution cycles (used as a timeout)
static ddword program_cycles_limit;

// filename of the file being executed
static char program_name[256];
static int program_size;

// ---------------------------------------------------------------------------------
// vars that hold simulator state. These are kept as global vars for simplicity

// cycle counter (64 bits)
static ddword program_cycles;

// cpu_data is used to store registers, ioport values and actual SRAM
static byte cpu_data[MAX_RAM_SIZE];
static int cpu_PC;

// flash

static byte cpu_flash[MAX_FLASH_SIZE];

// ---------------------------------------------------------------------------------
//     global vars that hold temporary values (to keep code simple and lean)

static int wants_to_exit, exit_code;
static int global_Rr, global_Rd, global_bitmask;


// ---------------------------------------------------------------------------------
//     log functions

#define LOG_SIZE	128

static char log_data[LOG_SIZE][256];
int cur_log_pos;

void log_add_data(char *data)
{
	strcat(log_data[cur_log_pos], data);
}

void log_dump_line(void)
{
	// right now, dump everything (this is to be replaced
	// by a proper crash dump with just LOG_SIZE lines)
	printf("%s\n", log_data[cur_log_pos]);

	cur_log_pos++;
	if (cur_log_pos >= LOG_SIZE)
		cur_log_pos = 0;
	log_data[cur_log_pos][0] = '\0';
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
		case ABORT_PORT:
			printf("%s %s at address %04x\n", (address == ABORT_PORT || value) ? "ABORTED" : "EXIT", program_name, cpu_PC);
			exit(0);
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

#ifdef LOG_DUMP
static void get_address_name(int addr, char *result)
{
	if (addr < 32) {
		sprintf(result, "R%d", addr);
		return;
	}

	switch (addr) {
		case SREG: strcpy(result, "SREG"); return;
		case SPH: strcpy(result, "SPH"); return;
		case SPL: strcpy(result, "SPL"); return;
	}

	if (addr < 256)
		sprintf(result, "%02x", addr);
	else
		sprintf(result, "%04x", addr);
}
#endif

// read a byte from memory / ioport / register

static int data_read_byte(int address)
{
	int ret;

	ret = data_read_byte_raw(address);
#ifdef LOG_DUMP
	char buf[32], adname[16];
	get_address_name(address, adname);
	sprintf(buf, "(%s)->%02x ", adname, ret);
	log_add_data(buf);
#endif
	return ret;
}

// write a byte to memory / ioport / register

static void data_write_byte(int address, int value)
{
#ifdef LOG_DUMP
	char buf[32], adname[16];
	get_address_name(address, adname);
	sprintf(buf, "(%s)<-%02x ", adname, value);
	log_add_data(buf);
#endif
	data_write_byte_raw(address, value);
}

// get_reg / put_reg are just placeholders for read/write calls where we can
// be sure that the adress is < 32

static byte get_reg(int address)
{
	return data_read_byte(address);
}

static void put_reg(int address, byte value)
{
	data_write_byte(address, value);
}

// read a word from memory / ioport / register

static int data_read_word(int address)
{
	int ret;

	ret = data_read_byte_raw(address) | (data_read_byte_raw(address + 1) << 8);
#ifdef LOG_DUMP
	char buf[32], adname[16];
	get_address_name(address, adname);
	sprintf(buf, "(%s)->%04x ", adname, ret);
	log_add_data(buf);
#endif
	return ret;
}

// write a word to memory / ioport / register

static void data_write_word(int address, int value)
{
#ifdef LOG_DUMP
	char buf[32], adname[16];
	get_address_name(address, adname);
	sprintf(buf, "(%s)<-%04x ", adname, value);
	log_add_data(buf);
#endif
	data_write_byte_raw(address, value & 0xFF);
	data_write_byte_raw(address + 1, (value >> 8) & 0xFF);
}

static int flash_read_word(int address)
{
	return flash_read_byte(address) | (flash_read_byte(address + 1) << 8);
}

// ---------------------------------------------------------------------------------
//     helper functions

static void add_program_cycles(int count)
{
	program_cycles += count;
	if (program_cycles >= program_cycles_limit) {
		printf("TIMEOUT executing %s\n", program_name);
		exit(0);
	}
}

static void push_byte(int value)
{
	int sp = data_read_word(SPL);
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

static void set_flags(int flags)
{
	data_write_byte(SREG, data_read_byte(SREG) | flags);
}

static void clear_flags(int flags)
{
	data_write_byte(SREG, data_read_byte(SREG) & (~flags));
}

static void update_flags(int flags, int new_values)
{
	int sreg = data_read_byte(SREG);
	sreg &= ~flags;
	sreg |= new_values;
	data_write_byte(SREG, sreg);
}


static int get_carry(void)
{
	return (data_read_byte(SREG) & FLAG_C) != 0;
}

// set the appropriate flags after a basic arithmetic operation (add or sub)
static void set_arith_8_flags(int sreg, int value1, int value2, int result)
{
	if (result & 0x80)
		sreg |= FLAG_N;
	if (result & 0xFF00)
		sreg |= FLAG_C;
	if (((value1 & value2) | (~result & value1) | (~result & value2)) & 0x08)
		sreg |= FLAG_H;
	if (((sreg & FLAG_N) != 0) ^ ((sreg & FLAG_V) != 0))
		sreg |= FLAG_S;
	update_flags(FLAG_H | FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
}

// perform the addition and set the appropriate flags
static void do_addition_8(int carry)
{
	int value1, value2, sreg, result;

	value1 = get_reg(global_Rd);
	value2 = get_reg(global_Rr);

	result = value1 + value2 + carry;

	if (((value1 & 0x80) == (value2 & 0x80)) && ((result & 0x80) != (value1 & 0x80)))
		sreg = FLAG_V;
	else
		sreg = 0;
	if ((result & 0xFF) == 0x00)
		sreg |= FLAG_Z;
	set_arith_8_flags(sreg, value1, value2, result);

	add_program_cycles(1);

	put_reg(global_Rd, result & 0xFF);
}

// perform the subtraction and set the appropriate flags
static int do_subtraction_8(int value1, int value2, int carry, int use_carry)
{
	int sreg, result = value1 - value2 - carry;

	if (((result & 0x80) == (value2 & 0x80)) && ((value1 & 0x80) != (value2 & 0x80)))
		sreg = FLAG_V;
	else
		sreg = 0;
	if ((result & 0xFF) == 0x00) {
		if ((use_carry && (data_read_byte(SREG) & FLAG_Z)) || !use_carry)
			sreg |= FLAG_Z;
	}
	set_arith_8_flags(sreg, value1, value2, result);

	return result & 0xFF;
}

static void store_logical_result(int result)
{
	int sreg;

	result &= 0xFF;
	put_reg(global_Rd, result);

	sreg = 0;
	if (result == 0)
		sreg |= FLAG_Z;
	if (result & 0x80)
		sreg |= FLAG_N | FLAG_S;
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z, sreg);

	add_program_cycles(1);
}

// only LDS, STS, CALL and JMP use 2 words
static int opcode_size(word opcode)
{
	int decode;
	decode = opcode & ~(mask_Rd_5);
	// LDS, STS
	if (decode == 0x9000 || decode == 0x9200)
		return 2;
	decode = opcode & ~(mask_k_22);
	// CALL, JMP
	if (decode == 0x940E || decode == 0x940C)
		return 2;
	return 1;
}

/* 10q0 qq0d dddd 1qqq | LDD */

static void load_indirect(int ind_register, int adjust, int displacement_bits)
{
	//TODO: use RAMPx registers to address more than 64kb of RAM
	int ind_value = data_read_word(ind_register);
	displacement_bits = (displacement_bits & 0x7) | ((displacement_bits >> 7) & 0x18) |
			    ((displacement_bits >> 8) & 0x20);
	if (adjust < 0)
		ind_value += adjust;
	put_reg(global_Rd, data_read_byte(ind_value + displacement_bits));
	if (adjust > 0)
		ind_value += adjust;
	if (adjust)
		data_write_word(ind_register, ind_value);
	add_program_cycles(2);
}

static void store_indirect(int ind_register, int adjust, int displacement_bits)
{
	//TODO: use RAMPx registers to address more than 64kb of RAM
	int ind_value = data_read_word(ind_register);
	displacement_bits = (displacement_bits & 0x7) | ((displacement_bits >> 7) & 0x18) |
			    ((displacement_bits >> 8) & 0x20);
	if (adjust < 0)
		ind_value += adjust;
	data_write_byte(ind_value + displacement_bits, get_reg(global_Rd));
	if (adjust > 0)
		ind_value += adjust;
	if (adjust)
		data_write_word(ind_register, ind_value);
	add_program_cycles(2);
}

static void load_program_memory(int use_RAMPZ, int incr)
{
	int address = data_read_word(REGZ);
	if (use_RAMPZ)
		address |= data_read_byte(RAMPZ) << 16;
	put_reg(global_Rd, flash_read_byte(address));
	if (incr) {
		address++;
		data_write_word(REGZ, address & 0xFFFF);
		if (use_RAMPZ)
			data_write_byte(RAMPZ, address >> 16);
	}
	add_program_cycles(3);
}

static void skip_instruction_on_condition(int condition)
{
	int size;
	if (condition) {
		size = opcode_size(flash_read_word(cpu_PC * 2));
		cpu_PC += size;
		add_program_cycles(1 + size);
	} else
		add_program_cycles(1);
}

static void branch_on_sreg_condition(word opcode, int flag_value)
{
	if (((data_read_byte(SREG) & global_bitmask) != 0) == flag_value) {
		int delta = (opcode >> 3) & 0x7F;
		if (delta & 0x40) delta |= 0xFFFFFF80;
		cpu_PC += delta;
		add_program_cycles(2);
	} else {
		add_program_cycles(1);
	}
}

static void rotate_right(int value, int top_bit)
{
	int sreg, result;

	sreg = 0;
	if (value & 1)
		sreg |= FLAG_C;
	result = value >> 1;
	if (top_bit)
		result |= 0x80;
	put_reg(global_Rd, result);

	if (result == 0)
		sreg |= FLAG_Z;
	if (result & 0x80)
		sreg |= FLAG_N;
	if (((sreg & FLAG_N) != 0) ^ ((sreg & FLAG_V) != 0))
		sreg |= FLAG_S;
	if (((sreg & FLAG_N) != 0) ^ ((sreg & FLAG_C) != 0))
		sreg |= FLAG_V;
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
	add_program_cycles(1);
}

static void do_multiply(int signed1, int signed2, int shift)
{
	int v1, v2, result, sreg;

	v1 = signed1 ? ((signed char) get_reg(global_Rd)) : get_reg(global_Rd);
	v2 = signed2 ? ((signed char) get_reg(global_Rr)) : get_reg(global_Rr);
	result = (v1 * v2) & 0xFFFF;

	sreg = 0;
	if (result & 0x8000)
		sreg |= FLAG_C;
	if (shift)
		result <<= 1;
	if (result == 0)
		sreg |= FLAG_Z;
	update_flags(FLAG_Z | FLAG_C, sreg);
	data_write_word(0, result);
	add_program_cycles(2);
}

// handle illegal instructions
void avr_op_ILLEGAL(word opcode)
{
	printf("ABORTED: Illegal opcode %04x at address %04x\n", opcode, cpu_PC - 1);
	exit(0);
}

// ---------------------------------------------------------------------------------
//     opcode execution functions

/* opcodes with no operands */
/* 1001 0101 0001 1001 | EICALL */
static void avr_op_EICALL(word opcode)
{
	avr_op_ILLEGAL(opcode);
	//TODO
}

/* 1001 0100 0001 1001 | EIJMP */
static void avr_op_EIJMP(word opcode)
{
	avr_op_ILLEGAL(opcode);
	//TODO
}

/* 1001 0101 1101 1000 | ELPM */
static void avr_op_ELPM(void)
{
	global_Rd = 0;
	load_program_memory(1, 0);
}

/* 1001 0101 1111 1000 | ESPM */
static void avr_op_ESPM(word opcode)
{
	avr_op_ILLEGAL(opcode);
	//TODO
}

/* 1001 0101 0000 1001 | ICALL */
static void avr_op_ICALL(void)
{
	push_PC();
	cpu_PC = data_read_word(REGZ);
	add_program_cycles(PC_is_22_bits ? 4 : 3);
}

/* 1001 0100 0000 1001 | IJMP */
static void avr_op_IJMP(void)
{
	cpu_PC = data_read_word(REGZ);
	add_program_cycles(2);
}

/* 1001 0101 1100 1000 | LPM */
static void avr_op_LPM(void)
{
	global_Rd = 0;
	load_program_memory(0, 0);
}

/* 0000 0000 0000 0000 | NOP */
static void avr_op_NOP(void)
{
	add_program_cycles(1);
}

/* 1001 0101 0000 1000 | RET */
static void avr_op_RET(void)
{
	pop_PC();
	add_program_cycles(PC_is_22_bits ? 5 : 4);
}

/* 1001 0101 0001 1000 | RETI */
static void avr_op_RETI(void)
{
	avr_op_RET();
	set_flags(FLAG_I);
}

/* 1001 0101 1000 1000 | SLEEP */
static void avr_op_SLEEP(void)
{
	// we don't have anything to wake us up, so just pretend we wake up immediately
	add_program_cycles(1);
}

/* 1001 0101 1110 1000 | SPM */
static void avr_op_SPM(word opcode)
{
	avr_op_ILLEGAL(opcode);
	//TODO
}

/* 1001 0101 1010 1000 | WDR */
static void avr_op_WDR(void)
{
	// we don't have a watchdog, so do nothing
	add_program_cycles(1);
}


/* opcodes with two 5-bit register (Rd and Rr) operands */
/* 0001 11rd dddd rrrr | ADC or ROL */
static void avr_op_ADC(void)
{
	do_addition_8(get_carry());
}

/* 0000 11rd dddd rrrr | ADD or LSL */
static void avr_op_ADD(void)
{
	do_addition_8(0);
}

/* 0010 00rd dddd rrrr | AND or TST */
static void avr_op_AND(void)
{
	int result = get_reg(global_Rd) & get_reg(global_Rr);
	store_logical_result(result);
}

/* 0001 01rd dddd rrrr | CP */
static void avr_op_CP(void)
{
	do_subtraction_8(get_reg(global_Rd), get_reg(global_Rr), 0, 0);
	add_program_cycles(1);
}

/* 0000 01rd dddd rrrr | CPC */
static void avr_op_CPC(void)
{
	do_subtraction_8(get_reg(global_Rd), get_reg(global_Rr), get_carry(), 1);
	add_program_cycles(1);
}

/* 0001 00rd dddd rrrr | CPSE */
static void avr_op_CPSE(void)
{
	skip_instruction_on_condition(get_reg(global_Rd) ==  get_reg(global_Rr));
}

/* 0010 01rd dddd rrrr | EOR or CLR */
static void avr_op_EOR(void)
{
	int result = get_reg(global_Rd) ^ get_reg(global_Rr);
	store_logical_result(result);
}

/* 0010 11rd dddd rrrr | MOV */
static void avr_op_MOV(void)
{
	put_reg(global_Rd, get_reg(global_Rr));
	add_program_cycles(1);
}

/* 1001 11rd dddd rrrr | MUL */
static void avr_op_MUL(void)
{
	do_multiply(0,0,0);
}

/* 0010 10rd dddd rrrr | OR */
static void avr_op_OR(void)
{
	int result = get_reg(global_Rd) | get_reg(global_Rr);
	store_logical_result(result);
}

/* 0000 10rd dddd rrrr | SBC */
static void avr_op_SBC(void)
{
	put_reg(global_Rd, do_subtraction_8(get_reg(global_Rd), get_reg(global_Rr), get_carry(), 1));
	add_program_cycles(1);
}

/* 0001 10rd dddd rrrr | SUB */
static void avr_op_SUB(void)
{
	put_reg(global_Rd, do_subtraction_8(get_reg(global_Rd), get_reg(global_Rr), 0, 0));
	add_program_cycles(1);
}


/* opcode with a single register (Rd) as operand */
/* 1001 010d dddd 0101 | ASR */
static void avr_op_ASR(void)
{
	int value = get_reg(global_Rd);
	rotate_right(value, value & 0x80);
}

/* 1001 010d dddd 0000 | COM */
static void avr_op_COM(void)
{
	int sreg, result;

	result = ~get_reg(global_Rd);
	put_reg(global_Rd, result);

	sreg = FLAG_C;
	if ((result & 0xFF) == 0)
		sreg |= FLAG_Z;
	if (result & 0x80)
		sreg |= FLAG_N | FLAG_S;
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);
	add_program_cycles(1);
}

/* 1001 010d dddd 1010 | DEC */
static void avr_op_INC_DEC(int inc)
{
	int sreg, value, result;

	value = get_reg(global_Rd);
	result = (value + inc) & 0xFF;
	put_reg(global_Rd, result);

	sreg = 0;
	if (result == 0)
		sreg |= FLAG_Z;
	if (result & 0x80)
		sreg |= FLAG_N;
	if ((result & 0x80) ^ (value & 0x80))
		sreg |= FLAG_V;
	if (((sreg & FLAG_N) != 0) ^ ((sreg & FLAG_V) != 0))
		sreg |= FLAG_S;
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z, sreg);
	add_program_cycles(1);
}

static void avr_op_DEC(void)
{
	avr_op_INC_DEC(-1);
}

/* 1001 000d dddd 0110 | ELPM */
static void avr_op_ELPM_Z(void)
{
	load_program_memory(1, 0);
}

/* 1001 000d dddd 0111 | ELPM */
static void avr_op_ELPM_Z_incr(void)
{
	load_program_memory(1, 1);
}

/* 1001 010d dddd 0011 | INC */
static void avr_op_INC(void)
{
	avr_op_INC_DEC(1);
}

/* 1001 000d dddd 0000 | LDS */
static void avr_op_LDS(word opcode2)
{
	//TODO:RAMPD
	put_reg(global_Rd, data_read_byte(opcode2));
	add_program_cycles(2);
}

/* 1001 000d dddd 1100 | LD */
static void avr_op_LD_X(void)
{
	load_indirect(REGX, 0, 0);
}

/* 1001 000d dddd 1110 | LD */
static void avr_op_LD_X_decr(void)
{
	load_indirect(REGX, -1, 0);
}

/* 1001 000d dddd 1101 | LD */
static void avr_op_LD_X_incr(void)
{
	load_indirect(REGX, 1, 0);
}

/* 1001 000d dddd 1010 | LD */
static void avr_op_LD_Y_decr(void)
{
	load_indirect(REGY, -1, 0);
}

/* 1001 000d dddd 1001 | LD */
static void avr_op_LD_Y_incr(void)
{
	load_indirect(REGY, 1, 0);
}

/* 1001 000d dddd 0010 | LD */
static void avr_op_LD_Z_decr(void)
{
	load_indirect(REGZ, -1, 0);
}

/* 1001 000d dddd 0010 | LD */
static void avr_op_LD_Z_incr(void)
{
	load_indirect(REGZ, 1, 0);
}

/* 1001 000d dddd 0100 | LPM Z */
static void avr_op_LPM_Z(void)
{
	load_program_memory(0, 0);
}

/* 1001 000d dddd 0101 | LPM Z+ */
static void avr_op_LPM_Z_incr(void)
{
	load_program_memory(0, 1);
}

/* 1001 010d dddd 0110 | LSR */
static void avr_op_LSR(void)
{
	rotate_right(get_reg(global_Rd), 0);
}

/* 1001 010d dddd 0001 | NEG */
static void avr_op_NEG(void)
{
	put_reg(global_Rd, do_subtraction_8(0, get_reg(global_Rd), 0, 0));
	add_program_cycles(1);
}

/* 1001 000d dddd 1111 | POP */
static void avr_op_POP(void)
{
	put_reg(global_Rd, pop_byte());
	add_program_cycles(2);
}

/* 1001 001d dddd 1111 | PUSH */
static void avr_op_PUSH(void)
{
	push_byte(get_reg(global_Rd));
	add_program_cycles(2);
}

/* 1001 010d dddd 0111 | ROR */
static void avr_op_ROR(void)
{
	rotate_right(get_reg(global_Rd), get_carry());
}

/* 1001 001d dddd 0000 | STS */
static void avr_op_STS(word opcode2)
{
	//TODO:RAMPD
	data_write_byte(opcode2, get_reg(global_Rd));
	add_program_cycles(2);
}

/* 1001 001d dddd 1100 | ST */
static void avr_op_ST_X(void)
{
	store_indirect(REGX, 0, 0);
}

/* 1001 001d dddd 1110 | ST */
static void avr_op_ST_X_decr(void)
{
	store_indirect(REGX, -1, 0);
}

/* 1001 001d dddd 1101 | ST */
static void avr_op_ST_X_incr(void)
{
	store_indirect(REGX, 1, 0);
}

/* 1001 001d dddd 1010 | ST */
static void avr_op_ST_Y_decr(void)
{
	store_indirect(REGY, -1, 0);
}

/* 1001 001d dddd 1001 | ST */
static void avr_op_ST_Y_incr(void)
{
	store_indirect(REGY, 1, 0);
}

/* 1001 001d dddd 0010 | ST */
static void avr_op_ST_Z_decr(void)
{
	store_indirect(REGZ, -1, 0);
}

/* 1001 001d dddd 0001 | ST */
static void avr_op_ST_Z_incr(void)
{
	store_indirect(REGZ, 1, 0);
}

/* 1001 010d dddd 0010 | SWAP */
static void avr_op_SWAP(void)
{
	int value = get_reg(global_Rd);
	put_reg(global_Rd, ((value << 4) & 0xF0) | ((value >> 4) & 0x0F));
	add_program_cycles(1);
}

/* opcodes with a register (Rd) and a constant data (K) as operands */
/* 0111 KKKK dddd KKKK | CBR or ANDI */
static void avr_op_ANDI(word opcode)
{
	int imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
	global_Rd |= 0x10;
	int result = get_reg(global_Rd) & imm;
	store_logical_result(result);
}

/* 0011 KKKK dddd KKKK | CPI */
static void avr_op_CPI(word opcode)
{
	int imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
	do_subtraction_8(get_reg(global_Rd | 0x10), imm, 0, 0);
	add_program_cycles(1);
}

/* 1110 KKKK dddd KKKK | LDI or SER */
static void avr_op_LDI(word opcode)
{
	int imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
	put_reg(global_Rd | 0x10, imm);
	add_program_cycles(1);
}

/* 0110 KKKK dddd KKKK | SBR or ORI */
static void avr_op_ORI(word opcode)
{
	int imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
	global_Rd |= 0x10;
	int result = get_reg(global_Rd) | imm;
	store_logical_result(result);
}

/* 0100 KKKK dddd KKKK | SBCI */
/* 0101 KKKK dddd KKKK | SUBI */
static void avr_op_SBCI_SUBI(word opcode, int carry, int use_carry)
{
	int imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
	global_Rd |= 0x10;
	put_reg(global_Rd, do_subtraction_8(get_reg(global_Rd), imm, carry, use_carry));
	add_program_cycles(1);
}

static void avr_op_SBCI(word opcode)
{
	avr_op_SBCI_SUBI(opcode, get_carry(), 1);
}

static void avr_op_SUBI(word opcode)
{
	avr_op_SBCI_SUBI(opcode, 0, 0);
}


/* opcodes with a register (Rd) and a register bit number (b) as operands */
/* 1111 100d dddd 0bbb | BLD */
static void avr_op_BLD(void)
{
	int value = get_reg(global_Rd);
	if (data_read_byte(SREG) & FLAG_T)
		value |= global_bitmask;
	else
		value &= ~global_bitmask;
	put_reg(global_Rd, value);
	add_program_cycles(1);
}

/* 1111 101d dddd 0bbb | BST */
static void avr_op_BST(void)
{
	int bit = get_reg(global_Rd) & global_bitmask;
	update_flags(FLAG_T, bit ? FLAG_T : 0);
	add_program_cycles(1);
}

/* 1111 110d dddd 0bbb | SBRC */
static void avr_op_SBRC(void)
{
	skip_instruction_on_condition(!(get_reg(global_Rd) & global_bitmask));
}

/* 1111 111d dddd 0bbb | SBRS */
static void avr_op_SBRS(void)
{
	skip_instruction_on_condition(get_reg(global_Rd) & global_bitmask);
}

/* opcodes with a relative 7-bit address (k) and a register bit number (b) as operands */
/* 1111 01kk kkkk kbbb | BRBC */
static void avr_op_BRBC(word opcode)
{
	branch_on_sreg_condition(opcode, 0);
}

/* 1111 00kk kkkk kbbb | BRBS */
static void avr_op_BRBS(word opcode)
{
	branch_on_sreg_condition(opcode, 1);
}


/* opcodes with a 6-bit address displacement (q) and a register (Rd) as operands */
/* 10q0 qq0d dddd 1qqq | LDD */
static void avr_op_LDD_Y(word opcode)
{
	load_indirect(REGY, 0, opcode);
}

/* 10q0 qq0d dddd 0qqq | LDD */
static void avr_op_LDD_Z(word opcode)
{
	load_indirect(REGZ, 0, opcode);
}

/* 10q0 qq1d dddd 1qqq | STD */
static void avr_op_STD_Y(word opcode)
{
	store_indirect(REGY, 0, opcode);
}

/* 10q0 qq1d dddd 0qqq | STD */
static void avr_op_STD_Z(word opcode)
{
	store_indirect(REGZ, 0, opcode);
}


/* opcodes with a absolute 22-bit address (k) operand */
/* 1001 010k kkkk 110k | JMP */
static void avr_op_JMP(word opcode1, word opcode2)
{
	cpu_PC = (((opcode1 & 0x1) | ((opcode1 >> 3) & 0x3E)) << 16) | opcode2;
	add_program_cycles(3);
}

/* 1001 010k kkkk 111k | CALL */
static void avr_op_CALL(word opcode1, word opcode2)
{
	push_PC();
	add_program_cycles(PC_is_22_bits ? 2 : 1);  // the 3 missing cycles will be added by the JMP
	avr_op_JMP(opcode1, opcode2);
}


/* opcode with a sreg bit select (s) operand */
/* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
/* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
/* 1001 0100 1sss 1000 | BCLR */
static void avr_op_BCLR(word opcode)
{
	clear_flags(1 << ((opcode & 0x70) >> 4));
	add_program_cycles(1);
}

/* 1001 0100 0sss 1000 | BSET */
static void avr_op_BSET(word opcode)
{
	set_flags(1 << ((opcode & 0x70) >> 4));
	add_program_cycles(1);
}


/* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
/* 1001 0110 KKdd KKKK | ADIW */
/* 1001 0111 KKdd KKKK | SBIW */
static void avr_op_ADIW_SBIW(word opcode)
{
	int reg, delta, svalue, evalue, sreg;

	reg = ((opcode >> 3) & 0x06) + 24;
	delta = ((opcode & 0xF) | ((opcode >> 2) & 0x30));

	if (opcode & 0x0100)
		delta = -delta;

	svalue = data_read_word(reg);
	evalue = svalue + delta;
	data_write_word(reg, evalue);

	sreg = 0;
	if (evalue & 0x8000)
		sreg |= FLAG_N;
	if ((evalue & 0xFFFF) == 0x0000)
		sreg |= FLAG_Z;
	if (delta < 0) {
		if ((svalue & 0x8000) && !(evalue & 0x8000))
			sreg |= FLAG_V;
	} else {
		if (!(svalue & 0x8000) && (evalue & 0x8000))
			sreg |= FLAG_V;
	}
	if (evalue & 0xFFFF0000)
		sreg |= FLAG_C;
	if (((sreg & FLAG_N) != 0) ^ ((sreg & FLAG_V) != 0))
		sreg |= FLAG_S;
	update_flags(FLAG_S | FLAG_V | FLAG_N | FLAG_Z | FLAG_C, sreg);

	add_program_cycles(2);
}


/* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
/* 1001 1000 AAAA Abbb | CBI */
static void avr_op_CBI(word opcode)
{
	int ioport = ((opcode >> 3) & 0x1F) + IOBASE;
	data_write_byte(ioport, data_read_byte(ioport) & ~(global_bitmask));
	add_program_cycles(2);
}

/* 1001 1010 AAAA Abbb | SBI */
static void avr_op_SBI(word opcode)
{
	int ioport = ((opcode >> 3) & 0x1F) + IOBASE;
	data_write_byte(ioport, data_read_byte(ioport) | global_bitmask);
	add_program_cycles(2);
}

/* 1001 1001 AAAA Abbb | SBIC */
static void avr_op_SBIC(word opcode)
{
	int ioport = ((opcode >> 3) & 0x1F) + IOBASE;
	skip_instruction_on_condition(!(data_read_byte(ioport) & global_bitmask));
}

/* 1001 1011 AAAA Abbb | SBIS */
static void avr_op_SBIS(word opcode)
{
	int ioport = ((opcode >> 3) & 0x1F) + IOBASE;
	skip_instruction_on_condition(data_read_byte(ioport) & global_bitmask);
}


/* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
/* 1011 0AAd dddd AAAA | IN */
static void avr_op_IN(word opcode)
{
	int ioport = ((opcode & 0x0F) | ((opcode >> 5) & 0x30)) + 0x20;
	put_reg(global_Rd, data_read_byte(ioport));
	add_program_cycles(1);
}

/* 1011 1AAd dddd AAAA | OUT */
static void avr_op_OUT(word opcode)
{
	int ioport = ((opcode & 0x0F) | ((opcode >> 5) & 0x30)) + 0x20;
	data_write_byte(ioport, get_reg(global_Rd));
	add_program_cycles(1);
}


/* opcodes with a relative 12-bit address (k) operand */
/* 1100 kkkk kkkk kkkk | RJMP */
static void avr_op_RJMP(word opcode)
{
	int delta = opcode & 0xFFF;
	if (delta & 0x800) delta |= 0xFFFFF000;

	// special case: endless loop usually means that the program has ended
	if (delta == -1) {
		printf("EXIT: normal program exit\n");
		exit(0);
	}

	cpu_PC += delta;
	add_program_cycles(2);
}

/* 1101 kkkk kkkk kkkk | RCALL */
static void avr_op_RCALL(word opcode)
{
	push_PC();
	add_program_cycles(PC_is_22_bits ? 2 : 1);
	avr_op_RJMP(opcode);
}


/* opcodes with two 4-bit register (Rd and Rr) operands */
/* 0000 0001 dddd rrrr | MOVW */
static void avr_op_MOVW(word opcode)
{
	data_write_word(((opcode >> 4) & 0x0F) << 1,
		data_read_word((opcode & 0x0F) << 1));
	add_program_cycles(1);
}

/* 0000 0010 dddd rrrr | MULS */
static void avr_op_MULS(void)
{
	global_Rd |= 0x10;
	global_Rr |= 0x10;
	do_multiply(1, 1, 0);
}

/* opcodes with two 3-bit register (Rd and Rr) operands */
/* 0000 0011 0ddd 0rrr | MULSU */
static void avr_op_MULSU(void)
{
	global_Rd = (global_Rd & 0x07) | 0x10;
	global_Rr = (global_Rr & 0x07) | 0x10;
	do_multiply(1, 0, 0);
}

/* 0000 0011 0ddd 1rrr | FMUL */
static void avr_op_FMUL(void)
{
	global_Rd = (global_Rd & 0x07) | 0x10;
	global_Rr = (global_Rr & 0x07) | 0x10;
	do_multiply(0, 0, 1);
}

/* 0000 0011 1ddd 0rrr | FMULS */
static void avr_op_FMULS(void)
{
	global_Rd = (global_Rd & 0x07) | 0x10;
	global_Rr = (global_Rr & 0x07) | 0x10;
	do_multiply(1, 1, 1);
}

/* 0000 0011 1ddd 1rrr | FMULSU */
static void avr_op_FMULSU(void)
{
	global_Rd = (global_Rd & 0x07) | 0x10;
	global_Rr = (global_Rr & 0x07) | 0x10;
	do_multiply(1, 0, 1);
}


// ---------------------------------------------------------------------------------
//     main execution loop

static void execute_opcode(word opcode1, word opcode2)
{
	int decode;

	/* opcodes with no operands */
	switch (opcode1) {
		case 0x9519: avr_op_EICALL(opcode1); return;       /* 1001 0101 0001 1001 | EICALL */
		case 0x9419: avr_op_EIJMP(opcode1); return;        /* 1001 0100 0001 1001 | EIJMP */
		case 0x95D8: avr_op_ELPM(); return;                /* 1001 0101 1101 1000 | ELPM */
		case 0x95F8: avr_op_ESPM(opcode1); return;         /* 1001 0101 1111 1000 | ESPM */
		case 0x9509: avr_op_ICALL(); return;               /* 1001 0101 0000 1001 | ICALL */
		case 0x9409: avr_op_IJMP(); return;                /* 1001 0100 0000 1001 | IJMP */
		case 0x95C8: avr_op_LPM(); return;                 /* 1001 0101 1100 1000 | LPM */
		case 0x0000: avr_op_NOP(); return;                 /* 0000 0000 0000 0000 | NOP */
		case 0x9508: avr_op_RET(); return;                 /* 1001 0101 0000 1000 | RET */
		case 0x9518: avr_op_RETI(); return;                /* 1001 0101 0001 1000 | RETI */
		case 0x9588: avr_op_SLEEP(); return;               /* 1001 0101 1000 1000 | SLEEP */
		case 0x95E8: avr_op_SPM(opcode1); return;          /* 1001 0101 1110 1000 | SPM */
		case 0x95A8: avr_op_WDR(); return;                 /* 1001 0101 1010 1000 | WDR */
	}

	// since there are a lot of opcodes that use these parameters, we
	// extract them here to simplify the opcode implementations themselves
	global_Rr = (opcode1 & 0x0F) | ((opcode1 >> 5) & 0x0010);
	global_Rd = ((opcode1 >> 4) & 0x1F);

	/* opcodes with two 5-bit register (Rd and Rr) operands */
	decode = opcode1 & ~(mask_Rd_5 | mask_Rr_5);
	switch ( decode ) {
		case 0x1C00: avr_op_ADC(); return;         /* 0001 11rd dddd rrrr | ADC or ROL */
		case 0x0C00: avr_op_ADD(); return;         /* 0000 11rd dddd rrrr | ADD or LSL */
		case 0x2000: avr_op_AND(); return;         /* 0010 00rd dddd rrrr | AND or TST */
		case 0x1400: avr_op_CP(); return;          /* 0001 01rd dddd rrrr | CP */
		case 0x0400: avr_op_CPC(); return;         /* 0000 01rd dddd rrrr | CPC */
		case 0x1000: avr_op_CPSE(); return;        /* 0001 00rd dddd rrrr | CPSE */
		case 0x2400: avr_op_EOR(); return;         /* 0010 01rd dddd rrrr | EOR or CLR */
		case 0x2C00: avr_op_MOV(); return;         /* 0010 11rd dddd rrrr | MOV */
		case 0x9C00: avr_op_MUL(); return;         /* 1001 11rd dddd rrrr | MUL */
		case 0x2800: avr_op_OR(); return;          /* 0010 10rd dddd rrrr | OR */
		case 0x0800: avr_op_SBC(); return;         /* 0000 10rd dddd rrrr | SBC */
		case 0x1800: avr_op_SUB(); return;         /* 0001 10rd dddd rrrr | SUB */
	}

	/* opcodes with a single register (Rd) as operand */
	decode = opcode1 & ~(mask_Rd_5);
	switch (decode) {
		case 0x9405: avr_op_ASR(); return;         /* 1001 010d dddd 0101 | ASR */
		case 0x9400: avr_op_COM(); return;         /* 1001 010d dddd 0000 | COM */
		case 0x940A: avr_op_DEC(); return;         /* 1001 010d dddd 1010 | DEC */
		case 0x9006: avr_op_ELPM_Z(); return;      /* 1001 000d dddd 0110 | ELPM */
		case 0x9007: avr_op_ELPM_Z_incr(); return; /* 1001 000d dddd 0111 | ELPM */
		case 0x9403: avr_op_INC(); return;         /* 1001 010d dddd 0011 | INC */
		case 0x9000: avr_op_LDS(opcode2); return;  /* 1001 000d dddd 0000 | LDS */
		case 0x900C: avr_op_LD_X(); return;        /* 1001 000d dddd 1100 | LD */
		case 0x900E: avr_op_LD_X_decr(); return;   /* 1001 000d dddd 1110 | LD */
		case 0x900D: avr_op_LD_X_incr(); return;   /* 1001 000d dddd 1101 | LD */
		case 0x900A: avr_op_LD_Y_decr(); return;   /* 1001 000d dddd 1010 | LD */
		case 0x9009: avr_op_LD_Y_incr(); return;   /* 1001 000d dddd 1001 | LD */
		case 0x9002: avr_op_LD_Z_decr(); return;   /* 1001 000d dddd 0010 | LD */
		case 0x9001: avr_op_LD_Z_incr(); return;   /* 1001 000d dddd 0001 | LD */
		case 0x9004: avr_op_LPM_Z(); return;       /* 1001 000d dddd 0100 | LPM */
		case 0x9005: avr_op_LPM_Z_incr(); return;  /* 1001 000d dddd 0101 | LPM */
		case 0x9406: avr_op_LSR(); return;         /* 1001 010d dddd 0110 | LSR */
		case 0x9401: avr_op_NEG(); return;         /* 1001 010d dddd 0001 | NEG */
		case 0x900F: avr_op_POP(); return;         /* 1001 000d dddd 1111 | POP */
		case 0x920F: avr_op_PUSH(); return;        /* 1001 001d dddd 1111 | PUSH */
		case 0x9407: avr_op_ROR(); return;         /* 1001 010d dddd 0111 | ROR */
		case 0x9200: avr_op_STS(opcode2); return;  /* 1001 001d dddd 0000 | STS */
		case 0x920C: avr_op_ST_X(); return;        /* 1001 001d dddd 1100 | ST */
		case 0x920E: avr_op_ST_X_decr(); return;   /* 1001 001d dddd 1110 | ST */
		case 0x920D: avr_op_ST_X_incr(); return;   /* 1001 001d dddd 1101 | ST */
		case 0x920A: avr_op_ST_Y_decr(); return;   /* 1001 001d dddd 1010 | ST */
		case 0x9209: avr_op_ST_Y_incr(); return;   /* 1001 001d dddd 1001 | ST */
		case 0x9202: avr_op_ST_Z_decr(); return;   /* 1001 001d dddd 0010 | ST */
		case 0x9201: avr_op_ST_Z_incr(); return;   /* 1001 001d dddd 0001 | ST */
		case 0x9402: avr_op_SWAP(); return;        /* 1001 010d dddd 0010 | SWAP */
	}

	/* opcodes with a register (Rd) and a constant data (K) as operands */
	decode = opcode1 & ~(mask_Rd_4 | mask_K_8);
	switch ( decode ) {
		case 0x7000: avr_op_ANDI(opcode1); return;        /* 0111 KKKK dddd KKKK | CBR or ANDI */
		case 0x3000: avr_op_CPI(opcode1); return;         /* 0011 KKKK dddd KKKK | CPI */
		case 0xE000: avr_op_LDI(opcode1); return;         /* 1110 KKKK dddd KKKK | LDI or SER */
		case 0x6000: avr_op_ORI(opcode1); return;         /* 0110 KKKK dddd KKKK | SBR or ORI */
		case 0x4000: avr_op_SBCI(opcode1); return;        /* 0100 KKKK dddd KKKK | SBCI */
		case 0x5000: avr_op_SUBI(opcode1); return;        /* 0101 KKKK dddd KKKK | SUBI */
	}

	global_bitmask = 1 << (opcode1 & 0x0007);
	/* opcodes with a register (Rd) and a register bit number (b) as operands */
	decode = opcode1 & ~(mask_Rd_5 | mask_reg_bit);
	switch ( decode ) {
		case 0xF800: avr_op_BLD(); return;         /* 1111 100d dddd 0bbb | BLD */
		case 0xFA00: avr_op_BST(); return;         /* 1111 101d dddd 0bbb | BST */
		case 0xFC00: avr_op_SBRC(); return;        /* 1111 110d dddd 0bbb | SBRC */
		case 0xFE00: avr_op_SBRS(); return;        /* 1111 111d dddd 0bbb | SBRS */
	}

	/* opcodes with a relative 7-bit address (k) and a register bit number (b) as operands */
	decode = opcode1 & ~(mask_k_7 | mask_reg_bit);
	switch ( decode ) {
		case 0xF400: avr_op_BRBC(opcode1); return;        /* 1111 01kk kkkk kbbb | BRBC */
		case 0xF000: avr_op_BRBS(opcode1); return;        /* 1111 00kk kkkk kbbb | BRBS */
	}

	/* opcodes with a 6-bit address displacement (q) and a register (Rd) as operands */
	decode = opcode1 & ~(mask_Rd_5 | mask_q_displ);
	switch ( decode ) {
		case 0x8008: avr_op_LDD_Y(opcode1); return;       /* 10q0 qq0d dddd 1qqq | LDD */
		case 0x8000: avr_op_LDD_Z(opcode1); return;       /* 10q0 qq0d dddd 0qqq | LDD */
		case 0x8208: avr_op_STD_Y(opcode1); return;       /* 10q0 qq1d dddd 1qqq | STD */
		case 0x8200: avr_op_STD_Z(opcode1); return;       /* 10q0 qq1d dddd 0qqq | STD */
	}

	/* opcodes with a absolute 22-bit address (k) operand */
	decode = opcode1 & ~(mask_k_22);
	switch ( decode ) {
		case 0x940E: avr_op_CALL(opcode1, opcode2); return;        /* 1001 010k kkkk 111k | CALL */
		case 0x940C: avr_op_JMP(opcode1, opcode2); return;         /* 1001 010k kkkk 110k | JMP */
	}

	/* opcode1 with a sreg bit select (s) operand */
	decode = opcode1 & ~(mask_sreg_bit);
	switch ( decode ) {
		/* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
		/* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
		case 0x9488: avr_op_BCLR(opcode1); return;        /* 1001 0100 1sss 1000 | BCLR */
		case 0x9408: avr_op_BSET(opcode1); return;        /* 1001 0100 0sss 1000 | BSET */
	}

	/* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
	decode = opcode1 & ~(mask_K_6 | mask_Rd_2);
	switch ( decode ) {
		case 0x9600:                                      /* 1001 0110 KKdd KKKK | ADIW */
		case 0x9700: avr_op_ADIW_SBIW(opcode1); return;   /* 1001 0111 KKdd KKKK | SBIW */
	}

	/* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
	decode = opcode1 & ~(mask_A_5 | mask_reg_bit);
	switch ( decode ) {
		case 0x9800: avr_op_CBI(opcode1); return;         /* 1001 1000 AAAA Abbb | CBI */
		case 0x9A00: avr_op_SBI(opcode1); return;         /* 1001 1010 AAAA Abbb | SBI */
		case 0x9900: avr_op_SBIC(opcode1); return;        /* 1001 1001 AAAA Abbb | SBIC */
		case 0x9B00: avr_op_SBIS(opcode1); return;        /* 1001 1011 AAAA Abbb | SBIS */
	}

	/* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
	decode = opcode1 & ~(mask_A_6 | mask_Rd_5);
	switch ( decode ) {
		case 0xB000: avr_op_IN(opcode1); return;          /* 1011 0AAd dddd AAAA | IN */
		case 0xB800: avr_op_OUT(opcode1); return;         /* 1011 1AAd dddd AAAA | OUT */
	}

	/* opcodes with a relative 12-bit address (k) operand */
	decode = opcode1 & ~(mask_k_12);
	switch ( decode ) {
		case 0xD000: avr_op_RCALL(opcode1); return;       /* 1101 kkkk kkkk kkkk | RCALL */
		case 0xC000: avr_op_RJMP(opcode1); return;        /* 1100 kkkk kkkk kkkk | RJMP */
	}

	/* opcodes with two 4-bit register (Rd and Rr) operands */
	decode = opcode1 & ~(mask_Rd_4 | mask_Rr_4);
	switch ( decode ) {
		case 0x0100: avr_op_MOVW(opcode1); return;        /* 0000 0001 dddd rrrr | MOVW */
		case 0x0200: avr_op_MULS(); return;               /* 0000 0010 dddd rrrr | MULS */
	}

	/* opcodes with two 3-bit register (Rd and Rr) operands */
	decode = opcode1 & ~(mask_Rd_3 | mask_Rr_3);
	switch ( decode ) {
		case 0x0300: avr_op_MULSU(); return;       /* 0000 0011 0ddd 0rrr | MULSU */
		case 0x0308: avr_op_FMUL(); return;        /* 0000 0011 0ddd 1rrr | FMUL */
		case 0x0380: avr_op_FMULS(); return;       /* 0000 0011 1ddd 0rrr | FMULS */
		case 0x0388: avr_op_FMULSU(); return;      /* 0000 0011 1ddd 1rrr | FMULSU */
	}

	avr_op_ILLEGAL(opcode1);
}

static int execute(void)
{
	word opcode1, opcode2;
#ifdef LOG_DUMP
	char buf[32];
#endif

	program_cycles = 0;
	cpu_PC = 0;
	wants_to_exit = 0;

	while (!wants_to_exit) {
		if ((cpu_PC * 2) % flash_size > program_size) {
			printf("ABORTED: program counter out of range\n");
			exit(0);
		}

#ifdef LOG_DUMP
		sprintf(buf, "%04x: ", cpu_PC * 2);
		log_add_data(buf);
#endif

		opcode1 = flash_read_word(cpu_PC * 2);
		if (opcode_size(opcode1) == 2) {
			cpu_PC++;
			opcode2 = flash_read_word(cpu_PC * 2);
		} else
			opcode2 = 0;
		cpu_PC++;
		execute_opcode(opcode1, opcode2);

#ifdef LOG_DUMP
		log_dump_line();
#endif
	}
	return exit_code;
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
	program_cycles_limit = 500000000;

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



// main: as simple as it gets
int main(int argc, char *argv[])
{
	if (parse_args(argc, argv))
		return 1;

	return execute();
}

