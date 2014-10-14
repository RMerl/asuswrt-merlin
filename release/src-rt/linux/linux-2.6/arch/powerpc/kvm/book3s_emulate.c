/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright SUSE Linux Products GmbH 2009
 *
 * Authors: Alexander Graf <agraf@suse.de>
 */

#include <asm/kvm_ppc.h>
#include <asm/disassemble.h>
#include <asm/kvm_book3s.h>
#include <asm/reg.h>

#define OP_19_XOP_RFID		18
#define OP_19_XOP_RFI		50

#define OP_31_XOP_MFMSR		83
#define OP_31_XOP_MTMSR		146
#define OP_31_XOP_MTMSRD	178
#define OP_31_XOP_MTSR		210
#define OP_31_XOP_MTSRIN	242
#define OP_31_XOP_TLBIEL	274
#define OP_31_XOP_TLBIE		306
#define OP_31_XOP_SLBMTE	402
#define OP_31_XOP_SLBIE		434
#define OP_31_XOP_SLBIA		498
#define OP_31_XOP_MFSR		595
#define OP_31_XOP_MFSRIN	659
#define OP_31_XOP_DCBA		758
#define OP_31_XOP_SLBMFEV	851
#define OP_31_XOP_EIOIO		854
#define OP_31_XOP_SLBMFEE	915

/* DCBZ is actually 1014, but we patch it to 1010 so we get a trap */
#define OP_31_XOP_DCBZ		1010

#define OP_LFS			48
#define OP_LFD			50
#define OP_STFS			52
#define OP_STFD			54

#define SPRN_GQR0		912
#define SPRN_GQR1		913
#define SPRN_GQR2		914
#define SPRN_GQR3		915
#define SPRN_GQR4		916
#define SPRN_GQR5		917
#define SPRN_GQR6		918
#define SPRN_GQR7		919

/* Book3S_32 defines mfsrin(v) - but that messes up our abstract
 * function pointers, so let's just disable the define. */
#undef mfsrin

int kvmppc_core_emulate_op(struct kvm_run *run, struct kvm_vcpu *vcpu,
                           unsigned int inst, int *advance)
{
	int emulated = EMULATE_DONE;

	switch (get_op(inst)) {
	case 19:
		switch (get_xop(inst)) {
		case OP_19_XOP_RFID:
		case OP_19_XOP_RFI:
			kvmppc_set_pc(vcpu, vcpu->arch.shared->srr0);
			kvmppc_set_msr(vcpu, vcpu->arch.shared->srr1);
			*advance = 0;
			break;

		default:
			emulated = EMULATE_FAIL;
			break;
		}
		break;
	case 31:
		switch (get_xop(inst)) {
		case OP_31_XOP_MFMSR:
			kvmppc_set_gpr(vcpu, get_rt(inst),
				       vcpu->arch.shared->msr);
			break;
		case OP_31_XOP_MTMSRD:
		{
			ulong rs = kvmppc_get_gpr(vcpu, get_rs(inst));
			if (inst & 0x10000) {
				vcpu->arch.shared->msr &= ~(MSR_RI | MSR_EE);
				vcpu->arch.shared->msr |= rs & (MSR_RI | MSR_EE);
			} else
				kvmppc_set_msr(vcpu, rs);
			break;
		}
		case OP_31_XOP_MTMSR:
			kvmppc_set_msr(vcpu, kvmppc_get_gpr(vcpu, get_rs(inst)));
			break;
		case OP_31_XOP_MFSR:
		{
			int srnum;

			srnum = kvmppc_get_field(inst, 12 + 32, 15 + 32);
			if (vcpu->arch.mmu.mfsrin) {
				u32 sr;
				sr = vcpu->arch.mmu.mfsrin(vcpu, srnum);
				kvmppc_set_gpr(vcpu, get_rt(inst), sr);
			}
			break;
		}
		case OP_31_XOP_MFSRIN:
		{
			int srnum;

			srnum = (kvmppc_get_gpr(vcpu, get_rb(inst)) >> 28) & 0xf;
			if (vcpu->arch.mmu.mfsrin) {
				u32 sr;
				sr = vcpu->arch.mmu.mfsrin(vcpu, srnum);
				kvmppc_set_gpr(vcpu, get_rt(inst), sr);
			}
			break;
		}
		case OP_31_XOP_MTSR:
			vcpu->arch.mmu.mtsrin(vcpu,
				(inst >> 16) & 0xf,
				kvmppc_get_gpr(vcpu, get_rs(inst)));
			break;
		case OP_31_XOP_MTSRIN:
			vcpu->arch.mmu.mtsrin(vcpu,
				(kvmppc_get_gpr(vcpu, get_rb(inst)) >> 28) & 0xf,
				kvmppc_get_gpr(vcpu, get_rs(inst)));
			break;
		case OP_31_XOP_TLBIE:
		case OP_31_XOP_TLBIEL:
		{
			bool large = (inst & 0x00200000) ? true : false;
			ulong addr = kvmppc_get_gpr(vcpu, get_rb(inst));
			vcpu->arch.mmu.tlbie(vcpu, addr, large);
			break;
		}
		case OP_31_XOP_EIOIO:
			break;
		case OP_31_XOP_SLBMTE:
			if (!vcpu->arch.mmu.slbmte)
				return EMULATE_FAIL;

			vcpu->arch.mmu.slbmte(vcpu,
					kvmppc_get_gpr(vcpu, get_rs(inst)),
					kvmppc_get_gpr(vcpu, get_rb(inst)));
			break;
		case OP_31_XOP_SLBIE:
			if (!vcpu->arch.mmu.slbie)
				return EMULATE_FAIL;

			vcpu->arch.mmu.slbie(vcpu,
					kvmppc_get_gpr(vcpu, get_rb(inst)));
			break;
		case OP_31_XOP_SLBIA:
			if (!vcpu->arch.mmu.slbia)
				return EMULATE_FAIL;

			vcpu->arch.mmu.slbia(vcpu);
			break;
		case OP_31_XOP_SLBMFEE:
			if (!vcpu->arch.mmu.slbmfee) {
				emulated = EMULATE_FAIL;
			} else {
				ulong t, rb;

				rb = kvmppc_get_gpr(vcpu, get_rb(inst));
				t = vcpu->arch.mmu.slbmfee(vcpu, rb);
				kvmppc_set_gpr(vcpu, get_rt(inst), t);
			}
			break;
		case OP_31_XOP_SLBMFEV:
			if (!vcpu->arch.mmu.slbmfev) {
				emulated = EMULATE_FAIL;
			} else {
				ulong t, rb;

				rb = kvmppc_get_gpr(vcpu, get_rb(inst));
				t = vcpu->arch.mmu.slbmfev(vcpu, rb);
				kvmppc_set_gpr(vcpu, get_rt(inst), t);
			}
			break;
		case OP_31_XOP_DCBA:
			/* Gets treated as NOP */
			break;
		case OP_31_XOP_DCBZ:
		{
			ulong rb = kvmppc_get_gpr(vcpu, get_rb(inst));
			ulong ra = 0;
			ulong addr, vaddr;
			u32 zeros[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			u32 dsisr;
			int r;

			if (get_ra(inst))
				ra = kvmppc_get_gpr(vcpu, get_ra(inst));

			addr = (ra + rb) & ~31ULL;
			if (!(vcpu->arch.shared->msr & MSR_SF))
				addr &= 0xffffffff;
			vaddr = addr;

			r = kvmppc_st(vcpu, &addr, 32, zeros, true);
			if ((r == -ENOENT) || (r == -EPERM)) {
				*advance = 0;
				vcpu->arch.shared->dar = vaddr;
				to_svcpu(vcpu)->fault_dar = vaddr;

				dsisr = DSISR_ISSTORE;
				if (r == -ENOENT)
					dsisr |= DSISR_NOHPTE;
				else if (r == -EPERM)
					dsisr |= DSISR_PROTFAULT;

				vcpu->arch.shared->dsisr = dsisr;
				to_svcpu(vcpu)->fault_dsisr = dsisr;

				kvmppc_book3s_queue_irqprio(vcpu,
					BOOK3S_INTERRUPT_DATA_STORAGE);
			}

			break;
		}
		default:
			emulated = EMULATE_FAIL;
		}
		break;
	default:
		emulated = EMULATE_FAIL;
	}

	if (emulated == EMULATE_FAIL)
		emulated = kvmppc_emulate_paired_single(run, vcpu);

	return emulated;
}

void kvmppc_set_bat(struct kvm_vcpu *vcpu, struct kvmppc_bat *bat, bool upper,
                    u32 val)
{
	if (upper) {
		/* Upper BAT */
		u32 bl = (val >> 2) & 0x7ff;
		bat->bepi_mask = (~bl << 17);
		bat->bepi = val & 0xfffe0000;
		bat->vs = (val & 2) ? 1 : 0;
		bat->vp = (val & 1) ? 1 : 0;
		bat->raw = (bat->raw & 0xffffffff00000000ULL) | val;
	} else {
		/* Lower BAT */
		bat->brpn = val & 0xfffe0000;
		bat->wimg = (val >> 3) & 0xf;
		bat->pp = val & 3;
		bat->raw = (bat->raw & 0x00000000ffffffffULL) | ((u64)val << 32);
	}
}

static struct kvmppc_bat *kvmppc_find_bat(struct kvm_vcpu *vcpu, int sprn)
{
	struct kvmppc_vcpu_book3s *vcpu_book3s = to_book3s(vcpu);
	struct kvmppc_bat *bat;

	switch (sprn) {
	case SPRN_IBAT0U ... SPRN_IBAT3L:
		bat = &vcpu_book3s->ibat[(sprn - SPRN_IBAT0U) / 2];
		break;
	case SPRN_IBAT4U ... SPRN_IBAT7L:
		bat = &vcpu_book3s->ibat[4 + ((sprn - SPRN_IBAT4U) / 2)];
		break;
	case SPRN_DBAT0U ... SPRN_DBAT3L:
		bat = &vcpu_book3s->dbat[(sprn - SPRN_DBAT0U) / 2];
		break;
	case SPRN_DBAT4U ... SPRN_DBAT7L:
		bat = &vcpu_book3s->dbat[4 + ((sprn - SPRN_DBAT4U) / 2)];
		break;
	default:
		BUG();
	}

	return bat;
}

int kvmppc_core_emulate_mtspr(struct kvm_vcpu *vcpu, int sprn, int rs)
{
	int emulated = EMULATE_DONE;
	ulong spr_val = kvmppc_get_gpr(vcpu, rs);

	switch (sprn) {
	case SPRN_SDR1:
		to_book3s(vcpu)->sdr1 = spr_val;
		break;
	case SPRN_DSISR:
		vcpu->arch.shared->dsisr = spr_val;
		break;
	case SPRN_DAR:
		vcpu->arch.shared->dar = spr_val;
		break;
	case SPRN_HIOR:
		to_book3s(vcpu)->hior = spr_val;
		break;
	case SPRN_IBAT0U ... SPRN_IBAT3L:
	case SPRN_IBAT4U ... SPRN_IBAT7L:
	case SPRN_DBAT0U ... SPRN_DBAT3L:
	case SPRN_DBAT4U ... SPRN_DBAT7L:
	{
		struct kvmppc_bat *bat = kvmppc_find_bat(vcpu, sprn);

		kvmppc_set_bat(vcpu, bat, !(sprn % 2), (u32)spr_val);
		/* BAT writes happen so rarely that we're ok to flush
		 * everything here */
		kvmppc_mmu_pte_flush(vcpu, 0, 0);
		kvmppc_mmu_flush_segments(vcpu);
		break;
	}
	case SPRN_HID0:
		to_book3s(vcpu)->hid[0] = spr_val;
		break;
	case SPRN_HID1:
		to_book3s(vcpu)->hid[1] = spr_val;
		break;
	case SPRN_HID2:
		to_book3s(vcpu)->hid[2] = spr_val;
		break;
	case SPRN_HID2_GEKKO:
		to_book3s(vcpu)->hid[2] = spr_val;
		/* HID2.PSE controls paired single on gekko */
		switch (vcpu->arch.pvr) {
		case 0x00080200:	/* lonestar 2.0 */
		case 0x00088202:	/* lonestar 2.2 */
		case 0x70000100:	/* gekko 1.0 */
		case 0x00080100:	/* gekko 2.0 */
		case 0x00083203:	/* gekko 2.3a */
		case 0x00083213:	/* gekko 2.3b */
		case 0x00083204:	/* gekko 2.4 */
		case 0x00083214:	/* gekko 2.4e (8SE) - retail HW2 */
		case 0x00087200:	/* broadway */
			if (vcpu->arch.hflags & BOOK3S_HFLAG_NATIVE_PS) {
				/* Native paired singles */
			} else if (spr_val & (1 << 29)) { /* HID2.PSE */
				vcpu->arch.hflags |= BOOK3S_HFLAG_PAIRED_SINGLE;
				kvmppc_giveup_ext(vcpu, MSR_FP);
			} else {
				vcpu->arch.hflags &= ~BOOK3S_HFLAG_PAIRED_SINGLE;
			}
			break;
		}
		break;
	case SPRN_HID4:
	case SPRN_HID4_GEKKO:
		to_book3s(vcpu)->hid[4] = spr_val;
		break;
	case SPRN_HID5:
		to_book3s(vcpu)->hid[5] = spr_val;
		/* guest HID5 set can change is_dcbz32 */
		if (vcpu->arch.mmu.is_dcbz32(vcpu) &&
		    (mfmsr() & MSR_HV))
			vcpu->arch.hflags |= BOOK3S_HFLAG_DCBZ32;
		break;
	case SPRN_GQR0:
	case SPRN_GQR1:
	case SPRN_GQR2:
	case SPRN_GQR3:
	case SPRN_GQR4:
	case SPRN_GQR5:
	case SPRN_GQR6:
	case SPRN_GQR7:
		to_book3s(vcpu)->gqr[sprn - SPRN_GQR0] = spr_val;
		break;
	case SPRN_ICTC:
	case SPRN_THRM1:
	case SPRN_THRM2:
	case SPRN_THRM3:
	case SPRN_CTRLF:
	case SPRN_CTRLT:
	case SPRN_L2CR:
	case SPRN_MMCR0_GEKKO:
	case SPRN_MMCR1_GEKKO:
	case SPRN_PMC1_GEKKO:
	case SPRN_PMC2_GEKKO:
	case SPRN_PMC3_GEKKO:
	case SPRN_PMC4_GEKKO:
	case SPRN_WPAR_GEKKO:
		break;
	default:
		printk(KERN_INFO "KVM: invalid SPR write: %d\n", sprn);
#ifndef DEBUG_SPR
		emulated = EMULATE_FAIL;
#endif
		break;
	}

	return emulated;
}

int kvmppc_core_emulate_mfspr(struct kvm_vcpu *vcpu, int sprn, int rt)
{
	int emulated = EMULATE_DONE;

	switch (sprn) {
	case SPRN_IBAT0U ... SPRN_IBAT3L:
	case SPRN_IBAT4U ... SPRN_IBAT7L:
	case SPRN_DBAT0U ... SPRN_DBAT3L:
	case SPRN_DBAT4U ... SPRN_DBAT7L:
	{
		struct kvmppc_bat *bat = kvmppc_find_bat(vcpu, sprn);

		if (sprn % 2)
			kvmppc_set_gpr(vcpu, rt, bat->raw >> 32);
		else
			kvmppc_set_gpr(vcpu, rt, bat->raw);

		break;
	}
	case SPRN_SDR1:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->sdr1);
		break;
	case SPRN_DSISR:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.shared->dsisr);
		break;
	case SPRN_DAR:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.shared->dar);
		break;
	case SPRN_HIOR:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hior);
		break;
	case SPRN_HID0:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hid[0]);
		break;
	case SPRN_HID1:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hid[1]);
		break;
	case SPRN_HID2:
	case SPRN_HID2_GEKKO:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hid[2]);
		break;
	case SPRN_HID4:
	case SPRN_HID4_GEKKO:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hid[4]);
		break;
	case SPRN_HID5:
		kvmppc_set_gpr(vcpu, rt, to_book3s(vcpu)->hid[5]);
		break;
	case SPRN_GQR0:
	case SPRN_GQR1:
	case SPRN_GQR2:
	case SPRN_GQR3:
	case SPRN_GQR4:
	case SPRN_GQR5:
	case SPRN_GQR6:
	case SPRN_GQR7:
		kvmppc_set_gpr(vcpu, rt,
			       to_book3s(vcpu)->gqr[sprn - SPRN_GQR0]);
		break;
	case SPRN_THRM1:
	case SPRN_THRM2:
	case SPRN_THRM3:
	case SPRN_CTRLF:
	case SPRN_CTRLT:
	case SPRN_L2CR:
	case SPRN_MMCR0_GEKKO:
	case SPRN_MMCR1_GEKKO:
	case SPRN_PMC1_GEKKO:
	case SPRN_PMC2_GEKKO:
	case SPRN_PMC3_GEKKO:
	case SPRN_PMC4_GEKKO:
	case SPRN_WPAR_GEKKO:
		kvmppc_set_gpr(vcpu, rt, 0);
		break;
	default:
		printk(KERN_INFO "KVM: invalid SPR read: %d\n", sprn);
#ifndef DEBUG_SPR
		emulated = EMULATE_FAIL;
#endif
		break;
	}

	return emulated;
}

u32 kvmppc_alignment_dsisr(struct kvm_vcpu *vcpu, unsigned int inst)
{
	u32 dsisr = 0;

	/*
	 * This is what the spec says about DSISR bits (not mentioned = 0):
	 *
	 * 12:13		[DS]	Set to bits 30:31
	 * 15:16		[X]	Set to bits 29:30
	 * 17			[X]	Set to bit 25
	 *			[D/DS]	Set to bit 5
	 * 18:21		[X]	Set to bits 21:24
	 *			[D/DS]	Set to bits 1:4
	 * 22:26			Set to bits 6:10 (RT/RS/FRT/FRS)
	 * 27:31			Set to bits 11:15 (RA)
	 */

	switch (get_op(inst)) {
	/* D-form */
	case OP_LFS:
	case OP_LFD:
	case OP_STFD:
	case OP_STFS:
		dsisr |= (inst >> 12) & 0x4000;	/* bit 17 */
		dsisr |= (inst >> 17) & 0x3c00; /* bits 18:21 */
		break;
	/* X-form */
	case 31:
		dsisr |= (inst << 14) & 0x18000; /* bits 15:16 */
		dsisr |= (inst << 8)  & 0x04000; /* bit 17 */
		dsisr |= (inst << 3)  & 0x03c00; /* bits 18:21 */
		break;
	default:
		printk(KERN_INFO "KVM: Unaligned instruction 0x%x\n", inst);
		break;
	}

	dsisr |= (inst >> 16) & 0x03ff; /* bits 22:31 */

	return dsisr;
}

ulong kvmppc_alignment_dar(struct kvm_vcpu *vcpu, unsigned int inst)
{
	ulong dar = 0;
	ulong ra;

	switch (get_op(inst)) {
	case OP_LFS:
	case OP_LFD:
	case OP_STFD:
	case OP_STFS:
		ra = get_ra(inst);
		if (ra)
			dar = kvmppc_get_gpr(vcpu, ra);
		dar += (s32)((s16)inst);
		break;
	case 31:
		ra = get_ra(inst);
		if (ra)
			dar = kvmppc_get_gpr(vcpu, ra);
		dar += kvmppc_get_gpr(vcpu, get_rb(inst));
		break;
	default:
		printk(KERN_INFO "KVM: Unaligned instruction 0x%x\n", inst);
		break;
	}

	return dar;
}
