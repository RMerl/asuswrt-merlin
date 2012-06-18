/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 97, 98, 99, 2000 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_POSIX_TYPES_H
#define _ASM_POSIX_TYPES_H

#include <asm/sgidefs.h>

/*
 * This file is generally used by user-level software, so you need to
 * be a little careful about namespace pollution etc.  Also, we cannot
 * assume GCC is being used.
 */

typedef unsigned long	__kernel_ino_t;
typedef unsigned int	__kernel_mode_t;
#if (_MIPS_SZLONG == 32)
typedef unsigned long	__kernel_nlink_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned int	__kernel_nlink_t;
#endif
typedef long		__kernel_off_t;
typedef int		__kernel_pid_t;
typedef int		__kernel_ipc_pid_t;
typedef unsigned int	__kernel_uid_t;
typedef unsigned int	__kernel_gid_t;
#if (_MIPS_SZLONG == 32)
typedef unsigned int	__kernel_size_t;
typedef int		__kernel_ssize_t;
typedef int		__kernel_ptrdiff_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long	__kernel_size_t;
typedef long		__kernel_ssize_t;
typedef long		__kernel_ptrdiff_t;
#endif
typedef long		__kernel_time_t;
typedef long		__kernel_suseconds_t;
typedef long		__kernel_clock_t;
typedef int		__kernel_timer_t;
typedef int		__kernel_clockid_t;
typedef long		__kernel_daddr_t;
typedef char *		__kernel_caddr_t;

typedef unsigned short	__kernel_uid16_t;
typedef unsigned short	__kernel_gid16_t;
typedef unsigned int	__kernel_uid32_t;
typedef unsigned int	__kernel_gid32_t;
typedef __kernel_uid_t	__kernel_old_uid_t;
typedef __kernel_gid_t	__kernel_old_gid_t;
typedef unsigned int	__kernel_old_dev_t;

#ifdef __GNUC__
typedef long long      __kernel_loff_t;
#endif

typedef struct {
#if (_MIPS_SZLONG == 32)
	long	val[2];
#endif
#if (_MIPS_SZLONG == 64)
	int	val[2];
#endif
} __kernel_fsid_t;


#endif /* _ASM_POSIX_TYPES_H */
