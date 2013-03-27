/*
 * Simulator for the Renesas (formerly Hitachi) H8/300 architecture.
 *
 * Written by Steve Chamberlain of Cygnus Support. sac@cygnus.com
 *
 * This file is part of H8/300 sim
 *
 *
 * THIS SOFTWARE IS NOT COPYRIGHTED
 *
 * Cygnus offers the following for use in the public domain.  Cygnus makes no
 * warranty with regard to the software or its performance and the user
 * accepts the software "AS IS" with all faults.
 *
 * CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <signal.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include "bfd.h"
#include "sim-main.h"
#include "gdb/sim-h8300.h"
#include "sys/stat.h"
#include "sys/types.h"

#ifndef SIGTRAP
# define SIGTRAP 5
#endif

int debug;

host_callback *sim_callback;

static SIM_OPEN_KIND sim_kind;
static char *myname;

/* FIXME: Needs to live in header file.
   This header should also include the things in remote-sim.h.
   One could move this to remote-sim.h but this function isn't needed
   by gdb.  */
static void set_simcache_size (SIM_DESC, int);

#define X(op, size)  (op * 4 + size)

#define SP (h8300hmode && !h8300_normal_mode ? SL : SW)

#define h8_opcodes ops
#define DEFINE_TABLE
#include "opcode/h8300.h"

/* CPU data object: */

static int
sim_state_initialize (SIM_DESC sd, sim_cpu *cpu)
{
  /* FIXME: not really necessary, since sim_cpu_alloc calls zalloc.  */

  memset (&cpu->regs, 0, sizeof(cpu->regs));
  cpu->regs[SBR_REGNUM] = 0xFFFFFF00;
  cpu->pc = 0;
  cpu->delayed_branch = 0;
  cpu->memory = NULL;
  cpu->eightbit = NULL;
  cpu->mask = 0;

  /* Initialize local simulator state.  */
  sd->sim_cache = NULL;
  sd->sim_cache_size = 0;
  sd->cache_idx = NULL;
  sd->cache_top = 0;
  sd->memory_size = 0;
  sd->compiles = 0;
#ifdef ADEBUG
  memset (&cpu->stats, 0, sizeof (cpu->stats));
#endif
  return 0;
}

static unsigned int
h8_get_pc (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> pc;
}

static void
h8_set_pc (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> pc = val;
}

static unsigned int
h8_get_ccr (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[CCR_REGNUM];
}

static void
h8_set_ccr (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[CCR_REGNUM] = val;
}

static unsigned int
h8_get_exr (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[EXR_REGNUM];
}

static void
h8_set_exr (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[EXR_REGNUM] = val;
}

static int
h8_get_sbr (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[SBR_REGNUM];
}

static void
h8_set_sbr (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> regs[SBR_REGNUM] = val;
}

static int
h8_get_vbr (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[VBR_REGNUM];
}

static void
h8_set_vbr (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> regs[VBR_REGNUM] = val;
}

static int
h8_get_cache_top (SIM_DESC sd)
{
  return sd -> cache_top;
}

static void
h8_set_cache_top (SIM_DESC sd, int val)
{
  sd -> cache_top = val;
}

static int
h8_get_mask (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> mask;
}

static void
h8_set_mask (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> mask = val;
}
#if 0
static int
h8_get_exception (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> exception;
}

static void
h8_set_exception (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> exception = val;
}

static enum h8300_sim_state
h8_get_state (SIM_DESC sd)
{
  return sd -> state;
}

static void
h8_set_state (SIM_DESC sd, enum h8300_sim_state val)
{
  sd -> state = val;
}
#endif
static unsigned int
h8_get_cycles (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[CYCLE_REGNUM];
}

static void
h8_set_cycles (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[CYCLE_REGNUM] = val;
}

static unsigned int
h8_get_insts (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[INST_REGNUM];
}

static void
h8_set_insts (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[INST_REGNUM] = val;
}

static unsigned int
h8_get_ticks (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[TICK_REGNUM];
}

static void
h8_set_ticks (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[TICK_REGNUM] = val;
}

static unsigned int
h8_get_mach (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[MACH_REGNUM];
}

static void
h8_set_mach (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[MACH_REGNUM] = val;
}

static unsigned int
h8_get_macl (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> regs[MACL_REGNUM];
}

static void
h8_set_macl (SIM_DESC sd, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> regs[MACL_REGNUM] = val;
}

static int
h8_get_compiles (SIM_DESC sd)
{
  return sd -> compiles;
}

static void
h8_increment_compiles (SIM_DESC sd)
{
  sd -> compiles ++;
}

static unsigned int *
h8_get_reg_buf (SIM_DESC sd)
{
  return &(((STATE_CPU (sd, 0)) -> regs)[0]);
}

static unsigned int
h8_get_reg (SIM_DESC sd, int regnum)
{
  return (STATE_CPU (sd, 0)) -> regs[regnum];
}

static void
h8_set_reg (SIM_DESC sd, int regnum, int val)
{
  (STATE_CPU (sd, 0)) -> regs[regnum] = val;
}

#ifdef ADEBUG
static int
h8_get_stats (SIM_DESC sd, int idx)
{
  return sd -> stats[idx];
}

static void
h8_increment_stats (SIM_DESC sd, int idx)
{
  sd -> stats[idx] ++;
}
#endif /* ADEBUG */

static unsigned short *
h8_get_cache_idx_buf (SIM_DESC sd)
{
  return sd -> cache_idx;
}

static void
h8_set_cache_idx_buf (SIM_DESC sd, unsigned short *ptr)
{
  sd -> cache_idx = ptr;
}

static unsigned short
h8_get_cache_idx (SIM_DESC sd, unsigned int idx)
{
  if (idx > sd->memory_size)
    return (unsigned short) -1;
  return sd -> cache_idx[idx];
}

static void
h8_set_cache_idx (SIM_DESC sd, int idx, unsigned int val)
{
  sd -> cache_idx[idx] = (unsigned short) val;
}

static unsigned char *
h8_get_memory_buf (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> memory;
}

static void
h8_set_memory_buf (SIM_DESC sd, unsigned char *ptr)
{
  (STATE_CPU (sd, 0)) -> memory = ptr;
}

static unsigned char
h8_get_memory (SIM_DESC sd, int idx)
{
  return (STATE_CPU (sd, 0)) -> memory[idx];
}

static void
h8_set_memory (SIM_DESC sd, int idx, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> memory[idx] = (unsigned char) val;
}

static unsigned char *
h8_get_eightbit_buf (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> eightbit;
}

static void
h8_set_eightbit_buf (SIM_DESC sd, unsigned char *ptr)
{
  (STATE_CPU (sd, 0)) -> eightbit = ptr;
}

static unsigned char
h8_get_eightbit (SIM_DESC sd, int idx)
{
  return (STATE_CPU (sd, 0)) -> eightbit[idx];
}

static void
h8_set_eightbit (SIM_DESC sd, int idx, unsigned int val)
{
  (STATE_CPU (sd, 0)) -> eightbit[idx] = (unsigned char) val;
}

static unsigned int
h8_get_delayed_branch (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> delayed_branch;
}

static void
h8_set_delayed_branch (SIM_DESC sd, unsigned int dest)
{
  (STATE_CPU (sd, 0)) -> delayed_branch = dest;
}

static char **
h8_get_command_line (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> command_line;
}

static void
h8_set_command_line (SIM_DESC sd, char ** val)
{
  (STATE_CPU (sd, 0)) -> command_line = val;
}

static char *
h8_get_cmdline_arg (SIM_DESC sd, int index)
{
  return (STATE_CPU (sd, 0)) -> command_line[index];
}

static void
h8_set_cmdline_arg (SIM_DESC sd, int index, char * val)
{
  (STATE_CPU (sd, 0)) -> command_line[index] = val;
}

/* MAC Saturation Mode */
static int
h8_get_macS (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> macS;
}

static void
h8_set_macS (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> macS = (val != 0);
}

/* MAC Zero Flag */
static int
h8_get_macZ (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> macZ;
}

static void
h8_set_macZ (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> macZ = (val != 0);
}

/* MAC Negative Flag */
static int
h8_get_macN (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> macN;
}

static void
h8_set_macN (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> macN = (val != 0);
}

/* MAC Overflow Flag */
static int
h8_get_macV (SIM_DESC sd)
{
  return (STATE_CPU (sd, 0)) -> macV;
}

static void
h8_set_macV (SIM_DESC sd, int val)
{
  (STATE_CPU (sd, 0)) -> macV = (val != 0);
}

/* End CPU data object.  */

/* The rate at which to call the host's poll_quit callback.  */

enum { POLL_QUIT_INTERVAL = 0x80000 };

#define LOW_BYTE(x) ((x) & 0xff)
#define HIGH_BYTE(x) (((x) >> 8) & 0xff)
#define P(X, Y) ((X << 8) | Y)

#define C (c != 0)
#define Z (nz == 0)
#define V (v != 0)
#define N (n != 0)
#define U (u != 0)
#define H (h != 0)
#define UI (ui != 0)
#define I (intMaskBit != 0)

#define BUILDSR(SD)						\
  h8_set_ccr (SD, (I << 7) | (UI << 6) | (H << 5) | (U << 4)	\
	     | (N << 3) | (Z << 2) | (V << 1) | C)

#define GETSR(SD) \
  /* Get Status Register (flags).  */		\
  c = (h8_get_ccr (sd) >> 0) & 1;		\
  v = (h8_get_ccr (sd) >> 1) & 1;		\
  nz = !((h8_get_ccr (sd) >> 2) & 1);		\
  n = (h8_get_ccr (sd) >> 3) & 1;		\
  u = (h8_get_ccr (sd) >> 4) & 1;		\
  h = (h8_get_ccr (sd) >> 5) & 1;		\
  ui = ((h8_get_ccr (sd) >> 6) & 1);		\
  intMaskBit = (h8_get_ccr (sd) >> 7) & 1


#ifdef __CHAR_IS_SIGNED__
#define SEXTCHAR(x) ((char) (x))
#endif

#ifndef SEXTCHAR
#define SEXTCHAR(x) ((x & 0x80) ? (x | ~0xff) : x & 0xff)
#endif

#define UEXTCHAR(x) ((x) & 0xff)
#define UEXTSHORT(x) ((x) & 0xffff)
#define SEXTSHORT(x) ((short) (x))

int h8300hmode  = 0;
int h8300smode  = 0;
int h8300_normal_mode  = 0;
int h8300sxmode = 0;

static int memory_size;

static int
get_now (void)
{
  return time (0);	/* WinXX HAS UNIX like 'time', so why not use it? */
}

static int
now_persec (void)
{
  return 1;
}

static int
bitfrom (int x)
{
  switch (x & SIZE)
    {
    case L_8:
      return SB;
    case L_16:
    case L_16U:
      return SW;
    case L_32:
      return SL;
    case L_P:
      return (h8300hmode && !h8300_normal_mode)? SL : SW;
    }
  return 0;
}

/* Simulate an indirection / dereference.  
   return 0 for success, -1 for failure.
*/

static unsigned int
lvalue (SIM_DESC sd, int x, int rn, unsigned int *val)
{
  if (val == NULL)	/* Paranoia.  */
    return -1;

  switch (x / 4)
    {
    case OP_DISP:
      if (rn == ZERO_REGNUM)
	*val = X (OP_IMM, SP);
      else
	*val = X (OP_REG, SP);
      break;
    case OP_MEM:
      *val = X (OP_MEM, SP);
      break;
    default:
      sim_engine_set_run_state (sd, sim_stopped, SIGSEGV);
      return -1;
    }
  return 0;
}

static int
cmdline_location()
{
  if (h8300smode && !h8300_normal_mode)
    return 0xffff00L;
  else if (h8300hmode && !h8300_normal_mode)
    return 0x2ff00L;
  else
    return 0xff00L;
}

static void
decode (SIM_DESC sd, int addr, unsigned char *data, decoded_inst *dst)
{
  int cst[3]   = {0, 0, 0};
  int reg[3]   = {0, 0, 0};
  int rdisp[3] = {0, 0, 0};
  int opnum;
  const struct h8_opcode *q;

  dst->dst.type = -1;
  dst->src.type = -1;

  /* Find the exact opcode/arg combo.  */
  for (q = h8_opcodes; q->name; q++)
    {
      op_type *nib = q->data.nib;
      unsigned int len = 0;

      if ((q->available == AV_H8SX && !h8300sxmode) ||
	  (q->available == AV_H8S  && !h8300smode)  ||
	  (q->available == AV_H8H  && !h8300hmode))
	continue;

      cst[0]   = cst[1]   = cst[2]   = 0;
      reg[0]   = reg[1]   = reg[2]   = 0;
      rdisp[0] = rdisp[1] = rdisp[2] = 0;

      while (1)
	{
	  op_type looking_for = *nib;
	  int thisnib = data[len / 2];

	  thisnib = (len & 1) ? (thisnib & 0xf) : ((thisnib >> 4) & 0xf);
	  opnum = ((looking_for & OP3) ? 2 :
		   (looking_for & DST) ? 1 : 0);

	  if (looking_for < 16 && looking_for >= 0)
	    {
	      if (looking_for != thisnib)
		goto fail;
	    }
	  else
	    {
	      if (looking_for & B31)
		{
		  if (!((thisnib & 0x8) != 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B31);
		  thisnib &= 0x7;
		}
	      else if (looking_for & B30)
		{
		  if (!((thisnib & 0x8) == 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B30);
		}

	      if (looking_for & B21)
		{
		  if (!((thisnib & 0x4) != 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B21);
		  thisnib &= 0xb;
		}
	      else if (looking_for & B20)
		{
		  if (!((thisnib & 0x4) == 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B20);
		}

	      if (looking_for & B11)
		{
		  if (!((thisnib & 0x2) != 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B11);
		  thisnib &= 0xd;
		}
	      else if (looking_for & B10)
		{
		  if (!((thisnib & 0x2) == 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B10);
		}

	      if (looking_for & B01)
		{
		  if (!((thisnib & 0x1) != 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B01);
		  thisnib &= 0xe;
		}
	      else if (looking_for & B00)
		{
		  if (!((thisnib & 0x1) == 0))
		    goto fail;

		  looking_for = (op_type) (looking_for & ~B00);
		}

	      if (looking_for & IGNORE)
		{
		  /* Hitachi has declared that IGNORE must be zero.  */
		  if (thisnib != 0)
		    goto fail;
		}
	      else if ((looking_for & MODE) == DATA)
		{
		  ;			/* Skip embedded data.  */
		}
	      else if ((looking_for & MODE) == DBIT)
		{
		  /* Exclude adds/subs by looking at bit 0 and 2, and
                     make sure the operand size, either w or l,
                     matches by looking at bit 1.  */
		  if ((looking_for & 7) != (thisnib & 7))
		    goto fail;

		  cst[opnum] = (thisnib & 0x8) ? 2 : 1;
		}
	      else if ((looking_for & MODE) == REG     ||
		       (looking_for & MODE) == LOWREG  ||
		       (looking_for & MODE) == IND     ||
		       (looking_for & MODE) == PREINC  ||
		       (looking_for & MODE) == POSTINC ||
		       (looking_for & MODE) == PREDEC  ||
		       (looking_for & MODE) == POSTDEC)
		{
		  reg[opnum] = thisnib;
		}
	      else if (looking_for & CTRL)
		{
		  thisnib &= 7;
		  if (((looking_for & MODE) == CCR  && (thisnib != C_CCR))  ||
		      ((looking_for & MODE) == EXR  && (thisnib != C_EXR))  ||
		      ((looking_for & MODE) == MACH && (thisnib != C_MACH)) ||
		      ((looking_for & MODE) == MACL && (thisnib != C_MACL)) ||
		      ((looking_for & MODE) == VBR  && (thisnib != C_VBR))  ||
		      ((looking_for & MODE) == SBR  && (thisnib != C_SBR)))
		    goto fail;
		  if (((looking_for & MODE) == CCR_EXR && 
		       (thisnib != C_CCR && thisnib != C_EXR)) ||
		      ((looking_for & MODE) == VBR_SBR && 
		       (thisnib != C_VBR && thisnib != C_SBR)) ||
		      ((looking_for & MODE) == MACREG && 
		       (thisnib != C_MACH && thisnib != C_MACL)))
		    goto fail;
		  if (((looking_for & MODE) == CC_EX_VB_SB && 
		       (thisnib != C_CCR && thisnib != C_EXR &&
			thisnib != C_VBR && thisnib != C_SBR)))
		    goto fail;

		  reg[opnum] = thisnib;
		}
	      else if ((looking_for & MODE) == ABS)
		{
		  /* Absolute addresses are unsigned.  */
		  switch (looking_for & SIZE)
		    {
		    case L_8:
		      cst[opnum] = UEXTCHAR (data[len / 2]);
		      break;
		    case L_16:
		    case L_16U:
		      cst[opnum] = (data[len / 2] << 8) + data[len / 2 + 1];
		      break;
		    case L_32:
		      cst[opnum] = 
			(data[len / 2 + 0] << 24) + 
			(data[len / 2 + 1] << 16) +
			(data[len / 2 + 2] <<  8) +  
			(data[len / 2 + 3]);
		      break;
		    default:
		      printf ("decode: bad size ABS: %d\n", 
			      (looking_for & SIZE));
		      goto end;
		    }
		}
	      else if ((looking_for & MODE) == DISP   ||
		       (looking_for & MODE) == PCREL  ||
		       (looking_for & MODE) == INDEXB ||
		       (looking_for & MODE) == INDEXW ||
		       (looking_for & MODE) == INDEXL)
		{
		  switch (looking_for & SIZE)
		    {
		    case L_2:
		      cst[opnum] = thisnib & 3;
		      break;
		    case L_8:
		      cst[opnum] = SEXTCHAR (data[len / 2]);
		      break;
		    case L_16:
		      cst[opnum] = (data[len / 2] << 8) + data[len / 2 + 1];
		      cst[opnum] = (short) cst[opnum];	/* Sign extend.  */
		      break;
		    case L_16U:
		      cst[opnum] = (data[len / 2] << 8) + data[len / 2 + 1];
		      break;
		    case L_32:
		      cst[opnum] = 
			(data[len / 2 + 0] << 24) + 
			(data[len / 2 + 1] << 16) +
			(data[len / 2 + 2] <<  8) +  
			(data[len / 2 + 3]);
		      break;
		    default:
		      printf ("decode: bad size DISP/PCREL/INDEX: %d\n", 
			      (looking_for & SIZE));
		      goto end;
		    }
		}
	      else if ((looking_for & SIZE) == L_16 ||
		       (looking_for & SIZE) == L_16U)
		{
		  cst[opnum] = (data[len / 2] << 8) + data[len / 2 + 1];
		  /* Immediates are always unsigned.  */
		  if ((looking_for & SIZE) != L_16U &&
		      (looking_for & MODE) != IMM)
		    cst[opnum] = (short) cst[opnum];	/* Sign extend.  */
		}
	      else if (looking_for & ABSJMP)
		{
		  switch (looking_for & SIZE) {
		  case L_24:
		    cst[opnum] = (data[1] << 16) | (data[2] << 8) | (data[3]);
		    break;
		  case L_32:
		    cst[opnum] = 
		      (data[len / 2 + 0] << 24) + 
		      (data[len / 2 + 1] << 16) +
		      (data[len / 2 + 2] <<  8) +  
		      (data[len / 2 + 3]);
		    break;
		  default:
		    printf ("decode: bad size ABSJMP: %d\n", 
			    (looking_for & SIZE));
		      goto end;
		  }
		}
	      else if ((looking_for & MODE) == MEMIND)
		{
		  cst[opnum] = data[1];
		}
	      else if ((looking_for & MODE) == VECIND)
		{
		  if(h8300_normal_mode)
		    cst[opnum] = ((data[1] & 0x7f) + 0x80) * 2;
		  else
		    cst[opnum] = ((data[1] & 0x7f) + 0x80) * 4;
		  cst[opnum] += h8_get_vbr (sd); /* Add vector base reg.  */
		}
	      else if ((looking_for & SIZE) == L_32)
		{
		  int i = len / 2;

		  cst[opnum] = 
		    (data[i + 0] << 24) |
		    (data[i + 1] << 16) |
		    (data[i + 2] <<  8) |
		    (data[i + 3]);
		}
	      else if ((looking_for & SIZE) == L_24)
		{
		  int i = len / 2;

		  cst[opnum] = 
		    (data[i + 0] << 16) | 
		    (data[i + 1] << 8) | 
		    (data[i + 2]);
		}
	      else if (looking_for & DISPREG)
		{
		  rdisp[opnum] = thisnib & 0x7;
		}
	      else if ((looking_for & MODE) == KBIT)
		{
		  switch (thisnib)
		    {
		    case 9:
		      cst[opnum] = 4;
		      break;
		    case 8:
		      cst[opnum] = 2;
		      break;
		    case 0:
		      cst[opnum] = 1;
		      break;
		    default:
		      goto fail;
		    }
		}
	      else if ((looking_for & SIZE) == L_8)
		{
		  if ((looking_for & MODE) == ABS)
		    {
		      /* Will be combined with contents of SBR_REGNUM
			 by fetch ().  For all modes except h8sx, this
			 will always contain the value 0xFFFFFF00.  */
		      cst[opnum] = data[len / 2] & 0xff;
		    }
		  else
		    {
		      cst[opnum] = data[len / 2] & 0xff;
		    }
		}
	      else if ((looking_for & SIZE) == L_2)
		{
		  cst[opnum] = thisnib & 3;
		}
	      else if ((looking_for & SIZE) == L_3 ||
		       (looking_for & SIZE) == L_3NZ)
		{
		  cst[opnum] = thisnib & 7;
		  if (cst[opnum] == 0 && (looking_for & SIZE) == L_3NZ)
		    goto fail;
		}
	      else if ((looking_for & SIZE) == L_4)
		{
		  cst[opnum] = thisnib & 15;
		}
	      else if ((looking_for & SIZE) == L_5)
		{
		  cst[opnum] = data[len / 2] & 0x1f;
		}
	      else if (looking_for == E)
		{
#ifdef ADEBUG
		  dst->op = q;
#endif
		  /* Fill in the args.  */
		  {
		    op_type *args = q->args.nib;
		    int hadone = 0;
		    int nargs;

		    for (nargs = 0; 
			 nargs < 3 && *args != E; 
			 nargs++)
		      {
			int x = *args;
			ea_type *p;

			opnum = ((x & OP3) ? 2 :
				 (x & DST) ? 1 : 0);
			if (x & DST)
			  p = &dst->dst;
			else if (x & OP3)
			  p = &dst->op3;
			else
			  p = &dst->src;

			if ((x & MODE) == IMM  ||
			    (x & MODE) == KBIT ||
			    (x & MODE) == DBIT)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_IMM, OP_SIZE (q->how));
			    p->literal = cst[opnum];
			  }
			else if ((x & MODE) == CONST_2 ||
				 (x & MODE) == CONST_4 ||
				 (x & MODE) == CONST_8 ||
				 (x & MODE) == CONST_16)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_IMM, OP_SIZE (q->how));
			    switch (x & MODE) {
			    case CONST_2:	p->literal =  2; break;
			    case CONST_4:	p->literal =  4; break;
			    case CONST_8:	p->literal =  8; break;
			    case CONST_16:	p->literal = 16; break;
			    }
			  }
			else if ((x & MODE) == REG)
			  {
			    p->type = X (OP_REG, bitfrom (x));
			    p->reg = reg[opnum];
			  }
			else if ((x & MODE) == LOWREG)
			  {
			    p->type = X (OP_LOWREG, bitfrom (x));
			    p->reg = reg[opnum];
			  }
			else if ((x & MODE) == PREINC)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_PREINC, OP_SIZE (q->how));
			    p->reg = reg[opnum] & 0x7;
			  }
			else if ((x & MODE) == POSTINC)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_POSTINC, OP_SIZE (q->how));
			    p->reg = reg[opnum] & 0x7;
			  }
			else if ((x & MODE) == PREDEC)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_PREDEC, OP_SIZE (q->how));
			    p->reg = reg[opnum] & 0x7;
			  }
			else if ((x & MODE) == POSTDEC)
			  {
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_POSTDEC, OP_SIZE (q->how));
			    p->reg = reg[opnum] & 0x7;
			  }
			else if ((x & MODE) == IND)
			  {
			    /* Note: an indirect is transformed into
			       a displacement of zero.  
			    */
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_DISP, OP_SIZE (q->how));
			    p->reg = reg[opnum] & 0x7;
			    p->literal = 0;
			    if (OP_KIND (q->how) == O_JSR ||
				OP_KIND (q->how) == O_JMP)
			      if (lvalue (sd, p->type, p->reg, (unsigned int *)&p->type))
				goto end;
			  }
			else if ((x & MODE) == ABS)
			  {
			    /* Note: a 16 or 32 bit ABS is transformed into a 
			       displacement from pseudo-register ZERO_REGNUM,
			       which is always zero.  An 8 bit ABS becomes
			       a displacement from SBR_REGNUM.
			    */
			    /* Use the instruction to determine 
			       the operand size.  */
			    p->type = X (OP_DISP, OP_SIZE (q->how));
			    p->literal = cst[opnum];

			    /* 8-bit ABS is displacement from SBR.
			       16 and 32-bit ABS are displacement from ZERO.
			       (SBR will always be zero except for h8/sx)
			    */
			    if ((x & SIZE) == L_8)
			      p->reg = SBR_REGNUM;
			    else
			      p->reg = ZERO_REGNUM;;
			  }
			else if ((x & MODE) == MEMIND ||
				 (x & MODE) == VECIND)
			  {
			    /* Size doesn't matter.  */
			    p->type = X (OP_MEM, SB);
			    p->literal = cst[opnum];
			    if (OP_KIND (q->how) == O_JSR ||
				OP_KIND (q->how) == O_JMP)
			      if (lvalue (sd, p->type, p->reg, (unsigned int *)&p->type))
				goto end;
			  }
			else if ((x & MODE) == PCREL)
			  {
			    /* Size doesn't matter.  */
			    p->type = X (OP_PCREL, SB);
			    p->literal = cst[opnum];
			  }
			else if (x & ABSJMP)
			  {
			    p->type = X (OP_IMM, SP);
			    p->literal = cst[opnum];
			  }
			else if ((x & MODE) == INDEXB)
			  {
			    p->type = X (OP_INDEXB, OP_SIZE (q->how));
			    p->literal = cst[opnum];
			    p->reg     = rdisp[opnum];
			  }
			else if ((x & MODE) == INDEXW)
			  {
			    p->type = X (OP_INDEXW, OP_SIZE (q->how));
			    p->literal = cst[opnum];
			    p->reg     = rdisp[opnum];
			  }
			else if ((x & MODE) == INDEXL)
			  {
			    p->type = X (OP_INDEXL, OP_SIZE (q->how));
			    p->literal = cst[opnum];
			    p->reg     = rdisp[opnum];
			  }
			else if ((x & MODE) == DISP)
			  {
			    /* Yuck -- special for mova args.  */
			    if (strncmp (q->name, "mova", 4) == 0 &&
				(x & SIZE) == L_2)
			      {
				/* Mova can have a DISP2 dest, with an
				   INDEXB or INDEXW src.  The multiplier
				   for the displacement value is determined
				   by the src operand, not by the insn.  */

				switch (OP_KIND (dst->src.type))
				  {
				  case OP_INDEXB:
				    p->type = X (OP_DISP, SB);
				    p->literal = cst[opnum];
				    break;
				  case OP_INDEXW:
				    p->type = X (OP_DISP, SW);
				    p->literal = cst[opnum] * 2;
				    break;
				  default:
				    goto fail;
				  }
			      }
			    else
			      {
				p->type = X (OP_DISP,   OP_SIZE (q->how));
				p->literal = cst[opnum];
				/* DISP2 is special.  */
				if ((x & SIZE) == L_2)
				  switch (OP_SIZE (q->how))
				    {
				    case SB:                  break;
				    case SW: p->literal *= 2; break;
				    case SL: p->literal *= 4; break;
				    }
			      }
			    p->reg     = rdisp[opnum];
			  }
			else if (x & CTRL)
			  {
			    switch (reg[opnum])
			      {
			      case C_CCR:
				p->type = X (OP_CCR, SB);
				break;
			      case C_EXR:
				p->type = X (OP_EXR, SB);
				break;
			      case C_MACH:
				p->type = X (OP_MACH, SL);
				break;
			      case C_MACL:
				p->type = X (OP_MACL, SL);
				break;
			      case C_VBR:
				p->type = X (OP_VBR, SL);
				break;
			      case C_SBR:
				p->type = X (OP_SBR, SL);
				break;
			      }
			  }
			else if ((x & MODE) == CCR)
			  {
			    p->type = OP_CCR;
			  }
			else if ((x & MODE) == EXR)
			  {
			    p->type = OP_EXR;
			  }
			else
			  printf ("Hmmmm 0x%x...\n", x);

			args++;
		      }
		  }

		  /* Unary operators: treat src and dst as equivalent.  */
		  if (dst->dst.type == -1)
		    dst->dst = dst->src;
		  if (dst->src.type == -1)
		    dst->src = dst->dst;

		  dst->opcode = q->how;
		  dst->cycles = q->time;

		  /* And jsr's to these locations are turned into 
		     magic traps.  */

		  if (OP_KIND (dst->opcode) == O_JSR)
		    {
		      switch (dst->src.literal)
			{
			case 0xc5:
			  dst->opcode = O (O_SYS_OPEN, SB);
			  break;
			case 0xc6:
			  dst->opcode = O (O_SYS_READ, SB);
			  break;
			case 0xc7:
			  dst->opcode = O (O_SYS_WRITE, SB);
			  break;
			case 0xc8:
			  dst->opcode = O (O_SYS_LSEEK, SB);
			  break;
			case 0xc9:
			  dst->opcode = O (O_SYS_CLOSE, SB);
			  break;
			case 0xca:
			  dst->opcode = O (O_SYS_STAT, SB);
			  break;
			case 0xcb:
			  dst->opcode = O (O_SYS_FSTAT, SB);
			  break;
			case 0xcc:
			  dst->opcode = O (O_SYS_CMDLINE, SB);
			  break;
			}
		      /* End of Processing for system calls.  */
		    }

		  dst->next_pc = addr + len / 2;
		  return;
		}
	      else
		printf ("Don't understand 0x%x \n", looking_for);
	    }

	  len++;
	  nib++;
	}

    fail:
      ;
    }
 end:
  /* Fell off the end.  */
  dst->opcode = O (O_ILL, SB);
}

static void
compile (SIM_DESC sd, int pc)
{
  int idx;

  /* Find the next cache entry to use.  */
  idx = h8_get_cache_top (sd) + 1;
  h8_increment_compiles (sd);
  if (idx >= sd->sim_cache_size)
    {
      idx = 1;
    }
  h8_set_cache_top (sd, idx);

  /* Throw away its old meaning.  */
  h8_set_cache_idx (sd, sd->sim_cache[idx].oldpc, 0);

  /* Set to new address.  */
  sd->sim_cache[idx].oldpc = pc;

  /* Fill in instruction info.  */
  decode (sd, pc, h8_get_memory_buf (sd) + pc, sd->sim_cache + idx);

  /* Point to new cache entry.  */
  h8_set_cache_idx (sd, pc, idx);
}


static unsigned char  *breg[32];
static unsigned short *wreg[16];
static unsigned int   *lreg[18];

#define GET_B_REG(X)     *(breg[X])
#define SET_B_REG(X, Y) (*(breg[X])) = (Y)
#define GET_W_REG(X)     *(wreg[X])
#define SET_W_REG(X, Y) (*(wreg[X])) = (Y)
#define GET_L_REG(X)     h8_get_reg (sd, X)
#define SET_L_REG(X, Y)  h8_set_reg (sd, X, Y)

#define GET_MEMORY_L(X) \
  ((X) < memory_size \
   ? ((h8_get_memory (sd, (X)+0) << 24) | (h8_get_memory (sd, (X)+1) << 16)  \
    | (h8_get_memory (sd, (X)+2) <<  8) | (h8_get_memory (sd, (X)+3) <<  0)) \
   : ((h8_get_eightbit (sd, ((X)+0) & 0xff) << 24) \
    | (h8_get_eightbit (sd, ((X)+1) & 0xff) << 16) \
    | (h8_get_eightbit (sd, ((X)+2) & 0xff) <<  8) \
    | (h8_get_eightbit (sd, ((X)+3) & 0xff) <<  0)))

#define GET_MEMORY_W(X) \
  ((X) < memory_size \
   ? ((h8_get_memory   (sd, (X)+0) << 8) \
    | (h8_get_memory   (sd, (X)+1) << 0)) \
   : ((h8_get_eightbit (sd, ((X)+0) & 0xff) << 8) \
    | (h8_get_eightbit (sd, ((X)+1) & 0xff) << 0)))


#define GET_MEMORY_B(X) \
  ((X) < memory_size ? (h8_get_memory   (sd, (X))) \
                     : (h8_get_eightbit (sd, (X) & 0xff)))

#define SET_MEMORY_L(X, Y)  \
{  register unsigned char *_p; register int __y = (Y); \
   _p = ((X) < memory_size ? h8_get_memory_buf   (sd) +  (X) : \
                             h8_get_eightbit_buf (sd) + ((X) & 0xff)); \
   _p[0] = __y >> 24; _p[1] = __y >> 16; \
   _p[2] = __y >>  8; _p[3] = __y >>  0; \
}

#define SET_MEMORY_W(X, Y) \
{  register unsigned char *_p; register int __y = (Y); \
   _p = ((X) < memory_size ? h8_get_memory_buf   (sd) +  (X) : \
                             h8_get_eightbit_buf (sd) + ((X) & 0xff)); \
   _p[0] = __y >> 8; _p[1] = __y; \
}

#define SET_MEMORY_B(X, Y) \
  ((X) < memory_size ? (h8_set_memory   (sd, (X), (Y))) \
                     : (h8_set_eightbit (sd, (X) & 0xff, (Y))))

/* Simulate a memory fetch.
   Return 0 for success, -1 for failure.
*/

static int
fetch_1 (SIM_DESC sd, ea_type *arg, int *val, int twice)
{
  int rn = arg->reg;
  int abs = arg->literal;
  int r;
  int t;

  if (val == NULL)
    return -1;		/* Paranoia.  */

  switch (arg->type)
    {
      /* Indexed register plus displacement mode:

	 This new family of addressing modes are similar to OP_DISP
	 (register plus displacement), with two differences:
	   1) INDEXB uses only the least significant byte of the register,
	      INDEXW uses only the least significant word, and
	      INDEXL uses the entire register (just like OP_DISP).
	 and
	   2) The displacement value in abs is multiplied by two
	      for SW-sized operations, and by four for SL-size.

	This gives nine possible variations.
      */

    case X (OP_INDEXB, SB):
    case X (OP_INDEXB, SW):
    case X (OP_INDEXB, SL):
    case X (OP_INDEXW, SB):
    case X (OP_INDEXW, SW):
    case X (OP_INDEXW, SL):
    case X (OP_INDEXL, SB):
    case X (OP_INDEXL, SW):
    case X (OP_INDEXL, SL):
      t = GET_L_REG (rn);
      switch (OP_KIND (arg->type)) {
      case OP_INDEXB:	t &= 0xff;	break;
      case OP_INDEXW:	t &= 0xffff;	break;
      case OP_INDEXL:
      default:		break;
      }
      switch (OP_SIZE (arg->type)) {
      case SB:
	*val = GET_MEMORY_B ((t * 1 + abs) & h8_get_mask (sd));
	break;
      case SW:
	*val = GET_MEMORY_W ((t * 2 + abs) & h8_get_mask (sd));
	break;
      case SL:
	*val = GET_MEMORY_L ((t * 4 + abs) & h8_get_mask (sd));
	break;
      }
      break;

    case X (OP_LOWREG, SB):
      *val = GET_L_REG (rn) & 0xff;
      break;
    case X (OP_LOWREG, SW):
      *val = GET_L_REG (rn) & 0xffff; 
      break;

    case X (OP_REG, SB):	/* Register direct, byte.  */
      *val = GET_B_REG (rn);
      break;
    case X (OP_REG, SW):	/* Register direct, word.  */
      *val = GET_W_REG (rn);
      break;
    case X (OP_REG, SL):	/* Register direct, long.  */
      *val = GET_L_REG (rn);
      break;
    case X (OP_IMM, SB):	/* Immediate, byte.  */
    case X (OP_IMM, SW):	/* Immediate, word.  */
    case X (OP_IMM, SL):	/* Immediate, long.  */
      *val = abs;
      break;
    case X (OP_POSTINC, SB):	/* Register indirect w/post-incr: byte.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_B (t);
      if (!twice)
	t += 1;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;
    case X (OP_POSTINC, SW):	/* Register indirect w/post-incr: word.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_W (t);
      if (!twice)
	t += 2;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;
    case X (OP_POSTINC, SL):	/* Register indirect w/post-incr: long.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_L (t);
      if (!twice)
	t += 4;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;

    case X (OP_POSTDEC, SB):	/* Register indirect w/post-decr: byte.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_B (t);
      if (!twice)
	t -= 1;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;
    case X (OP_POSTDEC, SW):	/* Register indirect w/post-decr: word.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_W (t);
      if (!twice)
	t -= 2;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;
    case X (OP_POSTDEC, SL):	/* Register indirect w/post-decr: long.  */
      t = GET_L_REG (rn);
      t &= h8_get_mask (sd);
      r = GET_MEMORY_L (t);
      if (!twice)
	t -= 4;
      t = t & h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = r;
      break;

    case X (OP_PREDEC, SB):	/* Register indirect w/pre-decr: byte.  */
      t = GET_L_REG (rn) - 1;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_B (t);
      break;
      
    case X (OP_PREDEC, SW):	/* Register indirect w/pre-decr: word.  */
      t = GET_L_REG (rn) - 2;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_W (t);
      break;
      
    case X (OP_PREDEC, SL):	/* Register indirect w/pre-decr: long.  */
      t = GET_L_REG (rn) - 4;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_L (t);
      break;
      
    case X (OP_PREINC, SB):	/* Register indirect w/pre-incr: byte.  */
      t = GET_L_REG (rn) + 1;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_B (t);
      break;

    case X (OP_PREINC, SW):	/* Register indirect w/pre-incr: long.  */
      t = GET_L_REG (rn) + 2;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_W (t);
      break;

    case X (OP_PREINC, SL):	/* Register indirect w/pre-incr: long.  */
      t = GET_L_REG (rn) + 4;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      *val = GET_MEMORY_L (t);
      break;

    case X (OP_DISP, SB):	/* Register indirect w/displacement: byte.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      *val = GET_MEMORY_B (t);
      break;

    case X (OP_DISP, SW):	/* Register indirect w/displacement: word.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      *val = GET_MEMORY_W (t);
      break;

    case X (OP_DISP, SL):	/* Register indirect w/displacement: long.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      *val =GET_MEMORY_L (t);
      break;

    case X (OP_MEM, SL):	/* Absolute memory address, long.  */
      t = GET_MEMORY_L (abs);
      t &= h8_get_mask (sd);
      *val = t;
      break;

    case X (OP_MEM, SW):	/* Absolute memory address, word.  */
      t = GET_MEMORY_W (abs);
      t &= h8_get_mask (sd);
      *val = t;
      break;

    case X (OP_PCREL, SB):	/* PC relative (for jump, branch etc).  */
    case X (OP_PCREL, SW):
    case X (OP_PCREL, SL):
    case X (OP_PCREL, SN):
      *val = abs;
      break;

    case X (OP_MEM, SB):	/* Why isn't this implemented?  */
    default:
      sim_engine_set_run_state (sd, sim_stopped, SIGSEGV);
      return -1;
    }
  return 0;	/* Success.  */
}

/* Normal fetch.  */

static int
fetch (SIM_DESC sd, ea_type *arg, int *val)
{
  return fetch_1 (sd, arg, val, 0);
}

/* Fetch which will be followed by a store to the same location.
   The difference being that we don't want to do a post-increment
   or post-decrement at this time: we'll do it when we store.  */

static int
fetch2 (SIM_DESC sd, ea_type *arg, int *val)
{
  return fetch_1 (sd, arg, val, 1);
}

/* Simulate a memory store.
   Return 0 for success, -1 for failure.
*/

static int
store_1 (SIM_DESC sd, ea_type *arg, int n, int twice)
{
  int rn = arg->reg;
  int abs = arg->literal;
  int t;

  switch (arg->type)
    {
      /* Indexed register plus displacement mode:

	 This new family of addressing modes are similar to OP_DISP
	 (register plus displacement), with two differences:
	   1) INDEXB uses only the least significant byte of the register,
	      INDEXW uses only the least significant word, and
	      INDEXL uses the entire register (just like OP_DISP).
	 and
	   2) The displacement value in abs is multiplied by two
	      for SW-sized operations, and by four for SL-size.

	This gives nine possible variations.
      */

    case X (OP_INDEXB, SB):
    case X (OP_INDEXB, SW):
    case X (OP_INDEXB, SL):
    case X (OP_INDEXW, SB):
    case X (OP_INDEXW, SW):
    case X (OP_INDEXW, SL):
    case X (OP_INDEXL, SB):
    case X (OP_INDEXL, SW):
    case X (OP_INDEXL, SL):
      t = GET_L_REG (rn);
      switch (OP_KIND (arg->type)) {
      case OP_INDEXB:	t &= 0xff;	break;
      case OP_INDEXW:	t &= 0xffff;	break;
      case OP_INDEXL:
      default:		break;
      }
      switch (OP_SIZE (arg->type)) {
      case SB:
	SET_MEMORY_B ((t * 1 + abs) & h8_get_mask (sd), n);
	break;
      case SW:
	SET_MEMORY_W ((t * 2 + abs) & h8_get_mask (sd), n);
	break;
      case SL:
	SET_MEMORY_L ((t * 4 + abs) & h8_get_mask (sd), n);
	break;
      }
      break;

    case X (OP_REG, SB):	/* Register direct, byte.  */
      SET_B_REG (rn, n);
      break;
    case X (OP_REG, SW):	/* Register direct, word.  */
      SET_W_REG (rn, n);
      break;
    case X (OP_REG, SL):	/* Register direct, long.  */
      SET_L_REG (rn, n);
      break;

    case X (OP_PREDEC, SB):	/* Register indirect w/pre-decr, byte.  */
      t = GET_L_REG (rn);
      if (!twice)
	t -= 1;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_B (t, n);

      break;
    case X (OP_PREDEC, SW):	/* Register indirect w/pre-decr, word.  */
      t = GET_L_REG (rn);
      if (!twice)
	t -= 2;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_W (t, n);
      break;

    case X (OP_PREDEC, SL):	/* Register indirect w/pre-decr, long.  */
      t = GET_L_REG (rn);
      if (!twice)
	t -= 4;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_L (t, n);
      break;

    case X (OP_PREINC, SB):	/* Register indirect w/pre-incr, byte.  */
      t = GET_L_REG (rn);
      if (!twice)
	t += 1;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_B (t, n);

      break;
    case X (OP_PREINC, SW):	/* Register indirect w/pre-incr, word.  */
      t = GET_L_REG (rn);
      if (!twice)
	t += 2;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_W (t, n);
      break;

    case X (OP_PREINC, SL):	/* Register indirect w/pre-incr, long.  */
      t = GET_L_REG (rn);
      if (!twice)
	t += 4;
      t &= h8_get_mask (sd);
      SET_L_REG (rn, t);
      SET_MEMORY_L (t, n);
      break;

    case X (OP_POSTDEC, SB):	/* Register indirect w/post-decr, byte.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_B (t, n);
      SET_L_REG (rn, t - 1);
      break;

    case X (OP_POSTDEC, SW):	/* Register indirect w/post-decr, word.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_W (t, n);
      SET_L_REG (rn, t - 2);
      break;

    case X (OP_POSTDEC, SL):	/* Register indirect w/post-decr, long.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_L (t, n);
      SET_L_REG (rn, t - 4);
      break;

    case X (OP_POSTINC, SB):	/* Register indirect w/post-incr, byte.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_B (t, n);
      SET_L_REG (rn, t + 1);
      break;

    case X (OP_POSTINC, SW):	/* Register indirect w/post-incr, word.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_W (t, n);
      SET_L_REG (rn, t + 2);
      break;

    case X (OP_POSTINC, SL):	/* Register indirect w/post-incr, long.  */
      t = GET_L_REG (rn) & h8_get_mask (sd);
      SET_MEMORY_L (t, n);
      SET_L_REG (rn, t + 4);
      break;

    case X (OP_DISP, SB):	/* Register indirect w/displacement, byte.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      SET_MEMORY_B (t, n);
      break;

    case X (OP_DISP, SW):	/* Register indirect w/displacement, word.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      SET_MEMORY_W (t, n);
      break;

    case X (OP_DISP, SL):	/* Register indirect w/displacement, long.  */
      t = GET_L_REG (rn) + abs;
      t &= h8_get_mask (sd);
      SET_MEMORY_L (t, n);
      break;


    case X (OP_MEM, SB):	/* Why isn't this implemented?  */
    case X (OP_MEM, SW):	/* Why isn't this implemented?  */
    case X (OP_MEM, SL):	/* Why isn't this implemented?  */
    default:
      sim_engine_set_run_state (sd, sim_stopped, SIGSEGV);
      return -1;
    }
  return 0;
}

/* Normal store.  */

static int
store (SIM_DESC sd, ea_type *arg, int n)
{
  return store_1 (sd, arg, n, 0);
}

/* Store which follows a fetch from the same location.
   The difference being that we don't want to do a pre-increment
   or pre-decrement at this time: it was already done when we fetched.  */

static int
store2 (SIM_DESC sd, ea_type *arg, int n)
{
  return store_1 (sd, arg, n, 1);
}

static union
{
  short int i;
  struct
    {
      char low;
      char high;
    }
  u;
} littleendian;

/* Flag to be set whenever a new SIM_DESC object is created.  */
static int init_pointers_needed = 1;

static void
init_pointers (SIM_DESC sd)
{
  if (init_pointers_needed)
    {
      int i;

      littleendian.i = 1;

      if (h8300smode && !h8300_normal_mode)
	memory_size = H8300S_MSIZE;
      else if (h8300hmode && !h8300_normal_mode)
	memory_size = H8300H_MSIZE;
      else
	memory_size = H8300_MSIZE;
      /* `msize' must be a power of two.  */
      if ((memory_size & (memory_size - 1)) != 0)
	{
	  (*sim_callback->printf_filtered) 
	    (sim_callback,
	     "init_pointers: bad memory size %d, defaulting to %d.\n", 
	     memory_size, memory_size = H8300S_MSIZE);
	}

      if (h8_get_memory_buf (sd))
	free (h8_get_memory_buf (sd));
      if (h8_get_cache_idx_buf (sd))
	free (h8_get_cache_idx_buf (sd));
      if (h8_get_eightbit_buf (sd))
	free (h8_get_eightbit_buf (sd));

      h8_set_memory_buf (sd, (unsigned char *) 
			 calloc (sizeof (char), memory_size));
      h8_set_cache_idx_buf (sd, (unsigned short *) 
			    calloc (sizeof (short), memory_size));
      sd->memory_size = memory_size;
      h8_set_eightbit_buf (sd, (unsigned char *) calloc (sizeof (char), 256));

      h8_set_mask (sd, memory_size - 1);

      memset (h8_get_reg_buf (sd), 0, sizeof (((STATE_CPU (sd, 0))->regs)));

      for (i = 0; i < 8; i++)
	{
	  /* FIXME: rewrite using local buffer.  */
	  unsigned char *p = (unsigned char *) (h8_get_reg_buf (sd) + i);
	  unsigned char *e = (unsigned char *) (h8_get_reg_buf (sd) + i + 1);
	  unsigned short *q = (unsigned short *) (h8_get_reg_buf (sd) + i);
	  unsigned short *u = (unsigned short *) (h8_get_reg_buf (sd) + i + 1);
	  h8_set_reg (sd, i, 0x00112233);

	  while (p < e)
	    {
	      if (*p == 0x22)
		  breg[i] = p;
	      if (*p == 0x33)
		  breg[i + 8] = p;
	      if (*p == 0x11)
		breg[i + 16] = p;
	      if (*p == 0x00)
		breg[i + 24] = p;
	      p++;
	    }

	  wreg[i] = wreg[i + 8] = 0;
	  while (q < u)
	    {
	      if (*q == 0x2233)
		{
		  wreg[i] = q;
		}
	      if (*q == 0x0011)
		{
		  wreg[i + 8] = q;
		}
	      q++;
	    }

	  if (wreg[i] == 0 || wreg[i + 8] == 0)
	    (*sim_callback->printf_filtered) (sim_callback, 
					      "init_pointers: internal error.\n");

	  h8_set_reg (sd, i, 0);
	  lreg[i] = h8_get_reg_buf (sd) + i;
	}

      /* Note: sim uses pseudo-register ZERO as a zero register.  */
      lreg[ZERO_REGNUM] = h8_get_reg_buf (sd) + ZERO_REGNUM;
      init_pointers_needed = 0;

      /* Initialize the seg registers.  */
      if (!sd->sim_cache)
	set_simcache_size (sd, CSIZE);
    }
}

/* Grotty global variable for use by control_c signal handler.  */
static SIM_DESC control_c_sim_desc;

static void
control_c (int sig)
{
  sim_engine_set_run_state (control_c_sim_desc, sim_stopped, SIGINT);
}

int
sim_stop (SIM_DESC sd)
{
  /* FIXME: use a real signal value.  */
  sim_engine_set_run_state (sd, sim_stopped, SIGINT);
  return 1;
}

#define OBITOP(name, f, s, op) 			\
case O (name, SB):				\
{						\
  int m, tmp;					\
	 					\
  if (f)					\
    if (fetch (sd, &code->dst, &ea))		\
      goto end;					\
  if (fetch (sd, &code->src, &tmp))		\
    goto end;					\
  m = 1 << (tmp & 7);				\
  op;						\
  if (s)					\
    if (store (sd, &code->dst,ea))		\
      goto end;					\
  goto next;					\
}

void
sim_resume (SIM_DESC sd, int step, int siggnal)
{
  static int init1;
  int cycles = 0;
  int insts = 0;
  int tick_start = get_now ();
  void (*prev) ();
  int poll_count = 0;
  int res;
  int tmp;
  int rd;
  int ea;
  int bit;
  int pc;
  int c, nz, v, n, u, h, ui, intMaskBit;
  int trace, intMask;
  int oldmask;
  enum sim_stop reason;
  int sigrc;

  init_pointers (sd);

  control_c_sim_desc = sd;
  prev = signal (SIGINT, control_c);

  if (step)
    {
      sim_engine_set_run_state (sd, sim_stopped, SIGTRAP);
    }
  else
    {
      sim_engine_set_run_state (sd, sim_running, 0);
    }

  pc = h8_get_pc (sd);

  /* The PC should never be odd.  */
  if (pc & 0x1)
    {
      sim_engine_set_run_state (sd, sim_stopped, SIGBUS);
      return;
    }

  /* Get Status Register (flags).  */
  GETSR (sd);

  if (h8300smode)	/* Get exr.  */
    {
      trace = (h8_get_exr (sd) >> 7) & 1;
      intMask = h8_get_exr (sd) & 7;
    }

  oldmask = h8_get_mask (sd);
  if (!h8300hmode || h8300_normal_mode)
    h8_set_mask (sd, 0xffff);
  do
    {
      unsigned short cidx;
      decoded_inst *code;

    top:
      cidx = h8_get_cache_idx (sd, pc);
      if (cidx == (unsigned short) -1 ||
	  cidx >= sd->sim_cache_size)
	goto illegal;
	  
      code = sd->sim_cache + cidx;

#if ADEBUG
      if (debug)
	{
	  printf ("%x %d %s\n", pc, code->opcode,
		  code->op ? code->op->name : "**");
	}
      h8_increment_stats (sd, code->opcode);
#endif

      if (code->opcode)
	{
	  cycles += code->cycles;
	  insts++;
	}

      switch (code->opcode)
	{
	case 0:
	  /*
	   * This opcode is a fake for when we get to an
	   * instruction which hasnt been compiled
	   */
	  compile (sd, pc);
	  goto top;
	  break;

	case O (O_MOVAB, SL):
	case O (O_MOVAW, SL):
	case O (O_MOVAL, SL):
	  /* 1) Evaluate 2nd argument (dst).
	     2) Mask / zero extend according to whether 1st argument (src)
	        is INDEXB, INDEXW, or INDEXL.
	     3) Left-shift the result by 0, 1 or 2, according to size of mova
	        (mova/b, mova/w, mova/l).
	     4) Add literal value of 1st argument (src).
	     5) Store result in 3rd argument (op3).
	  */

	  /* Alas, since this is the only instruction with 3 arguments, 
	     decode doesn't handle them very well.  Some fix-up is required.

	     a) The size of dst is determined by whether src is 
	        INDEXB or INDEXW.  */

	  if (OP_KIND (code->src.type) == OP_INDEXB)
	    code->dst.type = X (OP_KIND (code->dst.type), SB);
	  else if (OP_KIND (code->src.type) == OP_INDEXW)
	    code->dst.type = X (OP_KIND (code->dst.type), SW);

	  /* b) If op3 == null, then this is the short form of the insn.
	        Dst is the dispreg of src, and op3 is the 32-bit form
		of the same register.
	  */

	  if (code->op3.type == 0)
	    {
	      /* Short form: src == INDEXB/INDEXW, dst == op3 == 0.
		 We get to compose dst and op3 as follows:

		     op3 is a 32-bit register, ID == src.reg.
		     dst is the same register, but 8 or 16 bits
		     depending on whether src is INDEXB or INDEXW.
	      */

	      code->op3.type = X (OP_REG, SL);
	      code->op3.reg  = code->src.reg;
	      code->op3.literal = 0;

	      if (OP_KIND (code->src.type) == OP_INDEXB)
		{
		  code->dst.type = X (OP_REG, SB);
		  code->dst.reg = code->op3.reg + 8;
		}
	      else
		code->dst.type = X (OP_REG, SW);
	    }

	  if (fetch (sd, &code->dst, &ea))
	    goto end;

	  switch (OP_KIND (code->src.type)) {
	  case OP_INDEXB:    ea = ea & 0xff;		break;
	  case OP_INDEXW:    ea = ea & 0xffff;		break;
	  case OP_INDEXL:    				break;
	  default:	     goto illegal;
	  }

	  switch (code->opcode) {
	  case O (O_MOVAB, SL):	    			break;
	  case O (O_MOVAW, SL):	    ea = ea << 1;	break;
	  case O (O_MOVAL, SL):     ea = ea << 2;	break;
	  default: 		    goto illegal;
	  }
	  
	  ea = ea + code->src.literal;

	  if (store (sd, &code->op3, ea))
	    goto end;

	  goto next;	  

	case O (O_SUBX, SB):	/* subx, extended sub */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -(ea + C);
	  res = rd + ea;
	  goto alu8;

	case O (O_SUBX, SW):	/* subx, extended sub */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -(ea + C);
	  res = rd + ea;
	  goto alu16;

	case O (O_SUBX, SL):	/* subx, extended sub */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -(ea + C);
	  res = rd + ea;
	  goto alu32;

	case O (O_ADDX, SB):	/* addx, extended add */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = ea + C;
	  res = rd + ea;
	  goto alu8;

	case O (O_ADDX, SW):	/* addx, extended add */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = ea + C;
	  res = rd + ea;
	  goto alu16;

	case O (O_ADDX, SL):	/* addx, extended add */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = ea + C;
	  res = rd + ea;
	  goto alu32;

	case O (O_SUB, SB):		/* sub.b */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto alu8;

	case O (O_SUB, SW):		/* sub.w */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto alu16;

	case O (O_SUB, SL):		/* sub.l */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto alu32;

	case O (O_NEG, SB):		/* neg.b */
	  /* Fetch ea.  */
	  if (fetch2 (sd, &code->src, &ea)) 
	    goto end;
	  ea = -ea;
	  rd = 0;
	  res = rd + ea;
	  goto alu8;

	case O (O_NEG, SW):		/* neg.w */
	  /* Fetch ea.  */
	  if (fetch2 (sd, &code->src, &ea)) 
	    goto end;
	  ea = -ea;
	  rd = 0;
	  res = rd + ea;
	  goto alu16;

	case O (O_NEG, SL):		/* neg.l */
	  /* Fetch ea.  */
	  if (fetch2 (sd, &code->src, &ea)) 
	    goto end;
	  ea = -ea;
	  rd = 0;
	  res = rd + ea;
	  goto alu32;

	case O (O_ADD, SB):		/* add.b */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  res = rd + ea;
	  goto alu8;

	case O (O_ADD, SW):		/* add.w */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  res = rd + ea;
	  goto alu16;

	case O (O_ADD, SL):		/* add.l */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  res = rd + ea;
	  goto alu32;

	case O (O_AND, SB):		/* and.b */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd & ea;
	  goto log8;

	case O (O_AND, SW):		/* and.w */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd & ea;
	  goto log16;

	case O (O_AND, SL):		/* and.l */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd & ea;
	  goto log32;

	case O (O_OR, SB):		/* or.b */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd | ea;
	  goto log8;

	case O (O_OR, SW):		/* or.w */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd | ea;
	  goto log16;

	case O (O_OR, SL):		/* or.l */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd | ea;
	  goto log32;

	case O (O_XOR, SB):		/* xor.b */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd ^ ea;
	  goto log8;

	case O (O_XOR, SW):		/* xor.w */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd ^ ea;
	  goto log16;

	case O (O_XOR, SL):		/* xor.l */
	  /* Fetch rd and ea.  */
	  if (fetch (sd, &code->src, &ea) || fetch2 (sd, &code->dst, &rd)) 
	    goto end;
	  res = rd ^ ea;
	  goto log32;

	case O (O_MOV, SB):
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto just_flags_log8;
	case O (O_MOV, SW):
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto just_flags_log16;
	case O (O_MOV, SL):
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto just_flags_log32;

	case O (O_MOVMD, SB):		/* movmd.b */
	  ea = GET_W_REG (4);
	  if (ea == 0)
	    ea = 0x10000;

	  while (ea--)
	    {
	      rd = GET_MEMORY_B (GET_L_REG (5));
	      SET_MEMORY_B (GET_L_REG (6), rd);
	      SET_L_REG (5, GET_L_REG (5) + 1);
	      SET_L_REG (6, GET_L_REG (6) + 1);
	      SET_W_REG (4, ea);
	    }
	  goto next;

	case O (O_MOVMD, SW):		/* movmd.w */
	  ea = GET_W_REG (4);
	  if (ea == 0)
	    ea = 0x10000;

	  while (ea--)
	    {
	      rd = GET_MEMORY_W (GET_L_REG (5));
	      SET_MEMORY_W (GET_L_REG (6), rd);
	      SET_L_REG (5, GET_L_REG (5) + 2);
	      SET_L_REG (6, GET_L_REG (6) + 2);
	      SET_W_REG (4, ea);
	    }
	  goto next;

	case O (O_MOVMD, SL):		/* movmd.l */
	  ea = GET_W_REG (4);
	  if (ea == 0)
	    ea = 0x10000;

	  while (ea--)
	    {
	      rd = GET_MEMORY_L (GET_L_REG (5));
	      SET_MEMORY_L (GET_L_REG (6), rd);
	      SET_L_REG (5, GET_L_REG (5) + 4);
	      SET_L_REG (6, GET_L_REG (6) + 4);
	      SET_W_REG (4, ea);
	    }
	  goto next;

	case O (O_MOVSD, SB):		/* movsd.b */
	  /* This instruction implements strncpy, with a conditional branch.
	     r4 contains n, r5 contains src, and r6 contains dst.
	     The 16-bit displacement operand is added to the pc
	     if and only if the end of string is reached before
	     n bytes are transferred.  */

	  ea = GET_L_REG (4) & 0xffff;
	  if (ea == 0)
	    ea = 0x10000;

	  while (ea--)
	    {
	      rd = GET_MEMORY_B (GET_L_REG (5));
	      SET_MEMORY_B (GET_L_REG (6), rd);
	      SET_L_REG (5, GET_L_REG (5) + 1);
	      SET_L_REG (6, GET_L_REG (6) + 1);
	      SET_W_REG (4, ea); 
	      if (rd == 0)
		goto condtrue;
	    }
	  goto next;

	case O (O_EEPMOV, SB):		/* eepmov.b */
	case O (O_EEPMOV, SW):		/* eepmov.w */
	  if (h8300hmode || h8300smode)
	    {
	      register unsigned char *_src, *_dst;
	      unsigned int count = ((code->opcode == O (O_EEPMOV, SW))
				    ? h8_get_reg (sd, R4_REGNUM) & 0xffff
				    : h8_get_reg (sd, R4_REGNUM) & 0xff);

	      _src = (h8_get_reg (sd, R5_REGNUM) < memory_size
		      ? h8_get_memory_buf   (sd) + h8_get_reg (sd, R5_REGNUM)
		      : h8_get_eightbit_buf (sd) + 
		       (h8_get_reg (sd, R5_REGNUM) & 0xff));
	      if ((_src + count) >= (h8_get_memory_buf (sd) + memory_size))
		{
		  if ((_src + count) >= (h8_get_eightbit_buf (sd) + 0x100))
		    goto illegal;
		}
	      _dst = (h8_get_reg (sd, R6_REGNUM) < memory_size
		      ? h8_get_memory_buf   (sd) + h8_get_reg (sd, R6_REGNUM)
		      : h8_get_eightbit_buf (sd) + 
		       (h8_get_reg (sd, R6_REGNUM) & 0xff));

	      if ((_dst + count) >= (h8_get_memory_buf (sd) + memory_size))
		{
		  if ((_dst + count) >= (h8_get_eightbit_buf (sd) + 0x100))
		    goto illegal;
		}
	      memcpy (_dst, _src, count);

	      h8_set_reg (sd, R5_REGNUM, h8_get_reg (sd, R5_REGNUM) + count);
	      h8_set_reg (sd, R6_REGNUM, h8_get_reg (sd, R6_REGNUM) + count);
	      h8_set_reg (sd, R4_REGNUM, h8_get_reg (sd, R4_REGNUM) &
			  ((code->opcode == O (O_EEPMOV, SW))
			  ? (~0xffff) : (~0xff)));
	      cycles += 2 * count;
	      goto next;
	    }
	  goto illegal;

	case O (O_ADDS, SL):		/* adds (.l) */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  SET_L_REG (code->dst.reg,
		     GET_L_REG (code->dst.reg)
		     + code->src.literal);

	  goto next;

	case O (O_SUBS, SL):		/* subs (.l) */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  SET_L_REG (code->dst.reg,
		     GET_L_REG (code->dst.reg)
		     - code->src.literal);
	  goto next;

	case O (O_CMP, SB):		/* cmp.b */
	  if (fetch (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto just_flags_alu8;

	case O (O_CMP, SW):		/* cmp.w */
	  if (fetch (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto just_flags_alu16;

	case O (O_CMP, SL):		/* cmp.l */
	  if (fetch (sd, &code->dst, &rd))
	    goto end;
	  if (fetch (sd, &code->src, &ea))
	    goto end;
	  ea = -ea;
	  res = rd + ea;
	  goto just_flags_alu32;

	case O (O_DEC, SB):		/* dec.b */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_B_REG (code->src.reg);
	  ea = -1;
	  res = rd + ea;
	  SET_B_REG (code->src.reg, res);
	  goto just_flags_inc8;

	case O (O_DEC, SW):		/* dec.w */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_W_REG (code->dst.reg);
	  ea = -code->src.literal;
	  res = rd + ea;
	  SET_W_REG (code->dst.reg, res);
	  goto just_flags_inc16;

	case O (O_DEC, SL):		/* dec.l */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_L_REG (code->dst.reg);
	  ea = -code->src.literal;
	  res = rd + ea;
	  SET_L_REG (code->dst.reg, res);
	  goto just_flags_inc32;

	case O (O_INC, SB):		/* inc.b */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_B_REG (code->src.reg);
	  ea = 1;
	  res = rd + ea;
	  SET_B_REG (code->src.reg, res);
	  goto just_flags_inc8;

	case O (O_INC, SW):		/* inc.w */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_W_REG (code->dst.reg);
	  ea = code->src.literal;
	  res = rd + ea;
	  SET_W_REG (code->dst.reg, res);
	  goto just_flags_inc16;

	case O (O_INC, SL):		/* inc.l */
	  /* FIXME fetch.
	   * This insn only uses register operands, but still
	   * it would be cleaner to use fetch and store...  */	  
	  rd = GET_L_REG (code->dst.reg);
	  ea = code->src.literal;
	  res = rd + ea;
	  SET_L_REG (code->dst.reg, res);
	  goto just_flags_inc32;

	case O (O_LDC, SB):		/* ldc.b */
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  goto setc;

	case O (O_LDC, SW):		/* ldc.w */
	  if (fetch (sd, &code->src, &res))
	    goto end;

	  /* Word operand, value from MSB, must be shifted.  */
	  res >>= 8;
	  goto setc;

	case O (O_LDC, SL):		/* ldc.l */
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  switch (code->dst.type) {
	  case X (OP_SBR, SL):
	    h8_set_sbr (sd, res);
	    break;
	  case X (OP_VBR, SL):
	    h8_set_vbr (sd, res);
	    break;
	  default:
	    goto illegal;
	  }
	  goto next;

	case O (O_STC, SW):		/* stc.w */
	case O (O_STC, SB):		/* stc.b */
	  if (code->src.type == X (OP_CCR, SB))
	    {
	      BUILDSR (sd);
	      res = h8_get_ccr (sd);
	    }
	  else if (code->src.type == X (OP_EXR, SB) && h8300smode)
	    {
	      if (h8300smode)
		h8_set_exr (sd, (trace << 7) | intMask);
	      res = h8_get_exr (sd);
	    }
	  else
	    goto illegal;

	  /* Word operand, value to MSB, must be shifted.  */
	  if (code->opcode == X (O_STC, SW))
	    res <<= 8;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;
	case O (O_STC, SL):		/* stc.l */
	  switch (code->src.type) {
	  case X (OP_SBR, SL):
	    res = h8_get_sbr (sd);
	    break;
	  case X (OP_VBR, SL):
	    res = h8_get_vbr (sd);
	    break;
	  default:
	    goto illegal;
	  }
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_ANDC, SB):		/* andc.b */
	  if (code->dst.type == X (OP_CCR, SB))
	    {
	      BUILDSR (sd);
	      rd = h8_get_ccr (sd);
	    }
	  else if (code->dst.type == X (OP_EXR, SB) && h8300smode)
	    {
	      if (h8300smode)
		h8_set_exr (sd, (trace << 7) | intMask);
	      rd = h8_get_exr (sd);
	    }
	  else
	    goto illegal;
	  ea = code->src.literal;
	  res = rd & ea;
	  goto setc;

	case O (O_ORC, SB):		/* orc.b */
	  if (code->dst.type == X (OP_CCR, SB))
	    {
	      BUILDSR (sd);
	      rd = h8_get_ccr (sd);
	    }
	  else if (code->dst.type == X (OP_EXR, SB) && h8300smode)
	    {
	      if (h8300smode)
		h8_set_exr (sd, (trace << 7) | intMask);
	      rd = h8_get_exr (sd);
	    }
	  else
	    goto illegal;
	  ea = code->src.literal;
	  res = rd | ea;
	  goto setc;

	case O (O_XORC, SB):		/* xorc.b */
	  if (code->dst.type == X (OP_CCR, SB))
	    {
	      BUILDSR (sd);
	      rd = h8_get_ccr (sd);
	    }
	  else if (code->dst.type == X (OP_EXR, SB) && h8300smode)
	    {
	      if (h8300smode)
		h8_set_exr (sd, (trace << 7) | intMask);
	      rd = h8_get_exr (sd);
	    }
	  else
	    goto illegal;
	  ea = code->src.literal;
	  res = rd ^ ea;
	  goto setc;

	case O (O_BRAS, SB):		/* bra/s  */
	  /* This is basically an ordinary branch, with a delay slot.  */
	  if (fetch (sd, &code->src, &res))
	    goto end;

	  if ((res & 1) == 0)
	    goto illegal;

	  res -= 1;

	  /* Execution continues at next instruction, but
	     delayed_branch is set up for next cycle.  */
	  h8_set_delayed_branch (sd, code->next_pc + res);
	  pc = code->next_pc;
	  goto end;

	case O (O_BRAB, SB):		/* bra rd.b */
	case O (O_BRAW, SW):		/* bra rd.w */
	case O (O_BRAL, SL):		/* bra erd.l */
	  if (fetch (sd, &code->src, &rd))
	    goto end;
	  switch (OP_SIZE (code->opcode)) {
	  case SB:	rd &= 0xff;		break;
	  case SW:	rd &= 0xffff;		break;
	  case SL:	rd &= 0xffffffff;	break;
	  }
	  pc = code->next_pc + rd;
	  goto end;

	case O (O_BRABC, SB):		/* bra/bc, branch if bit clear */
	case O (O_BRABS, SB):		/* bra/bs, branch if bit set   */
	case O (O_BSRBC, SB):		/* bsr/bc, call   if bit clear */
	case O (O_BSRBS, SB):		/* bsr/bs, call   if bit set   */
	  if (fetch (sd, &code->dst, &rd) ||
	      fetch (sd, &code->src, &bit))
	    goto end;

	  if (code->opcode == O (O_BRABC, SB) || /* branch if clear */
	      code->opcode == O (O_BSRBC, SB))	 /* call   if clear */
	    {
	      if ((rd & (1 << bit)))		/* no branch */
		goto next;
	    }
	  else					/* branch/call if set */
	    {
	      if (!(rd & (1 << bit)))		/* no branch */
		goto next;
	    }

	  if (fetch (sd, &code->op3, &res))	/* branch */
	    goto end;
	  pc = code->next_pc + res;

	  if (code->opcode == O (O_BRABC, SB) ||
	      code->opcode == O (O_BRABS, SB))	/* branch */
	    goto end;
	  else					/* call   */
	    goto call;

	case O (O_BRA, SN):
	case O (O_BRA, SL):
	case O (O_BRA, SW):
	case O (O_BRA, SB):		/* bra, branch always */
	  if (1)
	    goto condtrue;
	  goto next;

	case O (O_BRN, SB):		/* brn, ;-/  branch never? */
	  if (0)
	    goto condtrue;
	  goto next;

	case O (O_BHI, SB):		/* bhi */
	  if ((C || Z) == 0)
	    goto condtrue;
	  goto next;


	case O (O_BLS, SB):		/* bls */
	  if ((C || Z))
	    goto condtrue;
	  goto next;

	case O (O_BCS, SB):		/* bcs, branch if carry set */
	  if ((C == 1))
	    goto condtrue;
	  goto next;

	case O (O_BCC, SB):		/* bcc, branch if carry clear */
	  if ((C == 0))
	    goto condtrue;
	  goto next;

	case O (O_BEQ, SB):		/* beq, branch if zero set */
	  if (Z)
	    goto condtrue;
	  goto next;
	case O (O_BGT, SB):		/* bgt */
	  if (((Z || (N ^ V)) == 0))
	    goto condtrue;
	  goto next;

	case O (O_BLE, SB):		/* ble */
	  if (((Z || (N ^ V)) == 1))
	    goto condtrue;
	  goto next;

	case O (O_BGE, SB):		/* bge */
	  if ((N ^ V) == 0)
	    goto condtrue;
	  goto next;
	case O (O_BLT, SB):		/* blt */
	  if ((N ^ V))
	    goto condtrue;
	  goto next;
	case O (O_BMI, SB):		/* bmi */
	  if ((N))
	    goto condtrue;
	  goto next;
	case O (O_BNE, SB):		/* bne, branch if zero clear */
	  if ((Z == 0))
	    goto condtrue;
	  goto next;

	case O (O_BPL, SB):		/* bpl */
	  if (N == 0)
	    goto condtrue;
	  goto next;
	case O (O_BVC, SB):		/* bvc */
	  if ((V == 0))
	    goto condtrue;
	  goto next;
	case O (O_BVS, SB):		/* bvs */
	  if ((V == 1))
	    goto condtrue;
	  goto next;

	/* Trap for Command Line setup.  */
	case O (O_SYS_CMDLINE, SB):
	  {
	    int i = 0;		/* Loop counter.  */
	    int j = 0;		/* Loop counter.  */
	    int ind_arg_len = 0;	/* Length of each argument.  */
	    int no_of_args = 0;	/* The no. or cmdline args.  */
	    int current_location = 0;	/* Location of string.  */
	    int old_sp = 0;	/* The Initial Stack Pointer.  */
	    int no_of_slots = 0;	/* No. of slots required on the stack
					   for storing cmdline args.  */
	    int sp_move = 0;	/* No. of locations by which the stack needs
				   to grow.  */
	    int new_sp = 0;	/* The final stack pointer location passed
				   back.  */
	    int *argv_ptrs;	/* Pointers of argv strings to be stored.  */
	    int argv_ptrs_location = 0;	/* Location of pointers to cmdline
					   args on the stack.  */
	    int char_ptr_size = 0;	/* Size of a character pointer on
					   target machine.  */
	    int addr_cmdline = 0;	/* Memory location where cmdline has
					   to be stored.  */
	    int size_cmdline = 0;	/* Size of cmdline.  */

	    /* Set the address of 256 free locations where command line is
	       stored.  */
	    addr_cmdline = cmdline_location();
	    h8_set_reg (sd, 0, addr_cmdline);

	    /* Counting the no. of commandline arguments.  */
	    for (i = 0; h8_get_cmdline_arg (sd, i) != NULL; i++)
	      continue;

	    /* No. of arguments in the command line.  */
	    no_of_args = i;

	    /* Current location is just a temporary variable,which we are
	       setting to the point to the start of our commandline string.  */
	    current_location = addr_cmdline;

	    /* Allocating space for storing pointers of the command line
	       arguments.  */
	    argv_ptrs = (int *) malloc (sizeof (int) * no_of_args);

	    /* Setting char_ptr_size to the sizeof (char *) on the different
	       architectures.  */
	    if ((h8300hmode || h8300smode) && !h8300_normal_mode)
	      {
		char_ptr_size = 4;
	      }
	    else
	      {
		char_ptr_size = 2;
	      }

	    for (i = 0; i < no_of_args; i++)
	      {
		ind_arg_len = 0;

		/* The size of the commandline argument.  */
		ind_arg_len = strlen (h8_get_cmdline_arg (sd, i)) + 1;

		/* The total size of the command line string.  */
		size_cmdline += ind_arg_len;

		/* As we have only 256 bytes, we need to provide a graceful
		   exit. Anyways, a program using command line arguments 
		   where we cannot store all the command line arguments
		   given may behave unpredictably.  */
		if (size_cmdline >= 256)
		  {
		    h8_set_reg (sd, 0, 0);
		    goto next;
		  }
		else
		  {
		    /* current_location points to the memory where the next
		       commandline argument is stored.  */
		    argv_ptrs[i] = current_location;
		    for (j = 0; j < ind_arg_len; j++)
		      {
			SET_MEMORY_B ((current_location +
				       (sizeof (char) * j)),
				      *(h8_get_cmdline_arg (sd, i) + 
				       sizeof (char) * j));
		      }

		    /* Setting current_location to the starting of next
		       argument.  */
		    current_location += ind_arg_len;
		  }
	      }

	    /* This is the original position of the stack pointer.  */
	    old_sp = h8_get_reg (sd, SP_REGNUM);

	    /* We need space from the stack to store the pointers to argvs.  */
	    /* As we will infringe on the stack, we need to shift the stack
	       pointer so that the data is not overwritten. We calculate how
	       much space is required.  */
	    sp_move = (no_of_args) * (char_ptr_size);

	    /* The final position of stack pointer, we have thus taken some
	       space from the stack.  */
	    new_sp = old_sp - sp_move;

	    /* Temporary variable holding value where the argv pointers need
	       to be stored.  */
	    argv_ptrs_location = new_sp;

	    /* The argv pointers are stored at sequential locations. As per
	       the H8300 ABI.  */
	    for (i = 0; i < no_of_args; i++)
	      {
		/* Saving the argv pointer.  */
		if ((h8300hmode || h8300smode) && !h8300_normal_mode)
		  {
		    SET_MEMORY_L (argv_ptrs_location, argv_ptrs[i]);
		  }
		else
		  {
		    SET_MEMORY_W (argv_ptrs_location, argv_ptrs[i]);
		  }
	
		/* The next location where the pointer to the next argv
		   string has to be stored.  */    
		argv_ptrs_location += char_ptr_size;
	      }

	    /* Required by POSIX, Setting 0x0 at the end of the list of argv
	       pointers.  */
	    if ((h8300hmode || h8300smode) && !h8300_normal_mode)
	      {
		SET_MEMORY_L (old_sp, 0x0);
	      }
	    else
	      {
		SET_MEMORY_W (old_sp, 0x0);
	      }

	    /* Freeing allocated memory.  */
	    free (argv_ptrs);
	    for (i = 0; i <= no_of_args; i++)
	      {
		free (h8_get_cmdline_arg (sd, i));
	      }
	    free (h8_get_command_line (sd));

	    /* The no. of argv arguments are returned in Reg 0.  */
	    h8_set_reg (sd, 0, no_of_args);
	    /* The Pointer to argv in Register 1.  */
	    h8_set_reg (sd, 1, new_sp);
	    /* Setting the stack pointer to the new value.  */
	    h8_set_reg (sd, SP_REGNUM, new_sp);
	  }
	  goto next;

	  /* System call processing starts.  */
	case O (O_SYS_OPEN, SB):
	  {
	    int len = 0;	/* Length of filename.  */
	    char *filename;	/* Filename would go here.  */
	    char temp_char;	/* Temporary character */
	    int mode = 0;	/* Mode bits for the file.  */
	    int open_return;	/* Return value of open, file descriptor.  */
	    int i;		/* Loop counter */
	    int filename_ptr;	/* Pointer to filename in cpu memory.  */

	    /* Setting filename_ptr to first argument of open,  */
	    /* and trying to get mode.  */
	    if ((h8300sxmode || h8300hmode || h8300smode) && !h8300_normal_mode)
	      {
		filename_ptr = GET_L_REG (0);
		mode = GET_MEMORY_L (h8_get_reg (sd, SP_REGNUM) + 4);
	      }
	    else
	      {
		filename_ptr = GET_W_REG (0);
		mode = GET_MEMORY_W (h8_get_reg (sd, SP_REGNUM) + 2);
	      }

	    /* Trying to find the length of the filename.  */
	    temp_char = GET_MEMORY_B (h8_get_reg (sd, 0));

	    len = 1;
	    while (temp_char != '\0')
	      {
		temp_char = GET_MEMORY_B (filename_ptr + len);
		len++;
	      }

	    /* Allocating space for the filename.  */
	    filename = (char *) malloc (sizeof (char) * len);

	    /* String copying the filename from memory.  */
	    for (i = 0; i < len; i++)
	      {
		temp_char = GET_MEMORY_B (filename_ptr + i);
		filename[i] = temp_char;
	      }

	    /* Callback to open and return the file descriptor.  */
	    open_return = sim_callback->open (sim_callback, filename, mode);

	    /* Return value in register 0.  */
	    h8_set_reg (sd, 0, open_return);

	    /* Freeing memory used for filename. */
	    free (filename);
	  }
	  goto next;

	case O (O_SYS_READ, SB):
	  {
	    char *char_ptr;	/* Where characters read would be stored.  */
	    int fd;		/* File descriptor */
	    int buf_size;	/* BUF_SIZE parameter in read.  */
	    int i = 0;		/* Temporary Loop counter */
	    int read_return = 0;	/* Return value from callback to
					   read.  */

	    fd = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);
	    buf_size = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (2) : GET_W_REG (2);

	    char_ptr = (char *) malloc (sizeof (char) * buf_size);

	    /* Callback to read and return the no. of characters read.  */
	    read_return =
	      sim_callback->read (sim_callback, fd, char_ptr, buf_size);

	    /* The characters read are stored in cpu memory.  */
	    for (i = 0; i < buf_size; i++)
	      {
		SET_MEMORY_B ((h8_get_reg (sd, 1) + (sizeof (char) * i)),
			      *(char_ptr + (sizeof (char) * i)));
	      }

	    /* Return value in Register 0.  */
	    h8_set_reg (sd, 0, read_return);

	    /* Freeing memory used as buffer.  */
	    free (char_ptr);
	  }
	  goto next;

	case O (O_SYS_WRITE, SB):
	  {
	    int fd;		/* File descriptor */
	    char temp_char;	/* Temporary character */
	    int len;		/* Length of write, Parameter II to write.  */
	    int char_ptr;	/* Character Pointer, Parameter I of write.  */
	    char *ptr;		/* Where characters to be written are stored. 
				 */
	    int write_return;	/* Return value from callback to write.  */
	    int i = 0;		/* Loop counter */

	    fd = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);
	    char_ptr = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (1) : GET_W_REG (1);
	    len = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (2) : GET_W_REG (2);

	    /* Allocating space for the characters to be written.  */
	    ptr = (char *) malloc (sizeof (char) * len);

	    /* Fetching the characters from cpu memory.  */
	    for (i = 0; i < len; i++)
	      {
		temp_char = GET_MEMORY_B (char_ptr + i);
		ptr[i] = temp_char;
	      }

	    /* Callback write and return the no. of characters written.  */
	    write_return = sim_callback->write (sim_callback, fd, ptr, len);

	    /* Return value in Register 0.  */
	    h8_set_reg (sd, 0, write_return);

	    /* Freeing memory used as buffer.  */
	    free (ptr);
	  }
	  goto next;

	case O (O_SYS_LSEEK, SB):
	  {
	    int fd;		/* File descriptor */
	    int offset;		/* Offset */
	    int origin;		/* Origin */
	    int lseek_return;	/* Return value from callback to lseek.  */

	    fd = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);
	    offset = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (1) : GET_W_REG (1);
	    origin = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (2) : GET_W_REG (2);

	    /* Callback lseek and return offset.  */
	    lseek_return =
	      sim_callback->lseek (sim_callback, fd, offset, origin);

	    /* Return value in register 0.  */
	    h8_set_reg (sd, 0, lseek_return);
	  }
	  goto next;

	case O (O_SYS_CLOSE, SB):
	  {
	    int fd;		/* File descriptor */
	    int close_return;	/* Return value from callback to close.  */

	    fd = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);

	    /* Callback close and return.  */
	    close_return = sim_callback->close (sim_callback, fd);

	    /* Return value in register 0.  */
	    h8_set_reg (sd, 0, close_return);
	  }
	  goto next;

	case O (O_SYS_FSTAT, SB):
	  {
	    int fd;		/* File descriptor */
	    struct stat stat_rec;	/* Stat record */
	    int fstat_return;	/* Return value from callback to stat.  */
	    int stat_ptr;	/* Pointer to stat record.  */
	    char *temp_stat_ptr;	/* Temporary stat_rec pointer.  */

	    fd = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);

	    /* Setting stat_ptr to second argument of stat.  */
	    stat_ptr = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (1) : GET_W_REG (1);

	    /* Callback stat and return.  */
	    fstat_return = sim_callback->fstat (sim_callback, fd, &stat_rec);

	    /* Have stat_ptr point to starting of stat_rec.  */
	    temp_stat_ptr = (char *) (&stat_rec);

	    /* Setting up the stat structure returned.  */
	    SET_MEMORY_W (stat_ptr, stat_rec.st_dev);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_ino);
	    stat_ptr += 2;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_mode);
	    stat_ptr += 4;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_nlink);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_uid);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_gid);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_rdev);
	    stat_ptr += 2;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_size);
	    stat_ptr += 4;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_atime);
	    stat_ptr += 8;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_mtime);
	    stat_ptr += 8;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_ctime);

	    /* Return value in register 0.  */
	    h8_set_reg (sd, 0, fstat_return);
	  }
	  goto next;

	case O (O_SYS_STAT, SB):
	  {
	    int len = 0;	/* Length of filename.  */
	    char *filename;	/* Filename would go here.  */
	    char temp_char;	/* Temporary character */
	    int filename_ptr;	/* Pointer to filename in cpu memory.  */
	    struct stat stat_rec;	/* Stat record */
	    int stat_return;	/* Return value from callback to stat */
	    int stat_ptr;	/* Pointer to stat record.  */
	    char *temp_stat_ptr;	/* Temporary stat_rec pointer.  */
	    int i = 0;		/* Loop Counter */

	    /* Setting filename_ptr to first argument of open.  */
	    filename_ptr = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (0) : GET_W_REG (0);

	    /* Trying to find the length of the filename.  */
	    temp_char = GET_MEMORY_B (h8_get_reg (sd, 0));

	    len = 1;
	    while (temp_char != '\0')
	      {
		temp_char = GET_MEMORY_B (filename_ptr + len);
		len++;
	      }

	    /* Allocating space for the filename.  */
	    filename = (char *) malloc (sizeof (char) * len);

	    /* String copying the filename from memory.  */
	    for (i = 0; i < len; i++)
	      {
		temp_char = GET_MEMORY_B (filename_ptr + i);
		filename[i] = temp_char;
	      }

	    /* Setting stat_ptr to second argument of stat.  */
	    /* stat_ptr = h8_get_reg (sd, 1); */
	    stat_ptr = (h8300hmode && !h8300_normal_mode) ? GET_L_REG (1) : GET_W_REG (1);

	    /* Callback stat and return.  */
	    stat_return =
	      sim_callback->stat (sim_callback, filename, &stat_rec);

	    /* Have stat_ptr point to starting of stat_rec.  */
	    temp_stat_ptr = (char *) (&stat_rec);
 
	    /* Freeing memory used for filename.  */
	    free (filename);
 
	    /* Setting up the stat structure returned.  */
	    SET_MEMORY_W (stat_ptr, stat_rec.st_dev);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_ino);
	    stat_ptr += 2;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_mode);
	    stat_ptr += 4;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_nlink);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_uid);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_gid);
	    stat_ptr += 2;
	    SET_MEMORY_W (stat_ptr, stat_rec.st_rdev);
	    stat_ptr += 2;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_size);
	    stat_ptr += 4;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_atime);
	    stat_ptr += 8;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_mtime);
	    stat_ptr += 8;
	    SET_MEMORY_L (stat_ptr, stat_rec.st_ctime);
 
	    /* Return value in register 0.  */
	    h8_set_reg (sd, 0, stat_return);
	  }
	  goto next;
	  /* End of system call processing.  */

	case O (O_NOT, SB):		/* not.b */
	  if (fetch2 (sd, &code->src, &rd))
	    goto end;
	  rd = ~rd; 
	  v = 0;
	  goto shift8;

	case O (O_NOT, SW):		/* not.w */
	  if (fetch2 (sd, &code->src, &rd))
	    goto end;
	  rd = ~rd; 
	  v = 0;
	  goto shift16;

	case O (O_NOT, SL):		/* not.l */
	  if (fetch2 (sd, &code->src, &rd))
	    goto end;
	  rd = ~rd; 
	  v = 0;
	  goto shift32;

	case O (O_SHLL, SB):	/* shll.b */
	case O (O_SHLR, SB):	/* shlr.b */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (memcmp (&code->src, &code->dst, sizeof (code->src)) == 0)
	    ea = 1;		/* unary  op */
	  else			/* binary op */
	    fetch (sd, &code->src, &ea);

	  if (code->opcode == O (O_SHLL, SB))
	    {
	      v = (ea > 8);
	      c = rd & (0x80 >> (ea - 1));
	      rd <<= ea;
	    }
	  else
	    {
	      v = 0;
	      c = rd & (1 << (ea - 1));
	      rd = (unsigned char) rd >> ea;
	    }
	  goto shift8;

	case O (O_SHLL, SW):	/* shll.w */
	case O (O_SHLR, SW):	/* shlr.w */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (memcmp (&code->src, &code->dst, sizeof (code->src)) == 0)
	    ea = 1;		/* unary  op */
	  else
	    fetch (sd, &code->src, &ea);

	  if (code->opcode == O (O_SHLL, SW))
	    {
	      v = (ea > 16);
	      c = rd & (0x8000 >> (ea - 1));
	      rd <<= ea;
	    }
	  else
	    {
	      v = 0;
	      c = rd & (1 << (ea - 1));
	      rd = (unsigned short) rd >> ea;
	    }
	  goto shift16;

	case O (O_SHLL, SL):	/* shll.l */
	case O (O_SHLR, SL):	/* shlr.l */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (memcmp (&code->src, &code->dst, sizeof (code->src)) == 0)
	    ea = 1;		/* unary  op */
	  else
	    fetch (sd, &code->src, &ea);

	  if (code->opcode == O (O_SHLL, SL))
	    {
	      v = (ea > 32);
	      c = rd & (0x80000000 >> (ea - 1));
	      rd <<= ea;
	    }
	  else
	    {
	      v = 0;
	      c = rd & (1 << (ea - 1));
	      rd = (unsigned int) rd >> ea;
	    }
	  goto shift32;

	case O (O_SHAL, SB):
	case O (O_SHAR, SB):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SB))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  if (code->opcode == O (O_SHAL, SB))
	    {
	      c = rd & (0x80 >> (ea - 1));
	      res = rd >> (7 - ea);
	      v = ((res & 1) && !(res & 2)) 
		|| (!(res & 1) && (res & 2));
	      rd <<= ea;
	    }
	  else
	    {
	      c = rd & (1 << (ea - 1));
	      v = 0;
	      rd = ((signed char) rd) >> ea;
	    }
	  goto shift8;

	case O (O_SHAL, SW):
	case O (O_SHAR, SW):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SW))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  if (code->opcode == O (O_SHAL, SW))
	    {
	      c = rd & (0x8000 >> (ea - 1));
	      res = rd >> (15 - ea);
	      v = ((res & 1) && !(res & 2)) 
		|| (!(res & 1) && (res & 2));
	      rd <<= ea;
	    }
	  else
	    {
	      c = rd & (1 << (ea - 1));
	      v = 0;
	      rd = ((signed short) rd) >> ea;
	    }
	  goto shift16;

	case O (O_SHAL, SL):
	case O (O_SHAR, SL):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SL))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  if (code->opcode == O (O_SHAL, SL))
	    {
	      c = rd & (0x80000000 >> (ea - 1));
	      res = rd >> (31 - ea);
	      v = ((res & 1) && !(res & 2)) 
		|| (!(res & 1) && (res & 2));
	      rd <<= ea;
	    }
	  else
	    {
	      c = rd & (1 << (ea - 1));
	      v = 0;
	      rd = ((signed int) rd) >> ea;
	    }
	  goto shift32;

	case O (O_ROTL, SB):
	case O (O_ROTR, SB):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SB))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTL, SB))
	      {
		c = rd & 0x80;
		rd <<= 1;
		if (c)
		  rd |= 1;
	      }
	    else
	      {
		c = rd & 1;
		rd = ((unsigned char) rd) >> 1;
		if (c)
		  rd |= 0x80;
	      }

	  v = 0;
	  goto shift8;

	case O (O_ROTL, SW):
	case O (O_ROTR, SW):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SW))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTL, SW))
	      {
		c = rd & 0x8000;
		rd <<= 1;
		if (c)
		  rd |= 1;
	      }
	    else
	      {
		c = rd & 1;
		rd = ((unsigned short) rd) >> 1;
		if (c)
		  rd |= 0x8000;
	      }

	  v = 0;
	  goto shift16;

	case O (O_ROTL, SL):
	case O (O_ROTR, SL):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SL))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTL, SL))
	      {
		c = rd & 0x80000000;
		rd <<= 1;
		if (c)
		  rd |= 1;
	      }
	    else
	      {
		c = rd & 1;
		rd = ((unsigned int) rd) >> 1;
		if (c)
		  rd |= 0x80000000;
	      }

	  v = 0;
	  goto shift32;

	case O (O_ROTXL, SB):
	case O (O_ROTXR, SB):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SB))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTXL, SB))
	      {
		res = rd & 0x80;
		rd <<= 1;
		if (C)
		  rd |= 1;
		c = res;
	      }
	    else
	      {
		res = rd & 1;
		rd = ((unsigned char) rd) >> 1;
		if (C)
		  rd |= 0x80;
		c = res;
	      }

	  v = 0;
	  goto shift8;

	case O (O_ROTXL, SW):
	case O (O_ROTXR, SW):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SW))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTXL, SW))
	      {
		res = rd & 0x8000;
		rd <<= 1;
		if (C)
		  rd |= 1;
		c = res;
	      }
	    else
	      {
		res = rd & 1;
		rd = ((unsigned short) rd) >> 1;
		if (C)
		  rd |= 0x8000;
		c = res;
	      }

	  v = 0;
	  goto shift16;

	case O (O_ROTXL, SL):
	case O (O_ROTXR, SL):
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;

	  if (code->src.type == X (OP_IMM, SL))
	    fetch (sd, &code->src, &ea);
	  else
	    ea = 1;

	  while (ea--)
	    if (code->opcode == O (O_ROTXL, SL))
	      {
		res = rd & 0x80000000;
		rd <<= 1;
		if (C)
		  rd |= 1;
		c = res;
	      }
	    else
	      {
		res = rd & 1;
		rd = ((unsigned int) rd) >> 1;
		if (C)
		  rd |= 0x80000000;
		c = res;
	      }

	  v = 0;
	  goto shift32;

        case O (O_JMP, SN):
        case O (O_JMP, SL):
        case O (O_JMP, SB):		/* jmp */
        case O (O_JMP, SW):
	  fetch (sd, &code->src, &pc);
	  goto end;

	case O (O_JSR, SN):
	case O (O_JSR, SL):
	case O (O_JSR, SB):		/* jsr, jump to subroutine */
	case O (O_JSR, SW):
	  if (fetch (sd, &code->src, &pc))
	    goto end;
	call:
	  tmp = h8_get_reg (sd, SP_REGNUM);

	  if (h8300hmode && !h8300_normal_mode)
	    {
	      tmp -= 4;
	      SET_MEMORY_L (tmp, code->next_pc);
	    }
	  else
	    {
	      tmp -= 2;
	      SET_MEMORY_W (tmp, code->next_pc);
	    }
	  h8_set_reg (sd, SP_REGNUM, tmp);

	  goto end;

	case O (O_BSR, SW):
	case O (O_BSR, SL):
	case O (O_BSR, SB):		/* bsr, branch to subroutine */
	  if (fetch (sd, &code->src, &res))
	    goto end;
	  pc = code->next_pc + res;
	  goto call;

	case O (O_RTE, SN):		/* rte, return from exception */
	rte:
	  /* Pops exr and ccr before pc -- otherwise identical to rts.  */
	  tmp = h8_get_reg (sd, SP_REGNUM);

	  if (h8300smode)			/* pop exr */
	    {
	      h8_set_exr (sd, GET_MEMORY_L (tmp));
	      tmp += 4;
	    }
	  if (h8300hmode && !h8300_normal_mode)
	    {
	      h8_set_ccr (sd, GET_MEMORY_L (tmp));
	      tmp += 4;
	      pc = GET_MEMORY_L (tmp);
	      tmp += 4;
	    }
	  else
	    {
	      h8_set_ccr (sd, GET_MEMORY_W (tmp));
	      tmp += 2;
	      pc = GET_MEMORY_W (tmp);
	      tmp += 2;
	    }

	  GETSR (sd);
	  h8_set_reg (sd, SP_REGNUM, tmp);
	  goto end;

	case O (O_RTS, SN):		/* rts, return from subroutine */
	rts:
	  tmp = h8_get_reg (sd, SP_REGNUM);

	  if (h8300hmode && !h8300_normal_mode)
	    {
	      pc = GET_MEMORY_L (tmp);
	      tmp += 4;
	    }
	  else
	    {
	      pc = GET_MEMORY_W (tmp);
	      tmp += 2;
	    }

	  h8_set_reg (sd, SP_REGNUM, tmp);
	  goto end;

	case O (O_ILL, SB):		/* illegal */
	  sim_engine_set_run_state (sd, sim_stopped, SIGILL);
	  goto end;

	case O (O_SLEEP, SN):		/* sleep */
	  /* Check for magic numbers in r1 and r2.  */
	  if ((h8_get_reg (sd, R1_REGNUM) & 0xffff) == LIBC_EXIT_MAGIC1 &&
	      (h8_get_reg (sd, R2_REGNUM) & 0xffff) == LIBC_EXIT_MAGIC2 &&
	      SIM_WIFEXITED (h8_get_reg (sd, 0)))
	    {
	      /* This trap comes from _exit, not from gdb.  */
	      sim_engine_set_run_state (sd, sim_exited, 
					SIM_WEXITSTATUS (h8_get_reg (sd, 0)));
	    }
#if 0
	  /* Unfortunately this won't really work, because
	     when we take a breakpoint trap, R0 has a "random", 
	     user-defined value.  Don't see any immediate solution.  */
	  else if (SIM_WIFSTOPPED (h8_get_reg (sd, 0)))
	    {
	      /* Pass the stop signal up to gdb.  */
	      sim_engine_set_run_state (sd, sim_stopped, 
					SIM_WSTOPSIG (h8_get_reg (sd, 0)));
	    }
#endif
	  else
	    {
	      /* Treat it as a sigtrap.  */
	      sim_engine_set_run_state (sd, sim_stopped, SIGTRAP);
	    }
	  goto end;

	case O (O_TRAPA, SB):		/* trapa */
	  if (fetch (sd, &code->src, &res))
   	    goto end;			/* res is vector number.  */
  
   	  tmp = h8_get_reg (sd, SP_REGNUM);
   	  if(h8300_normal_mode)
   	    {
   	      tmp -= 2;
   	      SET_MEMORY_W (tmp, code->next_pc);
   	      tmp -= 2;
   	      SET_MEMORY_W (tmp, h8_get_ccr (sd));
   	    }
   	  else
   	    {
   	      tmp -= 4;
   	      SET_MEMORY_L (tmp, code->next_pc);
   	      tmp -= 4;
   	      SET_MEMORY_L (tmp, h8_get_ccr (sd));
   	    }
   	  intMaskBit = 1;
   	  BUILDSR (sd);
 
	  if (h8300smode)
	    {
	      tmp -= 4;
	      SET_MEMORY_L (tmp, h8_get_exr (sd));
	    }

	  h8_set_reg (sd, SP_REGNUM, tmp);

	  if(h8300_normal_mode)
	    pc = GET_MEMORY_L (0x10 + res * 2); /* Vector addresses are 0x10,0x12,0x14 and 0x16 */
	  else
	    pc = GET_MEMORY_L (0x20 + res * 4);
	  goto end;

	case O (O_BPT, SN):
	  sim_engine_set_run_state (sd, sim_stopped, SIGTRAP);
	  goto end;

	case O (O_BSETEQ, SB):
	  if (Z)
	    goto bset;
	  goto next;

	case O (O_BSETNE, SB):
	  if (!Z)
	    goto bset;
	  goto next;

	case O (O_BCLREQ, SB):
	  if (Z)
	    goto bclr;
	  goto next;

	case O (O_BCLRNE, SB):
	  if (!Z)
	    goto bclr;
	  goto next;

	  OBITOP (O_BNOT, 1, 1, ea ^= m);		/* bnot */
	  OBITOP (O_BTST, 1, 0, nz = ea & m);		/* btst */
	bset:
	  OBITOP (O_BSET, 1, 1, ea |= m);		/* bset */
	bclr:
	  OBITOP (O_BCLR, 1, 1, ea &= ~m);		/* bclr */
	  OBITOP (O_BLD, 1, 0, c = ea & m);		/* bld  */
	  OBITOP (O_BILD, 1, 0, c = !(ea & m));		/* bild */
	  OBITOP (O_BST, 1, 1, ea &= ~m;
		  if (C) ea |= m);			/* bst  */
	  OBITOP (O_BIST, 1, 1, ea &= ~m;
		  if (!C) ea |= m);			/* bist */
	  OBITOP (O_BSTZ, 1, 1, ea &= ~m;
		  if (Z) ea |= m);			/* bstz */
	  OBITOP (O_BISTZ, 1, 1, ea &= ~m;
		  if (!Z) ea |= m);			/* bistz */
	  OBITOP (O_BAND, 1, 0, c = (ea & m) && C);	/* band */
	  OBITOP (O_BIAND, 1, 0, c = !(ea & m) && C);	/* biand */
	  OBITOP (O_BOR, 1, 0, c = (ea & m) || C);	/* bor  */
	  OBITOP (O_BIOR, 1, 0, c = !(ea & m) || C);	/* bior */
	  OBITOP (O_BXOR, 1, 0, c = ((ea & m) != 0)!= C);	/* bxor */
	  OBITOP (O_BIXOR, 1, 0, c = !(ea & m) != C);	/* bixor */

	case O (O_BFLD, SB):				/* bfld */
	  /* bitfield load */
	  ea = 0;
	  if (fetch (sd, &code->src, &bit))
	    goto end;

	  if (bit != 0)
	    {
	      if (fetch (sd, &code->dst, &ea))
		goto end;

	      ea &= bit;
	      while (!(bit & 1))
		{
		  ea  >>= 1;
		  bit >>= 1;
		}
	    }
	  if (store (sd, &code->op3, ea))
	    goto end;

	  goto next;

	case O(O_BFST, SB):			/* bfst */
	  /* bitfield store */
	  /* NOTE: the imm8 value is in dst, and the ea value
	     (which is actually the destination) is in op3.
	     It has to be that way, to avoid breaking the assembler.  */

	  if (fetch (sd, &code->dst, &bit))	/* imm8 */
	    goto end;
	  if (bit == 0)				/* noop -- nothing to do.  */
	    goto next;

	  if (fetch (sd, &code->src, &rd))	/* reg8 src */
	    goto end;

	  if (fetch2 (sd, &code->op3, &ea))	/* ea dst */
	    goto end;

	  /* Left-shift the register data into position.  */
	  for (tmp = bit; !(tmp & 1); tmp >>= 1)
	    rd <<= 1;

	  /* Combine it with the neighboring bits.  */
	  ea = (ea & ~bit) | (rd & bit);

	  /* Put it back.  */
	  if (store2 (sd, &code->op3, ea))
	    goto end;
	  goto next;

	case O (O_CLRMAC, SN):		/* clrmac */
	  h8_set_mach (sd, 0);
	  h8_set_macl (sd, 0);
	  h8_set_macZ (sd, 1);
	  h8_set_macV (sd, 0);
	  h8_set_macN (sd, 0);
	  goto next;

	case O (O_STMAC, SL):		/* stmac, 260 */
	  switch (code->src.type) {
	  case X (OP_MACH, SL): 
	    res = h8_get_mach (sd);
	    if (res & 0x200)		/* sign extend */
	      res |= 0xfffffc00;
	    break;
	  case X (OP_MACL, SL): 
	    res = h8_get_macl (sd);
	    break;
	  default:	goto illegal;
	  }
	  nz = !h8_get_macZ (sd);
	  n = h8_get_macN (sd);
	  v = h8_get_macV (sd);

	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_LDMAC, SL):		/* ldmac, 179 */
	  if (fetch (sd, &code->src, &rd))
	    goto end;

	  switch (code->dst.type) {
	  case X (OP_MACH, SL):	
	    rd &= 0x3ff;		/* Truncate to 10 bits */
	    h8_set_mach (sd, rd);
	    break;
	  case X (OP_MACL, SL):	
	    h8_set_macl (sd, rd);
	    break;
	  default:	goto illegal;
	  }
	  h8_set_macV (sd, 0);
	  goto next;

	case O (O_MAC, SW):
	  if (fetch (sd, &code->src, &rd) ||
	      fetch (sd, &code->dst, &res))
	    goto end;

	  /* Ye gods, this is non-portable!
	     However, the existing mul/div code is similar.  */
	  res = SEXTSHORT (res) * SEXTSHORT (rd);

	  if (h8_get_macS (sd))		/* Saturating mode */
	    {
	      long long mac = h8_get_macl (sd);

	      if (mac & 0x80000000)		/* sign extend */
		mac |= 0xffffffff00000000LL;

	      mac += res;
	      if (mac > 0x7fffffff || mac < 0xffffffff80000000LL)
		h8_set_macV (sd, 1);
	      h8_set_macZ (sd, (mac == 0));
	      h8_set_macN (sd, (mac  < 0));
	      h8_set_macl (sd, (int) mac);
	    }
	  else				/* "Less Saturating" mode */
	    {
	      long long mac = h8_get_mach (sd);
	      mac <<= 32;
	      mac += h8_get_macl (sd);

	      if (mac & 0x20000000000LL)	/* sign extend */
		mac |= 0xfffffc0000000000LL;

	      mac += res;
	      if (mac > 0x1ffffffffffLL || 
		  mac < (long long) 0xfffffe0000000000LL)
		h8_set_macV (sd, 1);
	      h8_set_macZ (sd, (mac == 0));
	      h8_set_macN (sd, (mac  < 0));
	      h8_set_macl (sd, (int) mac);
	      mac >>= 32;
	      h8_set_mach (sd, (int) (mac & 0x3ff));
	    }
	  goto next;

	case O (O_MULS, SW):		/* muls.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  ea = SEXTSHORT (ea);
	  res = SEXTSHORT (ea * SEXTSHORT (rd));

	  n  = res & 0x8000;
	  nz = res & 0xffff;
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULS, SL):		/* muls.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  res = ea * rd;

	  n  = res & 0x80000000;
	  nz = res & 0xffffffff;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_MULSU, SL):		/* muls/u.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  /* Compute upper 32 bits of the 64-bit result.  */
	  res = (((long long) ea) * ((long long) rd)) >> 32;

	  n  = res & 0x80000000;
	  nz = res & 0xffffffff;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_MULU, SW):		/* mulu.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  res = UEXTSHORT ((UEXTSHORT (ea) * UEXTSHORT (rd)));

	  /* Don't set Z or N.  */
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULU, SL):		/* mulu.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  res = ea * rd;

	  /* Don't set Z or N.  */
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULUU, SL):		/* mulu/u.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  /* Compute upper 32 bits of the 64-bit result.  */
	  res = (((unsigned long long) (unsigned) ea) *
		 ((unsigned long long) (unsigned) rd)) >> 32;

	  /* Don't set Z or N.  */
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULXS, SB):		/* mulxs.b */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  ea = SEXTCHAR (ea);
	  res = ea * SEXTCHAR (rd);

	  n  = res & 0x8000;
	  nz = res & 0xffff;
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULXS, SW):		/* mulxs.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  ea = SEXTSHORT (ea);
	  res = ea * SEXTSHORT (rd & 0xffff);

	  n  = res & 0x80000000;
	  nz = res & 0xffffffff;
	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULXU, SB):		/* mulxu.b */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  res = UEXTCHAR (ea) * UEXTCHAR (rd);

	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_MULXU, SW):		/* mulxu.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  res = UEXTSHORT (ea) * UEXTSHORT (rd);

	  if (store (sd, &code->dst, res))
	    goto end;

	  goto next;

	case O (O_TAS, SB):		/* tas (test and set) */
	  if (!h8300sxmode)		/* h8sx can use any register. */
	    switch (code->src.reg)
	      {
	      case R0_REGNUM:
	      case R1_REGNUM:
	      case R4_REGNUM:
	      case R5_REGNUM:
		break;
	      default:
		goto illegal;
	      }

	  if (fetch (sd, &code->src, &res))
	    goto end;
	  if (store (sd, &code->src, res | 0x80))
	    goto end;

	  goto just_flags_log8;

	case O (O_DIVU, SW):			/* divu.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  n  = ea & 0x8000;
	  nz = ea & 0xffff;
	  if (ea)
	    res = (unsigned) (UEXTSHORT (rd) / UEXTSHORT (ea));
	  else
	    res = 0;

	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_DIVU, SL):			/* divu.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  n  = ea & 0x80000000;
	  nz = ea & 0xffffffff;
	  if (ea)
	    res = (unsigned) rd / ea;
	  else
	    res = 0;

	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_DIVS, SW):			/* divs.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  if (ea)
	    {
	      res = SEXTSHORT (rd) / SEXTSHORT (ea);
	      nz  = 1;
	    }
	  else
	    {
	      res = 0;
	      nz  = 0;
	    }

	  n = res & 0x8000;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_DIVS, SL):			/* divs.l */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  if (ea)
	    {
	      res = rd / ea;
	      nz  = 1;
	    }
	  else
	    {
	      res = 0;
	      nz  = 0;
	    }

	  n = res & 0x80000000;
	  if (store (sd, &code->dst, res))
	    goto end;
	  goto next;

	case O (O_DIVXU, SB):			/* divxu.b */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  rd = UEXTSHORT (rd);
	  ea = UEXTCHAR (ea);

	  n  = ea & 0x80;
	  nz = ea & 0xff;
	  if (ea)
	    {
	      tmp = (unsigned) rd % ea;
	      res = (unsigned) rd / ea;
	    }
	  else
	    {
	      tmp = 0;
	      res = 0;
	    }

	  if (store (sd, &code->dst, (res & 0xff) | (tmp << 8)))
	    goto end;
	  goto next;

	case O (O_DIVXU, SW):			/* divxu.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  ea = UEXTSHORT (ea);

	  n  = ea & 0x8000;
	  nz = ea & 0xffff;
	  if (ea)
	    {
	      tmp = (unsigned) rd % ea;
	      res = (unsigned) rd / ea;
	    }
	  else
	    {
	      tmp = 0;
	      res = 0;
	    }

	  if (store (sd, &code->dst, (res & 0xffff) | (tmp << 16)))
	    goto end;
	  goto next;

	case O (O_DIVXS, SB):			/* divxs.b */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  rd = SEXTSHORT (rd);
	  ea = SEXTCHAR (ea);

	  if (ea)
	    {
	      tmp = (int) rd % (int) ea;
	      res = (int) rd / (int) ea;
	      nz  = 1;
	    }
	  else
	    {
	      tmp = 0;
	      res = 0;
	      nz  = 0;
	    }

	  n = res & 0x8000;
	  if (store (sd, &code->dst, (res & 0xff) | (tmp << 8)))
	    goto end;
	  goto next;

	case O (O_DIVXS, SW):			/* divxs.w */
	  if (fetch (sd, &code->src, &ea) ||
	      fetch (sd, &code->dst, &rd))
	    goto end;

	  ea = SEXTSHORT (ea);

	  if (ea)
	    {
	      tmp = (int) rd % (int) ea;
	      res = (int) rd / (int) ea;
	      nz  = 1;
	    }
	  else
	    {
	      tmp = 0;
	      res = 0;
	      nz  = 0;
	    }

	  n = res & 0x80000000;
	  if (store (sd, &code->dst, (res & 0xffff) | (tmp << 16)))
	    goto end;
	  goto next;

	case O (O_EXTS, SW):			/* exts.w, signed extend */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  ea = rd & 0x80 ? -256 : 0;
	  res = (rd & 0xff) + ea;
	  goto log16;

	case O (O_EXTS, SL):			/* exts.l, signed extend */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (code->src.type == X (OP_IMM, SL))
	    {
	      if (fetch (sd, &code->src, &ea))
		goto end;

	      if (ea == 2)			/* exts.l #2, nn */
		{
		  /* Sign-extend from 8-bit to 32-bit.  */
		  ea = rd & 0x80 ? -256 : 0;
		  res = (rd & 0xff) + ea;
		  goto log32;
		}
	    }
	  /* Sign-extend from 16-bit to 32-bit.  */
	  ea = rd & 0x8000 ? -65536 : 0;
	  res = (rd & 0xffff) + ea;
	  goto log32;

	case O (O_EXTU, SW):			/* extu.w, unsigned extend */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  ea = 0;
	  res = (rd & 0xff) + ea;
	  goto log16;

	case O (O_EXTU, SL):			/* extu.l, unsigned extend */
	  if (fetch2 (sd, &code->dst, &rd))
	    goto end;
	  if (code->src.type == X (OP_IMM, SL))
	    {
	      if (fetch (sd, &code->src, &ea))
		goto end;

	      if (ea == 2)			/* extu.l #2, nn */
		{
		  /* Zero-extend from 8-bit to 32-bit.  */
		  ea = 0;
		  res = (rd & 0xff) + ea;
		  goto log32;
		}
	    }
	  /* Zero-extend from 16-bit to 32-bit.  */
	  ea = 0;
	  res = (rd & 0xffff) + ea;
	  goto log32;

	case O (O_NOP, SN):			/* nop */
	  goto next;

	case O (O_STM, SL):			/* stm, store to memory */
	  {
	    int nregs, firstreg, i;

	    nregs = GET_MEMORY_B (pc + 1);
	    nregs >>= 4;
	    nregs &= 0xf;
	    firstreg = code->src.reg;
	    firstreg &= 0xf;
	    for (i = firstreg; i <= firstreg + nregs; i++)
	      {
		h8_set_reg (sd, SP_REGNUM, h8_get_reg (sd, SP_REGNUM) - 4);
		SET_MEMORY_L (h8_get_reg (sd, SP_REGNUM), h8_get_reg (sd, i));
	      }
	  }
	  goto next;

	case O (O_LDM, SL):			/* ldm,  load from memory */
	case O (O_RTEL, SN):			/* rte/l, ldm plus rte */
	case O (O_RTSL, SN):			/* rts/l, ldm plus rts */
	  {
	    int nregs, firstreg, i;

	    nregs = ((GET_MEMORY_B (pc + 1) >> 4) & 0xf);
	    firstreg = code->dst.reg & 0xf;
	    for (i = firstreg; i >= firstreg - nregs; i--)
	      {
		h8_set_reg (sd, i, GET_MEMORY_L (h8_get_reg (sd, SP_REGNUM)));
		h8_set_reg (sd, SP_REGNUM, h8_get_reg (sd, SP_REGNUM) + 4);
	      }
	  }
	  switch (code->opcode) {
	  case O (O_RTEL, SN):
	    goto rte;
	  case O (O_RTSL, SN):
	    goto rts;
	  case O (O_LDM, SL):
	    goto next;
	  default:
	    goto illegal;
	  }

	case O (O_DAA, SB):
	  /* Decimal Adjust Addition.  This is for BCD arithmetic.  */
	  res = GET_B_REG (code->src.reg);	/* FIXME fetch? */
	  if (!c && (0 <= (res >>  4) && (res >>  4) <= 9) && 
	      !h && (0 <= (res & 0xf) && (res & 0xf) <= 9))
	    res = res;		/* Value added == 0.  */
	  else if (!c && (0  <= (res >>  4) && (res >>  4) <=  8) && 
		   !h && (10 <= (res & 0xf) && (res & 0xf) <= 15))
	    res = res + 0x6;		/* Value added == 6.  */
	  else if (!c && (0 <= (res >>  4) && (res >>  4) <= 9) && 
		    h && (0 <= (res & 0xf) && (res & 0xf) <= 3))
	    res = res + 0x6;		/* Value added == 6.  */
	  else if (!c && (10 <= (res >>  4) && (res >>  4) <= 15) && 
		   !h && (0  <= (res & 0xf) && (res & 0xf) <=  9))
	    res = res + 0x60;		/* Value added == 60.  */
	  else if (!c && (9  <= (res >>  4) && (res >>  4) <= 15) && 
		   !h && (10 <= (res & 0xf) && (res & 0xf) <= 15))
	    res = res + 0x66;		/* Value added == 66.  */
	  else if (!c && (10 <= (res >>  4) && (res >>  4) <= 15) && 
		    h && (0  <= (res & 0xf) && (res & 0xf) <=  3))
	    res = res + 0x66;		/* Value added == 66.  */
	  else if ( c && (1 <= (res >>  4) && (res >>  4) <= 2) && 
		   !h && (0 <= (res & 0xf) && (res & 0xf) <= 9))
	    res = res + 0x60;		/* Value added == 60.  */
	  else if ( c && (1  <= (res >>  4) && (res >>  4) <=  2) && 
		   !h && (10 <= (res & 0xf) && (res & 0xf) <= 15))
	    res = res + 0x66;		/* Value added == 66.  */
	  else if (c && (1 <= (res >>  4) && (res >>  4) <= 3) && 
		   h && (0 <= (res & 0xf) && (res & 0xf) <= 3))
	    res = res + 0x66;		/* Value added == 66.  */

	  goto alu8;

	case O (O_DAS, SB):
	  /* Decimal Adjust Subtraction.  This is for BCD arithmetic.  */
	  res = GET_B_REG (code->src.reg); /* FIXME fetch, fetch2... */
	  if (!c && (0 <= (res >>  4) && (res >>  4) <= 9) && 
	      !h && (0 <= (res & 0xf) && (res & 0xf) <= 9))
	    res = res;		/* Value added == 0.  */
	  else if (!c && (0 <= (res >>  4) && (res >>  4) <=  8) && 
		    h && (6 <= (res & 0xf) && (res & 0xf) <= 15))
	    res = res + 0xfa;		/* Value added == 0xfa.  */
	  else if ( c && (7 <= (res >>  4) && (res >>  4) <= 15) && 
		   !h && (0 <= (res & 0xf) && (res & 0xf) <=  9))
	    res = res + 0xa0;		/* Value added == 0xa0.  */
	  else if (c && (6 <= (res >>  4) && (res >>  4) <= 15) && 
		   h && (6 <= (res & 0xf) && (res & 0xf) <= 15))
	    res = res + 0x9a;		/* Value added == 0x9a.  */

	  goto alu8;

	default:
	illegal:
	  sim_engine_set_run_state (sd, sim_stopped, SIGILL);
	  goto end;

	}

      (*sim_callback->printf_filtered) (sim_callback,
					"sim_resume: internal error.\n");
      sim_engine_set_run_state (sd, sim_stopped, SIGILL);
      goto end;

    setc:
      if (code->dst.type == X (OP_CCR, SB) ||
	  code->dst.type == X (OP_CCR, SW))
	{
	  h8_set_ccr (sd, res);
	  GETSR (sd);
	}
      else if (h8300smode &&
	       (code->dst.type == X (OP_EXR, SB) ||
		code->dst.type == X (OP_EXR, SW)))
	{
	  h8_set_exr (sd, res);
	  if (h8300smode)	/* Get exr.  */
	    {
	      trace = (h8_get_exr (sd) >> 7) & 1;
	      intMask = h8_get_exr (sd) & 7;
	    }
	}
      else
	goto illegal;

      goto next;

    condtrue:
      /* When a branch works */
      if (fetch (sd, &code->src, &res))
	goto end;
      if (res & 1)		/* bad address */
	goto illegal;
      pc = code->next_pc + res;
      goto end;

      /* Set the cond codes from res */
    bitop:

      /* Set the flags after an 8 bit inc/dec operation */
    just_flags_inc8:
      n = res & 0x80;
      nz = res & 0xff;
      v = (rd & 0x7f) == 0x7f;
      goto next;

      /* Set the flags after an 16 bit inc/dec operation */
    just_flags_inc16:
      n = res & 0x8000;
      nz = res & 0xffff;
      v = (rd & 0x7fff) == 0x7fff;
      goto next;

      /* Set the flags after an 32 bit inc/dec operation */
    just_flags_inc32:
      n = res & 0x80000000;
      nz = res & 0xffffffff;
      v = (rd & 0x7fffffff) == 0x7fffffff;
      goto next;

    shift8:
      /* Set flags after an 8 bit shift op, carry,overflow set in insn */
      n = (rd & 0x80);
      nz = rd & 0xff;
      if (store2 (sd, &code->dst, rd))
	goto end;
      goto next;

    shift16:
      /* Set flags after an 16 bit shift op, carry,overflow set in insn */
      n = (rd & 0x8000);
      nz = rd & 0xffff;
      if (store2 (sd, &code->dst, rd))
	goto end;
      goto next;

    shift32:
      /* Set flags after an 32 bit shift op, carry,overflow set in insn */
      n = (rd & 0x80000000);
      nz = rd & 0xffffffff;
      if (store2 (sd, &code->dst, rd))
	goto end;
      goto next;

    log32:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_log32:
      /* flags after a 32bit logical operation */
      n = res & 0x80000000;
      nz = res & 0xffffffff;
      v = 0;
      goto next;

    log16:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_log16:
      /* flags after a 16bit logical operation */
      n = res & 0x8000;
      nz = res & 0xffff;
      v = 0;
      goto next;

    log8:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_log8:
      n = res & 0x80;
      nz = res & 0xff;
      v = 0;
      goto next;

    alu8:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_alu8:
      n = res & 0x80;
      nz = res & 0xff;
      c = (res & 0x100);
      switch (code->opcode / 4)
	{
	case O_ADD:
	case O_ADDX:
	  v = ((rd & 0x80) == (ea & 0x80)
	       && (rd & 0x80) != (res & 0x80));
	  break;
	case O_SUB:
	case O_SUBX:
	case O_CMP:
	  v = ((rd & 0x80) != (-ea & 0x80)
	       && (rd & 0x80) != (res & 0x80));
	  break;
	case O_NEG:
	  v = (rd == 0x80);
	  break;
	case O_DAA:
	case O_DAS:
	  break;	/* No effect on v flag.  */
	}
      goto next;

    alu16:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_alu16:
      n = res & 0x8000;
      nz = res & 0xffff;
      c = (res & 0x10000);
      switch (code->opcode / 4)
	{
	case O_ADD:
	case O_ADDX:
	  v = ((rd & 0x8000) == (ea & 0x8000)
	       && (rd & 0x8000) != (res & 0x8000));
	  break;
	case O_SUB:
	case O_SUBX:
	case O_CMP:
	  v = ((rd & 0x8000) != (-ea & 0x8000)
	       && (rd & 0x8000) != (res & 0x8000));
	  break;
	case O_NEG:
	  v = (rd == 0x8000);
	  break;
	}
      goto next;

    alu32:
      if (store2 (sd, &code->dst, res))
	goto end;

    just_flags_alu32:
      n = res & 0x80000000;
      nz = res & 0xffffffff;
      switch (code->opcode / 4)
	{
	case O_ADD:
	case O_ADDX:
	  v = ((rd & 0x80000000) == (ea & 0x80000000)
	       && (rd & 0x80000000) != (res & 0x80000000));
	  c = ((unsigned) res < (unsigned) rd) || 
	    ((unsigned) res < (unsigned) ea);
	  break;
	case O_SUB:
	case O_SUBX:
	case O_CMP:
	  v = ((rd & 0x80000000) != (-ea & 0x80000000)
	       && (rd & 0x80000000) != (res & 0x80000000));
	  c = (unsigned) rd < (unsigned) -ea;
	  break;
	case O_NEG:
	  v = (rd == 0x80000000);
	  c = res != 0;
	  break;
	}
      goto next;

    next:
      if ((res = h8_get_delayed_branch (sd)) != 0)
	{
	  pc = res;
	  h8_set_delayed_branch (sd, 0);
	}
      else
	pc = code->next_pc;

    end:
      
      if (--poll_count < 0)
	{
	  poll_count = POLL_QUIT_INTERVAL;
	  if ((*sim_callback->poll_quit) != NULL
	      && (*sim_callback->poll_quit) (sim_callback))
	    sim_engine_set_run_state (sd, sim_stopped, SIGINT);
	}
      sim_engine_get_run_state (sd, &reason, &sigrc);
    } while (reason == sim_running);

  h8_set_ticks (sd, h8_get_ticks (sd) + get_now () - tick_start);
  h8_set_cycles (sd, h8_get_cycles (sd) + cycles);
  h8_set_insts (sd, h8_get_insts (sd) + insts);
  h8_set_pc (sd, pc);
  BUILDSR (sd);

  if (h8300smode)
    h8_set_exr (sd, (trace<<7) | intMask);

  h8_set_mask (sd, oldmask);
  signal (SIGINT, prev);
}

int
sim_trace (SIM_DESC sd)
{
  /* FIXME: Unfinished.  */
  (*sim_callback->printf_filtered) (sim_callback,
				    "sim_trace: trace not supported.\n");
  return 1;	/* Done.  */
}

int
sim_write (SIM_DESC sd, SIM_ADDR addr, unsigned char *buffer, int size)
{
  int i;

  init_pointers (sd);
  if (addr < 0)
    return 0;
  for (i = 0; i < size; i++)
    {
      if (addr < memory_size)
	{
	  h8_set_memory    (sd, addr + i, buffer[i]);
	  h8_set_cache_idx (sd, addr + i,  0);
	}
      else
	{
	  h8_set_eightbit (sd, (addr + i) & 0xff, buffer[i]);
	}
    }
  return size;
}

int
sim_read (SIM_DESC sd, SIM_ADDR addr, unsigned char *buffer, int size)
{
  init_pointers (sd);
  if (addr < 0)
    return 0;
  if (addr < memory_size)
    memcpy (buffer, h8_get_memory_buf (sd) + addr, size);
  else
    memcpy (buffer, h8_get_eightbit_buf (sd) + (addr & 0xff), size);
  return size;
}


int
sim_store_register (SIM_DESC sd, int rn, unsigned char *value, int length)
{
  int longval;
  int shortval;
  int intval;
  longval = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
  shortval = (value[0] << 8) | (value[1]);
  intval = h8300hmode ? longval : shortval;

  init_pointers (sd);
  switch (rn)
    {
    case PC_REGNUM:
      if(h8300_normal_mode)
        h8_set_pc (sd, shortval); /* PC for Normal mode is 2 bytes */
      else
        h8_set_pc (sd, intval);
      break;
    default:
      (*sim_callback->printf_filtered) (sim_callback, 
					"sim_store_register: bad regnum %d.\n",
					rn);
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
      h8_set_reg (sd, rn, intval);
      break;
    case CCR_REGNUM:
      h8_set_ccr (sd, intval);
      break;
    case EXR_REGNUM:
      h8_set_exr (sd, intval);
      break;
    case SBR_REGNUM:
      h8_set_sbr (sd, intval);
      break;
    case VBR_REGNUM:
      h8_set_vbr (sd, intval);
      break;
    case MACH_REGNUM:
      h8_set_mach (sd, intval);
      break;
    case MACL_REGNUM:
      h8_set_macl (sd, intval);
      break;
    case CYCLE_REGNUM:
      h8_set_cycles (sd, longval);
      break;

    case INST_REGNUM:
      h8_set_insts (sd, longval);
      break;

    case TICK_REGNUM:
      h8_set_ticks (sd, longval);
      break;
    }
  return -1;
}

int
sim_fetch_register (SIM_DESC sd, int rn, unsigned char *buf, int length)
{
  int v;
  int longreg = 0;

  init_pointers (sd);

  if (!h8300smode && rn >= EXR_REGNUM)
    rn++;
  switch (rn)
    {
    default:
      (*sim_callback->printf_filtered) (sim_callback, 
					"sim_fetch_register: bad regnum %d.\n",
					rn);
      v = 0;
      break;
    case CCR_REGNUM:
      v = h8_get_ccr (sd);
      break;
    case EXR_REGNUM:
      v = h8_get_exr (sd);
      break;
    case PC_REGNUM:
      v = h8_get_pc (sd);
      break;
    case SBR_REGNUM:
      v = h8_get_sbr (sd);
      break;
    case VBR_REGNUM:
      v = h8_get_vbr (sd);
      break;
    case MACH_REGNUM:
      v = h8_get_mach (sd);
      break;
    case MACL_REGNUM:
      v = h8_get_macl (sd);
      break;
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
      v = h8_get_reg (sd, rn);
      break;
    case CYCLE_REGNUM:
      v = h8_get_cycles (sd);
      longreg = 1;
      break;
    case TICK_REGNUM:
      v = h8_get_ticks (sd);
      longreg = 1;
      break;
    case INST_REGNUM:
      v = h8_get_insts (sd);
      longreg = 1;
      break;
    }
  /* In Normal mode PC is 2 byte, but other registers are 4 byte */
  if ((h8300hmode || longreg) && !(rn == PC_REGNUM && h8300_normal_mode))
    {
      buf[0] = v >> 24;
      buf[1] = v >> 16;
      buf[2] = v >> 8;
      buf[3] = v >> 0;
    }
  else
    {
      buf[0] = v >> 8;
      buf[1] = v;
    }
  return -1;
}

void
sim_stop_reason (SIM_DESC sd, enum sim_stop *reason, int *sigrc)
{
  sim_engine_get_run_state (sd, reason, sigrc);
}

/* FIXME: Rename to sim_set_mem_size.  */

void
sim_size (int n)
{
  /* Memory size is fixed.  */
}

static void
set_simcache_size (SIM_DESC sd, int n)
{
  if (sd->sim_cache)
    free (sd->sim_cache);
  if (n < 2)
    n = 2;
  sd->sim_cache = (decoded_inst *) malloc (sizeof (decoded_inst) * n);
  memset (sd->sim_cache, 0, sizeof (decoded_inst) * n);
  sd->sim_cache_size = n;
}


void
sim_info (SIM_DESC sd, int verbose)
{
  double timetaken = (double) h8_get_ticks (sd) / (double) now_persec ();
  double virttime = h8_get_cycles (sd) / 10.0e6;

  (*sim_callback->printf_filtered) (sim_callback,
				    "\n\n#instructions executed  %10d\n",
				    h8_get_insts (sd));
  (*sim_callback->printf_filtered) (sim_callback,
				    "#cycles (v approximate) %10d\n",
				    h8_get_cycles (sd));
  (*sim_callback->printf_filtered) (sim_callback,
				    "#real time taken        %10.4f\n",
				    timetaken);
  (*sim_callback->printf_filtered) (sim_callback,
				    "#virtual time taken     %10.4f\n",
				    virttime);
  if (timetaken != 0.0)
    (*sim_callback->printf_filtered) (sim_callback,
				      "#simulation ratio       %10.4f\n",
				      virttime / timetaken);
  (*sim_callback->printf_filtered) (sim_callback,
				    "#compiles               %10d\n",
				    h8_get_compiles (sd));
  (*sim_callback->printf_filtered) (sim_callback,
				    "#cache size             %10d\n",
				    sd->sim_cache_size);

#ifdef ADEBUG
  /* This to be conditional on `what' (aka `verbose'),
     however it was never passed as non-zero.  */
  if (1)
    {
      int i;
      for (i = 0; i < O_LAST; i++)
	{
	  if (h8_get_stats (sd, i))
	    (*sim_callback->printf_filtered) (sim_callback, "%d: %d\n", 
					      i, h8_get_stats (sd, i));
	}
    }
#endif
}

/* Indicate whether the cpu is an H8/300 or H8/300H.
   FLAG is non-zero for the H8/300H.  */

void
set_h8300h (unsigned long machine)
{
  /* FIXME: Much of the code in sim_load can be moved to sim_open.
     This function being replaced by a sim_open:ARGV configuration
     option.  */

  h8300hmode = h8300smode = h8300sxmode = h8300_normal_mode = 0;

  if (machine == bfd_mach_h8300sx || machine == bfd_mach_h8300sxn)
    h8300sxmode = 1;

  if (machine == bfd_mach_h8300s || machine == bfd_mach_h8300sn || h8300sxmode)
    h8300smode = 1;

  if (machine == bfd_mach_h8300h || machine == bfd_mach_h8300hn || h8300smode)
    h8300hmode = 1;

  if(machine == bfd_mach_h8300hn || machine == bfd_mach_h8300sn || machine == bfd_mach_h8300sxn)
    h8300_normal_mode = 1;
}

/* Cover function of sim_state_free to free the cpu buffers as well.  */

static void
free_state (SIM_DESC sd)
{
  if (STATE_MODULES (sd) != NULL)
    sim_module_uninstall (sd);

  /* Fixme: free buffers in _sim_cpu.  */
  sim_state_free (sd);
}

SIM_DESC
sim_open (SIM_OPEN_KIND kind, 
	  struct host_callback_struct *callback, 
	  struct bfd *abfd, 
	  char **argv)
{
  SIM_DESC sd;
  sim_cpu *cpu;

  sd = sim_state_alloc (kind, callback);
  sd->cpu = sim_cpu_alloc (sd, 0);
  cpu = STATE_CPU (sd, 0);
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_state_initialize (sd, cpu);
  /* sim_cpu object is new, so some initialization is needed.  */
  init_pointers_needed = 1;

  /* For compatibility (FIXME: is this right?).  */
  current_alignment = NONSTRICT_ALIGNMENT;
  current_target_byte_order = BIG_ENDIAN;

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

    /* getopt will print the error message so we just have to exit if
       this fails.  FIXME: Hmmm...  in the case of gdb we need getopt
       to call print_filtered.  */
  if (sim_parse_args (sd, argv) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      free_state (sd);
      return 0;
    }

  /* Check for/establish the a reference program image.  */
  if (sim_analyze_program (sd,
			   (STATE_PROG_ARGV (sd) != NULL
			    ? *STATE_PROG_ARGV (sd)
			    : NULL), abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Establish any remaining configuration options.  */
  if (sim_config (sd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  if (sim_post_argv_init (sd) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      free_state (sd);
      return 0;
    }

  /*  sim_hw_configure (sd); */

  /* FIXME: Much of the code in sim_load can be moved here.  */

  sim_kind = kind;
  myname = argv[0];
  sim_callback = callback;
  return sd;
}

void
sim_close (SIM_DESC sd, int quitting)
{
  /* Nothing to do.  */
}

/* Called by gdb to load a program into memory.  */

SIM_RC
sim_load (SIM_DESC sd, char *prog, bfd *abfd, int from_tty)
{
  bfd *prog_bfd;

  /* FIXME: The code below that sets a specific variant of the H8/300
     being simulated should be moved to sim_open().  */

  /* See if the file is for the H8/300 or H8/300H.  */
  /* ??? This may not be the most efficient way.  The z8k simulator
     does this via a different mechanism (INIT_EXTRA_SYMTAB_INFO).  */
  if (abfd != NULL)
    prog_bfd = abfd;
  else
    prog_bfd = bfd_openr (prog, NULL);
  if (prog_bfd != NULL)
    {
      /* Set the cpu type.  We ignore failure from bfd_check_format
	 and bfd_openr as sim_load_file checks too.  */
      if (bfd_check_format (prog_bfd, bfd_object))
	{
	  set_h8300h (bfd_get_mach (prog_bfd));
	}
    }

  /* If we're using gdb attached to the simulator, then we have to
     reallocate memory for the simulator.

     When gdb first starts, it calls fetch_registers (among other
     functions), which in turn calls init_pointers, which allocates
     simulator memory.

     The problem is when we do that, we don't know whether we're
     debugging an H8/300 or H8/300H program.

     This is the first point at which we can make that determination,
     so we just reallocate memory now; this will also allow us to handle
     switching between H8/300 and H8/300H programs without exiting
     gdb.  */

  if (h8300smode && !h8300_normal_mode)
    memory_size = H8300S_MSIZE;
  else if (h8300hmode && !h8300_normal_mode)
    memory_size = H8300H_MSIZE;
  else
    memory_size = H8300_MSIZE;

  if (h8_get_memory_buf (sd))
    free (h8_get_memory_buf (sd));
  if (h8_get_cache_idx_buf (sd))
    free (h8_get_cache_idx_buf (sd));
  if (h8_get_eightbit_buf (sd))
    free (h8_get_eightbit_buf (sd));

  h8_set_memory_buf (sd, (unsigned char *) 
		     calloc (sizeof (char), memory_size));
  h8_set_cache_idx_buf (sd, (unsigned short *) 
			calloc (sizeof (short), memory_size));
  sd->memory_size = memory_size;
  h8_set_eightbit_buf (sd, (unsigned char *) calloc (sizeof (char), 256));

  /* `msize' must be a power of two.  */
  if ((memory_size & (memory_size - 1)) != 0)
    {
      (*sim_callback->printf_filtered) (sim_callback, 
					"sim_load: bad memory size.\n");
      return SIM_RC_FAIL;
    }
  h8_set_mask (sd, memory_size - 1);

  if (sim_load_file (sd, myname, sim_callback, prog, prog_bfd,
		     sim_kind == SIM_OPEN_DEBUG,
		     0, sim_write)
      == NULL)
    {
      /* Close the bfd if we opened it.  */
      if (abfd == NULL && prog_bfd != NULL)
	bfd_close (prog_bfd);
      return SIM_RC_FAIL;
    }

  /* Close the bfd if we opened it.  */
  if (abfd == NULL && prog_bfd != NULL)
    bfd_close (prog_bfd);
  return SIM_RC_OK;
}

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *abfd, char **argv, char **env)
{
  int i = 0;
  int len_arg = 0;
  int no_of_args = 0;

  if (abfd != NULL)
    h8_set_pc (sd, bfd_get_start_address (abfd));
  else
    h8_set_pc (sd, 0);

  /* Command Line support.  */
  if (argv != NULL)
    {
      /* Counting the no. of commandline arguments.  */
      for (no_of_args = 0; argv[no_of_args] != NULL; no_of_args++)
        continue;

      /* Allocating memory for the argv pointers.  */
      h8_set_command_line (sd, (char **) malloc ((sizeof (char *))
						 * (no_of_args + 1)));

      for (i = 0; i < no_of_args; i++)
	{
	  /* Copying the argument string.  */
	  h8_set_cmdline_arg (sd, i, (char *) strdup (argv[i]));
	}
      h8_set_cmdline_arg (sd, i, NULL);
    }
  
  return SIM_RC_OK;
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  (*sim_callback->printf_filtered) (sim_callback,
				    "This simulator does not accept any commands.\n");
}

void
sim_set_callbacks (struct host_callback_struct *ptr)
{
  sim_callback = ptr;
}
