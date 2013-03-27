/* trace.c --- tracing output for the M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "bfd.h"
#include "dis-asm.h"
#include "m32c-desc.h"

#include "cpu.h"
#include "mem.h"
#include "load.h"

static int
sim_dis_read (bfd_vma memaddr, bfd_byte * ptr, unsigned int length,
	      struct disassemble_info *info)
{
  mem_get_blk (memaddr, ptr, length);
  return 0;
}

/* Filter out (in place) symbols that are useless for disassembly.
   COUNT is the number of elements in SYMBOLS.
   Return the number of useful symbols. */

static long
remove_useless_symbols (asymbol ** symbols, long count)
{
  register asymbol **in_ptr = symbols, **out_ptr = symbols;

  while (--count >= 0)
    {
      asymbol *sym = *in_ptr++;

      if (strstr (sym->name, "gcc2_compiled"))
	continue;
      if (sym->name == NULL || sym->name[0] == '\0')
	continue;
      if (sym->flags & (BSF_DEBUGGING))
	continue;
      if (bfd_is_und_section (sym->section)
	  || bfd_is_com_section (sym->section))
	continue;

      *out_ptr++ = sym;
    }
  return out_ptr - symbols;
}

static int
compare_symbols (const PTR ap, const PTR bp)
{
  const asymbol *a = *(const asymbol **) ap;
  const asymbol *b = *(const asymbol **) bp;

  if (bfd_asymbol_value (a) > bfd_asymbol_value (b))
    return 1;
  else if (bfd_asymbol_value (a) < bfd_asymbol_value (b))
    return -1;
  return 0;
}

static char opbuf[1000];

static int
op_printf (char *buf, char *fmt, ...)
{
  int ret;
  va_list ap;

  va_start (ap, fmt);
  ret = vsprintf (opbuf + strlen (opbuf), fmt, ap);
  va_end (ap);
  return ret;
}

static bfd *current_bfd;

void
sim_disasm_init (bfd *prog)
{
  current_bfd = prog;
}

typedef struct Files
{
  struct Files *next;
  char *filename;
  int nlines;
  char **lines;
  char *data;
} Files;
Files *files = 0;

static char *
load_file_and_line (const char *filename, int lineno)
{
  Files *f;
  for (f = files; f; f = f->next)
    if (strcmp (f->filename, filename) == 0)
      break;
  if (!f)
    {
      int i;
      struct stat s;
      const char *found_filename, *slash;

      found_filename = filename;
      while (1)
	{
	  if (stat (found_filename, &s) == 0)
	    break;
	  slash = strchr (found_filename, '/');
	  if (!slash)
	    return "";
	  found_filename = slash + 1;
	}

      f = (Files *) malloc (sizeof (Files));
      f->next = files;
      files = f;
      f->filename = strdup (filename);
      f->data = (char *) malloc (s.st_size + 2);
      FILE *file = fopen (found_filename, "rb");
      fread (f->data, 1, s.st_size, file);
      f->data[s.st_size] = 0;
      fclose (file);

      f->nlines = 1;
      for (i = 0; i < s.st_size; i++)
	if (f->data[i] == '\n')
	  f->nlines++;
      f->lines = (char **) malloc (f->nlines * sizeof (char *));
      f->lines[0] = f->data;
      f->nlines = 1;
      for (i = 0; i < s.st_size; i++)
	if (f->data[i] == '\n')
	  {
	    f->lines[f->nlines] = f->data + i + 1;
	    while (*f->lines[f->nlines] == ' '
		   || *f->lines[f->nlines] == '\t')
	      f->lines[f->nlines]++;
	    f->nlines++;
	    f->data[i] = 0;
	  }
    }
  if (lineno < 1 || lineno > f->nlines)
    return "";
  return f->lines[lineno - 1];
}

void
sim_disasm_one ()
{
  static int initted = 0;
  static asymbol **symtab = 0;
  static int symcount = 0;
  static int last_sym = -1;
  static struct disassemble_info info;
  int storage, sym, bestaddr;
  int min, max, i;
  static asection *code_section = 0;
  static bfd_vma code_base = 0;
  asection *s;
  int save_trace = trace;

  static const char *prev_filename = "";
  static int prev_lineno = 0;
  const char *filename;
  const char *functionname;
  unsigned int lineno;

  int mypc = get_reg (pc);

  if (current_bfd == 0)
    return;

  trace = 0;

  if (!initted)
    {
      initted = 1;
      memset (&info, 0, sizeof (info));
      INIT_DISASSEMBLE_INFO (info, stdout, op_printf);
      info.read_memory_func = sim_dis_read;
      info.arch = bfd_get_arch (current_bfd);
      info.mach = bfd_get_mach (current_bfd);
      if (info.mach == 0)
	{
	  info.arch = bfd_arch_m32c;
	  info.mach = default_machine;
	}
      disassemble_init_for_target (&info);

      storage = bfd_get_symtab_upper_bound (current_bfd);
      if (storage > 0)
	{
	  symtab = (asymbol **) malloc (storage);
	  symcount = bfd_canonicalize_symtab (current_bfd, symtab);
	  symcount = remove_useless_symbols (symtab, symcount);
	  qsort (symtab, symcount, sizeof (asymbol *), compare_symbols);
	}
      for (s = current_bfd->sections; s; s = s->next)
	{
	  if (s->flags & SEC_CODE || code_section == 0)
	    {
	      code_section = s;
	      code_base = bfd_section_lma (current_bfd, s);
	      break;
	    }
	}
    }

  filename = functionname = 0;
  lineno = 0;
  if (bfd_find_nearest_line
      (current_bfd, code_section, symtab, mypc - code_base, &filename,
       &functionname, &lineno))
    {
      if (filename && functionname && lineno)
	{
	  if (lineno != prev_lineno || strcmp (prev_filename, filename))
	    {
	      char *the_line = load_file_and_line (filename, lineno);
	      const char *slash = strrchr (filename, '/');
	      if (!slash)
		slash = filename;
	      else
		slash++;
	      printf
		("========================================"
                 "=====================================\n");
	      printf ("\033[37;41m %s:%d: \033[33;40m %s\033[K\033[0m\n",
		      slash, lineno, the_line);
	    }
	  prev_lineno = lineno;
	  prev_filename = filename;
	}
    }

  {
    min = -1;
    max = symcount;
    while (min < max - 1)
      {
	bfd_vma sa;
	sym = (min + max) / 2;
	sa = bfd_asymbol_value (symtab[sym]);
	/*printf("checking %4d %08x %s\n",
                 sym, sa, bfd_asymbol_name (symtab[sym])); */
	if (sa > mypc)
	  max = sym;
	else if (sa < mypc)
	  min = sym;
	else
	  {
	    min = sym;
	    break;
	  }
      }
    if (min != -1 && min != last_sym)
      {
	bestaddr = bfd_asymbol_value (symtab[min]);
	printf ("\033[43;30m%s", bfd_asymbol_name (symtab[min]));
	if (bestaddr != mypc)
	  printf ("+%d", mypc - bestaddr);
	printf (":\t\t\t\033[0m\n");
	last_sym = min;
#if 0
	if (trace == 1)
	  if (strcmp (bfd_asymbol_name (symtab[min]), "abort") == 0
	      || strcmp (bfd_asymbol_name (symtab[min]), "exit") == 0)
	    trace = 0;
#endif
      }
  }

  opbuf[0] = 0;
  printf ("\033[33m%06x: ", mypc);
  max = print_insn_m32c (mypc, &info);
  for (i = 0; i < max; i++)
    printf ("%02x", mem_get_qi (mypc + i));
  for (; i < 6; i++)
    printf ("  ");
  printf ("%-16s  ", opbuf);

  printf ("\033[0m\n");
  trace = save_trace;
}
