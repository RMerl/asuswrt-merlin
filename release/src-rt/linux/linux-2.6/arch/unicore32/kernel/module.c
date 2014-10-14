/*
 * linux/arch/unicore32/kernel/module.c
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/gfp.h>

#include <asm/pgtable.h>
#include <asm/sections.h>

void *module_alloc(unsigned long size)
{
	struct vm_struct *area;

	size = PAGE_ALIGN(size);
	if (!size)
		return NULL;

	area = __get_vm_area(size, VM_ALLOC, MODULES_VADDR, MODULES_END);
	if (!area)
		return NULL;

	return __vmalloc_area(area, GFP_KERNEL, PAGE_KERNEL_EXEC);
}

void module_free(struct module *module, void *region)
{
	vfree(region);
}

int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{
	return 0;
}

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
	       unsigned int relindex, struct module *module)
{
	Elf32_Shdr *symsec = sechdrs + symindex;
	Elf32_Shdr *relsec = sechdrs + relindex;
	Elf32_Shdr *dstsec = sechdrs + relsec->sh_info;
	Elf32_Rel *rel = (void *)relsec->sh_addr;
	unsigned int i;

	for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
		unsigned long loc;
		Elf32_Sym *sym;
		s32 offset;

		offset = ELF32_R_SYM(rel->r_info);
		if (offset < 0 || offset >
				(symsec->sh_size / sizeof(Elf32_Sym))) {
			printk(KERN_ERR "%s: bad relocation, "
					"section %d reloc %d\n",
					module->name, relindex, i);
			return -ENOEXEC;
		}

		sym = ((Elf32_Sym *)symsec->sh_addr) + offset;

		if (rel->r_offset < 0 || rel->r_offset >
				dstsec->sh_size - sizeof(u32)) {
			printk(KERN_ERR "%s: out of bounds relocation, "
				"section %d reloc %d offset %d size %d\n",
				module->name, relindex, i, rel->r_offset,
				dstsec->sh_size);
			return -ENOEXEC;
		}

		loc = dstsec->sh_addr + rel->r_offset;

		switch (ELF32_R_TYPE(rel->r_info)) {
		case R_UNICORE_NONE:
			/* ignore */
			break;

		case R_UNICORE_ABS32:
			*(u32 *)loc += sym->st_value;
			break;

		case R_UNICORE_PC24:
		case R_UNICORE_CALL:
		case R_UNICORE_JUMP24:
			offset = (*(u32 *)loc & 0x00ffffff) << 2;
			if (offset & 0x02000000)
				offset -= 0x04000000;

			offset += sym->st_value - loc;
			if (offset & 3 ||
			    offset <= (s32)0xfe000000 ||
			    offset >= (s32)0x02000000) {
				printk(KERN_ERR
				       "%s: relocation out of range, section "
				       "%d reloc %d sym '%s'\n", module->name,
				       relindex, i, strtab + sym->st_name);
				return -ENOEXEC;
			}

			offset >>= 2;

			*(u32 *)loc &= 0xff000000;
			*(u32 *)loc |= offset & 0x00ffffff;
			break;

		default:
			printk(KERN_ERR "%s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel->r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int
apply_relocate_add(Elf32_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec,
		   struct module *module)
{
	printk(KERN_ERR "module %s: ADD RELOCATION unsupported\n",
	       module->name);
	return -ENOEXEC;
}

int
module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		struct module *module)
{
	return 0;
}

void
module_arch_cleanup(struct module *mod)
{
}
