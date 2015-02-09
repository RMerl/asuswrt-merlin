/*
 * linux/fs/jbd2/recovery.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1999-2000 Red Hat Software --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Journal recovery routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 */

#ifndef __KERNEL__
#include "jfs_user.h"
#else
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/errno.h>
#include <linux/crc32.h>
#endif

/*
 * Maintain information about the progress of the recovery job, so that
 * the different passes can carry information between them.
 */
struct recovery_info
{
	tid_t		start_transaction;
	tid_t		end_transaction;

	int		nr_replays;
	int		nr_revokes;
	int		nr_revoke_hits;
};

enum passtype {PASS_SCAN, PASS_REVOKE, PASS_REPLAY};
static int do_one_pass(journal_t *journal,
				struct recovery_info *info, enum passtype pass);
static int scan_revoke_records(journal_t *, struct buffer_head *,
				tid_t, struct recovery_info *);

#ifdef __KERNEL__

/* Release readahead buffers after use */
static void journal_brelse_array(struct buffer_head *b[], int n)
{
	while (--n >= 0)
		brelse (b[n]);
}


/*
 * When reading from the journal, we are going through the block device
 * layer directly and so there is no readahead being done for us.  We
 * need to implement any readahead ourselves if we want it to happen at
 * all.  Recovery is basically one long sequential read, so make sure we
 * do the IO in reasonably large chunks.
 *
 * This is not so critical that we need to be enormously clever about
 * the readahead size, though.  128K is a purely arbitrary, good-enough
 * fixed value.
 */

#define MAXBUF 8
static int do_readahead(journal_t *journal, unsigned int start)
{
	int err;
	unsigned int max, nbufs, next;
	unsigned long long blocknr;
	struct buffer_head *bh;

	struct buffer_head * bufs[MAXBUF];

	/* Do up to 128K of readahead */
	max = start + (128 * 1024 / journal->j_blocksize);
	if (max > journal->j_maxlen)
		max = journal->j_maxlen;

	/* Do the readahead itself.  We'll submit MAXBUF buffer_heads at
	 * a time to the block device IO layer. */

	nbufs = 0;

	for (next = start; next < max; next++) {
		err = jbd2_journal_bmap(journal, next, &blocknr);

		if (err) {
			printk (KERN_ERR "JBD: bad block at offset %u\n",
				next);
			goto failed;
		}

		bh = __getblk(journal->j_dev, blocknr, journal->j_blocksize);
		if (!bh) {
			err = -ENOMEM;
			goto failed;
		}

		if (!buffer_uptodate(bh) && !buffer_locked(bh)) {
			bufs[nbufs++] = bh;
			if (nbufs == MAXBUF) {
				ll_rw_block(READ, nbufs, bufs);
				journal_brelse_array(bufs, nbufs);
				nbufs = 0;
			}
		} else
			brelse(bh);
	}

	if (nbufs)
		ll_rw_block(READ, nbufs, bufs);
	err = 0;

failed:
	if (nbufs)
		journal_brelse_array(bufs, nbufs);
	return err;
}

#endif /* __KERNEL__ */


/*
 * Read a block from the journal
 */

static int jread(struct buffer_head **bhp, journal_t *journal,
		 unsigned int offset)
{
	int err;
	unsigned long long blocknr;
	struct buffer_head *bh;

	*bhp = NULL;

	if (offset >= journal->j_maxlen) {
		printk(KERN_ERR "JBD: corrupted journal superblock\n");
		return -EIO;
	}

	err = jbd2_journal_bmap(journal, offset, &blocknr);

	if (err) {
		printk (KERN_ERR "JBD: bad block at offset %u\n",
			offset);
		return err;
	}

	bh = __getblk(journal->j_dev, blocknr, journal->j_blocksize);
	if (!bh)
		return -ENOMEM;

	if (!buffer_uptodate(bh)) {
		/* If this is a brand new buffer, start readahead.
                   Otherwise, we assume we are already reading it.  */
		if (!buffer_req(bh))
			do_readahead(journal, offset);
		wait_on_buffer(bh);
	}

	if (!buffer_uptodate(bh)) {
		printk (KERN_ERR "JBD: Failed to read block at offset %u\n",
			offset);
		brelse(bh);
		return -EIO;
	}

	*bhp = bh;
	return 0;
}


/*
 * Count the number of in-use tags in a journal descriptor block.
 */

static int count_tags(journal_t *journal, struct buffer_head *bh)
{
	char *			tagp;
	journal_block_tag_t *	tag;
	int			nr = 0, size = journal->j_blocksize;
	int			tag_bytes = journal_tag_bytes(journal);

	tagp = &bh->b_data[sizeof(journal_header_t)];

	while ((tagp - bh->b_data + tag_bytes) <= size) {
		tag = (journal_block_tag_t *) tagp;

		nr++;
		tagp += tag_bytes;
		if (!(tag->t_flags & cpu_to_be32(JBD2_FLAG_SAME_UUID)))
			tagp += 16;

		if (tag->t_flags & cpu_to_be32(JBD2_FLAG_LAST_TAG))
			break;
	}

	return nr;
}


/* Make sure we wrap around the log correctly! */
#define wrap(journal, var)						\
do {									\
	if (var >= (journal)->j_last)					\
		var -= ((journal)->j_last - (journal)->j_first);	\
} while (0)

/**
 * jbd2_journal_recover - recovers a on-disk journal
 * @journal: the journal to recover
 *
 * The primary function for recovering the log contents when mounting a
 * journaled device.
 *
 * Recovery is done in three passes.  In the first pass, we look for the
 * end of the log.  In the second, we assemble the list of revoke
 * blocks.  In the third and final pass, we replay any un-revoked blocks
 * in the log.
 */
int jbd2_journal_recover(journal_t *journal)
{
	int			err, err2;
	journal_superblock_t *	sb;

	struct recovery_info	info;

	memset(&info, 0, sizeof(info));
	sb = journal->j_superblock;

	/*
	 * The journal superblock's s_start field (the current log head)
	 * is always zero if, and only if, the journal was cleanly
	 * unmounted.
	 */

	if (!sb->s_start) {
		jbd_debug(1, "No recovery required, last transaction %d\n",
			  be32_to_cpu(sb->s_sequence));
		journal->j_transaction_sequence = be32_to_cpu(sb->s_sequence) + 1;
		return 0;
	}

	err = do_one_pass(journal, &info, PASS_SCAN);
	if (!err)
		err = do_one_pass(journal, &info, PASS_REVOKE);
	if (!err)
		err = do_one_pass(journal, &info, PASS_REPLAY);

	jbd_debug(1, "JBD: recovery, exit status %d, "
		  "recovered transactions %u to %u\n",
		  err, info.start_transaction, info.end_transaction);
	jbd_debug(1, "JBD: Replayed %d and revoked %d/%d blocks\n",
		  info.nr_replays, info.nr_revoke_hits, info.nr_revokes);

	/* Restart the log at the next transaction ID, thus invalidating
	 * any existing commit records in the log. */
	journal->j_transaction_sequence = ++info.end_transaction;

	jbd2_journal_clear_revoke(journal);
	err2 = sync_blockdev(journal->j_fs_dev);
	if (!err)
		err = err2;

	return err;
}

/**
 * jbd2_journal_skip_recovery - Start journal and wipe exiting records
 * @journal: journal to startup
 *
 * Locate any valid recovery information from the journal and set up the
 * journal structures in memory to ignore it (presumably because the
 * caller has evidence that it is out of date).
 * This function does'nt appear to be exorted..
 *
 * We perform one pass over the journal to allow us to tell the user how
 * much recovery information is being erased, and to let us initialise
 * the journal transaction sequence numbers to the next unused ID.
 */
int jbd2_journal_skip_recovery(journal_t *journal)
{
	int			err;

	struct recovery_info	info;

	memset (&info, 0, sizeof(info));

	err = do_one_pass(journal, &info, PASS_SCAN);

	if (err) {
		printk(KERN_ERR "JBD: error %d scanning journal\n", err);
		++journal->j_transaction_sequence;
	} else {
#ifdef CONFIG_JBD2_DEBUG
		int dropped = info.end_transaction - 
			be32_to_cpu(journal->j_superblock->s_sequence);
#endif
		jbd_debug(1,
			  "JBD: ignoring %d transaction%s from the journal.\n",
			  dropped, (dropped == 1) ? "" : "s");
		journal->j_transaction_sequence = ++info.end_transaction;
	}

	journal->j_tail = 0;
	return err;
}

static inline unsigned long long read_tag_block(int tag_bytes, journal_block_tag_t *tag)
{
	unsigned long long block = be32_to_cpu(tag->t_blocknr);
	if (tag_bytes > JBD2_TAG_SIZE32)
		block |= (u64)be32_to_cpu(tag->t_blocknr_high) << 32;
	return block;
}

/*
 * calc_chksums calculates the checksums for the blocks described in the
 * descriptor block.
 */
static int calc_chksums(journal_t *journal, struct buffer_head *bh,
			unsigned long *next_log_block, __u32 *crc32_sum)
{
	int i, num_blks, err;
	unsigned long io_block;
	struct buffer_head *obh;

	num_blks = count_tags(journal, bh);
	/* Calculate checksum of the descriptor block. */
	*crc32_sum = crc32_be(*crc32_sum, (void *)bh->b_data, bh->b_size);

	for (i = 0; i < num_blks; i++) {
		io_block = (*next_log_block)++;
		wrap(journal, *next_log_block);
		err = jread(&obh, journal, io_block);
		if (err) {
			printk(KERN_ERR "JBD: IO error %d recovering block "
				"%lu in log\n", err, io_block);
			return 1;
		} else {
			*crc32_sum = crc32_be(*crc32_sum, (void *)obh->b_data,
				     obh->b_size);
		}
		put_bh(obh);
	}
	return 0;
}

static int do_one_pass(journal_t *journal,
			struct recovery_info *info, enum passtype pass)
{
	unsigned int		first_commit_ID, next_commit_ID;
	unsigned long		next_log_block;
	int			err, success = 0;
	journal_superblock_t *	sb;
	journal_header_t *	tmp;
	struct buffer_head *	bh;
	unsigned int		sequence;
	int			blocktype;
	int			tag_bytes = journal_tag_bytes(journal);
	__u32			crc32_sum = ~0; /* Transactional Checksums */

	/*
	 * First thing is to establish what we expect to find in the log
	 * (in terms of transaction IDs), and where (in terms of log
	 * block offsets): query the superblock.
	 */

	sb = journal->j_superblock;
	next_commit_ID = be32_to_cpu(sb->s_sequence);
	next_log_block = be32_to_cpu(sb->s_start);

	first_commit_ID = next_commit_ID;
	if (pass == PASS_SCAN)
		info->start_transaction = first_commit_ID;

	jbd_debug(1, "Starting recovery pass %d\n", pass);

	/*
	 * Now we walk through the log, transaction by transaction,
	 * making sure that each transaction has a commit block in the
	 * expected place.  Each complete transaction gets replayed back
	 * into the main filesystem.
	 */

	while (1) {
		int			flags;
		char *			tagp;
		journal_block_tag_t *	tag;
		struct buffer_head *	obh;
		struct buffer_head *	nbh;

		cond_resched();

		/* If we already know where to stop the log traversal,
		 * check right now that we haven't gone past the end of
		 * the log. */

		if (pass != PASS_SCAN)
			if (tid_geq(next_commit_ID, info->end_transaction))
				break;

		jbd_debug(2, "Scanning for sequence ID %u at %lu/%lu\n",
			  next_commit_ID, next_log_block, journal->j_last);

		/* Skip over each chunk of the transaction looking
		 * either the next descriptor block or the final commit
		 * record. */

		jbd_debug(3, "JBD: checking block %ld\n", next_log_block);
		err = jread(&bh, journal, next_log_block);
		if (err)
			goto failed;

		next_log_block++;
		wrap(journal, next_log_block);

		/* What kind of buffer is it?
		 *
		 * If it is a descriptor block, check that it has the
		 * expected sequence number.  Otherwise, we're all done
		 * here. */

		tmp = (journal_header_t *)bh->b_data;

		if (tmp->h_magic != cpu_to_be32(JBD2_MAGIC_NUMBER)) {
			brelse(bh);
			break;
		}

		blocktype = be32_to_cpu(tmp->h_blocktype);
		sequence = be32_to_cpu(tmp->h_sequence);
		jbd_debug(3, "Found magic %d, sequence %d\n",
			  blocktype, sequence);

		if (sequence != next_commit_ID) {
			brelse(bh);
			break;
		}

		/* OK, we have a valid descriptor block which matches
		 * all of the sequence number checks.  What are we going
		 * to do with it?  That depends on the pass... */

		switch(blocktype) {
		case JBD2_DESCRIPTOR_BLOCK:
			/* If it is a valid descriptor block, replay it
			 * in pass REPLAY; if journal_checksums enabled, then
			 * calculate checksums in PASS_SCAN, otherwise,
			 * just skip over the blocks it describes. */
			if (pass != PASS_REPLAY) {
				if (pass == PASS_SCAN &&
				    JBD2_HAS_COMPAT_FEATURE(journal,
					    JBD2_FEATURE_COMPAT_CHECKSUM) &&
				    !info->end_transaction) {
					if (calc_chksums(journal, bh,
							&next_log_block,
							&crc32_sum)) {
						put_bh(bh);
						break;
					}
					put_bh(bh);
					continue;
				}
				next_log_block += count_tags(journal, bh);
				wrap(journal, next_log_block);
				put_bh(bh);
				continue;
			}

			/* A descriptor block: we can now write all of
			 * the data blocks.  Yay, useful work is finally
			 * getting done here! */

			tagp = &bh->b_data[sizeof(journal_header_t)];
			while ((tagp - bh->b_data + tag_bytes)
			       <= journal->j_blocksize) {
				unsigned long io_block;

				tag = (journal_block_tag_t *) tagp;
				flags = be32_to_cpu(tag->t_flags);

				io_block = next_log_block++;
				wrap(journal, next_log_block);
				err = jread(&obh, journal, io_block);
				if (err) {
					/* Recover what we can, but
					 * report failure at the end. */
					success = err;
					printk (KERN_ERR
						"JBD: IO error %d recovering "
						"block %ld in log\n",
						err, io_block);
				} else {
					unsigned long long blocknr;

					J_ASSERT(obh != NULL);
					blocknr = read_tag_block(tag_bytes,
								 tag);

					/* If the block has been
					 * revoked, then we're all done
					 * here. */
					if (jbd2_journal_test_revoke
					    (journal, blocknr,
					     next_commit_ID)) {
						brelse(obh);
						++info->nr_revoke_hits;
						goto skip_write;
					}

					/* Find a buffer for the new
					 * data being restored */
					nbh = __getblk(journal->j_fs_dev,
							blocknr,
							journal->j_blocksize);
					if (nbh == NULL) {
						printk(KERN_ERR
						       "JBD: Out of memory "
						       "during recovery.\n");
						err = -ENOMEM;
						brelse(bh);
						brelse(obh);
						goto failed;
					}

					lock_buffer(nbh);
					memcpy(nbh->b_data, obh->b_data,
							journal->j_blocksize);
					if (flags & JBD2_FLAG_ESCAPE) {
						*((__be32 *)nbh->b_data) =
						cpu_to_be32(JBD2_MAGIC_NUMBER);
					}

					BUFFER_TRACE(nbh, "marking dirty");
					set_buffer_uptodate(nbh);
					mark_buffer_dirty(nbh);
					BUFFER_TRACE(nbh, "marking uptodate");
					++info->nr_replays;
					/* ll_rw_block(WRITE, 1, &nbh); */
					unlock_buffer(nbh);
					brelse(obh);
					brelse(nbh);
				}

			skip_write:
				tagp += tag_bytes;
				if (!(flags & JBD2_FLAG_SAME_UUID))
					tagp += 16;

				if (flags & JBD2_FLAG_LAST_TAG)
					break;
			}

			brelse(bh);
			continue;

		case JBD2_COMMIT_BLOCK:
			/*     How to differentiate between interrupted commit
			 *               and journal corruption ?
			 *
			 * {nth transaction}
			 *        Checksum Verification Failed
			 *			 |
			 *		 ____________________
			 *		|		     |
			 * 	async_commit             sync_commit
			 *     		|                    |
			 *		| GO TO NEXT    "Journal Corruption"
			 *		| TRANSACTION
			 *		|
			 * {(n+1)th transanction}
			 *		|
			 * 	 _______|______________
			 * 	|	 	      |
			 * Commit block found	Commit block not found
			 *      |		      |
			 * "Journal Corruption"       |
			 *		 _____________|_________
			 *     		|	           	|
			 *	nth trans corrupt	OR   nth trans
			 *	and (n+1)th interrupted     interrupted
			 *	before commit block
			 *      could reach the disk.
			 *	(Cannot find the difference in above
			 *	 mentioned conditions. Hence assume
			 *	 "Interrupted Commit".)
			 */

			/* Found an expected commit block: if checksums
			 * are present verify them in PASS_SCAN; else not
			 * much to do other than move on to the next sequence
			 * number. */
			if (pass == PASS_SCAN &&
			    JBD2_HAS_COMPAT_FEATURE(journal,
				    JBD2_FEATURE_COMPAT_CHECKSUM)) {
				int chksum_err, chksum_seen;
				struct commit_header *cbh =
					(struct commit_header *)bh->b_data;
				unsigned found_chksum =
					be32_to_cpu(cbh->h_chksum[0]);

				chksum_err = chksum_seen = 0;

				if (info->end_transaction) {
					journal->j_failed_commit =
						info->end_transaction;
					brelse(bh);
					break;
				}

				if (crc32_sum == found_chksum &&
				    cbh->h_chksum_type == JBD2_CRC32_CHKSUM &&
				    cbh->h_chksum_size ==
						JBD2_CRC32_CHKSUM_SIZE)
				       chksum_seen = 1;
				else if (!(cbh->h_chksum_type == 0 &&
					     cbh->h_chksum_size == 0 &&
					     found_chksum == 0 &&
					     !chksum_seen))
				/*
				 * If fs is mounted using an old kernel and then
				 * kernel with journal_chksum is used then we
				 * get a situation where the journal flag has
				 * checksum flag set but checksums are not
				 * present i.e chksum = 0, in the individual
				 * commit blocks.
				 * Hence to avoid checksum failures, in this
				 * situation, this extra check is added.
				 */
						chksum_err = 1;

				if (chksum_err) {
					info->end_transaction = next_commit_ID;

					if (!JBD2_HAS_INCOMPAT_FEATURE(journal,
					   JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT)){
						journal->j_failed_commit =
							next_commit_ID;
						brelse(bh);
						break;
					}
				}
				crc32_sum = ~0;
			}
			brelse(bh);
			next_commit_ID++;
			continue;

		case JBD2_REVOKE_BLOCK:
			/* If we aren't in the REVOKE pass, then we can
			 * just skip over this block. */
			if (pass != PASS_REVOKE) {
				brelse(bh);
				continue;
			}

			err = scan_revoke_records(journal, bh,
						  next_commit_ID, info);
			brelse(bh);
			if (err)
				goto failed;
			continue;

		default:
			jbd_debug(3, "Unrecognised magic %d, end of scan.\n",
				  blocktype);
			brelse(bh);
			goto done;
		}
	}

 done:
	/*
	 * We broke out of the log scan loop: either we came to the
	 * known end of the log or we found an unexpected block in the
	 * log.  If the latter happened, then we know that the "current"
	 * transaction marks the end of the valid log.
	 */

	if (pass == PASS_SCAN) {
		if (!info->end_transaction)
			info->end_transaction = next_commit_ID;
	} else {
		/* It's really bad news if different passes end up at
		 * different places (but possible due to IO errors). */
		if (info->end_transaction != next_commit_ID) {
			printk (KERN_ERR "JBD: recovery pass %d ended at "
				"transaction %u, expected %u\n",
				pass, next_commit_ID, info->end_transaction);
			if (!success)
				success = -EIO;
		}
	}

	return success;

 failed:
	return err;
}


/* Scan a revoke record, marking all blocks mentioned as revoked. */

static int scan_revoke_records(journal_t *journal, struct buffer_head *bh,
			       tid_t sequence, struct recovery_info *info)
{
	jbd2_journal_revoke_header_t *header;
	int offset, max;
	int record_len = 4;

	header = (jbd2_journal_revoke_header_t *) bh->b_data;
	offset = sizeof(jbd2_journal_revoke_header_t);
	max = be32_to_cpu(header->r_count);

	if (JBD2_HAS_INCOMPAT_FEATURE(journal, JBD2_FEATURE_INCOMPAT_64BIT))
		record_len = 8;

	while (offset + record_len <= max) {
		unsigned long long blocknr;
		int err;

		if (record_len == 4)
			blocknr = be32_to_cpu(* ((__be32 *) (bh->b_data+offset)));
		else
			blocknr = be64_to_cpu(* ((__be64 *) (bh->b_data+offset)));
		offset += record_len;
		err = jbd2_journal_set_revoke(journal, blocknr, sequence);
		if (err)
			return err;
		++info->nr_revokes;
	}
	return 0;
}
