/*
 *  include/asm-s390/setup.h
 *
 *  S390 version
 *    Copyright IBM Corp. 1999,2006
 */

#ifndef _ASM_S390_SETUP_H
#define _ASM_S390_SETUP_H

#define COMMAND_LINE_SIZE 	896

#ifdef __KERNEL__

#include <asm/types.h>

#define PARMAREA		0x10400
#define MEMORY_CHUNKS		16	/* max 0x7fff */

#ifndef __ASSEMBLY__

#ifndef __s390x__
#define IPL_DEVICE        (*(unsigned long *)  (0x10404))
#define INITRD_START      (*(unsigned long *)  (0x1040C))
#define INITRD_SIZE       (*(unsigned long *)  (0x10414))
#else /* __s390x__ */
#define IPL_DEVICE        (*(unsigned long *)  (0x10400))
#define INITRD_START      (*(unsigned long *)  (0x10408))
#define INITRD_SIZE       (*(unsigned long *)  (0x10410))
#endif /* __s390x__ */
#define COMMAND_LINE      ((char *)            (0x10480))

#define CHUNK_READ_WRITE 0
#define CHUNK_READ_ONLY  1

struct mem_chunk {
	unsigned long addr;
	unsigned long size;
	unsigned long type;
};

extern struct mem_chunk memory_chunk[];
extern unsigned long real_memory_size;

#ifdef CONFIG_S390_SWITCH_AMODE
extern unsigned int switch_amode;
#else
#define switch_amode	(0)
#endif

#ifdef CONFIG_S390_EXEC_PROTECT
extern unsigned int s390_noexec;
#else
#define s390_noexec	(0)
#endif

/*
 * Machine features detected in head.S
 */
extern unsigned long machine_flags;

#define MACHINE_IS_VM		(machine_flags & 1)
#define MACHINE_IS_P390		(machine_flags & 4)
#define MACHINE_HAS_MVPG	(machine_flags & 16)
#define MACHINE_HAS_IDTE	(machine_flags & 128)
#define MACHINE_HAS_DIAG9C	(machine_flags & 256)

#ifndef __s390x__
#define MACHINE_HAS_IEEE	(machine_flags & 2)
#define MACHINE_HAS_CSP		(machine_flags & 8)
#define MACHINE_HAS_DIAG44	(1)
#define MACHINE_HAS_MVCOS	(0)
#else /* __s390x__ */
#define MACHINE_HAS_IEEE	(1)
#define MACHINE_HAS_CSP		(1)
#define MACHINE_HAS_DIAG44	(machine_flags & 32)
#define MACHINE_HAS_MVCOS	(machine_flags & 512)
#endif /* __s390x__ */

#define MACHINE_HAS_SCLP	(!MACHINE_IS_P390)
#define ZFCPDUMP_HSA_SIZE	(32UL<<20)

/*
 * Console mode. Override with conmode=
 */
extern unsigned int console_mode;
extern unsigned int console_devno;
extern unsigned int console_irq;

extern char vmhalt_cmd[];
extern char vmpoff_cmd[];

#define CONSOLE_IS_UNDEFINED	(console_mode == 0)
#define CONSOLE_IS_SCLP		(console_mode == 1)
#define CONSOLE_IS_3215		(console_mode == 2)
#define CONSOLE_IS_3270		(console_mode == 3)
#define SET_CONSOLE_SCLP	do { console_mode = 1; } while (0)
#define SET_CONSOLE_3215	do { console_mode = 2; } while (0)
#define SET_CONSOLE_3270	do { console_mode = 3; } while (0)

#define NSS_NAME_SIZE	8
extern char kernel_nss_name[];

#else /* __ASSEMBLY__ */

#ifndef __s390x__
#define IPL_DEVICE        0x10404
#define INITRD_START      0x1040C
#define INITRD_SIZE       0x10414
#else /* __s390x__ */
#define IPL_DEVICE        0x10400
#define INITRD_START      0x10408
#define INITRD_SIZE       0x10410
#endif /* __s390x__ */
#define COMMAND_LINE      0x10480

#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* _ASM_S390_SETUP_H */
