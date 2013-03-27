#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include "ansidecl.h"
#include "gdb/callback.h"
#include "opcode/d10v.h"
#include "bfd.h"

#define DEBUG_TRACE		0x00000001
#define DEBUG_VALUES		0x00000002
#define DEBUG_LINE_NUMBER	0x00000004
#define DEBUG_MEMSIZE		0x00000008
#define DEBUG_INSTRUCTION	0x00000010
#define DEBUG_TRAP		0x00000020
#define DEBUG_MEMORY		0x00000040

#ifndef	DEBUG
#define	DEBUG (DEBUG_TRACE | DEBUG_VALUES | DEBUG_LINE_NUMBER)
#endif

extern int d10v_debug;

#include "gdb/remote-sim.h"
#include "sim-config.h"
#include "sim-types.h"

typedef unsigned8 uint8;
typedef unsigned16 uint16;
typedef signed16 int16;
typedef unsigned32 uint32;
typedef signed32 int32;
typedef unsigned64 uint64;
typedef signed64 int64;

/* FIXME: D10V defines */
typedef uint16 reg_t;

struct simops 
{
  long opcode;
  int  is_long;
  long mask;
  int format;
  int cycles;
  int unit;
  int exec_type;
  void (*func)();
  int numops;
  int operands[9];
};

enum _ins_type
{
  INS_UNKNOWN,			/* unknown instruction */
  INS_COND_TRUE,		/* # times EXExxx executed other instruction */
  INS_COND_FALSE,		/* # times EXExxx did not execute other instruction */
  INS_COND_JUMP,		/* # times JUMP skipped other instruction */
  INS_CYCLES,			/* # cycles */
  INS_LONG,			/* long instruction (both containers, ie FM == 11) */
  INS_LEFTRIGHT,		/* # times instruction encoded as L -> R (ie, FM == 01) */
  INS_RIGHTLEFT,		/* # times instruction encoded as L <- R (ie, FM == 10) */
  INS_PARALLEL,			/* # times instruction encoded as L || R (ie, RM == 00) */

  INS_LEFT,			/* normal left instructions */
  INS_LEFT_PARALLEL,		/* left side of || */
  INS_LEFT_COND_TEST,		/* EXExx test on left side */
  INS_LEFT_COND_EXE,		/* execution after EXExxx test on right side succeeded */
  INS_LEFT_NOPS,		/* NOP on left side */

  INS_RIGHT,			/* normal right instructions */
  INS_RIGHT_PARALLEL,		/* right side of || */
  INS_RIGHT_COND_TEST,		/* EXExx test on right side */
  INS_RIGHT_COND_EXE,		/* execution after EXExxx test on left side succeeded */
  INS_RIGHT_NOPS,		/* NOP on right side */

  INS_MAX
};

extern unsigned long ins_type_counters[ (int)INS_MAX ];

enum {
  SP_IDX = 15,
};

/* Write-back slots */
union slot_data {
  unsigned_1 _1;
  unsigned_2 _2;
  unsigned_4 _4;
  unsigned_8 _8;
};
struct slot {
  void *dest;
  int size;
  union slot_data data;
  union slot_data mask;
};
enum {
 NR_SLOTS = 16,
};
#define SLOT (State.slot)
#define SLOT_NR (State.slot_nr)
#define SLOT_PEND_MASK(DEST, MSK, VAL) \
  do \
    { \
      SLOT[SLOT_NR].dest = &(DEST); \
      SLOT[SLOT_NR].size = sizeof (DEST); \
      switch (sizeof (DEST)) \
        { \
        case 1: \
          SLOT[SLOT_NR].data._1 = (unsigned_1) (VAL); \
          SLOT[SLOT_NR].mask._1 = (unsigned_1) (MSK); \
          break; \
        case 2: \
          SLOT[SLOT_NR].data._2 = (unsigned_2) (VAL); \
          SLOT[SLOT_NR].mask._2 = (unsigned_2) (MSK); \
          break; \
        case 4: \
          SLOT[SLOT_NR].data._4 = (unsigned_4) (VAL); \
          SLOT[SLOT_NR].mask._4 = (unsigned_4) (MSK); \
          break; \
        case 8: \
          SLOT[SLOT_NR].data._8 = (unsigned_8) (VAL); \
          SLOT[SLOT_NR].mask._8 = (unsigned_8) (MSK); \
          break; \
        } \
      SLOT_NR = (SLOT_NR + 1); \
    } \
  while (0)
#define SLOT_PEND(DEST, VAL) SLOT_PEND_MASK(DEST, 0, VAL)
#define SLOT_DISCARD() (SLOT_NR = 0)
#define SLOT_FLUSH() \
  do \
    { \
      int i; \
      for (i = 0; i < SLOT_NR; i++) \
	{ \
	  switch (SLOT[i].size) \
	    { \
	    case 1: \
	      *(unsigned_1*) SLOT[i].dest &= SLOT[i].mask._1; \
	      *(unsigned_1*) SLOT[i].dest |= SLOT[i].data._1; \
	      break; \
	    case 2: \
	      *(unsigned_2*) SLOT[i].dest &= SLOT[i].mask._2; \
	      *(unsigned_2*) SLOT[i].dest |= SLOT[i].data._2; \
	      break; \
	    case 4: \
	      *(unsigned_4*) SLOT[i].dest &= SLOT[i].mask._4; \
	      *(unsigned_4*) SLOT[i].dest |= SLOT[i].data._4; \
	      break; \
	    case 8: \
	      *(unsigned_8*) SLOT[i].dest &= SLOT[i].mask._8; \
	      *(unsigned_8*) SLOT[i].dest |= SLOT[i].data._8; \
	      break; \
	    } \
        } \
      SLOT_NR = 0; \
    } \
  while (0)
#define SLOT_DUMP() \
  do \
    { \
      int i; \
      for (i = 0; i < SLOT_NR; i++) \
	{ \
	  switch (SLOT[i].size) \
	    { \
	    case 1: \
              printf ("SLOT %d *0x%08lx & 0x%02x | 0x%02x\n", i, \
		      (long) SLOT[i].dest, \
                      (unsigned) SLOT[i].mask._1, \
                      (unsigned) SLOT[i].data._1); \
	      break; \
	    case 2: \
              printf ("SLOT %d *0x%08lx & 0x%04x | 0x%04x\n", i, \
		      (long) SLOT[i].dest, \
                      (unsigned) SLOT[i].mask._2, \
                      (unsigned) SLOT[i].data._2); \
	      break; \
	    case 4: \
              printf ("SLOT %d *0x%08lx & 0x%08x | 0x%08x\n", i, \
		      (long) SLOT[i].dest, \
                      (unsigned) SLOT[i].mask._4, \
                      (unsigned) SLOT[i].data._4); \
	      break; \
	    case 8: \
              printf ("SLOT %d *0x%08lx & 0x%08x%08x | 0x%08x%08x\n", i, \
		      (long) SLOT[i].dest, \
                      (unsigned) (SLOT[i].mask._8 >> 32),  \
                      (unsigned) SLOT[i].mask._8, \
                      (unsigned) (SLOT[i].data._8 >> 32),  \
                      (unsigned) SLOT[i].data._8); \
	      break; \
	    } \
        } \
    } \
  while (0)

/* d10v memory: There are three separate d10v memory regions IMEM,
   UMEM and DMEM.  The IMEM and DMEM are further broken down into
   blocks (very like VM pages). */

enum
{
  IMAP_BLOCK_SIZE = 0x20000,
  DMAP_BLOCK_SIZE = 0x4000,
};

/* Implement the three memory regions using sparse arrays.  Allocate
   memory using ``segments''.  A segment must be at least as large as
   a BLOCK - ensures that an access that doesn't cross a block
   boundary can't cross a segment boundary */

enum
{
  SEGMENT_SIZE = 0x20000, /* 128KB - MAX(IMAP_BLOCK_SIZE,DMAP_BLOCK_SIZE) */
  IMEM_SEGMENTS = 8, /* 1MB */
  DMEM_SEGMENTS = 8, /* 1MB */
  UMEM_SEGMENTS = 128 /* 16MB */
};

struct d10v_memory
{
  uint8 *insn[IMEM_SEGMENTS];
  uint8 *data[DMEM_SEGMENTS];
  uint8 *unif[UMEM_SEGMENTS];
  uint8 fault[16];
};

struct _state
{
  reg_t regs[16];		/* general-purpose registers */
#define GPR(N) (State.regs[(N)] + 0)
#define SET_GPR(N,VAL) SLOT_PEND (State.regs[(N)], (VAL))

#define GPR32(N) ((((uint32) State.regs[(N) + 0]) << 16) \
		  | (uint16) State.regs[(N) + 1])
#define SET_GPR32(N,VAL) do { SET_GPR (OP[0] + 0, (VAL) >> 16); SET_GPR (OP[0] + 1, (VAL)); } while (0)

  reg_t cregs[16];		/* control registers */
#define CREG(N) (State.cregs[(N)] + 0)
#define SET_CREG(N,VAL) move_to_cr ((N), 0, (VAL), 0)
#define SET_HW_CREG(N,VAL) move_to_cr ((N), 0, (VAL), 1)

  reg_t sp[2];                  /* holding area for SPI(0)/SPU(1) */
#define HELD_SP(N) (State.sp[(N)] + 0)
#define SET_HELD_SP(N,VAL) SLOT_PEND (State.sp[(N)], (VAL))

  int64 a[2];			/* accumulators */
#define ACC(N) (State.a[(N)] + 0)
#define SET_ACC(N,VAL) SLOT_PEND (State.a[(N)], (VAL) & MASK40)

  /* writeback info */
  struct slot slot[NR_SLOTS];
  int slot_nr;

  /* trace data */
  struct {
    uint16 psw;
  } trace;

  uint8 exe;
  int	exception;
  int	pc_changed;

  /* NOTE: everything below this line is not reset by
     sim_create_inferior() */

  struct d10v_memory mem;

  enum _ins_type ins_type;

} State;


extern host_callback *d10v_callback;
extern uint16 OP[4];
extern struct simops Simops[];
extern asection *text;
extern bfd_vma text_start;
extern bfd_vma text_end;
extern bfd *prog_bfd;

enum
{
  PSW_CR = 0,
  BPSW_CR = 1,
  PC_CR = 2,
  BPC_CR = 3,
  DPSW_CR = 4,
  DPC_CR = 5,
  RPT_C_CR = 7,
  RPT_S_CR = 8,
  RPT_E_CR = 9,
  MOD_S_CR = 10,
  MOD_E_CR = 11,
  IBA_CR = 14,
};

enum
{
  PSW_SM_BIT = 0x8000,
  PSW_EA_BIT = 0x2000,
  PSW_DB_BIT = 0x1000,
  PSW_DM_BIT = 0x0800,
  PSW_IE_BIT = 0x0400,
  PSW_RP_BIT = 0x0200,
  PSW_MD_BIT = 0x0100,
  PSW_FX_BIT = 0x0080,
  PSW_ST_BIT = 0x0040,
  PSW_F0_BIT = 0x0008,
  PSW_F1_BIT = 0x0004,
  PSW_C_BIT =  0x0001,
};

#define PSW CREG (PSW_CR)
#define SET_PSW(VAL) SET_CREG (PSW_CR, (VAL))
#define SET_HW_PSW(VAL) SET_HW_CREG (PSW_CR, (VAL))
#define SET_PSW_BIT(MASK,VAL) move_to_cr (PSW_CR, ~((reg_t) MASK), (VAL) ? (MASK) : 0, 1)

#define PSW_SM ((PSW & PSW_SM_BIT) != 0)
#define SET_PSW_SM(VAL) SET_PSW_BIT (PSW_SM_BIT, (VAL))

#define PSW_EA ((PSW & PSW_EA_BIT) != 0)
#define SET_PSW_EA(VAL) SET_PSW_BIT (PSW_EA_BIT, (VAL))

#define PSW_DB ((PSW & PSW_DB_BIT) != 0)
#define SET_PSW_DB(VAL) SET_PSW_BIT (PSW_DB_BIT, (VAL))

#define PSW_DM ((PSW & PSW_DM_BIT) != 0)
#define SET_PSW_DM(VAL) SET_PSW_BIT (PSW_DM_BIT, (VAL))

#define PSW_IE ((PSW & PSW_IE_BIT) != 0)
#define SET_PSW_IE(VAL) SET_PSW_BIT (PSW_IE_BIT, (VAL))

#define PSW_RP ((PSW & PSW_RP_BIT) != 0)
#define SET_PSW_RP(VAL) SET_PSW_BIT (PSW_RP_BIT, (VAL))

#define PSW_MD ((PSW & PSW_MD_BIT) != 0)
#define SET_PSW_MD(VAL) SET_PSW_BIT (PSW_MD_BIT, (VAL))

#define PSW_FX ((PSW & PSW_FX_BIT) != 0)
#define SET_PSW_FX(VAL) SET_PSW_BIT (PSW_FX_BIT, (VAL))

#define PSW_ST ((PSW & PSW_ST_BIT) != 0)
#define SET_PSW_ST(VAL) SET_PSW_BIT (PSW_ST_BIT, (VAL))

#define PSW_F0 ((PSW & PSW_F0_BIT) != 0)
#define SET_PSW_F0(VAL) SET_PSW_BIT (PSW_F0_BIT, (VAL))

#define PSW_F1 ((PSW & PSW_F1_BIT) != 0)
#define SET_PSW_F1(VAL) SET_PSW_BIT (PSW_F1_BIT, (VAL))

#define PSW_C ((PSW & PSW_C_BIT) != 0)
#define SET_PSW_C(VAL) SET_PSW_BIT (PSW_C_BIT, (VAL))

/* See simopsc.:move_to_cr() for registers that can not be read-from
   or assigned-to directly */

#define PC	CREG (PC_CR)
#define SET_PC(VAL) SET_CREG (PC_CR, (VAL))

#define BPSW	CREG (BPSW_CR)
#define SET_BPSW(VAL) SET_CREG (BPSW_CR, (VAL))

#define BPC	CREG (BPC_CR)
#define SET_BPC(VAL) SET_CREG (BPC_CR, (VAL))

#define DPSW	CREG (DPSW_CR)
#define SET_DPSW(VAL) SET_CREG (DPSW_CR, (VAL))

#define DPC	CREG (DPC_CR)
#define SET_DPC(VAL) SET_CREG (DPC_CR, (VAL))

#define RPT_C	CREG (RPT_C_CR)
#define SET_RPT_C(VAL) SET_CREG (RPT_C_CR, (VAL))

#define RPT_S	CREG (RPT_S_CR)
#define SET_RPT_S(VAL) SET_CREG (RPT_S_CR, (VAL))

#define RPT_E	CREG (RPT_E_CR)
#define SET_RPT_E(VAL) SET_CREG (RPT_E_CR, (VAL))

#define MOD_S	CREG (MOD_S_CR)
#define SET_MOD_S(VAL) SET_CREG (MOD_S_CR, (VAL))

#define MOD_E	CREG (MOD_E_CR)
#define SET_MOD_E(VAL) SET_CREG (MOD_E_CR, (VAL))

#define IBA	CREG (IBA_CR)
#define SET_IBA(VAL) SET_CREG (IBA_CR, (VAL))


#define SIG_D10V_STOP	-1
#define SIG_D10V_EXIT	-2
#define SIG_D10V_BUS    -3

#define SEXT3(x)	((((x)&0x7)^(~3))+4)	

/* sign-extend a 4-bit number */
#define SEXT4(x)	((((x)&0xf)^(~7))+8)	

/* sign-extend an 8-bit number */
#define SEXT8(x)	((((x)&0xff)^(~0x7f))+0x80)

/* sign-extend a 16-bit number */
#define SEXT16(x)	((((x)&0xffff)^(~0x7fff))+0x8000)

/* sign-extend a 32-bit number */
#define SEXT32(x)	((((x)&SIGNED64(0xffffffff))^(~SIGNED64(0x7fffffff)))+SIGNED64(0x80000000))

/* sign extend a 40 bit number */
#define SEXT40(x)	((((x)&SIGNED64(0xffffffffff))^(~SIGNED64(0x7fffffffff)))+SIGNED64(0x8000000000))

/* sign extend a 44 bit number */
#define SEXT44(x)	((((x)&SIGNED64(0xfffffffffff))^(~SIGNED64(0x7ffffffffff)))+SIGNED64(0x80000000000))

/* sign extend a 56 bit number */
#define SEXT56(x)	((((x)&SIGNED64(0xffffffffffffff))^(~SIGNED64(0x7fffffffffffff)))+SIGNED64(0x80000000000000))

/* sign extend a 60 bit number */
#define SEXT60(x)	((((x)&SIGNED64(0xfffffffffffffff))^(~SIGNED64(0x7ffffffffffffff)))+SIGNED64(0x800000000000000))

#define MAX32	SIGNED64(0x7fffffff)
#define MIN32	SIGNED64(0xff80000000)
#define MASK32	SIGNED64(0xffffffff)
#define MASK40	SIGNED64(0xffffffffff)

/* The alignment of MOD_E in the following macro depends upon "i"
   always being a power of 2. */
#define INC_ADDR(x,i) \
do \
  { \
    int test_i = i < 0 ? i : ~((i) - 1); \
    if (PSW_MD && GPR (x) == (MOD_E & test_i)) \
      SET_GPR (x, MOD_S & test_i); \
    else \
      SET_GPR (x, GPR (x) + (i)); \
  } \
while (0)

extern uint8 *dmem_addr (uint16 offset);
extern uint8 *imem_addr PARAMS ((uint32));
extern bfd_vma decode_pc PARAMS ((void));

#define	RB(x)	(*(dmem_addr(x)))
#define SB(addr,data)	( RB(addr) = (data & 0xff))

#if defined(__GNUC__) && defined(__OPTIMIZE__) && !defined(NO_ENDIAN_INLINE)
#define ENDIAN_INLINE static __inline__
#include "endian.c"
#undef ENDIAN_INLINE

#else
extern uint32 get_longword PARAMS ((uint8 *));
extern uint16 get_word PARAMS ((uint8 *));
extern int64 get_longlong PARAMS ((uint8 *));
extern void write_word PARAMS ((uint8 *addr, uint16 data));
extern void write_longword PARAMS ((uint8 *addr, uint32 data));
extern void write_longlong PARAMS ((uint8 *addr, int64 data));
#endif

#define SW(addr,data)		write_word(dmem_addr(addr),data)
#define RW(x)			get_word(dmem_addr(x))
#define SLW(addr,data)  	write_longword(dmem_addr(addr),data)
#define RLW(x)			get_longword(dmem_addr(x))
#define READ_16(x)		get_word(x)
#define WRITE_16(addr,data)	write_word(addr,data)
#define READ_64(x)		get_longlong(x)
#define WRITE_64(addr,data)	write_longlong(addr,data)

#define JMP(x)			do { SET_PC (x); State.pc_changed = 1; } while (0)

#define RIE_VECTOR_START 0xffc2
#define AE_VECTOR_START 0xffc3
#define TRAP_VECTOR_START 0xffc4	/* vector for trap 0 */
#define DBT_VECTOR_START 0xffd4
#define SDBT_VECTOR_START 0xffd5

/* Scedule a store of VAL into cr[CR].  MASK indicates the bits in
   cr[CR] that should not be modified (i.e. cr[CR] = (cr[CR] & MASK) |
   (VAL & ~MASK)).  In addition, unless PSW_HW_P, a VAL intended for
   PSW is masked for zero bits. */

extern reg_t move_to_cr (int cr, reg_t mask, reg_t val, int psw_hw_p);
