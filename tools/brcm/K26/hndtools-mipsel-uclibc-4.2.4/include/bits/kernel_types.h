/* Note that we use the exact same include guard #define names
 * as asm/posix_types.h.  This will avoid gratuitous conflicts
 * with the posix_types.h kernel header, and will ensure that
 * our private content, and not the kernel header, will win.
 *  -Erik
 */
#ifndef _ASM_POSIX_TYPES_H
#define _ASM_POSIX_TYPES_H

# if __WORDSIZE == 64
typedef unsigned int	__kernel_dev_t;
typedef unsigned long	__kernel_ino_t;
typedef unsigned int	__kernel_mode_t;
typedef unsigned int	__kernel_nlink_t;
typedef long		__kernel_off_t;
typedef int		__kernel_pid_t;
typedef long int		__kernel_ipc_pid_t;
typedef int		__kernel_uid_t;
typedef int		__kernel_gid_t;
typedef unsigned long	__kernel_size_t;
typedef long		__kernel_ssize_t;
typedef long		__kernel_ptrdiff_t;
typedef long		__kernel_time_t;
typedef long		__kernel_suseconds_t;
typedef long		__kernel_clock_t;
typedef long		__kernel_daddr_t;
typedef char *		__kernel_caddr_t;
typedef unsigned short	__kernel_uid16_t;
typedef unsigned short	__kernel_gid16_t;
typedef int		__kernel_uid32_t;
typedef int		__kernel_gid32_t;
typedef __kernel_uid_t	__kernel_old_uid_t;
typedef __kernel_gid_t	__kernel_old_gid_t;
typedef __kernel_dev_t	__kernel_old_dev_t;
typedef long long      __kernel_loff_t;
#else
typedef unsigned int	__kernel_dev_t;
typedef unsigned long	__kernel_ino_t;
typedef unsigned int	__kernel_mode_t;

/* Linux 2.4.20 include/asm-mips/posix_types.h has this:
but apparently that is an error?!?!?
*/
#if 0
typedef int            __kernel_nlink_t;
#else
/* So use this instead */
typedef unsigned long  __kernel_nlink_t;
#endif

typedef long		__kernel_off_t;
typedef int		__kernel_pid_t;
typedef long int		__kernel_ipc_pid_t;
typedef int		__kernel_uid_t;
typedef int		__kernel_gid_t;
typedef unsigned int	__kernel_size_t;
typedef int		__kernel_ssize_t;
typedef int		__kernel_ptrdiff_t;
typedef long		__kernel_time_t;
typedef long		__kernel_suseconds_t;
typedef long		__kernel_clock_t;
typedef long		__kernel_daddr_t;
typedef char *		__kernel_caddr_t;
typedef unsigned short	__kernel_uid16_t;
typedef unsigned short	__kernel_gid16_t;
typedef int		__kernel_uid32_t;
typedef int		__kernel_gid32_t;
typedef __kernel_uid_t	__kernel_old_uid_t;
typedef __kernel_gid_t	__kernel_old_gid_t;
typedef __kernel_dev_t	__kernel_old_dev_t;
typedef long long      __kernel_loff_t;
#endif


typedef struct {
	long val[2];
} __kernel_fsid_t;

#endif /* _ASM_POSIX_TYPES_H */
