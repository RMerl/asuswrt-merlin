#ifndef _ASM_X86_OLPC_OFW_H
#define _ASM_X86_OLPC_OFW_H

/* index into the page table containing the entry OFW occupies */
#define OLPC_OFW_PDE_NR 1022

#define OLPC_OFW_SIG 0x2057464F	/* aka "OFW " */

#ifdef CONFIG_OLPC_OPENFIRMWARE

/* run an OFW command by calling into the firmware */
#define olpc_ofw(name, args, res) \
	__olpc_ofw((name), ARRAY_SIZE(args), args, ARRAY_SIZE(res), res)

extern int __olpc_ofw(const char *name, int nr_args, const void **args, int nr_res,
		void **res);

/* determine whether OFW is available and lives in the proper memory */
extern void olpc_ofw_detect(void);

/* install OFW's pde permanently into the kernel's pgtable */
extern void setup_olpc_ofw_pgd(void);

#else /* !CONFIG_OLPC_OPENFIRMWARE */

static inline void olpc_ofw_detect(void) { }
static inline void setup_olpc_ofw_pgd(void) { }

#endif /* !CONFIG_OLPC_OPENFIRMWARE */

#endif /* _ASM_X86_OLPC_OFW_H */
