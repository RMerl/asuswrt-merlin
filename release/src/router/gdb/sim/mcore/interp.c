/* Simulator for Motorola's MCore processor
   Copyright (C) 1999, 2000, 2002, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

#include <signal.h>
#include "sysdep.h"
#include <sys/times.h>
#include <sys/param.h>
#include <netinet/in.h>	/* for byte ordering macros */
#include "bfd.h"
#include "gdb/callback.h"
#include "libiberty.h"
#include "gdb/remote-sim.h"

#ifndef NUM_ELEM
#define NUM_ELEM(A) (sizeof (A) / sizeof (A)[0])
#endif


typedef long int           word;
typedef unsigned long int  uword;

static int            target_big_endian = 0;
static unsigned long  heap_ptr = 0;
host_callback *       callback;


unsigned long
mcore_extract_unsigned_integer (addr, len)
     unsigned char * addr;
     int len;
{
  unsigned long retval;
  unsigned char * p;
  unsigned char * startaddr = (unsigned char *)addr;
  unsigned char * endaddr = startaddr + len;
 
  if (len > (int) sizeof (unsigned long))
    printf ("That operation is not available on integers of more than %d bytes.",
	    sizeof (unsigned long));
 
  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;

  if (! target_big_endian)
    {
      for (p = endaddr; p > startaddr;)
	retval = (retval << 8) | * -- p;
    }
  else
    {
      for (p = startaddr; p < endaddr;)
	retval = (retval << 8) | * p ++;
    }
  
  return retval;
}

void
mcore_store_unsigned_integer (addr, len, val)
     unsigned char * addr;
     int len;
     unsigned long val;
{
  unsigned char * p;
  unsigned char * startaddr = (unsigned char *)addr;
  unsigned char * endaddr = startaddr + len;

  if (! target_big_endian)
    {
      for (p = startaddr; p < endaddr;)
	{
	  * p ++ = val & 0xff;
	  val >>= 8;
	}
    }
  else
    {
      for (p = endaddr; p > startaddr;)
	{
	  * -- p = val & 0xff;
	  val >>= 8;
	}
    }
}

/* The machine state.
   This state is maintained in host byte order.  The 
   fetch/store register functions must translate between host
   byte order and the target processor byte order.  
   Keeping this data in target byte order simplifies the register
   read/write functions.  Keeping this data in native order improves
   the performance of the simulator.  Simulation speed is deemed more
   important.  */

/* The ordering of the mcore_regset structure is matched in the
   gdb/config/mcore/tm-mcore.h file in the REGISTER_NAMES macro.  */
struct mcore_regset
{
  word	          gregs [16];		/* primary registers */
  word	          alt_gregs [16];	/* alt register file */
  word	          cregs [32];		/* control registers */
  word	          pc;			/* the pc */
  int		  ticks;
  int		  stalls;
  int		  cycles;
  int		  insts;
  int		  exception;
  unsigned long   msize;
  unsigned char * memory;
  word *          active_gregs;
};

union
{
  struct mcore_regset asregs;
  word asints [1];		/* but accessed larger... */
} cpu;

#define LAST_VALID_CREG	32		/* only 0..12 implemented */
#define	NUM_MCORE_REGS	(16 + 16 + LAST_VALID_CREG + 1)

int memcycles = 1;

static SIM_OPEN_KIND sim_kind;
static char * myname;

static int issue_messages = 0;

#define gr	asregs.active_gregs
#define cr	asregs.cregs
#define sr	asregs.cregs[0]
#define	vbr	asregs.cregs[1]
#define	esr	asregs.cregs[2]
#define	fsr	asregs.cregs[3]
#define	epc	asregs.cregs[4]
#define	fpc	asregs.cregs[5]
#define	ss0	asregs.cregs[6]
#define	ss1	asregs.cregs[7]
#define	ss2	asregs.cregs[8]
#define	ss3	asregs.cregs[9]
#define	ss4	asregs.cregs[10]
#define	gcr	asregs.cregs[11]
#define	gsr	asregs.cregs[12]
#define mem	asregs.memory

/* maniuplate the carry bit */
#define	C_ON()	 (cpu.sr & 1)
#define	C_VALUE() (cpu.sr & 1)
#define	C_OFF()	 ((cpu.sr & 1) == 0)
#define	SET_C()	 {cpu.sr |= 1;}
#define	CLR_C()	 {cpu.sr &= 0xfffffffe;}
#define	NEW_C(v) {CLR_C(); cpu.sr |= ((v) & 1);}

#define	SR_AF() ((cpu.sr >> 1) & 1)

#define	TRAPCODE	1	/* r1 holds which function we want */
#define	PARM1	2		/* first parameter  */
#define	PARM2	3
#define	PARM3	4
#define	PARM4	5
#define	RET1	2		/* register for return values. */

long
int_sbrk (inc_bytes)
     int inc_bytes;
{
  long addr;
  
  addr = heap_ptr;
  
  heap_ptr += inc_bytes;
  
  if (issue_messages && heap_ptr>cpu.gr[0])
    fprintf (stderr, "Warning: heap_ptr overlaps stack!\n");
  
  return addr;
}

static void INLINE 
wbat (x, v)
     word x, v;
{
  if (((uword)x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "byte write to 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
    }
  else
    {
      unsigned char *p = cpu.mem + x;
      p[0] = v;
    }
}

static void INLINE 
wlat (x, v)
     word x, v;
{
  if (((uword)x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "word write to 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
    }
  else
    {
      if ((x & 3) != 0)
	{
	  if (issue_messages)
	    fprintf (stderr, "word write to unaligned memory address: 0x%x\n", x);
      
	  cpu.asregs.exception = SIGBUS;
	}
      else if (! target_big_endian)
	{
	  unsigned char * p = cpu.mem + x;
	  p[3] = v >> 24;
	  p[2] = v >> 16;
	  p[1] = v >> 8;
	  p[0] = v;
	}
      else
	{
	  unsigned char * p = cpu.mem + x;
	  p[0] = v >> 24;
	  p[1] = v >> 16;
	  p[2] = v >> 8;
	  p[3] = v;
	}
    }
}

static void INLINE 
what (x, v)
     word x, v;
{
  if (((uword)x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "short write to 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
    }
  else
    {
      if ((x & 1) != 0)
	{
	  if (issue_messages)
	    fprintf (stderr, "short write to unaligned memory address: 0x%x\n",
		     x);
      
	  cpu.asregs.exception = SIGBUS;
	}
      else if (! target_big_endian)
	{
	  unsigned char * p = cpu.mem + x;
	  p[1] = v >> 8;
	  p[0] = v;
	}
      else
	{
	  unsigned char * p = cpu.mem + x;
	  p[0] = v >> 8;
	  p[1] = v;
	}
    }
}

/* Read functions.  */
static int INLINE 
rbat (x)
     word x;
{
  if (((uword)x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "byte read from 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
      return 0;
    }
  else
    {
      unsigned char * p = cpu.mem + x;
      return p[0];
    }
}

static int INLINE 
rlat (x)
     word x;
{
  if (((uword) x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "word read from 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
      return 0;
    }
  else
    {
      if ((x & 3) != 0)
	{
	  if (issue_messages)
	    fprintf (stderr, "word read from unaligned address: 0x%x\n", x);
      
	  cpu.asregs.exception = SIGBUS;
	  return 0;
	}
      else if (! target_big_endian)
	{
	  unsigned char * p = cpu.mem + x;
	  return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
	}
      else
	{
	  unsigned char * p = cpu.mem + x;
	  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	}
    }
}

static int INLINE 
rhat (x)
     word x;
{
  if (((uword)x) >= cpu.asregs.msize)
    {
      if (issue_messages)
	fprintf (stderr, "short read from 0x%x outside memory range\n", x);
      
      cpu.asregs.exception = SIGSEGV;
      return 0;
    }
  else
    {
      if ((x & 1) != 0)
	{
	  if (issue_messages)
	    fprintf (stderr, "short read from unaligned address: 0x%x\n", x);
      
	  cpu.asregs.exception = SIGBUS;
	  return 0;
	}
      else if (! target_big_endian)
	{
	  unsigned char * p = cpu.mem + x;
	  return (p[1] << 8) | p[0];
	}
      else
	{
	  unsigned char * p = cpu.mem + x;
	  return (p[0] << 8) | p[1];
	}
    }
}


#define SEXTB(x)     	(((x & 0xff) ^ (~ 0x7f)) + 0x80)
#define SEXTW(y)    	((int)((short)y))

static int
IOMEM (addr, write, value)
     int addr;
     int write;
     int value;
{
}

/* Default to a 8 Mbyte (== 2^23) memory space.  */
static int sim_memory_size = 23;

#define	MEM_SIZE_FLOOR	64
void
sim_size (power)
     int power;
{
  sim_memory_size = power;
  cpu.asregs.msize = 1 << sim_memory_size;

  if (cpu.mem)
    free (cpu.mem);

  /* Watch out for the '0 count' problem. There's probably a better
     way.. e.g., why do we use 64 here?  */
  if (cpu.asregs.msize < 64)	/* Ensure a boundary.  */
    cpu.mem = (unsigned char *) calloc (64, (64 + cpu.asregs.msize) / 64);
  else
    cpu.mem = (unsigned char *) calloc (64, cpu.asregs.msize / 64);

  if (!cpu.mem)
    {
      if (issue_messages)
	fprintf (stderr,
		 "Not enough VM for simulation of %d bytes of RAM\n",
		 cpu.asregs.msize);

      cpu.asregs.msize = 1;
      cpu.mem = (unsigned char *) calloc (1, 1);
    }
}

static void
init_pointers ()
{
  if (cpu.asregs.msize != (1 << sim_memory_size))
    sim_size (sim_memory_size);
}

static void
set_initial_gprs ()
{
  int i;
  long space;
  unsigned long memsize;
  
  init_pointers ();

  /* Set up machine just out of reset.  */
  cpu.asregs.pc = 0;
  cpu.sr = 0;
  
  memsize = cpu.asregs.msize / (1024 * 1024);

  if (issue_messages > 1)
    fprintf (stderr, "Simulated memory of %d Mbytes (0x0 .. 0x%08x)\n",
	     memsize, cpu.asregs.msize - 1);

  /* Clean out the GPRs and alternate GPRs.  */
  for (i = 0; i < 16; i++)
    {
      cpu.asregs.gregs[i] = 0;
      cpu.asregs.alt_gregs[i] = 0;
    }
  
  /* Make our register set point to the right place.  */
  if (SR_AF())
    cpu.asregs.active_gregs = &cpu.asregs.alt_gregs[0];
  else
    cpu.asregs.active_gregs = &cpu.asregs.gregs[0];
  
  /* ABI specifies initial values for these registers.  */
  cpu.gr[0] = cpu.asregs.msize - 4;
 
  /* dac fix, the stack address must be 8-byte aligned! */
  cpu.gr[0] = cpu.gr[0] - cpu.gr[0] % 8;
  cpu.gr[PARM1] = 0;
  cpu.gr[PARM2] = 0;
  cpu.gr[PARM3] = 0;
  cpu.gr[PARM4] = cpu.gr[0];
}

static void
interrupt ()
{
  cpu.asregs.exception = SIGINT;
}

/* Functions so that trapped open/close don't interfere with the
   parent's functions.  We say that we can't close the descriptors
   that we didn't open.  exit() and cleanup() get in trouble here,
   to some extent.  That's the price of emulation.  */

unsigned char opened[100];

static void
log_open (fd)
    int fd;
{
  if (fd < 0 || fd > NUM_ELEM (opened))
    return;
  
  opened[fd] = 1;
}

static void
log_close (fd)
     int fd;
{
  if (fd < 0 || fd > NUM_ELEM (opened))
    return;
  
  opened[fd] = 0;
}

static int
is_opened (fd)
    int fd;
{
  if (fd < 0 || fd > NUM_ELEM (opened))
    return 0;

  return opened[fd];
}

static void
handle_trap1 ()
{
  unsigned long a[3];

  switch ((unsigned long) (cpu.gr [TRAPCODE]))
    {
    case 3:
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.mem + cpu.gr[PARM2]);
      a[2] = (unsigned long) (cpu.gr[PARM3]);
      cpu.gr[RET1] = callback->read (callback, a[0], (char *) a[1], a[2]);
      break;
      
    case 4:
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.mem + cpu.gr[PARM2]);
      a[2] = (unsigned long) (cpu.gr[PARM3]);
      cpu.gr[RET1] = (int)callback->write (callback, a[0], (char *) a[1], a[2]);
      break;
      
    case 5:
      a[0] = (unsigned long) (cpu.mem + cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.gr[PARM2]);
      /* a[2] = (unsigned long) (cpu.gr[PARM3]); */
      cpu.gr[RET1] = callback->open (callback, (char *) a[0], a[1]);
      log_open (cpu.gr[RET1]);
      break;
      
    case 6:
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      /* Watch out for debugger's files. */
      if (is_opened (a[0]))
	{
	  log_close (a[0]);
	  cpu.gr[RET1] = callback->close (callback, a[0]);
	}
      else
	{
	  /* Don't let him close it.  */
	  cpu.gr[RET1] = (-1);
	}
      break;
      
    case 9:
      a[0] = (unsigned long) (cpu.mem + cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.mem + cpu.gr[PARM2]);
      cpu.gr[RET1] = link ((char *) a[0], (char *) a[1]);
      break;
      
    case 10:
      a[0] = (unsigned long) (cpu.mem + cpu.gr[PARM1]);
      cpu.gr[RET1] = callback->unlink (callback, (char *) a[0]);
      break;
      
    case 13:
      /* handle time(0) vs time(&var) */
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      if (a[0])
	a[0] += (unsigned long) cpu.mem;
      cpu.gr[RET1] = callback->time (callback, (time_t *) a[0]);
      break;
      
    case 19:
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.gr[PARM2]);
      a[2] = (unsigned long) (cpu.gr[PARM3]);
      cpu.gr[RET1] = callback->lseek (callback, a[0], a[1], a[2]);
      break;
      
    case 33:
      a[0] = (unsigned long) (cpu.mem + cpu.gr[PARM1]);
      a[1] = (unsigned long) (cpu.gr[PARM2]);
      cpu.gr[RET1] = access ((char *) a[0], a[1]);
      break;
      
    case 43:
      a[0] = (unsigned long) (cpu.mem + cpu.gr[PARM1]);
#if 0
      cpu.gr[RET1] = times ((char *)a[0]);
#else
      {
	/* Give him simulated cycles for utime
	   and an instruction count for stime. */
	struct tms
	{
	  time_t tms_utime;
	  time_t tms_stime;
	  time_t tms_cutime;
	  time_t tms_cstime;
	} t;

	t.tms_utime = cpu.asregs.cycles;
	t.tms_stime = cpu.asregs.insts;
	t.tms_cutime = t.tms_utime;
	t.tms_cstime = t.tms_stime;
			    
	memcpy ((struct tms *)(a[0]), &t, sizeof (t));
			    
	cpu.gr[RET1] = cpu.asregs.cycles;
      }
#endif
      break;
      
    case 69:
      a[0] = (unsigned long) (cpu.gr[PARM1]);
      cpu.gr[RET1] = int_sbrk (a[0]);
      break;
      
    default:
      if (issue_messages)
	fprintf (stderr, "WARNING: sys call %d unimplemented\n",
		 cpu.gr[TRAPCODE]);
      break;
    }
}

static void
process_stub (what)
     int what;
{
  /* These values should match those in libgloss/mcore/syscalls.s.  */
  switch (what)
    {
    case 3:  /* _read */
    case 4:  /* _write */
    case 5:  /* _open */
    case 6:  /* _close */
    case 10: /* _unlink */
    case 19: /* _lseek */
    case 43: /* _times */
      cpu.gr [TRAPCODE] = what;
      handle_trap1 ();
      break;
      
    default:
      if (issue_messages)
	fprintf (stderr, "Unhandled stub opcode: %d\n", what);
      break;
    }
}

static void
util (what)
     unsigned what;
{
  switch (what)
    {
    case 0:	/* exit */
      cpu.asregs.exception = SIGQUIT;
      break;

    case 1:	/* printf */
      {
	unsigned long a[6];
	unsigned char *s;
	int i;

	a[0] = (unsigned long)(cpu.mem + cpu.gr[PARM1]);

	for (s = (unsigned char *)a[0], i = 1 ; *s && i < 6 ; s++)
	  {
	    if (*s == '%')
	      {
		if (*++s == 's')
		  a[i] = (unsigned long)(cpu.mem + cpu.gr[PARM1+i]);
		else
		  a[i] = cpu.gr[i+PARM1];
		i++;
	      }
	  }
	
	cpu.gr[RET1] = printf ((char *)a[0], a[1], a[2], a[3], a[4], a[5]);
      }
      break;
      
    case 2:	/* scanf */
      if (issue_messages)
	fprintf (stderr, "WARNING: scanf unimplemented\n");
      break;
      
    case 3:	/* utime */
      cpu.gr[RET1] = cpu.asregs.insts;
      break;

    case 0xFF:
      process_stub (cpu.gr[1]);
      break;
      
    default:
      if (issue_messages)
	fprintf (stderr, "Unhandled util code: %x\n", what);
      break;
    }
}	

/* For figuring out whether we carried; addc/subc use this. */
static int
iu_carry (a, b, cin)
     unsigned long a;
     unsigned long b;
     int cin;
{
  unsigned long	x;
  
  x = (a & 0xffff) + (b & 0xffff) + cin;
  x = (x >> 16) + (a >> 16) + (b >> 16);
  x >>= 16;

  return (x != 0);
}

#define WATCHFUNCTIONS 1
#ifdef WATCHFUNCTIONS

#define MAXWL 80
word WL[MAXWL];
char * WLstr[MAXWL];

int ENDWL=0;
int WLincyc;
int WLcyc[MAXWL];
int WLcnts[MAXWL];
int WLmax[MAXWL];
int WLmin[MAXWL];
word WLendpc;
int WLbcyc;
int WLW;
#endif

#define RD	(inst        & 0xF)
#define RS	((inst >> 4) & 0xF)
#define RX	((inst >> 8) & 0xF)
#define IMM5	((inst >> 4) & 0x1F)
#define IMM4	((inst) & 0xF)

static int tracing = 0;

void
sim_resume (sd, step, siggnal)
     SIM_DESC sd;
     int step, siggnal;
{
  int needfetch;
  word ibuf;
  word pc;
  unsigned short inst;
  void (* sigsave)();
  int memops;
  int bonus_cycles;
  int insts;
  int w;
  int cycs;
  word WLhash;

  sigsave = signal (SIGINT, interrupt);
  cpu.asregs.exception = step ? SIGTRAP: 0;
  pc = cpu.asregs.pc;

  /* Fetch the initial instructions that we'll decode. */
  ibuf = rlat (pc & 0xFFFFFFFC);
  needfetch = 0;

  memops = 0;
  bonus_cycles = 0;
  insts = 0;
  
  /* make our register set point to the right place */
  if (SR_AF ())
    cpu.asregs.active_gregs = & cpu.asregs.alt_gregs[0];
  else
    cpu.asregs.active_gregs = & cpu.asregs.gregs[0];
  
  /* make a hash to speed exec loop, hope it's nonzero */
  WLhash = 0xFFFFFFFF;

  for (w = 1; w <= ENDWL; w++)
    WLhash = WLhash & WL[w];

  do
    {
      word oldpc;
      
      insts ++;
      
      if (pc & 02)
	{
	  if (! target_big_endian)
	    inst = ibuf >> 16;
	  else
	    inst = ibuf & 0xFFFF;
	  needfetch = 1;
	}
      else
	{
	  if (! target_big_endian)
	    inst = ibuf & 0xFFFF;
	  else
	    inst = ibuf >> 16;
	}

#ifdef WATCHFUNCTIONS
      /* now scan list of watch addresses, if match, count it and
	 note return address and count cycles until pc=return address */
      
      if ((WLincyc == 1) && (pc == WLendpc))
	{
	  cycs = (cpu.asregs.cycles + (insts + bonus_cycles +
				       (memops * memcycles)) - WLbcyc);
	  
	  if (WLcnts[WLW] == 1)
	    {
	      WLmax[WLW] = cycs;
	      WLmin[WLW] = cycs;
	      WLcyc[WLW] = 0;
	    }
	  
	  if (cycs > WLmax[WLW])
	    {
	      WLmax[WLW] = cycs;
	    }
	  
	  if (cycs < WLmin[WLW])
	    {
	      WLmin[WLW] = cycs;
	    }
	  
	  WLcyc[WLW] += cycs;
	  WLincyc = 0;
	  WLendpc = 0;
	} 

      /* Optimize with a hash to speed loop.  */
      if (WLincyc == 0)
	{
          if ((WLhash == 0) || ((WLhash & pc) != 0))
	    {
	      for (w=1; w <= ENDWL; w++)
		{
		  if (pc == WL[w])
		    {
		      WLcnts[w]++;
		      WLbcyc = cpu.asregs.cycles + insts 
			+ bonus_cycles + (memops * memcycles);
		      WLendpc = cpu.gr[15];
		      WLincyc = 1;
		      WLW = w;
		      break;
		    }
		}
	    }
	}
#endif

      if (tracing)
	fprintf (stderr, "%.4x: inst = %.4x ", pc, inst);

      oldpc = pc;
      
      pc += 2;
      
      switch (inst >> 8)
	{
	case 0x00:
	  switch RS
	    {
	    case 0x0:
	      switch RD
		{
		case 0x0:				/* bkpt */
		  cpu.asregs.exception = SIGTRAP;
		  pc -= 2;
		  break;
		  
		case 0x1:				/* sync */
		  break;
		  
		case 0x2:				/* rte */
		  pc = cpu.epc;
		  cpu.sr = cpu.esr;
		  needfetch = 1;
		  
		  if (SR_AF ())
		    cpu.asregs.active_gregs = & cpu.asregs.alt_gregs[0];
		  else
		    cpu.asregs.active_gregs = & cpu.asregs.gregs[0];
		  break;

		case 0x3:				/* rfi */
		  pc = cpu.fpc;
		  cpu.sr = cpu.fsr;
		  needfetch = 1;

		  if (SR_AF ())
		    cpu.asregs.active_gregs = &cpu.asregs.alt_gregs[0];
		  else
		    cpu.asregs.active_gregs = &cpu.asregs.gregs[0];
		  break;
		  
		case 0x4:				/* stop */
		  if (issue_messages)
		    fprintf (stderr, "WARNING: stop unimplemented\n");
		  break;
		    
		case 0x5:				/* wait */
		  if (issue_messages)
		    fprintf (stderr, "WARNING: wait unimplemented\n");
		  break;
		    
		case 0x6:				/* doze */
		  if (issue_messages)
		    fprintf (stderr, "WARNING: doze unimplemented\n");
		  break;
		    
		case 0x7:
		  cpu.asregs.exception = SIGILL;	/* illegal */
		  break;
		    
		case 0x8:				/* trap 0 */
		case 0xA:				/* trap 2 */
		case 0xB:				/* trap 3 */
		  cpu.asregs.exception = SIGTRAP;
		  break;
		    
		case 0xC:				/* trap 4 */
		case 0xD:				/* trap 5 */
		case 0xE:				/* trap 6 */
		  cpu.asregs.exception = SIGILL;	/* illegal */
		  break;
		  
		case 0xF: 				/* trap 7 */
		  cpu.asregs.exception = SIGTRAP;	/* integer div-by-0 */
		  break;
		    
		case 0x9:				/* trap 1 */
		  handle_trap1 ();
		  break;
		}
	      break;
	      
	    case 0x1:
	      cpu.asregs.exception = SIGILL;		/* illegal */
	      break;
	      
	    case 0x2:					/* mvc */
	      cpu.gr[RD] = C_VALUE();
	      break;
	    case 0x3:					/* mvcv */
	      cpu.gr[RD] = C_OFF();
	      break;
	    case 0x4:					/* ldq */
	      {
		char *addr = (char *)cpu.gr[RD];
		int regno = 4;			/* always r4-r7 */
		
		bonus_cycles++;
		memops += 4;
		do
		  {
		    cpu.gr[regno] = rlat(addr);
		    addr += 4;
		    regno++;
		  }
		while ((regno&0x3) != 0);
	      }
	      break;
	    case 0x5:					/* stq */
	      {
		char *addr = (char *)cpu.gr[RD];
		int regno = 4;			/* always r4-r7 */
		
		memops += 4;
		bonus_cycles++;
		do
		  {
		    wlat(addr, cpu.gr[regno]);
		    addr += 4;
		    regno++;
		  }
		while ((regno & 0x3) != 0);
	      }
	      break;
	    case 0x6:					/* ldm */
	      {
		char *addr = (char *)cpu.gr[0];
		int regno = RD;
		
		/* bonus cycle is really only needed if
		   the next insn shifts the last reg loaded.
		   
		   bonus_cycles++;
		*/
		memops += 16-regno;
		while (regno <= 0xF)
		  {
		    cpu.gr[regno] = rlat(addr);
		    addr += 4;
		    regno++;
		  }
	      }
	      break;
	    case 0x7:					/* stm */
	      {
		char *addr = (char *)cpu.gr[0];
		int regno = RD;
		
		/* this should be removed! */
		/*  bonus_cycles ++; */

		memops += 16 - regno;
		while (regno <= 0xF)
		  {
		    wlat(addr, cpu.gr[regno]);
		    addr += 4;
		    regno++;
		  }
	      }
	      break;

	    case 0x8:					/* dect */
	      cpu.gr[RD] -= C_VALUE();
	      break;
	    case 0x9:					/* decf */
	      cpu.gr[RD] -= C_OFF();
	      break;
	    case 0xA:					/* inct */
	      cpu.gr[RD] += C_VALUE();
	      break;
	    case 0xB:					/* incf */
	      cpu.gr[RD] += C_OFF();
	      break;
	    case 0xC:					/* jmp */
	      pc = cpu.gr[RD];
	      if (tracing && RD == 15)
		fprintf (stderr, "Func return, r2 = %x, r3 = %x\n",
			 cpu.gr[2], cpu.gr[3]);
	      bonus_cycles++;
	      needfetch = 1;
	      break;
	    case 0xD:					/* jsr */
	      cpu.gr[15] = pc;
	      pc = cpu.gr[RD];
	      bonus_cycles++;
	      needfetch = 1;
	      break;
	    case 0xE:					/* ff1 */
	      {
		word tmp, i;
		tmp = cpu.gr[RD];
		for (i = 0; !(tmp & 0x80000000) && i < 32; i++)
		  tmp <<= 1;
		cpu.gr[RD] = i;
	      }
	      break;
	    case 0xF:					/* brev */
	      {
		word tmp;
		tmp = cpu.gr[RD];
		tmp = ((tmp & 0xaaaaaaaa) >>  1) | ((tmp & 0x55555555) <<  1);
		tmp = ((tmp & 0xcccccccc) >>  2) | ((tmp & 0x33333333) <<  2);
		tmp = ((tmp & 0xf0f0f0f0) >>  4) | ((tmp & 0x0f0f0f0f) <<  4);
		tmp = ((tmp & 0xff00ff00) >>  8) | ((tmp & 0x00ff00ff) <<  8);
		cpu.gr[RD] = ((tmp & 0xffff0000) >> 16) | ((tmp & 0x0000ffff) << 16);
	      }
	      break;
	    }
	  break;
	case 0x01:
	  switch RS
	    {
	    case 0x0:					/* xtrb3 */	
	      cpu.gr[1] = (cpu.gr[RD]) & 0xFF;
	      NEW_C (cpu.gr[RD] != 0);
	      break;
	    case 0x1:					/* xtrb2 */
	      cpu.gr[1] = (cpu.gr[RD]>>8) & 0xFF;
	      NEW_C (cpu.gr[RD] != 0);
	      break;
	    case 0x2:					/* xtrb1 */
	      cpu.gr[1] = (cpu.gr[RD]>>16) & 0xFF;
	      NEW_C (cpu.gr[RD] != 0);
	      break;
	    case 0x3:					/* xtrb0 */
	      cpu.gr[1] = (cpu.gr[RD]>>24) & 0xFF;
	      NEW_C (cpu.gr[RD] != 0);
	      break;
	    case 0x4:					/* zextb */
	      cpu.gr[RD] &= 0x000000FF;
	      break;
	    case 0x5:					/* sextb */
	      {
		long tmp;
		tmp = cpu.gr[RD];
		tmp <<= 24;
		tmp >>= 24;
		cpu.gr[RD] = tmp;
	      }
	      break;
	    case 0x6:					/* zexth */
	      cpu.gr[RD] &= 0x0000FFFF;
	      break;
	    case 0x7:					/* sexth */
	      {
		long tmp;
		tmp = cpu.gr[RD];
		tmp <<= 16;
		tmp >>= 16;
		cpu.gr[RD] = tmp;
	      }
	      break;
	    case 0x8:					/* declt */ 
	      --cpu.gr[RD];
	      NEW_C ((long)cpu.gr[RD] < 0);
	      break;
	    case 0x9:					/* tstnbz */
	      {
		word tmp = cpu.gr[RD];
		NEW_C ((tmp & 0xFF000000) != 0 &&
		       (tmp & 0x00FF0000) != 0 && (tmp & 0x0000FF00) != 0 &&
		       (tmp & 0x000000FF) != 0);
	      }
	      break; 
	    case 0xA:					/* decgt */
	      --cpu.gr[RD];
	      NEW_C ((long)cpu.gr[RD] > 0);
	      break;
	    case 0xB:					/* decne */
	      --cpu.gr[RD];
	      NEW_C ((long)cpu.gr[RD] != 0);
	      break;
	    case 0xC:					/* clrt */
	      if (C_ON())
		cpu.gr[RD] = 0;
	      break;
	    case 0xD:					/* clrf */
	      if (C_OFF())
		cpu.gr[RD] = 0;
	      break;
	    case 0xE:					/* abs */
	      if (cpu.gr[RD] & 0x80000000)
		cpu.gr[RD] = ~cpu.gr[RD] + 1;
	      break;
	    case 0xF:					/* not */
	      cpu.gr[RD] = ~cpu.gr[RD];
	      break;
	    }
	  break;
	case 0x02:					/* movt */
	  if (C_ON())
	    cpu.gr[RD] = cpu.gr[RS];
	  break;
	case 0x03:					/* mult */
	  /* consume 2 bits per cycle from rs, until rs is 0 */
	  {
	    unsigned int t = cpu.gr[RS];
	    int ticks;
	    for (ticks = 0; t != 0 ; t >>= 2) 
	      ticks++;
	    bonus_cycles += ticks;
	  }
	  bonus_cycles += 2;  /* min. is 3, so add 2, plus ticks above */
	  if (tracing)
	    fprintf (stderr, "  mult %x by %x to give %x",
		     cpu.gr[RD], cpu.gr[RS], cpu.gr[RD] * cpu.gr[RS]);
	  cpu.gr[RD] = cpu.gr[RD] * cpu.gr[RS];
	  break;
	case 0x04:					/* loopt */
	  if (C_ON())
	    {
	      pc += (IMM4 << 1) - 32;
	      bonus_cycles ++;
	      needfetch = 1;
	    }
	  --cpu.gr[RS];				/* not RD! */
	  NEW_C (((long)cpu.gr[RS]) > 0);
	  break; 
	case 0x05:					/* subu */
	  cpu.gr[RD] -= cpu.gr[RS];
	  break;
	case 0x06:					/* addc */
	  {
	    unsigned long tmp, a, b;
	    a = cpu.gr[RD];
	    b = cpu.gr[RS];
	    cpu.gr[RD] = a + b + C_VALUE ();
	    tmp = iu_carry (a, b, C_VALUE ());
	    NEW_C (tmp);
	  }
	  break;
	case 0x07:					/* subc */
	  {
	    unsigned long tmp, a, b;
	    a = cpu.gr[RD];
	    b = cpu.gr[RS];
	    cpu.gr[RD] = a - b + C_VALUE () - 1;
	    tmp = iu_carry (a,~b, C_VALUE ());
	    NEW_C (tmp);
	  }
	  break;
	case 0x08:					/* illegal */
	case 0x09:					/* illegal*/
	  cpu.asregs.exception = SIGILL;
	  break;
	case 0x0A:					/* movf */
	  if (C_OFF())
	    cpu.gr[RD] = cpu.gr[RS];
	  break;
	case 0x0B:					/* lsr */
	  { 
	    unsigned long dst, src;
	    dst = cpu.gr[RD];
	    src = cpu.gr[RS];
	    /* We must not rely solely upon the native shift operations, since they
	       may not match the M*Core's behaviour on boundary conditions.  */
	    dst = src > 31 ? 0 : dst >> src;
	    cpu.gr[RD] = dst;
	  }
	  break;
	case 0x0C:					/* cmphs */
	  NEW_C ((unsigned long )cpu.gr[RD] >= 
		 (unsigned long)cpu.gr[RS]);
	  break;
	case 0x0D:					/* cmplt */
	  NEW_C ((long)cpu.gr[RD] < (long)cpu.gr[RS]);
	  break;
	case 0x0E:					/* tst */
	  NEW_C ((cpu.gr[RD] & cpu.gr[RS]) != 0);
	  break;
	case 0x0F:					/* cmpne */
	  NEW_C (cpu.gr[RD] != cpu.gr[RS]);
	  break;
	case 0x10: case 0x11:				/* mfcr */
	  {
	    unsigned r;
	    r = IMM5;
	    if (r <= LAST_VALID_CREG)
	      cpu.gr[RD] = cpu.cr[r];
	    else
	      cpu.asregs.exception = SIGILL;
	  }
	  break;

	case 0x12:					/* mov */
	  cpu.gr[RD] = cpu.gr[RS];
	  if (tracing)
	    fprintf (stderr, "MOV %x into reg %d", cpu.gr[RD], RD);
	  break;

	case 0x13:					/* bgenr */
	  if (cpu.gr[RS] & 0x20)
	    cpu.gr[RD] = 0;
	  else
	    cpu.gr[RD] = 1 << (cpu.gr[RS] & 0x1F);
	  break;

	case 0x14:					/* rsub */
	  cpu.gr[RD] = cpu.gr[RS] - cpu.gr[RD];
	  break;

	case 0x15:					/* ixw */
	  cpu.gr[RD] += cpu.gr[RS]<<2;
	  break;

	case 0x16:					/* and */
	  cpu.gr[RD] &= cpu.gr[RS];
	  break;

	case 0x17:					/* xor */
	  cpu.gr[RD] ^= cpu.gr[RS];
	  break;

	case 0x18: case 0x19:				/* mtcr */
	  {
	    unsigned r;
	    r = IMM5;
	    if (r <= LAST_VALID_CREG)
	      cpu.cr[r] = cpu.gr[RD];
	    else
	      cpu.asregs.exception = SIGILL;
	    
	    /* we might have changed register sets... */
	    if (SR_AF ())
	      cpu.asregs.active_gregs = & cpu.asregs.alt_gregs[0];
	    else
	      cpu.asregs.active_gregs = & cpu.asregs.gregs[0];
	  }
	  break;

	case 0x1A:					/* asr */
	  /* We must not rely solely upon the native shift operations, since they
	     may not match the M*Core's behaviour on boundary conditions.  */
	  if (cpu.gr[RS] > 30)
	    cpu.gr[RD] = ((long) cpu.gr[RD]) < 0 ? -1 : 0;
	  else
	    cpu.gr[RD] = (long) cpu.gr[RD] >> cpu.gr[RS];
	  break;

	case 0x1B:					/* lsl */
	  /* We must not rely solely upon the native shift operations, since they
	     may not match the M*Core's behaviour on boundary conditions.  */
	  cpu.gr[RD] = cpu.gr[RS] > 31 ? 0 : cpu.gr[RD] << cpu.gr[RS];
	  break;

	case 0x1C:					/* addu */
	  cpu.gr[RD] += cpu.gr[RS];
	  break;

	case 0x1D:					/* ixh */
	  cpu.gr[RD] += cpu.gr[RS] << 1;
	  break;

	case 0x1E:					/* or */
	  cpu.gr[RD] |= cpu.gr[RS];
	  break;

	case 0x1F:					/* andn */
	  cpu.gr[RD] &= ~cpu.gr[RS];
	  break;
	case 0x20: case 0x21:				/* addi */
	  cpu.gr[RD] =
	    cpu.gr[RD] + (IMM5 + 1);
	  break;
	case 0x22: case 0x23:				/* cmplti */
	  {
	    int tmp = (IMM5 + 1);
	    if (cpu.gr[RD] < tmp)
	      {
	        SET_C();
	      }
	    else
	      {
	        CLR_C();
	      }
	  }
	  break;
	case 0x24: case 0x25:				/* subi */
	  cpu.gr[RD] =
	    cpu.gr[RD] - (IMM5 + 1);
	  break;
	case 0x26: case 0x27:				/* illegal */
	  cpu.asregs.exception = SIGILL;
	  break;
	case 0x28: case 0x29:				/* rsubi */
	  cpu.gr[RD] =
	    IMM5 - cpu.gr[RD];
	  break;
	case 0x2A: case 0x2B:				/* cmpnei */
	  if (cpu.gr[RD] != IMM5)
	    {
	      SET_C();
	    }
	  else
	    {
	      CLR_C();
	    }
	  break;
	  
	case 0x2C: case 0x2D:				/* bmaski, divu */
	  {
	    unsigned imm = IMM5;
	    
	    if (imm == 1)
	      {
		int exe;
		int rxnlz, r1nlz;
		unsigned int rx, r1;

		rx = cpu.gr[RD];
		r1 = cpu.gr[1];
		exe = 0;

		/* unsigned divide */
		cpu.gr[RD] = (word) ((unsigned int) cpu.gr[RD] / (unsigned int)cpu.gr[1] );
		
		/* compute bonus_cycles for divu */
		for (r1nlz = 0; ((r1 & 0x80000000) == 0) && (r1nlz < 32); r1nlz ++)
		  r1 = r1 << 1;

		for (rxnlz = 0; ((rx & 0x80000000) == 0) && (rxnlz < 32); rxnlz ++)
		  rx = rx << 1;

		if (r1nlz < rxnlz)
		  exe += 4;
		else
		  exe += 5 + r1nlz - rxnlz;

		if (exe >= (2 * memcycles - 1))
		  {
		    bonus_cycles += exe - (2 * memcycles) + 1;
		  }
	      }
	    else if (imm == 0 || imm >= 8)
	      {
		/* bmaski */
		if (imm == 0)
		  cpu.gr[RD] = -1;
		else
		  cpu.gr[RD] = (1 << imm) - 1;
	      }
	    else
	      {
		/* illegal */
		cpu.asregs.exception = SIGILL;
	      }
	  }
	  break;
	case 0x2E: case 0x2F:				/* andi */
	  cpu.gr[RD] = cpu.gr[RD] & IMM5;
	  break;
	case 0x30: case 0x31:				/* bclri */
	  cpu.gr[RD] = cpu.gr[RD] & ~(1<<IMM5);
	  break;
	case 0x32: case 0x33:				/* bgeni, divs */
	  {
	    unsigned imm = IMM5;
	    if (imm == 1)
	      {
		int exe,sc;
		int rxnlz, r1nlz;
		signed int rx, r1;
		
		/* compute bonus_cycles for divu */
		rx = cpu.gr[RD];
		r1 = cpu.gr[1];
		exe = 0;
		
		if (((rx < 0) && (r1 > 0)) || ((rx >= 0) && (r1 < 0)))
		  sc = 1;
		else
		  sc = 0;
		
		rx = abs (rx);
		r1 = abs (r1);
		
		/* signed divide, general registers are of type int, so / op is OK */
		cpu.gr[RD] = cpu.gr[RD] / cpu.gr[1];
	      
		for (r1nlz = 0; ((r1 & 0x80000000) == 0) && (r1nlz < 32) ; r1nlz ++ )
		  r1 = r1 << 1;
		
		for (rxnlz = 0; ((rx & 0x80000000) == 0) && (rxnlz < 32) ; rxnlz ++ )
		  rx = rx << 1;
		
		if (r1nlz < rxnlz)
		  exe += 5;
		else
		  exe += 6 + r1nlz - rxnlz + sc;
		
		if (exe >= (2 * memcycles - 1))
		  {
		    bonus_cycles += exe - (2 * memcycles) + 1;
		  }
	      }
	    else if (imm >= 7)
	      {
		/* bgeni */
		cpu.gr[RD] = (1 << IMM5);
	      }
	    else
	      {
		/* illegal */
		cpu.asregs.exception = SIGILL;
	      }
	    break;
	  }
	case 0x34: case 0x35:				/* bseti */
	  cpu.gr[RD] = cpu.gr[RD] | (1 << IMM5);
	  break;
	case 0x36: case 0x37:				/* btsti */
	  NEW_C (cpu.gr[RD] >> IMM5);
	  break;
	case 0x38: case 0x39:				/* xsr, rotli */
	  {
	    unsigned imm = IMM5;
	    unsigned long tmp = cpu.gr[RD];
	    if (imm == 0)
	      {
		word cbit;
		cbit = C_VALUE();
		NEW_C (tmp);
		cpu.gr[RD] = (cbit << 31) | (tmp >> 1);
	      }
	    else
	      cpu.gr[RD] = (tmp << imm) | (tmp >> (32 - imm));
	  }
	  break;
	case 0x3A: case 0x3B:				/* asrc, asri */
	  {
	    unsigned imm = IMM5;
	    long tmp = cpu.gr[RD];
	    if (imm == 0)
	      {
		NEW_C (tmp);
		cpu.gr[RD] = tmp >> 1;
	      }
	    else
	      cpu.gr[RD] = tmp >> imm;
	  }
	  break;
	case 0x3C: case 0x3D:				/* lslc, lsli */
	  {
	    unsigned imm = IMM5;
	    unsigned long tmp = cpu.gr[RD];
	    if (imm == 0)
	      {
		NEW_C (tmp >> 31);
		cpu.gr[RD] = tmp << 1;
	      }
	    else
	      cpu.gr[RD] = tmp << imm;
	  }
	  break;
	case 0x3E: case 0x3F:				/* lsrc, lsri */
	  {
	    unsigned imm = IMM5;
	    unsigned long tmp = cpu.gr[RD];
	    if (imm == 0)
	      {
		NEW_C (tmp);
		cpu.gr[RD] = tmp >> 1;
	      }
	    else
	      cpu.gr[RD] = tmp >> imm;
	  }
	  break;
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47:
	case 0x48: case 0x49: case 0x4A: case 0x4B:
	case 0x4C: case 0x4D: case 0x4E: case 0x4F:
	  cpu.asregs.exception = SIGILL;
	  break;
	case 0x50:
	  util (inst & 0xFF); 
	  break;
	case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5A: case 0x5B:
	case 0x5C: case 0x5D: case 0x5E: case 0x5F:
	  cpu.asregs.exception = SIGILL;
	  break;
	case 0x60: case 0x61: case 0x62: case 0x63:	/* movi  */
	case 0x64: case 0x65: case 0x66: case 0x67:
	  cpu.gr[RD] = (inst >> 4) & 0x7F;
	  break;
	case 0x68: case 0x69: case 0x6A: case 0x6B:
	case 0x6C: case 0x6D: case 0x6E: case 0x6F:	/* illegal */
	  cpu.asregs.exception = SIGILL;
	  break;
	case 0x71: case 0x72: case 0x73:
	case 0x74: case 0x75: case 0x76: case 0x77:
	case 0x78: case 0x79: case 0x7A: case 0x7B:
	case 0x7C: case 0x7D: case 0x7E:		/* lrw */
	  cpu.gr[RX] =  rlat ((pc + ((inst & 0xFF) << 2)) & 0xFFFFFFFC);
	  if (tracing)
	    fprintf (stderr, "LRW of 0x%x from 0x%x to reg %d",
		     rlat ((pc + ((inst & 0xFF) << 2)) & 0xFFFFFFFC),
		     (pc + ((inst & 0xFF) << 2)) & 0xFFFFFFFC, RX);
	  memops++;
	  break;
	case 0x7F:					/* jsri */
	  cpu.gr[15] = pc;
	  if (tracing)
	    fprintf (stderr, "func call: r2 = %x r3 = %x r4 = %x r5 = %x r6 = %x r7 = %x\n",
		     cpu.gr[2], cpu.gr[3], cpu.gr[4], cpu.gr[5], cpu.gr[6], cpu.gr[7]);
	case 0x70:					/* jmpi */
	  pc = rlat ((pc + ((inst & 0xFF) << 2)) & 0xFFFFFFFC);
	  memops++;
	  bonus_cycles++;
	  needfetch = 1;
	  break;

	case 0x80: case 0x81: case 0x82: case 0x83:
	case 0x84: case 0x85: case 0x86: case 0x87:
	case 0x88: case 0x89: case 0x8A: case 0x8B:
	case 0x8C: case 0x8D: case 0x8E: case 0x8F:	/* ld */
	  cpu.gr[RX] = rlat (cpu.gr[RD] + ((inst >> 2) & 0x003C));
	  if (tracing)
	    fprintf (stderr, "load reg %d from 0x%x with 0x%x",
		     RX,
		     cpu.gr[RD] + ((inst >> 2) & 0x003C), cpu.gr[RX]);
	  memops++;
	  break;
	case 0x90: case 0x91: case 0x92: case 0x93:
	case 0x94: case 0x95: case 0x96: case 0x97:
	case 0x98: case 0x99: case 0x9A: case 0x9B:
	case 0x9C: case 0x9D: case 0x9E: case 0x9F:	/* st */
	  wlat (cpu.gr[RD] + ((inst >> 2) & 0x003C), cpu.gr[RX]);
	  if (tracing)
	    fprintf (stderr, "store reg %d (containing 0x%x) to 0x%x",
		     RX, cpu.gr[RX],
		     cpu.gr[RD] + ((inst >> 2) & 0x003C));
	  memops++;
	  break;
	case 0xA0: case 0xA1: case 0xA2: case 0xA3:
	case 0xA4: case 0xA5: case 0xA6: case 0xA7:
	case 0xA8: case 0xA9: case 0xAA: case 0xAB:
	case 0xAC: case 0xAD: case 0xAE: case 0xAF:	/* ld.b */
	  cpu.gr[RX] = rbat (cpu.gr[RD] + RS);
	  memops++;
	  break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3:
	case 0xB4: case 0xB5: case 0xB6: case 0xB7:
	case 0xB8: case 0xB9: case 0xBA: case 0xBB:
	case 0xBC: case 0xBD: case 0xBE: case 0xBF:	/* st.b */
	  wbat (cpu.gr[RD] + RS, cpu.gr[RX]);
	  memops++;
	  break;
	case 0xC0: case 0xC1: case 0xC2: case 0xC3:
	case 0xC4: case 0xC5: case 0xC6: case 0xC7:
	case 0xC8: case 0xC9: case 0xCA: case 0xCB:
	case 0xCC: case 0xCD: case 0xCE: case 0xCF:	/* ld.h */
	  cpu.gr[RX] = rhat (cpu.gr[RD] + ((inst >> 3) & 0x001E));
	  memops++;
	  break;
	case 0xD0: case 0xD1: case 0xD2: case 0xD3:
	case 0xD4: case 0xD5: case 0xD6: case 0xD7:
	case 0xD8: case 0xD9: case 0xDA: case 0xDB:
	case 0xDC: case 0xDD: case 0xDE: case 0xDF:	/* st.h */
	  what (cpu.gr[RD] + ((inst >> 3) & 0x001E), cpu.gr[RX]);
	  memops++;
	  break;
	case 0xE8: case 0xE9: case 0xEA: case 0xEB:
	case 0xEC: case 0xED: case 0xEE: case 0xEF:	/* bf */	
	  if (C_OFF())
	    {
	      int disp;
	      disp = inst & 0x03FF;
	      if (inst & 0x0400)
		disp |= 0xFFFFFC00;
	      pc += disp<<1;
	      bonus_cycles++;
	      needfetch = 1;
	    }
	  break;
	case 0xE0: case 0xE1: case 0xE2: case 0xE3:
	case 0xE4: case 0xE5: case 0xE6: case 0xE7:	/* bt */
	  if (C_ON())
	    {
	      int disp;
	      disp = inst & 0x03FF;
	      if (inst & 0x0400)
		disp |= 0xFFFFFC00;
	      pc += disp<<1;
	      bonus_cycles++;
	      needfetch = 1;
	    }
	  break;

	case 0xF8: case 0xF9: case 0xFA: case 0xFB:
	case 0xFC: case 0xFD: case 0xFE: case 0xFF:	/* bsr */	
	  cpu.gr[15] = pc;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3:
	case 0xF4: case 0xF5: case 0xF6: case 0xF7:	/* br */
	  {
	    int disp;
	    disp = inst & 0x03FF;
	    if (inst & 0x0400)
	      disp |= 0xFFFFFC00;
	    pc += disp<<1;
	    bonus_cycles++;
	    needfetch = 1;
	  }
	  break;

	}

      if (tracing)
	fprintf (stderr, "\n");

      if (needfetch)   
	{
	  /* Do not let him fetch from a bad address! */
	  if (((uword)pc) >= cpu.asregs.msize)
	    {
	      if (issue_messages)
		fprintf (stderr, "PC loaded at 0x%x is outside of available memory! (0x%x)\n", oldpc, pc);
	      
	      cpu.asregs.exception = SIGSEGV;
	    }
	  else
	    {
	      ibuf = rlat (pc & 0xFFFFFFFC);
	      needfetch = 0;
	    }
	}
    }
  while (!cpu.asregs.exception);

  /* Hide away the things we've cached while executing.  */
  cpu.asregs.pc = pc;
  cpu.asregs.insts += insts;		/* instructions done ... */
  cpu.asregs.cycles += insts;		/* and each takes a cycle */
  cpu.asregs.cycles += bonus_cycles;	/* and extra cycles for branches */
  cpu.asregs.cycles += memops * memcycles;	/* and memop cycle delays */
  
  signal (SIGINT, sigsave);
}


int
sim_write (sd, addr, buffer, size)
     SIM_DESC sd;
     SIM_ADDR addr;
     unsigned char * buffer;
     int size;
{
  int i;
  init_pointers ();
  
  memcpy (& cpu.mem[addr], buffer, size);
  
  return size;
}

int
sim_read (sd, addr, buffer, size)
     SIM_DESC sd;
     SIM_ADDR addr;
     unsigned char * buffer;
     int size;
{
  int i;
  init_pointers ();
  
  memcpy (buffer, & cpu.mem[addr], size);
  
  return size;
}


int
sim_store_register (sd, rn, memory, length)
     SIM_DESC sd;
     int rn;
     unsigned char * memory;
     int length;
{
  init_pointers ();

  if (rn < NUM_MCORE_REGS && rn >= 0)
    {
      if (length == 4)
	{
	  long ival;
	  
	  /* misalignment safe */
	  ival = mcore_extract_unsigned_integer (memory, 4);
	  cpu.asints[rn] = ival;
	}

      return 4;
    }
  else
    return 0;
}

int
sim_fetch_register (sd, rn, memory, length)
     SIM_DESC sd;
     int rn;
     unsigned char * memory;
     int length;
{
  init_pointers ();
  
  if (rn < NUM_MCORE_REGS && rn >= 0)
    {
      if (length == 4)
	{
	  long ival = cpu.asints[rn];

	  /* misalignment-safe */
	  mcore_store_unsigned_integer (memory, 4, ival);
	}
      
      return 4;
    }
  else
    return 0;
}


int
sim_trace (sd)
     SIM_DESC sd;
{
  tracing = 1;
  
  sim_resume (sd, 0, 0);

  tracing = 0;
  
  return 1;
}

void
sim_stop_reason (sd, reason, sigrc)
     SIM_DESC sd;
     enum sim_stop * reason;
     int * sigrc;
{
  if (cpu.asregs.exception == SIGQUIT)
    {
      * reason = sim_exited;
      * sigrc = cpu.gr[PARM1];
    }
  else
    {
      * reason = sim_stopped;
      * sigrc = cpu.asregs.exception;
    }
}


int
sim_stop (sd)
     SIM_DESC sd;
{
  cpu.asregs.exception = SIGINT;
  return 1;
}


void
sim_info (sd, verbose)
     SIM_DESC sd;
     int verbose;
{
#ifdef WATCHFUNCTIONS
  int w, wcyc;
#endif
  double virttime = cpu.asregs.cycles / 36.0e6;

  callback->printf_filtered (callback, "\n\n# instructions executed  %10d\n",
			     cpu.asregs.insts);
  callback->printf_filtered (callback, "# cycles                 %10d\n",
			     cpu.asregs.cycles);
  callback->printf_filtered (callback, "# pipeline stalls        %10d\n",
			     cpu.asregs.stalls);
  callback->printf_filtered (callback, "# virtual time taken     %10.4f\n",
			     virttime);

#ifdef WATCHFUNCTIONS
  callback->printf_filtered (callback, "\nNumber of watched functions: %d\n",
			     ENDWL);

  wcyc = 0;
  
  for (w = 1; w <= ENDWL; w++)
    {
      callback->printf_filtered (callback, "WL = %s %8x\n",WLstr[w],WL[w]);
      callback->printf_filtered (callback, "  calls = %d, cycles = %d\n",
				 WLcnts[w],WLcyc[w]);
      
      if (WLcnts[w] != 0)
	callback->printf_filtered (callback,
				   "  maxcpc = %d, mincpc = %d, avecpc = %d\n",
				   WLmax[w],WLmin[w],WLcyc[w]/WLcnts[w]);
      wcyc += WLcyc[w];
    }
  
  callback->printf_filtered (callback,
			     "Total cycles for watched functions: %d\n",wcyc);
#endif
}

struct	aout
{
  unsigned char  sa_machtype[2];
  unsigned char  sa_magic[2];
  unsigned char  sa_tsize[4];
  unsigned char  sa_dsize[4];
  unsigned char  sa_bsize[4];
  unsigned char  sa_syms[4];
  unsigned char  sa_entry[4];
  unsigned char  sa_trelo[4];
  unsigned char  sa_drelo[4];
} aout;

#define	LONG(x)		(((x)[0]<<24)|((x)[1]<<16)|((x)[2]<<8)|(x)[3])
#define	SHORT(x)	(((x)[0]<<8)|(x)[1])

SIM_DESC
sim_open (kind, cb, abfd, argv)
     SIM_OPEN_KIND kind;
     host_callback * cb;
     struct bfd * abfd;
     char ** argv;
{
  int osize = sim_memory_size;
  myname = argv[0];
  callback = cb;
  
  if (kind == SIM_OPEN_STANDALONE)
    issue_messages = 1;
  
  /* Discard and reacquire memory -- start with a clean slate.  */
  sim_size (1);		/* small */
  sim_size (osize);	/* and back again */

  set_initial_gprs ();	/* Reset the GPR registers.  */
  
  /* Fudge our descriptor for now.  */
  return (SIM_DESC) 1;
}

void
sim_close (sd, quitting)
     SIM_DESC sd;
     int quitting;
{
  /* nothing to do */
}

SIM_RC
sim_load (sd, prog, abfd, from_tty)
     SIM_DESC sd;
     char * prog;
     bfd * abfd;
     int from_tty;
{
  /* Do the right thing for ELF executables; this turns out to be
     just about the right thing for any object format that:
       - we crack using BFD routines
       - follows the traditional UNIX text/data/bss layout
       - calls the bss section ".bss".   */

  extern bfd * sim_load_file (); /* ??? Don't know where this should live.  */
  bfd * prog_bfd;

  {
    bfd * handle;
    asection * s_bss;
    handle = bfd_openr (prog, 0);	/* could be "mcore" */
    
    if (!handle)
      {
	printf("``%s'' could not be opened.\n", prog);
	return SIM_RC_FAIL;
      }
    
    /* Makes sure that we have an object file, also cleans gets the 
       section headers in place.  */
    if (!bfd_check_format (handle, bfd_object))
      {
	/* wasn't an object file */
	bfd_close (handle);
	printf ("``%s'' is not appropriate object file.\n", prog);
	return SIM_RC_FAIL;
      }

    /* Look for that bss section.  */
    s_bss = bfd_get_section_by_name (handle, ".bss");
    
    if (!s_bss)
      {
	printf("``%s'' has no bss section.\n", prog);
	return SIM_RC_FAIL;
      }

    /* Appropriately paranoid would check that we have
       a traditional text/data/bss ordering within memory.  */

    /* figure the end of the bss section */
#if 0
    printf ("bss section at 0x%08x for 0x%08x bytes\n",
	    (unsigned long) bfd_get_section_vma (handle, s_bss),
	    (unsigned long) bfd_section_size (handle, s_bss));
#endif
    heap_ptr = ((unsigned long) bfd_get_section_vma (handle, s_bss)
		+ (unsigned long) bfd_section_size (handle, s_bss));

    /* Clean up after ourselves.  */
    bfd_close (handle);
    
    /* XXX: do we need to free the s_bss and handle structures? */
  }

  /* from sh -- dac */
  prog_bfd = sim_load_file (sd, myname, callback, prog, abfd,
                            sim_kind == SIM_OPEN_DEBUG,
                            0, sim_write);
  if (prog_bfd == NULL)
    return SIM_RC_FAIL;
  
  target_big_endian = bfd_big_endian (prog_bfd);
    
  if (abfd == NULL)
    bfd_close (prog_bfd);

  return SIM_RC_OK;
}

SIM_RC
sim_create_inferior (sd, prog_bfd, argv, env)
     SIM_DESC sd;
     struct bfd * prog_bfd;
     char ** argv;
     char ** env;
{
  char ** avp;
  int nargs = 0;
  int nenv = 0;
  int s_length;
  int l;
  unsigned long strings;
  unsigned long pointers;
  unsigned long hi_stack;


  /* Set the initial register set.  */
  l = issue_messages;
  issue_messages = 0;
  set_initial_gprs ();
  issue_messages = l;
  
  hi_stack = cpu.asregs.msize - 4;
  cpu.asregs.pc = bfd_get_start_address (prog_bfd);

  /* Calculate the argument and environment strings.  */
  s_length = 0;
  nargs = 0;
  avp = argv;
  while (avp && *avp)
    {
      l = strlen (*avp) + 1;	/* include the null */
      s_length += (l + 3) & ~3;	/* make it a 4 byte boundary */
      nargs++; avp++;
    }

  nenv = 0;
  avp = env;
  while (avp && *avp)
    {
      l = strlen (*avp) + 1;	/* include the null */
      s_length += (l + 3) & ~ 3;/* make it a 4 byte boundary */
      nenv++; avp++;
    }

  /* Claim some memory for the pointers and strings. */
  pointers = hi_stack - sizeof(word) * (nenv+1+nargs+1);
  pointers &= ~3;		/* must be 4-byte aligned */
  cpu.gr[0] = pointers;

  strings = cpu.gr[0] - s_length;
  strings &= ~3;		/* want to make it 4-byte aligned */
  cpu.gr[0] = strings;
  /* dac fix, the stack address must be 8-byte aligned! */
  cpu.gr[0] = cpu.gr[0] - cpu.gr[0] % 8;

  /* Loop through the arguments and fill them in.  */
  cpu.gr[PARM1] = nargs;
  if (nargs == 0)
    {
      /* No strings to fill in.  */
      cpu.gr[PARM2] = 0;
    }
  else
    {
      cpu.gr[PARM2] = pointers;
      avp = argv;
      while (avp && *avp)
	{
	  /* Save where we're putting it.  */
	  wlat (pointers, strings);

	  /* Copy the string.  */
	  l = strlen (* avp) + 1;
	  strcpy ((char *)(cpu.mem + strings), *avp);

	  /* Bump the pointers.  */
	  avp++;
	  pointers += 4;
	  strings += l+1;
	}
      
      /* A null to finish the list.  */
      wlat (pointers, 0);
      pointers += 4;
    }

  /* Now do the environment pointers.  */
  if (nenv == 0)
    {
      /* No strings to fill in.  */
      cpu.gr[PARM3] = 0;
    }
  else
    {
      cpu.gr[PARM3] = pointers;
      avp = env;
      
      while (avp && *avp)
	{
	  /* Save where we're putting it.  */
	  wlat (pointers, strings);

	  /* Copy the string.  */
	  l = strlen (* avp) + 1;
	  strcpy ((char *)(cpu.mem + strings), *avp);

	  /* Bump the pointers.  */
	  avp++;
	  pointers += 4;
	  strings += l+1;
	}
      
      /* A null to finish the list.  */
      wlat (pointers, 0);
      pointers += 4;
    }
  
  return SIM_RC_OK;
}

void
sim_kill (sd)
     SIM_DESC sd;
{
  /* nothing to do */
}

void
sim_do_command (sd, cmd)
     SIM_DESC sd;
     char * cmd;
{
  /* Nothing there yet; it's all an error.  */
  
  if (cmd != NULL)
    {
      char ** simargv = buildargv (cmd);
      
      if (strcmp (simargv[0], "watch") == 0)
	{
	  if ((simargv[1] == NULL) || (simargv[2] == NULL))
	    {
	      fprintf (stderr, "Error: missing argument to watch cmd.\n");
	      return;
	    }
	  
	  ENDWL++;
	  
	  WL[ENDWL] = strtol (simargv[2], NULL, 0);
	  WLstr[ENDWL] = strdup (simargv[1]);
	  fprintf (stderr, "Added %s (%x) to watchlist, #%d\n",WLstr[ENDWL],
		   WL[ENDWL], ENDWL);

	}
      else if (strcmp (simargv[0], "dumpmem") == 0)
	{
	  unsigned char * p;
	  FILE * dumpfile;

	  if (simargv[1] == NULL)
	    fprintf (stderr, "Error: missing argument to dumpmem cmd.\n");

	  fprintf (stderr, "Writing dumpfile %s...",simargv[1]);
	  
	  dumpfile = fopen (simargv[1], "w");
	  p = cpu.mem;
	  fwrite (p, cpu.asregs.msize-1, 1, dumpfile);
	  fclose (dumpfile);
	  
	  fprintf (stderr, "done.\n");
	}
      else if (strcmp (simargv[0], "clearstats") == 0)
	{
	  cpu.asregs.cycles = 0;
	  cpu.asregs.insts = 0;
	  cpu.asregs.stalls = 0;
	  ENDWL = 0;
	}
      else if (strcmp (simargv[0], "verbose") == 0)
	{
	  issue_messages = 2;
	}
      else
	{
	  fprintf (stderr,"Error: \"%s\" is not a valid M.CORE simulator command.\n",
		   cmd);
	}
    }
  else
    {
      fprintf (stderr, "M.CORE sim commands: \n");
      fprintf (stderr, "  watch <funcname> <addr>\n");
      fprintf (stderr, "  dumpmem <filename>\n");
      fprintf (stderr, "  clearstats\n");
      fprintf (stderr, "  verbose\n");
    }
}

void
sim_set_callbacks (ptr)
     host_callback * ptr;
{
  callback = ptr; 
}
