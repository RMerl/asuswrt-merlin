/*
 *  linux/include/linux/nfs_fs.h
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  OS-specific nfs filesystem definitions and declarations
 */

#ifndef _LINUX_NFS_FS_H
#define _LINUX_NFS_FS_H

#include <linux/magic.h>

/* Default timeout values */
#define NFS_MAX_UDP_TIMEOUT	(60*HZ)
#define NFS_MAX_TCP_TIMEOUT	(600*HZ)

/*
 * When flushing a cluster of dirty pages, there can be different
 * strategies:
 */
#define FLUSH_SYNC		1	/* file being synced, or contention */
#define FLUSH_STABLE		4	/* commit to stable storage */
#define FLUSH_LOWPRI		8	/* low priority background flush */
#define FLUSH_HIGHPRI		16	/* high priority memory reclaim flush */
#define FLUSH_NOCOMMIT		32	/* Don't send the NFSv3/v4 COMMIT */
#define FLUSH_INVALIDATE	64	/* Invalidate the page cache */
#define FLUSH_NOWRITEPAGE	128	/* Don't call writepage() */


/*
 * NFS debug flags
 */
#define NFSDBG_VFS		0x0001
#define NFSDBG_DIRCACHE		0x0002
#define NFSDBG_LOOKUPCACHE	0x0004
#define NFSDBG_PAGECACHE	0x0008
#define NFSDBG_PROC		0x0010
#define NFSDBG_XDR		0x0020
#define NFSDBG_FILE		0x0040
#define NFSDBG_ROOT		0x0080
#define NFSDBG_CALLBACK		0x0100
#define NFSDBG_CLIENT		0x0200
#define NFSDBG_ALL		0xFFFF


#endif
