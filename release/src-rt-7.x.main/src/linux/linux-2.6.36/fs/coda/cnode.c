/* cnode related routines for the coda kernel code
   (C) 1996 Peter Braam
   */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/time.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>

static inline int coda_fideq(struct CodaFid *fid1, struct CodaFid *fid2)
{
	return memcmp(fid1, fid2, sizeof(*fid1)) == 0;
}

static const struct inode_operations coda_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
	.setattr	= coda_setattr,
};

/* cnode.c */
static void coda_fill_inode(struct inode *inode, struct coda_vattr *attr)
{
        coda_vattr_to_iattr(inode, attr);

        if (S_ISREG(inode->i_mode)) {
                inode->i_op = &coda_file_inode_operations;
                inode->i_fop = &coda_file_operations;
        } else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &coda_dir_inode_operations;
                inode->i_fop = &coda_dir_operations;
        } else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &coda_symlink_inode_operations;
		inode->i_data.a_ops = &coda_symlink_aops;
		inode->i_mapping = &inode->i_data;
	} else
                init_special_inode(inode, inode->i_mode, huge_decode_dev(attr->va_rdev));
}

static int coda_test_inode(struct inode *inode, void *data)
{
	struct CodaFid *fid = (struct CodaFid *)data;
	return coda_fideq(&(ITOC(inode)->c_fid), fid);
}

static int coda_set_inode(struct inode *inode, void *data)
{
	struct CodaFid *fid = (struct CodaFid *)data;
	ITOC(inode)->c_fid = *fid;
	return 0;
}

struct inode * coda_iget(struct super_block * sb, struct CodaFid * fid,
			 struct coda_vattr * attr)
{
	struct inode *inode;
	struct coda_inode_info *cii;
	unsigned long hash = coda_f2i(fid);

	inode = iget5_locked(sb, hash, coda_test_inode, coda_set_inode, fid);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		cii = ITOC(inode);
		/* we still need to set i_ino for things like stat(2) */
		inode->i_ino = hash;
		cii->c_mapcount = 0;
		unlock_new_inode(inode);
	}

	/* always replace the attributes, type might have changed */
	coda_fill_inode(inode, attr);
	return inode;
}

/* this is effectively coda_iget:
   - get attributes (might be cached)
   - get the inode for the fid using vfs iget
   - link the two up if this is needed
   - fill in the attributes
*/
int coda_cnode_make(struct inode **inode, struct CodaFid *fid, struct super_block *sb)
{
        struct coda_vattr attr;
        int error;
        
	/* We get inode numbers from Venus -- see venus source */
	error = venus_getattr(sb, fid, &attr);
	if ( error ) {
	    *inode = NULL;
	    return error;
	} 

	*inode = coda_iget(sb, fid, &attr);
	if ( IS_ERR(*inode) ) {
		printk("coda_cnode_make: coda_iget failed\n");
                return PTR_ERR(*inode);
        }
	return 0;
}


void coda_replace_fid(struct inode *inode, struct CodaFid *oldfid, 
		      struct CodaFid *newfid)
{
	struct coda_inode_info *cii;
	unsigned long hash = coda_f2i(newfid);
	
	cii = ITOC(inode);

	BUG_ON(!coda_fideq(&cii->c_fid, oldfid));

	/* replace fid and rehash inode */
	remove_inode_hash(inode);
	cii->c_fid = *newfid;
	inode->i_ino = hash;
	__insert_inode_hash(inode, hash);
}

/* convert a fid to an inode. */
struct inode *coda_fid_to_inode(struct CodaFid *fid, struct super_block *sb) 
{
	struct inode *inode;
	unsigned long hash = coda_f2i(fid);

	if ( !sb ) {
		printk("coda_fid_to_inode: no sb!\n");
		return NULL;
	}

	inode = ilookup5(sb, hash, coda_test_inode, fid);
	if ( !inode )
		return NULL;

	/* we should never see newly created inodes because we intentionally
	 * fail in the initialization callback */
	BUG_ON(inode->i_state & I_NEW);

	return inode;
}

/* the CONTROL inode is made without asking attributes from Venus */
int coda_cnode_makectl(struct inode **inode, struct super_block *sb)
{
	int error = -ENOMEM;

	*inode = new_inode(sb);
	if (*inode) {
		(*inode)->i_ino = CTL_INO;
		(*inode)->i_op = &coda_ioctl_inode_operations;
		(*inode)->i_fop = &coda_ioctl_operations;
		(*inode)->i_mode = 0444;
		error = 0;
	}

	return error;
}
