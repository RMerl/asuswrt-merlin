/*
 *  scsi_lib.c Copyright (C) 1999 Eric Youngdale
 *
 *  SCSI queueing library.
 *      Initial versions: Eric Youngdale (eric@andante.org).
 *                        Based upon conversations with large numbers
 *                        of people at Linux Expo.
 */

#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/hardirq.h>
#include <linux/scatterlist.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>

#include "scsi_priv.h"
#include "scsi_logging.h"


#define SG_MEMPOOL_NR		ARRAY_SIZE(scsi_sg_pools)
#define SG_MEMPOOL_SIZE		2

struct scsi_host_sg_pool {
	size_t		size;
	char		*name;
	struct kmem_cache	*slab;
	mempool_t	*pool;
};

#define SP(x) { x, "sgpool-" __stringify(x) }
#if (SCSI_MAX_SG_SEGMENTS < 32)
#error SCSI_MAX_SG_SEGMENTS is too small (must be 32 or greater)
#endif
static struct scsi_host_sg_pool scsi_sg_pools[] = {
	SP(8),
	SP(16),
#if (SCSI_MAX_SG_SEGMENTS > 32)
	SP(32),
#if (SCSI_MAX_SG_SEGMENTS > 64)
	SP(64),
#if (SCSI_MAX_SG_SEGMENTS > 128)
	SP(128),
#if (SCSI_MAX_SG_SEGMENTS > 256)
#error SCSI_MAX_SG_SEGMENTS is too large (256 MAX)
#endif
#endif
#endif
#endif
	SP(SCSI_MAX_SG_SEGMENTS)
};
#undef SP

struct kmem_cache *scsi_sdb_cache;

static void scsi_run_queue(struct request_queue *q);

/*
 * Function:	scsi_unprep_request()
 *
 * Purpose:	Remove all preparation done for a request, including its
 *		associated scsi_cmnd, so that it can be requeued.
 *
 * Arguments:	req	- request to unprepare
 *
 * Lock status:	Assumed that no locks are held upon entry.
 *
 * Returns:	Nothing.
 */
static void scsi_unprep_request(struct request *req)
{
	struct scsi_cmnd *cmd = req->special;

	blk_unprep_request(req);
	req->special = NULL;

	scsi_put_command(cmd);
}

/**
 * __scsi_queue_insert - private queue insertion
 * @cmd: The SCSI command being requeued
 * @reason:  The reason for the requeue
 * @unbusy: Whether the queue should be unbusied
 *
 * This is a private queue insertion.  The public interface
 * scsi_queue_insert() always assumes the queue should be unbusied
 * because it's always called before the completion.  This function is
 * for a requeue after completion, which should only occur in this
 * file.
 */
static int __scsi_queue_insert(struct scsi_cmnd *cmd, int reason, int unbusy)
{
	struct Scsi_Host *host = cmd->device->host;
	struct scsi_device *device = cmd->device;
	struct scsi_target *starget = scsi_target(device);
	struct request_queue *q = device->request_queue;
	unsigned long flags;

	SCSI_LOG_MLQUEUE(1,
		 printk("Inserting command %p into mlqueue\n", cmd));

	/*
	 * Set the appropriate busy bit for the device/host.
	 *
	 * If the host/device isn't busy, assume that something actually
	 * completed, and that we should be able to queue a command now.
	 *
	 * Note that the prior mid-layer assumption that any host could
	 * always queue at least one command is now broken.  The mid-layer
	 * will implement a user specifiable stall (see
	 * scsi_host.max_host_blocked and scsi_device.max_device_blocked)
	 * if a command is requeued with no other commands outstanding
	 * either for the device or for the host.
	 */
	switch (reason) {
	case SCSI_MLQUEUE_HOST_BUSY:
		host->host_blocked = host->max_host_blocked;
		break;
	case SCSI_MLQUEUE_DEVICE_BUSY:
		device->device_blocked = device->max_device_blocked;
		break;
	case SCSI_MLQUEUE_TARGET_BUSY:
		starget->target_blocked = starget->max_target_blocked;
		break;
	}

	/*
	 * Decrement the counters, since these commands are no longer
	 * active on the host/device.
	 */
	if (unbusy)
		scsi_device_unbusy(device);

	/*
	 * Requeue this command.  It will go before all other commands
	 * that are already in the queue.
	 *
	 * NOTE: there is magic here about the way the queue is plugged if
	 * we have no outstanding commands.
	 * 
	 * Although we *don't* plug the queue, we call the request
	 * function.  The SCSI request function detects the blocked condition
	 * and plugs the queue appropriately.
         */
	spin_lock_irqsave(q->queue_lock, flags);
	blk_requeue_request(q, cmd->request);
	spin_unlock_irqrestore(q->queue_lock, flags);

	scsi_run_queue(q);

	return 0;
}

/*
 * Function:    scsi_queue_insert()
 *
 * Purpose:     Insert a command in the midlevel queue.
 *
 * Arguments:   cmd    - command that we are adding to queue.
 *              reason - why we are inserting command to queue.
 *
 * Lock status: Assumed that lock is not held upon entry.
 *
 * Returns:     Nothing.
 *
 * Notes:       We do this for one of two cases.  Either the host is busy
 *              and it cannot accept any more commands for the time being,
 *              or the device returned QUEUE_FULL and can accept no more
 *              commands.
 * Notes:       This could be called either from an interrupt context or a
 *              normal process context.
 */
int scsi_queue_insert(struct scsi_cmnd *cmd, int reason)
{
	return __scsi_queue_insert(cmd, reason, 1);
}
/**
 * scsi_execute - insert request and wait for the result
 * @sdev:	scsi device
 * @cmd:	scsi command
 * @data_direction: data direction
 * @buffer:	data buffer
 * @bufflen:	len of buffer
 * @sense:	optional sense buffer
 * @timeout:	request timeout in seconds
 * @retries:	number of times to retry request
 * @flags:	or into request flags;
 * @resid:	optional residual length
 *
 * returns the req->errors value which is the scsi_cmnd result
 * field.
 */
int scsi_execute(struct scsi_device *sdev, const unsigned char *cmd,
		 int data_direction, void *buffer, unsigned bufflen,
		 unsigned char *sense, int timeout, int retries, int flags,
		 int *resid)
{
	struct request *req;
	int write = (data_direction == DMA_TO_DEVICE);
	int ret = DRIVER_ERROR << 24;

	req = blk_get_request(sdev->request_queue, write, __GFP_WAIT);

	if (bufflen &&	blk_rq_map_kern(sdev->request_queue, req,
					buffer, bufflen, __GFP_WAIT))
		goto out;

	req->cmd_len = COMMAND_SIZE(cmd[0]);
	memcpy(req->cmd, cmd, req->cmd_len);
	req->sense = sense;
	req->sense_len = 0;
	req->retries = retries;
	req->timeout = timeout;
	req->cmd_type = REQ_TYPE_BLOCK_PC;
	req->cmd_flags |= flags | REQ_QUIET | REQ_PREEMPT;

	/*
	 * head injection *required* here otherwise quiesce won't work
	 */
	blk_execute_rq(req->q, NULL, req, 1);

	/*
	 * Some devices (USB mass-storage in particular) may transfer
	 * garbage data together with a residue indicating that the data
	 * is invalid.  Prevent the garbage from being misinterpreted
	 * and prevent security leaks by zeroing out the excess data.
	 */
	if (unlikely(req->resid_len > 0 && req->resid_len <= bufflen))
		memset(buffer + (bufflen - req->resid_len), 0, req->resid_len);

	if (resid)
		*resid = req->resid_len;
	ret = req->errors;
 out:
	blk_put_request(req);

	return ret;
}
EXPORT_SYMBOL(scsi_execute);


int scsi_execute_req(struct scsi_device *sdev, const unsigned char *cmd,
		     int data_direction, void *buffer, unsigned bufflen,
		     struct scsi_sense_hdr *sshdr, int timeout, int retries,
		     int *resid)
{
	char *sense = NULL;
	int result;
	
	if (sshdr) {
		sense = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
		if (!sense)
			return DRIVER_ERROR << 24;
	}
	result = scsi_execute(sdev, cmd, data_direction, buffer, bufflen,
			      sense, timeout, retries, 0, resid);
	if (sshdr)
		scsi_normalize_sense(sense, SCSI_SENSE_BUFFERSIZE, sshdr);

	kfree(sense);
	return result;
}
EXPORT_SYMBOL(scsi_execute_req);

/*
 * Function:    scsi_init_cmd_errh()
 *
 * Purpose:     Initialize cmd fields related to error handling.
 *
 * Arguments:   cmd	- command that is ready to be queued.
 *
 * Notes:       This function has the job of initializing a number of
 *              fields related to error handling.   Typically this will
 *              be called once for each command, as required.
 */
static void scsi_init_cmd_errh(struct scsi_cmnd *cmd)
{
	cmd->serial_number = 0;
	scsi_set_resid(cmd, 0);
	memset(cmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
	if (cmd->cmd_len == 0)
		cmd->cmd_len = scsi_command_size(cmd->cmnd);
}

void scsi_device_unbusy(struct scsi_device *sdev)
{
	struct Scsi_Host *shost = sdev->host;
	struct scsi_target *starget = scsi_target(sdev);
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	shost->host_busy--;
	starget->target_busy--;
	if (unlikely(scsi_host_in_recovery(shost) &&
		     (shost->host_failed || shost->host_eh_scheduled)))
		scsi_eh_wakeup(shost);
	spin_unlock(shost->host_lock);
	spin_lock(sdev->request_queue->queue_lock);
	sdev->device_busy--;
	spin_unlock_irqrestore(sdev->request_queue->queue_lock, flags);
}

/*
 * Called for single_lun devices on IO completion. Clear starget_sdev_user,
 * and call blk_run_queue for all the scsi_devices on the target -
 * including current_sdev first.
 *
 * Called with *no* scsi locks held.
 */
static void scsi_single_lun_run(struct scsi_device *current_sdev)
{
	struct Scsi_Host *shost = current_sdev->host;
	struct scsi_device *sdev, *tmp;
	struct scsi_target *starget = scsi_target(current_sdev);
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	starget->starget_sdev_user = NULL;
	spin_unlock_irqrestore(shost->host_lock, flags);

	/*
	 * Call blk_run_queue for all LUNs on the target, starting with
	 * current_sdev. We race with others (to set starget_sdev_user),
	 * but in most cases, we will be first. Ideally, each LU on the
	 * target would get some limited time or requests on the target.
	 */
	blk_run_queue(current_sdev->request_queue);

	spin_lock_irqsave(shost->host_lock, flags);
	if (starget->starget_sdev_user)
		goto out;
	list_for_each_entry_safe(sdev, tmp, &starget->devices,
			same_target_siblings) {
		if (sdev == current_sdev)
			continue;
		if (scsi_device_get(sdev))
			continue;

		spin_unlock_irqrestore(shost->host_lock, flags);
		blk_run_queue(sdev->request_queue);
		spin_lock_irqsave(shost->host_lock, flags);
	
		scsi_device_put(sdev);
	}
 out:
	spin_unlock_irqrestore(shost->host_lock, flags);
}

static inline int scsi_device_is_busy(struct scsi_device *sdev)
{
	if (sdev->device_busy >= sdev->queue_depth || sdev->device_blocked)
		return 1;

	return 0;
}

static inline int scsi_target_is_busy(struct scsi_target *starget)
{
	return ((starget->can_queue > 0 &&
		 starget->target_busy >= starget->can_queue) ||
		 starget->target_blocked);
}

static inline int scsi_host_is_busy(struct Scsi_Host *shost)
{
	if ((shost->can_queue > 0 && shost->host_busy >= shost->can_queue) ||
	    shost->host_blocked || shost->host_self_blocked)
		return 1;

	return 0;
}

/*
 * Function:	scsi_run_queue()
 *
 * Purpose:	Select a proper request queue to serve next
 *
 * Arguments:	q	- last request's queue
 *
 * Returns:     Nothing
 *
 * Notes:	The previous command was completely finished, start
 *		a new one if possible.
 */
static void scsi_run_queue(struct request_queue *q)
{
	struct scsi_device *sdev = q->queuedata;
	struct Scsi_Host *shost = sdev->host;
	LIST_HEAD(starved_list);
	unsigned long flags;

	if (scsi_target(sdev)->single_lun)
		scsi_single_lun_run(sdev);

	spin_lock_irqsave(shost->host_lock, flags);
	list_splice_init(&shost->starved_list, &starved_list);

	while (!list_empty(&starved_list)) {
		int flagset;

		/*
		 * As long as shost is accepting commands and we have
		 * starved queues, call blk_run_queue. scsi_request_fn
		 * drops the queue_lock and can add us back to the
		 * starved_list.
		 *
		 * host_lock protects the starved_list and starved_entry.
		 * scsi_request_fn must get the host_lock before checking
		 * or modifying starved_list or starved_entry.
		 */
		if (scsi_host_is_busy(shost))
			break;

		sdev = list_entry(starved_list.next,
				  struct scsi_device, starved_entry);
		list_del_init(&sdev->starved_entry);
		if (scsi_target_is_busy(scsi_target(sdev))) {
			list_move_tail(&sdev->starved_entry,
				       &shost->starved_list);
			continue;
		}

		spin_unlock(shost->host_lock);

		spin_lock(sdev->request_queue->queue_lock);
		flagset = test_bit(QUEUE_FLAG_REENTER, &q->queue_flags) &&
				!test_bit(QUEUE_FLAG_REENTER,
					&sdev->request_queue->queue_flags);
		if (flagset)
			queue_flag_set(QUEUE_FLAG_REENTER, sdev->request_queue);
		__blk_run_queue(sdev->request_queue);
		if (flagset)
			queue_flag_clear(QUEUE_FLAG_REENTER, sdev->request_queue);
		spin_unlock(sdev->request_queue->queue_lock);

		spin_lock(shost->host_lock);
	}
	/* put any unprocessed entries back */
	list_splice(&starved_list, &shost->starved_list);
	spin_unlock_irqrestore(shost->host_lock, flags);

	blk_run_queue(q);
}

/*
 * Function:	scsi_requeue_command()
 *
 * Purpose:	Handle post-processing of completed commands.
 *
 * Arguments:	q	- queue to operate on
 *		cmd	- command that may need to be requeued.
 *
 * Returns:	Nothing
 *
 * Notes:	After command completion, there may be blocks left
 *		over which weren't finished by the previous command
 *		this can be for a number of reasons - the main one is
 *		I/O errors in the middle of the request, in which case
 *		we need to request the blocks that come after the bad
 *		sector.
 * Notes:	Upon return, cmd is a stale pointer.
 */
static void scsi_requeue_command(struct request_queue *q, struct scsi_cmnd *cmd)
{
	struct request *req = cmd->request;
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	scsi_unprep_request(req);
	blk_requeue_request(q, req);
	spin_unlock_irqrestore(q->queue_lock, flags);

	scsi_run_queue(q);
}

void scsi_next_command(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdev = cmd->device;
	struct request_queue *q = sdev->request_queue;

	/* need to hold a reference on the device before we let go of the cmd */
	get_device(&sdev->sdev_gendev);

	scsi_put_command(cmd);
	scsi_run_queue(q);

	/* ok to remove device now */
	put_device(&sdev->sdev_gendev);
}

void scsi_run_host_queues(struct Scsi_Host *shost)
{
	struct scsi_device *sdev;

	shost_for_each_device(sdev, shost)
		scsi_run_queue(sdev->request_queue);
}

static void __scsi_release_buffers(struct scsi_cmnd *, int);

/*
 * Function:    scsi_end_request()
 *
 * Purpose:     Post-processing of completed commands (usually invoked at end
 *		of upper level post-processing and scsi_io_completion).
 *
 * Arguments:   cmd	 - command that is complete.
 *              error    - 0 if I/O indicates success, < 0 for I/O error.
 *              bytes    - number of bytes of completed I/O
 *		requeue  - indicates whether we should requeue leftovers.
 *
 * Lock status: Assumed that lock is not held upon entry.
 *
 * Returns:     cmd if requeue required, NULL otherwise.
 *
 * Notes:       This is called for block device requests in order to
 *              mark some number of sectors as complete.
 * 
 *		We are guaranteeing that the request queue will be goosed
 *		at some point during this call.
 * Notes:	If cmd was requeued, upon return it will be a stale pointer.
 */
static struct scsi_cmnd *scsi_end_request(struct scsi_cmnd *cmd, int error,
					  int bytes, int requeue)
{
	struct request_queue *q = cmd->device->request_queue;
	struct request *req = cmd->request;

	/*
	 * If there are blocks left over at the end, set up the command
	 * to queue the remainder of them.
	 */
	if (blk_end_request(req, error, bytes)) {
		/* kill remainder if no retrys */
		if (error && scsi_noretry_cmd(cmd))
			blk_end_request_all(req, error);
		else {
			if (requeue) {
				/*
				 * Bleah.  Leftovers again.  Stick the
				 * leftovers in the front of the
				 * queue, and goose the queue again.
				 */
				scsi_release_buffers(cmd);
				scsi_requeue_command(q, cmd);
				cmd = NULL;
			}
			return cmd;
		}
	}

	/*
	 * This will goose the queue request function at the end, so we don't
	 * need to worry about launching another command.
	 */
	__scsi_release_buffers(cmd, 0);
	scsi_next_command(cmd);
	return NULL;
}

static inline unsigned int scsi_sgtable_index(unsigned short nents)
{
	unsigned int index;

	BUG_ON(nents > SCSI_MAX_SG_SEGMENTS);

	if (nents <= 8)
		index = 0;
	else
		index = get_count_order(nents) - 3;

	return index;
}

static void scsi_sg_free(struct scatterlist *sgl, unsigned int nents)
{
	struct scsi_host_sg_pool *sgp;

	sgp = scsi_sg_pools + scsi_sgtable_index(nents);
	mempool_free(sgl, sgp->pool);
}

static struct scatterlist *scsi_sg_alloc(unsigned int nents, gfp_t gfp_mask)
{
	struct scsi_host_sg_pool *sgp;

	sgp = scsi_sg_pools + scsi_sgtable_index(nents);
	return mempool_alloc(sgp->pool, gfp_mask);
}

static int scsi_alloc_sgtable(struct scsi_data_buffer *sdb, int nents,
			      gfp_t gfp_mask)
{
	int ret;

	BUG_ON(!nents);

	ret = __sg_alloc_table(&sdb->table, nents, SCSI_MAX_SG_SEGMENTS,
			       gfp_mask, scsi_sg_alloc);
	if (unlikely(ret))
		__sg_free_table(&sdb->table, SCSI_MAX_SG_SEGMENTS,
				scsi_sg_free);

	return ret;
}

static void scsi_free_sgtable(struct scsi_data_buffer *sdb)
{
	__sg_free_table(&sdb->table, SCSI_MAX_SG_SEGMENTS, scsi_sg_free);
}

static void __scsi_release_buffers(struct scsi_cmnd *cmd, int do_bidi_check)
{

	if (cmd->sdb.table.nents)
		scsi_free_sgtable(&cmd->sdb);

	memset(&cmd->sdb, 0, sizeof(cmd->sdb));

	if (do_bidi_check && scsi_bidi_cmnd(cmd)) {
		struct scsi_data_buffer *bidi_sdb =
			cmd->request->next_rq->special;
		scsi_free_sgtable(bidi_sdb);
		kmem_cache_free(scsi_sdb_cache, bidi_sdb);
		cmd->request->next_rq->special = NULL;
	}

	if (scsi_prot_sg_count(cmd))
		scsi_free_sgtable(cmd->prot_sdb);
}

/*
 * Function:    scsi_release_buffers()
 *
 * Purpose:     Completion processing for block device I/O requests.
 *
 * Arguments:   cmd	- command that we are bailing.
 *
 * Lock status: Assumed that no lock is held upon entry.
 *
 * Returns:     Nothing
 *
 * Notes:       In the event that an upper level driver rejects a
 *		command, we must release resources allocated during
 *		the __init_io() function.  Primarily this would involve
 *		the scatter-gather table, and potentially any bounce
 *		buffers.
 */
void scsi_release_buffers(struct scsi_cmnd *cmd)
{
	__scsi_release_buffers(cmd, 1);
}
EXPORT_SYMBOL(scsi_release_buffers);

/*
 * Function:    scsi_io_completion()
 *
 * Purpose:     Completion processing for block device I/O requests.
 *
 * Arguments:   cmd   - command that is finished.
 *
 * Lock status: Assumed that no lock is held upon entry.
 *
 * Returns:     Nothing
 *
 * Notes:       This function is matched in terms of capabilities to
 *              the function that created the scatter-gather list.
 *              In other words, if there are no bounce buffers
 *              (the normal case for most drivers), we don't need
 *              the logic to deal with cleaning up afterwards.
 *
 *		We must call scsi_end_request().  This will finish off
 *		the specified number of sectors.  If we are done, the
 *		command block will be released and the queue function
 *		will be goosed.  If we are not done then we have to
 *		figure out what to do next:
 *
 *		a) We can call scsi_requeue_command().  The request
 *		   will be unprepared and put back on the queue.  Then
 *		   a new command will be created for it.  This should
 *		   be used if we made forward progress, or if we want
 *		   to switch from READ(10) to READ(6) for example.
 *
 *		b) We can call scsi_queue_insert().  The request will
 *		   be put back on the queue and retried using the same
 *		   command as before, possibly after a delay.
 *
 *		c) We can call blk_end_request() with -EIO to fail
 *		   the remainder of the request.
 */
void scsi_io_completion(struct scsi_cmnd *cmd, unsigned int good_bytes)
{
	int result = cmd->result;
	struct request_queue *q = cmd->device->request_queue;
	struct request *req = cmd->request;
	int error = 0;
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;
	int sense_deferred = 0;
	enum {ACTION_FAIL, ACTION_REPREP, ACTION_RETRY,
	      ACTION_DELAYED_RETRY} action;
	char *description = NULL;

	if (result) {
		sense_valid = scsi_command_normalize_sense(cmd, &sshdr);
		if (sense_valid)
			sense_deferred = scsi_sense_is_deferred(&sshdr);
	}

	if (req->cmd_type == REQ_TYPE_BLOCK_PC) { /* SG_IO ioctl from block level */
		req->errors = result;
		if (result) {
			if (sense_valid && req->sense) {
				/*
				 * SG_IO wants current and deferred errors
				 */
				int len = 8 + cmd->sense_buffer[7];

				if (len > SCSI_SENSE_BUFFERSIZE)
					len = SCSI_SENSE_BUFFERSIZE;
				memcpy(req->sense, cmd->sense_buffer,  len);
				req->sense_len = len;
			}
			if (!sense_deferred)
				error = -EIO;
		}

		req->resid_len = scsi_get_resid(cmd);

		if (scsi_bidi_cmnd(cmd)) {
			/*
			 * Bidi commands Must be complete as a whole,
			 * both sides at once.
			 */
			req->next_rq->resid_len = scsi_in(cmd)->resid;

			scsi_release_buffers(cmd);
			blk_end_request_all(req, 0);

			scsi_next_command(cmd);
			return;
		}
	}

	/* no bidi support for !REQ_TYPE_BLOCK_PC yet */
	BUG_ON(blk_bidi_rq(req));

	/*
	 * Next deal with any sectors which we were able to correctly
	 * handle.
	 */
	SCSI_LOG_HLCOMPLETE(1, printk("%u sectors total, "
				      "%d bytes done.\n",
				      blk_rq_sectors(req), good_bytes));

	/*
	 * Recovered errors need reporting, but they're always treated
	 * as success, so fiddle the result code here.  For BLOCK_PC
	 * we already took a copy of the original into rq->errors which
	 * is what gets returned to the user
	 */
	if (sense_valid && (sshdr.sense_key == RECOVERED_ERROR)) {
		/* if ATA PASS-THROUGH INFORMATION AVAILABLE skip
		 * print since caller wants ATA registers. Only occurs on
		 * SCSI ATA PASS_THROUGH commands when CK_COND=1
		 */
		if ((sshdr.asc == 0x0) && (sshdr.ascq == 0x1d))
			;
		else if (!(req->cmd_flags & REQ_QUIET))
			scsi_print_sense("", cmd);
		result = 0;
		/* BLOCK_PC may have set error */
		error = 0;
	}

	/*
	 * A number of bytes were successfully read.  If there
	 * are leftovers and there is some kind of error
	 * (result != 0), retry the rest.
	 */
	if (scsi_end_request(cmd, error, good_bytes, result == 0) == NULL)
		return;

	error = -EIO;

	if (host_byte(result) == DID_RESET) {
		/* Third party bus reset or reset for error recovery
		 * reasons.  Just retry the command and see what
		 * happens.
		 */
		action = ACTION_RETRY;
	} else if (sense_valid && !sense_deferred) {
		switch (sshdr.sense_key) {
		case UNIT_ATTENTION:
			if (cmd->device->removable) {
				/* Detected disc change.  Set a bit
				 * and quietly refuse further access.
				 */
				cmd->device->changed = 1;
				description = "Media Changed";
				action = ACTION_FAIL;
			} else {
				/* Must have been a power glitch, or a
				 * bus reset.  Could not have been a
				 * media change, so we just retry the
				 * command and see what happens.
				 */
				action = ACTION_RETRY;
			}
			break;
		case ILLEGAL_REQUEST:
			/* If we had an ILLEGAL REQUEST returned, then
			 * we may have performed an unsupported
			 * command.  The only thing this should be
			 * would be a ten byte read where only a six
			 * byte read was supported.  Also, on a system
			 * where READ CAPACITY failed, we may have
			 * read past the end of the disk.
			 */
			if ((cmd->device->use_10_for_rw &&
			    sshdr.asc == 0x20 && sshdr.ascq == 0x00) &&
			    (cmd->cmnd[0] == READ_10 ||
			     cmd->cmnd[0] == WRITE_10)) {
				/* This will issue a new 6-byte command. */
				cmd->device->use_10_for_rw = 0;
				action = ACTION_REPREP;
			} else if (sshdr.asc == 0x10) /* DIX */ {
				description = "Host Data Integrity Failure";
				action = ACTION_FAIL;
				error = -EILSEQ;
			} else
				action = ACTION_FAIL;
			break;
		case ABORTED_COMMAND:
			action = ACTION_FAIL;
			if (sshdr.asc == 0x10) { /* DIF */
				description = "Target Data Integrity Failure";
				error = -EILSEQ;
			}
			break;
		case NOT_READY:
			/* If the device is in the process of becoming
			 * ready, or has a temporary blockage, retry.
			 */
			if (sshdr.asc == 0x04) {
				switch (sshdr.ascq) {
				case 0x01: /* becoming ready */
				case 0x04: /* format in progress */
				case 0x05: /* rebuild in progress */
				case 0x06: /* recalculation in progress */
				case 0x07: /* operation in progress */
				case 0x08: /* Long write in progress */
				case 0x09: /* self test in progress */
				case 0x14: /* space allocation in progress */
					action = ACTION_DELAYED_RETRY;
					break;
				default:
					description = "Device not ready";
					action = ACTION_FAIL;
					break;
				}
			} else {
				description = "Device not ready";
				action = ACTION_FAIL;
			}
			break;
		case VOLUME_OVERFLOW:
			/* See SSC3rXX or current. */
			action = ACTION_FAIL;
			break;
		default:
			description = "Unhandled sense code";
			action = ACTION_FAIL;
			break;
		}
	} else {
		description = "Unhandled error code";
		action = ACTION_FAIL;
	}

	switch (action) {
	case ACTION_FAIL:
		/* Give up and fail the remainder of the request */
		scsi_release_buffers(cmd);
		if (!(req->cmd_flags & REQ_QUIET)) {
			if (description)
				scmd_printk(KERN_INFO, cmd, "%s\n",
					    description);
			scsi_print_result(cmd);
			if (driver_byte(result) & DRIVER_SENSE)
				scsi_print_sense("", cmd);
			scsi_print_command(cmd);
		}
		if (blk_end_request_err(req, error))
			scsi_requeue_command(q, cmd);
		else
			scsi_next_command(cmd);
		break;
	case ACTION_REPREP:
		/* Unprep the request and put it back at the head of the queue.
		 * A new command will be prepared and issued.
		 */
		scsi_release_buffers(cmd);
		scsi_requeue_command(q, cmd);
		break;
	case ACTION_RETRY:
		/* Retry the same command immediately */
		__scsi_queue_insert(cmd, SCSI_MLQUEUE_EH_RETRY, 0);
		break;
	case ACTION_DELAYED_RETRY:
		/* Retry the same command after a delay */
		__scsi_queue_insert(cmd, SCSI_MLQUEUE_DEVICE_BUSY, 0);
		break;
	}
}

static int scsi_init_sgtable(struct request *req, struct scsi_data_buffer *sdb,
			     gfp_t gfp_mask)
{
	int count;

	/*
	 * If sg table allocation fails, requeue request later.
	 */
	if (unlikely(scsi_alloc_sgtable(sdb, req->nr_phys_segments,
					gfp_mask))) {
		return BLKPREP_DEFER;
	}

	req->buffer = NULL;

	/* 
	 * Next, walk the list, and fill in the addresses and sizes of
	 * each segment.
	 */
	count = blk_rq_map_sg(req->q, req, sdb->table.sgl);
	BUG_ON(count > sdb->table.nents);
	sdb->table.nents = count;
	sdb->length = blk_rq_bytes(req);
	return BLKPREP_OK;
}

/*
 * Function:    scsi_init_io()
 *
 * Purpose:     SCSI I/O initialize function.
 *
 * Arguments:   cmd   - Command descriptor we wish to initialize
 *
 * Returns:     0 on success
 *		BLKPREP_DEFER if the failure is retryable
 *		BLKPREP_KILL if the failure is fatal
 */
int scsi_init_io(struct scsi_cmnd *cmd, gfp_t gfp_mask)
{
	int error = scsi_init_sgtable(cmd->request, &cmd->sdb, gfp_mask);
	if (error)
		goto err_exit;

	if (blk_bidi_rq(cmd->request)) {
		struct scsi_data_buffer *bidi_sdb = kmem_cache_zalloc(
			scsi_sdb_cache, GFP_ATOMIC);
		if (!bidi_sdb) {
			error = BLKPREP_DEFER;
			goto err_exit;
		}

		cmd->request->next_rq->special = bidi_sdb;
		error = scsi_init_sgtable(cmd->request->next_rq, bidi_sdb,
								    GFP_ATOMIC);
		if (error)
			goto err_exit;
	}

	if (blk_integrity_rq(cmd->request)) {
		struct scsi_data_buffer *prot_sdb = cmd->prot_sdb;
		int ivecs, count;

		BUG_ON(prot_sdb == NULL);
		ivecs = blk_rq_count_integrity_sg(cmd->request);

		if (scsi_alloc_sgtable(prot_sdb, ivecs, gfp_mask)) {
			error = BLKPREP_DEFER;
			goto err_exit;
		}

		count = blk_rq_map_integrity_sg(cmd->request,
						prot_sdb->table.sgl);
		BUG_ON(unlikely(count > ivecs));

		cmd->prot_sdb = prot_sdb;
		cmd->prot_sdb->table.nents = count;
	}

	return BLKPREP_OK ;

err_exit:
	scsi_release_buffers(cmd);
	cmd->request->special = NULL;
	scsi_put_command(cmd);
	return error;
}
EXPORT_SYMBOL(scsi_init_io);

static struct scsi_cmnd *scsi_get_cmd_from_req(struct scsi_device *sdev,
		struct request *req)
{
	struct scsi_cmnd *cmd;

	if (!req->special) {
		cmd = scsi_get_command(sdev, GFP_ATOMIC);
		if (unlikely(!cmd))
			return NULL;
		req->special = cmd;
	} else {
		cmd = req->special;
	}

	/* pull a tag out of the request if we have one */
	cmd->tag = req->tag;
	cmd->request = req;

	cmd->cmnd = req->cmd;

	return cmd;
}

int scsi_setup_blk_pc_cmnd(struct scsi_device *sdev, struct request *req)
{
	struct scsi_cmnd *cmd;
	int ret = scsi_prep_state_check(sdev, req);

	if (ret != BLKPREP_OK)
		return ret;

	cmd = scsi_get_cmd_from_req(sdev, req);
	if (unlikely(!cmd))
		return BLKPREP_DEFER;

	/*
	 * BLOCK_PC requests may transfer data, in which case they must
	 * a bio attached to them.  Or they might contain a SCSI command
	 * that does not transfer data, in which case they may optionally
	 * submit a request without an attached bio.
	 */
	if (req->bio) {
		int ret;

		BUG_ON(!req->nr_phys_segments);

		ret = scsi_init_io(cmd, GFP_ATOMIC);
		if (unlikely(ret))
			return ret;
	} else {
		BUG_ON(blk_rq_bytes(req));

		memset(&cmd->sdb, 0, sizeof(cmd->sdb));
		req->buffer = NULL;
	}

	cmd->cmd_len = req->cmd_len;
	if (!blk_rq_bytes(req))
		cmd->sc_data_direction = DMA_NONE;
	else if (rq_data_dir(req) == WRITE)
		cmd->sc_data_direction = DMA_TO_DEVICE;
	else
		cmd->sc_data_direction = DMA_FROM_DEVICE;
	
	cmd->transfersize = blk_rq_bytes(req);
	cmd->allowed = req->retries;
	return BLKPREP_OK;
}
EXPORT_SYMBOL(scsi_setup_blk_pc_cmnd);

/*
 * Setup a REQ_TYPE_FS command.  These are simple read/write request
 * from filesystems that still need to be translated to SCSI CDBs from
 * the ULD.
 */
int scsi_setup_fs_cmnd(struct scsi_device *sdev, struct request *req)
{
	struct scsi_cmnd *cmd;
	int ret = scsi_prep_state_check(sdev, req);

	if (ret != BLKPREP_OK)
		return ret;

	if (unlikely(sdev->scsi_dh_data && sdev->scsi_dh_data->scsi_dh
			 && sdev->scsi_dh_data->scsi_dh->prep_fn)) {
		ret = sdev->scsi_dh_data->scsi_dh->prep_fn(sdev, req);
		if (ret != BLKPREP_OK)
			return ret;
	}

	/*
	 * Filesystem requests must transfer data.
	 */
	BUG_ON(!req->nr_phys_segments);

	cmd = scsi_get_cmd_from_req(sdev, req);
	if (unlikely(!cmd))
		return BLKPREP_DEFER;

	memset(cmd->cmnd, 0, BLK_MAX_CDB);
	return scsi_init_io(cmd, GFP_ATOMIC);
}
EXPORT_SYMBOL(scsi_setup_fs_cmnd);

int scsi_prep_state_check(struct scsi_device *sdev, struct request *req)
{
	int ret = BLKPREP_OK;

	/*
	 * If the device is not in running state we will reject some
	 * or all commands.
	 */
	if (unlikely(sdev->sdev_state != SDEV_RUNNING)) {
		switch (sdev->sdev_state) {
		case SDEV_OFFLINE:
			/*
			 * If the device is offline we refuse to process any
			 * commands.  The device must be brought online
			 * before trying any recovery commands.
			 */
			sdev_printk(KERN_ERR, sdev,
				    "rejecting I/O to offline device\n");
			ret = BLKPREP_KILL;
			break;
		case SDEV_DEL:
			/*
			 * If the device is fully deleted, we refuse to
			 * process any commands as well.
			 */
			sdev_printk(KERN_ERR, sdev,
				    "rejecting I/O to dead device\n");
			ret = BLKPREP_KILL;
			break;
		case SDEV_QUIESCE:
		case SDEV_BLOCK:
		case SDEV_CREATED_BLOCK:
			/*
			 * If the devices is blocked we defer normal commands.
			 */
			if (!(req->cmd_flags & REQ_PREEMPT))
				ret = BLKPREP_DEFER;
			break;
		default:
			/*
			 * For any other not fully online state we only allow
			 * special commands.  In particular any user initiated
			 * command is not allowed.
			 */
			if (!(req->cmd_flags & REQ_PREEMPT))
				ret = BLKPREP_KILL;
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(scsi_prep_state_check);

int scsi_prep_return(struct request_queue *q, struct request *req, int ret)
{
	struct scsi_device *sdev = q->queuedata;

	switch (ret) {
	case BLKPREP_KILL:
		req->errors = DID_NO_CONNECT << 16;
		/* release the command and kill it */
		if (req->special) {
			struct scsi_cmnd *cmd = req->special;
			scsi_release_buffers(cmd);
			scsi_put_command(cmd);
			req->special = NULL;
		}
		break;
	case BLKPREP_DEFER:
		/*
		 * If we defer, the blk_peek_request() returns NULL, but the
		 * queue must be restarted, so we plug here if no returning
		 * command will automatically do that.
		 */
		if (sdev->device_busy == 0)
			blk_plug_device(q);
		break;
	default:
		req->cmd_flags |= REQ_DONTPREP;
	}

	return ret;
}
EXPORT_SYMBOL(scsi_prep_return);

int scsi_prep_fn(struct request_queue *q, struct request *req)
{
	struct scsi_device *sdev = q->queuedata;
	int ret = BLKPREP_KILL;

	if (req->cmd_type == REQ_TYPE_BLOCK_PC)
		ret = scsi_setup_blk_pc_cmnd(sdev, req);
	return scsi_prep_return(q, req, ret);
}
EXPORT_SYMBOL(scsi_prep_fn);

/*
 * scsi_dev_queue_ready: if we can send requests to sdev, return 1 else
 * return 0.
 *
 * Called with the queue_lock held.
 */
static inline int scsi_dev_queue_ready(struct request_queue *q,
				  struct scsi_device *sdev)
{
	if (sdev->device_busy == 0 && sdev->device_blocked) {
		/*
		 * unblock after device_blocked iterates to zero
		 */
		if (--sdev->device_blocked == 0) {
			SCSI_LOG_MLQUEUE(3,
				   sdev_printk(KERN_INFO, sdev,
				   "unblocking device at zero depth\n"));
		} else {
			blk_plug_device(q);
			return 0;
		}
	}
	if (scsi_device_is_busy(sdev))
		return 0;

	return 1;
}


/*
 * scsi_target_queue_ready: checks if there we can send commands to target
 * @sdev: scsi device on starget to check.
 *
 * Called with the host lock held.
 */
static inline int scsi_target_queue_ready(struct Scsi_Host *shost,
					   struct scsi_device *sdev)
{
	struct scsi_target *starget = scsi_target(sdev);

	if (starget->single_lun) {
		if (starget->starget_sdev_user &&
		    starget->starget_sdev_user != sdev)
			return 0;
		starget->starget_sdev_user = sdev;
	}

	if (starget->target_busy == 0 && starget->target_blocked) {
		/*
		 * unblock after target_blocked iterates to zero
		 */
		if (--starget->target_blocked == 0) {
			SCSI_LOG_MLQUEUE(3, starget_printk(KERN_INFO, starget,
					 "unblocking target at zero depth\n"));
		} else
			return 0;
	}

	if (scsi_target_is_busy(starget)) {
		if (list_empty(&sdev->starved_entry)) {
			list_add_tail(&sdev->starved_entry,
				      &shost->starved_list);
			return 0;
		}
	}

	/* We're OK to process the command, so we can't be starved */
	if (!list_empty(&sdev->starved_entry))
		list_del_init(&sdev->starved_entry);
	return 1;
}

/*
 * scsi_host_queue_ready: if we can send requests to shost, return 1 else
 * return 0. We must end up running the queue again whenever 0 is
 * returned, else IO can hang.
 *
 * Called with host_lock held.
 */
static inline int scsi_host_queue_ready(struct request_queue *q,
				   struct Scsi_Host *shost,
				   struct scsi_device *sdev)
{
	if (scsi_host_in_recovery(shost))
		return 0;
	if (shost->host_busy == 0 && shost->host_blocked) {
		/*
		 * unblock after host_blocked iterates to zero
		 */
		if (--shost->host_blocked == 0) {
			SCSI_LOG_MLQUEUE(3,
				printk("scsi%d unblocking host at zero depth\n",
					shost->host_no));
		} else {
			return 0;
		}
	}
	if (scsi_host_is_busy(shost)) {
		if (list_empty(&sdev->starved_entry))
			list_add_tail(&sdev->starved_entry, &shost->starved_list);
		return 0;
	}

	/* We're OK to process the command, so we can't be starved */
	if (!list_empty(&sdev->starved_entry))
		list_del_init(&sdev->starved_entry);

	return 1;
}

/*
 * Busy state exporting function for request stacking drivers.
 *
 * For efficiency, no lock is taken to check the busy state of
 * shost/starget/sdev, since the returned value is not guaranteed and
 * may be changed after request stacking drivers call the function,
 * regardless of taking lock or not.
 *
 * When scsi can't dispatch I/Os anymore and needs to kill I/Os
 * (e.g. !sdev), scsi needs to return 'not busy'.
 * Otherwise, request stacking drivers may hold requests forever.
 */
static int scsi_lld_busy(struct request_queue *q)
{
	struct scsi_device *sdev = q->queuedata;
	struct Scsi_Host *shost;
	struct scsi_target *starget;

	if (!sdev)
		return 0;

	shost = sdev->host;
	starget = scsi_target(sdev);

	if (scsi_host_in_recovery(shost) || scsi_host_is_busy(shost) ||
	    scsi_target_is_busy(starget) || scsi_device_is_busy(sdev))
		return 1;

	return 0;
}

/*
 * Kill a request for a dead device
 */
static void scsi_kill_request(struct request *req, struct request_queue *q)
{
	struct scsi_cmnd *cmd = req->special;
	struct scsi_device *sdev;
	struct scsi_target *starget;
	struct Scsi_Host *shost;

	blk_start_request(req);

	sdev = cmd->device;
	starget = scsi_target(sdev);
	shost = sdev->host;
	scsi_init_cmd_errh(cmd);
	cmd->result = DID_NO_CONNECT << 16;
	atomic_inc(&cmd->device->iorequest_cnt);

	/*
	 * SCSI request completion path will do scsi_device_unbusy(),
	 * bump busy counts.  To bump the counters, we need to dance
	 * with the locks as normal issue path does.
	 */
	sdev->device_busy++;
	spin_unlock(sdev->request_queue->queue_lock);
	spin_lock(shost->host_lock);
	shost->host_busy++;
	starget->target_busy++;
	spin_unlock(shost->host_lock);
	spin_lock(sdev->request_queue->queue_lock);

	blk_complete_request(req);
}

static void scsi_softirq_done(struct request *rq)
{
	struct scsi_cmnd *cmd = rq->special;
	unsigned long wait_for = (cmd->allowed + 1) * rq->timeout;
	int disposition;

	INIT_LIST_HEAD(&cmd->eh_entry);

	/*
	 * Set the serial numbers back to zero
	 */
	cmd->serial_number = 0;

	atomic_inc(&cmd->device->iodone_cnt);
	if (cmd->result)
		atomic_inc(&cmd->device->ioerr_cnt);

	disposition = scsi_decide_disposition(cmd);
	if (disposition != SUCCESS &&
	    time_before(cmd->jiffies_at_alloc + wait_for, jiffies)) {
		sdev_printk(KERN_ERR, cmd->device,
			    "timing out command, waited %lus\n",
			    wait_for/HZ);
		disposition = SUCCESS;
	}
			
	scsi_log_completion(cmd, disposition);

	switch (disposition) {
		case SUCCESS:
			scsi_finish_command(cmd);
			break;
		case NEEDS_RETRY:
			scsi_queue_insert(cmd, SCSI_MLQUEUE_EH_RETRY);
			break;
		case ADD_TO_MLQUEUE:
			scsi_queue_insert(cmd, SCSI_MLQUEUE_DEVICE_BUSY);
			break;
		default:
			if (!scsi_eh_scmd_add(cmd, 0))
				scsi_finish_command(cmd);
	}
}

/*
 * Function:    scsi_request_fn()
 *
 * Purpose:     Main strategy routine for SCSI.
 *
 * Arguments:   q       - Pointer to actual queue.
 *
 * Returns:     Nothing
 *
 * Lock status: IO request lock assumed to be held when called.
 */
static void scsi_request_fn(struct request_queue *q)
{
	struct scsi_device *sdev = q->queuedata;
	struct Scsi_Host *shost;
	struct scsi_cmnd *cmd;
	struct request *req;

	if (!sdev) {
		printk("scsi: killing requests for dead queue\n");
		while ((req = blk_peek_request(q)) != NULL)
			scsi_kill_request(req, q);
		return;
	}

	if(!get_device(&sdev->sdev_gendev))
		/* We must be tearing the block queue down already */
		return;

	/*
	 * To start with, we keep looping until the queue is empty, or until
	 * the host is no longer able to accept any more requests.
	 */
	shost = sdev->host;
	while (!blk_queue_plugged(q)) {
		int rtn;
		/*
		 * get next queueable request.  We do this early to make sure
		 * that the request is fully prepared even if we cannot 
		 * accept it.
		 */
		req = blk_peek_request(q);
		if (!req || !scsi_dev_queue_ready(q, sdev))
			break;

		if (unlikely(!scsi_device_online(sdev))) {
			sdev_printk(KERN_ERR, sdev,
				    "rejecting I/O to offline device\n");
			scsi_kill_request(req, q);
			continue;
		}


		/*
		 * Remove the request from the request list.
		 */
		if (!(blk_queue_tagged(q) && !blk_queue_start_tag(q, req)))
			blk_start_request(req);
		sdev->device_busy++;

		spin_unlock(q->queue_lock);
		cmd = req->special;
		if (unlikely(cmd == NULL)) {
			printk(KERN_CRIT "impossible request in %s.\n"
					 "please mail a stack trace to "
					 "linux-scsi@vger.kernel.org\n",
					 __func__);
			blk_dump_rq_flags(req, "foo");
			BUG();
		}
		spin_lock(shost->host_lock);

		/*
		 * We hit this when the driver is using a host wide
		 * tag map. For device level tag maps the queue_depth check
		 * in the device ready fn would prevent us from trying
		 * to allocate a tag. Since the map is a shared host resource
		 * we add the dev to the starved list so it eventually gets
		 * a run when a tag is freed.
		 */
		if (blk_queue_tagged(q) && !blk_rq_tagged(req)) {
			if (list_empty(&sdev->starved_entry))
				list_add_tail(&sdev->starved_entry,
					      &shost->starved_list);
			goto not_ready;
		}

		if (!scsi_target_queue_ready(shost, sdev))
			goto not_ready;

		if (!scsi_host_queue_ready(q, shost, sdev))
			goto not_ready;

		scsi_target(sdev)->target_busy++;
		shost->host_busy++;

		spin_unlock_irq(shost->host_lock);

		/*
		 * Finally, initialize any error handling parameters, and set up
		 * the timers for timeouts.
		 */
		scsi_init_cmd_errh(cmd);

		/*
		 * Dispatch the command to the low-level driver.
		 */
		rtn = scsi_dispatch_cmd(cmd);
		spin_lock_irq(q->queue_lock);
		if(rtn) {
			/* we're refusing the command; because of
			 * the way locks get dropped, we need to 
			 * check here if plugging is required */
			if(sdev->device_busy == 0)
				blk_plug_device(q);

			break;
		}
	}

	goto out;

 not_ready:
	spin_unlock_irq(shost->host_lock);

	/*
	 * lock q, handle tag, requeue req, and decrement device_busy. We
	 * must return with queue_lock held.
	 *
	 * Decrementing device_busy without checking it is OK, as all such
	 * cases (host limits or settings) should run the queue at some
	 * later time.
	 */
	spin_lock_irq(q->queue_lock);
	blk_requeue_request(q, req);
	sdev->device_busy--;
	if(sdev->device_busy == 0)
		blk_plug_device(q);
 out:
	/* must be careful here...if we trigger the ->remove() function
	 * we cannot be holding the q lock */
	spin_unlock_irq(q->queue_lock);
	put_device(&sdev->sdev_gendev);
	spin_lock_irq(q->queue_lock);
}

u64 scsi_calculate_bounce_limit(struct Scsi_Host *shost)
{
	struct device *host_dev;
	u64 bounce_limit = 0xffffffff;

	if (shost->unchecked_isa_dma)
		return BLK_BOUNCE_ISA;
	/*
	 * Platforms with virtual-DMA translation
	 * hardware have no practical limit.
	 */
	if (!PCI_DMA_BUS_IS_PHYS)
		return BLK_BOUNCE_ANY;

	host_dev = scsi_get_device(shost);
	if (host_dev && host_dev->dma_mask)
		bounce_limit = *host_dev->dma_mask;

	return bounce_limit;
}
EXPORT_SYMBOL(scsi_calculate_bounce_limit);

struct request_queue *__scsi_alloc_queue(struct Scsi_Host *shost,
					 request_fn_proc *request_fn)
{
	struct request_queue *q;
	struct device *dev = shost->shost_gendev.parent;

	q = blk_init_queue(request_fn, NULL);
	if (!q)
		return NULL;

	/*
	 * this limit is imposed by hardware restrictions
	 */
	blk_queue_max_segments(q, min_t(unsigned short, shost->sg_tablesize,
					SCSI_MAX_SG_CHAIN_SEGMENTS));

	blk_queue_max_hw_sectors(q, shost->max_sectors);
	blk_queue_bounce_limit(q, scsi_calculate_bounce_limit(shost));
	blk_queue_segment_boundary(q, shost->dma_boundary);
	dma_set_seg_boundary(dev, shost->dma_boundary);

	blk_queue_max_segment_size(q, dma_get_max_seg_size(dev));

	if (!shost->use_clustering)
		q->limits.cluster = 0;

	/*
	 * set a reasonable default alignment on word boundaries: the
	 * host and device may alter it using
	 * blk_queue_update_dma_alignment() later.
	 */
	blk_queue_dma_alignment(q, 0x03);

	return q;
}
EXPORT_SYMBOL(__scsi_alloc_queue);

struct request_queue *scsi_alloc_queue(struct scsi_device *sdev)
{
	struct request_queue *q;

	q = __scsi_alloc_queue(sdev->host, scsi_request_fn);
	if (!q)
		return NULL;

	blk_queue_prep_rq(q, scsi_prep_fn);
	blk_queue_softirq_done(q, scsi_softirq_done);
	blk_queue_rq_timed_out(q, scsi_times_out);
	blk_queue_lld_busy(q, scsi_lld_busy);
	return q;
}

void scsi_free_queue(struct request_queue *q)
{
	blk_cleanup_queue(q);
}

/*
 * Function:    scsi_block_requests()
 *
 * Purpose:     Utility function used by low-level drivers to prevent further
 *		commands from being queued to the device.
 *
 * Arguments:   shost       - Host in question
 *
 * Returns:     Nothing
 *
 * Lock status: No locks are assumed held.
 *
 * Notes:       There is no timer nor any other means by which the requests
 *		get unblocked other than the low-level driver calling
 *		scsi_unblock_requests().
 */
void scsi_block_requests(struct Scsi_Host *shost)
{
	shost->host_self_blocked = 1;
}
EXPORT_SYMBOL(scsi_block_requests);

/*
 * Function:    scsi_unblock_requests()
 *
 * Purpose:     Utility function used by low-level drivers to allow further
 *		commands from being queued to the device.
 *
 * Arguments:   shost       - Host in question
 *
 * Returns:     Nothing
 *
 * Lock status: No locks are assumed held.
 *
 * Notes:       There is no timer nor any other means by which the requests
 *		get unblocked other than the low-level driver calling
 *		scsi_unblock_requests().
 *
 *		This is done as an API function so that changes to the
 *		internals of the scsi mid-layer won't require wholesale
 *		changes to drivers that use this feature.
 */
void scsi_unblock_requests(struct Scsi_Host *shost)
{
	shost->host_self_blocked = 0;
	scsi_run_host_queues(shost);
}
EXPORT_SYMBOL(scsi_unblock_requests);

int __init scsi_init_queue(void)
{
	int i;

	scsi_sdb_cache = kmem_cache_create("scsi_data_buffer",
					   sizeof(struct scsi_data_buffer),
					   0, 0, NULL);
	if (!scsi_sdb_cache) {
		printk(KERN_ERR "SCSI: can't init scsi sdb cache\n");
		return -ENOMEM;
	}

	for (i = 0; i < SG_MEMPOOL_NR; i++) {
		struct scsi_host_sg_pool *sgp = scsi_sg_pools + i;
		int size = sgp->size * sizeof(struct scatterlist);

		sgp->slab = kmem_cache_create(sgp->name, size, 0,
				SLAB_HWCACHE_ALIGN, NULL);
		if (!sgp->slab) {
			printk(KERN_ERR "SCSI: can't init sg slab %s\n",
					sgp->name);
			goto cleanup_sdb;
		}

		sgp->pool = mempool_create_slab_pool(SG_MEMPOOL_SIZE,
						     sgp->slab);
		if (!sgp->pool) {
			printk(KERN_ERR "SCSI: can't init sg mempool %s\n",
					sgp->name);
			goto cleanup_sdb;
		}
	}

	return 0;

cleanup_sdb:
	for (i = 0; i < SG_MEMPOOL_NR; i++) {
		struct scsi_host_sg_pool *sgp = scsi_sg_pools + i;
		if (sgp->pool)
			mempool_destroy(sgp->pool);
		if (sgp->slab)
			kmem_cache_destroy(sgp->slab);
	}
	kmem_cache_destroy(scsi_sdb_cache);

	return -ENOMEM;
}

void scsi_exit_queue(void)
{
	int i;

	kmem_cache_destroy(scsi_sdb_cache);

	for (i = 0; i < SG_MEMPOOL_NR; i++) {
		struct scsi_host_sg_pool *sgp = scsi_sg_pools + i;
		mempool_destroy(sgp->pool);
		kmem_cache_destroy(sgp->slab);
	}
}

/**
 *	scsi_mode_select - issue a mode select
 *	@sdev:	SCSI device to be queried
 *	@pf:	Page format bit (1 == standard, 0 == vendor specific)
 *	@sp:	Save page bit (0 == don't save, 1 == save)
 *	@modepage: mode page being requested
 *	@buffer: request buffer (may not be smaller than eight bytes)
 *	@len:	length of request buffer.
 *	@timeout: command timeout
 *	@retries: number of retries before failing
 *	@data: returns a structure abstracting the mode header data
 *	@sshdr: place to put sense data (or NULL if no sense to be collected).
 *		must be SCSI_SENSE_BUFFERSIZE big.
 *
 *	Returns zero if successful; negative error number or scsi
 *	status on error
 *
 */
int
scsi_mode_select(struct scsi_device *sdev, int pf, int sp, int modepage,
		 unsigned char *buffer, int len, int timeout, int retries,
		 struct scsi_mode_data *data, struct scsi_sense_hdr *sshdr)
{
	unsigned char cmd[10];
	unsigned char *real_buffer;
	int ret;

	memset(cmd, 0, sizeof(cmd));
	cmd[1] = (pf ? 0x10 : 0) | (sp ? 0x01 : 0);

	if (sdev->use_10_for_ms) {
		if (len > 65535)
			return -EINVAL;
		real_buffer = kmalloc(8 + len, GFP_KERNEL);
		if (!real_buffer)
			return -ENOMEM;
		memcpy(real_buffer + 8, buffer, len);
		len += 8;
		real_buffer[0] = 0;
		real_buffer[1] = 0;
		real_buffer[2] = data->medium_type;
		real_buffer[3] = data->device_specific;
		real_buffer[4] = data->longlba ? 0x01 : 0;
		real_buffer[5] = 0;
		real_buffer[6] = data->block_descriptor_length >> 8;
		real_buffer[7] = data->block_descriptor_length;

		cmd[0] = MODE_SELECT_10;
		cmd[7] = len >> 8;
		cmd[8] = len;
	} else {
		if (len > 255 || data->block_descriptor_length > 255 ||
		    data->longlba)
			return -EINVAL;

		real_buffer = kmalloc(4 + len, GFP_KERNEL);
		if (!real_buffer)
			return -ENOMEM;
		memcpy(real_buffer + 4, buffer, len);
		len += 4;
		real_buffer[0] = 0;
		real_buffer[1] = data->medium_type;
		real_buffer[2] = data->device_specific;
		real_buffer[3] = data->block_descriptor_length;
		

		cmd[0] = MODE_SELECT;
		cmd[4] = len;
	}

	ret = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE, real_buffer, len,
			       sshdr, timeout, retries, NULL);
	kfree(real_buffer);
	return ret;
}
EXPORT_SYMBOL_GPL(scsi_mode_select);

/**
 *	scsi_mode_sense - issue a mode sense, falling back from 10 to six bytes if necessary.
 *	@sdev:	SCSI device to be queried
 *	@dbd:	set if mode sense will allow block descriptors to be returned
 *	@modepage: mode page being requested
 *	@buffer: request buffer (may not be smaller than eight bytes)
 *	@len:	length of request buffer.
 *	@timeout: command timeout
 *	@retries: number of retries before failing
 *	@data: returns a structure abstracting the mode header data
 *	@sshdr: place to put sense data (or NULL if no sense to be collected).
 *		must be SCSI_SENSE_BUFFERSIZE big.
 *
 *	Returns zero if unsuccessful, or the header offset (either 4
 *	or 8 depending on whether a six or ten byte command was
 *	issued) if successful.
 */
int
scsi_mode_sense(struct scsi_device *sdev, int dbd, int modepage,
		  unsigned char *buffer, int len, int timeout, int retries,
		  struct scsi_mode_data *data, struct scsi_sense_hdr *sshdr)
{
	unsigned char cmd[12];
	int use_10_for_ms;
	int header_length;
	int result;
	struct scsi_sense_hdr my_sshdr;

	memset(data, 0, sizeof(*data));
	memset(&cmd[0], 0, 12);
	cmd[1] = dbd & 0x18;	/* allows DBD and LLBA bits */
	cmd[2] = modepage;

	/* caller might not be interested in sense, but we need it */
	if (!sshdr)
		sshdr = &my_sshdr;

 retry:
	use_10_for_ms = sdev->use_10_for_ms;

	if (use_10_for_ms) {
		if (len < 8)
			len = 8;

		cmd[0] = MODE_SENSE_10;
		cmd[8] = len;
		header_length = 8;
	} else {
		if (len < 4)
			len = 4;

		cmd[0] = MODE_SENSE;
		cmd[4] = len;
		header_length = 4;
	}

	memset(buffer, 0, len);

	result = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE, buffer, len,
				  sshdr, timeout, retries, NULL);

	/* This code looks awful: what it's doing is making sure an
	 * ILLEGAL REQUEST sense return identifies the actual command
	 * byte as the problem.  MODE_SENSE commands can return
	 * ILLEGAL REQUEST if the code page isn't supported */

	if (use_10_for_ms && !scsi_status_is_good(result) &&
	    (driver_byte(result) & DRIVER_SENSE)) {
		if (scsi_sense_valid(sshdr)) {
			if ((sshdr->sense_key == ILLEGAL_REQUEST) &&
			    (sshdr->asc == 0x20) && (sshdr->ascq == 0)) {
				/* 
				 * Invalid command operation code
				 */
				sdev->use_10_for_ms = 0;
				goto retry;
			}
		}
	}

	if(scsi_status_is_good(result)) {
		if (unlikely(buffer[0] == 0x86 && buffer[1] == 0x0b &&
			     (modepage == 6 || modepage == 8))) {
			/* Initio breakage? */
			header_length = 0;
			data->length = 13;
			data->medium_type = 0;
			data->device_specific = 0;
			data->longlba = 0;
			data->block_descriptor_length = 0;
		} else if(use_10_for_ms) {
			data->length = buffer[0]*256 + buffer[1] + 2;
			data->medium_type = buffer[2];
			data->device_specific = buffer[3];
			data->longlba = buffer[4] & 0x01;
			data->block_descriptor_length = buffer[6]*256
				+ buffer[7];
		} else {
			data->length = buffer[0] + 1;
			data->medium_type = buffer[1];
			data->device_specific = buffer[2];
			data->block_descriptor_length = buffer[3];
		}
		data->header_length = header_length;
	}

	return result;
}
EXPORT_SYMBOL(scsi_mode_sense);

/**
 *	scsi_test_unit_ready - test if unit is ready
 *	@sdev:	scsi device to change the state of.
 *	@timeout: command timeout
 *	@retries: number of retries before failing
 *	@sshdr_external: Optional pointer to struct scsi_sense_hdr for
 *		returning sense. Make sure that this is cleared before passing
 *		in.
 *
 *	Returns zero if unsuccessful or an error if TUR failed.  For
 *	removable media, a return of NOT_READY or UNIT_ATTENTION is
 *	translated to success, with the ->changed flag updated.
 **/
int
scsi_test_unit_ready(struct scsi_device *sdev, int timeout, int retries,
		     struct scsi_sense_hdr *sshdr_external)
{
	char cmd[] = {
		TEST_UNIT_READY, 0, 0, 0, 0, 0,
	};
	struct scsi_sense_hdr *sshdr;
	int result;

	if (!sshdr_external)
		sshdr = kzalloc(sizeof(*sshdr), GFP_KERNEL);
	else
		sshdr = sshdr_external;

	/* try to eat the UNIT_ATTENTION if there are enough retries */
	do {
		result = scsi_execute_req(sdev, cmd, DMA_NONE, NULL, 0, sshdr,
					  timeout, retries, NULL);
		if (sdev->removable && scsi_sense_valid(sshdr) &&
		    sshdr->sense_key == UNIT_ATTENTION)
			sdev->changed = 1;
	} while (scsi_sense_valid(sshdr) &&
		 sshdr->sense_key == UNIT_ATTENTION && --retries);

	if (!sshdr)
		/* could not allocate sense buffer, so can't process it */
		return result;

	if (sdev->removable && scsi_sense_valid(sshdr) &&
	    (sshdr->sense_key == UNIT_ATTENTION ||
	     sshdr->sense_key == NOT_READY)) {
		sdev->changed = 1;
		result = 0;
	}
	if (!sshdr_external)
		kfree(sshdr);
	return result;
}
EXPORT_SYMBOL(scsi_test_unit_ready);

/**
 *	scsi_device_set_state - Take the given device through the device state model.
 *	@sdev:	scsi device to change the state of.
 *	@state:	state to change to.
 *
 *	Returns zero if unsuccessful or an error if the requested 
 *	transition is illegal.
 */
int
scsi_device_set_state(struct scsi_device *sdev, enum scsi_device_state state)
{
	enum scsi_device_state oldstate = sdev->sdev_state;

	if (state == oldstate)
		return 0;

	switch (state) {
	case SDEV_CREATED:
		switch (oldstate) {
		case SDEV_CREATED_BLOCK:
			break;
		default:
			goto illegal;
		}
		break;
			
	case SDEV_RUNNING:
		switch (oldstate) {
		case SDEV_CREATED:
		case SDEV_OFFLINE:
		case SDEV_QUIESCE:
		case SDEV_BLOCK:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_QUIESCE:
		switch (oldstate) {
		case SDEV_RUNNING:
		case SDEV_OFFLINE:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_OFFLINE:
		switch (oldstate) {
		case SDEV_CREATED:
		case SDEV_RUNNING:
		case SDEV_QUIESCE:
		case SDEV_BLOCK:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_BLOCK:
		switch (oldstate) {
		case SDEV_RUNNING:
		case SDEV_CREATED_BLOCK:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_CREATED_BLOCK:
		switch (oldstate) {
		case SDEV_CREATED:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_CANCEL:
		switch (oldstate) {
		case SDEV_CREATED:
		case SDEV_RUNNING:
		case SDEV_QUIESCE:
		case SDEV_OFFLINE:
		case SDEV_BLOCK:
			break;
		default:
			goto illegal;
		}
		break;

	case SDEV_DEL:
		switch (oldstate) {
		case SDEV_CREATED:
		case SDEV_RUNNING:
		case SDEV_OFFLINE:
		case SDEV_CANCEL:
			break;
		default:
			goto illegal;
		}
		break;

	}
	sdev->sdev_state = state;
	return 0;

 illegal:
	SCSI_LOG_ERROR_RECOVERY(1, 
				sdev_printk(KERN_ERR, sdev,
					    "Illegal state transition %s->%s\n",
					    scsi_device_state_name(oldstate),
					    scsi_device_state_name(state))
				);
	return -EINVAL;
}
EXPORT_SYMBOL(scsi_device_set_state);

/**
 * 	sdev_evt_emit - emit a single SCSI device uevent
 *	@sdev: associated SCSI device
 *	@evt: event to emit
 *
 *	Send a single uevent (scsi_event) to the associated scsi_device.
 */
static void scsi_evt_emit(struct scsi_device *sdev, struct scsi_event *evt)
{
	int idx = 0;
	char *envp[3];

	switch (evt->evt_type) {
	case SDEV_EVT_MEDIA_CHANGE:
		envp[idx++] = "SDEV_MEDIA_CHANGE=1";
		break;

	default:
		/* do nothing */
		break;
	}

	envp[idx++] = NULL;

	kobject_uevent_env(&sdev->sdev_gendev.kobj, KOBJ_CHANGE, envp);
}

/**
 * 	sdev_evt_thread - send a uevent for each scsi event
 *	@work: work struct for scsi_device
 *
 *	Dispatch queued events to their associated scsi_device kobjects
 *	as uevents.
 */
void scsi_evt_thread(struct work_struct *work)
{
	struct scsi_device *sdev;
	LIST_HEAD(event_list);

	sdev = container_of(work, struct scsi_device, event_work);

	while (1) {
		struct scsi_event *evt;
		struct list_head *this, *tmp;
		unsigned long flags;

		spin_lock_irqsave(&sdev->list_lock, flags);
		list_splice_init(&sdev->event_list, &event_list);
		spin_unlock_irqrestore(&sdev->list_lock, flags);

		if (list_empty(&event_list))
			break;

		list_for_each_safe(this, tmp, &event_list) {
			evt = list_entry(this, struct scsi_event, node);
			list_del(&evt->node);
			scsi_evt_emit(sdev, evt);
			kfree(evt);
		}
	}
}

/**
 * 	sdev_evt_send - send asserted event to uevent thread
 *	@sdev: scsi_device event occurred on
 *	@evt: event to send
 *
 *	Assert scsi device event asynchronously.
 */
void sdev_evt_send(struct scsi_device *sdev, struct scsi_event *evt)
{
	unsigned long flags;


	spin_lock_irqsave(&sdev->list_lock, flags);
	list_add_tail(&evt->node, &sdev->event_list);
	schedule_work(&sdev->event_work);
	spin_unlock_irqrestore(&sdev->list_lock, flags);
}
EXPORT_SYMBOL_GPL(sdev_evt_send);

/**
 * 	sdev_evt_alloc - allocate a new scsi event
 *	@evt_type: type of event to allocate
 *	@gfpflags: GFP flags for allocation
 *
 *	Allocates and returns a new scsi_event.
 */
struct scsi_event *sdev_evt_alloc(enum scsi_device_event evt_type,
				  gfp_t gfpflags)
{
	struct scsi_event *evt = kzalloc(sizeof(struct scsi_event), gfpflags);
	if (!evt)
		return NULL;

	evt->evt_type = evt_type;
	INIT_LIST_HEAD(&evt->node);

	/* evt_type-specific initialization, if any */
	switch (evt_type) {
	case SDEV_EVT_MEDIA_CHANGE:
	default:
		/* do nothing */
		break;
	}

	return evt;
}
EXPORT_SYMBOL_GPL(sdev_evt_alloc);

/**
 * 	sdev_evt_send_simple - send asserted event to uevent thread
 *	@sdev: scsi_device event occurred on
 *	@evt_type: type of event to send
 *	@gfpflags: GFP flags for allocation
 *
 *	Assert scsi device event asynchronously, given an event type.
 */
void sdev_evt_send_simple(struct scsi_device *sdev,
			  enum scsi_device_event evt_type, gfp_t gfpflags)
{
	struct scsi_event *evt = sdev_evt_alloc(evt_type, gfpflags);
	if (!evt) {
		sdev_printk(KERN_ERR, sdev, "event %d eaten due to OOM\n",
			    evt_type);
		return;
	}

	sdev_evt_send(sdev, evt);
}
EXPORT_SYMBOL_GPL(sdev_evt_send_simple);

/**
 *	scsi_device_quiesce - Block user issued commands.
 *	@sdev:	scsi device to quiesce.
 *
 *	This works by trying to transition to the SDEV_QUIESCE state
 *	(which must be a legal transition).  When the device is in this
 *	state, only special requests will be accepted, all others will
 *	be deferred.  Since special requests may also be requeued requests,
 *	a successful return doesn't guarantee the device will be 
 *	totally quiescent.
 *
 *	Must be called with user context, may sleep.
 *
 *	Returns zero if unsuccessful or an error if not.
 */
int
scsi_device_quiesce(struct scsi_device *sdev)
{
	int err = scsi_device_set_state(sdev, SDEV_QUIESCE);
	if (err)
		return err;

	scsi_run_queue(sdev->request_queue);
	while (sdev->device_busy) {
		msleep_interruptible(200);
		scsi_run_queue(sdev->request_queue);
	}
	return 0;
}
EXPORT_SYMBOL(scsi_device_quiesce);

/**
 *	scsi_device_resume - Restart user issued commands to a quiesced device.
 *	@sdev:	scsi device to resume.
 *
 *	Moves the device from quiesced back to running and restarts the
 *	queues.
 *
 *	Must be called with user context, may sleep.
 */
void
scsi_device_resume(struct scsi_device *sdev)
{
	if(scsi_device_set_state(sdev, SDEV_RUNNING))
		return;
	scsi_run_queue(sdev->request_queue);
}
EXPORT_SYMBOL(scsi_device_resume);

static void
device_quiesce_fn(struct scsi_device *sdev, void *data)
{
	scsi_device_quiesce(sdev);
}

void
scsi_target_quiesce(struct scsi_target *starget)
{
	starget_for_each_device(starget, NULL, device_quiesce_fn);
}
EXPORT_SYMBOL(scsi_target_quiesce);

static void
device_resume_fn(struct scsi_device *sdev, void *data)
{
	scsi_device_resume(sdev);
}

void
scsi_target_resume(struct scsi_target *starget)
{
	starget_for_each_device(starget, NULL, device_resume_fn);
}
EXPORT_SYMBOL(scsi_target_resume);

/**
 * scsi_internal_device_block - internal function to put a device temporarily into the SDEV_BLOCK state
 * @sdev:	device to block
 *
 * Block request made by scsi lld's to temporarily stop all
 * scsi commands on the specified device.  Called from interrupt
 * or normal process context.
 *
 * Returns zero if successful or error if not
 *
 * Notes:       
 *	This routine transitions the device to the SDEV_BLOCK state
 *	(which must be a legal transition).  When the device is in this
 *	state, all commands are deferred until the scsi lld reenables
 *	the device with scsi_device_unblock or device_block_tmo fires.
 *	This routine assumes the host_lock is held on entry.
 */
int
scsi_internal_device_block(struct scsi_device *sdev)
{
	struct request_queue *q = sdev->request_queue;
	unsigned long flags;
	int err = 0;

	err = scsi_device_set_state(sdev, SDEV_BLOCK);
	if (err) {
		err = scsi_device_set_state(sdev, SDEV_CREATED_BLOCK);

		if (err)
			return err;
	}

	/* 
	 * The device has transitioned to SDEV_BLOCK.  Stop the
	 * block layer from calling the midlayer with this device's
	 * request queue. 
	 */
	spin_lock_irqsave(q->queue_lock, flags);
	blk_stop_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(scsi_internal_device_block);
 
/**
 * scsi_internal_device_unblock - resume a device after a block request
 * @sdev:	device to resume
 *
 * Called by scsi lld's or the midlayer to restart the device queue
 * for the previously suspended scsi device.  Called from interrupt or
 * normal process context.
 *
 * Returns zero if successful or error if not.
 *
 * Notes:       
 *	This routine transitions the device to the SDEV_RUNNING state
 *	(which must be a legal transition) allowing the midlayer to
 *	goose the queue for this device.  This routine assumes the 
 *	host_lock is held upon entry.
 */
int
scsi_internal_device_unblock(struct scsi_device *sdev)
{
	struct request_queue *q = sdev->request_queue; 
	unsigned long flags;
	
	/* 
	 * Try to transition the scsi device to SDEV_RUNNING
	 * and goose the device queue if successful.  
	 */
	if (sdev->sdev_state == SDEV_BLOCK)
		sdev->sdev_state = SDEV_RUNNING;
	else if (sdev->sdev_state == SDEV_CREATED_BLOCK)
		sdev->sdev_state = SDEV_CREATED;
	else if (sdev->sdev_state != SDEV_CANCEL &&
		 sdev->sdev_state != SDEV_OFFLINE)
		return -EINVAL;

	spin_lock_irqsave(q->queue_lock, flags);
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(scsi_internal_device_unblock);

static void
device_block(struct scsi_device *sdev, void *data)
{
	scsi_internal_device_block(sdev);
}

static int
target_block(struct device *dev, void *data)
{
	if (scsi_is_target_device(dev))
		starget_for_each_device(to_scsi_target(dev), NULL,
					device_block);
	return 0;
}

void
scsi_target_block(struct device *dev)
{
	if (scsi_is_target_device(dev))
		starget_for_each_device(to_scsi_target(dev), NULL,
					device_block);
	else
		device_for_each_child(dev, NULL, target_block);
}
EXPORT_SYMBOL_GPL(scsi_target_block);

static void
device_unblock(struct scsi_device *sdev, void *data)
{
	scsi_internal_device_unblock(sdev);
}

static int
target_unblock(struct device *dev, void *data)
{
	if (scsi_is_target_device(dev))
		starget_for_each_device(to_scsi_target(dev), NULL,
					device_unblock);
	return 0;
}

void
scsi_target_unblock(struct device *dev)
{
	if (scsi_is_target_device(dev))
		starget_for_each_device(to_scsi_target(dev), NULL,
					device_unblock);
	else
		device_for_each_child(dev, NULL, target_unblock);
}
EXPORT_SYMBOL_GPL(scsi_target_unblock);

/**
 * scsi_kmap_atomic_sg - find and atomically map an sg-elemnt
 * @sgl:	scatter-gather list
 * @sg_count:	number of segments in sg
 * @offset:	offset in bytes into sg, on return offset into the mapped area
 * @len:	bytes to map, on return number of bytes mapped
 *
 * Returns virtual address of the start of the mapped page
 */
void *scsi_kmap_atomic_sg(struct scatterlist *sgl, int sg_count,
			  size_t *offset, size_t *len)
{
	int i;
	size_t sg_len = 0, len_complete = 0;
	struct scatterlist *sg;
	struct page *page;

	WARN_ON(!irqs_disabled());

	for_each_sg(sgl, sg, sg_count, i) {
		len_complete = sg_len; /* Complete sg-entries */
		sg_len += sg->length;
		if (sg_len > *offset)
			break;
	}

	if (unlikely(i == sg_count)) {
		printk(KERN_ERR "%s: Bytes in sg: %zu, requested offset %zu, "
			"elements %d\n",
		       __func__, sg_len, *offset, sg_count);
		WARN_ON(1);
		return NULL;
	}

	/* Offset starting from the beginning of first page in this sg-entry */
	*offset = *offset - len_complete + sg->offset;

	/* Assumption: contiguous pages can be accessed as "page + i" */
	page = nth_page(sg_page(sg), (*offset >> PAGE_SHIFT));
	*offset &= ~PAGE_MASK;

	/* Bytes in this sg-entry from *offset to the end of the page */
	sg_len = PAGE_SIZE - *offset;
	if (*len > sg_len)
		*len = sg_len;

	return kmap_atomic(page, KM_BIO_SRC_IRQ);
}
EXPORT_SYMBOL(scsi_kmap_atomic_sg);

/**
 * scsi_kunmap_atomic_sg - atomically unmap a virtual address, previously mapped with scsi_kmap_atomic_sg
 * @virt:	virtual address to be unmapped
 */
void scsi_kunmap_atomic_sg(void *virt)
{
	kunmap_atomic(virt, KM_BIO_SRC_IRQ);
}
EXPORT_SYMBOL(scsi_kunmap_atomic_sg);
