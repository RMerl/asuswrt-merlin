/*
 * include/asm-m68knommu/flat.h -- uClinux flat-format executables
 */

#ifndef __M68KNOMMU_FLAT_H__
#define __M68KNOMMU_FLAT_H__

#define	flat_stack_align(sp)			/* nothing needed */
#define	flat_argvp_envp_on_stack()		1
#define	flat_old_ram_flag(flags)		(flags)
#define	flat_reloc_valid(reloc, size)		((reloc) <= (size))
#define	flat_get_addr_from_rp(rp, relval, flags)	get_unaligned(rp)
#define	flat_put_addr_at_rp(rp, val, relval)	put_unaligned(val,rp)
#define	flat_get_relocate_addr(rel)		(rel)

#endif /* __M68KNOMMU_FLAT_H__ */
