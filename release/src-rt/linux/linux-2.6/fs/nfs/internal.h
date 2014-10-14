/*
 * NFS internal definitions
 */

#include "nfs4_fs.h"
#include <linux/mount.h>
#include <linux/security.h>

#define NFS_MS_MASK (MS_RDONLY|MS_NOSUID|MS_NODEV|MS_NOEXEC|MS_SYNCHRONOUS)

struct nfs_string;

/* Maximum number of readahead requests
 * FIXME: this should really be a sysctl so that users may tune it to suit
 *        their needs. People that do NFS over a slow network, might for
 *        instance want to reduce it to something closer to 1 for improved
 *        interactive response.
 */
#define NFS_MAX_READAHEAD	(RPC_DEF_SLOT_TABLE - 1)

/*
 * Determine if sessions are in use.
 */
static inline int nfs4_has_session(const struct nfs_client *clp)
{
#ifdef CONFIG_NFS_V4_1
	if (clp->cl_session)
		return 1;
#endif /* CONFIG_NFS_V4_1 */
	return 0;
}

static inline int nfs4_has_persistent_session(const struct nfs_client *clp)
{
#ifdef CONFIG_NFS_V4_1
	if (nfs4_has_session(clp))
		return (clp->cl_session->flags & SESSION4_PERSIST);
#endif /* CONFIG_NFS_V4_1 */
	return 0;
}

static inline void nfs_attr_check_mountpoint(struct super_block *parent, struct nfs_fattr *fattr)
{
	if (!nfs_fsid_equal(&NFS_SB(parent)->fsid, &fattr->fsid))
		fattr->valid |= NFS_ATTR_FATTR_MOUNTPOINT;
}

static inline int nfs_attr_use_mounted_on_fileid(struct nfs_fattr *fattr)
{
	if (((fattr->valid & NFS_ATTR_FATTR_MOUNTED_ON_FILEID) == 0) ||
	    (((fattr->valid & NFS_ATTR_FATTR_MOUNTPOINT) == 0) &&
	     ((fattr->valid & NFS_ATTR_FATTR_V4_REFERRAL) == 0)))
		return 0;

	fattr->fileid = fattr->mounted_on_fileid;
	return 1;
}

struct nfs_clone_mount {
	const struct super_block *sb;
	const struct dentry *dentry;
	struct nfs_fh *fh;
	struct nfs_fattr *fattr;
	char *hostname;
	char *mnt_path;
	struct sockaddr *addr;
	size_t addrlen;
	rpc_authflavor_t authflavor;
};

/*
 * Note: RFC 1813 doesn't limit the number of auth flavors that
 * a server can return, so make something up.
 */
#define NFS_MAX_SECFLAVORS	(12)

/*
 * Value used if the user did not specify a port value.
 */
#define NFS_UNSPEC_PORT		(-1)

/*
 * Maximum number of pages that readdir can use for creating
 * a vmapped array of pages.
 */
#define NFS_MAX_READDIR_PAGES 8

/*
 * In-kernel mount arguments
 */
struct nfs_parsed_mount_data {
	int			flags;
	int			rsize, wsize;
	int			timeo, retrans;
	int			acregmin, acregmax,
				acdirmin, acdirmax;
	int			namlen;
	unsigned int		options;
	unsigned int		bsize;
	unsigned int		auth_flavor_len;
	rpc_authflavor_t	auth_flavors[1];
	char			*client_address;
	unsigned int		version;
	unsigned int		minorversion;
	char			*fscache_uniq;

	struct {
		struct sockaddr_storage	address;
		size_t			addrlen;
		char			*hostname;
		u32			version;
		int			port;
		unsigned short		protocol;
	} mount_server;

	struct {
		struct sockaddr_storage	address;
		size_t			addrlen;
		char			*hostname;
		char			*export_path;
		int			port;
		unsigned short		protocol;
	} nfs_server;

	struct security_mnt_opts lsm_opts;
};

/* mount_clnt.c */
struct nfs_mount_request {
	struct sockaddr		*sap;
	size_t			salen;
	char			*hostname;
	char			*dirpath;
	u32			version;
	unsigned short		protocol;
	struct nfs_fh		*fh;
	int			noresvport;
	unsigned int		*auth_flav_len;
	rpc_authflavor_t	*auth_flavs;
};

extern int nfs_mount(struct nfs_mount_request *info);
extern void nfs_umount(const struct nfs_mount_request *info);

/* client.c */
extern struct rpc_program nfs_program;

extern void nfs_cleanup_cb_ident_idr(void);
extern void nfs_put_client(struct nfs_client *);
extern struct nfs_client *nfs4_find_client_no_ident(const struct sockaddr *);
extern struct nfs_client *nfs4_find_client_ident(int);
extern struct nfs_client *
nfs4_find_client_sessionid(const struct sockaddr *, struct nfs4_sessionid *);
extern struct nfs_server *nfs_create_server(
					const struct nfs_parsed_mount_data *,
					struct nfs_fh *);
extern struct nfs_server *nfs4_create_server(
					const struct nfs_parsed_mount_data *,
					struct nfs_fh *);
extern struct nfs_server *nfs4_create_referral_server(struct nfs_clone_mount *,
						      struct nfs_fh *);
extern void nfs_free_server(struct nfs_server *server);
extern struct nfs_server *nfs_clone_server(struct nfs_server *,
					   struct nfs_fh *,
					   struct nfs_fattr *);
extern void nfs_mark_client_ready(struct nfs_client *clp, int state);
extern int nfs4_check_client_ready(struct nfs_client *clp);
extern struct nfs_client *nfs4_set_ds_client(struct nfs_client* mds_clp,
					     const struct sockaddr *ds_addr,
					     int ds_addrlen, int ds_proto);
#ifdef CONFIG_PROC_FS
extern int __init nfs_fs_proc_init(void);
extern void nfs_fs_proc_exit(void);
#else
static inline int nfs_fs_proc_init(void)
{
	return 0;
}
static inline void nfs_fs_proc_exit(void)
{
}
#endif

/* nfs4namespace.c */
#ifdef CONFIG_NFS_V4
extern struct vfsmount *nfs_do_refmount(struct dentry *dentry);
#else
static inline
struct vfsmount *nfs_do_refmount(struct dentry *dentry)
{
	return ERR_PTR(-ENOENT);
}
#endif

/* callback_xdr.c */
extern struct svc_version nfs4_callback_version1;
extern struct svc_version nfs4_callback_version4;

/* pagelist.c */
extern int __init nfs_init_nfspagecache(void);
extern void nfs_destroy_nfspagecache(void);
extern int __init nfs_init_readpagecache(void);
extern void nfs_destroy_readpagecache(void);
extern int __init nfs_init_writepagecache(void);
extern void nfs_destroy_writepagecache(void);

extern int __init nfs_init_directcache(void);
extern void nfs_destroy_directcache(void);

/* nfs2xdr.c */
extern int nfs_stat_to_errno(enum nfs_stat);
extern struct rpc_procinfo nfs_procedures[];
extern int nfs2_decode_dirent(struct xdr_stream *,
				struct nfs_entry *, int);

/* nfs3xdr.c */
extern struct rpc_procinfo nfs3_procedures[];
extern int nfs3_decode_dirent(struct xdr_stream *,
				struct nfs_entry *, int);

/* nfs4xdr.c */
#ifdef CONFIG_NFS_V4
extern int nfs4_decode_dirent(struct xdr_stream *,
				struct nfs_entry *, int);
#endif
#ifdef CONFIG_NFS_V4_1
extern const u32 nfs41_maxread_overhead;
extern const u32 nfs41_maxwrite_overhead;
#endif

/* nfs4proc.c */
#ifdef CONFIG_NFS_V4
extern struct rpc_procinfo nfs4_procedures[];
void nfs_fixup_secinfo_attributes(struct nfs_fattr *, struct nfs_fh *);
#endif

extern int nfs4_init_ds_session(struct nfs_client *clp);

/* proc.c */
void nfs_close_context(struct nfs_open_context *ctx, int is_sync);
extern int nfs_init_client(struct nfs_client *clp,
			   const struct rpc_timeout *timeparms,
			   const char *ip_addr, rpc_authflavor_t authflavour,
			   int noresvport);

/* dir.c */
extern int nfs_access_cache_shrinker(struct shrinker *shrink,
					int nr_to_scan, gfp_t gfp_mask);

/* inode.c */
extern struct workqueue_struct *nfsiod_workqueue;
extern struct inode *nfs_alloc_inode(struct super_block *sb);
extern void nfs_destroy_inode(struct inode *);
extern int nfs_write_inode(struct inode *, struct writeback_control *);
extern void nfs_evict_inode(struct inode *);
#ifdef CONFIG_NFS_V4
extern void nfs4_evict_inode(struct inode *);
#endif
void nfs_zap_acl_cache(struct inode *inode);
extern int nfs_wait_bit_killable(void *word);

/* super.c */
extern struct file_system_type nfs_xdev_fs_type;
#ifdef CONFIG_NFS_V4
extern struct file_system_type nfs4_xdev_fs_type;
extern struct file_system_type nfs4_referral_fs_type;
#endif

extern struct rpc_stat nfs_rpcstat;

extern int __init register_nfs_fs(void);
extern void __exit unregister_nfs_fs(void);
extern void nfs_sb_active(struct super_block *sb);
extern void nfs_sb_deactive(struct super_block *sb);

/* namespace.c */
extern char *nfs_path(char **p, struct dentry *dentry,
		      char *buffer, ssize_t buflen);
extern struct vfsmount *nfs_d_automount(struct path *path);

/* getroot.c */
extern struct dentry *nfs_get_root(struct super_block *, struct nfs_fh *,
				   const char *);
#ifdef CONFIG_NFS_V4
extern struct dentry *nfs4_get_root(struct super_block *, struct nfs_fh *,
				    const char *);

extern int nfs4_get_rootfh(struct nfs_server *server, struct nfs_fh *mntfh);
#endif

/* read.c */
extern int nfs_initiate_read(struct nfs_read_data *data, struct rpc_clnt *clnt,
			     const struct rpc_call_ops *call_ops);
extern void nfs_read_prepare(struct rpc_task *task, void *calldata);

/* write.c */
extern void nfs_commit_free(struct nfs_write_data *p);
extern int nfs_initiate_write(struct nfs_write_data *data,
			      struct rpc_clnt *clnt,
			      const struct rpc_call_ops *call_ops,
			      int how);
extern void nfs_write_prepare(struct rpc_task *task, void *calldata);
extern int nfs_initiate_commit(struct nfs_write_data *data,
			       struct rpc_clnt *clnt,
			       const struct rpc_call_ops *call_ops,
			       int how);
extern void nfs_init_commit(struct nfs_write_data *data,
			    struct list_head *head,
			    struct pnfs_layout_segment *lseg);
void nfs_retry_commit(struct list_head *page_list,
		      struct pnfs_layout_segment *lseg);
void nfs_commit_clear_lock(struct nfs_inode *nfsi);
void nfs_commitdata_release(void *data);
void nfs_commit_release_pages(struct nfs_write_data *data);

#ifdef CONFIG_MIGRATION
extern int nfs_migrate_page(struct address_space *,
		struct page *, struct page *);
#else
#define nfs_migrate_page NULL
#endif

/* nfs4proc.c */
extern void nfs4_reset_read(struct rpc_task *task, struct nfs_read_data *data);
extern int nfs4_init_client(struct nfs_client *clp,
			    const struct rpc_timeout *timeparms,
			    const char *ip_addr,
			    rpc_authflavor_t authflavour,
			    int noresvport);
extern void nfs4_reset_write(struct rpc_task *task, struct nfs_write_data *data);
extern int _nfs4_call_sync(struct rpc_clnt *clnt,
			   struct nfs_server *server,
			   struct rpc_message *msg,
			   struct nfs4_sequence_args *args,
			   struct nfs4_sequence_res *res,
			   int cache_reply);
extern int _nfs4_call_sync_session(struct rpc_clnt *clnt,
				   struct nfs_server *server,
				   struct rpc_message *msg,
				   struct nfs4_sequence_args *args,
				   struct nfs4_sequence_res *res,
				   int cache_reply);

/*
 * Determine the device name as a string
 */
static inline char *nfs_devname(struct dentry *dentry,
				char *buffer, ssize_t buflen)
{
	char *dummy;
	return nfs_path(&dummy, dentry, buffer, buflen);
}

/*
 * Determine the actual block size (and log2 thereof)
 */
static inline
unsigned long nfs_block_bits(unsigned long bsize, unsigned char *nrbitsp)
{
	/* make sure blocksize is a power of two */
	if ((bsize & (bsize - 1)) || nrbitsp) {
		unsigned char	nrbits;

		for (nrbits = 31; nrbits && !(bsize & (1 << nrbits)); nrbits--)
			;
		bsize = 1 << nrbits;
		if (nrbitsp)
			*nrbitsp = nrbits;
	}

	return bsize;
}

/*
 * Calculate the number of 512byte blocks used.
 */
static inline blkcnt_t nfs_calc_block_size(u64 tsize)
{
	blkcnt_t used = (tsize + 511) >> 9;
	return (used > ULONG_MAX) ? ULONG_MAX : used;
}

/*
 * Compute and set NFS server blocksize
 */
static inline
unsigned long nfs_block_size(unsigned long bsize, unsigned char *nrbitsp)
{
	if (bsize < NFS_MIN_FILE_IO_SIZE)
		bsize = NFS_DEF_FILE_IO_SIZE;
	else if (bsize >= NFS_MAX_FILE_IO_SIZE)
		bsize = NFS_MAX_FILE_IO_SIZE;

	return nfs_block_bits(bsize, nrbitsp);
}

/*
 * Determine the maximum file size for a superblock
 */
static inline
void nfs_super_set_maxbytes(struct super_block *sb, __u64 maxfilesize)
{
	sb->s_maxbytes = (loff_t)maxfilesize;
	if (sb->s_maxbytes > MAX_LFS_FILESIZE || sb->s_maxbytes <= 0)
		sb->s_maxbytes = MAX_LFS_FILESIZE;
}

/*
 * Determine the number of bytes of data the page contains
 */
static inline
unsigned int nfs_page_length(struct page *page)
{
	loff_t i_size = i_size_read(page->mapping->host);

	if (i_size > 0) {
		pgoff_t end_index = (i_size - 1) >> PAGE_CACHE_SHIFT;
		if (page->index < end_index)
			return PAGE_CACHE_SIZE;
		if (page->index == end_index)
			return ((i_size - 1) & ~PAGE_CACHE_MASK) + 1;
	}
	return 0;
}

/*
 * Convert a umode to a dirent->d_type
 */
static inline
unsigned char nfs_umode_to_dtype(umode_t mode)
{
	return (mode >> 12) & 15;
}

/*
 * Determine the number of pages in an array of length 'len' and
 * with a base offset of 'base'
 */
static inline
unsigned int nfs_page_array_len(unsigned int base, size_t len)
{
	return ((unsigned long)len + (unsigned long)base +
		PAGE_SIZE - 1) >> PAGE_SHIFT;
}

/*
 * Helper for restarting RPC calls in the possible presence of NFSv4.1
 * sessions.
 */
static inline int nfs_restart_rpc(struct rpc_task *task, const struct nfs_client *clp)
{
	if (nfs4_has_session(clp))
		return rpc_restart_call_prepare(task);
	return rpc_restart_call(task);
}
