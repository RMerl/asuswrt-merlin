/*
 * Block data types and constants.  Directly include this file only to
 * break include dependency loop.
 */
#ifndef __LINUX_BLK_TYPES_H
#define __LINUX_BLK_TYPES_H

#ifdef CONFIG_BLOCK

#include <linux/types.h>

struct bio_set;
struct bio;
struct bio_integrity_payload;
struct page;
struct block_device;
typedef void (bio_end_io_t) (struct bio *, int);
typedef void (bio_destructor_t) (struct bio *);

/*
 * was unsigned short, but we might as well be ready for > 64kB I/O pages
 */
struct bio_vec {
	struct page	*bv_page;
	unsigned int	bv_len;
	unsigned int	bv_offset;
};

/*
 * main unit of I/O for the block layer and lower layers (ie drivers and
 * stacking drivers)
 */
struct bio {
	sector_t		bi_sector;	/* device address in 512 byte
						   sectors */
	struct bio		*bi_next;	/* request queue link */
	struct block_device	*bi_bdev;
	unsigned long		bi_flags;	/* status, command, etc */
	unsigned long		bi_rw;		/* bottom bits READ/WRITE,
						 * top bits priority
						 */

	unsigned short		bi_vcnt;	/* how many bio_vec's */
	unsigned short		bi_idx;		/* current index into bvl_vec */

	/* Number of segments in this BIO after
	 * physical address coalescing is performed.
	 */
	unsigned int		bi_phys_segments;

	unsigned int		bi_size;	/* residual I/O count */

	/*
	 * To keep track of the max segment size, we account for the
	 * sizes of the first and last mergeable segments in this bio.
	 */
	unsigned int		bi_seg_front_size;
	unsigned int		bi_seg_back_size;

	unsigned int		bi_max_vecs;	/* max bvl_vecs we can hold */

	unsigned int		bi_comp_cpu;	/* completion CPU */

	atomic_t		bi_cnt;		/* pin count */

	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	bio_end_io_t		*bi_end_io;

	void			*bi_private;
#if defined(CONFIG_BLK_DEV_INTEGRITY)
	struct bio_integrity_payload *bi_integrity;  /* data integrity */
#endif

	bio_destructor_t	*bi_destructor;	/* destructor */

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[0];
};

/*
 * bio flags
 */
#define BIO_UPTODATE	0	/* ok after I/O completion */
#define BIO_RW_BLOCK	1	/* RW_AHEAD set, and read/write would block */
#define BIO_EOF		2	/* out-out-bounds error */
#define BIO_SEG_VALID	3	/* bi_phys_segments valid */
#define BIO_CLONED	4	/* doesn't own data */
#define BIO_BOUNCED	5	/* bio is a bounce bio */
#define BIO_USER_MAPPED 6	/* contains user pages */
#define BIO_EOPNOTSUPP	7	/* not supported */
#define BIO_CPU_AFFINE	8	/* complete bio on same CPU as submitted */
#define BIO_NULL_MAPPED 9	/* contains invalid user pages */
#define BIO_FS_INTEGRITY 10	/* fs owns integrity data, not block layer */
#define BIO_QUIET	11	/* Make BIO Quiet */
#define bio_flagged(bio, flag)	((bio)->bi_flags & (1 << (flag)))

/*
 * top 4 bits of bio flags indicate the pool this bio came from
 */
#define BIO_POOL_BITS		(4)
#define BIO_POOL_NONE		((1UL << BIO_POOL_BITS) - 1)
#define BIO_POOL_OFFSET		(BITS_PER_LONG - BIO_POOL_BITS)
#define BIO_POOL_MASK		(1UL << BIO_POOL_OFFSET)
#define BIO_POOL_IDX(bio)	((bio)->bi_flags >> BIO_POOL_OFFSET)

#endif /* CONFIG_BLOCK */

/*
 * Request flags.  For use in the cmd_flags field of struct request, and in
 * bi_rw of struct bio.  Note that some flags are only valid in either one.
 */
enum rq_flag_bits {
	/* common flags */
	__REQ_WRITE,		/* not set, read. set, write */
	__REQ_FAILFAST_DEV,	/* no driver retries of device errors */
	__REQ_FAILFAST_TRANSPORT, /* no driver retries of transport errors */
	__REQ_FAILFAST_DRIVER,	/* no driver retries of driver errors */

	__REQ_HARDBARRIER,	/* may not be passed by drive either */
	__REQ_SYNC,		/* request is sync (sync write or read) */
	__REQ_META,		/* metadata io request */
	__REQ_DISCARD,		/* request to discard sectors */
	__REQ_NOIDLE,		/* don't anticipate more IO after this one */

	/* bio only flags */
	__REQ_UNPLUG,		/* unplug the immediately after submission */
	__REQ_RAHEAD,		/* read ahead, can fail anytime */

	/* request only flags */
	__REQ_SORTED,		/* elevator knows about this request */
	__REQ_SOFTBARRIER,	/* may not be passed by ioscheduler */
	__REQ_FUA,		/* forced unit access */
	__REQ_NOMERGE,		/* don't touch this for merging */
	__REQ_STARTED,		/* drive already may have started this one */
	__REQ_DONTPREP,		/* don't call prep for this one */
	__REQ_QUEUED,		/* uses queueing */
	__REQ_ELVPRIV,		/* elevator private data attached */
	__REQ_FAILED,		/* set if the request failed */
	__REQ_QUIET,		/* don't worry about errors */
	__REQ_PREEMPT,		/* set for "ide_preempt" requests */
	__REQ_ORDERED_COLOR,	/* is before or after barrier */
	__REQ_ALLOCED,		/* request came from our alloc pool */
	__REQ_COPY_USER,	/* contains copies of user pages */
	__REQ_INTEGRITY,	/* integrity metadata has been remapped */
	__REQ_FLUSH,		/* request for cache flush */
	__REQ_IO_STAT,		/* account I/O stat */
	__REQ_MIXED_MERGE,	/* merge of different types, fail separately */
	__REQ_SECURE,		/* secure discard (used with __REQ_DISCARD) */
	__REQ_NR_BITS,		/* stops here */
};

#define REQ_WRITE		(1 << __REQ_WRITE)
#define REQ_FAILFAST_DEV	(1 << __REQ_FAILFAST_DEV)
#define REQ_FAILFAST_TRANSPORT	(1 << __REQ_FAILFAST_TRANSPORT)
#define REQ_FAILFAST_DRIVER	(1 << __REQ_FAILFAST_DRIVER)
#define REQ_HARDBARRIER		(1 << __REQ_HARDBARRIER)
#define REQ_SYNC		(1 << __REQ_SYNC)
#define REQ_META		(1 << __REQ_META)
#define REQ_DISCARD		(1 << __REQ_DISCARD)
#define REQ_NOIDLE		(1 << __REQ_NOIDLE)

#define REQ_FAILFAST_MASK \
	(REQ_FAILFAST_DEV | REQ_FAILFAST_TRANSPORT | REQ_FAILFAST_DRIVER)
#define REQ_COMMON_MASK \
	(REQ_WRITE | REQ_FAILFAST_MASK | REQ_HARDBARRIER | REQ_SYNC | \
	 REQ_META| REQ_DISCARD | REQ_NOIDLE)

#define REQ_UNPLUG		(1 << __REQ_UNPLUG)
#define REQ_RAHEAD		(1 << __REQ_RAHEAD)

#define REQ_SORTED		(1 << __REQ_SORTED)
#define REQ_SOFTBARRIER		(1 << __REQ_SOFTBARRIER)
#define REQ_FUA			(1 << __REQ_FUA)
#define REQ_NOMERGE		(1 << __REQ_NOMERGE)
#define REQ_STARTED		(1 << __REQ_STARTED)
#define REQ_DONTPREP		(1 << __REQ_DONTPREP)
#define REQ_QUEUED		(1 << __REQ_QUEUED)
#define REQ_ELVPRIV		(1 << __REQ_ELVPRIV)
#define REQ_FAILED		(1 << __REQ_FAILED)
#define REQ_QUIET		(1 << __REQ_QUIET)
#define REQ_PREEMPT		(1 << __REQ_PREEMPT)
#define REQ_ORDERED_COLOR	(1 << __REQ_ORDERED_COLOR)
#define REQ_ALLOCED		(1 << __REQ_ALLOCED)
#define REQ_COPY_USER		(1 << __REQ_COPY_USER)
#define REQ_INTEGRITY		(1 << __REQ_INTEGRITY)
#define REQ_FLUSH		(1 << __REQ_FLUSH)
#define REQ_IO_STAT		(1 << __REQ_IO_STAT)
#define REQ_MIXED_MERGE		(1 << __REQ_MIXED_MERGE)
#define REQ_SECURE		(1 << __REQ_SECURE)

#endif /* __LINUX_BLK_TYPES_H */
