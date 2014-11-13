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
#include <stdbool.h>
#include <math.h>

#include "testavr.h"
#include "options.h"
#include "graph.h"

#ifndef AVRTEST_LOG
#error no function herein is needed without AVRTEST_LOG
#endif // AVRTEST_LOG

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
  bool entered;
} graph_t;


// Word address --> string_t that holds the symbol or NULL.
static symbol_t *func_sym[MAX_FLASH_SIZE/2];

#define EPRIM 43
static edge_t *ebucket[EPRIM];

static graph_t graph;

static list_t *ystack = NULL;
static list_t *yend = NULL;
static list_t *yfree = NULL;
static list_t *lnores;

// -graph-leaf: Functions to be treated as leaf functions
static char* const *s_leafs;
static int n_leafs;

// -graph-sub: Functions to be expanded completely
static char* const *s_subs;
static int n_subs;

// -graph-skip: Functions to be ignored
static char* const *s_skips;
static int n_skips;

#define DEBUG_TREE (options.do_debug_tree)

static edge_t*
get_edge (symbol_t *from, symbol_t *to)
{
  static int n_edges;
  unsigned hash = (unsigned) (from->id - to->id) % EPRIM;

  for (edge_t *e = ebucket[hash]; e != NULL; e = e->next)
    if (e->from->id == from->id
        && e->to->id == to->id)
      return e;

  edge_t *e = get_mem (1, sizeof (edge_t), "edge");
  memset (e, 0, sizeof (edge_t));
  e->from = from;
  e->to = to;
  e->id = ++n_edges;
  e->next = ebucket[hash];
  ebucket[hash] = e;

  return e;
}


static void
lmark_edges (list_t *from, list_t *to, unsigned mask)
{
  for (; from; from = from->next)
    {
      if (mask & EM_CLEAR)
        from->edge->mark &= ~mask;
      else
        from->edge->mark |= mask;
      if (from == to)
        break;
    }
}


// Find deepest base or NULL
static list_t*
lfind_base (bool maybe_addr)
{
  list_t *l = NULL;
  for (l = yend; l; l = l->prev)
    if (l->sym->is_base)
      break;

  if (!maybe_addr)
    for (; l && l->next && (l->sym->type & T_ADDR); l = l->next)
      l->edge->mark |= EM_TRACE | EM_SHOW;
  return l;
}


// Find deepest type or NULL
static list_t*
lfind_type (int type)
{
  for (list_t *l = yend; l; l = l->prev)
    if (l->edge->to->type == type)
      return l;
  return NULL;
}


static void
lpush (list_t **head, edge_t *e)
{
  list_t *l = yfree;

  if (l != NULL)
    yfree = l->next;
  else
    l = get_mem (1, sizeof (list_t), "list");

  l->edge = e;
  if (e)
    l->sym = e->to;
  l->prev = NULL;
  l->next = *head;
  if (l->next)
    l->next->prev = l;
  *head = l;
}


/* Remove first elment of list *HEAD and add it to the free list `yfree'.  */

static void
lpop (list_t **head)
{
  list_t *l = *head;

  if (!l)
    return;

  if (l->next)
    l->next->prev = NULL;
  *head = l->next;

  l->next = yfree;
  yfree = l;
}


static inline
bool is_func_prefix (const char *prefix, const char *fun)
{
  size_t len = strlen (prefix);
  return (str_prefix (prefix, fun)
          && (fun[len] == '\0'
              // cover cloned functions
              || fun[len] == '.'));
}


static bool
have_elf_symbol (const char *name)
{
  for (size_t off = 1; off < string_table.size; )
    if (string_table.have[off]
        && str_eq (name, string_table.data + off))
      return true;
    else
      off += 1 + strlen (string_table.data + off);

  return false;
}


static const char *const not_reserved[] =
  {
    // Functions called by inlined standard functions like utoa
    "__itoa",  "itoa",
    "__ltoa",  "ltoa",
    "__utoa",  "utoa",
    "__ultoa", "ultoa",
    "__itoa_ncheck",  "itoa",
    "__ltoa_ncheck",  "ltoa",
    "__utoa_ncheck",  "utoa",
    "__ultoa_ncheck", "ultoa",
    NULL
  };


typedef struct
{
  const char *name;
  int type;
  symbol_t **psym;
} spec_t;


static void
dump_node (const symbol_t *s)
{
  if (s)
    printf (" %s:%d:%d%s%s%s%s%s ", s->name,
            s->is_reserved_caller, s->is_reserved,
            s->is_leaf ? ":L" : "", s->is_sub ? ":S" : "",
            s->cycles.account ? ":A":"", s->is_base ? ":B":"",
            s->is_skip ? ":I":"");
  else
    printf (" (Nnull) ");
}


static void
dump_listel (const list_t *l)
{
  if (l)
    printf (" [%d,%04x]  <--%s%s%s%s ", l->depth, l->sp,
            l->edge->mark & EM_ACCOUNT ? "A":"",
            l->is_leaf ? "L":"", l->is_sub ? "S":"",
            l->edge->mark & EM_SHOW ? "!":"");
  else
    printf (" (Lnull) ");
}

static void
dump_ystack (void)
{
  printf ("/// ");
  for (const list_t *l = ystack; l; l = l->next)
    {
      dump_node (l->sym);
      dump_listel (l);
    }
  printf ("\n");
}


/* Classify symbol according to its name (main, exit, ...) depending
   on some command options.  */

static void
classify_symbol (symbol_t *s, bool is_func)
{
  static const char *res_callers[] =
    { "__utoa_common", "__ultoa_common", NULL };
  const char *name = s->name;

  s->is_reserved = (! options.do_graph_reserved
                    && str_prefix ("__", name)
                    // From ld --wrap
                    && ! str_prefix ("__wrap_", name));

  s->is_reserved_caller = (! options.do_graph_reserved
                           && str_in (name, res_callers));
  s->is_func = is_func;

  static const spec_t special[] =
    {
      { "main",                  T_MAIN,      & graph.main },
      { "exit",                  T_EXIT,      & graph.exit },
      { "_exit",                 T__EXIT,     & graph._exit },
      { "abort",                 T_ABORT,     & graph.abort },
      { "setjmp",                T_SETJMP,    & graph.setjmp },
      { "longjmp",               T_LONGJMP,   & graph.longjmp },
      { "__prologue_saves__",    T_PROLOGUE,  & graph.prologue_saves },
      { "__epilogue_restores__", T_EPILOGUE,  & graph.epilogue_restores },
      { NULL, 0, NULL }
    };

  for (const spec_t *y = special; y->name; y++)
    if (str_eq (y->name, name))
      (* y->psym = s) -> type = y->type;

  (void)
    (options.do_graph_base
     && *options.s_graph_base != '\0'
     && (s->is_base = is_func_prefix (options.s_graph_base, name))
     && (graph.base = s));

  for (int n = 0; n < n_subs && !s->is_sub; n++)
    s->is_sub = is_func_prefix (s_subs[n], name);

  for (int n = 0; n < n_leafs && !s->is_leaf; n++)
    s->is_leaf = is_func_prefix (s_leafs[n], name);

  for (int n = 0; n < n_skips && !s->is_skip; n++)
    s->is_skip = is_func_prefix (s_skips[n], name);

  if (s->is_base || s->is_leaf || s->is_sub)
    s->is_reserved = s->is_reserved_caller = false;
}


static symbol_t*
graph_add_symbol (const char *name, unsigned pc, bool is_func)
{
  static int n_symbols;
  symbol_t *s = get_mem (1, sizeof (symbol_t), "symbol_t");
  memset (s, 0, sizeof (symbol_t));
  s->type = T_NONE;
  s->id = ++ n_symbols;
  s->pc = pc;

  // No name available: use address as name
  if (!(s->name = name))
    {
      char *s_addr;
      s_addr = get_mem (10, sizeof (char), "s_addr");
      sprintf (s_addr, "0x%x", 2 * pc);
      s->name = s_addr;
      s->type = T_ADDR;
    }

  classify_symbol (s, is_func);

  return s;
}


/* Priority of some known symbols as a specific address might be featured
   with mode than one symbol.  */

static int
rate_symbol (const char *s)
{
  static const char *prior[] = { "_exit", "__init", "__bad_interrupt", NULL };
  return str_in (s, prior)
    ? 2
    : s[0] != '_';
}


/* Called from ELF reader as is comes across a new symbol.
   NAME is the name of the symbol and STOFF is its byte offset into
   the string table as supplied with `graph_set_string_table'.
   PC is the word location ans IS_FUNC is the set iff the symbol
   type is STT_FUNC.  */

void
graph_elf_symbol (const char *name, size_t stoff, unsigned pc, bool is_func)
{
  symbol_t *sym = func_sym[pc];
  string_table.have[stoff] = true;

  if (sym
      && rate_symbol (name) <= rate_symbol (sym->name))
    return;

  sym = graph_add_symbol (name, pc, is_func);

  // Collect names that might be remapped to their non-inline originator.
  // We must have all symbols available to decide on this because the
  // non-inline version may also be.
  for (const char* const *r = not_reserved; r[0]; r += 2)
    if (str_eq (name, r[0]))
      {
        lpush (&lnores, NULL);
        lnores->sym = sym;
        lnores->res = r[1];
      }

  func_sym[pc] = sym;
}


/* Called from ELF reader as is comes across the symbol table.  */

void
graph_set_string_table (char *stab, size_t size, int n_entries)
{
  string_table.have = get_mem (size, sizeof (bool), "string_table.have");

  if (options.do_graph_leaf)
    s_leafs = comma_list_to_array (options.s_graph_leaf, &n_leafs);

  if (options.do_graph_sub)
    s_subs = comma_list_to_array (options.s_graph_sub, &n_subs);

  if (options.do_graph_skip)
    s_skips = comma_list_to_array (options.s_graph_skip, &n_skips);
}


/* Called from ELF reader when it has finished traversing the symbol table. */

void
graph_finish_string_table (void)
{
  // remap inlined functions to their original
  while (lnores)
    {
      if (!have_elf_symbol (lnores->res))
        {
          lnores->sym->name = lnores->res;
          lnores->sym->is_func = true;
          lnores->sym->is_reserved = false;
        }
      lpop (&lnores);
    }

  // Add artificial node and edge representing the program entry.
  symbol_t *entry = func_sym[cpu_PC];
  graph.entry_point = graph_add_symbol ("Entry Point", cpu_PC, false);
  graph.entry_point->type = T_ENTRY;
  if (!entry)
    entry = graph_add_symbol (NULL, cpu_PC, false);
  func_sym[cpu_PC] = entry;

  lpush (&ystack, graph.entry_edge = get_edge (graph.entry_point, entry));
  yend = ystack;

  // -graph-base=BASE is turned on but we did not yet see a BASE function.
  // Try special value "0" which stands for program entry and integer
  // values which stand for plain byte addresses.
  if (options.do_graph_base
      && !graph.base)
    {
      char *end;
      unsigned long pc = strtoul (options.s_graph_base, &end, 0) / 2;

      if (str_eq ("0", options.s_graph_base))
        graph.base = entry;
      else if (*options.s_graph_base
               && *end == '\0'
               && pc < MAX_FLASH_SIZE / 2)
        (void)
          ((graph.base = func_sym[pc])
           || (graph.base = func_sym[pc] = graph_add_symbol (NULL, pc, 0)));
    }

  // Still no BASE:  Use main, and if there is no main, use program entry.
  (void)
    (graph.base
     || (graph.base = graph.main)
     || (graph.base = entry));

  graph.base->is_base = true;
  graph.base->is_reserved = graph.base->is_reserved_caller = false;

  graph.entered = true;

  if (DEBUG_TREE)
    printf ("BASE = %s\n", graph.base->name);
}


/* Add CYCLES to L->sym's own costs.  Also add CYCLES to child costs of all
   parents of L until BASE is reached.  */

static void
account (list_t *l, list_t *base, unsigned cycles)
{
  bool own = true;

  for (; l; l = l->next)
    {
      symbol_t *sym = l->sym;

      if (l == base
          && sym->cycles.done)
        return;

      if (sym->cycles.done)
        continue;
      sym->cycles.done = true;

      if (own)
        {
          sym->cycles.account = true;
          sym->cycles.own += cycles;
          graph.n_cycles += cycles;
        }
      else
        sym->cycles.childs += cycles;

      if (DEBUG_TREE)
        printf ("A:%s %s +%d = %d\n", own ? "COST" : "CHLD", sym->name,
                cycles, own ? sym->cycles.own : sym->cycles.childs);

      if (l == base)
        return;

      if (own)
        l->edge->n_cycles += cycles;
      l->edge->mark |= EM_ACCOUNT;
      own = false;
    }
}


/* The call stack has changed.  Assign the cycles accumulated since the last
   update of the call stack to a node appropriate for it.  */

static void
account_cycles (void)
{
  static unsigned cycle;
  unsigned cycles = program.n_cycles - cycle;
  cycle = program.n_cycles;

  // Find a "base" symbol from bottom of callstack as end point
  list_t *l, *base = lfind_base (true);

  if (DEBUG_TREE)
    {
      dump_ystack();
      printf ("BASE = %s\n", base ? base->sym->name : "(Bnull)");
    }

  if (!base)
    return;

  if (ystack == base || ystack->sym == graph.main)
    lmark_edges (ystack, NULL, EM_TRACE);

  // Set markers to not account cycles to a node more than once
  for (l = ystack; l; l = l->next)
    l->sym->cycles.done = false;

  // Climb up the call stack until we find a node to account the cycles
  for (l = ystack; l && l != base; l = l->next)
    if (l->is_leaf
        || l->sym->is_skip)
      continue;
    else if (l->sym == base->sym || l->sym->is_base
             || l->sym->is_leaf
             // Expand whole sub-tree?
             || l->is_sub || l->sym->is_sub)
      break;
    else if (! (l->sym->is_reserved
                || l->edge->from->is_reserved_caller))
      break;

  account (l, base, cycles);
}


static edge_t*
traverse_edge (symbol_t *from, symbol_t *to, int delta, bool back)
{
  edge_t *e = get_edge (from, to);
  e->n++;
  if (delta)    e->n_call++;
  else          e->n_tail++;
  if (back || T_LONGJMP == e->from->type || T_LONGJMP == e->to->type)
    e->mark |= EM_BACK;

  return e;
}


static void
update_call_stack (symbol_t *sym, int delta, bool is_longjmp)
{
  list_t *l;
  int sp = pSP[0] | (pSP[1] << 8);

  account_cycles ();

  // Fix change of call depth for (very) special functions

  if ((is_longjmp |= T_LONGJMP == ystack->sym->type)
      || T_SETJMP == ystack->sym->type)
    delta = -1;

  if (sym
      && T_TERMINATE == sym->type)
    delta = 0;

  if (delta == 0
      && ystack && sym
      && sym == ystack->sym
      && sp == ystack->sp)
    {
      // Skip nodes jumping to themselves without changing anything,
      // just add the respective edge.
      traverse_edge (sym, sym, 0, false);
    }
  else if (ystack && delta >= 0)
    {
      // If the call depth does not change, e.g. for tail calls, add the node
      // to the call stack rather than replacing the top of the call stack by
      // the new node.  This yields better display of call tree if the
      // program fails in the remainder.  Moreover, this is needed in order
      // to correctly account cost to tail calls as costs might be promoted
      // up from callees to their callers.
      if (!sym)
        {
          // Jumping to / calling a location that has no symbol:
          // Cook up a node that has the target address as label.
          func_sym[cpu_PC] = sym = graph_add_symbol (NULL, cpu_PC, false);
          sym->is_reserved = true;
        }

      edge_t *e = traverse_edge (ystack->edge->to, sym, delta, false);

      int depth = ystack->depth + delta;
      l = ystack;
      lpush (&ystack, e);
      ystack->depth = depth;
      ystack->sp = sp;

      // Promote some "sticky" properties to callees.
      ystack->is_leaf = (l != NULL
                         && (l->is_leaf || l->sym->is_leaf)
                         /*&& ! l->sym->is_sub*/);
      ystack->is_sub  = (l != NULL
                         && (l->is_sub || l->sym->is_sub)
                         && ! l->sym->is_leaf);
      ystack->edge->n_leaf += ystack->is_leaf;
      ystack->edge->n_sub  += ystack->is_sub;
    }
  else if (ystack && delta < 0)
    {
      list_t *lj = NULL;

      l = ystack;

      if (is_longjmp)
        {
          if (DEBUG_TREE)
            printf ("/// UNWIND lj=%d\n", l->sym->type == T_LONGJMP);

          delta = 0;

          // longjmps make the call to contain cycles.  This is different to
          // ordinary recursive functions because in the case of a program
          // failure the call graph contains all the recursive calls and
          // they are part of the highlighted path to the termination point.
          // This is quite different with longjmps because we will unwind
          // the call stack to the respective setjmp.

          if (sym)
            {
              // If a longjmp targets a symbol, in almost all cases this is
              // due to bad jmp_buf content and the program jumps to 0x0.
              for (lj = ystack; lj; lj = lj->next)
                if (lj->sym == sym)
                  {
                    lmark_edges (ystack, lj, EM_TRACE);
                    // If the call tree already contains the longjmp's target
                    // symbol, then unwind (at least) to that point.
                    while (lj != ystack)
                      lpop (&ystack);
                    break;
                  }
            }
          else
            {
              // Mark edges that we are going to unwind below.
              for (lj = ystack; lj && lj->sp < sp; lj = lj->next)
                ;
              lmark_edges (ystack, lj ? lj->prev : NULL, EM_TRACE);
            }

          if (DEBUG_TREE)
            dump_ystack();
        }

      // The normal case
      while (ystack->next
             && (delta < 0
                 // this might yield better recovery from longjmp
                 || ystack->sp < sp))
        {
          bool main_returns = delta < 0 && ystack && ystack->sym == graph.main;
          lpop (&ystack);
          delta += delta < 0;
          if (main_returns)
            break;
        }

      if (is_longjmp)
        {
          symbol_t *from = sym = l->sym;
          if (T_LONGJMP != from->type)
            {
              // The case is tedious; presumably a __builtin_longjmp.
              if (!(sym = func_sym[old_PC]))
                {
                  // Cook up a hidden symbol so we can connect the
                  // incoming edge (code issues maybe longjmp) to the outgoing
                  // edge (maybe longjmp targets its maybe setjmp).
                  const char *s_longjmp = "longjmp?";
                  unsigned len = 3 + strlen (from->name) + strlen (s_longjmp);
                  char *s_name = get_mem (len, sizeof (char), s_longjmp);
                  sprintf (s_name, "%s\\n%s", s_longjmp, from->name);
                  sym = graph_add_symbol (s_name, old_PC, false);
                  sym->is_hidden = true;
                  func_sym[old_PC] = sym;
                }

              // Incoming edge: longjmp is a proper function, hence there
              // is already an incoming edge and we only need the following
              // edge for non-longjmp jumps.
              traverse_edge (from, sym, 0, true) -> mark |= EM_TRACE;
            }
          // The outgoing edge.
          traverse_edge (sym, ystack->sym, 0, true) -> mark |= EM_TRACE;
        } // is_longjmp
    } // delta < 0

  if (DEBUG_TREE)
    dump_ystack();
}


/* Log each transition,i.e. change of call stack.  This maked it much more
   convenient to track execution logs.  */

static void
log_transition (list_t *yold, list_t *ynew, int is_proep, const char *s_pe)
{
  symbol_t *old = yold ? yold->sym : NULL;
  symbol_t *new = ynew ? ynew->sym : NULL;
  int d_old = yold ? yold->depth : 0;
  int d_new = ynew ? ynew->depth : 0;
  int d = d_new - d_old;

  if (is_proep)
    {
      log_append ("%s+++[%d", d < 0 ? "\n" : "", d_new);
      if (is_proep == 1)
        log_append ("] %s -->", old->name);
      else if (d < 0)
        log_append ("<-%d] %s <-- %s <--", d_old, new->name, old->name);
      else
        log_append ("] %s <--", new->name);
      log_append (" %s \n", s_pe);
    }
  else if (old && new
           && (old != new || old == func_sym[cpu_PC]))
    {
      const char *s_lj = func_sym[old_PC] && func_sym[old_PC]->is_hidden
        ? "longjmp? <-- " : "";

      if (d == 0)  log_append ("\n+++[%d] ", d_old);
      if (d < 0)   log_append ("\n+++[%d<-%d] ", d_new, d_old);
      if (d > 0)   log_append ("\n+++[%d->%d] ", d_old, d_new);

      if (d < 0 || is_proep == 2)
        log_append ("%s <-- %s%s \n", new->name, s_lj, old->name);
      else
        log_append ("%s --> %s \n", old->name, new->name);
    }
  else if (old != new)
    {
      if (!old)
        old = new, d_old = d_new;
      log_append ("\n+++[%d] %s \n", d_old, old->name);
    }
}


/* Track the current call depth for performance metering and to display
   during instruction logging as functions are entered / left.  */

int
graph_update_call_depth (const decoded_t *deco)
{
  graph.old_id = graph.id;
  int id = graph.id = deco->id;
  int call = 0;

  if (! need.call_depth
      || ! graph.entered)
    return 0;

  switch (id)
    {
    case ID_RCALL:
      // avr-gcc uses "rcall ." to allocate stack, but an assembler
      // program might use that instruction just as well for an
      // ordinary call.  We cannot decide what's going on and take
      // the case that's more likely: Offset == 0 is allocating stack.
      call = deco->op2 != 0;
      break;
    case ID_ICALL: case ID_CALL: case ID_EICALL:
      call = 1;
      break;
    case ID_RET:
      // GCC might use push/push/ret for indirect jump,
      // don't account these for call depth
      if (graph.old_id != ID_PUSH)
        call = -1;
      break;
    }

  bool maybe_longjmp = ID_RET == id && ID_PUSH == graph.old_id;
  bool jump_indirect = ID_IJMP == id || ID_EIJMP == id;
  symbol_t *fun = func_sym[cpu_PC];
  symbol_t *cur = ystack->sym;

  // Pretty-print __prologure_saves__ and __epilogue_restores__ when logging,
  // but don't show them in the call tree:  the tree might be cluttered up
  // because too many functions are using these helpers from libgcc.
  static symbol_t *pro_ep = NULL;
  static char s_pe[50];
  int is_proep = 0;
  if (!pro_ep
      && (id == ID_RJMP || id == ID_JMP))
    {
      if (graph.prologue_saves
          && (unsigned) (cpu_PC - graph.prologue_saves->pc) <= 18)
        pro_ep = graph.prologue_saves;

      if (graph.epilogue_restores
          && (unsigned) (cpu_PC - graph.epilogue_restores->pc) <= 18)
        pro_ep = graph.epilogue_restores;

      if ((is_proep = NULL != pro_ep))
        {
          int n_regs = cpu_PC - pro_ep->pc;
          sprintf (s_pe, "%s + 0x%x (%d regs)", pro_ep->name,
                   2 * n_regs, 18 - n_regs);
        }
    }
  else if (pro_ep
           && (jump_indirect || id == ID_RET))
    is_proep = 2;

  fun = is_proep || !fun || fun->is_hidden ? NULL : fun;
  bool changed = call != 0 || fun != NULL;

  if (T_SETJMP == cur->type)
    jump_indirect && (changed = true);
  else if (T_LONGJMP == cur->type)
    maybe_longjmp = jump_indirect;
  else if (maybe_longjmp || jump_indirect)
    {
      int sp = pSP[0] | (pSP[1] << 8);
      maybe_longjmp = ystack && sp > ystack->sp;
    }

  // Entering main.  main is somewhat special in C programs.
  if (fun && fun == graph.main)
    {
      graph.main_return.n_call ++;
      if (call == 1)
        graph.main_return.pc = old_PC + opcodes[id].size;
      graph.no_startup_cycles = !graph.n_cycles;
    }

  bool main_returns = false;

  if ((main_returns
       = (fun && (fun == graph.exit || fun == graph._exit)
          && graph.old_id == ID_RET
          && (id == ID_JMP || id == ID_RJMP)
          && graph.main_return.n_call == 1
          && graph.main_return.pc == old_PC
          && yfree && yfree->sym == graph.main
          && ystack && ystack->sym == yfree->edge->from)))
    {
      // Resurrect the call of main.
      list_t *l = yfree;
      yfree = l->next;
      l->prev = NULL;
      l->next = ystack;
      if (l->next)
        l->next->prev = l;
      ystack = l;
      lmark_edges (ystack, NULL, EM_TRACE);
    }

  list_t *yold = ystack;

  if (changed || maybe_longjmp)
    update_call_stack (fun, call, maybe_longjmp);

  if (main_returns
      && ystack
      && (ystack->sym == graph.exit || ystack->sym == graph._exit))
    {
      // main returns.  If immediately after return from main exit or _exit
      // are entered, show an edge from main to the respective function.
      static char str[20];
      byte *r24 = log_cpu_address (24, AR_REG);
      int16_t ret_val = r24[0] | (r24[1] << 8);
      sprintf (str, "return %d", ret_val);
      ystack->edge->s_label = str;
      ystack->edge->mark |= EM_MAIN_RET | EM_DASHED;
    }

  if (!log_unused)
    log_transition (yold, ystack, is_proep, s_pe);

  if (is_proep == 2)
    pro_ep = NULL;

  return ystack ? ystack->depth : 0;
}


static void
write_dot_node (FILE *stream, symbol_t *n, const char *extra)
{
  if (n->dot_done)
    return;
  n->dot_done = true;

  fprintf (stream, "\t%d [label=\"%s%s%s%s%s", n->id, n->name,
           DEBUG_TREE && n->is_leaf ? ":L" : "",
           DEBUG_TREE && n->is_skip ? ":I" : "",
           DEBUG_TREE && n->is_sub ? ":S" : "",
           DEBUG_TREE && n->is_base ? ":B" : "");

  if (extra)
    fprintf (stream, "\\nreason: %s", extra);
  if (n->type != T_ADDR)
    fprintf (stream, "\\n0x%x", 2 * n->pc);

  if (n->type == T_TERMINATE)
    fprintf (stream, "\\ncycles:%u", program.n_cycles);
  else if (n->cycles.account && n->cycles.childs)
    fprintf (stream, "\\nch:%u own:%u", n->cycles.childs, n->cycles.own);
  else if (n->cycles.account)
    fprintf (stream, "\\n    own:%u", n->cycles.own);

  fprintf (stream, "\"]"); // ] label

  fprintf (stream, "[shape=%s]", n->type == T_ENTRY || n->type == T_TERMINATE
           ? "doubleoctagon"
           : n->is_base ? "box3d" : n->is_func ? "box"
           : "ellipse");

  double per = graph.n_cycles ? (double) n->cycles.own / graph.n_cycles : 0.0;

  if (n->type == T_ENTRY || n->type == T_TERMINATE)
    fprintf (stream, "[style=filled fillcolor=\"0.6 0.3 1\"]");
  else if (! n->cycles.account)
    fprintf (stream, "[color=gray][fontcolor=gray]");
  else
    fprintf (stream, "[style=filled fillcolor=\"%1.3lf %1.3lf %1.3lf\"]",
             0.4 * (1 - pow(per,0.3)), pow (per, 0.5), 1.0);
  fprintf (stream, ";\n");
}


static void
write_dot_edge (FILE *stream, edge_t *e, bool force, bool fat)
{
  bool synthetic = e->from->type == T_ENTRY || e->to->type == T_TERMINATE;
  bool show = force || synthetic || (e->mark & (EM_SHOW | EM_ACCOUNT));
  bool back = e->mark & EM_BACK;
  bool nfrom = show || e->from->is_base || e->from->cycles.account;
  bool nto =   show || e->to->is_base || e->to->cycles.account;

  if (e->mark & EM_DOT_DONE)
    return;
  e->mark |= EM_DOT_DONE;

  if (nfrom)
    write_dot_node (stream, e->from, NULL);
  if (nto)
    write_dot_node (stream, e->to, NULL);

  if (!show || !nfrom || !nto)
    return;

  const char *s_color = back
    ? fat ? "0.0 0.6 0.9" : "red"
    : (fat || (e->mark & EM_TRACE)
       ? "0.5 0.5 0.7"
       : e->mark & (EM_SHOW | EM_ACCOUNT) ? NULL : "gray");

  symbol_t *from = back ? e->to : e->from;
  symbol_t *to   = back ? e->from : e->to;

  fprintf (stream, "\t%d -> %d ", from->id, to->id);

  if (back)
    fprintf (stream, "[dir=back][fontcolor=red]");

  if (0 == (e->mark & (EM_ACCOUNT | EM_SHOW)))
    fprintf (stream, "[fontcolor=gray]");

  if (s_color)
    fprintf (stream, "[color=\"%s\"]", s_color);

  char s_dbg[30];
  *s_dbg = '\0';
  if (DEBUG_TREE && e->n)
    {
      size_t s1 = 0, s2 = 0;
      if (e->n_leaf)
        s1 = sprintf (s_dbg, " L:%d", e->n_leaf);
      if (e->n_sub)
        s2 = sprintf (s_dbg + s1, " S:%d", e->n_sub);
      if (e->mark & EM_TRACE)
        sprintf (s_dbg + s1 + s2, " >>");
    }

  if (!synthetic)
    {
      fprintf (stream, "[label=\"%s", e->s_label ? e->s_label : "");
      fprintf (stream, "%s#%d%s", e->s_label ? "\\n" : "", e->n, s_dbg);
      if (e->n_cycles)
        fprintf (stream, "\\n%u", e->n_cycles);
      fprintf (stream, "\"]");
    }

  if (e->s_tail)
    fprintf (stream, "[taillabel=\"%s\"]", e->s_tail);

  fprintf (stream, "%s%s", fat ? "[penwidth=4]" : "",
           e->mark & EM_DOTTED ? "[style=dotted]"
           : e->mark & EM_DASHED ? "[style=dashed]"
           : synthetic || e->n_call ? "" : "[style=dashed]");
  fprintf (stream, ";\n");
}


static const char*
make_dot_filename (void)
{
  if (options.do_graph_filename)
    return (str_eq ("", options.s_graph_filename)
            || str_eq ("-", options.s_graph_filename))
      ? NULL
      : options.s_graph_filename;

  const char *suffix = ".dot";
  const char *p, *s = program.name;
  if ((p = strrchr (s, '/')))  s = p;
  if ((p = strrchr (s, '\\'))) s = p;
  size_t len = (p = strrchr (s, '.'))
    ? (size_t) (p - program.name) - 1
    : strlen (program.name);

  char *fname = get_mem (len + 1 + strlen (suffix), sizeof (char), ".dot");
  strncpy (fname, program.name, len);
  return strcat (fname, suffix);
}


void
graph_write_dot (void)
{
  if (!graph.entered)
    return;

  const char *fname = make_dot_filename ();
  FILE *fdot = fname ? fopen (fname, "w") : stdout;

  if (!fdot)
    leave (LEAVE_FATAL, "cannot open \"%s\" for writing", fname);

  char reason[20];
  switch (program.leave_status)
    {
    case LEAVE_EXIT:  sprintf (reason, "exit %d", program.exit_value); break;
    case LEAVE_ABORTED: strcpy (reason, "abort");   break;
    case LEAVE_TIMEOUT: strcpy (reason, "timeout"); break;
    default:            strcpy (reason, "unknown"); break;
    }

  bool problem = (LEAVE_EXIT != program.leave_status
                  || 0 != program.exit_value);

  // Add artificial node and edge representing program termination.
  symbol_t *exit_point = graph_add_symbol ("Program Stop", old_PC, false);
  exit_point->type = T_TERMINATE;
  exit_point->is_reserved = true;
  update_call_stack (exit_point, 0, false);

  // Mark the way to the termination
  lmark_edges (ystack, NULL, EM_TRACE);

  fprintf (fdot, "digraph \"%s\"\n{\n", program.short_name);

  write_dot_node (fdot, graph.entry_point, NULL);
  write_dot_node (fdot, ystack->sym, reason);
  write_dot_edge (fdot, graph.entry_edge, true, true);
  write_dot_edge (fdot, ystack->edge, true, true);

  // Startup code before main is boring, use neat shortcut instead
  list_t *lmain;
  if (!options.do_graph_all
      && graph.no_startup_cycles
      && yend && yend->edge == graph.entry_edge
      && (lmain = lfind_type (T_MAIN)))
    {
      lmark_edges (lmain, NULL, EM_DOT_DONE);
      edge_t *e = traverse_edge (yend->sym, graph.main, 0, false);
      e->n -= 0 != (e->mark & EM_DOT_DONE);
      e->mark |= EM_TRACE | EM_DASHED;
      e->mark &= ~EM_DOT_DONE;
      e->s_label = " Startup Code";
    }

  for (int i = 0; i < EPRIM; i++)
    for (edge_t *e = ebucket[i]; e != NULL; e = e->next)
      write_dot_edge (fdot, e, (e->mark & EM_TRACE) || options.do_graph_all,
                      (e->mark & EM_TRACE) && problem);

  fprintf (fdot, "}\n");
  fflush (fdot);
  if (fdot != stdout)
    fclose (fdot);
}
