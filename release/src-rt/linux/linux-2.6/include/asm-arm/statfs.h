#ifndef _ASMARM_STATFS_H
#define _ASMARM_STATFS_H

#ifndef __KERNEL_STRICT_NAMES
# include <linux/types.h>
typedef __kernel_fsid_t	fsid_t;
#endif

struct statfs {
	__u32 f_type;
	__u32 f_bsize;
	__u32 f_blocks;
	__u32 f_bfree;
	__u32 f_bavail;
	__u32 f_files;
	__u32 f_ffree;
	__kernel_fsid_t f_fsid;
	__u32 f_namelen;
	__u32 f_frsize;
	__u32 f_spare[5];
};

/*
 * With EABI there is 4 bytes of padding added to this structure.
 * Let's pack it so the padding goes away to simplify dual ABI support.
 * Note that user space does NOT have to pack this structure.
 */
struct statfs64 {
	__u32 f_type;
	__u32 f_bsize;
	__u64 f_blocks;
	__u64 f_bfree;
	__u64 f_bavail;
	__u64 f_files;
	__u64 f_ffree;
	__kernel_fsid_t f_fsid;
	__u32 f_namelen;
	__u32 f_frsize;
	__u32 f_spare[5];
} __attribute__ ((packed,aligned(4)));

#endif
