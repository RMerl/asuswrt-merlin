/*
 * include/linux/nfsd/const.h
 *
 * Various constants related to NFS.
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_NFSD_CONST_H
#define _LINUX_NFSD_CONST_H

#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs3.h>
#include <asm/page.h>

/*
 * Maximum protocol version supported by knfsd
 */
#define NFSSVC_MAXVERS		3

/*
 * Maximum blocksize supported by daemon.  We want the largest
 * value which 1) fits in a UDP datagram less some headers
 * 2) is a multiple of page size 3) can be successfully kmalloc()ed
 * by each nfsd.   
 */
/* Disable dependence from PAGE_SIZE, force it to 32kB
#if PAGE_SIZE > (16*1024)
#define NFSSVC_MAXBLKSIZE	(32*1024)
#else
#define NFSSVC_MAXBLKSIZE	(2*PAGE_SIZE)
#endif
 */
#define NFSSVC_MAXBLKSIZE	(32*1024)

#ifdef __KERNEL__

#ifndef NFS_SUPER_MAGIC
# define NFS_SUPER_MAGIC	0x6969
#endif

#endif /* __KERNEL__ */

#endif /* _LINUX_NFSD_CONST_H */
