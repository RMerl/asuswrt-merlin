#ifndef _NFS_NFS_H
#define _NFS_NFS_H

#include <linux/posix_types.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <rpcsvc/nfs_prot.h>
#include <nfs/export.h>

#define	NFS3_FHSIZE	64
#define	NFS_FHSIZE	32

#define NFSD_MINVERS 2
#define NFSD_MAXVERS 4

struct nfs_fh_len {
	int		fh_size;
	u_int8_t	fh_handle[NFS3_FHSIZE];
};
struct nfs_fh_old {
	u_int8_t	fh_handle[NFS_FHSIZE];
};

/*
 * Version of the syscall interface
 */
#define NFSCTL_VERSION		0x0201

/*
 * These are the commands understood by nfsctl().
 */
#define NFSCTL_SVC		0	/* This is a server process. */
#define NFSCTL_ADDCLIENT	1	/* Add an NFS client. */
#define NFSCTL_DELCLIENT	2	/* Remove an NFS client. */
#define NFSCTL_EXPORT		3	/* export a file system. */
#define NFSCTL_UNEXPORT		4	/* unexport a file system. */
#define NFSCTL_UGIDUPDATE	5	/* update a client's uid/gid map. */
#define NFSCTL_GETFH		6	/* get an fh (used by mountd) */
#define NFSCTL_GETFD		7	/* get an fh by path (used by mountd) */
#define NFSCTL_GETFS		8	/* get an fh by path with max size (used by mountd) */

#define NFSCTL_VERUNSET(_cltbits, _v) ((_cltbits) &= ~(1 << ((_v) - 1))) 
#define NFSCTL_UDPUNSET(_cltbits)     ((_cltbits) &= ~(1 << (17 - 1))) 
#define NFSCTL_TCPUNSET(_cltbits)     ((_cltbits) &= ~(1 << (18 - 1))) 

#define NFSCTL_VERISSET(_cltbits, _v) ((_cltbits) & (1 << ((_v) - 1))) 
#define NFSCTL_UDPISSET(_cltbits)     ((_cltbits) & (1 << (17 - 1))) 
#define NFSCTL_TCPISSET(_cltbits)     ((_cltbits) & (1 << (18 - 1))) 

#define NFSCTL_ALLBITS (~0)

/* SVC */
struct nfsctl_svc {
	unsigned short		svc_port;
	int			svc_nthreads;
};

/* ADDCLIENT/DELCLIENT */
struct nfsctl_client {
	char			cl_ident[NFSCLNT_IDMAX+1];
	int			cl_naddr;
	struct in_addr		cl_addrlist[NFSCLNT_ADDRMAX];
	int			cl_fhkeytype;
	int			cl_fhkeylen;
	unsigned char		cl_fhkey[NFSCLNT_KEYMAX];
};

/* IN 2.5.6? __kernel_dev_t changed size, and __kernel_old_dev_t was left
 * with the old value.  We need to make sure we use the right one.
 *
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,70)
# define __nfsd_dev_t  __kernel_old_dev_t
#else
# define __nfsd_dev_t  __kernel_dev_t
#endif

/* EXPORT/UNEXPORT */
struct nfsctl_export {
	char			ex_client[NFSCLNT_IDMAX+1];
	char			ex_path[NFS_MAXPATHLEN+1];
	__nfsd_dev_t		ex_dev;
	__kernel_ino_t		ex_ino;
	int			ex_flags;
	__kernel_uid_t		ex_anon_uid;
	__kernel_gid_t		ex_anon_gid;
};

/* UGIDUPDATE */
struct nfsctl_uidmap {
	char *			ug_ident;
	__kernel_uid_t		ug_uidbase;
	int			ug_uidlen;
	__kernel_uid_t *	ug_udimap;
	__kernel_gid_t		ug_gidbase;
	int			ug_gidlen;
	__kernel_gid_t *	ug_gdimap;
};

/* GETFH */
struct nfsctl_fhparm {
	struct sockaddr		gf_addr;
	__nfsd_dev_t		gf_dev;
	__kernel_ino_t		gf_ino;
	int			gf_version;
};

/* GETFD */
struct nfsctl_fdparm {
	struct sockaddr		gd_addr;
	char			gd_path[NFS_MAXPATHLEN+1];
	int			gd_version;
};

/* GETFS - GET Filehandle with Size */
struct nfsctl_fsparm {
	struct sockaddr		gd_addr;
	char			gd_path[NFS_MAXPATHLEN+1];
	int			gd_maxlen;
};

/*
 * This is the argument union.
 */
struct nfsctl_arg {
	int			ca_version;	/* safeguard */
	union {
		struct nfsctl_svc	u_svc;
		struct nfsctl_client	u_client;
		struct nfsctl_export	u_export;
		struct nfsctl_uidmap	u_umap;
		struct nfsctl_fhparm	u_getfh;
		struct nfsctl_fdparm	u_getfd;
		struct nfsctl_fsparm	u_getfs;
	} u;
#define ca_svc		u.u_svc
#define ca_client	u.u_client
#define ca_export	u.u_export
#define ca_umap		u.u_umap
#define ca_getfh	u.u_getfh
#define ca_getfd	u.u_getfd
#define ca_getfs	u.u_getfs
#define ca_authd	u.u_authd
};

union nfsctl_res {
	struct nfs_fh_old	cr_getfh;
	struct nfs_fh_len	cr_getfs;
};

#endif /* _NFS_NFS_H */
