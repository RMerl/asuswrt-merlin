#include <linux/fs.h>
#include <linux/types.h>
#include "ctree.h"
#include "disk-io.h"
#include "btrfs_inode.h"
#include "print-tree.h"
#include "export.h"
#include "compat.h"

#define BTRFS_FID_SIZE_NON_CONNECTABLE (offsetof(struct btrfs_fid, \
						 parent_objectid) / 4)
#define BTRFS_FID_SIZE_CONNECTABLE (offsetof(struct btrfs_fid, \
					     parent_root_objectid) / 4)
#define BTRFS_FID_SIZE_CONNECTABLE_ROOT (sizeof(struct btrfs_fid) / 4)

static int btrfs_encode_fh(struct dentry *dentry, u32 *fh, int *max_len,
			   int connectable)
{
	struct btrfs_fid *fid = (struct btrfs_fid *)fh;
	struct inode *inode = dentry->d_inode;
	int len = *max_len;
	int type;

	if ((len < BTRFS_FID_SIZE_NON_CONNECTABLE) ||
	    (connectable && len < BTRFS_FID_SIZE_CONNECTABLE))
		return 255;

	len  = BTRFS_FID_SIZE_NON_CONNECTABLE;
	type = FILEID_BTRFS_WITHOUT_PARENT;

	fid->objectid = inode->i_ino;
	fid->root_objectid = BTRFS_I(inode)->root->objectid;
	fid->gen = inode->i_generation;

	if (connectable && !S_ISDIR(inode->i_mode)) {
		struct inode *parent;
		u64 parent_root_id;

		spin_lock(&dentry->d_lock);

		parent = dentry->d_parent->d_inode;
		fid->parent_objectid = BTRFS_I(parent)->location.objectid;
		fid->parent_gen = parent->i_generation;
		parent_root_id = BTRFS_I(parent)->root->objectid;

		spin_unlock(&dentry->d_lock);

		if (parent_root_id != fid->root_objectid) {
			fid->parent_root_objectid = parent_root_id;
			len = BTRFS_FID_SIZE_CONNECTABLE_ROOT;
			type = FILEID_BTRFS_WITH_PARENT_ROOT;
		} else {
			len = BTRFS_FID_SIZE_CONNECTABLE;
			type = FILEID_BTRFS_WITH_PARENT;
		}
	}

	*max_len = len;
	return type;
}

static struct dentry *btrfs_get_dentry(struct super_block *sb, u64 objectid,
				       u64 root_objectid, u32 generation,
				       int check_generation)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(sb)->fs_info;
	struct btrfs_root *root;
	struct dentry *dentry;
	struct inode *inode;
	struct btrfs_key key;
	int index;
	int err = 0;

	if (objectid < BTRFS_FIRST_FREE_OBJECTID)
		return ERR_PTR(-ESTALE);

	key.objectid = root_objectid;
	btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);
	key.offset = (u64)-1;

	index = srcu_read_lock(&fs_info->subvol_srcu);

	root = btrfs_read_fs_root_no_name(fs_info, &key);
	if (IS_ERR(root)) {
		err = PTR_ERR(root);
		goto fail;
	}

	if (btrfs_root_refs(&root->root_item) == 0) {
		err = -ENOENT;
		goto fail;
	}

	key.objectid = objectid;
	btrfs_set_key_type(&key, BTRFS_INODE_ITEM_KEY);
	key.offset = 0;

	inode = btrfs_iget(sb, &key, root, NULL);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto fail;
	}

	srcu_read_unlock(&fs_info->subvol_srcu, index);

	if (check_generation && generation != inode->i_generation) {
		iput(inode);
		return ERR_PTR(-ESTALE);
	}

	dentry = d_obtain_alias(inode);
	if (!IS_ERR(dentry))
		dentry->d_op = &btrfs_dentry_operations;
	return dentry;
fail:
	srcu_read_unlock(&fs_info->subvol_srcu, index);
	return ERR_PTR(err);
}

static struct dentry *btrfs_fh_to_parent(struct super_block *sb, struct fid *fh,
					 int fh_len, int fh_type)
{
	struct btrfs_fid *fid = (struct btrfs_fid *) fh;
	u64 objectid, root_objectid;
	u32 generation;

	if (fh_type == FILEID_BTRFS_WITH_PARENT) {
		if (fh_len !=  BTRFS_FID_SIZE_CONNECTABLE)
			return NULL;
		root_objectid = fid->root_objectid;
	} else if (fh_type == FILEID_BTRFS_WITH_PARENT_ROOT) {
		if (fh_len != BTRFS_FID_SIZE_CONNECTABLE_ROOT)
			return NULL;
		root_objectid = fid->parent_root_objectid;
	} else
		return NULL;

	objectid = fid->parent_objectid;
	generation = fid->parent_gen;

	return btrfs_get_dentry(sb, objectid, root_objectid, generation, 1);
}

static struct dentry *btrfs_fh_to_dentry(struct super_block *sb, struct fid *fh,
					 int fh_len, int fh_type)
{
	struct btrfs_fid *fid = (struct btrfs_fid *) fh;
	u64 objectid, root_objectid;
	u32 generation;

	if ((fh_type != FILEID_BTRFS_WITH_PARENT ||
	     fh_len != BTRFS_FID_SIZE_CONNECTABLE) &&
	    (fh_type != FILEID_BTRFS_WITH_PARENT_ROOT ||
	     fh_len != BTRFS_FID_SIZE_CONNECTABLE_ROOT) &&
	    (fh_type != FILEID_BTRFS_WITHOUT_PARENT ||
	     fh_len != BTRFS_FID_SIZE_NON_CONNECTABLE))
		return NULL;

	objectid = fid->objectid;
	root_objectid = fid->root_objectid;
	generation = fid->gen;

	return btrfs_get_dentry(sb, objectid, root_objectid, generation, 1);
}

static struct dentry *btrfs_get_parent(struct dentry *child)
{
	struct inode *dir = child->d_inode;
	static struct dentry *dentry;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_root_ref *ref;
	struct btrfs_key key;
	struct btrfs_key found_key;
	int ret;

	path = btrfs_alloc_path();

	if (dir->i_ino == BTRFS_FIRST_FREE_OBJECTID) {
		key.objectid = root->root_key.objectid;
		key.type = BTRFS_ROOT_BACKREF_KEY;
		key.offset = (u64)-1;
		root = root->fs_info->tree_root;
	} else {
		key.objectid = dir->i_ino;
		key.type = BTRFS_INODE_REF_KEY;
		key.offset = (u64)-1;
	}

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto fail;

	BUG_ON(ret == 0);
	if (path->slots[0] == 0) {
		ret = -ENOENT;
		goto fail;
	}

	path->slots[0]--;
	leaf = path->nodes[0];

	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
	if (found_key.objectid != key.objectid || found_key.type != key.type) {
		ret = -ENOENT;
		goto fail;
	}

	if (found_key.type == BTRFS_ROOT_BACKREF_KEY) {
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_root_ref);
		key.objectid = btrfs_root_ref_dirid(leaf, ref);
	} else {
		key.objectid = found_key.offset;
	}
	btrfs_free_path(path);

	if (found_key.type == BTRFS_ROOT_BACKREF_KEY) {
		return btrfs_get_dentry(root->fs_info->sb, key.objectid,
					found_key.offset, 0, 0);
	}

	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	dentry = d_obtain_alias(btrfs_iget(root->fs_info->sb, &key, root, NULL));
	if (!IS_ERR(dentry))
		dentry->d_op = &btrfs_dentry_operations;
	return dentry;
fail:
	btrfs_free_path(path);
	return ERR_PTR(ret);
}

const struct export_operations btrfs_export_ops = {
	.encode_fh	= btrfs_encode_fh,
	.fh_to_dentry	= btrfs_fh_to_dentry,
	.fh_to_parent	= btrfs_fh_to_parent,
	.get_parent	= btrfs_get_parent,
};
