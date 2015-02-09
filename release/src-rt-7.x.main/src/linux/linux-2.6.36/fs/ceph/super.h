#ifndef _FS_CEPH_SUPER_H
#define _FS_CEPH_SUPER_H

#include "ceph_debug.h"

#include <asm/unaligned.h>
#include <linux/backing-dev.h>
#include <linux/completion.h>
#include <linux/exportfs.h>
#include <linux/fs.h>
#include <linux/mempool.h>
#include <linux/pagemap.h>
#include <linux/wait.h>
#include <linux/writeback.h>
#include <linux/slab.h>

#include "types.h"
#include "messenger.h"
#include "msgpool.h"
#include "mon_client.h"
#include "mds_client.h"
#include "osd_client.h"
#include "ceph_fs.h"

/* f_type in struct statfs */
#define CEPH_SUPER_MAGIC 0x00c36400

/* large granularity for statfs utilization stats to facilitate
 * large volume sizes on 32-bit machines. */
#define CEPH_BLOCK_SHIFT   20  /* 1 MB */
#define CEPH_BLOCK         (1 << CEPH_BLOCK_SHIFT)

/*
 * Supported features
 */
#define CEPH_FEATURE_SUPPORTED CEPH_FEATURE_NOSRCADDR | CEPH_FEATURE_FLOCK
#define CEPH_FEATURE_REQUIRED  CEPH_FEATURE_NOSRCADDR

/*
 * mount options
 */
#define CEPH_OPT_FSID             (1<<0)
#define CEPH_OPT_NOSHARE          (1<<1) /* don't share client with other sbs */
#define CEPH_OPT_MYIP             (1<<2) /* specified my ip */
#define CEPH_OPT_DIRSTAT          (1<<4) /* funky `cat dirname` for stats */
#define CEPH_OPT_RBYTES           (1<<5) /* dir st_bytes = rbytes */
#define CEPH_OPT_NOCRC            (1<<6) /* no data crc on writes */
#define CEPH_OPT_NOASYNCREADDIR   (1<<7) /* no dcache readdir */

#define CEPH_OPT_DEFAULT   (CEPH_OPT_RBYTES)

#define ceph_set_opt(client, opt) \
	(client)->mount_args->flags |= CEPH_OPT_##opt;
#define ceph_test_opt(client, opt) \
	(!!((client)->mount_args->flags & CEPH_OPT_##opt))


struct ceph_mount_args {
	int sb_flags;
	int flags;
	struct ceph_fsid fsid;
	struct ceph_entity_addr my_addr;
	int num_mon;
	struct ceph_entity_addr *mon_addr;
	int mount_timeout;
	int osd_idle_ttl;
	int osd_timeout;
	int osd_keepalive_timeout;
	int wsize;
	int rsize;            /* max readahead */
	int congestion_kb;    /* max writeback in flight */
	int caps_wanted_delay_min, caps_wanted_delay_max;
	int cap_release_safety;
	int max_readdir;       /* max readdir result (entires) */
	int max_readdir_bytes; /* max readdir result (bytes) */
	char *snapdir_name;   /* default ".snap" */
	char *name;
	char *secret;
};

/*
 * defaults
 */
#define CEPH_MOUNT_TIMEOUT_DEFAULT  60
#define CEPH_OSD_TIMEOUT_DEFAULT    60  /* seconds */
#define CEPH_OSD_KEEPALIVE_DEFAULT  5
#define CEPH_OSD_IDLE_TTL_DEFAULT    60
#define CEPH_MOUNT_RSIZE_DEFAULT    (512*1024) /* readahead */
#define CEPH_MAX_READDIR_DEFAULT    1024
#define CEPH_MAX_READDIR_BYTES_DEFAULT    (512*1024)

#define CEPH_MSG_MAX_FRONT_LEN	(16*1024*1024)
#define CEPH_MSG_MAX_DATA_LEN	(16*1024*1024)

#define CEPH_SNAPDIRNAME_DEFAULT ".snap"
#define CEPH_AUTH_NAME_DEFAULT   "guest"
/*
 * Delay telling the MDS we no longer want caps, in case we reopen
 * the file.  Delay a minimum amount of time, even if we send a cap
 * message for some other reason.  Otherwise, take the oppotunity to
 * update the mds to avoid sending another message later.
 */
#define CEPH_CAPS_WANTED_DELAY_MIN_DEFAULT      5  /* cap release delay */
#define CEPH_CAPS_WANTED_DELAY_MAX_DEFAULT     60  /* cap release delay */

#define CEPH_CAP_RELEASE_SAFETY_DEFAULT        (CEPH_CAPS_PER_RELEASE * 4)

/* mount state */
enum {
	CEPH_MOUNT_MOUNTING,
	CEPH_MOUNT_MOUNTED,
	CEPH_MOUNT_UNMOUNTING,
	CEPH_MOUNT_UNMOUNTED,
	CEPH_MOUNT_SHUTDOWN,
};

/*
 * subtract jiffies
 */
static inline unsigned long time_sub(unsigned long a, unsigned long b)
{
	BUG_ON(time_after(b, a));
	return (long)a - (long)b;
}

/*
 * per-filesystem client state
 *
 * possibly shared by multiple mount points, if they are
 * mounting the same ceph filesystem/cluster.
 */
struct ceph_client {
	struct ceph_fsid fsid;
	bool have_fsid;

	struct mutex mount_mutex;       /* serialize mount attempts */
	struct ceph_mount_args *mount_args;

	struct super_block *sb;

	unsigned long mount_state;
	wait_queue_head_t auth_wq;

	int auth_err;

	int min_caps;                  /* min caps i added */

	struct ceph_messenger *msgr;   /* messenger instance */
	struct ceph_mon_client monc;
	struct ceph_mds_client mdsc;
	struct ceph_osd_client osdc;

	/* writeback */
	mempool_t *wb_pagevec_pool;
	struct workqueue_struct *wb_wq;
	struct workqueue_struct *pg_inv_wq;
	struct workqueue_struct *trunc_wq;
	atomic_long_t writeback_count;

	struct backing_dev_info backing_dev_info;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_monmap;
	struct dentry *debugfs_mdsmap, *debugfs_osdmap;
	struct dentry *debugfs_dir, *debugfs_dentry_lru, *debugfs_caps;
	struct dentry *debugfs_congestion_kb;
	struct dentry *debugfs_bdi;
#endif
};

/*
 * File i/o capability.  This tracks shared state with the metadata
 * server that allows us to cache or writeback attributes or to read
 * and write data.  For any given inode, we should have one or more
 * capabilities, one issued by each metadata server, and our
 * cumulative access is the OR of all issued capabilities.
 *
 * Each cap is referenced by the inode's i_caps rbtree and by per-mds
 * session capability lists.
 */
struct ceph_cap {
	struct ceph_inode_info *ci;
	struct rb_node ci_node;          /* per-ci cap tree */
	struct ceph_mds_session *session;
	struct list_head session_caps;   /* per-session caplist */
	int mds;
	u64 cap_id;       /* unique cap id (mds provided) */
	int issued;       /* latest, from the mds */
	int implemented;  /* implemented superset of issued (for revocation) */
	int mds_wanted;
	u32 seq, issue_seq, mseq;
	u32 cap_gen;      /* active/stale cycle */
	unsigned long last_used;
	struct list_head caps_item;
};

#define CHECK_CAPS_NODELAY    1  /* do not delay any further */
#define CHECK_CAPS_AUTHONLY   2  /* only check auth cap */
#define CHECK_CAPS_FLUSH      4  /* flush any dirty caps */

/*
 * Snapped cap state that is pending flush to mds.  When a snapshot occurs,
 * we first complete any in-process sync writes and writeback any dirty
 * data before flushing the snapped state (tracked here) back to the MDS.
 */
struct ceph_cap_snap {
	atomic_t nref;
	struct ceph_inode_info *ci;
	struct list_head ci_item, flushing_item;

	u64 follows, flush_tid;
	int issued, dirty;
	struct ceph_snap_context *context;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	struct ceph_buffer *xattr_blob;
	u64 xattr_version;

	u64 size;
	struct timespec mtime, atime, ctime;
	u64 time_warp_seq;
	int writing;   /* a sync write is still in progress */
	int dirty_pages;     /* dirty pages awaiting writeback */
};

static inline void ceph_put_cap_snap(struct ceph_cap_snap *capsnap)
{
	if (atomic_dec_and_test(&capsnap->nref)) {
		if (capsnap->xattr_blob)
			ceph_buffer_put(capsnap->xattr_blob);
		kfree(capsnap);
	}
}

/*
 * The frag tree describes how a directory is fragmented, potentially across
 * multiple metadata servers.  It is also used to indicate points where
 * metadata authority is delegated, and whether/where metadata is replicated.
 *
 * A _leaf_ frag will be present in the i_fragtree IFF there is
 * delegation info.  That is, if mds >= 0 || ndist > 0.
 */
#define CEPH_MAX_DIRFRAG_REP 4

struct ceph_inode_frag {
	struct rb_node node;

	/* fragtree state */
	u32 frag;
	int split_by;         /* i.e. 2^(split_by) children */

	/* delegation and replication info */
	int mds;              /* -1 if same authority as parent */
	int ndist;            /* >0 if replicated */
	int dist[CEPH_MAX_DIRFRAG_REP];
};

/*
 * We cache inode xattrs as an encoded blob until they are first used,
 * at which point we parse them into an rbtree.
 */
struct ceph_inode_xattr {
	struct rb_node node;

	const char *name;
	int name_len;
	const char *val;
	int val_len;
	int dirty;

	int should_free_name;
	int should_free_val;
};

struct ceph_inode_xattrs_info {
	/*
	 * (still encoded) xattr blob. we avoid the overhead of parsing
	 * this until someone actually calls getxattr, etc.
	 *
	 * blob->vec.iov_len == 4 implies there are no xattrs; blob ==
	 * NULL means we don't know.
	*/
	struct ceph_buffer *blob, *prealloc_blob;

	struct rb_root index;
	bool dirty;
	int count;
	int names_size;
	int vals_size;
	u64 version, index_version;
};

/*
 * Ceph inode.
 */
#define CEPH_I_COMPLETE  1  /* we have complete directory cached */
#define CEPH_I_NODELAY   4  /* do not delay cap release */
#define CEPH_I_FLUSH     8  /* do not delay flush of dirty metadata */
#define CEPH_I_NOFLUSH  16  /* do not flush dirty caps */

struct ceph_inode_info {
	struct ceph_vino i_vino;   /* ceph ino + snap */

	u64 i_version;
	u32 i_time_warp_seq;

	unsigned i_ceph_flags;
	unsigned long i_release_count;

	struct ceph_file_layout i_layout;
	char *i_symlink;

	/* for dirs */
	struct timespec i_rctime;
	u64 i_rbytes, i_rfiles, i_rsubdirs;
	u64 i_files, i_subdirs;
	u64 i_max_offset;  /* largest readdir offset, set with I_COMPLETE */

	struct rb_root i_fragtree;
	struct mutex i_fragtree_mutex;

	struct ceph_inode_xattrs_info i_xattrs;

	/* capabilities.  protected _both_ by i_lock and cap->session's
	 * s_mutex. */
	struct rb_root i_caps;           /* cap list */
	struct ceph_cap *i_auth_cap;     /* authoritative cap, if any */
	unsigned i_dirty_caps, i_flushing_caps;     /* mask of dirtied fields */
	struct list_head i_dirty_item, i_flushing_item;
	u64 i_cap_flush_seq;
	/* we need to track cap writeback on a per-cap-bit basis, to allow
	 * overlapping, pipelined cap flushes to the mds.  we can probably
	 * reduce the tid to 8 bits if we're concerned about inode size. */
	u16 i_cap_flush_last_tid, i_cap_flush_tid[CEPH_CAP_BITS];
	wait_queue_head_t i_cap_wq;      /* threads waiting on a capability */
	unsigned long i_hold_caps_min; /* jiffies */
	unsigned long i_hold_caps_max; /* jiffies */
	struct list_head i_cap_delay_list;  /* for delayed cap release to mds */
	int i_cap_exporting_mds;         /* to handle cap migration between */
	unsigned i_cap_exporting_mseq;   /*  mds's. */
	unsigned i_cap_exporting_issued;
	struct ceph_cap_reservation i_cap_migration_resv;
	struct list_head i_cap_snaps;   /* snapped state pending flush to mds */
	struct ceph_snap_context *i_head_snapc;  /* set if wr_buffer_head > 0 or
						    dirty|flushing caps */
	unsigned i_snap_caps;           /* cap bits for snapped files */

	int i_nr_by_mode[CEPH_FILE_MODE_NUM];  /* open file counts */

	u32 i_truncate_seq;        /* last truncate to smaller size */
	u64 i_truncate_size;       /*  and the size we last truncated down to */
	int i_truncate_pending;    /*  still need to call vmtruncate */

	u64 i_max_size;            /* max file size authorized by mds */
	u64 i_reported_size; /* (max_)size reported to or requested of mds */
	u64 i_wanted_max_size;     /* offset we'd like to write too */
	u64 i_requested_max_size;  /* max_size we've requested */

	/* held references to caps */
	int i_pin_ref;
	int i_rd_ref, i_rdcache_ref, i_wr_ref;
	int i_wrbuffer_ref, i_wrbuffer_ref_head;
	u32 i_shared_gen;       /* increment each time we get FILE_SHARED */
	u32 i_rdcache_gen;      /* we increment this each time we get
				   FILE_CACHE.  If it's non-zero, we
				   _may_ have cached pages. */
	u32 i_rdcache_revoking; /* RDCACHE gen to async invalidate, if any */

	struct list_head i_unsafe_writes; /* uncommitted sync writes */
	struct list_head i_unsafe_dirops; /* uncommitted mds dir ops */
	spinlock_t i_unsafe_lock;

	struct ceph_snap_realm *i_snap_realm; /* snap realm (if caps) */
	int i_snap_realm_counter; /* snap realm (if caps) */
	struct list_head i_snap_realm_item;
	struct list_head i_snap_flush_item;

	struct work_struct i_wb_work;  /* writeback work */
	struct work_struct i_pg_inv_work;  /* page invalidation work */

	struct work_struct i_vmtruncate_work;

	struct inode vfs_inode; /* at end */
};

static inline struct ceph_inode_info *ceph_inode(struct inode *inode)
{
	return container_of(inode, struct ceph_inode_info, vfs_inode);
}

static inline void ceph_i_clear(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	spin_lock(&inode->i_lock);
	ci->i_ceph_flags &= ~mask;
	spin_unlock(&inode->i_lock);
}

static inline void ceph_i_set(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	spin_lock(&inode->i_lock);
	ci->i_ceph_flags |= mask;
	spin_unlock(&inode->i_lock);
}

static inline bool ceph_i_test(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	bool r;

	smp_mb();
	r = (ci->i_ceph_flags & mask) == mask;
	return r;
}


/* find a specific frag @f */
extern struct ceph_inode_frag *__ceph_find_frag(struct ceph_inode_info *ci,
						u32 f);

/*
 * choose fragment for value @v.  copy frag content to pfrag, if leaf
 * exists
 */
extern u32 ceph_choose_frag(struct ceph_inode_info *ci, u32 v,
			    struct ceph_inode_frag *pfrag,
			    int *found);

/*
 * Ceph dentry state
 */
struct ceph_dentry_info {
	struct ceph_mds_session *lease_session;
	u32 lease_gen, lease_shared_gen;
	u32 lease_seq;
	unsigned long lease_renew_after, lease_renew_from;
	struct list_head lru;
	struct dentry *dentry;
	u64 time;
	u64 offset;
};

static inline struct ceph_dentry_info *ceph_dentry(struct dentry *dentry)
{
	return (struct ceph_dentry_info *)dentry->d_fsdata;
}

static inline loff_t ceph_make_fpos(unsigned frag, unsigned off)
{
	return ((loff_t)frag << 32) | (loff_t)off;
}

/*
 * ino_t is <64 bits on many architectures, blech.
 *
 * don't include snap in ino hash, at least for now.
 */
static inline ino_t ceph_vino_to_ino(struct ceph_vino vino)
{
	ino_t ino = (ino_t)vino.ino;  /* ^ (vino.snap << 20); */
#if BITS_PER_LONG == 32
	ino ^= vino.ino >> (sizeof(u64)-sizeof(ino_t)) * 8;
	if (!ino)
		ino = 1;
#endif
	return ino;
}

static inline int ceph_set_ino_cb(struct inode *inode, void *data)
{
	ceph_inode(inode)->i_vino = *(struct ceph_vino *)data;
	inode->i_ino = ceph_vino_to_ino(*(struct ceph_vino *)data);
	return 0;
}

static inline struct ceph_vino ceph_vino(struct inode *inode)
{
	return ceph_inode(inode)->i_vino;
}

/* for printf-style formatting */
#define ceph_vinop(i) ceph_inode(i)->i_vino.ino, ceph_inode(i)->i_vino.snap

static inline u64 ceph_ino(struct inode *inode)
{
	return ceph_inode(inode)->i_vino.ino;
}
static inline u64 ceph_snap(struct inode *inode)
{
	return ceph_inode(inode)->i_vino.snap;
}

static inline int ceph_ino_compare(struct inode *inode, void *data)
{
	struct ceph_vino *pvino = (struct ceph_vino *)data;
	struct ceph_inode_info *ci = ceph_inode(inode);
	return ci->i_vino.ino == pvino->ino &&
		ci->i_vino.snap == pvino->snap;
}

static inline struct inode *ceph_find_inode(struct super_block *sb,
					    struct ceph_vino vino)
{
	ino_t t = ceph_vino_to_ino(vino);
	return ilookup5(sb, t, ceph_ino_compare, &vino);
}


/*
 * caps helpers
 */
static inline bool __ceph_is_any_real_caps(struct ceph_inode_info *ci)
{
	return !RB_EMPTY_ROOT(&ci->i_caps);
}

extern int __ceph_caps_issued(struct ceph_inode_info *ci, int *implemented);
extern int __ceph_caps_issued_mask(struct ceph_inode_info *ci, int mask, int t);
extern int __ceph_caps_issued_other(struct ceph_inode_info *ci,
				    struct ceph_cap *cap);

static inline int ceph_caps_issued(struct ceph_inode_info *ci)
{
	int issued;
	spin_lock(&ci->vfs_inode.i_lock);
	issued = __ceph_caps_issued(ci, NULL);
	spin_unlock(&ci->vfs_inode.i_lock);
	return issued;
}

static inline int ceph_caps_issued_mask(struct ceph_inode_info *ci, int mask,
					int touch)
{
	int r;
	spin_lock(&ci->vfs_inode.i_lock);
	r = __ceph_caps_issued_mask(ci, mask, touch);
	spin_unlock(&ci->vfs_inode.i_lock);
	return r;
}

static inline int __ceph_caps_dirty(struct ceph_inode_info *ci)
{
	return ci->i_dirty_caps | ci->i_flushing_caps;
}
extern void __ceph_mark_dirty_caps(struct ceph_inode_info *ci, int mask);

extern int ceph_caps_revoking(struct ceph_inode_info *ci, int mask);
extern int __ceph_caps_used(struct ceph_inode_info *ci);

extern int __ceph_caps_file_wanted(struct ceph_inode_info *ci);

/*
 * wanted, by virtue of open file modes AND cap refs (buffered/cached data)
 */
static inline int __ceph_caps_wanted(struct ceph_inode_info *ci)
{
	int w = __ceph_caps_file_wanted(ci) | __ceph_caps_used(ci);
	if (w & CEPH_CAP_FILE_BUFFER)
		w |= CEPH_CAP_FILE_EXCL;  /* we want EXCL if dirty data */
	return w;
}

/* what the mds thinks we want */
extern int __ceph_caps_mds_wanted(struct ceph_inode_info *ci);

extern void ceph_caps_init(struct ceph_mds_client *mdsc);
extern void ceph_caps_finalize(struct ceph_mds_client *mdsc);
extern void ceph_adjust_min_caps(struct ceph_mds_client *mdsc, int delta);
extern int ceph_reserve_caps(struct ceph_mds_client *mdsc,
			     struct ceph_cap_reservation *ctx, int need);
extern int ceph_unreserve_caps(struct ceph_mds_client *mdsc,
			       struct ceph_cap_reservation *ctx);
extern void ceph_reservation_status(struct ceph_client *client,
				    int *total, int *avail, int *used,
				    int *reserved, int *min);

static inline struct ceph_client *ceph_inode_to_client(struct inode *inode)
{
	return (struct ceph_client *)inode->i_sb->s_fs_info;
}

static inline struct ceph_client *ceph_sb_to_client(struct super_block *sb)
{
	return (struct ceph_client *)sb->s_fs_info;
}


/*
 * we keep buffered readdir results attached to file->private_data
 */
struct ceph_file_info {
	int fmode;     /* initialized on open */

	/* readdir: position within the dir */
	u32 frag;
	struct ceph_mds_request *last_readdir;
	int at_end;

	/* readdir: position within a frag */
	unsigned offset;       /* offset of last chunk, adjusted for . and .. */
	u64 next_offset;       /* offset of next chunk (last_name's + 1) */
	char *last_name;       /* last entry in previous chunk */
	struct dentry *dentry; /* next dentry (for dcache readdir) */
	unsigned long dir_release_count;

	/* used for -o dirstat read() on directory thing */
	char *dir_info;
	int dir_info_len;
};



/*
 * snapshots
 */

/*
 * A "snap context" is the set of existing snapshots when we
 * write data.  It is used by the OSD to guide its COW behavior.
 *
 * The ceph_snap_context is refcounted, and attached to each dirty
 * page, indicating which context the dirty data belonged when it was
 * dirtied.
 */
struct ceph_snap_context {
	atomic_t nref;
	u64 seq;
	int num_snaps;
	u64 snaps[];
};

static inline struct ceph_snap_context *
ceph_get_snap_context(struct ceph_snap_context *sc)
{
	/*
	printk("get_snap_context %p %d -> %d\n", sc, atomic_read(&sc->nref),
	       atomic_read(&sc->nref)+1);
	*/
	if (sc)
		atomic_inc(&sc->nref);
	return sc;
}

static inline void ceph_put_snap_context(struct ceph_snap_context *sc)
{
	if (!sc)
		return;
	/*
	printk("put_snap_context %p %d -> %d\n", sc, atomic_read(&sc->nref),
	       atomic_read(&sc->nref)-1);
	*/
	if (atomic_dec_and_test(&sc->nref)) {
		/*printk(" deleting snap_context %p\n", sc);*/
		kfree(sc);
	}
}

/*
 * A "snap realm" describes a subset of the file hierarchy sharing
 * the same set of snapshots that apply to it.  The realms themselves
 * are organized into a hierarchy, such that children inherit (some of)
 * the snapshots of their parents.
 *
 * All inodes within the realm that have capabilities are linked into a
 * per-realm list.
 */
struct ceph_snap_realm {
	u64 ino;
	atomic_t nref;
	struct rb_node node;

	u64 created, seq;
	u64 parent_ino;
	u64 parent_since;   /* snapid when our current parent became so */

	u64 *prior_parent_snaps;      /* snaps inherited from any parents we */
	int num_prior_parent_snaps;   /*  had prior to parent_since */
	u64 *snaps;                   /* snaps specific to this realm */
	int num_snaps;

	struct ceph_snap_realm *parent;
	struct list_head children;       /* list of child realms */
	struct list_head child_item;

	struct list_head empty_item;     /* if i have ref==0 */

	struct list_head dirty_item;     /* if realm needs new context */

	/* the current set of snaps for this realm */
	struct ceph_snap_context *cached_context;

	struct list_head inodes_with_caps;
	spinlock_t inodes_with_caps_lock;
};



/*
 * calculate the number of pages a given length and offset map onto,
 * if we align the data.
 */
static inline int calc_pages_for(u64 off, u64 len)
{
	return ((off+len+PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT) -
		(off >> PAGE_CACHE_SHIFT);
}



/* snap.c */
struct ceph_snap_realm *ceph_lookup_snap_realm(struct ceph_mds_client *mdsc,
					       u64 ino);
extern void ceph_get_snap_realm(struct ceph_mds_client *mdsc,
				struct ceph_snap_realm *realm);
extern void ceph_put_snap_realm(struct ceph_mds_client *mdsc,
				struct ceph_snap_realm *realm);
extern int ceph_update_snap_trace(struct ceph_mds_client *m,
				  void *p, void *e, bool deletion);
extern void ceph_handle_snap(struct ceph_mds_client *mdsc,
			     struct ceph_mds_session *session,
			     struct ceph_msg *msg);
extern void ceph_queue_cap_snap(struct ceph_inode_info *ci);
extern int __ceph_finish_cap_snap(struct ceph_inode_info *ci,
				  struct ceph_cap_snap *capsnap);
extern void ceph_cleanup_empty_realms(struct ceph_mds_client *mdsc);

/*
 * a cap_snap is "pending" if it is still awaiting an in-progress
 * sync write (that may/may not still update size, mtime, etc.).
 */
static inline bool __ceph_have_pending_cap_snap(struct ceph_inode_info *ci)
{
	return !list_empty(&ci->i_cap_snaps) &&
		list_entry(ci->i_cap_snaps.prev, struct ceph_cap_snap,
			   ci_item)->writing;
}


/* super.c */
extern struct kmem_cache *ceph_inode_cachep;
extern struct kmem_cache *ceph_cap_cachep;
extern struct kmem_cache *ceph_dentry_cachep;
extern struct kmem_cache *ceph_file_cachep;

extern const char *ceph_msg_type_name(int type);
extern int ceph_check_fsid(struct ceph_client *client, struct ceph_fsid *fsid);

/* inode.c */
extern const struct inode_operations ceph_file_iops;

extern struct inode *ceph_alloc_inode(struct super_block *sb);
extern void ceph_destroy_inode(struct inode *inode);

extern struct inode *ceph_get_inode(struct super_block *sb,
				    struct ceph_vino vino);
extern struct inode *ceph_get_snapdir(struct inode *parent);
extern int ceph_fill_file_size(struct inode *inode, int issued,
			       u32 truncate_seq, u64 truncate_size, u64 size);
extern void ceph_fill_file_time(struct inode *inode, int issued,
				u64 time_warp_seq, struct timespec *ctime,
				struct timespec *mtime, struct timespec *atime);
extern int ceph_fill_trace(struct super_block *sb,
			   struct ceph_mds_request *req,
			   struct ceph_mds_session *session);
extern int ceph_readdir_prepopulate(struct ceph_mds_request *req,
				    struct ceph_mds_session *session);

extern int ceph_inode_holds_cap(struct inode *inode, int mask);

extern int ceph_inode_set_size(struct inode *inode, loff_t size);
extern void __ceph_do_pending_vmtruncate(struct inode *inode);
extern void ceph_queue_vmtruncate(struct inode *inode);

extern void ceph_queue_invalidate(struct inode *inode);
extern void ceph_queue_writeback(struct inode *inode);

extern int ceph_do_getattr(struct inode *inode, int mask);
extern int ceph_permission(struct inode *inode, int mask);
extern int ceph_setattr(struct dentry *dentry, struct iattr *attr);
extern int ceph_getattr(struct vfsmount *mnt, struct dentry *dentry,
			struct kstat *stat);

/* xattr.c */
extern int ceph_setxattr(struct dentry *, const char *, const void *,
			 size_t, int);
extern ssize_t ceph_getxattr(struct dentry *, const char *, void *, size_t);
extern ssize_t ceph_listxattr(struct dentry *, char *, size_t);
extern int ceph_removexattr(struct dentry *, const char *);
extern void __ceph_build_xattrs_blob(struct ceph_inode_info *ci);
extern void __ceph_destroy_xattrs(struct ceph_inode_info *ci);

/* caps.c */
extern const char *ceph_cap_string(int c);
extern void ceph_handle_caps(struct ceph_mds_session *session,
			     struct ceph_msg *msg);
extern int ceph_add_cap(struct inode *inode,
			struct ceph_mds_session *session, u64 cap_id,
			int fmode, unsigned issued, unsigned wanted,
			unsigned cap, unsigned seq, u64 realmino, int flags,
			struct ceph_cap_reservation *caps_reservation);
extern void __ceph_remove_cap(struct ceph_cap *cap);
static inline void ceph_remove_cap(struct ceph_cap *cap)
{
	struct inode *inode = &cap->ci->vfs_inode;
	spin_lock(&inode->i_lock);
	__ceph_remove_cap(cap);
	spin_unlock(&inode->i_lock);
}
extern void ceph_put_cap(struct ceph_mds_client *mdsc,
			 struct ceph_cap *cap);

extern void ceph_queue_caps_release(struct inode *inode);
extern int ceph_write_inode(struct inode *inode, struct writeback_control *wbc);
extern int ceph_fsync(struct file *file, int datasync);
extern void ceph_kick_flushing_caps(struct ceph_mds_client *mdsc,
				    struct ceph_mds_session *session);
extern struct ceph_cap *ceph_get_cap_for_mds(struct ceph_inode_info *ci,
					     int mds);
extern int ceph_get_cap_mds(struct inode *inode);
extern void ceph_get_cap_refs(struct ceph_inode_info *ci, int caps);
extern void ceph_put_cap_refs(struct ceph_inode_info *ci, int had);
extern void ceph_put_wrbuffer_cap_refs(struct ceph_inode_info *ci, int nr,
				       struct ceph_snap_context *snapc);
extern void __ceph_flush_snaps(struct ceph_inode_info *ci,
			       struct ceph_mds_session **psession,
			       int again);
extern void ceph_check_caps(struct ceph_inode_info *ci, int flags,
			    struct ceph_mds_session *session);
extern void ceph_check_delayed_caps(struct ceph_mds_client *mdsc);
extern void ceph_flush_dirty_caps(struct ceph_mds_client *mdsc);

extern int ceph_encode_inode_release(void **p, struct inode *inode,
				     int mds, int drop, int unless, int force);
extern int ceph_encode_dentry_release(void **p, struct dentry *dn,
				      int mds, int drop, int unless);

extern int ceph_get_caps(struct ceph_inode_info *ci, int need, int want,
			 int *got, loff_t endoff);

/* for counting open files by mode */
static inline void __ceph_get_fmode(struct ceph_inode_info *ci, int mode)
{
	ci->i_nr_by_mode[mode]++;
}
extern void ceph_put_fmode(struct ceph_inode_info *ci, int mode);

/* addr.c */
extern const struct address_space_operations ceph_aops;
extern int ceph_mmap(struct file *file, struct vm_area_struct *vma);

/* file.c */
extern const struct file_operations ceph_file_fops;
extern const struct address_space_operations ceph_aops;
extern int ceph_open(struct inode *inode, struct file *file);
extern struct dentry *ceph_lookup_open(struct inode *dir, struct dentry *dentry,
				       struct nameidata *nd, int mode,
				       int locked_dir);
extern int ceph_release(struct inode *inode, struct file *filp);
extern void ceph_release_page_vector(struct page **pages, int num_pages);

/* dir.c */
extern const struct file_operations ceph_dir_fops;
extern const struct inode_operations ceph_dir_iops;
extern const struct dentry_operations ceph_dentry_ops, ceph_snap_dentry_ops,
	ceph_snapdir_dentry_ops;

extern int ceph_handle_notrace_create(struct inode *dir, struct dentry *dentry);
extern struct dentry *ceph_finish_lookup(struct ceph_mds_request *req,
					 struct dentry *dentry, int err);

extern void ceph_dentry_lru_add(struct dentry *dn);
extern void ceph_dentry_lru_touch(struct dentry *dn);
extern void ceph_dentry_lru_del(struct dentry *dn);
extern void ceph_invalidate_dentry_lease(struct dentry *dentry);

/*
 * our d_ops vary depending on whether the inode is live,
 * snapshotted (read-only), or a virtual ".snap" directory.
 */
int ceph_init_dentry(struct dentry *dentry);


/* ioctl.c */
extern long ceph_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* export.c */
extern const struct export_operations ceph_export_ops;

/* debugfs.c */
extern int ceph_debugfs_init(void);
extern void ceph_debugfs_cleanup(void);
extern int ceph_debugfs_client_init(struct ceph_client *client);
extern void ceph_debugfs_client_cleanup(struct ceph_client *client);

/* locks.c */
extern int ceph_lock(struct file *file, int cmd, struct file_lock *fl);
extern int ceph_flock(struct file *file, int cmd, struct file_lock *fl);
extern void ceph_count_locks(struct inode *inode, int *p_num, int *f_num);
extern int ceph_encode_locks(struct inode *i, struct ceph_pagelist *p,
			     int p_locks, int f_locks);
extern int lock_to_ceph_filelock(struct file_lock *fl, struct ceph_filelock *c);

static inline struct inode *get_dentry_parent_inode(struct dentry *dentry)
{
	if (dentry && dentry->d_parent)
		return dentry->d_parent->d_inode;

	return NULL;
}

#endif /* _FS_CEPH_SUPER_H */
