/*
 *  Copyright (c) 2001 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Kendrick Smith <kmsmith@umich.edu>
 *  Andy Adamson <andros@umich.edu>
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NFSD4_STATE_H
#define _NFSD4_STATE_H

#include <linux/nfsd/nfsfh.h>
#include "nfsfh.h"

typedef struct {
	u32             cl_boot;
	u32             cl_id;
} clientid_t;

typedef struct {
	u32             so_boot;
	u32             so_stateownerid;
	u32             so_fileid;
} stateid_opaque_t;

typedef struct {
	u32                     si_generation;
	stateid_opaque_t        si_opaque;
} stateid_t;
#define si_boot           si_opaque.so_boot
#define si_stateownerid   si_opaque.so_stateownerid
#define si_fileid         si_opaque.so_fileid

#define STATEID_FMT	"(%08x/%08x/%08x/%08x)"
#define STATEID_VAL(s) \
	(s)->si_boot, \
	(s)->si_stateownerid, \
	(s)->si_fileid, \
	(s)->si_generation

struct nfsd4_cb_sequence {
	/* args/res */
	u32			cbs_minorversion;
	struct nfs4_client	*cbs_clp;
};

struct nfs4_rpc_args {
	void				*args_op;
	struct nfsd4_cb_sequence	args_seq;
};

struct nfsd4_callback {
	struct nfs4_rpc_args cb_args;
	struct work_struct cb_work;
};

struct nfs4_delegation {
	struct list_head	dl_perfile;
	struct list_head	dl_perclnt;
	struct list_head	dl_recall_lru;  /* delegation recalled */
	atomic_t		dl_count;       /* ref count */
	struct nfs4_client	*dl_client;
	struct nfs4_file	*dl_file;
	struct file_lock	*dl_flock;
	u32			dl_type;
	time_t			dl_time;
/* For recall: */
	u32			dl_ident;
	stateid_t		dl_stateid;
	struct knfsd_fh		dl_fh;
	int			dl_retries;
	struct nfsd4_callback	dl_recall;
};

/* client delegation callback info */
struct nfs4_cb_conn {
	/* SETCLIENTID info */
	struct sockaddr_storage	cb_addr;
	size_t			cb_addrlen;
	u32                     cb_prog;
	u32			cb_minorversion;
	u32                     cb_ident;	/* minorversion 0 only */
	struct svc_xprt		*cb_xprt;	/* minorversion 1 only */
};

/* Maximum number of slots per session. 160 is useful for long haul TCP */
#define NFSD_MAX_SLOTS_PER_SESSION     160
/* Maximum number of operations per session compound */
#define NFSD_MAX_OPS_PER_COMPOUND	16
/* Maximum  session per slot cache size */
#define NFSD_SLOT_CACHE_SIZE		1024
/* Maximum number of NFSD_SLOT_CACHE_SIZE slots per session */
#define NFSD_CACHE_SIZE_SLOTS_PER_SESSION	32
#define NFSD_MAX_MEM_PER_SESSION  \
		(NFSD_CACHE_SIZE_SLOTS_PER_SESSION * NFSD_SLOT_CACHE_SIZE)

struct nfsd4_slot {
	bool	sl_inuse;
	bool	sl_cachethis;
	u16	sl_opcnt;
	u32	sl_seqid;
	__be32	sl_status;
	u32	sl_datalen;
	char	sl_data[];
};

struct nfsd4_channel_attrs {
	u32		headerpadsz;
	u32		maxreq_sz;
	u32		maxresp_sz;
	u32		maxresp_cached;
	u32		maxops;
	u32		maxreqs;
	u32		nr_rdma_attrs;
	u32		rdma_attrs;
};

struct nfsd4_create_session {
	clientid_t			clientid;
	struct nfs4_sessionid		sessionid;
	u32				seqid;
	u32				flags;
	struct nfsd4_channel_attrs	fore_channel;
	struct nfsd4_channel_attrs	back_channel;
	u32				callback_prog;
	u32				uid;
	u32				gid;
};

/* The single slot clientid cache structure */
struct nfsd4_clid_slot {
	u32				sl_seqid;
	__be32				sl_status;
	struct nfsd4_create_session	sl_cr_ses;
};

struct nfsd4_session {
	struct kref		se_ref;
	struct list_head	se_hash;	/* hash by sessionid */
	struct list_head	se_perclnt;
	u32			se_flags;
	struct nfs4_client	*se_client;
	struct nfs4_sessionid	se_sessionid;
	struct nfsd4_channel_attrs se_fchannel;
	struct nfsd4_channel_attrs se_bchannel;
	struct nfsd4_slot	*se_slots[];	/* forward channel slots */
};

static inline void
nfsd4_put_session(struct nfsd4_session *ses)
{
	extern void free_session(struct kref *kref);
	kref_put(&ses->se_ref, free_session);
}

static inline void
nfsd4_get_session(struct nfsd4_session *ses)
{
	kref_get(&ses->se_ref);
}

/* formatted contents of nfs4_sessionid */
struct nfsd4_sessionid {
	clientid_t	clientid;
	u32		sequence;
	u32		reserved;
};

#define HEXDIR_LEN     33 /* hex version of 16 byte md5 of cl_name plus '\0' */

/*
 * struct nfs4_client - one per client.  Clientids live here.
 * 	o Each nfs4_client is hashed by clientid.
 *
 * 	o Each nfs4_clients is also hashed by name 
 * 	  (the opaque quantity initially sent by the client to identify itself).
 * 	  
 *	o cl_perclient list is used to ensure no dangling stateowner references
 *	  when we expire the nfs4_client
 */
struct nfs4_client {
	struct list_head	cl_idhash; 	/* hash by cl_clientid.id */
	struct list_head	cl_strhash; 	/* hash by cl_name */
	struct list_head	cl_openowners;
	struct list_head	cl_delegations;
	struct list_head        cl_lru;         /* tail queue */
	struct xdr_netobj	cl_name; 	/* id generated by client */
	char                    cl_recdir[HEXDIR_LEN]; /* recovery dir */
	nfs4_verifier		cl_verifier; 	/* generated by client */
	time_t                  cl_time;        /* time of last lease renewal */
	struct sockaddr_storage	cl_addr; 	/* client ipaddress */
	u32			cl_flavor;	/* setclientid pseudoflavor */
	char			*cl_principal;	/* setclientid principal name */
	struct svc_cred		cl_cred; 	/* setclientid principal */
	clientid_t		cl_clientid;	/* generated by server */
	nfs4_verifier		cl_confirm;	/* generated by server */
	u32			cl_firststate;	/* recovery dir creation */

	/* for v4.0 and v4.1 callbacks: */
	struct nfs4_cb_conn	cl_cb_conn;
	struct rpc_clnt		*cl_cb_client;
	atomic_t		cl_cb_set;

	/* for nfs41 */
	struct list_head	cl_sessions;
	struct nfsd4_clid_slot	cl_cs_slot;	/* create_session slot */
	u32			cl_exchange_flags;
	struct nfs4_sessionid	cl_sessionid;
	/* number of rpc's in progress over an associated session: */
	atomic_t		cl_refcount;

	/* for nfs41 callbacks */
	/* We currently support a single back channel with a single slot */
	unsigned long		cl_cb_slot_busy;
	u32			cl_cb_seq_nr;
	struct rpc_wait_queue	cl_cb_waitq;	/* backchannel callers may */
						/* wait here for slots */
};

static inline void
mark_client_expired(struct nfs4_client *clp)
{
	clp->cl_time = 0;
}

static inline bool
is_client_expired(struct nfs4_client *clp)
{
	return clp->cl_time == 0;
}

/* struct nfs4_client_reset
 * one per old client. Populates reset_str_hashtbl. Filled from conf_id_hashtbl
 * upon lease reset, or from upcall to state_daemon (to read in state
 * from non-volitile storage) upon reboot.
 */
struct nfs4_client_reclaim {
	struct list_head	cr_strhash;	/* hash by cr_name */
	char			cr_recdir[HEXDIR_LEN]; /* recover dir */
};

static inline void
update_stateid(stateid_t *stateid)
{
	stateid->si_generation++;
}

/* A reasonable value for REPLAY_ISIZE was estimated as follows:  
 * The OPEN response, typically the largest, requires 
 *   4(status) + 8(stateid) + 20(changeinfo) + 4(rflags) +  8(verifier) + 
 *   4(deleg. type) + 8(deleg. stateid) + 4(deleg. recall flag) + 
 *   20(deleg. space limit) + ~32(deleg. ace) = 112 bytes 
 */

#define NFSD4_REPLAY_ISIZE       112 

/*
 * Replay buffer, where the result of the last seqid-mutating operation 
 * is cached. 
 */
struct nfs4_replay {
	__be32			rp_status;
	unsigned int		rp_buflen;
	char			*rp_buf;
	unsigned		intrp_allocated;
	struct knfsd_fh		rp_openfh;
	char			rp_ibuf[NFSD4_REPLAY_ISIZE];
};

/*
* nfs4_stateowner can either be an open_owner, or a lock_owner
*
*    so_idhash:  stateid_hashtbl[] for open owner, lockstateid_hashtbl[]
*         for lock_owner
*    so_strhash: ownerstr_hashtbl[] for open_owner, lock_ownerstr_hashtbl[]
*         for lock_owner
*    so_perclient: nfs4_client->cl_perclient entry - used when nfs4_client
*         struct is reaped.
*    so_perfilestate: heads the list of nfs4_stateid (either open or lock) 
*         and is used to ensure no dangling nfs4_stateid references when we 
*         release a stateowner.
*    so_perlockowner: (open) nfs4_stateid->st_perlockowner entry - used when
*         close is called to reap associated byte-range locks
*    so_close_lru: (open) stateowner is placed on this list instead of being
*         reaped (when so_perfilestate is empty) to hold the last close replay.
*         reaped by laundramat thread after lease period.
*/
struct nfs4_stateowner {
	struct kref		so_ref;
	struct list_head        so_idhash;   /* hash by so_id */
	struct list_head        so_strhash;   /* hash by op_name */
	struct list_head        so_perclient;
	struct list_head        so_stateids;
	struct list_head        so_perstateid; /* for lockowners only */
	struct list_head	so_close_lru; /* tail queue */
	time_t			so_time; /* time of placement on so_close_lru */
	int			so_is_open_owner; /* 1=openowner,0=lockowner */
	u32                     so_id;
	struct nfs4_client *    so_client;
	/* after increment in ENCODE_SEQID_OP_TAIL, represents the next
	 * sequence id expected from the client: */
	u32                     so_seqid;
	struct xdr_netobj       so_owner;     /* open owner name */
	int                     so_confirmed; /* successful OPEN_CONFIRM? */
	struct nfs4_replay	so_replay;
};

/*
*  nfs4_file: a file opened by some number of (open) nfs4_stateowners.
*    o fi_perfile list is used to search for conflicting 
*      share_acces, share_deny on the file.
*/
struct nfs4_file {
	atomic_t		fi_ref;
	struct list_head        fi_hash;    /* hash by "struct inode *" */
	struct list_head        fi_stateids;
	struct list_head	fi_delegations;
	/* One each for O_RDONLY, O_WRONLY, O_RDWR: */
	struct file *		fi_fds[3];
	/* One each for O_RDONLY, O_WRONLY: */
	atomic_t		fi_access[2];
	/*
	 * Each open stateid contributes 1 to either fi_readers or
	 * fi_writers, or both, depending on the open mode.  A
	 * delegation also takes an fi_readers reference.  Lock
	 * stateid's take none.
	 */
	atomic_t		fi_readers;
	atomic_t		fi_writers;
	struct inode		*fi_inode;
	u32                     fi_id;      /* used with stateowner->so_id 
					     * for stateid_hashtbl hash */
	bool			fi_had_conflict;
};

static inline struct file *find_writeable_file(struct nfs4_file *f)
{
	if (f->fi_fds[O_WRONLY])
		return f->fi_fds[O_WRONLY];
	return f->fi_fds[O_RDWR];
}

static inline struct file *find_readable_file(struct nfs4_file *f)
{
	if (f->fi_fds[O_RDONLY])
		return f->fi_fds[O_RDONLY];
	return f->fi_fds[O_RDWR];
}

static inline struct file *find_any_file(struct nfs4_file *f)
{
	if (f->fi_fds[O_RDWR])
		return f->fi_fds[O_RDWR];
	else if (f->fi_fds[O_WRONLY])
		return f->fi_fds[O_WRONLY];
	else
		return f->fi_fds[O_RDONLY];
}


struct nfs4_stateid {
	struct list_head              st_hash; 
	struct list_head              st_perfile;
	struct list_head              st_perstateowner;
	struct list_head              st_lockowners;
	struct nfs4_stateowner      * st_stateowner;
	struct nfs4_file            * st_file;
	stateid_t                     st_stateid;
	unsigned long                 st_access_bmap;
	unsigned long                 st_deny_bmap;
	struct nfs4_stateid         * st_openstp;
};

/* flags for preprocess_seqid_op() */
#define HAS_SESSION             0x00000001
#define CONFIRM                 0x00000002
#define OPEN_STATE              0x00000004
#define LOCK_STATE              0x00000008
#define RD_STATE	        0x00000010
#define WR_STATE	        0x00000020
#define CLOSE_STATE             0x00000040

#define seqid_mutating_err(err)                       \
	(((err) != nfserr_stale_clientid) &&    \
	((err) != nfserr_bad_seqid) &&          \
	((err) != nfserr_stale_stateid) &&      \
	((err) != nfserr_bad_stateid))

struct nfsd4_compound_state;

extern __be32 nfs4_preprocess_stateid_op(struct nfsd4_compound_state *cstate,
		stateid_t *stateid, int flags, struct file **filp);
extern void nfs4_lock_state(void);
extern void nfs4_unlock_state(void);
extern int nfs4_in_grace(void);
extern __be32 nfs4_check_open_reclaim(clientid_t *clid);
extern void nfs4_free_stateowner(struct kref *kref);
extern int set_callback_cred(void);
extern void nfsd4_probe_callback(struct nfs4_client *clp, struct nfs4_cb_conn *);
extern void nfsd4_do_callback_rpc(struct work_struct *);
extern void nfsd4_cb_recall(struct nfs4_delegation *dp);
extern int nfsd4_create_callback_queue(void);
extern void nfsd4_destroy_callback_queue(void);
extern void nfsd4_set_callback_client(struct nfs4_client *, struct rpc_clnt *);
extern void nfs4_put_delegation(struct nfs4_delegation *dp);
extern __be32 nfs4_make_rec_clidname(char *clidname, struct xdr_netobj *clname);
extern void nfsd4_init_recdir(char *recdir_name);
extern int nfsd4_recdir_load(void);
extern void nfsd4_shutdown_recdir(void);
extern int nfs4_client_to_reclaim(const char *name);
extern int nfs4_has_reclaimed_state(const char *name, bool use_exchange_id);
extern void nfsd4_recdir_purge_old(void);
extern int nfsd4_create_clid_dir(struct nfs4_client *clp);
extern void nfsd4_remove_clid_dir(struct nfs4_client *clp);
extern void release_session_client(struct nfsd4_session *);

static inline void
nfs4_put_stateowner(struct nfs4_stateowner *so)
{
	kref_put(&so->so_ref, nfs4_free_stateowner);
}

static inline void
nfs4_get_stateowner(struct nfs4_stateowner *so)
{
	kref_get(&so->so_ref);
}

#endif   /* NFSD4_STATE_H */
