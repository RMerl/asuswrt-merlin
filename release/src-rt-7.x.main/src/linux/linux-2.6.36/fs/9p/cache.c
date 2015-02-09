/*
 * V9FS cache definitions.
 *
 *  Copyright (C) 2009 by Abhishek Kulkarni <adkulkar@umail.iu.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/jiffies.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <net/9p/9p.h>

#include "v9fs.h"
#include "cache.h"

#define CACHETAG_LEN  11

struct kmem_cache *vcookie_cache;

struct fscache_netfs v9fs_cache_netfs = {
	.name 		= "9p",
	.version 	= 0,
};

static void init_once(void *foo)
{
	struct v9fs_cookie *vcookie = (struct v9fs_cookie *) foo;
	vcookie->fscache = NULL;
	vcookie->qid = NULL;
	inode_init_once(&vcookie->inode);
}

/**
 * v9fs_init_vcookiecache - initialize a cache for vcookies to maintain
 *			    vcookie to inode mapping
 *
 * Returns 0 on success.
 */

static int v9fs_init_vcookiecache(void)
{
	vcookie_cache = kmem_cache_create("vcookie_cache",
					  sizeof(struct v9fs_cookie),
					  0, (SLAB_RECLAIM_ACCOUNT|
					      SLAB_MEM_SPREAD),
					  init_once);
	if (!vcookie_cache)
		return -ENOMEM;

	return 0;
}

/**
 * v9fs_destroy_vcookiecache - destroy the cache of vcookies
 *
 */

static void v9fs_destroy_vcookiecache(void)
{
	kmem_cache_destroy(vcookie_cache);
}

int __v9fs_cache_register(void)
{
	int ret;
	ret = v9fs_init_vcookiecache();
	if (ret < 0)
		return ret;

	return fscache_register_netfs(&v9fs_cache_netfs);
}

void __v9fs_cache_unregister(void)
{
	v9fs_destroy_vcookiecache();
	fscache_unregister_netfs(&v9fs_cache_netfs);
}

/**
 * v9fs_random_cachetag - Generate a random tag to be associated
 *			  with a new cache session.
 *
 * The value of jiffies is used for a fairly randomly cache tag.
 */

static
int v9fs_random_cachetag(struct v9fs_session_info *v9ses)
{
	v9ses->cachetag = kmalloc(CACHETAG_LEN, GFP_KERNEL);
	if (!v9ses->cachetag)
		return -ENOMEM;

	return scnprintf(v9ses->cachetag, CACHETAG_LEN, "%lu", jiffies);
}

static uint16_t v9fs_cache_session_get_key(const void *cookie_netfs_data,
					   void *buffer, uint16_t bufmax)
{
	struct v9fs_session_info *v9ses;
	uint16_t klen = 0;

	v9ses = (struct v9fs_session_info *)cookie_netfs_data;
	P9_DPRINTK(P9_DEBUG_FSC, "session %p buf %p size %u", v9ses,
		   buffer, bufmax);

	if (v9ses->cachetag)
		klen = strlen(v9ses->cachetag);

	if (klen > bufmax)
		return 0;

	memcpy(buffer, v9ses->cachetag, klen);
	P9_DPRINTK(P9_DEBUG_FSC, "cache session tag %s", v9ses->cachetag);
	return klen;
}

const struct fscache_cookie_def v9fs_cache_session_index_def = {
	.name 		= "9P.session",
	.type 		= FSCACHE_COOKIE_TYPE_INDEX,
	.get_key 	= v9fs_cache_session_get_key,
};

void v9fs_cache_session_get_cookie(struct v9fs_session_info *v9ses)
{
	/* If no cache session tag was specified, we generate a random one. */
	if (!v9ses->cachetag)
		v9fs_random_cachetag(v9ses);

	v9ses->fscache = fscache_acquire_cookie(v9fs_cache_netfs.primary_index,
						&v9fs_cache_session_index_def,
						v9ses);
	P9_DPRINTK(P9_DEBUG_FSC, "session %p get cookie %p", v9ses,
		   v9ses->fscache);
}

void v9fs_cache_session_put_cookie(struct v9fs_session_info *v9ses)
{
	P9_DPRINTK(P9_DEBUG_FSC, "session %p put cookie %p", v9ses,
		   v9ses->fscache);
	fscache_relinquish_cookie(v9ses->fscache, 0);
	v9ses->fscache = NULL;
}


static uint16_t v9fs_cache_inode_get_key(const void *cookie_netfs_data,
					 void *buffer, uint16_t bufmax)
{
	const struct v9fs_cookie *vcookie = cookie_netfs_data;
	memcpy(buffer, &vcookie->qid->path, sizeof(vcookie->qid->path));

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p get key %llu", &vcookie->inode,
		   vcookie->qid->path);
	return sizeof(vcookie->qid->path);
}

static void v9fs_cache_inode_get_attr(const void *cookie_netfs_data,
				      uint64_t *size)
{
	const struct v9fs_cookie *vcookie = cookie_netfs_data;
	*size = i_size_read(&vcookie->inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p get attr %llu", &vcookie->inode,
		   *size);
}

static uint16_t v9fs_cache_inode_get_aux(const void *cookie_netfs_data,
					 void *buffer, uint16_t buflen)
{
	const struct v9fs_cookie *vcookie = cookie_netfs_data;
	memcpy(buffer, &vcookie->qid->version, sizeof(vcookie->qid->version));

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p get aux %u", &vcookie->inode,
		   vcookie->qid->version);
	return sizeof(vcookie->qid->version);
}

static enum
fscache_checkaux v9fs_cache_inode_check_aux(void *cookie_netfs_data,
					    const void *buffer,
					    uint16_t buflen)
{
	const struct v9fs_cookie *vcookie = cookie_netfs_data;

	if (buflen != sizeof(vcookie->qid->version))
		return FSCACHE_CHECKAUX_OBSOLETE;

	if (memcmp(buffer, &vcookie->qid->version,
		   sizeof(vcookie->qid->version)))
		return FSCACHE_CHECKAUX_OBSOLETE;

	return FSCACHE_CHECKAUX_OKAY;
}

static void v9fs_cache_inode_now_uncached(void *cookie_netfs_data)
{
	struct v9fs_cookie *vcookie = cookie_netfs_data;
	struct pagevec pvec;
	pgoff_t first;
	int loop, nr_pages;

	pagevec_init(&pvec, 0);
	first = 0;

	for (;;) {
		nr_pages = pagevec_lookup(&pvec, vcookie->inode.i_mapping,
					  first,
					  PAGEVEC_SIZE - pagevec_count(&pvec));
		if (!nr_pages)
			break;

		for (loop = 0; loop < nr_pages; loop++)
			ClearPageFsCache(pvec.pages[loop]);

		first = pvec.pages[nr_pages - 1]->index + 1;

		pvec.nr = nr_pages;
		pagevec_release(&pvec);
		cond_resched();
	}
}

const struct fscache_cookie_def v9fs_cache_inode_index_def = {
	.name		= "9p.inode",
	.type		= FSCACHE_COOKIE_TYPE_DATAFILE,
	.get_key	= v9fs_cache_inode_get_key,
	.get_attr	= v9fs_cache_inode_get_attr,
	.get_aux	= v9fs_cache_inode_get_aux,
	.check_aux	= v9fs_cache_inode_check_aux,
	.now_uncached	= v9fs_cache_inode_now_uncached,
};

void v9fs_cache_inode_get_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie;
	struct v9fs_session_info *v9ses;

	if (!S_ISREG(inode->i_mode))
		return;

	vcookie = v9fs_inode2cookie(inode);
	if (vcookie->fscache)
		return;

	v9ses = v9fs_inode2v9ses(inode);
	vcookie->fscache = fscache_acquire_cookie(v9ses->fscache,
						  &v9fs_cache_inode_index_def,
						  vcookie);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p get cookie %p", inode,
		   vcookie->fscache);
}

void v9fs_cache_inode_put_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	if (!vcookie->fscache)
		return;
	P9_DPRINTK(P9_DEBUG_FSC, "inode %p put cookie %p", inode,
		   vcookie->fscache);

	fscache_relinquish_cookie(vcookie->fscache, 0);
	vcookie->fscache = NULL;
}

void v9fs_cache_inode_flush_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	if (!vcookie->fscache)
		return;
	P9_DPRINTK(P9_DEBUG_FSC, "inode %p flush cookie %p", inode,
		   vcookie->fscache);

	fscache_relinquish_cookie(vcookie->fscache, 1);
	vcookie->fscache = NULL;
}

void v9fs_cache_inode_set_cookie(struct inode *inode, struct file *filp)
{
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);
	struct p9_fid *fid;

	if (!vcookie->fscache)
		return;

	spin_lock(&vcookie->lock);
	fid = filp->private_data;
	if ((filp->f_flags & O_ACCMODE) != O_RDONLY)
		v9fs_cache_inode_flush_cookie(inode);
	else
		v9fs_cache_inode_get_cookie(inode);

	spin_unlock(&vcookie->lock);
}

void v9fs_cache_inode_reset_cookie(struct inode *inode)
{
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);
	struct v9fs_session_info *v9ses;
	struct fscache_cookie *old;

	if (!vcookie->fscache)
		return;

	old = vcookie->fscache;

	spin_lock(&vcookie->lock);
	fscache_relinquish_cookie(vcookie->fscache, 1);

	v9ses = v9fs_inode2v9ses(inode);
	vcookie->fscache = fscache_acquire_cookie(v9ses->fscache,
						  &v9fs_cache_inode_index_def,
						  vcookie);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p revalidating cookie old %p new %p",
		   inode, old, vcookie->fscache);

	spin_unlock(&vcookie->lock);
}

int __v9fs_fscache_release_page(struct page *page, gfp_t gfp)
{
	struct inode *inode = page->mapping->host;
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	BUG_ON(!vcookie->fscache);

	return fscache_maybe_release_page(vcookie->fscache, page, gfp);
}

void __v9fs_fscache_invalidate_page(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	BUG_ON(!vcookie->fscache);

	if (PageFsCache(page)) {
		fscache_wait_on_page_write(vcookie->fscache, page);
		BUG_ON(!PageLocked(page));
		fscache_uncache_page(vcookie->fscache, page);
	}
}

static void v9fs_vfs_readpage_complete(struct page *page, void *data,
				       int error)
{
	if (!error)
		SetPageUptodate(page);

	unlock_page(page);
}

/**
 * __v9fs_readpage_from_fscache - read a page from cache
 *
 * Returns 0 if the pages are in cache and a BIO is submitted,
 * 1 if the pages are not in cache and -error otherwise.
 */

int __v9fs_readpage_from_fscache(struct inode *inode, struct page *page)
{
	int ret;
	const struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p page %p", inode, page);
	if (!vcookie->fscache)
		return -ENOBUFS;

	ret = fscache_read_or_alloc_page(vcookie->fscache,
					 page,
					 v9fs_vfs_readpage_complete,
					 NULL,
					 GFP_KERNEL);
	switch (ret) {
	case -ENOBUFS:
	case -ENODATA:
		P9_DPRINTK(P9_DEBUG_FSC, "page/inode not in cache %d", ret);
		return 1;
	case 0:
		P9_DPRINTK(P9_DEBUG_FSC, "BIO submitted");
		return ret;
	default:
		P9_DPRINTK(P9_DEBUG_FSC, "ret %d", ret);
		return ret;
	}
}

/**
 * __v9fs_readpages_from_fscache - read multiple pages from cache
 *
 * Returns 0 if the pages are in cache and a BIO is submitted,
 * 1 if the pages are not in cache and -error otherwise.
 */

int __v9fs_readpages_from_fscache(struct inode *inode,
				  struct address_space *mapping,
				  struct list_head *pages,
				  unsigned *nr_pages)
{
	int ret;
	const struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p pages %u", inode, *nr_pages);
	if (!vcookie->fscache)
		return -ENOBUFS;

	ret = fscache_read_or_alloc_pages(vcookie->fscache,
					  mapping, pages, nr_pages,
					  v9fs_vfs_readpage_complete,
					  NULL,
					  mapping_gfp_mask(mapping));
	switch (ret) {
	case -ENOBUFS:
	case -ENODATA:
		P9_DPRINTK(P9_DEBUG_FSC, "pages/inodes not in cache %d", ret);
		return 1;
	case 0:
		BUG_ON(!list_empty(pages));
		BUG_ON(*nr_pages != 0);
		P9_DPRINTK(P9_DEBUG_FSC, "BIO submitted");
		return ret;
	default:
		P9_DPRINTK(P9_DEBUG_FSC, "ret %d", ret);
		return ret;
	}
}

/**
 * __v9fs_readpage_to_fscache - write a page to the cache
 *
 */

void __v9fs_readpage_to_fscache(struct inode *inode, struct page *page)
{
	int ret;
	const struct v9fs_cookie *vcookie = v9fs_inode2cookie(inode);

	P9_DPRINTK(P9_DEBUG_FSC, "inode %p page %p", inode, page);
	ret = fscache_write_page(vcookie->fscache, page, GFP_KERNEL);
	P9_DPRINTK(P9_DEBUG_FSC, "ret =  %d", ret);
	if (ret != 0)
		v9fs_uncache_page(inode, page);
}
