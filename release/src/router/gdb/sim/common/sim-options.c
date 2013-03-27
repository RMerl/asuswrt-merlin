/* Simulator option handling.
   Copyright (C) 1996, 1997, 2004, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

#include "sim-main.h"
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#include "libiberty.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-assert.h"

#include "bfd.h"

/* Add a set of options to the simulator.
   TABLE is an array of OPTIONS terminated by a NULL `opt.name' entry.
   This is intended to be called by modules in their `install' handler.  */

SIM_RC
sim_add_option_table (SIM_DESC sd, sim_cpu *cpu, const OPTION *table)
{
  struct option_list *ol = ((struct option_list *)
			    xmalloc (sizeof (struct option_list)));

  /* Note: The list is constructed in the reverse order we're called so
     later calls will override earlier ones (in case that ever happens).
     This is the intended behaviour.  */

  if (cpu)
    {
      ol->next = CPU_OPTIONS (cpu);
      ol->options = table;
      CPU_OPTIONS (cpu) = ol;
    }
  else
    {
      ol->next = STATE_OPTIONS (sd);
      ol->options = table;
      STATE_OPTIONS (sd) = ol;
    }

  return SIM_RC_OK;
}

/* Standard option table.
   Modules may specify additional ones.
   The caller of sim_parse_args may also specify additional options
   by calling sim_add_option_table first.  */

static DECLARE_OPTION_HANDLER (standard_option_handler);

/* FIXME: We shouldn't print in --help output options that aren't usable.
   Some fine tuning will be necessary.  One can either move less general
   options to another table or use a HAVE_FOO macro to ifdef out unavailable
   options.  */

/* ??? One might want to conditionally compile out the entries that
   aren't enabled.  There's a distinction, however, between options a
   simulator can't support and options that haven't been configured in.
   Certainly options a simulator can't support shouldn't appear in the
   output of --help.  Whether the same thing applies to options that haven't
   been configured in or not isn't something I can get worked up over.
   [Note that conditionally compiling them out might simply involve moving
   the option to another table.]
   If you decide to conditionally compile them out as well, delete this
   comment and add a comment saying that that is the rule.  */

typedef enum {
  OPTION_DEBUG_INSN = OPTION_START,
  OPTION_DEBUG_FILE,
  OPTION_DO_COMMAND,
  OPTION_ARCHITECTURE,
  OPTION_TARGET,
  OPTION_ARCHITECTURE_INFO,
  OPTION_ENVIRONMENT,
  OPTION_ALIGNMENT,
  OPTION_VERBOSE,
#if defined (SIM_HAVE_BIENDIAN)
  OPTION_ENDIAN,
#endif
  OPTION_DEBUG,
#ifdef SIM_HAVE_FLATMEM
  OPTION_MEM_SIZE,
#endif
  OPTION_HELP,
#ifdef SIM_H8300 /* FIXME: Should be movable to h8300 dir.  */
  OPTION_H8300H,
  OPTION_H8300S,
  OPTION_H8300SX,
#endif
  OPTION_LOAD_LMA,
  OPTION_LOAD_VMA,
  OPTION_SYSROOT
} STANDARD_OPTIONS;

static const OPTION standard_options[] =
{
  { {"verbose", no_argument, NULL, OPTION_VERBOSE},
      'v', NULL, "Verbose output",
      standard_option_handler },

#if defined (SIM_HAVE_BIENDIAN) /* ??? && WITH_TARGET_BYTE_ORDER == 0 */
  { {"endian", required_argument, NULL, OPTION_ENDIAN},
      'E', "big|little", "Set endianness",
      standard_option_handler },
#endif

#ifdef SIM_HAVE_ENVIRONMENT
  /* This option isn't supported unless all choices are supported in keeping
     with the goal of not printing in --help output things the simulator can't
     do [as opposed to things that just haven't been configured in].  */
  { {"environment", required_argument, NULL, OPTION_ENVIRONMENT},
      '\0', "user|virtual|operating", "Set running environment",
      standard_option_handler },
#endif

  { {"alignment", required_argument, NULL, OPTION_ALIGNMENT},
      '\0', "strict|nonstrict|forced", "Set memory access alignment",
      standard_option_handler },

  { {"debug", no_argument, NULL, OPTION_DEBUG},
      'D', NULL, "Print debugging messages",
      standard_option_handler },
  { {"debug-insn", no_argument, NULL, OPTION_DEBUG_INSN},
      '\0', NULL, "Print instruction debugging messages",
      standard_option_handler },
  { {"debug-file", required_argument, NULL, OPTION_DEBUG_FILE},
      '\0', "FILE NAME", "Specify debugging output file",
      standard_option_handler },

#ifdef SIM_H8300 /* FIXME: Should be movable to h8300 dir.  */
  { {"h8300h", no_argument, NULL, OPTION_H8300H},
      'h', NULL, "Indicate the CPU is H8/300H",
      standard_option_handler },
  { {"h8300s", no_argument, NULL, OPTION_H8300S},
      'S', NULL, "Indicate the CPU is H8S",
      standard_option_handler },
  { {"h8300sx", no_argument, NULL, OPTION_H8300SX},
      'x', NULL, "Indicate the CPU is H8SX",
      standard_option_handler },
#endif

#ifdef SIM_HAVE_FLATMEM
  { {"mem-size", required_argument, NULL, OPTION_MEM_SIZE},
     'm', "<size>[in bytes, Kb (k suffix), Mb (m suffix) or Gb (g suffix)]",
     "Specify memory size", standard_option_handler },
#endif

  { {"do-command", required_argument, NULL, OPTION_DO_COMMAND},
      '\0', "COMMAND", ""/*undocumented*/,
      standard_option_handler },

  { {"help", no_argument, NULL, OPTION_HELP},
      'H', NULL, "Print help information",
      standard_option_handler },

  { {"architecture", required_argument, NULL, OPTION_ARCHITECTURE},
      '\0', "MACHINE", "Specify the architecture to use",
      standard_option_handler },
  { {"architecture-info", no_argument, NULL, OPTION_ARCHITECTURE_INFO},
      '\0', NULL, "List supported architectures",
      standard_option_handler },
  { {"info-architecture", no_argument, NULL, OPTION_ARCHITECTURE_INFO},
      '\0', NULL, NULL,
      standard_option_handler },

  { {"target", required_argument, NULL, OPTION_TARGET},
      '\0', "BFDNAME", "Specify the object-code format for the object files",
      standard_option_handler },

#ifdef SIM_HANDLES_LMA
  { {"load-lma", no_argument, NULL, OPTION_LOAD_LMA},
      '\0', NULL,
#if SIM_HANDLES_LMA
    "Use VMA or LMA addresses when loading image (default LMA)",
#else
    "Use VMA or LMA addresses when loading image (default VMA)",
#endif
      standard_option_handler, "load-{lma,vma}" },
  { {"load-vma", no_argument, NULL, OPTION_LOAD_VMA},
      '\0', NULL, "", standard_option_handler,  "" },
#endif

  { {"sysroot", required_argument, NULL, OPTION_SYSROOT},
      '\0', "SYSROOT",
    "Root for system calls with absolute file-names and cwd at start",
      standard_option_handler },

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

static SIM_RC
standard_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
			 char *arg, int is_command)
{
  int i,n;

  switch ((STANDARD_OPTIONS) opt)
    {
    case OPTION_VERBOSE:
      STATE_VERBOSE_P (sd) = 1;
      break;

#ifdef SIM_HAVE_BIENDIAN
    case OPTION_ENDIAN:
      if (strcmp (arg, "big") == 0)
	{
	  if (WITH_TARGET_BYTE_ORDER == LITTLE_ENDIAN)
	    {
	      sim_io_eprintf (sd, "Simulator compiled for little endian only.\n");
	      return SIM_RC_FAIL;
	    }
	  /* FIXME:wip: Need to set something in STATE_CONFIG.  */
	  current_target_byte_order = BIG_ENDIAN;
	}
      else if (strcmp (arg, "little") == 0)
	{
	  if (WITH_TARGET_BYTE_ORDER == BIG_ENDIAN)
	    {
	      sim_io_eprintf (sd, "Simulator compiled for big endian only.\n");
	      return SIM_RC_FAIL;
	    }
	  /* FIXME:wip: Need to set something in STATE_CONFIG.  */
	  current_target_byte_order = LITTLE_ENDIAN;
	}
      else
	{
	  sim_io_eprintf (sd, "Invalid endian specification `%s'\n", arg);
	  return SIM_RC_FAIL;
	}
      break;
#endif

    case OPTION_ENVIRONMENT:
      if (strcmp (arg, "user") == 0)
	STATE_ENVIRONMENT (sd) = USER_ENVIRONMENT;
      else if (strcmp (arg, "virtual") == 0)
	STATE_ENVIRONMENT (sd) = VIRTUAL_ENVIRONMENT;
      else if (strcmp (arg, "operating") == 0)
	STATE_ENVIRONMENT (sd) = OPERATING_ENVIRONMENT;
      else
	{
	  sim_io_eprintf (sd, "Invalid environment specification `%s'\n", arg);
	  return SIM_RC_FAIL;
	}
      if (WITH_ENVIRONMENT != ALL_ENVIRONMENT
	  && WITH_ENVIRONMENT != STATE_ENVIRONMENT (sd))
	{
	  char *type;
	  switch (WITH_ENVIRONMENT)
	    {
	    case USER_ENVIRONMENT: type = "user"; break;
	    case VIRTUAL_ENVIRONMENT: type = "virtual"; break;
	    case OPERATING_ENVIRONMENT: type = "operating"; break;
	    }
	  sim_io_eprintf (sd, "Simulator compiled for the %s environment only.\n",
			  type);
	  return SIM_RC_FAIL;
	}
      break;

    case OPTION_ALIGNMENT:
      if (strcmp (arg, "strict") == 0)
	{
	  if (WITH_ALIGNMENT == 0 || WITH_ALIGNMENT == STRICT_ALIGNMENT)
	    {
	      current_alignment = STRICT_ALIGNMENT;
	      break;
	    }
	}
      else if (strcmp (arg, "nonstrict") == 0)
	{
	  if (WITH_ALIGNMENT == 0 || WITH_ALIGNMENT == NONSTRICT_ALIGNMENT)
	    {
	      current_alignment = NONSTRICT_ALIGNMENT;
	      break;
	    }
	}
      else if (strcmp (arg, "forced") == 0)
	{
	  if (WITH_ALIGNMENT == 0 || WITH_ALIGNMENT == FORCED_ALIGNMENT)
	    {
	      current_alignment = FORCED_ALIGNMENT;
	      break;
	    }
	}
      else
	{
	  sim_io_eprintf (sd, "Invalid alignment specification `%s'\n", arg);
	  return SIM_RC_FAIL;
	}
      switch (WITH_ALIGNMENT)
	{
	case STRICT_ALIGNMENT:
	  sim_io_eprintf (sd, "Simulator compiled for strict alignment only.\n");
	  break;
	case NONSTRICT_ALIGNMENT:
	  sim_io_eprintf (sd, "Simulator compiled for nonstrict alignment only.\n");
	  break;
	case FORCED_ALIGNMENT:
	  sim_io_eprintf (sd, "Simulator compiled for forced alignment only.\n");
	  break;
	}
      return SIM_RC_FAIL;

    case OPTION_DEBUG:
      if (! WITH_DEBUG)
	sim_io_eprintf (sd, "Debugging not compiled in, `-D' ignored\n");
      else
	{
	  for (n = 0; n < MAX_NR_PROCESSORS; ++n)
	    for (i = 0; i < MAX_DEBUG_VALUES; ++i)
	      CPU_DEBUG_FLAGS (STATE_CPU (sd, n))[i] = 1;
	}
      break;

    case OPTION_DEBUG_INSN :
      if (! WITH_DEBUG)
	sim_io_eprintf (sd, "Debugging not compiled in, `--debug-insn' ignored\n");
      else
	{
	  for (n = 0; n < MAX_NR_PROCESSORS; ++n)
	    CPU_DEBUG_FLAGS (STATE_CPU (sd, n))[DEBUG_INSN_IDX] = 1;
	}
      break;

    case OPTION_DEBUG_FILE :
      if (! WITH_DEBUG)
	sim_io_eprintf (sd, "Debugging not compiled in, `--debug-file' ignored\n");
      else
	{
	  FILE *f = fopen (arg, "w");

	  if (f == NULL)
	    {
	      sim_io_eprintf (sd, "Unable to open debug output file `%s'\n", arg);
	      return SIM_RC_FAIL;
	    }
	  for (n = 0; n < MAX_NR_PROCESSORS; ++n)
	    CPU_DEBUG_FILE (STATE_CPU (sd, n)) = f;
	}
      break;

#ifdef SIM_H8300 /* FIXME: Can be moved to h8300 dir.  */
    case OPTION_H8300H:
      set_h8300h (bfd_mach_h8300h);
      break;
    case OPTION_H8300S:
      set_h8300h (bfd_mach_h8300s);
      break;
    case OPTION_H8300SX:
      set_h8300h (bfd_mach_h8300sx);
      break;
#endif

#ifdef SIM_HAVE_FLATMEM
    case OPTION_MEM_SIZE:
      {
	char * endp;
	unsigned long ul = strtol (arg, &endp, 0);

	switch (* endp)
	  {
	  case 'k': case 'K': size <<= 10; break;
	  case 'm': case 'M': size <<= 20; break;
	  case 'g': case 'G': size <<= 30; break;
	  case ' ': case '\0': case '\t':  break;
	  default:
	    if (ul > 0)
	      sim_io_eprintf (sd, "Ignoring strange character at end of memory size: %c\n", * endp);
	    break;
	  }

	/* 16384: some minimal amount */
	if (! isdigit (arg[0]) || ul < 16384)
	  {
	    sim_io_eprintf (sd, "Invalid memory size `%s'", arg);
	    return SIM_RC_FAIL;
	  }
	STATE_MEM_SIZE (sd) = ul;
      }
      break;
#endif

    case OPTION_DO_COMMAND:
      sim_do_command (sd, arg);
      break;

    case OPTION_ARCHITECTURE:
      {
	const struct bfd_arch_info *ap = bfd_scan_arch (arg);
	if (ap == NULL)
	  {
	    sim_io_eprintf (sd, "Architecture `%s' unknown\n", arg);
	    return SIM_RC_FAIL;
	  }
	STATE_ARCHITECTURE (sd) = ap;
	break;
      }

    case OPTION_ARCHITECTURE_INFO:
      {
	const char **list = bfd_arch_list();
	const char **lp;
	if (list == NULL)
	  abort ();
	sim_io_printf (sd, "Possible architectures:");
	for (lp = list; *lp != NULL; lp++)
	  sim_io_printf (sd, " %s", *lp);
	sim_io_printf (sd, "\n");
	free (list);
	break;
      }

    case OPTION_TARGET:
      {
	STATE_TARGET (sd) = xstrdup (arg);
	break;
      }

    case OPTION_LOAD_LMA:
      {
	STATE_LOAD_AT_LMA_P (sd) = 1;
	break;
      }

    case OPTION_LOAD_VMA:
      {
	STATE_LOAD_AT_LMA_P (sd) = 0;
	break;
      }

    case OPTION_HELP:
      sim_print_help (sd, is_command);
      if (STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
	exit (0);
      /* FIXME: 'twould be nice to do something similar if gdb.  */
      break;

    case OPTION_SYSROOT:
      /* Don't leak memory in the odd event that there's lots of
	 --sysroot=... options.  */
      if (simulator_sysroot[0] != '\0' && arg[0] != '\0')
	free (simulator_sysroot);
      simulator_sysroot = xstrdup (arg);
      break;
    }

  return SIM_RC_OK;
}

/* Add the standard option list to the simulator.  */

SIM_RC
standard_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sim_add_option_table (sd, NULL, standard_options) != SIM_RC_OK)
    return SIM_RC_FAIL;
#ifdef SIM_HANDLES_LMA
  STATE_LOAD_AT_LMA_P (sd) = SIM_HANDLES_LMA;
#endif
  return SIM_RC_OK;
}

/* Return non-zero if arg is a duplicate argument.
   If ARG is NULL, initialize.  */

#define ARG_HASH_SIZE 97
#define ARG_HASH(a) ((256 * (unsigned char) a[0] + (unsigned char) a[1]) % ARG_HASH_SIZE)

static int
dup_arg_p (arg)
     char *arg;
{
  int hash;
  static char **arg_table = NULL;

  if (arg == NULL)
    {
      if (arg_table == NULL)
	arg_table = (char **) xmalloc (ARG_HASH_SIZE * sizeof (char *));
      memset (arg_table, 0, ARG_HASH_SIZE * sizeof (char *));
      return 0;
    }

  hash = ARG_HASH (arg);
  while (arg_table[hash] != NULL)
    {
      if (strcmp (arg, arg_table[hash]) == 0)
	return 1;
      /* We assume there won't be more than ARG_HASH_SIZE arguments so we
	 don't check if the table is full.  */
      if (++hash == ARG_HASH_SIZE)
	hash = 0;
    }
  arg_table[hash] = arg;
  return 0;
}
     
/* Called by sim_open to parse the arguments.  */

SIM_RC
sim_parse_args (sd, argv)
     SIM_DESC sd;
     char **argv;
{
  int c, i, argc, num_opts;
  char *p, *short_options;
  /* The `val' option struct entry is dynamically assigned for options that
     only come in the long form.  ORIG_VAL is used to get the original value
     back.  */
  int *orig_val;
  struct option *lp, *long_options;
  const struct option_list *ol;
  const OPTION *opt;
  OPTION_HANDLER **handlers;
  sim_cpu **opt_cpu;
  SIM_RC result = SIM_RC_OK;

  /* Count the number of arguments.  */
  for (argc = 0; argv[argc] != NULL; ++argc)
    continue;

  /* Count the number of options.  */
  num_opts = 0;
  for (ol = STATE_OPTIONS (sd); ol != NULL; ol = ol->next)
    for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
      ++num_opts;
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    for (ol = CPU_OPTIONS (STATE_CPU (sd, i)); ol != NULL; ol = ol->next)
      for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
	++num_opts;

  /* Initialize duplicate argument checker.  */
  (void) dup_arg_p (NULL);

  /* Build the option table for getopt.  */

  long_options = NZALLOC (struct option, num_opts + 1);
  lp = long_options;
  short_options = NZALLOC (char, num_opts * 3 + 1);
  p = short_options;
  handlers = NZALLOC (OPTION_HANDLER *, OPTION_START + num_opts);
  orig_val = NZALLOC (int, OPTION_START + num_opts);
  opt_cpu = NZALLOC (sim_cpu *, OPTION_START + num_opts);

  /* Set '+' as first char so argument permutation isn't done.  This
     is done to stop getopt_long returning options that appear after
     the target program.  Such options should be passed unchanged into
     the program image. */
  *p++ = '+';

  for (i = OPTION_START, ol = STATE_OPTIONS (sd); ol != NULL; ol = ol->next)
    for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
      {
	if (dup_arg_p (opt->opt.name))
	  continue;
	if (opt->shortopt != 0)
	  {
	    *p++ = opt->shortopt;
	    if (opt->opt.has_arg == required_argument)
	      *p++ = ':';
	    else if (opt->opt.has_arg == optional_argument)
	      { *p++ = ':'; *p++ = ':'; }
	    handlers[(unsigned char) opt->shortopt] = opt->handler;
	    if (opt->opt.val != 0)
	      orig_val[(unsigned char) opt->shortopt] = opt->opt.val;
	    else
	      orig_val[(unsigned char) opt->shortopt] = opt->shortopt;
	  }
	if (opt->opt.name != NULL)
	  {
	    *lp = opt->opt;
	    /* Dynamically assign `val' numbers for long options. */
	    lp->val = i++;
	    handlers[lp->val] = opt->handler;
	    orig_val[lp->val] = opt->opt.val;
	    opt_cpu[lp->val] = NULL;
	    ++lp;
	  }
      }

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      sim_cpu *cpu = STATE_CPU (sd, c);
      for (ol = CPU_OPTIONS (cpu); ol != NULL; ol = ol->next)
	for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
	  {
#if 0 /* Each option is prepended with --<cpuname>- so this greatly cuts down
	 on the need for dup_arg_p checking.  Maybe in the future it'll be
	 needed so this is just commented out, and not deleted.  */
	    if (dup_arg_p (opt->opt.name))
	      continue;
#endif
	    /* Don't allow short versions of cpu specific options for now.  */
	    if (opt->shortopt != 0)
	      {
		sim_io_eprintf (sd, "internal error, short cpu specific option");
		result = SIM_RC_FAIL;
		break;
	      }
	    if (opt->opt.name != NULL)
	      {
		char *name;
		*lp = opt->opt;
		/* Prepend --<cpuname>- to the option.  */
		asprintf (&name, "%s-%s", CPU_NAME (cpu), lp->name);
		lp->name = name;
		/* Dynamically assign `val' numbers for long options. */
		lp->val = i++;
		handlers[lp->val] = opt->handler;
		orig_val[lp->val] = opt->opt.val;
		opt_cpu[lp->val] = cpu;
		++lp;
	      }
	  }
    }
	    
  /* Terminate the short and long option lists.  */
  *p = 0;
  lp->name = NULL;

  /* Ensure getopt is initialized.  */
  optind = 0;

  while (1)
    {
      int longind, optc;

      optc = getopt_long (argc, argv, short_options, long_options, &longind);
      if (optc == -1)
	{
	  if (STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
	    STATE_PROG_ARGV (sd) = dupargv (argv + optind);
	  break;
	}
      if (optc == '?')
	{
	  result = SIM_RC_FAIL;
	  break;
	}

      if ((*handlers[optc]) (sd, opt_cpu[optc], orig_val[optc], optarg, 0/*!is_command*/) == SIM_RC_FAIL)
	{
	  result = SIM_RC_FAIL;
	  break;
	}
    }

  zfree (long_options);
  zfree (short_options);
  zfree (handlers);
  zfree (opt_cpu);
  zfree (orig_val);
  return result;
}

/* Utility of sim_print_help to print a list of option tables.  */

static void
print_help (SIM_DESC sd, sim_cpu *cpu, const struct option_list *ol, int is_command)
{
  const OPTION *opt;

  for ( ; ol != NULL; ol = ol->next)
    for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
      {
	const int indent = 30;
	int comma, len;
	const OPTION *o;

	if (dup_arg_p (opt->opt.name))
	  continue;

	if (opt->doc == NULL)
	  continue;

	if (opt->doc_name != NULL && opt->doc_name [0] == '\0')
	  continue;

	sim_io_printf (sd, "  ");

	comma = 0;
	len = 2;

	/* list any short options (aliases) for the current OPT */
	if (!is_command)
	  {
	    o = opt;
	    do
	      {
		if (o->shortopt != '\0')
		  {
		    sim_io_printf (sd, "%s-%c", comma ? ", " : "", o->shortopt);
		    len += (comma ? 2 : 0) + 2;
		    if (o->arg != NULL)
		      {
			if (o->opt.has_arg == optional_argument)
			  {
			    sim_io_printf (sd, "[%s]", o->arg);
			    len += 1 + strlen (o->arg) + 1;
			  }
			else
			  {
			    sim_io_printf (sd, " %s", o->arg);
			    len += 1 + strlen (o->arg);
			  }
		      }
		    comma = 1;
		  }
		++o;
	      }
	    while (OPTION_VALID_P (o) && o->doc == NULL);
	  }
	
	/* list any long options (aliases) for the current OPT */
	o = opt;
	do
	  {
	    const char *name;
	    const char *cpu_prefix = cpu ? CPU_NAME (cpu) : NULL;
	    if (o->doc_name != NULL)
	      name = o->doc_name;
	    else
	      name = o->opt.name;
	    if (name != NULL)
	      {
		sim_io_printf (sd, "%s%s%s%s%s",
			       comma ? ", " : "",
			       is_command ? "" : "--",
			       cpu ? cpu_prefix : "",
			       cpu ? "-" : "",
			       name);
		len += ((comma ? 2 : 0)
			+ (is_command ? 0 : 2)
			+ strlen (name));
		if (o->arg != NULL)
		  {
		    if (o->opt.has_arg == optional_argument)
		      {
			sim_io_printf (sd, "[=%s]", o->arg);
			len += 2 + strlen (o->arg) + 1;
		      }
		    else
		      {
			sim_io_printf (sd, " %s", o->arg);
			len += 1 + strlen (o->arg);
		      }
		  }
		comma = 1;
	      }
	    ++o;
	  }
	while (OPTION_VALID_P (o) && o->doc == NULL);

	if (len >= indent)
	  {
	    sim_io_printf (sd, "\n%*s", indent, "");
	  }
	else
	  sim_io_printf (sd, "%*s", indent - len, "");

	/* print the description, word wrap long lines */
	{
	  const char *chp = opt->doc;
	  unsigned doc_width = 80 - indent;
	  while (strlen (chp) >= doc_width) /* some slack */
	    {
	      const char *end = chp + doc_width - 1;
	      while (end > chp && !isspace (*end))
		end --;
	      if (end == chp)
		end = chp + doc_width - 1;
	      /* The cast should be ok - its distances between to
                 points in a string.  */
	      sim_io_printf (sd, "%.*s\n%*s", (int) (end - chp), chp, indent,
			     "");
	      chp = end;
	      while (isspace (*chp) && *chp != '\0')
		chp++;
	    }
	  sim_io_printf (sd, "%s\n", chp);
	}
      }
}

/* Print help messages for the options.  */

void
sim_print_help (sd, is_command)
     SIM_DESC sd;
     int is_command;
{
  if (STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
    sim_io_printf (sd, "Usage: %s [options] program [program args]\n",
		   STATE_MY_NAME (sd));

  /* Initialize duplicate argument checker.  */
  (void) dup_arg_p (NULL);

  if (STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
    sim_io_printf (sd, "Options:\n");
  else
    sim_io_printf (sd, "Commands:\n");

  print_help (sd, NULL, STATE_OPTIONS (sd), is_command);
  sim_io_printf (sd, "\n");

  /* Print cpu-specific options.  */
  {
    int i;

    for (i = 0; i < MAX_NR_PROCESSORS; ++i)
      {
	sim_cpu *cpu = STATE_CPU (sd, i);
	if (CPU_OPTIONS (cpu) == NULL)
	  continue;
	sim_io_printf (sd, "CPU %s specific options:\n", CPU_NAME (cpu));
	print_help (sd, cpu, CPU_OPTIONS (cpu), is_command);
	sim_io_printf (sd, "\n");
      }
  }

  sim_io_printf (sd, "Note: Depending on the simulator configuration some %ss\n",
		 STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE ? "option" : "command");
  sim_io_printf (sd, "      may not be applicable\n");

  if (STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
    {
      sim_io_printf (sd, "\n");
      sim_io_printf (sd, "program args    Arguments to pass to simulated program.\n");
      sim_io_printf (sd, "                Note: Very few simulators support this.\n");
    }
}

/* Utility of sim_args_command to find the closest match for a command.
   Commands that have "-" in them can be specified as separate words.
   e.g. sim memory-region 0x800000,0x4000
   or   sim memory region 0x800000,0x4000
   If CPU is non-null, use its option table list, otherwise use the main one.
   *PARGI is where to start looking in ARGV.  It is updated to point past
   the found option.  */

static const OPTION *
find_match (SIM_DESC sd, sim_cpu *cpu, char *argv[], int *pargi)
{
  const struct option_list *ol;
  const OPTION *opt;
  /* most recent option match */
  const OPTION *matching_opt = NULL;
  int matching_argi = -1;

  if (cpu)
    ol = CPU_OPTIONS (cpu);
  else
    ol = STATE_OPTIONS (sd);

  /* Skip passed elements specified by *PARGI.  */
  argv += *pargi;

  for ( ; ol != NULL; ol = ol->next)
    for (opt = ol->options; OPTION_VALID_P (opt); ++opt)
      {
	int argi = 0;
	const char *name = opt->opt.name;
	if (name == NULL)
	  continue;
	while (argv [argi] != NULL
	       && strncmp (name, argv [argi], strlen (argv [argi])) == 0)
	  {
	    name = &name [strlen (argv[argi])];
	    if (name [0] == '-')
	      {
		/* leading match ...<a-b-c>-d-e-f - continue search */
		name ++; /* skip `-' */
		argi ++;
		continue;
	      }
	    else if (name [0] == '\0')
	      {
		/* exact match ...<a-b-c-d-e-f> - better than before? */
		if (argi > matching_argi)
		  {
		    matching_argi = argi;
		    matching_opt = opt;
		  }
		break;
	      }
	    else
	      break;
	  }
      }

  *pargi = matching_argi;
  return matching_opt;
}

SIM_RC
sim_args_command (SIM_DESC sd, char *cmd)
{
  /* something to do? */
  if (cmd == NULL)
    return SIM_RC_OK; /* FIXME - perhaps help would be better */
  
  if (cmd [0] == '-')
    {
      /* user specified -<opt> ... form? */
      char **argv = buildargv (cmd);
      SIM_RC rc = sim_parse_args (sd, argv);
      freeargv (argv);
      return rc;
    }
  else
    {
      char **argv = buildargv (cmd);
      const OPTION *matching_opt = NULL;
      int matching_argi;
      sim_cpu *cpu;

      if (argv [0] == NULL)
	return SIM_RC_OK; /* FIXME - perhaps help would be better */

      /* First check for a cpu selector.  */
      {
	char *cpu_name = xstrdup (argv[0]);
	char *hyphen = strchr (cpu_name, '-');
	if (hyphen)
	  *hyphen = 0;
	cpu = sim_cpu_lookup (sd, cpu_name);
	if (cpu)
	  {
	    /* If <cpuname>-<command>, point argv[0] at <command>.  */
	    if (hyphen)
	      {
		matching_argi = 0;
		argv[0] += hyphen - cpu_name + 1;
	      }
	    else
	      matching_argi = 1;
	    matching_opt = find_match (sd, cpu, argv, &matching_argi);
	    /* If hyphen found restore argv[0].  */
	    if (hyphen)
	      argv[0] -= hyphen - cpu_name + 1;
	  }
	free (cpu_name);
      }

      /* If that failed, try the main table.  */
      if (matching_opt == NULL)
	{
	  matching_argi = 0;
	  matching_opt = find_match (sd, NULL, argv, &matching_argi);
	}

      if (matching_opt != NULL)
	{
	  switch (matching_opt->opt.has_arg)
	    {
	    case no_argument:
	      if (argv [matching_argi + 1] == NULL)
		matching_opt->handler (sd, cpu, matching_opt->opt.val,
				       NULL, 1/*is_command*/);
	      else
		sim_io_eprintf (sd, "Command `%s' takes no arguments\n",
				matching_opt->opt.name);
	      break;
	    case optional_argument:
	      if (argv [matching_argi + 1] == NULL)
		matching_opt->handler (sd, cpu, matching_opt->opt.val,
				       NULL, 1/*is_command*/);
	      else if (argv [matching_argi + 2] == NULL)
		matching_opt->handler (sd, cpu, matching_opt->opt.val,
				       argv [matching_argi + 1], 1/*is_command*/);
	      else
		sim_io_eprintf (sd, "Command `%s' requires no more than one argument\n",
				matching_opt->opt.name);
	      break;
	    case required_argument:
	      if (argv [matching_argi + 1] == NULL)
		sim_io_eprintf (sd, "Command `%s' requires an argument\n",
				matching_opt->opt.name);
	      else if (argv [matching_argi + 2] == NULL)
		matching_opt->handler (sd, cpu, matching_opt->opt.val,
				       argv [matching_argi + 1], 1/*is_command*/);
	      else
		sim_io_eprintf (sd, "Command `%s' requires only one argument\n",
				matching_opt->opt.name);
	    }
	  freeargv (argv);
	  return SIM_RC_OK;
	}

      freeargv (argv);
    }
      
  /* didn't find anything that remotly matched */
  return SIM_RC_FAIL;
}
