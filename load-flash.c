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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "testavr.h"
#include "options.h"


enum decoder_operand_masks
  {
    mask_Rd_2     = 0x0030,    // 2 bit register id (R24, R26, R28, R30)
    mask_Rd_3     = 0x0070,    // 3 bit register id (R16--R23)
    mask_Rd_4     = 0x00f0,    // 4 bit register id (R16--R31)
    mask_Rd_5     = 0x01f0,    // 5 bit register id (R00--R31)

    mask_Rr_3     = 0x0007,    // 3 bit register id (R16--R23)
    mask_Rr_4     = 0x000f,    // 4 bit register id (R16--R31)
    mask_Rr_5     = 0x020f,    // 5 bit register id (R00--R31)

    mask_K_8      = 0x0F0F,    // for 8 bit constant
    mask_K_6      = 0x00CF,    // for 6 bit constant

    mask_k_7      = 0x03F8,    // for  7 bit relative address
    mask_k_12     = 0x0FFF,    // for 12 bit relative address
    mask_k_22     = 0x01F1,    // for 22 bit absolute address


    mask_reg_bit  = 0x0007,    // register bit select
    mask_sreg_bit = 0x0070,    // status register bit select
    mask_q_displ  = 0x2C07,    // address displacement (q)

    mask_A_5      = 0x00F8,    // 5 bit register id (R00--R31)
    mask_A_6      = 0x060F,    // 6 bit IO port id

    mask_JMP_CALL = 0xfe0c,    // identify JMP / CALL (1001 010x xxxx 11xx)
    mask_LDS_STS  = 0xfc0f     // identify LDS / STS  (1001 00xx xxxx 0000)
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
  uint8_t e_ident[EI_NIDENT];  // Magic number and other info
  Elf32_Half  e_type;          // Object file type
  Elf32_Half  e_machine;       // Architecture
  Elf32_Word  e_version;       // Object file version
  Elf32_Addr  e_entry;         // Entry point virtual address
  Elf32_Off   e_phoff;         // Program header table file offset
  Elf32_Off   e_shoff;         // Section header table file offset
  Elf32_Word  e_flags;         // Processor-specific flags
  Elf32_Half  e_ehsize;        // ELF header size in bytes
  Elf32_Half  e_phentsize;     // Program header table entry size
  Elf32_Half  e_phnum;         // Program header table entry count
  Elf32_Half  e_shentsize;     // Section header table entry size
  Elf32_Half  e_shnum;         // Section header table entry count
  Elf32_Half  e_shstrndx;      // Section header string table index
} Elf32_Ehdr;

typedef struct
{
  Elf32_Word  p_type;          // Segment type
  Elf32_Off   p_offset;        // Segment file offset
  Elf32_Addr  p_vaddr;         // Segment virtual address
  Elf32_Addr  p_paddr;         // Segment physical address
  Elf32_Word  p_filesz;        // Segment size in file
  Elf32_Word  p_memsz;         // Segment size in memory
  Elf32_Word  p_flags;         // Segment flags
  Elf32_Word  p_align;         // Segment alignment
} Elf32_Phdr;

typedef struct
{
  Elf32_Word sh_name;          // Section name
  Elf32_Word sh_type;          // Section type
  Elf32_Word sh_flags;         // Section flags
  Elf32_Addr sh_addr;          // Section address of 1st byte in memory
  Elf32_Off  sh_offset;        // Byte offset from beginning of program
  Elf32_Word sh_size;          // Section size in bytes
  Elf32_Word sh_link;          // Section header index list
  Elf32_Word sh_info;          // Extra section info
  Elf32_Word sh_addralign;     // Section alignment
  Elf32_Word sh_entsize;       // Size of members in table
} Elf32_Shdr;

typedef struct
{
  Elf32_Word n_namesz;         // Length of the name.  Follows this note.
  Elf32_Word n_descsz;         // Length of the descriptor field. Follows name.
  Elf32_Word n_type;
} Elf32_Nhdr;


// Symbol table entry
typedef struct
{
  Elf32_Word   st_name;        // Index into string table
  Elf32_Addr   st_value;       // Value of the symbol: address, abs, ...
  Elf32_Word   st_size;        // Size (of object etc) or 0 for unknown
  uint8_t      st_info;        // Symbol type and binding
  uint8_t      st_other;       // Reserved (0)
  Elf32_Half   st_shndx;       // Section header table index
} Elf32_Sym;


#define EI_CLASS 4
#define ELFCLASS32 1

#define EI_DATA 5
#define ELFDATA2LSB 1

#define EI_VERSION 5
#define EV_CURRENT 1

#define ET_EXEC 2
#define EM_AVR 0x53

// Processor specific flags for the ELF header e_flags field.
#define EF_AVR_MACH 0x7F

// Program Header Type
#define PT_LOAD 1

// Program Header Flags
#define PF_X    (1 << 0)       // Execute
#define PF_W    (1 << 1)       // Write
#define PF_R    (1 << 2)       // Read

#define DATA_VADDR        0x800000
#define DATA_VADDR_END    0x80ffff

#define EEPROM_VADDR      0x810000
#define EEPROM_VADDR_END  0x81ffff

// Section header type
#define SHT_NULL      0        // Section header is inactive
#define SHT_PROGBITS  1        // Holds information defined by the program
#define SHT_SYMTAB    2        // Holds a symbol table
#define SHT_STRTAB    3        // Holds a string table
#define SHT_RELA      4        // Holds relocation entries with addends
#define SHT_HASH      5        // Holds holds a hash table
#define SHT_DYNAMIC   6        // Holds info for dynamic linking
#define SHT_NOTE      7        // Holds information to mark the file
#define SHT_NOBITS    8        // Like SHT_PROGBITS but occupies no size
#define SHT_REL       9        // Holds relocation entries without addends
#define SHT_SHLIB    10        // Reserved
#define SHT_DYNSYM   11        // Minimal symbol table for dynamic linking

const char * const s_SHT[] =
  {
    [SHT_NULL]     = "NULL",
    [SHT_PROGBITS] = "PROGBITS",
    [SHT_SYMTAB]   = "SYMTAB",
    [SHT_STRTAB]   = "STRTAB",
    [SHT_RELA]     = "RELA",
    [SHT_HASH]     = "HASH",
    [SHT_DYNAMIC]  = "DYNAMIC",
    [SHT_NOTE]     = "NOTE",
    [SHT_NOBITS]   = "NOBITS",
    [SHT_REL]      = "REL",
    [SHT_SHLIB]    = "SHLIB",
    [SHT_DYNSYM]   = "DYNSYM",
  };

// Section header flags
#define SHF_WRITE      (1u <<  0)  // Section can be written during execution
#define SHF_ALLOC      (1u <<  1)  // Section occupies memory on target
#define SHF_EXEC       (1u <<  2)  // Section contains executable code
#define SHF_MERGE      (1u <<  4)  // Section data can be merged
#define SHF_STRINGS    (1u <<  5)  // Section contains 0-terminated strings
#define SHF_INFO_LINK  (1u <<  6)  // Section .sh_info links to a section header
#define SHF_GROUP      (1u <<  9)  // Section group
#define SHF_EXCLUDE    (1u << 31)  // Section exclude
//                   "01234567890123456789012345678901"
const char s_SHF[] = "wax.MS>..G.....................E";

// Special values for Elf32_Sym.st_shndx
#define SHN_LORESERVE 0xff00   // First reserved section header index

// Symbol table bindings
#define ELF32_ST_BIND(sym) (0xf & ((sym) >> 4))
#define STB_LOCAL   0          // Symbol binds local
#define STB_GLOBAL  1          // Symbol binds global
#define STB_WEAK    2          // Symbol binds global, low precedence

// Symbol table types
#define ELF32_ST_TYPE(sym) (0xf & (sym))
#define STT_NOTYPE  0          // Symbol's type not specified
#define STT_OBJECT  1          // Symbol associated to an object
#define STT_FUNC    2          // Symbol associated to a function
#define STT_SECTION 3          // Symbol associated to a section
#define STT_FILE    4          // Symbol associated to a file

typedef struct
{
  Elf32_Word flash_start;
  Elf32_Word flash_end;
  Elf32_Word ram_start;
  Elf32_Word ram_size;
  Elf32_Word eeprom_start;
  Elf32_Word eeprom_size;
  // 8 bytes of info for a string table that follows.
  Elf32_Word index_len; // = 8
  Elf32_Word devname_offset; // = 1 = Offset of "atmega*" etc. in that table.
} avr_deviceinfo_t;

#define NOTE_AVR_DEVICEINFO ".note.gnu.avr.deviceinfo"

static bool have_strtab;

// From .note.gnu.avr.devicename if present.
static bool have_deviceinfo;
static avr_deviceinfo_t avr_deviceinfo;
static char avr_devicename[32];

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

// Load and allocate a string table from SHDR with sh_type = SHT_STRTAB.
static char*
load_string_table (FILE *f, const Elf32_Shdr *shdr, Elf32_Word *psh_size,
                   const char *name)
{
  Elf32_Word sh_type = get_elf32_word (&shdr->sh_type);
  if (sh_type != SHT_STRTAB)
    leave (LEAVE_ELF, "%s header invalid", name);

  Elf32_Off sh_offset = get_elf32_word (&shdr->sh_offset);
  *psh_size = get_elf32_word (&shdr->sh_size);

  char *strtab = get_mem (*psh_size, sizeof (char), name);

  if (fseek (f, sh_offset, SEEK_SET) != 0
      || fread (strtab, *psh_size, 1, f) != 1)
    leave (LEAVE_ELF, "%s truncated", name);

  return strtab;
}

// N is the index of SYMTAB in the section headers SHDR.
static void
load_symbol_table (FILE *f, const Elf32_Ehdr *ehdr, const Elf32_Shdr *shdr,
                   int n)
{
  Elf32_Half e_shnum = get_elf32_half (&ehdr->e_shnum);

  Elf32_Word sh_link = get_elf32_word (&shdr[n].sh_link);
  Elf32_Word sh_size = get_elf32_word (&shdr[n].sh_size);
  Elf32_Off sh_offset = get_elf32_word (&shdr[n].sh_offset);
  size_t sh_entsize = (size_t) get_elf32_word (&shdr[n].sh_entsize);

  if (sh_entsize != sizeof (Elf32_Sym)
      || sh_size % sh_entsize != 0)
    leave (LEAVE_ELF, "ELF symbol section header invalid");

  // Read symbol table
  size_t n_syms = sh_size / sh_entsize;
  Elf32_Sym *symtab = get_mem ((unsigned) n_syms, sizeof (Elf32_Sym),
                               "ELF symbol table");
  if (fseek (f, sh_offset, SEEK_SET) != 0
      || fread (symtab, sizeof (Elf32_Sym), n_syms, f) != n_syms)
    leave (LEAVE_ELF, "ELF symbol table truncated");

  // Read string table section header
  if (sh_link >= e_shnum)
    leave (LEAVE_ELF, "ELF section header truncated");

  // Read string table
  char *strtab = load_string_table (f, &shdr[sh_link], &sh_size,
                                    "ELF string table");

  sim.set_elf_string_table (strtab, (size_t) sh_size, (unsigned) n_syms);

  // Iterate all symbols
  for (size_t n = 0; n < n_syms; n++)
    {
      const Elf32_Sym *sym = symtab + n;
      int type = ELF32_ST_TYPE (sym->st_info);
      size_t name = (size_t) get_elf32_word (&sym->st_name);
      int shndx = get_elf32_half (&sym->st_shndx);
      if (name >= sh_size)
        leave (LEAVE_ELF, "ELF string table too short");

      unsigned flags = 0;
      if (type == STT_NOTYPE && shndx < SHN_LORESERVE)
        {
          if (shndx >= e_shnum)
            leave (LEAVE_ELF, "ELF section header truncated");
          Elf32_Word sh_type = get_elf32_word (&shdr[shndx].sh_type);
          if (sh_type == SHT_PROGBITS)
            flags = get_elf32_word (&shdr[shndx].sh_flags);
        }
      if (type == STT_FUNC || (flags & SHF_EXEC))
        {
          int value = get_elf32_word (&sym->st_value);
          sim.set_elf_function_symbol (value, name, type == STT_FUNC);
        }
      else if (type == STT_OBJECT)
        {
          int value = get_elf32_word (&sym->st_value);
          sim.set_elf_object_symbol (value, name);
        }
    }

  free (symtab);

  sim.finish_elf_string_table();
}


static void
decode_avr_deviceinfo (avr_deviceinfo_t *info)
{
  Elf32_Word *pin = (Elf32_Word*) info;
  for (size_t i = 0; i < sizeof (*info) / sizeof (Elf32_Word); ++i)
    pin[i] = get_elf32_word (& pin[i]);
}


// Read and decode a .note.gnu.avr.deviceinfo note.
// Return TRUE on success.
static bool
load_deviceinfo_note (FILE *f, const Elf32_Shdr *shdr)
{
  Elf32_Word sh_type = get_elf32_word (&shdr->sh_type);
  if (sh_type != SHT_NOTE)
    leave (LEAVE_FATAL, "expecting a %s section header", s_SHT[SHT_NOTE]);

  // Read the head of the note that contains length information about the
  // fields that follow.
  Elf32_Off sh_offset = get_elf32_word (&shdr->sh_offset);
  Elf32_Nhdr nhdr;
  if (fseek (f, sh_offset, SEEK_SET) != 0
      || fread (&nhdr, sizeof (nhdr), 1, f) != 1)
    leave (LEAVE_ELF, "ELF note header truncated");

  Elf32_Word n_namesz = get_elf32_word (&nhdr.n_namesz);
  Elf32_Word n_descsz = get_elf32_word (&nhdr.n_descsz);
  // Elf32_Word n_type = get_elf32_word (&nhdr.n_type);

  char name[n_namesz];
  if (fread (name, n_namesz, 1, f) != 1)
    leave (LEAVE_ELF, "ELF note name truncated");
  if (! str_eq (name, "AVR"))
    return false;

  avr_deviceinfo_t *info = & avr_deviceinfo;

  if (n_descsz <= sizeof (avr_deviceinfo_t))
    leave (LEAVE_ELF, "ELF note descript truncated");
  if (fread (info, sizeof (avr_deviceinfo_t), 1, f) != 1)
    leave (LEAVE_ELF, "ELF note descript truncated");
  decode_avr_deviceinfo (info);

  char info_strtab[n_descsz - sizeof (avr_deviceinfo_t)];
  if (fread (&info_strtab, sizeof (info_strtab), 1, f) != 1
      || info_strtab[sizeof (info_strtab) - 1] != '\0')
    leave (LEAVE_ELF, "ELF note descript strtab truncated");

  if (strlen (info_strtab + info->devname_offset) < sizeof (avr_devicename))
    strcpy (avr_devicename, info_strtab + info->devname_offset);

  if (options.do_verbose)
    {
      printf (">>> Load %s %s: mcu=\"%s\": Flash 0x%x -- 0x%x-1",
              s_SHT[SHT_NOTE], NOTE_AVR_DEVICEINFO, avr_devicename,
              (unsigned) info->flash_start, (unsigned) info->flash_end);
      if (info->flash_start == 0  && info->flash_end % 1024 == 0)
        printf (" = %u KiB\n", (unsigned) info->flash_end / 1024);
      else
        printf (" = %u B\n", (unsigned) info->flash_end);
    }

  return true;
}


// Load symbols from section .symtab if LOAD_SYMTAB_P, i.e. if this
// is avrtest_log.  As an aside, set `have_strtab' and `have_deviceinfo'.
// `avr_deviceinfo' is read from NOTE section .note.gnu.avr.deviceinfo as
// provided by AVR-LibC via crt<mcu>.o from crt1/gcrt1.S.
static void
load_sections (FILE *f, const Elf32_Ehdr *ehdr, bool load_symtab_p)
{
  char *shstrtab = NULL;
  Elf32_Word e_shoff = get_elf32_word (&ehdr->e_shoff);
  Elf32_Half e_shnum = get_elf32_half (&ehdr->e_shnum);
  Elf32_Half e_shentsize = get_elf32_half (&ehdr->e_shentsize);
  Elf32_Half e_shstrndx = get_elf32_half (&ehdr->e_shstrndx);

  // Read section headers
  if (e_shentsize != sizeof (Elf32_Shdr))
    leave (LEAVE_ELF, "ELF section headers invalid");
  Elf32_Shdr shdr[e_shnum];
  if (fseek (f, e_shoff, SEEK_SET) != 0
      || fread (shdr, sizeof (Elf32_Shdr), e_shnum, f) != e_shnum)
    leave (LEAVE_ELF, "ELF section headers truncated");

  for (int n = 0; n < e_shnum; n++)
    {
      Elf32_Word sh_type = get_elf32_word (&shdr[n].sh_type);

      if (load_symtab_p
          && sh_type == SHT_SYMTAB)
        {
          load_symbol_table (f, ehdr, shdr, n);
          have_strtab = true;
        }

      // Search for note section ".note.gnu.avr.deviceinfo".
      if (sh_type == SHT_NOTE)
        {
          // Need section header string tab which holds the section names.
          if (! shstrtab
              && e_shstrndx < e_shnum)
            {
              Elf32_Word sz;
              shstrtab = load_string_table (f, shdr + e_shstrndx, &sz,
                                            "ELF section header string table");
            }
          if (shstrtab)
            {
              Elf32_Word sh_name = get_elf32_word (&shdr[n].sh_name);
              const char *name = shstrtab + sh_name;
              if (str_eq (name, NOTE_AVR_DEVICEINFO))
                have_deviceinfo = load_deviceinfo_note (f, shdr + n);
            }
        }
    }

  free (shstrtab);
}


/* Check arch supplied by command line options in arch against
   the arch of the ELF file.  */

static void
check_arch (int elf_arch)
{
  // Check that simulation is consistent with is_xmega and is_tiny.

  bool elf_tiny = elf_arch == 100;
  bool elf_xmega = elf_arch >= 102;

  const char *target
    = elf_tiny ? "Reduced Tiny AVR"
    : elf_xmega ? "Xmega AVR"
    : "Classic AVR";

  const char *prog
    = elf_tiny ? "avrtest-tiny"
    : elf_xmega ? "avrtest-xmega"
    : "avrtest";

  const char *l = is_avrtest_log ? "_log" : "";
  char mcu[40] = { 0 };
  if (*avr_devicename)
    sprintf (mcu, " \"%s\"", avr_devicename);

  if (elf_tiny != is_tiny
      || elf_xmega != is_xmega)
    leave (LEAVE_USAGE, "ELF file was generated for %s (avr:%d)%s, use %s%s"
           " for simulation", target, elf_arch, mcu, prog, l);

  // Check that simulation is consistent with PC size of 2 or 3 bytes.

  bool elf_pc_3bytes = elf_arch == 6 || elf_arch >= 106;
  int n_pc = 2 + elf_pc_3bytes;

  if (elf_pc_3bytes != arch.pc_3bytes)
    leave (LEAVE_USAGE, "ELF file was generated for AVR core with %d-byte PC"
           " (avr:%d)%s, but simulating for -mmcu=%s with a %d-byte PC",
           n_pc, elf_arch, mcu, arch.name, 5 - n_pc);

  // Check that simulation is consistent with Flash seen in RAM address-space.

  bool elf_pm_off = elf_tiny || elf_arch == 103;
  bool pm_off = !! arch.flash_pm_offset;
  static const char* const ro[] = { ".rodata in RAM", ".rodata in Flash" };

  if (elf_pm_off != pm_off)
    leave (LEAVE_USAGE, "ELF file was generated for AVR core with %s"
           " (avr:%d)%s, but simulating for -mmcu=%s with %s",
           ro[elf_pm_off], elf_arch, mcu, arch.name, ro[pm_off]);
}


// Try to guess the MEMORY region name like "text".
static const char*
phdr_name (Elf32_Addr lma, Elf32_Addr vma, Elf32_Word flags)
{
  bool r  = (flags & PF_R) && ! (flags & PF_W) && ! (flags & PF_X);
  bool rw = (flags & PF_R) && (flags & PF_W) && ! (flags & PF_X);
  bool rx = (flags & PF_R) && ! (flags & PF_W) && (flags & PF_X);
  return 0 ? ""
    : lma < DATA_VADDR && vma < DATA_VADDR && rx ? "text"
    : vma >= DATA_VADDR && vma <= DATA_VADDR_END && rw ? "data"
    : vma >= EEPROM_VADDR && vma <= EEPROM_VADDR_END ? "eeprom"
    : vma >= 0x820000 && vma <= 0x82ffff ? "fuse"
    : vma >= 0x830000 && vma <= 0x83ffff ? "lock"
    : vma >= 0x840000 && vma <= 0x84ffff ? "signature"
    : vma >= 0x850000 && vma <= 0x85ffff ? "user_signatures"
    : lma < DATA_VADDR && lma != vma && r ? "rodata"
    : "(unknown)";
}


// Turn phdr flags into a quoted string.
static const char*
phdr_flags_str (Elf32_Word flags)
{
  static char str[10];
  char *s = str;
  *s++ = '"';
  if (flags & PF_R) *s++ = 'r';
  if (flags & PF_W) *s++ = 'w';
  if (flags & PF_X) *s++ = 'x';
  *s++ = '"';
  *s++ = '\0';
  return str;
}


static void
load_elf (FILE *f, byte *flash, byte *ram, byte *eeprom)
{
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdr[16];

  rewind (f);
  if (fread (&ehdr, sizeof (ehdr), 1, f) != 1)
    leave (LEAVE_ELF, "can't read ELF header");

  if (ehdr.e_ident[EI_CLASS] != ELFCLASS32
      || ehdr.e_ident[EI_DATA] != ELFDATA2LSB
      || ehdr.e_ident[EI_VERSION] != EV_CURRENT)
    leave (LEAVE_ELF, "bad ELF header");

  if (get_elf32_half (&ehdr.e_type) != ET_EXEC
      || get_elf32_half (&ehdr.e_machine) != EM_AVR
      || get_elf32_word (&ehdr.e_version) != EV_CURRENT
      || get_elf32_half (&ehdr.e_phentsize) != sizeof (Elf32_Phdr))
    leave (LEAVE_ELF, "ELF file is not an AVR executable");

  int elf_arch = EF_AVR_MACH & get_elf32_word (&ehdr.e_flags);

  if (!options.do_entry_point)
    {
      unsigned pc = get_elf32_word (&ehdr.e_entry);
      // Symbol 'start' is special for the GNU linker:  It is one way to
      // specify the entry point.  This would lead to an error if the
      // program defines a global variable 'start', hence only consider
      // ehdr.e_entry as entry point if the address is reasonable and
      // not in .data.
      if (pc < DATA_VADDR)
        {
          program.entry_point = pc;
          cpu.pc = pc / 2;
          if (pc >= MAX_FLASH_SIZE)
            leave (LEAVE_ELF, "ELF entry-point 0x%x it too big", pc);
          else if (pc % 2 != 0)
            leave (LEAVE_ELF, "ELF entry-point 0x%x is odd", pc);
        }
    }

  load_sections (f, &ehdr, is_avrtest_log);

  // Some devices deviate from the 0x8000 default for flash_pm_offset, all
  // in avrxmega3.
  if (elf_arch == 103
      && have_deviceinfo
      && ! options.do_flash_pm_offset)
    {
      const char *devs[] =
        {
          "atmega808", "atmega809", "atmega1608", "atmega1609", "atmega3208",
          "atmega3209", "atmega4808", "atmega4809", NULL
        };
      for (const char **dev = devs; *dev; ++dev)
        if (str_eq (*dev, avr_devicename))
          arch.flash_pm_offset = 0x4000;
    }

  int nbr_phdr = get_elf32_half (&ehdr.e_phnum);
  if ((unsigned) nbr_phdr > sizeof (phdr) / sizeof (*phdr))
    leave (LEAVE_ELF, "ELF file contains too many PHDR");

  if (fseek (f, get_elf32_word (&ehdr.e_phoff), SEEK_SET) != 0)
    leave (LEAVE_ELF, "ELF file truncated");
  size_t res = fread (phdr, sizeof (Elf32_Phdr), nbr_phdr, f);
  if (res != (size_t) nbr_phdr)
    leave (LEAVE_ELF, "can't read PHDRs of ELF file");

  for (int i = 0; i < nbr_phdr; i++)
    {
      Elf32_Addr addr = get_elf32_word (&phdr[i].p_paddr);
      Elf32_Addr vaddr = get_elf32_word (&phdr[i].p_vaddr);
      Elf32_Word filesz = get_elf32_word (&phdr[i].p_filesz);
      Elf32_Word memsz = get_elf32_word (&phdr[i].p_memsz);
      Elf32_Word flags = get_elf32_word (&phdr[i].p_flags);

      if (get_elf32_word (&phdr[i].p_type) != PT_LOAD)
        continue;
      if (filesz == 0)
        continue;

      if (options.do_verbose)
        printf (">>> Load PHDR 0x%06x -- 0x%06x (vaddr = 0x%06x) %-5s %s\n",
                (unsigned) addr, (unsigned) (addr + memsz - 1),
                (unsigned) vaddr, phdr_flags_str (flags),
                phdr_name (addr, vaddr, flags));

      if (addr < DATA_VADDR
          && addr + memsz > MAX_FLASH_SIZE)
        leave (LEAVE_ELF,
               "program is too big to fit in flash");
      if (fseek (f, get_elf32_word (&phdr[i].p_offset), SEEK_SET) != 0)
        leave (LEAVE_ELF, "ELF file truncated");

      program.n_bytes += filesz;

      // Read to eeprom
      if (vaddr >= EEPROM_VADDR
          && vaddr <= EEPROM_VADDR_END)
        {
          addr -= EEPROM_VADDR;
          if (addr + filesz > MAX_EEPROM_SIZE)
            leave (LEAVE_ELF, ".eeprom too big to fit in memory");
          if (fread (eeprom + addr, filesz, 1, f) != 1)
            leave (LEAVE_ELF, "ELF file truncated");
          continue;
        }

      // Skip anything that does not go into flash memory
      if (addr >= DATA_VADDR)
        continue;

      // Read to Flash
      if (fread (flash + addr, filesz, 1, f) != 1)
        leave (LEAVE_ELF, "ELF file truncated");

      bool is_data_for_sram_init = (vaddr >= DATA_VADDR
                                    && vaddr + filesz -1 <= DATA_VADDR_END);
      if (arch.flash_pm_offset)
        {
          if (options.do_verbose
              && (addr + memsz + arch.flash_pm_offset <= 0x10000
                  || !is_data_for_sram_init))
            {
              printf (">>> CopyFlash 0x%06x -- 0x%06x to RAM 0x%04x -- 0x%04x"
                      "\n", (unsigned) addr, (unsigned) (addr + memsz - 1),
                      (unsigned) (addr + arch.flash_pm_offset),
                      (unsigned) (addr + arch.flash_pm_offset + memsz - 1));
            }

          if (addr + memsz + arch.flash_pm_offset <= 0x10000)
            {
              // Showing flash in RAM is possible; just copy.  This is faster
              // than special-casing LDS and LD*.  The downside of just
              // copying is that target code can write to .rodata and it
              // will be harder to debug programs that fail due to that.
              memcpy (ram + addr + arch.flash_pm_offset, flash + addr, memsz);
            }
          else if (is_data_for_sram_init)
            {
              // This occurs only if .data is big and hence its initializer,
              // too.  There is no need for the initializer to be seen in
              // RAM as the startup-code in libgcc's __do_copy_data uses
              // LPM to read it.  Moreover, this case cannot occur on a real
              // device as the biggest "avrxmega3" features much less RAM
              // than supplied by attiny3216-sim.exp.  This is also the reason
              // for why we use 0xffff as flash_addr_mask and not 0x7fff.
              printf (">>> Skipped CopyFlash, PHDR only needed to"
                      " initialize .data, and 0x%06x exceeds 0xffff\n",
                      (unsigned) (addr + memsz + arch.flash_pm_offset));
            }
          else
            leave (LEAVE_ELF, "program is too large to be seen in RAM");
        }

      // Also copy in SRAM
      if (options.do_initialize_sram
          && is_data_for_sram_init)
        memcpy (ram + vaddr - DATA_VADDR, flash + addr, filesz);

      if ((unsigned) (addr + memsz) > program.size)
        program.size = addr + memsz;

      if (flags & PF_X)
        {
          if ((unsigned) addr < program.code_start)
            program.code_start = addr;
          if ((unsigned) (addr + memsz - 1) > program.code_end)
            program.code_end = addr + memsz - 1;
        }
    } // for PHDR

  check_arch (elf_arch);
}

void
load_to_flash (const char *filename, byte *flash, byte *ram, byte *eeprom)
{
  char buf[EI_NIDENT];

  program.code_start = -1U;

  FILE *fp = fopen (filename, "rb");
  if (!fp)
    leave (LEAVE_FOPEN, "can't find or read program file");

  size_t len = fread (buf, 1, sizeof (buf), fp);
  if (len == sizeof (buf)
      && buf[0] == 0x7f
      && buf[1] == 'E'
      && buf[2] == 'L'
      && buf[3] == 'F')
    {
      load_elf (fp, flash, ram, eeprom);
    }
  else
    {
      rewind (fp);
      program.size = program.n_bytes = fread (flash, 1, MAX_FLASH_SIZE, fp);
      program.code_start = 0;
      program.code_end = program.size - 1;
    }
  fclose (fp);

  if (options.do_size == -1)
    // Ignore info from .note.gnu.avr.deviceinfo even if we have it.
    program.pc_mask = arch.flash_addr_mask >> 1;
  else if (options.do_size)
    // -s SIZE takes precedence over deviceinfo.
    program.pc_mask = (unsigned) options.do_size / 2 - 1;
  else if (have_deviceinfo
           && 0 == avr_deviceinfo.flash_start
           // flash_end is an integral power of 2.
           && exact_log2 (avr_deviceinfo.flash_end) >= 0)
    program.pc_mask = (unsigned) avr_deviceinfo.flash_end / 2 - 1;
  else
    program.pc_mask = arch.flash_addr_mask >> 1;

  unsigned max_size = (program.pc_mask + 1) << 1;
  if (program.size > max_size)
    {
      leave (LEAVE_ELF, "program is too large (size: %"PRIu32
             ", max: %u)", program.size, max_size);
    }

  if (is_avrtest_log && !have_strtab)
    {
      static char stab[1];
      sim.set_elf_string_table (stab, 1, 0);
      sim.finish_elf_string_table();
    }
}


// Opcodes with no arguments except NOP       1001 010~ ~~~~ ~~~~
static const byte avr_op_16_index[1 + 0x1ff] = {
  [0x9598 ^ 0x9400] = ID_BREAK,  // 1001 0101 1001 1000 | BREAK
  [0x9519 ^ 0x9400] = ID_EICALL, // 1001 0101 0001 1001 | EICALL
  [0x9419 ^ 0x9400] = ID_EIJMP,  // 1001 0100 0001 1001 | EIJMP
  [0x95D8 ^ 0x9400] = ID_ELPM,   // 1001 0101 1101 1000 | ELPM
  [0x95F8 ^ 0x9400] = ID_ESPM,   // 1001 0101 1111 1000 | ESPM
  [0x9509 ^ 0x9400] = ID_ICALL,  // 1001 0101 0000 1001 | ICALL
  [0x9409 ^ 0x9400] = ID_IJMP,   // 1001 0100 0000 1001 | IJMP
  [0x95C8 ^ 0x9400] = ID_LPM,    // 1001 0101 1100 1000 | LPM
  [0x9508 ^ 0x9400] = ID_RET,    // 1001 0101 0000 1000 | RET
  [0x9518 ^ 0x9400] = ID_RETI,   // 1001 0101 0001 1000 | RETI
  [0x9588 ^ 0x9400] = ID_SLEEP,  // 1001 0101 1000 1000 | SLEEP
  [0x95E8 ^ 0x9400] = ID_SPM,    // 1001 0101 1110 1000 | SPM
  [0x95A8 ^ 0x9400] = ID_WDR,    // 1001 0101 1010 1000 | WDR
};

// Opcodes unique with upper 6 bits           ~~~~ ~~rd dddd rrrr
static const byte avr_op_6_index[1 << 6] = {
  [0x1C00 >> 10] = ID_ADC,       // 0001 11rd dddd rrrr | ADC, ROL
  [0x0C00 >> 10] = ID_ADD,       // 0000 11rd dddd rrrr | ADD, LSL
  [0x2000 >> 10] = ID_AND,       // 0010 00rd dddd rrrr | AND, TST
  [0x1400 >> 10] = ID_CP,        // 0001 01rd dddd rrrr | CP
  [0x0400 >> 10] = ID_CPC,       // 0000 01rd dddd rrrr | CPC
  [0x1000 >> 10] = ID_CPSE,      // 0001 00rd dddd rrrr | CPSE
  [0x2400 >> 10] = ID_EOR,       // 0010 01rd dddd rrrr | EOR, CLR
  [0x2C00 >> 10] = ID_MOV,       // 0010 11rd dddd rrrr | MOV
  [0x9C00 >> 10] = ID_MUL,       // 1001 11rd dddd rrrr | MUL
  [0x2800 >> 10] = ID_OR,        // 0010 10rd dddd rrrr | OR
  [0x0800 >> 10] = ID_SBC,       // 0000 10rd dddd rrrr | SBC
  [0x1800 >> 10] = ID_SUB,       // 0001 10rd dddd rrrr | SUB
};

// Unique with upper 7, lower 4 bits              1001 0~~d dddd ~~~~
static const byte avr_op_74_index[1 + 0x7ff] = {
  [0x9405 ^ 0x9000] = ID_ASR,        // 1001 010d dddd 0101 | ASR
  [0x9400 ^ 0x9000] = ID_COM,        // 1001 010d dddd 0000 | COM
  [0x940A ^ 0x9000] = ID_DEC,        // 1001 010d dddd 1010 | DEC
  [0x9006 ^ 0x9000] = ID_ELPM_Z,     // 1001 000d dddd 0110 | ELPM
  [0x9007 ^ 0x9000] = ID_ELPM_Z_incr,// 1001 000d dddd 0111 | ELPM
  [0x9403 ^ 0x9000] = ID_INC,        // 1001 010d dddd 0011 | INC
  [0x9000 ^ 0x9000] = ID_LDS,        // 1001 000d dddd 0000 | LDS
  [0x900C ^ 0x9000] = ID_LD_X,       // 1001 000d dddd 1100 | LD
  [0x900E ^ 0x9000] = ID_LD_X_decr,  // 1001 000d dddd 1110 | LD
  [0x900D ^ 0x9000] = ID_LD_X_incr,  // 1001 000d dddd 1101 | LD
  [0x900A ^ 0x9000] = ID_LD_Y_decr,  // 1001 000d dddd 1010 | LD
  [0x9009 ^ 0x9000] = ID_LD_Y_incr,  // 1001 000d dddd 1001 | LD
  [0x9002 ^ 0x9000] = ID_LD_Z_decr,  // 1001 000d dddd 0010 | LD
  [0x9001 ^ 0x9000] = ID_LD_Z_incr,  // 1001 000d dddd 0001 | LD
  [0x9004 ^ 0x9000] = ID_LPM_Z,      // 1001 000d dddd 0100 | LPM
  [0x9005 ^ 0x9000] = ID_LPM_Z_incr, // 1001 000d dddd 0101 | LPM
  [0x9406 ^ 0x9000] = ID_LSR,        // 1001 010d dddd 0110 | LSR
  [0x9401 ^ 0x9000] = ID_NEG,        // 1001 010d dddd 0001 | NEG
  [0x900F ^ 0x9000] = ID_POP,        // 1001 000d dddd 1111 | POP

  [0x9204 ^ 0x9000] = ID_XCH,        // 1001 001d dddd 0100 | XCH
  [0x9205 ^ 0x9000] = ID_LAS,        // 1001 001d dddd 0101 | LAS
  [0x9206 ^ 0x9000] = ID_LAC,        // 1001 001d dddd 0110 | LAC
  [0x9207 ^ 0x9000] = ID_LAT,        // 1001 001d dddd 0111 | LAT

  [0x920F ^ 0x9000] = ID_PUSH,       // 1001 001d dddd 1111 | PUSH
  [0x9407 ^ 0x9000] = ID_ROR,        // 1001 010d dddd 0111 | ROR
  [0x9200 ^ 0x9000] = ID_STS,        // 1001 001d dddd 0000 | STS
  [0x920C ^ 0x9000] = ID_ST_X,       // 1001 001d dddd 1100 | ST
  [0x920E ^ 0x9000] = ID_ST_X_decr,  // 1001 001d dddd 1110 | ST
  [0x920D ^ 0x9000] = ID_ST_X_incr,  // 1001 001d dddd 1101 | ST
  [0x920A ^ 0x9000] = ID_ST_Y_decr,  // 1001 001d dddd 1010 | ST
  [0x9209 ^ 0x9000] = ID_ST_Y_incr,  // 1001 001d dddd 1001 | ST
  [0x9202 ^ 0x9000] = ID_ST_Z_decr,  // 1001 001d dddd 0010 | ST
  [0x9201 ^ 0x9000] = ID_ST_Z_incr,  // 1001 001d dddd 0001 | ST
  [0x9402 ^ 0x9000] = ID_SWAP,       // 1001 010d dddd 0010 | SWAP
};


#define DO_SKIP(ID)   \
  do {                \
      index = ID;     \
      goto do_skip;   \
    } while (0)

static int
decode_opcode (decoded_t *d, unsigned opcode1, unsigned opcode2)
{
  byte index = 0;

  // opcodes with no operands

  if (0x0000 == opcode1)
    return ID_NOP;                     // 0000 0000 0000 0000 | NOP

  // All other opcodes with no operands are:        1001 010x xxxx xxxx
  if ((opcode1 ^ 0x9400) <= 0x1ff
      && ((index = avr_op_16_index[opcode1 ^ 0x9400])))
    return index;

  // opcodes with two 5-bit register operands       xxxx xxrd dddd rrrr

  if ((index = avr_op_6_index[opcode1 >> 10]))
    {
      d->op2 = (opcode1 & 0x0F) | ((opcode1 >> 5) & 0x0010);
      d->op1 = (opcode1 >> 4) & 0x1F;
      if (ID_ADD == index && d->op1 == d->op2)  return ID_LSL;
      if (ID_ADC == index && d->op1 == d->op2)  return ID_ROL;
      if (ID_EOR == index && d->op1 == d->op2)  return ID_CLR;
      if (ID_AND == index && d->op1 == d->op2)  return ID_TST;
      if (ID_CPSE == index) DO_SKIP (ID_CPSE);
      return index;
    }

  // opcodes with a single register operand         1001 0xxd dddd xxxx

  unsigned decode = opcode1 & ~(mask_Rd_5);
  if ((decode ^ 0x9000) <= 0x7ff
      && ((index = avr_op_74_index[decode ^ 0x9000])))
    {
      // op2 only used with LDS, STS
      byte rd = (opcode1 >> 4) & 0x1F;
      d->op2 = opcode2;
      d->op1 = rd;
      int ill = 0;
      switch (index)
        {
        case ID_LPM_Z_incr: case ID_ELPM_Z_incr:
        case ID_LD_Z_incr:  case ID_ST_Z_incr:
        case ID_LD_Z_decr:  case ID_ST_Z_decr: ill = 3u << REGZ; break;
        case ID_LD_Y_incr:  case ID_ST_Y_incr:
        case ID_LD_Y_decr:  case ID_ST_Y_decr: ill = 3u << REGY; break;
        case ID_LD_X_incr:  case ID_ST_X_incr:
        case ID_LD_X_decr:  case ID_ST_X_decr: ill = 3u << REGX; break;
        }
      if (ill & (1u << rd))
        {
          d->op1 = index;
          d->op2 = opcode1;
          return ID_UNDEF;
        }

      return index;
    }

  // opcodes with a register (Rd) and a constant data (K) as operands
  unsigned hi4 = opcode1 >> 12;
  if (0x40f8 & (1 << hi4)) // 0100 0000 1111 1000
    {
      d->op1 = ((opcode1 >> 4) & 0xF) | 0x10;
      d->op2 = (opcode1 & 0x0F) | ((opcode1 >> 4) & 0x00F0);

      switch (hi4) {
      case 0x3: return ID_CPI;     // 0011 KKKK dddd KKKK | CPI
      case 0x4: return ID_SBCI;    // 0100 KKKK dddd KKKK | SBCI
      case 0x5: return ID_SUBI;    // 0101 KKKK dddd KKKK | SUBI
      case 0x6: return ID_ORI;     // 0110 KKKK dddd KKKK | SBR or ORI
      case 0x7: return ID_ANDI;    // 0111 KKKK dddd KKKK | CBR or ANDI
      case 0xE: return ID_LDI;     // 1110 KKKK dddd KKKK | LDI or SER
      }
    }

  unsigned hi5 = opcode1 >> 11;
  if (hi5 == (0xf800 >> 11))
    {
      // opcodes with a register (Rd) and a register bit number (b)
      d->op1 = (opcode1 >> 4) & 0x1F;
      d->op2 = 1 << (opcode1 & 0x0007);
      decode = opcode1 & ~(mask_Rd_5 | mask_reg_bit);
      switch (decode) {
      case 0xF800: return ID_BLD;       // 1111 100d dddd 0bbb | BLD
      case 0xFA00: return ID_BST;       // 1111 101d dddd 0bbb | BST
      case 0xFC00: DO_SKIP (ID_SBRC);   // 1111 110d dddd 0bbb | SBRC
      case 0xFE00: DO_SKIP (ID_SBRS);   // 1111 111d dddd 0bbb | SBRS
      }
    }

  if (hi5 == (0xf000 >> 11))
    {
      /* opcodes with a relative 7-bit address (k) and a register bit
         number (b) as operands */
      d->op2 = 1 << (opcode1 & 0x7);  // bit mask
      unsigned h = (opcode1 >> 3) & 0x7F;
      d->op1 = h & (1 << 6);          // sign
      d->op1 = h | -d->op1;           // jump offset
      return opcode1 & (1 << 10)
        ? ID_BRBC                     // 1111 01kk kkkk kbbb | BRBC
        : ID_BRBS;                    // 1111 00kk kkkk kbbb | BRBS
    }

  if ((opcode1 & 0xd000) == 0x8000)
    {
      /* opcodes with a 6-bit address displacement (q) and a register (Rd)
         as operands */
      d->op1 = (opcode1 >> 4) & 0x1F;
      d->op2 = ((opcode1 & 0x7)
                | ((opcode1 >> 7) & 0x18)
                | ((opcode1 >> 8) & 0x20));
      decode = opcode1 & ~(mask_Rd_5 | mask_q_displ);
      if (!is_tiny || d->op2 == 0)
        switch (decode) {
        case 0x8008: return ID_LDD_Y;   // 10q0 qq0d dddd 1qqq | LDD
        case 0x8000: return ID_LDD_Z;   // 10q0 qq0d dddd 0qqq | LDD
        case 0x8208: return ID_STD_Y;   // 10q0 qq1d dddd 1qqq | STD
        case 0x8200: return ID_STD_Z;   // 10q0 qq1d dddd 0qqq | STD
        }
    }

  if (is_tiny && hi4 == 0xa)
    {
      /* TINY opcodes with a 7-bit address and a register (4-bit Rd)
         as operands */
      d->op1 = 16 + ((opcode1 >> 4) & 0xF);
      d->op2 = ((opcode1 & 0xf)
                | ((opcode1 >> 5) & 0x30)
                | ((opcode1 & 0x100) ? 0x40 : 0x80));
      switch (hi5) {
      case 0xa000 >> 11: return ID_LDS1; // 1010 0kkk dddd kkkk | LDS (Tiny)
      case 0xa800 >> 11: return ID_STS1; // 1010 1kkk dddd kkkk | STS (Tiny)
      }
    }

  unsigned hi7 = opcode1 >> 9;
  if (hi7 == (0x9400 >> 9))
    {
      // opcodes with a absolute 22-bit address (k) operand
      d->op1 = (opcode1 & 1) | ((opcode1 >> 3) & 0x3E);
      d->op2 = opcode2;
      decode = opcode1 & ~(mask_k_22);
      switch (decode) {
      case 0x940E: return ID_CALL;        // 1001 010k kkkk 111k | CALL
      case 0x940C: return ID_JMP;         // 1001 010k kkkk 110k | JMP
      }
    }

  unsigned hi8 = opcode1 >> 8;
  if (hi8 == (0x9400 >> 8))
    {
      // opcode1 with a sreg bit select (s) operand
      d->op1 = 1 << ((opcode1 >> 4) & 0x07);
      decode = opcode1 & ~(mask_sreg_bit);
      switch (decode) {
      // BCLR takes care of CL{C,Z,N,V,S,H,T,I}
      // BSET takes care of SE{C,Z,N,V,S,H,T,I}
      case 0x9488: return ID_BCLR;        // 1001 0100 1sss 1000 | BCLR
      case 0x9408: return ID_BSET;        // 1001 0100 0sss 1000 | BSET
      case 0x948b: d->op1 |= 0x8; // fallthrough
      case 0x940b: return ID_DES;         // 1001 0100 KKKK 1011 | DES
      }
    }

  if (hi7 == (0x9600 >> 9))
    {
      // opcodes with a 6-bit constant (K) and a register (Rd) as operands
      d->op1 = ((opcode1 >> 3) & 0x06) + 24;
      d->op2 = (opcode1 & 0xF) | ((opcode1 >> 2) & 0x30);
      decode = opcode1 & ~(mask_K_6 | mask_Rd_2);
      switch (decode) {
      case 0x9600: return ID_ADIW;          // 1001 0110 KKdd KKKK | ADIW
      case 0x9700: return ID_SBIW;          // 1001 0111 KKdd KKKK | SBIW
      }
    }

  unsigned hi6 = opcode1 >> 10;
  if (hi6 == (0x9800 >> 10))
    {
      // opcodes with a 5-bit IO Addr (A) and register bit number (b)
      d->op1 = ((opcode1 >> 3) & 0x1F) + io_base;
      d->op2 = 1 << (opcode1 & 0x0007);
      switch (opcode1 >> 8) {
      case 0x9800 >> 8: return ID_CBI;         // 1001 1000 AAAA Abbb | CBI
      case 0x9A00 >> 8: return ID_SBI;         // 1001 1010 AAAA Abbb | SBI
      case 0x9900 >> 8: DO_SKIP (ID_SBIC);     // 1001 1001 AAAA Abbb | SBIC
      case 0x9B00 >> 8: DO_SKIP (ID_SBIS);     // 1001 1011 AAAA Abbb | SBIS
      }
    }

  if (hi4 == (0xb800 >> 12))
    {
      // opcodes with a 6-bit IO Addr (A) and register (Rd) as operands
      d->op1 = ((opcode1 >> 4) & 0x1F);
      d->op2 = ((opcode1 & 0x0F) | ((opcode1 >> 5) & 0x30)) + io_base;
      switch (opcode1 >> 11) {
      case 0xB000 >> 11: return ID_IN;          // 1011 0AAd dddd AAAA | IN
      case 0xB800 >> 11: return ID_OUT;         // 1011 1AAd dddd AAAA | OUT
      }
    }

  if (hi4 == 0xc || hi4 == 0xd)
    {
      // opcodes with a relative 12-bit address (k) operand
      d->op2 = opcode1 & 0x800;
      d->op2 = (opcode1 & 0x7FF) | -d->op2;
      switch (hi4) {
      case 0xD000 >> 12: return ID_RCALL;   // 1101 kkkk kkkk kkkk | RCALL
      case 0xC000 >> 12: return ID_RJMP;    // 1100 kkkk kkkk kkkk | RJMP
      }
    }

  // opcodes with two 4-bit register (Rd and Rr) operands
  decode = opcode1 & ~(mask_Rd_4 | mask_Rr_4);
  switch (decode) {
  case 0x0100:
    d->op1 = ((opcode1 >> 4) & 0x0F) << 1;
    d->op2 = (opcode1 & 0x0F) << 1;
    return ID_MOVW;        // 0000 0001 dddd rrrr | MOVW
  case 0x0200:
    d->op1 = ((opcode1 >> 4) & 0x0F) | 0x10;
    d->op2 = (opcode1 & 0x0F) | 0x10;
    return ID_MULS;        // 0000 0010 dddd rrrr | MULS
  }

  if (hi8 == 3)
    {
      // opcodes with two 3-bit register (Rd and Rr) operands
      d->op1 = ((opcode1 >> 4) & 0x07) | 0x10;
      d->op2 = (opcode1 & 0x07) | 0x10;
      decode = opcode1 & ~(mask_Rd_3 | mask_Rr_3);
      switch (decode) {
      case 0x0300: return ID_MULSU;   // 0000 0011 0ddd 0rrr | MULSU
      case 0x0308: return ID_FMUL;    // 0000 0011 0ddd 1rrr | FMUL
      case 0x0380: return ID_FMULS;   // 0000 0011 1ddd 0rrr | FMULS
      case 0x0388: return ID_FMULSU;  // 0000 0011 1ddd 1rrr | FMULSU
    }
  }

  d->op1 = IL_ILL;
  d->op2 = 0;
  return ID_ILLEGAL;

  // Map ID_CPSE to ID_CPSE2 if CPSE has to skip 2 words.
  // Dito for SBRC, SBRS, SBIC, SBIS.
do_skip:;

  if (index == ID_CPSE
      && d->op1 == d->op2 && opcode2 == invalid_opcode)
    {
      if (d->op1 < 32)
        have_syscall[d->op1] = true;

      // Always skipping invalid opcode 0xffff represents a syscall
      return ID_SYSCALL;   //  0001 00xX XXXX xxxx | CPSE X X = SYSCALL X
    }

  if (is_tiny)
    return index;

  if ((opcode2 & mask_LDS_STS) == 0x9000)
    return 1 + index;

  if ((opcode2 & mask_JMP_CALL) == 0x940c)
    return 1 + index;

  return index;
}

static void
tiny_opcode_maybe_illegal (decoded_t *d)
{
  switch (d->id)
    {
    default:
      break;

    case ID_LDS:   case ID_JMP:      case ID_MOVW:
    case ID_STS:   case ID_CALL:     case ID_ADIW:
    case ID_MUL:   case ID_FMUL:     case ID_SBIW:
    case ID_MULS:  case ID_FMULS:    case ID_EICALL:
    case ID_MULSU: case ID_FMULSU:   case ID_EIJMP:
    case ID_DES:   case ID_LPM:         case ID_ELPM:
    case ID_XCH:   case ID_LPM_Z:       case ID_ELPM_Z:
    case ID_LAS:   case ID_LPM_Z_incr:  case ID_ELPM_Z_incr:
    case ID_LAC:   case ID_ESPM:
    case ID_LAT:
      d->op1 = IL_ILL;
      d->op2 = 0;
      d->id = ID_ILLEGAL;
      break;
    }
}

void
decode_flash (decoded_t d[], const byte flash[])
{
  unsigned i = program.code_start;
  word opcode1 = flash[i] | (flash[i + 1] << 8);

  for (; i <= program.code_end; i += 2)
    {
      word opcode2 = flash[i + 2] | (flash[i + 3] << 8);
      d[i / 2].id = decode_opcode (&d[i / 2], opcode1, opcode2);
      if (is_tiny)
        tiny_opcode_maybe_illegal (&d[i / 2]);
      opcode1 = opcode2;
    }

  // Allow a PC past the last code address so that no abort occurs
  // when the last instruction is a [R]JMP or RET:  do_step() sets
  // the new PC *before* executing an instruction.
  program.max_pc = 1 + program.code_end / 2;
}
