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

#include "testavr.h"
#include "options.h"


enum decoder_operand_masks
  {
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


// ----------------------------------------------------------------------------
//     ELF loader.

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

#define EI_NIDENT 16
typedef struct
{
  unsigned char e_ident[EI_NIDENT];  // Magic number and other info
  Elf32_Half    e_type;           // Object file type
  Elf32_Half    e_machine;        // Architecture
  Elf32_Word    e_version;        // Object file version
  Elf32_Addr    e_entry;          // Entry point virtual address
  Elf32_Off     e_phoff;          // Program header table file offset
  Elf32_Off     e_shoff;          // Section header table file offset
  Elf32_Word    e_flags;          // Processor-specific flags
  Elf32_Half    e_ehsize;         // ELF header size in bytes
  Elf32_Half    e_phentsize;      // Program header table entry size
  Elf32_Half    e_phnum;          // Program header table entry count
  Elf32_Half    e_shentsize;      // Section header table entry size
  Elf32_Half    e_shnum;          // Section header table entry count
  Elf32_Half    e_shstrndx;       // Section header string table index
} Elf32_Ehdr;

typedef struct
{
  Elf32_Word    p_type;           // Segment type
  Elf32_Off     p_offset;         // Segment file offset
  Elf32_Addr    p_vaddr;          // Segment virtual address
  Elf32_Addr    p_paddr;          // Segment physical address
  Elf32_Word    p_filesz;         // Segment size in file
  Elf32_Word    p_memsz;          // Segment size in memory
  Elf32_Word    p_flags;          // Segment flags
  Elf32_Word    p_align;          // Segment alignment
} Elf32_Phdr;

typedef struct
{
  Elf32_Word sh_name;              // Section name
  Elf32_Word sh_type;              // Section type
  Elf32_Word sh_flags;             // Section flags
  Elf32_Addr sh_addr;              // Section address of 1st byte in memory
  Elf32_Off  sh_offset;            // Byte offset from beginning of program
  Elf32_Word sh_size;              // Section size in bytes
  Elf32_Word sh_link;              // Section header index list
  Elf32_Word sh_info;              // Extra section info
  Elf32_Word sh_addralign;         // Section alignment
  Elf32_Word sh_entsize;           // Size of members in table
} Elf32_Shdr;


// Symbol table entry
typedef struct
{
  Elf32_Word   st_name;            // Index into string table
  Elf32_Addr   st_value;           // Value of the symbol: address, abs, ...
  Elf32_Word   st_size;            // Size (of object etc) or 0 for unknown
  uint8_t      st_info;            // Symbol type and binding
  uint8_t      st_other;           // Reserved (0)
  Elf32_Half   st_shndx;           // Section header table index
} Elf32_Sym;


#define EI_CLASS 4
#define ELFCLASS32 1

#define EI_DATA 5
#define ELFDATA2LSB 1

#define EI_VERSION 5
#define EV_CURRENT 1

#define ET_EXEC 2
#define EM_AVR 0x53

#define PT_LOAD 1

#define DATA_VADDR 0x800000

#define SHT_NULL      0            // Section header is inactive
#define SHT_PROGBITS  1            // Holds information defined by the program
#define SHT_SYMTAB    2            // Holds a symbol table
#define SHT_STRTAB    3            // Holds a string table
#define SHT_RELA      4            // Holds relocation entries with addends
#define SHT_HASH      5            // Holds holds a hash table
#define SHT_DYNAMIC   6            // Holds info for dynamic linking
#define SHT_NOTE      7            // Holds information to mark the file
#define SHT_NOBITS    8            // Like SHT_PROGBITS but occupies no size
#define SHT_REL       9            // Holds relocation entries without addends
#define SHT_SHLIB    10            // Reserved
#define SHT_DYNSYM   11            // Minimal symbol table for dynamic linking

#define ELF32_ST_BIND(sym) (0xf & ((sym) >> 4))
#define STB_LOCAL   0              // Symbol binds local
#define STB_GLOBAL  1              // Symbol binds global
#define STB_WEAK    2              // Symbol binds global, low precedence

#define ELF32_ST_TYPE(sym) (0xf & (sym))
#define STT_NOTYPE  0              // Symbol's type not specified
#define STT_OBJECT  1              // Symbol associated to an object
#define STT_FUNC    2              // Symbol associated to a function
#define STT_SECTION 3              // Symbol associated to a section
#define STT_FILE    4              // Symbol associated to a file

// Section header flags
#define SHF_WRITE   (1u << 0)      // Section can be written during execution
#define SHF_ALLOC   (1u << 1)      // Section occupies memory on target
#define SHF_EXEC    (1u << 2)      // Section contains executable code
#define SHF_MERGE   (1u << 4)      // Section data can be merged
#define SHF_STRINGS (1u << 5)      // Section contains 0-terminated strings

#define SHN_LORESERVE 0xff00       // First reserved section header index

static Elf32_Half
get_elf32_half (const Elf32_Half *v)
{
  const unsigned char *p = (const unsigned char*) v;
  return p[0] | (p[1] << 8);
}

static Elf32_Word
get_elf32_word (const Elf32_Word *v)
{
  const unsigned char *p = (const unsigned char*) v;
  return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void
load_symbol_string_table (FILE *f, const Elf32_Ehdr *ehdr)
{
  Elf32_Shdr *shdr;
  Elf32_Word e_shoff = get_elf32_word (&ehdr->e_shoff);
  Elf32_Half e_shnum = get_elf32_half (&ehdr->e_shnum);
  Elf32_Half e_shentsize = get_elf32_half (&ehdr->e_shentsize);

  // Read section headers
  if (e_shentsize != sizeof (Elf32_Shdr))
    leave (EXIT_STATUS_ABORTED, "ELF section headers invalid");
  if (fseek (f, e_shoff, SEEK_SET) != 0)
    leave (EXIT_STATUS_ABORTED, "ELF section header truncated");
  shdr = (Elf32_Shdr*) calloc (e_shnum, sizeof (Elf32_Shdr));
  if (fread (shdr, sizeof (Elf32_Shdr), e_shnum, f) != e_shnum)
    leave (EXIT_STATUS_ABORTED, "can't read ELF section headers");

  for (int n = 0; n < e_shnum; n++)
    {
      Elf32_Word sh_type = get_elf32_word (&shdr[n].sh_type);
      Elf32_Word sh_link = get_elf32_word (&shdr[n].sh_link);
      Elf32_Off  sh_offset = get_elf32_word (&shdr[n].sh_offset);
      Elf32_Word sh_size = get_elf32_word (&shdr[n].sh_size);
      size_t sh_entsize = (size_t) get_elf32_word (&shdr[n].sh_entsize);

      // Currently ELF does not hold more than 1 symbol table
      if (sh_type != SHT_SYMTAB)
        continue;

      if (sh_entsize != sizeof (Elf32_Sym))
        leave (EXIT_STATUS_ABORTED, "ELF symbol table header invalid: "
               "size %d != %d", (int) sh_entsize, (int) sizeof (Elf32_Sym));
      if (sh_size % sh_entsize != 0)
        leave (EXIT_STATUS_ABORTED, "ELF symbol table size invalid");

      // Read symbol table
      size_t n_syms = sh_size / sh_entsize;
      Elf32_Sym *symtab = (Elf32_Sym*) calloc (n_syms, sizeof (Elf32_Sym));
      if (fseek (f, sh_offset, SEEK_SET) != 0)
        leave (EXIT_STATUS_ABORTED, "ELF symbol table truncated");
      if (fread (symtab, sizeof (Elf32_Sym), n_syms, f) != n_syms)
        leave (EXIT_STATUS_ABORTED, "ELF symbol table truncated");

      // Read string table section header
      if (sh_link >= e_shnum)
        leave (EXIT_STATUS_ABORTED, "ELF section header truncated");
      sh_type   = get_elf32_word (&shdr[sh_link].sh_type);
      if (sh_type != SHT_STRTAB)
        leave (EXIT_STATUS_ABORTED, "ELF string table header invalid");

      // Read string table
      sh_offset = get_elf32_word (&shdr[sh_link].sh_offset);
      sh_size   = get_elf32_word (&shdr[sh_link].sh_size);
      char *strtab = (char*) malloc (sh_size);
      if (fseek (f, sh_offset, SEEK_SET) != 0)
        leave (EXIT_STATUS_ABORTED, "ELF string table truncated");
      if (fread (strtab, sh_size, 1, f) != 1)
        leave (EXIT_STATUS_ABORTED, "ELF string table truncated");

      // Iterate all symbols
      for (size_t n = 0; n < n_syms; n++)
        {
          int st_info = symtab[n].st_info;
          int type = ELF32_ST_TYPE (st_info);
          size_t name = (size_t) get_elf32_word (&symtab[n].st_name);
          int shndx = get_elf32_half (&symtab[n].st_shndx);
          if (name >= sh_size)
            leave (EXIT_STATUS_ABORTED, "ELF string table too short");

          unsigned flags = 0;
          if (type == STT_NOTYPE && shndx < SHN_LORESERVE)
            {
              if (shndx >= e_shnum)
                leave (EXIT_STATUS_ABORTED, "ELF section header truncated");
              sh_type = get_elf32_word (&shdr[shndx].sh_type);
              if (sh_type == SHT_PROGBITS)
                flags = get_elf32_word (&shdr[shndx].sh_flags);
            }
          if (type == STT_FUNC || (flags & SHF_EXEC))
            {
              int value = get_elf32_word (&symtab[n].st_value);
              set_function_symbol (value, &strtab[name], type == STT_FUNC);
            }
        }

      free (symtab);
      free (shdr);

      // Currently ELF does not hold more than 1 symbol table
      break;
    }
}

static void
load_elf (FILE *f, byte *flash, byte *ram)
{
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdr[16];
    
  rewind (f);
  if (fread (&ehdr, sizeof (ehdr), 1, f) != 1)
    leave (EXIT_STATUS_ABORTED, "can't read ELF header");
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS32
      || ehdr.e_ident[EI_DATA] != ELFDATA2LSB
      || ehdr.e_ident[EI_VERSION] != EV_CURRENT)
    leave (EXIT_STATUS_ABORTED, "bad ELF header");
  if (get_elf32_half (&ehdr.e_type) != ET_EXEC
      || get_elf32_half (&ehdr.e_machine) != EM_AVR
      || get_elf32_word (&ehdr.e_version) != EV_CURRENT
      || get_elf32_half (&ehdr.e_phentsize) != sizeof (Elf32_Phdr))
    leave (EXIT_STATUS_ABORTED, "ELF file is not an AVR executable");

  if (!options.do_entry_point)
    {
      unsigned pc = program_entry_point = get_elf32_word (&ehdr.e_entry);
      cpu_PC = pc / 2;
      if (pc >= MAX_FLASH_SIZE)
        leave (EXIT_STATUS_ABORTED, "ELF entry-point 0x%x it too big", pc);
      else if (pc % 2 != 0)
        leave (EXIT_STATUS_ABORTED, "ELF entry-point 0x%x is odd", pc);
    }

  int nbr_phdr = get_elf32_half (&ehdr.e_phnum);
  if ((unsigned) nbr_phdr > sizeof (phdr) / sizeof (*phdr))
    leave (EXIT_STATUS_ABORTED, "ELF file contains too many PHDR");

  if (fseek (f, get_elf32_word (&ehdr.e_phoff), SEEK_SET) != 0)
    leave (EXIT_STATUS_ABORTED, "ELF file truncated");
  size_t res = fread (phdr, sizeof (Elf32_Phdr), nbr_phdr, f);
  if (res != (size_t)nbr_phdr)
    leave (EXIT_STATUS_ABORTED, "can't read PHDRs of ELF file");

  for (int i = 0; i < nbr_phdr; i++)
    {
      Elf32_Addr addr = get_elf32_word (&phdr[i].p_paddr);
      Elf32_Addr vaddr = get_elf32_word (&phdr[i].p_vaddr);
      Elf32_Word filesz = get_elf32_word (&phdr[i].p_filesz);
      Elf32_Word memsz= get_elf32_word (&phdr[i].p_memsz);
        
      if (get_elf32_word (&phdr[i].p_type) != PT_LOAD)
        continue;
      if (filesz == 0)
        continue;
      /* if (verbose)
         printf ("Load 0x%06x - 0x%06x\n",
         (unsigned)addr, (unsigned)(addr + memsz)); */
      if (addr + memsz > MAX_FLASH_SIZE)
        leave (EXIT_STATUS_ABORTED,
               "program too big to fit in flash");
      if (fseek (f, get_elf32_word (&phdr[i].p_offset), SEEK_SET) != 0)
        leave (EXIT_STATUS_ABORTED, "ELF file truncated");

      /* Read to Flash.  */
      if (fread (flash + addr, filesz, 1, f) != 1)
        leave (EXIT_STATUS_ABORTED, "ELF file truncated");

      /* Also copy in SRAM.  */
      if (options.do_initialize_sram && vaddr >= DATA_VADDR)
        memcpy (ram + vaddr - DATA_VADDR, flash + addr, filesz);

      /* No need to clear memory.  */
      if ((unsigned) (addr + memsz) > program_size)
        program_size = addr + memsz;
    }

  if (options.do_symbols)
    load_symbol_string_table (f, &ehdr);
}

void
load_to_flash (const char *filename, byte *flash, byte *ram)
{
  char buf[EI_NIDENT];

  FILE *fp = fopen (filename, "rb");
  if (!fp)
    leave (EXIT_STATUS_ABORTED, "can't open program file");

  size_t len = fread (buf, 1, sizeof (buf), fp);
  if (len == sizeof (buf)
      && buf[0] == 0x7f
      && buf[1] == 'E'
      && buf[2] == 'L'
      && buf[3] == 'F')
    {
      load_elf (fp, flash, ram);
    }
  else
    {
      rewind (fp);
      program_size = fread (flash, 1, MAX_FLASH_SIZE, fp);
    }
  fclose (fp);

  if (program_size & (~arch.flash_addr_mask))
    {
      leave (EXIT_STATUS_ABORTED, "program is too large (size: %"PRIu32
             ", max: %u)", program_size, arch.flash_addr_mask + 1);
    }
}


// Opcodes with no arguments except NOP       1001 010~ ~~~~ ~~~~ 
static const byte avr_op_16_index[1 + 0x1ff] = {
  [0x9519 ^ 0x9400] = ID_EICALL, /* 1001 0101 0001 1001 | EICALL */
  [0x9419 ^ 0x9400] = ID_EIJMP,  /* 1001 0100 0001 1001 | EIJMP */
  [0x95D8 ^ 0x9400] = ID_ELPM,   /* 1001 0101 1101 1000 | ELPM */
  [0x95F8 ^ 0x9400] = ID_ESPM,   /* 1001 0101 1111 1000 | ESPM */
  [0x9509 ^ 0x9400] = ID_ICALL,  /* 1001 0101 0000 1001 | ICALL */
  [0x9409 ^ 0x9400] = ID_IJMP,   /* 1001 0100 0000 1001 | IJMP */
  [0x95C8 ^ 0x9400] = ID_LPM,    /* 1001 0101 1100 1000 | LPM */
  [0x9508 ^ 0x9400] = ID_RET,    /* 1001 0101 0000 1000 | RET */
  [0x9518 ^ 0x9400] = ID_RETI,   /* 1001 0101 0001 1000 | RETI */
  [0x9588 ^ 0x9400] = ID_SLEEP,  /* 1001 0101 1000 1000 | SLEEP */
  [0x95E8 ^ 0x9400] = ID_SPM,    /* 1001 0101 1110 1000 | SPM */
  [0x95A8 ^ 0x9400] = ID_WDR,    /* 1001 0101 1010 1000 | WDR */
};

// Opcodes unique with upper 6 bits           ~~~~ ~~rd dddd rrrr
static const byte avr_op_6_index[1 << 6] = {
  [0x1C00 >> 10] = ID_ADC,       /* 0001 11rd dddd rrrr | ADC, ROL */
  [0x0C00 >> 10] = ID_ADD,       /* 0000 11rd dddd rrrr | ADD, LSL */
  [0x2000 >> 10] = ID_AND,       /* 0010 00rd dddd rrrr | AND, TST */
  [0x1400 >> 10] = ID_CP,        /* 0001 01rd dddd rrrr | CP */
  [0x0400 >> 10] = ID_CPC,       /* 0000 01rd dddd rrrr | CPC */
  [0x1000 >> 10] = ID_CPSE,      /* 0001 00rd dddd rrrr | CPSE */
  [0x2400 >> 10] = ID_EOR,       /* 0010 01rd dddd rrrr | EOR, CLR */
  [0x2C00 >> 10] = ID_MOV,       /* 0010 11rd dddd rrrr | MOV */
  [0x9C00 >> 10] = ID_MUL,       /* 1001 11rd dddd rrrr | MUL */
  [0x2800 >> 10] = ID_OR,        /* 0010 10rd dddd rrrr | OR */
  [0x0800 >> 10] = ID_SBC,       /* 0000 10rd dddd rrrr | SBC */
  [0x1800 >> 10] = ID_SUB,       /* 0001 10rd dddd rrrr | SUB */
};

// Unique with upper 7, lower 4 bits              1001 0~~d dddd ~~~~
static const byte avr_op_74_index[1 + 0x7ff] = {
  [0x9405 ^ 0x9000] = ID_ASR,        /* 1001 010d dddd 0101 | ASR */
  [0x9400 ^ 0x9000] = ID_COM,        /* 1001 010d dddd 0000 | COM */
  [0x940A ^ 0x9000] = ID_DEC,        /* 1001 010d dddd 1010 | DEC */
  [0x9006 ^ 0x9000] = ID_ELPM_Z,     /* 1001 000d dddd 0110 | ELPM */
  [0x9007 ^ 0x9000] = ID_ELPM_Z_incr,/* 1001 000d dddd 0111 | ELPM */
  [0x9403 ^ 0x9000] = ID_INC,        /* 1001 010d dddd 0011 | INC */
  [0x9000 ^ 0x9000] = ID_LDS,        /* 1001 000d dddd 0000 | LDS */
  [0x900C ^ 0x9000] = ID_LD_X,       /* 1001 000d dddd 1100 | LD */
  [0x900E ^ 0x9000] = ID_LD_X_decr,  /* 1001 000d dddd 1110 | LD */
  [0x900D ^ 0x9000] = ID_LD_X_incr,  /* 1001 000d dddd 1101 | LD */
  [0x900A ^ 0x9000] = ID_LD_Y_decr,  /* 1001 000d dddd 1010 | LD */
  [0x9009 ^ 0x9000] = ID_LD_Y_incr,  /* 1001 000d dddd 1001 | LD */
  [0x9002 ^ 0x9000] = ID_LD_Z_decr,  /* 1001 000d dddd 0010 | LD */
  [0x9001 ^ 0x9000] = ID_LD_Z_incr,  /* 1001 000d dddd 0001 | LD */
  [0x9004 ^ 0x9000] = ID_LPM_Z,      /* 1001 000d dddd 0100 | LPM */
  [0x9005 ^ 0x9000] = ID_LPM_Z_incr, /* 1001 000d dddd 0101 | LPM */
  [0x9406 ^ 0x9000] = ID_LSR,        /* 1001 010d dddd 0110 | LSR */
  [0x9401 ^ 0x9000] = ID_NEG,        /* 1001 010d dddd 0001 | NEG */
  [0x900F ^ 0x9000] = ID_POP,        /* 1001 000d dddd 1111 | POP */

  [0x9204 ^ 0x9000] = ID_XCH,        /* 1001 001d dddd 0100 | XCH */
  [0x9205 ^ 0x9000] = ID_LAS,        /* 1001 001d dddd 0101 | LAS */
  [0x9206 ^ 0x9000] = ID_LAC,        /* 1001 001d dddd 0110 | LAC */
  [0x9207 ^ 0x9000] = ID_LAT,        /* 1001 001d dddd 0111 | LAT */

  [0x920F ^ 0x9000] = ID_PUSH,       /* 1001 001d dddd 1111 | PUSH */
  [0x9407 ^ 0x9000] = ID_ROR,        /* 1001 010d dddd 0111 | ROR */
  [0x9200 ^ 0x9000] = ID_STS,        /* 1001 001d dddd 0000 | STS */
  [0x920C ^ 0x9000] = ID_ST_X,       /* 1001 001d dddd 1100 | ST */
  [0x920E ^ 0x9000] = ID_ST_X_decr,  /* 1001 001d dddd 1110 | ST */
  [0x920D ^ 0x9000] = ID_ST_X_incr,  /* 1001 001d dddd 1101 | ST */
  [0x920A ^ 0x9000] = ID_ST_Y_decr,  /* 1001 001d dddd 1010 | ST */
  [0x9209 ^ 0x9000] = ID_ST_Y_incr,  /* 1001 001d dddd 1001 | ST */
  [0x9202 ^ 0x9000] = ID_ST_Z_decr,  /* 1001 001d dddd 0010 | ST */
  [0x9201 ^ 0x9000] = ID_ST_Z_incr,  /* 1001 001d dddd 0001 | ST */
  [0x9402 ^ 0x9000] = ID_SWAP,       /* 1001 010d dddd 0010 | SWAP */
};


static int
decode_opcode (decoded_op *op, unsigned opcode1, unsigned opcode2)
{
  byte index;

  // opcodes with no operands

  if (0x0000 == opcode1)
    return ID_NOP;                     /* 0000 0000 0000 0000 | NOP */

  // All other opcodes with no operands are:        1001 010x xxxx xxxx
  if ((opcode1 ^ 0x9400) <= 0x1ff
      && ((index = avr_op_16_index[opcode1 ^ 0x9400])))
    return index;

  // opcodes with two 5-bit register operands       xxxx xxrd dddd rrrr

  if ((index = avr_op_6_index[opcode1 >> 10]))
    {
      op->oper2 = (opcode1 & 0x0F) | ((opcode1 >> 5) & 0x0010);
      op->oper1 = (opcode1 >> 4) & 0x1F;
      if (ID_ADD == index && op->oper1 == op->oper2)  return ID_LSL;
      if (ID_ADC == index && op->oper1 == op->oper2)  return ID_ROL;
      if (ID_EOR == index && op->oper1 == op->oper2)  return ID_CLR;
      if (ID_AND == index && op->oper1 == op->oper2)  return ID_TST;
      return index;
    }

  // opcodes with a single register operand         1001 0xxd dddd xxxx

  unsigned decode = opcode1 & ~(mask_Rd_5);
  if ((decode ^ 0x9000) <= 0x7ff
      && ((index = avr_op_74_index[decode ^ 0x9000])))
    {
      // oper2 only used with LDS, STS
      op->oper2 = opcode2;
      op->oper1 = (opcode1 >> 4) & 0x1F;
      return index;
    }

  /* opcodes with a register (Rd) and a constant data (K) as operands */
  op->oper1 = ((opcode1 >> 4) & 0xF) | 0x10;
  op->oper2 = (opcode1 & 0x0F) | ((opcode1 >> 4) & 0x00F0);

  decode = opcode1 & ~(mask_Rd_4 | mask_K_8);
  switch (decode) {
  case 0x7000: return ID_ANDI; /* 0111 KKKK dddd KKKK | CBR or ANDI */
  case 0x3000: return ID_CPI;  /* 0011 KKKK dddd KKKK | CPI */
  case 0xE000: return ID_LDI;  /* 1110 KKKK dddd KKKK | LDI or SER */
  case 0x6000: return ID_ORI;  /* 0110 KKKK dddd KKKK | SBR or ORI */
  case 0x4000: return ID_SBCI; /* 0100 KKKK dddd KKKK | SBCI */
  case 0x5000: return ID_SUBI; /* 0101 KKKK dddd KKKK | SUBI */
  }

  /* opcodes with a register (Rd) and a register bit number (b) as operands */
  op->oper1 = (opcode1 >> 4) & 0x1F;
  op->oper2 = 1 << (opcode1 & 0x0007);
  decode = opcode1 & ~(mask_Rd_5 | mask_reg_bit);
  switch (decode) {
  case 0xF800: return ID_BLD;   /* 1111 100d dddd 0bbb | BLD */
  case 0xFA00: return ID_BST;   /* 1111 101d dddd 0bbb | BST */
  case 0xFC00: return ID_SBRC;  /* 1111 110d dddd 0bbb | SBRC */
  case 0xFE00: return ID_SBRS;  /* 1111 111d dddd 0bbb | SBRS */
  }

  /* opcodes with a relative 7-bit address (k) and a register bit number (b)
     as operands */
  op->oper1 = (opcode1 >> 3) & 0x7F;
  decode = opcode1 & ~(mask_k_7 | mask_reg_bit);
  switch (decode) {
  case 0xF400: return ID_BRBC;   /* 1111 01kk kkkk kbbb | BRBC */
  case 0xF000: return ID_BRBS;   /* 1111 00kk kkkk kbbb | BRBS */
  }

  /* opcodes with a 6-bit address displacement (q) and a register (Rd)
     as operands */

  if ((opcode1 & 0xd000) == 0x8000)
    {
      op->oper1 = (opcode1 >> 4) & 0x1F;
      op->oper2 = ((opcode1 & 0x7)
                   | ((opcode1 >> 7) & 0x18)
                   | ((opcode1 >> 8) & 0x20));
      decode = opcode1 & ~(mask_Rd_5 | mask_q_displ);
      switch (decode) {
      case 0x8008: return ID_LDD_Y;   /* 10q0 qq0d dddd 1qqq | LDD */
      case 0x8000: return ID_LDD_Z;   /* 10q0 qq0d dddd 0qqq | LDD */
      case 0x8208: return ID_STD_Y;   /* 10q0 qq1d dddd 1qqq | STD */
      case 0x8200: return ID_STD_Z;   /* 10q0 qq1d dddd 0qqq | STD */
      }
    }

  /* opcodes with a absolute 22-bit address (k) operand */
  op->oper1 = (opcode1 & 1) | ((opcode1 >> 3) & 0x3E);
  op->oper2 = opcode2;
  decode = opcode1 & ~(mask_k_22);
  switch (decode) {
  case 0x940E: return ID_CALL;        /* 1001 010k kkkk 111k | CALL */
  case 0x940C: return ID_JMP;         /* 1001 010k kkkk 110k | JMP */
  }

  /* opcode1 with a sreg bit select (s) operand */
  op->oper1 = 1 << ((opcode1 >> 4) & 0x07);
  decode = opcode1 & ~(mask_sreg_bit);
  switch (decode) {
    /* BCLR takes care of CL{C,Z,N,V,S,H,T,I} */
    /* BSET takes care of SE{C,Z,N,V,S,H,T,I} */
  case 0x9488: return ID_BCLR;        /* 1001 0100 1sss 1000 | BCLR */
  case 0x9408: return ID_BSET;        /* 1001 0100 0sss 1000 | BSET */
  }

  /* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
  op->oper1 = ((opcode1 >> 3) & 0x06) + 24;
  op->oper2 = (opcode1 & 0xF) | ((opcode1 >> 2) & 0x30);
  decode = opcode1 & ~(mask_K_6 | mask_Rd_2);
  switch (decode) {
  case 0x9600: return ID_ADIW;        /* 1001 0110 KKdd KKKK | ADIW */
  case 0x9700: return ID_SBIW;        /* 1001 0111 KKdd KKKK | SBIW */
  }

  /* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
  op->oper1 = ((opcode1 >> 3) & 0x1F) + io_base;
  op->oper2 = 1 << (opcode1 & 0x0007);
  decode = opcode1 & ~(mask_A_5 | mask_reg_bit);
  switch (decode) {
  case 0x9800: return ID_CBI;         /* 1001 1000 AAAA Abbb | CBI */
  case 0x9A00: return ID_SBI;         /* 1001 1010 AAAA Abbb | SBI */
  case 0x9900: return ID_SBIC;        /* 1001 1001 AAAA Abbb | SBIC */
  case 0x9B00: return ID_SBIS;        /* 1001 1011 AAAA Abbb | SBIS */
  }

  /* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
  op->oper1 = ((opcode1 >> 4) & 0x1F);
  op->oper2 = ((opcode1 & 0x0F) | ((opcode1 >> 5) & 0x30)) + io_base;
  decode = opcode1 & ~(mask_A_6 | mask_Rd_5);
  switch (decode) {
  case 0xB000: return ID_IN;          /* 1011 0AAd dddd AAAA | IN */
  case 0xB800: return ID_OUT;         /* 1011 1AAd dddd AAAA | OUT */
  }

  /* opcodes with a relative 12-bit address (k) operand */
  op->oper2 = opcode1 & 0xFFF;
  decode = opcode1 & ~(mask_k_12);
  switch (decode) {
  case 0xD000: return ID_RCALL;   /* 1101 kkkk kkkk kkkk | RCALL */
  case 0xC000: return ID_RJMP;    /* 1100 kkkk kkkk kkkk | RJMP */
  }

  /* opcodes with two 4-bit register (Rd and Rr) operands */
  decode = opcode1 & ~(mask_Rd_4 | mask_Rr_4);
  switch (decode) {
  case 0x0100:
    op->oper1 = ((opcode1 >> 4) & 0x0F) << 1;
    op->oper2 = (opcode1 & 0x0F) << 1;
    return ID_MOVW;        /* 0000 0001 dddd rrrr | MOVW */
  case 0x0200:
    op->oper1 = ((opcode1 >> 4) & 0x0F) | 0x10;
    op->oper2 = (opcode1 & 0x0F) | 0x10;
    return ID_MULS;        /* 0000 0010 dddd rrrr | MULS */
  }

  /* opcodes with two 3-bit register (Rd and Rr) operands */
  op->oper1 = ((opcode1 >> 4) & 0x07) | 0x10;
  op->oper2 = (opcode1 & 0x07) | 0x10;
  decode = opcode1 & ~(mask_Rd_3 | mask_Rr_3);
  switch (decode) {
  case 0x0300: return ID_MULSU;   /* 0000 0011 0ddd 0rrr | MULSU */
  case 0x0308: return ID_FMUL;    /* 0000 0011 0ddd 1rrr | FMUL */
  case 0x0380: return ID_FMULS;   /* 0000 0011 1ddd 0rrr | FMULS */
  case 0x0388: return ID_FMULSU;  /* 0000 0011 1ddd 1rrr | FMULSU */
  }

  op->oper2 = opcode1;
  return ID_ILLEGAL;
}

void
decode_flash (decoded_op decoded[], const byte flash[])
{
  word opcode1 = flash[0] | (flash[1] << 8);
  
  for (unsigned i = 0; i < program_size; i += 2)
    {
      word opcode2 = flash[i + 2] | (flash[i + 3] << 8);
      decoded[i / 2].id
        = decode_opcode (&decoded[i / 2], opcode1, opcode2);
      opcode1 = opcode2;
    }
}
