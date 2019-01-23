/* 
   Unix SMB/CIFS implementation.
   support for quotas
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/* 
 * This is one of the most system dependent parts of Samba, and its
 * done a litle differently. Each system has its own way of doing 
 * things :-(
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "system/filesys.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_QUOTA

#ifndef HAVE_SYS_QUOTAS

/* just a quick hack because sysquotas.h is included before linux/quota.h */
#ifdef QUOTABLOCK_SIZE
#undef QUOTABLOCK_SIZE
#endif

#ifdef WITH_QUOTAS

#if defined(VXFS_QUOTA)

/*
 * In addition to their native filesystems, some systems have Veritas VxFS.
 * Declare here, define at end: reduces likely "include" interaction problems.
 *	David Lee <T.D.Lee@durham.ac.uk>
 */
bool disk_quotas_vxfs(const char *name, char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize);

#endif /* VXFS_QUOTA */

#ifdef LINUX

#include <sys/types.h>
#include <mntent.h>

/*
 * This shouldn't be neccessary - it should be /usr/include/sys/quota.h
 * So we include all the files has *should* be in the system into a large,
 * grungy samba_linux_quoatas.h Sometimes I *hate* Linux :-). JRA.
 */

#include "samba_linux_quota.h"

typedef struct _LINUX_SMB_DISK_QUOTA {
	uint64_t bsize;
	uint64_t hardlimit; /* In bsize units. */
	uint64_t softlimit; /* In bsize units. */
	uint64_t curblocks; /* In bsize units. */
	uint64_t ihardlimit; /* inode hard limit. */
	uint64_t isoftlimit; /* inode soft limit. */
	uint64_t curinodes; /* Current used inodes. */
} LINUX_SMB_DISK_QUOTA;


/*
 * nfs quota support
 * (essentially taken from FreeBSD / SUNOS5 section)
 */
#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpcsvc/rquota.h>
#ifdef HAVE_RPC_NETTYPE_H
#include <rpc/nettype.h>
#endif
#include <rpc/xdr.h>

static int my_xdr_getquota_args(XDR *xdrsp, struct getquota_args *args)
{
	if (!xdr_string(xdrsp, &args->gqa_pathp, RQ_PATHLEN ))
		return(0);
	if (!xdr_int(xdrsp, &args->gqa_uid))
		return(0);
	return (1);
}

static int my_xdr_getquota_rslt(XDR *xdrsp, struct getquota_rslt *gqr)
{
	int quotastat;

	if (!xdr_int(xdrsp, &quotastat)) {
		DEBUG(6,("nfs_quotas: Status bad or zero\n"));
		return 0;
	}
	gqr->status = quotastat;

	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bsize)) {
		DEBUG(6,("nfs_quotas: Block size bad or zero\n"));
		return 0;
	}
	if (!xdr_bool(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_active)) {
		DEBUG(6,("nfs_quotas: Active bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, (int *)&gqr->getquota_rslt_u.gqr_rquota.rq_bhardlimit)) {
		DEBUG(6,("nfs_quotas: Hardlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, (int *)&gqr->getquota_rslt_u.gqr_rquota.rq_bsoftlimit)) {
		DEBUG(6,("nfs_quotas: Softlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, (int *)&gqr->getquota_rslt_u.gqr_rquota.rq_curblocks)) {
		DEBUG(6,("nfs_quotas: Currentblocks bad or zero\n"));
		return 0;
	}
	return 1;
}

static bool nfs_quotas(char *nfspath, uid_t euser_id, uint64_t *bsize,
		       uint64_t *dfree, uint64_t *dsize)
{
	uid_t uid = euser_id;
	LINUX_SMB_DISK_QUOTA D;
	char *mnttype = nfspath;
	CLIENT *clnt;
	struct getquota_rslt gqr;
	struct getquota_args args;
	char *cutstr, *pathname, *host, *testpath;
	int len;
	static struct timeval timeout = {2,0};
	enum clnt_stat clnt_stat;
	bool ret = True;

	*bsize = *dfree = *dsize = (uint64_t)0;

	len=strcspn(mnttype, ":");
	pathname=strstr(mnttype, ":");
	cutstr = (char *) SMB_MALLOC(len+1);
	if (!cutstr)
		return False;

	memset(cutstr, '\0', len+1);
	host = strncat(cutstr,mnttype, sizeof(char) * len );
	DEBUG(5,("nfs_quotas: looking for mount on \"%s\"\n", cutstr));
	DEBUG(5,("nfs_quotas: of path \"%s\"\n", mnttype));
	testpath=strchr_m(mnttype, ':');
	args.gqa_pathp = testpath+1;
	args.gqa_uid = uid;

	DEBUG(5, ("nfs_quotas: Asking for host \"%s\" rpcprog \"%i\" rpcvers "
		  "\"%i\" network \"%s\"\n", host, RQUOTAPROG, RQUOTAVERS,
		  "udp"));

	if ((clnt = clnt_create(host, RQUOTAPROG, RQUOTAVERS, "udp")) == NULL) {
		ret = False;
		goto out;
	}

	clnt->cl_auth = authunix_create_default();
	DEBUG(9,("nfs_quotas: auth_success\n"));

	clnt_stat=clnt_call(clnt,
			    RQUOTAPROC_GETQUOTA,
			    (const xdrproc_t)my_xdr_getquota_args,
			    (caddr_t)&args,
			    (const xdrproc_t)my_xdr_getquota_rslt,
			    (caddr_t)&gqr, timeout);

	if (clnt_stat != RPC_SUCCESS) {
		DEBUG(9,("nfs_quotas: clnt_call fail\n"));
		ret = False;
		goto out;
	}

	/*
	 * gqr.status returns 0 if the rpc call fails, 1 if quotas exist, 2 if there is
	 * no quota set, and 3 if no permission to get the quota.  If 0 or 3 return
	 * something sensible.
	 */

	switch (gqr.status) {
	case 0:
		DEBUG(9, ("nfs_quotas: Remote Quotas Failed!  Error \"%i\" \n",
			  gqr.status));
		ret = False;
		goto out;

	case 1:
		DEBUG(9,("nfs_quotas: Good quota data\n"));
		D.softlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit;
		D.hardlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit;
		D.curblocks = gqr.getquota_rslt_u.gqr_rquota.rq_curblocks;
		break;

	case 2:
	case 3:
		D.softlimit = 1;
		D.curblocks = 1;
		DEBUG(9, ("nfs_quotas: Remote Quotas returned \"%i\" \n",
			  gqr.status));
		break;

	default:
		DEBUG(9, ("nfs_quotas: Remote Quotas Questionable!  "
			  "Error \"%i\" \n", gqr.status));
		break;
	}

	DEBUG(10, ("nfs_quotas: Let`s look at D a bit closer... "
		   "status \"%i\" bsize \"%i\" active? \"%i\" bhard "
		   "\"%i\" bsoft \"%i\" curb \"%i\" \n",
		   gqr.status,
		   gqr.getquota_rslt_u.gqr_rquota.rq_bsize,
		   gqr.getquota_rslt_u.gqr_rquota.rq_active,
		   gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit,
		   gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit,
		   gqr.getquota_rslt_u.gqr_rquota.rq_curblocks));

	if (D.softlimit == 0)
		D.softlimit = D.hardlimit;
	if (D.softlimit == 0)
		return False;

	*bsize = gqr.getquota_rslt_u.gqr_rquota.rq_bsize;
	*dsize = D.softlimit;

	if (D.curblocks == 1)
		*bsize = DEV_BSIZE;

	if (D.curblocks > D.softlimit) {
		*dfree = 0;
		*dsize = D.curblocks;
	} else
		*dfree = D.softlimit - D.curblocks;

  out:

	if (clnt) {
		if (clnt->cl_auth)
			auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
	}

	DEBUG(5, ("nfs_quotas: For path \"%s\" returning "
		  "bsize %.0f, dfree %.0f, dsize %.0f\n",
		  args.gqa_pathp, (double)*bsize, (double)*dfree,
		  (double)*dsize));

	SAFE_FREE(cutstr);
	DEBUG(10,("nfs_quotas: End of nfs_quotas\n" ));
	return ret;
}

/* end of nfs quota section */

#ifdef HAVE_LINUX_DQBLK_XFS_H
#include <linux/dqblk_xfs.h>

/****************************************************************************
 Abstract out the XFS Quota Manager quota get call.
****************************************************************************/

static int get_smb_linux_xfs_quota(char *path, uid_t euser_id, gid_t egrp_id, LINUX_SMB_DISK_QUOTA *dp)
{
	struct fs_disk_quota D;
	int ret;

	ZERO_STRUCT(D);

	ret = quotactl(QCMD(Q_XGETQUOTA,USRQUOTA), path, euser_id, (caddr_t)&D);

	if (ret)
		ret = quotactl(QCMD(Q_XGETQUOTA,GRPQUOTA), path, egrp_id, (caddr_t)&D);

	if (ret)
		return ret;

	dp->bsize = (uint64_t)512;
	dp->softlimit = (uint64_t)D.d_blk_softlimit;
	dp->hardlimit = (uint64_t)D.d_blk_hardlimit;
	dp->ihardlimit = (uint64_t)D.d_ino_hardlimit;
	dp->isoftlimit = (uint64_t)D.d_ino_softlimit;
	dp->curinodes = (uint64_t)D.d_icount;
	dp->curblocks = (uint64_t)D.d_bcount;

	return ret;
}
#else
static int get_smb_linux_xfs_quota(char *path, uid_t euser_id, gid_t egrp_id, LINUX_SMB_DISK_QUOTA *dp)
{
	DEBUG(0,("XFS quota support not available\n"));
	errno = ENOSYS;
	return -1;
}
#endif


/****************************************************************************
 Abstract out the old and new Linux quota get calls.
****************************************************************************/

static int get_smb_linux_v1_quota(char *path, uid_t euser_id, gid_t egrp_id, LINUX_SMB_DISK_QUOTA *dp)
{
	struct v1_kern_dqblk D;
	int ret;

	ZERO_STRUCT(D);

	ret = quotactl(QCMD(Q_V1_GETQUOTA,USRQUOTA), path, euser_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		ret = quotactl(QCMD(Q_V1_GETQUOTA,GRPQUOTA), path, egrp_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		return ret;

	dp->bsize = (uint64_t)QUOTABLOCK_SIZE;
	dp->softlimit = (uint64_t)D.dqb_bsoftlimit;
	dp->hardlimit = (uint64_t)D.dqb_bhardlimit;
	dp->ihardlimit = (uint64_t)D.dqb_ihardlimit;
	dp->isoftlimit = (uint64_t)D.dqb_isoftlimit;
	dp->curinodes = (uint64_t)D.dqb_curinodes;
	dp->curblocks = (uint64_t)D.dqb_curblocks;

	return ret;
}

static int get_smb_linux_v2_quota(char *path, uid_t euser_id, gid_t egrp_id, LINUX_SMB_DISK_QUOTA *dp)
{
	struct v2_kern_dqblk D;
	int ret;

	ZERO_STRUCT(D);

	ret = quotactl(QCMD(Q_V2_GETQUOTA,USRQUOTA), path, euser_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		ret = quotactl(QCMD(Q_V2_GETQUOTA,GRPQUOTA), path, egrp_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		return ret;

	dp->bsize = (uint64_t)QUOTABLOCK_SIZE;
	dp->softlimit = (uint64_t)D.dqb_bsoftlimit;
	dp->hardlimit = (uint64_t)D.dqb_bhardlimit;
	dp->ihardlimit = (uint64_t)D.dqb_ihardlimit;
	dp->isoftlimit = (uint64_t)D.dqb_isoftlimit;
	dp->curinodes = (uint64_t)D.dqb_curinodes;
	dp->curblocks = ((uint64_t)D.dqb_curspace) / dp->bsize;

	return ret;
}

/****************************************************************************
 Brand-new generic quota interface.
****************************************************************************/

static int get_smb_linux_gen_quota(char *path, uid_t euser_id, gid_t egrp_id, LINUX_SMB_DISK_QUOTA *dp)
{
	struct if_dqblk D;
	int ret;

	ZERO_STRUCT(D);

	ret = quotactl(QCMD(Q_GETQUOTA,USRQUOTA), path, euser_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		ret = quotactl(QCMD(Q_GETQUOTA,GRPQUOTA), path, egrp_id, (caddr_t)&D);

	if (ret && errno != EDQUOT)
		return ret;

	dp->bsize = (uint64_t)QUOTABLOCK_SIZE;
	dp->softlimit = (uint64_t)D.dqb_bsoftlimit;
	dp->hardlimit = (uint64_t)D.dqb_bhardlimit;
	dp->ihardlimit = (uint64_t)D.dqb_ihardlimit;
	dp->isoftlimit = (uint64_t)D.dqb_isoftlimit;
	dp->curinodes = (uint64_t)D.dqb_curinodes;
	dp->curblocks = ((uint64_t)D.dqb_curspace) / dp->bsize;

	return ret;
}

/****************************************************************************
 Try to get the disk space from disk quotas (LINUX version).
****************************************************************************/

bool disk_quotas(const char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
	int r;
	SMB_STRUCT_STAT S;
	FILE *fp;
	LINUX_SMB_DISK_QUOTA D;
	struct mntent *mnt;
	SMB_DEV_T devno;
	int found;
	uid_t euser_id;
	gid_t egrp_id;

	ZERO_STRUCT(D);

	euser_id = geteuid();
	egrp_id = getegid();

	/* find the block device file */

	if (sys_stat(path, &S, false) == -1 )
		return(False) ;

	devno = S.st_ex_dev ;

	if ((fp = setmntent(MOUNTED,"r")) == NULL)
		return(False) ;

	found = False ;

	while ((mnt = getmntent(fp))) {
		if (sys_stat(mnt->mnt_dir, &S, false) == -1)
			continue ;

		if (S.st_ex_dev == devno) {
			found = True ;
			break;
		}
	}

	endmntent(fp) ;

	if (!found)
		return(False);

	become_root();

	if (strcmp(mnt->mnt_type, "nfs") == 0) {
		bool retval;
		retval = nfs_quotas(mnt->mnt_fsname , euser_id, bsize, dfree, dsize);
		unbecome_root();
		return retval;
	}

	if (strcmp(mnt->mnt_type, "xfs")==0) {
		r=get_smb_linux_xfs_quota(mnt->mnt_fsname, euser_id, egrp_id, &D);
	} else {
		r=get_smb_linux_gen_quota(mnt->mnt_fsname, euser_id, egrp_id, &D);
		if (r == -1 && errno != EDQUOT) {
			r=get_smb_linux_v2_quota(mnt->mnt_fsname, euser_id, egrp_id, &D);
			if (r == -1 && errno != EDQUOT)
				r=get_smb_linux_v1_quota(mnt->mnt_fsname, euser_id, egrp_id, &D);
		}
	}

	unbecome_root();

	/* Use softlimit to determine disk space, except when it has been exceeded */
	*bsize = D.bsize;
	if (r == -1) {
		if (errno == EDQUOT) {
			*dfree =0;
			*dsize =D.curblocks;
			return (True);
		} else {
			return(False);
		}
	}

	/* Use softlimit to determine disk space, except when it has been exceeded */
	if (
		(D.softlimit && D.curblocks >= D.softlimit) ||
		(D.hardlimit && D.curblocks >= D.hardlimit) ||
		(D.isoftlimit && D.curinodes >= D.isoftlimit) ||
		(D.ihardlimit && D.curinodes>=D.ihardlimit)
	) {
		*dfree = 0;
		*dsize = D.curblocks;
	} else if (D.softlimit==0 && D.hardlimit==0) {
		return(False);
	} else {
		if (D.softlimit == 0)
			D.softlimit = D.hardlimit;
		*dfree = D.softlimit - D.curblocks;
		*dsize = D.softlimit;
	}

	return (True);
}

#elif defined(CRAY)

#include <sys/quota.h>
#include <mntent.h>

/****************************************************************************
try to get the disk space from disk quotas (CRAY VERSION)
****************************************************************************/

bool disk_quotas(const char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
	struct mntent *mnt;
	FILE *fd;
	SMB_STRUCT_STAT sbuf;
	SMB_DEV_T devno ;
	struct q_request request ;
	struct qf_header header ;
	int quota_default = 0 ;
	bool found = false;

	if (sys_stat(path, &sbuf, false) == -1) {
		return false;
	}

	devno = sbuf.st_ex_dev ;

	if ((fd = setmntent(KMTAB)) == NULL) {
		return false;
	}

	while ((mnt = getmntent(fd)) != NULL) {
		if (sys_stat(mnt->mnt_dir, &sbuf, false) == -1) {
			continue;
		}
		if (sbuf.st_ex_dev == devno) {
			found = frue ;
			break;
		}
	}

	name = talloc_strdup(talloc_tos(), mnt->mnt_dir);
	endmntent(fd);
	if (!found) {
		return false;
	}

	if (!name) {
		return false;
	}

	request.qf_magic = QF_MAGIC ;
	request.qf_entry.id = geteuid() ;

	if (quotactl(name, Q_GETQUOTA, &request) == -1) {
		return false;
	}

	if (!request.user) {
		return False;
	}

	if (request.qf_entry.user_q.f_quota == QFV_DEFAULT) {
		if (!quota_default) {
			if (quotactl(name, Q_GETHEADER, &header) == -1) {
				return false;
			} else {
				quota_default = header.user_h.def_fq;
			}
		}
		*dfree = quota_default;
	} else if (request.qf_entry.user_q.f_quota == QFV_PREVENT) {
		*dfree = 0;
	} else {
		*dfree = request.qf_entry.user_q.f_quota;
	}

	*dsize = request.qf_entry.user_q.f_use;

	if (*dfree < *dsize) {
		*dfree = 0;
	} else {
		*dfree -= *dsize;
	}

	*bsize = 4096 ;  /* Cray blocksize */
	return true;
}


#elif defined(SUNOS5) || defined(SUNOS4)

#include <fcntl.h>
#include <sys/param.h>
#if defined(SUNOS5)
#include <sys/fs/ufs_quota.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else /* defined(SUNOS4) */
#include <ufs/quota.h>
#include <mntent.h>
#endif

#if defined(SUNOS5)

/****************************************************************************
 Allows querying of remote hosts for quotas on NFS mounted shares.
 Supports normal NFS and AMD mounts.
 Alan Romeril <a.romeril@ic.ac.uk> July 2K.
****************************************************************************/

#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpcsvc/rquota.h>
#include <rpc/nettype.h>
#include <rpc/xdr.h>

static int my_xdr_getquota_args(XDR *xdrsp, struct getquota_args *args)
{
	if (!xdr_string(xdrsp, &args->gqa_pathp, RQ_PATHLEN ))
		return(0);
	if (!xdr_int(xdrsp, &args->gqa_uid))
		return(0);
	return (1);
}

static int my_xdr_getquota_rslt(XDR *xdrsp, struct getquota_rslt *gqr)
{
	int quotastat;

	if (!xdr_int(xdrsp, &quotastat)) {
		DEBUG(6,("nfs_quotas: Status bad or zero\n"));
		return 0;
	}
	gqr->status = quotastat;

	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bsize)) {
		DEBUG(6,("nfs_quotas: Block size bad or zero\n"));
		return 0;
	}
	if (!xdr_bool(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_active)) {
		DEBUG(6,("nfs_quotas: Active bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bhardlimit)) {
		DEBUG(6,("nfs_quotas: Hardlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bsoftlimit)) {
		DEBUG(6,("nfs_quotas: Softlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_curblocks)) {
		DEBUG(6,("nfs_quotas: Currentblocks bad or zero\n"));
		return 0;
	}
	return (1);
}

/* Restricted to SUNOS5 for the moment, I haven`t access to others to test. */
static bool nfs_quotas(char *nfspath, uid_t euser_id, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
	uid_t uid = euser_id;
	struct dqblk D;
	char *mnttype = nfspath;
	CLIENT *clnt;
	struct getquota_rslt gqr;
	struct getquota_args args;
	char *cutstr, *pathname, *host, *testpath;
	int len;
	static struct timeval timeout = {2,0};
	enum clnt_stat clnt_stat;
	bool ret = True;

	*bsize = *dfree = *dsize = (uint64_t)0;

	len=strcspn(mnttype, ":");
	pathname=strstr(mnttype, ":");
	cutstr = (char *) SMB_MALLOC(len+1);
	if (!cutstr)
		return False;

	memset(cutstr, '\0', len+1);
	host = strncat(cutstr,mnttype, sizeof(char) * len );
	DEBUG(5,("nfs_quotas: looking for mount on \"%s\"\n", cutstr));
	DEBUG(5,("nfs_quotas: of path \"%s\"\n", mnttype));
	testpath=strchr_m(mnttype, ':');
	args.gqa_pathp = testpath+1;
	args.gqa_uid = uid;

	DEBUG(5,("nfs_quotas: Asking for host \"%s\" rpcprog \"%i\" rpcvers \"%i\" network \"%s\"\n", host, RQUOTAPROG, RQUOTAVERS, "udp"));

	if ((clnt = clnt_create(host, RQUOTAPROG, RQUOTAVERS, "udp")) == NULL) {
		ret = False;
		goto out;
	}

	clnt->cl_auth = authunix_create_default();
	DEBUG(9,("nfs_quotas: auth_success\n"));

	clnt_stat=clnt_call(clnt, RQUOTAPROC_GETQUOTA, my_xdr_getquota_args, (caddr_t)&args, my_xdr_getquota_rslt, (caddr_t)&gqr, timeout);

	if (clnt_stat != RPC_SUCCESS) {
		DEBUG(9,("nfs_quotas: clnt_call fail\n"));
		ret = False;
		goto out;
	}

	/*
	 * gqr.status returns 0 if the rpc call fails, 1 if quotas exist, 2 if there is
	 * no quota set, and 3 if no permission to get the quota.  If 0 or 3 return
	 * something sensible.
	 */

	switch (gqr.status) {
	case 0:
		DEBUG(9,("nfs_quotas: Remote Quotas Failed!  Error \"%i\" \n", gqr.status));
		ret = False;
		goto out;

	case 1:
		DEBUG(9,("nfs_quotas: Good quota data\n"));
		D.dqb_bsoftlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit;
		D.dqb_bhardlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit;
		D.dqb_curblocks = gqr.getquota_rslt_u.gqr_rquota.rq_curblocks;
		break;

	case 2:
	case 3:
		D.dqb_bsoftlimit = 1;
		D.dqb_curblocks = 1;
		DEBUG(9,("nfs_quotas: Remote Quotas returned \"%i\" \n", gqr.status));
		break;

	default:
		DEBUG(9,("nfs_quotas: Remote Quotas Questionable!  Error \"%i\" \n", gqr.status ));
		break;
	}

	DEBUG(10,("nfs_quotas: Let`s look at D a bit closer... status \"%i\" bsize \"%i\" active? \"%i\" bhard \"%i\" bsoft \"%i\" curb \"%i\" \n",
			gqr.status,
			gqr.getquota_rslt_u.gqr_rquota.rq_bsize,
			gqr.getquota_rslt_u.gqr_rquota.rq_active,
			gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit,
			gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit,
			gqr.getquota_rslt_u.gqr_rquota.rq_curblocks));

	*bsize = gqr.getquota_rslt_u.gqr_rquota.rq_bsize;
	*dsize = D.dqb_bsoftlimit;

	if (D.dqb_curblocks > D.dqb_bsoftlimit) {
		*dfree = 0;
		*dsize = D.dqb_curblocks;
	} else
		*dfree = D.dqb_bsoftlimit - D.dqb_curblocks;

  out:

	if (clnt) {
		if (clnt->cl_auth)
			auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
	}

	DEBUG(5,("nfs_quotas: For path \"%s\" returning  bsize %.0f, dfree %.0f, dsize %.0f\n",args.gqa_pathp,(double)*bsize,(double)*dfree,(double)*dsize));

	SAFE_FREE(cutstr);
	DEBUG(10,("nfs_quotas: End of nfs_quotas\n" ));
	return ret;
}
#endif

/****************************************************************************
try to get the disk space from disk quotas (SunOS & Solaris2 version)
Quota code by Peter Urbanec (amiga@cse.unsw.edu.au).
****************************************************************************/

bool disk_quotas(const char *path,
		uint64_t *bsize,
		uint64_t *dfree,
		uint64_t *dsize)
{
	uid_t euser_id;
	int ret;
	struct dqblk D;
#if defined(SUNOS5)
	struct quotctl command;
	int file;
	struct mnttab mnt;
#else /* SunOS4 */
	struct mntent *mnt;
#endif
	char *name = NULL;
	FILE *fd;
	SMB_STRUCT_STAT sbuf;
	SMB_DEV_T devno;
	bool found = false;

	euser_id = geteuid();

	if (sys_stat(path, &sbuf, false) == -1) {
		return false;
	}

	devno = sbuf.st_ex_dev ;
	DEBUG(5,("disk_quotas: looking for path \"%s\" devno=%x\n",
		path, (unsigned int)devno));
#if defined(SUNOS5)
	if ((fd = sys_fopen(MNTTAB, "r")) == NULL) {
		return false;
	}

	while (getmntent(fd, &mnt) == 0) {
		if (sys_stat(mnt.mnt_mountp, &sbuf, false) == -1) {
			continue;
		}

		DEBUG(5,("disk_quotas: testing \"%s\" devno=%x\n",
			mnt.mnt_mountp, (unsigned int)devno));

		/* quotas are only on vxfs, UFS or NFS */
		if ((sbuf.st_ex_dev == devno) && (
			strcmp( mnt.mnt_fstype, MNTTYPE_UFS ) == 0 ||
			strcmp( mnt.mnt_fstype, "nfs" ) == 0    ||
			strcmp( mnt.mnt_fstype, "vxfs" ) == 0 )) {
				found = true;
				name = talloc_asprintf(talloc_tos(),
						"%s/quotas",
						mnt.mnt_mountp);
				break;
		}
	}

	fclose(fd);
#else /* SunOS4 */
	if ((fd = setmntent(MOUNTED, "r")) == NULL) {
		return false;
	}

	while ((mnt = getmntent(fd)) != NULL) {
		if (sys_stat(mnt->mnt_dir, &sbuf, false) == -1) {
			continue;
		}
		DEBUG(5,("disk_quotas: testing \"%s\" devno=%x\n",
					mnt->mnt_dir,
					(unsigned int)sbuf.st_ex_dev));
		if (sbuf.st_ex_dev == devno) {
			found = true;
			name = talloc_strdup(talloc_tos(),
					mnt->mnt_fsname);
			break;
		}
	}

	endmntent(fd);
#endif
	if (!found) {
		return false;
	}

	if (!name) {
		return false;
	}
	become_root();

#if defined(SUNOS5)
	if (strcmp(mnt.mnt_fstype, "nfs") == 0) {
		bool retval;
		DEBUG(5,("disk_quotas: looking for mountpath (NFS) \"%s\"\n",
					mnt.mnt_special));
		retval = nfs_quotas(mnt.mnt_special,
				euser_id, bsize, dfree, dsize);
		unbecome_root();
		return retval;
	}

	DEBUG(5,("disk_quotas: looking for quotas file \"%s\"\n", name));
	if((file=sys_open(name, O_RDONLY,0))<0) {
		unbecome_root();
		return false;
	}
	command.op = Q_GETQUOTA;
	command.uid = euser_id;
	command.addr = (caddr_t) &D;
	ret = ioctl(file, Q_QUOTACTL, &command);
	close(file);
#else
	DEBUG(5,("disk_quotas: trying quotactl on device \"%s\"\n", name));
	ret = quotactl(Q_GETQUOTA, name, euser_id, &D);
#endif

	unbecome_root();

	if (ret < 0) {
		DEBUG(5,("disk_quotas ioctl (Solaris) failed. Error = %s\n",
					strerror(errno) ));

#if defined(SUNOS5) && defined(VXFS_QUOTA)
		/* If normal quotactl() fails, try vxfs private calls */
		set_effective_uid(euser_id);
		DEBUG(5,("disk_quotas: mount type \"%s\"\n", mnt.mnt_fstype));
		if ( 0 == strcmp ( mnt.mnt_fstype, "vxfs" )) {
			bool retval;
			retval = disk_quotas_vxfs(name, path,
					bsize, dfree, dsize);
			return retval;
		}
#else
		return false;
#endif
	}

	/* If softlimit is zero, set it equal to hardlimit.
	 */

	if (D.dqb_bsoftlimit==0) {
		D.dqb_bsoftlimit = D.dqb_bhardlimit;
	}

	/* Use softlimit to determine disk space. A user exceeding the quota
	 * is told that there's no space left. Writes might actually work for
	 * a bit if the hardlimit is set higher than softlimit. Effectively
	 * the disk becomes made of rubber latex and begins to expand to
	 * accommodate the user :-)
	 */

	if (D.dqb_bsoftlimit==0)
		return(False);
	*bsize = DEV_BSIZE;
	*dsize = D.dqb_bsoftlimit;

	if (D.dqb_curblocks > D.dqb_bsoftlimit) {
		*dfree = 0;
		*dsize = D.dqb_curblocks;
	} else {
		*dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
	}

	DEBUG(5,("disk_quotas for path \"%s\" returning "
		"bsize %.0f, dfree %.0f, dsize %.0f\n",
		path,(double)*bsize,(double)*dfree,(double)*dsize));

	return true;
}


#elif defined(OSF1)
#include <ufs/quota.h>

/****************************************************************************
try to get the disk space from disk quotas - OSF1 version
****************************************************************************/

bool disk_quotas(const char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
  int r, save_errno;
  struct dqblk D;
  SMB_STRUCT_STAT S;
  uid_t euser_id;

  /*
   * This code presumes that OSF1 will only
   * give out quota info when the real uid 
   * matches the effective uid. JRA.
   */
  euser_id = geteuid();
  save_re_uid();
  if (set_re_uid() != 0) return False;

  r= quotactl(path,QCMD(Q_GETQUOTA, USRQUOTA),euser_id,(char *) &D);
  if (r) {
     save_errno = errno;
  }

  restore_re_uid();

  *bsize = DEV_BSIZE;

  if (r)
  {
      if (save_errno == EDQUOT)   /* disk quota exceeded */
      {
         *dfree = 0;
         *dsize = D.dqb_curblocks;
         return (True);
      }
      else
         return (False);  
  }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  /* Use softlimit to determine disk space, except when it has been exceeded */

  if (D.dqb_bsoftlimit==0)
    return(False);

  if ((D.dqb_curblocks>D.dqb_bsoftlimit)) {
    *dfree = 0;
    *dsize = D.dqb_curblocks;
  } else {
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
    *dsize = D.dqb_bsoftlimit;
  }
  return (True);
}

#elif defined (IRIX6)
/****************************************************************************
try to get the disk space from disk quotas (IRIX 6.2 version)
****************************************************************************/

#include <sys/quota.h>
#include <mntent.h>

bool disk_quotas(const char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
  uid_t euser_id;
  int r;
  struct dqblk D;
  struct fs_disk_quota        F;
  SMB_STRUCT_STAT S;
  FILE *fp;
  struct mntent *mnt;
  SMB_DEV_T devno;
  int found;
  
  /* find the block device file */
  
  if ( sys_stat(path, &S, false) == -1 ) {
    return(False) ;
  }

  devno = S.st_ex_dev ;
  
  fp = setmntent(MOUNTED,"r");
  found = False ;
  
  while ((mnt = getmntent(fp))) {
    if ( sys_stat(mnt->mnt_dir, &S, false) == -1 )
      continue ;
    if (S.st_ex_dev == devno) {
      found = True ;
      break ;
    }
  }
  endmntent(fp) ;
  
  if (!found) {
    return(False);
  }

  euser_id=geteuid();
  become_root();

  /* Use softlimit to determine disk space, except when it has been exceeded */

  *bsize = 512;

  if ( 0 == strcmp ( mnt->mnt_type, "efs" ))
  {
    r=quotactl (Q_GETQUOTA, mnt->mnt_fsname, euser_id, (caddr_t) &D);

    unbecome_root();

    if (r==-1)
      return(False);
        
    /* Use softlimit to determine disk space, except when it has been exceeded */
    if (
        (D.dqb_bsoftlimit && D.dqb_curblocks>=D.dqb_bsoftlimit) ||
        (D.dqb_bhardlimit && D.dqb_curblocks>=D.dqb_bhardlimit) ||
        (D.dqb_fsoftlimit && D.dqb_curfiles>=D.dqb_fsoftlimit) ||
        (D.dqb_fhardlimit && D.dqb_curfiles>=D.dqb_fhardlimit)
       )
    {
      *dfree = 0;
      *dsize = D.dqb_curblocks;
    }
    else if (D.dqb_bsoftlimit==0 && D.dqb_bhardlimit==0)
    {
      return(False);
    }
    else 
    {
      *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
      *dsize = D.dqb_bsoftlimit;
    }

  }
  else if ( 0 == strcmp ( mnt->mnt_type, "xfs" ))
  {
    r=quotactl (Q_XGETQUOTA, mnt->mnt_fsname, euser_id, (caddr_t) &F);

    unbecome_root();

    if (r==-1)
    {
      DEBUG(5, ("quotactl for uid=%u: %s", euser_id, strerror(errno)));
      return(False);
    }
        
    /* No quota for this user. */
    if (F.d_blk_softlimit==0 && F.d_blk_hardlimit==0)
    {
      return(False);
    }

    /* Use softlimit to determine disk space, except when it has been exceeded */
    if (
        (F.d_blk_softlimit && F.d_bcount>=F.d_blk_softlimit) ||
        (F.d_blk_hardlimit && F.d_bcount>=F.d_blk_hardlimit) ||
        (F.d_ino_softlimit && F.d_icount>=F.d_ino_softlimit) ||
        (F.d_ino_hardlimit && F.d_icount>=F.d_ino_hardlimit)
       )
    {
      *dfree = 0;
      *dsize = F.d_bcount;
    }
    else 
    {
      *dfree = (F.d_blk_softlimit - F.d_bcount);
      *dsize = F.d_blk_softlimit ? F.d_blk_softlimit : F.d_blk_hardlimit;
    }

  }
  else
  {
	  unbecome_root();
	  return(False);
  }

  return (True);

}

#else

#if    defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <ufs/ufs/quota.h>
#include <machine/param.h>
#elif         AIX
/* AIX quota patch from Ole Holm Nielsen <ohnielse@fysik.dtu.dk> */
#include <jfs/quota.h>
/* AIX 4.X: Rename members of the dqblk structure (ohnielse@fysik.dtu.dk) */
#define dqb_curfiles dqb_curinodes
#define dqb_fhardlimit dqb_ihardlimit
#define dqb_fsoftlimit dqb_isoftlimit
#ifdef _AIXVERSION_530 
#include <sys/statfs.h>
#include <sys/vmount.h>
#endif /* AIX 5.3 */
#else /* !__FreeBSD__ && !AIX && !__OpenBSD__ && !__DragonFly__ */
#include <sys/quota.h>
#include <devnm.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)

#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpcsvc/rquota.h>
#ifdef HAVE_RPC_NETTYPE_H
#include <rpc/nettype.h>
#endif
#include <rpc/xdr.h>

static int my_xdr_getquota_args(XDR *xdrsp, struct getquota_args *args)
{
	if (!xdr_string(xdrsp, &args->gqa_pathp, RQ_PATHLEN ))
		return(0);
	if (!xdr_int(xdrsp, &args->gqa_uid))
		return(0);
	return (1);
}

static int my_xdr_getquota_rslt(XDR *xdrsp, struct getquota_rslt *gqr)
{
	int quotastat;

	if (!xdr_int(xdrsp, &quotastat)) {
		DEBUG(6,("nfs_quotas: Status bad or zero\n"));
		return 0;
	}
	gqr->status = quotastat;

	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bsize)) {
		DEBUG(6,("nfs_quotas: Block size bad or zero\n"));
		return 0;
	}
	if (!xdr_bool(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_active)) {
		DEBUG(6,("nfs_quotas: Active bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bhardlimit)) {
		DEBUG(6,("nfs_quotas: Hardlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_bsoftlimit)) {
		DEBUG(6,("nfs_quotas: Softlimit bad or zero\n"));
		return 0;
	}
	if (!xdr_int(xdrsp, &gqr->getquota_rslt_u.gqr_rquota.rq_curblocks)) {
		DEBUG(6,("nfs_quotas: Currentblocks bad or zero\n"));
		return 0;
	}
	return (1);
}

/* Works on FreeBSD, too. :-) */
static bool nfs_quotas(char *nfspath, uid_t euser_id, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
	uid_t uid = euser_id;
	struct dqblk D;
	char *mnttype = nfspath;
	CLIENT *clnt;
	struct getquota_rslt gqr;
	struct getquota_args args;
	char *cutstr, *pathname, *host, *testpath;
	int len;
	static struct timeval timeout = {2,0};
	enum clnt_stat clnt_stat;
	bool ret = True;

	*bsize = *dfree = *dsize = (uint64_t)0;

	len=strcspn(mnttype, ":");
	pathname=strstr(mnttype, ":");
	cutstr = (char *) SMB_MALLOC(len+1);
	if (!cutstr)
		return False;

	memset(cutstr, '\0', len+1);
	host = strncat(cutstr,mnttype, sizeof(char) * len );
	DEBUG(5,("nfs_quotas: looking for mount on \"%s\"\n", cutstr));
	DEBUG(5,("nfs_quotas: of path \"%s\"\n", mnttype));
	testpath=strchr_m(mnttype, ':');
	args.gqa_pathp = testpath+1;
	args.gqa_uid = uid;

	DEBUG(5,("nfs_quotas: Asking for host \"%s\" rpcprog \"%i\" rpcvers \"%i\" network \"%s\"\n", host, RQUOTAPROG, RQUOTAVERS, "udp"));

	if ((clnt = clnt_create(host, RQUOTAPROG, RQUOTAVERS, "udp")) == NULL) {
		ret = False;
		goto out;
	}

	clnt->cl_auth = authunix_create_default();
	DEBUG(9,("nfs_quotas: auth_success\n"));

	clnt_stat=clnt_call(clnt, RQUOTAPROC_GETQUOTA, (const xdrproc_t) my_xdr_getquota_args, (caddr_t)&args, (const xdrproc_t) my_xdr_getquota_rslt, (caddr_t)&gqr, timeout);

	if (clnt_stat != RPC_SUCCESS) {
		DEBUG(9,("nfs_quotas: clnt_call fail\n"));
		ret = False;
		goto out;
	}

	/* 
	 * gqr->status returns 0 if the rpc call fails, 1 if quotas exist, 2 if there is
	 * no quota set, and 3 if no permission to get the quota.  If 0 or 3 return
	 * something sensible.
	 */   

	switch (gqr.status) {
	case 0:
		DEBUG(9,("nfs_quotas: Remote Quotas Failed!  Error \"%i\" \n", gqr.status));
		ret = False;
		goto out;

	case 1:
		DEBUG(9,("nfs_quotas: Good quota data\n"));
		D.dqb_bsoftlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit;
		D.dqb_bhardlimit = gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit;
		D.dqb_curblocks = gqr.getquota_rslt_u.gqr_rquota.rq_curblocks;
		break;

	case 2:
	case 3:
		D.dqb_bsoftlimit = 1;
		D.dqb_curblocks = 1;
		DEBUG(9,("nfs_quotas: Remote Quotas returned \"%i\" \n", gqr.status));
		break;

	default:
		DEBUG(9,("nfs_quotas: Remote Quotas Questionable!  Error \"%i\" \n", gqr.status));
		break;
	}

	DEBUG(10,("nfs_quotas: Let`s look at D a bit closer... status \"%i\" bsize \"%i\" active? \"%i\" bhard \"%i\" bsoft \"%i\" curb \"%i\" \n",
			gqr.status,
			gqr.getquota_rslt_u.gqr_rquota.rq_bsize,
			gqr.getquota_rslt_u.gqr_rquota.rq_active,
			gqr.getquota_rslt_u.gqr_rquota.rq_bhardlimit,
			gqr.getquota_rslt_u.gqr_rquota.rq_bsoftlimit,
			gqr.getquota_rslt_u.gqr_rquota.rq_curblocks));

	if (D.dqb_bsoftlimit == 0)
		D.dqb_bsoftlimit = D.dqb_bhardlimit;
	if (D.dqb_bsoftlimit == 0)
		return False;

	*bsize = gqr.getquota_rslt_u.gqr_rquota.rq_bsize;
	*dsize = D.dqb_bsoftlimit;

	if (D.dqb_curblocks == 1)
		*bsize = DEV_BSIZE;

	if (D.dqb_curblocks > D.dqb_bsoftlimit) {
		*dfree = 0;
		*dsize = D.dqb_curblocks;
	} else
		*dfree = D.dqb_bsoftlimit - D.dqb_curblocks;

  out:

	if (clnt) {
		if (clnt->cl_auth)
			auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
	}

	DEBUG(5,("nfs_quotas: For path \"%s\" returning  bsize %.0f, dfree %.0f, dsize %.0f\n",args.gqa_pathp,(double)*bsize,(double)*dfree,(double)*dsize));

	SAFE_FREE(cutstr);
	DEBUG(10,("nfs_quotas: End of nfs_quotas\n" ));
	return ret;
}

#endif

/****************************************************************************
try to get the disk space from disk quotas - default version
****************************************************************************/

bool disk_quotas(const char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
  int r;
  struct dqblk D;
  uid_t euser_id;
#if !defined(__FreeBSD__) && !defined(AIX) && !defined(__OpenBSD__) && !defined(__DragonFly__)
  char dev_disk[256];
  SMB_STRUCT_STAT S;

  /* find the block device file */

#ifdef HPUX
  /* Need to set the cache flag to 1 for HPUX. Seems
   * to have a significant performance boost when
   * lstat calls on /dev access this function.
   */
  if ((sys_stat(path, &S, false)<0)
      || (devnm(S_IFBLK, S.st_ex_dev, dev_disk, 256, 1)<0))
#else
  if ((sys_stat(path, &S, false)<0)
      || (devnm(S_IFBLK, S.st_ex_dev, dev_disk, 256, 0)<0))
	return (False);
#endif /* ifdef HPUX */

#endif /* !defined(__FreeBSD__) && !defined(AIX) && !defined(__OpenBSD__) && !defined(__DragonFly__) */

  euser_id = geteuid();

#ifdef HPUX
  /* for HPUX, real uid must be same as euid to execute quotactl for euid */
  save_re_uid();
  if (set_re_uid() != 0) return False;
  
  r=quotactl(Q_GETQUOTA, dev_disk, euser_id, &D);

  restore_re_uid();
#else 
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
  {
    /* FreeBSD patches from Marty Moll <martym@arbor.edu> */
    gid_t egrp_id;
#if defined(__FreeBSD__) || defined(__DragonFly__)
    SMB_DEV_T devno;
    struct statfs *mnts;
    SMB_STRUCT_STAT st;
    int mntsize, i;
    
    if (sys_stat(path, &st, false) < 0)
        return False;
    devno = st.st_ex_dev;

    mntsize = getmntinfo(&mnts,MNT_NOWAIT);
    if (mntsize <= 0)
        return False;

    for (i = 0; i < mntsize; i++) {
	if (sys_stat(mnts[i].f_mntonname, &st, false) < 0)
            return False;
        if (st.st_ex_dev == devno)
            break;
    }
    if (i == mntsize)
        return False;
#endif
    
    become_root();

#if defined(__FreeBSD__) || defined(__DragonFly__)
    if (strcmp(mnts[i].f_fstypename,"nfs") == 0) {
        bool retval;
        retval = nfs_quotas(mnts[i].f_mntfromname,euser_id,bsize,dfree,dsize);
        unbecome_root();
        return retval;
    }
#endif

    egrp_id = getegid();
    r= quotactl(path,QCMD(Q_GETQUOTA,USRQUOTA),euser_id,(char *) &D);

    /* As FreeBSD has group quotas, if getting the user
       quota fails, try getting the group instead. */
    if (r) {
	    r= quotactl(path,QCMD(Q_GETQUOTA,GRPQUOTA),egrp_id,(char *) &D);
    }

    unbecome_root();
  }
#elif defined(AIX)
  /* AIX has both USER and GROUP quotas: 
     Get the USER quota (ohnielse@fysik.dtu.dk) */
#ifdef _AIXVERSION_530
  {
    struct statfs statbuf;
    quota64_t user_quota;
    if (statfs(path,&statbuf) != 0)
      return False;
    if(statbuf.f_vfstype == MNT_J2)
    {
    /* For some reason we need to be root for jfs2 */
      become_root();
      r = quotactl(path,QCMD(Q_J2GETQUOTA,USRQUOTA),euser_id,(char *) &user_quota);
      unbecome_root();
    /* Copy results to old struct to let the following code work as before */
      D.dqb_curblocks  = user_quota.bused;
      D.dqb_bsoftlimit = user_quota.bsoft;
      D.dqb_bhardlimit = user_quota.bhard;
      D.dqb_curfiles   = user_quota.iused;
      D.dqb_fsoftlimit = user_quota.isoft;
      D.dqb_fhardlimit = user_quota.ihard;
    }
    else if(statbuf.f_vfstype == MNT_JFS)
    {
#endif /* AIX 5.3 */
  save_re_uid();
  if (set_re_uid() != 0) 
    return False;
  r= quotactl(path,QCMD(Q_GETQUOTA,USRQUOTA),euser_id,(char *) &D);
  restore_re_uid();
#ifdef _AIXVERSION_530
    }
    else
      r = 1; /* Fail for other FS-types */
  }
#endif /* AIX 5.3 */
#else /* !__FreeBSD__ && !AIX && !__OpenBSD__ && !__DragonFly__ */
  r=quotactl(Q_GETQUOTA, dev_disk, euser_id, &D);
#endif /* !__FreeBSD__ && !AIX && !__OpenBSD__ && !__DragonFly__ */
#endif /* HPUX */

  /* Use softlimit to determine disk space, except when it has been exceeded */
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
  *bsize = DEV_BSIZE;
#else /* !__FreeBSD__ && !__OpenBSD__ && !__DragonFly__ */
  *bsize = 1024;
#endif /*!__FreeBSD__ && !__OpenBSD__ && !__DragonFly__ */

  if (r)
    {
      if (errno == EDQUOT) 
	{
 	  *dfree =0;
 	  *dsize =D.dqb_curblocks;
 	  return (True);
	}
      else return(False);
    }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  if (D.dqb_bsoftlimit==0)
    return(False);
  /* Use softlimit to determine disk space, except when it has been exceeded */
  if ((D.dqb_curblocks>D.dqb_bsoftlimit)
#if !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__)
||((D.dqb_curfiles>D.dqb_fsoftlimit) && (D.dqb_fsoftlimit != 0))
#endif
    ) {
      *dfree = 0;
      *dsize = D.dqb_curblocks;
    }
  else {
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
    *dsize = D.dqb_bsoftlimit;
  }
  return (True);
}

#endif

#if defined(VXFS_QUOTA)

/****************************************************************************
Try to get the disk space from Veritas disk quotas.
    David Lee <T.D.Lee@durham.ac.uk> August 1999.

Background assumptions:
    Potentially under many Operating Systems.  Initially Solaris 2.

    My guess is that Veritas is largely, though not entirely,
    independent of OS.  So I have separated it out.

    There may be some details.  For example, OS-specific "include" files.

    It is understood that HPUX 10 somehow gets Veritas quotas without
    any special effort; if so, this routine need not be compiled in.
        Dirk De Wachter <Dirk.DeWachter@rug.ac.be>

Warning:
    It is understood that Veritas do not publicly support this ioctl interface.
    Rather their preference would be for the user (us) to call the native
    OS and then for the OS itself to call through to the VxFS filesystem.
    Presumably HPUX 10, see above, does this.

Hints for porting:
    Add your OS to "IFLIST" below.
    Get it to compile successfully:
        Almost certainly "include"s require attention: see SUNOS5.
    In the main code above, arrange for it to be called: see SUNOS5.
    Test!
    
****************************************************************************/

/* "IFLIST"
 * This "if" is a list of ports:
 *	if defined(OS1) || defined(OS2) || ...
 */
#if defined(SUNOS5)

#if defined(SUNOS5)
#include <sys/fs/vx_solaris.h>
#endif
#include <sys/fs/vx_machdep.h>
#include <sys/fs/vx_layout.h>
#include <sys/fs/vx_quota.h>
#include <sys/fs/vx_aioctl.h>
#include <sys/fs/vx_ioctl.h>

bool disk_quotas_vxfs(const char *name, char *path, uint64_t *bsize, uint64_t *dfree, uint64_t *dsize)
{
  uid_t user_id, euser_id;
  int ret;
  struct vx_dqblk D;
  struct vx_quotctl quotabuf;
  struct vx_genioctl genbuf;
  char *qfname;
  int file;

  /*
   * "name" may or may not include a trailing "/quotas".
   * Arranging consistency of calling here in "quotas.c" may not be easy and
   * it might be easier to examine and adjust it here.
   * Fortunately, VxFS seems not to mind at present.
   */
  qfname = talloc_strdup(talloc_tos(), name);
  if (!qfname) {
	  return false;
  }
  /* pstrcat(qfname, "/quotas") ; */	/* possibly examine and adjust "name" */

  euser_id = geteuid();
  set_effective_uid(0);

  DEBUG(5,("disk_quotas: looking for VxFS quotas file \"%s\"\n", qfname));
  if((file=sys_open(qfname, O_RDONLY,0))<0) {
    set_effective_uid(euser_id);
    return(False);
  }
  genbuf.ioc_cmd = VX_QUOTACTL;
  genbuf.ioc_up = (void *) &quotabuf;

  quotabuf.cmd = VX_GETQUOTA;
  quotabuf.uid = euser_id;
  quotabuf.addr = (caddr_t) &D;
  ret = ioctl(file, VX_ADMIN_IOCTL, &genbuf);
  close(file);

  set_effective_uid(euser_id);

  if (ret < 0) {
    DEBUG(5,("disk_quotas ioctl (VxFS) failed. Error = %s\n", strerror(errno) ));
    return(False);
  }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  /* Use softlimit to determine disk space. A user exceeding the quota is told
   * that there's no space left. Writes might actually work for a bit if the
   * hardlimit is set higher than softlimit. Effectively the disk becomes
   * made of rubber latex and begins to expand to accommodate the user :-)
   */
  DEBUG(5,("disk_quotas for path \"%s\" block c/s/h %ld/%ld/%ld; file c/s/h %ld/%ld/%ld\n",
         path, D.dqb_curblocks, D.dqb_bsoftlimit, D.dqb_bhardlimit,
         D.dqb_curfiles, D.dqb_fsoftlimit, D.dqb_fhardlimit));

  if (D.dqb_bsoftlimit==0)
    return(False);
  *bsize = DEV_BSIZE;
  *dsize = D.dqb_bsoftlimit;

  if (D.dqb_curblocks > D.dqb_bsoftlimit) {
     *dfree = 0;
     *dsize = D.dqb_curblocks;
  } else
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
      
  DEBUG(5,("disk_quotas for path \"%s\" returning  bsize %.0f, dfree %.0f, dsize %.0f\n",
         path,(double)*bsize,(double)*dfree,(double)*dsize));

  return(True);
}

#endif /* SUNOS5 || ... */

#endif /* VXFS_QUOTA */

#else /* WITH_QUOTAS */

bool disk_quotas(const char *path,uint64_t *bsize,uint64_t *dfree,uint64_t *dsize)
{
	(*bsize) = 512; /* This value should be ignored */

	/* And just to be sure we set some values that hopefully */
	/* will be larger that any possible real-world value     */
	(*dfree) = (uint64_t)-1;
	(*dsize) = (uint64_t)-1;

	/* As we have select not to use quotas, allways fail */
	return false;
}
#endif /* WITH_QUOTAS */

#else /* HAVE_SYS_QUOTAS */
/* wrapper to the new sys_quota interface
   this file should be removed later
   */
bool disk_quotas(const char *path,uint64_t *bsize,uint64_t *dfree,uint64_t *dsize)
{
	int r;
	SMB_DISK_QUOTA D;
	unid_t id;

	id.uid = geteuid();

	ZERO_STRUCT(D);
  	r=sys_get_quota(path, SMB_USER_QUOTA_TYPE, id, &D);

	/* Use softlimit to determine disk space, except when it has been exceeded */
	*bsize = D.bsize;
	if (r == -1) {
		if (errno == EDQUOT) {
			*dfree =0;
			*dsize =D.curblocks;
			return (True);
		} else {
			goto try_group_quota;
		}
	}

	/* Use softlimit to determine disk space, except when it has been exceeded */
	if (
		(D.softlimit && D.curblocks >= D.softlimit) ||
		(D.hardlimit && D.curblocks >= D.hardlimit) ||
		(D.isoftlimit && D.curinodes >= D.isoftlimit) ||
		(D.ihardlimit && D.curinodes>=D.ihardlimit)
	) {
		*dfree = 0;
		*dsize = D.curblocks;
	} else if (D.softlimit==0 && D.hardlimit==0) {
		goto try_group_quota;
	} else {
		if (D.softlimit == 0)
			D.softlimit = D.hardlimit;
		*dfree = D.softlimit - D.curblocks;
		*dsize = D.softlimit;
	}

	return True;
	
try_group_quota:
	id.gid = getegid();

	ZERO_STRUCT(D);
  	r=sys_get_quota(path, SMB_GROUP_QUOTA_TYPE, id, &D);

	/* Use softlimit to determine disk space, except when it has been exceeded */
	*bsize = D.bsize;
	if (r == -1) {
		if (errno == EDQUOT) {
			*dfree =0;
			*dsize =D.curblocks;
			return (True);
		} else {
			return False;
		}
	}

	/* Use softlimit to determine disk space, except when it has been exceeded */
	if (
		(D.softlimit && D.curblocks >= D.softlimit) ||
		(D.hardlimit && D.curblocks >= D.hardlimit) ||
		(D.isoftlimit && D.curinodes >= D.isoftlimit) ||
		(D.ihardlimit && D.curinodes>=D.ihardlimit)
	) {
		*dfree = 0;
		*dsize = D.curblocks;
	} else if (D.softlimit==0 && D.hardlimit==0) {
		return False;
	} else {
		if (D.softlimit == 0)
			D.softlimit = D.hardlimit;
		*dfree = D.softlimit - D.curblocks;
		*dsize = D.softlimit;
	}

	return (True);
}
#endif /* HAVE_SYS_QUOTAS */
