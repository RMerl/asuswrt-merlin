/* XDR types for nfsd. This is mainly a typing exercise. */

#ifndef LINUX_NFSD_H
#define LINUX_NFSD_H

#include <linux/vfs.h>
#include "nfsd.h"
#include "nfsfh.h"

struct nfsd_fhandle {
	struct svc_fh		fh;
};

struct nfsd_sattrargs {
	struct svc_fh		fh;
	struct iattr		attrs;
};

struct nfsd_diropargs {
	struct svc_fh		fh;
	char *			name;
	unsigned int		len;
};

struct nfsd_readargs {
	struct svc_fh		fh;
	__u32			offset;
	__u32			count;
	int			vlen;
};

struct nfsd_writeargs {
	svc_fh			fh;
	__u32			offset;
	int			len;
	int			vlen;
};

struct nfsd_createargs {
	struct svc_fh		fh;
	char *			name;
	unsigned int		len;
	struct iattr		attrs;
};

struct nfsd_renameargs {
	struct svc_fh		ffh;
	char *			fname;
	unsigned int		flen;
	struct svc_fh		tfh;
	char *			tname;
	unsigned int		tlen;
};

struct nfsd_readlinkargs {
	struct svc_fh		fh;
	char *			buffer;
};
	
struct nfsd_linkargs {
	struct svc_fh		ffh;
	struct svc_fh		tfh;
	char *			tname;
	unsigned int		tlen;
};

struct nfsd_symlinkargs {
	struct svc_fh		ffh;
	char *			fname;
	unsigned int		flen;
	char *			tname;
	unsigned int		tlen;
	struct iattr		attrs;
};

struct nfsd_readdirargs {
	struct svc_fh		fh;
	__u32			cookie;
	__u32			count;
	__be32 *		buffer;
};

struct nfsd_attrstat {
	struct svc_fh		fh;
	struct kstat		stat;
};

struct nfsd_diropres  {
	struct svc_fh		fh;
	struct kstat		stat;
};

struct nfsd_readlinkres {
	int			len;
};

struct nfsd_readres {
	struct svc_fh		fh;
	unsigned long		count;
	struct kstat		stat;
};

struct nfsd_readdirres {
	int			count;

	struct readdir_cd	common;
	__be32 *		buffer;
	int			buflen;
	__be32 *		offset;
};

struct nfsd_statfsres {
	struct kstatfs		stats;
};

/*
 * Storage requirements for XDR arguments and results.
 */
union nfsd_xdrstore {
	struct nfsd_sattrargs	sattr;
	struct nfsd_diropargs	dirop;
	struct nfsd_readargs	read;
	struct nfsd_writeargs	write;
	struct nfsd_createargs	create;
	struct nfsd_renameargs	rename;
	struct nfsd_linkargs	link;
	struct nfsd_symlinkargs	symlink;
	struct nfsd_readdirargs	readdir;
};

#define NFS2_SVC_XDRSIZE	sizeof(union nfsd_xdrstore)


int nfssvc_decode_void(struct svc_rqst *, __be32 *, void *);
int nfssvc_decode_fhandle(struct svc_rqst *, __be32 *, struct nfsd_fhandle *);
int nfssvc_decode_sattrargs(struct svc_rqst *, __be32 *,
				struct nfsd_sattrargs *);
int nfssvc_decode_diropargs(struct svc_rqst *, __be32 *,
				struct nfsd_diropargs *);
int nfssvc_decode_readargs(struct svc_rqst *, __be32 *,
				struct nfsd_readargs *);
int nfssvc_decode_writeargs(struct svc_rqst *, __be32 *,
				struct nfsd_writeargs *);
int nfssvc_decode_createargs(struct svc_rqst *, __be32 *,
				struct nfsd_createargs *);
int nfssvc_decode_renameargs(struct svc_rqst *, __be32 *,
				struct nfsd_renameargs *);
int nfssvc_decode_readlinkargs(struct svc_rqst *, __be32 *,
				struct nfsd_readlinkargs *);
int nfssvc_decode_linkargs(struct svc_rqst *, __be32 *,
				struct nfsd_linkargs *);
int nfssvc_decode_symlinkargs(struct svc_rqst *, __be32 *,
				struct nfsd_symlinkargs *);
int nfssvc_decode_readdirargs(struct svc_rqst *, __be32 *,
				struct nfsd_readdirargs *);
int nfssvc_encode_void(struct svc_rqst *, __be32 *, void *);
int nfssvc_encode_attrstat(struct svc_rqst *, __be32 *, struct nfsd_attrstat *);
int nfssvc_encode_diropres(struct svc_rqst *, __be32 *, struct nfsd_diropres *);
int nfssvc_encode_readlinkres(struct svc_rqst *, __be32 *, struct nfsd_readlinkres *);
int nfssvc_encode_readres(struct svc_rqst *, __be32 *, struct nfsd_readres *);
int nfssvc_encode_statfsres(struct svc_rqst *, __be32 *, struct nfsd_statfsres *);
int nfssvc_encode_readdirres(struct svc_rqst *, __be32 *, struct nfsd_readdirres *);

int nfssvc_encode_entry(void *, const char *name,
			int namlen, loff_t offset, u64 ino, unsigned int);

int nfssvc_release_fhandle(struct svc_rqst *, __be32 *, struct nfsd_fhandle *);

/* Helper functions for NFSv2 ACL code */
__be32 *nfs2svc_encode_fattr(struct svc_rqst *rqstp, __be32 *p, struct svc_fh *fhp);
__be32 *nfs2svc_decode_fh(__be32 *p, struct svc_fh *fhp);

#endif /* LINUX_NFSD_H */
