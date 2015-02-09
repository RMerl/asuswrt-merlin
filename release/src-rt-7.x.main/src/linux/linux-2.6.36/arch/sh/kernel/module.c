/*  Kernel module help for SH.

    SHcompact version by Kaz Kojima and Paul Mundt.

    SHmedia bits:

	Copyright 2004 SuperH (UK) Ltd
	Author: Richard Curnow

	Based on the sh version, and on code from the sh64-specific parts of
	modutils, originally written by Richard Curnow and Ben Gaster.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/bug.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <asm/dwarf.h>

void *module_alloc(unsigned long size)
{
	if (size == 0)
		return NULL;

	return vmalloc_exec(size);
}


/* Free memory returned from module_alloc */
void module_free(struct module *mod, void *module_region)
{
	vfree(module_region);
}

/* We don't need anything special. */
int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{
	return 0;
}

int apply_relocate_add(Elf32_Shdr *sechdrs,
		   const char *strtab,
		   unsigned int symindex,
		   unsigned int relsec,
		   struct module *me)
{
	unsigned int i;
	Elf32_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	Elf32_Addr relocation;
	uint32_t *location;
	uint32_t value;

	pr_debug("Applying relocate section %u to %u\n", relsec,
		 sechdrs[relsec].sh_info);
	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;
		/* This is the symbol it is referring to.  Note that all
		   undefined symbols have been resolved.  */
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
			+ ELF32_R_SYM(rel[i].r_info);
		relocation = sym->st_value + rel[i].r_addend;

#ifdef CONFIG_SUPERH64
		/* For text addresses, bit2 of the st_other field indicates
		 * whether the symbol is SHmedia (1) or SHcompact (0).  If
		 * SHmedia, the LSB of the symbol needs to be asserted
		 * for the CPU to be in SHmedia mode when it starts executing
		 * the branch target. */
		relocation |= !!(sym->st_other & 4);
#endif

		switch (ELF32_R_TYPE(rel[i].r_info)) {
		case R_SH_DIR32:
			value = get_unaligned(location);
			value += relocation;
			put_unaligned(value, location);
			break;
		case R_SH_REL32:
			relocation = (relocation - (Elf32_Addr) location);
			value = get_unaligned(location);
			value += relocation;
			put_unaligned(value, location);
			break;
		case R_SH_IMM_LOW16:
			*location = (*location & ~0x3fffc00) |
				((relocation & 0xffff) << 10);
			break;
		case R_SH_IMM_MEDLOW16:
			*location = (*location & ~0x3fffc00) |
				(((relocation >> 16) & 0xffff) << 10);
			break;
		case R_SH_IMM_LOW16_PCREL:
			relocation -= (Elf32_Addr) location;
			*location = (*location & ~0x3fffc00) |
				((relocation & 0xffff) << 10);
			break;
		case R_SH_IMM_MEDLOW16_PCREL:
			relocation -= (Elf32_Addr) location;
			*location = (*location & ~0x3fffc00) |
				(((relocation >> 16) & 0xffff) << 10);
			break;
		default:
			printk(KERN_ERR "module %s: Unknown relocation: %u\n",
			       me->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int apply_relocate(Elf32_Shdr *sechdrs,
		       const char *strtab,
		       unsigned int symindex,
		       unsigned int relsec,
		       struct module *me)
{
	printk(KERN_ERR "module %s: REL RELOCATION unsupported\n",
	       me->name);
	return -ENOEXEC;
}

int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *me)
{
	int ret = 0;

	ret |= module_dwarf_finalize(hdr, sechdrs, me);

	return ret;
}

void module_arch_cleanup(struct module *mod)
{
	module_dwarf_cleanup(mod);
}
