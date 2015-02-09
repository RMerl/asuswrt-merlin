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
#define NFS_DEF_UDP_TIMEO	(11)
#define NFS_DEF_UDP_RETRANS	(3)
#define NFS_DEF_TCP_TIMEO	(600)
#define NFS_DEF_TCP_RETRANS	(2)

#define NFS_MAX_UDP_TIMEOUT	(60*HZ)
#define NFS_MAX_TCP_TIMEOUT	(600*HZ)

#define NFS_DEF_ACREGMIN	(3)
#define NFS_DEF_ACREGMAX	(60)
#define NFS_DEF_ACDIRMIN	(30)
#define NFS_DEF_ACDIRMAX	(60)

/*
 * When flushing a cluster of dirty pages, there can be different
 * strategies:
 */
#define FLUSH_SYNC		1	/* file being synced, or contention */
#define FLUSH_STABLE		4	/* commit to stable storage */
#define FLUSH_LOWPRI		8	/* low priority background flush */
#define FLUSH_HIGHPRI		16	/* high priority memory reclaim flush */

#ifdef __KERNEL__

#include <linux/in.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/wait.h>

#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/clnt.h>

#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs3.h>
#include <linux/nfs4.h>
#include <linux/nfs_xdr.h>
#include <linux/nfs_fs_sb.h>

#include <linux/mempool.h>

/*
 * These are the default flags for swap requests
 */
#define NFS_RPC_SWAPFLAGS		(RPC_TASK_SWAPPER|RPC_TASK_ROOTCREDS)

/*
 * NFSv3/v4 Access mode cache entry
 */
struct nfs_access_entry {
	struct rb_node		rb_node;
	struct list_head	lru;
	unsigned long		jiffies;
	struct rpc_cred *	cred;
	int			mask;
};

struct nfs_lock_context {
	atomic_t count;
	struct list_head list;
	struct nfs_open_context *open_context;
	fl_owner_t lockowner;
	pid_t pid;
};

struct nfs4_state;
struct nfs_open_context {
	struct nfs_lock_context lock_context;
	struct path path;
	struct rpc_cred *cred;
	struct nfs4_state *state;
	fmode_t mode;

	unsigned long flags;
#define NFS_CONTEXT_ERROR_WRITE		(0)
	int error;

	struct list_head list;

	__u64 dir_cookie;
};

/*
 * NFSv4 delegation
 */
struct nfs_delegation;

struct posix_acl;

/*
 * nfs fs inode data in memory
 */
struct nfs_inode {
	/*
	 * The 64bit 'inode number'
	 */
	__u64 fileid;

	/*
	 * NFS file handle
	 */
	struct nfs_fh		fh;

	/*
	 * Various flags
	 */
	unsigned long		flags;			/* atomic bit ops */
	unsigned long		cache_validity;		/* bit mask */

	/*
	 * read_cache_jiffies is when we started read-caching this inode.
	 * attrtimeo is for how long the cached information is assumed
	 * to be valid. A successful attribute revalidation doubles
	 * attrtimeo (up to acregmax/acdirmax), a failure resets it to
	 * acregmin/acdirmin.
	 *
	 * We need to revalidate the cached attrs for this inode if
	 *
	 *	jiffies - read_cache_jiffies >= attrtimeo
	 *
	 * Please note the comparison is greater than or equal
	 * so that zero timeout values can be specified.
	 */
	unsigned long		read_cache_jiffies;
	unsigned long		attrtimeo;
	unsigned long		attrtimeo_timestamp;
	__u64			change_attr;		/* v4 only */

	unsigned long		attr_gencount;
	/* "Generation counter" for the attribute cache. This is
	 * bumped whenever we update the metadata on the
	 * server.
	 */
	unsigned long		cache_change_attribute;

	struct rb_root		access_cache;
	struct list_head	access_cache_entry_lru;
	struct list_head	access_cache_inode_lru;
#ifdef CONFIG_NFS_V3_ACL
	struct posix_acl	*acl_access;
	struct posix_acl	*acl_default;
#endif

	/*
	 * This is the cookie verifier used for NFSv3 readdir
	 * operations
	 */
	__be32			cookieverf[2];

	/*
	 * This is the list of dirty unwritten pages.
	 */
	struct radix_tree_root	nfs_page_tree;

	unsigned long		npages;
	unsigned long		ncommit;

	/* Open contexts for shared mmap writes */
	struct list_head	open_files;

	/* Number of in-flight sillydelete RPC calls */
	atomic_t		silly_count;
	/* List of deferred sillydelete requests */
	struct hlist_head	silly_list;
	wait_queue_head_t	waitqueue;

#ifdef CONFIG_NFS_V4
	struct nfs4_cached_acl	*nfs4_acl;
        /* NFSv4 state */
	struct list_head	open_states;
	struct nfs_delegation	*delegation;
	fmode_t			 delegation_state;
	struct rw_semaphore	rwsem;
#endif /* CONFIG_NFS_V4*/
#ifdef CONFIG_NFS_FSCACHE
	struct fscache_cookie	*fscache;
#endif
	struct inode		vfs_inode;
};

/*
 * Cache validity bit flags
 */
#define NFS_INO_INVALID_ATTR	0x0001		/* cached attrs are invalid */
#define NFS_INO_INVALID_DATA	0x0002		/* cached data is invalid */
#define NFS_INO_INVALID_ATIME	0x0004		/* cached atime is invalid */
#define NFS_INO_INVALID_ACCESS	0x0008		/* cached access cred invalid */
#define NFS_INO_INVALID_ACL	0x0010		/* cached acls are invalid */
#define NFS_INO_REVAL_PAGECACHE	0x0020		/* must revalidate pagecache */
#define NFS_INO_REVAL_FORCED	0x0040		/* force revalidation ignoring a delegation */

/*
 * Bit offsets in flags field
 */
#define NFS_INO_ADVISE_RDPLUS	(0)		/* advise readdirplus */
#define NFS_INO_STALE		(1)		/* possible stale inode */
#define NFS_INO_ACL_LRU_SET	(2)		/* Inode is on the LRU list */
#define NFS_INO_MOUNTPOINT	(3)		/* inode is remote mountpoint */
#define NFS_INO_FLUSHING	(4)		/* inode is flushing out data */
#define NFS_INO_FSCACHE		(5)		/* inode can be cached by FS-Cache */
#define NFS_INO_FSCACHE_LOCK	(6)		/* FS-Cache cookie management lock */
#define NFS_INO_COMMIT		(7)		/* inode is committing unstable writes */

static inline struct nfs_inode *NFS_I(const struct inode *inode)
{
	return container_of(inode, struct nfs_inode, vfs_inode);
}

static inline struct nfs_server *NFS_SB(const struct super_block *s)
{
	return (struct nfs_server *)(s->s_fs_info);
}

static inline struct nfs_fh *NFS_FH(const struct inode *inode)
{
	return &NFS_I(inode)->fh;
}

static inline struct nfs_server *NFS_SERVER(const struct inode *inode)
{
	return NFS_SB(inode->i_sb);
}

static inline struct rpc_clnt *NFS_CLIENT(const struct inode *inode)
{
	return NFS_SERVER(inode)->client;
}

static inline const struct nfs_rpc_ops *NFS_PROTO(const struct inode *inode)
{
	return NFS_SERVER(inode)->nfs_client->rpc_ops;
}

static inline __be32 *NFS_COOKIEVERF(const struct inode *inode)
{
	return NFS_I(inode)->cookieverf;
}

static inline unsigned NFS_MINATTRTIMEO(const struct inode *inode)
{
	struct nfs_server *nfss = NFS_SERVER(inode);
	return S_ISDIR(inode->i_mode) ? nfss->acdirmin : nfss->acregmin;
}

static inline unsigned NFS_MAXATTRTIMEO(const struct inode *inode)
{
	struct nfs_server *nfss = NFS_SERVER(inode);
	return S_ISDIR(inode->i_mode) ? nfss->acdirmax : nfss->acregmax;
}

static inline int NFS_STALE(const struct inode *inode)
{
	return test_bit(NFS_INO_STALE, &NFS_I(inode)->flags);
}

static inline int NFS_FSCACHE(const struct inode *inode)
{
	return test_bit(NFS_INO_FSCACHE, &NFS_I(inode)->flags);
}

static inline __u64 NFS_FILEID(const struct inode *inode)
{
	return NFS_I(inode)->fileid;
}

static inline void set_nfs_fileid(struct inode *inode, __u64 fileid)
{
	NFS_I(inode)->fileid = fileid;
}

static inline void nfs_mark_for_revalidate(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	spin_lock(&inode->i_lock);
	nfsi->cache_validity |= NFS_INO_INVALID_ATTR|NFS_INO_INVALID_ACCESS;
	if (S_ISDIR(inode->i_mode))
		nfsi->cache_validity |= NFS_INO_REVAL_PAGECACHE|NFS_INO_INVALID_DATA;
	spin_unlock(&inode->i_lock);
}

static inline int nfs_server_capable(struct inode *inode, int cap)
{
	return NFS_SERVER(inode)->caps & cap;
}

static inline int NFS_USE_READDIRPLUS(struct inode *inode)
{
	return test_bit(NFS_INO_ADVISE_RDPLUS, &NFS_I(inode)->flags);
}

static inline void nfs_set_verifier(struct dentry * dentry, unsigned long verf)
{
	dentry->d_time = verf;
}

/**
 * nfs_save_change_attribute - Returns the inode attribute change cookie
 * @dir - pointer to parent directory inode
 * The "change attribute" is updated every time we finish an operation
 * that will result in a metadata change on the server.
 */
static inline unsigned long nfs_save_change_attribute(struct inode *dir)
{
	return NFS_I(dir)->cache_change_attribute;
}

/**
 * nfs_verify_change_attribute - Detects NFS remote directory changes
 * @dir - pointer to parent directory inode
 * @chattr - previously saved change attribute
 * Return "false" if the verifiers doesn't match the change attribute.
 * This would usually indicate that the directory contents have changed on
 * the server, and that any dentries need revalidating.
 */
static inline int nfs_verify_change_attribute(struct inode *dir, unsigned long chattr)
{
	return chattr == NFS_I(dir)->cache_change_attribute;
}

/*
 * linux/fs/nfs/inode.c
 */
extern int nfs_sync_mapping(struct address_space *mapping);
extern void nfs_zap_mapping(struct inode *inode, struct address_space *mapping);
extern void nfs_zap_caches(struct inode *);
extern void nfs_invalidate_atime(struct inode *);
extern struct inode *nfs_fhget(struct super_block *, struct nfs_fh *,
				struct nfs_fattr *);
extern int nfs_refresh_inode(struct inode *, struct nfs_fattr *);
extern int nfs_post_op_update_inode(struct inode *inode, struct nfs_fattr *fattr);
extern int nfs_post_op_update_inode_force_wcc(struct inode *inode, struct nfs_fattr *fattr);
extern int nfs_getattr(struct vfsmount *, struct dentry *, struct kstat *);
extern int nfs_permission(struct inode *, int);
extern int nfs_open(struct inode *, struct file *);
extern int nfs_release(struct inode *, struct file *);
extern int nfs_attribute_timeout(struct inode *inode);
extern int nfs_revalidate_inode(struct nfs_server *server, struct inode *inode);
extern int __nfs_revalidate_inode(struct nfs_server *, struct inode *);
extern int nfs_revalidate_mapping(struct inode *inode, struct address_space *mapping);
extern int nfs_setattr(struct dentry *, struct iattr *);
extern void nfs_setattr_update_inode(struct inode *inode, struct iattr *attr);
extern struct nfs_open_context *get_nfs_open_context(struct nfs_open_context *ctx);
extern void put_nfs_open_context(struct nfs_open_context *ctx);
extern struct nfs_open_context *nfs_find_open_context(struct inode *inode, struct rpc_cred *cred, fmode_t mode);
extern struct nfs_lock_context *nfs_get_lock_context(struct nfs_open_context *ctx);
extern void nfs_put_lock_context(struct nfs_lock_context *l_ctx);
extern u64 nfs_compat_user_ino64(u64 fileid);
extern void nfs_fattr_init(struct nfs_fattr *fattr);

extern struct nfs_fattr *nfs_alloc_fattr(void);

static inline void nfs_free_fattr(const struct nfs_fattr *fattr)
{
	kfree(fattr);
}

extern struct nfs_fh *nfs_alloc_fhandle(void);

static inline void nfs_free_fhandle(const struct nfs_fh *fh)
{
	kfree(fh);
}

/* linux/net/ipv4/ipconfig.c: trims ip addr off front of name, too. */
extern __be32 root_nfs_parse_addr(char *name); /*__init*/
extern unsigned long nfs_inc_attr_generation_counter(void);

/*
 * linux/fs/nfs/file.c
 */
extern const struct inode_operations nfs_file_inode_operations;
#ifdef CONFIG_NFS_V3
extern const struct inode_operations nfs3_file_inode_operations;
#endif /* CONFIG_NFS_V3 */
extern const struct file_operations nfs_file_operations;
extern const struct address_space_operations nfs_file_aops;

static inline struct nfs_open_context *nfs_file_open_context(struct file *filp)
{
	return filp->private_data;
}

static inline struct rpc_cred *nfs_file_cred(struct file *file)
{
	if (file != NULL) {
		struct nfs_open_context *ctx =
			nfs_file_open_context(file);
		if (ctx)
			return ctx->cred;
	}
	return NULL;
}

/*
 * linux/fs/nfs/xattr.c
 */
#ifdef CONFIG_NFS_V3_ACL
extern ssize_t nfs3_listxattr(struct dentry *, char *, size_t);
extern ssize_t nfs3_getxattr(struct dentry *, const char *, void *, size_t);
extern int nfs3_setxattr(struct dentry *, const char *,
			const void *, size_t, int);
extern int nfs3_removexattr (struct dentry *, const char *name);
#else
# define nfs3_listxattr NULL
# define nfs3_getxattr NULL
# define nfs3_setxattr NULL
# define nfs3_removexattr NULL
#endif

/*
 * linux/fs/nfs/direct.c
 */
extern ssize_t nfs_direct_IO(int, struct kiocb *, const struct iovec *, loff_t,
			unsigned long);
extern ssize_t nfs_file_direct_read(struct kiocb *iocb,
			const struct iovec *iov, unsigned long nr_segs,
			loff_t pos);
extern ssize_t nfs_file_direct_write(struct kiocb *iocb,
			const struct iovec *iov, unsigned long nr_segs,
			loff_t pos);

/*
 * linux/fs/nfs/dir.c
 */
extern const struct inode_operations nfs_dir_inode_operations;
#ifdef CONFIG_NFS_V3
extern const struct inode_operations nfs3_dir_inode_operations;
#endif /* CONFIG_NFS_V3 */
extern const struct file_operations nfs_dir_operations;
extern const struct dentry_operations nfs_dentry_operations;

extern void nfs_force_lookup_revalidate(struct inode *dir);
extern int nfs_instantiate(struct dentry *dentry, struct nfs_fh *fh, struct nfs_fattr *fattr);
extern int nfs_may_open(struct inode *inode, struct rpc_cred *cred, int openflags);
extern void nfs_access_zap_cache(struct inode *inode);

/*
 * linux/fs/nfs/symlink.c
 */
extern const struct inode_operations nfs_symlink_inode_operations;

/*
 * linux/fs/nfs/sysctl.c
 */
#ifdef CONFIG_SYSCTL
extern int nfs_register_sysctl(void);
extern void nfs_unregister_sysctl(void);
#else
#define nfs_register_sysctl() 0
#define nfs_unregister_sysctl() do { } while(0)
#endif

/*
 * linux/fs/nfs/namespace.c
 */
extern const struct inode_operations nfs_mountpoint_inode_operations;
extern const struct inode_operations nfs_referral_inode_operations;
extern int nfs_mountpoint_expiry_timeout;
extern void nfs_release_automount_timer(void);

/*
 * linux/fs/nfs/unlink.c
 */
extern int  nfs_async_unlink(struct inode *dir, struct dentry *dentry);
extern void nfs_complete_unlink(struct dentry *dentry, struct inode *);
extern void nfs_block_sillyrename(struct dentry *dentry);
extern void nfs_unblock_sillyrename(struct dentry *dentry);

/*
 * linux/fs/nfs/write.c
 */
extern int  nfs_congestion_kb;
extern int  nfs_writepage(struct page *page, struct writeback_control *wbc);
extern int  nfs_writepages(struct address_space *, struct writeback_control *);
extern int  nfs_flush_incompatible(struct file *file, struct page *page);
extern int  nfs_updatepage(struct file *, struct page *, unsigned int, unsigned int);
extern int nfs_writeback_done(struct rpc_task *, struct nfs_write_data *);

/*
 * Try to write back everything synchronously (but check the
 * return value!)
 */
extern int nfs_wb_all(struct inode *inode);
extern int nfs_wb_page(struct inode *inode, struct page* page);
extern int nfs_wb_page_cancel(struct inode *inode, struct page* page);
#if defined(CONFIG_NFS_V3) || defined(CONFIG_NFS_V4)
extern int  nfs_commit_inode(struct inode *, int);
extern struct nfs_write_data *nfs_commitdata_alloc(void);
extern void nfs_commit_free(struct nfs_write_data *wdata);
#else
static inline int
nfs_commit_inode(struct inode *inode, int how)
{
	return 0;
}
#endif

static inline int
nfs_have_writebacks(struct inode *inode)
{
	return NFS_I(inode)->npages != 0;
}

/*
 * Allocate nfs_write_data structures
 */
extern struct nfs_write_data *nfs_writedata_alloc(unsigned int npages);
extern void nfs_writedata_free(struct nfs_write_data *);

/*
 * linux/fs/nfs/read.c
 */
extern int  nfs_readpage(struct file *, struct page *);
extern int  nfs_readpages(struct file *, struct address_space *,
		struct list_head *, unsigned);
extern int  nfs_readpage_result(struct rpc_task *, struct nfs_read_data *);
extern int  nfs_readpage_async(struct nfs_open_context *, struct inode *,
			       struct page *);

/*
 * Allocate nfs_read_data structures
 */
extern struct nfs_read_data *nfs_readdata_alloc(unsigned int npages);
extern void nfs_readdata_free(struct nfs_read_data *);

/*
 * linux/fs/nfs3proc.c
 */
#ifdef CONFIG_NFS_V3_ACL
extern struct posix_acl *nfs3_proc_getacl(struct inode *inode, int type);
extern int nfs3_proc_setacl(struct inode *inode, int type,
			    struct posix_acl *acl);
extern int nfs3_proc_set_default_acl(struct inode *dir, struct inode *inode,
		mode_t mode);
extern void nfs3_forget_cached_acls(struct inode *inode);
#else
static inline int nfs3_proc_set_default_acl(struct inode *dir,
					    struct inode *inode,
					    mode_t mode)
{
	return 0;
}

static inline void nfs3_forget_cached_acls(struct inode *inode)
{
}
#endif /* CONFIG_NFS_V3_ACL */

/*
 * inline functions
 */

static inline loff_t nfs_size_to_loff_t(__u64 size)
{
	if (size > (__u64) OFFSET_MAX - 1)
		return OFFSET_MAX - 1;
	return (loff_t) size;
}

static inline ino_t
nfs_fileid_to_ino_t(u64 fileid)
{
	ino_t ino = (ino_t) fileid;
	if (sizeof(ino_t) < sizeof(u64))
		ino ^= fileid >> (sizeof(u64)-sizeof(ino_t)) * 8;
	return ino;
}

/* NFS root */

extern void * nfs_root_data(void);

#define nfs_wait_event(clnt, wq, condition)				\
({									\
	int __retval = wait_event_killable(wq, condition);		\
	__retval;							\
})

#define NFS_JUKEBOX_RETRY_TIME (5 * HZ)

#endif /* __KERNEL__ */

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
#define NFSDBG_MOUNT		0x0400
#define NFSDBG_FSCACHE		0x0800
#define NFSDBG_ALL		0xFFFF

#ifdef __KERNEL__

/*
 * Enable debugging support for nfs client.
 * Requires RPC_DEBUG.
 */
#ifdef RPC_DEBUG
# define NFS_DEBUG
#endif

# undef ifdebug
# ifdef NFS_DEBUG
#  define ifdebug(fac)		if (unlikely(nfs_debug & NFSDBG_##fac))
# else
#  define ifdebug(fac)		if (0)
# endif
#endif /* __KERNEL */

#endif
