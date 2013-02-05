/*
 * We want to be able to compile mount on old kernels in such a way
 * that the binary will work well on more recent kernels.
 * Thus, if necessary we teach nfsmount.c the structure of new fields
 * that will come later.
 *
 * Moreover, the new kernel includes conflict with glibc includes
 * so it is easiest to ignore the kernel altogether (at compile time).
 */

#ifndef _NFS_UTILS_MOUNT_NFS_MOUNT_H
#define _NFS_UTILS_MOUNT_NFS_MOUNT_H

#include <netinet/in.h>
#include <arpa/inet.h>

#define NFS_MOUNT_VERSION	6
#define NFS_MAX_CONTEXT_LEN	256

struct nfs2_fh {
        char                    data[32];
};
struct nfs3_fh {
        unsigned short          size;
        unsigned char           data[64];
};

struct nfs_mount_data {
	int		version;		/* 1 */
	int		fd;			/* 1 */
	struct nfs2_fh	old_root;		/* 1 */
	int		flags;			/* 1 */
	int		rsize;			/* 1 */
	int		wsize;			/* 1 */
	int		timeo;			/* 1 */
	int		retrans;		/* 1 */
	int		acregmin;		/* 1 */
	int		acregmax;		/* 1 */
	int		acdirmin;		/* 1 */
	int		acdirmax;		/* 1 */
	struct sockaddr_in addr;		/* 1 */
	char		hostname[256];		/* 1 */
	int		namlen;			/* 2 */
	unsigned int	bsize;			/* 3 */
	struct nfs3_fh	root;			/* 4 */
	int		pseudoflavor;		/* 5 */
	char    context[NFS_MAX_CONTEXT_LEN + 1]; /* 6 */

};

/* bits in the flags field */

#define NFS_MOUNT_SOFT		0x0001	/* 1 */
#define NFS_MOUNT_INTR		0x0002	/* 1 */
#define NFS_MOUNT_SECURE	0x0004	/* 1 */
#define NFS_MOUNT_POSIX		0x0008	/* 1 */
#define NFS_MOUNT_NOCTO		0x0010	/* 1 */
#define NFS_MOUNT_NOAC		0x0020	/* 1 */
#define NFS_MOUNT_TCP		0x0040	/* 2 */
#define NFS_MOUNT_VER3		0x0080	/* 3 */
#define NFS_MOUNT_KERBEROS	0x0100	/* 3 */
#define NFS_MOUNT_NONLM		0x0200	/* 3 */
#define NFS_MOUNT_BROKEN_SUID	0x0400	/* 4 */
#define NFS_MOUNT_NOACL     0x0800  /* 4 */
#define NFS_MOUNT_SECFLAVOUR	0x2000	/* 5 */
#define NFS_MOUNT_NORDIRPLUS	0x4000	/* 5 */
#define NFS_MOUNT_UNSHARED	0x8000	/* 5 */

/* security pseudoflavors */

#ifndef AUTH_GSS_KRB5
#define AUTH_GSS_KRB5		390003
#define AUTH_GSS_KRB5I		390004
#define AUTH_GSS_KRB5P		390005
#define AUTH_GSS_LKEY		390006
#define AUTH_GSS_LKEYI		390007
#define AUTH_GSS_LKEYP		390008
#define AUTH_GSS_SPKM		390009
#define AUTH_GSS_SPKMI		390010
#define AUTH_GSS_SPKMP		390011
#endif

int	nfsmount(const char *, const char *, int , char **, int, int);
int	nfsumount(int, char **);

#endif	/* _NFS_UTILS_MOUNT_NFS_MOUNT_H */
