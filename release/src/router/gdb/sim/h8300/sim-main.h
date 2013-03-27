/* Main header for the Hitachi h8/300 architecture.  */

#include "bfd.h"

#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#define DEBUG

/* These define the size of main memory for the simulator.

   Note the size of main memory for the H8/300H is only 256k.  Keeping it
   small makes the simulator run much faster and consume less memory.

   The linker knows about the limited size of the simulator's main memory
   on the H8/300H (via the h8300h.sc linker script).  So if you change
   H8300H_MSIZE, be sure to fix the linker script too.

   Also note that there's a separate "eightbit" area aside from main
   memory.  For simplicity, the simulator assumes any data memory reference
   outside of main memory refers to the eightbit area (in theory, this
   can only happen when simulating H8/300H programs).  We make no attempt
   to catch overlapping addresses, wrapped addresses, etc etc.  */

#define H8300_MSIZE (1 << 16)

/* avolkov: 
   Next 2 macros are ugly for any workstation, but while they're work.
   Memory size MUST be configurable.  */
#define H8300H_MSIZE (1 << 24) 
#define H8300S_MSIZE (1 << 24) 

#define CSIZE 1024

enum h8_regnum {
  R0_REGNUM = 0,
  R1_REGNUM = 1,
  R2_REGNUM = 2,
  R3_REGNUM = 3,
  R4_REGNUM = 4,
  R5_REGNUM = 5,
  R6_REGNUM = 6,
  R7_REGNUM = 7,

  SP_REGNUM = R7_REGNUM,	/* Contains address of top of stack */
  FP_REGNUM = R6_REGNUM,	/* Contains address of executing
				   stack frame */
  CCR_REGNUM = 8,		/* Contains processor status */
  PC_REGNUM  = 9,		/* Contains program counter */
  CYCLE_REGNUM = 10,
  EXR_REGNUM  = 11,
  INST_REGNUM = 12,
  TICK_REGNUM = 13,
  MACH_REGNUM = 14,
  MACL_REGNUM = 15,
  SBR_REGNUM =  16,
  VBR_REGNUM =  17,

  ZERO_REGNUM = 18
};

enum h8_typecodes {
  OP_NULL,
  OP_REG,		/* Register direct.  */
  OP_LOWREG,		/* Special reg syntax for "bra".  */
  OP_DISP,		/* Register indirect w/displacement.  */
  /* Note: h8300, h8300h, and h8300s permit only pre-decr and post-incr.  */
  OP_PREDEC,		/* Register indirect w/pre-decrement.  */
  OP_POSTDEC,		/* Register indirect w/post-decrement.  */
  OP_PREINC,		/* Register indirect w/pre-increment.  */
  OP_POSTINC,		/* Register indirect w/post-increment.  */
  OP_PCREL,		/* PC Relative.  */
  OP_MEM,		/* Absolute memory address.  */
  OP_CCR,		/* Condition Code Register.  */
  OP_IMM,		/* Immediate value.  */
  /*OP_ABS*/		/* Un-used (duplicates op_mem?).  */
  OP_EXR,		/* EXtended control Register.  */
  OP_SBR, 		/* Vector Base Register.  */
  OP_VBR,		/* Short-address Base Register.  */
  OP_MACH,		/* Multiply Accumulator - high.  */
  OP_MACL,		/* Multiply Accumulator - low.   */
  /* FIXME: memory indirect?  */
  OP_INDEXB,		/* Byte index mode */
  OP_INDEXW,		/* Word index mode */
  OP_INDEXL		/* Long index mode */
};

#include "sim-basics.h"

/* Define sim_cia.  */
typedef unsigned32 sim_cia;

#include "sim-base.h"

/* Structure used to describe addressing */

typedef struct
{
  int type;
  int reg;
  int literal;
} ea_type;

/* Struct for instruction decoder.  */
typedef struct
{
  ea_type src;
  ea_type dst;
  ea_type op3;
  int opcode;
  int next_pc;
  int oldpc;
  int cycles;
#ifdef DEBUG
  struct h8_opcode *op;
#endif
} decoded_inst;

struct _sim_cpu {
  unsigned int regs[20];	/* 8 GR's plus ZERO, SBR, and VBR.  */
  unsigned int pc;

  int macS;			/* MAC Saturating mode */
  int macV;			/* MAC Overflow */
  int macN;			/* MAC Negative */
  int macZ;			/* MAC Zero     */

  int delayed_branch;
  char **command_line;		/* Pointer to command line arguments.  */

  unsigned char *memory;
  unsigned char *eightbit;
  int mask;
  
  sim_cpu_base base;
};

/* The sim_state struct.  */
struct sim_state {
  struct _sim_cpu *cpu;
  unsigned int sim_cache_size;
  decoded_inst *sim_cache;
  unsigned short *cache_idx;
  unsigned long memory_size;
  int cache_top;
  int compiles;
#ifdef ADEBUG
  int stats[O_LAST];
#endif
  sim_state_base base;
};

/* The current state of the processor; registers, memory, etc.  */

#define CIA_GET(CPU)		(cpu_get_pc (CPU))
#define CIA_SET(CPU, VAL)	(cpu_set_pc ((CPU), (VAL)))
#define STATE_CPU(SD, N)	((SD)->cpu)	/* Single Processor.  */
#define cpu_set_pc(CPU, VAL)	(((CPU)->pc)  = (VAL))
#define cpu_get_pc(CPU)		(((CPU)->pc))

/* Magic numbers used to distinguish an exit from a breakpoint.  */
#define LIBC_EXIT_MAGIC1 0xdead	
#define LIBC_EXIT_MAGIC2 0xbeef	
/* Local version of macros for decoding exit status.  
   (included here rather than try to find target version of wait.h)
*/
#define SIM_WIFEXITED(V)	(((V) & 0xff) == 0)
#define SIM_WIFSTOPPED(V)	(!SIM_WIFEXITED (V))
#define SIM_WEXITSTATUS(V)	(((V) >> 8) & 0xff)
#define SIM_WSTOPSIG(V)		((V) & 0x7f)

#endif /* SIM_MAIN_H */
