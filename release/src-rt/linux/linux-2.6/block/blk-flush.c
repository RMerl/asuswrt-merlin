/*
 * Functions to sequence FLUSH and FUA writes.
 *
 * Copyright (C) 2011		Max Planck Institute for Gravitational Physics
 * Copyright (C) 2011		Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 *
 * REQ_{FLUSH|FUA} requests are decomposed to sequences consisted of three
 * optional steps - PREFLUSH, DATA and POSTFLUSH - according to the request
 * properties and hardware capability.
 *
 * If a request doesn't have data, only REQ_FLUSH makes sense, which
 * indicates a simple flush request.  If there is data, REQ_FLUSH indicates
 * that the device cache should be flushed before the data is executed, and
 * REQ_FUA means that the data must be on non-volatile media on request
 * completion.
 *
 * If the device doesn't have writeback cache, FLUSH and FUA don't make any
 * difference.  The requests are either completed immediately if there's no
 * data or executed as normal requests otherwise.
 *
 * If the device has writeback cache and supports FUA, REQ_FLUSH is
 * translated to PREFLUSH but REQ_FUA is passed down directly with DATA.
 *
 * If the device has writeback cache and doesn't support FUA, REQ_FLUSH is
 * translated to PREFLUSH and REQ_FUA to POSTFLUSH.
 *
 * The actual execution of flush is double buffered.  Whenever a request
 * needs to execute PRE or POSTFLUSH, it queues at
 * q->flush_queue[q->flush_pending_idx].  Once certain criteria are met, a
 * flush is issued and the pending_idx is toggled.  When the flush
 * completes, all the requests which were pending are proceeded to the next
 * step.  This allows arbitrary merging of different types of FLUSH/FUA
 * requests.
 *
 * Currently, the following conditions are used to determine when to issue
 * flush.
 *
 * C1. At any given time, only one flush shall be in progress.  This makes
 *     double buffering sufficient.
 *
 * C2. Flush is deferred if any request is executing DATA of its sequence.
 *     This avoids issuing separate POSTFLUSHes for requests which shared
 *     PREFLUSH.
 *
 * C3. The second condition is ignored if there is a request which has
 *     waited longer than FLUSH_PENDING_TIMEOUT.  This is to avoid
 *     starvation in the unlikely case where there are continuous stream of
 *     FUA (without FLUSH) requests.
 *
 * For devices which support FUA, it isn't clear whether C2 (and thus C3)
 * is beneficial.
 *
 * Note that a sequenced FLUSH/FUA request with DATA is completed twice.
 * Once while executing DATA and again after the whole sequence is
 * complete.  The first completion updates the contained bio but doesn't
 * finish it so that the bio submitter is notified only after the whole
 * sequence is complete.  This is implemented by testing REQ_FLUSH_SEQ in
 * req_bio_endio().
 *
 * The above peculiarity requires that each FLUSH/FUA request has only one
 * bio attached to it, which is guaranteed as they aren't allowed to be
 * merged in the usual way.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/gfp.h>

#include "blk.h"

/* FLUSH/FUA sequences */
enum {
	REQ_FSEQ_PREFLUSH	= (1 << 0), /* pre-flushing in progress */
	REQ_FSEQ_DATA		= (1 << 1), /* data write in progress */
	REQ_FSEQ_POSTFLUSH	= (1 << 2), /* post-flushing in progress */
	REQ_FSEQ_DONE		= (1 << 3),

	REQ_FSEQ_ACTIONS	= REQ_FSEQ_PREFLUSH | REQ_FSEQ_DATA |
				  REQ_FSEQ_POSTFLUSH,

	/*
	 * If flush has been pending longer than the following timeout,
	 * it's issued even if flush_data requests are still in flight.
	 */
	FLUSH_PENDING_TIMEOUT	= 5 * HZ,
};

static bool blk_kick_flush(struct request_queue *q);

static unsigned int blk_flush_policy(unsigned int fflags, struct request *rq)
{
	unsigned int policy = 0;

	if (fflags & REQ_FLUSH) {
		if (rq->cmd_flags & REQ_FLUSH)
			policy |= REQ_FSEQ_PREFLUSH;
		if (blk_rq_sectors(rq))
			policy |= REQ_FSEQ_DATA;
		if (!(fflags & REQ_FUA) && (rq->cmd_flags & REQ_FUA))
			policy |= REQ_FSEQ_POSTFLUSH;
	}
	return policy;
}

static unsigned int blk_flush_cur_seq(struct request *rq)
{
	return 1 << ffz(rq->flush.seq);
}

static void blk_flush_restore_request(struct request *rq)
{
	/*
	 * After flush data completion, @rq->bio is %NULL but we need to
	 * complete the bio again.  @rq->biotail is guaranteed to equal the
	 * original @rq->bio.  Restore it.
	 */
	rq->bio = rq->biotail;

	/* make @rq a normal request */
	rq->cmd_flags &= ~REQ_FLUSH_SEQ;
	rq->end_io = NULL;
}

/**
 * blk_flush_complete_seq - complete flush sequence
 * @rq: FLUSH/FUA request being sequenced
 * @seq: sequences to complete (mask of %REQ_FSEQ_*, can be zero)
 * @error: whether an error occurred
 *
 * @rq just completed @seq part of its flush sequence, record the
 * completion and trigger the next step.
 *
 * CONTEXT:
 * spin_lock_irq(q->queue_lock)
 *
 * RETURNS:
 * %true if requests were added to the dispatch queue, %false otherwise.
 */
static bool blk_flush_complete_seq(struct request *rq, unsigned int seq,
				   int error)
{
	struct request_queue *q = rq->q;
	struct list_head *pending = &q->flush_queue[q->flush_pending_idx];
	bool queued = false;

	BUG_ON(rq->flush.seq & seq);
	rq->flush.seq |= seq;

	if (likely(!error))
		seq = blk_flush_cur_seq(rq);
	else
		seq = REQ_FSEQ_DONE;

	switch (seq) {
	case REQ_FSEQ_PREFLUSH:
	case REQ_FSEQ_POSTFLUSH:
		/* queue for flush */
		if (list_empty(pending))
			q->flush_pending_since = jiffies;
		list_move_tail(&rq->flush.list, pending);
		break;

	case REQ_FSEQ_DATA:
		list_move_tail(&rq->flush.list, &q->flush_data_in_flight);
		list_add(&rq->queuelist, &q->queue_head);
		queued = true;
		break;

	case REQ_FSEQ_DONE:
		/*
		 * @rq was previously adjusted by blk_flush_issue() for
		 * flush sequencing and may already have gone through the
		 * flush data request completion path.  Restore @rq for
		 * normal completion and end it.
		 */
		BUG_ON(!list_empty(&rq->queuelist));
		list_del_init(&rq->flush.list);
		blk_flush_restore_request(rq);
		__blk_end_request_all(rq, error);
		break;

	default:
		BUG();
	}

	return blk_kick_flush(q) | queued;
}

static void flush_end_io(struct request *flush_rq, int error)
{
	struct request_queue *q = flush_rq->q;
	struct list_head *running = &q->flush_queue[q->flush_running_idx];
	bool queued = false;
	struct request *rq, *n;

	BUG_ON(q->flush_pending_idx == q->flush_running_idx);

	/* account completion of the flush request */
	q->flush_running_idx ^= 1;
	elv_completed_request(q, flush_rq);

	/* and push the waiting requests to the next stage */
	list_for_each_entry_safe(rq, n, running, flush.list) {
		unsigned int seq = blk_flush_cur_seq(rq);

		BUG_ON(seq != REQ_FSEQ_PREFLUSH && seq != REQ_FSEQ_POSTFLUSH);
		queued |= blk_flush_complete_seq(rq, seq, error);
	}

	/*
	 * Kick the queue to avoid stall for two cases:
	 * 1. Moving a request silently to empty queue_head may stall the
	 * queue.
	 * 2. When flush request is running in non-queueable queue, the
	 * queue is hold. Restart the queue after flush request is finished
	 * to avoid stall.
	 * This function is called from request completion path and calling
	 * directly into request_fn may confuse the driver.  Always use
	 * kblockd.
	 */
	if (queued || q->flush_queue_delayed)
		blk_run_queue_async(q);
	q->flush_queue_delayed = 0;
}

/**
 * blk_kick_flush - consider issuing flush request
 * @q: request_queue being kicked
 *
 * Flush related states of @q have changed, consider issuing flush request.
 * Please read the comment at the top of this file for more info.
 *
 * CONTEXT:
 * spin_lock_irq(q->queue_lock)
 *
 * RETURNS:
 * %true if flush was issued, %false otherwise.
 */
static bool blk_kick_flush(struct request_queue *q)
{
	struct list_head *pending = &q->flush_queue[q->flush_pending_idx];
	struct request *first_rq =
		list_first_entry(pending, struct request, flush.list);

	/* C1 described at the top of this file */
	if (q->flush_pending_idx != q->flush_running_idx || list_empty(pending))
		return false;

	/* C2 and C3 */
	if (!list_empty(&q->flush_data_in_flight) &&
	    time_before(jiffies,
			q->flush_pending_since + FLUSH_PENDING_TIMEOUT))
		return false;

	/*
	 * Issue flush and toggle pending_idx.  This makes pending_idx
	 * different from running_idx, which means flush is in flight.
	 */
	blk_rq_init(q, &q->flush_rq);
	q->flush_rq.cmd_type = REQ_TYPE_FS;
	q->flush_rq.cmd_flags = WRITE_FLUSH | REQ_FLUSH_SEQ;
	q->flush_rq.rq_disk = first_rq->rq_disk;
	q->flush_rq.end_io = flush_end_io;

	q->flush_pending_idx ^= 1;
	list_add_tail(&q->flush_rq.queuelist, &q->queue_head);
	return true;
}

static void flush_data_end_io(struct request *rq, int error)
{
	struct request_queue *q = rq->q;

	/*
	 * After populating an empty queue, kick it to avoid stall.  Read
	 * the comment in flush_end_io().
	 */
	if (blk_flush_complete_seq(rq, REQ_FSEQ_DATA, error))
		blk_run_queue_async(q);
}

/**
 * blk_insert_flush - insert a new FLUSH/FUA request
 * @rq: request to insert
 *
 * To be called from __elv_add_request() for %ELEVATOR_INSERT_FLUSH insertions.
 * @rq is being submitted.  Analyze what needs to be done and put it on the
 * right queue.
 *
 * CONTEXT:
 * spin_lock_irq(q->queue_lock)
 */
void blk_insert_flush(struct request *rq)
{
	struct request_queue *q = rq->q;
	unsigned int fflags = q->flush_flags;	/* may change, cache */
	unsigned int policy = blk_flush_policy(fflags, rq);

	BUG_ON(rq->end_io);
	BUG_ON(!rq->bio || rq->bio != rq->biotail);

	/*
	 * @policy now records what operations need to be done.  Adjust
	 * REQ_FLUSH and FUA for the driver.
	 */
	rq->cmd_flags &= ~REQ_FLUSH;
	if (!(fflags & REQ_FUA))
		rq->cmd_flags &= ~REQ_FUA;

	/*
	 * If there's data but flush is not necessary, the request can be
	 * processed directly without going through flush machinery.  Queue
	 * for normal execution.
	 */
	if ((policy & REQ_FSEQ_DATA) &&
	    !(policy & (REQ_FSEQ_PREFLUSH | REQ_FSEQ_POSTFLUSH))) {
		list_add_tail(&rq->queuelist, &q->queue_head);
		return;
	}

	/*
	 * @rq should go through flush machinery.  Mark it part of flush
	 * sequence and submit for further processing.
	 */
	memset(&rq->flush, 0, sizeof(rq->flush));
	INIT_LIST_HEAD(&rq->flush.list);
	rq->cmd_flags |= REQ_FLUSH_SEQ;
	rq->end_io = flush_data_end_io;

	blk_flush_complete_seq(rq, REQ_FSEQ_ACTIONS & ~policy, 0);
}

/**
 * blk_abort_flushes - @q is being aborted, abort flush requests
 * @q: request_queue being aborted
 *
 * To be called from elv_abort_queue().  @q is being aborted.  Prepare all
 * FLUSH/FUA requests for abortion.
 *
 * CONTEXT:
 * spin_lock_irq(q->queue_lock)
 */
void blk_abort_flushes(struct request_queue *q)
{
	struct request *rq, *n;
	int i;

	/*
	 * Requests in flight for data are already owned by the dispatch
	 * queue or the device driver.  Just restore for normal completion.
	 */
	list_for_each_entry_safe(rq, n, &q->flush_data_in_flight, flush.list) {
		list_del_init(&rq->flush.list);
		blk_flush_restore_request(rq);
	}

	/*
	 * We need to give away requests on flush queues.  Restore for
	 * normal completion and put them on the dispatch queue.
	 */
	for (i = 0; i < ARRAY_SIZE(q->flush_queue); i++) {
		list_for_each_entry_safe(rq, n, &q->flush_queue[i],
					 flush.list) {
			list_del_init(&rq->flush.list);
			blk_flush_restore_request(rq);
			list_add_tail(&rq->queuelist, &q->queue_head);
		}
	}
}

static void bio_end_flush(struct bio *bio, int err)
{
	if (err)
		clear_bit(BIO_UPTODATE, &bio->bi_flags);
	if (bio->bi_private)
		complete(bio->bi_private);
	bio_put(bio);
}

/**
 * blkdev_issue_flush - queue a flush
 * @bdev:	blockdev to issue flush for
 * @gfp_mask:	memory allocation flags (for bio_alloc)
 * @error_sector:	error sector
 *
 * Description:
 *    Issue a flush for the block device in question. Caller can supply
 *    room for storing the error offset in case of a flush error, if they
 *    wish to. If WAIT flag is not passed then caller may check only what
 *    request was pushed in some internal queue for later handling.
 */
int blkdev_issue_flush(struct block_device *bdev, gfp_t gfp_mask,
		sector_t *error_sector)
{
	DECLARE_COMPLETION_ONSTACK(wait);
	struct request_queue *q;
	struct bio *bio;
	int ret = 0;

	if (bdev->bd_disk == NULL)
		return -ENXIO;

	q = bdev_get_queue(bdev);
	if (!q)
		return -ENXIO;

	/*
	 * some block devices may not have their queue correctly set up here
	 * (e.g. loop device without a backing file) and so issuing a flush
	 * here will panic. Ensure there is a request function before issuing
	 * the flush.
	 */
	if (!q->make_request_fn)
		return -ENXIO;

	bio = bio_alloc(gfp_mask, 0);
	bio->bi_end_io = bio_end_flush;
	bio->bi_bdev = bdev;
	bio->bi_private = &wait;

	bio_get(bio);
	submit_bio(WRITE_FLUSH, bio);
	wait_for_completion(&wait);

	/*
	 * The driver must store the error location in ->bi_sector, if
	 * it supports it. For non-stacked drivers, this should be
	 * copied from blk_rq_pos(rq).
	 */
	if (error_sector)
               *error_sector = bio->bi_sector;

	if (!bio_flagged(bio, BIO_UPTODATE))
		ret = -EIO;

	bio_put(bio);
	return ret;
}
EXPORT_SYMBOL(blkdev_issue_flush);
