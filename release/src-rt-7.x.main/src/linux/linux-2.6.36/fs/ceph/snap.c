#include "ceph_debug.h"

#include <linux/sort.h>
#include <linux/slab.h>

#include "super.h"
#include "decode.h"

/*
 * Snapshots in ceph are driven in large part by cooperation from the
 * client.  In contrast to local file systems or file servers that
 * implement snapshots at a single point in the system, ceph's
 * distributed access to storage requires clients to help decide
 * whether a write logically occurs before or after a recently created
 * snapshot.
 *
 * This provides a perfect instantanous client-wide snapshot.  Between
 * clients, however, snapshots may appear to be applied at slightly
 * different points in time, depending on delays in delivering the
 * snapshot notification.
 *
 * Snapshots are _not_ file system-wide.  Instead, each snapshot
 * applies to the subdirectory nested beneath some directory.  This
 * effectively divides the hierarchy into multiple "realms," where all
 * of the files contained by each realm share the same set of
 * snapshots.  An individual realm's snap set contains snapshots
 * explicitly created on that realm, as well as any snaps in its
 * parent's snap set _after_ the point at which the parent became it's
 * parent (due to, say, a rename).  Similarly, snaps from prior parents
 * during the time intervals during which they were the parent are included.
 *
 * The client is spared most of this detail, fortunately... it must only
 * maintains a hierarchy of realms reflecting the current parent/child
 * realm relationship, and for each realm has an explicit list of snaps
 * inherited from prior parents.
 *
 * A snap_realm struct is maintained for realms containing every inode
 * with an open cap in the system.  (The needed snap realm information is
 * provided by the MDS whenever a cap is issued, i.e., on open.)  A 'seq'
 * version number is used to ensure that as realm parameters change (new
 * snapshot, new parent, etc.) the client's realm hierarchy is updated.
 *
 * The realm hierarchy drives the generation of a 'snap context' for each
 * realm, which simply lists the resulting set of snaps for the realm.  This
 * is attached to any writes sent to OSDs.
 */
/*
 * Unfortunately error handling is a bit mixed here.  If we get a snap
 * update, but don't have enough memory to update our realm hierarchy,
 * it's not clear what we can do about it (besides complaining to the
 * console).
 */


/*
 * increase ref count for the realm
 *
 * caller must hold snap_rwsem for write.
 */
void ceph_get_snap_realm(struct ceph_mds_client *mdsc,
			 struct ceph_snap_realm *realm)
{
	dout("get_realm %p %d -> %d\n", realm,
	     atomic_read(&realm->nref), atomic_read(&realm->nref)+1);
	/*
	 * since we _only_ increment realm refs or empty the empty
	 * list with snap_rwsem held, adjusting the empty list here is
	 * safe.  we do need to protect against concurrent empty list
	 * additions, however.
	 */
	if (atomic_read(&realm->nref) == 0) {
		spin_lock(&mdsc->snap_empty_lock);
		list_del_init(&realm->empty_item);
		spin_unlock(&mdsc->snap_empty_lock);
	}

	atomic_inc(&realm->nref);
}

static void __insert_snap_realm(struct rb_root *root,
				struct ceph_snap_realm *new)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ceph_snap_realm *r = NULL;

	while (*p) {
		parent = *p;
		r = rb_entry(parent, struct ceph_snap_realm, node);
		if (new->ino < r->ino)
			p = &(*p)->rb_left;
		else if (new->ino > r->ino)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&new->node, parent, p);
	rb_insert_color(&new->node, root);
}

/*
 * create and get the realm rooted at @ino and bump its ref count.
 *
 * caller must hold snap_rwsem for write.
 */
static struct ceph_snap_realm *ceph_create_snap_realm(
	struct ceph_mds_client *mdsc,
	u64 ino)
{
	struct ceph_snap_realm *realm;

	realm = kzalloc(sizeof(*realm), GFP_NOFS);
	if (!realm)
		return ERR_PTR(-ENOMEM);

	atomic_set(&realm->nref, 0);    /* tree does not take a ref */
	realm->ino = ino;
	INIT_LIST_HEAD(&realm->children);
	INIT_LIST_HEAD(&realm->child_item);
	INIT_LIST_HEAD(&realm->empty_item);
	INIT_LIST_HEAD(&realm->dirty_item);
	INIT_LIST_HEAD(&realm->inodes_with_caps);
	spin_lock_init(&realm->inodes_with_caps_lock);
	__insert_snap_realm(&mdsc->snap_realms, realm);
	dout("create_snap_realm %llx %p\n", realm->ino, realm);
	return realm;
}

/*
 * lookup the realm rooted at @ino.
 *
 * caller must hold snap_rwsem for write.
 */
struct ceph_snap_realm *ceph_lookup_snap_realm(struct ceph_mds_client *mdsc,
					       u64 ino)
{
	struct rb_node *n = mdsc->snap_realms.rb_node;
	struct ceph_snap_realm *r;

	while (n) {
		r = rb_entry(n, struct ceph_snap_realm, node);
		if (ino < r->ino)
			n = n->rb_left;
		else if (ino > r->ino)
			n = n->rb_right;
		else {
			dout("lookup_snap_realm %llx %p\n", r->ino, r);
			return r;
		}
	}
	return NULL;
}

static void __put_snap_realm(struct ceph_mds_client *mdsc,
			     struct ceph_snap_realm *realm);

/*
 * called with snap_rwsem (write)
 */
static void __destroy_snap_realm(struct ceph_mds_client *mdsc,
				 struct ceph_snap_realm *realm)
{
	dout("__destroy_snap_realm %p %llx\n", realm, realm->ino);

	rb_erase(&realm->node, &mdsc->snap_realms);

	if (realm->parent) {
		list_del_init(&realm->child_item);
		__put_snap_realm(mdsc, realm->parent);
	}

	kfree(realm->prior_parent_snaps);
	kfree(realm->snaps);
	ceph_put_snap_context(realm->cached_context);
	kfree(realm);
}

/*
 * caller holds snap_rwsem (write)
 */
static void __put_snap_realm(struct ceph_mds_client *mdsc,
			     struct ceph_snap_realm *realm)
{
	dout("__put_snap_realm %llx %p %d -> %d\n", realm->ino, realm,
	     atomic_read(&realm->nref), atomic_read(&realm->nref)-1);
	if (atomic_dec_and_test(&realm->nref))
		__destroy_snap_realm(mdsc, realm);
}

/*
 * caller needn't hold any locks
 */
void ceph_put_snap_realm(struct ceph_mds_client *mdsc,
			 struct ceph_snap_realm *realm)
{
	dout("put_snap_realm %llx %p %d -> %d\n", realm->ino, realm,
	     atomic_read(&realm->nref), atomic_read(&realm->nref)-1);
	if (!atomic_dec_and_test(&realm->nref))
		return;

	if (down_write_trylock(&mdsc->snap_rwsem)) {
		__destroy_snap_realm(mdsc, realm);
		up_write(&mdsc->snap_rwsem);
	} else {
		spin_lock(&mdsc->snap_empty_lock);
		list_add(&mdsc->snap_empty, &realm->empty_item);
		spin_unlock(&mdsc->snap_empty_lock);
	}
}

/*
 * Clean up any realms whose ref counts have dropped to zero.  Note
 * that this does not include realms who were created but not yet
 * used.
 *
 * Called under snap_rwsem (write)
 */
static void __cleanup_empty_realms(struct ceph_mds_client *mdsc)
{
	struct ceph_snap_realm *realm;

	spin_lock(&mdsc->snap_empty_lock);
	while (!list_empty(&mdsc->snap_empty)) {
		realm = list_first_entry(&mdsc->snap_empty,
				   struct ceph_snap_realm, empty_item);
		list_del(&realm->empty_item);
		spin_unlock(&mdsc->snap_empty_lock);
		__destroy_snap_realm(mdsc, realm);
		spin_lock(&mdsc->snap_empty_lock);
	}
	spin_unlock(&mdsc->snap_empty_lock);
}

void ceph_cleanup_empty_realms(struct ceph_mds_client *mdsc)
{
	down_write(&mdsc->snap_rwsem);
	__cleanup_empty_realms(mdsc);
	up_write(&mdsc->snap_rwsem);
}

/*
 * adjust the parent realm of a given @realm.  adjust child list, and parent
 * pointers, and ref counts appropriately.
 *
 * return true if parent was changed, 0 if unchanged, <0 on error.
 *
 * caller must hold snap_rwsem for write.
 */
static int adjust_snap_realm_parent(struct ceph_mds_client *mdsc,
				    struct ceph_snap_realm *realm,
				    u64 parentino)
{
	struct ceph_snap_realm *parent;

	if (realm->parent_ino == parentino)
		return 0;

	parent = ceph_lookup_snap_realm(mdsc, parentino);
	if (!parent) {
		parent = ceph_create_snap_realm(mdsc, parentino);
		if (IS_ERR(parent))
			return PTR_ERR(parent);
	}
	dout("adjust_snap_realm_parent %llx %p: %llx %p -> %llx %p\n",
	     realm->ino, realm, realm->parent_ino, realm->parent,
	     parentino, parent);
	if (realm->parent) {
		list_del_init(&realm->child_item);
		ceph_put_snap_realm(mdsc, realm->parent);
	}
	realm->parent_ino = parentino;
	realm->parent = parent;
	ceph_get_snap_realm(mdsc, parent);
	list_add(&realm->child_item, &parent->children);
	return 1;
}


static int cmpu64_rev(const void *a, const void *b)
{
	if (*(u64 *)a < *(u64 *)b)
		return 1;
	if (*(u64 *)a > *(u64 *)b)
		return -1;
	return 0;
}

/*
 * build the snap context for a given realm.
 */
static int build_snap_context(struct ceph_snap_realm *realm)
{
	struct ceph_snap_realm *parent = realm->parent;
	struct ceph_snap_context *snapc;
	int err = 0;
	int i;
	int num = realm->num_prior_parent_snaps + realm->num_snaps;

	/*
	 * build parent context, if it hasn't been built.
	 * conservatively estimate that all parent snaps might be
	 * included by us.
	 */
	if (parent) {
		if (!parent->cached_context) {
			err = build_snap_context(parent);
			if (err)
				goto fail;
		}
		num += parent->cached_context->num_snaps;
	}

	/* do i actually need to update?  not if my context seq
	   matches realm seq, and my parents' does to.  (this works
	   because we rebuild_snap_realms() works _downward_ in
	   hierarchy after each update.) */
	if (realm->cached_context &&
	    realm->cached_context->seq == realm->seq &&
	    (!parent ||
	     realm->cached_context->seq >= parent->cached_context->seq)) {
		dout("build_snap_context %llx %p: %p seq %lld (%d snaps)"
		     " (unchanged)\n",
		     realm->ino, realm, realm->cached_context,
		     realm->cached_context->seq,
		     realm->cached_context->num_snaps);
		return 0;
	}

	/* alloc new snap context */
	err = -ENOMEM;
	if (num > ULONG_MAX / sizeof(u64) - sizeof(*snapc))
		goto fail;
	snapc = kzalloc(sizeof(*snapc) + num*sizeof(u64), GFP_NOFS);
	if (!snapc)
		goto fail;
	atomic_set(&snapc->nref, 1);

	/* build (reverse sorted) snap vector */
	num = 0;
	snapc->seq = realm->seq;
	if (parent) {
		/* include any of parent's snaps occuring _after_ my
		   parent became my parent */
		for (i = 0; i < parent->cached_context->num_snaps; i++)
			if (parent->cached_context->snaps[i] >=
			    realm->parent_since)
				snapc->snaps[num++] =
					parent->cached_context->snaps[i];
		if (parent->cached_context->seq > snapc->seq)
			snapc->seq = parent->cached_context->seq;
	}
	memcpy(snapc->snaps + num, realm->snaps,
	       sizeof(u64)*realm->num_snaps);
	num += realm->num_snaps;
	memcpy(snapc->snaps + num, realm->prior_parent_snaps,
	       sizeof(u64)*realm->num_prior_parent_snaps);
	num += realm->num_prior_parent_snaps;

	sort(snapc->snaps, num, sizeof(u64), cmpu64_rev, NULL);
	snapc->num_snaps = num;
	dout("build_snap_context %llx %p: %p seq %lld (%d snaps)\n",
	     realm->ino, realm, snapc, snapc->seq, snapc->num_snaps);

	if (realm->cached_context)
		ceph_put_snap_context(realm->cached_context);
	realm->cached_context = snapc;
	return 0;

fail:
	/*
	 * if we fail, clear old (incorrect) cached_context... hopefully
	 * we'll have better luck building it later
	 */
	if (realm->cached_context) {
		ceph_put_snap_context(realm->cached_context);
		realm->cached_context = NULL;
	}
	pr_err("build_snap_context %llx %p fail %d\n", realm->ino,
	       realm, err);
	return err;
}

/*
 * rebuild snap context for the given realm and all of its children.
 */
static void rebuild_snap_realms(struct ceph_snap_realm *realm)
{
	struct ceph_snap_realm *child;

	dout("rebuild_snap_realms %llx %p\n", realm->ino, realm);
	build_snap_context(realm);

	list_for_each_entry(child, &realm->children, child_item)
		rebuild_snap_realms(child);
}


/*
 * helper to allocate and decode an array of snapids.  free prior
 * instance, if any.
 */
static int dup_array(u64 **dst, __le64 *src, int num)
{
	int i;

	kfree(*dst);
	if (num) {
		*dst = kcalloc(num, sizeof(u64), GFP_NOFS);
		if (!*dst)
			return -ENOMEM;
		for (i = 0; i < num; i++)
			(*dst)[i] = get_unaligned_le64(src + i);
	} else {
		*dst = NULL;
	}
	return 0;
}


/*
 * When a snapshot is applied, the size/mtime inode metadata is queued
 * in a ceph_cap_snap (one for each snapshot) until writeback
 * completes and the metadata can be flushed back to the MDS.
 *
 * However, if a (sync) write is currently in-progress when we apply
 * the snapshot, we have to wait until the write succeeds or fails
 * (and a final size/mtime is known).  In this case the
 * cap_snap->writing = 1, and is said to be "pending."  When the write
 * finishes, we __ceph_finish_cap_snap().
 *
 * Caller must hold snap_rwsem for read (i.e., the realm topology won't
 * change).
 */
void ceph_queue_cap_snap(struct ceph_inode_info *ci)
{
	struct inode *inode = &ci->vfs_inode;
	struct ceph_cap_snap *capsnap;
	int used, dirty;

	capsnap = kzalloc(sizeof(*capsnap), GFP_NOFS);
	if (!capsnap) {
		pr_err("ENOMEM allocating ceph_cap_snap on %p\n", inode);
		return;
	}

	spin_lock(&inode->i_lock);
	used = __ceph_caps_used(ci);
	dirty = __ceph_caps_dirty(ci);
	if (__ceph_have_pending_cap_snap(ci)) {
		/* there is no point in queuing multiple "pending" cap_snaps,
		   as no new writes are allowed to start when pending, so any
		   writes in progress now were started before the previous
		   cap_snap.  lucky us. */
		dout("queue_cap_snap %p already pending\n", inode);
		kfree(capsnap);
	} else if (ci->i_wrbuffer_ref_head || (used & CEPH_CAP_FILE_WR) ||
		   (dirty & (CEPH_CAP_AUTH_EXCL|CEPH_CAP_XATTR_EXCL|
			     CEPH_CAP_FILE_EXCL|CEPH_CAP_FILE_WR))) {
		struct ceph_snap_context *snapc = ci->i_head_snapc;

		dout("queue_cap_snap %p cap_snap %p queuing under %p\n", inode,
		     capsnap, snapc);
		igrab(inode);
		
		atomic_set(&capsnap->nref, 1);
		capsnap->ci = ci;
		INIT_LIST_HEAD(&capsnap->ci_item);
		INIT_LIST_HEAD(&capsnap->flushing_item);

		capsnap->follows = snapc->seq;
		capsnap->issued = __ceph_caps_issued(ci, NULL);
		capsnap->dirty = dirty;

		capsnap->mode = inode->i_mode;
		capsnap->uid = inode->i_uid;
		capsnap->gid = inode->i_gid;

		if (dirty & CEPH_CAP_XATTR_EXCL) {
			__ceph_build_xattrs_blob(ci);
			capsnap->xattr_blob =
				ceph_buffer_get(ci->i_xattrs.blob);
			capsnap->xattr_version = ci->i_xattrs.version;
		} else {
			capsnap->xattr_blob = NULL;
			capsnap->xattr_version = 0;
		}

		/* dirty page count moved from _head to this cap_snap;
		   all subsequent writes page dirties occur _after_ this
		   snapshot. */
		capsnap->dirty_pages = ci->i_wrbuffer_ref_head;
		ci->i_wrbuffer_ref_head = 0;
		capsnap->context = snapc;
		ci->i_head_snapc =
			ceph_get_snap_context(ci->i_snap_realm->cached_context);
		dout(" new snapc is %p\n", ci->i_head_snapc);
		list_add_tail(&capsnap->ci_item, &ci->i_cap_snaps);

		if (used & CEPH_CAP_FILE_WR) {
			dout("queue_cap_snap %p cap_snap %p snapc %p"
			     " seq %llu used WR, now pending\n", inode,
			     capsnap, snapc, snapc->seq);
			capsnap->writing = 1;
		} else {
			/* note mtime, size NOW. */
			__ceph_finish_cap_snap(ci, capsnap);
		}
	} else {
		dout("queue_cap_snap %p nothing dirty|writing\n", inode);
		kfree(capsnap);
	}

	spin_unlock(&inode->i_lock);
}

/*
 * Finalize the size, mtime for a cap_snap.. that is, settle on final values
 * to be used for the snapshot, to be flushed back to the mds.
 *
 * If capsnap can now be flushed, add to snap_flush list, and return 1.
 *
 * Caller must hold i_lock.
 */
int __ceph_finish_cap_snap(struct ceph_inode_info *ci,
			    struct ceph_cap_snap *capsnap)
{
	struct inode *inode = &ci->vfs_inode;
	struct ceph_mds_client *mdsc = &ceph_sb_to_client(inode->i_sb)->mdsc;

	BUG_ON(capsnap->writing);
	capsnap->size = inode->i_size;
	capsnap->mtime = inode->i_mtime;
	capsnap->atime = inode->i_atime;
	capsnap->ctime = inode->i_ctime;
	capsnap->time_warp_seq = ci->i_time_warp_seq;
	if (capsnap->dirty_pages) {
		dout("finish_cap_snap %p cap_snap %p snapc %p %llu %s s=%llu "
		     "still has %d dirty pages\n", inode, capsnap,
		     capsnap->context, capsnap->context->seq,
		     ceph_cap_string(capsnap->dirty), capsnap->size,
		     capsnap->dirty_pages);
		return 0;
	}
	dout("finish_cap_snap %p cap_snap %p snapc %p %llu %s s=%llu\n",
	     inode, capsnap, capsnap->context,
	     capsnap->context->seq, ceph_cap_string(capsnap->dirty),
	     capsnap->size);

	spin_lock(&mdsc->snap_flush_lock);
	list_add_tail(&ci->i_snap_flush_item, &mdsc->snap_flush_list);
	spin_unlock(&mdsc->snap_flush_lock);
	return 1;  /* caller may want to ceph_flush_snaps */
}

/*
 * Queue cap_snaps for snap writeback for this realm and its children.
 * Called under snap_rwsem, so realm topology won't change.
 */
static void queue_realm_cap_snaps(struct ceph_snap_realm *realm)
{
	struct ceph_inode_info *ci;
	struct inode *lastinode = NULL;
	struct ceph_snap_realm *child;

	dout("queue_realm_cap_snaps %p %llx inodes\n", realm, realm->ino);

	spin_lock(&realm->inodes_with_caps_lock);
	list_for_each_entry(ci, &realm->inodes_with_caps,
			    i_snap_realm_item) {
		struct inode *inode = igrab(&ci->vfs_inode);
		if (!inode)
			continue;
		spin_unlock(&realm->inodes_with_caps_lock);
		if (lastinode)
			iput(lastinode);
		lastinode = inode;
		ceph_queue_cap_snap(ci);
		spin_lock(&realm->inodes_with_caps_lock);
	}
	spin_unlock(&realm->inodes_with_caps_lock);
	if (lastinode)
		iput(lastinode);

	dout("queue_realm_cap_snaps %p %llx children\n", realm, realm->ino);
	list_for_each_entry(child, &realm->children, child_item)
		queue_realm_cap_snaps(child);

	dout("queue_realm_cap_snaps %p %llx done\n", realm, realm->ino);
}

/*
 * Parse and apply a snapblob "snap trace" from the MDS.  This specifies
 * the snap realm parameters from a given realm and all of its ancestors,
 * up to the root.
 *
 * Caller must hold snap_rwsem for write.
 */
int ceph_update_snap_trace(struct ceph_mds_client *mdsc,
			   void *p, void *e, bool deletion)
{
	struct ceph_mds_snap_realm *ri;    /* encoded */
	__le64 *snaps;                     /* encoded */
	__le64 *prior_parent_snaps;        /* encoded */
	struct ceph_snap_realm *realm;
	int invalidate = 0;
	int err = -ENOMEM;
	LIST_HEAD(dirty_realms);

	dout("update_snap_trace deletion=%d\n", deletion);
more:
	ceph_decode_need(&p, e, sizeof(*ri), bad);
	ri = p;
	p += sizeof(*ri);
	ceph_decode_need(&p, e, sizeof(u64)*(le32_to_cpu(ri->num_snaps) +
			    le32_to_cpu(ri->num_prior_parent_snaps)), bad);
	snaps = p;
	p += sizeof(u64) * le32_to_cpu(ri->num_snaps);
	prior_parent_snaps = p;
	p += sizeof(u64) * le32_to_cpu(ri->num_prior_parent_snaps);

	realm = ceph_lookup_snap_realm(mdsc, le64_to_cpu(ri->ino));
	if (!realm) {
		realm = ceph_create_snap_realm(mdsc, le64_to_cpu(ri->ino));
		if (IS_ERR(realm)) {
			err = PTR_ERR(realm);
			goto fail;
		}
	}

	/* ensure the parent is correct */
	err = adjust_snap_realm_parent(mdsc, realm, le64_to_cpu(ri->parent));
	if (err < 0)
		goto fail;
	invalidate += err;

	if (le64_to_cpu(ri->seq) > realm->seq) {
		dout("update_snap_trace updating %llx %p %lld -> %lld\n",
		     realm->ino, realm, realm->seq, le64_to_cpu(ri->seq));
		/* update realm parameters, snap lists */
		realm->seq = le64_to_cpu(ri->seq);
		realm->created = le64_to_cpu(ri->created);
		realm->parent_since = le64_to_cpu(ri->parent_since);

		realm->num_snaps = le32_to_cpu(ri->num_snaps);
		err = dup_array(&realm->snaps, snaps, realm->num_snaps);
		if (err < 0)
			goto fail;

		realm->num_prior_parent_snaps =
			le32_to_cpu(ri->num_prior_parent_snaps);
		err = dup_array(&realm->prior_parent_snaps, prior_parent_snaps,
				realm->num_prior_parent_snaps);
		if (err < 0)
			goto fail;

		/* queue realm for cap_snap creation */
		list_add(&realm->dirty_item, &dirty_realms);

		invalidate = 1;
	} else if (!realm->cached_context) {
		dout("update_snap_trace %llx %p seq %lld new\n",
		     realm->ino, realm, realm->seq);
		invalidate = 1;
	} else {
		dout("update_snap_trace %llx %p seq %lld unchanged\n",
		     realm->ino, realm, realm->seq);
	}

	dout("done with %llx %p, invalidated=%d, %p %p\n", realm->ino,
	     realm, invalidate, p, e);

	if (p < e)
		goto more;

	/* invalidate when we reach the _end_ (root) of the trace */
	if (invalidate)
		rebuild_snap_realms(realm);

	/*
	 * queue cap snaps _after_ we've built the new snap contexts,
	 * so that i_head_snapc can be set appropriately.
	 */
	list_for_each_entry(realm, &dirty_realms, dirty_item) {
		queue_realm_cap_snaps(realm);
	}

	__cleanup_empty_realms(mdsc);
	return 0;

bad:
	err = -EINVAL;
fail:
	pr_err("update_snap_trace error %d\n", err);
	return err;
}


/*
 * Send any cap_snaps that are queued for flush.  Try to carry
 * s_mutex across multiple snap flushes to avoid locking overhead.
 *
 * Caller holds no locks.
 */
static void flush_snaps(struct ceph_mds_client *mdsc)
{
	struct ceph_inode_info *ci;
	struct inode *inode;
	struct ceph_mds_session *session = NULL;

	dout("flush_snaps\n");
	spin_lock(&mdsc->snap_flush_lock);
	while (!list_empty(&mdsc->snap_flush_list)) {
		ci = list_first_entry(&mdsc->snap_flush_list,
				struct ceph_inode_info, i_snap_flush_item);
		inode = &ci->vfs_inode;
		igrab(inode);
		spin_unlock(&mdsc->snap_flush_lock);
		spin_lock(&inode->i_lock);
		__ceph_flush_snaps(ci, &session, 0);
		spin_unlock(&inode->i_lock);
		iput(inode);
		spin_lock(&mdsc->snap_flush_lock);
	}
	spin_unlock(&mdsc->snap_flush_lock);

	if (session) {
		mutex_unlock(&session->s_mutex);
		ceph_put_mds_session(session);
	}
	dout("flush_snaps done\n");
}


/*
 * Handle a snap notification from the MDS.
 *
 * This can take two basic forms: the simplest is just a snap creation
 * or deletion notification on an existing realm.  This should update the
 * realm and its children.
 *
 * The more difficult case is realm creation, due to snap creation at a
 * new point in the file hierarchy, or due to a rename that moves a file or
 * directory into another realm.
 */
void ceph_handle_snap(struct ceph_mds_client *mdsc,
		      struct ceph_mds_session *session,
		      struct ceph_msg *msg)
{
	struct super_block *sb = mdsc->client->sb;
	int mds = session->s_mds;
	u64 split;
	int op;
	int trace_len;
	struct ceph_snap_realm *realm = NULL;
	void *p = msg->front.iov_base;
	void *e = p + msg->front.iov_len;
	struct ceph_mds_snap_head *h;
	int num_split_inos, num_split_realms;
	__le64 *split_inos = NULL, *split_realms = NULL;
	int i;
	int locked_rwsem = 0;

	/* decode */
	if (msg->front.iov_len < sizeof(*h))
		goto bad;
	h = p;
	op = le32_to_cpu(h->op);
	split = le64_to_cpu(h->split);   /* non-zero if we are splitting an
					  * existing realm */
	num_split_inos = le32_to_cpu(h->num_split_inos);
	num_split_realms = le32_to_cpu(h->num_split_realms);
	trace_len = le32_to_cpu(h->trace_len);
	p += sizeof(*h);

	dout("handle_snap from mds%d op %s split %llx tracelen %d\n", mds,
	     ceph_snap_op_name(op), split, trace_len);

	mutex_lock(&session->s_mutex);
	session->s_seq++;
	mutex_unlock(&session->s_mutex);

	down_write(&mdsc->snap_rwsem);
	locked_rwsem = 1;

	if (op == CEPH_SNAP_OP_SPLIT) {
		struct ceph_mds_snap_realm *ri;

		/*
		 * A "split" breaks part of an existing realm off into
		 * a new realm.  The MDS provides a list of inodes
		 * (with caps) and child realms that belong to the new
		 * child.
		 */
		split_inos = p;
		p += sizeof(u64) * num_split_inos;
		split_realms = p;
		p += sizeof(u64) * num_split_realms;
		ceph_decode_need(&p, e, sizeof(*ri), bad);
		/* we will peek at realm info here, but will _not_
		 * advance p, as the realm update will occur below in
		 * ceph_update_snap_trace. */
		ri = p;

		realm = ceph_lookup_snap_realm(mdsc, split);
		if (!realm) {
			realm = ceph_create_snap_realm(mdsc, split);
			if (IS_ERR(realm))
				goto out;
		}
		ceph_get_snap_realm(mdsc, realm);

		dout("splitting snap_realm %llx %p\n", realm->ino, realm);
		for (i = 0; i < num_split_inos; i++) {
			struct ceph_vino vino = {
				.ino = le64_to_cpu(split_inos[i]),
				.snap = CEPH_NOSNAP,
			};
			struct inode *inode = ceph_find_inode(sb, vino);
			struct ceph_inode_info *ci;
			struct ceph_snap_realm *oldrealm;

			if (!inode)
				continue;
			ci = ceph_inode(inode);

			spin_lock(&inode->i_lock);
			if (!ci->i_snap_realm)
				goto skip_inode;
			/*
			 * If this inode belongs to a realm that was
			 * created after our new realm, we experienced
			 * a race (due to another split notifications
			 * arriving from a different MDS).  So skip
			 * this inode.
			 */
			if (ci->i_snap_realm->created >
			    le64_to_cpu(ri->created)) {
				dout(" leaving %p in newer realm %llx %p\n",
				     inode, ci->i_snap_realm->ino,
				     ci->i_snap_realm);
				goto skip_inode;
			}
			dout(" will move %p to split realm %llx %p\n",
			     inode, realm->ino, realm);
			/*
			 * Move the inode to the new realm
			 */
			spin_lock(&realm->inodes_with_caps_lock);
			list_del_init(&ci->i_snap_realm_item);
			list_add(&ci->i_snap_realm_item,
				 &realm->inodes_with_caps);
			oldrealm = ci->i_snap_realm;
			ci->i_snap_realm = realm;
			spin_unlock(&realm->inodes_with_caps_lock);
			spin_unlock(&inode->i_lock);

			ceph_get_snap_realm(mdsc, realm);
			ceph_put_snap_realm(mdsc, oldrealm);

			iput(inode);
			continue;

skip_inode:
			spin_unlock(&inode->i_lock);
			iput(inode);
		}

		/* we may have taken some of the old realm's children. */
		for (i = 0; i < num_split_realms; i++) {
			struct ceph_snap_realm *child =
				ceph_lookup_snap_realm(mdsc,
					   le64_to_cpu(split_realms[i]));
			if (!child)
				continue;
			adjust_snap_realm_parent(mdsc, child, realm->ino);
		}
	}

	/*
	 * update using the provided snap trace. if we are deleting a
	 * snap, we can avoid queueing cap_snaps.
	 */
	ceph_update_snap_trace(mdsc, p, e,
			       op == CEPH_SNAP_OP_DESTROY);

	if (op == CEPH_SNAP_OP_SPLIT)
		/* we took a reference when we created the realm, above */
		ceph_put_snap_realm(mdsc, realm);

	__cleanup_empty_realms(mdsc);

	up_write(&mdsc->snap_rwsem);

	flush_snaps(mdsc);
	return;

bad:
	pr_err("corrupt snap message from mds%d\n", mds);
	ceph_msg_dump(msg);
out:
	if (locked_rwsem)
		up_write(&mdsc->snap_rwsem);
	return;
}
