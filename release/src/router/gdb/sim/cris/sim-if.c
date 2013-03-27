/* Main simulator entry points specific to the CRIS.
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

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

/* Based on the fr30 file, mixing in bits from the i960 and pruning of
   dead code.  */

#include "libiberty.h"
#include "bfd.h"

#include "sim-main.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "sim-options.h"
#include "dis-asm.h"

/* Apparently the autoconf bits are missing (though HAVE_ENVIRON is used
   in other dirs; also lacking there).  Patch around it for major systems.  */
#if defined (HAVE_ENVIRON) || defined (__GLIBC__)
extern char **environ;
#define GET_ENVIRON() environ
#else
char *missing_environ[] = { "SHELL=/bin/sh", "PATH=/bin:/usr/bin", NULL };
#define GET_ENVIRON() missing_environ
#endif

/* AUX vector entries.  */
#define TARGET_AT_NULL 0
#define TARGET_AT_PHDR 3
#define TARGET_AT_PHENT 4
#define TARGET_AT_PHNUM 5
#define TARGET_AT_PAGESZ 6
#define TARGET_AT_BASE 7
#define TARGET_AT_FLAGS 8
#define TARGET_AT_ENTRY 9
#define TARGET_AT_UID 11
#define TARGET_AT_EUID 12
#define TARGET_AT_GID 13
#define TARGET_AT_EGID 14
#define TARGET_AT_HWCAP 16
#define TARGET_AT_CLKTCK 17

/* Used with get_progbounds to find out how much memory is needed for the
   program.  We don't want to allocate more, since that could mask
   invalid memory accesses program bugs.  */
struct progbounds {
  USI startmem;
  USI endmem;
};

static void free_state (SIM_DESC);
static void get_progbounds (bfd *, asection *, void *);
static SIM_RC cris_option_handler (SIM_DESC, sim_cpu *, int, char *, int);

/* Since we don't build the cgen-opcode table, we use the old
   disassembler.  */
static CGEN_DISASSEMBLER cris_disassemble_insn;

/* By default, we set up stack and environment variables like the Linux
   kernel.  */
static char cris_bare_iron = 0;

/* Whether 0x9000000xx have simulator-specific meanings.  */
char cris_have_900000xxif = 0;

/* What to do when we face a more or less unknown syscall.  */
enum cris_unknown_syscall_action_type cris_unknown_syscall_action
  = CRIS_USYSC_MSG_STOP;

/* Records simulator descriptor so utilities like cris_dump_regs can be
   called from gdb.  */
SIM_DESC current_state;

/* CRIS-specific options.  */
typedef enum {
  OPTION_CRIS_STATS = OPTION_START,
  OPTION_CRIS_TRACE,
  OPTION_CRIS_NAKED,
  OPTION_CRIS_900000XXIF,
  OPTION_CRIS_UNKNOWN_SYSCALL
} CRIS_OPTIONS;

static const OPTION cris_options[] =
{
  { {"cris-cycles", required_argument, NULL, OPTION_CRIS_STATS},
      '\0', "basic|unaligned|schedulable|all",
    "Dump execution statistics",
      cris_option_handler, NULL },
  { {"cris-trace", required_argument, NULL, OPTION_CRIS_TRACE},
      '\0', "basic",
    "Emit trace information while running",
      cris_option_handler, NULL },
  { {"cris-naked", no_argument, NULL, OPTION_CRIS_NAKED},
     '\0', NULL, "Don't set up stack and environment",
     cris_option_handler, NULL },
  { {"cris-900000xx", no_argument, NULL, OPTION_CRIS_900000XXIF},
     '\0', NULL, "Define addresses at 0x900000xx with simulator semantics",
     cris_option_handler, NULL },
  { {"cris-unknown-syscall", required_argument, NULL,
     OPTION_CRIS_UNKNOWN_SYSCALL},
     '\0', "stop|enosys|enosys-quiet", "Action at an unknown system call",
     cris_option_handler, NULL },
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL, NULL }
};

/* Add the CRIS-specific option list to the simulator.  */

SIM_RC
cris_option_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sim_add_option_table (sd, NULL, cris_options) != SIM_RC_OK)
    return SIM_RC_FAIL;
  return SIM_RC_OK;
}

/* Handle CRIS-specific options.  */

static SIM_RC
cris_option_handler (SIM_DESC sd, sim_cpu *cpu ATTRIBUTE_UNUSED, int opt,
		     char *arg, int is_command ATTRIBUTE_UNUSED)
{
  /* The options are CRIS-specific, but cpu-specific option-handling is
     broken; required to being with "--cpu0-".  We store the flags in an
     unused field in the global state structure and move the flags over
     to the module-specific CPU data when we store things in the
     cpu-specific structure.  */
  char *tracefp = STATE_TRACE_FLAGS (sd);

  switch ((CRIS_OPTIONS) opt)
    {
      case OPTION_CRIS_STATS:
	if (strcmp (arg, "basic") == 0)
	  *tracefp = FLAG_CRIS_MISC_PROFILE_SIMPLE;
	else if (strcmp (arg, "unaligned") == 0)
	  *tracefp
	    = (FLAG_CRIS_MISC_PROFILE_UNALIGNED
	       | FLAG_CRIS_MISC_PROFILE_SIMPLE);
	else if (strcmp (arg, "schedulable") == 0)
	  *tracefp
	    = (FLAG_CRIS_MISC_PROFILE_SCHEDULABLE
	       | FLAG_CRIS_MISC_PROFILE_SIMPLE);
	else if (strcmp (arg, "all") == 0)
	  *tracefp = FLAG_CRIS_MISC_PROFILE_ALL;
	else
	  {
	    /* Beware; the framework does not handle the error case;
	       we have to do it ourselves.  */
	    sim_io_eprintf (sd, "Unknown option `--cris-cycles=%s'\n", arg);
	    return SIM_RC_FAIL;
	  }
	break;

      case OPTION_CRIS_TRACE:
	if (strcmp (arg, "basic") == 0)
	  *tracefp |= FLAG_CRIS_MISC_PROFILE_XSIM_TRACE;
	else
	  {
	    sim_io_eprintf (sd, "Unknown option `--cris-trace=%s'\n", arg);
	    return SIM_RC_FAIL;
	  }
	break;

      case OPTION_CRIS_NAKED:
	cris_bare_iron = 1;
	break;

      case OPTION_CRIS_900000XXIF:
	cris_have_900000xxif = 1;
	break;

      case OPTION_CRIS_UNKNOWN_SYSCALL:
	if (strcmp (arg, "enosys") == 0)
	  cris_unknown_syscall_action = CRIS_USYSC_MSG_ENOSYS;
	else if (strcmp (arg, "enosys-quiet") == 0)
	  cris_unknown_syscall_action = CRIS_USYSC_QUIET_ENOSYS;
	else if (strcmp (arg, "stop") == 0)
	  cris_unknown_syscall_action = CRIS_USYSC_MSG_STOP;
	else
	  {
	    sim_io_eprintf (sd, "Unknown option `--cris-unknown-syscall=%s'\n",
			    arg);
	    return SIM_RC_FAIL;
	  }
	break;

      default:
	/* We'll actually never get here; the caller handles the error
	   case.  */
	sim_io_eprintf (sd, "Unknown option `%s'\n", arg);
	return SIM_RC_FAIL;
    }

  /* Imply --profile-model=on.  */
  return sim_profile_set_option (sd, "-model", PROFILE_MODEL_IDX, "on");
}

/* Cover function of sim_state_free to free the cpu buffers as well.  */

static void
free_state (SIM_DESC sd)
{
  if (STATE_MODULES (sd) != NULL)
    sim_module_uninstall (sd);
  sim_cpu_free_all (sd);
  sim_state_free (sd);
}

/* BFD section iterator to find the highest allocated section address
   (plus one).  If we could, we should use the program header table
   instead, but we can't get to that using bfd.  */

void
get_progbounds (bfd *abfd ATTRIBUTE_UNUSED, asection *s, void *vp)
{
  struct progbounds *pbp = (struct progbounds *) vp;

  if ((bfd_get_section_flags (abfd, s) & SEC_ALLOC))
    {
      bfd_size_type sec_size = bfd_get_section_size (s);
      bfd_size_type sec_start = bfd_get_section_vma (abfd, s);
      bfd_size_type sec_end = sec_start + sec_size;

      if (sec_end > pbp->endmem)
	pbp->endmem = sec_end;

      if (sec_start < pbp->startmem)
	pbp->startmem = sec_start;
    }
}

/* Create an instance of the simulator.  */

SIM_DESC
sim_open (SIM_OPEN_KIND kind, host_callback *callback, struct bfd *abfd,
	  char **argv)
{
  char c;
  int i;
  USI startmem = 0;
  USI endmem = CRIS_DEFAULT_MEM_SIZE;
  USI endbrk = endmem;
  USI stack_low = 0;
  SIM_DESC sd = sim_state_alloc (kind, callback);

  /* Can't initialize to "" below.  It's either a GCC bug in old
     releases (up to and including 2.95.3 (.4 in debian) or a bug in the
     standard ;-) that the rest of the elements won't be initialized.  */
  bfd_byte sp_init[4] = {0, 0, 0, 0};

  /* The cpu data is kept in a separately allocated chunk of memory.  */
  if (sim_cpu_alloc_all (sd, 1, cgen_cpu_max_extra_bytes ()) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* getopt will print the error message so we just have to exit if this fails.
     FIXME: Hmmm...  in the case of gdb we need getopt to call
     print_filtered.  */
  if (sim_parse_args (sd, argv) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* If we have a binary program, endianness-setting would not be taken
     from elsewhere unfortunately, so set it here.  At the time of this
     writing, it isn't used until sim_config, but that might change so
     set it here before memory is defined or touched.  */
  current_target_byte_order = LITTLE_ENDIAN;

  /* check for/establish the reference program image */
  if (sim_analyze_program (sd,
			   (STATE_PROG_ARGV (sd) != NULL
			    ? *STATE_PROG_ARGV (sd)
			    : NULL),
			   abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* For CRIS simulator-specific use, we need to find out the bounds of
     the program as well, which is not done by sim_analyze_program
     above.  */
  if (STATE_PROG_BFD (sd))
    {
      struct progbounds pb;

      /* The sections should now be accessible using bfd functions.  */
      pb.startmem = 0x7fffffff;
      pb.endmem = 0;
      bfd_map_over_sections (STATE_PROG_BFD (sd), get_progbounds, &pb);

      /* We align the area that the program uses to page boundaries.  */
      startmem = pb.startmem & ~8191;
      endbrk = pb.endmem;
      endmem = (endbrk + 8191) & ~8191;
    }

  /* Find out how much room is needed for the environment and argv, create
     that memory and fill it.  Only do this when there's a program
     specified.  */
  if (STATE_PROG_BFD (sd) && !cris_bare_iron)
    {
      char *name = bfd_get_filename (STATE_PROG_BFD (sd));
      char **my_environ = GET_ENVIRON ();
      /* We use these maps to give the same behavior as the old xsim
	 simulator.  */
      USI envtop = 0x40000000;
      USI stacktop = 0x3e000000;
      USI envstart;
      int envc;
      int len = strlen (name) + 1;
      USI epp, epp0;
      USI stacklen;
      int i;
      char **prog_argv = STATE_PROG_ARGV (sd);
      int my_argc = 0;
      /* All CPU:s have the same memory map, apparently.  */
      SIM_CPU *cpu = STATE_CPU (sd, 0);
      USI csp;
      bfd_byte buf[4];

      /* Count in the environment as well. */
      for (envc = 0; my_environ[envc] != NULL; envc++)
	len += strlen (my_environ[envc]) + 1;

      for (i = 0; prog_argv[i] != NULL; my_argc++, i++)
	len += strlen (prog_argv[i]) + 1;

      envstart = (envtop - len) & ~8191;

      /* Create read-only block for the environment strings.  */
      sim_core_attach (sd, NULL, 0, access_read, 0,
		       envstart, (len + 8191) & ~8191,
		       0, NULL, NULL);

      /* This shouldn't happen.  */
      if (envstart < stacktop)
	stacktop = envstart - 64 * 8192;

      csp = stacktop;

      /* Note that the linux kernel does not correctly compute the storage
	 needs for the static-exe AUX vector.  */
      csp -= 4 * 4 * 2;

      csp -= (envc + 1) * 4;
      csp -= (my_argc + 1) * 4;
      csp -= 4;

      /* Write the target representation of the start-up-value for the
	 stack-pointer suitable for register initialization below.  */
      bfd_putl32 (csp, sp_init);

      /* If we make this 1M higher; say 8192*1024, we have to take
	 special precautions for pthreads, because pthreads assumes that
	 the memory that low isn't mmapped, and that it can mmap it
	 without fallback in case of failure (and we fail ungracefully
	 long before *that*: the memory isn't accounted for in our mmap
	 list).  */
      stack_low = (csp - (7168*1024)) & ~8191;

      stacklen = stacktop - stack_low;

      /* Tee hee, we have an executable stack.  Well, it's necessary to
	 test GCC trampolines...  */
      sim_core_attach (sd, NULL, 0, access_read_write_exec, 0,
		       stack_low, stacklen,
		       0, NULL, NULL);

      epp = epp0 = envstart;

      /* Can't use sim_core_write_unaligned_4 without everything
	 initialized when tracing, and then these writes would get into
	 the trace.  */
#define write_dword(addr, data)						\
 do									\
   {									\
     USI data_ = data;							\
     USI addr_ = addr;							\
     bfd_putl32 (data_, buf);						\
     if (sim_core_write_buffer (sd, cpu, 0, buf, addr_, 4) != 4)	\
	goto abandon_chip;						\
   }									\
 while (0)

      write_dword (csp, my_argc);
      csp += 4;

      for (i = 0; i < my_argc; i++, csp += 4)
	{
	  size_t strln = strlen (prog_argv[i]) + 1;

	  if (sim_core_write_buffer (sd, cpu, 0, prog_argv[i], epp, strln)
	      != strln)
	  goto abandon_chip;

	  write_dword (csp, envstart + epp - epp0);
	  epp += strln;
	}

      write_dword (csp, 0);
      csp += 4;

      for (i = 0; i < envc; i++, csp += 4)
	{
	  unsigned int strln = strlen (my_environ[i]) + 1;

	  if (sim_core_write_buffer (sd, cpu, 0, my_environ[i], epp, strln)
	      != strln)
	    goto abandon_chip;

	  write_dword (csp, envstart + epp - epp0);
	  epp += strln;
	}

      write_dword (csp, 0);
      csp += 4;

#define NEW_AUX_ENT(nr, id, val)			\
 do							\
   {							\
     write_dword (csp + (nr) * 4 * 2, (id));		\
     write_dword (csp + (nr) * 4 * 2 + 4, (val));	\
   }							\
 while (0)

      /* Note that there are some extra AUX entries for a dynlinked
	 program loaded image.  */

      /* AUX entries always present. */
      NEW_AUX_ENT (0, TARGET_AT_HWCAP, 0);
      NEW_AUX_ENT (1, TARGET_AT_PAGESZ, 8192);
      NEW_AUX_ENT (2, TARGET_AT_CLKTCK, 100);

      csp += 4 * 2 * 3;
      NEW_AUX_ENT (0, TARGET_AT_NULL, 0);
#undef NEW_AUX_ENT

      /* Register R10 should hold 0 at static start (no initfunc), but
	 that's the default, so don't bother.  */
    }

  /* Allocate core managed memory if none specified by user.  */
  if (sim_core_read_buffer (sd, NULL, read_map, &c, startmem, 1) == 0)
    sim_do_commandf (sd, "memory region 0x%lx,0x%lx", startmem,
		     endmem - startmem);

  /* Allocate simulator I/O managed memory if none specified by user.  */
  if (cris_have_900000xxif)
    {
      if (sim_core_read_buffer (sd, NULL, read_map, &c, 0x90000000, 1) == 0)
	sim_core_attach (sd, NULL, 0, access_write, 0, 0x90000000, 0x100,
			 0, &cris_devices, NULL);
      else
	{
	  (*callback->
	   printf_filtered) (callback,
			     "Seeing --cris-900000xx with memory defined there\n");
	  goto abandon_chip;
	}
    }

  /* Establish any remaining configuration options.  */
  if (sim_config (sd) != SIM_RC_OK)
    {
    abandon_chip:
      free_state (sd);
      return 0;
    }

  if (sim_post_argv_init (sd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Open a copy of the cpu descriptor table.  */
  {
    CGEN_CPU_DESC cd = cris_cgen_cpu_open_1 (STATE_ARCHITECTURE (sd)->printable_name,
					     CGEN_ENDIAN_LITTLE);
    for (i = 0; i < MAX_NR_PROCESSORS; ++i)
      {
	SIM_CPU *cpu = STATE_CPU (sd, i);
	CPU_CPU_DESC (cpu) = cd;
	CPU_DISASSEMBLER (cpu) = cris_disassemble_insn;

	/* See cris_option_handler for the reason why this is needed.  */
	CPU_CRIS_MISC_PROFILE (cpu)->flags = STATE_TRACE_FLAGS (sd)[0];

	/* Set SP to the stack we allocated above.  */
	(* CPU_REG_STORE (cpu)) (cpu, H_GR_SP, (char *) sp_init, 4);

	/* Set the simulator environment data.  */
	cpu->highest_mmapped_page = NULL;
	cpu->endmem = endmem;
	cpu->endbrk = endbrk;
	cpu->stack_low = stack_low;
	cpu->syscalls = 0;
	cpu->m1threads = 0;
	cpu->threadno = 0;
	cpu->max_threadid = 0;
	cpu->thread_data = NULL;
	memset (cpu->sighandler, 0, sizeof (cpu->sighandler));
	cpu->make_thread_cpu_data = NULL;
	cpu->thread_cpu_data_size = 0;
#if WITH_HW
	cpu->deliver_interrupt = NULL;
#endif
      }
#if WITH_HW
    /* Always be cycle-accurate and call before/after functions if
       with-hardware.  */
    sim_profile_set_option (sd, "-model", PROFILE_MODEL_IDX, "on");
#endif
  }

  /* Initialize various cgen things not done by common framework.
     Must be done after cris_cgen_cpu_open.  */
  cgen_init (sd);

  /* Store in a global so things like cris_dump_regs can be invoked
     from the gdb command line.  */
  current_state = sd;

  cris_set_callbacks (callback);

  return sd;
}

void
sim_close (SIM_DESC sd, int quitting ATTRIBUTE_UNUSED)
{
  cris_cgen_cpu_close (CPU_CPU_DESC (STATE_CPU (sd, 0)));
  sim_module_uninstall (sd);
}

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *abfd,
		     char **argv ATTRIBUTE_UNUSED,
		     char **envp ATTRIBUTE_UNUSED)
{
  SIM_CPU *current_cpu = STATE_CPU (sd, 0);
  SIM_ADDR addr;

  if (abfd != NULL)
    addr = bfd_get_start_address (abfd);
  else
    addr = 0;
  sim_pc_set (current_cpu, addr);

  /* Other simulators have #if 0:d code that says
      STATE_ARGV (sd) = sim_copy_argv (argv);
      STATE_ENVP (sd) = sim_copy_argv (envp);
     Enabling that gives you not-found link-errors for sim_copy_argv.
     FIXME: Do archaeology to find out more.  */

  return SIM_RC_OK;
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  if (sim_args_command (sd, cmd) != SIM_RC_OK)
    sim_io_eprintf (sd, "Unknown command `%s'\n", cmd);
}

/* Disassemble an instruction.  */

static void
cris_disassemble_insn (SIM_CPU *cpu,
		       const CGEN_INSN *insn ATTRIBUTE_UNUSED,
		       const ARGBUF *abuf ATTRIBUTE_UNUSED,
		       IADDR pc, char *buf)
{
  disassembler_ftype pinsn;
  struct disassemble_info disasm_info;
  SFILE sfile;
  SIM_DESC sd = CPU_STATE (cpu);

  sfile.buffer = sfile.current = buf;
  INIT_DISASSEMBLE_INFO (disasm_info, (FILE *) &sfile,
			 (fprintf_ftype) sim_disasm_sprintf);
  disasm_info.endian =
    (bfd_big_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_BIG
     : bfd_little_endian (STATE_PROG_BFD (sd)) ? BFD_ENDIAN_LITTLE
     : BFD_ENDIAN_UNKNOWN);
  /* We live with the cast until the prototype is fixed, or else we get a
     warning because the functions differ in the signedness of one parameter.  */
  disasm_info.read_memory_func =
    sim_disasm_read_memory;
  disasm_info.memory_error_func = sim_disasm_perror_memory;
  disasm_info.application_data = (PTR) cpu;
  pinsn = cris_get_disassembler (STATE_PROG_BFD (sd));
  (*pinsn) (pc, &disasm_info);
}
