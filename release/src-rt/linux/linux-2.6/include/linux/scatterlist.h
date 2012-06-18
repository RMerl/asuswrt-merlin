#ifndef _LINUX_SCATTERLIST_H
#define _LINUX_SCATTERLIST_H

#include <asm/scatterlist.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/string.h>

/**
 * sg_set_page - Set sg entry to point at given page
 * @sg:		 SG entry
 * @page:	 The page
 *
 * Description:
 *   Use this function to set an sg entry pointing at a page, never assign
 *   the page directly. We encode sg table information in the lower bits
 *   of the page pointer. See sg_page() for looking up the page belonging
 *   to an sg entry.
 *
 **/
static inline void sg_set_page(struct scatterlist *sg, struct page *page)
{
	sg->page = page;
}

#define sg_page(sg)	((sg)->page)

static inline void sg_set_buf(struct scatterlist *sg, const void *buf,
			      unsigned int buflen)
{
	sg_set_page(sg, virt_to_page(buf));
	sg->offset = offset_in_page(buf);
	sg->length = buflen;
}

#define sg_next(sg)		((sg) + 1)
#define sg_last(sg, nents)	(&(sg[(nents) - 1]))

/*
 * Loop over each sg element, following the pointer to a new list if necessary
 */
#define for_each_sg(sglist, sg, nr, __i)	\
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

/**
 * sg_mark_end - Mark the end of the scatterlist
 * @sgl:	Scatterlist
 * @nents:	Number of entries in sgl
 *
 * Description:
 *   Marks the last entry as the termination point for sg_next()
 *
 **/
static inline void sg_mark_end(struct scatterlist *sgl, unsigned int nents)
{
}

static inline void __sg_mark_end(struct scatterlist *sg)
{
}

/**
 * sg_init_one - Initialize a single entry sg list
 * @sg:		 SG entry
 * @buf:	 Virtual address for IO
 * @buflen:	 IO length
 *
 * Notes:
 *   This should not be used on a single entry that is part of a larger
 *   table. Use sg_init_table() for that.
 *
 **/
static inline void sg_init_one(struct scatterlist *sg, const void *buf,
			       unsigned int buflen)
{
	memset(sg, 0, sizeof(*sg));
	sg_mark_end(sg, 1);
	sg_set_buf(sg, buf, buflen);
}

/**
 * sg_init_table - Initialize SG table
 * @sgl:	   The SG table
 * @nents:	   Number of entries in table
 *
 * Notes:
 *   If this is part of a chained sg table, sg_mark_end() should be
 *   used only on the last table part.
 *
 **/
static inline void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);
	sg_mark_end(sgl, nents);
}

/**
 * sg_phys - Return physical address of an sg entry
 * @sg:	     SG entry
 *
 * Description:
 *   This calls page_to_phys() on the page in this sg entry, and adds the
 *   sg offset. The caller must know that it is legal to call page_to_phys()
 *   on the sg page.
 *
 **/
static inline unsigned long sg_phys(struct scatterlist *sg)
{
	return page_to_phys(sg_page(sg)) + sg->offset;
}

/**
 * sg_virt - Return virtual address of an sg entry
 * @sg:	     SG entry
 *
 * Description:
 *   This calls page_address() on the page in this sg entry, and adds the
 *   sg offset. The caller must know that the sg page has a valid virtual
 *   mapping.
 *
 **/
static inline void *sg_virt(struct scatterlist *sg)
{
	return page_address(sg_page(sg)) + sg->offset;
}

#endif /* _LINUX_SCATTERLIST_H */
