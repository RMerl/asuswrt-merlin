#ifndef _ASM_IA64_PATCH_H
#define _ASM_IA64_PATCH_H

#include <linux/elf.h>
#include <linux/types.h>

extern void ia64_patch (u64 insn_addr, u64 mask, u64 val);	/* patch any insn slot */
extern void ia64_patch_imm64 (u64 insn_addr, u64 val);		/* patch "movl" w/abs. value*/
extern void ia64_patch_imm60 (u64 insn_addr, u64 val);		/* patch "brl" w/ip-rel value */

extern void ia64_patch_mckinley_e9 (unsigned long start, unsigned long end);
extern void ia64_patch_vtop (unsigned long start, unsigned long end);
extern void ia64_patch_phys_stack_reg(unsigned long val);
extern void ia64_patch_rse (unsigned long start, unsigned long end);
extern void ia64_patch_gate (void);

#endif /* _ASM_IA64_PATCH_H */
