/*
 * linux/include/linux/nfs_page.h
 *
 * Copyright (C) 2000 Trond Myklebust
 *
 * NFS page cache wrapper.
 */

#ifndef _LINUX_NFS_PAGE_H
#define _LINUX_NFS_PAGE_H


#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/wait.h>
#include <linux/sunrpc/auth.h>
#include <linux/nfs_xdr.h>

#include <linux/kref.h>

/*
 * Valid flags for the radix tree
 */
#define NFS_PAGE_TAG_LOCKED	0
#define NFS_PAGE_TAG_COMMIT	1

/*
 * Valid flags for a dirty buffer
 */
enum {
	PG_BUSY = 0,
	PG_CLEAN,
	PG_NEED_COMMIT,
	PG_NEED_RESCHED,
};

struct nfs_inode;
struct nfs_page {
	struct list_head	wb_list;	/* Defines state of page: */
	struct page		*wb_page;	/* page to read in/write out */
	struct nfs_open_context	*wb_context;	/* File state context info */
	struct nfs_lock_context	*wb_lock_context;	/* lock context info */
	atomic_t		wb_complete;	/* i/os we're waiting for */
	pgoff_t			wb_index;	/* Offset >> PAGE_CACHE_SHIFT */
	unsigned int		wb_offset,	/* Offset & ~PAGE_CACHE_MASK */
				wb_pgbase,	/* Start of page data */
				wb_bytes;	/* Length of request */
	struct kref		wb_kref;	/* reference count */
	unsigned long		wb_flags;
	struct nfs_writeverf	wb_verf;	/* Commit cookie */
};

struct nfs_pageio_descriptor {
	struct list_head	pg_list;
	unsigned long		pg_bytes_written;
	size_t			pg_count;
	size_t			pg_bsize;
	unsigned int		pg_base;

	struct inode		*pg_inode;
	int			(*pg_doio)(struct inode *, struct list_head *, unsigned int, size_t, int);
	int 			pg_ioflags;
	int			pg_error;
};

#define NFS_WBACK_BUSY(req)	(test_bit(PG_BUSY,&(req)->wb_flags))

extern	struct nfs_page *nfs_create_request(struct nfs_open_context *ctx,
					    struct inode *inode,
					    struct page *page,
					    unsigned int offset,
					    unsigned int count);
extern	void nfs_clear_request(struct nfs_page *req);
extern	void nfs_release_request(struct nfs_page *req);


extern	int nfs_scan_list(struct nfs_inode *nfsi, struct list_head *dst,
			  pgoff_t idx_start, unsigned int npages, int tag);
extern	void nfs_pageio_init(struct nfs_pageio_descriptor *desc,
			     struct inode *inode,
			     int (*doio)(struct inode *, struct list_head *, unsigned int, size_t, int),
			     size_t bsize,
			     int how);
extern	int nfs_pageio_add_request(struct nfs_pageio_descriptor *,
				   struct nfs_page *);
extern	void nfs_pageio_complete(struct nfs_pageio_descriptor *desc);
extern	void nfs_pageio_cond_complete(struct nfs_pageio_descriptor *, pgoff_t);
extern  int nfs_wait_on_request(struct nfs_page *);
extern	void nfs_unlock_request(struct nfs_page *req);
extern	int nfs_set_page_tag_locked(struct nfs_page *req);
extern  void nfs_clear_page_tag_locked(struct nfs_page *req);


/*
 * Lock the page of an asynchronous request without getting a new reference
 */
static inline int
nfs_lock_request_dontget(struct nfs_page *req)
{
	return !test_and_set_bit(PG_BUSY, &req->wb_flags);
}

/**
 * nfs_list_add_request - Insert a request into a list
 * @req: request
 * @head: head of list into which to insert the request.
 */
static inline void
nfs_list_add_request(struct nfs_page *req, struct list_head *head)
{
	list_add_tail(&req->wb_list, head);
}


/**
 * nfs_list_remove_request - Remove a request from its wb_list
 * @req: request
 */
static inline void
nfs_list_remove_request(struct nfs_page *req)
{
	if (list_empty(&req->wb_list))
		return;
	list_del_init(&req->wb_list);
}

static inline struct nfs_page *
nfs_list_entry(struct list_head *head)
{
	return list_entry(head, struct nfs_page, wb_list);
}

static inline
loff_t req_offset(struct nfs_page *req)
{
	return (((loff_t)req->wb_index) << PAGE_CACHE_SHIFT) + req->wb_offset;
}

#endif /* _LINUX_NFS_PAGE_H */
