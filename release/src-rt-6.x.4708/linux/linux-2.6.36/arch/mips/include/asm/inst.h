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
	lx_op = 0x0a,
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
	rfe_op        = 0x10, eret_op       = 0x18,
	iret_op       = 0x38
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
 * func field for special3 lx opcodes (Cavium Octeon).
 */
enum lx_func {
	lwx_op	= 0x00,
	lhx_op	= 0x04,
	lbux_op	= 0x06,
	ldx_op	= 0x08,
	lwux_op	= 0x10,
	lhux_op	= 0x14,
	lbx_op	= 0x16,
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

struct b_format {	/* BREAK and SYSCALL */
	unsigned int opcode:6;
	unsigned int code:20;
	unsigned int func:6;
};

struct fb_format {	/* FPU branch format */
	unsigned int opcode:6;
	unsigned int bc:5;
	unsigned int cc:3;
	unsigned int flag:2;
	unsigned int simmediate:16;
};

struct fp0_format {      /* FPU multipy and add format (MIPS32) */
	unsigned int opcode:6;
	unsigned int fmt:5;
	unsigned int ft:5;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int func:6;
};

struct mm_fp0_format {      /* FPU multipy and add format (micro_mips) */
	unsigned int opcode:6;
	unsigned int ft:5;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int fmt:3;
	unsigned int op:2;
	unsigned int func:6;
};

struct fp1_format {      /* FPU mfc1 and cfc1 format (MIPS32) */
	unsigned int opcode:6;
	unsigned int op:5;
	unsigned int rt:5;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int func:6;
};

struct mm_fp1_format {      /* FPU mfc1 and cfc1 format (micro_mips) */
	unsigned int opcode:6;
	unsigned int rt:5;
	unsigned int fs:5;
	unsigned int fmt:2;
	unsigned int op:8;
	unsigned int func:6;
};

struct mm_fp2_format {      /* FPU movt and movf format (micro_mips) */
	unsigned int opcode:6;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int cc:3;
	unsigned int zero:2;
	unsigned int fmt:2;
	unsigned int op:3;
	unsigned int func:6;
};

struct mm_fp3_format {      /* FPU abs and neg format (micro_mips) */
	unsigned int opcode:6;
	unsigned int rt:5;
	unsigned int fs:5;
	unsigned int fmt:3;
	unsigned int op:7;
	unsigned int func:6;
};

struct mm_fp4_format {      /* FPU c.cond format (micro_mips) */
	unsigned int opcode:6;
	unsigned int rt:5;
	unsigned int fs:5;
	unsigned int cc:3;
	unsigned int fmt:3;
	unsigned int cond:4;
	unsigned int func:6;
};

struct mm_fp5_format {      /* FPU lwxc1 and swxc1 format (micro_mips) */
	unsigned int opcode:6;
	unsigned int index:5;
	unsigned int base:5;
	unsigned int fd:5;
	unsigned int op:5;
	unsigned int func:6;
};

struct fp6_format {	/* FPU madd and msub format (MIPS IV) */
	unsigned int opcode:6;
	unsigned int fr:5;
	unsigned int ft:5;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int func:6;
};

struct mm_fp6_format {	/* FPU madd and msub format (micro_mips) */
	unsigned int opcode:6;
	unsigned int ft:5;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int fr:5;
	unsigned int func:6;
};

struct mm16b1_format {		/* micro_mips 16-bit branch format */
	unsigned int opcode:6;
	unsigned int rs:3;
	signed int simmediate:7;
	unsigned int duplicate:16;	/* a copy of the instn */
};

struct mm16b0_format {		/* micro_mips 16-bit branch format */
	unsigned int opcode:6;
	signed int simmediate:10;
	unsigned int duplicate:16;	/* a copy of the instn */
};

struct mm_i_format {		/* Immediate format (addi, lw, ...) */
	unsigned int opcode:6;
	unsigned int rt:5;
	unsigned int rs:5;
	signed int simmediate:16;
};

/*  MIPS16e */

struct rr {
	unsigned int opcode:5;
	unsigned int rx:3;
	unsigned int nd:1;
	unsigned int l:1;
	unsigned int ra:1;
	unsigned int func:5;
};

struct jal {
	unsigned int opcode:5;
	unsigned int x:1;
	unsigned int imm20_16:5;
	signed int imm25_21:5;
	/* unsigned int    imm20_15:0;  here is only first 16bits in first HW */
};

struct i64 {
	unsigned int opcode:5;
	unsigned int func:3;
	unsigned int imm:8;
};

struct ri64 {
	unsigned int opcode:5;
	unsigned int func:3;
	unsigned int ry:3;
	unsigned int imm:5;
};

struct ri {
	unsigned int opcode:5;
	unsigned int rx:3;
	unsigned int imm:8;
};

struct rri {
	unsigned int opcode:5;
	unsigned int rx:3;
	unsigned int ry:3;
	unsigned int imm:5;
};

struct i8 {
	unsigned int opcode:5;
	unsigned int func:3;
	unsigned int imm:8;
};

struct mm_m_format {
	unsigned int opcode:6;
	unsigned int rd:5;
	unsigned int base:5;
	unsigned int func:4;
	signed int simmediate:12;
};

struct mm_x_format {
	unsigned int opcode:6;
	unsigned int index:5;
	unsigned int base:5;
	unsigned int rd:5;
	unsigned int func:11;
};

struct mm16_m_format {
	unsigned int opcode:6;
	unsigned int func:4;
	unsigned int rlist:2;
	unsigned int imm:4;
	unsigned int duplicate:16;	/* a copy of the instn */
};

struct mm16_rb_format {
	unsigned int opcode:6;
	unsigned int rt:3;
	unsigned int base:3;
	signed int simmediate:4;
	unsigned int duplicate:16;	/* a copy of the instn */
};

struct mm16_r5_format {
	unsigned int opcode:6;
	unsigned int rt:5;
	signed int simmediate:5;
	unsigned int duplicate:16;	/* a copy of the instn */
};

struct mm16_r3_format {
	unsigned int opcode:6;
	unsigned int rt:3;
	signed int simmediate:7;
	unsigned int duplicate:16;	/* a copy of the instn */
};

#elif defined(__MIPSEL__)

struct j_format {		/* Jump format */
	unsigned int target:26;
	unsigned int opcode:6;
};

struct i_format {		/* Immediate format */
	signed int simmediate:16;
	unsigned int rt:5;
	unsigned int rs:5;
	unsigned int opcode:6;
};

struct u_format {		/* Unsigned immediate format */
	unsigned int uimmediate:16;
	unsigned int rt:5;
	unsigned int rs:5;
	unsigned int opcode:6;
};

struct c_format {		/* Cache (>= R6000) format */
	unsigned int simmediate:16;
	unsigned int cache:2;
	unsigned int c_op:3;
	unsigned int rs:5;
	unsigned int opcode:6;
};

struct r_format {		/* Register format */
	unsigned int func:6;
	unsigned int re:5;
	unsigned int rd:5;
	unsigned int rt:5;
	unsigned int rs:5;
	unsigned int opcode:6;
};

struct p_format {		/* Performance counter format (R10000) */
	unsigned int func:6;
	unsigned int re:5;
	unsigned int rd:5;
	unsigned int rt:5;
	unsigned int rs:5;
	unsigned int opcode:6;
};

struct f_format {		/* FPU register format */
	unsigned int func:6;
	unsigned int re:5;
	unsigned int rd:5;
	unsigned int rt:5;
	unsigned int fmt:4;
	unsigned int:1;
	unsigned int opcode:6;
};

struct ma_format {		/* FPU multipy and add format (MIPS IV) */
	unsigned int fmt:2;
	unsigned int func:4;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int ft:5;
	unsigned int fr:5;
	unsigned int opcode:6;
};

struct b_format {		/* BREAK and SYSCALL */
	unsigned int func:6;
	unsigned int code:20;
	unsigned int opcode:6;
};

struct fb_format {		/* FPU branch format */
	unsigned int simmediate:16;
	unsigned int flag:2;
	unsigned int cc:3;
	unsigned int bc:5;
	unsigned int opcode:6;
};

struct fp0_format {		/* FPU multipy and add format (MIPS32) */
	unsigned int func:6;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int ft:5;
	unsigned int fmt:5;
	unsigned int opcode:6;
};

struct mm_fp0_format {		/* FPU multipy and add format (micro_mips) */
	unsigned int func:6;
	unsigned int op:2;
	unsigned int fmt:3;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int ft:5;
	unsigned int opcode:6;
};

struct fp1_format {		/* FPU mfc1 and cfc1 format (MIPS32) */
	unsigned int func:6;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int rt:5;
	unsigned int op:5;
	unsigned int opcode:6;
};

struct mm_fp1_format {		/* FPU mfc1 and cfc1 format (micro_mips) */
	unsigned int func:6;
	unsigned int op:8;
	unsigned int fmt:2;
	unsigned int fs:5;
	unsigned int rt:5;
	unsigned int opcode:6;
};

struct mm_fp2_format {		/* FPU movt and movf format (micro_mips) */
	unsigned int func:6;
	unsigned int op:3;
	unsigned int fmt:2;
	unsigned int zero:2;
	unsigned int cc:3;
	unsigned int fs:5;
	unsigned int fd:5;
	unsigned int opcode:6;
};

struct mm_fp3_format {		/* FPU abs and neg format (micro_mips) */
	unsigned int func:6;
	unsigned int op:7;
	unsigned int fmt:3;
	unsigned int fs:5;
	unsigned int rt:5;
	unsigned int opcode:6;
};

struct mm_fp4_format {		/* FPU c.cond format (micro_mips) */
	unsigned int func:6;
	unsigned int cond:4;
	unsigned int fmt:3;
	unsigned int cc:3;
	unsigned int fs:5;
	unsigned int rt:5;
	unsigned int opcode:6;
};

struct mm_fp5_format {		/* FPU lwxc1 and swxc1 format (micro_mips) */
	unsigned int func:6;
	unsigned int op:5;
	unsigned int fd:5;
	unsigned int base:5;
	unsigned int index:5;
	unsigned int opcode:6;
};

struct fp6_format {		/* FPU madd and msub format (MIPS IV) */
	unsigned int func:6;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int ft:5;
	unsigned int fr:5;
	unsigned int opcode:6;
};

struct mm_fp6_format {		/* FPU madd and msub format (micro_mips) */
	unsigned int func:6;
	unsigned int fr:5;
	unsigned int fd:5;
	unsigned int fs:5;
	unsigned int ft:5;
	unsigned int opcode:6;
};

struct mm16b1_format {		/* micro_mips 16-bit branch format */
	unsigned int duplicate:16;	/* a copy of the instn */
	signed int simmediate:7;
	unsigned int rs:3;
	unsigned int opcode:6;
};

struct mm16b0_format {		/* micro_mips 16-bit branch format */
	unsigned int duplicate:16;	/* a copy of the instn */
	signed int simmediate:10;
	unsigned int opcode:6;
};

struct mm_i_format {		/* Immediate format */
	signed int simmediate:16;
	unsigned int rs:5;
	unsigned int rt:5;
	unsigned int opcode:6;
};

/*  MIPS16e */

struct rr {
	unsigned int func:5;
	unsigned int ra:1;
	unsigned int l:1;
	unsigned int nd:1;
	unsigned int rx:3;
	unsigned int opcode:5;
};

struct jal {
	/* unsigned int    imm20_15:0;  here is only first 16bits in first HW */
	signed int imm25_21:5;
	unsigned int imm20_16:5;
	unsigned int x:1;
	unsigned int opcode:5;
};

struct i64 {
	unsigned int imm:8;
	unsigned int func:3;
	unsigned int opcode:5;
};

struct ri64 {
	unsigned int imm:5;
	unsigned int ry:3;
	unsigned int func:3;
	unsigned int opcode:5;
};

struct ri {
	unsigned int imm:8;
	unsigned int rx:3;
	unsigned int opcode:5;
};

struct rri {
	unsigned int imm:5;
	unsigned int ry:3;
	unsigned int rx:3;
	unsigned int opcode:5;
};

struct i8 {
	unsigned int imm:8;
	unsigned int func:3;
	unsigned int opcode:5;
};

struct mm_m_format {
	signed int simmediate:12;
	unsigned int func:4;
	unsigned int base:5;
	unsigned int rd:5;
	unsigned int opcode:6;
};

struct mm_x_format {
	unsigned int func:11;
	unsigned int rd:5;
	unsigned int base:5;
	unsigned int index:5;
	unsigned int opcode:6;
};

struct mm16_m_format {
	unsigned int duplicate:16;	/* a copy of the instn */
	unsigned int imm:4;
	unsigned int rlist:2;
	unsigned int func:4;
	unsigned int opcode:6;
};

struct mm16_rb_format {
	unsigned int duplicate:16;	/* a copy of the instn */
	signed int simmediate:4;
	unsigned int base:3;
	unsigned int rt:3;
	unsigned int opcode:6;
};

struct mm16_r5_format {
	unsigned int duplicate:16;	/* a copy of the instn */
	signed int simmediate:5;
	unsigned int rt:5;
	unsigned int opcode:6;
};

struct mm16_r3_format {
	unsigned int duplicate:16;	/* a copy of the instn */
	signed int simmediate:7;
	unsigned int rt:3;
	unsigned int opcode:6;
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
	struct b_format b_format;
	struct mm16b0_format mm16b0_format;
	struct mm16b1_format mm16b1_format;
	struct mm_i_format mm_i_format;
	struct fb_format fb_format;
	struct fp0_format fp0_format;
	struct fp1_format fp1_format;
	struct fp6_format fp6_format;
	struct mm_fp0_format mm_fp0_format;
	struct mm_fp1_format mm_fp1_format;
	struct mm_fp2_format mm_fp2_format;
	struct mm_fp3_format mm_fp3_format;
	struct mm_fp4_format mm_fp4_format;
	struct mm_fp5_format mm_fp5_format;
	struct mm_fp6_format mm_fp6_format;
	struct mm_m_format mm_m_format;
	struct mm_x_format mm_x_format;
	struct mm16_m_format mm16_m_format;
	struct mm16_rb_format mm16_rb_format;
	struct mm16_r3_format mm16_r3_format;
	struct mm16_r5_format mm16_r5_format;
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

/* The following are for micro_mips mode */
#define MM_16_OPCODE_SFT        10
#define MM_NOP16                0x0c00
#define MM_POOL32A_MINOR_MSK    0x3f
#define MM_POOL32A_MINOR_SFT    0x6
#define MIPS32_COND_FC          0x30

/*
 * Major opcodes; micro_mips mode.
 */
enum mm_major_op {
	mm_pool32a_op, mm_pool16a_op, mm_lbu16_op, mm_move16_op,
	mm_addi32_op, mm_lbu32_op, mm_sb32_op, mm_lb32_op,
	mm_pool32b_op, mm_pool16b_op, mm_lhu16_op, mm_andi16_op,
	mm_andiu32_op, mm_lhu32_op, mm_sh32_op, mm_lh32_op,
	mm_pool32i_op, mm_pool16c_op, mm_lwsp16_op, mm_pool16d_op,
	mm_ori32_op, mm_pool32f_op, mm_reserve1_op, mm_reserve2_op,
	mm_pool32c_op, mm_lwgp16_op, mm_lw16_op, mm_pool16e_op,
	mm_xori32_op, mm_jals32_op, mm_addiupc_op, mm_reserve3_op,
	mm_reserve4_op, mm_pool16f_op, mm_sb16_op, mm_beqz16_op,
	mm_slti32_op, mm_beq32_op, mm_swc132_op, mm_lwc132_op,
	mm_reserve5_op, mm_reserve6_op, mm_sh16_op, mm_bnez16_op,
	mm_sltiu32_op, mm_bne32_op, mm_sdc132_op, mm_ldc132_op,
	mm_reserve7_op, mm_reserve8_op, mm_swsp16_op, mm_b16_op,
	mm_and32_op, mm_j32_op, mm_sd32_op, mm_ld32_op,
	mm_reserve11_op, mm_reserve12_op, mm_sw16_op, mm_li16_op,
	mm_jalx32_op, mm_jal32_op, mm_sw32_op, mm_lw32_op
};

/*
 * POOL32I minor opcodes.
 */
enum mm_32i_minor_op {
	mm_bltz_op, mm_bltzal_op, mm_bgez_op, mm_bgezal_op,
	mm_blez_op, mm_bnezc_op, mm_bgtz_op, mm_beqzc_op,
	mm_tlti_op, mm_tgei_op, mm_tltiu_op, mm_tgeiu_op,
	mm_tnei_op, mm_lui_op, mm_teqi_op, mm_resv1_op,
	mm_synci_op, mm_bltzals_op, mm_resv2_op, mm_bgezals_op,
	mm_bc2f_op, mm_bc2t_op, mm_resv3_op, mm_resv4_op,
	mm_resv5_op, mm_resv6_op, mm_bposge64_op, mm_bposge32_op,
	mm_bc1f_op, mm_bc1t_op, mm_resv7_op, mm_resv8_op,
	mm_bc1any2f_op, mm_bc1any2t_op, mm_bc1any4f_op, mm_bc1any4t_op
};

/*
 * POOL32A minor opcodes.
 */
enum mm_32a_minor_op {
	mm_pool32axf_op = 0x3c
};

enum mm_32a_func {
	mm_lwxs32_func = 0x118
};

/*
 * POOL32B minor opcodes.
 */
enum mm_32b_func {
	mm_lwc2_func = 0,
	mm_lwp32_func = 1,
	mm_ldc2_func = 2,
	mm_ldp32_func = 4,
	mm_lwm32_func = 5,
	mm_ldm32_func = 7,
	mm_swc2_func = 8,
	mm_swp32_func = 9,
	mm_sdc2_func = 0xa,
	mm_sdp32_func = 0xc,
	mm_swm32_func = 0xd,
	mm_sdm32_func = 0xf,
};

/*
 * POOL32C minor opcodes.
 */
enum mm_32c_func {
	mm_lwu32_func = 0xe,
};

/*
 * POOL32AXF minor opcodes.
 */
enum mm_32axf_minor_op {
	mm_jalr_op = 0x03c,
	mm_jalrhb_op = 0x07c,
	mm_jalrs_op = 0x13c,
	mm_jalrshb_op = 0x17c,
};

/*
 * POOL32F minor opcodes.
 */
enum mm_32f_minor_op {
	mm_32f_00_op = 0x00,
	mm_32f_01_op = 0x01,
	mm_32f_02_op = 0x02,
	mm_32f_10_op = 0x08,
	mm_32f_11_op = 0x09,
	mm_32f_12_op = 0x0a,
	mm_32f_20_op = 0x10,
	mm_32f_30_op = 0x18,
	mm_32f_40_op = 0x20,
	mm_32f_41_op = 0x21,
	mm_32f_42_op = 0x22,
	mm_32f_50_op = 0x28,
	mm_32f_51_op = 0x29,
	mm_32f_52_op = 0x2a,
	mm_32f_60_op = 0x30,
	mm_32f_70_op = 0x38,
	mm_32f_73_op = 0x3b,
	mm_32f_74_op = 0x3c,
};

/*
 * POOL32F secondary minor opcodes.
 */
enum mm_32f_10_minor_op {
	mm_lwxc1_op = 0x1,
	mm_swxc1_op,
	mm_ldxc1_op,
	mm_sdxc1_op,
	mm_luxc1_op,
	mm_suxc1_op,
};

enum mm_32f_func {
	mm_lwxc1_func = 0x48,
	mm_swxc1_func = 0x88,
	mm_ldxc1_func = 0xc8,
	mm_sdxc1_func = 0x108,
};

/*
 * POOL32F secondary minor opcodes.
 */
enum mm_32f_40_minor_op {
	mm_fmovf_op,
	mm_fmovt_op,
};

/*
 * POOL32F secondary minor opcodes.
 */
enum mm_32f_60_minor_op {
	mm_fadd_op,
	mm_fsub_op,
	mm_fmul_op,
	mm_fdiv_op,
};

/*
 * POOL32F secondary minor opcodes.
 */
enum mm_32f_70_minor_op {
	mm_fmovn_op,
	mm_fmovz_op,
};

/*
 * POOL32F secondary minor opcodes (POOL32FXF).
 */
enum mm_32f_73_minor_op {
	mm_fmov0_op = 0x01,
	mm_fcvtl_op = 0x04,
	mm_movf0_op = 0x05,
	mm_frsqrt_op = 0x08,
	mm_ffloorl_op = 0x0c,
	mm_fabs0_op = 0x0d,
	mm_fcvtw_op = 0x24,
	mm_movt0_op = 0x25,
	mm_fsqrt_op = 0x28,
	mm_ffloorw_op = 0x2c,
	mm_fneg0_op = 0x2d,
	mm_cfc1_op = 0x40,
	mm_frecip_op = 0x48,
	mm_fceill_op = 0x4c,
	mm_fcvtd0_op = 0x4d,
	mm_ctc1_op = 0x60,
	mm_fceilw_op = 0x6c,
	mm_fcvts0_op = 0x6d,
	mm_mfc1_op = 0x80,
	mm_fmov1_op = 0x81,
	mm_movf1_op = 0x85,
	mm_ftruncl_op = 0x8c,
	mm_fabs1_op = 0x8d,
	mm_mtc1_op = 0xa0,
	mm_movt1_op = 0xa5,
	mm_ftruncw_op = 0xac,
	mm_fneg1_op = 0xad,
	mm_froundl_op = 0xcc,
	mm_fcvtd1_op = 0xcd,
	mm_froundw_op = 0xec,
	mm_fcvts1_op = 0xed,
};

/*
 * POOL16C minor opcodes.
 */
enum mm_16c_minor_op {
	mm_lwm16_func = 4,
	mm_swm16_func,
	mm_jr16_op = 0x0c,
	mm_jrc_op,
	mm_jalr16_op,
	mm_jalrs16_op
};

struct decoded_instn {
	mips_instruction insn;
	mips_instruction next_insn;
	int pc_inc;
	int next_pc_inc;
	int micro_mips_mode;
};

/*  recode table from MIPS16e register notation to GPR */
extern int mips16e_reg2gpr[];

union mips16e_instruction {
	unsigned int full:16;
	struct rr rr;
	struct jal jal;
	struct i64 i64;
	struct ri64 ri64;
	struct ri ri;
	struct rri rri;
	struct i8 i8;
};

enum MIPS16e_ops {
	MIPS16e_jal_op = 003,
	MIPS16e_ld_op = 007,
	MIPS16e_i8_op = 014,
	MIPS16e_sd_op = 017,
	MIPS16e_lb_op = 020,
	MIPS16e_lh_op = 021,
	MIPS16e_lwsp_op = 022,
	MIPS16e_lw_op = 023,
	MIPS16e_lbu_op = 024,
	MIPS16e_lhu_op = 025,
	MIPS16e_lwpc_op = 026,
	MIPS16e_lwu_op = 027,
	MIPS16e_sb_op = 030,
	MIPS16e_sh_op = 031,
	MIPS16e_swsp_op = 032,
	MIPS16e_sw_op = 033,
	MIPS16e_rr_op = 035,
	MIPS16e_extend_op = 036,
	MIPS16e_i64_op = 037,
};

enum MIPS16e_i64_func {
	MIPS16e_ldsp_func,
	MIPS16e_sdsp_func,
	MIPS16e_sdrasp_func,
	MIPS16e_dadjsp_func,
	MIPS16e_ldpc_func,
};

enum MIPS16e_rr_func {
	MIPS16e_jr_func,
};

enum MIPS6e_i8_func {
	MIPS16e_swrasp_func = 02,
};

/*
 * This functions returns 1 if the micro_mips instr is a 16 bit instr.
 * Otherwise return 0.
 */
#define MIPS_ISA_MODE   01
#define is16mode(regs)  (regs->cp0_epc & MIPS_ISA_MODE)

static inline int mm_is16bit(u16 instr)
{
	/* take LS 3 bits */
	u16 opcode_low = (instr >> MM_16_OPCODE_SFT) & 0x7;

	if (opcode_low >= 1 && opcode_low <= 3)
		return 1;
	else
		return 0;
}

#endif /* _ASM_INST_H */
