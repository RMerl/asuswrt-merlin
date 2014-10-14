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
 * Copyright IBM Corp. 2008
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#include <asm/kvm_ppc.h>
#include <asm/dcr.h>
#include <asm/dcr-regs.h>
#include <asm/disassemble.h>
#include <asm/kvm_44x.h>
#include "timing.h"

#include "booke.h"
#include "44x_tlb.h"

#define XOP_MFDCR   323
#define XOP_MTDCR   451
#define XOP_TLBSX   914
#define XOP_ICCCI   966
#define XOP_TLBWE   978

int kvmppc_core_emulate_op(struct kvm_run *run, struct kvm_vcpu *vcpu,
                           unsigned int inst, int *advance)
{
	int emulated = EMULATE_DONE;
	int dcrn;
	int ra;
	int rb;
	int rc;
	int rs;
	int rt;
	int ws;

	switch (get_op(inst)) {
	case 31:
		switch (get_xop(inst)) {

		case XOP_MFDCR:
			dcrn = get_dcrn(inst);
			rt = get_rt(inst);

			/* The guest may access CPR0 registers to determine the timebase
			 * frequency, and it must know the real host frequency because it
			 * can directly access the timebase registers.
			 *
			 * It would be possible to emulate those accesses in userspace,
			 * but userspace can really only figure out the end frequency.
			 * We could decompose that into the factors that compute it, but
			 * that's tricky math, and it's easier to just report the real
			 * CPR0 values.
			 */
			switch (dcrn) {
			case DCRN_CPR0_CONFIG_ADDR:
				kvmppc_set_gpr(vcpu, rt, vcpu->arch.cpr0_cfgaddr);
				break;
			case DCRN_CPR0_CONFIG_DATA:
				local_irq_disable();
				mtdcr(DCRN_CPR0_CONFIG_ADDR,
					  vcpu->arch.cpr0_cfgaddr);
				kvmppc_set_gpr(vcpu, rt,
					       mfdcr(DCRN_CPR0_CONFIG_DATA));
				local_irq_enable();
				break;
			default:
				run->dcr.dcrn = dcrn;
				run->dcr.data =  0;
				run->dcr.is_write = 0;
				vcpu->arch.io_gpr = rt;
				vcpu->arch.dcr_needed = 1;
				kvmppc_account_exit(vcpu, DCR_EXITS);
				emulated = EMULATE_DO_DCR;
			}

			break;

		case XOP_MTDCR:
			dcrn = get_dcrn(inst);
			rs = get_rs(inst);

			/* emulate some access in kernel */
			switch (dcrn) {
			case DCRN_CPR0_CONFIG_ADDR:
				vcpu->arch.cpr0_cfgaddr = kvmppc_get_gpr(vcpu, rs);
				break;
			default:
				run->dcr.dcrn = dcrn;
				run->dcr.data = kvmppc_get_gpr(vcpu, rs);
				run->dcr.is_write = 1;
				vcpu->arch.dcr_needed = 1;
				kvmppc_account_exit(vcpu, DCR_EXITS);
				emulated = EMULATE_DO_DCR;
			}

			break;

		case XOP_TLBWE:
			ra = get_ra(inst);
			rs = get_rs(inst);
			ws = get_ws(inst);
			emulated = kvmppc_44x_emul_tlbwe(vcpu, ra, rs, ws);
			break;

		case XOP_TLBSX:
			rt = get_rt(inst);
			ra = get_ra(inst);
			rb = get_rb(inst);
			rc = get_rc(inst);
			emulated = kvmppc_44x_emul_tlbsx(vcpu, rt, ra, rb, rc);
			break;

		case XOP_ICCCI:
			break;

		default:
			emulated = EMULATE_FAIL;
		}

		break;

	default:
		emulated = EMULATE_FAIL;
	}

	if (emulated == EMULATE_FAIL)
		emulated = kvmppc_booke_emulate_op(run, vcpu, inst, advance);

	return emulated;
}

int kvmppc_core_emulate_mtspr(struct kvm_vcpu *vcpu, int sprn, int rs)
{
	int emulated = EMULATE_DONE;

	switch (sprn) {
	case SPRN_PID:
		kvmppc_set_pid(vcpu, kvmppc_get_gpr(vcpu, rs)); break;
	case SPRN_MMUCR:
		vcpu->arch.mmucr = kvmppc_get_gpr(vcpu, rs); break;
	case SPRN_CCR0:
		vcpu->arch.ccr0 = kvmppc_get_gpr(vcpu, rs); break;
	case SPRN_CCR1:
		vcpu->arch.ccr1 = kvmppc_get_gpr(vcpu, rs); break;
	default:
		emulated = kvmppc_booke_emulate_mtspr(vcpu, sprn, rs);
	}

	kvmppc_set_exit_type(vcpu, EMULATED_MTSPR_EXITS);
	return emulated;
}

int kvmppc_core_emulate_mfspr(struct kvm_vcpu *vcpu, int sprn, int rt)
{
	int emulated = EMULATE_DONE;

	switch (sprn) {
	case SPRN_PID:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.pid); break;
	case SPRN_MMUCR:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.mmucr); break;
	case SPRN_CCR0:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.ccr0); break;
	case SPRN_CCR1:
		kvmppc_set_gpr(vcpu, rt, vcpu->arch.ccr1); break;
	default:
		emulated = kvmppc_booke_emulate_mfspr(vcpu, sprn, rt);
	}

	kvmppc_set_exit_type(vcpu, EMULATED_MFSPR_EXITS);
	return emulated;
}

