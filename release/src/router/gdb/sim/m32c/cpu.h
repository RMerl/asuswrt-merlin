/* cpu.h --- declarations for the M32C core.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

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


extern int verbose;
extern int trace;
extern int enable_counting;

typedef unsigned char QI;
typedef unsigned short HI;
typedef unsigned long SI;
typedef unsigned long long DI;

#define CPU_R8C		0x11
#define CPU_M16C	0x12
#define CPU_M32CM	0x23
#define CPU_M32C	0x24
extern int m32c_cpu;
void m32c_set_cpu (int cpu);

#define A16 (m32c_cpu & 0x10)
#define A24 (m32c_cpu & 0x20)

typedef struct
{
  HI r_r0;
  HI r_r2;
  HI r_r1;
  HI r_r3;
  SI r_a0;
  SI r_a1;
  SI r_sb;
  SI r_fb;
} reg_bank_type;

typedef struct
{
  reg_bank_type r[2];
  QI r_intbh;
  HI r_intbl;
  SI r_usp;
  SI r_isp;
  SI r_pc;
  HI r_flags;
} regs_type;

extern regs_type regs;
extern int addr_mask;
extern int membus_mask;

#define FLAGBIT_C	0x0001
#define FLAGBIT_D	0x0002
#define FLAGBIT_Z	0x0004
#define FLAGBIT_S	0x0008
#define FLAGBIT_B	0x0010
#define FLAGBIT_O	0x0020
#define FLAGBIT_I	0x0040
#define FLAGBIT_U	0x0080

#define REG_BANK (regs.r_flags & FLAG_B ? 1 : 0)

typedef enum
{
  mem,
  r0, r0h, r0l,
  r1, r1h, r1l,
  r2, r2r0,
  r3, r3r1,
  r3r1r2r0,
  r3r2r1r0,
  a0,
  a1, a1a0,
  sb, fb,
  intb, intbl, intbh,
  sp, usp, isp, pc, flags,
  num_regs
} reg_id;

extern char *reg_names[];
extern int reg_bytes[];

extern unsigned int b2mask[];
extern unsigned int b2signbit[];
extern int b2maxsigned[];
extern int b2minsigned[];

void init_regs (void);
void stack_heap_stats (void);
void set_pointer_width (int bytes);
unsigned int get_reg (reg_id id);
DI get_reg_ll (reg_id id);
void put_reg (reg_id id, unsigned int value);
void put_reg_ll (reg_id id, DI value);

void set_flags (int mask, int newbits);
void set_oszc (int value, int bytes, int c);
void set_szc (int value, int bytes, int c);
void set_osz (int value, int bytes);
void set_sz (int value, int bytes);
void set_zc (int z, int c);
void set_c (int c);

const char *bits (int v, int b);

typedef struct
{
  QI bytes;
  QI mem;
  HI mask;
  union
  {
    unsigned int addr;
    reg_id reg;
  } u;
} srcdest;

void decode_indirect (int src_indirect, int dest_indirect);
void decode_index (int src_addend, int dest_addend);

/* r8c */
srcdest decode_srcdest4 (int destcode, int bw);
srcdest decode_dest3 (int destcode, int bw);
srcdest decode_src2 (int srccode, int bw, int d);
srcdest decode_dest1 (int destcode, int bw);
srcdest decode_jumpdest (int destcode, int w);
srcdest decode_cr (int crcode);
srcdest decode_cr_b (int crcode, int bank);
#define CR_B_DCT0	0
#define CR_B_INTB	1
#define CR_B_DMA0	2

/* m32c */
srcdest decode_dest23 (int ddd, int dd, int bytes);
srcdest decode_src23 (int sss, int ss, int bytes);
srcdest decode_src3 (int sss, int bytes);
srcdest decode_dest2 (int dd, int bytes);

srcdest widen_sd (srcdest sd);
srcdest reg_sd (reg_id reg);

/* Mask has the one appropriate bit set.  */
srcdest decode_bit (int destcode);
srcdest decode_bit11 (int op0);
int get_bit (srcdest sd);
void put_bit (srcdest sd, int val);
int get_bit2 (srcdest sd, int bit);
void put_bit2 (srcdest sd, int bit, int val);

int get_src (srcdest sd);
void put_dest (srcdest sd, int value);

int condition_true (int cond_id);

#define FLAG(f) (regs.r_flags & f ? 1 : 0)
#define FLAG_C	FLAG(FLAGBIT_C)
#define FLAG_D	FLAG(FLAGBIT_D)
#define FLAG_Z	FLAG(FLAGBIT_Z)
#define FLAG_S	FLAG(FLAGBIT_S)
#define FLAG_B	FLAG(FLAGBIT_B)
#define FLAG_O	FLAG(FLAGBIT_O)
#define FLAG_I	FLAG(FLAGBIT_I)
#define FLAG_U	FLAG(FLAGBIT_U)

/* Instruction step return codes.
   Suppose one of the decode_* functions below returns a value R:
   - If M32C_STEPPED (R), then the single-step completed normally.
   - If M32C_HIT_BREAK (R), then the program hit a breakpoint.
   - If M32C_EXITED (R), then the program has done an 'exit' system
     call, and the exit code is M32C_EXIT_STATUS (R).
   - If M32C_STOPPED (R), then a signal (number M32C_STOP_SIG (R)) was
     generated.

   For building step return codes:
   - M32C_MAKE_STEPPED is the return code for finishing a normal step.
   - M32C_MAKE_HIT_BREAK is the return code for hitting a breakpoint.
   - M32C_MAKE_EXITED (C) is the return code for exiting with status C.
   - M32C_MAKE_STOPPED (S) is the return code for stopping on signal S.  */
#define M32C_MAKE_STEPPED()   (0)
#define M32C_MAKE_HIT_BREAK() (1)
#define M32C_MAKE_EXITED(c)   (((int) (c) << 8) + 2)
#define M32C_MAKE_STOPPED(s)  (((int) (s) << 8) + 3)

#define M32C_STEPPED(r)       ((r) == M32C_MAKE_STEPPED ())
#define M32C_HIT_BREAK(r)     ((r) == M32C_MAKE_HIT_BREAK ())
#define M32C_EXITED(r)        (((r) & 0xff) == 2)
#define M32C_EXIT_STATUS(r)   ((r) >> 8)
#define M32C_STOPPED(r)       (((r) & 0xff) == 3)
#define M32C_STOP_SIG(r)      ((r) >> 8)

/* The step result for the current step.  Global to allow
   communication between the stepping function and the system
   calls.  */
extern int step_result;

/* Used to detect heap/stack collisions */
extern unsigned int heaptop;
extern unsigned int heapbottom;

/* Points to one of the below functions, set by m32c_load().  */
extern int (*decode_opcode) ();

extern int decode_r8c ();
extern int decode_m32c ();

extern void trace_register_changes ();
