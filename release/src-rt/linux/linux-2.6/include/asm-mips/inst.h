/*
 * Format of an instruction in memory.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 2000 by Ralf Baechle
 * Copyright (C) 2006 by Thiemo Seufer
 */
#ifndef _ASM_INST_H
#define _ASM_INST_H

/*
 * Major opcodes; before MIPS IV cop1x was called cop3.
 */
enum major_op {
	spec_op, bcond_op, j_op, jal_op,
	beq_op, bne_op, blez_op, bgtz_op,
	addi_op, addiu_op, slti_op, sltiu_op,
	andi_op, ori_op, xori_op, lui_op,
	cop0_op, cop1_op, cop2_op, cop1x_op,
	beql_op, bnel_op, blezl_op, bgtzl_op,
	daddi_op, daddiu_op, ldl_op, ldr_op,
	spec2_op, jalx_op, mdmx_op, spec3_op,
	lb_op, lh_op, lwl_op, lw_op,
	lbu_op, lhu_op, lwr_op, lwu_op,
	sb_op, sh_op, swl_op, sw_op,
	sdl_op, sdr_op, swr_op, cache_op,
	ll_op, lwc1_op, lwc2_op, pref_op,
	lld_op, ldc1_op, ldc2_op, ld_op,
	sc_op, swc1_op, swc2_op, major_3b_op,
	scd_op, sdc1_op, sdc2_op, sd_op
};

/*
 * func field of spec opcode.
 */
enum spec_op {
	sll_op, movc_op, srl_op, sra_op,
	sllv_op, pmon_op, srlv_op, srav_op,
	jr_op, jalr_op, movz_op, movn_op,
	syscall_op, break_op, spim_op, sync_op,
	mfhi_op, mthi_op, mflo_op, mtlo_op,
	dsllv_op, spec2_unused_op, dsrlv_op, dsrav_op,
	mult_op, multu_op, div_op, divu_op,
	dmult_op, dmultu_op, ddiv_op, ddivu_op,
	add_op, addu_op, sub_op, subu_op,
	and_op, or_op, xor_op, nor_op,
	spec3_unused_op, spec4_unused_op, slt_op, sltu_op,
	dadd_op, daddu_op, dsub_op, dsubu_op,
	tge_op, tgeu_op, tlt_op, tltu_op,
	teq_op, spec5_unused_op, tne_op, spec6_unused_op,
	dsll_op, spec7_unused_op, dsrl_op, dsra_op,
	dsll32_op, spec8_unused_op, dsrl32_op, dsra32_op
};

/*
 * func field of spec2 opcode.
 */
enum spec2_op {
	madd_op, maddu_op, mul_op, spec2_3_unused_op,
	msub_op, msubu_op, /* more unused ops */
	clz_op = 0x20, clo_op,
	dclz_op = 0x24, dclo_op,
	sdbpp_op = 0x3f
};

/*
 * func field of spec3 opcode.
 */
enum spec3_op {
	ext_op, dextm_op, dextu_op, dext_op,
	ins_op, dinsm_op, dinsu_op, dins_op,
	bshfl_op = 0x20,
	dbshfl_op = 0x24,
	rdhwr_op = 0x3b
};

/*
 * rt field of bcond opcodes.
 */
enum rt_op {
	bltz_op, bgez_op, bltzl_op, bgezl_op,
	spimi_op, unused_rt_op_0x05, unused_rt_op_0x06, unused_rt_op_0x07,
	tgei_op, tgeiu_op, tlti_op, tltiu_op,
	teqi_op, unused_0x0d_rt_op, tnei_op, unused_0x0f_rt_op,
	bltzal_op, bgezal_op, bltzall_op, bgezall_op,
	rt_op_0x14, rt_op_0x15, rt_op_0x16, rt_op_0x17,
	rt_op_0x18, rt_op_0x19, rt_op_0x1a, rt_op_0x1b,
	bposge32_op, rt_op_0x1d, rt_op_0x1e, rt_op_0x1f
};

/*
 * rs field of cop opcodes.
 */
enum cop_op {
	mfc_op        = 0x00, dmfc_op       = 0x01,
	cfc_op        = 0x02, mtc_op        = 0x04,
	dmtc_op       = 0x05, ctc_op        = 0x06,
	bc_op         = 0x08, cop_op        = 0x10,
	copm_op       = 0x18
};

/*
 * rt field of cop.bc_op opcodes
 */
enum bcop_op {
	bcf_op, bct_op, bcfl_op, bctl_op
};

/*
 * func field of cop0 coi opcodes.
 */
enum cop0_coi_func {
	tlbr_op       = 0x01, tlbwi_op      = 0x02,
	tlbwr_op      = 0x06, tlbp_op       = 0x08,
	rfe_op        = 0x10, eret_op       = 0x18
};

/*
 * func field of cop0 com opcodes.
 */
enum cop0_com_func {
	tlbr1_op      = 0x01, tlbw_op       = 0x02,
	tlbp1_op      = 0x08, dctr_op       = 0x09,
	dctw_op       = 0x0a
};

/*
 * fmt field of cop1 opcodes.
 */
enum cop1_fmt {
	s_fmt, d_fmt, e_fmt, q_fmt,
	w_fmt, l_fmt
};

/*
 * func field of cop1 instructions using d, s or w format.
 */
enum cop1_sdw_func {
	fadd_op      =  0x00, fsub_op      =  0x01,
	fmul_op      =  0x02, fdiv_op      =  0x03,
	fsqrt_op     =  0x04, fabs_op      =  0x05,
	fmov_op      =  0x06, fneg_op      =  0x07,
	froundl_op   =  0x08, ftruncl_op   =  0x09,
	fceill_op    =  0x0a, ffloorl_op   =  0x0b,
	fround_op    =  0x0c, ftrunc_op    =  0x0d,
	fceil_op     =  0x0e, ffloor_op    =  0x0f,
	fmovc_op     =  0x11, fmovz_op     =  0x12,
	fmovn_op     =  0x13, frecip_op    =  0x15,
	frsqrt_op    =  0x16, fcvts_op     =  0x20,
	fcvtd_op     =  0x21, fcvte_op     =  0x22,
	fcvtw_op     =  0x24, fcvtl_op     =  0x25,
	fcmp_op      =  0x30
};

/*
 * func field of cop1x opcodes (MIPS IV).
 */
enum cop1x_func {
	lwxc1_op     =  0x00, ldxc1_op     =  0x01,
	pfetch_op    =  0x07, swxc1_op     =  0x08,
	sdxc1_op     =  0x09, madd_s_op    =  0x20,
	madd_d_op    =  0x21, madd_e_op    =  0x22,
	msub_s_op    =  0x28, msub_d_op    =  0x29,
	msub_e_op    =  0x2a, nmadd_s_op   =  0x30,
	nmadd_d_op   =  0x31, nmadd_e_op   =  0x32,
	nmsub_s_op   =  0x38, nmsub_d_op   =  0x39,
	nmsub_e_op   =  0x3a
};

/*
 * func field for mad opcodes (MIPS IV).
 */
enum mad_func {
	madd_fp_op      = 0x08, msub_fp_op      = 0x0a,
	nmadd_fp_op     = 0x0c, nmsub_fp_op     = 0x0e
};

/*
 * Damn ...  bitfields depend from byteorder :-(
 */
#ifdef __MIPSEB__
struct j_format {	/* Jump format */
	unsigned int opcode : 6;
	unsigned int target : 26;
};

struct i_format {	/* Immediate format (addi, lw, ...) */
	unsigned int opcode : 6;
	unsigned int rs : 5;
	unsigned int rt : 5;
	signed int simmediate : 16;
};

struct u_format {	/* Unsigned immediate format (ori, xori, ...) */
	unsigned int opcode : 6;
	unsigned int rs : 5;
	unsigned int rt : 5;
	unsigned int uimmediate : 16;
};

struct c_format {	/* Cache (>= R6000) format */
	unsigned int opcode : 6;
	unsigned int rs : 5;
	unsigned int c_op : 3;
	unsigned int cache : 2;
	unsigned int simmediate : 16;
};

struct r_format {	/* Register format */
	unsigned int opcode : 6;
	unsigned int rs : 5;
	unsigned int rt : 5;
	unsigned int rd : 5;
	unsigned int re : 5;
	unsigned int func : 6;
};

struct p_format {	/* Performance counter format (R10000) */
	unsigned int opcode : 6;
	unsigned int rs : 5;
	unsigned int rt : 5;
	unsigned int rd : 5;
	unsigned int re : 5;
	unsigned int func : 6;
};

struct f_format {	/* FPU register format */
	unsigned int opcode : 6;
	unsigned int : 1;
	unsigned int fmt : 4;
	unsigned int rt : 5;
	unsigned int rd : 5;
	unsigned int re : 5;
	unsigned int func : 6;
};

struct ma_format {	/* FPU multipy and add format (MIPS IV) */
	unsigned int opcode : 6;
	unsigned int fr : 5;
	unsigned int ft : 5;
	unsigned int fs : 5;
	unsigned int fd : 5;
	unsigned int func : 4;
	unsigned int fmt : 2;
};

#elif defined(__MIPSEL__)

struct j_format {	/* Jump format */
	unsigned int target : 26;
	unsigned int opcode : 6;
};

struct i_format {	/* Immediate format */
	signed int simmediate : 16;
	unsigned int rt : 5;
	unsigned int rs : 5;
	unsigned int opcode : 6;
};

struct u_format {	/* Unsigned immediate format */
	unsigned int uimmediate : 16;
	unsigned int rt : 5;
	unsigned int rs : 5;
	unsigned int opcode : 6;
};

struct c_format {	/* Cache (>= R6000) format */
	unsigned int simmediate : 16;
	unsigned int cache : 2;
	unsigned int c_op : 3;
	unsigned int rs : 5;
	unsigned int opcode : 6;
};

struct r_format {	/* Register format */
	unsigned int func : 6;
	unsigned int re : 5;
	unsigned int rd : 5;
	unsigned int rt : 5;
	unsigned int rs : 5;
	unsigned int opcode : 6;
};

struct p_format {	/* Performance counter format (R10000) */
	unsigned int func : 6;
	unsigned int re : 5;
	unsigned int rd : 5;
	unsigned int rt : 5;
	unsigned int rs : 5;
	unsigned int opcode : 6;
};

struct f_format {	/* FPU register format */
	unsigned int func : 6;
	unsigned int re : 5;
	unsigned int rd : 5;
	unsigned int rt : 5;
	unsigned int fmt : 4;
	unsigned int : 1;
	unsigned int opcode : 6;
};

struct ma_format {	/* FPU multipy and add format (MIPS IV) */
	unsigned int fmt : 2;
	unsigned int func : 4;
	unsigned int fd : 5;
	unsigned int fs : 5;
	unsigned int ft : 5;
	unsigned int fr : 5;
	unsigned int opcode : 6;
};

#else /* !defined (__MIPSEB__) && !defined (__MIPSEL__) */
#error "MIPS but neither __MIPSEL__ nor __MIPSEB__?"
#endif

union mips_instruction {
	unsigned int word;
	unsigned short halfword[2];
	unsigned char byte[4];
	struct j_format j_format;
	struct i_format i_format;
	struct u_format u_format;
	struct c_format c_format;
	struct r_format r_format;
	struct f_format f_format;
        struct ma_format ma_format;
};

/* HACHACHAHCAHC ...  */

/* In case some other massaging is needed, keep MIPSInst as wrapper */

#define MIPSInst(x) x

#define I_OPCODE_SFT	26
#define MIPSInst_OPCODE(x) (MIPSInst(x) >> I_OPCODE_SFT)

#define I_JTARGET_SFT	0
#define MIPSInst_JTARGET(x) (MIPSInst(x) & 0x03ffffff)

#define I_RS_SFT	21
#define MIPSInst_RS(x) ((MIPSInst(x) & 0x03e00000) >> I_RS_SFT)

#define I_RT_SFT	16
#define MIPSInst_RT(x) ((MIPSInst(x) & 0x001f0000) >> I_RT_SFT)

#define I_IMM_SFT	0
#define MIPSInst_SIMM(x) ((int)((short)(MIPSInst(x) & 0xffff)))
#define MIPSInst_UIMM(x) (MIPSInst(x) & 0xffff)

#define I_CACHEOP_SFT	18
#define MIPSInst_CACHEOP(x) ((MIPSInst(x) & 0x001c0000) >> I_CACHEOP_SFT)

#define I_CACHESEL_SFT	16
#define MIPSInst_CACHESEL(x) ((MIPSInst(x) & 0x00030000) >> I_CACHESEL_SFT)

#define I_RD_SFT	11
#define MIPSInst_RD(x) ((MIPSInst(x) & 0x0000f800) >> I_RD_SFT)

#define I_RE_SFT	6
#define MIPSInst_RE(x) ((MIPSInst(x) & 0x000007c0) >> I_RE_SFT)

#define I_FUNC_SFT	0
#define MIPSInst_FUNC(x) (MIPSInst(x) & 0x0000003f)

#define I_FFMT_SFT	21
#define MIPSInst_FFMT(x) ((MIPSInst(x) & 0x01e00000) >> I_FFMT_SFT)

#define I_FT_SFT	16
#define MIPSInst_FT(x) ((MIPSInst(x) & 0x001f0000) >> I_FT_SFT)

#define I_FS_SFT	11
#define MIPSInst_FS(x) ((MIPSInst(x) & 0x0000f800) >> I_FS_SFT)

#define I_FD_SFT	6
#define MIPSInst_FD(x) ((MIPSInst(x) & 0x000007c0) >> I_FD_SFT)

#define I_FR_SFT	21
#define MIPSInst_FR(x) ((MIPSInst(x) & 0x03e00000) >> I_FR_SFT)

#define I_FMA_FUNC_SFT	2
#define MIPSInst_FMA_FUNC(x) ((MIPSInst(x) & 0x0000003c) >> I_FMA_FUNC_SFT)

#define I_FMA_FFMT_SFT	0
#define MIPSInst_FMA_FFMT(x) (MIPSInst(x) & 0x00000003)

typedef unsigned int mips_instruction;

#endif /* _ASM_INST_H */
