#include "ceph_debug.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/writeback.h>

#include "super.h"
#include "decode.h"
#include "messenger.h"

/*
 * Capability management
 *
 * The Ceph metadata servers control client access to inode metadata
 * and file data by issuing capabilities, granting clients permission
 * to read and/or write both inode field and file data to OSDs
 * (storage nodes).  Each capability consists of a set of bits
 * indicating which operations are allowed.
 *
 * If the client holds a *_SHARED cap, the client has a coherent value
 * that can be safely read from the cached inode.
 *
 * In the case of a *_EXCL (exclusive) or FILE_WR capabilities, the
 * client is allowed to change inode attributes (e.g., file size,
 * mtime), note its dirty state in the ceph_cap, and asynchronously
 * flush that metadata change to the MDS.
 *
 * In the event of a conflicting operation (perhaps by another
 * client), the MDS will revoke the conflicting client capabilities.
 *
 * In order for a client to cache an inode, it must hold a capability
 * with at least one MDS server.  When inodes are released, release
 * notifications are batched and periodically sent en masse to the MDS
 * cluster to release server state.
 */


/*
 * Generate readable cap strings for debugging output.
 */
#define MAX_CAP_STR 20
static char cap_str[MAX_CAP_STR][40];
static DEFINE_SPINLOCK(cap_str_lock);
static int last_cap_str;

static char *gcap_string(char *s, int c)
{
	if (c & CEPH_CAP_GSHARED)
		*s++ = 's';
	if (c & CEPH_CAP_GEXCL)
		*s++ = 'x';
	if (c & CEPH_CAP_GCACHE)
		*s++ = 'c';
	if (c & CEPH_CAP_GRD)
		*s++ = 'r';
	if (c & CEPH_CAP_GWR)
		*s++ = 'w';
	if (c & CEPH_CAP_GBUFFER)
		*s++ = 'b';
	if (c & CEPH_CAP_GLAZYIO)
		*s++ = 'l';
	return s;
}

const char *ceph_cap_string(int caps)
{
	int i;
	char *s;
	int c;

	spin_lock(&cap_str_lock);
	i = last_cap_str++;
	if (last_cap_str == MAX_CAP_STR)
		last_cap_str = 0;
	spin_unlock(&cap_str_lock);

	s = cap_str[i];

	if (caps & CEPH_CAP_PIN)
		*s++ = 'p';

	c = (caps >> CEPH_CAP_SAUTH) & 3;
	if (c) {
		*s++ = 'A';
		s = gcap_string(s, c);
	}

	c = (caps >> CEPH_CAP_SLINK) & 3;
	if (c) {
		*s++ = 'L';
		s = gcap_string(s, c);
	}

	c = (caps >> CEPH_CAP_SXATTR) & 3;
	if (c) {
		*s++ = 'X';
		s = gcap_string(s, c);
	}

	c = caps >> CEPH_CAP_SFILE;
	if (c) {
		*s++ = 'F';
		s = gcap_string(s, c);
	}

	if (s == cap_str[i])
		*s++ = '-';
	*s = 0;
	return cap_str[i];
}

void ceph_caps_init(struct ceph_mds_client *mdsc)
{
	INIT_LIST_HEAD(&mdsc->caps_list);
	spin_lock_init(&mdsc->caps_list_lock);
}

void ceph_caps_finalize(struct ceph_mds_client *mdsc)
{
	struct ceph_cap *cap;

	spin_lock(&mdsc->caps_list_lock);
	while (!list_empty(&mdsc->caps_list)) {
		cap = list_first_entry(&mdsc->caps_list,
				       struct ceph_cap, caps_item);
		list_del(&cap->caps_item);
		kmem_cache_free(ceph_cap_cachep, cap);
	}
	mdsc->caps_total_count = 0;
	mdsc->caps_avail_count = 0;
	mdsc->caps_use_count = 0;
	mdsc->caps_reserve_count = 0;
	mdsc->caps_min_count = 0;
	spin_unlock(&mdsc->caps_list_lock);
}

void ceph_adjust_min_caps(struct ceph_mds_client *mdsc, int delta)
{
	spin_lock(&mdsc->caps_list_lock);
	mdsc->caps_min_count += delta;
	BUG_ON(mdsc->caps_min_count < 0);
	spin_unlock(&mdsc->caps_list_lock);
}

int ceph_reserve_caps(struct ceph_mds_client *mdsc,
		      struct ceph_cap_reservation *ctx, int need)
{
	int i;
	struct ceph_cap *cap;
	int have;
	int alloc = 0;
	LIST_HEAD(newcaps);
	int ret = 0;

	dout("reserve caps ctx=%p need=%d\n", ctx, need);

	/* first reserve any caps that are already allocated */
	spin_lock(&mdsc->caps_list_lock);
	if (mdsc->caps_avail_count >= need)
		have = need;
	else
		have = mdsc->caps_avail_count;
	mdsc->caps_avail_count -= have;
	mdsc->caps_reserve_count += have;
	BUG_ON(mdsc->caps_total_count != mdsc->caps_use_count +
					 mdsc->caps_reserve_count +
					 mdsc->caps_avail_count);
	spin_unlock(&mdsc->caps_list_lock);

	for (i = have; i < need; i++) {
		cap = kmem_cache_alloc(ceph_cap_cachep, GFP_NOFS);
		if (!cap) {
			ret = -ENOMEM;
			goto out_alloc_count;
		}
		list_add(&cap->caps_item, &newcaps);
		alloc++;
	}
	BUG_ON(have + alloc != need);

	spin_lock(&mdsc->caps_list_lock);
	mdsc->caps_total_count += alloc;
	mdsc->caps_reserve_count += alloc;
	list_splice(&newcaps, &mdsc->caps_list);

	BUG_ON(mdsc->caps_total_count != mdsc->caps_use_count +
					 mdsc->caps_reserve_count +
					 mdsc->caps_avail_count);
	spin_unlock(&mdsc->caps_list_lock);

	ctx->count = need;
	dout("reserve caps ctx=%p %d = %d used + %d resv + %d avail\n",
	     ctx, mdsc->caps_total_count, mdsc->caps_use_count,
	     mdsc->caps_reserve_count, mdsc->caps_avail_count);
	return 0;

out_alloc_count:
	/* we didn't manage to reserve as much as we needed */
	pr_warning("reserve caps ctx=%p ENOMEM need=%d got=%d\n",
		   ctx, need, have);
	return ret;
}

int ceph_unreserve_caps(struct ceph_mds_client *mdsc,
			struct ceph_cap_reservation *ctx)
{
	dout("unreserve caps ctx=%p count=%d\n", ctx, ctx->count);
	if (ctx->count) {
		spin_lock(&mdsc->caps_list_lock);
		BUG_ON(mdsc->caps_reserve_count < ctx->count);
		mdsc->caps_reserve_count -= ctx->count;
		mdsc->caps_avail_count += ctx->count;
		ctx->count = 0;
		dout("unreserve caps %d = %d used + %d resv + %d avail\n",
		     mdsc->caps_total_count, mdsc->caps_use_count,
		     mdsc->caps_reserve_count, mdsc->caps_avail_count);
		BUG_ON(mdsc->caps_total_count != mdsc->caps_use_count +
						 mdsc->caps_reserve_count +
						 mdsc->caps_avail_count);
		spin_unlock(&mdsc->caps_list_lock);
	}
	return 0;
}

static struct ceph_cap *get_cap(struct ceph_mds_client *mdsc,
				struct ceph_cap_reservation *ctx)
{
	struct ceph_cap *cap = NULL;

	/* temporary, until we do something about cap import/export */
	if (!ctx) {
		cap = kmem_cache_alloc(ceph_cap_cachep, GFP_NOFS);
		if (cap) {
			mdsc->caps_use_count++;
			mdsc->caps_total_count++;
		}
		return cap;
	}

	spin_lock(&mdsc->caps_list_lock);
	dout("get_cap ctx=%p (%d) %d = %d used + %d resv + %d avail\n",
	     ctx, ctx->count, mdsc->caps_total_count, mdsc->caps_use_count,
	     mdsc->caps_reserve_count, mdsc->caps_avail_count);
	BUG_ON(!ctx->count);
	BUG_ON(ctx->count > mdsc->caps_reserve_count);
	BUG_ON(list_empty(&mdsc->caps_list));

	ctx->count--;
	mdsc->caps_reserve_count--;
	mdsc->caps_use_count++;

	cap = list_first_entry(&mdsc->caps_list, struct ceph_cap, caps_item);
	list_del(&cap->caps_item);

	BUG_ON(mdsc->caps_total_count != mdsc->caps_use_count +
	       mdsc->caps_reserve_count + mdsc->caps_avail_count);
	spin_unlock(&mdsc->caps_list_lock);
	return cap;
}

void ceph_put_cap(struct ceph_mds_client *mdsc, struct ceph_cap *cap)
{
	spin_lock(&mdsc->caps_list_lock);
	dout("put_cap %p %d = %d used + %d resv + %d avail\n",
	     cap, mdsc->caps_total_count, mdsc->caps_use_count,
	     mdsc->caps_reserve_count, mdsc->caps_avail_count);
	mdsc->caps_use_count--;
	/*
	 * Keep some preallocated caps around (ceph_min_count), to
	 * avoid lots of free/alloc churn.
	 */
	if (mdsc->caps_avail_count >= mdsc->caps_reserve_count +
				      mdsc->caps_min_count) {
		mdsc->caps_total_count--;
		kmem_cache_free(ceph_cap_cachep, cap);
	} else {
		mdsc->caps_avail_count++;
		list_add(&cap->caps_item, &mdsc->caps_list);
	}

	BUG_ON(mdsc->caps_total_count != mdsc->caps_use_count +
	       mdsc->caps_reserve_count + mdsc->caps_avail_count);
	spin_unlock(&mdsc->caps_list_lock);
}

void ceph_reservation_status(struct ceph_client *client,
			     int *total, int *avail, int *used, int *reserved,
			     int *min)
{
	struct ceph_mds_client *mdsc = &client->mdsc;

	if (total)
		*total = mdsc->caps_total_count;
	if (avail)
		*avail = mdsc->caps_avail_count;
	if (used)
		*used = mdsc->caps_use_count;
	if (reserved)
		*reserved = mdsc->caps_reserve_count;
	if (min)
		*min = mdsc->caps_min_count;
}

/*
 * Find ceph_cap for given mds, if any.
 *
 * Called with i_lock held.
 */
static struct ceph_cap *__get_cap_for_mds(struct ceph_inode_info *ci, int mds)
{
	struct ceph_cap *cap;
	struct rb_node *n = ci->i_caps.rb_node;

	while (n) {
		cap = rb_entry(n, struct ceph_cap, ci_node);
		if (mds < cap->mds)
			n = n->rb_left;
		else if (mds > cap->mds)
			n = n->rb_right;
		else
			return cap;
	}
	return NULL;
}

struct ceph_cap *ceph_get_cap_for_mds(struct ceph_inode_info *ci, int mds)
{
	struct ceph_cap *cap;

	spin_lock(&ci->vfs_inode.i_lock);
	cap = __get_cap_for_mds(ci, mds);
	spin_unlock(&ci->vfs_inode.i_lock);
	return cap;
}

/*
 * Return id of any MDS with a cap, preferably FILE_WR|BUFFER|EXCL, else -1.
 */
static int __ceph_get_cap_mds(struct ceph_inode_info *ci)
{
	struct ceph_cap *cap;
	int mds = -1;
	struct rb_node *p;

	/* prefer mds with WR|BUFFER|EXCL caps */
	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		mds = cap->mds;
		if (cap->issued & (CEPH_CAP_FILE_WR |
				   CEPH_CAP_FILE_BUFFER |
				   CEPH_CAP_FILE_EXCL))
			break;
	}
	return mds;
}

int ceph_get_cap_mds(struct inode *inode)
{
	int mds;
	spin_lock(&inode->i_lock);
	mds = __ceph_get_cap_mds(ceph_inode(inode));
	spin_unlock(&inode->i_lock);
	return mds;
}

/*
 * Called under i_lock.
 */
static void __insert_cap_node(struct ceph_inode_info *ci,
			      struct ceph_cap *new)
{
	struct rb_node **p = &ci->i_caps.rb_node;
	struct rb_node *parent = NULL;
	struct ceph_cap *cap = NULL;

	while (*p) {
		parent = *p;
		cap = rb_entry(parent, struct ceph_cap, ci_node);
		if (new->mds < cap->mds)
			p = &(*p)->rb_left;
		else if (new->mds > cap->mds)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&new->ci_node, parent, p);
	rb_insert_color(&new->ci_node, &ci->i_caps);
}

/*
 * (re)set cap hold timeouts, which control the delayed release
 * of unused caps back to the MDS.  Should be called on cap use.
 */
static void __cap_set_timeouts(struct ceph_mds_client *mdsc,
			       struct ceph_inode_info *ci)
{
	struct ceph_mount_args *ma = mdsc->client->mount_args;

	ci->i_hold_caps_min = round_jiffies(jiffies +
					    ma->caps_wanted_delay_min * HZ);
	ci->i_hold_caps_max = round_jiffies(jiffies +
					    ma->caps_wanted_delay_max * HZ);
	dout("__cap_set_timeouts %p min %lu max %lu\n", &ci->vfs_inode,
	     ci->i_hold_caps_min - jiffies, ci->i_hold_caps_max - jiffies);
}

/*
 * (Re)queue cap at the end of the delayed cap release list.
 *
 * If I_FLUSH is set, leave the inode at the front of the list.
 *
 * Caller holds i_lock
 *    -> we take mdsc->cap_delay_lock
 */
static void __cap_delay_requeue(struct ceph_mds_client *mdsc,
				struct ceph_inode_info *ci)
{
	__cap_set_timeouts(mdsc, ci);
	dout("__cap_delay_requeue %p flags %d at %lu\n", &ci->vfs_inode,
	     ci->i_ceph_flags, ci->i_hold_caps_max);
	if (!mdsc->stopping) {
		spin_lock(&mdsc->cap_delay_lock);
		if (!list_empty(&ci->i_cap_delay_list)) {
			if (ci->i_ceph_flags & CEPH_I_FLUSH)
				goto no_change;
			list_del_init(&ci->i_cap_delay_list);
		}
		list_add_tail(&ci->i_cap_delay_list, &mdsc->cap_delay_list);
no_change:
		spin_unlock(&mdsc->cap_delay_lock);
	}
}

/*
 * Queue an inode for immediate writeback.  Mark inode with I_FLUSH,
 * indicating we should send a cap message to flush dirty metadata
 * asap, and move to the front of the delayed cap list.
 */
static void __cap_delay_requeue_front(struct ceph_mds_client *mdsc,
				      struct ceph_inode_info *ci)
{
	dout("__cap_delay_requeue_front %p\n", &ci->vfs_inode);
	spin_lock(&mdsc->cap_delay_lock);
	ci->i_ceph_flags |= CEPH_I_FLUSH;
	if (!list_empty(&ci->i_cap_delay_list))
		list_del_init(&ci->i_cap_delay_list);
	list_add(&ci->i_cap_delay_list, &mdsc->cap_delay_list);
	spin_unlock(&mdsc->cap_delay_lock);
}

/*
 * Cancel delayed work on cap.
 *
 * Caller must hold i_lock.
 */
static void __cap_delay_cancel(struct ceph_mds_client *mdsc,
			       struct ceph_inode_info *ci)
{
	dout("__cap_delay_cancel %p\n", &ci->vfs_inode);
	if (list_empty(&ci->i_cap_delay_list))
		return;
	spin_lock(&mdsc->cap_delay_lock);
	list_del_init(&ci->i_cap_delay_list);
	spin_unlock(&mdsc->cap_delay_lock);
}

/*
 * Common issue checks for add_cap, handle_cap_grant.
 */
static void __check_cap_issue(struct ceph_inode_info *ci, struct ceph_cap *cap,
			      unsigned issued)
{
	unsigned had = __ceph_caps_issued(ci, NULL);

	/*
	 * Each time we receive FILE_CACHE anew, we increment
	 * i_rdcache_gen.
	 */
	if ((issued & (CEPH_CAP_FILE_CACHE|CEPH_CAP_FILE_LAZYIO)) &&
	    (had & (CEPH_CAP_FILE_CACHE|CEPH_CAP_FILE_LAZYIO)) == 0)
		ci->i_rdcache_gen++;

	/*
	 * if we are newly issued FILE_SHARED, clear I_COMPLETE; we
	 * don't know what happened to this directory while we didn't
	 * have the cap.
	 */
	if ((issued & CEPH_CAP_FILE_SHARED) &&
	    (had & CEPH_CAP_FILE_SHARED) == 0) {
		ci->i_shared_gen++;
		if (S_ISDIR(ci->vfs_inode.i_mode)) {
			dout(" marking %p NOT complete\n", &ci->vfs_inode);
			ci->i_ceph_flags &= ~CEPH_I_COMPLETE;
		}
	}
}

/*
 * Add a capability under the given MDS session.
 *
 * Caller should hold session snap_rwsem (read) and s_mutex.
 *
 * @fmode is the open file mode, if we are opening a file, otherwise
 * it is < 0.  (This is so we can atomically add the cap and add an
 * open file reference to it.)
 */
int ceph_add_cap(struct inode *inode,
		 struct ceph_mds_session *session, u64 cap_id,
		 int fmode, unsigned issued, unsigned wanted,
		 unsigned seq, unsigned mseq, u64 realmino, int flags,
		 struct ceph_cap_reservation *caps_reservation)
{
	struct ceph_mds_client *mdsc = &ceph_inode_to_client(inode)->mdsc;
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct ceph_cap *new_cap = NULL;
	struct ceph_cap *cap;
	int mds = session->s_mds;
	int actual_wanted;

	dout("add_cap %p mds%d cap %llx %s seq %d\n", inode,
	     session->s_mds, cap_id, ceph_cap_string(issued), seq);

	/*
	 * If we are opening the file, include file mode wanted bits
	 * in wanted.
	 */
	if (fmode >= 0)
		wanted |= ceph_caps_for_mode(fmode);

retry:
	spin_lock(&inode->i_lock);
	cap = __get_cap_for_mds(ci, mds);
	if (!cap) {
		if (new_cap) {
			cap = new_cap;
			new_cap = NULL;
		} else {
			spin_unlock(&inode->i_lock);
			new_cap = get_cap(mdsc, caps_reservation);
			if (new_cap == NULL)
				return -ENOMEM;
			goto retry;
		}

		cap->issued = 0;
		cap->implemented = 0;
		cap->mds = mds;
		cap->mds_wanted = 0;

		cap->ci = ci;
		__insert_cap_node(ci, cap);

		/* clear out old exporting info?  (i.e. on cap import) */
		if (ci->i_cap_exporting_mds == mds) {
			ci->i_cap_exporting_issued = 0;
			ci->i_cap_exporting_mseq = 0;
			ci->i_cap_exporting_mds = -1;
		}

		/* add to session cap list */
		cap->session = session;
		spin_lock(&session->s_cap_lock);
		list_add_tail(&cap->session_caps, &session->s_caps);
		session->s_nr_caps++;
		spin_unlock(&session->s_cap_lock);
	}

	if (!ci->i_snap_realm) {
		/*
		 * add this inode to the appropriate snap realm
		 */
		struct ceph_snap_realm *realm = ceph_lookup_snap_realm(mdsc,
							       realmino);
		if (realm) {
			ceph_get_snap_realm(mdsc, realm);
			spin_lock(&realm->inodes_with_caps_lock);
			ci->i_snap_realm = realm;
			list_add(&ci->i_snap_realm_item,
				 &realm->inodes_with_caps);
			spin_unlock(&realm->inodes_with_caps_lock);
		} else {
			pr_err("ceph_add_cap: couldn't find snap realm %llx\n",
			       realmino);
			WARN_ON(!realm);
		}
	}

	__check_cap_issue(ci, cap, issued);

	/*
	 * If we are issued caps we don't want, or the mds' wanted
	 * value appears to be off, queue a check so we'll release
	 * later and/or update the mds wanted value.
	 */
	actual_wanted = __ceph_caps_wanted(ci);
	if ((wanted & ~actual_wanted) ||
	    (issued & ~actual_wanted & CEPH_CAP_ANY_WR)) {
		dout(" issued %s, mds wanted %s, actual %s, queueing\n",
		     ceph_cap_string(issued), ceph_cap_string(wanted),
		     ceph_cap_string(actual_wanted));
		__cap_delay_requeue(mdsc, ci);
	}

	if (flags & CEPH_CAP_FLAG_AUTH)
		ci->i_auth_cap = cap;
	else if (ci->i_auth_cap == cap)
		ci->i_auth_cap = NULL;

	dout("add_cap inode %p (%llx.%llx) cap %p %s now %s seq %d mds%d\n",
	     inode, ceph_vinop(inode), cap, ceph_cap_string(issued),
	     ceph_cap_string(issued|cap->issued), seq, mds);
	cap->cap_id = cap_id;
	cap->issued = issued;
	cap->implemented |= issued;
	cap->mds_wanted |= wanted;
	cap->seq = seq;
	cap->issue_seq = seq;
	cap->mseq = mseq;
	cap->cap_gen = session->s_cap_gen;

	if (fmode >= 0)
		__ceph_get_fmode(ci, fmode);
	spin_unlock(&inode->i_lock);
	wake_up_all(&ci->i_cap_wq);
	return 0;
}

/*
 * Return true if cap has not timed out and belongs to the current
 * generation of the MDS session (i.e. has not gone 'stale' due to
 * us losing touch with the mds).
 */
static int __cap_is_valid(struct ceph_cap *cap)
{
	unsigned long ttl;
	u32 gen;

	spin_lock(&cap->session->s_cap_lock);
	gen = cap->session->s_cap_gen;
	ttl = cap->session->s_cap_ttl;
	spin_unlock(&cap->session->s_cap_lock);

	if (cap->cap_gen < gen || time_after_eq(jiffies, ttl)) {
		dout("__cap_is_valid %p cap %p issued %s "
		     "but STALE (gen %u vs %u)\n", &cap->ci->vfs_inode,
		     cap, ceph_cap_string(cap->issued), cap->cap_gen, gen);
		return 0;
	}

	return 1;
}

/*
 * Return set of valid cap bits issued to us.  Note that caps time
 * out, and may be invalidated in bulk if the client session times out
 * and session->s_cap_gen is bumped.
 */
int __ceph_caps_issued(struct ceph_inode_info *ci, int *implemented)
{
	int have = ci->i_snap_caps | ci->i_cap_exporting_issued;
	struct ceph_cap *cap;
	struct rb_node *p;

	if (implemented)
		*implemented = 0;
	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		if (!__cap_is_valid(cap))
			continue;
		dout("__ceph_caps_issued %p cap %p issued %s\n",
		     &ci->vfs_inode, cap, ceph_cap_string(cap->issued));
		have |= cap->issued;
		if (implemented)
			*implemented |= cap->implemented;
	}
	return have;
}

/*
 * Get cap bits issued by caps other than @ocap
 */
int __ceph_caps_issued_other(struct ceph_inode_info *ci, struct ceph_cap *ocap)
{
	int have = ci->i_snap_caps;
	struct ceph_cap *cap;
	struct rb_node *p;

	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		if (cap == ocap)
			continue;
		if (!__cap_is_valid(cap))
			continue;
		have |= cap->issued;
	}
	return have;
}

/*
 * Move a cap to the end of the LRU (oldest caps at list head, newest
 * at list tail).
 */
static void __touch_cap(struct ceph_cap *cap)
{
	struct ceph_mds_session *s = cap->session;

	spin_lock(&s->s_cap_lock);
	if (s->s_cap_iterator == NULL) {
		dout("__touch_cap %p cap %p mds%d\n", &cap->ci->vfs_inode, cap,
		     s->s_mds);
		list_move_tail(&cap->session_caps, &s->s_caps);
	} else {
		dout("__touch_cap %p cap %p mds%d NOP, iterating over caps\n",
		     &cap->ci->vfs_inode, cap, s->s_mds);
	}
	spin_unlock(&s->s_cap_lock);
}

/*
 * Check if we hold the given mask.  If so, move the cap(s) to the
 * front of their respective LRUs.  (This is the preferred way for
 * callers to check for caps they want.)
 */
int __ceph_caps_issued_mask(struct ceph_inode_info *ci, int mask, int touch)
{
	struct ceph_cap *cap;
	struct rb_node *p;
	int have = ci->i_snap_caps;

	if ((have & mask) == mask) {
		dout("__ceph_caps_issued_mask %p snap issued %s"
		     " (mask %s)\n", &ci->vfs_inode,
		     ceph_cap_string(have),
		     ceph_cap_string(mask));
		return 1;
	}

	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		if (!__cap_is_valid(cap))
			continue;
		if ((cap->issued & mask) == mask) {
			dout("__ceph_caps_issued_mask %p cap %p issued %s"
			     " (mask %s)\n", &ci->vfs_inode, cap,
			     ceph_cap_string(cap->issued),
			     ceph_cap_string(mask));
			if (touch)
				__touch_cap(cap);
			return 1;
		}

		/* does a combination of caps satisfy mask? */
		have |= cap->issued;
		if ((have & mask) == mask) {
			dout("__ceph_caps_issued_mask %p combo issued %s"
			     " (mask %s)\n", &ci->vfs_inode,
			     ceph_cap_string(cap->issued),
			     ceph_cap_string(mask));
			if (touch) {
				struct rb_node *q;

				/* touch this + preceeding caps */
				__touch_cap(cap);
				for (q = rb_first(&ci->i_caps); q != p;
				     q = rb_next(q)) {
					cap = rb_entry(q, struct ceph_cap,
						       ci_node);
					if (!__cap_is_valid(cap))
						continue;
					__touch_cap(cap);
				}
			}
			return 1;
		}
	}

	return 0;
}

/*
 * Return true if mask caps are currently being revoked by an MDS.
 */
int ceph_caps_revoking(struct ceph_inode_info *ci, int mask)
{
	struct inode *inode = &ci->vfs_inode;
	struct ceph_cap *cap;
	struct rb_node *p;
	int ret = 0;

	spin_lock(&inode->i_lock);
	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		if (__cap_is_valid(cap) &&
		    (cap->implemented & ~cap->issued & mask)) {
			ret = 1;
			break;
		}
	}
	spin_unlock(&inode->i_lock);
	dout("ceph_caps_revoking %p %s = %d\n", inode,
	     ceph_cap_string(mask), ret);
	return ret;
}

int __ceph_caps_used(struct ceph_inode_info *ci)
{
	int used = 0;
	if (ci->i_pin_ref)
		used |= CEPH_CAP_PIN;
	if (ci->i_rd_ref)
		used |= CEPH_CAP_FILE_RD;
	if (ci->i_rdcache_ref || ci->vfs_inode.i_data.nrpages)
		used |= CEPH_CAP_FILE_CACHE;
	if (ci->i_wr_ref)
		used |= CEPH_CAP_FILE_WR;
	if (ci->i_wrbuffer_ref)
		used |= CEPH_CAP_FILE_BUFFER;
	return used;
}

/*
 * wanted, by virtue of open file modes
 */
int __ceph_caps_file_wanted(struct ceph_inode_info *ci)
{
	int want = 0;
	int mode;
	for (mode = 0; mode < CEPH_FILE_MODE_NUM; mode++)
		if (ci->i_nr_by_mode[mode])
			want |= ceph_caps_for_mode(mode);
	return want;
}

/*
 * Return caps we have registered with the MDS(s) as 'wanted'.
 */
int __ceph_caps_mds_wanted(struct ceph_inode_info *ci)
{
	struct ceph_cap *cap;
	struct rb_node *p;
	int mds_wanted = 0;

	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		if (!__cap_is_valid(cap))
			continue;
		mds_wanted |= cap->mds_wanted;
	}
	return mds_wanted;
}

/*
 * called under i_lock
 */
static int __ceph_is_any_caps(struct ceph_inode_info *ci)
{
	return !RB_EMPTY_ROOT(&ci->i_caps) || ci->i_cap_exporting_mds >= 0;
}

/*
 * Remove a cap.  Take steps to deal with a racing iterate_session_caps.
 *
 * caller should hold i_lock.
 * caller will not hold session s_mutex if called from destroy_inode.
 */
void __ceph_remove_cap(struct ceph_cap *cap)
{
	struct ceph_mds_session *session = cap->session;
	struct ceph_inode_info *ci = cap->ci;
	struct ceph_mds_client *mdsc =
		&ceph_sb_to_client(ci->vfs_inode.i_sb)->mdsc;
	int removed = 0;

	dout("__ceph_remove_cap %p from %p\n", cap, &ci->vfs_inode);

	/* remove from session list */
	spin_lock(&session->s_cap_lock);
	if (session->s_cap_iterator == cap) {
		/* not yet, we are iterating over this very cap */
		dout("__ceph_remove_cap  delaying %p removal from session %p\n",
		     cap, cap->session);
	} else {
		list_del_init(&cap->session_caps);
		session->s_nr_caps--;
		cap->session = NULL;
		removed = 1;
	}
	/* protect backpointer with s_cap_lock: see iterate_session_caps */
	cap->ci = NULL;
	spin_unlock(&session->s_cap_lock);

	/* remove from inode list */
	rb_erase(&cap->ci_node, &ci->i_caps);
	if (ci->i_auth_cap == cap)
		ci->i_auth_cap = NULL;

	if (removed)
		ceph_put_cap(mdsc, cap);

	if (!__ceph_is_any_caps(ci) && ci->i_snap_realm) {
		struct ceph_snap_realm *realm = ci->i_snap_realm;
		spin_lock(&realm->inodes_with_caps_lock);
		list_del_init(&ci->i_snap_realm_item);
		ci->i_snap_realm_counter++;
		ci->i_snap_realm = NULL;
		spin_unlock(&realm->inodes_with_caps_lock);
		ceph_put_snap_realm(mdsc, realm);
	}
	if (!__ceph_is_any_real_caps(ci))
		__cap_delay_cancel(mdsc, ci);
}

/*
 * Build and send a cap message to the given MDS.
 *
 * Caller should be holding s_mutex.
 */
static int send_cap_msg(struct ceph_mds_session *session,
			u64 ino, u64 cid, int op,
			int caps, int wanted, int dirty,
			u32 seq, u64 flush_tid, u32 issue_seq, u32 mseq,
			u64 size, u64 max_size,
			struct timespec *mtime, struct timespec *atime,
			u64 time_warp_seq,
			uid_t uid, gid_t gid, mode_t mode,
			u64 xattr_version,
			struct ceph_buffer *xattrs_buf,
			u64 follows)
{
	struct ceph_mds_caps *fc;
	struct ceph_msg *msg;

	dout("send_cap_msg %s %llx %llx caps %s wanted %s dirty %s"
	     " seq %u/%u mseq %u follows %lld size %llu/%llu"
	     " xattr_ver %llu xattr_len %d\n", ceph_cap_op_name(op),
	     cid, ino, ceph_cap_string(caps), ceph_cap_string(wanted),
	     ceph_cap_string(dirty),
	     seq, issue_seq, mseq, follows, size, max_size,
	     xattr_version, xattrs_buf ? (int)xattrs_buf->vec.iov_len : 0);

	msg = ceph_msg_new(CEPH_MSG_CLIENT_CAPS, sizeof(*fc), GFP_NOFS);
	if (!msg)
		return -ENOMEM;

	msg->hdr.tid = cpu_to_le64(flush_tid);

	fc = msg->front.iov_base;
	memset(fc, 0, sizeof(*fc));

	fc->cap_id = cpu_to_le64(cid);
	fc->op = cpu_to_le32(op);
	fc->seq = cpu_to_le32(seq);
	fc->issue_seq = cpu_to_le32(issue_seq);
	fc->migrate_seq = cpu_to_le32(mseq);
	fc->caps = cpu_to_le32(caps);
	fc->wanted = cpu_to_le32(wanted);
	fc->dirty = cpu_to_le32(dirty);
	fc->ino = cpu_to_le64(ino);
	fc->snap_follows = cpu_to_le64(follows);

	fc->size = cpu_to_le64(size);
	fc->max_size = cpu_to_le64(max_size);
	if (mtime)
		ceph_encode_timespec(&fc->mtime, mtime);
	if (atime)
		ceph_encode_timespec(&fc->atime, atime);
	fc->time_warp_seq = cpu_to_le32(time_warp_seq);

	fc->uid = cpu_to_le32(uid);
	fc->gid = cpu_to_le32(gid);
	fc->mode = cpu_to_le32(mode);

	fc->xattr_version = cpu_to_le64(xattr_version);
	if (xattrs_buf) {
		msg->middle = ceph_buffer_get(xattrs_buf);
		fc->xattr_len = cpu_to_le32(xattrs_buf->vec.iov_len);
		msg->hdr.middle_len = cpu_to_le32(xattrs_buf->vec.iov_len);
	}

	ceph_con_send(&session->s_con, msg);
	return 0;
}

static void __queue_cap_release(struct ceph_mds_session *session,
				u64 ino, u64 cap_id, u32 migrate_seq,
				u32 issue_seq)
{
	struct ceph_msg *msg;
	struct ceph_mds_cap_release *head;
	struct ceph_mds_cap_item *item;

	spin_lock(&session->s_cap_lock);
	BUG_ON(!session->s_num_cap_releases);
	msg = list_first_entry(&session->s_cap_releases,
			       struct ceph_msg, list_head);

	dout(" adding %llx release to mds%d msg %p (%d left)\n",
	     ino, session->s_mds, msg, session->s_num_cap_releases);

	BUG_ON(msg->front.iov_len + sizeof(*item) > PAGE_CACHE_SIZE);
	head = msg->front.iov_base;
	head->num = cpu_to_le32(le32_to_cpu(head->num) + 1);
	item = msg->front.iov_base + msg->front.iov_len;
	item->ino = cpu_to_le64(ino);
	item->cap_id = cpu_to_le64(cap_id);
	item->migrate_seq = cpu_to_le32(migrate_seq);
	item->seq = cpu_to_le32(issue_seq);

	session->s_num_cap_releases--;

	msg->front.iov_len += sizeof(*item);
	if (le32_to_cpu(head->num) == CEPH_CAPS_PER_RELEASE) {
		dout(" release msg %p full\n", msg);
		list_move_tail(&msg->list_head, &session->s_cap_releases_done);
	} else {
		dout(" release msg %p at %d/%d (%d)\n", msg,
		     (int)le32_to_cpu(head->num),
		     (int)CEPH_CAPS_PER_RELEASE,
		     (int)msg->front.iov_len);
	}
	spin_unlock(&session->s_cap_lock);
}

/*
 * Queue cap releases when an inode is dropped from our cache.  Since
 * inode is about to be destroyed, there is no need for i_lock.
 */
void ceph_queue_caps_release(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct rb_node *p;

	p = rb_first(&ci->i_caps);
	while (p) {
		struct ceph_cap *cap = rb_entry(p, struct ceph_cap, ci_node);
		struct ceph_mds_session *session = cap->session;

		__queue_cap_release(session, ceph_ino(inode), cap->cap_id,
				    cap->mseq, cap->issue_seq);
		p = rb_next(p);
		__ceph_remove_cap(cap);
	}
}

/*
 * Send a cap msg on the given inode.  Update our caps state, then
 * drop i_lock and send the message.
 *
 * Make note of max_size reported/requested from mds, revoked caps
 * that have now been implemented.
 *
 * Make half-hearted attempt ot to invalidate page cache if we are
 * dropping RDCACHE.  Note that this will leave behind locked pages
 * that we'll then need to deal with elsewhere.
 *
 * Return non-zero if delayed release, or we experienced an error
 * such that the caller should requeue + retry later.
 *
 * called with i_lock, then drops it.
 * caller should hold snap_rwsem (read), s_mutex.
 */
static int __send_cap(struct ceph_mds_client *mdsc, struct ceph_cap *cap,
		      int op, int used, int want, int retain, int flushing,
		      unsigned *pflush_tid)
	__releases(cap->ci->vfs_inode->i_lock)
{
	struct ceph_inode_info *ci = cap->ci;
	struct inode *inode = &ci->vfs_inode;
	u64 cap_id = cap->cap_id;
	int held, revoking, dropping, keep;
	u64 seq, issue_seq, mseq, time_warp_seq, follows;
	u64 size, max_size;
	struct timespec mtime, atime;
	int wake = 0;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	struct ceph_mds_session *session;
	u64 xattr_version = 0;
	struct ceph_buffer *xattr_blob = NULL;
	int delayed = 0;
	u64 flush_tid = 0;
	int i;
	int ret;

	held = cap->issued | cap->implemented;
	revoking = cap->implemented & ~cap->issued;
	retain &= ~revoking;
	dropping = cap->issued & ~retain;

	dout("__send_cap %p cap %p session %p %s -> %s (revoking %s)\n",
	     inode, cap, cap->session,
	     ceph_cap_string(held), ceph_cap_string(held & retain),
	     ceph_cap_string(revoking));
	BUG_ON((retain & CEPH_CAP_PIN) == 0);

	session = cap->session;

	/* don't release wanted unless we've waited a bit. */
	if ((ci->i_ceph_flags & CEPH_I_NODELAY) == 0 &&
	    time_before(jiffies, ci->i_hold_caps_min)) {
		dout(" delaying issued %s -> %s, wanted %s -> %s on send\n",
		     ceph_cap_string(cap->issued),
		     ceph_cap_string(cap->issued & retain),
		     ceph_cap_string(cap->mds_wanted),
		     ceph_cap_string(want));
		want |= cap->mds_wanted;
		retain |= cap->issued;
		delayed = 1;
	}
	ci->i_ceph_flags &= ~(CEPH_I_NODELAY | CEPH_I_FLUSH);

	cap->issued &= retain;  /* drop bits we don't want */
	if (cap->implemented & ~cap->issued) {
		/*
		 * Wake up any waiters on wanted -> needed transition.
		 * This is due to the weird transition from buffered
		 * to sync IO... we need to flush dirty pages _before_
		 * allowing sync writes to avoid reordering.
		 */
		wake = 1;
	}
	cap->implemented &= cap->issued | used;
	cap->mds_wanted = want;

	if (flushing) {
		/*
		 * assign a tid for flush operations so we can avoid
		 * flush1 -> dirty1 -> flush2 -> flushack1 -> mark
		 * clean type races.  track latest tid for every bit
		 * so we can handle flush AxFw, flush Fw, and have the
		 * first ack clean Ax.
		 */
		flush_tid = ++ci->i_cap_flush_last_tid;
		if (pflush_tid)
			*pflush_tid = flush_tid;
		dout(" cap_flush_tid %d\n", (int)flush_tid);
		for (i = 0; i < CEPH_CAP_BITS; i++)
			if (flushing & (1 << i))
				ci->i_cap_flush_tid[i] = flush_tid;

		follows = ci->i_head_snapc->seq;
	} else {
		follows = 0;
	}

	keep = cap->implemented;
	seq = cap->seq;
	issue_seq = cap->issue_seq;
	mseq = cap->mseq;
	size = inode->i_size;
	ci->i_reported_size = size;
	max_size = ci->i_wanted_max_size;
	ci->i_requested_max_size = max_size;
	mtime = inode->i_mtime;
	atime = inode->i_atime;
	time_warp_seq = ci->i_time_warp_seq;
	uid = inode->i_uid;
	gid = inode->i_gid;
	mode = inode->i_mode;

	if (flushing & CEPH_CAP_XATTR_EXCL) {
		__ceph_build_xattrs_blob(ci);
		xattr_blob = ci->i_xattrs.blob;
		xattr_version = ci->i_xattrs.version;
	}

	spin_unlock(&inode->i_lock);

	ret = send_cap_msg(session, ceph_vino(inode).ino, cap_id,
		op, keep, want, flushing, seq, flush_tid, issue_seq, mseq,
		size, max_size, &mtime, &atime, time_warp_seq,
		uid, gid, mode, xattr_version, xattr_blob,
		follows);
	if (ret < 0) {
		dout("error sending cap msg, must requeue %p\n", inode);
		delayed = 1;
	}

	if (wake)
		wake_up_all(&ci->i_cap_wq);

	return delayed;
}

/*
 * When a snapshot is taken, clients accumulate dirty metadata on
 * inodes with capabilities in ceph_cap_snaps to describe the file
 * state at the time the snapshot was taken.  This must be flushed
 * asynchronously back to the MDS once sync writes complete and dirty
 * data is written out.
 *
 * Unless @again is true, skip cap_snaps that were already sent to
 * the MDS (i.e., during this session).
 *
 * Called under i_lock.  Takes s_mutex as needed.
 */
void __ceph_flush_snaps(struct ceph_inode_info *ci,
			struct ceph_mds_session **psession,
			int again)
		__releases(ci->vfs_inode->i_lock)
		__acquires(ci->vfs_inode->i_lock)
{
	struct inode *inode = &ci->vfs_inode;
	int mds;
	struct ceph_cap_snap *capsnap;
	u32 mseq;
	struct ceph_mds_client *mdsc = &ceph_inode_to_client(inode)->mdsc;
	struct ceph_mds_session *session = NULL; /* if session != NULL, we hold
						    session->s_mutex */
	u64 next_follows = 0;  /* keep track of how far we've gotten through the
			     i_cap_snaps list, and skip these entries next time
			     around to avoid an infinite loop */

	if (psession)
		session = *psession;

	dout("__flush_snaps %p\n", inode);
retry:
	list_for_each_entry(capsnap, &ci->i_cap_snaps, ci_item) {
		/* avoid an infiniute loop after retry */
		if (capsnap->follows < next_follows)
			continue;
		/*
		 * we need to wait for sync writes to complete and for dirty
		 * pages to be written out.
		 */
		if (capsnap->dirty_pages || capsnap->writing)
			break;

		/*
		 * if cap writeback already occurred, we should have dropped
		 * the capsnap in ceph_put_wrbuffer_cap_refs.
		 */
		BUG_ON(capsnap->dirty == 0);

		/* pick mds, take s_mutex */
		if (ci->i_auth_cap == NULL) {
			dout("no auth cap (migrating?), doing nothing\n");
			goto out;
		}

		/* only flush each capsnap once */
		if (!again && !list_empty(&capsnap->flushing_item)) {
			dout("already flushed %p, skipping\n", capsnap);
			continue;
		}

		mds = ci->i_auth_cap->session->s_mds;
		mseq = ci->i_auth_cap->mseq;

		if (session && session->s_mds != mds) {
			dout("oops, wrong session %p mutex\n", session);
			mutex_unlock(&session->s_mutex);
			ceph_put_mds_session(session);
			session = NULL;
		}
		if (!session) {
			spin_unlock(&inode->i_lock);
			mutex_lock(&mdsc->mutex);
			session = __ceph_lookup_mds_session(mdsc, mds);
			mutex_unlock(&mdsc->mutex);
			if (session) {
				dout("inverting session/ino locks on %p\n",
				     session);
				mutex_lock(&session->s_mutex);
			}
			/*
			 * if session == NULL, we raced against a cap
			 * deletion or migration.  retry, and we'll
			 * get a better @mds value next time.
			 */
			spin_lock(&inode->i_lock);
			goto retry;
		}

		capsnap->flush_tid = ++ci->i_cap_flush_last_tid;
		atomic_inc(&capsnap->nref);
		if (!list_empty(&capsnap->flushing_item))
			list_del_init(&capsnap->flushing_item);
		list_add_tail(&capsnap->flushing_item,
			      &session->s_cap_snaps_flushing);
		spin_unlock(&inode->i_lock);

		dout("flush_snaps %p cap_snap %p follows %lld tid %llu\n",
		     inode, capsnap, capsnap->follows, capsnap->flush_tid);
		send_cap_msg(session, ceph_vino(inode).ino, 0,
			     CEPH_CAP_OP_FLUSHSNAP, capsnap->issued, 0,
			     capsnap->dirty, 0, capsnap->flush_tid, 0, mseq,
			     capsnap->size, 0,
			     &capsnap->mtime, &capsnap->atime,
			     capsnap->time_warp_seq,
			     capsnap->uid, capsnap->gid, capsnap->mode,
			     capsnap->xattr_version, capsnap->xattr_blob,
			     capsnap->follows);

		next_follows = capsnap->follows + 1;
		ceph_put_cap_snap(capsnap);

		spin_lock(&inode->i_lock);
		goto retry;
	}

	/* we flushed them all; remove this inode from the queue */
	spin_lock(&mdsc->snap_flush_lock);
	list_del_init(&ci->i_snap_flush_item);
	spin_unlock(&mdsc->snap_flush_lock);

out:
	if (psession)
		*psession = session;
	else if (session) {
		mutex_unlock(&session->s_mutex);
		ceph_put_mds_session(session);
	}
}

static void ceph_flush_snaps(struct ceph_inode_info *ci)
{
	struct inode *inode = &ci->vfs_inode;

	spin_lock(&inode->i_lock);
	__ceph_flush_snaps(ci, NULL, 0);
	spin_unlock(&inode->i_lock);
}

/*
 * Mark caps dirty.  If inode is newly dirty, add to the global dirty
 * list.
 */
void __ceph_mark_dirty_caps(struct ceph_inode_info *ci, int mask)
{
	struct ceph_mds_client *mdsc =
		&ceph_sb_to_client(ci->vfs_inode.i_sb)->mdsc;
	struct inode *inode = &ci->vfs_inode;
	int was = ci->i_dirty_caps;
	int dirty = 0;

	dout("__mark_dirty_caps %p %s dirty %s -> %s\n", &ci->vfs_inode,
	     ceph_cap_string(mask), ceph_cap_string(was),
	     ceph_cap_string(was | mask));
	ci->i_dirty_caps |= mask;
	if (was == 0) {
		if (!ci->i_head_snapc)
			ci->i_head_snapc = ceph_get_snap_context(
				ci->i_snap_realm->cached_context);
		dout(" inode %p now dirty snapc %p\n", &ci->vfs_inode,
			ci->i_head_snapc);
		BUG_ON(!list_empty(&ci->i_dirty_item));
		spin_lock(&mdsc->cap_dirty_lock);
		list_add(&ci->i_dirty_item, &mdsc->cap_dirty);
		spin_unlock(&mdsc->cap_dirty_lock);
		if (ci->i_flushing_caps == 0) {
			igrab(inode);
			dirty |= I_DIRTY_SYNC;
		}
	}
	BUG_ON(list_empty(&ci->i_dirty_item));
	if (((was | ci->i_flushing_caps) & CEPH_CAP_FILE_BUFFER) &&
	    (mask & CEPH_CAP_FILE_BUFFER))
		dirty |= I_DIRTY_DATASYNC;
	if (dirty)
		__mark_inode_dirty(inode, dirty);
	__cap_delay_requeue(mdsc, ci);
}

/*
 * Add dirty inode to the flushing list.  Assigned a seq number so we
 * can wait for caps to flush without starving.
 *
 * Called under i_lock.
 */
static int __mark_caps_flushing(struct inode *inode,
				 struct ceph_mds_session *session)
{
	struct ceph_mds_client *mdsc = &ceph_sb_to_client(inode->i_sb)->mdsc;
	struct ceph_inode_info *ci = ceph_inode(inode);
	int flushing;

	BUG_ON(ci->i_dirty_caps == 0);
	BUG_ON(list_empty(&ci->i_dirty_item));

	flushing = ci->i_dirty_caps;
	dout("__mark_caps_flushing flushing %s, flushing_caps %s -> %s\n",
	     ceph_cap_string(flushing),
	     ceph_cap_string(ci->i_flushing_caps),
	     ceph_cap_string(ci->i_flushing_caps | flushing));
	ci->i_flushing_caps |= flushing;
	ci->i_dirty_caps = 0;
	dout(" inode %p now !dirty\n", inode);

	spin_lock(&mdsc->cap_dirty_lock);
	list_del_init(&ci->i_dirty_item);

	ci->i_cap_flush_seq = ++mdsc->cap_flush_seq;
	if (list_empty(&ci->i_flushing_item)) {
		list_add_tail(&ci->i_flushing_item, &session->s_cap_flushing);
		mdsc->num_cap_flushing++;
		dout(" inode %p now flushing seq %lld\n", inode,
		     ci->i_cap_flush_seq);
	} else {
		list_move_tail(&ci->i_flushing_item, &session->s_cap_flushing);
		dout(" inode %p now flushing (more) seq %lld\n", inode,
		     ci->i_cap_flush_seq);
	}
	spin_unlock(&mdsc->cap_dirty_lock);

	return flushing;
}

/*
 * try to invalidate mapping pages without blocking.
 */
static int mapping_is_empty(struct address_space *mapping)
{
	struct page *page = find_get_page(mapping, 0);

	if (!page)
		return 1;

	put_page(page);
	return 0;
}

static int try_nonblocking_invalidate(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	u32 invalidating_gen = ci->i_rdcache_gen;

	spin_unlock(&inode->i_lock);
	invalidate_mapping_pages(&inode->i_data, 0, -1);
	spin_lock(&inode->i_lock);

	if (mapping_is_empty(&inode->i_data) &&
	    invalidating_gen == ci->i_rdcache_gen) {
		/* success. */
		dout("try_nonblocking_invalidate %p success\n", inode);
		ci->i_rdcache_gen = 0;
		ci->i_rdcache_revoking = 0;
		return 0;
	}
	dout("try_nonblocking_invalidate %p failed\n", inode);
	return -1;
}

/*
 * Swiss army knife function to examine currently used and wanted
 * versus held caps.  Release, flush, ack revoked caps to mds as
 * appropriate.
 *
 *  CHECK_CAPS_NODELAY - caller is delayed work and we should not delay
 *    cap release further.
 *  CHECK_CAPS_AUTHONLY - we should only check the auth cap
 *  CHECK_CAPS_FLUSH - we should flush any dirty caps immediately, without
 *    further delay.
 */
void ceph_check_caps(struct ceph_inode_info *ci, int flags,
		     struct ceph_mds_session *session)
{
	struct ceph_client *client = ceph_inode_to_client(&ci->vfs_inode);
	struct ceph_mds_client *mdsc = &client->mdsc;
	struct inode *inode = &ci->vfs_inode;
	struct ceph_cap *cap;
	int file_wanted, used;
	int took_snap_rwsem = 0;             /* true if mdsc->snap_rwsem held */
	int issued, implemented, want, retain, revoking, flushing = 0;
	int mds = -1;   /* keep track of how far we've gone through i_caps list
			   to avoid an infinite loop on retry */
	struct rb_node *p;
	int tried_invalidate = 0;
	int delayed = 0, sent = 0, force_requeue = 0, num;
	int queue_invalidate = 0;
	int is_delayed = flags & CHECK_CAPS_NODELAY;

	/* if we are unmounting, flush any unused caps immediately. */
	if (mdsc->stopping)
		is_delayed = 1;

	spin_lock(&inode->i_lock);

	if (ci->i_ceph_flags & CEPH_I_FLUSH)
		flags |= CHECK_CAPS_FLUSH;

	/* flush snaps first time around only */
	if (!list_empty(&ci->i_cap_snaps))
		__ceph_flush_snaps(ci, &session, 0);
	goto retry_locked;
retry:
	spin_lock(&inode->i_lock);
retry_locked:
	file_wanted = __ceph_caps_file_wanted(ci);
	used = __ceph_caps_used(ci);
	want = file_wanted | used;
	issued = __ceph_caps_issued(ci, &implemented);
	revoking = implemented & ~issued;

	retain = want | CEPH_CAP_PIN;
	if (!mdsc->stopping && inode->i_nlink > 0) {
		if (want) {
			retain |= CEPH_CAP_ANY;       /* be greedy */
		} else {
			retain |= CEPH_CAP_ANY_SHARED;
			/*
			 * keep RD only if we didn't have the file open RW,
			 * because then the mds would revoke it anyway to
			 * journal max_size=0.
			 */
			if (ci->i_max_size == 0)
				retain |= CEPH_CAP_ANY_RD;
		}
	}

	dout("check_caps %p file_want %s used %s dirty %s flushing %s"
	     " issued %s revoking %s retain %s %s%s%s\n", inode,
	     ceph_cap_string(file_wanted),
	     ceph_cap_string(used), ceph_cap_string(ci->i_dirty_caps),
	     ceph_cap_string(ci->i_flushing_caps),
	     ceph_cap_string(issued), ceph_cap_string(revoking),
	     ceph_cap_string(retain),
	     (flags & CHECK_CAPS_AUTHONLY) ? " AUTHONLY" : "",
	     (flags & CHECK_CAPS_NODELAY) ? " NODELAY" : "",
	     (flags & CHECK_CAPS_FLUSH) ? " FLUSH" : "");

	/*
	 * If we no longer need to hold onto old our caps, and we may
	 * have cached pages, but don't want them, then try to invalidate.
	 * If we fail, it's because pages are locked.... try again later.
	 */
	if ((!is_delayed || mdsc->stopping) &&
	    ci->i_wrbuffer_ref == 0 &&               /* no dirty pages... */
	    ci->i_rdcache_gen &&                     /* may have cached pages */
	    (file_wanted == 0 ||                     /* no open files */
	     (revoking & (CEPH_CAP_FILE_CACHE|
			  CEPH_CAP_FILE_LAZYIO))) && /*  or revoking cache */
	    !tried_invalidate) {
		dout("check_caps trying to invalidate on %p\n", inode);
		if (try_nonblocking_invalidate(inode) < 0) {
			if (revoking & (CEPH_CAP_FILE_CACHE|
					CEPH_CAP_FILE_LAZYIO)) {
				dout("check_caps queuing invalidate\n");
				queue_invalidate = 1;
				ci->i_rdcache_revoking = ci->i_rdcache_gen;
			} else {
				dout("check_caps failed to invalidate pages\n");
				/* we failed to invalidate pages.  check these
				   caps again later. */
				force_requeue = 1;
				__cap_set_timeouts(mdsc, ci);
			}
		}
		tried_invalidate = 1;
		goto retry_locked;
	}

	num = 0;
	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		cap = rb_entry(p, struct ceph_cap, ci_node);
		num++;

		/* avoid looping forever */
		if (mds >= cap->mds ||
		    ((flags & CHECK_CAPS_AUTHONLY) && cap != ci->i_auth_cap))
			continue;

		/* NOTE: no side-effects allowed, until we take s_mutex */

		revoking = cap->implemented & ~cap->issued;
		if (revoking)
			dout(" mds%d revoking %s\n", cap->mds,
			     ceph_cap_string(revoking));

		if (cap == ci->i_auth_cap &&
		    (cap->issued & CEPH_CAP_FILE_WR)) {
			/* request larger max_size from MDS? */
			if (ci->i_wanted_max_size > ci->i_max_size &&
			    ci->i_wanted_max_size > ci->i_requested_max_size) {
				dout("requesting new max_size\n");
				goto ack;
			}

			/* approaching file_max? */
			if ((inode->i_size << 1) >= ci->i_max_size &&
			    (ci->i_reported_size << 1) < ci->i_max_size) {
				dout("i_size approaching max_size\n");
				goto ack;
			}
		}
		/* flush anything dirty? */
		if (cap == ci->i_auth_cap && (flags & CHECK_CAPS_FLUSH) &&
		    ci->i_dirty_caps) {
			dout("flushing dirty caps\n");
			goto ack;
		}

		/* completed revocation? going down and there are no caps? */
		if (revoking && (revoking & used) == 0) {
			dout("completed revocation of %s\n",
			     ceph_cap_string(cap->implemented & ~cap->issued));
			goto ack;
		}

		/* want more caps from mds? */
		if (want & ~(cap->mds_wanted | cap->issued))
			goto ack;

		/* things we might delay */
		if ((cap->issued & ~retain) == 0 &&
		    cap->mds_wanted == want)
			continue;     /* nope, all good */

		if (is_delayed)
			goto ack;

		/* delay? */
		if ((ci->i_ceph_flags & CEPH_I_NODELAY) == 0 &&
		    time_before(jiffies, ci->i_hold_caps_max)) {
			dout(" delaying issued %s -> %s, wanted %s -> %s\n",
			     ceph_cap_string(cap->issued),
			     ceph_cap_string(cap->issued & retain),
			     ceph_cap_string(cap->mds_wanted),
			     ceph_cap_string(want));
			delayed++;
			continue;
		}

ack:
		if (ci->i_ceph_flags & CEPH_I_NOFLUSH) {
			dout(" skipping %p I_NOFLUSH set\n", inode);
			continue;
		}

		if (session && session != cap->session) {
			dout("oops, wrong session %p mutex\n", session);
			mutex_unlock(&session->s_mutex);
			session = NULL;
		}
		if (!session) {
			session = cap->session;
			if (mutex_trylock(&session->s_mutex) == 0) {
				dout("inverting session/ino locks on %p\n",
				     session);
				spin_unlock(&inode->i_lock);
				if (took_snap_rwsem) {
					up_read(&mdsc->snap_rwsem);
					took_snap_rwsem = 0;
				}
				mutex_lock(&session->s_mutex);
				goto retry;
			}
		}
		/* take snap_rwsem after session mutex */
		if (!took_snap_rwsem) {
			if (down_read_trylock(&mdsc->snap_rwsem) == 0) {
				dout("inverting snap/in locks on %p\n",
				     inode);
				spin_unlock(&inode->i_lock);
				down_read(&mdsc->snap_rwsem);
				took_snap_rwsem = 1;
				goto retry;
			}
			took_snap_rwsem = 1;
		}

		if (cap == ci->i_auth_cap && ci->i_dirty_caps)
			flushing = __mark_caps_flushing(inode, session);

		mds = cap->mds;  /* remember mds, so we don't repeat */
		sent++;

		/* __send_cap drops i_lock */
		delayed += __send_cap(mdsc, cap, CEPH_CAP_OP_UPDATE, used, want,
				      retain, flushing, NULL);
		goto retry; /* retake i_lock and restart our cap scan. */
	}

	/*
	 * Reschedule delayed caps release if we delayed anything,
	 * otherwise cancel.
	 */
	if (delayed && is_delayed)
		force_requeue = 1;   /* __send_cap delayed release; requeue */
	if (!delayed && !is_delayed)
		__cap_delay_cancel(mdsc, ci);
	else if (!is_delayed || force_requeue)
		__cap_delay_requeue(mdsc, ci);

	spin_unlock(&inode->i_lock);

	if (queue_invalidate)
		ceph_queue_invalidate(inode);

	if (session)
		mutex_unlock(&session->s_mutex);
	if (took_snap_rwsem)
		up_read(&mdsc->snap_rwsem);
}

/*
 * Try to flush dirty caps back to the auth mds.
 */
static int try_flush_caps(struct inode *inode, struct ceph_mds_session *session,
			  unsigned *flush_tid)
{
	struct ceph_mds_client *mdsc = &ceph_sb_to_client(inode->i_sb)->mdsc;
	struct ceph_inode_info *ci = ceph_inode(inode);
	int unlock_session = session ? 0 : 1;
	int flushing = 0;

retry:
	spin_lock(&inode->i_lock);
	if (ci->i_ceph_flags & CEPH_I_NOFLUSH) {
		dout("try_flush_caps skipping %p I_NOFLUSH set\n", inode);
		goto out;
	}
	if (ci->i_dirty_caps && ci->i_auth_cap) {
		struct ceph_cap *cap = ci->i_auth_cap;
		int used = __ceph_caps_used(ci);
		int want = __ceph_caps_wanted(ci);
		int delayed;

		if (!session) {
			spin_unlock(&inode->i_lock);
			session = cap->session;
			mutex_lock(&session->s_mutex);
			goto retry;
		}
		BUG_ON(session != cap->session);
		if (cap->session->s_state < CEPH_MDS_SESSION_OPEN)
			goto out;

		flushing = __mark_caps_flushing(inode, session);

		/* __send_cap drops i_lock */
		delayed = __send_cap(mdsc, cap, CEPH_CAP_OP_FLUSH, used, want,
				     cap->issued | cap->implemented, flushing,
				     flush_tid);
		if (!delayed)
			goto out_unlocked;

		spin_lock(&inode->i_lock);
		__cap_delay_requeue(mdsc, ci);
	}
out:
	spin_unlock(&inode->i_lock);
out_unlocked:
	if (session && unlock_session)
		mutex_unlock(&session->s_mutex);
	return flushing;
}

/*
 * Return true if we've flushed caps through the given flush_tid.
 */
static int caps_are_flushed(struct inode *inode, unsigned tid)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int i, ret = 1;

	spin_lock(&inode->i_lock);
	for (i = 0; i < CEPH_CAP_BITS; i++)
		if ((ci->i_flushing_caps & (1 << i)) &&
		    ci->i_cap_flush_tid[i] <= tid) {
			/* still flushing this bit */
			ret = 0;
			break;
		}
	spin_unlock(&inode->i_lock);
	return ret;
}

/*
 * Wait on any unsafe replies for the given inode.  First wait on the
 * newest request, and make that the upper bound.  Then, if there are
 * more requests, keep waiting on the oldest as long as it is still older
 * than the original request.
 */
static void sync_write_wait(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct list_head *head = &ci->i_unsafe_writes;
	struct ceph_osd_request *req;
	u64 last_tid;

	spin_lock(&ci->i_unsafe_lock);
	if (list_empty(head))
		goto out;

	/* set upper bound as _last_ entry in chain */
	req = list_entry(head->prev, struct ceph_osd_request,
			 r_unsafe_item);
	last_tid = req->r_tid;

	do {
		ceph_osdc_get_request(req);
		spin_unlock(&ci->i_unsafe_lock);
		dout("sync_write_wait on tid %llu (until %llu)\n",
		     req->r_tid, last_tid);
		wait_for_completion(&req->r_safe_completion);
		spin_lock(&ci->i_unsafe_lock);
		ceph_osdc_put_request(req);

		/*
		 * from here on look at first entry in chain, since we
		 * only want to wait for anything older than last_tid
		 */
		if (list_empty(head))
			break;
		req = list_entry(head->next, struct ceph_osd_request,
				 r_unsafe_item);
	} while (req->r_tid < last_tid);
out:
	spin_unlock(&ci->i_unsafe_lock);
}

int ceph_fsync(struct file *file, int datasync)
{
	struct inode *inode = file->f_mapping->host;
	struct ceph_inode_info *ci = ceph_inode(inode);
	unsigned flush_tid;
	int ret;
	int dirty;

	dout("fsync %p%s\n", inode, datasync ? " datasync" : "");
	sync_write_wait(inode);

	ret = filemap_write_and_wait(inode->i_mapping);
	if (ret < 0)
		return ret;

	dirty = try_flush_caps(inode, NULL, &flush_tid);
	dout("fsync dirty caps are %s\n", ceph_cap_string(dirty));

	/*
	 * only wait on non-file metadata writeback (the mds
	 * can recover size and mtime, so we don't need to
	 * wait for that)
	 */
	if (!datasync && (dirty & ~CEPH_CAP_ANY_FILE_WR)) {
		dout("fsync waiting for flush_tid %u\n", flush_tid);
		ret = wait_event_interruptible(ci->i_cap_wq,
				       caps_are_flushed(inode, flush_tid));
	}

	dout("fsync %p%s done\n", inode, datasync ? " datasync" : "");
	return ret;
}

/*
 * Flush any dirty caps back to the mds.  If we aren't asked to wait,
 * queue inode for flush but don't do so immediately, because we can
 * get by with fewer MDS messages if we wait for data writeback to
 * complete first.
 */
int ceph_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	unsigned flush_tid;
	int err = 0;
	int dirty;
	int wait = wbc->sync_mode == WB_SYNC_ALL;

	dout("write_inode %p wait=%d\n", inode, wait);
	if (wait) {
		dirty = try_flush_caps(inode, NULL, &flush_tid);
		if (dirty)
			err = wait_event_interruptible(ci->i_cap_wq,
				       caps_are_flushed(inode, flush_tid));
	} else {
		struct ceph_mds_client *mdsc =
			&ceph_sb_to_client(inode->i_sb)->mdsc;

		spin_lock(&inode->i_lock);
		if (__ceph_caps_dirty(ci))
			__cap_delay_requeue_front(mdsc, ci);
		spin_unlock(&inode->i_lock);
	}
	return err;
}

/*
 * After a recovering MDS goes active, we need to resend any caps
 * we were flushing.
 *
 * Caller holds session->s_mutex.
 */
static void kick_flushing_capsnaps(struct ceph_mds_client *mdsc,
				   struct ceph_mds_session *session)
{
	struct ceph_cap_snap *capsnap;

	dout("kick_flushing_capsnaps mds%d\n", session->s_mds);
	list_for_each_entry(capsnap, &session->s_cap_snaps_flushing,
			    flushing_item) {
		struct ceph_inode_info *ci = capsnap->ci;
		struct inode *inode = &ci->vfs_inode;
		struct ceph_cap *cap;

		spin_lock(&inode->i_lock);
		cap = ci->i_auth_cap;
		if (cap && cap->session == session) {
			dout("kick_flushing_caps %p cap %p capsnap %p\n", inode,
			     cap, capsnap);
			__ceph_flush_snaps(ci, &session, 1);
		} else {
			pr_err("%p auth cap %p not mds%d ???\n", inode,
			       cap, session->s_mds);
		}
		spin_unlock(&inode->i_lock);
	}
}

void ceph_kick_flushing_caps(struct ceph_mds_client *mdsc,
			     struct ceph_mds_session *session)
{
	struct ceph_inode_info *ci;

	kick_flushing_capsnaps(mdsc, session);

	dout("kick_flushing_caps mds%d\n", session->s_mds);
	list_for_each_entry(ci, &session->s_cap_flushing, i_flushing_item) {
		struct inode *inode = &ci->vfs_inode;
		struct ceph_cap *cap;
		int delayed = 0;

		spin_lock(&inode->i_lock);
		cap = ci->i_auth_cap;
		if (cap && cap->session == session) {
			dout("kick_flushing_caps %p cap %p %s\n", inode,
			     cap, ceph_cap_string(ci->i_flushing_caps));
			delayed = __send_cap(mdsc, cap, CEPH_CAP_OP_FLUSH,
					     __ceph_caps_used(ci),
					     __ceph_caps_wanted(ci),
					     cap->issued | cap->implemented,
					     ci->i_flushing_caps, NULL);
			if (delayed) {
				spin_lock(&inode->i_lock);
				__cap_delay_requeue(mdsc, ci);
				spin_unlock(&inode->i_lock);
			}
		} else {
			pr_err("%p auth cap %p not mds%d ???\n", inode,
			       cap, session->s_mds);
			spin_unlock(&inode->i_lock);
		}
	}
}


/*
 * Take references to capabilities we hold, so that we don't release
 * them to the MDS prematurely.
 *
 * Protected by i_lock.
 */
static void __take_cap_refs(struct ceph_inode_info *ci, int got)
{
	if (got & CEPH_CAP_PIN)
		ci->i_pin_ref++;
	if (got & CEPH_CAP_FILE_RD)
		ci->i_rd_ref++;
	if (got & CEPH_CAP_FILE_CACHE)
		ci->i_rdcache_ref++;
	if (got & CEPH_CAP_FILE_WR)
		ci->i_wr_ref++;
	if (got & CEPH_CAP_FILE_BUFFER) {
		if (ci->i_wrbuffer_ref == 0)
			igrab(&ci->vfs_inode);
		ci->i_wrbuffer_ref++;
		dout("__take_cap_refs %p wrbuffer %d -> %d (?)\n",
		     &ci->vfs_inode, ci->i_wrbuffer_ref-1, ci->i_wrbuffer_ref);
	}
}

/*
 * Try to grab cap references.  Specify those refs we @want, and the
 * minimal set we @need.  Also include the larger offset we are writing
 * to (when applicable), and check against max_size here as well.
 * Note that caller is responsible for ensuring max_size increases are
 * requested from the MDS.
 */
static int try_get_cap_refs(struct ceph_inode_info *ci, int need, int want,
			    int *got, loff_t endoff, int *check_max, int *err)
{
	struct inode *inode = &ci->vfs_inode;
	int ret = 0;
	int have, implemented;
	int file_wanted;

	dout("get_cap_refs %p need %s want %s\n", inode,
	     ceph_cap_string(need), ceph_cap_string(want));
	spin_lock(&inode->i_lock);

	/* make sure file is actually open */
	file_wanted = __ceph_caps_file_wanted(ci);
	if ((file_wanted & need) == 0) {
		dout("try_get_cap_refs need %s file_wanted %s, EBADF\n",
		     ceph_cap_string(need), ceph_cap_string(file_wanted));
		*err = -EBADF;
		ret = 1;
		goto out;
	}

	if (need & CEPH_CAP_FILE_WR) {
		if (endoff >= 0 && endoff > (loff_t)ci->i_max_size) {
			dout("get_cap_refs %p endoff %llu > maxsize %llu\n",
			     inode, endoff, ci->i_max_size);
			if (endoff > ci->i_wanted_max_size) {
				*check_max = 1;
				ret = 1;
			}
			goto out;
		}
		/*
		 * If a sync write is in progress, we must wait, so that we
		 * can get a final snapshot value for size+mtime.
		 */
		if (__ceph_have_pending_cap_snap(ci)) {
			dout("get_cap_refs %p cap_snap_pending\n", inode);
			goto out;
		}
	}
	have = __ceph_caps_issued(ci, &implemented);

	/*
	 * disallow writes while a truncate is pending
	 */
	if (ci->i_truncate_pending)
		have &= ~CEPH_CAP_FILE_WR;

	if ((have & need) == need) {
		/*
		 * Look at (implemented & ~have & not) so that we keep waiting
		 * on transition from wanted -> needed caps.  This is needed
		 * for WRBUFFER|WR -> WR to avoid a new WR sync write from
		 * going before a prior buffered writeback happens.
		 */
		int not = want & ~(have & need);
		int revoking = implemented & ~have;
		dout("get_cap_refs %p have %s but not %s (revoking %s)\n",
		     inode, ceph_cap_string(have), ceph_cap_string(not),
		     ceph_cap_string(revoking));
		if ((revoking & not) == 0) {
			*got = need | (have & want);
			__take_cap_refs(ci, *got);
			ret = 1;
		}
	} else {
		dout("get_cap_refs %p have %s needed %s\n", inode,
		     ceph_cap_string(have), ceph_cap_string(need));
	}
out:
	spin_unlock(&inode->i_lock);
	dout("get_cap_refs %p ret %d got %s\n", inode,
	     ret, ceph_cap_string(*got));
	return ret;
}

/*
 * Check the offset we are writing up to against our current
 * max_size.  If necessary, tell the MDS we want to write to
 * a larger offset.
 */
static void check_max_size(struct inode *inode, loff_t endoff)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int check = 0;

	/* do we need to explicitly request a larger max_size? */
	spin_lock(&inode->i_lock);
	if ((endoff >= ci->i_max_size ||
	     endoff > (inode->i_size << 1)) &&
	    endoff > ci->i_wanted_max_size) {
		dout("write %p at large endoff %llu, req max_size\n",
		     inode, endoff);
		ci->i_wanted_max_size = endoff;
		check = 1;
	}
	spin_unlock(&inode->i_lock);
	if (check)
		ceph_check_caps(ci, CHECK_CAPS_AUTHONLY, NULL);
}

/*
 * Wait for caps, and take cap references.  If we can't get a WR cap
 * due to a small max_size, make sure we check_max_size (and possibly
 * ask the mds) so we don't get hung up indefinitely.
 */
int ceph_get_caps(struct ceph_inode_info *ci, int need, int want, int *got,
		  loff_t endoff)
{
	int check_max, ret, err;

retry:
	if (endoff > 0)
		check_max_size(&ci->vfs_inode, endoff);
	check_max = 0;
	err = 0;
	ret = wait_event_interruptible(ci->i_cap_wq,
				       try_get_cap_refs(ci, need, want,
							got, endoff,
							&check_max, &err));
	if (err)
		ret = err;
	if (check_max)
		goto retry;
	return ret;
}

/*
 * Take cap refs.  Caller must already know we hold at least one ref
 * on the caps in question or we don't know this is safe.
 */
void ceph_get_cap_refs(struct ceph_inode_info *ci, int caps)
{
	spin_lock(&ci->vfs_inode.i_lock);
	__take_cap_refs(ci, caps);
	spin_unlock(&ci->vfs_inode.i_lock);
}

/*
 * Release cap refs.
 *
 * If we released the last ref on any given cap, call ceph_check_caps
 * to release (or schedule a release).
 *
 * If we are releasing a WR cap (from a sync write), finalize any affected
 * cap_snap, and wake up any waiters.
 */
void ceph_put_cap_refs(struct ceph_inode_info *ci, int had)
{
	struct inode *inode = &ci->vfs_inode;
	int last = 0, put = 0, flushsnaps = 0, wake = 0;
	struct ceph_cap_snap *capsnap;

	spin_lock(&inode->i_lock);
	if (had & CEPH_CAP_PIN)
		--ci->i_pin_ref;
	if (had & CEPH_CAP_FILE_RD)
		if (--ci->i_rd_ref == 0)
			last++;
	if (had & CEPH_CAP_FILE_CACHE)
		if (--ci->i_rdcache_ref == 0)
			last++;
	if (had & CEPH_CAP_FILE_BUFFER) {
		if (--ci->i_wrbuffer_ref == 0) {
			last++;
			put++;
		}
		dout("put_cap_refs %p wrbuffer %d -> %d (?)\n",
		     inode, ci->i_wrbuffer_ref+1, ci->i_wrbuffer_ref);
	}
	if (had & CEPH_CAP_FILE_WR)
		if (--ci->i_wr_ref == 0) {
			last++;
			if (!list_empty(&ci->i_cap_snaps)) {
				capsnap = list_first_entry(&ci->i_cap_snaps,
						     struct ceph_cap_snap,
						     ci_item);
				if (capsnap->writing) {
					capsnap->writing = 0;
					flushsnaps =
						__ceph_finish_cap_snap(ci,
								       capsnap);
					wake = 1;
				}
			}
		}
	spin_unlock(&inode->i_lock);

	dout("put_cap_refs %p had %s%s%s\n", inode, ceph_cap_string(had),
	     last ? " last" : "", put ? " put" : "");

	if (last && !flushsnaps)
		ceph_check_caps(ci, 0, NULL);
	else if (flushsnaps)
		ceph_flush_snaps(ci);
	if (wake)
		wake_up_all(&ci->i_cap_wq);
	if (put)
		iput(inode);
}

/*
 * Release @nr WRBUFFER refs on dirty pages for the given @snapc snap
 * context.  Adjust per-snap dirty page accounting as appropriate.
 * Once all dirty data for a cap_snap is flushed, flush snapped file
 * metadata back to the MDS.  If we dropped the last ref, call
 * ceph_check_caps.
 */
void ceph_put_wrbuffer_cap_refs(struct ceph_inode_info *ci, int nr,
				struct ceph_snap_context *snapc)
{
	struct inode *inode = &ci->vfs_inode;
	int last = 0;
	int complete_capsnap = 0;
	int drop_capsnap = 0;
	int found = 0;
	struct ceph_cap_snap *capsnap = NULL;

	spin_lock(&inode->i_lock);
	ci->i_wrbuffer_ref -= nr;
	last = !ci->i_wrbuffer_ref;

	if (ci->i_head_snapc == snapc) {
		ci->i_wrbuffer_ref_head -= nr;
		if (ci->i_wrbuffer_ref_head == 0 &&
		    ci->i_dirty_caps == 0 && ci->i_flushing_caps == 0) {
			BUG_ON(!ci->i_head_snapc);
			ceph_put_snap_context(ci->i_head_snapc);
			ci->i_head_snapc = NULL;
		}
		dout("put_wrbuffer_cap_refs on %p head %d/%d -> %d/%d %s\n",
		     inode,
		     ci->i_wrbuffer_ref+nr, ci->i_wrbuffer_ref_head+nr,
		     ci->i_wrbuffer_ref, ci->i_wrbuffer_ref_head,
		     last ? " LAST" : "");
	} else {
		list_for_each_entry(capsnap, &ci->i_cap_snaps, ci_item) {
			if (capsnap->context == snapc) {
				found = 1;
				break;
			}
		}
		BUG_ON(!found);
		capsnap->dirty_pages -= nr;
		if (capsnap->dirty_pages == 0) {
			complete_capsnap = 1;
			if (capsnap->dirty == 0)
				/* cap writeback completed before we created
				 * the cap_snap; no FLUSHSNAP is needed */
				drop_capsnap = 1;
		}
		dout("put_wrbuffer_cap_refs on %p cap_snap %p "
		     " snap %lld %d/%d -> %d/%d %s%s%s\n",
		     inode, capsnap, capsnap->context->seq,
		     ci->i_wrbuffer_ref+nr, capsnap->dirty_pages + nr,
		     ci->i_wrbuffer_ref, capsnap->dirty_pages,
		     last ? " (wrbuffer last)" : "",
		     complete_capsnap ? " (complete capsnap)" : "",
		     drop_capsnap ? " (drop capsnap)" : "");
		if (drop_capsnap) {
			ceph_put_snap_context(capsnap->context);
			list_del(&capsnap->ci_item);
			list_del(&capsnap->flushing_item);
			ceph_put_cap_snap(capsnap);
		}
	}

	spin_unlock(&inode->i_lock);

	if (last) {
		ceph_check_caps(ci, CHECK_CAPS_AUTHONLY, NULL);
		iput(inode);
	} else if (complete_capsnap) {
		ceph_flush_snaps(ci);
		wake_up_all(&ci->i_cap_wq);
	}
	if (drop_capsnap)
		iput(inode);
}

/*
 * Handle a cap GRANT message from the MDS.  (Note that a GRANT may
 * actually be a revocation if it specifies a smaller cap set.)
 *
 * caller holds s_mutex and i_lock, we drop both.
 *
 * return value:
 *  0 - ok
 *  1 - check_caps on auth cap only (writeback)
 *  2 - check_caps (ack revoke)
 */
static void handle_cap_grant(struct inode *inode, struct ceph_mds_caps *grant,
			     struct ceph_mds_session *session,
			     struct ceph_cap *cap,
			     struct ceph_buffer *xattr_buf)
		__releases(inode->i_lock)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int mds = session->s_mds;
	unsigned seq = le32_to_cpu(grant->seq);
	unsigned issue_seq = le32_to_cpu(grant->issue_seq);
	int newcaps = le32_to_cpu(grant->caps);
	int issued, implemented, used, wanted, dirty;
	u64 size = le64_to_cpu(grant->size);
	u64 max_size = le64_to_cpu(grant->max_size);
	struct timespec mtime, atime, ctime;
	int check_caps = 0;
	int wake = 0;
	int writeback = 0;
	int revoked_rdcache = 0;
	int queue_invalidate = 0;

	dout("handle_cap_grant inode %p cap %p mds%d seq %u/%u %s\n",
	     inode, cap, mds, seq, issue_seq, ceph_cap_string(newcaps));
	dout(" size %llu max_size %llu, i_size %llu\n", size, max_size,
		inode->i_size);

	/*
	 * If CACHE is being revoked, and we have no dirty buffers,
	 * try to invalidate (once).  (If there are dirty buffers, we
	 * will invalidate _after_ writeback.)
	 */
	if (((cap->issued & ~newcaps) & CEPH_CAP_FILE_CACHE) &&
	    (newcaps & CEPH_CAP_FILE_LAZYIO) == 0 &&
	    !ci->i_wrbuffer_ref) {
		if (try_nonblocking_invalidate(inode) == 0) {
			revoked_rdcache = 1;
		} else {
			/* there were locked pages.. invalidate later
			   in a separate thread. */
			if (ci->i_rdcache_revoking != ci->i_rdcache_gen) {
				queue_invalidate = 1;
				ci->i_rdcache_revoking = ci->i_rdcache_gen;
			}
		}
	}

	/* side effects now are allowed */

	issued = __ceph_caps_issued(ci, &implemented);
	issued |= implemented | __ceph_caps_dirty(ci);

	cap->cap_gen = session->s_cap_gen;

	__check_cap_issue(ci, cap, newcaps);

	if ((issued & CEPH_CAP_AUTH_EXCL) == 0) {
		inode->i_mode = le32_to_cpu(grant->mode);
		inode->i_uid = le32_to_cpu(grant->uid);
		inode->i_gid = le32_to_cpu(grant->gid);
		dout("%p mode 0%o uid.gid %d.%d\n", inode, inode->i_mode,
		     inode->i_uid, inode->i_gid);
	}

	if ((issued & CEPH_CAP_LINK_EXCL) == 0)
		inode->i_nlink = le32_to_cpu(grant->nlink);

	if ((issued & CEPH_CAP_XATTR_EXCL) == 0 && grant->xattr_len) {
		int len = le32_to_cpu(grant->xattr_len);
		u64 version = le64_to_cpu(grant->xattr_version);

		if (version > ci->i_xattrs.version) {
			dout(" got new xattrs v%llu on %p len %d\n",
			     version, inode, len);
			if (ci->i_xattrs.blob)
				ceph_buffer_put(ci->i_xattrs.blob);
			ci->i_xattrs.blob = ceph_buffer_get(xattr_buf);
			ci->i_xattrs.version = version;
		}
	}

	/* size/ctime/mtime/atime? */
	ceph_fill_file_size(inode, issued,
			    le32_to_cpu(grant->truncate_seq),
			    le64_to_cpu(grant->truncate_size), size);
	ceph_decode_timespec(&mtime, &grant->mtime);
	ceph_decode_timespec(&atime, &grant->atime);
	ceph_decode_timespec(&ctime, &grant->ctime);
	ceph_fill_file_time(inode, issued,
			    le32_to_cpu(grant->time_warp_seq), &ctime, &mtime,
			    &atime);

	/* max size increase? */
	if (max_size != ci->i_max_size) {
		dout("max_size %lld -> %llu\n", ci->i_max_size, max_size);
		ci->i_max_size = max_size;
		if (max_size >= ci->i_wanted_max_size) {
			ci->i_wanted_max_size = 0;  /* reset */
			ci->i_requested_max_size = 0;
		}
		wake = 1;
	}

	/* check cap bits */
	wanted = __ceph_caps_wanted(ci);
	used = __ceph_caps_used(ci);
	dirty = __ceph_caps_dirty(ci);
	dout(" my wanted = %s, used = %s, dirty %s\n",
	     ceph_cap_string(wanted),
	     ceph_cap_string(used),
	     ceph_cap_string(dirty));
	if (wanted != le32_to_cpu(grant->wanted)) {
		dout("mds wanted %s -> %s\n",
		     ceph_cap_string(le32_to_cpu(grant->wanted)),
		     ceph_cap_string(wanted));
		grant->wanted = cpu_to_le32(wanted);
	}

	cap->seq = seq;
	cap->issue_seq = issue_seq;

	/* file layout may have changed */
	ci->i_layout = grant->layout;

	/* revocation, grant, or no-op? */
	if (cap->issued & ~newcaps) {
		int revoking = cap->issued & ~newcaps;

		dout("revocation: %s -> %s (revoking %s)\n",
		     ceph_cap_string(cap->issued),
		     ceph_cap_string(newcaps),
		     ceph_cap_string(revoking));
		if (revoking & used & CEPH_CAP_FILE_BUFFER)
			writeback = 1;  /* initiate writeback; will delay ack */
		else if (revoking == CEPH_CAP_FILE_CACHE &&
			 (newcaps & CEPH_CAP_FILE_LAZYIO) == 0 &&
			 queue_invalidate)
			; /* do nothing yet, invalidation will be queued */
		else if (cap == ci->i_auth_cap)
			check_caps = 1; /* check auth cap only */
		else
			check_caps = 2; /* check all caps */
		cap->issued = newcaps;
		cap->implemented |= newcaps;
	} else if (cap->issued == newcaps) {
		dout("caps unchanged: %s -> %s\n",
		     ceph_cap_string(cap->issued), ceph_cap_string(newcaps));
	} else {
		dout("grant: %s -> %s\n", ceph_cap_string(cap->issued),
		     ceph_cap_string(newcaps));
		cap->issued = newcaps;
		cap->implemented |= newcaps; /* add bits only, to
					      * avoid stepping on a
					      * pending revocation */
		wake = 1;
	}
	BUG_ON(cap->issued & ~cap->implemented);

	spin_unlock(&inode->i_lock);
	if (writeback)
		/*
		 * queue inode for writeback: we can't actually call
		 * filemap_write_and_wait, etc. from message handler
		 * context.
		 */
		ceph_queue_writeback(inode);
	if (queue_invalidate)
		ceph_queue_invalidate(inode);
	if (wake)
		wake_up_all(&ci->i_cap_wq);

	if (check_caps == 1)
		ceph_check_caps(ci, CHECK_CAPS_NODELAY|CHECK_CAPS_AUTHONLY,
				session);
	else if (check_caps == 2)
		ceph_check_caps(ci, CHECK_CAPS_NODELAY, session);
	else
		mutex_unlock(&session->s_mutex);
}

/*
 * Handle FLUSH_ACK from MDS, indicating that metadata we sent to the
 * MDS has been safely committed.
 */
static void handle_cap_flush_ack(struct inode *inode, u64 flush_tid,
				 struct ceph_mds_caps *m,
				 struct ceph_mds_session *session,
				 struct ceph_cap *cap)
	__releases(inode->i_lock)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct ceph_mds_client *mdsc = &ceph_sb_to_client(inode->i_sb)->mdsc;
	unsigned seq = le32_to_cpu(m->seq);
	int dirty = le32_to_cpu(m->dirty);
	int cleaned = 0;
	int drop = 0;
	int i;

	for (i = 0; i < CEPH_CAP_BITS; i++)
		if ((dirty & (1 << i)) &&
		    flush_tid == ci->i_cap_flush_tid[i])
			cleaned |= 1 << i;

	dout("handle_cap_flush_ack inode %p mds%d seq %d on %s cleaned %s,"
	     " flushing %s -> %s\n",
	     inode, session->s_mds, seq, ceph_cap_string(dirty),
	     ceph_cap_string(cleaned), ceph_cap_string(ci->i_flushing_caps),
	     ceph_cap_string(ci->i_flushing_caps & ~cleaned));

	if (ci->i_flushing_caps == (ci->i_flushing_caps & ~cleaned))
		goto out;

	ci->i_flushing_caps &= ~cleaned;

	spin_lock(&mdsc->cap_dirty_lock);
	if (ci->i_flushing_caps == 0) {
		list_del_init(&ci->i_flushing_item);
		if (!list_empty(&session->s_cap_flushing))
			dout(" mds%d still flushing cap on %p\n",
			     session->s_mds,
			     &list_entry(session->s_cap_flushing.next,
					 struct ceph_inode_info,
					 i_flushing_item)->vfs_inode);
		mdsc->num_cap_flushing--;
		wake_up_all(&mdsc->cap_flushing_wq);
		dout(" inode %p now !flushing\n", inode);

		if (ci->i_dirty_caps == 0) {
			dout(" inode %p now clean\n", inode);
			BUG_ON(!list_empty(&ci->i_dirty_item));
			drop = 1;
			if (ci->i_wrbuffer_ref_head == 0) {
				BUG_ON(!ci->i_head_snapc);
				ceph_put_snap_context(ci->i_head_snapc);
				ci->i_head_snapc = NULL;
			}
		} else {
			BUG_ON(list_empty(&ci->i_dirty_item));
		}
	}
	spin_unlock(&mdsc->cap_dirty_lock);
	wake_up_all(&ci->i_cap_wq);

out:
	spin_unlock(&inode->i_lock);
	if (drop)
		iput(inode);
}

/*
 * Handle FLUSHSNAP_ACK.  MDS has flushed snap data to disk and we can
 * throw away our cap_snap.
 *
 * Caller hold s_mutex.
 */
static void handle_cap_flushsnap_ack(struct inode *inode, u64 flush_tid,
				     struct ceph_mds_caps *m,
				     struct ceph_mds_session *session)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	u64 follows = le64_to_cpu(m->snap_follows);
	struct ceph_cap_snap *capsnap;
	int drop = 0;

	dout("handle_cap_flushsnap_ack inode %p ci %p mds%d follows %lld\n",
	     inode, ci, session->s_mds, follows);

	spin_lock(&inode->i_lock);
	list_for_each_entry(capsnap, &ci->i_cap_snaps, ci_item) {
		if (capsnap->follows == follows) {
			if (capsnap->flush_tid != flush_tid) {
				dout(" cap_snap %p follows %lld tid %lld !="
				     " %lld\n", capsnap, follows,
				     flush_tid, capsnap->flush_tid);
				break;
			}
			WARN_ON(capsnap->dirty_pages || capsnap->writing);
			dout(" removing %p cap_snap %p follows %lld\n",
			     inode, capsnap, follows);
			ceph_put_snap_context(capsnap->context);
			list_del(&capsnap->ci_item);
			list_del(&capsnap->flushing_item);
			ceph_put_cap_snap(capsnap);
			drop = 1;
			break;
		} else {
			dout(" skipping cap_snap %p follows %lld\n",
			     capsnap, capsnap->follows);
		}
	}
	spin_unlock(&inode->i_lock);
	if (drop)
		iput(inode);
}

/*
 * Handle TRUNC from MDS, indicating file truncation.
 *
 * caller hold s_mutex.
 */
static void handle_cap_trunc(struct inode *inode,
			     struct ceph_mds_caps *trunc,
			     struct ceph_mds_session *session)
	__releases(inode->i_lock)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int mds = session->s_mds;
	int seq = le32_to_cpu(trunc->seq);
	u32 truncate_seq = le32_to_cpu(trunc->truncate_seq);
	u64 truncate_size = le64_to_cpu(trunc->truncate_size);
	u64 size = le64_to_cpu(trunc->size);
	int implemented = 0;
	int dirty = __ceph_caps_dirty(ci);
	int issued = __ceph_caps_issued(ceph_inode(inode), &implemented);
	int queue_trunc = 0;

	issued |= implemented | dirty;

	dout("handle_cap_trunc inode %p mds%d seq %d to %lld seq %d\n",
	     inode, mds, seq, truncate_size, truncate_seq);
	queue_trunc = ceph_fill_file_size(inode, issued,
					  truncate_seq, truncate_size, size);
	spin_unlock(&inode->i_lock);

	if (queue_trunc)
		ceph_queue_vmtruncate(inode);
}

/*
 * Handle EXPORT from MDS.  Cap is being migrated _from_ this mds to a
 * different one.  If we are the most recent migration we've seen (as
 * indicated by mseq), make note of the migrating cap bits for the
 * duration (until we see the corresponding IMPORT).
 *
 * caller holds s_mutex
 */
static void handle_cap_export(struct inode *inode, struct ceph_mds_caps *ex,
			      struct ceph_mds_session *session,
			      int *open_target_sessions)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int mds = session->s_mds;
	unsigned mseq = le32_to_cpu(ex->migrate_seq);
	struct ceph_cap *cap = NULL, *t;
	struct rb_node *p;
	int remember = 1;

	dout("handle_cap_export inode %p ci %p mds%d mseq %d\n",
	     inode, ci, mds, mseq);

	spin_lock(&inode->i_lock);

	/* make sure we haven't seen a higher mseq */
	for (p = rb_first(&ci->i_caps); p; p = rb_next(p)) {
		t = rb_entry(p, struct ceph_cap, ci_node);
		if (ceph_seq_cmp(t->mseq, mseq) > 0) {
			dout(" higher mseq on cap from mds%d\n",
			     t->session->s_mds);
			remember = 0;
		}
		if (t->session->s_mds == mds)
			cap = t;
	}

	if (cap) {
		if (remember) {
			/* make note */
			ci->i_cap_exporting_mds = mds;
			ci->i_cap_exporting_mseq = mseq;
			ci->i_cap_exporting_issued = cap->issued;

			/*
			 * make sure we have open sessions with all possible
			 * export targets, so that we get the matching IMPORT
			 */
			*open_target_sessions = 1;
		}
		__ceph_remove_cap(cap);
	}
	/* else, we already released it */

	spin_unlock(&inode->i_lock);
}

/*
 * Handle cap IMPORT.  If there are temp bits from an older EXPORT,
 * clean them up.
 *
 * caller holds s_mutex.
 */
static void handle_cap_import(struct ceph_mds_client *mdsc,
			      struct inode *inode, struct ceph_mds_caps *im,
			      struct ceph_mds_session *session,
			      void *snaptrace, int snaptrace_len)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	int mds = session->s_mds;
	unsigned issued = le32_to_cpu(im->caps);
	unsigned wanted = le32_to_cpu(im->wanted);
	unsigned seq = le32_to_cpu(im->seq);
	unsigned mseq = le32_to_cpu(im->migrate_seq);
	u64 realmino = le64_to_cpu(im->realm);
	u64 cap_id = le64_to_cpu(im->cap_id);

	if (ci->i_cap_exporting_mds >= 0 &&
	    ceph_seq_cmp(ci->i_cap_exporting_mseq, mseq) < 0) {
		dout("handle_cap_import inode %p ci %p mds%d mseq %d"
		     " - cleared exporting from mds%d\n",
		     inode, ci, mds, mseq,
		     ci->i_cap_exporting_mds);
		ci->i_cap_exporting_issued = 0;
		ci->i_cap_exporting_mseq = 0;
		ci->i_cap_exporting_mds = -1;
	} else {
		dout("handle_cap_import inode %p ci %p mds%d mseq %d\n",
		     inode, ci, mds, mseq);
	}

	down_write(&mdsc->snap_rwsem);
	ceph_update_snap_trace(mdsc, snaptrace, snaptrace+snaptrace_len,
			       false);
	downgrade_write(&mdsc->snap_rwsem);
	ceph_add_cap(inode, session, cap_id, -1,
		     issued, wanted, seq, mseq, realmino, CEPH_CAP_FLAG_AUTH,
		     NULL /* no caps context */);
	try_flush_caps(inode, session, NULL);
	up_read(&mdsc->snap_rwsem);
}

/*
 * Handle a caps message from the MDS.
 *
 * Identify the appropriate session, inode, and call the right handler
 * based on the cap op.
 */
void ceph_handle_caps(struct ceph_mds_session *session,
		      struct ceph_msg *msg)
{
	struct ceph_mds_client *mdsc = session->s_mdsc;
	struct super_block *sb = mdsc->client->sb;
	struct inode *inode;
	struct ceph_cap *cap;
	struct ceph_mds_caps *h;
	int mds = session->s_mds;
	int op;
	u32 seq, mseq;
	struct ceph_vino vino;
	u64 cap_id;
	u64 size, max_size;
	u64 tid;
	void *snaptrace;
	size_t snaptrace_len;
	void *flock;
	u32 flock_len;
	int open_target_sessions = 0;

	dout("handle_caps from mds%d\n", mds);

	/* decode */
	tid = le64_to_cpu(msg->hdr.tid);
	if (msg->front.iov_len < sizeof(*h))
		goto bad;
	h = msg->front.iov_base;
	op = le32_to_cpu(h->op);
	vino.ino = le64_to_cpu(h->ino);
	vino.snap = CEPH_NOSNAP;
	cap_id = le64_to_cpu(h->cap_id);
	seq = le32_to_cpu(h->seq);
	mseq = le32_to_cpu(h->migrate_seq);
	size = le64_to_cpu(h->size);
	max_size = le64_to_cpu(h->max_size);

	snaptrace = h + 1;
	snaptrace_len = le32_to_cpu(h->snap_trace_len);

	if (le16_to_cpu(msg->hdr.version) >= 2) {
		void *p, *end;

		p = snaptrace + snaptrace_len;
		end = msg->front.iov_base + msg->front.iov_len;
		ceph_decode_32_safe(&p, end, flock_len, bad);
		flock = p;
	} else {
		flock = NULL;
		flock_len = 0;
	}

	mutex_lock(&session->s_mutex);
	session->s_seq++;
	dout(" mds%d seq %lld cap seq %u\n", session->s_mds, session->s_seq,
	     (unsigned)seq);

	/* lookup ino */
	inode = ceph_find_inode(sb, vino);
	dout(" op %s ino %llx.%llx inode %p\n", ceph_cap_op_name(op), vino.ino,
	     vino.snap, inode);
	if (!inode) {
		dout(" i don't have ino %llx\n", vino.ino);

		if (op == CEPH_CAP_OP_IMPORT)
			__queue_cap_release(session, vino.ino, cap_id,
					    mseq, seq);
		goto flush_cap_releases;
	}

	/* these will work even if we don't have a cap yet */
	switch (op) {
	case CEPH_CAP_OP_FLUSHSNAP_ACK:
		handle_cap_flushsnap_ack(inode, tid, h, session);
		goto done;

	case CEPH_CAP_OP_EXPORT:
		handle_cap_export(inode, h, session, &open_target_sessions);
		goto done;

	case CEPH_CAP_OP_IMPORT:
		handle_cap_import(mdsc, inode, h, session,
				  snaptrace, snaptrace_len);
		ceph_check_caps(ceph_inode(inode), CHECK_CAPS_NODELAY,
				session);
		goto done_unlocked;
	}

	/* the rest require a cap */
	spin_lock(&inode->i_lock);
	cap = __get_cap_for_mds(ceph_inode(inode), mds);
	if (!cap) {
		dout(" no cap on %p ino %llx.%llx from mds%d\n",
		     inode, ceph_ino(inode), ceph_snap(inode), mds);
		spin_unlock(&inode->i_lock);
		goto flush_cap_releases;
	}

	/* note that each of these drops i_lock for us */
	switch (op) {
	case CEPH_CAP_OP_REVOKE:
	case CEPH_CAP_OP_GRANT:
		handle_cap_grant(inode, h, session, cap, msg->middle);
		goto done_unlocked;

	case CEPH_CAP_OP_FLUSH_ACK:
		handle_cap_flush_ack(inode, tid, h, session, cap);
		break;

	case CEPH_CAP_OP_TRUNC:
		handle_cap_trunc(inode, h, session);
		break;

	default:
		spin_unlock(&inode->i_lock);
		pr_err("ceph_handle_caps: unknown cap op %d %s\n", op,
		       ceph_cap_op_name(op));
	}

	goto done;

flush_cap_releases:
	/*
	 * send any full release message to try to move things
	 * along for the mds (who clearly thinks we still have this
	 * cap).
	 */
	ceph_add_cap_releases(mdsc, session);
	ceph_send_cap_releases(mdsc, session);

done:
	mutex_unlock(&session->s_mutex);
done_unlocked:
	if (inode)
		iput(inode);
	if (open_target_sessions)
		ceph_mdsc_open_export_target_sessions(mdsc, session);
	return;

bad:
	pr_err("ceph_handle_caps: corrupt message\n");
	ceph_msg_dump(msg);
	return;
}

/*
 * Delayed work handler to process end of delayed cap release LRU list.
 */
void ceph_check_delayed_caps(struct ceph_mds_client *mdsc)
{
	struct ceph_inode_info *ci;
	int flags = CHECK_CAPS_NODELAY;

	dout("check_delayed_caps\n");
	while (1) {
		spin_lock(&mdsc->cap_delay_lock);
		if (list_empty(&mdsc->cap_delay_list))
			break;
		ci = list_first_entry(&mdsc->cap_delay_list,
				      struct ceph_inode_info,
				      i_cap_delay_list);
		if ((ci->i_ceph_flags & CEPH_I_FLUSH) == 0 &&
		    time_before(jiffies, ci->i_hold_caps_max))
			break;
		list_del_init(&ci->i_cap_delay_list);
		spin_unlock(&mdsc->cap_delay_lock);
		dout("check_delayed_caps on %p\n", &ci->vfs_inode);
		ceph_check_caps(ci, flags, NULL);
	}
	spin_unlock(&mdsc->cap_delay_lock);
}

/*
 * Flush all dirty caps to the mds
 */
void ceph_flush_dirty_caps(struct ceph_mds_client *mdsc)
{
	struct ceph_inode_info *ci, *nci = NULL;
	struct inode *inode, *ninode = NULL;
	struct list_head *p, *n;

	dout("flush_dirty_caps\n");
	spin_lock(&mdsc->cap_dirty_lock);
	list_for_each_safe(p, n, &mdsc->cap_dirty) {
		if (nci) {
			ci = nci;
			inode = ninode;
			ci->i_ceph_flags &= ~CEPH_I_NOFLUSH;
			dout("flush_dirty_caps inode %p (was next inode)\n",
			     inode);
		} else {
			ci = list_entry(p, struct ceph_inode_info,
					i_dirty_item);
			inode = igrab(&ci->vfs_inode);
			BUG_ON(!inode);
			dout("flush_dirty_caps inode %p\n", inode);
		}
		if (n != &mdsc->cap_dirty) {
			nci = list_entry(n, struct ceph_inode_info,
					 i_dirty_item);
			ninode = igrab(&nci->vfs_inode);
			BUG_ON(!ninode);
			nci->i_ceph_flags |= CEPH_I_NOFLUSH;
			dout("flush_dirty_caps next inode %p, noflush\n",
			     ninode);
		} else {
			nci = NULL;
			ninode = NULL;
		}
		spin_unlock(&mdsc->cap_dirty_lock);
		if (inode) {
			ceph_check_caps(ci, CHECK_CAPS_NODELAY|CHECK_CAPS_FLUSH,
					NULL);
			iput(inode);
		}
		spin_lock(&mdsc->cap_dirty_lock);
	}
	spin_unlock(&mdsc->cap_dirty_lock);
}

/*
 * Drop open file reference.  If we were the last open file,
 * we may need to release capabilities to the MDS (or schedule
 * their delayed release).
 */
void ceph_put_fmode(struct ceph_inode_info *ci, int fmode)
{
	struct inode *inode = &ci->vfs_inode;
	int last = 0;

	spin_lock(&inode->i_lock);
	dout("put_fmode %p fmode %d %d -> %d\n", inode, fmode,
	     ci->i_nr_by_mode[fmode], ci->i_nr_by_mode[fmode]-1);
	BUG_ON(ci->i_nr_by_mode[fmode] == 0);
	if (--ci->i_nr_by_mode[fmode] == 0)
		last++;
	spin_unlock(&inode->i_lock);

	if (last && ci->i_vino.snap == CEPH_NOSNAP)
		ceph_check_caps(ci, 0, NULL);
}

/*
 * Helpers for embedding cap and dentry lease releases into mds
 * requests.
 *
 * @force is used by dentry_release (below) to force inclusion of a
 * record for the directory inode, even when there aren't any caps to
 * drop.
 */
int ceph_encode_inode_release(void **p, struct inode *inode,
			      int mds, int drop, int unless, int force)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct ceph_cap *cap;
	struct ceph_mds_request_release *rel = *p;
	int used, dirty;
	int ret = 0;

	spin_lock(&inode->i_lock);
	used = __ceph_caps_used(ci);
	dirty = __ceph_caps_dirty(ci);

	dout("encode_inode_release %p mds%d used|dirty %s drop %s unless %s\n",
	     inode, mds, ceph_cap_string(used|dirty), ceph_cap_string(drop),
	     ceph_cap_string(unless));

	/* only drop unused, clean caps */
	drop &= ~(used | dirty);

	cap = __get_cap_for_mds(ci, mds);
	if (cap && __cap_is_valid(cap)) {
		if (force ||
		    ((cap->issued & drop) &&
		     (cap->issued & unless) == 0)) {
			if ((cap->issued & drop) &&
			    (cap->issued & unless) == 0) {
				dout("encode_inode_release %p cap %p %s -> "
				     "%s\n", inode, cap,
				     ceph_cap_string(cap->issued),
				     ceph_cap_string(cap->issued & ~drop));
				cap->issued &= ~drop;
				cap->implemented &= ~drop;
				if (ci->i_ceph_flags & CEPH_I_NODELAY) {
					int wanted = __ceph_caps_wanted(ci);
					dout("  wanted %s -> %s (act %s)\n",
					     ceph_cap_string(cap->mds_wanted),
					     ceph_cap_string(cap->mds_wanted &
							     ~wanted),
					     ceph_cap_string(wanted));
					cap->mds_wanted &= wanted;
				}
			} else {
				dout("encode_inode_release %p cap %p %s"
				     " (force)\n", inode, cap,
				     ceph_cap_string(cap->issued));
			}

			rel->ino = cpu_to_le64(ceph_ino(inode));
			rel->cap_id = cpu_to_le64(cap->cap_id);
			rel->seq = cpu_to_le32(cap->seq);
			rel->issue_seq = cpu_to_le32(cap->issue_seq),
			rel->mseq = cpu_to_le32(cap->mseq);
			rel->caps = cpu_to_le32(cap->issued);
			rel->wanted = cpu_to_le32(cap->mds_wanted);
			rel->dname_len = 0;
			rel->dname_seq = 0;
			*p += sizeof(*rel);
			ret = 1;
		} else {
			dout("encode_inode_release %p cap %p %s\n",
			     inode, cap, ceph_cap_string(cap->issued));
		}
	}
	spin_unlock(&inode->i_lock);
	return ret;
}

int ceph_encode_dentry_release(void **p, struct dentry *dentry,
			       int mds, int drop, int unless)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct ceph_mds_request_release *rel = *p;
	struct ceph_dentry_info *di = ceph_dentry(dentry);
	int force = 0;
	int ret;

	/*
	 * force an record for the directory caps if we have a dentry lease.
	 * this is racy (can't take i_lock and d_lock together), but it
	 * doesn't have to be perfect; the mds will revoke anything we don't
	 * release.
	 */
	spin_lock(&dentry->d_lock);
	if (di->lease_session && di->lease_session->s_mds == mds)
		force = 1;
	spin_unlock(&dentry->d_lock);

	ret = ceph_encode_inode_release(p, dir, mds, drop, unless, force);

	spin_lock(&dentry->d_lock);
	if (ret && di->lease_session && di->lease_session->s_mds == mds) {
		dout("encode_dentry_release %p mds%d seq %d\n",
		     dentry, mds, (int)di->lease_seq);
		rel->dname_len = cpu_to_le32(dentry->d_name.len);
		memcpy(*p, dentry->d_name.name, dentry->d_name.len);
		*p += dentry->d_name.len;
		rel->dname_seq = cpu_to_le32(di->lease_seq);
		__ceph_mdsc_drop_dentry_lease(dentry);
	}
	spin_unlock(&dentry->d_lock);
	return ret;
}
