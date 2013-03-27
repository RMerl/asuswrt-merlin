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
#define H8300H_MSIZE (1 << 18) 
#define H8300S_MSIZE (1 << 24) 

#define CSIZE 1000

/* Local register names */
typedef enum
{
  R0, R1, R2, R3, R4, R5, R6, R7,
  R_ZERO,
  R_PC,				
  R_CCR,
  R_EXR,
  R_HARD_0,			
  R_LAST,
} reg_type;


/* Structure used to describe addressing */

typedef struct
{
  int type;
  int reg;
  int literal;
} ea_type;



typedef struct
{
  ea_type src;
  ea_type dst;
  int opcode;
  int next_pc;
  int oldpc;
  int cycles;
#ifdef DEBUG
struct h8_opcode *op;
#endif
}
decoded_inst;

enum h8300_sim_state {
  SIM_STATE_RUNNING, SIM_STATE_EXITED, SIM_STATE_SIGNALLED, SIM_STATE_STOPPED
};

/* For Command Line.  */
char **ptr_command_line; /* Pointer to Command Line Arguments. */

typedef struct
{
  enum h8300_sim_state state;
  int exception;
  unsigned  int regs[9];
  int pc;
  int ccr;
  int exr;

  unsigned char *memory;
  unsigned char *eightbit;
  unsigned short *cache_idx;
  int cache_top;
  int maximum;
  int csize;
  int mask;
  
  decoded_inst *cache;
  int cycles;
  int insts;
  int ticks;
  int compiles;
#ifdef ADEBUG
  int stats[O_LAST];
#endif
}
cpu_state_type;
