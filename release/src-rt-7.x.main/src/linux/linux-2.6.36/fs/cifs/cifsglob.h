/*
 *   fs/cifs/cifsglob.h
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *              Jeremy Allison (jra@samba.org)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 */
#ifndef _CIFS_GLOB_H
#define _CIFS_GLOB_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "cifs_fs_sb.h"
#include "cifsacl.h"
/*
 * The sizes of various internal tables and strings
 */
#define MAX_UID_INFO 16
#define MAX_SES_INFO 2
#define MAX_TCON_INFO 4

#define MAX_TREE_SIZE (2 + MAX_SERVER_SIZE + 1 + MAX_SHARE_SIZE + 1)
#define MAX_SERVER_SIZE 15
#define MAX_SHARE_SIZE  64	/* used to be 20, this should still be enough */
#define MAX_USERNAME_SIZE 32	/* 32 is to allow for 15 char names + null
				   termination then *2 for unicode versions */
#define MAX_PASSWORD_SIZE 512  /* max for windows seems to be 256 wide chars */

#define CIFS_MIN_RCV_POOL 4

/*
 * MAX_REQ is the maximum number of requests that WE will send
 * on one socket concurrently. It also matches the most common
 * value of max multiplex returned by servers.  We may
 * eventually want to use the negotiated value (in case
 * future servers can handle more) when we are more confident that
 * we will not have problems oveloading the socket with pending
 * write data.
 */
#define CIFS_MAX_REQ 50

#define RFC1001_NAME_LEN 15
#define RFC1001_NAME_LEN_WITH_NULL (RFC1001_NAME_LEN + 1)

/* currently length of NIP6_FMT */
#define SERVER_NAME_LENGTH 40
#define SERVER_NAME_LEN_WITH_NULL     (SERVER_NAME_LENGTH + 1)

/* used to define string lengths for reversing unicode strings */
/*         (256+1)*2 = 514                                     */
/*           (max path length + 1 for null) * 2 for unicode    */
#define MAX_NAME 514

#include "cifspdu.h"

#ifndef XATTR_DOS_ATTRIB
#define XATTR_DOS_ATTRIB "user.DOSATTRIB"
#endif

/*
 * CIFS vfs client Status information (based on what we know.)
 */

 /* associated with each tcp and smb session */
enum statusEnum {
	CifsNew = 0,
	CifsGood,
	CifsExiting,
	CifsNeedReconnect
};

enum securityEnum {
	LANMAN = 0,			/* Legacy LANMAN auth */
	NTLM,			/* Legacy NTLM012 auth with NTLM hash */
	NTLMv2,			/* Legacy NTLM auth with NTLMv2 hash */
	RawNTLMSSP,		/* NTLMSSP without SPNEGO, NTLMv2 hash */
/*	NTLMSSP, */ /* can use rawNTLMSSP instead of NTLMSSP via SPNEGO */
	Kerberos,		/* Kerberos via SPNEGO */
};

enum protocolEnum {
	TCP = 0,
	SCTP
	/* Netbios frames protocol not supported at this time */
};

struct mac_key {
	unsigned int len;
	union {
		char ntlm[CIFS_SESS_KEY_SIZE + 16];
		char krb5[CIFS_SESS_KEY_SIZE + 16]; /* BB: length correct? */
		struct {
			char key[16];
			struct ntlmv2_resp resp;
		} ntlmv2;
	} data;
};

struct cifs_cred {
	int uid;
	int gid;
	int mode;
	int cecount;
	struct cifs_sid osid;
	struct cifs_sid gsid;
	struct cifs_ntace *ntaces;
	struct cifs_ace *aces;
};

/*
 *****************************************************************
 * Except the CIFS PDUs themselves all the
 * globally interesting structs should go here
 *****************************************************************
 */

struct TCP_Server_Info {
	struct list_head tcp_ses_list;
	struct list_head smb_ses_list;
	int srv_count; /* reference counter */
	/* 15 character server name + 0x20 16th byte indicating type = srv */
	char server_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	char *hostname; /* hostname portion of UNC string */
	struct socket *ssocket;
	union {
		struct sockaddr_in sockAddr;
		struct sockaddr_in6 sockAddr6;
	} addr;
	wait_queue_head_t response_q;
	wait_queue_head_t request_q; /* if more than maxmpx to srvr must block*/
	struct list_head pending_mid_q;
	void *Server_NlsInfo;	/* BB - placeholder for future NLS info  */
	unsigned short server_codepage;	/* codepage for the server    */
	enum protocolEnum protocolType;
	char versionMajor;
	char versionMinor;
	bool svlocal:1;			/* local server or remote */
	bool noblocksnd;		/* use blocking sendmsg */
	bool noautotune;		/* do not autotune send buf sizes */
	bool tcp_nodelay;
	atomic_t inFlight;  /* number of requests on the wire to server */
#ifdef CONFIG_CIFS_STATS2
	atomic_t inSend; /* requests trying to send */
	atomic_t num_waiters;   /* blocked waiting to get in sendrecv */
#endif
	enum statusEnum tcpStatus; /* what we think the status is */
	struct mutex srv_mutex;
	struct task_struct *tsk;
	char server_GUID[16];
	char secMode;
	enum securityEnum secType;
	unsigned int maxReq;	/* Clients should submit no more */
	/* than maxReq distinct unanswered SMBs to the server when using  */
	/* multiplexed reads or writes */
	unsigned int maxBuf;	/* maxBuf specifies the maximum */
	/* message size the server can send or receive for non-raw SMBs */
	unsigned int max_rw;	/* maxRw specifies the maximum */
	/* message size the server can send or receive for */
	/* SMB_COM_WRITE_RAW or SMB_COM_READ_RAW. */
	unsigned int max_vcs;	/* maximum number of smb sessions, at least
				   those that can be specified uniquely with
				   vcnumbers */
	char sessid[4];		/* unique token id for this session */
	/* (returned on Negotiate */
	int capabilities; /* allow selective disabling of caps by smb sess */
	int timeAdj;  /* Adjust for difference in server time zone in sec */
	__u16 CurrentMid;         /* multiplex id - rotating counter */
	char cryptKey[CIFS_CRYPTO_KEY_SIZE];
	/* 16th byte of RFC1001 workstation name is always null */
	char workstation_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	__u32 sequence_number; /* needed for CIFS PDU signature */
	struct mac_key mac_signing_key;
	char ntlmv2_hash[16];
	unsigned long lstrp; /* when we got last response from this server */
	u16 dialect; /* dialect index that server chose */
	/* extended security flavors that server supports */
	bool	sec_kerberos;		/* supports plain Kerberos */
	bool	sec_mskerberos;		/* supports legacy MS Kerberos */
	bool	sec_kerberosu2u;	/* supports U2U Kerberos */
	bool	sec_ntlmssp;		/* supports NTLMSSP */
#ifdef CONFIG_CIFS_FSCACHE
	struct fscache_cookie   *fscache; /* client index cache cookie */
#endif
};

/*
 * Session structure.  One of these for each uid session with a particular host
 */
struct cifsSesInfo {
	struct list_head smb_ses_list;
	struct list_head tcon_list;
	struct mutex session_mutex;
	struct TCP_Server_Info *server;	/* pointer to server info */
	int ses_count;		/* reference counter */
	enum statusEnum status;
	unsigned overrideSecFlg;  /* if non-zero override global sec flags */
	__u16 ipc_tid;		/* special tid for connection to IPC share */
	__u16 flags;
	__u16 vcnum;
	char *serverOS;		/* name of operating system underlying server */
	char *serverNOS;	/* name of network operating system of server */
	char *serverDomain;	/* security realm of server */
	int Suid;		/* remote smb uid  */
	uid_t linux_uid;        /* overriding owner of files on the mount */
	uid_t cred_uid;		/* owner of credentials */
	int capabilities;
	char serverName[SERVER_NAME_LEN_WITH_NULL * 2];	/* BB make bigger for
				TCP names - will ipv6 and sctp addresses fit? */
	char userName[MAX_USERNAME_SIZE + 1];
	char *domainName;
	char *password;
	bool need_reconnect:1; /* connection reset, uid now invalid */
};
/* no more than one of the following three session flags may be set */
#define CIFS_SES_NT4 1
#define CIFS_SES_OS2 2
#define CIFS_SES_W9X 4
/* following flag is set for old servers such as OS2 (and Win95?)
   which do not negotiate NTLM or POSIX dialects, but instead
   negotiate one of the older LANMAN dialects */
#define CIFS_SES_LANMAN 8
/*
 * there is one of these for each connection to a resource on a particular
 * session
 */
struct cifsTconInfo {
	struct list_head tcon_list;
	int tc_count;
	struct list_head openFileList;
	struct cifsSesInfo *ses;	/* pointer to session associated with */
	char treeName[MAX_TREE_SIZE + 1]; /* UNC name of resource in ASCII */
	char *nativeFileSystem;
	char *password;		/* for share-level security */
	__u16 tid;		/* The 2 byte tree id */
	__u16 Flags;		/* optional support bits */
	enum statusEnum tidStatus;
#ifdef CONFIG_CIFS_STATS
	atomic_t num_smbs_sent;
	atomic_t num_writes;
	atomic_t num_reads;
	atomic_t num_flushes;
	atomic_t num_oplock_brks;
	atomic_t num_opens;
	atomic_t num_closes;
	atomic_t num_deletes;
	atomic_t num_mkdirs;
	atomic_t num_posixopens;
	atomic_t num_posixmkdirs;
	atomic_t num_rmdirs;
	atomic_t num_renames;
	atomic_t num_t2renames;
	atomic_t num_ffirst;
	atomic_t num_fnext;
	atomic_t num_fclose;
	atomic_t num_hardlinks;
	atomic_t num_symlinks;
	atomic_t num_locks;
	atomic_t num_acl_get;
	atomic_t num_acl_set;
#ifdef CONFIG_CIFS_STATS2
	unsigned long long time_writes;
	unsigned long long time_reads;
	unsigned long long time_opens;
	unsigned long long time_deletes;
	unsigned long long time_closes;
	unsigned long long time_mkdirs;
	unsigned long long time_rmdirs;
	unsigned long long time_renames;
	unsigned long long time_t2renames;
	unsigned long long time_ffirst;
	unsigned long long time_fnext;
	unsigned long long time_fclose;
#endif /* CONFIG_CIFS_STATS2 */
	__u64    bytes_read;
	__u64    bytes_written;
	spinlock_t stat_lock;
#endif /* CONFIG_CIFS_STATS */
	FILE_SYSTEM_DEVICE_INFO fsDevInfo;
	FILE_SYSTEM_ATTRIBUTE_INFO fsAttrInfo; /* ok if fs name truncated */
	FILE_SYSTEM_UNIX_INFO fsUnixInfo;
	bool ipc:1;		/* set if connection to IPC$ eg for RPC/PIPES */
	bool retry:1;
	bool nocase:1;
	bool seal:1;      /* transport encryption for this mounted share */
	bool unix_ext:1;  /* if false disable Linux extensions to CIFS protocol
				for this mount even if server would support */
	bool local_lease:1; /* check leases (only) on local system not remote */
	bool broken_posix_open; /* e.g. Samba server versions < 3.3.2, 3.2.9 */
	bool need_reconnect:1; /* connection reset, tid now invalid */
#ifdef CONFIG_CIFS_FSCACHE
	u64 resource_id;		/* server resource id */
	struct fscache_cookie *fscache;	/* cookie for share */
#endif
	/* BB add field for back pointer to sb struct(s)? */
};

/*
 * This info hangs off the cifsFileInfo structure, pointed to by llist.
 * This is used to track byte stream locks on the file
 */
struct cifsLockInfo {
	struct list_head llist;	/* pointer to next cifsLockInfo */
	__u64 offset;
	__u64 length;
	__u8 type;
};

/*
 * One of these for each open instance of a file
 */
struct cifs_search_info {
	loff_t index_of_last_entry;
	__u16 entries_in_buffer;
	__u16 info_level;
	__u32 resume_key;
	char *ntwrk_buf_start;
	char *srch_entries_start;
	char *last_entry;
	char *presume_name;
	unsigned int resume_name_len;
	bool endOfSearch:1;
	bool emptyDir:1;
	bool unicode:1;
	bool smallBuf:1; /* so we know which buf_release function to call */
};

struct cifsFileInfo {
	struct list_head tlist;	/* pointer to next fid owned by tcon */
	struct list_head flist;	/* next fid (file instance) for this inode */
	unsigned int uid;	/* allows finding which FileInfo structure */
	__u32 pid;		/* process id who opened file */
	__u16 netfid;		/* file id from remote */
	/* BB add lock scope info here if needed */ ;
	/* lock scope id (0 if none) */
	struct file *pfile; /* needed for writepage */
	struct inode *pInode; /* needed for oplock break */
	struct vfsmount *mnt;
	struct mutex lock_mutex;
	struct list_head llist; /* list of byte range locks we have. */
	bool closePend:1;	/* file is marked to close */
	bool invalidHandle:1;	/* file closed via session abend */
	bool oplock_break_cancelled:1;
	atomic_t count;		/* reference count */
	struct mutex fh_mutex; /* prevents reopen race after dead ses*/
	struct cifs_search_info srch_inf;
	struct work_struct oplock_break; /* work for oplock breaks */
};

/* Take a reference on the file private data */
static inline void cifsFileInfo_get(struct cifsFileInfo *cifs_file)
{
	atomic_inc(&cifs_file->count);
}

/* Release a reference on the file private data */
static inline void cifsFileInfo_put(struct cifsFileInfo *cifs_file)
{
	if (atomic_dec_and_test(&cifs_file->count)) {
		iput(cifs_file->pInode);
		kfree(cifs_file);
	}
}

/*
 * One of these for each file inode
 */

struct cifsInodeInfo {
	struct list_head lockList;
	/* BB add in lists for dirty pages i.e. write caching info for oplock */
	struct list_head openFileList;
	int write_behind_rc;
	__u32 cifsAttrs; /* e.g. DOS archive bit, sparse, compressed, system */
	unsigned long time;	/* jiffies of last update/check of inode */
	bool clientCanCacheRead:1;	/* read oplock */
	bool clientCanCacheAll:1;	/* read and writebehind oplock */
	bool delete_pending:1;		/* DELETE_ON_CLOSE is set */
	bool invalid_mapping:1;		/* pagecache is invalid */
	u64  server_eof;		/* current file size on server */
	u64  uniqueid;			/* server inode number */
#ifdef CONFIG_CIFS_FSCACHE
	struct fscache_cookie *fscache;
#endif
	struct inode vfs_inode;
};

static inline struct cifsInodeInfo *
CIFS_I(struct inode *inode)
{
	return container_of(inode, struct cifsInodeInfo, vfs_inode);
}

static inline struct cifs_sb_info *
CIFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline char CIFS_DIR_SEP(const struct cifs_sb_info *cifs_sb)
{
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_POSIX_PATHS)
		return '/';
	else
		return '\\';
}

#ifdef CONFIG_CIFS_STATS
#define cifs_stats_inc atomic_inc

static inline void cifs_stats_bytes_written(struct cifsTconInfo *tcon,
					    unsigned int bytes)
{
	if (bytes) {
		spin_lock(&tcon->stat_lock);
		tcon->bytes_written += bytes;
		spin_unlock(&tcon->stat_lock);
	}
}

static inline void cifs_stats_bytes_read(struct cifsTconInfo *tcon,
					 unsigned int bytes)
{
	spin_lock(&tcon->stat_lock);
	tcon->bytes_read += bytes;
	spin_unlock(&tcon->stat_lock);
}
#else

#define  cifs_stats_inc(field) do {} while (0)
#define  cifs_stats_bytes_written(tcon, bytes) do {} while (0)
#define  cifs_stats_bytes_read(tcon, bytes) do {} while (0)

#endif

/* one of these for every pending CIFS request to the server */
struct mid_q_entry {
	struct list_head qhead;	/* mids waiting on reply from this server */
	__u16 mid;		/* multiplex id */
	__u16 pid;		/* process id */
	__u32 sequence_number;  /* for CIFS signing */
	unsigned long when_alloc;  /* when mid was created */
#ifdef CONFIG_CIFS_STATS2
	unsigned long when_sent; /* time when smb send finished */
	unsigned long when_received; /* when demux complete (taken off wire) */
#endif
	struct task_struct *tsk;	/* task waiting for response */
	struct smb_hdr *resp_buf;	/* response buffer */
	int midState;	/* wish this were enum but can not pass to wait_event */
	__u8 command;	/* smb command code */
	bool largeBuf:1;	/* if valid response, is pointer to large buf */
	bool multiRsp:1;	/* multiple trans2 responses for one request  */
	bool multiEnd:1;	/* both received */
};

struct oplock_q_entry {
	struct list_head qhead;
	struct inode *pinode;
	struct cifsTconInfo *tcon;
	__u16 netfid;
};

/* for pending dnotify requests */
struct dir_notify_req {
       struct list_head lhead;
       __le16 Pid;
       __le16 PidHigh;
       __u16 Mid;
       __u16 Tid;
       __u16 Uid;
       __u16 netfid;
       __u32 filter; /* CompletionFilter (for multishot) */
       int multishot;
       struct file *pfile;
};

struct dfs_info3_param {
	int flags; /* DFSREF_REFERRAL_SERVER, DFSREF_STORAGE_SERVER*/
	int path_consumed;
	int server_type;
	int ref_flag;
	char *path_name;
	char *node_name;
};

/*
 * common struct for holding inode info when searching for or updating an
 * inode with new info
 */

#define CIFS_FATTR_DFS_REFERRAL		0x1
#define CIFS_FATTR_DELETE_PENDING	0x2
#define CIFS_FATTR_NEED_REVAL		0x4
#define CIFS_FATTR_INO_COLLISION	0x8

struct cifs_fattr {
	u32		cf_flags;
	u32		cf_cifsattrs;
	u64		cf_uniqueid;
	u64		cf_eof;
	u64		cf_bytes;
	uid_t		cf_uid;
	gid_t		cf_gid;
	umode_t		cf_mode;
	dev_t		cf_rdev;
	unsigned int	cf_nlink;
	unsigned int	cf_dtype;
	struct timespec	cf_atime;
	struct timespec	cf_mtime;
	struct timespec	cf_ctime;
};

static inline void free_dfs_info_param(struct dfs_info3_param *param)
{
	if (param) {
		kfree(param->path_name);
		kfree(param->node_name);
		kfree(param);
	}
}

static inline void free_dfs_info_array(struct dfs_info3_param *param,
				       int number_of_items)
{
	int i;
	if ((number_of_items == 0) || (param == NULL))
		return;
	for (i = 0; i < number_of_items; i++) {
		kfree(param[i].path_name);
		kfree(param[i].node_name);
	}
	kfree(param);
}

#define   MID_FREE 0
#define   MID_REQUEST_ALLOCATED 1
#define   MID_REQUEST_SUBMITTED 2
#define   MID_RESPONSE_RECEIVED 4
#define   MID_RETRY_NEEDED      8 /* session closed while this request out */
#define   MID_NO_RESP_NEEDED 0x10

/* Types of response buffer returned from SendReceive2 */
#define   CIFS_NO_BUFFER        0    /* Response buffer not returned */
#define   CIFS_SMALL_BUFFER     1
#define   CIFS_LARGE_BUFFER     2
#define   CIFS_IOVEC            4    /* array of response buffers */

/* Type of Request to SendReceive2 */
#define   CIFS_STD_OP	        0    /* normal request timeout */
#define   CIFS_LONG_OP          1    /* long op (up to 45 sec, oplock time) */
#define   CIFS_VLONG_OP         2    /* sloow op - can take up to 180 seconds */
#define   CIFS_BLOCKING_OP      4    /* operation can block */
#define   CIFS_ASYNC_OP         8    /* do not wait for response */
#define   CIFS_TIMEOUT_MASK 0x00F    /* only one of 5 above set in req */
#define   CIFS_LOG_ERROR    0x010    /* log NT STATUS if non-zero */
#define   CIFS_LARGE_BUF_OP 0x020    /* large request buffer */
#define   CIFS_NO_RESP      0x040    /* no response buffer required */

/* Security Flags: indicate type of session setup needed */
#define   CIFSSEC_MAY_SIGN	0x00001
#define   CIFSSEC_MAY_NTLM	0x00002
#define   CIFSSEC_MAY_NTLMV2	0x00004
#define   CIFSSEC_MAY_KRB5	0x00008
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define   CIFSSEC_MAY_LANMAN	0x00010
#define   CIFSSEC_MAY_PLNTXT	0x00020
#else
#define   CIFSSEC_MAY_LANMAN    0
#define   CIFSSEC_MAY_PLNTXT    0
#endif /* weak passwords */
#define   CIFSSEC_MAY_SEAL	0x00040 /* not supported yet */
#define   CIFSSEC_MAY_NTLMSSP	0x00080 /* raw ntlmssp with ntlmv2 */

#define   CIFSSEC_MUST_SIGN	0x01001
/* note that only one of the following can be set so the
result of setting MUST flags more than once will be to
require use of the stronger protocol */
#define   CIFSSEC_MUST_NTLM	0x02002
#define   CIFSSEC_MUST_NTLMV2	0x04004
#define   CIFSSEC_MUST_KRB5	0x08008
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define   CIFSSEC_MUST_LANMAN	0x10010
#define   CIFSSEC_MUST_PLNTXT	0x20020
#ifdef CONFIG_CIFS_UPCALL
#define   CIFSSEC_MASK          0xBF0BF /* allows weak security but also krb5 */
#else
#define   CIFSSEC_MASK          0xB70B7 /* current flags supported if weak */
#endif /* UPCALL */
#else /* do not allow weak pw hash */
#ifdef CONFIG_CIFS_UPCALL
#define   CIFSSEC_MASK          0x8F08F /* flags supported if no weak allowed */
#else
#define	  CIFSSEC_MASK          0x87087 /* flags supported if no weak allowed */
#endif /* UPCALL */
#endif /* WEAK_PW_HASH */
#define   CIFSSEC_MUST_SEAL	0x40040 /* not supported yet */
#define   CIFSSEC_MUST_NTLMSSP	0x80080 /* raw ntlmssp with ntlmv2 */

#define   CIFSSEC_DEF (CIFSSEC_MAY_SIGN | CIFSSEC_MAY_NTLM | CIFSSEC_MAY_NTLMV2)
#define   CIFSSEC_MAX (CIFSSEC_MUST_SIGN | CIFSSEC_MUST_NTLMV2)
#define   CIFSSEC_AUTH_MASK (CIFSSEC_MAY_NTLM | CIFSSEC_MAY_NTLMV2 | CIFSSEC_MAY_LANMAN | CIFSSEC_MAY_PLNTXT | CIFSSEC_MAY_KRB5 | CIFSSEC_MAY_NTLMSSP)
/*
 *****************************************************************
 * All constants go here
 *****************************************************************
 */

#define UID_HASH (16)

/*
 * Note that ONE module should define _DECLARE_GLOBALS_HERE to cause the
 * following to be declared.
 */

/****************************************************************************
 *  Locking notes.  All updates to global variables and lists should be
 *                  protected by spinlocks or semaphores.
 *
 *  Spinlocks
 *  ---------
 *  GlobalMid_Lock protects:
 *	list operations on pending_mid_q and oplockQ
 *      updates to XID counters, multiplex id  and SMB sequence numbers
 *  GlobalSMBSesLock protects:
 *	list operations on tcp and SMB session lists and tCon lists
 *  f_owner.lock protects certain per file struct operations
 *  mapping->page_lock protects certain per page operations
 *
 *  Semaphores
 *  ----------
 *  sesSem     operations on smb session
 *  tconSem    operations on tree connection
 *  fh_sem      file handle reconnection operations
 *
 ****************************************************************************/

#ifdef DECLARE_GLOBALS_HERE
#define GLOBAL_EXTERN
#else
#define GLOBAL_EXTERN extern
#endif

/*
 * the list of TCP_Server_Info structures, ie each of the sockets
 * connecting our client to a distinct server (ip address), is
 * chained together by cifs_tcp_ses_list. The list of all our SMB
 * sessions (and from that the tree connections) can be found
 * by iterating over cifs_tcp_ses_list
 */
GLOBAL_EXTERN struct list_head		cifs_tcp_ses_list;

/*
 * This lock protects the cifs_tcp_ses_list, the list of smb sessions per
 * tcp session, and the list of tcon's per smb session. It also protects
 * the reference counters for the server, smb session, and tcon. Finally,
 * changes to the tcon->tidStatus should be done while holding this lock.
 */
GLOBAL_EXTERN rwlock_t		cifs_tcp_ses_lock;

/*
 * This lock protects the cifs_file->llist and cifs_file->flist
 * list operations, and updates to some flags (cifs_file->invalidHandle)
 * It will be moved to either use the tcon->stat_lock or equivalent later.
 * If cifs_tcp_ses_lock and the lock below are both needed to be held, then
 * the cifs_tcp_ses_lock must be grabbed first and released last.
 */
GLOBAL_EXTERN rwlock_t GlobalSMBSeslock;

/* Outstanding dir notify requests */
GLOBAL_EXTERN struct list_head GlobalDnotifyReqList;
/* DirNotify response queue */
GLOBAL_EXTERN struct list_head GlobalDnotifyRsp_Q;

/*
 * Global transaction id (XID) information
 */
GLOBAL_EXTERN unsigned int GlobalCurrentXid;	/* protected by GlobalMid_Sem */
GLOBAL_EXTERN unsigned int GlobalTotalActiveXid; /* prot by GlobalMid_Sem */
GLOBAL_EXTERN unsigned int GlobalMaxActiveXid;	/* prot by GlobalMid_Sem */
GLOBAL_EXTERN spinlock_t GlobalMid_Lock;  /* protects above & list operations */
					  /* on midQ entries */
GLOBAL_EXTERN char Local_System_Name[15];

/*
 *  Global counters, updated atomically
 */
GLOBAL_EXTERN atomic_t sesInfoAllocCount;
GLOBAL_EXTERN atomic_t tconInfoAllocCount;
GLOBAL_EXTERN atomic_t tcpSesAllocCount;
GLOBAL_EXTERN atomic_t tcpSesReconnectCount;
GLOBAL_EXTERN atomic_t tconInfoReconnectCount;

/* Various Debug counters */
GLOBAL_EXTERN atomic_t bufAllocCount;    /* current number allocated  */
#ifdef CONFIG_CIFS_STATS2
GLOBAL_EXTERN atomic_t totBufAllocCount; /* total allocated over all time */
GLOBAL_EXTERN atomic_t totSmBufAllocCount;
#endif
GLOBAL_EXTERN atomic_t smBufAllocCount;
GLOBAL_EXTERN atomic_t midCount;

/* Misc globals */
GLOBAL_EXTERN unsigned int multiuser_mount; /* if enabled allows new sessions
				to be established on existing mount if we
				have the uid/password or Kerberos credential
				or equivalent for current user */
GLOBAL_EXTERN unsigned int oplockEnabled;
GLOBAL_EXTERN unsigned int experimEnabled;
GLOBAL_EXTERN unsigned int lookupCacheEnabled;
GLOBAL_EXTERN unsigned int global_secflags;	/* if on, session setup sent
				with more secure ntlmssp2 challenge/resp */
GLOBAL_EXTERN unsigned int sign_CIFS_PDUs;  /* enable smb packet signing */
GLOBAL_EXTERN unsigned int linuxExtEnabled;/*enable Linux/Unix CIFS extensions*/
GLOBAL_EXTERN unsigned int CIFSMaxBufSize;  /* max size not including hdr */
GLOBAL_EXTERN unsigned int cifs_min_rcv;    /* min size of big ntwrk buf pool */
GLOBAL_EXTERN unsigned int cifs_min_small;  /* min size of small buf pool */
GLOBAL_EXTERN unsigned int cifs_max_pending; /* MAX requests at once to server*/

void cifs_oplock_break(struct work_struct *work);
void cifs_oplock_break_get(struct cifsFileInfo *cfile);
void cifs_oplock_break_put(struct cifsFileInfo *cfile);

extern const struct slow_work_ops cifs_oplock_break_ops;

#endif	/* _CIFS_GLOB_H */
