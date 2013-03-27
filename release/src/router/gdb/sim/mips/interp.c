/*> interp.c <*/
/* Simulator for the MIPS architecture.

   This file is part of the MIPS sim

		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

NOTEs:

The IDT monitor (found on the VR4300 board), seems to lie about
register contents. It seems to treat the registers as sign-extended
32-bit values. This cause *REAL* problems when single-stepping 64-bit
code on the hardware.

*/

/* The TRACE manifests enable the provision of extra features. If they
   are not defined then a simpler (quicker) simulator is constructed
   without the required run-time checks, etc. */
#if 1 /* 0 to allow user build selection, 1 to force inclusion */
#define TRACE (1)
#endif

#include "bfd.h"
#include "sim-main.h"
#include "sim-utils.h"
#include "sim-options.h"
#include "sim-assert.h"
#include "sim-hw.h"

#include "itable.h"


#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <ansidecl.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "getopt.h"
#include "libiberty.h"
#include "bfd.h"
#include "gdb/callback.h"   /* GDB simulator callback interface */
#include "gdb/remote-sim.h" /* GDB simulator interface */

#include "sysdep.h"

#ifndef PARAMS
#define PARAMS(x) 
#endif

char* pr_addr PARAMS ((SIM_ADDR addr));
char* pr_uword64 PARAMS ((uword64 addr));


/* Within interp.c we refer to the sim_state and sim_cpu directly. */
#define CPU cpu
#define SD sd


/* The following reserved instruction value is used when a simulator
   trap is required. NOTE: Care must be taken, since this value may be
   used in later revisions of the MIPS ISA. */

#define RSVD_INSTRUCTION           (0x00000005)
#define RSVD_INSTRUCTION_MASK      (0xFC00003F)

#define RSVD_INSTRUCTION_ARG_SHIFT 6
#define RSVD_INSTRUCTION_ARG_MASK  0xFFFFF  


/* Bits in the Debug register */
#define Debug_DBD 0x80000000   /* Debug Branch Delay */
#define Debug_DM  0x40000000   /* Debug Mode         */
#define Debug_DBp 0x00000002   /* Debug Breakpoint indicator */

/*---------------------------------------------------------------------------*/
/*-- GDB simulator interface ------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void ColdReset PARAMS((SIM_DESC sd));

/*---------------------------------------------------------------------------*/



#define DELAYSLOT()     {\
                          if (STATE & simDELAYSLOT)\
                            sim_io_eprintf(sd,"Delay slot already activated (branch in delay slot?)\n");\
                          STATE |= simDELAYSLOT;\
                        }

#define JALDELAYSLOT()	{\
			  DELAYSLOT ();\
			  STATE |= simJALDELAYSLOT;\
			}

#define NULLIFY()       {\
                          STATE &= ~simDELAYSLOT;\
                          STATE |= simSKIPNEXT;\
                        }

#define CANCELDELAYSLOT() {\
                            DSSTATE = 0;\
                            STATE &= ~(simDELAYSLOT | simJALDELAYSLOT);\
                          }

#define INDELAYSLOT()	((STATE & simDELAYSLOT) != 0)
#define INJALDELAYSLOT() ((STATE & simJALDELAYSLOT) != 0)

/* Note that the monitor code essentially assumes this layout of memory.
   If you change these, change the monitor code, too.  */
/* FIXME Currently addresses are truncated to 32-bits, see
   mips/sim-main.c:address_translation(). If that changes, then these
   values will need to be extended, and tested for more carefully. */
#define K0BASE  (0x80000000)
#define K0SIZE  (0x20000000)
#define K1BASE  (0xA0000000)
#define K1SIZE  (0x20000000)

/* Simple run-time monitor support.
   
   We emulate the monitor by placing magic reserved instructions at
   the monitor's entry points; when we hit these instructions, instead
   of raising an exception (as we would normally), we look at the
   instruction and perform the appropriate monitory operation.
   
   `*_monitor_base' are the physical addresses at which the corresponding 
        monitor vectors are located.  `0' means none.  By default,
        install all three.
    The RSVD_INSTRUCTION... macros specify the magic instructions we
    use at the monitor entry points.  */
static int firmware_option_p = 0;
static SIM_ADDR idt_monitor_base =     0xBFC00000;
static SIM_ADDR pmon_monitor_base =    0xBFC00500;
static SIM_ADDR lsipmon_monitor_base = 0xBFC00200;

static SIM_RC sim_firmware_command (SIM_DESC sd, char* arg);


#define MEM_SIZE (8 << 20)	/* 8 MBytes */


#if defined(TRACE)
static char *tracefile = "trace.din"; /* default filename for trace log */
FILE *tracefh = NULL;
static void open_trace PARAMS((SIM_DESC sd));
#endif /* TRACE */

static const char * get_insn_name (sim_cpu *, int);

/* simulation target board.  NULL=canonical */
static char* board = NULL;


static DECLARE_OPTION_HANDLER (mips_option_handler);

enum {
  OPTION_DINERO_TRACE = OPTION_START,
  OPTION_DINERO_FILE,
  OPTION_FIRMWARE,
  OPTION_INFO_MEMORY,
  OPTION_BOARD
};

static int display_mem_info = 0;

static SIM_RC
mips_option_handler (sd, cpu, opt, arg, is_command)
     SIM_DESC sd;
     sim_cpu *cpu;
     int opt;
     char *arg;
     int is_command;
{
  int cpu_nr;
  switch (opt)
    {
    case OPTION_DINERO_TRACE: /* ??? */
#if defined(TRACE)
      /* Eventually the simTRACE flag could be treated as a toggle, to
	 allow external control of the program points being traced
	 (i.e. only from main onwards, excluding the run-time setup,
	 etc.). */
      for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++)
	{
	  sim_cpu *cpu = STATE_CPU (sd, cpu_nr);
	  if (arg == NULL)
	    STATE |= simTRACE;
	  else if (strcmp (arg, "yes") == 0)
	    STATE |= simTRACE;
	  else if (strcmp (arg, "no") == 0)
	    STATE &= ~simTRACE;
	  else if (strcmp (arg, "on") == 0)
	    STATE |= simTRACE;
	  else if (strcmp (arg, "off") == 0)
	    STATE &= ~simTRACE;
	  else
	    {
	      fprintf (stderr, "Unrecognized dinero-trace option `%s'\n", arg);
	      return SIM_RC_FAIL;
	    }
	}
      return SIM_RC_OK;
#else /* !TRACE */
      fprintf(stderr,"\
Simulator constructed without dinero tracing support (for performance).\n\
Re-compile simulator with \"-DTRACE\" to enable this option.\n");
      return SIM_RC_FAIL;
#endif /* !TRACE */

    case OPTION_DINERO_FILE:
#if defined(TRACE)
      if (optarg != NULL) {
	char *tmp;
	tmp = (char *)malloc(strlen(optarg) + 1);
	if (tmp == NULL)
	  {
	    sim_io_printf(sd,"Failed to allocate buffer for tracefile name \"%s\"\n",optarg);
	    return SIM_RC_FAIL;
	  }
	else {
	  strcpy(tmp,optarg);
	  tracefile = tmp;
	  sim_io_printf(sd,"Placing trace information into file \"%s\"\n",tracefile);
	}
      }
#endif /* TRACE */
      return SIM_RC_OK;

    case OPTION_FIRMWARE:
      return sim_firmware_command (sd, arg);

    case OPTION_BOARD:
      {
	if (arg)
	  {
	    board = zalloc(strlen(arg) + 1);
	    strcpy(board, arg);
	  }
	return SIM_RC_OK;
      }

    case OPTION_INFO_MEMORY:
      display_mem_info = 1;
      break;
    }
  
  return SIM_RC_OK;
}


static const OPTION mips_options[] =
{
  { {"dinero-trace", optional_argument, NULL, OPTION_DINERO_TRACE},
      '\0', "on|off", "Enable dinero tracing",
      mips_option_handler },
  { {"dinero-file", required_argument, NULL, OPTION_DINERO_FILE},
      '\0', "FILE", "Write dinero trace to FILE",
      mips_option_handler },
  { {"firmware", required_argument, NULL, OPTION_FIRMWARE},
    '\0', "[idt|pmon|lsipmon|none][@ADDRESS]", "Emulate ROM monitor",
    mips_option_handler },
  { {"board", required_argument, NULL, OPTION_BOARD},
     '\0', "none" /* rely on compile-time string concatenation for other options */

#define BOARD_JMR3904 "jmr3904"
           "|" BOARD_JMR3904
#define BOARD_JMR3904_PAL "jmr3904pal"
           "|" BOARD_JMR3904_PAL
#define BOARD_JMR3904_DEBUG "jmr3904debug"
           "|" BOARD_JMR3904_DEBUG
#define BOARD_BSP "bsp"
           "|" BOARD_BSP

    , "Customize simulation for a particular board.", mips_option_handler },

  /* These next two options have the same names as ones found in the
     memory_options[] array in common/sim-memopt.c.  This is because
     the intention is to provide an alternative handler for those two
     options.  We need an alternative handler because the memory
     regions are not set up until after the command line arguments
     have been parsed, and so we cannot display the memory info whilst
     processing the command line.  There is a hack in sim_open to
     remove these handlers when we want the real --memory-info option
     to work.  */
  { { "info-memory", no_argument, NULL, OPTION_INFO_MEMORY },
    '\0', NULL, "List configured memory regions", mips_option_handler },
  { { "memory-info", no_argument, NULL, OPTION_INFO_MEMORY },
    '\0', NULL, NULL, mips_option_handler },
  
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};


int interrupt_pending;

void
interrupt_event (SIM_DESC sd, void *data)
{
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */
  address_word cia = CIA_GET (cpu);
  if (SR & status_IE)
    {
      interrupt_pending = 0;
      SignalExceptionInterrupt (1); /* interrupt "1" */
    }
  else if (!interrupt_pending)
    sim_events_schedule (sd, 1, interrupt_event, data);
}


/*---------------------------------------------------------------------------*/
/*-- Device registration hook -----------------------------------------------*/
/*---------------------------------------------------------------------------*/
static void device_init(SIM_DESC sd) {
#ifdef DEVICE_INIT
  extern void register_devices(SIM_DESC);
  register_devices(sd);
#endif
}

/*---------------------------------------------------------------------------*/
/*-- GDB simulator interface ------------------------------------------------*/
/*---------------------------------------------------------------------------*/

SIM_DESC
sim_open (kind, cb, abfd, argv)
     SIM_OPEN_KIND kind;
     host_callback *cb;
     struct bfd *abfd;
     char **argv;
{
  SIM_DESC sd = sim_state_alloc (kind, cb);
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* FIXME: watchpoints code shouldn't need this */
  STATE_WATCHPOINTS (sd)->pc = &(PC);
  STATE_WATCHPOINTS (sd)->sizeof_pc = sizeof (PC);
  STATE_WATCHPOINTS (sd)->interrupt_handler = interrupt_event;

  /* Initialize the mechanism for doing insn profiling.  */
  CPU_INSN_NAME (cpu) = get_insn_name;
  CPU_MAX_INSNS (cpu) = nr_itable_entries;

  STATE = 0;
  
  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    return 0;
  sim_add_option_table (sd, NULL, mips_options);


  /* getopt will print the error message so we just have to exit if this fails.
     FIXME: Hmmm...  in the case of gdb we need getopt to call
     print_filtered.  */
  if (sim_parse_args (sd, argv) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
	 file descriptor leaks, etc.  */
      sim_module_uninstall (sd);
      return 0;
    }

  /* handle board-specific memory maps */
  if (board == NULL)
    {
      /* Allocate core managed memory */
      sim_memopt *entry, *match = NULL;
      address_word mem_size = 0;
      int mapped = 0;

      /* For compatibility with the old code - under this (at level one)
	 are the kernel spaces K0 & K1.  Both of these map to a single
	 smaller sub region */
      sim_do_command(sd," memory region 0x7fff8000,0x8000") ; /* MTZ- 32 k stack */

      /* Look for largest memory region defined on command-line at
	 phys address 0. */
#ifdef SIM_HAVE_FLATMEM
      mem_size = STATE_MEM_SIZE (sd);
#endif
      for (entry = STATE_MEMOPT (sd); entry != NULL; entry = entry->next)
	{
	  /* If we find an entry at address 0, then we will end up
	     allocating a new buffer in the "memory alias" command
	     below. The region at address 0 will be deleted. */
	  address_word size = (entry->modulo != 0
			       ? entry->modulo : entry->nr_bytes);
	  if (entry->addr == 0
	      && (!match || entry->level < match->level))
	    match = entry;
	  else if (entry->addr == K0BASE || entry->addr == K1BASE)
	    mapped = 1;
	  else
	    {
	      sim_memopt *alias;
	      for (alias = entry->alias; alias != NULL; alias = alias->next)
		{
		  if (alias->addr == 0
		      && (!match || entry->level < match->level))
		    match = entry;
		  else if (alias->addr == K0BASE || alias->addr == K1BASE)
		    mapped = 1;
		}
	    }
	}

      if (!mapped)
	{
	  if (match)
	    {
	      /* Get existing memory region size. */
	      mem_size = (match->modulo != 0
			  ? match->modulo : match->nr_bytes);
	      /* Delete old region. */
	      sim_do_commandf (sd, "memory delete %d:0x%lx@%d",
			       match->space, match->addr, match->level);
	    }	      
	  else if (mem_size == 0)
	    mem_size = MEM_SIZE;
	  /* Limit to KSEG1 size (512MB) */
	  if (mem_size > K1SIZE)
	    mem_size = K1SIZE;
	  /* memory alias K1BASE@1,K1SIZE%MEMSIZE,K0BASE */
	  sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx%%0x%lx,0x%0x",
			   K1BASE, K1SIZE, (long)mem_size, K0BASE);
	}

      device_init(sd);
    }
  else if (board != NULL
	   && (strcmp(board, BOARD_BSP) == 0))
    {
      int i;

      STATE_ENVIRONMENT (sd) = OPERATING_ENVIRONMENT;

      /* ROM: 0x9FC0_0000 - 0x9FFF_FFFF and 0xBFC0_0000 - 0xBFFF_FFFF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
		       0x9FC00000, 
		       4 * 1024 * 1024, /* 4 MB */
		       0xBFC00000);

      /* SRAM: 0x8000_0000 - 0x803F_FFFF and 0xA000_0000 - 0xA03F_FFFF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
		       0x80000000, 
		       4 * 1024 * 1024, /* 4 MB */
		       0xA0000000);

      /* DRAM: 0x8800_0000 - 0x89FF_FFFF and 0xA800_0000 - 0xA9FF_FFFF */
      for (i=0; i<8; i++) /* 32 MB total */
	{
	  unsigned size = 4 * 1024 * 1024;  /* 4 MB */
	  sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
			   0x88000000 + (i * size), 
			   size, 
			   0xA8000000 + (i * size));
	}
    }
#if (WITH_HW)
  else if (board != NULL
	   && (strcmp(board, BOARD_JMR3904) == 0 ||
	       strcmp(board, BOARD_JMR3904_PAL) == 0 ||
	       strcmp(board, BOARD_JMR3904_DEBUG) == 0))
    {
      /* match VIRTUAL memory layout of JMR-TX3904 board */
      int i;

      /* --- disable monitor unless forced on by user --- */

      if (! firmware_option_p)
	{
	  idt_monitor_base = 0;
	  pmon_monitor_base = 0;
	  lsipmon_monitor_base = 0;
	}

      /* --- environment --- */

      STATE_ENVIRONMENT (sd) = OPERATING_ENVIRONMENT;

      /* --- memory --- */

      /* ROM: 0x9FC0_0000 - 0x9FFF_FFFF and 0xBFC0_0000 - 0xBFFF_FFFF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
		       0x9FC00000, 
		       4 * 1024 * 1024, /* 4 MB */
		       0xBFC00000);

      /* SRAM: 0x8000_0000 - 0x803F_FFFF and 0xA000_0000 - 0xA03F_FFFF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
		       0x80000000, 
		       4 * 1024 * 1024, /* 4 MB */
		       0xA0000000);

      /* DRAM: 0x8800_0000 - 0x89FF_FFFF and 0xA800_0000 - 0xA9FF_FFFF */
      for (i=0; i<8; i++) /* 32 MB total */
	{
	  unsigned size = 4 * 1024 * 1024;  /* 4 MB */
	  sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx,0x%0x",
			   0x88000000 + (i * size), 
			   size, 
			   0xA8000000 + (i * size));
	}

      /* Dummy memory regions for unsimulated devices - sorted by address */

      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xB1000000, 0x400); /* ISA I/O */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xB2100000, 0x004); /* ISA ctl */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xB2500000, 0x004); /* LED/switch */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xB2700000, 0x004); /* RTC */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xB3C00000, 0x004); /* RTC */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xFFFF8000, 0x900); /* DRAMC */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xFFFF9000, 0x200); /* EBIF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xFFFFE000, 0x01c); /* EBIF */
      sim_do_commandf (sd, "memory alias 0x%lx@1,0x%lx", 0xFFFFF500, 0x300); /* PIO */


      /* --- simulated devices --- */
      sim_hw_parse (sd, "/tx3904irc@0xffffc000/reg 0xffffc000 0x20");
      sim_hw_parse (sd, "/tx3904cpu");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff000/reg 0xfffff000 0x100");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff100/reg 0xfffff100 0x100");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff200/reg 0xfffff200 0x100");
      sim_hw_parse (sd, "/tx3904sio@0xfffff300/reg 0xfffff300 0x100");
      {
	/* FIXME: poking at dv-sockser internals, use tcp backend if
	 --sockser_addr option was given.*/
	extern char* sockser_addr;
	if(sockser_addr == NULL)
	  sim_hw_parse (sd, "/tx3904sio@0xfffff300/backend stdio");
	else
	  sim_hw_parse (sd, "/tx3904sio@0xfffff300/backend tcp");
      }
      sim_hw_parse (sd, "/tx3904sio@0xfffff400/reg 0xfffff400 0x100");
      sim_hw_parse (sd, "/tx3904sio@0xfffff400/backend stdio");

      /* -- device connections --- */
      sim_hw_parse (sd, "/tx3904irc > ip level /tx3904cpu");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff000 > int tmr0 /tx3904irc");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff100 > int tmr1 /tx3904irc");
      sim_hw_parse (sd, "/tx3904tmr@0xfffff200 > int tmr2 /tx3904irc");
      sim_hw_parse (sd, "/tx3904sio@0xfffff300 > int sio0 /tx3904irc");
      sim_hw_parse (sd, "/tx3904sio@0xfffff400 > int sio1 /tx3904irc");

      /* add PAL timer & I/O module */
      if(! strcmp(board, BOARD_JMR3904_PAL))
	{
	 /* the device */
	 sim_hw_parse (sd, "/pal@0xffff0000");
	 sim_hw_parse (sd, "/pal@0xffff0000/reg 0xffff0000 64");

	 /* wire up interrupt ports to irc */
	 sim_hw_parse (sd, "/pal@0x31000000 > countdown tmr0 /tx3904irc");
	 sim_hw_parse (sd, "/pal@0x31000000 > timer tmr1 /tx3904irc");
	 sim_hw_parse (sd, "/pal@0x31000000 > int int0 /tx3904irc");
	}

      if(! strcmp(board, BOARD_JMR3904_DEBUG))
	{
	  /* -- DEBUG: glue interrupt generators --- */
	  sim_hw_parse (sd, "/glue@0xffff0000/reg 0xffff0000 0x50");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int0 int0 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int1 int1 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int2 int2 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int3 int3 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int4 int4 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int5 int5 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int6 int6 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int7 int7 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int8 dmac0 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int9 dmac1 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int10 dmac2 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int11 dmac3 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int12 sio0 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int13 sio1 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int14 tmr0 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int15 tmr1 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int16 tmr2 /tx3904irc");
	  sim_hw_parse (sd, "/glue@0xffff0000 > int17 nmi /tx3904cpu");
	}

      device_init(sd);
    }
#endif

  if (display_mem_info)
    {
      struct option_list * ol;
      struct option_list * prev;

      /* This is a hack.  We want to execute the real --memory-info command
	 line switch which is handled in common/sim-memopts.c, not the
	 override we have defined in this file.  So we remove the
	 mips_options array from the state options list.  This is safe
         because we have now processed all of the command line.  */
      for (ol = STATE_OPTIONS (sd), prev = NULL;
	   ol != NULL;
	   prev = ol, ol = ol->next)
	if (ol->options == mips_options)
	  break;

      SIM_ASSERT (ol != NULL);

      if (prev == NULL)
	STATE_OPTIONS (sd) = ol->next;
      else
	prev->next = ol->next;

      sim_do_commandf (sd, "memory-info");
    }

  /* check for/establish the a reference program image */
  if (sim_analyze_program (sd,
			   (STATE_PROG_ARGV (sd) != NULL
			    ? *STATE_PROG_ARGV (sd)
			    : NULL),
			   abfd) != SIM_RC_OK)
    {
      sim_module_uninstall (sd);
      return 0;
    }

  /* Configure/verify the target byte order and other runtime
     configuration options */
  if (sim_config (sd) != SIM_RC_OK)
    {
      sim_module_uninstall (sd);
      return 0;
    }

  if (sim_post_argv_init (sd) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
	 file descriptor leaks, etc.  */
      sim_module_uninstall (sd);
      return 0;
    }

  /* verify assumptions the simulator made about the host type system.
     This macro does not return if there is a problem */
  SIM_ASSERT (sizeof(int) == (4 * sizeof(char)));
  SIM_ASSERT (sizeof(word64) == (8 * sizeof(char)));

  /* This is NASTY, in that we are assuming the size of specific
     registers: */
  {
    int rn;
    for (rn = 0; (rn < (LAST_EMBED_REGNUM + 1)); rn++)
      {
	if (rn < 32)
	  cpu->register_widths[rn] = WITH_TARGET_WORD_BITSIZE;
	else if ((rn >= FGR_BASE) && (rn < (FGR_BASE + NR_FGR)))
	  cpu->register_widths[rn] = WITH_TARGET_FLOATING_POINT_BITSIZE;
	else if ((rn >= 33) && (rn <= 37))
	  cpu->register_widths[rn] = WITH_TARGET_WORD_BITSIZE;
	else if ((rn == SRIDX)
		 || (rn == FCR0IDX)
		 || (rn == FCR31IDX)
		 || ((rn >= 72) && (rn <= 89)))
	  cpu->register_widths[rn] = 32;
	else
	  cpu->register_widths[rn] = 0;
      }


  }

#if defined(TRACE)
  if (STATE & simTRACE)
    open_trace(sd);
#endif /* TRACE */

  /*
  sim_io_eprintf (sd, "idt@%x pmon@%x lsipmon@%x\n", 
		  idt_monitor_base,
		  pmon_monitor_base, 
		  lsipmon_monitor_base);
  */

  /* Write the monitor trap address handlers into the monitor (eeprom)
     address space.  This can only be done once the target endianness
     has been determined. */
  if (idt_monitor_base != 0)
    {
      unsigned loop;
      unsigned idt_monitor_size = 1 << 11;

      /* the default monitor region */
      sim_do_commandf (sd, "memory region 0x%x,0x%x",
		       idt_monitor_base, idt_monitor_size);

      /* Entry into the IDT monitor is via fixed address vectors, and
	 not using machine instructions. To avoid clashing with use of
	 the MIPS TRAP system, we place our own (simulator specific)
	 "undefined" instructions into the relevant vector slots. */
      for (loop = 0; (loop < idt_monitor_size); loop += 4)
	{
	  address_word vaddr = (idt_monitor_base + loop);
	  unsigned32 insn = (RSVD_INSTRUCTION |
			     (((loop >> 2) & RSVD_INSTRUCTION_ARG_MASK)
			      << RSVD_INSTRUCTION_ARG_SHIFT));
	  H2T (insn);
	  sim_write (sd, vaddr, (char *)&insn, sizeof (insn));
	}
    }

  if ((pmon_monitor_base != 0) || (lsipmon_monitor_base != 0))
    {
    /* The PMON monitor uses the same address space, but rather than
       branching into it the address of a routine is loaded. We can
       cheat for the moment, and direct the PMON routine to IDT style
       instructions within the monitor space. This relies on the IDT
       monitor not using the locations from 0xBFC00500 onwards as its
       entry points.*/
      unsigned loop;
      for (loop = 0; (loop < 24); loop++)
	{
	  unsigned32 value = ((0x500 - 8) / 8); /* default UNDEFINED reason code */
	  switch (loop)
	    {
            case 0: /* read */
              value = 7;
              break;
            case 1: /* write */
              value = 8;
              break;
            case 2: /* open */
              value = 6;
              break;
            case 3: /* close */
              value = 10;
              break;
            case 5: /* printf */
              value = ((0x500 - 16) / 8); /* not an IDT reason code */
              break;
            case 8: /* cliexit */
              value = 17;
              break;
            case 11: /* flush_cache */
              value = 28;
              break;
          }

	SIM_ASSERT (idt_monitor_base != 0);
        value = ((unsigned int) idt_monitor_base + (value * 8));
	H2T (value);

	if (pmon_monitor_base != 0)
	  {
	    address_word vaddr = (pmon_monitor_base + (loop * 4));
	    sim_write (sd, vaddr, (char *)&value, sizeof (value));
	  }

	if (lsipmon_monitor_base != 0)
	  {
	    address_word vaddr = (lsipmon_monitor_base + (loop * 4));
	    sim_write (sd, vaddr, (char *)&value, sizeof (value));
	  }
      }

  /* Write an abort sequence into the TRAP (common) exception vector
     addresses.  This is to catch code executing a TRAP (et.al.)
     instruction without installing a trap handler. */
  if ((idt_monitor_base != 0) || 
      (pmon_monitor_base != 0) || 
      (lsipmon_monitor_base != 0))
    {
      unsigned32 halt[2] = { 0x2404002f /* addiu r4, r0, 47 */,
			     HALT_INSTRUCTION /* BREAK */ };
      H2T (halt[0]);
      H2T (halt[1]);
      sim_write (sd, 0x80000000, (char *) halt, sizeof (halt));
      sim_write (sd, 0x80000180, (char *) halt, sizeof (halt));
      sim_write (sd, 0x80000200, (char *) halt, sizeof (halt));
      /* XXX: Write here unconditionally? */
      sim_write (sd, 0xBFC00200, (char *) halt, sizeof (halt));
      sim_write (sd, 0xBFC00380, (char *) halt, sizeof (halt));
      sim_write (sd, 0xBFC00400, (char *) halt, sizeof (halt));
    }
  }



  return sd;
}

#if defined(TRACE)
static void
open_trace(sd)
     SIM_DESC sd;
{
  tracefh = fopen(tracefile,"wb+");
  if (tracefh == NULL)
    {
      sim_io_eprintf(sd,"Failed to create file \"%s\", writing trace information to stderr.\n",tracefile);
      tracefh = stderr;
  }
}
#endif /* TRACE */

/* Return name of an insn, used by insn profiling.  */
static const char *
get_insn_name (sim_cpu *cpu, int i)
{
  return itable[i].name;
}

void
sim_close (sd, quitting)
     SIM_DESC sd;
     int quitting;
{
#ifdef DEBUG
  printf("DBG: sim_close: entered (quitting = %d)\n",quitting);
#endif


  /* "quitting" is non-zero if we cannot hang on errors */

  /* shut down modules */
  sim_module_uninstall (sd);

  /* Ensure that any resources allocated through the callback
     mechanism are released: */
  sim_io_shutdown (sd);

#if defined(TRACE)
  if (tracefh != NULL && tracefh != stderr)
   fclose(tracefh);
  tracefh = NULL;
#endif /* TRACE */

  /* FIXME - free SD */

  return;
}


int
sim_write (sd,addr,buffer,size)
     SIM_DESC sd;
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int index;
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */

  /* Return the number of bytes written, or zero if error. */
#ifdef DEBUG
  sim_io_printf(sd,"sim_write(0x%s,buffer,%d);\n",pr_addr(addr),size);
#endif

  /* We use raw read and write routines, since we do not want to count
     the GDB memory accesses in our statistics gathering. */

  for (index = 0; index < size; index++)
    {
      address_word vaddr = (address_word)addr + index;
      address_word paddr;
      int cca;
      if (!address_translation (SD, CPU, NULL_CIA, vaddr, isDATA, isSTORE, &paddr, &cca, isRAW))
	break;
      if (sim_core_write_buffer (SD, CPU, read_map, buffer + index, paddr, 1) != 1)
	break;
    }

  return(index);
}

int
sim_read (sd,addr,buffer,size)
     SIM_DESC sd;
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int index;
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */

  /* Return the number of bytes read, or zero if error. */
#ifdef DEBUG
  sim_io_printf(sd,"sim_read(0x%s,buffer,%d);\n",pr_addr(addr),size);
#endif /* DEBUG */

  for (index = 0; (index < size); index++)
    {
      address_word vaddr = (address_word)addr + index;
      address_word paddr;
      int cca;
      if (!address_translation (SD, CPU, NULL_CIA, vaddr, isDATA, isLOAD, &paddr, &cca, isRAW))
	break;
      if (sim_core_read_buffer (SD, CPU, read_map, buffer + index, paddr, 1) != 1)
	break;
    }

  return(index);
}

int
sim_store_register (sd,rn,memory,length)
     SIM_DESC sd;
     int rn;
     unsigned char *memory;
     int length;
{
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */
  /* NOTE: gdb (the client) stores registers in target byte order
     while the simulator uses host byte order */
#ifdef DEBUG
  sim_io_printf(sd,"sim_store_register(%d,*memory=0x%s);\n",rn,pr_addr(*((SIM_ADDR *)memory)));
#endif /* DEBUG */

  /* Unfortunately this suffers from the same problem as the register
     numbering one. We need to know what the width of each logical
     register number is for the architecture being simulated. */

  if (cpu->register_widths[rn] == 0)
    {
      sim_io_eprintf(sd,"Invalid register width for %d (register store ignored)\n",rn);
      return 0;
    }



  if (rn >= FGR_BASE && rn < FGR_BASE + NR_FGR)
    {
      cpu->fpr_state[rn - FGR_BASE] = fmt_uninterpreted;
      if (cpu->register_widths[rn] == 32)
	{
	  if (length == 8)
	    {
	      cpu->fgr[rn - FGR_BASE] = 
		(unsigned32) T2H_8 (*(unsigned64*)memory);
	      return 8;
	    }
	  else
	    {
	      cpu->fgr[rn - FGR_BASE] = T2H_4 (*(unsigned32*)memory);
	      return 4;
	    }
	}
      else
	{
          if (length == 8)
	    {
	      cpu->fgr[rn - FGR_BASE] = T2H_8 (*(unsigned64*)memory);
	      return 8;
	    }
	  else
	    {
	      cpu->fgr[rn - FGR_BASE] = T2H_4 (*(unsigned32*)memory);
	      return 4;
	    }
	}
    }

  if (cpu->register_widths[rn] == 32)
    {
      if (length == 8)
	{
	  cpu->registers[rn] =
	    (unsigned32) T2H_8 (*(unsigned64*)memory);
	  return 8;
	}
      else
	{
	  cpu->registers[rn] = T2H_4 (*(unsigned32*)memory);
	  return 4;
	}
    }
  else
    {
      if (length == 8)
	{
	  cpu->registers[rn] = T2H_8 (*(unsigned64*)memory);
	  return 8;
	}
      else
	{
	  cpu->registers[rn] = (signed32) T2H_4(*(unsigned32*)memory);
	  return 4;
	}
    }

  return 0;
}

int
sim_fetch_register (sd,rn,memory,length)
     SIM_DESC sd;
     int rn;
     unsigned char *memory;
     int length;
{
  sim_cpu *cpu = STATE_CPU (sd, 0); /* FIXME */
  /* NOTE: gdb (the client) stores registers in target byte order
     while the simulator uses host byte order */
#ifdef DEBUG
#if 0  /* FIXME: doesn't compile */
  sim_io_printf(sd,"sim_fetch_register(%d=0x%s,mem) : place simulator registers into memory\n",rn,pr_addr(registers[rn]));
#endif
#endif /* DEBUG */

  if (cpu->register_widths[rn] == 0)
    {
      sim_io_eprintf (sd, "Invalid register width for %d (register fetch ignored)\n",rn);
      return 0;
    }



  /* Any floating point register */
  if (rn >= FGR_BASE && rn < FGR_BASE + NR_FGR)
    {
      if (cpu->register_widths[rn] == 32)
	{
	  if (length == 8)
	    {
	      *(unsigned64*)memory =
		H2T_8 ((unsigned32) (cpu->fgr[rn - FGR_BASE]));
	      return 8;
	    }
	  else
	    {
	      *(unsigned32*)memory = H2T_4 (cpu->fgr[rn - FGR_BASE]);
	      return 4;
	    }
	}
      else
	{
	  if (length == 8)
	    {
	      *(unsigned64*)memory = H2T_8 (cpu->fgr[rn - FGR_BASE]);
	      return 8;
	    }
	  else
	    {
	      *(unsigned32*)memory = H2T_4 ((unsigned32)(cpu->fgr[rn - FGR_BASE]));
	      return 4;
	    }
	}
    }

  if (cpu->register_widths[rn] == 32)
    {
      if (length == 8)
	{
	  *(unsigned64*)memory =
	    H2T_8 ((unsigned32) (cpu->registers[rn]));
	  return 8;
	}
      else
	{
	  *(unsigned32*)memory = H2T_4 ((unsigned32)(cpu->registers[rn]));
	  return 4;
	}
    }
  else
    {
      if (length == 8)
	{
	  *(unsigned64*)memory =
	    H2T_8 ((unsigned64) (cpu->registers[rn]));
	  return 8;
	}
      else
	{
	  *(unsigned32*)memory = H2T_4 ((unsigned32)(cpu->registers[rn]));
	  return 4;
	}
    }

  return 0;
}


SIM_RC
sim_create_inferior (sd, abfd, argv,env)
     SIM_DESC sd;
     struct bfd *abfd;
     char **argv;
     char **env;
{

#ifdef DEBUG
#if 0 /* FIXME: doesn't compile */
  printf("DBG: sim_create_inferior entered: start_address = 0x%s\n",
	 pr_addr(PC));
#endif
#endif /* DEBUG */

  ColdReset(sd);

  if (abfd != NULL)
    {
      /* override PC value set by ColdReset () */
      int cpu_nr;
      for (cpu_nr = 0; cpu_nr < sim_engine_nr_cpus (sd); cpu_nr++)
	{
	  sim_cpu *cpu = STATE_CPU (sd, cpu_nr);
	  CIA_SET (cpu, (unsigned64) bfd_get_start_address (abfd));
	}
    }

#if 0 /* def DEBUG */
  if (argv || env)
    {
      /* We should really place the argv slot values into the argument
	 registers, and onto the stack as required. However, this
	 assumes that we have a stack defined, which is not
	 necessarily true at the moment. */
      char **cptr;
      sim_io_printf(sd,"sim_create_inferior() : passed arguments ignored\n");
      for (cptr = argv; (cptr && *cptr); cptr++)
	printf("DBG: arg \"%s\"\n",*cptr);
    }
#endif /* DEBUG */

  return SIM_RC_OK;
}

void
sim_do_command (sd,cmd)
     SIM_DESC sd;
     char *cmd;
{
  if (sim_args_command (sd, cmd) != SIM_RC_OK)
    sim_io_printf (sd, "Error: \"%s\" is not a valid MIPS simulator command.\n",
		   cmd);
}

/*---------------------------------------------------------------------------*/
/*-- Private simulator support interface ------------------------------------*/
/*---------------------------------------------------------------------------*/

/* Read a null terminated string from memory, return in a buffer */
static char *
fetch_str (SIM_DESC sd,
	   address_word addr)
{
  char *buf;
  int nr = 0;
  char null;
  while (sim_read (sd, addr + nr, &null, 1) == 1 && null != 0)
    nr++;
  buf = NZALLOC (char, nr + 1);
  sim_read (sd, addr, buf, nr);
  return buf;
}


/* Implements the "sim firmware" command:
	sim firmware NAME[@ADDRESS] --- emulate ROM monitor named NAME.
		NAME can be idt, pmon, or lsipmon.  If omitted, ADDRESS
		defaults to the normal address for that monitor.
	sim firmware none --- don't emulate any ROM monitor.  Useful
		if you need a clean address space.  */
static SIM_RC
sim_firmware_command (SIM_DESC sd, char *arg)
{
  int address_present = 0;
  SIM_ADDR address;

  /* Signal occurrence of this option. */
  firmware_option_p = 1;

  /* Parse out the address, if present.  */
  {
    char *p = strchr (arg, '@');
    if (p)
      {
	char *q;
	address_present = 1;
	p ++; /* skip over @ */

	address = strtoul (p, &q, 0);
	if (*q != '\0') 
	  {
	    sim_io_printf (sd, "Invalid address given to the"
			   "`sim firmware NAME@ADDRESS' command: %s\n",
			   p);
	    return SIM_RC_FAIL;
	  }
      }
    else
      {
	address_present = 0;
	address = -1; /* Dummy value.  */
      }
  }

  if (! strncmp (arg, "idt", 3))
    {
      idt_monitor_base = address_present ? address : 0xBFC00000;
      pmon_monitor_base = 0;
      lsipmon_monitor_base = 0;
    }
  else if (! strncmp (arg, "pmon", 4))
    {
      /* pmon uses indirect calls.  Hook into implied idt. */
      pmon_monitor_base = address_present ? address : 0xBFC00500;
      idt_monitor_base = pmon_monitor_base - 0x500;
      lsipmon_monitor_base = 0;
    }
  else if (! strncmp (arg, "lsipmon", 7))
    {
      /* lsipmon uses indirect calls.  Hook into implied idt. */
      pmon_monitor_base = 0;
      lsipmon_monitor_base = address_present ? address : 0xBFC00200;
      idt_monitor_base = lsipmon_monitor_base - 0x200;
    }
  else if (! strncmp (arg, "none", 4))
    {
      if (address_present)
	{
	  sim_io_printf (sd,
			 "The `sim firmware none' command does "
			 "not take an `ADDRESS' argument.\n");
	  return SIM_RC_FAIL;
	}
      idt_monitor_base = 0;
      pmon_monitor_base = 0;
      lsipmon_monitor_base = 0;
    }
  else
    {
      sim_io_printf (sd, "\
Unrecognized name given to the `sim firmware NAME' command: %s\n\
Recognized firmware names are: `idt', `pmon', `lsipmon', and `none'.\n",
		     arg);
      return SIM_RC_FAIL;
    }
  
  return SIM_RC_OK;
}



/* Simple monitor interface (currently setup for the IDT and PMON monitors) */
int
sim_monitor (SIM_DESC sd,
	     sim_cpu *cpu,
	     address_word cia,
	     unsigned int reason)
{
#ifdef DEBUG
  printf("DBG: sim_monitor: entered (reason = %d)\n",reason);
#endif /* DEBUG */

  /* The IDT monitor actually allows two instructions per vector
     slot. However, the simulator currently causes a trap on each
     individual instruction. We cheat, and lose the bottom bit. */
  reason >>= 1;

  /* The following callback functions are available, however the
     monitor we are simulating does not make use of them: get_errno,
     isatty, lseek, rename, system, time and unlink */
  switch (reason)
    {

    case 6: /* int open(char *path,int flags) */
      {
	char *path = fetch_str (sd, A0);
	V0 = sim_io_open (sd, path, (int)A1);
	zfree (path);
	break;
      }

    case 7: /* int read(int file,char *ptr,int len) */
      {
	int fd = A0;
	int nr = A2;
	char *buf = zalloc (nr);
	V0 = sim_io_read (sd, fd, buf, nr);
	sim_write (sd, A1, buf, nr);
	zfree (buf);
      }
      break;

    case 8: /* int write(int file,char *ptr,int len) */
      {
	int fd = A0;
	int nr = A2;
	char *buf = zalloc (nr);
	sim_read (sd, A1, buf, nr);
	V0 = sim_io_write (sd, fd, buf, nr);
	if (fd == 1)
	    sim_io_flush_stdout (sd);
	else if (fd == 2)
	    sim_io_flush_stderr (sd);
	zfree (buf);
	break;
      }

    case 10: /* int close(int file) */
      {
	V0 = sim_io_close (sd, (int)A0);
	break;
      }

    case 2:  /* Densan monitor: char inbyte(int waitflag) */
      {
	if (A0 == 0)	/* waitflag == NOWAIT */
	  V0 = (unsigned_word)-1;
      }
     /* Drop through to case 11 */

    case 11: /* char inbyte(void) */
      {
        char tmp;
	/* ensure that all output has gone... */
	sim_io_flush_stdout (sd);
        if (sim_io_read_stdin (sd, &tmp, sizeof(char)) != sizeof(char))
	  {
	    sim_io_error(sd,"Invalid return from character read");
	    V0 = (unsigned_word)-1;
	  }
        else
	  V0 = (unsigned_word)tmp;
	break;
      }

    case 3:  /* Densan monitor: void co(char chr) */
    case 12: /* void outbyte(char chr) : write a byte to "stdout" */
      {
        char tmp = (char)(A0 & 0xFF);
        sim_io_write_stdout (sd, &tmp, sizeof(char));
	break;
      }

    case 17: /* void _exit() */
      {
	sim_io_eprintf (sd, "sim_monitor(17): _exit(int reason) to be coded\n");
	sim_engine_halt (SD, CPU, NULL, NULL_CIA, sim_exited,
			 (unsigned int)(A0 & 0xFFFFFFFF));
	break;
      }

    case 28: /* PMON flush_cache */
      break;

    case 55: /* void get_mem_info(unsigned int *ptr) */
      /* in:  A0 = pointer to three word memory location */
      /* out: [A0 + 0] = size */
      /*      [A0 + 4] = instruction cache size */
      /*      [A0 + 8] = data cache size */
      {
	unsigned_4 value;
	unsigned_4 zero = 0;
	address_word mem_size;
	sim_memopt *entry, *match = NULL;

	/* Search for memory region mapped to KSEG0 or KSEG1. */
	for (entry = STATE_MEMOPT (sd); 
	     entry != NULL;
	     entry = entry->next)
	  {
	    if ((entry->addr == K0BASE || entry->addr == K1BASE)
		&& (!match || entry->level < match->level))
	      match = entry;
	    else
	      {
		sim_memopt *alias;
		for (alias = entry->alias; 
		     alias != NULL;
		     alias = alias->next)
		  if ((alias->addr == K0BASE || alias->addr == K1BASE)
		      && (!match || entry->level < match->level))
		    match = entry;
	      }
	  }

	/* Get region size, limit to KSEG1 size (512MB). */
	SIM_ASSERT (match != NULL);
	mem_size = (match->modulo != 0
		    ? match->modulo : match->nr_bytes);
	if (mem_size > K1SIZE)
	  mem_size = K1SIZE;

	value = mem_size;
	H2T (value);
	sim_write (sd, A0 + 0, (char *)&value, 4);
	sim_write (sd, A0 + 4, (char *)&zero, 4);
	sim_write (sd, A0 + 8, (char *)&zero, 4);
	/* sim_io_eprintf (sd, "sim: get_mem_info() deprecated\n"); */
	break;
      }
    
    case 158: /* PMON printf */
      /* in:  A0 = pointer to format string */
      /*      A1 = optional argument 1 */
      /*      A2 = optional argument 2 */
      /*      A3 = optional argument 3 */
      /* out: void */
      /* The following is based on the PMON printf source */
      {
	address_word s = A0;
	char c;
	signed_word *ap = &A1; /* 1st argument */
        /* This isn't the quickest way, since we call the host print
           routine for every character almost. But it does avoid
           having to allocate and manage a temporary string buffer. */
	/* TODO: Include check that we only use three arguments (A1,
           A2 and A3) */
	while (sim_read (sd, s++, &c, 1) && c != '\0')
	  {
            if (c == '%')
	      {
		char tmp[40];
		enum {FMT_RJUST, FMT_LJUST, FMT_RJUST0, FMT_CENTER} fmt = FMT_RJUST;
		int width = 0, trunc = 0, haddot = 0, longlong = 0;
		while (sim_read (sd, s++, &c, 1) && c != '\0')
		  {
		    if (strchr ("dobxXulscefg%", c))
		      break;
		    else if (c == '-')
		      fmt = FMT_LJUST;
		    else if (c == '0')
		      fmt = FMT_RJUST0;
		    else if (c == '~')
		      fmt = FMT_CENTER;
		    else if (c == '*')
		      {
			if (haddot)
			  trunc = (int)*ap++;
			else
			  width = (int)*ap++;
		      }
		    else if (c >= '1' && c <= '9')
		      {
			address_word t = s;
			unsigned int n;
			while (sim_read (sd, s++, &c, 1) == 1 && isdigit (c))
			  tmp[s - t] = c;
			tmp[s - t] = '\0';
			n = (unsigned int)strtol(tmp,NULL,10);
			if (haddot)
			  trunc = n;
			else
			  width = n;
			s--;
		      }
		    else if (c == '.')
		      haddot = 1;
		  }
		switch (c)
		  {
		  case '%':
		    sim_io_printf (sd, "%%");
		    break;
		  case 's':
		    if ((int)*ap != 0)
		      {
			address_word p = *ap++;
			char ch;
			while (sim_read (sd, p++, &ch, 1) == 1 && ch != '\0')
			  sim_io_printf(sd, "%c", ch);
		      }
		    else
		      sim_io_printf(sd,"(null)");
		    break;
		  case 'c':
		    sim_io_printf (sd, "%c", (int)*ap++);
		    break;
		  default:
		    if (c == 'l')
		      {
			sim_read (sd, s++, &c, 1);
			if (c == 'l')
			  {
			    longlong = 1;
			    sim_read (sd, s++, &c, 1);
			  }
		      }
		    if (strchr ("dobxXu", c))
		      {
			word64 lv = (word64) *ap++;
			if (c == 'b')
			  sim_io_printf(sd,"<binary not supported>");
			else
			  {
			    sprintf (tmp, "%%%s%c", longlong ? "ll" : "", c);
			    if (longlong)
			      sim_io_printf(sd, tmp, lv);
			    else
			      sim_io_printf(sd, tmp, (int)lv);
			  }
		      }
		    else if (strchr ("eEfgG", c))
		      {
			double dbl = *(double*)(ap++);
			sprintf (tmp, "%%%d.%d%c", width, trunc, c);
			sim_io_printf (sd, tmp, dbl);
			trunc = 0;
		      }
		  }
	      }
	    else
	      sim_io_printf(sd, "%c", c);
	  }
	break;
      }

    default:
      /* Unknown reason.  */
      return 0;
  }
  return 1;
}

/* Store a word into memory.  */

static void
store_word (SIM_DESC sd,
	    sim_cpu *cpu,
	    address_word cia,
	    uword64 vaddr,
	    signed_word val)
{
  address_word paddr;
  int uncached;

  if ((vaddr & 3) != 0)
    SignalExceptionAddressStore ();
  else
    {
      if (AddressTranslation (vaddr, isDATA, isSTORE, &paddr, &uncached,
			      isTARGET, isREAL))
	{
	  const uword64 mask = 7;
	  uword64 memval;
	  unsigned int byte;

	  paddr = (paddr & ~mask) | ((paddr & mask) ^ (ReverseEndian << 2));
	  byte = (vaddr & mask) ^ (BigEndianCPU << 2);
	  memval = ((uword64) val) << (8 * byte);
	  StoreMemory (uncached, AccessLength_WORD, memval, 0, paddr, vaddr,
		       isREAL);
	}
    }
}

/* Load a word from memory.  */

static signed_word
load_word (SIM_DESC sd,
	   sim_cpu *cpu,
	   address_word cia,
	   uword64 vaddr)
{
  if ((vaddr & 3) != 0)
    {
      SIM_CORE_SIGNAL (SD, cpu, cia, read_map, AccessLength_WORD+1, vaddr, read_transfer, sim_core_unaligned_signal);
    }
  else
    {
      address_word paddr;
      int uncached;

      if (AddressTranslation (vaddr, isDATA, isLOAD, &paddr, &uncached,
			      isTARGET, isREAL))
	{
	  const uword64 mask = 0x7;
	  const unsigned int reverse = ReverseEndian ? 1 : 0;
	  const unsigned int bigend = BigEndianCPU ? 1 : 0;
	  uword64 memval;
	  unsigned int byte;

	  paddr = (paddr & ~mask) | ((paddr & mask) ^ (reverse << 2));
	  LoadMemory (&memval,NULL,uncached, AccessLength_WORD, paddr, vaddr,
			       isDATA, isREAL);
	  byte = (vaddr & mask) ^ (bigend << 2);
	  return EXTEND32 (memval >> (8 * byte));
	}
    }

  return 0;
}

/* Simulate the mips16 entry and exit pseudo-instructions.  These
   would normally be handled by the reserved instruction exception
   code, but for ease of simulation we just handle them directly.  */

static void
mips16_entry (SIM_DESC sd,
	      sim_cpu *cpu,
	      address_word cia,
	      unsigned int insn)
{
  int aregs, sregs, rreg;

#ifdef DEBUG
  printf("DBG: mips16_entry: entered (insn = 0x%08X)\n",insn);
#endif /* DEBUG */

  aregs = (insn & 0x700) >> 8;
  sregs = (insn & 0x0c0) >> 6;
  rreg =  (insn & 0x020) >> 5;

  /* This should be checked by the caller.  */
  if (sregs == 3)
    abort ();

  if (aregs < 5)
    {
      int i;
      signed_word tsp;

      /* This is the entry pseudo-instruction.  */

      for (i = 0; i < aregs; i++)
	store_word (SD, CPU, cia, (uword64) (SP + 4 * i), GPR[i + 4]);

      tsp = SP;
      SP -= 32;

      if (rreg)
	{
	  tsp -= 4;
	  store_word (SD, CPU, cia, (uword64) tsp, RA);
	}

      for (i = 0; i < sregs; i++)
	{
	  tsp -= 4;
	  store_word (SD, CPU, cia, (uword64) tsp, GPR[16 + i]);
	}
    }
  else
    {
      int i;
      signed_word tsp;

      /* This is the exit pseudo-instruction.  */

      tsp = SP + 32;

      if (rreg)
	{
	  tsp -= 4;
	  RA = load_word (SD, CPU, cia, (uword64) tsp);
	}

      for (i = 0; i < sregs; i++)
	{
	  tsp -= 4;
	  GPR[i + 16] = load_word (SD, CPU, cia, (uword64) tsp);
	}

      SP += 32;

      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
	  if (aregs == 5)
	    {
	      FGR[0] = WORD64LO (GPR[4]);
	      FPR_STATE[0] = fmt_uninterpreted;
	    }
	  else if (aregs == 6)
	    {
	      FGR[0] = WORD64LO (GPR[5]);
	      FGR[1] = WORD64LO (GPR[4]);
	      FPR_STATE[0] = fmt_uninterpreted;
	      FPR_STATE[1] = fmt_uninterpreted;
	    }
	}	  

      PC = RA;
    }
  
}

/*-- trace support ----------------------------------------------------------*/

/* The TRACE support is provided (if required) in the memory accessing
   routines. Since we are also providing the architecture specific
   features, the architecture simulation code can also deal with
   notifying the TRACE world of cache flushes, etc. Similarly we do
   not need to provide profiling support in the simulator engine,
   since we can sample in the instruction fetch control loop. By
   defining the TRACE manifest, we add tracing as a run-time
   option. */

#if defined(TRACE)
/* Tracing by default produces "din" format (as required by
   dineroIII). Each line of such a trace file *MUST* have a din label
   and address field. The rest of the line is ignored, so comments can
   be included if desired. The first field is the label which must be
   one of the following values:

	0       read data
        1       write data
        2       instruction fetch
        3       escape record (treated as unknown access type)
        4       escape record (causes cache flush)

   The address field is a 32bit (lower-case) hexadecimal address
   value. The address should *NOT* be preceded by "0x".

   The size of the memory transfer is not important when dealing with
   cache lines (as long as no more than a cache line can be
   transferred in a single operation :-), however more information
   could be given following the dineroIII requirement to allow more
   complete memory and cache simulators to provide better
   results. i.e. the University of Pisa has a cache simulator that can
   also take bus size and speed as (variable) inputs to calculate
   complete system performance (a much more useful ability when trying
   to construct an end product, rather than a processor). They
   currently have an ARM version of their tool called ChARM. */


void
dotrace (SIM_DESC sd,
	 sim_cpu *cpu,
	 FILE *tracefh,
	 int type,
	 SIM_ADDR address,
	 int width,
	 char *comment,...)
{
  if (STATE & simTRACE) {
    va_list ap;
    fprintf(tracefh,"%d %s ; width %d ; ", 
		type,
		pr_addr(address),
		width);
    va_start(ap,comment);
    vfprintf(tracefh,comment,ap);
    va_end(ap);
    fprintf(tracefh,"\n");
  }
  /* NOTE: Since the "din" format will only accept 32bit addresses, and
     we may be generating 64bit ones, we should put the hi-32bits of the
     address into the comment field. */

  /* TODO: Provide a buffer for the trace lines. We can then avoid
     performing writes until the buffer is filled, or the file is
     being closed. */

  /* NOTE: We could consider adding a comment field to the "din" file
     produced using type 3 markers (unknown access). This would then
     allow information about the program that the "din" is for, and
     the MIPs world that was being simulated, to be placed into the
     trace file. */

  return;
}
#endif /* TRACE */

/*---------------------------------------------------------------------------*/
/*-- simulator engine -------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void
ColdReset (SIM_DESC sd)
{
  int cpu_nr;
  for (cpu_nr = 0; cpu_nr < sim_engine_nr_cpus (sd); cpu_nr++)
    {
      sim_cpu *cpu = STATE_CPU (sd, cpu_nr);
      /* RESET: Fixed PC address: */
      PC = (unsigned_word) UNSIGNED64 (0xFFFFFFFFBFC00000);
      /* The reset vector address is in the unmapped, uncached memory space. */
      
      SR &= ~(status_SR | status_TS | status_RP);
      SR |= (status_ERL | status_BEV);
      
      /* Cheat and allow access to the complete register set immediately */
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT
	  && WITH_TARGET_WORD_BITSIZE == 64)
	SR |= status_FR; /* 64bit registers */
      
      /* Ensure that any instructions with pending register updates are
	 cleared: */
      PENDING_INVALIDATE();
      
      /* Initialise the FPU registers to the unknown state */
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
	  int rn;
	  for (rn = 0; (rn < 32); rn++)
	    FPR_STATE[rn] = fmt_uninterpreted;
	}
      
      /* Initialise the Config0 register. */
      C0_CONFIG = 0x80000000 		/* Config1 present */
	| 2;				/* KSEG0 uncached */
      if (WITH_TARGET_WORD_BITSIZE == 64)
	{
	  /* FIXME Currently mips/sim-main.c:address_translation()
	     truncates all addresses to 32-bits. */
	  if (0 && WITH_TARGET_ADDRESS_BITSIZE == 64)
	    C0_CONFIG |= (2 << 13);	/* MIPS64, 64-bit addresses */
	  else
	    C0_CONFIG |= (1 << 13);	/* MIPS64, 32-bit addresses */
	}
      if (BigEndianMem)
	C0_CONFIG |= 0x00008000;	/* Big Endian */
    }
}




/* Description from page A-26 of the "MIPS IV Instruction Set" manual (revision 3.1) */
/* Signal an exception condition. This will result in an exception
   that aborts the instruction. The instruction operation pseudocode
   will never see a return from this function call. */

void
signal_exception (SIM_DESC sd,
		  sim_cpu *cpu,
		  address_word cia,
		  int exception,...)
{
  /* int vector; */

#ifdef DEBUG
  sim_io_printf(sd,"DBG: SignalException(%d) PC = 0x%s\n",exception,pr_addr(cia));
#endif /* DEBUG */

  /* Ensure that any active atomic read/modify/write operation will fail: */
  LLBIT = 0;

  /* Save registers before interrupt dispatching */
#ifdef SIM_CPU_EXCEPTION_TRIGGER
  SIM_CPU_EXCEPTION_TRIGGER(sd, cpu, cia);
#endif

  switch (exception) {

    case DebugBreakPoint:
      if (! (Debug & Debug_DM))
        {
          if (INDELAYSLOT())
            {
              CANCELDELAYSLOT();
              
              Debug |= Debug_DBD;  /* signaled from within in delay slot */
              DEPC = cia - 4;      /* reference the branch instruction */
            }
          else
            {
              Debug &= ~Debug_DBD; /* not signaled from within a delay slot */
              DEPC = cia;
            }
        
          Debug |= Debug_DM;            /* in debugging mode */
          Debug |= Debug_DBp;           /* raising a DBp exception */
          PC = 0xBFC00200;
          sim_engine_restart (SD, CPU, NULL, NULL_CIA);
        }
      break;

    case ReservedInstruction:
     {
       va_list ap;
       unsigned int instruction;
       va_start(ap,exception);
       instruction = va_arg(ap,unsigned int);
       va_end(ap);
       /* Provide simple monitor support using ReservedInstruction
          exceptions. The following code simulates the fixed vector
          entry points into the IDT monitor by causing a simulator
          trap, performing the monitor operation, and returning to
          the address held in the $ra register (standard PCS return
          address). This means we only need to pre-load the vector
          space with suitable instruction values. For systems were
          actual trap instructions are used, we would not need to
          perform this magic. */
       if ((instruction & RSVD_INSTRUCTION_MASK) == RSVD_INSTRUCTION)
	 {
	   int reason = (instruction >> RSVD_INSTRUCTION_ARG_SHIFT) & RSVD_INSTRUCTION_ARG_MASK;
	   if (!sim_monitor (SD, CPU, cia, reason))
	     sim_io_error (sd, "sim_monitor: unhandled reason = %d, pc = 0x%s\n", reason, pr_addr (cia));

	   /* NOTE: This assumes that a branch-and-link style
	      instruction was used to enter the vector (which is the
	      case with the current IDT monitor). */
	   sim_engine_restart (SD, CPU, NULL, RA);
	 }
       /* Look for the mips16 entry and exit instructions, and
          simulate a handler for them.  */
       else if ((cia & 1) != 0
		&& (instruction & 0xf81f) == 0xe809
		&& (instruction & 0x0c0) != 0x0c0)
	 {
	   mips16_entry (SD, CPU, cia, instruction);
	   sim_engine_restart (sd, NULL, NULL, NULL_CIA);
	 }
       /* else fall through to normal exception processing */
       sim_io_eprintf(sd,"ReservedInstruction at PC = 0x%s\n", pr_addr (cia));
     }

    default:
     /* Store exception code into current exception id variable (used
        by exit code): */

     /* TODO: If not simulating exceptions then stop the simulator
        execution. At the moment we always stop the simulation. */

#ifdef SUBTARGET_R3900
      /* update interrupt-related registers */

      /* insert exception code in bits 6:2 */
      CAUSE = LSMASKED32(CAUSE, 31, 7) | LSINSERTED32(exception, 6, 2);
      /* shift IE/KU history bits left */
      SR = LSMASKED32(SR, 31, 4) | LSINSERTED32(LSEXTRACTED32(SR, 3, 0), 5, 2);

      if (STATE & simDELAYSLOT)
	{
	  STATE &= ~simDELAYSLOT;
	  CAUSE |= cause_BD;
	  EPC = (cia - 4); /* reference the branch instruction */
	}
      else
	EPC = cia;

     if (SR & status_BEV)
       PC = (signed)0xBFC00000 + 0x180;
     else
       PC = (signed)0x80000000 + 0x080;
#else
     /* See figure 5-17 for an outline of the code below */
     if (! (SR & status_EXL))
       {
	 CAUSE = (exception << 2);
	 if (STATE & simDELAYSLOT)
	   {
	     STATE &= ~simDELAYSLOT;
	     CAUSE |= cause_BD;
	     EPC = (cia - 4); /* reference the branch instruction */
	   }
	 else
	   EPC = cia;
	 /* FIXME: TLB et.al. */
	 /* vector = 0x180; */
       }
     else
       {
	 CAUSE = (exception << 2);
	 /* vector = 0x180; */
       }
     SR |= status_EXL;
     /* Store exception code into current exception id variable (used
        by exit code): */

     if (SR & status_BEV)
       PC = (signed)0xBFC00200 + 0x180;
     else
       PC = (signed)0x80000000 + 0x180;
#endif

     switch ((CAUSE >> 2) & 0x1F)
       {
       case Interrupt:
	 /* Interrupts arrive during event processing, no need to
            restart */
	 return;

       case NMIReset:
	 /* Ditto */
#ifdef SUBTARGET_3900
	 /* Exception vector: BEV=0 BFC00000 / BEF=1 BFC00000  */
	 PC = (signed)0xBFC00000;
#endif /* SUBTARGET_3900 */
	 return;

       case TLBModification:
       case TLBLoad:
       case TLBStore:
       case AddressLoad:
       case AddressStore:
       case InstructionFetch:
       case DataReference:
	 /* The following is so that the simulator will continue from the
	    exception handler address. */
	 sim_engine_halt (SD, CPU, NULL, PC,
			  sim_stopped, SIM_SIGBUS);

       case ReservedInstruction:
       case CoProcessorUnusable:
	 PC = EPC;
	 sim_engine_halt (SD, CPU, NULL, PC,
			  sim_stopped, SIM_SIGILL);

       case IntegerOverflow:
       case FPE:
	 sim_engine_halt (SD, CPU, NULL, PC,
			  sim_stopped, SIM_SIGFPE);
	 
       case BreakPoint:
	 sim_engine_halt (SD, CPU, NULL, PC, sim_stopped, SIM_SIGTRAP);
	 break;

       case SystemCall:
       case Trap:
	 sim_engine_restart (SD, CPU, NULL, PC);
	 break;

       case Watch:
	 PC = EPC;
	 sim_engine_halt (SD, CPU, NULL, PC,
			  sim_stopped, SIM_SIGTRAP);

       default: /* Unknown internal exception */
	 PC = EPC;
	 sim_engine_halt (SD, CPU, NULL, PC,
			  sim_stopped, SIM_SIGABRT);

       }

    case SimulatorFault:
     {
       va_list ap;
       char *msg;
       va_start(ap,exception);
       msg = va_arg(ap,char *);
       va_end(ap);
       sim_engine_abort (SD, CPU, NULL_CIA,
			 "FATAL: Simulator error \"%s\"\n",msg);
     }
   }

  return;
}



/* This function implements what the MIPS32 and MIPS64 ISAs define as
   "UNPREDICTABLE" behaviour.

   About UNPREDICTABLE behaviour they say: "UNPREDICTABLE results
   may vary from processor implementation to processor implementation,
   instruction to instruction, or as a function of time on the same
   implementation or instruction.  Software can never depend on results
   that are UNPREDICTABLE. ..."  (MIPS64 Architecture for Programmers
   Volume II, The MIPS64 Instruction Set.  MIPS Document MD00087 revision
   0.95, page 2.)
  
   For UNPREDICTABLE behaviour, we print a message, if possible print
   the offending instructions mips.igen instruction name (provided by
   the caller), and stop the simulator.

   XXX FIXME: eventually, stopping the simulator should be made conditional
   on a command-line option.  */
void
unpredictable_action(sim_cpu *cpu, address_word cia)
{
  SIM_DESC sd = CPU_STATE(cpu);

  sim_io_eprintf(sd, "UNPREDICTABLE: PC = 0x%s\n", pr_addr (cia));
  sim_engine_halt (SD, CPU, NULL, cia, sim_stopped, SIM_SIGABRT);
}


/*-- co-processor support routines ------------------------------------------*/

static int UNUSED
CoProcPresent(unsigned int coproc_number)
{
  /* Return TRUE if simulator provides a model for the given co-processor number */
  return(0);
}

void
cop_lw (SIM_DESC sd,
	sim_cpu *cpu,
	address_word cia,
	int coproc_num,
	int coproc_reg,
	unsigned int memword)
{
  switch (coproc_num)
    {
    case 1:
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
#ifdef DEBUG
	  printf("DBG: COP_LW: memword = 0x%08X (uword64)memword = 0x%s\n",memword,pr_addr(memword));
#endif
	  StoreFPR(coproc_reg,fmt_uninterpreted_32,(uword64)memword);
	  break;
	}

    default:
#if 0 /* this should be controlled by a configuration option */
      sim_io_printf(sd,"COP_LW(%d,%d,0x%08X) at PC = 0x%s : TODO (architecture specific)\n",coproc_num,coproc_reg,memword,pr_addr(cia));
#endif
      break;
    }

  return;
}

void
cop_ld (SIM_DESC sd,
	sim_cpu *cpu,
	address_word cia,
	int coproc_num,
	int coproc_reg,
	uword64 memword)
{

#ifdef DEBUG
  printf("DBG: COP_LD: coproc_num = %d, coproc_reg = %d, value = 0x%s : PC = 0x%s\n", coproc_num, coproc_reg, pr_uword64(memword), pr_addr(cia) );
#endif

  switch (coproc_num) {
    case 1:
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
	  StoreFPR(coproc_reg,fmt_uninterpreted_64,memword);
	  break;
	}

    default:
#if 0 /* this message should be controlled by a configuration option */
     sim_io_printf(sd,"COP_LD(%d,%d,0x%s) at PC = 0x%s : TODO (architecture specific)\n",coproc_num,coproc_reg,pr_addr(memword),pr_addr(cia));
#endif
     break;
  }

  return;
}




unsigned int
cop_sw (SIM_DESC sd,
	sim_cpu *cpu,
	address_word cia,
	int coproc_num,
	int coproc_reg)
{
  unsigned int value = 0;

  switch (coproc_num)
    {
    case 1:
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
	  value = (unsigned int)ValueFPR(coproc_reg,fmt_uninterpreted_32);
	  break;
	}

    default:
#if 0 /* should be controlled by configuration option */
      sim_io_printf(sd,"COP_SW(%d,%d) at PC = 0x%s : TODO (architecture specific)\n",coproc_num,coproc_reg,pr_addr(cia));
#endif
      break;
    }

  return(value);
}

uword64
cop_sd (SIM_DESC sd,
	sim_cpu *cpu,
	address_word cia,
	int coproc_num,
	int coproc_reg)
{
  uword64 value = 0;
  switch (coproc_num)
    {
    case 1:
      if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
	{
	  value = ValueFPR(coproc_reg,fmt_uninterpreted_64);
	  break;
	}

    default:
#if 0 /* should be controlled by configuration option */
      sim_io_printf(sd,"COP_SD(%d,%d) at PC = 0x%s : TODO (architecture specific)\n",coproc_num,coproc_reg,pr_addr(cia));
#endif
      break;
    }

  return(value);
}




void
decode_coproc (SIM_DESC sd,
	       sim_cpu *cpu,
	       address_word cia,
	       unsigned int instruction)
{
  int coprocnum = ((instruction >> 26) & 3);

  switch (coprocnum)
    {
    case 0: /* standard CPU control and cache registers */
      {
        int code = ((instruction >> 21) & 0x1F);
	int rt = ((instruction >> 16) & 0x1F);
	int rd = ((instruction >> 11) & 0x1F);
	int tail = instruction & 0x3ff;
        /* R4000 Users Manual (second edition) lists the following CP0
           instructions:
	                                                           CODE><-RT><RD-><--TAIL--->
	   DMFC0   Doubleword Move From CP0        (VR4100 = 01000000001tttttddddd00000000000)
	   DMTC0   Doubleword Move To CP0          (VR4100 = 01000000101tttttddddd00000000000)
	   MFC0    word Move From CP0              (VR4100 = 01000000000tttttddddd00000000000)
	   MTC0    word Move To CP0                (VR4100 = 01000000100tttttddddd00000000000)
	   TLBR    Read Indexed TLB Entry          (VR4100 = 01000010000000000000000000000001)
	   TLBWI   Write Indexed TLB Entry         (VR4100 = 01000010000000000000000000000010)
	   TLBWR   Write Random TLB Entry          (VR4100 = 01000010000000000000000000000110)
	   TLBP    Probe TLB for Matching Entry    (VR4100 = 01000010000000000000000000001000)
	   CACHE   Cache operation                 (VR4100 = 101111bbbbbpppppiiiiiiiiiiiiiiii)
	   ERET    Exception return                (VR4100 = 01000010000000000000000000011000)
	   */
        if (((code == 0x00) || (code == 0x04)      /* MFC0  /  MTC0  */        
	     || (code == 0x01) || (code == 0x05))  /* DMFC0 / DMTC0  */        
	    && tail == 0)
	  {
	    /* Clear double/single coprocessor move bit. */
	    code &= ~1;

	    /* M[TF]C0 (32 bits) | DM[TF]C0 (64 bits) */
	    
	    switch (rd)  /* NOTEs: Standard CP0 registers */
	      {
		/* 0 = Index               R4000   VR4100  VR4300 */
		/* 1 = Random              R4000   VR4100  VR4300 */
		/* 2 = EntryLo0            R4000   VR4100  VR4300 */
		/* 3 = EntryLo1            R4000   VR4100  VR4300 */
		/* 4 = Context             R4000   VR4100  VR4300 */
		/* 5 = PageMask            R4000   VR4100  VR4300 */
		/* 6 = Wired               R4000   VR4100  VR4300 */
		/* 8 = BadVAddr            R4000   VR4100  VR4300 */
		/* 9 = Count               R4000   VR4100  VR4300 */
		/* 10 = EntryHi            R4000   VR4100  VR4300 */
		/* 11 = Compare            R4000   VR4100  VR4300 */
		/* 12 = SR                 R4000   VR4100  VR4300 */
#ifdef SUBTARGET_R3900
	      case 3:
		/* 3 = Config              R3900                  */
	      case 7:
		/* 7 = Cache               R3900                  */
	      case 15:
		/* 15 = PRID               R3900                  */

		/* ignore */
		break;

	      case 8:
		/* 8 = BadVAddr            R4000   VR4100  VR4300 */
		if (code == 0x00)
		  GPR[rt] = (signed_word) (signed_address) COP0_BADVADDR;
		else
		  COP0_BADVADDR = GPR[rt];
		break;

#endif /* SUBTARGET_R3900 */
	      case 12:
		if (code == 0x00)
		  GPR[rt] = SR;
		else
		  SR = GPR[rt];
		break;
		/* 13 = Cause              R4000   VR4100  VR4300 */
	      case 13:
		if (code == 0x00)
		  GPR[rt] = CAUSE;
		else
		  CAUSE = GPR[rt];
		break;
		/* 14 = EPC                R4000   VR4100  VR4300 */
	      case 14:
		if (code == 0x00)
		  GPR[rt] = (signed_word) (signed_address) EPC;
		else
		  EPC = GPR[rt];
		break;
		/* 15 = PRId               R4000   VR4100  VR4300 */
#ifdef SUBTARGET_R3900
                /* 16 = Debug */
              case 16:
                if (code == 0x00)
                  GPR[rt] = Debug;
                else
                  Debug = GPR[rt];
                break;
#else
		/* 16 = Config             R4000   VR4100  VR4300 */
              case 16:
		if (code == 0x00)
		  GPR[rt] = C0_CONFIG;
		else
		  /* only bottom three bits are writable */
		  C0_CONFIG = (C0_CONFIG & ~0x7) | (GPR[rt] & 0x7);
                break;
#endif
#ifdef SUBTARGET_R3900
                /* 17 = Debug */
              case 17:
                if (code == 0x00)
                  GPR[rt] = DEPC;
                else
                  DEPC = GPR[rt];
                break;
#else
		/* 17 = LLAddr             R4000   VR4100  VR4300 */
#endif
		/* 18 = WatchLo            R4000   VR4100  VR4300 */
		/* 19 = WatchHi            R4000   VR4100  VR4300 */
		/* 20 = XContext           R4000   VR4100  VR4300 */
		/* 26 = PErr or ECC        R4000   VR4100  VR4300 */
		/* 27 = CacheErr           R4000   VR4100 */
		/* 28 = TagLo              R4000   VR4100  VR4300 */
		/* 29 = TagHi              R4000   VR4100  VR4300 */
		/* 30 = ErrorEPC           R4000   VR4100  VR4300 */
		if (STATE_VERBOSE_P(SD))
		  sim_io_eprintf (SD, 
				  "Warning: PC 0x%lx:interp.c decode_coproc DEADC0DE\n",
				  (unsigned long)cia);
		GPR[rt] = 0xDEADC0DE; /* CPR[0,rd] */
		/* CPR[0,rd] = GPR[rt]; */
	      default:
		if (code == 0x00)
		  GPR[rt] = (signed_word) (signed32) COP0_GPR[rd];
		else
		  COP0_GPR[rd] = GPR[rt];
#if 0
		if (code == 0x00)
		  sim_io_printf(sd,"Warning: MFC0 %d,%d ignored, PC=%08x (architecture specific)\n",rt,rd, (unsigned)cia);
		else
		  sim_io_printf(sd,"Warning: MTC0 %d,%d ignored, PC=%08x (architecture specific)\n",rt,rd, (unsigned)cia);
#endif
	      }
	  }
	else if ((code == 0x00 || code == 0x01)
		 && rd == 16)
	  {
	    /* [D]MFC0 RT,C0_CONFIG,SEL */
	    signed32 cfg = 0;
	    switch (tail & 0x07) 
	      {
	      case 0:
		cfg = C0_CONFIG;
		break;
	      case 1:
		/* MIPS32 r/o Config1: 
		   Config2 present */
		cfg = 0x80000000;
		/* MIPS16 implemented. 
		   XXX How to check configuration? */
		cfg |= 0x0000004;
		if (CURRENT_FLOATING_POINT == HARD_FLOATING_POINT)
		  /* MDMX & FPU implemented */
		  cfg |= 0x00000021;
		break;
	      case 2:
		/* MIPS32 r/o Config2: 
		   Config3 present. */
		cfg = 0x80000000;
		break;
	      case 3:
		/* MIPS32 r/o Config3: 
		   SmartMIPS implemented. */
		cfg = 0x00000002;
		break;
	      }
	    GPR[rt] = cfg;
	  }
	else if (code == 0x10 && (tail & 0x3f) == 0x18)
	  {
	    /* ERET */
	    if (SR & status_ERL)
	      {
		/* Oops, not yet available */
		sim_io_printf(sd,"Warning: ERET when SR[ERL] set not handled yet");
		PC = EPC;
		SR &= ~status_ERL;
	      }
	    else
	      {
		PC = EPC;
		SR &= ~status_EXL;
	      }
	  }
        else if (code == 0x10 && (tail & 0x3f) == 0x10)
          {
            /* RFE */
#ifdef SUBTARGET_R3900
	    /* TX39: Copy IEp/KUp -> IEc/KUc, and IEo/KUo -> IEp/KUp */

	    /* shift IE/KU history bits right */
	    SR = LSMASKED32(SR, 31, 4) | LSINSERTED32(LSEXTRACTED32(SR, 5, 2), 3, 0);

	    /* TODO: CACHE register */
#endif /* SUBTARGET_R3900 */
          }
        else if (code == 0x10 && (tail & 0x3f) == 0x1F)
          {
            /* DERET */
            Debug &= ~Debug_DM;
            DELAYSLOT();
            DSPC = DEPC;
          }
	else
	  sim_io_eprintf(sd,"Unrecognised COP0 instruction 0x%08X at PC = 0x%s : No handler present\n",instruction,pr_addr(cia));
        /* TODO: When executing an ERET or RFE instruction we should
           clear LLBIT, to ensure that any out-standing atomic
           read/modify/write sequence fails. */
      }
    break;
    
    case 2: /* co-processor 2 */
      {
	int handle = 0;


	if(! handle)
	  {
	    sim_io_eprintf(sd, "COP2 instruction 0x%08X at PC = 0x%s : No handler present\n",
			   instruction,pr_addr(cia));
	  }
      }
    break;
    
    case 1: /* should not occur (FPU co-processor) */
    case 3: /* should not occur (FPU co-processor) */
      SignalException(ReservedInstruction,instruction);
      break;
    }
  
  return;
}


/* This code copied from gdb's utils.c.  Would like to share this code,
   but don't know of a common place where both could get to it. */

/* Temporary storage using circular buffer */
#define NUMCELLS 16
#define CELLSIZE 32
static char*
get_cell (void)
{
  static char buf[NUMCELLS][CELLSIZE];
  static int cell=0;
  if (++cell>=NUMCELLS) cell=0;
  return buf[cell];
}     

/* Print routines to handle variable size regs, etc */

/* Eliminate warning from compiler on 32-bit systems */
static int thirty_two = 32;	

char* 
pr_addr(addr)
  SIM_ADDR addr;
{
  char *paddr_str=get_cell();
  switch (sizeof(addr))
    {
      case 8:
        sprintf(paddr_str,"%08lx%08lx",
		(unsigned long)(addr>>thirty_two),(unsigned long)(addr&0xffffffff));
	break;
      case 4:
        sprintf(paddr_str,"%08lx",(unsigned long)addr);
	break;
      case 2:
        sprintf(paddr_str,"%04x",(unsigned short)(addr&0xffff));
	break;
      default:
        sprintf(paddr_str,"%x",addr);
    }
  return paddr_str;
}

char* 
pr_uword64(addr)
  uword64 addr;
{
  char *paddr_str=get_cell();
  sprintf(paddr_str,"%08lx%08lx",
          (unsigned long)(addr>>thirty_two),(unsigned long)(addr&0xffffffff));
  return paddr_str;
}


void
mips_core_signal (SIM_DESC sd,
                 sim_cpu *cpu,
                 sim_cia cia,
                 unsigned map,
                 int nr_bytes,
                 address_word addr,
                 transfer_type transfer,
                 sim_core_signals sig)
{
  const char *copy = (transfer == read_transfer ? "read" : "write");
  address_word ip = CIA_ADDR (cia);

  switch (sig)
    {
    case sim_core_unmapped_signal:
      sim_io_eprintf (sd, "mips-core: %d byte %s to unmapped address 0x%lx at 0x%lx\n",
                      nr_bytes, copy, 
		      (unsigned long) addr, (unsigned long) ip);
      COP0_BADVADDR = addr;
      SignalExceptionDataReference();
      break;

    case sim_core_unaligned_signal:
      sim_io_eprintf (sd, "mips-core: %d byte %s to unaligned address 0x%lx at 0x%lx\n",
                      nr_bytes, copy, 
		      (unsigned long) addr, (unsigned long) ip);
      COP0_BADVADDR = addr;
      if(transfer == read_transfer) 
	SignalExceptionAddressLoad();
      else
	SignalExceptionAddressStore();
      break;

    default:
      sim_engine_abort (sd, cpu, cia,
                        "mips_core_signal - internal error - bad switch");
    }
}


void
mips_cpu_exception_trigger(SIM_DESC sd, sim_cpu* cpu, address_word cia)
{
  ASSERT(cpu != NULL);

  if(cpu->exc_suspended > 0)
    sim_io_eprintf(sd, "Warning, nested exception triggered (%d)\n", cpu->exc_suspended); 

  PC = cia;
  memcpy(cpu->exc_trigger_registers, cpu->registers, sizeof(cpu->exc_trigger_registers));
  cpu->exc_suspended = 0;
}

void
mips_cpu_exception_suspend(SIM_DESC sd, sim_cpu* cpu, int exception)
{
  ASSERT(cpu != NULL);

  if(cpu->exc_suspended > 0)
    sim_io_eprintf(sd, "Warning, nested exception signal (%d then %d)\n", 
		   cpu->exc_suspended, exception); 

  memcpy(cpu->exc_suspend_registers, cpu->registers, sizeof(cpu->exc_suspend_registers));
  memcpy(cpu->registers, cpu->exc_trigger_registers, sizeof(cpu->registers));
  cpu->exc_suspended = exception;
}

void
mips_cpu_exception_resume(SIM_DESC sd, sim_cpu* cpu, int exception)
{
  ASSERT(cpu != NULL);

  if(exception == 0 && cpu->exc_suspended > 0)
    {
      /* warn not for breakpoints */
      if(cpu->exc_suspended != sim_signal_to_host(sd, SIM_SIGTRAP))
	sim_io_eprintf(sd, "Warning, resuming but ignoring pending exception signal (%d)\n",
		       cpu->exc_suspended); 
    }
  else if(exception != 0 && cpu->exc_suspended > 0)
    {
      if(exception != cpu->exc_suspended) 
	sim_io_eprintf(sd, "Warning, resuming with mismatched exception signal (%d vs %d)\n",
		       cpu->exc_suspended, exception); 
      
      memcpy(cpu->registers, cpu->exc_suspend_registers, sizeof(cpu->registers)); 
    }
  else if(exception != 0 && cpu->exc_suspended == 0)
    {
      sim_io_eprintf(sd, "Warning, ignoring spontanous exception signal (%d)\n", exception); 
    }
  cpu->exc_suspended = 0; 
}


/*---------------------------------------------------------------------------*/
/*> EOF interp.c <*/
