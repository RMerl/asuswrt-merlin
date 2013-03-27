#include "sim-main.h"
#include "targ-vals.h"

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>



#define REG0(X) ((X) & 0x3)
#define REG1(X) (((X) & 0xc) >> 2)
#define REG0_4(X) (((X) & 0x30) >> 4)
#define REG0_8(X) (((X) & 0x300) >> 8)
#define REG1_8(X) (((X) & 0xc00) >> 10)
#define REG0_16(X) (((X) & 0x30000) >> 16)
#define REG1_16(X) (((X) & 0xc0000) >> 18)


INLINE_SIM_MAIN (void)
genericAdd(unsigned32 source, unsigned32 destReg)
{
  int z, c, n, v;
  unsigned32 dest, sum;

  dest = State.regs[destReg];
  sum = source + dest;
  State.regs[destReg] = sum;

  z = (sum == 0);
  n = (sum & 0x80000000);
  c = (sum < source) || (sum < dest);
  v = ((dest & 0x80000000) == (source & 0x80000000)
       && (dest & 0x80000000) != (sum & 0x80000000));

  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= ((z ? PSW_Z : 0) | ( n ? PSW_N : 0)
	  | (c ? PSW_C : 0) | (v ? PSW_V : 0));
}




INLINE_SIM_MAIN (void)
genericSub(unsigned32 source, unsigned32 destReg)
{
  int z, c, n, v;
  unsigned32 dest, difference;

  dest = State.regs[destReg];
  difference = dest - source;
  State.regs[destReg] = difference;

  z = (difference == 0);
  n = (difference & 0x80000000);
  c = (source > dest);
  v = ((dest & 0x80000000) != (source & 0x80000000)
       && (dest & 0x80000000) != (difference & 0x80000000));

  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= ((z ? PSW_Z : 0) | ( n ? PSW_N : 0)
	  | (c ? PSW_C : 0) | (v ? PSW_V : 0));
}

INLINE_SIM_MAIN (void)
genericCmp(unsigned32 leftOpnd, unsigned32 rightOpnd)
{
  int z, c, n, v;
  unsigned32 value;

  value = rightOpnd - leftOpnd;

  z = (value == 0);
  n = (value & 0x80000000);
  c = (leftOpnd > rightOpnd);
  v = ((rightOpnd & 0x80000000) != (leftOpnd & 0x80000000)
       && (rightOpnd & 0x80000000) != (value & 0x80000000));

  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= ((z ? PSW_Z : 0) | ( n ? PSW_N : 0)
	  | (c ? PSW_C : 0) | (v ? PSW_V : 0));
}


INLINE_SIM_MAIN (void)
genericOr(unsigned32 source, unsigned32 destReg)
{
  int n, z;

  State.regs[destReg] |= source;
  z = (State.regs[destReg] == 0);
  n = (State.regs[destReg] & 0x80000000) != 0;
  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= ((z ? PSW_Z : 0) | (n ? PSW_N : 0));
}


INLINE_SIM_MAIN (void)
genericXor(unsigned32 source, unsigned32 destReg)
{
  int n, z;

  State.regs[destReg] ^= source;
  z = (State.regs[destReg] == 0);
  n = (State.regs[destReg] & 0x80000000) != 0;
  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= ((z ? PSW_Z : 0) | (n ? PSW_N : 0));
}


INLINE_SIM_MAIN (void)
genericBtst(unsigned32 leftOpnd, unsigned32 rightOpnd)
{
  unsigned32 temp;
  int z, n;

  temp = rightOpnd;
  temp &= leftOpnd;
  n = (temp & 0x80000000) != 0;
  z = (temp == 0);
  PSW &= ~(PSW_Z | PSW_N | PSW_C | PSW_V);
  PSW |= (z ? PSW_Z : 0) | (n ? PSW_N : 0);
}

/* Read/write functions for system call interface.  */
INLINE_SIM_MAIN (int)
syscall_read_mem (host_callback *cb, struct cb_syscall *sc,
		  unsigned long taddr, char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  sim_cpu *cpu = STATE_CPU(sd, 0);

  return sim_core_read_buffer (sd, cpu, read_map, buf, taddr, bytes);
}

INLINE_SIM_MAIN (int)
syscall_write_mem (host_callback *cb, struct cb_syscall *sc,
		   unsigned long taddr, const char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  sim_cpu *cpu = STATE_CPU(sd, 0);

  return sim_core_write_buffer (sd, cpu, write_map, buf, taddr, bytes);
}


/* syscall */
INLINE_SIM_MAIN (void)
do_syscall (void)
{

  /* We use this for simulated system calls; we may need to change
     it to a reserved instruction if we conflict with uses at
     Matsushita.  */
  int save_errno = errno;	
  errno = 0;

/* Registers passed to trap 0 */

/* Function number.  */
#define FUNC   (State.regs[0])

/* Parameters.  */
#define PARM1   (State.regs[1])
#define PARM2   (load_word (State.regs[REG_SP] + 12))
#define PARM3   (load_word (State.regs[REG_SP] + 16))

/* Registers set by trap 0 */

#define RETVAL State.regs[0]	/* return value */
#define RETERR State.regs[1]	/* return error code */

/* Turn a pointer in a register into a pointer into real memory. */
#define MEMPTR(x) (State.mem + x)

  if ( FUNC == TARGET_SYS_exit )
    {
      /* EXIT - caller can look in PARM1 to work out the reason */
      if (PARM1 == 0xdead)
	State.exception = SIGABRT;
      else
	{
	  sim_engine_halt (simulator, STATE_CPU (simulator, 0), NULL, PC,
			   sim_exited, PARM1);
	  State.exception = SIGQUIT;
	}
      State.exited = 1;
    }
  else
    {
      CB_SYSCALL syscall;

      CB_SYSCALL_INIT (&syscall);
      syscall.arg1 = PARM1;
      syscall.arg2 = PARM2;
      syscall.arg3 = PARM3;
      syscall.func = FUNC;
      syscall.p1 = (PTR) simulator;
      syscall.read_mem = syscall_read_mem;
      syscall.write_mem = syscall_write_mem;
      cb_syscall (STATE_CALLBACK (simulator), &syscall);
      RETERR = syscall.errcode;
      RETVAL = syscall.result;
    }


  errno = save_errno;
}

