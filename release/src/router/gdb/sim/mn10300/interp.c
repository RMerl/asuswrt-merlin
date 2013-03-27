#include <signal.h>

#include "sim-main.h"
#include "sim-options.h"
#include "sim-hw.h"

#include "sysdep.h"
#include "bfd.h"
#include "sim-assert.h"


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

#include "bfd.h"

#ifndef INLINE
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif
#endif


host_callback *mn10300_callback;
int mn10300_debug;
struct _state State;


/* simulation target board.  NULL=default configuration */
static char* board = NULL;

static DECLARE_OPTION_HANDLER (mn10300_option_handler);

enum {
  OPTION_BOARD = OPTION_START,
};

static SIM_RC
mn10300_option_handler (SIM_DESC sd,
			sim_cpu *cpu,
			int opt,
			char *arg,
			int is_command)
{
  int cpu_nr;
  switch (opt)
    {
    case OPTION_BOARD:
      {
	if (arg)
	  {
	    board = zalloc(strlen(arg) + 1);
	    strcpy(board, arg);
	  }
	return SIM_RC_OK;
      }
    }
  
  return SIM_RC_OK;
}

static const OPTION mn10300_options[] = 
{
#define BOARD_AM32 "stdeval1"
  { {"board", required_argument, NULL, OPTION_BOARD},
     '\0', "none" /* rely on compile-time string concatenation for other options */
           "|" BOARD_AM32
    , "Customize simulation for a particular board.", mn10300_option_handler },

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

/* For compatibility */
SIM_DESC simulator;

/* These default values correspond to expected usage for the chip.  */

SIM_DESC
sim_open (SIM_OPEN_KIND kind,
	  host_callback *cb,
	  struct bfd *abfd,
	  char **argv)
{
  SIM_DESC sd = sim_state_alloc (kind, cb);
  mn10300_callback = cb;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* for compatibility */
  simulator = sd;

  /* FIXME: should be better way of setting up interrupts.  For
     moment, only support watchpoints causing a breakpoint (gdb
     halt). */
  STATE_WATCHPOINTS (sd)->pc = &(PC);
  STATE_WATCHPOINTS (sd)->sizeof_pc = sizeof (PC);
  STATE_WATCHPOINTS (sd)->interrupt_handler = NULL;
  STATE_WATCHPOINTS (sd)->interrupt_names = NULL;

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    return 0;
  sim_add_option_table (sd, NULL, mn10300_options);

  /* Allocate core managed memory */
  sim_do_command (sd, "memory region 0,0x100000");
  sim_do_command (sd, "memory region 0x40000000,0x200000");

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

  if ( NULL != board
       && (strcmp(board, BOARD_AM32) == 0 ) )
    {
      /* environment */
      STATE_ENVIRONMENT (sd) = OPERATING_ENVIRONMENT;

      sim_do_command (sd, "memory region 0x44000000,0x40000");
      sim_do_command (sd, "memory region 0x48000000,0x400000");

      /* device support for mn1030002 */
      /* interrupt controller */

      sim_hw_parse (sd, "/mn103int@0x34000100/reg 0x34000100 0x7C 0x34000200 0x8 0x34000280 0x8");

      /* DEBUG: NMI input's */
      sim_hw_parse (sd, "/glue@0x30000000/reg 0x30000000 12");
      sim_hw_parse (sd, "/glue@0x30000000 > int0 nmirq /mn103int");
      sim_hw_parse (sd, "/glue@0x30000000 > int1 watchdog /mn103int");
      sim_hw_parse (sd, "/glue@0x30000000 > int2 syserr /mn103int");
      
      /* DEBUG: ACK input */
      sim_hw_parse (sd, "/glue@0x30002000/reg 0x30002000 4");
      sim_hw_parse (sd, "/glue@0x30002000 > int ack /mn103int");
      
      /* DEBUG: LEVEL output */
      sim_hw_parse (sd, "/glue@0x30004000/reg 0x30004000 8");
      sim_hw_parse (sd, "/mn103int > nmi int0 /glue@0x30004000");
      sim_hw_parse (sd, "/mn103int > level int1 /glue@0x30004000");
      
      /* DEBUG: A bunch of interrupt inputs */
      sim_hw_parse (sd, "/glue@0x30006000/reg 0x30006000 32");
      sim_hw_parse (sd, "/glue@0x30006000 > int0 irq-0 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int1 irq-1 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int2 irq-2 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int3 irq-3 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int4 irq-4 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int5 irq-5 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int6 irq-6 /mn103int");
      sim_hw_parse (sd, "/glue@0x30006000 > int7 irq-7 /mn103int");
      
      /* processor interrupt device */
      
      /* the device */
      sim_hw_parse (sd, "/mn103cpu@0x20000000");
      sim_hw_parse (sd, "/mn103cpu@0x20000000/reg 0x20000000 0x42");
      
      /* DEBUG: ACK output wired upto a glue device */
      sim_hw_parse (sd, "/glue@0x20002000");
      sim_hw_parse (sd, "/glue@0x20002000/reg 0x20002000 4");
      sim_hw_parse (sd, "/mn103cpu > ack int0 /glue@0x20002000");
      
      /* DEBUG: RESET/NMI/LEVEL wired up to a glue device */
      sim_hw_parse (sd, "/glue@0x20004000");
      sim_hw_parse (sd, "/glue@0x20004000/reg 0x20004000 12");
      sim_hw_parse (sd, "/glue@0x20004000 > int0 reset /mn103cpu");
      sim_hw_parse (sd, "/glue@0x20004000 > int1 nmi /mn103cpu");
      sim_hw_parse (sd, "/glue@0x20004000 > int2 level /mn103cpu");
      
      /* REAL: The processor wired up to the real interrupt controller */
      sim_hw_parse (sd, "/mn103cpu > ack ack /mn103int");
      sim_hw_parse (sd, "/mn103int > level level /mn103cpu");
      sim_hw_parse (sd, "/mn103int > nmi nmi /mn103cpu");
      
      
      /* PAL */
      
      /* the device */
      sim_hw_parse (sd, "/pal@0x31000000");
      sim_hw_parse (sd, "/pal@0x31000000/reg 0x31000000 64");
      sim_hw_parse (sd, "/pal@0x31000000/poll? true");
      
      /* DEBUG: PAL wired up to a glue device */
      sim_hw_parse (sd, "/glue@0x31002000");
      sim_hw_parse (sd, "/glue@0x31002000/reg 0x31002000 16");
      sim_hw_parse (sd, "/pal@0x31000000 > countdown int0 /glue@0x31002000");
      sim_hw_parse (sd, "/pal@0x31000000 > timer int1 /glue@0x31002000");
      sim_hw_parse (sd, "/pal@0x31000000 > int int2 /glue@0x31002000");
      sim_hw_parse (sd, "/glue@0x31002000 > int0 int3 /glue@0x31002000");
      sim_hw_parse (sd, "/glue@0x31002000 > int1 int3 /glue@0x31002000");
      sim_hw_parse (sd, "/glue@0x31002000 > int2 int3 /glue@0x31002000");
      
      /* REAL: The PAL wired up to the real interrupt controller */
      sim_hw_parse (sd, "/pal@0x31000000 > countdown irq-0 /mn103int");
      sim_hw_parse (sd, "/pal@0x31000000 > timer irq-1 /mn103int");
      sim_hw_parse (sd, "/pal@0x31000000 > int irq-2 /mn103int");
      
      /* 8 and 16 bit timers */
      sim_hw_parse (sd, "/mn103tim@0x34001000/reg 0x34001000 36 0x34001080 100 0x34004000 16");

      /* Hook timer interrupts up to interrupt controller */
      sim_hw_parse (sd, "/mn103tim > timer-0-underflow timer-0-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-1-underflow timer-1-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-2-underflow timer-2-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-3-underflow timer-3-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-4-underflow timer-4-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-5-underflow timer-5-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-6-underflow timer-6-underflow /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-6-compare-a timer-6-compare-a /mn103int");
      sim_hw_parse (sd, "/mn103tim > timer-6-compare-b timer-6-compare-b /mn103int");
      
      
      /* Serial devices 0,1,2 */
      sim_hw_parse (sd, "/mn103ser@0x34000800/reg 0x34000800 48");
      sim_hw_parse (sd, "/mn103ser@0x34000800/poll? true");
      
      /* Hook serial interrupts up to interrupt controller */
      sim_hw_parse (sd, "/mn103ser > serial-0-receive serial-0-receive /mn103int");
      sim_hw_parse (sd, "/mn103ser > serial-0-transmit serial-0-transmit /mn103int");
      sim_hw_parse (sd, "/mn103ser > serial-1-receive serial-1-receive /mn103int");
      sim_hw_parse (sd, "/mn103ser > serial-1-transmit serial-1-transmit /mn103int");
      sim_hw_parse (sd, "/mn103ser > serial-2-receive serial-2-receive /mn103int");
      sim_hw_parse (sd, "/mn103ser > serial-2-transmit serial-2-transmit /mn103int");
      
      sim_hw_parse (sd, "/mn103iop@0x36008000/reg 0x36008000 8 0x36008020 8 0x36008040 0xc 0x36008060 8 0x36008080 8");

      /* Memory control registers */
      sim_do_command (sd, "memory region 0x32000020,0x30");
      /* Cache control register */
      sim_do_command (sd, "memory region 0x20000070,0x4");
      /* Cache purge regions */
      sim_do_command (sd, "memory region 0x28400000,0x800");
      sim_do_command (sd, "memory region 0x28401000,0x800");
      /* DMA registers */
      sim_do_command (sd, "memory region 0x32000100,0xF");
      sim_do_command (sd, "memory region 0x32000200,0xF");
      sim_do_command (sd, "memory region 0x32000400,0xF");
      sim_do_command (sd, "memory region 0x32000800,0xF");
    }
  else
    {
      if (board != NULL)
        {
	  sim_io_eprintf (sd, "Error: Board `%s' unknown.\n", board);
          return 0;
	}
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

  /* establish any remaining configuration options */
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


  /* set machine specific configuration */
/*   STATE_CPU (sd, 0)->psw_mask = (PSW_NP | PSW_EP | PSW_ID | PSW_SAT */
/* 			     | PSW_CY | PSW_OV | PSW_S | PSW_Z); */

  return sd;
}


void
sim_close (SIM_DESC sd, int quitting)
{
  sim_module_uninstall (sd);
}


SIM_RC
sim_create_inferior (SIM_DESC sd,
		     struct bfd *prog_bfd,
		     char **argv,
		     char **env)
{
  memset (&State, 0, sizeof (State));
  if (prog_bfd != NULL) {
    PC = bfd_get_start_address (prog_bfd);
  } else {
    PC = 0;
  }
  CIA_SET (STATE_CPU (sd, 0), (unsigned64) PC);

  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_am33_2)
    PSW |= PSW_FE;

  return SIM_RC_OK;
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  char *mm_cmd = "memory-map";
  char *int_cmd = "interrupt";

  if (sim_args_command (sd, cmd) != SIM_RC_OK)
    {
      if (strncmp (cmd, mm_cmd, strlen (mm_cmd) == 0))
	sim_io_eprintf (sd, "`memory-map' command replaced by `sim memory'\n");
      else if (strncmp (cmd, int_cmd, strlen (int_cmd)) == 0)
	sim_io_eprintf (sd, "`interrupt' command replaced by `sim watch'\n");
      else
	sim_io_eprintf (sd, "Unknown command `%s'\n", cmd);
    }
}

/* FIXME These would more efficient to use than load_mem/store_mem,
   but need to be changed to use the memory map.  */

uint8
get_byte (uint8 *x)
{
  return *x;
}

uint16
get_half (uint8 *x)
{
  uint8 *a = x;
  return (a[1] << 8) + (a[0]);
}

uint32
get_word (uint8 *x)
{
  uint8 *a = x;
  return (a[3]<<24) + (a[2]<<16) + (a[1]<<8) + (a[0]);
}

void
put_byte (uint8 *addr, uint8 data)
{
  uint8 *a = addr;
  a[0] = data;
}

void
put_half (uint8 *addr, uint16 data)
{
  uint8 *a = addr;
  a[0] = data & 0xff;
  a[1] = (data >> 8) & 0xff;
}

void
put_word (uint8 *addr, uint32 data)
{
  uint8 *a = addr;
  a[0] = data & 0xff;
  a[1] = (data >> 8) & 0xff;
  a[2] = (data >> 16) & 0xff;
  a[3] = (data >> 24) & 0xff;
}

int
sim_fetch_register (SIM_DESC sd,
		    int rn,
		    unsigned char *memory,
		    int length)
{
  put_word (memory, State.regs[rn]);
  return -1;
}
 
int
sim_store_register (SIM_DESC sd,
		    int rn,
		    unsigned char *memory,
		    int length)
{
  State.regs[rn] = get_word (memory);
  return -1;
}


void
mn10300_core_signal (SIM_DESC sd,
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
      sim_io_eprintf (sd, "mn10300-core: %d byte %s to unmapped address 0x%lx at 0x%lx\n",
                      nr_bytes, copy, 
                      (unsigned long) addr, (unsigned long) ip);
      program_interrupt(sd, cpu, cia, SIM_SIGSEGV);
      break;

    case sim_core_unaligned_signal:
      sim_io_eprintf (sd, "mn10300-core: %d byte %s to unaligned address 0x%lx at 0x%lx\n",
                      nr_bytes, copy, 
                      (unsigned long) addr, (unsigned long) ip);
      program_interrupt(sd, cpu, cia, SIM_SIGBUS);
      break;

    default:
      sim_engine_abort (sd, cpu, cia,
                        "mn10300_core_signal - internal error - bad switch");
    }
}


void
program_interrupt (SIM_DESC sd,
		   sim_cpu *cpu,
		   sim_cia cia,
		   SIM_SIGNAL sig)
{
  int status;
  struct hw *device;
  static int in_interrupt = 0;

#ifdef SIM_CPU_EXCEPTION_TRIGGER
  SIM_CPU_EXCEPTION_TRIGGER(sd,cpu,cia);
#endif

  /* avoid infinite recursion */
  if (in_interrupt)
    {
      (*mn10300_callback->printf_filtered) (mn10300_callback, 
					    "ERROR: recursion in program_interrupt during software exception dispatch.");
    }
  else
    {
      in_interrupt = 1;
      /* copy NMI handler code from dv-mn103cpu.c */
      store_word (SP - 4, CIA_GET (cpu));
      store_half (SP - 8, PSW);

      /* Set the SYSEF flag in NMICR by backdoor method.  See
	 dv-mn103int.c:write_icr().  This is necessary because
         software exceptions are not modelled by actually talking to
         the interrupt controller, so it cannot set its own SYSEF
         flag. */
     if ((NULL != board) && (strcmp(board, BOARD_AM32) == 0))
       store_byte (0x34000103, 0x04);
    }

  PSW &= ~PSW_IE;
  SP = SP - 8;
  CIA_SET (cpu, 0x40000008);

  in_interrupt = 0;
  sim_engine_halt(sd, cpu, NULL, cia, sim_stopped, sig);
}


void
mn10300_cpu_exception_trigger(SIM_DESC sd, sim_cpu* cpu, address_word cia)
{
  ASSERT(cpu != NULL);

  if(State.exc_suspended > 0)
    sim_io_eprintf(sd, "Warning, nested exception triggered (%d)\n", State.exc_suspended); 

  CIA_SET (cpu, cia);
  memcpy(State.exc_trigger_regs, State.regs, sizeof(State.exc_trigger_regs));
  State.exc_suspended = 0;
}

void
mn10300_cpu_exception_suspend(SIM_DESC sd, sim_cpu* cpu, int exception)
{
  ASSERT(cpu != NULL);

  if(State.exc_suspended > 0)
    sim_io_eprintf(sd, "Warning, nested exception signal (%d then %d)\n", 
		   State.exc_suspended, exception); 

  memcpy(State.exc_suspend_regs, State.regs, sizeof(State.exc_suspend_regs));
  memcpy(State.regs, State.exc_trigger_regs, sizeof(State.regs));
  CIA_SET (cpu, PC); /* copy PC back from new State.regs */
  State.exc_suspended = exception;
}

void
mn10300_cpu_exception_resume(SIM_DESC sd, sim_cpu* cpu, int exception)
{
  ASSERT(cpu != NULL);

  if(exception == 0 && State.exc_suspended > 0)
    {
      if(State.exc_suspended != SIGTRAP) /* warn not for breakpoints */
         sim_io_eprintf(sd, "Warning, resuming but ignoring pending exception signal (%d)\n",
  		       State.exc_suspended); 
    }
  else if(exception != 0 && State.exc_suspended > 0)
    {
      if(exception != State.exc_suspended) 
	sim_io_eprintf(sd, "Warning, resuming with mismatched exception signal (%d vs %d)\n",
		       State.exc_suspended, exception); 
      
      memcpy(State.regs, State.exc_suspend_regs, sizeof(State.regs)); 
      CIA_SET (cpu, PC); /* copy PC back from new State.regs */
    }
  else if(exception != 0 && State.exc_suspended == 0)
    {
      sim_io_eprintf(sd, "Warning, ignoring spontanous exception signal (%d)\n", exception); 
    }
  State.exc_suspended = 0; 
}

/* This is called when an FP instruction is issued when the FP unit is
   disabled, i.e., the FE bit of PSW is zero.  It raises interrupt
   code 0x1c0.  */
void
fpu_disabled_exception (SIM_DESC sd, sim_cpu *cpu, sim_cia cia)
{
  sim_io_eprintf(sd, "FPU disabled exception\n");
  program_interrupt (sd, cpu, cia, SIM_SIGFPE);
}

/* This is called when the FP unit is enabled but one of the
   unimplemented insns is issued.  It raises interrupt code 0x1c8.  */
void
fpu_unimp_exception (SIM_DESC sd, sim_cpu *cpu, sim_cia cia)
{
  sim_io_eprintf(sd, "Unimplemented FPU instruction exception\n");
  program_interrupt (sd, cpu, cia, SIM_SIGFPE);
}

/* This is called at the end of any FP insns that may have triggered
   FP exceptions.  If no exception is enabled, it returns immediately.
   Otherwise, it raises an exception code 0x1d0.  */
void
fpu_check_signal_exception (SIM_DESC sd, sim_cpu *cpu, sim_cia cia)
{
  if ((FPCR & EC_MASK) == 0)
    return;

  sim_io_eprintf(sd, "FPU %s%s%s%s%s exception\n",
		 (FPCR & EC_V) ? "V" : "",
		 (FPCR & EC_Z) ? "Z" : "",
		 (FPCR & EC_O) ? "O" : "",
		 (FPCR & EC_U) ? "U" : "",
		 (FPCR & EC_I) ? "I" : "");
  program_interrupt (sd, cpu, cia, SIM_SIGFPE);
}

/* Convert a 32-bit single-precision FP value in the target platform
   format to a sim_fpu value.  */
static void
reg2val_32 (const void *reg, sim_fpu *val)
{
  FS2FPU (*(reg_t *)reg, *val);
}

/* Round the given sim_fpu value to single precision, following the
   target platform rounding and denormalization conventions.  On
   AM33/2.0, round_near is the only rounding mode.  */
static int
round_32 (sim_fpu *val)
{
  return sim_fpu_round_32 (val, sim_fpu_round_near, sim_fpu_denorm_zero);
}

/* Convert a sim_fpu value to the 32-bit single-precision target
   representation.  */
static void
val2reg_32 (const sim_fpu *val, void *reg)
{
  FPU2FS (*val, *(reg_t *)reg);
}

/* Define the 32-bit single-precision conversion and rounding uniform
   interface.  */
const struct fp_prec_t
fp_single_prec = {
  reg2val_32, round_32, val2reg_32
};

/* Convert a 64-bit double-precision FP value in the target platform
   format to a sim_fpu value.  */
static void
reg2val_64 (const void *reg, sim_fpu *val)
{
  FD2FPU (*(dword *)reg, *val);
}

/* Round the given sim_fpu value to double precision, following the
   target platform rounding and denormalization conventions.  On
   AM33/2.0, round_near is the only rounding mode.  */
int
round_64 (sim_fpu *val)
{
  return sim_fpu_round_64 (val, sim_fpu_round_near, sim_fpu_denorm_zero);
}

/* Convert a sim_fpu value to the 64-bit double-precision target
   representation.  */
static void
val2reg_64 (const sim_fpu *val, void *reg)
{
  FPU2FD (*val, *(dword *)reg);
}

/* Define the 64-bit single-precision conversion and rounding uniform
   interface.  */
const struct fp_prec_t
fp_double_prec = {
  reg2val_64, round_64, val2reg_64
};

/* Define shortcuts to the uniform interface operations.  */
#define REG2VAL(reg,val) (*ops->reg2val) (reg,val)
#define ROUND(val) (*ops->round) (val)
#define VAL2REG(val,reg) (*ops->val2reg) (val,reg)

/* Check whether overflow, underflow or inexact exceptions should be
   raised.  */
int
fpu_status_ok (sim_fpu_status stat)
{
  if ((stat & sim_fpu_status_overflow)
      && (FPCR & EE_O))
    FPCR |= EC_O;
  else if ((stat & (sim_fpu_status_underflow | sim_fpu_status_denorm))
	   && (FPCR & EE_U))
    FPCR |= EC_U;
  else if ((stat & (sim_fpu_status_inexact | sim_fpu_status_rounded))
	   && (FPCR & EE_I))
    FPCR |= EC_I;
  else if (stat & ~ (sim_fpu_status_overflow
		     | sim_fpu_status_underflow
		     | sim_fpu_status_denorm
		     | sim_fpu_status_inexact
		     | sim_fpu_status_rounded))
    abort ();
  else
    return 1;
  return 0;
}

/* Implement a 32/64 bit reciprocal square root, signaling FP
   exceptions when appropriate.  */
void
fpu_rsqrt (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	   const void *reg_in, void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu in, med, out;

  REG2VAL (reg_in, &in);
  ROUND (&in);
  FPCR &= ~ EC_MASK;
  switch (sim_fpu_is (&in))
    {
    case SIM_FPU_IS_SNAN:
    case SIM_FPU_IS_NNUMBER:
    case SIM_FPU_IS_NINF:
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
      break;
	    
    case SIM_FPU_IS_QNAN:
      VAL2REG (&sim_fpu_qnan, reg_out);
      break;

    case SIM_FPU_IS_PINF:
      VAL2REG (&sim_fpu_zero, reg_out);
      break;

    case SIM_FPU_IS_PNUMBER:
      {
	/* Since we don't have a function to compute rsqrt directly,
	   use sqrt and inv.  */
	sim_fpu_status stat = 0;
	stat |= sim_fpu_sqrt (&med, &in);
	stat |= sim_fpu_inv (&out, &med);
	stat |= ROUND (&out);
	if (fpu_status_ok (stat))
	  VAL2REG (&out, reg_out);
      }
      break;

    case SIM_FPU_IS_NZERO:
    case SIM_FPU_IS_PZERO:
      if (FPCR & EE_Z)
	FPCR |= EC_Z;
      else
	{
	  /* Generate an INF with the same sign.  */
	  sim_fpu_inv (&out, &in);
	  VAL2REG (&out, reg_out);
	}
      break;

    default:
      abort ();
    }

  fpu_check_signal_exception (sd, cpu, cia);
}

static inline reg_t
cmp2fcc (int res)
{
  switch (res)
    {
    case SIM_FPU_IS_SNAN:
    case SIM_FPU_IS_QNAN:
      return FCC_U;
      
    case SIM_FPU_IS_NINF:
    case SIM_FPU_IS_NNUMBER:
    case SIM_FPU_IS_NDENORM:
      return FCC_L;
      
    case SIM_FPU_IS_PINF:
    case SIM_FPU_IS_PNUMBER:
    case SIM_FPU_IS_PDENORM:
      return FCC_G;
      
    case SIM_FPU_IS_NZERO:
    case SIM_FPU_IS_PZERO:
      return FCC_E;
      
    default:
      abort ();
    }
}

/* Implement a 32/64 bit FP compare, setting the FPCR status and/or
   exception bits as specified.  */
void
fpu_cmp (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	 const void *reg_in1, const void *reg_in2,
	 const struct fp_prec_t *ops)
{
  sim_fpu m, n;

  REG2VAL (reg_in1, &m);
  REG2VAL (reg_in2, &n);
  FPCR &= ~ EC_MASK;
  FPCR &= ~ FCC_MASK;
  ROUND (&m);
  ROUND (&n);
  if (sim_fpu_is_snan (&m) || sim_fpu_is_snan (&n))
    {
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	FPCR |= FCC_U;
    }
  else
    FPCR |= cmp2fcc (sim_fpu_cmp (&m, &n));

  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP add, setting FP exception bits when
   appropriate.  */
void
fpu_add (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	 const void *reg_in1, const void *reg_in2,
	 void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m, n, r;

  REG2VAL (reg_in1, &m);
  REG2VAL (reg_in2, &n);
  ROUND (&m);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m) || sim_fpu_is_snan (&n)
      || (sim_fpu_is (&m) == SIM_FPU_IS_PINF
	  && sim_fpu_is (&n) == SIM_FPU_IS_NINF)
      || (sim_fpu_is (&m) == SIM_FPU_IS_NINF
	  && sim_fpu_is (&n) == SIM_FPU_IS_PINF))
    {
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_add (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP sub, setting FP exception bits when
   appropriate.  */
void
fpu_sub (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	 const void *reg_in1, const void *reg_in2,
	 void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m, n, r;

  REG2VAL (reg_in1, &m);
  REG2VAL (reg_in2, &n);
  ROUND (&m);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m) || sim_fpu_is_snan (&n)
      || (sim_fpu_is (&m) == SIM_FPU_IS_PINF
	  && sim_fpu_is (&n) == SIM_FPU_IS_PINF)
      || (sim_fpu_is (&m) == SIM_FPU_IS_NINF
	  && sim_fpu_is (&n) == SIM_FPU_IS_NINF))
    {
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_sub (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP mul, setting FP exception bits when
   appropriate.  */
void
fpu_mul (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	 const void *reg_in1, const void *reg_in2,
	 void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m, n, r;

  REG2VAL (reg_in1, &m);
  REG2VAL (reg_in2, &n);
  ROUND (&m);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m) && sim_fpu_is_zero (&n))
      || (sim_fpu_is_zero (&m) && sim_fpu_is_infinity (&n)))
    {
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_mul (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP div, setting FP exception bits when
   appropriate.  */
void
fpu_div (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	 const void *reg_in1, const void *reg_in2,
	 void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m, n, r;

  REG2VAL (reg_in1, &m);
  REG2VAL (reg_in2, &n);
  ROUND (&m);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m) && sim_fpu_is_infinity (&n))
      || (sim_fpu_is_zero (&m) && sim_fpu_is_zero (&n)))
    {
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else if (sim_fpu_is_number (&m) && sim_fpu_is_zero (&n)
	   && (FPCR & EE_Z))
    FPCR |= EC_Z;
  else
    {
      sim_fpu_status stat = sim_fpu_div (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP madd, setting FP exception bits when
   appropriate.  */
void
fpu_fmadd (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	   const void *reg_in1, const void *reg_in2, const void *reg_in3,
	   void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m1, m2, m, n, r;

  REG2VAL (reg_in1, &m1);
  REG2VAL (reg_in2, &m2);
  REG2VAL (reg_in3, &n);
  ROUND (&m1);
  ROUND (&m2);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m1) || sim_fpu_is_snan (&m2) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m1) && sim_fpu_is_zero (&m2))
      || (sim_fpu_is_zero (&m1) && sim_fpu_is_infinity (&m2)))
    {
    invalid_operands:
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_mul (&m, &m1, &m2);

      if (sim_fpu_is_infinity (&m) && sim_fpu_is_infinity (&n)
	  && sim_fpu_sign (&m) != sim_fpu_sign (&n))
	goto invalid_operands;

      stat |= sim_fpu_add (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP msub, setting FP exception bits when
   appropriate.  */
void
fpu_fmsub (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	   const void *reg_in1, const void *reg_in2, const void *reg_in3,
	   void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m1, m2, m, n, r;

  REG2VAL (reg_in1, &m1);
  REG2VAL (reg_in2, &m2);
  REG2VAL (reg_in3, &n);
  ROUND (&m1);
  ROUND (&m2);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m1) || sim_fpu_is_snan (&m2) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m1) && sim_fpu_is_zero (&m2))
      || (sim_fpu_is_zero (&m1) && sim_fpu_is_infinity (&m2)))
    {
    invalid_operands:
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_mul (&m, &m1, &m2);

      if (sim_fpu_is_infinity (&m) && sim_fpu_is_infinity (&n)
	  && sim_fpu_sign (&m) == sim_fpu_sign (&n))
	goto invalid_operands;

      stat |= sim_fpu_sub (&r, &m, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP nmadd, setting FP exception bits when
   appropriate.  */
void
fpu_fnmadd (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	    const void *reg_in1, const void *reg_in2, const void *reg_in3,
	    void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m1, m2, m, mm, n, r;

  REG2VAL (reg_in1, &m1);
  REG2VAL (reg_in2, &m2);
  REG2VAL (reg_in3, &n);
  ROUND (&m1);
  ROUND (&m2);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m1) || sim_fpu_is_snan (&m2) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m1) && sim_fpu_is_zero (&m2))
      || (sim_fpu_is_zero (&m1) && sim_fpu_is_infinity (&m2)))
    {
    invalid_operands:
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_mul (&m, &m1, &m2);

      if (sim_fpu_is_infinity (&m) && sim_fpu_is_infinity (&n)
	  && sim_fpu_sign (&m) == sim_fpu_sign (&n))
	goto invalid_operands;

      stat |= sim_fpu_neg (&mm, &m);
      stat |= sim_fpu_add (&r, &mm, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}

/* Implement a 32/64 bit FP nmsub, setting FP exception bits when
   appropriate.  */
void
fpu_fnmsub (SIM_DESC sd, sim_cpu *cpu, sim_cia cia,
	    const void *reg_in1, const void *reg_in2, const void *reg_in3,
	    void *reg_out, const struct fp_prec_t *ops)
{
  sim_fpu m1, m2, m, mm, n, r;

  REG2VAL (reg_in1, &m1);
  REG2VAL (reg_in2, &m2);
  REG2VAL (reg_in3, &n);
  ROUND (&m1);
  ROUND (&m2);
  ROUND (&n);
  FPCR &= ~ EC_MASK;
  if (sim_fpu_is_snan (&m1) || sim_fpu_is_snan (&m2) || sim_fpu_is_snan (&n)
      || (sim_fpu_is_infinity (&m1) && sim_fpu_is_zero (&m2))
      || (sim_fpu_is_zero (&m1) && sim_fpu_is_infinity (&m2)))
    {
    invalid_operands:
      if (FPCR & EE_V)
	FPCR |= EC_V;
      else
	VAL2REG (&sim_fpu_qnan, reg_out);
    }
  else
    {
      sim_fpu_status stat = sim_fpu_mul (&m, &m1, &m2);

      if (sim_fpu_is_infinity (&m) && sim_fpu_is_infinity (&n)
	  && sim_fpu_sign (&m) != sim_fpu_sign (&n))
	goto invalid_operands;

      stat |= sim_fpu_neg (&mm, &m);
      stat |= sim_fpu_sub (&r, &mm, &n);
      stat |= ROUND (&r);
      if (fpu_status_ok (stat))
	VAL2REG (&r, reg_out);
    }
  
  fpu_check_signal_exception (sd, cpu, cia);
}
