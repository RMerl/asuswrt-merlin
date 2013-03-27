/* Target-dependent code for GDB, the GNU debugger.

   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

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

#ifndef PPC_TDEP_H
#define PPC_TDEP_H

struct gdbarch;
struct frame_info;
struct value;
struct regcache;
struct type;

/* From ppc-linux-tdep.c... */
enum return_value_convention ppc_sysv_abi_return_value (struct gdbarch *gdbarch,
							struct type *valtype,
							struct regcache *regcache,
							gdb_byte *readbuf,
							const gdb_byte *writebuf);
enum return_value_convention ppc_sysv_abi_broken_return_value (struct gdbarch *gdbarch,
							       struct type *valtype,
							       struct regcache *regcache,
							       gdb_byte *readbuf,
							       const gdb_byte *writebuf);
CORE_ADDR ppc_sysv_abi_push_dummy_call (struct gdbarch *gdbarch,
					struct value *function,
					struct regcache *regcache,
					CORE_ADDR bp_addr, int nargs,
					struct value **args, CORE_ADDR sp,
					int struct_return,
					CORE_ADDR struct_addr);
CORE_ADDR ppc64_sysv_abi_push_dummy_call (struct gdbarch *gdbarch,
					  struct value *function,
					  struct regcache *regcache,
					  CORE_ADDR bp_addr, int nargs,
					  struct value **args, CORE_ADDR sp,
					  int struct_return,
					  CORE_ADDR struct_addr);
CORE_ADDR ppc64_sysv_abi_adjust_breakpoint_address (struct gdbarch *gdbarch,
						    CORE_ADDR bpaddr);
int ppc_linux_memory_remove_breakpoint (struct bp_target_info *bp_tgt);
struct link_map_offsets *ppc_linux_svr4_fetch_link_map_offsets (void);
const struct regset *ppc_linux_gregset (int);
const struct regset *ppc_linux_fpregset (void);

enum return_value_convention ppc64_sysv_abi_return_value (struct gdbarch *gdbarch,
							  struct type *valtype,
							  struct regcache *regcache,
							  gdb_byte *readbuf,
							  const gdb_byte *writebuf);

/* From rs6000-tdep.c... */
int altivec_register_p (int regno);
int spe_register_p (int regno);

/* Return non-zero if the architecture described by GDBARCH has
   floating-point registers (f0 --- f31 and fpscr).  */
int ppc_floating_point_unit_p (struct gdbarch *gdbarch);

/* Register set description.  */

struct ppc_reg_offsets
{
  /* General-purpose registers.  */
  int r0_offset;
  int gpr_size; /* size for r0-31, pc, ps, lr, ctr.  */
  int xr_size;  /* size for cr, xer, mq.  */
  int pc_offset;
  int ps_offset;
  int cr_offset;
  int lr_offset;
  int ctr_offset;
  int xer_offset;
  int mq_offset;

  /* Floating-point registers.  */
  int f0_offset;
  int fpscr_offset;
  int fpscr_size;

  /* AltiVec registers.  */
  int vr0_offset;
  int vscr_offset;
  int vrsave_offset;
};

/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

extern void ppc_supply_gregset (const struct regset *regset,
				struct regcache *regcache,
				int regnum, const void *gregs, size_t len);

/* Supply register REGNUM in the floating-point register set REGSET
   from the buffer specified by FPREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

extern void ppc_supply_fpregset (const struct regset *regset,
				 struct regcache *regcache,
				 int regnum, const void *fpregs, size_t len);

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

extern void ppc_collect_gregset (const struct regset *regset,
				 const struct regcache *regcache,
				 int regnum, void *gregs, size_t len);

/* Collect register REGNUM in the floating-point register set
   REGSET. from register cache REGCACHE into the buffer specified by
   FPREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

extern void ppc_collect_fpregset (const struct regset *regset,
				  const struct regcache *regcache,
				  int regnum, void *fpregs, size_t len);

/* Private data that this module attaches to struct gdbarch. */

struct gdbarch_tdep
  {
    int wordsize;              /* size in bytes of fixed-point word */
    const struct reg *regs;    /* from current variant */
    int ppc_gp0_regnum;		/* GPR register 0 */
    int ppc_toc_regnum;		/* TOC register */
    int ppc_ps_regnum;	        /* Processor (or machine) status (%msr) */
    int ppc_cr_regnum;		/* Condition register */
    int ppc_lr_regnum;		/* Link register */
    int ppc_ctr_regnum;		/* Count register */
    int ppc_xer_regnum;		/* Integer exception register */

    /* Not all PPC and RS6000 variants will have the registers
       represented below.  A -1 is used to indicate that the register
       is not present in this variant.  */

    /* Floating-point registers.  */
    int ppc_fp0_regnum;         /* floating-point register 0 */
    int ppc_fpscr_regnum;	/* fp status and condition register */

    /* Segment registers.  */
    int ppc_sr0_regnum;         /* segment register 0 */

    /* Multiplier-Quotient Register (older POWER architectures only).  */
    int ppc_mq_regnum;

    /* Altivec registers.  */
    int ppc_vr0_regnum;		/* First AltiVec register */
    int ppc_vrsave_regnum;	/* Last AltiVec register */

    /* SPE registers.  */
    int ppc_ev0_upper_regnum;   /* First GPR upper half register */
    int ppc_ev0_regnum;         /* First ev register */
    int ppc_ev31_regnum;        /* Last ev register */
    int ppc_acc_regnum;         /* SPE 'acc' register */
    int ppc_spefscr_regnum;     /* SPE 'spefscr' register */

    /* Offset to ABI specific location where link register is saved.  */
    int lr_frame_offset;	

    /* An array of integers, such that sim_regno[I] is the simulator
       register number for GDB register number I, or -1 if the
       simulator does not implement that register.  */
    int *sim_regno;

    /* Minimum possible text address.  */
    CORE_ADDR text_segment_base;

    /* ISA-specific types.  */
    struct type *ppc_builtin_type_vec64;
    struct type *ppc_builtin_type_vec128;
};


/* Constants for register set sizes.  */
enum
  {
    ppc_num_gprs = 32,          /* 32 general-purpose registers */
    ppc_num_fprs = 32,          /* 32 floating-point registers */
    ppc_num_srs = 16,           /* 16 segment registers */
    ppc_num_vrs = 32            /* 32 Altivec vector registers */
  };


/* Constants for SPR register numbers.  These are *not* GDB register
   numbers: they are the numbers used in the PowerPC ISA itself to
   refer to these registers.

   This table includes all the SPRs from all the variants I could find
   documentation for.

   There may be registers from different PowerPC variants assigned the
   same number, but that's fine: GDB and the SIM always use the
   numbers in the context of a particular variant, so it's not
   ambiguous.

   We need to deviate from the naming pattern when variants have
   special-purpose registers of the same name, but with different
   numbers.  Fortunately, this is rare: look below to see how we
   handle the 'tcr' registers on the 403/403GX and 602.  */

enum
  {
    ppc_spr_mq        =    0,
    ppc_spr_xer       =    1,
    ppc_spr_rtcu      =    4,
    ppc_spr_rtcl      =    5,
    ppc_spr_lr        =    8,
    ppc_spr_ctr       =    9,
    ppc_spr_cnt       =    9,
    ppc_spr_dsisr     =   18,
    ppc_spr_dar       =   19,
    ppc_spr_dec       =   22,
    ppc_spr_sdr1      =   25,
    ppc_spr_srr0      =   26,
    ppc_spr_srr1      =   27,
    ppc_spr_eie       =   80,
    ppc_spr_eid       =   81,
    ppc_spr_nri       =   82,
    ppc_spr_sp        =  102,
    ppc_spr_cmpa      =  144,
    ppc_spr_cmpb      =  145,
    ppc_spr_cmpc      =  146,
    ppc_spr_cmpd      =  147,
    ppc_spr_icr       =  148,
    ppc_spr_der       =  149,
    ppc_spr_counta    =  150,
    ppc_spr_countb    =  151,
    ppc_spr_cmpe      =  152,
    ppc_spr_cmpf      =  153,
    ppc_spr_cmpg      =  154,
    ppc_spr_cmph      =  155,
    ppc_spr_lctrl1    =  156,
    ppc_spr_lctrl2    =  157,
    ppc_spr_ictrl     =  158,
    ppc_spr_bar       =  159,
    ppc_spr_vrsave    =  256,
    ppc_spr_sprg0     =  272,
    ppc_spr_sprg1     =  273,
    ppc_spr_sprg2     =  274,
    ppc_spr_sprg3     =  275,
    ppc_spr_asr       =  280,
    ppc_spr_ear       =  282,
    ppc_spr_tbl       =  284,
    ppc_spr_tbu       =  285,
    ppc_spr_pvr       =  287,
    ppc_spr_spefscr   =  512,
    ppc_spr_ibat0u    =  528,
    ppc_spr_ibat0l    =  529,
    ppc_spr_ibat1u    =  530,
    ppc_spr_ibat1l    =  531,
    ppc_spr_ibat2u    =  532,
    ppc_spr_ibat2l    =  533,
    ppc_spr_ibat3u    =  534,
    ppc_spr_ibat3l    =  535,
    ppc_spr_dbat0u    =  536,
    ppc_spr_dbat0l    =  537,
    ppc_spr_dbat1u    =  538,
    ppc_spr_dbat1l    =  539,
    ppc_spr_dbat2u    =  540,
    ppc_spr_dbat2l    =  541,
    ppc_spr_dbat3u    =  542,
    ppc_spr_dbat3l    =  543,
    ppc_spr_ic_cst    =  560,
    ppc_spr_ic_adr    =  561,
    ppc_spr_ic_dat    =  562,
    ppc_spr_dc_cst    =  568,
    ppc_spr_dc_adr    =  569,
    ppc_spr_dc_dat    =  570,
    ppc_spr_dpdr      =  630,
    ppc_spr_dpir      =  631,
    ppc_spr_immr      =  638,
    ppc_spr_mi_ctr    =  784,
    ppc_spr_mi_ap     =  786,
    ppc_spr_mi_epn    =  787,
    ppc_spr_mi_twc    =  789,
    ppc_spr_mi_rpn    =  790,
    ppc_spr_mi_cam    =  816,
    ppc_spr_mi_ram0   =  817,
    ppc_spr_mi_ram1   =  818,
    ppc_spr_md_ctr    =  792,
    ppc_spr_m_casid   =  793,
    ppc_spr_md_ap     =  794,
    ppc_spr_md_epn    =  795,
    ppc_spr_m_twb     =  796,
    ppc_spr_md_twc    =  797,
    ppc_spr_md_rpn    =  798,
    ppc_spr_m_tw      =  799,
    ppc_spr_mi_dbcam  =  816,
    ppc_spr_mi_dbram0 =  817,
    ppc_spr_mi_dbram1 =  818,
    ppc_spr_md_dbcam  =  824,
    ppc_spr_md_cam    =  824,
    ppc_spr_md_dbram0 =  825,
    ppc_spr_md_ram0   =  825,
    ppc_spr_md_dbram1 =  826,
    ppc_spr_md_ram1   =  826,
    ppc_spr_ummcr0    =  936,
    ppc_spr_upmc1     =  937,
    ppc_spr_upmc2     =  938,
    ppc_spr_usia      =  939,
    ppc_spr_ummcr1    =  940,
    ppc_spr_upmc3     =  941,
    ppc_spr_upmc4     =  942,
    ppc_spr_zpr       =  944,
    ppc_spr_pid       =  945,
    ppc_spr_mmcr0     =  952,
    ppc_spr_pmc1      =  953,
    ppc_spr_sgr       =  953,
    ppc_spr_pmc2      =  954,
    ppc_spr_dcwr      =  954,
    ppc_spr_sia       =  955,
    ppc_spr_mmcr1     =  956,
    ppc_spr_pmc3      =  957,
    ppc_spr_pmc4      =  958,
    ppc_spr_sda       =  959,
    ppc_spr_tbhu      =  972,
    ppc_spr_tblu      =  973,
    ppc_spr_dmiss     =  976,
    ppc_spr_dcmp      =  977,
    ppc_spr_hash1     =  978,
    ppc_spr_hash2     =  979,
    ppc_spr_icdbdr    =  979,
    ppc_spr_imiss     =  980,
    ppc_spr_esr       =  980,
    ppc_spr_icmp      =  981,
    ppc_spr_dear      =  981,
    ppc_spr_rpa       =  982,
    ppc_spr_evpr      =  982,
    ppc_spr_cdbcr     =  983,
    ppc_spr_tsr       =  984,
    ppc_spr_602_tcr   =  984,
    ppc_spr_403_tcr   =  986,
    ppc_spr_ibr       =  986,
    ppc_spr_pit       =  987,
    ppc_spr_esasrr    =  988,
    ppc_spr_tbhi      =  988,
    ppc_spr_tblo      =  989,
    ppc_spr_srr2      =  990,
    ppc_spr_sebr      =  990,
    ppc_spr_srr3      =  991,
    ppc_spr_ser       =  991,
    ppc_spr_hid0      = 1008,
    ppc_spr_dbsr      = 1008,
    ppc_spr_hid1      = 1009,
    ppc_spr_iabr      = 1010,
    ppc_spr_dbcr      = 1010,
    ppc_spr_iac1      = 1012,
    ppc_spr_dabr      = 1013,
    ppc_spr_iac2      = 1013,
    ppc_spr_dac1      = 1014,
    ppc_spr_dac2      = 1015,
    ppc_spr_l2cr      = 1017,
    ppc_spr_dccr      = 1018,
    ppc_spr_ictc      = 1019,
    ppc_spr_iccr      = 1019,
    ppc_spr_thrm1     = 1020,
    ppc_spr_pbl1      = 1020,
    ppc_spr_thrm2     = 1021,
    ppc_spr_pbu1      = 1021,
    ppc_spr_thrm3     = 1022,
    ppc_spr_pbl2      = 1022,
    ppc_spr_fpecr     = 1022,
    ppc_spr_lt        = 1022,
    ppc_spr_pir       = 1023,
    ppc_spr_pbu2      = 1023
  };

/* Instruction size.  */
#define PPC_INSN_SIZE 4

/* Estimate for the maximum number of instrctions in a function epilogue.  */
#define PPC_MAX_EPILOGUE_INSTRUCTIONS  52

#endif /* ppc-tdep.h */
