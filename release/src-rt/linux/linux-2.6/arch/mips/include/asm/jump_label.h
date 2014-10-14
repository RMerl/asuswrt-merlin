/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2010 Cavium Networks, Inc.
 */
#ifndef _ASM_MIPS_JUMP_LABEL_H
#define _ASM_MIPS_JUMP_LABEL_H

#include <linux/types.h>

#ifdef __KERNEL__

#define JUMP_LABEL_NOP_SIZE 4

#ifdef CONFIG_64BIT
#define WORD_INSN ".dword"
#else
#define WORD_INSN ".word"
#endif

#define JUMP_LABEL(key, label)						\
	do {								\
		asm goto("1:\tnop\n\t"					\
			"nop\n\t"					\
			".pushsection __jump_table,  \"a\"\n\t"		\
			WORD_INSN " 1b, %l[" #label "], %0\n\t"		\
			".popsection\n\t"				\
			: :  "i" (key) :  : label);			\
	} while (0)


#endif /* __KERNEL__ */

#ifdef CONFIG_64BIT
typedef u64 jump_label_t;
#else
typedef u32 jump_label_t;
#endif

struct jump_entry {
	jump_label_t code;
	jump_label_t target;
	jump_label_t key;
};

#endif /* _ASM_MIPS_JUMP_LABEL_H */
