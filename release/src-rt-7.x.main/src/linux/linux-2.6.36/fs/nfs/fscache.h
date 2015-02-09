/* NFS filesystem cache interface definitions
 *
 * Copyright (C) 2008 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _NFS_FSCACHE_H
#define _NFS_FSCACHE_H

#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/nfs4_mount.h>
#include <linux/fscache.h>

#ifdef CONFIG_NFS_FSCACHE

/*
 * set of NFS FS-Cache objects that form a superblock key
 */
struct nfs_fscache_key {
	struct rb_node		node;
	struct nfs_client	*nfs_client;	/* the server */

	/* the elements of the unique key - as used by nfs_compare_super() and
	 * nfs_compare_mount_options() to distinguish superblocks */
	struct {
		struct {
			unsigned long	s_flags;	/* various flags
							 * (& NFS_MS_MASK) */
		} super;

		struct {
			struct nfs_fsid fsid;
			int		flags;
			unsigned int	rsize;		/* read size */
			unsigned int	wsize;		/* write size */
			unsigned int	acregmin;	/* attr cache timeouts */
			unsigned int	acregmax;
			unsigned int	acdirmin;
			unsigned int	acdirmax;
		} nfs_server;

		struct {
			rpc_authflavor_t au_flavor;
		} rpc_auth;

		/* uniquifier - can be used if nfs_server.flags includes
		 * NFS_MOUNT_UNSHARED  */
		u8 uniq_len;
		char uniquifier[0];
	} key;
};

/*
 * fscache-index.c
 */
extern struct fscache_netfs nfs_fscache_netfs;
extern const struct fscache_cookie_def nfs_fscache_server_index_def;
extern const struct fscache_cookie_def nfs_fscache_super_index_def;
extern const struct fscache_cookie_def nfs_fscache_inode_object_def;

extern int nfs_fscache_register(void);
extern void nfs_fscache_unregister(void);

/*
 * fscache.c
 */
extern void nfs_fscache_get_client_cookie(struct nfs_client *);
extern void nfs_fscache_release_client_cookie(struct nfs_client *);

extern void nfs_fscache_get_super_cookie(struct super_block *,
					 const char *,
					 struct nfs_clone_mount *);
extern void nfs_fscache_release_super_cookie(struct super_block *);

extern void nfs_fscache_init_inode_cookie(struct inode *);
extern void nfs_fscache_release_inode_cookie(struct inode *);
extern void nfs_fscache_zap_inode_cookie(struct inode *);
extern void nfs_fscache_set_inode_cookie(struct inode *, struct file *);
extern void nfs_fscache_reset_inode_cookie(struct inode *);

extern void __nfs_fscache_invalidate_page(struct page *, struct inode *);
extern int nfs_fscache_release_page(struct page *, gfp_t);

extern int __nfs_readpage_from_fscache(struct nfs_open_context *,
				       struct inode *, struct page *);
extern int __nfs_readpages_from_fscache(struct nfs_open_context *,
					struct inode *, struct address_space *,
					struct list_head *, unsigned *);
extern void __nfs_readpage_to_fscache(struct inode *, struct page *, int);

/*
 * wait for a page to complete writing to the cache
 */
static inline void nfs_fscache_wait_on_page_write(struct nfs_inode *nfsi,
						  struct page *page)
{
	if (PageFsCache(page))
		fscache_wait_on_page_write(nfsi->fscache, page);
}

/*
 * release the caching state associated with a page if undergoing complete page
 * invalidation
 */
static inline void nfs_fscache_invalidate_page(struct page *page,
					       struct inode *inode)
{
	if (PageFsCache(page))
		__nfs_fscache_invalidate_page(page, inode);
}

/*
 * Retrieve a page from an inode data storage object.
 */
static inline int nfs_readpage_from_fscache(struct nfs_open_context *ctx,
					    struct inode *inode,
					    struct page *page)
{
	if (NFS_I(inode)->fscache)
		return __nfs_readpage_from_fscache(ctx, inode, page);
	return -ENOBUFS;
}

/*
 * Retrieve a set of pages from an inode data storage object.
 */
static inline int nfs_readpages_from_fscache(struct nfs_open_context *ctx,
					     struct inode *inode,
					     struct address_space *mapping,
					     struct list_head *pages,
					     unsigned *nr_pages)
{
	if (NFS_I(inode)->fscache)
		return __nfs_readpages_from_fscache(ctx, inode, mapping, pages,
						    nr_pages);
	return -ENOBUFS;
}

/*
 * Store a page newly fetched from the server in an inode data storage object
 * in the cache.
 */
static inline void nfs_readpage_to_fscache(struct inode *inode,
					   struct page *page,
					   int sync)
{
	if (PageFsCache(page))
		__nfs_readpage_to_fscache(inode, page, sync);
}

/*
 * indicate the client caching state as readable text
 */
static inline const char *nfs_server_fscache_state(struct nfs_server *server)
{
	if (server->fscache && (server->options & NFS_OPTION_FSCACHE))
		return "yes";
	return "no ";
}


#else /* CONFIG_NFS_FSCACHE */
static inline int nfs_fscache_register(void) { return 0; }
static inline void nfs_fscache_unregister(void) {}

static inline void nfs_fscache_get_client_cookie(struct nfs_client *clp) {}
static inline void nfs_fscache_release_client_cookie(struct nfs_client *clp) {}

static inline void nfs_fscache_get_super_cookie(
	struct super_block *sb,
	const char *uniq,
	struct nfs_clone_mount *mntdata)
{
}
static inline void nfs_fscache_release_super_cookie(struct super_block *sb) {}

static inline void nfs_fscache_init_inode_cookie(struct inode *inode) {}
static inline void nfs_fscache_release_inode_cookie(struct inode *inode) {}
static inline void nfs_fscache_zap_inode_cookie(struct inode *inode) {}
static inline void nfs_fscache_set_inode_cookie(struct inode *inode,
						struct file *filp) {}
static inline void nfs_fscache_reset_inode_cookie(struct inode *inode) {}

static inline int nfs_fscache_release_page(struct page *page, gfp_t gfp)
{
	return 1; /* True: may release page */
}
static inline void nfs_fscache_invalidate_page(struct page *page,
					       struct inode *inode) {}
static inline void nfs_fscache_wait_on_page_write(struct nfs_inode *nfsi,
						  struct page *page) {}

static inline int nfs_readpage_from_fscache(struct nfs_open_context *ctx,
					    struct inode *inode,
					    struct page *page)
{
	return -ENOBUFS;
}
static inline int nfs_readpages_from_fscache(struct nfs_open_context *ctx,
					     struct inode *inode,
					     struct address_space *mapping,
					     struct list_head *pages,
					     unsigned *nr_pages)
{
	return -ENOBUFS;
}
static inline void nfs_readpage_to_fscache(struct inode *inode,
					   struct page *page, int sync) {}

static inline const char *nfs_server_fscache_state(struct nfs_server *server)
{
	return "no ";
}

#endif /* CONFIG_NFS_FSCACHE */
#endif /* _NFS_FSCACHE_H */
