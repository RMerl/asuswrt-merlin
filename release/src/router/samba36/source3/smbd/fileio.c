/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   read/write to a files_struct
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2000-2002. - write cache.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "printing.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "smbprofile.h"

static bool setup_write_cache(files_struct *, SMB_OFF_T);

/****************************************************************************
 Read from write cache if we can.
****************************************************************************/

static bool read_from_write_cache(files_struct *fsp,char *data,SMB_OFF_T pos,size_t n)
{
	write_cache *wcp = fsp->wcp;

	if(!wcp) {
		return False;
	}

	if( n > wcp->data_size || pos < wcp->offset || pos + n > wcp->offset + wcp->data_size) {
		return False;
	}

	memcpy(data, wcp->data + (pos - wcp->offset), n);

	DO_PROFILE_INC(writecache_read_hits);

	return True;
}

/****************************************************************************
 Read from a file.
****************************************************************************/

ssize_t read_file(files_struct *fsp,char *data,SMB_OFF_T pos,size_t n)
{
	ssize_t ret=0,readret;

	/* you can't read from print files */
	if (fsp->print_file) {
		errno = EBADF;
		return -1;
	}

	/*
	 * Serve from write cache if we can.
	 */

	if(read_from_write_cache(fsp, data, pos, n)) {
		fsp->fh->pos = pos + n;
		fsp->fh->position_information = fsp->fh->pos;
		return n;
	}

	flush_write_cache(fsp, READ_FLUSH);

	fsp->fh->pos = pos;

	if (n > 0) {
#ifdef DMF_FIX
		int numretries = 3;
tryagain:
		readret = SMB_VFS_PREAD(fsp,data,n,pos);

		if (readret == -1) {
			if ((errno == EAGAIN) && numretries) {
				DEBUG(3,("read_file EAGAIN retry in 10 seconds\n"));
				(void)sleep(10);
				--numretries;
				goto tryagain;
			}
			return -1;
		}
#else /* NO DMF fix. */
		readret = SMB_VFS_PREAD(fsp,data,n,pos);

		if (readret == -1) {
			return -1;
		}
#endif
		if (readret > 0) {
			ret += readret;
		}
	}

	DEBUG(10,("read_file (%s): pos = %.0f, size = %lu, returned %lu\n",
		  fsp_str_dbg(fsp), (double)pos, (unsigned long)n, (long)ret));

	fsp->fh->pos += ret;
	fsp->fh->position_information = fsp->fh->pos;

	return(ret);
}

/****************************************************************************
 *Really* write to a file.
****************************************************************************/

static ssize_t real_write_file(struct smb_request *req,
				files_struct *fsp,
				const char *data,
				SMB_OFF_T pos,
				size_t n)
{
	ssize_t ret;

        if (pos == -1) {
                ret = vfs_write_data(req, fsp, data, n);
        } else {
		fsp->fh->pos = pos;
		if (pos && lp_strict_allocate(SNUM(fsp->conn) &&
				!fsp->is_sparse)) {
			if (vfs_fill_sparse(fsp, pos) == -1) {
				return -1;
			}
		}
                ret = vfs_pwrite_data(req, fsp, data, n, pos);
	}

	DEBUG(10,("real_write_file (%s): pos = %.0f, size = %lu, returned %ld\n",
		  fsp_str_dbg(fsp), (double)pos, (unsigned long)n, (long)ret));

	if (ret != -1) {
		fsp->fh->pos += ret;

/* Yes - this is correct - writes don't update this. JRA. */
/* Found by Samba4 tests. */
#if 0
		fsp->position_information = fsp->pos;
#endif
	}

	return ret;
}

/****************************************************************************
 File size cache change.
 Updates size on disk but doesn't flush the cache.
****************************************************************************/

static int wcp_file_size_change(files_struct *fsp)
{
	int ret;
	write_cache *wcp = fsp->wcp;

	wcp->file_size = wcp->offset + wcp->data_size;
	ret = SMB_VFS_FTRUNCATE(fsp, wcp->file_size);
	if (ret == -1) {
		DEBUG(0,("wcp_file_size_change (%s): ftruncate of size %.0f "
			 "error %s\n", fsp_str_dbg(fsp),
			 (double)wcp->file_size, strerror(errno)));
	}
	return ret;
}

void update_write_time_handler(struct event_context *ctx,
				      struct timed_event *te,
				      struct timeval now,
				      void *private_data)
{
	files_struct *fsp = (files_struct *)private_data;

	DEBUG(5, ("Update write time on %s\n", fsp_str_dbg(fsp)));

	/* change the write time in the open file db. */
	(void)set_write_time(fsp->file_id, timespec_current());

	/* And notify. */
        notify_fname(fsp->conn, NOTIFY_ACTION_MODIFIED,
                     FILE_NOTIFY_CHANGE_LAST_WRITE, fsp->fsp_name->base_name);

	/* Remove the timed event handler. */
	TALLOC_FREE(fsp->update_write_time_event);
}

/*********************************************************
 Schedule a write time update for WRITE_TIME_UPDATE_USEC_DELAY
 in the future.
*********************************************************/

void trigger_write_time_update(struct files_struct *fsp)
{
	int delay;

	if (fsp->posix_open) {
		/* Don't use delayed writes on POSIX files. */
		return;
	}

	if (fsp->write_time_forced) {
		/* No point - "sticky" write times
		 * in effect.
		 */
		return;
	}

	/* We need to remember someone did a write
	 * and update to current time on close. */

	fsp->update_write_time_on_close = true;

	if (fsp->update_write_time_triggered) {
		/*
		 * We only update the write time after 2 seconds
		 * on the first normal write. After that
		 * no other writes affect this until close.
		 */
		return;
	}
	fsp->update_write_time_triggered = true;

	delay = lp_parm_int(SNUM(fsp->conn),
			    "smbd", "writetimeupdatedelay",
			    WRITE_TIME_UPDATE_USEC_DELAY);

	DEBUG(5, ("Update write time %d usec later on %s\n",
		  delay, fsp_str_dbg(fsp)));

	/* trigger the update 2 seconds later */
	fsp->update_write_time_event =
		event_add_timed(smbd_event_context(), NULL,
				timeval_current_ofs(0, delay),
				update_write_time_handler, fsp);
}

void trigger_write_time_update_immediate(struct files_struct *fsp)
{
	struct smb_file_time ft;

	if (fsp->posix_open) {
		/* Don't use delayed writes on POSIX files. */
		return;
	}

        if (fsp->write_time_forced) {
		/*
		 * No point - "sticky" write times
		 * in effect.
		 */
                return;
        }

	TALLOC_FREE(fsp->update_write_time_event);
	DEBUG(5, ("Update write time immediate on %s\n",
		  fsp_str_dbg(fsp)));

	/* After an immediate update, reset the trigger. */
	fsp->update_write_time_triggered = true;
        fsp->update_write_time_on_close = false;

	ZERO_STRUCT(ft);
	ft.mtime = timespec_current();

	/* Update the time in the open file db. */
	(void)set_write_time(fsp->file_id, ft.mtime);

	/* Now set on disk - takes care of notify. */
	(void)smb_set_file_time(fsp->conn, fsp, fsp->fsp_name, &ft, false);
}

/****************************************************************************
 Write to a file.
****************************************************************************/

ssize_t write_file(struct smb_request *req,
			files_struct *fsp,
			const char *data,
			SMB_OFF_T pos,
			size_t n)
{
	write_cache *wcp = fsp->wcp;
	ssize_t total_written = 0;
	int write_path = -1;

	if (fsp->print_file) {
		uint32_t t;
		int ret;

#ifndef PRINTER_SUPPORT
		return -1;
#endif

		ret = print_spool_write(fsp, data, n, pos, &t);
		if (ret) {
			errno = ret;
			return -1;
		}
		return t;
	}

	if (!fsp->can_write) {
		errno = EPERM;
		return -1;
	}

	if (!fsp->modified) {
		fsp->modified = True;

		if (SMB_VFS_FSTAT(fsp, &fsp->fsp_name->st) == 0) {
			trigger_write_time_update(fsp);
			if (!fsp->posix_open &&
					(lp_store_dos_attributes(SNUM(fsp->conn)) ||
					MAP_ARCHIVE(fsp->conn))) {
				int dosmode = dos_mode(fsp->conn, fsp->fsp_name);
				if (!IS_DOS_ARCHIVE(dosmode)) {
					file_set_dosmode(fsp->conn, fsp->fsp_name,
						 dosmode | FILE_ATTRIBUTE_ARCHIVE, NULL, false);
				}
			}

			/*
			 * If this is the first write and we have an exclusive oplock then setup
			 * the write cache.
			 */

			if (EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type) && !wcp) {
				setup_write_cache(fsp,
						 fsp->fsp_name->st.st_ex_size);
				wcp = fsp->wcp;
			}
		}
	}

#ifdef WITH_PROFILE
	DO_PROFILE_INC(writecache_total_writes);
	if (!fsp->oplock_type) {
		DO_PROFILE_INC(writecache_non_oplock_writes);
	}
#endif

	/*
	 * If this file is level II oplocked then we need
	 * to grab the shared memory lock and inform all
	 * other files with a level II lock that they need
	 * to flush their read caches. We keep the lock over
	 * the shared memory area whilst doing this.
	 */

	/* This should actually be improved to span the write. */
	contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_WRITE);
	contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_WRITE);

#ifdef WITH_PROFILE
	if (profile_p && profile_p->writecache_total_writes % 500 == 0) {
		DEBUG(3,("WRITECACHE: initwrites=%u abutted=%u total=%u \
nonop=%u allocated=%u active=%u direct=%u perfect=%u readhits=%u\n",
			profile_p->writecache_init_writes,
			profile_p->writecache_abutted_writes,
			profile_p->writecache_total_writes,
			profile_p->writecache_non_oplock_writes,
			profile_p->writecache_allocated_write_caches,
			profile_p->writecache_num_write_caches,
			profile_p->writecache_direct_writes,
			profile_p->writecache_num_perfect_writes,
			profile_p->writecache_read_hits ));

		DEBUG(3,("WRITECACHE: Flushes SEEK=%d, READ=%d, WRITE=%d, READRAW=%d, OPLOCK=%d, CLOSE=%d, SYNC=%d\n",
			profile_p->writecache_flushed_writes[SEEK_FLUSH],
			profile_p->writecache_flushed_writes[READ_FLUSH],
			profile_p->writecache_flushed_writes[WRITE_FLUSH],
			profile_p->writecache_flushed_writes[READRAW_FLUSH],
			profile_p->writecache_flushed_writes[OPLOCK_RELEASE_FLUSH],
			profile_p->writecache_flushed_writes[CLOSE_FLUSH],
			profile_p->writecache_flushed_writes[SYNC_FLUSH] ));
	}
#endif

	if (wcp && req->unread_bytes) {
		/* If we're using receivefile don't
		 * deal with a write cache.
		 */
		flush_write_cache(fsp, WRITE_FLUSH);
		delete_write_cache(fsp);
		wcp = NULL;
	}

	if(!wcp) {
		DO_PROFILE_INC(writecache_direct_writes);
		total_written = real_write_file(req, fsp, data, pos, n);
		return total_written;
	}

	DEBUG(9,("write_file (%s)(fd=%d pos=%.0f size=%u) wcp->offset=%.0f "
		 "wcp->data_size=%u\n", fsp_str_dbg(fsp), fsp->fh->fd,
		 (double)pos, (unsigned int)n, (double)wcp->offset,
		 (unsigned int)wcp->data_size));

	fsp->fh->pos = pos + n;

	if ((n == 1) && (data[0] == '\0') && (pos > wcp->file_size)) {
		int ret;

		/*
		 * This is a 1-byte write of a 0 beyond the EOF and
		 * thus implicitly also beyond the current active
		 * write cache, the typical file-extending (and
		 * allocating, but we're using the write cache here)
		 * write done by Windows. We just have to ftruncate
		 * the file and rely on posix semantics to return
		 * zeros for non-written file data that is within the
		 * file length.
		 *
		 * We can not use wcp_file_size_change here because we
		 * might have an existing write cache, and
		 * wcp_file_size_change assumes a change to just the
		 * end of the current write cache.
		 */

		wcp->file_size = pos + 1;
		ret = SMB_VFS_FTRUNCATE(fsp, wcp->file_size);
		if (ret == -1) {
			DEBUG(0,("wcp_file_size_change (%s): ftruncate of size %.0f"
				 "error %s\n", fsp_str_dbg(fsp),
				 (double)wcp->file_size, strerror(errno)));
			return -1;
		}
		return 1;
	}


	/*
	 * If we have active cache and it isn't contiguous then we flush.
	 * NOTE: There is a small problem with running out of disk ....
	 */

	if (wcp->data_size) {
		bool cache_flush_needed = False;

		if ((pos >= wcp->offset) && (pos <= wcp->offset + wcp->data_size)) {
      
			/* ASCII art.... JRA.

      +--------------+-----
      | Cached data  | Rest of allocated cache buffer....
      +--------------+-----

            +-------------------+
            | Data to write     |
            +-------------------+

	    		*/

			/*
			 * Start of write overlaps or abutts the existing data.
			 */

			size_t data_used = MIN((wcp->alloc_size - (pos - wcp->offset)), n);

			memcpy(wcp->data + (pos - wcp->offset), data, data_used);

			/*
			 * Update the current buffer size with the new data.
			 */

			if(pos + data_used > wcp->offset + wcp->data_size) {
				wcp->data_size = pos + data_used - wcp->offset;
			}

			/*
			 * Update the file size if changed.
			 */

			if (wcp->offset + wcp->data_size > wcp->file_size) {
				if (wcp_file_size_change(fsp) == -1) {
					return -1;
				}
			}

			/*
			 * If we used all the data then
			 * return here.
			 */

			if(n == data_used) {
				return n;
			} else {
				cache_flush_needed = True;
			}
			/*
			 * Move the start of data forward by the amount used,
			 * cut down the amount left by the same amount.
			 */

			data += data_used;
			pos += data_used;
			n -= data_used;

			DO_PROFILE_INC(writecache_abutted_writes);
			total_written = data_used;

			write_path = 1;

		} else if ((pos < wcp->offset) && (pos + n > wcp->offset) && 
					(pos + n <= wcp->offset + wcp->alloc_size)) {

			/* ASCII art.... JRA.

                        +---------------+
                        | Cache buffer  |
                        +---------------+

            +-------------------+
            | Data to write     |
            +-------------------+

	    		*/

			/*
			 * End of write overlaps the existing data.
			 */

			size_t data_used = pos + n - wcp->offset;

			memcpy(wcp->data, data + n - data_used, data_used);

			/*
			 * Update the current buffer size with the new data.
			 */

			if(pos + n > wcp->offset + wcp->data_size) {
				wcp->data_size = pos + n - wcp->offset;
			}

			/*
			 * Update the file size if changed.
			 */

			if (wcp->offset + wcp->data_size > wcp->file_size) {
				if (wcp_file_size_change(fsp) == -1) {
					return -1;
				}
			}

			/*
			 * We don't need to move the start of data, but we
			 * cut down the amount left by the amount used.
			 */

			n -= data_used;

			/*
			 * We cannot have used all the data here.
			 */

			cache_flush_needed = True;

			DO_PROFILE_INC(writecache_abutted_writes);
			total_written = data_used;

			write_path = 2;

		} else if ( (pos >= wcp->file_size) && 
					(wcp->offset + wcp->data_size == wcp->file_size) &&
					(pos > wcp->offset + wcp->data_size) && 
					(pos < wcp->offset + wcp->alloc_size) ) {

			/* ASCII art.... JRA.

                       End of file ---->|

                        +---------------+---------------+
                        | Cached data   | Cache buffer  |
                        +---------------+---------------+

                                              +-------------------+
                                              | Data to write     |
                                              +-------------------+

	    		*/

			/*
			 * Non-contiguous write part of which fits within
			 * the cache buffer and is extending the file
			 * and the cache contents reflect the current
			 * data up to the current end of the file.
			 */

			size_t data_used;

			if(pos + n <= wcp->offset + wcp->alloc_size) {
				data_used = n;
			} else {
				data_used = wcp->offset + wcp->alloc_size - pos;
			}

			/*
			 * Fill in the non-continuous area with zeros.
			 */

			memset(wcp->data + wcp->data_size, '\0',
				pos - (wcp->offset + wcp->data_size) );

			memcpy(wcp->data + (pos - wcp->offset), data, data_used);

			/*
			 * Update the current buffer size with the new data.
			 */

			if(pos + data_used > wcp->offset + wcp->data_size) {
				wcp->data_size = pos + data_used - wcp->offset;
			}

			/*
			 * Update the file size if changed.
			 */

			if (wcp->offset + wcp->data_size > wcp->file_size) {
				if (wcp_file_size_change(fsp) == -1) {
					return -1;
				}
			}

			/*
			 * If we used all the data then
			 * return here.
			 */

			if(n == data_used) {
				return n;
			} else {
				cache_flush_needed = True;
			}

			/*
			 * Move the start of data forward by the amount used,
			 * cut down the amount left by the same amount.
			 */

			data += data_used;
			pos += data_used;
			n -= data_used;

			DO_PROFILE_INC(writecache_abutted_writes);
			total_written = data_used;

			write_path = 3;

                } else if ( (pos >= wcp->file_size) &&
			    (n == 1) &&
			    (wcp->file_size == wcp->offset + wcp->data_size) &&
			    (pos < wcp->file_size + wcp->alloc_size)) {

                        /*

                End of file ---->|

                 +---------------+---------------+
                 | Cached data   | Cache buffer  |
                 +---------------+---------------+

                                 |<------- allocated size ---------------->|

                                                         +--------+
                                                         | 1 Byte |
                                                         +--------+

			MS-Office seems to do this a lot to determine if there's enough
			space on the filesystem to write a new file.

			Change to :

                End of file ---->|
                                 +-----------------------+--------+
                                 | Zeroed Cached data    | 1 Byte |
                                 +-----------------------+--------+
                        */

			flush_write_cache(fsp, WRITE_FLUSH);
			wcp->offset = wcp->file_size;
			wcp->data_size = pos - wcp->file_size + 1;
			memset(wcp->data, '\0', wcp->data_size);
			memcpy(wcp->data + wcp->data_size-1, data, 1);

			/*
			 * Update the file size if changed.
			 */

			if (wcp->offset + wcp->data_size > wcp->file_size) {
				if (wcp_file_size_change(fsp) == -1) {
					return -1;
				}
			}

			return n;

		} else {

			/* ASCII art..... JRA.

   Case 1).

                        +---------------+---------------+
                        | Cached data   | Cache buffer  |
                        +---------------+---------------+

                                                              +-------------------+
                                                              | Data to write     |
                                                              +-------------------+

   Case 2).

                           +---------------+---------------+
                           | Cached data   | Cache buffer  |
                           +---------------+---------------+

   +-------------------+
   | Data to write     |
   +-------------------+

    Case 3).

                           +---------------+---------------+
                           | Cached data   | Cache buffer  |
                           +---------------+---------------+

                  +-----------------------------------------------------+
                  | Data to write                                       |
                  +-----------------------------------------------------+

		  */

 			/*
			 * Write is bigger than buffer, or there is no overlap on the
			 * low or high ends.
			 */

			DEBUG(9,("write_file: non cacheable write : fd = %d, pos = %.0f, len = %u, current cache pos = %.0f \
len = %u\n",fsp->fh->fd, (double)pos, (unsigned int)n, (double)wcp->offset, (unsigned int)wcp->data_size ));

			/*
			 * If write would fit in the cache, and is larger than
			 * the data already in the cache, flush the cache and
			 * preferentially copy the data new data into it. Otherwise
			 * just write the data directly.
			 */

			if ( n <= wcp->alloc_size && n > wcp->data_size) {
				cache_flush_needed = True;
			} else {
				ssize_t ret = real_write_file(NULL,fsp, data, pos, n);

				/*
				 * If the write overlaps the entire cache, then
				 * discard the current contents of the cache.
				 * Fix from Rasmus Borup Hansen rbh@math.ku.dk.
				 */

				if ((pos <= wcp->offset) &&
						(pos + n >= wcp->offset + wcp->data_size) ) {
					DEBUG(9,("write_file: discarding overwritten write \
cache: fd = %d, off=%.0f, size=%u\n", fsp->fh->fd, (double)wcp->offset, (unsigned int)wcp->data_size ));
					wcp->data_size = 0;
				}

				DO_PROFILE_INC(writecache_direct_writes);
				if (ret == -1) {
					return ret;
				}

				if (pos + ret > wcp->file_size) {
					wcp->file_size = pos + ret;
				}

				return ret;
			}

			write_path = 4;

		}

		if (cache_flush_needed) {
			DEBUG(3,("WRITE_FLUSH:%d: due to noncontinuous write: fd = %d, size = %.0f, pos = %.0f, \
n = %u, wcp->offset=%.0f, wcp->data_size=%u\n",
				write_path, fsp->fh->fd, (double)wcp->file_size, (double)pos, (unsigned int)n,
				(double)wcp->offset, (unsigned int)wcp->data_size ));

			flush_write_cache(fsp, WRITE_FLUSH);
		}
	}

	/*
	 * If the write request is bigger than the cache
	 * size, write it all out.
	 */

	if (n > wcp->alloc_size ) {
		ssize_t ret = real_write_file(NULL,fsp, data, pos, n);
		if (ret == -1) {
			return -1;
		}

		if (pos + ret > wcp->file_size) {
			wcp->file_size = pos + n;
		}

		DO_PROFILE_INC(writecache_direct_writes);
		return total_written + n;
	}

	/*
	 * If there's any data left, cache it.
	 */

	if (n) {
#ifdef WITH_PROFILE
		if (wcp->data_size) {
			DO_PROFILE_INC(writecache_abutted_writes);
		} else {
			DO_PROFILE_INC(writecache_init_writes);
		}
#endif

		if ((wcp->data_size == 0)
		    && (pos > wcp->file_size)
		    && (pos + n <= wcp->file_size + wcp->alloc_size)) {
			/*
			 * This is a write completely beyond the
			 * current EOF, but within reach of the write
			 * cache. We expect fill-up writes pretty
			 * soon, so it does not make sense to start
			 * the write cache at the current
			 * offset. These fill-up writes would trigger
			 * separate pwrites or even unnecessary cache
			 * flushes because they overlap if this is a
			 * one-byte allocating write.
			 */
			wcp->offset = wcp->file_size;
			wcp->data_size = pos - wcp->file_size;
			memset(wcp->data, 0, wcp->data_size);
		}

		memcpy(wcp->data+wcp->data_size, data, n);
		if (wcp->data_size == 0) {
			wcp->offset = pos;
			DO_PROFILE_INC(writecache_num_write_caches);
		}
		wcp->data_size += n;

		/*
		 * Update the file size if changed.
		 */

		if (wcp->offset + wcp->data_size > wcp->file_size) {
			if (wcp_file_size_change(fsp) == -1) {
				return -1;
			}
		}
		DEBUG(9,("wcp->offset = %.0f wcp->data_size = %u cache return %u\n",
			(double)wcp->offset, (unsigned int)wcp->data_size, (unsigned int)n));

		total_written += n;
		return total_written; /* .... that's a write :) */
	}
  
	return total_written;
}

/****************************************************************************
 Delete the write cache structure.
****************************************************************************/

void delete_write_cache(files_struct *fsp)
{
	write_cache *wcp;

	if(!fsp) {
		return;
	}

	if(!(wcp = fsp->wcp)) {
		return;
	}

	DO_PROFILE_DEC(writecache_allocated_write_caches);
	allocated_write_caches--;

	SMB_ASSERT(wcp->data_size == 0);

	SAFE_FREE(wcp->data);
	SAFE_FREE(fsp->wcp);

	DEBUG(10,("delete_write_cache: File %s deleted write cache\n",
		  fsp_str_dbg(fsp)));
}

/****************************************************************************
 Setup the write cache structure.
****************************************************************************/

static bool setup_write_cache(files_struct *fsp, SMB_OFF_T file_size)
{
	ssize_t alloc_size = lp_write_cache_size(SNUM(fsp->conn));
	write_cache *wcp;

	if (allocated_write_caches >= MAX_WRITE_CACHES) {
		return False;
	}

	if(alloc_size == 0 || fsp->wcp) {
		return False;
	}

	if((wcp = SMB_MALLOC_P(write_cache)) == NULL) {
		DEBUG(0,("setup_write_cache: malloc fail.\n"));
		return False;
	}

	wcp->file_size = file_size;
	wcp->offset = 0;
	wcp->alloc_size = alloc_size;
	wcp->data_size = 0;
	if((wcp->data = (char *)SMB_MALLOC(wcp->alloc_size)) == NULL) {
		DEBUG(0,("setup_write_cache: malloc fail for buffer size %u.\n",
			(unsigned int)wcp->alloc_size ));
		SAFE_FREE(wcp);
		return False;
	}

	memset(wcp->data, '\0', wcp->alloc_size );

	fsp->wcp = wcp;
	DO_PROFILE_INC(writecache_allocated_write_caches);
	allocated_write_caches++;

	DEBUG(10,("setup_write_cache: File %s allocated write cache size %lu\n",
		  fsp_str_dbg(fsp), (unsigned long)wcp->alloc_size));

	return True;
}

/****************************************************************************
 Cope with a size change.
****************************************************************************/

void set_filelen_write_cache(files_struct *fsp, SMB_OFF_T file_size)
{
	if(fsp->wcp) {
		/* The cache *must* have been flushed before we do this. */
		if (fsp->wcp->data_size != 0) {
			char *msg;
			if (asprintf(&msg, "set_filelen_write_cache: size change "
				 "on file %s with write cache size = %lu\n",
				 fsp->fsp_name->base_name,
				 (unsigned long)fsp->wcp->data_size) != -1) {
				smb_panic(msg);
			} else {
				smb_panic("set_filelen_write_cache");
			}
		}
		fsp->wcp->file_size = file_size;
	}
}

/*******************************************************************
 Flush a write cache struct to disk.
********************************************************************/

ssize_t flush_write_cache(files_struct *fsp, enum flush_reason_enum reason)
{
	write_cache *wcp = fsp->wcp;
	size_t data_size;
	ssize_t ret;

	if(!wcp || !wcp->data_size) {
		return 0;
	}

	data_size = wcp->data_size;
	wcp->data_size = 0;

	DO_PROFILE_DEC_INC(writecache_num_write_caches,writecache_flushed_writes[reason]);

	DEBUG(9,("flushing write cache: fd = %d, off=%.0f, size=%u\n",
		fsp->fh->fd, (double)wcp->offset, (unsigned int)data_size));

#ifdef WITH_PROFILE
	if(data_size == wcp->alloc_size) {
		DO_PROFILE_INC(writecache_num_perfect_writes);
	}
#endif

	ret = real_write_file(NULL, fsp, wcp->data, wcp->offset, data_size);

	/*
	 * Ensure file size if kept up to date if write extends file.
	 */

	if ((ret != -1) && (wcp->offset + ret > wcp->file_size)) {
		wcp->file_size = wcp->offset + ret;
	}

	return ret;
}

/*******************************************************************
sync a file
********************************************************************/

NTSTATUS sync_file(connection_struct *conn, files_struct *fsp, bool write_through)
{
       	if (fsp->fh->fd == -1)
		return NT_STATUS_INVALID_HANDLE;

	if (lp_strict_sync(SNUM(conn)) &&
	    (lp_syncalways(SNUM(conn)) || write_through)) {
		int ret = flush_write_cache(fsp, SYNC_FLUSH);
		if (ret == -1) {
			return map_nt_error_from_unix(errno);
		}
		ret = SMB_VFS_FSYNC(fsp);
		if (ret == -1) {
			return map_nt_error_from_unix(errno);
		}
	}
	return NT_STATUS_OK;
}

/************************************************************
 Perform a stat whether a valid fd or not.
************************************************************/

int fsp_stat(files_struct *fsp)
{
	if (fsp->fh->fd == -1) {
		if (fsp->posix_open) {
			return SMB_VFS_LSTAT(fsp->conn, fsp->fsp_name);
		} else {
			return SMB_VFS_STAT(fsp->conn, fsp->fsp_name);
		}
	} else {
		return SMB_VFS_FSTAT(fsp, &fsp->fsp_name->st);
	}
}
