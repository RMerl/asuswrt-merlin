/*
 *  linux/arch/arm/mm/extable.c
 */
#include <linux/module.h>
#include <linux/uaccess.h>

int fixup_exception(struct pt_regs *regs)
{
	const struct exception_table_entry *fixup;

	fixup = search_exception_tables(instruction_pointer(regs));
	if (fixup)
		regs->ARM_pc = fixup->fixup;

	return fixup != NULL;
}
