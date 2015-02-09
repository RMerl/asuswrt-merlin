/*
 *   fs/cifs/fscache.c - CIFS filesystem cache interface
 *
 *   Copyright (c) 2010 Novell, Inc.
 *   Author(s): Suresh Jayaraman (sjayaraman@suse.de>
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
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "fscache.h"
#include "cifsglob.h"
#include "cifs_debug.h"
#include "cifs_fs_sb.h"

void cifs_fscache_get_client_cookie(struct TCP_Server_Info *server)
{
	server->fscache =
		fscache_acquire_cookie(cifs_fscache_netfs.primary_index,
				&cifs_fscache_server_index_def, server);
	cFYI(1, "CIFS: get client cookie (0x%p/0x%p)", server,
				server->fscache);
}

void cifs_fscache_release_client_cookie(struct TCP_Server_Info *server)
{
	cFYI(1, "CIFS: release client cookie (0x%p/0x%p)", server,
				server->fscache);
	fscache_relinquish_cookie(server->fscache, 0);
	server->fscache = NULL;
}

void cifs_fscache_get_super_cookie(struct cifsTconInfo *tcon)
{
	struct TCP_Server_Info *server = tcon->ses->server;

	tcon->fscache =
		fscache_acquire_cookie(server->fscache,
				&cifs_fscache_super_index_def, tcon);
	cFYI(1, "CIFS: get superblock cookie (0x%p/0x%p)",
				server->fscache, tcon->fscache);
}

void cifs_fscache_release_super_cookie(struct cifsTconInfo *tcon)
{
	cFYI(1, "CIFS: releasing superblock cookie (0x%p)", tcon->fscache);
	fscache_relinquish_cookie(tcon->fscache, 0);
	tcon->fscache = NULL;
}

static void cifs_fscache_enable_inode_cookie(struct inode *inode)
{
	struct cifsInodeInfo *cifsi = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);

	if (cifsi->fscache)
		return;

	cifsi->fscache = fscache_acquire_cookie(cifs_sb->tcon->fscache,
				&cifs_fscache_inode_object_def,
				cifsi);
	cFYI(1, "CIFS: got FH cookie (0x%p/0x%p)",
			cifs_sb->tcon->fscache, cifsi->fscache);
}

void cifs_fscache_release_inode_cookie(struct inode *inode)
{
	struct cifsInodeInfo *cifsi = CIFS_I(inode);

	if (cifsi->fscache) {
		cFYI(1, "CIFS releasing inode cookie (0x%p)",
				cifsi->fscache);
		fscache_relinquish_cookie(cifsi->fscache, 0);
		cifsi->fscache = NULL;
	}
}

static void cifs_fscache_disable_inode_cookie(struct inode *inode)
{
	struct cifsInodeInfo *cifsi = CIFS_I(inode);

	if (cifsi->fscache) {
		cFYI(1, "CIFS disabling inode cookie (0x%p)",
				cifsi->fscache);
		fscache_relinquish_cookie(cifsi->fscache, 1);
		cifsi->fscache = NULL;
	}
}

void cifs_fscache_set_inode_cookie(struct inode *inode, struct file *filp)
{
	if ((filp->f_flags & O_ACCMODE) != O_RDONLY)
		cifs_fscache_disable_inode_cookie(inode);
	else {
		cifs_fscache_enable_inode_cookie(inode);
		cFYI(1, "CIFS: fscache inode cookie set");
	}
}

void cifs_fscache_reset_inode_cookie(struct inode *inode)
{
	struct cifsInodeInfo *cifsi = CIFS_I(inode);
	struct cifs_sb_info *cifs_sb = CIFS_SB(inode->i_sb);
	struct fscache_cookie *old = cifsi->fscache;

	if (cifsi->fscache) {
		/* retire the current fscache cache and get a new one */
		fscache_relinquish_cookie(cifsi->fscache, 1);

		cifsi->fscache = fscache_acquire_cookie(cifs_sb->tcon->fscache,
					&cifs_fscache_inode_object_def,
					cifsi);
		cFYI(1, "CIFS: new cookie 0x%p oldcookie 0x%p",
				cifsi->fscache, old);
	}
}

int cifs_fscache_release_page(struct page *page, gfp_t gfp)
{
	if (PageFsCache(page)) {
		struct inode *inode = page->mapping->host;
		struct cifsInodeInfo *cifsi = CIFS_I(inode);

		cFYI(1, "CIFS: fscache release page (0x%p/0x%p)",
				page, cifsi->fscache);
		if (!fscache_maybe_release_page(cifsi->fscache, page, gfp))
			return 0;
	}

	return 1;
}

static void cifs_readpage_from_fscache_complete(struct page *page, void *ctx,
						int error)
{
	cFYI(1, "CFS: readpage_from_fscache_complete (0x%p/%d)",
			page, error);
	if (!error)
		SetPageUptodate(page);
	unlock_page(page);
}

/*
 * Retrieve a page from FS-Cache
 */
int __cifs_readpage_from_fscache(struct inode *inode, struct page *page)
{
	int ret;

	cFYI(1, "CIFS: readpage_from_fscache(fsc:%p, p:%p, i:0x%p",
			CIFS_I(inode)->fscache, page, inode);
	ret = fscache_read_or_alloc_page(CIFS_I(inode)->fscache, page,
					 cifs_readpage_from_fscache_complete,
					 NULL,
					 GFP_KERNEL);
	switch (ret) {

	case 0: /* page found in fscache, read submitted */
		cFYI(1, "CIFS: readpage_from_fscache: submitted");
		return ret;
	case -ENOBUFS:	/* page won't be cached */
	case -ENODATA:	/* page not in cache */
		cFYI(1, "CIFS: readpage_from_fscache %d", ret);
		return 1;

	default:
		cERROR(1, "unknown error ret = %d", ret);
	}
	return ret;
}

/*
 * Retrieve a set of pages from FS-Cache
 */
int __cifs_readpages_from_fscache(struct inode *inode,
				struct address_space *mapping,
				struct list_head *pages,
				unsigned *nr_pages)
{
	int ret;

	cFYI(1, "CIFS: __cifs_readpages_from_fscache (0x%p/%u/0x%p)",
			CIFS_I(inode)->fscache, *nr_pages, inode);
	ret = fscache_read_or_alloc_pages(CIFS_I(inode)->fscache, mapping,
					  pages, nr_pages,
					  cifs_readpage_from_fscache_complete,
					  NULL,
					  mapping_gfp_mask(mapping));
	switch (ret) {
	case 0:	/* read submitted to the cache for all pages */
		cFYI(1, "CIFS: readpages_from_fscache: submitted");
		return ret;

	case -ENOBUFS:	/* some pages are not cached and can't be */
	case -ENODATA:	/* some pages are not cached */
		cFYI(1, "CIFS: readpages_from_fscache: no page");
		return 1;

	default:
		cFYI(1, "unknown error ret = %d", ret);
	}

	return ret;
}

void __cifs_readpage_to_fscache(struct inode *inode, struct page *page)
{
	int ret;

	cFYI(1, "CIFS: readpage_to_fscache(fsc: %p, p: %p, i: %p",
			CIFS_I(inode)->fscache, page, inode);
	ret = fscache_write_page(CIFS_I(inode)->fscache, page, GFP_KERNEL);
	if (ret != 0)
		fscache_uncache_page(CIFS_I(inode)->fscache, page);
}

void __cifs_fscache_invalidate_page(struct page *page, struct inode *inode)
{
	struct cifsInodeInfo *cifsi = CIFS_I(inode);
	struct fscache_cookie *cookie = cifsi->fscache;

	cFYI(1, "CIFS: fscache invalidatepage (0x%p/0x%p)", page, cookie);
	fscache_wait_on_page_write(cookie, page);
	fscache_uncache_page(cookie, page);
}
