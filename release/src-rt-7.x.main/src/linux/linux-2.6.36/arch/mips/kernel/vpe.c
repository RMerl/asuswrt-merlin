/*
 * Copyright (C) 2004, 2005 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/*
 * VPE support module
 *
 * Provides support for loading a MIPS SP program on VPE1.
 * The SP enviroment is rather simple, no tlb's.  It needs to be relocatable
 * (or partially linked). You should initialise your stack in the startup
 * code. This loader looks for the symbol __start and sets up
 * execution to resume from there. The MIPS SDE kit contains suitable examples.
 *
 * To load and run, simply cat a SP 'program file' to /dev/vpe1.
 * i.e cat spapp >/dev/vpe1.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/elf.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/moduleloader.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/bootmem.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/cacheflush.h>
#include <asm/atomic.h>
#include <asm/cpu.h>
#include <asm/mips_mt.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/vpe.h>
#include <asm/kspd.h>

typedef void *vpe_handle;

#ifndef ARCH_SHF_SMALL
#define ARCH_SHF_SMALL 0
#endif

/* If this is set, the section belongs in the init part of the module */
#define INIT_OFFSET_MASK (1UL << (BITS_PER_LONG-1))

/*
 * The number of TCs and VPEs physically available on the core
 */
static int hw_tcs, hw_vpes;
static char module_name[] = "vpe";
static int major;
static const int minor = 1;	/* fixed for now  */

#ifdef CONFIG_MIPS_APSP_KSPD
static struct kspd_notifications kspd_events;
static int kspd_events_reqd;
#endif
/*
 * Size of private kernel buffer for ELF headers and sections
 */
#define P_SIZE (256 * 1024)

/*
 * Size of private kernel buffer for ELF headers and sections
 */
#define MAX_VPES 16
#define VPE_PATH_MAX 256

enum vpe_state {
	VPE_STATE_UNUSED = 0,
	VPE_STATE_INUSE,
	VPE_STATE_RUNNING
};

enum tc_state {
	TC_STATE_UNUSED = 0,
	TC_STATE_INUSE,
	TC_STATE_RUNNING,
	TC_STATE_DYNAMIC
};

enum load_state {
	LOAD_STATE_EHDR,
	LOAD_STATE_PHDR,
	LOAD_STATE_SHDR,
	LOAD_STATE_PIMAGE,
	LOAD_STATE_TRAILER,
	LOAD_STATE_DONE,
	LOAD_STATE_ERROR
};

struct vpe {
	enum vpe_state state;

	/* (device) minor associated with this vpe */
	int minor;

	/* elfloader stuff */
	unsigned long offset; /* File offset into input stream */
	void *load_addr;
	unsigned long copied;
	char *pbuffer;
	unsigned long pbsize;
	/* Program loading state */
	enum load_state l_state;
	Elf_Ehdr *l_ehdr;
	struct elf_phdr *l_phdr;
	unsigned int l_phlen;
	Elf_Shdr *l_shdr;
	unsigned int l_shlen;
	int *l_phsort;  /* Sorted index list of program headers */
	int l_segoff;   /* Offset into current program segment */
	int l_cur_seg;  /* Indirect index of segment currently being loaded */
	unsigned int l_progminad;
	unsigned int l_progmaxad;
	unsigned int l_trailer;

	unsigned int uid, gid;
	char cwd[VPE_PATH_MAX];

	unsigned long __start;

	/* tc's associated with this vpe */
	struct list_head tc;

	/* The list of vpe's */
	struct list_head list;

	/* legacy shared symbol address */
	void *shared_ptr;

	 /* shared area descriptor array address */
	struct vpe_shared_area *shared_areas;

	/* the list of who wants to know when something major happens */
	struct list_head notify;

	unsigned int ntcs;
};

struct tc {
	enum tc_state state;
	int index;

	struct vpe *pvpe;	/* parent VPE */
	struct list_head tc;	/* The list of TC's with this VPE */
	struct list_head list;	/* The global list of tc's */
};

struct {
	spinlock_t vpe_list_lock;
	struct list_head vpe_list;	/* Virtual processing elements */
	spinlock_t tc_list_lock;
	struct list_head tc_list;	/* Thread contexts */
} vpecontrol = {
	.vpe_list_lock	= SPIN_LOCK_UNLOCKED,
	.vpe_list	= LIST_HEAD_INIT(vpecontrol.vpe_list),
	.tc_list_lock	= SPIN_LOCK_UNLOCKED,
	.tc_list	= LIST_HEAD_INIT(vpecontrol.tc_list)
};

static void release_progmem(void *ptr);
/*
 * Values and state associated with publishing shared memory areas
 */

#define N_PUB_AREAS 4

static struct vpe_shared_area published_vpe_area[N_PUB_AREAS] = {
	{VPE_SHARED_RESERVED, 0},
	{VPE_SHARED_RESERVED, 0},
	{VPE_SHARED_RESERVED, 0},
	{VPE_SHARED_RESERVED, 0} };

/* get the vpe associated with this minor */
static struct vpe *get_vpe(int minor)
{
	struct vpe *res, *v;

	if (!cpu_has_mipsmt)
		return NULL;

	res = NULL;
	spin_lock(&vpecontrol.vpe_list_lock);
	list_for_each_entry(v, &vpecontrol.vpe_list, list) {
		if (v->minor == minor) {
			res = v;
			break;
		}
	}
	spin_unlock(&vpecontrol.vpe_list_lock);

	return res;
}

/* get the tc associated with this minor */
static struct tc *get_tc(int index)
{
	struct tc *res, *t;

	res = NULL;
	spin_lock(&vpecontrol.tc_list_lock);
	list_for_each_entry(t, &vpecontrol.tc_list, list) {
		if (t->index == index) {
			res = t;
			break;
		}
	}
	spin_unlock(&vpecontrol.tc_list_lock);

	return res;
}


/* allocate a vpe and associate it with this minor (or index) */
static struct vpe *alloc_vpe(int minor)
{
	struct vpe *v;

	if ((v = kzalloc(sizeof(struct vpe), GFP_KERNEL)) == NULL)
		return NULL;
	printk(KERN_DEBUG "Used kzalloc to allocate %d bytes at %x\n",
		sizeof(struct vpe), (unsigned int)v);
	INIT_LIST_HEAD(&v->tc);
	spin_lock(&vpecontrol.vpe_list_lock);
	list_add_tail(&v->list, &vpecontrol.vpe_list);
	spin_unlock(&vpecontrol.vpe_list_lock);

	INIT_LIST_HEAD(&v->notify);
	v->minor = minor;

	return v;
}

/* allocate a tc. At startup only tc0 is running, all other can be halted. */
static struct tc *alloc_tc(int index)
{
	struct tc *tc;

	if ((tc = kzalloc(sizeof(struct tc), GFP_KERNEL)) == NULL)
		goto out;
	printk(KERN_DEBUG "Used kzalloc to allocate %d bytes at %x\n",
		sizeof(struct tc), (unsigned int)tc);
	INIT_LIST_HEAD(&tc->tc);
	tc->index = index;

	spin_lock(&vpecontrol.tc_list_lock);
	list_add_tail(&tc->list, &vpecontrol.tc_list);
	spin_unlock(&vpecontrol.tc_list_lock);

out:
	return tc;
}

/* clean up and free everything */
static void release_vpe(struct vpe *v)
{
	list_del(&v->list);
	if (v->load_addr)
		release_progmem(v);
	printk(KERN_DEBUG "Used kfree to free memory at %x\n",
		(unsigned int)v->l_phsort);
	kfree(v->l_phsort);
	printk(KERN_DEBUG "Used kfree to free memory at %x\n",
		(unsigned int)v);
	kfree(v);
}

static void __maybe_unused dump_mtregs(void)
{
	unsigned long val;

	val = read_c0_config3();
	printk("config3 0x%lx MT %ld\n", val,
	       (val & CONFIG3_MT) >> CONFIG3_MT_SHIFT);

	val = read_c0_mvpcontrol();
	printk("MVPControl 0x%lx, STLB %ld VPC %ld EVP %ld\n", val,
	       (val & MVPCONTROL_STLB) >> MVPCONTROL_STLB_SHIFT,
	       (val & MVPCONTROL_VPC) >> MVPCONTROL_VPC_SHIFT,
	       (val & MVPCONTROL_EVP));

	val = read_c0_mvpconf0();
	printk("mvpconf0 0x%lx, PVPE %ld PTC %ld M %ld\n", val,
	       (val & MVPCONF0_PVPE) >> MVPCONF0_PVPE_SHIFT,
	       val & MVPCONF0_PTC, (val & MVPCONF0_M) >> MVPCONF0_M_SHIFT);
}

/*
 * The original APRP prototype assumed a single, unshared IRQ for
 * cross-VPE interrupts, used by the RTLX code.  But M3P networking
 * and other future functions may need to share an IRQ, particularly
 * in 34K/Malta configurations without an external interrupt controller.
 * All cross-VPE insterrupt users need to coordinate through shared
 * functions here.
 */

/*
 * It would be nice if I could just have this initialized to zero,
 * but the patchcheck police won't hear of it...
 */

static int xvpe_vector_set;

#define XVPE_INTR_OFFSET 0

static int xvpe_irq = MIPS_CPU_IRQ_BASE + XVPE_INTR_OFFSET;

static void xvpe_dispatch(void)
{
	do_IRQ(xvpe_irq);
}

/* Name here is generic, as m3pnet.c could in principle be used by non-MIPS */
int arch_get_xcpu_irq()
{
	/*
	 * Some of this will ultimately become platform code,
	 * but for now, we're only targeting 34K/FPGA/Malta,
	 * and there's only one generic mechanism.
	 */
	if (!xvpe_vector_set) {
		/*
		 * A more elaborate shared variable shouldn't be needed.
		 * Two initializations back-to-back should be harmless.
		 */
		if (cpu_has_vint) {
			set_vi_handler(XVPE_INTR_OFFSET, xvpe_dispatch);
			xvpe_vector_set = 1;
		} else {
			printk(KERN_ERR "APRP requires vectored interrupts\n");
			return -1;
		}
	}

	return xvpe_irq;
}
EXPORT_SYMBOL(arch_get_xcpu_irq);

int vpe_send_interrupt(int vpe, int inter)
{
	unsigned long flags;
	unsigned int vpeflags;

	local_irq_save(flags);
	vpeflags = dvpe();

	/*
	 * Initial version makes same simple-minded assumption
	 * as is implicit elsewhere in this module, that the
	 * only RP of interest is using the first non-Linux TC.
	 * We ignore the parameters provided by the caller!
	 */
	settc(tclimit);
	/*
	 * In 34K/Malta, the only cross-VPE interrupts possible
	 * are done by setting SWINT bits in Cause, of which there
	 * are two.  SMTC uses SW1 for a multiplexed class of IPIs,
	 * and this mechanism should be generalized to APRP and use
	 * the same protocol.  Until that's implemented, send only
	 * SW0 here, regardless of requested type.
	 */
	write_vpe_c0_cause(read_vpe_c0_cause() | C_SW0);
	evpe(vpeflags);
	local_irq_restore(flags);
	return 1;
}
EXPORT_SYMBOL(vpe_send_interrupt);
/* Find some VPE program space  */
static void *alloc_progmem(void *requested, unsigned long len)
{
	void *addr;

#ifdef CONFIG_MIPS_VPE_LOADER_TOM
	/*
	 * This means you must tell Linux to use less memory than you
	 * physically have, for example by passing a mem= boot argument.
	 */
	addr = pfn_to_kaddr(max_low_pfn);
	if (requested != 0) {
		if (requested >= addr)
			addr = requested;
		else
			addr = 0;
	}
	if (addr != 0)
		memset(addr, 0, len);
	printk(KERN_DEBUG "pfn_to_kaddr returns %lu bytes of memory at %x\n",
	       len, (unsigned int)addr);
#else
	if (requested != 0) {
		/* If we have a target in mind, grab a 2x slice and hope... */
		addr = kzalloc(len*2, GFP_KERNEL);
		if ((requested >= addr) && (requested < (addr + len)))
			addr = requested;
		else
			addr = 0;
	} else {
		/* simply grab some mem for now */
		addr = kzalloc(len, GFP_KERNEL);
	}
#endif

	return addr;
}

static void release_progmem(void *ptr)
{
#ifndef CONFIG_MIPS_VPE_LOADER_TOM
	kfree(ptr);
#endif
}

/* Update size with this section: return offset. */
static long get_offset(unsigned long *size, Elf_Shdr * sechdr)
{
	long ret;

	ret = ALIGN(*size, sechdr->sh_addralign ? : 1);
	*size = ret + sechdr->sh_size;
	return ret;
}

/* Lay out the SHF_ALLOC sections in a way not dissimilar to how ld
   might -- code, read-only data, read-write data, small data.  Tally
   sizes, and place the offsets into sh_entsize fields: high bit means it
   belongs in init. */
static void layout_sections(struct module *mod, const Elf_Ehdr * hdr,
			    Elf_Shdr * sechdrs, const char *secstrings)
{
	static unsigned long const masks[][2] = {
		/* NOTE: all executable code must be the first section
		 * in this array; otherwise modify the text_size
		 * finder in the two loops below */
		{SHF_EXECINSTR | SHF_ALLOC, ARCH_SHF_SMALL},
		{SHF_ALLOC, SHF_WRITE | ARCH_SHF_SMALL},
		{SHF_WRITE | SHF_ALLOC, ARCH_SHF_SMALL},
		{ARCH_SHF_SMALL | SHF_ALLOC, 0}
	};
	unsigned int m, i;

	for (i = 0; i < hdr->e_shnum; i++)
		sechdrs[i].sh_entsize = ~0UL;

	for (m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (i = 0; i < hdr->e_shnum; ++i) {
			Elf_Shdr *s = &sechdrs[i];

			//  || strncmp(secstrings + s->sh_name, ".init", 5) == 0)
			if ((s->sh_flags & masks[m][0]) != masks[m][0]
			    || (s->sh_flags & masks[m][1])
			    || s->sh_entsize != ~0UL)
				continue;
			s->sh_entsize =
				get_offset((unsigned long *)&mod->core_size, s);
		}

		if (m == 0)
			mod->core_text_size = mod->core_size;

	}
}


/* from module-elf32.c, but subverted a little */

struct mips_hi16 {
	struct mips_hi16 *next;
	Elf32_Addr *addr;
	Elf32_Addr value;
};

static struct mips_hi16 *mips_hi16_list;
static unsigned int gp_offs, gp_addr;

static int apply_r_mips_none(struct module *me, uint32_t *location,
			     Elf32_Addr v)
{
	return 0;
}

static int apply_r_mips_gprel16(struct module *me, uint32_t *location,
				Elf32_Addr v)
{
	int rel;

	if( !(*location & 0xffff) ) {
		rel = (int)v - gp_addr;
	}
	else {
		/* .sbss + gp(relative) + offset */
		/* kludge! */
		rel =  (int)(short)((int)v + gp_offs +
				    (int)(short)(*location & 0xffff) - gp_addr);
	}

	if( (rel > 32768) || (rel < -32768) ) {
		printk(KERN_DEBUG "VPE loader: apply_r_mips_gprel16: "
		       "relative address 0x%x out of range of gp register\n",
		       rel);
		return -ENOEXEC;
	}

	*location = (*location & 0xffff0000) | (rel & 0xffff);

	return 0;
}

static int apply_r_mips_pc16(struct module *me, uint32_t *location,
			     Elf32_Addr v)
{
	int rel;
	rel = (((unsigned int)v - (unsigned int)location));
	rel >>= 2;		// because the offset is in _instructions_ not bytes.
	rel -= 1;		// and one instruction less due to the branch delay slot.

	if( (rel > 32768) || (rel < -32768) ) {
		printk(KERN_DEBUG "VPE loader: "
 		       "apply_r_mips_pc16: relative address out of range 0x%x\n", rel);
		return -ENOEXEC;
	}

	*location = (*location & 0xffff0000) | (rel & 0xffff);

	return 0;
}

static int apply_r_mips_32(struct module *me, uint32_t *location,
			   Elf32_Addr v)
{
	*location += v;

	return 0;
}

static int apply_r_mips_26(struct module *me, uint32_t *location,
			   Elf32_Addr v)
{
	if (v % 4) {
		printk(KERN_DEBUG "VPE loader: apply_r_mips_26 "
		       " unaligned relocation\n");
		return -ENOEXEC;
	}

/*
 * Not desperately convinced this is a good check of an overflow condition
 * anyway. But it gets in the way of handling undefined weak symbols which
 * we want to set to zero.
 * if ((v & 0xf0000000) != (((unsigned long)location + 4) & 0xf0000000)) {
 * printk(KERN_ERR
 * "module %s: relocation overflow\n",
 * me->name);
 * return -ENOEXEC;
 * }
 */

	*location = (*location & ~0x03ffffff) |
		((*location + (v >> 2)) & 0x03ffffff);
	return 0;
}

static int apply_r_mips_hi16(struct module *me, uint32_t *location,
			     Elf32_Addr v)
{
	struct mips_hi16 *n;

	/*
	 * We cannot relocate this one now because we don't know the value of
	 * the carry we need to add.  Save the information, and let LO16 do the
	 * actual relocation.
	 */
	n = kmalloc(sizeof *n, GFP_KERNEL);
	printk(KERN_DEBUG "Used kmalloc to allocate %d bytes at %x\n",
	       sizeof(struct mips_hi16), (unsigned int)n);
	if (!n)
		return -ENOMEM;

	n->addr = location;
	n->value = v;
	n->next = mips_hi16_list;
	mips_hi16_list = n;

	return 0;
}

static int apply_r_mips_lo16(struct module *me, uint32_t *location,
			     Elf32_Addr v)
{
	unsigned long insnlo = *location;
	Elf32_Addr val, vallo;
	struct mips_hi16 *l, *next;

	/* Sign extend the addend we extract from the lo insn.  */
	vallo = ((insnlo & 0xffff) ^ 0x8000) - 0x8000;

	if (mips_hi16_list != NULL) {

		l = mips_hi16_list;
		while (l != NULL) {
			unsigned long insn;

			/*
			 * The value for the HI16 had best be the same.
			 */
 			if (v != l->value) {
				printk(KERN_DEBUG "VPE loader: "
				       "apply_r_mips_lo16/hi16: \t"
				       "inconsistent value information\n");
				goto out_free;
			}

			/*
			 * Do the HI16 relocation.  Note that we actually don't
			 * need to know anything about the LO16 itself, except
			 * where to find the low 16 bits of the addend needed
			 * by the LO16.
			 */
			insn = *l->addr;
			val = ((insn & 0xffff) << 16) + vallo;
			val += v;

			/*
			 * Account for the sign extension that will happen in
			 * the low bits.
			 */
			val = ((val >> 16) + ((val & 0x8000) != 0)) & 0xffff;

			insn = (insn & ~0xffff) | val;
			*l->addr = insn;

			next = l->next;
			printk(KERN_DEBUG "Used kfree to free memory at %x\n",
			       (unsigned int)l);
			kfree(l);
			l = next;
		}

		mips_hi16_list = NULL;
	}

	/*
	 * Ok, we're done with the HI16 relocs.  Now deal with the LO16.
	 */
	val = v + vallo;
	insnlo = (insnlo & ~0xffff) | (val & 0xffff);
	*location = insnlo;

	return 0;

out_free:
	while (l != NULL) {
		next = l->next;
		kfree(l);
		l = next;
	}
	mips_hi16_list = NULL;

	return -ENOEXEC;
}

static int (*reloc_handlers[]) (struct module *me, uint32_t *location,
				Elf32_Addr v) = {
	[R_MIPS_NONE]	= apply_r_mips_none,
	[R_MIPS_32]	= apply_r_mips_32,
	[R_MIPS_26]	= apply_r_mips_26,
	[R_MIPS_HI16]	= apply_r_mips_hi16,
	[R_MIPS_LO16]	= apply_r_mips_lo16,
	[R_MIPS_GPREL16] = apply_r_mips_gprel16,
	[R_MIPS_PC16] = apply_r_mips_pc16
};

static char *rstrs[] = {
	[R_MIPS_NONE]	= "MIPS_NONE",
	[R_MIPS_32]	= "MIPS_32",
	[R_MIPS_26]	= "MIPS_26",
	[R_MIPS_HI16]	= "MIPS_HI16",
	[R_MIPS_LO16]	= "MIPS_LO16",
	[R_MIPS_GPREL16] = "MIPS_GPREL16",
	[R_MIPS_PC16] = "MIPS_PC16"
};

static int apply_relocations(Elf32_Shdr *sechdrs,
		      const char *strtab,
		      unsigned int symindex,
		      unsigned int relsec,
		      struct module *me)
{
	Elf32_Rel *rel = (void *) sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	uint32_t *location;
	unsigned int i;
	Elf32_Addr v;
	int res;

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		Elf32_Word r_info = rel[i].r_info;

		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;
		/* This is the symbol it is referring to */
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
			+ ELF32_R_SYM(r_info);

		if (!sym->st_value) {
			printk(KERN_DEBUG "%s: undefined weak symbol %s\n",
			       me->name, strtab + sym->st_name);
			/* just print the warning, dont barf */
		}

		v = sym->st_value;

		res = reloc_handlers[ELF32_R_TYPE(r_info)](me, location, v);
		if( res ) {
			char *r = rstrs[ELF32_R_TYPE(r_info)];
		    	printk(KERN_WARNING "VPE loader: .text+0x%x "
			       "relocation type %s for symbol \"%s\" failed\n",
			       rel[i].r_offset, r ? r : "UNKNOWN",
			       strtab + sym->st_name);
			return res;
		}
	}

	return 0;
}

void save_gp_address(unsigned int secbase, unsigned int rel)
{
	gp_addr = secbase + rel;
	gp_offs = gp_addr - (secbase & 0xffff0000);
}
/* end module-elf32.c */



/* Change all symbols so that sh_value encodes the pointer directly. */
static void simplify_symbols(Elf_Shdr * sechdrs,
			    unsigned int symindex,
			    const char *strtab,
			    const char *secstrings,
			    unsigned int nsecs, struct module *mod)
{
	Elf_Sym *sym = (void *)sechdrs[symindex].sh_addr;
	unsigned long secbase, bssbase = 0;
	unsigned int i, n = sechdrs[symindex].sh_size / sizeof(Elf_Sym);
	int size;

	/* find the .bss section for COMMON symbols */
	for (i = 0; i < nsecs; i++) {
		if (strncmp(secstrings + sechdrs[i].sh_name, ".bss", 4) == 0) {
			bssbase = sechdrs[i].sh_addr;
			break;
		}
	}

	for (i = 1; i < n; i++) {
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			/* Allocate space for the symbol in the .bss section.
			   st_value is currently size.
			   We want it to have the address of the symbol. */

			size = sym[i].st_value;
			sym[i].st_value = bssbase;

			bssbase += size;
			break;

		case SHN_ABS:
			/* Don't need to do anything */
			break;

		case SHN_UNDEF:
			/* ret = -ENOENT; */
			break;

		case SHN_MIPS_SCOMMON:
			printk(KERN_DEBUG "simplify_symbols: ignoring SHN_MIPS_SCOMMON "
			       "symbol <%s> st_shndx %d\n", strtab + sym[i].st_name,
			       sym[i].st_shndx);
			// .sbss section
			break;

		default:
			secbase = sechdrs[sym[i].st_shndx].sh_addr;

			if (strncmp(strtab + sym[i].st_name, "_gp", 3) == 0) {
				save_gp_address(secbase, sym[i].st_value);
			}

			sym[i].st_value += secbase;
			break;
		}
	}
}

#ifdef DEBUG_ELFLOADER
static void dump_elfsymbols(Elf_Shdr * sechdrs, unsigned int symindex,
			    const char *strtab, struct module *mod)
{
	Elf_Sym *sym = (void *)sechdrs[symindex].sh_addr;
	unsigned int i, n = sechdrs[symindex].sh_size / sizeof(Elf_Sym);

	printk(KERN_DEBUG "dump_elfsymbols: n %d\n", n);
	for (i = 1; i < n; i++) {
		printk(KERN_DEBUG " i %d name <%s> 0x%x\n", i,
		       strtab + sym[i].st_name, sym[i].st_value);
	}
}
#endif

/* We are prepared so configure and start the VPE... */
static int vpe_run(struct vpe * v)
{
	unsigned long flags, val, dmt_flag;
	struct vpe_notifications *n;
	unsigned int vpeflags;
	struct tc *t;

	/* check we are the Master VPE */
	local_irq_save(flags);
	val = read_c0_vpeconf0();
	if (!(val & VPECONF0_MVP)) {
		printk(KERN_WARNING
		       "VPE loader: only Master VPE's are allowed to configure MT\n");
		local_irq_restore(flags);

		return -1;
	}

	dmt_flag = dmt();
	vpeflags = dvpe();

	if (!list_empty(&v->tc)) {
		if ((t = list_entry(v->tc.next, struct tc, tc)) == NULL) {
			evpe(vpeflags);
			emt(dmt_flag);
			local_irq_restore(flags);

			printk(KERN_WARNING
			       "VPE loader: TC %d is already in use.\n",
                               t->index);
			return -ENOEXEC;
		}
	} else {
		evpe(vpeflags);
		emt(dmt_flag);
		local_irq_restore(flags);

		printk(KERN_WARNING
		       "VPE loader: No TC's associated with VPE %d\n",
		       v->minor);

		return -ENOEXEC;
	}

	/* Put MVPE's into 'configuration state' */
	set_c0_mvpcontrol(MVPCONTROL_VPC);

	settc(t->index);

	/* should check it is halted, and not activated */
	if ((read_tc_c0_tcstatus() & TCSTATUS_A) || !(read_tc_c0_tchalt() & TCHALT_H)) {
		evpe(vpeflags);
		emt(dmt_flag);
		local_irq_restore(flags);

		printk(KERN_WARNING "VPE loader: TC %d is already active!\n",
		       t->index);

		return -ENOEXEC;
	}

	/* Write the address we want it to start running from in the TCPC register. */
	write_tc_c0_tcrestart((unsigned long)v->__start);
	write_tc_c0_tccontext((unsigned long)0);

	/*
	 * Mark the TC as activated, not interrupt exempt and not dynamically
	 * allocatable
	 */
	val = read_tc_c0_tcstatus();
	val = (val & ~(TCSTATUS_DA | TCSTATUS_IXMT)) | TCSTATUS_A;
	write_tc_c0_tcstatus(val);

	write_tc_c0_tchalt(read_tc_c0_tchalt() & ~TCHALT_H);

	/*
	 * The sde-kit passes 'memsize' to __start in $a3, so set something
	 * here...  Or set $a3 to zero and define DFLT_STACK_SIZE and
	 * DFLT_HEAP_SIZE when you compile your program
	 */
	mttgpr(6, v->ntcs);
	mttgpr(7, physical_memsize);

	/* set up VPE1 */
	/*
	 * bind the TC to VPE 1 as late as possible so we only have the final
	 * VPE registers to set up, and so an EJTAG probe can trigger on it
	 */
	write_tc_c0_tcbind((read_tc_c0_tcbind() & ~TCBIND_CURVPE) | 1);

	write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() & ~(VPECONF0_VPA));

	back_to_back_c0_hazard();

	/* Set up the XTC bit in vpeconf0 to point at our tc */
	write_vpe_c0_vpeconf0( (read_vpe_c0_vpeconf0() & ~(VPECONF0_XTC))
	                      | (t->index << VPECONF0_XTC_SHIFT));

	back_to_back_c0_hazard();

	/* enable this VPE */
	write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() | VPECONF0_VPA);

	/* clear out any left overs from a previous program */
	write_vpe_c0_status(0);
	write_vpe_c0_cause(0);

	/* take system out of configuration state */
	clear_c0_mvpcontrol(MVPCONTROL_VPC);

	/*
	 * SMTC/SMVP kernels manage VPE enable independently,
	 * but uniprocessor kernels need to turn it on, even
	 * if that wasn't the pre-dvpe() state.
	 */
#ifdef CONFIG_SMP
	evpe(vpeflags);
#else
	evpe(EVPE_ENABLE);
#endif
	emt(dmt_flag);
	local_irq_restore(flags);

	list_for_each_entry(n, &v->notify, list)
		n->start(minor);

	return 0;
}

static int find_vpe_symbols(struct vpe * v, Elf_Shdr * sechdrs,
				      unsigned int symindex, const char *strtab,
				      struct module *mod)
{
	Elf_Sym *sym = (void *)sechdrs[symindex].sh_addr;
	unsigned int i, j, n = sechdrs[symindex].sh_size / sizeof(Elf_Sym);

	for (i = 1; i < n; i++) {
	    if (strcmp(strtab + sym[i].st_name, "__start") == 0)
		v->__start = sym[i].st_value;

	    if (strcmp(strtab + sym[i].st_name, "vpe_shared") == 0)
		v->shared_ptr = (void *)sym[i].st_value;

	    if (strcmp(strtab + sym[i].st_name, "_vpe_shared_areas") == 0) {
		struct vpe_shared_area *psa
		    = (struct vpe_shared_area *)sym[i].st_value;
		struct vpe_shared_area *tpsa;
		v->shared_areas = psa;
		printk(KERN_INFO"_vpe_shared_areas found, 0x%x\n",
		    (unsigned int)v->shared_areas);
		/*
		 * Copy any "published" areas to the descriptor
		 */
		for (j = 0; j < N_PUB_AREAS; j++) {
		    if (published_vpe_area[j].type != VPE_SHARED_RESERVED) {
			tpsa = psa;
			while (tpsa->type != VPE_SHARED_NULL) {
			    if ((tpsa->type == VPE_SHARED_RESERVED)
			    || (tpsa->type == published_vpe_area[j].type)) {
				tpsa->type = published_vpe_area[j].type;
				tpsa->addr = published_vpe_area[j].addr;
				break;
			    }
			    tpsa++;
			}
		    }
		}
	    }

	}

	if ( (v->__start == 0) || (v->shared_ptr == NULL))
		return -1;

	return 0;
}

/*
 * Allocates a VPE with some program code space(the load address), copies the
 * contents of the program (p)buffer performing relocatations/etc, free's it
 * when finished.
 */
static int vpe_elfload(struct vpe * v)
{
	Elf_Ehdr *hdr;
	Elf_Shdr *sechdrs;
	long err = 0;
	char *secstrings, *strtab = NULL;
	unsigned int len, i, symindex = 0, strindex = 0, relocate = 0;
	struct module mod;	// so we can re-use the relocations code

	memset(&mod, 0, sizeof(struct module));
	strcpy(mod.name, "VPE loader");
	hdr = v->l_ehdr;
	len = v->pbsize;

	/* Sanity checks against insmoding binaries or wrong arch,
	   weird elf version */
	if ((hdr->e_type != ET_REL && hdr->e_type != ET_EXEC)
	    || !elf_check_arch(hdr)
	    || hdr->e_shentsize != sizeof(*sechdrs)) {
		printk(KERN_WARNING
		       "VPE loader: program wrong arch or weird elf version\n");

		return -ENOEXEC;
	}

	if (hdr->e_type == ET_REL)
		relocate = 1;

	if (len < v->l_phlen + v->l_shlen) {
		printk(KERN_ERR "VPE loader: Headers exceed %u bytes\n", len);

		return -ENOEXEC;
	}

	/* Convenience variables */
	sechdrs = (void *)hdr + hdr->e_shoff;
	secstrings = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
	sechdrs[0].sh_addr = 0;

	/* And these should exist, but gcc whinges if we don't init them */
	symindex = strindex = 0;

	if (relocate) {
		for (i = 1; i < hdr->e_shnum; i++) {
			if (sechdrs[i].sh_type != SHT_NOBITS
			    && len < sechdrs[i].sh_offset + sechdrs[i].sh_size) {
				printk(KERN_ERR "VPE program length %u truncated\n",
				       len);
				return -ENOEXEC;
			}

			/* Mark all sections sh_addr with their address in the
			   temporary image. */
			sechdrs[i].sh_addr = (size_t) hdr + sechdrs[i].sh_offset;

			/* Internal symbols and strings. */
			if (sechdrs[i].sh_type == SHT_SYMTAB) {
				symindex = i;
				strindex = sechdrs[i].sh_link;
				strtab = (char *)hdr + sechdrs[strindex].sh_offset;
			}
		}
		layout_sections(&mod, hdr, sechdrs, secstrings);
		/*
		 * Non-relocatable loads should have already done their
		 * allocates, based on program header table.
		 */
	}

	memset(v->load_addr, 0, mod.core_size);
	if (!v->load_addr)
		return -ENOMEM;

	pr_info("VPE loader: loading to %p\n", v->load_addr);

	if (relocate) {
		for (i = 0; i < hdr->e_shnum; i++) {
			void *dest;

			if (!(sechdrs[i].sh_flags & SHF_ALLOC))
				continue;

			dest = v->load_addr + sechdrs[i].sh_entsize;

			if (sechdrs[i].sh_type != SHT_NOBITS)
				memcpy(dest, (void *)sechdrs[i].sh_addr,
				       sechdrs[i].sh_size);
			/* Update sh_addr to point to copy in image. */
			sechdrs[i].sh_addr = (unsigned long)dest;

			printk(KERN_DEBUG " section sh_name %s sh_addr 0x%x\n",
			       secstrings + sechdrs[i].sh_name, sechdrs[i].sh_addr);
		}

 		/* Fix up syms, so that st_value is a pointer to location. */
 		simplify_symbols(sechdrs, symindex, strtab, secstrings,
 				 hdr->e_shnum, &mod);

 		/* Now do relocations. */
 		for (i = 1; i < hdr->e_shnum; i++) {
 			const char *strtab = (char *)sechdrs[strindex].sh_addr;
 			unsigned int info = sechdrs[i].sh_info;

 			/* Not a valid relocation section? */
 			if (info >= hdr->e_shnum)
 				continue;

 			/* Don't bother with non-allocated sections */
 			if (!(sechdrs[info].sh_flags & SHF_ALLOC))
 				continue;

 			if (sechdrs[i].sh_type == SHT_REL)
 				err = apply_relocations(sechdrs, strtab, symindex, i,
 							&mod);
 			else if (sechdrs[i].sh_type == SHT_RELA)
 				err = apply_relocate_add(sechdrs, strtab, symindex, i,
 							 &mod);
 			if (err < 0)
 				return err;

  		}
  	} else {

		/*
		 * Program image is already in memory.
		 */
		for (i = 0; i < hdr->e_shnum; i++) {
 			/* Internal symbols and strings. */
 			if (sechdrs[i].sh_type == SHT_SYMTAB) {
 				symindex = i;
 				strindex = sechdrs[i].sh_link;
 				strtab = (char *)hdr + sechdrs[strindex].sh_offset;

 				/* mark the symtab's address for when we try to find the
 				   magic symbols */
 				sechdrs[i].sh_addr = (size_t) hdr + sechdrs[i].sh_offset;
 			}
		}
	}

	/* make sure it's physically written out */
	flush_icache_range((unsigned long)v->load_addr,
			   (unsigned long)v->load_addr + v->copied);

	if ((find_vpe_symbols(v, sechdrs, symindex, strtab, &mod)) < 0) {
		if (v->__start == 0) {
			printk(KERN_WARNING "VPE loader: program does not contain "
			       "a __start symbol\n");
			return -ENOEXEC;
		}

		if (v->shared_ptr == NULL)
			printk(KERN_WARNING "VPE loader: "
			       "program does not contain vpe_shared symbol.\n"
			       " Unable to use AMVP (AP/SP) facilities.\n");
	}
	pr_info("APRP VPE loader: elf loaded\n");

	return 0;
}

static void cleanup_tc(struct tc *tc)
{
	unsigned long flags;
	unsigned int mtflags, vpflags;
	int tmp;

	local_irq_save(flags);
	mtflags = dmt();
	vpflags = dvpe();
	/* Put MVPE's into 'configuration state' */
	set_c0_mvpcontrol(MVPCONTROL_VPC);

	settc(tc->index);
	tmp = read_tc_c0_tcstatus();

	/* mark not allocated and not dynamically allocatable */
	tmp &= ~(TCSTATUS_A | TCSTATUS_DA);
	tmp |= TCSTATUS_IXMT;	/* interrupt exempt */
	write_tc_c0_tcstatus(tmp);

	write_tc_c0_tchalt(TCHALT_H);
	mips_ihb();

	/* bind it to anything other than VPE1 */
//	write_tc_c0_tcbind(read_tc_c0_tcbind() & ~TCBIND_CURVPE); // | TCBIND_CURVPE

	clear_c0_mvpcontrol(MVPCONTROL_VPC);
	evpe(vpflags);
	emt(mtflags);
	local_irq_restore(flags);
}

static int getcwd(char *buff, int size)
{
	mm_segment_t old_fs;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_getcwd(buff, size);

	set_fs(old_fs);

	return ret;
}

/* checks VPE is unused and gets ready to load program  */
static int vpe_open(struct inode *inode, struct file *filp)
{
	enum vpe_state state;
	struct vpe_notifications *not;
	struct vpe *v;
	int ret;

	if (minor != iminor(inode)) {
		/* assume only 1 device at the moment. */
		pr_warning("VPE loader: only vpe1 is supported\n");

		return -ENODEV;
	}
	/*
	 * This treats the tclimit command line configuration input
	 * as a minor device indication, which is probably unwholesome.
	 */

	if ((v = get_vpe(tclimit)) == NULL) {
		pr_warning("VPE loader: unable to get vpe\n");

		return -ENODEV;
	}

	state = xchg(&v->state, VPE_STATE_INUSE);
	if (state != VPE_STATE_UNUSED) {
		printk(KERN_DEBUG "VPE loader: tc in use dumping regs\n");

		list_for_each_entry(not, &v->notify, list) {
			not->stop(tclimit);
		}

		release_progmem(v->load_addr);
		kfree(v->l_phsort);
		cleanup_tc(get_tc(tclimit));
	}

	/* this of-course trashes what was there before... */
	v->pbuffer = vmalloc(P_SIZE);
	v->load_addr = NULL;
	v->copied = 0;
	v->offset = 0;
	v->l_state = LOAD_STATE_EHDR;
	v->l_ehdr = NULL;
	v->l_phdr = NULL;
	v->l_phsort = NULL;
	v->l_shdr = NULL;

	v->uid = filp->f_cred->fsuid;
	v->gid = filp->f_cred->fsgid;

#ifdef CONFIG_MIPS_APSP_KSPD
	/* get kspd to tell us when a syscall_exit happens */
	if (!kspd_events_reqd) {
		kspd_notify(&kspd_events);
		kspd_events_reqd++;
	}
#endif

	v->cwd[0] = 0;
	ret = getcwd(v->cwd, VPE_PATH_MAX);
	if (ret < 0)
		printk(KERN_WARNING "VPE loader: open, getcwd returned %d\n", ret);

	v->shared_ptr = NULL;
	v->shared_areas = NULL;
	v->__start = 0;

	return 0;
}

static int vpe_release(struct inode *inode, struct file *filp)
{
	struct vpe *v;
	int ret = 0;

	v = get_vpe(tclimit);
	if (v == NULL)
		return -ENODEV;
	/*
	 * If image load had no errors, massage program/section tables
	 * to reflect movement of program/section data into VPE program
	 * memory.
	 */
	if (v->l_state != LOAD_STATE_DONE) {
		printk(KERN_WARNING "VPE Release after incomplete load\n");
		printk(KERN_DEBUG "Used vfree to free memory at "
				  "%x after failed load attempt\n",
		       (unsigned int)v->pbuffer);
		if (v->pbuffer != NULL)
			vfree(v->pbuffer);
		return -ENOEXEC;
	}

	if (vpe_elfload(v) >= 0) {
		vpe_run(v);
	} else {
		printk(KERN_WARNING "VPE loader: ELF load failed.\n");
		printk(KERN_DEBUG "Used vfree to free memory at "
				  "%x after failed load attempt\n",
		       (unsigned int)v->pbuffer);
		if (v->pbuffer != NULL)
			vfree(v->pbuffer);
		ret = -ENOEXEC;
	}


	/* It's good to be able to run the SP and if it chokes have a look at
	   the /dev/rt?. But if we reset the pointer to the shared struct we
	   lose what has happened. So perhaps if garbage is sent to the vpe
	   device, use it as a trigger for the reset. Hopefully a nice
	   executable will be along shortly. */
	if (ret < 0)
		v->shared_ptr = NULL;

	// cleanup any temp buffers
	if (v->pbuffer) {
		printk(KERN_DEBUG "Used vfree to free memory at %x\n",
		       (unsigned int)v->pbuffer);
		vfree(v->pbuffer);
	}
	v->pbsize = 0;
	return ret;
}

/*
 * A sort of insertion sort to generate list of program header indices
 * in order of their file offsets.
 */

static void indexort(struct elf_phdr *phdr, int nph, int *index)
{
	int i, j, t;
	unsigned int toff;

	/* Create initial mapping */
	for (i = 0; i < nph; i++)
		index[i] = i;
	/* Do the indexed insert sort */
	for (i = 1; i < nph; i++) {
		j = i;
		t = index[j];
		toff = phdr[t].p_offset;
		while ((j > 0) && (phdr[index[j-1]].p_offset > toff)) {
			index[j] = index[j-1];
			j--;
		}
		index[j] = t;
	}
}


/*
 * This function has to convert the ELF file image being sequentially
 * streamed to the pseudo-device into the binary image, symbol, and
 * string information, which the ELF format allows to be in some degree
 * of disorder.
 *
 * The ELF header and, if present, program header table, are copied into
 * a temporary buffer.  Loadable program segments, if present, are copied
 * into the RP program memory at the addresses specified by the program
 * header table.
 *
 * Sections not specified by the program header table are loaded into
 * memory following the program segments if they are "allocated", or
 * into the temporary buffer if they are not. The section header
 * table is loaded into the temporary buffer.???
 */
#define CURPHDR (v->l_phdr[v->l_phsort[v->l_cur_seg]])
static ssize_t vpe_write(struct file *file, const char __user * buffer,
			 size_t count, loff_t * ppos)
{
	size_t ret = count;
	struct vpe *v;
	int tocopy, uncopied;
	int i;
	unsigned int progmemlen;

	if (iminor(file->f_path.dentry->d_inode) != minor)
		return -ENODEV;

	v = get_vpe(tclimit);
	if (v == NULL)
		return -ENODEV;

	if (v->pbuffer == NULL) {
		printk(KERN_ERR "VPE loader: no buffer for program\n");
		return -ENOMEM;
	}

	while (count) {
		switch (v->l_state) {
		case LOAD_STATE_EHDR:
			/* Loading ELF Header into scratch buffer */
			tocopy = min((unsigned long)count,
				     sizeof(Elf_Ehdr) - v->offset);
			uncopied = copy_from_user(v->pbuffer + v->copied,
						  buffer, tocopy);
			count -= tocopy - uncopied;
			v->copied += tocopy - uncopied;
			v->offset += tocopy - uncopied;
			buffer += tocopy - uncopied;
			if (v->copied == sizeof(Elf_Ehdr)) {
			    v->l_ehdr = (Elf_Ehdr *)v->pbuffer;
			    if (memcmp(v->l_ehdr->e_ident, ELFMAG, 4) != 0) {
				printk(KERN_WARNING "VPE loader: %s\n",
					"non-ELF file image");
				ret = -ENOEXEC;
				v->l_state = LOAD_STATE_ERROR;
				break;
			    }
			    if (v->l_ehdr->e_phoff != 0) {
				v->l_phdr = (struct elf_phdr *)
					(v->pbuffer + v->l_ehdr->e_phoff);
				v->l_phlen = v->l_ehdr->e_phentsize
					* v->l_ehdr->e_phnum;
				/* Check against buffer overflow */
				if ((v->copied + v->l_phlen) > v->pbsize) {
					printk(KERN_WARNING
		       "VPE loader: elf program header table size too big\n");
					v->l_state = LOAD_STATE_ERROR;
					return -ENOMEM;
				}
				v->l_state = LOAD_STATE_PHDR;
				/*
				 * Program headers generally indicate
				 * linked executable with possibly
				 * valid entry point.
				 */
				v->__start = v->l_ehdr->e_entry;
			    } else  if (v->l_ehdr->e_shoff != 0) {
				/*
				 * No program headers, but a section
				 * header table.  A relocatable binary.
				 * We need to load the works into the
				 * kernel temp buffer to compute the
				 * RP program image.  That limits our
				 * binary size, but at least we're no
				 * worse off than the original APRP
				 * prototype.
				 */
				v->l_shlen = v->l_ehdr->e_shentsize
					* v->l_ehdr->e_shnum;
				if ((v->l_ehdr->e_shoff + v->l_shlen
				     - v->offset) > v->pbsize) {
					printk(KERN_WARNING
			 "VPE loader: elf sections/section table too big.\n");
					v->l_state = LOAD_STATE_ERROR;
					return -ENOMEM;
				}
				v->l_state = LOAD_STATE_SHDR;
			    } else {
				/*
				 * If neither program nor section tables,
				 * we don't know what to do.
				 */
				v->l_state = LOAD_STATE_ERROR;
				return -ENOEXEC;
			    }
			}
			break;
		case LOAD_STATE_PHDR:
			/* Loading Program Headers into scratch */
			tocopy = min((unsigned long)count,
			    v->l_ehdr->e_phoff + v->l_phlen - v->copied);
			uncopied = copy_from_user(v->pbuffer + v->copied,
			    buffer, tocopy);
			count -= tocopy - uncopied;
			v->copied += tocopy - uncopied;
			v->offset += tocopy - uncopied;
			buffer += tocopy - uncopied;

			if (v->copied == v->l_ehdr->e_phoff + v->l_phlen) {
			    /*
			     * It's legal for the program headers to be
			     * out of order with respect to the file layout.
			     * Generate a list of indices, sorted by file
			     * offset.
			     */
			    v->l_phsort = kmalloc(v->l_ehdr->e_phnum
				* sizeof(int), GFP_KERNEL);
			    printk(KERN_DEBUG
		   "Used kmalloc to allocate %d bytes of memory at %x\n",
				   v->l_ehdr->e_phnum*sizeof(int),
				   (unsigned int)v->l_phsort);
			    if (!v->l_phsort)
				    return -ENOMEM; /* Preposterous, but... */
			    indexort(v->l_phdr, v->l_ehdr->e_phnum,
				     v->l_phsort);

			    v->l_progminad = (unsigned int)-1;
			    v->l_progmaxad = 0;
			    progmemlen = 0;
			    for (i = 0; i < v->l_ehdr->e_phnum; i++) {
				if (v->l_phdr[v->l_phsort[i]].p_type
				    == PT_LOAD) {
				    /* Unstripped .reginfo sections are bad */
				    if (v->l_phdr[v->l_phsort[i]].p_vaddr
					< __UA_LIMIT) {
					printk(KERN_WARNING "%s%s%s\n",
					    "VPE loader: ",
					    "User-mode p_vaddr, ",
					    "skipping program segment,");
					printk(KERN_WARNING "%s%s%s\n",
					    "VPE loader: ",
					    "strip .reginfo from binary ",
					    "if necessary.");
					continue;
				    }
				    if (v->l_phdr[v->l_phsort[i]].p_vaddr
					< v->l_progminad)
					    v->l_progminad =
					      v->l_phdr[v->l_phsort[i]].p_vaddr;
				    if ((v->l_phdr[v->l_phsort[i]].p_vaddr
					+ v->l_phdr[v->l_phsort[i]].p_memsz)
					> v->l_progmaxad)
					    v->l_progmaxad =
					     v->l_phdr[v->l_phsort[i]].p_vaddr +
					     v->l_phdr[v->l_phsort[i]].p_memsz;
				}
			    }
			    printk(KERN_INFO "APRP RP program 0x%x to 0x%x\n",
				v->l_progminad, v->l_progmaxad);
			    /*
			     * Do a simple sanity check of the memory being
			     * allocated. Abort if greater than an arbitrary
			     * value of 32MB
			     */
			    if (v->l_progmaxad - v->l_progminad >
				32*1024*1024) {
				printk(KERN_WARNING
	      "RP program failed to allocate %d kbytes - limit is 32,768 KB\n",
				       (v->l_progmaxad - v->l_progminad)/1024);
				return -ENOMEM;
			      }

			    v->load_addr = alloc_progmem((void *)v->l_progminad,
				v->l_progmaxad - v->l_progminad);
			    if (!v->load_addr)
				return -ENOMEM;
			    if ((unsigned int)v->load_addr
				> v->l_progminad) {
				release_progmem(v->load_addr);
				return -ENOMEM;
			    }
			    /* Find first segment with loadable content */
			    for (i = 0; i < v->l_ehdr->e_phnum; i++) {
				if (v->l_phdr[v->l_phsort[i]].p_type
				    == PT_LOAD) {
				    if (v->l_phdr[v->l_phsort[i]].p_vaddr
					< __UA_LIMIT) {
					/* Skip userspace segments */
					continue;
				    }
				    v->l_cur_seg = i;
				    break;
				}
			    }
			    if (i == v->l_ehdr->e_phnum) {
				/* No loadable program segment?  Bogus file. */
				printk(KERN_WARNING "Bad ELF file for APRP\n");
				return -ENOEXEC;
			    }
			    v->l_segoff = 0;
			    v->l_state = LOAD_STATE_PIMAGE;
			}
			break;
		case LOAD_STATE_PIMAGE:
			/*
			 * Skip through input stream until
			 * first program segment. Would be
			 * better to have loaded up to here
			 * into the temp buffer, but for now
			 * we simply rule out "interesting"
			 * sections prior to the last program
			 * segment in an executable file.
			 */
			if (v->offset < CURPHDR.p_offset) {
			    uncopied = CURPHDR.p_offset - v->offset;
			    if (uncopied > count)
				uncopied = count;
			    count -= uncopied;
			    buffer += uncopied;
			    v->offset += uncopied;
			    /* Go back through the "while" */
			    break;
			}
			/*
			 * Having dispensed with any unlikely fluff,
			 * copy from user I/O buffer to program segment.
			 */
			tocopy = min(count, CURPHDR.p_filesz - v->l_segoff);

			/* Loading image into RP memory */
			uncopied = copy_from_user((char *)CURPHDR.p_vaddr
			    + v->l_segoff, buffer, tocopy);
			count -= tocopy - uncopied;
			v->offset += tocopy - uncopied;
			v->l_segoff += tocopy - uncopied;
			buffer += tocopy - uncopied;
			if (v->l_segoff >= CURPHDR.p_filesz) {
			    /* Finished current segment load */
			    /* Zero out non-file-sourced image */
			    uncopied = CURPHDR.p_memsz - CURPHDR.p_filesz;
			    if (uncopied > 0)
				memset((char *)CURPHDR.p_vaddr + v->l_segoff,
				    0, uncopied);
			    /* Advance to next segment */
			    for (i = v->l_cur_seg + 1;
				i < v->l_ehdr->e_phnum; i++) {
				if (v->l_phdr[v->l_phsort[i]].p_type
				    == PT_LOAD) {
				    if (v->l_phdr[v->l_phsort[i]].p_vaddr
					< __UA_LIMIT) {
					/* Skip userspace segments */
					continue;
				    }
				    v->l_cur_seg = i;
				    break;
				}
			    }
			    /* If none left, prepare to load section headers */
			    if (i == v->l_ehdr->e_phnum) {
				if (v->l_ehdr->e_shoff != 0) {
				/* Copy to where we left off in temp buffer */
				    v->l_shlen = v->l_ehdr->e_shentsize
					* v->l_ehdr->e_shnum;
				    if ((v->l_ehdr->e_shoff + v->l_shlen
					- v->offset) > v->pbsize) {
					printk(KERN_WARNING
			   "VPE loader: elf sections/section table too big\n");
					v->l_state = LOAD_STATE_ERROR;
					return -ENOMEM;
				    }
				    v->l_state = LOAD_STATE_SHDR;
				    break;
				}
			    } else {
				/* reset offset for new program segment */
				v->l_segoff = 0;
			    }
			}
			break;
		case LOAD_STATE_SHDR:
			/*
			 * Read stream into private buffer up
			 * through and including the section header
			 * table.
			 */

			tocopy = min((unsigned long)count,
			    v->l_ehdr->e_shoff + v->l_shlen - v->offset);
			if (tocopy) {
			    uncopied = copy_from_user(v->pbuffer + v->copied,
			    buffer, tocopy);
			    count -= tocopy - uncopied;
			    v->copied += tocopy - uncopied;
			    v->offset += tocopy - uncopied;
			    buffer += tocopy - uncopied;
			}
			/* Finished? */
			if (v->offset == v->l_ehdr->e_shoff + v->l_shlen) {
			    unsigned int offset_delta = v->offset - v->copied;

			    v->l_shdr = (Elf_Shdr *)(v->pbuffer
				+ v->l_ehdr->e_shoff - offset_delta);
			    /*
			     * Check for sections after the section table,
			     * which for gcc MIPS binaries includes
			     * the symbol table. Do any other processing
			     * that requires value within stream, and
			     * normalize offsets to be relative to
			     * the header-only layout of temp buffer.
			     */

			    /* Assume no trailer until we detect one */
			    v->l_trailer = 0;
			    v->l_state = LOAD_STATE_DONE;
			    for (i = 0; i < v->l_ehdr->e_shnum; i++) {
				   if (v->l_shdr[i].sh_offset
					> v->l_ehdr->e_shoff) {
					v->l_state = LOAD_STATE_TRAILER;
					/* Track trailing data length */
					if (v->l_trailer
					    < (v->l_shdr[i].sh_offset
					    + v->l_shdr[i].sh_size)
					    - (v->l_ehdr->e_shoff
					    + v->l_shlen))
						v->l_trailer =
						    (v->l_shdr[i].sh_offset
						    + v->l_shdr[i].sh_size)
						    - (v->l_ehdr->e_shoff
						    + v->l_shlen);
				    }
				    /* Adjust section offset if necessary */
				    v->l_shdr[i].sh_offset -= offset_delta;
			    }
			    if ((v->copied + v->l_trailer) > v->pbsize) {
				printk(KERN_WARNING
	      "VPE loader: elf size too big. Perhaps strip uneeded symbols\n");
				v->l_state = LOAD_STATE_ERROR;
				return -ENOMEM;
			    }

			    /* Fix up offsets in ELF header */
			    v->l_ehdr->e_shoff = (unsigned int)v->l_shdr
				- (unsigned int)v->pbuffer;
			}
			break;
		case LOAD_STATE_TRAILER:
			/*
			 * Symbol and string tables follow section headers
			 * in gcc binaries for MIPS. Copy into temp buffer.
			 */
			if (v->l_trailer) {
			    tocopy = min(count, v->l_trailer);
			    uncopied = copy_from_user(v->pbuffer + v->copied,
			    buffer, tocopy);
			    count -= tocopy - uncopied;
			    v->l_trailer -= tocopy - uncopied;
			    v->copied += tocopy - uncopied;
			    v->offset += tocopy - uncopied;
			    buffer += tocopy - uncopied;
			}
			if (!v->l_trailer)
			    v->l_state = LOAD_STATE_DONE;
			break;
		case LOAD_STATE_DONE:
			if (count)
				count = 0;
			break;
		case LOAD_STATE_ERROR:
		default:
			return -EINVAL;
		}
	}
	return ret;
}

static const struct file_operations vpe_fops = {
	.owner = THIS_MODULE,
	.open = vpe_open,
	.release = vpe_release,
	.write = vpe_write
};

/* module wrapper entry points */
/* give me a vpe */
vpe_handle vpe_alloc(void)
{
	int i;
	struct vpe *v;

	/* find a vpe */
	for (i = 1; i < MAX_VPES; i++) {
		if ((v = get_vpe(i)) != NULL) {
			v->state = VPE_STATE_INUSE;
			return v;
		}
	}
	return NULL;
}

EXPORT_SYMBOL(vpe_alloc);

/* start running from here */
int vpe_start(vpe_handle vpe, unsigned long start)
{
	struct vpe *v = vpe;

	/* Null start address means use value from ELF file */
	if (start)
		v->__start = start;
	return vpe_run(v);
}

EXPORT_SYMBOL(vpe_start);

/* halt it for now */
int vpe_stop(vpe_handle vpe)
{
	struct vpe *v = vpe;
	struct tc *t;
	unsigned int evpe_flags;

	evpe_flags = dvpe();

	if ((t = list_entry(v->tc.next, struct tc, tc)) != NULL) {

		settc(t->index);
		write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() & ~VPECONF0_VPA);
	}

	evpe(evpe_flags);

	return 0;
}

EXPORT_SYMBOL(vpe_stop);

/* I've done with it thank you */
int vpe_free(vpe_handle vpe)
{
	struct vpe *v = vpe;
	struct tc *t;
	unsigned int evpe_flags;

	if ((t = list_entry(v->tc.next, struct tc, tc)) == NULL) {
		return -ENOEXEC;
	}

	evpe_flags = dvpe();

	/* Put MVPE's into 'configuration state' */
	set_c0_mvpcontrol(MVPCONTROL_VPC);

	settc(t->index);
	write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() & ~VPECONF0_VPA);

	/* halt the TC */
	write_tc_c0_tchalt(TCHALT_H);
	mips_ihb();

	/* mark the TC unallocated */
	write_tc_c0_tcstatus(read_tc_c0_tcstatus() & ~TCSTATUS_A);

	v->state = VPE_STATE_UNUSED;

	clear_c0_mvpcontrol(MVPCONTROL_VPC);
	evpe(evpe_flags);

	return 0;
}

EXPORT_SYMBOL(vpe_free);

void *vpe_get_shared(int index)
{
	struct vpe *v;

	if ((v = get_vpe(index)) == NULL)
		return NULL;

	return v->shared_ptr;
}

EXPORT_SYMBOL(vpe_get_shared);

int vpe_getuid(int index)
{
	struct vpe *v;

	if ((v = get_vpe(index)) == NULL)
		return -1;

	return v->uid;
}

EXPORT_SYMBOL(vpe_getuid);

int vpe_getgid(int index)
{
	struct vpe *v;

	if ((v = get_vpe(index)) == NULL)
		return -1;

	return v->gid;
}

EXPORT_SYMBOL(vpe_getgid);

int vpe_notify(int index, struct vpe_notifications *notify)
{
	struct vpe *v;

	if ((v = get_vpe(index)) == NULL)
		return -1;

	list_add(&notify->list, &v->notify);
	return 0;
}

EXPORT_SYMBOL(vpe_notify);

char *vpe_getcwd(int index)
{
	struct vpe *v;

	if ((v = get_vpe(index)) == NULL)
		return NULL;

	return v->cwd;
}

EXPORT_SYMBOL(vpe_getcwd);

/*
 * RP applications may contain a _vpe_shared_area descriptor
 * array to allow for data sharing with Linux kernel functions
 * that's slightly more abstracted and extensible than the
 * fixed binding used by the rtlx support.  Indeed, the rtlx
 * support should ideally be converted to use the generic
 * shared area descriptor scheme at some point.
 *
 * mips_get_vpe_shared_area() can be used by AP kernel
 * modules to get an area pointer of a given type, if
 * it exists.
 *
 * mips_publish_vpe_area() is used by AP kernel modules
 * to share kseg0 kernel memory with the RP.  It maintains
 * a private table, so that publishing can be done before
 * the RP program is launched.  Making this table dynamically
 * allocated and extensible would be good scalable OS design.
 * however, until there's more than one user of the mechanism,
 * it should be an acceptable simplification to allow a static
 * maximum of 4 published areas.
 */

void *mips_get_vpe_shared_area(int index, int type)
{
	struct vpe *v;
	struct vpe_shared_area *vsa;

	v = get_vpe(index);
	if (v == NULL)
		return NULL;

	if (v->shared_areas == NULL)
		return NULL;

	vsa = v->shared_areas;

	while (vsa->type != VPE_SHARED_NULL) {
		if (vsa->type == type)
			return vsa->addr;
		else
			vsa++;
	}
	/* Fell through without finding type */

	return NULL;
}
EXPORT_SYMBOL(mips_get_vpe_shared_area);

int  mips_publish_vpe_area(int type, void *ptr)
{
	int i;
	int retval = 0;
	struct vpe *v;
	unsigned long flags;
	unsigned int vpflags;

	printk(KERN_INFO "mips_publish_vpe_area(0x%x, 0x%x)\n", type, (int)ptr);
	if ((unsigned int)ptr >= KSEG2) {
	    printk(KERN_ERR "VPE area pubish of invalid address 0x%x\n",
		(int)ptr);
	    return 0;
	}
	for (i = 0; i < N_PUB_AREAS; i++) {
	    if (published_vpe_area[i].type == VPE_SHARED_RESERVED) {
		published_vpe_area[i].type = type;
		published_vpe_area[i].addr = ptr;
		retval = type;
		break;
	    }
	}
	/*
	 * If we've already got a VPE up and running, try to
	 * update the shared descriptor with the new data.
	 */
	list_for_each_entry(v, &vpecontrol.vpe_list, list) {
	    if (v->shared_areas != NULL) {
		local_irq_save(flags);
		vpflags = dvpe();
		for (i = 0; v->shared_areas[i].type != VPE_SHARED_NULL; i++) {
		    if ((v->shared_areas[i].type == type)
		    || (v->shared_areas[i].type == VPE_SHARED_RESERVED)) {
			v->shared_areas[i].type = type;
			v->shared_areas[i].addr = ptr;
		    }
		}
		evpe(vpflags);
		local_irq_restore(flags);
	    }
	}
	return retval;
}
EXPORT_SYMBOL(mips_publish_vpe_area);

#ifdef CONFIG_MIPS_APSP_KSPD
static void kspd_sp_exit( int sp_id)
{
	cleanup_tc(get_tc(sp_id));
}
#endif

static ssize_t store_kill(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct vpe *vpe = get_vpe(tclimit);
	struct vpe_notifications *not;

	list_for_each_entry(not, &vpe->notify, list) {
		not->stop(tclimit);
	}

	release_progmem(vpe->load_addr);
	kfree(vpe->l_phsort);
	cleanup_tc(get_tc(tclimit));
	vpe_stop(vpe);
	vpe_free(vpe);

	return len;
}

static ssize_t show_ntcs(struct device *cd, struct device_attribute *attr,
			 char *buf)
{
	struct vpe *vpe = get_vpe(tclimit);

	return sprintf(buf, "%d\n", vpe->ntcs);
}

static ssize_t store_ntcs(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct vpe *vpe = get_vpe(tclimit);
	unsigned long new;
	char *endp;

	new = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		goto out_einval;

	if (new == 0 || new > (hw_tcs - tclimit))
		goto out_einval;

	vpe->ntcs = new;

	return len;

out_einval:
	return -EINVAL;
}

static struct device_attribute vpe_class_attributes[] = {
	__ATTR(kill, S_IWUSR, NULL, store_kill),
	__ATTR(ntcs, S_IRUGO | S_IWUSR, show_ntcs, store_ntcs),
	{}
};

static void vpe_device_release(struct device *cd)
{
	printk(KERN_DEBUG "Using kfree to free vpe class device at %x\n",
	       (unsigned int)cd);
	kfree(cd);
}

struct class vpe_class = {
	.name = "vpe",
	.owner = THIS_MODULE,
	.dev_release = vpe_device_release,
	.dev_attrs = vpe_class_attributes,
};

struct device vpe_device;

static int __init vpe_module_init(void)
{
	unsigned int mtflags, vpflags;
	unsigned long flags, val;
	struct vpe *v = NULL;
	struct tc *t;
	int tc, err;

	if (!cpu_has_mipsmt) {
		printk("VPE loader: not a MIPS MT capable processor\n");
		return -ENODEV;
	}

	if (vpelimit == 0) {
#if defined(CONFIG_MIPS_MT_SMTC) || defined(MIPS_MT_SMP)
		printk(KERN_WARNING "No VPEs reserved for VPE loader.\n"
			"Pass maxvpes=<n> argument as kernel argument\n");
		return -ENODEV;
#else
		vpelimit = 1;
#endif
	}

	if (tclimit == 0) {
#if defined(CONFIG_MIPS_MT_SMTC) || defined(MIPS_MT_SMP)
		printk(KERN_WARNING "No TCs reserved for AP/SP, not "
		       "initializing VPE loader.\nPass maxtcs=<n> argument as "
		       "kernel argument\n");
		return -ENODEV;
#else
		tclimit = 1;
#endif
	}

	major = register_chrdev(0, module_name, &vpe_fops);
	if (major < 0) {
		printk("VPE loader: unable to register character device\n");
		return major;
	}

	err = class_register(&vpe_class);
	if (err) {
		printk(KERN_ERR "vpe_class registration failed\n");
		goto out_chrdev;
	}
	xvpe_vector_set = 0;
	device_initialize(&vpe_device);
	vpe_device.class	= &vpe_class,
	vpe_device.parent	= NULL,
	dev_set_name(&vpe_device, "vpe1");
	vpe_device.devt = MKDEV(major, minor);
	err = device_add(&vpe_device);
	if (err) {
		printk(KERN_ERR "Adding vpe_device failed\n");
		goto out_class;
	}

	local_irq_save(flags);
	mtflags = dmt();
	vpflags = dvpe();

	/* Put MVPE's into 'configuration state' */
	set_c0_mvpcontrol(MVPCONTROL_VPC);

	/* dump_mtregs(); */

	val = read_c0_mvpconf0();
	hw_tcs = (val & MVPCONF0_PTC) + 1;
	hw_vpes = ((val & MVPCONF0_PVPE) >> MVPCONF0_PVPE_SHIFT) + 1;

	for (tc = tclimit; tc < hw_tcs; tc++) {
		/*
		 * Must re-enable multithreading temporarily or in case we
		 * reschedule send IPIs or similar we might hang.
		 */
		clear_c0_mvpcontrol(MVPCONTROL_VPC);
		evpe(vpflags);
		emt(mtflags);
		local_irq_restore(flags);
		t = alloc_tc(tc);
		if (!t) {
			err = -ENOMEM;
			goto out;
		}

		local_irq_save(flags);
		mtflags = dmt();
		vpflags = dvpe();
		set_c0_mvpcontrol(MVPCONTROL_VPC);

		/* VPE's */
		if (tc < hw_tcs) {
			settc(tc);

			if ((v = alloc_vpe(tc)) == NULL) {
				printk(KERN_WARNING "VPE: unable to allocate VPE\n");

				goto out_reenable;
			}

			v->ntcs = hw_tcs - tclimit;

			/* add the tc to the list of this vpe's tc's. */
			list_add(&t->tc, &v->tc);

			/* deactivate all but vpe0 */
			if (tc >= tclimit) {
				unsigned long tmp = read_vpe_c0_vpeconf0();

				tmp &= ~VPECONF0_VPA;

				/* master VPE */
				tmp |= VPECONF0_MVP;
				write_vpe_c0_vpeconf0(tmp);
			}

			/* disable multi-threading with TC's */
			write_vpe_c0_vpecontrol(read_vpe_c0_vpecontrol() & ~VPECONTROL_TE);

			if (tc >= vpelimit) {
				/*
				 * Set config to be the same as vpe0,
				 * particularly kseg0 coherency alg
				 */
				write_vpe_c0_config(read_c0_config());
			}
		}

		/* TC's */
		t->pvpe = v;	/* set the parent vpe */

		if (tc >= tclimit) {
			unsigned long tmp;

			settc(tc);

			/* Any TC that is bound to VPE0 gets left as is - in case
			   we are running SMTC on VPE0. A TC that is bound to any
			   other VPE gets bound to VPE0, ideally I'd like to make
			   it homeless but it doesn't appear to let me bind a TC
			   to a non-existent VPE. Which is perfectly reasonable.

			   The (un)bound state is visible to an EJTAG probe so may
			   notify GDB...
			*/

			if (((tmp = read_tc_c0_tcbind()) & TCBIND_CURVPE)) {
				/* tc is bound >vpe0 */
				write_tc_c0_tcbind(tmp & ~TCBIND_CURVPE);

				t->pvpe = get_vpe(0);	/* set the parent vpe */
			}

			/* halt the TC */
			write_tc_c0_tchalt(TCHALT_H);
			mips_ihb();

			tmp = read_tc_c0_tcstatus();

			/* mark not activated and not dynamically allocatable */
			tmp &= ~(TCSTATUS_A | TCSTATUS_DA);
			tmp |= TCSTATUS_IXMT;	/* interrupt exempt */
			write_tc_c0_tcstatus(tmp);
		}
	}

out_reenable:
	/* release config state */
	clear_c0_mvpcontrol(MVPCONTROL_VPC);

	evpe(vpflags);
	emt(mtflags);
	local_irq_restore(flags);

#ifdef CONFIG_MIPS_APSP_KSPD
	kspd_events.kspd_sp_exit = kspd_sp_exit;
#endif
	return 0;

out_class:
	class_unregister(&vpe_class);
out_chrdev:
	unregister_chrdev(major, module_name);

out:
	return err;
}

static void __exit vpe_module_exit(void)
{
	struct vpe *v, *n;

	device_del(&vpe_device);
	unregister_chrdev(major, module_name);

	/* No locking needed here */
	list_for_each_entry_safe(v, n, &vpecontrol.vpe_list, list) {
		if (v->state != VPE_STATE_UNUSED)
			release_vpe(v);
	}
}

module_init(vpe_module_init);
module_exit(vpe_module_exit);
MODULE_DESCRIPTION("MIPS VPE Loader");
MODULE_AUTHOR("Elizabeth Oldham, MIPS Technologies, Inc.");
MODULE_LICENSE("GPL");
