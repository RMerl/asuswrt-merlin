#ifndef _ASM_X86_SETUP_H
#define _ASM_X86_SETUP_H

#ifdef __KERNEL__

#define COMMAND_LINE_SIZE 2048

#ifdef __i386__

#include <linux/pfn.h>
/*
 * Reserved space for vmalloc and iomap - defined in asm/page.h
 */
#define MAXMEM_PFN	PFN_DOWN(MAXMEM)
#define MAX_NONPAE_PFN	(1 << 20)

#endif /* __i386__ */

#define PARAM_SIZE 4096		/* sizeof(struct boot_params) */

#define OLD_CL_MAGIC		0xA33F
#define OLD_CL_ADDRESS		0x020	/* Relative to real mode data */
#define NEW_CL_POINTER		0x228	/* Relative to real mode data */

#ifndef __ASSEMBLY__
#include <asm/bootparam.h>
#include <asm/x86_init.h>

/* Interrupt control for vSMPowered x86_64 systems */
#ifdef CONFIG_X86_64
void vsmp_init(void);
#else
static inline void vsmp_init(void) { }
#endif

void setup_bios_corruption_check(void);

#ifdef CONFIG_X86_VISWS
extern void visws_early_detect(void);
#else
static inline void visws_early_detect(void) { }
#endif

extern unsigned long saved_video_mode;

extern void reserve_standard_io_resources(void);
extern void i386_reserve_resources(void);
extern void setup_default_timer_irq(void);

#ifdef CONFIG_X86_MRST
extern void x86_mrst_early_setup(void);
#else
static inline void x86_mrst_early_setup(void) { }
#endif

#ifndef _SETUP

/*
 * This is set up by the setup-routine at boot-time
 */
extern struct boot_params boot_params;

/*
 * Do NOT EVER look at the BIOS memory size location.
 * It does not work on many machines.
 */
#define LOWMEMSIZE()	(0x9f000)

/* exceedingly early brk-like allocator */
extern unsigned long _brk_end;
void *extend_brk(size_t size, size_t align);

/*
 * Reserve space in the brk section.  The name must be unique within
 * the file, and somewhat descriptive.  The size is in bytes.  Must be
 * used at file scope.
 *
 * (This uses a temp function to wrap the asm so we can pass it the
 * size parameter; otherwise we wouldn't be able to.  We can't use a
 * "section" attribute on a normal variable because it always ends up
 * being @progbits, which ends up allocating space in the vmlinux
 * executable.)
 */
#define RESERVE_BRK(name,sz)						\
	static void __section(.discard.text) __used			\
	__brk_reservation_fn_##name##__(void) {				\
		asm volatile (						\
			".pushsection .brk_reservation,\"aw\",@nobits;" \
			".brk." #name ":"				\
			" 1:.skip %c0;"					\
			" .size .brk." #name ", . - 1b;"		\
			" .popsection"					\
			: : "i" (sz));					\
	}

#ifdef __i386__

void __init i386_start_kernel(void);
extern void probe_roms(void);

#else
void __init x86_64_start_kernel(char *real_mode);
void __init x86_64_start_reservations(char *real_mode_data);

#endif /* __i386__ */
#endif /* _SETUP */
#else
#define RESERVE_BRK(name,sz)				\
	.pushsection .brk_reservation,"aw",@nobits;	\
.brk.name:						\
1:	.skip sz;					\
	.size .brk.name,.-1b;				\
	.popsection
#endif /* __ASSEMBLY__ */
#endif  /*  __KERNEL__  */

#endif /* _ASM_X86_SETUP_H */
