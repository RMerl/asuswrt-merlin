/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004, 2005, 2006, 2008  Thiemo Seufer
 * Copyright (C) 2005  Maciej W. Rozycki
 * Copyright (C) 2006  Ralf Baechle (ralf@linux-mips.org)
 */

#include <linux/types.h>


enum opcode {
	insn_invalid,
	insn_addu, insn_addiu, insn_and, insn_andi, insn_beq,
	insn_beql, insn_bgez, insn_bgezl, insn_bltz, insn_bltzl,
	insn_bne, insn_daddu, insn_daddiu, insn_dmfc0, insn_dmtc0,
	insn_dsll, insn_dsll32, insn_dsra, insn_dsrl, insn_dsrl32,
	insn_dsubu, insn_eret, insn_j, insn_jal, insn_jr, insn_ld,
	insn_ll, insn_lld, insn_lui, insn_lw, insn_mfc0, insn_mtc0,
	insn_or, insn_ori, insn_rfe, insn_sc, insn_scd, insn_sd, insn_sll,
	insn_sra, insn_srl, insn_subu, insn_sw, insn_tlbp, insn_tlbwi,
	insn_tlbwr, insn_xor, insn_xori,
	insn_syscall
};

void __cpuinit build_insn(u32 **buf, enum opcode opc, ...);

#define I_u1u2u3(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b, unsigned int c)			\
	{							\
		build_insn(buf, insn##op, a, b, c);		\
	}

#define I_u2u1u3(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b, unsigned int c)			\
	{							\
		build_insn(buf, insn##op, b, a, c);		\
	}

#define I_u3u1u2(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b, unsigned int c)			\
	{							\
		build_insn(buf, insn##op, b, c, a);		\
	}

#define I_u1u2s3(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b, signed int c)			\
	{							\
		build_insn(buf, insn##op, a, b, c);		\
	}

#define I_u2s3u1(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	signed int b, unsigned int c)			\
	{							\
		build_insn(buf, insn##op, c, a, b);		\
	}

#define I_u2u1s3(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b, signed int c)			\
	{							\
		build_insn(buf, insn##op, b, a, c);		\
	}

#define I_u1u2(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	unsigned int b)					\
	{							\
		build_insn(buf, insn##op, a, b);		\
	}

#define I_u1s2(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a,	\
	 	signed int b)					\
	{							\
		build_insn(buf, insn##op, a, b);		\
	}

#define I_u1(op)						\
	static inline void __cpuinit i##op(u32 **buf, unsigned int a)	\
	{							\
		build_insn(buf, insn##op, a);			\
	}

#define I_0(op)							\
	static inline void __cpuinit i##op(u32 **buf)		\
	{							\
		build_insn(buf, insn##op);			\
	}

I_u2u1s3(_addiu);
I_u3u1u2(_addu);
I_u2u1u3(_andi);
I_u3u1u2(_and);
I_u1u2s3(_beq);
I_u1u2s3(_beql);
I_u1s2(_bgez);
I_u1s2(_bgezl);
I_u1s2(_bltz);
I_u1s2(_bltzl);
I_u1u2s3(_bne);
I_u1u2u3(_dmfc0);
I_u1u2u3(_dmtc0);
I_u2u1s3(_daddiu);
I_u3u1u2(_daddu);
I_u2u1u3(_dsll);
I_u2u1u3(_dsll32);
I_u2u1u3(_dsra);
I_u2u1u3(_dsrl);
I_u2u1u3(_dsrl32);
I_u3u1u2(_dsubu);
I_0(_eret);
I_u1(_j);
I_u1(_jal);
I_u1(_jr);
I_u2s3u1(_ld);
I_u2s3u1(_ll);
I_u2s3u1(_lld);
I_u1s2(_lui);
I_u2s3u1(_lw);
I_u1u2u3(_mfc0);
I_u1u2u3(_mtc0);
I_u2u1u3(_ori);
I_u3u1u2(_or)
I_0(_rfe);
I_u2s3u1(_sc);
I_u2s3u1(_scd);
I_u2s3u1(_sd);
I_u2u1u3(_sll);
I_u2u1u3(_sra);
I_u2u1u3(_srl);
I_u3u1u2(_subu);
I_u2s3u1(_sw);
I_0(_tlbp);
I_0(_tlbwi);
I_0(_tlbwr);
I_u3u1u2(_xor)
I_u2u1u3(_xori);
I_u1(_syscall);

/*
 * handling labels
 */

enum label_id {
	label_invalid,
	label_second_part,
	label_leave,
#ifdef MODULE_START
	label_module_alloc,
#endif
	label_vmalloc,
	label_vmalloc_done,
	label_tlbw_hazard,
	label_split,
	label_nopage_tlbl,
	label_nopage_tlbs,
	label_nopage_tlbm,
	label_smp_pgtable_change,
	label_r3000_write_probe_fail,
};

struct label {
	u32 *addr;
	enum label_id lab;
};

static __cpuinit void build_label(struct label **lab, u32 *addr,
			       enum label_id l)
{
	(*lab)->addr = addr;
	(*lab)->lab = l;
	(*lab)++;
}

#define L_LA(lb)						\
	static inline void l##lb(struct label **lab, u32 *addr) \
	{							\
		build_label(lab, addr, label##lb);		\
	}

L_LA(_second_part)
L_LA(_leave)
#ifdef MODULE_START
L_LA(_module_alloc)
#endif
L_LA(_vmalloc)
L_LA(_vmalloc_done)
L_LA(_tlbw_hazard)
L_LA(_split)
L_LA(_nopage_tlbl)
L_LA(_nopage_tlbs)
L_LA(_nopage_tlbm)
L_LA(_smp_pgtable_change)
L_LA(_r3000_write_probe_fail)

/* convenience macros for instructions */
#ifdef CONFIG_64BIT
# define i_LW(buf, rs, rt, off) i_ld(buf, rs, rt, off)
# define i_SW(buf, rs, rt, off) i_sd(buf, rs, rt, off)
# define i_SLL(buf, rs, rt, sh) i_dsll(buf, rs, rt, sh)
# define i_SRA(buf, rs, rt, sh) i_dsra(buf, rs, rt, sh)
# define i_SRL(buf, rs, rt, sh) i_dsrl(buf, rs, rt, sh)
# define i_MFC0(buf, rt, rd...) i_dmfc0(buf, rt, rd)
# define i_MTC0(buf, rt, rd...) i_dmtc0(buf, rt, rd)
# define i_ADDIU(buf, rs, rt, val) i_daddiu(buf, rs, rt, val)
# define i_ADDU(buf, rs, rt, rd) i_daddu(buf, rs, rt, rd)
# define i_SUBU(buf, rs, rt, rd) i_dsubu(buf, rs, rt, rd)
# define i_LL(buf, rs, rt, off) i_lld(buf, rs, rt, off)
# define i_SC(buf, rs, rt, off) i_scd(buf, rs, rt, off)
#else
# define i_LW(buf, rs, rt, off) i_lw(buf, rs, rt, off)
# define i_SW(buf, rs, rt, off) i_sw(buf, rs, rt, off)
# define i_SLL(buf, rs, rt, sh) i_sll(buf, rs, rt, sh)
# define i_SRA(buf, rs, rt, sh) i_sra(buf, rs, rt, sh)
# define i_SRL(buf, rs, rt, sh) i_srl(buf, rs, rt, sh)
# define i_MFC0(buf, rt, rd...) i_mfc0(buf, rt, rd)
# define i_MTC0(buf, rt, rd...) i_mtc0(buf, rt, rd)
# define i_ADDIU(buf, rs, rt, val) i_addiu(buf, rs, rt, val)
# define i_ADDU(buf, rs, rt, rd) i_addu(buf, rs, rt, rd)
# define i_SUBU(buf, rs, rt, rd) i_subu(buf, rs, rt, rd)
# define i_LL(buf, rs, rt, off) i_ll(buf, rs, rt, off)
# define i_SC(buf, rs, rt, off) i_sc(buf, rs, rt, off)
#endif

#define i_b(buf, off) i_beq(buf, 0, 0, off)
#define i_beqz(buf, rs, off) i_beq(buf, rs, 0, off)
#define i_beqzl(buf, rs, off) i_beql(buf, rs, 0, off)
#define i_bnez(buf, rs, off) i_bne(buf, rs, 0, off)
#define i_bnezl(buf, rs, off) i_bnel(buf, rs, 0, off)
#define i_move(buf, a, b) i_ADDU(buf, a, 0, b)
#define i_nop(buf) i_sll(buf, 0, 0, 0)
#define i_ssnop(buf) i_sll(buf, 0, 0, 1)
#define i_ehb(buf) i_sll(buf, 0, 0, 3)

