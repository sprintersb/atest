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

#include <stdbool.h>
#include <stdlib.h>

#include "testavr.h"

enum
  {
    T_MAIN,
    T_EXIT,
    T__EXIT,
    T_ABORT,
    T_LONGJMP,
    T_LAST_BOLD,

    T_ENTRY,
    T_TERMINATE,
    T_PROLOGUE,
    T_EPILOGUE,
    T_SETJMP,
    T_ADDR,
    T_VECTOR,

    T_NONE
  };

enum
{
  // Backtrace to show in case of trouble
  EM_TRACE     = 1 << 0,
  EM_SHOW      = 1 << 1,
  EM_ACCOUNT   = 1 << 2,
  EM_MAIN_RET  = 1 << 3,
  EM_BACK      = 1 << 4,

  EM_DOT_DONE  = 1 << 5,
  EM_DOTTED    = 1 << 6,
  EM_DASHED    = 1 << 7,

  EM_CLEAR     = 1 << 30
};

typedef struct symbol
{
  // string's address, somewhere in string_table.data[]
  const char *name;
  // unique id > 0
  int id;
  // word address in Flash
  unsigned pc;
  // one from the ENUM above.
  int type;
  // from ELF
  int stt, bind;

  struct
  {
    unsigned own, childs;
    bool done, account;
  } cycles;
  // whether this is STT_FUNC
  bool is_func;
  // Graph: node done
  bool dot_done;
  // -graph-base: Graph: starts here
  bool is_base;
  bool account;
  // reserved C symbol
  bool is_reserved, is_reserved_caller;
  // -graph-leaf:
  // whether to handle the (function) symbol as leaf
  bool is_leaf;
  // -graph-sub
  // whether to account costs to the subtree, including reserved names
  bool is_sub;
  bool is_hidden;
  bool is_skip;
} symbol_t;


typedef struct edge
{
  struct edge *next;
  // unique
  int id;
  // egde occurs n-fold
  int n;
  symbol_t *from, *to;
  // number of tail calls
  int n_tail;
  // number of proper calls
  int n_call;
  // total cycles accounted to this edge
  unsigned n_cycles;
  // apprearances in sub-trees (-graph-sub)
  unsigned n_sub;
  // appearances in sub-trees of leaf functions (-graph-leaf)
  unsigned n_leaf;
  // Hilit if program aborts
  int mark;
  const char *s_tail, *s_label;
} edge_t;


typedef struct list
{
  struct list *next, *prev;
  symbol_t *sym;
  edge_t *edge;
  int depth;
  int sp;
  const char *res;
  bool is_leaf, is_sub;
} list_t;


typedef struct
{
  // ID of current instruction.
  int id, old_id;

  symbol_t *entry_point, *base, *prologue_saves, *epilogue_restores;
  symbol_t *setjmp, *longjmp;
  symbol_t *main, *exit, *_exit, *abort;
  // sum of "own" cycles accounted to nodes
  unsigned n_cycles;
  edge_t *entry_edge;
  struct
  {
    int n_call;
    unsigned pc;
  } main_return;
  bool no_startup_cycles;
} graph_t;

typedef struct
{
  bool perf;
  bool logging;
  bool graph, graph_cost;
  bool call_depth;
} need_t;

extern symbol_t* graph_elf_symbol (const char*, size_t, unsigned, bool);
extern void graph_set_string_table (char*, size_t, int);
extern void graph_finish_string_table (void);
extern int graph_update_call_depth (const decoded_t*);
extern void graph_write_dot (void);
extern need_t need;

extern symbol_t *func_sym[MAX_FLASH_SIZE/2];
