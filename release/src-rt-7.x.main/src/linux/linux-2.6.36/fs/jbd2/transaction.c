/*
 * linux/fs/jbd2/transaction.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Generic filesystem transaction handling code; part of the ext2fs
 * journaling system.
 *
 * This file manages transactions (compound commits managed by the
 * journaling code) and handles (individual atomic operations by the
 * filesystem).
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/hrtimer.h>
#include <linux/backing-dev.h>
#include <linux/module.h>

static void __jbd2_journal_temp_unlink_buffer(struct journal_head *jh);

/*
 * jbd2_get_transaction: obtain a new transaction_t object.
 *
 * Simply allocate and initialise a new transaction.  Create it in
 * RUNNING state and add it to the current journal (which should not
 * have an existing running transaction: we only make a new transaction
 * once we have started to commit the old one).
 *
 * Preconditions:
 *	The journal MUST be locked.  We don't perform atomic mallocs on the
 *	new transaction	and we can't block without protecting against other
 *	processes trying to touch the journal while it is in transition.
 *
 */

static transaction_t *
jbd2_get_transaction(journal_t *journal, transaction_t *transaction)
{
	transaction->t_journal = journal;
	transaction->t_state = T_RUNNING;
	transaction->t_start_time = ktime_get();
	transaction->t_tid = journal->j_transaction_sequence++;
	transaction->t_expires = jiffies + journal->j_commit_interval;
	spin_lock_init(&transaction->t_handle_lock);
	atomic_set(&transaction->t_updates, 0);
	atomic_set(&transaction->t_outstanding_credits, 0);
	atomic_set(&transaction->t_handle_count, 0);
	INIT_LIST_HEAD(&transaction->t_inode_list);
	INIT_LIST_HEAD(&transaction->t_private_list);

	/* Set up the commit timer for the new transaction. */
	journal->j_commit_timer.expires = round_jiffies_up(transaction->t_expires);
	add_timer(&journal->j_commit_timer);

	J_ASSERT(journal->j_running_transaction == NULL);
	journal->j_running_transaction = transaction;
	transaction->t_max_wait = 0;
	transaction->t_start = jiffies;

	return transaction;
}

/*
 * Handle management.
 *
 * A handle_t is an object which represents a single atomic update to a
 * filesystem, and which tracks all of the modifications which form part
 * of that one update.
 */

/*
 * Update transiaction's maximum wait time, if debugging is enabled.
 *
 * In order for t_max_wait to be reliable, it must be protected by a
 * lock.  But doing so will mean that start_this_handle() can not be
 * run in parallel on SMP systems, which limits our scalability.  So
 * unless debugging is enabled, we no longer update t_max_wait, which
 * means that maximum wait time reported by the jbd2_run_stats
 * tracepoint will always be zero.
 */
static inline void update_t_max_wait(transaction_t *transaction)
{
#ifdef CONFIG_JBD2_DEBUG
	unsigned long ts = jiffies;

	if (jbd2_journal_enable_debug &&
	    time_after(transaction->t_start, ts)) {
		ts = jbd2_time_diff(ts, transaction->t_start);
		spin_lock(&transaction->t_handle_lock);
		if (ts > transaction->t_max_wait)
			transaction->t_max_wait = ts;
		spin_unlock(&transaction->t_handle_lock);
	}
#endif
}

/*
 * start_this_handle: Given a handle, deal with any locking or stalling
 * needed to make sure that there is enough journal space for the handle
 * to begin.  Attach the handle to a transaction and set up the
 * transaction's buffer credits.
 */

static int start_this_handle(journal_t *journal, handle_t *handle,
			     int gfp_mask)
{
	transaction_t *transaction;
	int needed;
	int nblocks = handle->h_buffer_credits;
	transaction_t *new_transaction = NULL;

	if (nblocks > journal->j_max_transaction_buffers) {
		printk(KERN_ERR "JBD: %s wants too many credits (%d > %d)\n",
		       current->comm, nblocks,
		       journal->j_max_transaction_buffers);
		return -ENOSPC;
	}

alloc_transaction:
	if (!journal->j_running_transaction) {
		new_transaction = kzalloc(sizeof(*new_transaction), gfp_mask);
		if (!new_transaction) {
			/*
			 * If __GFP_FS is not present, then we may be
			 * being called from inside the fs writeback
			 * layer, so we MUST NOT fail.  Since
			 * __GFP_NOFAIL is going away, we will arrange
			 * to retry the allocation ourselves.
			 */
			if ((gfp_mask & __GFP_FS) == 0) {
				congestion_wait(BLK_RW_ASYNC, HZ/50);
				goto alloc_transaction;
			}
			return -ENOMEM;
		}
	}

	jbd_debug(3, "New handle %p going live.\n", handle);

	/*
	 * We need to hold j_state_lock until t_updates has been incremented,
	 * for proper journal barrier handling
	 */
repeat:
	read_lock(&journal->j_state_lock);
	if (is_journal_aborted(journal) ||
	    (journal->j_errno != 0 && !(journal->j_flags & JBD2_ACK_ERR))) {
		read_unlock(&journal->j_state_lock);
		kfree(new_transaction);
		return -EROFS;
	}

	/* Wait on the journal's transaction barrier if necessary */
	if (journal->j_barrier_count) {
		read_unlock(&journal->j_state_lock);
		wait_event(journal->j_wait_transaction_locked,
				journal->j_barrier_count == 0);
		goto repeat;
	}

	if (!journal->j_running_transaction) {
		read_unlock(&journal->j_state_lock);
		if (!new_transaction)
			goto alloc_transaction;
		write_lock(&journal->j_state_lock);
		if (!journal->j_running_transaction) {
			jbd2_get_transaction(journal, new_transaction);
			new_transaction = NULL;
		}
		write_unlock(&journal->j_state_lock);
		goto repeat;
	}

	transaction = journal->j_running_transaction;

	/*
	 * If the current transaction is locked down for commit, wait for the
	 * lock to be released.
	 */
	if (transaction->t_state == T_LOCKED) {
		DEFINE_WAIT(wait);

		prepare_to_wait(&journal->j_wait_transaction_locked,
					&wait, TASK_UNINTERRUPTIBLE);
		read_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_transaction_locked, &wait);
		goto repeat;
	}

	/*
	 * If there is not enough space left in the log to write all potential
	 * buffers requested by this operation, we need to stall pending a log
	 * checkpoint to free some more log space.
	 */
	needed = atomic_add_return(nblocks,
				   &transaction->t_outstanding_credits);

	if (needed > journal->j_max_transaction_buffers) {
		/*
		 * If the current transaction is already too large, then start
		 * to commit it: we can then go back and attach this handle to
		 * a new transaction.
		 */
		DEFINE_WAIT(wait);

		jbd_debug(2, "Handle %p starting new commit...\n", handle);
		atomic_sub(nblocks, &transaction->t_outstanding_credits);
		prepare_to_wait(&journal->j_wait_transaction_locked, &wait,
				TASK_UNINTERRUPTIBLE);
		__jbd2_log_start_commit(journal, transaction->t_tid);
		read_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_transaction_locked, &wait);
		goto repeat;
	}

	/*
	 * The commit code assumes that it can get enough log space
	 * without forcing a checkpoint.  This is *critical* for
	 * correctness: a checkpoint of a buffer which is also
	 * associated with a committing transaction creates a deadlock,
	 * so commit simply cannot force through checkpoints.
	 *
	 * We must therefore ensure the necessary space in the journal
	 * *before* starting to dirty potentially checkpointed buffers
	 * in the new transaction.
	 *
	 * The worst part is, any transaction currently committing can
	 * reduce the free space arbitrarily.  Be careful to account for
	 * those buffers when checkpointing.
	 */

	/*
	 * @@@ AKPM: This seems rather over-defensive.  We're giving commit
	 * a _lot_ of headroom: 1/4 of the journal plus the size of
	 * the committing transaction.  Really, we only need to give it
	 * committing_transaction->t_outstanding_credits plus "enough" for
	 * the log control blocks.
	 * Also, this test is inconsitent with the matching one in
	 * jbd2_journal_extend().
	 */
	if (__jbd2_log_space_left(journal) < jbd_space_needed(journal)) {
		jbd_debug(2, "Handle %p waiting for checkpoint...\n", handle);
		atomic_sub(nblocks, &transaction->t_outstanding_credits);
		read_unlock(&journal->j_state_lock);
		write_lock(&journal->j_state_lock);
		if (__jbd2_log_space_left(journal) < jbd_space_needed(journal))
			__jbd2_log_wait_for_space(journal);
		write_unlock(&journal->j_state_lock);
		goto repeat;
	}

	/* OK, account for the buffers that this operation expects to
	 * use and add the handle to the running transaction. 
	 */
	update_t_max_wait(transaction);
	handle->h_transaction = transaction;
	atomic_inc(&transaction->t_updates);
	atomic_inc(&transaction->t_handle_count);
	jbd_debug(4, "Handle %p given %d credits (total %d, free %d)\n",
		  handle, nblocks,
		  atomic_read(&transaction->t_outstanding_credits),
		  __jbd2_log_space_left(journal));
	read_unlock(&journal->j_state_lock);

	lock_map_acquire(&handle->h_lockdep_map);
	kfree(new_transaction);
	return 0;
}

static struct lock_class_key jbd2_handle_key;

/* Allocate a new handle.  This should probably be in a slab... */
static handle_t *new_handle(int nblocks)
{
	handle_t *handle = jbd2_alloc_handle(GFP_NOFS);
	if (!handle)
		return NULL;
	memset(handle, 0, sizeof(*handle));
	handle->h_buffer_credits = nblocks;
	handle->h_ref = 1;

	lockdep_init_map(&handle->h_lockdep_map, "jbd2_handle",
						&jbd2_handle_key, 0);

	return handle;
}

/**
 * handle_t *jbd2_journal_start() - Obtain a new handle.
 * @journal: Journal to start transaction on.
 * @nblocks: number of block buffer we might modify
 *
 * We make sure that the transaction can guarantee at least nblocks of
 * modified buffers in the log.  We block until the log can guarantee
 * that much space.
 *
 * This function is visible to journal users (like ext3fs), so is not
 * called with the journal already locked.
 *
 * Return a pointer to a newly allocated handle, or NULL on failure
 */
handle_t *jbd2__journal_start(journal_t *journal, int nblocks, int gfp_mask)
{
	handle_t *handle = journal_current_handle();
	int err;

	if (!journal)
		return ERR_PTR(-EROFS);

	if (handle) {
		J_ASSERT(handle->h_transaction->t_journal == journal);
		handle->h_ref++;
		return handle;
	}

	handle = new_handle(nblocks);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	current->journal_info = handle;

	err = start_this_handle(journal, handle, gfp_mask);
	if (err < 0) {
		jbd2_free_handle(handle);
		current->journal_info = NULL;
		handle = ERR_PTR(err);
		goto out;
	}
out:
	return handle;
}
EXPORT_SYMBOL(jbd2__journal_start);


handle_t *jbd2_journal_start(journal_t *journal, int nblocks)
{
	return jbd2__journal_start(journal, nblocks, GFP_NOFS);
}
EXPORT_SYMBOL(jbd2_journal_start);


/**
 * int jbd2_journal_extend() - extend buffer credits.
 * @handle:  handle to 'extend'
 * @nblocks: nr blocks to try to extend by.
 *
 * Some transactions, such as large extends and truncates, can be done
 * atomically all at once or in several stages.  The operation requests
 * a credit for a number of buffer modications in advance, but can
 * extend its credit if it needs more.
 *
 * jbd2_journal_extend tries to give the running handle more buffer credits.
 * It does not guarantee that allocation - this is a best-effort only.
 * The calling process MUST be able to deal cleanly with a failure to
 * extend here.
 *
 * Return 0 on success, non-zero on failure.
 *
 * return code < 0 implies an error
 * return code > 0 implies normal transaction-full status.
 */
int jbd2_journal_extend(handle_t *handle, int nblocks)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int result;
	int wanted;

	result = -EIO;
	if (is_handle_aborted(handle))
		goto out;

	result = 1;

	read_lock(&journal->j_state_lock);

	/* Don't extend a locked-down transaction! */
	if (handle->h_transaction->t_state != T_RUNNING) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction not running\n", handle, nblocks);
		goto error_out;
	}

	spin_lock(&transaction->t_handle_lock);
	wanted = atomic_read(&transaction->t_outstanding_credits) + nblocks;

	if (wanted > journal->j_max_transaction_buffers) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction too large\n", handle, nblocks);
		goto unlock;
	}

	if (wanted > __jbd2_log_space_left(journal)) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "insufficient log space\n", handle, nblocks);
		goto unlock;
	}

	handle->h_buffer_credits += nblocks;
	atomic_add(nblocks, &transaction->t_outstanding_credits);
	result = 0;

	jbd_debug(3, "extended handle %p by %d\n", handle, nblocks);
unlock:
	spin_unlock(&transaction->t_handle_lock);
error_out:
	read_unlock(&journal->j_state_lock);
out:
	return result;
}


/**
 * int jbd2_journal_restart() - restart a handle .
 * @handle:  handle to restart
 * @nblocks: nr credits requested
 *
 * Restart a handle for a multi-transaction filesystem
 * operation.
 *
 * If the jbd2_journal_extend() call above fails to grant new buffer credits
 * to a running handle, a call to jbd2_journal_restart will commit the
 * handle's transaction so far and reattach the handle to a new
 * transaction capabable of guaranteeing the requested number of
 * credits.
 */
int jbd2__journal_restart(handle_t *handle, int nblocks, int gfp_mask)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int ret;

	/* If we've had an abort of any type, don't even think about
	 * actually doing the restart! */
	if (is_handle_aborted(handle))
		return 0;

	/*
	 * First unlink the handle from its current transaction, and start the
	 * commit on that.
	 */
	J_ASSERT(atomic_read(&transaction->t_updates) > 0);
	J_ASSERT(journal_current_handle() == handle);

	read_lock(&journal->j_state_lock);
	spin_lock(&transaction->t_handle_lock);
	atomic_sub(handle->h_buffer_credits,
		   &transaction->t_outstanding_credits);
	if (atomic_dec_and_test(&transaction->t_updates))
		wake_up(&journal->j_wait_updates);
	spin_unlock(&transaction->t_handle_lock);

	jbd_debug(2, "restarting handle %p\n", handle);
	__jbd2_log_start_commit(journal, transaction->t_tid);
	read_unlock(&journal->j_state_lock);

	lock_map_release(&handle->h_lockdep_map);
	handle->h_buffer_credits = nblocks;
	ret = start_this_handle(journal, handle, gfp_mask);
	return ret;
}
EXPORT_SYMBOL(jbd2__journal_restart);


int jbd2_journal_restart(handle_t *handle, int nblocks)
{
	return jbd2__journal_restart(handle, nblocks, GFP_NOFS);
}
EXPORT_SYMBOL(jbd2_journal_restart);

/**
 * void jbd2_journal_lock_updates () - establish a transaction barrier.
 * @journal:  Journal to establish a barrier on.
 *
 * This locks out any further updates from being started, and blocks
 * until all existing updates have completed, returning only once the
 * journal is in a quiescent state with no updates running.
 *
 * The journal lock should not be held on entry.
 */
void jbd2_journal_lock_updates(journal_t *journal)
{
	DEFINE_WAIT(wait);

	write_lock(&journal->j_state_lock);
	++journal->j_barrier_count;

	/* Wait until there are no running updates */
	while (1) {
		transaction_t *transaction = journal->j_running_transaction;

		if (!transaction)
			break;

		spin_lock(&transaction->t_handle_lock);
		if (!atomic_read(&transaction->t_updates)) {
			spin_unlock(&transaction->t_handle_lock);
			break;
		}
		prepare_to_wait(&journal->j_wait_updates, &wait,
				TASK_UNINTERRUPTIBLE);
		spin_unlock(&transaction->t_handle_lock);
		write_unlock(&journal->j_state_lock);
		schedule();
		finish_wait(&journal->j_wait_updates, &wait);
		write_lock(&journal->j_state_lock);
	}
	write_unlock(&journal->j_state_lock);

	/*
	 * We have now established a barrier against other normal updates, but
	 * we also need to barrier against other jbd2_journal_lock_updates() calls
	 * to make sure that we serialise special journal-locked operations
	 * too.
	 */
	mutex_lock(&journal->j_barrier);
}

/**
 * void jbd2_journal_unlock_updates (journal_t* journal) - release barrier
 * @journal:  Journal to release the barrier on.
 *
 * Release a transaction barrier obtained with jbd2_journal_lock_updates().
 *
 * Should be called without the journal lock held.
 */
void jbd2_journal_unlock_updates (journal_t *journal)
{
	J_ASSERT(journal->j_barrier_count != 0);

	mutex_unlock(&journal->j_barrier);
	write_lock(&journal->j_state_lock);
	--journal->j_barrier_count;
	write_unlock(&journal->j_state_lock);
	wake_up(&journal->j_wait_transaction_locked);
}

static void warn_dirty_buffer(struct buffer_head *bh)
{
	char b[BDEVNAME_SIZE];

	printk(KERN_WARNING
	       "JBD: Spotted dirty metadata buffer (dev = %s, blocknr = %llu). "
	       "There's a risk of filesystem corruption in case of system "
	       "crash.\n",
	       bdevname(bh->b_bdev, b), (unsigned long long)bh->b_blocknr);
}

/*
 * If the buffer is already part of the current transaction, then there
 * is nothing we need to do.  If it is already part of a prior
 * transaction which we are still committing to disk, then we need to
 * make sure that we do not overwrite the old copy: we do copy-out to
 * preserve the copy going to disk.  We also account the buffer against
 * the handle's metadata buffer credits (unless the buffer is already
 * part of the transaction, that is).
 *
 */
static int
do_get_write_access(handle_t *handle, struct journal_head *jh,
			int force_copy)
{
	struct buffer_head *bh;
	transaction_t *transaction;
	journal_t *journal;
	int error;
	char *frozen_buffer = NULL;
	int need_copy = 0;

	if (is_handle_aborted(handle))
		return -EROFS;

	transaction = handle->h_transaction;
	journal = transaction->t_journal;

	jbd_debug(5, "buffer_head %p, force_copy %d\n", jh, force_copy);

	JBUFFER_TRACE(jh, "entry");
repeat:
	bh = jh2bh(jh);

	/* @@@ Need to check for errors here at some point. */

	lock_buffer(bh);
	jbd_lock_bh_state(bh);

	/* We now hold the buffer lock so it is safe to query the buffer
	 * state.  Is the buffer dirty?
	 *
	 * If so, there are two possibilities.  The buffer may be
	 * non-journaled, and undergoing a quite legitimate writeback.
	 * Otherwise, it is journaled, and we don't expect dirty buffers
	 * in that state (the buffers should be marked JBD_Dirty
	 * instead.)  So either the IO is being done under our own
	 * control and this is a bug, or it's a third party IO such as
	 * dump(8) (which may leave the buffer scheduled for read ---
	 * ie. locked but not dirty) or tune2fs (which may actually have
	 * the buffer dirtied, ugh.)  */

	if (buffer_dirty(bh)) {
		/*
		 * First question: is this buffer already part of the current
		 * transaction or the existing committing transaction?
		 */
		if (jh->b_transaction) {
			J_ASSERT_JH(jh,
				jh->b_transaction == transaction ||
				jh->b_transaction ==
					journal->j_committing_transaction);
			if (jh->b_next_transaction)
				J_ASSERT_JH(jh, jh->b_next_transaction ==
							transaction);
			warn_dirty_buffer(bh);
		}
		/*
		 * In any case we need to clean the dirty flag and we must
		 * do it under the buffer lock to be sure we don't race
		 * with running write-out.
		 */
		JBUFFER_TRACE(jh, "Journalling dirty buffer");
		clear_buffer_dirty(bh);
		set_buffer_jbddirty(bh);
	}

	unlock_buffer(bh);

	error = -EROFS;
	if (is_handle_aborted(handle)) {
		jbd_unlock_bh_state(bh);
		goto out;
	}
	error = 0;

	/*
	 * The buffer is already part of this transaction if b_transaction or
	 * b_next_transaction points to it
	 */
	if (jh->b_transaction == transaction ||
	    jh->b_next_transaction == transaction)
		goto done;

	/*
	 * this is the first time this transaction is touching this buffer,
	 * reset the modified flag
	 */
       jh->b_modified = 0;

	/*
	 * If there is already a copy-out version of this buffer, then we don't
	 * need to make another one
	 */
	if (jh->b_frozen_data) {
		JBUFFER_TRACE(jh, "has frozen data");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		jh->b_next_transaction = transaction;
		goto done;
	}

	/* Is there data here we need to preserve? */

	if (jh->b_transaction && jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "owned by older transaction");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);

		/* There is one case we have to be very careful about.
		 * If the committing transaction is currently writing
		 * this buffer out to disk and has NOT made a copy-out,
		 * then we cannot modify the buffer contents at all
		 * right now.  The essence of copy-out is that it is the
		 * extra copy, not the primary copy, which gets
		 * journaled.  If the primary copy is already going to
		 * disk then we cannot do copy-out here. */

		if (jh->b_jlist == BJ_Shadow) {
			DEFINE_WAIT_BIT(wait, &bh->b_state, BH_Unshadow);
			wait_queue_head_t *wqh;

			wqh = bit_waitqueue(&bh->b_state, BH_Unshadow);

			JBUFFER_TRACE(jh, "on shadow: sleep");
			jbd_unlock_bh_state(bh);
			/* commit wakes up all shadow buffers after IO */
			for ( ; ; ) {
				prepare_to_wait(wqh, &wait.wait,
						TASK_UNINTERRUPTIBLE);
				if (jh->b_jlist != BJ_Shadow)
					break;
				schedule();
			}
			finish_wait(wqh, &wait.wait);
			goto repeat;
		}

		/* Only do the copy if the currently-owning transaction
		 * still needs it.  If it is on the Forget list, the
		 * committing transaction is past that stage.  The
		 * buffer had better remain locked during the kmalloc,
		 * but that should be true --- we hold the journal lock
		 * still and the buffer is already on the BUF_JOURNAL
		 * list so won't be flushed.
		 *
		 * Subtle point, though: if this is a get_undo_access,
		 * then we will be relying on the frozen_data to contain
		 * the new value of the committed_data record after the
		 * transaction, so we HAVE to force the frozen_data copy
		 * in that case. */

		if (jh->b_jlist != BJ_Forget || force_copy) {
			JBUFFER_TRACE(jh, "generate frozen data");
			if (!frozen_buffer) {
				JBUFFER_TRACE(jh, "allocate memory for buffer");
				jbd_unlock_bh_state(bh);
				frozen_buffer =
					jbd2_alloc(jh2bh(jh)->b_size,
							 GFP_NOFS);
				if (!frozen_buffer) {
					printk(KERN_EMERG
					       "%s: OOM for frozen_buffer\n",
					       __func__);
					JBUFFER_TRACE(jh, "oom!");
					error = -ENOMEM;
					jbd_lock_bh_state(bh);
					goto done;
				}
				goto repeat;
			}
			jh->b_frozen_data = frozen_buffer;
			frozen_buffer = NULL;
			need_copy = 1;
		}
		jh->b_next_transaction = transaction;
	}


	/*
	 * Finally, if the buffer is not journaled right now, we need to make
	 * sure it doesn't get written to disk before the caller actually
	 * commits the new data
	 */
	if (!jh->b_transaction) {
		JBUFFER_TRACE(jh, "no transaction");
		J_ASSERT_JH(jh, !jh->b_next_transaction);
		jh->b_transaction = transaction;
		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		spin_lock(&journal->j_list_lock);
		__jbd2_journal_file_buffer(jh, transaction, BJ_Reserved);
		spin_unlock(&journal->j_list_lock);
	}

done:
	if (need_copy) {
		struct page *page;
		int offset;
		char *source;

		J_EXPECT_JH(jh, buffer_uptodate(jh2bh(jh)),
			    "Possible IO failure.\n");
		page = jh2bh(jh)->b_page;
		offset = ((unsigned long) jh2bh(jh)->b_data) & ~PAGE_MASK;
		source = kmap_atomic(page, KM_USER0);
		/* Fire data frozen trigger just before we copy the data */
		jbd2_buffer_frozen_trigger(jh, source + offset,
					   jh->b_triggers);
		memcpy(jh->b_frozen_data, source+offset, jh2bh(jh)->b_size);
		kunmap_atomic(source, KM_USER0);

		/*
		 * Now that the frozen data is saved off, we need to store
		 * any matching triggers.
		 */
		jh->b_frozen_triggers = jh->b_triggers;
	}
	jbd_unlock_bh_state(bh);

	/*
	 * If we are about to journal a buffer, then any revoke pending on it is
	 * no longer valid
	 */
	jbd2_journal_cancel_revoke(handle, jh);

out:
	if (unlikely(frozen_buffer))	/* It's usually NULL */
		jbd2_free(frozen_buffer, bh->b_size);

	JBUFFER_TRACE(jh, "exit");
	return error;
}

/**
 * int jbd2_journal_get_write_access() - notify intent to modify a buffer for metadata (not data) update.
 * @handle: transaction to add buffer modifications to
 * @bh:     bh to be used for metadata writes
 * @credits: variable that will receive credits for the buffer
 *
 * Returns an error code or 0 on success.
 *
 * In full data journalling mode the buffer may be of type BJ_AsyncData,
 * because we're write()ing a buffer which is also part of a shared mapping.
 */

int jbd2_journal_get_write_access(handle_t *handle, struct buffer_head *bh)
{
	struct journal_head *jh = jbd2_journal_add_journal_head(bh);
	int rc;

	/* We do not want to get caught playing with fields which the
	 * log thread also manipulates.  Make sure that the buffer
	 * completes any outstanding IO before proceeding. */
	rc = do_get_write_access(handle, jh, 0);
	jbd2_journal_put_journal_head(jh);
	return rc;
}


/*
 * When the user wants to journal a newly created buffer_head
 * (ie. getblk() returned a new buffer and we are going to populate it
 * manually rather than reading off disk), then we need to keep the
 * buffer_head locked until it has been completely filled with new
 * data.  In this case, we should be able to make the assertion that
 * the bh is not already part of an existing transaction.
 *
 * The buffer should already be locked by the caller by this point.
 * There is no lock ranking violation: it was a newly created,
 * unlocked buffer beforehand. */

/**
 * int jbd2_journal_get_create_access () - notify intent to use newly created bh
 * @handle: transaction to new buffer to
 * @bh: new buffer.
 *
 * Call this if you create a new bh.
 */
int jbd2_journal_get_create_access(handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = jbd2_journal_add_journal_head(bh);
	int err;

	jbd_debug(5, "journal_head %p\n", jh);
	err = -EROFS;
	if (is_handle_aborted(handle))
		goto out;
	err = 0;

	JBUFFER_TRACE(jh, "entry");
	/*
	 * The buffer may already belong to this transaction due to pre-zeroing
	 * in the filesystem's new_block code.  It may also be on the previous,
	 * committing transaction's lists, but it HAS to be in Forget state in
	 * that case: the transaction must have deleted the buffer for it to be
	 * reused here.
	 */
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);
	J_ASSERT_JH(jh, (jh->b_transaction == transaction ||
		jh->b_transaction == NULL ||
		(jh->b_transaction == journal->j_committing_transaction &&
			  jh->b_jlist == BJ_Forget)));

	J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
	J_ASSERT_JH(jh, buffer_locked(jh2bh(jh)));

	if (jh->b_transaction == NULL) {
		/*
		 * Previous jbd2_journal_forget() could have left the buffer
		 * with jbddirty bit set because it was being committed. When
		 * the commit finished, we've filed the buffer for
		 * checkpointing and marked it dirty. Now we are reallocating
		 * the buffer so the transaction freeing it must have
		 * committed and so it's safe to clear the dirty bit.
		 */
		clear_buffer_dirty(jh2bh(jh));
		jh->b_transaction = transaction;

		/* first access by this transaction */
		jh->b_modified = 0;

		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		__jbd2_journal_file_buffer(jh, transaction, BJ_Reserved);
	} else if (jh->b_transaction == journal->j_committing_transaction) {
		/* first access by this transaction */
		jh->b_modified = 0;

		JBUFFER_TRACE(jh, "set next transaction");
		jh->b_next_transaction = transaction;
	}
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);

	/*
	 * akpm: I added this.  ext3_alloc_branch can pick up new indirect
	 * blocks which contain freed but then revoked metadata.  We need
	 * to cancel the revoke in case we end up freeing it yet again
	 * and the reallocating as data - this would cause a second revoke,
	 * which hits an assertion error.
	 */
	JBUFFER_TRACE(jh, "cancelling revoke");
	jbd2_journal_cancel_revoke(handle, jh);
	jbd2_journal_put_journal_head(jh);
out:
	return err;
}

/**
 * int jbd2_journal_get_undo_access() -  Notify intent to modify metadata with
 *     non-rewindable consequences
 * @handle: transaction
 * @bh: buffer to undo
 * @credits: store the number of taken credits here (if not NULL)
 *
 * Sometimes there is a need to distinguish between metadata which has
 * been committed to disk and that which has not.  The ext3fs code uses
 * this for freeing and allocating space, we have to make sure that we
 * do not reuse freed space until the deallocation has been committed,
 * since if we overwrote that space we would make the delete
 * un-rewindable in case of a crash.
 *
 * To deal with that, jbd2_journal_get_undo_access requests write access to a
 * buffer for parts of non-rewindable operations such as delete
 * operations on the bitmaps.  The journaling code must keep a copy of
 * the buffer's contents prior to the undo_access call until such time
 * as we know that the buffer has definitely been committed to disk.
 *
 * We never need to know which transaction the committed data is part
 * of, buffers touched here are guaranteed to be dirtied later and so
 * will be committed to a new transaction in due course, at which point
 * we can discard the old committed data pointer.
 *
 * Returns error number or 0 on success.
 */
int jbd2_journal_get_undo_access(handle_t *handle, struct buffer_head *bh)
{
	int err;
	struct journal_head *jh = jbd2_journal_add_journal_head(bh);
	char *committed_data = NULL;

	JBUFFER_TRACE(jh, "entry");

	/*
	 * Do this first --- it can drop the journal lock, so we want to
	 * make sure that obtaining the committed_data is done
	 * atomically wrt. completion of any outstanding commits.
	 */
	err = do_get_write_access(handle, jh, 1);
	if (err)
		goto out;

repeat:
	if (!jh->b_committed_data) {
		committed_data = jbd2_alloc(jh2bh(jh)->b_size, GFP_NOFS);
		if (!committed_data) {
			printk(KERN_EMERG "%s: No memory for committed data\n",
				__func__);
			err = -ENOMEM;
			goto out;
		}
	}

	jbd_lock_bh_state(bh);
	if (!jh->b_committed_data) {
		/* Copy out the current buffer contents into the
		 * preserved, committed copy. */
		JBUFFER_TRACE(jh, "generate b_committed data");
		if (!committed_data) {
			jbd_unlock_bh_state(bh);
			goto repeat;
		}

		jh->b_committed_data = committed_data;
		committed_data = NULL;
		memcpy(jh->b_committed_data, bh->b_data, bh->b_size);
	}
	jbd_unlock_bh_state(bh);
out:
	jbd2_journal_put_journal_head(jh);
	if (unlikely(committed_data))
		jbd2_free(committed_data, bh->b_size);
	return err;
}

/**
 * void jbd2_journal_set_triggers() - Add triggers for commit writeout
 * @bh: buffer to trigger on
 * @type: struct jbd2_buffer_trigger_type containing the trigger(s).
 *
 * Set any triggers on this journal_head.  This is always safe, because
 * triggers for a committing buffer will be saved off, and triggers for
 * a running transaction will match the buffer in that transaction.
 *
 * Call with NULL to clear the triggers.
 */
void jbd2_journal_set_triggers(struct buffer_head *bh,
			       struct jbd2_buffer_trigger_type *type)
{
	struct journal_head *jh = bh2jh(bh);

	jh->b_triggers = type;
}

void jbd2_buffer_frozen_trigger(struct journal_head *jh, void *mapped_data,
				struct jbd2_buffer_trigger_type *triggers)
{
	struct buffer_head *bh = jh2bh(jh);

	if (!triggers || !triggers->t_frozen)
		return;

	triggers->t_frozen(triggers, bh, mapped_data, bh->b_size);
}

void jbd2_buffer_abort_trigger(struct journal_head *jh,
			       struct jbd2_buffer_trigger_type *triggers)
{
	if (!triggers || !triggers->t_abort)
		return;

	triggers->t_abort(triggers, jh2bh(jh));
}



/**
 * int jbd2_journal_dirty_metadata() -  mark a buffer as containing dirty metadata
 * @handle: transaction to add buffer to.
 * @bh: buffer to mark
 *
 * mark dirty metadata which needs to be journaled as part of the current
 * transaction.
 *
 * The buffer is placed on the transaction's metadata list and is marked
 * as belonging to the transaction.
 *
 * Returns error number or 0 on success.
 *
 * Special care needs to be taken if the buffer already belongs to the
 * current committing transaction (in which case we should have frozen
 * data present for that commit).  In that case, we don't relink the
 * buffer: that only gets done when the old transaction finally
 * completes its commit.
 */
int jbd2_journal_dirty_metadata(handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = bh2jh(bh);

	jbd_debug(5, "journal_head %p\n", jh);
	JBUFFER_TRACE(jh, "entry");
	if (is_handle_aborted(handle))
		goto out;

	jbd_lock_bh_state(bh);

	if (jh->b_modified == 0) {
		/*
		 * This buffer's got modified and becoming part
		 * of the transaction. This needs to be done
		 * once a transaction -bzzz
		 */
		jh->b_modified = 1;
		J_ASSERT_JH(jh, handle->h_buffer_credits > 0);
		handle->h_buffer_credits--;
	}

	/*
	 * fastpath, to avoid expensive locking.  If this buffer is already
	 * on the running transaction's metadata list there is nothing to do.
	 * Nobody can take it off again because there is a handle open.
	 * I _think_ we're OK here with SMP barriers - a mistaken decision will
	 * result in this test being false, so we go in and take the locks.
	 */
	if (jh->b_transaction == transaction && jh->b_jlist == BJ_Metadata) {
		JBUFFER_TRACE(jh, "fastpath");
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_running_transaction);
		goto out_unlock_bh;
	}

	set_buffer_jbddirty(bh);

	/*
	 * Metadata already on the current transaction list doesn't
	 * need to be filed.  Metadata on another transaction's list must
	 * be committing, and will be refiled once the commit completes:
	 * leave it alone for now.
	 */
	if (jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "already on other transaction");
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);
		J_ASSERT_JH(jh, jh->b_next_transaction == transaction);
		/* And this case is illegal: we can't reuse another
		 * transaction's data buffer, ever. */
		goto out_unlock_bh;
	}

	/* That test should have eliminated the following case: */
	J_ASSERT_JH(jh, jh->b_frozen_data == NULL);

	JBUFFER_TRACE(jh, "file as BJ_Metadata");
	spin_lock(&journal->j_list_lock);
	__jbd2_journal_file_buffer(jh, handle->h_transaction, BJ_Metadata);
	spin_unlock(&journal->j_list_lock);
out_unlock_bh:
	jbd_unlock_bh_state(bh);
out:
	JBUFFER_TRACE(jh, "exit");
	return 0;
}

/*
 * jbd2_journal_release_buffer: undo a get_write_access without any buffer
 * updates, if the update decided in the end that it didn't need access.
 *
 */
void
jbd2_journal_release_buffer(handle_t *handle, struct buffer_head *bh)
{
	BUFFER_TRACE(bh, "entry");
}

/**
 * void jbd2_journal_forget() - bforget() for potentially-journaled buffers.
 * @handle: transaction handle
 * @bh:     bh to 'forget'
 *
 * We can only do the bforget if there are no commits pending against the
 * buffer.  If the buffer is dirty in the current running transaction we
 * can safely unlink it.
 *
 * bh may not be a journalled buffer at all - it may be a non-JBD
 * buffer which came off the hashtable.  Check for this.
 *
 * Decrements bh->b_count by one.
 *
 * Allow this call even if the handle has aborted --- it may be part of
 * the caller's cleanup after an abort.
 */
int jbd2_journal_forget (handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh;
	int drop_reserve = 0;
	int err = 0;
	int was_modified = 0;

	BUFFER_TRACE(bh, "entry");

	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	if (!buffer_jbd(bh))
		goto not_jbd;
	jh = bh2jh(bh);

	/* Critical error: attempting to delete a bitmap buffer, maybe?
	 * Don't do any jbd operations, and return an error. */
	if (!J_EXPECT_JH(jh, !jh->b_committed_data,
			 "inconsistent data on disk")) {
		err = -EIO;
		goto not_jbd;
	}

	/* keep track of wether or not this transaction modified us */
	was_modified = jh->b_modified;

	/*
	 * The buffer's going from the transaction, we must drop
	 * all references -bzzz
	 */
	jh->b_modified = 0;

	if (jh->b_transaction == handle->h_transaction) {
		J_ASSERT_JH(jh, !jh->b_frozen_data);

		/* If we are forgetting a buffer which is already part
		 * of this transaction, then we can just drop it from
		 * the transaction immediately. */
		clear_buffer_dirty(bh);
		clear_buffer_jbddirty(bh);

		JBUFFER_TRACE(jh, "belongs to current transaction: unfile");

		/*
		 * we only want to drop a reference if this transaction
		 * modified the buffer
		 */
		if (was_modified)
			drop_reserve = 1;

		/*
		 * We are no longer going to journal this buffer.
		 * However, the commit of this transaction is still
		 * important to the buffer: the delete that we are now
		 * processing might obsolete an old log entry, so by
		 * committing, we can satisfy the buffer's checkpoint.
		 *
		 * So, if we have a checkpoint on the buffer, we should
		 * now refile the buffer on our BJ_Forget list so that
		 * we know to remove the checkpoint after we commit.
		 */

		if (jh->b_cp_transaction) {
			__jbd2_journal_temp_unlink_buffer(jh);
			__jbd2_journal_file_buffer(jh, transaction, BJ_Forget);
		} else {
			__jbd2_journal_unfile_buffer(jh);
			jbd2_journal_remove_journal_head(bh);
			__brelse(bh);
			if (!buffer_jbd(bh)) {
				spin_unlock(&journal->j_list_lock);
				jbd_unlock_bh_state(bh);
				__bforget(bh);
				goto drop;
			}
		}
	} else if (jh->b_transaction) {
		J_ASSERT_JH(jh, (jh->b_transaction ==
				 journal->j_committing_transaction));
		/* However, if the buffer is still owned by a prior
		 * (committing) transaction, we can't drop it yet... */
		JBUFFER_TRACE(jh, "belongs to older transaction");
		/* ... but we CAN drop it from the new transaction if we
		 * have also modified it since the original commit. */

		if (jh->b_next_transaction) {
			J_ASSERT(jh->b_next_transaction == transaction);
			jh->b_next_transaction = NULL;

			/*
			 * only drop a reference if this transaction modified
			 * the buffer
			 */
			if (was_modified)
				drop_reserve = 1;
		}
	}

not_jbd:
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	__brelse(bh);
drop:
	if (drop_reserve) {
		/* no need to reserve log space for this block -bzzz */
		handle->h_buffer_credits++;
	}
	return err;
}

/**
 * int jbd2_journal_stop() - complete a transaction
 * @handle: tranaction to complete.
 *
 * All done for a particular handle.
 *
 * There is not much action needed here.  We just return any remaining
 * buffer credits to the transaction and remove the handle.  The only
 * complication is that we need to start a commit operation if the
 * filesystem is marked for synchronous update.
 *
 * jbd2_journal_stop itself will not usually return an error, but it may
 * do so in unusual circumstances.  In particular, expect it to
 * return -EIO if a jbd2_journal_abort has been executed since the
 * transaction began.
 */
int jbd2_journal_stop(handle_t *handle)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int err, wait_for_commit = 0;
	tid_t tid;
	pid_t pid;

	J_ASSERT(journal_current_handle() == handle);

	if (is_handle_aborted(handle))
		err = -EIO;
	else {
		J_ASSERT(atomic_read(&transaction->t_updates) > 0);
		err = 0;
	}

	if (--handle->h_ref > 0) {
		jbd_debug(4, "h_ref %d -> %d\n", handle->h_ref + 1,
			  handle->h_ref);
		return err;
	}

	jbd_debug(4, "Handle %p going down\n", handle);

	/*
	 * Implement synchronous transaction batching.  If the handle
	 * was synchronous, don't force a commit immediately.  Let's
	 * yield and let another thread piggyback onto this
	 * transaction.  Keep doing that while new threads continue to
	 * arrive.  It doesn't cost much - we're about to run a commit
	 * and sleep on IO anyway.  Speeds up many-threaded, many-dir
	 * operations by 30x or more...
	 *
	 * We try and optimize the sleep time against what the
	 * underlying disk can do, instead of having a static sleep
	 * time.  This is useful for the case where our storage is so
	 * fast that it is more optimal to go ahead and force a flush
	 * and wait for the transaction to be committed than it is to
	 * wait for an arbitrary amount of time for new writers to
	 * join the transaction.  We achieve this by measuring how
	 * long it takes to commit a transaction, and compare it with
	 * how long this transaction has been running, and if run time
	 * < commit time then we sleep for the delta and commit.  This
	 * greatly helps super fast disks that would see slowdowns as
	 * more threads started doing fsyncs.
	 *
	 * But don't do this if this process was the most recent one
	 * to perform a synchronous write.  We do this to detect the
	 * case where a single process is doing a stream of sync
	 * writes.  No point in waiting for joiners in that case.
	 */
	pid = current->pid;
	if (handle->h_sync && journal->j_last_sync_writer != pid) {
		u64 commit_time, trans_time;

		journal->j_last_sync_writer = pid;

		read_lock(&journal->j_state_lock);
		commit_time = journal->j_average_commit_time;
		read_unlock(&journal->j_state_lock);

		trans_time = ktime_to_ns(ktime_sub(ktime_get(),
						   transaction->t_start_time));

		commit_time = max_t(u64, commit_time,
				    1000*journal->j_min_batch_time);
		commit_time = min_t(u64, commit_time,
				    1000*journal->j_max_batch_time);

		if (trans_time < commit_time) {
			ktime_t expires = ktime_add_ns(ktime_get(),
						       commit_time);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_hrtimeout(&expires, HRTIMER_MODE_ABS);
		}
	}

	if (handle->h_sync)
		transaction->t_synchronous_commit = 1;
	current->journal_info = NULL;
	atomic_sub(handle->h_buffer_credits,
		   &transaction->t_outstanding_credits);

	/*
	 * If the handle is marked SYNC, we need to set another commit
	 * going!  We also want to force a commit if the current
	 * transaction is occupying too much of the log, or if the
	 * transaction is too old now.
	 */
	if (handle->h_sync ||
	    (atomic_read(&transaction->t_outstanding_credits) >
	     journal->j_max_transaction_buffers) ||
	    time_after_eq(jiffies, transaction->t_expires)) {
		/* Do this even for aborted journals: an abort still
		 * completes the commit thread, it just doesn't write
		 * anything to disk. */

		jbd_debug(2, "transaction too old, requesting commit for "
					"handle %p\n", handle);
		/* This is non-blocking */
		jbd2_log_start_commit(journal, transaction->t_tid);

		/*
		 * Special case: JBD2_SYNC synchronous updates require us
		 * to wait for the commit to complete.
		 */
		if (handle->h_sync && !(current->flags & PF_MEMALLOC))
			wait_for_commit = 1;
	}

	/*
	 * Once we drop t_updates, if it goes to zero the transaction
	 * could start commiting on us and eventually disappear.  So
	 * once we do this, we must not dereference transaction
	 * pointer again.
	 */
	tid = transaction->t_tid;
	if (atomic_dec_and_test(&transaction->t_updates)) {
		wake_up(&journal->j_wait_updates);
		if (journal->j_barrier_count)
			wake_up(&journal->j_wait_transaction_locked);
	}

	if (wait_for_commit)
		err = jbd2_log_wait_commit(journal, tid);

	lock_map_release(&handle->h_lockdep_map);

	jbd2_free_handle(handle);
	return err;
}

/**
 * int jbd2_journal_force_commit() - force any uncommitted transactions
 * @journal: journal to force
 *
 * For synchronous operations: force any uncommitted transactions
 * to disk.  May seem kludgy, but it reuses all the handle batching
 * code in a very simple manner.
 */
int jbd2_journal_force_commit(journal_t *journal)
{
	handle_t *handle;
	int ret;

	handle = jbd2_journal_start(journal, 1);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
	} else {
		handle->h_sync = 1;
		ret = jbd2_journal_stop(handle);
	}
	return ret;
}

/*
 *
 * List management code snippets: various functions for manipulating the
 * transaction buffer lists.
 *
 */

/*
 * Append a buffer to a transaction list, given the transaction's list head
 * pointer.
 *
 * j_list_lock is held.
 *
 * jbd_lock_bh_state(jh2bh(jh)) is held.
 */

static inline void
__blist_add_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (!*list) {
		jh->b_tnext = jh->b_tprev = jh;
		*list = jh;
	} else {
		/* Insert at the tail of the list to preserve order */
		struct journal_head *first = *list, *last = first->b_tprev;
		jh->b_tprev = last;
		jh->b_tnext = first;
		last->b_tnext = first->b_tprev = jh;
	}
}

/*
 * Remove a buffer from a transaction list, given the transaction's list
 * head pointer.
 *
 * Called with j_list_lock held, and the journal may not be locked.
 *
 * jbd_lock_bh_state(jh2bh(jh)) is held.
 */

static inline void
__blist_del_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (*list == jh) {
		*list = jh->b_tnext;
		if (*list == jh)
			*list = NULL;
	}
	jh->b_tprev->b_tnext = jh->b_tnext;
	jh->b_tnext->b_tprev = jh->b_tprev;
}

/*
 * Remove a buffer from the appropriate transaction list.
 *
 * Note that this function can *change* the value of
 * bh->b_transaction->t_buffers, t_forget, t_iobuf_list, t_shadow_list,
 * t_log_list or t_reserved_list.  If the caller is holding onto a copy of one
 * of these pointers, it could go bad.  Generally the caller needs to re-read
 * the pointer from the transaction_t.
 *
 * Called under j_list_lock.  The journal may not be locked.
 */
void __jbd2_journal_temp_unlink_buffer(struct journal_head *jh)
{
	struct journal_head **list = NULL;
	transaction_t *transaction;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	transaction = jh->b_transaction;
	if (transaction)
		assert_spin_locked(&transaction->t_journal->j_list_lock);

	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);
	if (jh->b_jlist != BJ_None)
		J_ASSERT_JH(jh, transaction != NULL);

	switch (jh->b_jlist) {
	case BJ_None:
		return;
	case BJ_Metadata:
		transaction->t_nr_buffers--;
		J_ASSERT_JH(jh, transaction->t_nr_buffers >= 0);
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	}

	__blist_del_buffer(list, jh);
	jh->b_jlist = BJ_None;
	if (test_clear_buffer_jbddirty(bh))
		mark_buffer_dirty(bh);	/* Expose it to the VM */
}

void __jbd2_journal_unfile_buffer(struct journal_head *jh)
{
	__jbd2_journal_temp_unlink_buffer(jh);
	jh->b_transaction = NULL;
}

void jbd2_journal_unfile_buffer(journal_t *journal, struct journal_head *jh)
{
	jbd_lock_bh_state(jh2bh(jh));
	spin_lock(&journal->j_list_lock);
	__jbd2_journal_unfile_buffer(jh);
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(jh2bh(jh));
}

/*
 * Called from jbd2_journal_try_to_free_buffers().
 *
 * Called under jbd_lock_bh_state(bh)
 */
static void
__journal_try_to_free_buffer(journal_t *journal, struct buffer_head *bh)
{
	struct journal_head *jh;

	jh = bh2jh(bh);

	if (buffer_locked(bh) || buffer_dirty(bh))
		goto out;

	if (jh->b_next_transaction != NULL)
		goto out;

	spin_lock(&journal->j_list_lock);
	if (jh->b_cp_transaction != NULL && jh->b_transaction == NULL) {
		/* written-back checkpointed metadata buffer */
		if (jh->b_jlist == BJ_None) {
			JBUFFER_TRACE(jh, "remove from checkpoint list");
			__jbd2_journal_remove_checkpoint(jh);
			jbd2_journal_remove_journal_head(bh);
			__brelse(bh);
		}
	}
	spin_unlock(&journal->j_list_lock);
out:
	return;
}

/**
 * int jbd2_journal_try_to_free_buffers() - try to free page buffers.
 * @journal: journal for operation
 * @page: to try and free
 * @gfp_mask: we use the mask to detect how hard should we try to release
 * buffers. If __GFP_WAIT and __GFP_FS is set, we wait for commit code to
 * release the buffers.
 *
 *
 * For all the buffers on this page,
 * if they are fully written out ordered data, move them onto BUF_CLEAN
 * so try_to_free_buffers() can reap them.
 *
 * This function returns non-zero if we wish try_to_free_buffers()
 * to be called. We do this if the page is releasable by try_to_free_buffers().
 * We also do it if the page has locked or dirty buffers and the caller wants
 * us to perform sync or async writeout.
 *
 * This complicates JBD locking somewhat.  We aren't protected by the
 * BKL here.  We wish to remove the buffer from its committing or
 * running transaction's ->t_datalist via __jbd2_journal_unfile_buffer.
 *
 * This may *change* the value of transaction_t->t_datalist, so anyone
 * who looks at t_datalist needs to lock against this function.
 *
 * Even worse, someone may be doing a jbd2_journal_dirty_data on this
 * buffer.  So we need to lock against that.  jbd2_journal_dirty_data()
 * will come out of the lock with the buffer dirty, which makes it
 * ineligible for release here.
 *
 * Who else is affected by this?  hmm...  Really the only contender
 * is do_get_write_access() - it could be looking at the buffer while
 * journal_try_to_free_buffer() is changing its state.  But that
 * cannot happen because we never reallocate freed data as metadata
 * while the data is part of a transaction.  Yes?
 *
 * Return 0 on failure, 1 on success
 */
int jbd2_journal_try_to_free_buffers(journal_t *journal,
				struct page *page, gfp_t gfp_mask)
{
	struct buffer_head *head;
	struct buffer_head *bh;
	int ret = 0;

	J_ASSERT(PageLocked(page));

	head = page_buffers(page);
	bh = head;
	do {
		struct journal_head *jh;

		/*
		 * We take our own ref against the journal_head here to avoid
		 * having to add tons of locking around each instance of
		 * jbd2_journal_remove_journal_head() and
		 * jbd2_journal_put_journal_head().
		 */
		jh = jbd2_journal_grab_journal_head(bh);
		if (!jh)
			continue;

		jbd_lock_bh_state(bh);
		__journal_try_to_free_buffer(journal, bh);
		jbd2_journal_put_journal_head(jh);
		jbd_unlock_bh_state(bh);
		if (buffer_jbd(bh))
			goto busy;
	} while ((bh = bh->b_this_page) != head);

	ret = try_to_free_buffers(page);

busy:
	return ret;
}

/*
 * This buffer is no longer needed.  If it is on an older transaction's
 * checkpoint list we need to record it on this transaction's forget list
 * to pin this buffer (and hence its checkpointing transaction) down until
 * this transaction commits.  If the buffer isn't on a checkpoint list, we
 * release it.
 * Returns non-zero if JBD no longer has an interest in the buffer.
 *
 * Called under j_list_lock.
 *
 * Called under jbd_lock_bh_state(bh).
 */
static int __dispose_buffer(struct journal_head *jh, transaction_t *transaction)
{
	int may_free = 1;
	struct buffer_head *bh = jh2bh(jh);

	__jbd2_journal_unfile_buffer(jh);

	if (jh->b_cp_transaction) {
		JBUFFER_TRACE(jh, "on running+cp transaction");
		/*
		 * We don't want to write the buffer anymore, clear the
		 * bit so that we don't confuse checks in
		 * __journal_file_buffer
		 */
		clear_buffer_dirty(bh);
		__jbd2_journal_file_buffer(jh, transaction, BJ_Forget);
		may_free = 0;
	} else {
		JBUFFER_TRACE(jh, "on running transaction");
		jbd2_journal_remove_journal_head(bh);
		__brelse(bh);
	}
	return may_free;
}

/*
 * jbd2_journal_invalidatepage
 *
 * This code is tricky.  It has a number of cases to deal with.
 *
 * There are two invariants which this code relies on:
 *
 * i_size must be updated on disk before we start calling invalidatepage on the
 * data.
 *
 *  This is done in ext3 by defining an ext3_setattr method which
 *  updates i_size before truncate gets going.  By maintaining this
 *  invariant, we can be sure that it is safe to throw away any buffers
 *  attached to the current transaction: once the transaction commits,
 *  we know that the data will not be needed.
 *
 *  Note however that we can *not* throw away data belonging to the
 *  previous, committing transaction!
 *
 * Any disk blocks which *are* part of the previous, committing
 * transaction (and which therefore cannot be discarded immediately) are
 * not going to be reused in the new running transaction
 *
 *  The bitmap committed_data images guarantee this: any block which is
 *  allocated in one transaction and removed in the next will be marked
 *  as in-use in the committed_data bitmap, so cannot be reused until
 *  the next transaction to delete the block commits.  This means that
 *  leaving committing buffers dirty is quite safe: the disk blocks
 *  cannot be reallocated to a different file and so buffer aliasing is
 *  not possible.
 *
 *
 * The above applies mainly to ordered data mode.  In writeback mode we
 * don't make guarantees about the order in which data hits disk --- in
 * particular we don't guarantee that new dirty data is flushed before
 * transaction commit --- so it is always safe just to discard data
 * immediately in that mode.  --sct
 */

/*
 * The journal_unmap_buffer helper function returns zero if the buffer
 * concerned remains pinned as an anonymous buffer belonging to an older
 * transaction.
 *
 * We're outside-transaction here.  Either or both of j_running_transaction
 * and j_committing_transaction may be NULL.
 */
static int journal_unmap_buffer(journal_t *journal, struct buffer_head *bh)
{
	transaction_t *transaction;
	struct journal_head *jh;
	int may_free = 1;
	int ret;

	BUFFER_TRACE(bh, "entry");

	/*
	 * It is safe to proceed here without the j_list_lock because the
	 * buffers cannot be stolen by try_to_free_buffers as long as we are
	 * holding the page lock. --sct
	 */

	if (!buffer_jbd(bh))
		goto zap_buffer_unlocked;

	/* OK, we have data buffer in journaled mode */
	write_lock(&journal->j_state_lock);
	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	jh = jbd2_journal_grab_journal_head(bh);
	if (!jh)
		goto zap_buffer_no_jh;

	/*
	 * We cannot remove the buffer from checkpoint lists until the
	 * transaction adding inode to orphan list (let's call it T)
	 * is committed.  Otherwise if the transaction changing the
	 * buffer would be cleaned from the journal before T is
	 * committed, a crash will cause that the correct contents of
	 * the buffer will be lost.  On the other hand we have to
	 * clear the buffer dirty bit at latest at the moment when the
	 * transaction marking the buffer as freed in the filesystem
	 * structures is committed because from that moment on the
	 * buffer can be reallocated and used by a different page.
	 * Since the block hasn't been freed yet but the inode has
	 * already been added to orphan list, it is safe for us to add
	 * the buffer to BJ_Forget list of the newest transaction.
	 */
	transaction = jh->b_transaction;
	if (transaction == NULL) {
		/* First case: not on any transaction.  If it
		 * has no checkpoint link, then we can zap it:
		 * it's a writeback-mode buffer so we don't care
		 * if it hits disk safely. */
		if (!jh->b_cp_transaction) {
			JBUFFER_TRACE(jh, "not on any transaction: zap");
			goto zap_buffer;
		}

		if (!buffer_dirty(bh)) {
			/* bdflush has written it.  We can drop it now */
			goto zap_buffer;
		}

		/* OK, it must be in the journal but still not
		 * written fully to disk: it's metadata or
		 * journaled data... */

		if (journal->j_running_transaction) {
			/* ... and once the current transaction has
			 * committed, the buffer won't be needed any
			 * longer. */
			JBUFFER_TRACE(jh, "checkpointed: add to BJ_Forget");
			ret = __dispose_buffer(jh,
					journal->j_running_transaction);
			jbd2_journal_put_journal_head(jh);
			spin_unlock(&journal->j_list_lock);
			jbd_unlock_bh_state(bh);
			write_unlock(&journal->j_state_lock);
			return ret;
		} else {
			/* There is no currently-running transaction. So the
			 * orphan record which we wrote for this file must have
			 * passed into commit.  We must attach this buffer to
			 * the committing transaction, if it exists. */
			if (journal->j_committing_transaction) {
				JBUFFER_TRACE(jh, "give to committing trans");
				ret = __dispose_buffer(jh,
					journal->j_committing_transaction);
				jbd2_journal_put_journal_head(jh);
				spin_unlock(&journal->j_list_lock);
				jbd_unlock_bh_state(bh);
				write_unlock(&journal->j_state_lock);
				return ret;
			} else {
				/* The orphan record's transaction has
				 * committed.  We can cleanse this buffer */
				clear_buffer_jbddirty(bh);
				goto zap_buffer;
			}
		}
	} else if (transaction == journal->j_committing_transaction) {
		JBUFFER_TRACE(jh, "on committing transaction");
		/*
		 * The buffer is committing, we simply cannot touch
		 * it. So we just set j_next_transaction to the
		 * running transaction (if there is one) and mark
		 * buffer as freed so that commit code knows it should
		 * clear dirty bits when it is done with the buffer.
		 */
		set_buffer_freed(bh);
		if (journal->j_running_transaction && buffer_jbddirty(bh))
			jh->b_next_transaction = journal->j_running_transaction;
		jbd2_journal_put_journal_head(jh);
		spin_unlock(&journal->j_list_lock);
		jbd_unlock_bh_state(bh);
		write_unlock(&journal->j_state_lock);
		return 0;
	} else {
		/* Good, the buffer belongs to the running transaction.
		 * We are writing our own transaction's data, not any
		 * previous one's, so it is safe to throw it away
		 * (remember that we expect the filesystem to have set
		 * i_size already for this truncate so recovery will not
		 * expose the disk blocks we are discarding here.) */
		J_ASSERT_JH(jh, transaction == journal->j_running_transaction);
		JBUFFER_TRACE(jh, "on running transaction");
		may_free = __dispose_buffer(jh, transaction);
	}

zap_buffer:
	jbd2_journal_put_journal_head(jh);
zap_buffer_no_jh:
	spin_unlock(&journal->j_list_lock);
	jbd_unlock_bh_state(bh);
	write_unlock(&journal->j_state_lock);
zap_buffer_unlocked:
	clear_buffer_dirty(bh);
	J_ASSERT_BH(bh, !buffer_jbddirty(bh));
	clear_buffer_mapped(bh);
	clear_buffer_req(bh);
	clear_buffer_new(bh);
	bh->b_bdev = NULL;
	return may_free;
}

/**
 * void jbd2_journal_invalidatepage()
 * @journal: journal to use for flush...
 * @page:    page to flush
 * @offset:  length of page to invalidate.
 *
 * Reap page buffers containing data after offset in page.
 *
 */
void jbd2_journal_invalidatepage(journal_t *journal,
		      struct page *page,
		      unsigned long offset)
{
	struct buffer_head *head, *bh, *next;
	unsigned int curr_off = 0;
	int may_free = 1;

	if (!PageLocked(page))
		BUG();
	if (!page_has_buffers(page))
		return;

	/* We will potentially be playing with lists other than just the
	 * data lists (especially for journaled data mode), so be
	 * cautious in our locking. */

	head = bh = page_buffers(page);
	do {
		unsigned int next_off = curr_off + bh->b_size;
		next = bh->b_this_page;

		if (offset <= curr_off) {
			/* This block is wholly outside the truncation point */
			lock_buffer(bh);
			may_free &= journal_unmap_buffer(journal, bh);
			unlock_buffer(bh);
		}
		curr_off = next_off;
		bh = next;

	} while (bh != head);

	if (!offset) {
		if (may_free && try_to_free_buffers(page))
			J_ASSERT(!page_has_buffers(page));
	}
}

/*
 * File a buffer on the given transaction list.
 */
void __jbd2_journal_file_buffer(struct journal_head *jh,
			transaction_t *transaction, int jlist)
{
	struct journal_head **list = NULL;
	int was_dirty = 0;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	assert_spin_locked(&transaction->t_journal->j_list_lock);

	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);
	J_ASSERT_JH(jh, jh->b_transaction == transaction ||
				jh->b_transaction == NULL);

	if (jh->b_transaction && jh->b_jlist == jlist)
		return;

	if (jlist == BJ_Metadata || jlist == BJ_Reserved ||
	    jlist == BJ_Shadow || jlist == BJ_Forget) {
		/*
		 * For metadata buffers, we track dirty bit in buffer_jbddirty
		 * instead of buffer_dirty. We should not see a dirty bit set
		 * here because we clear it in do_get_write_access but e.g.
		 * tune2fs can modify the sb and set the dirty bit at any time
		 * so we try to gracefully handle that.
		 */
		if (buffer_dirty(bh))
			warn_dirty_buffer(bh);
		if (test_clear_buffer_dirty(bh) ||
		    test_clear_buffer_jbddirty(bh))
			was_dirty = 1;
	}

	if (jh->b_transaction)
		__jbd2_journal_temp_unlink_buffer(jh);
	jh->b_transaction = transaction;

	switch (jlist) {
	case BJ_None:
		J_ASSERT_JH(jh, !jh->b_committed_data);
		J_ASSERT_JH(jh, !jh->b_frozen_data);
		return;
	case BJ_Metadata:
		transaction->t_nr_buffers++;
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	}

	__blist_add_buffer(list, jh);
	jh->b_jlist = jlist;

	if (was_dirty)
		set_buffer_jbddirty(bh);
}

void jbd2_journal_file_buffer(struct journal_head *jh,
				transaction_t *transaction, int jlist)
{
	jbd_lock_bh_state(jh2bh(jh));
	spin_lock(&transaction->t_journal->j_list_lock);
	__jbd2_journal_file_buffer(jh, transaction, jlist);
	spin_unlock(&transaction->t_journal->j_list_lock);
	jbd_unlock_bh_state(jh2bh(jh));
}

/*
 * Remove a buffer from its current buffer list in preparation for
 * dropping it from its current transaction entirely.  If the buffer has
 * already started to be used by a subsequent transaction, refile the
 * buffer on that transaction's metadata list.
 *
 * Called under journal->j_list_lock
 *
 * Called under jbd_lock_bh_state(jh2bh(jh))
 */
void __jbd2_journal_refile_buffer(struct journal_head *jh)
{
	int was_dirty, jlist;
	struct buffer_head *bh = jh2bh(jh);

	J_ASSERT_JH(jh, jbd_is_locked_bh_state(bh));
	if (jh->b_transaction)
		assert_spin_locked(&jh->b_transaction->t_journal->j_list_lock);

	/* If the buffer is now unused, just drop it. */
	if (jh->b_next_transaction == NULL) {
		__jbd2_journal_unfile_buffer(jh);
		return;
	}

	/*
	 * It has been modified by a later transaction: add it to the new
	 * transaction's metadata list.
	 */

	was_dirty = test_clear_buffer_jbddirty(bh);
	__jbd2_journal_temp_unlink_buffer(jh);
	jh->b_transaction = jh->b_next_transaction;
	jh->b_next_transaction = NULL;
	if (buffer_freed(bh))
		jlist = BJ_Forget;
	else if (jh->b_modified)
		jlist = BJ_Metadata;
	else
		jlist = BJ_Reserved;
	__jbd2_journal_file_buffer(jh, jh->b_transaction, jlist);
	J_ASSERT_JH(jh, jh->b_transaction->t_state == T_RUNNING);

	if (was_dirty)
		set_buffer_jbddirty(bh);
}

/*
 * For the unlocked version of this call, also make sure that any
 * hanging journal_head is cleaned up if necessary.
 *
 * __jbd2_journal_refile_buffer is usually called as part of a single locked
 * operation on a buffer_head, in which the caller is probably going to
 * be hooking the journal_head onto other lists.  In that case it is up
 * to the caller to remove the journal_head if necessary.  For the
 * unlocked jbd2_journal_refile_buffer call, the caller isn't going to be
 * doing anything else to the buffer so we need to do the cleanup
 * ourselves to avoid a jh leak.
 *
 * *** The journal_head may be freed by this call! ***
 */
void jbd2_journal_refile_buffer(journal_t *journal, struct journal_head *jh)
{
	struct buffer_head *bh = jh2bh(jh);

	jbd_lock_bh_state(bh);
	spin_lock(&journal->j_list_lock);

	__jbd2_journal_refile_buffer(jh);
	jbd_unlock_bh_state(bh);
	jbd2_journal_remove_journal_head(bh);

	spin_unlock(&journal->j_list_lock);
	__brelse(bh);
}

/*
 * File inode in the inode list of the handle's transaction
 */
int jbd2_journal_file_inode(handle_t *handle, struct jbd2_inode *jinode)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;

	if (is_handle_aborted(handle))
		return -EIO;

	jbd_debug(4, "Adding inode %lu, tid:%d\n", jinode->i_vfs_inode->i_ino,
			transaction->t_tid);

	/*
	 * First check whether inode isn't already on the transaction's
	 * lists without taking the lock. Note that this check is safe
	 * without the lock as we cannot race with somebody removing inode
	 * from the transaction. The reason is that we remove inode from the
	 * transaction only in journal_release_jbd_inode() and when we commit
	 * the transaction. We are guarded from the first case by holding
	 * a reference to the inode. We are safe against the second case
	 * because if jinode->i_transaction == transaction, commit code
	 * cannot touch the transaction because we hold reference to it,
	 * and if jinode->i_next_transaction == transaction, commit code
	 * will only file the inode where we want it.
	 */
	if (jinode->i_transaction == transaction ||
	    jinode->i_next_transaction == transaction)
		return 0;

	spin_lock(&journal->j_list_lock);

	if (jinode->i_transaction == transaction ||
	    jinode->i_next_transaction == transaction)
		goto done;

	/* On some different transaction's list - should be
	 * the committing one */
	if (jinode->i_transaction) {
		J_ASSERT(jinode->i_next_transaction == NULL);
		J_ASSERT(jinode->i_transaction ==
					journal->j_committing_transaction);
		jinode->i_next_transaction = transaction;
		goto done;
	}
	/* Not on any transaction list... */
	J_ASSERT(!jinode->i_next_transaction);
	jinode->i_transaction = transaction;
	list_add(&jinode->i_list, &transaction->t_inode_list);
done:
	spin_unlock(&journal->j_list_lock);

	return 0;
}

/*
 * File truncate and transaction commit interact with each other in a
 * non-trivial way.  If a transaction writing data block A is
 * committing, we cannot discard the data by truncate until we have
 * written them.  Otherwise if we crashed after the transaction with
 * write has committed but before the transaction with truncate has
 * committed, we could see stale data in block A.  This function is a
 * helper to solve this problem.  It starts writeout of the truncated
 * part in case it is in the committing transaction.
 *
 * Filesystem code must call this function when inode is journaled in
 * ordered mode before truncation happens and after the inode has been
 * placed on orphan list with the new inode size. The second condition
 * avoids the race that someone writes new data and we start
 * committing the transaction after this function has been called but
 * before a transaction for truncate is started (and furthermore it
 * allows us to optimize the case where the addition to orphan list
 * happens in the same transaction as write --- we don't have to write
 * any data in such case).
 */
int jbd2_journal_begin_ordered_truncate(journal_t *journal,
					struct jbd2_inode *jinode,
					loff_t new_size)
{
	transaction_t *inode_trans, *commit_trans;
	int ret = 0;

	/* This is a quick check to avoid locking if not necessary */
	if (!jinode->i_transaction)
		goto out;
	/* Locks are here just to force reading of recent values, it is
	 * enough that the transaction was not committing before we started
	 * a transaction adding the inode to orphan list */
	read_lock(&journal->j_state_lock);
	commit_trans = journal->j_committing_transaction;
	read_unlock(&journal->j_state_lock);
	spin_lock(&journal->j_list_lock);
	inode_trans = jinode->i_transaction;
	spin_unlock(&journal->j_list_lock);
	if (inode_trans == commit_trans) {
		ret = filemap_fdatawrite_range(jinode->i_vfs_inode->i_mapping,
			new_size, LLONG_MAX);
		if (ret)
			jbd2_journal_abort(journal, ret);
	}
out:
	return ret;
}
