/* Target-dependent code for the Xtensa port of GDB, the GNU debugger.

   Copyright (C) 2003, 2005, 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

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


/* XTENSA_TDEP_VERSION can/should be changed along with XTENSA_CONFIG_VERSION
   whenever the "tdep" structure changes in an incompatible way.  */

#define XTENSA_TDEP_VERSION 0x60

/*  Xtensa register type.  */

typedef enum 
{
  xtRegisterTypeArRegfile = 1,	/* Register File ar0..arXX.  */
  xtRegisterTypeSpecialReg,	/* CPU states, such as PS, Booleans, (rsr).  */
  xtRegisterTypeUserReg,	/* User defined registers (rur).  */
  xtRegisterTypeTieRegfile,	/* User define register files.  */
  xtRegisterTypeTieState,	/* TIE States (mapped on user regs).  */
  xtRegisterTypeMapped,		/* Mapped on Special Registers.  */
  xtRegisterTypeUnmapped,	/* Special case of masked registers.  */
  xtRegisterTypeWindow,		/* Live window registers (a0..a15).  */
  xtRegisterTypeVirtual,	/* PC, FP.  */
  xtRegisterTypeUnknown
} xtensa_register_type_t;


/*  Xtensa register group.  */

#define XTENSA_MAX_COPROCESSOR	0x08  /* Number of Xtensa coprocessors.  */

typedef enum 
{
  xtRegisterGroupUnknown = 0,
  xtRegisterGroupRegFile	= 0x0001,    /* Register files without ARx.  */
  xtRegisterGroupAddrReg	= 0x0002,    /* ARx.  */
  xtRegisterGroupSpecialReg	= 0x0004,    /* SRxx.  */
  xtRegisterGroupUserReg	= 0x0008,    /* URxx.  */
  xtRegisterGroupState 		= 0x0010,    /* States.  */

  xtRegisterGroupGeneral	= 0x0100,    /* General registers, Ax, SR.  */
  xtRegisterGroupUser		= 0x0200,    /* User registers.  */
  xtRegisterGroupFloat		= 0x0400,    /* Floating Point.  */
  xtRegisterGroupVectra		= 0x0800,    /* Vectra.  */
  xtRegisterGroupSystem		= 0x1000,    /* System.  */

  xtRegisterGroupCP0	    = 0x01000000,    /* CP0.  */
  xtRegisterGroupCP1	    = 0x02000000,    /* CP1.  */
  xtRegisterGroupCP2	    = 0x04000000,    /* CP2.  */
  xtRegisterGroupCP3	    = 0x08000000,    /* CP3.  */
  xtRegisterGroupCP4	    = 0x10000000,    /* CP4.  */
  xtRegisterGroupCP5	    = 0x20000000,    /* CP5.  */
  xtRegisterGroupCP6	    = 0x40000000,    /* CP6.  */
  xtRegisterGroupCP7	    = 0x80000000,    /* CP7.  */

} xtensa_register_group_t;


/*  Xtensa target flags.  */

typedef enum 
{
  xtTargetFlagsNonVisibleRegs	= 0x0001,
  xtTargetFlagsUseFetchStore	= 0x0002,
} xtensa_target_flags_t;


/* Xtensa ELF core file register set representation ('.reg' section). 
   Copied from target-side ELF header <xtensa/elf.h>.  */

typedef unsigned long xtensa_elf_greg_t;

typedef struct
{
  xtensa_elf_greg_t xchal_config_id0;
  xtensa_elf_greg_t xchal_config_id1;
  xtensa_elf_greg_t cpux;
  xtensa_elf_greg_t cpuy;
  xtensa_elf_greg_t pc;
  xtensa_elf_greg_t ps;
  xtensa_elf_greg_t exccause;
  xtensa_elf_greg_t excvaddr;
  xtensa_elf_greg_t windowbase;
  xtensa_elf_greg_t windowstart;
  xtensa_elf_greg_t lbeg;
  xtensa_elf_greg_t lend;
  xtensa_elf_greg_t lcount;
  xtensa_elf_greg_t sar;
  xtensa_elf_greg_t syscall;
  xtensa_elf_greg_t ar[0];	/* variable size (per config).  */
} xtensa_elf_gregset_t;

#define SIZEOF_GREGSET (sizeof (xtensa_elf_gregset_t) + NUM_AREGS * 4)
#define XTENSA_ELF_NGREG (SIZEOF_GREGSET / sizeof(xtensa_elf_greg_t))


/*  Mask.  */

typedef struct 
{
  int reg_num;
  int bit_start;
  int bit_size;
} xtensa_reg_mask_t;

typedef struct 
{
  int count;
  xtensa_reg_mask_t *mask;
} xtensa_mask_t;


/*  Xtensa register representation.  */

typedef struct 
{
  char* name;             	/* Register name.  */
  int offset;             	/* Offset.  */
  xtensa_register_type_t type;  /* Register type.  */
  xtensa_register_group_t group;/* Register group.  */
  struct type* ctype;		/* C-type.  */
  int bit_size;  		/* The actual bit size in the target.  */
  int byte_size;          	/* Actual space allocated in registers[].  */
  int align;			/* Alignment for this register.  */

  unsigned int target_number;	/* Register target number.  */

  int flags;			/* Flags.  */

  const xtensa_mask_t *mask;	/* Register is a compilation of other regs.  */
  const char *fetch;		/* Instruction sequence to fetch register.  */
  const char *store;		/* Instruction sequence to store register.  */
} xtensa_register_t;


#define XTENSA_REGISTER_FLAGS_PRIVILEDGED	0x0001
#define XTENSA_REGISTER_FLAGS_READABLE		0x0002
#define XTENSA_REGISTER_FLAGS_WRITABLE		0x0004
#define XTENSA_REGISTER_FLAGS_VOLATILE		0x0008


/*  Call-ABI for stack frame.  */

typedef enum 
{
  CallAbiDefault = 0,		/* Any 'callX' instructions; default stack.  */
  CallAbiCall0Only,		/* Only 'call0' instructions; flat stack.  */
} call_abi_t;


/*  Xtensa-specific target dependencies.  */

struct gdbarch_tdep
{
  unsigned int target_flags;

  /* Spill location for TIE register files under ocd.  */

  unsigned int spill_location;
  unsigned int spill_size;

  char *unused;				/* Placeholder for compatibility.  */
  call_abi_t call_abi;			/* Calling convention.  */

  /* CPU configuration.  */

  unsigned int debug_interrupt_level;

  unsigned int icache_line_bytes;
  unsigned int dcache_line_bytes;
  unsigned int dcache_writeback;

  unsigned int isa_use_windowed_registers;
  unsigned int isa_use_density_instructions;
  unsigned int isa_use_exceptions;
  unsigned int isa_use_ext_l32r;
  unsigned int isa_max_insn_size;	/* Maximum instruction length.  */
  unsigned int debug_num_ibreaks;	/* Number of IBREAKs.  */
  unsigned int debug_num_dbreaks;

  /* Register map.  */

  xtensa_register_t* regmap;

  unsigned int num_regs;	/* Number of registers in regmap.  */
  unsigned int num_pseudo_regs;	/* Number of pseudo registers.  */
  unsigned int num_aregs;	/* Size of register file.  */
  unsigned int num_contexts;

  int ar_base;			/* Register number for AR0.  */
  int a0_base;			/* Register number for A0 (pseudo).  */
  int wb_regnum;		/* Register number for WB.  */
  int ws_regnum;		/* Register number for WS.  */
  int pc_regnum;		/* Register number for PC.  */
  int ps_regnum;		/* Register number for PS.  */
  int lbeg_regnum;		/* Register numbers for count regs.  */
  int lend_regnum;
  int lcount_regnum;
  int sar_regnum;		/* Register number of SAR.  */
  int litbase_regnum;		/* Register number of LITBASE.  */

  int interrupt_regnum;		/* Register number for interrupt.  */
  int interrupt2_regnum;	/* Register number for interrupt2.  */
  int cpenable_regnum;		/* Register number for cpenable.  */
  int debugcause_regnum;	/* Register number for debugcause.  */
  int exccause_regnum;		/* Register number for exccause.  */
  int excvaddr_regnum;		/* Register number for excvaddr.  */

  int max_register_raw_size;
  int max_register_virtual_size;
  unsigned long *fp_layout;	/* Layout of custom/TIE regs in 'FP' area.  */
  unsigned int fp_layout_bytes;	/* Size of layout information (in bytes).  */
  unsigned long *gregmap;
};


/* Define macros to access some of the gdbarch entries.  */
#define XTENSA_TARGET_FLAGS \
  (gdbarch_tdep (current_gdbarch)->target_flags)
#define SPILL_LOCATION \
  (gdbarch_tdep (current_gdbarch)->spill_location)
#define SPILL_SIZE \
  (gdbarch_tdep (current_gdbarch)->spill_size)
#define CALL_ABI		\
  (gdbarch_tdep (current_gdbarch)->call_abi)
#define ISA_USE_WINDOWED_REGISTERS \
  (gdbarch_tdep (current_gdbarch)->isa_use_windowed_registers)
#define ISA_USE_DENSITY_INSTRUCTIONS \
  (gdbarch_tdep (current_gdbarch)->isa_use_density_instructions)
#define ISA_USE_EXCEPTIONS \
  (gdbarch_tdep (current_gdbarch)->isa_use_exceptions)
#define ISA_USE_EXT_L32R \
  (gdbarch_tdep (current_gdbarch)->isa_use_ext_l32r)
#define DEBUG_DATA_VADDR_TRAP_COUNT \
  (gdbarch_tdep (current_gdbarch)->debug_data_vaddr_trap_count)
#define DEBUG_INST_VADDR_TRAP_COUNT \
  (gdbarch_tdep (current_gdbarch)->debug_inst_vaddr_trap_count)
#define ISA_MAX_INSN_SIZE \
  (gdbarch_tdep (current_gdbarch)->isa_max_insn_size)
#define DEBUG_NUM_IBREAKS \
  (gdbarch_tdep (current_gdbarch)->debug_num_ibreaks)
#define DEBUG_NUM_DBREAKS \
  (gdbarch_tdep (current_gdbarch)->debug_num_dbreaks)

#define NUM_AREGS         (gdbarch_tdep (current_gdbarch)->num_aregs)
#define WB_REGNUM         (gdbarch_tdep (current_gdbarch)->wb_regnum)
#define WS_REGNUM         (gdbarch_tdep (current_gdbarch)->ws_regnum)
#define LBEG_REGNUM       (gdbarch_tdep (current_gdbarch)->lbeg_regnum)
#define LEND_REGNUM       (gdbarch_tdep (current_gdbarch)->lend_regnum)
#define LCOUNT_REGNUM     (gdbarch_tdep (current_gdbarch)->lcount_regnum)
#define SAR_REGNUM        (gdbarch_tdep (current_gdbarch)->sar_regnum)
#define REGMAP            (gdbarch_tdep (current_gdbarch)->regmap)

#define LITBASE_REGNUM    (gdbarch_tdep (current_gdbarch)->litbase_regnum)
#define DEBUGCAUSE_REGNUM (gdbarch_tdep (current_gdbarch)->debugcause_regnum)
#define EXCCAUSE_REGNUM   (gdbarch_tdep (current_gdbarch)->exccause_regnum)
#define EXCVADDR_REGNUM   (gdbarch_tdep (current_gdbarch)->excvaddr_regnum)
#define NUM_IBREAKS       (gdbarch_tdep (current_gdbarch)->num_ibreaks)
#define REGMAP_BYTES      (gdbarch_tdep (current_gdbarch)->regmap_bytes)
#define A0_BASE           (gdbarch_tdep (current_gdbarch)->a0_base)
#define AR_BASE           (gdbarch_tdep (current_gdbarch)->ar_base)
#define FP_ALIAS \
  (gdbarch_num_regs (current_gdbarch) \
   + gdbarch_num_pseudo_regs (current_gdbarch))
#define CALL_ABI          (gdbarch_tdep (current_gdbarch)->call_abi)
#define NUM_CONTEXTS      (gdbarch_tdep (current_gdbarch)->num_contexts)
  
#define FP_LAYOUT         (gdbarch_tdep (current_gdbarch)->fp_layout)
#define FP_LAYOUT_BYTES   (gdbarch_tdep (current_gdbarch)->fp_layout_bytes)
#define GREGMAP           (gdbarch_tdep (current_gdbarch)->gregmap)

#define AREGS_MASK	  (NUM_AREGS - 1)
#define WB_MASK		  (AREGS_MASK >> 2)
#define WB_SHIFT	  2

/* We assign fixed numbers to the registers of the "current" window 
   (i.e., relative to WB).  The registers get remapped via the reg_map 
   data structure to their corresponding register in the AR register 
   file (see xtensa-tdep.c).  */

#define A0_REGNUM  (A0_BASE + 0)
#define A1_REGNUM  (A0_BASE + 1)
#define A2_REGNUM  (A0_BASE + 2)
#define A3_REGNUM  (A0_BASE + 3)
#define A4_REGNUM  (A0_BASE + 4)
#define A5_REGNUM  (A0_BASE + 5)
#define A6_REGNUM  (A0_BASE + 6)
#define A7_REGNUM  (A0_BASE + 7)
#define A8_REGNUM  (A0_BASE + 8)
#define A9_REGNUM  (A0_BASE + 9)
#define A10_REGNUM (A0_BASE + 10)
#define A11_REGNUM (A0_BASE + 11)
#define A12_REGNUM (A0_BASE + 12)
#define A13_REGNUM (A0_BASE + 13)
#define A14_REGNUM (A0_BASE + 14)
#define A15_REGNUM (A0_BASE + 15)

