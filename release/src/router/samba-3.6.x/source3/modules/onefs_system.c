/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS system interfaces.
 *
 * Copyright (C) Tim Prouty, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "onefs.h"
#include "onefs_config.h"
#include "oplock_onefs.h"

#include <ifs/ifs_syscalls.h>
#include <isi_acl/isi_acl_util.h>
#include <sys/isi_acl.h>

/*
 * Initialize the sm_lock struct before passing it to ifs_createfile.
 */
static void smlock_init(connection_struct *conn, struct sm_lock *sml,
    bool isexe, uint32_t access_mask, uint32_t share_access,
    uint32_t create_options)
{
	sml->sm_type.doc = false;
	sml->sm_type.isexe = isexe;
	sml->sm_type.statonly = is_stat_open(access_mask);
	sml->sm_type.access_mask = access_mask;
	sml->sm_type.share_access = share_access;

	/*
	 * private_options was previously used for DENY_DOS/DENY_FCB checks in
	 * the kernel, but are now properly handled by fcb_or_dos_open. In
	 * these cases, ifs_createfile will return a sharing violation, which
	 * gives fcb_or_dos_open the chance to open a duplicate file handle.
	 */
	sml->sm_type.private_options = 0;

	/* 1 second delay is handled in onefs_open.c by deferring the open */
	sml->sm_timeout = timeval_set(0, 0);
}

static void smlock_dump(int debuglevel, const struct sm_lock *sml)
{
	if (sml == NULL) {
		DEBUG(debuglevel, ("sml == NULL\n"));
		return;
	}

	DEBUG(debuglevel,
	      ("smlock: doc=%s, isexec=%s, statonly=%s, access_mask=0x%x, "
	       "share_access=0x%x, private_options=0x%x timeout=%d/%d\n",
	       sml->sm_type.doc ? "True" : "False",
	       sml->sm_type.isexe ? "True" : "False",
	       sml->sm_type.statonly ? "True" : "False",
	       sml->sm_type.access_mask,
	       sml->sm_type.share_access,
	       sml->sm_type.private_options,
	       (int)sml->sm_timeout.tv_sec,
	       (int)sml->sm_timeout.tv_usec));
}

/**
 * External interface to ifs_createfile
 */
int onefs_sys_create_file(connection_struct *conn,
			  int base_fd,
			  const char *path,
		          uint32_t access_mask,
		          uint32_t open_access_mask,
			  uint32_t share_access,
			  uint32_t create_options,
			  int flags,
			  mode_t mode,
			  int oplock_request,
			  uint64_t id,
			  struct security_descriptor *sd,
			  uint32_t dos_flags,
			  int *granted_oplock)
{
	struct sm_lock sml, *psml = NULL;
	enum oplock_type onefs_oplock;
	enum oplock_type onefs_granted_oplock = OPLOCK_NONE;
	struct ifs_security_descriptor ifs_sd = {}, *pifs_sd = NULL;
	uint32_t sec_info_effective = 0;
	int ret_fd = -1;
	uint32_t onefs_dos_attributes;
	struct ifs_createfile_flags cf_flags = CF_FLAGS_NONE;
	char *mapped_name = NULL;
	NTSTATUS result;

	START_PROFILE(syscall_createfile);

	/* Translate the name to UNIX before calling ifs_createfile */
	mapped_name = talloc_strdup(talloc_tos(), path);
	if (mapped_name == NULL) {
		errno = ENOMEM;
		goto out;
	}
	result = SMB_VFS_TRANSLATE_NAME(conn, &mapped_name,
					vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(result)) {
		goto out;
	}

	/* Setup security descriptor and get secinfo. */
	if (sd != NULL) {
		NTSTATUS status;
		uint32_t sec_info_sent = 0;

		sec_info_sent = (get_sec_info(sd) & IFS_SEC_INFO_KNOWN_MASK);

		status = onefs_samba_sd_to_sd(sec_info_sent, sd, &ifs_sd,
					      SNUM(conn), &sec_info_effective);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("SD initialization failure: %s\n",
				  nt_errstr(status)));
			errno = EINVAL;
			goto out;
		}

		pifs_sd = &ifs_sd;
	}

	/* Stripping off private bits will be done for us. */
	onefs_oplock = onefs_samba_oplock_to_oplock(oplock_request);

	if (!lp_oplocks(SNUM(conn))) {
		SMB_ASSERT(onefs_oplock == OPLOCK_NONE);
	}

	/* Convert samba dos flags to UF_DOS_* attributes. */
	onefs_dos_attributes = dos_attributes_to_stat_dos_flags(dos_flags);

	/**
	 * Deal with kernel creating Default ACLs. (Isilon bug 47447.)
	 *
	 * 1) "nt acl support = no", default_acl = no
	 * 2) "inherit permissions = yes", default_acl = no
	 */
	if (lp_nt_acl_support(SNUM(conn)) && !lp_inherit_perms(SNUM(conn)))
		cf_flags = cf_flags_or(cf_flags, CF_FLAGS_DEFAULT_ACL);

	/*
	 * Some customer workflows require the execute bit to be ignored.
	 */
	if (lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_ALLOW_EXECUTE_ALWAYS,
			 PARM_ALLOW_EXECUTE_ALWAYS_DEFAULT) &&
	    (open_access_mask & FILE_EXECUTE)) {

		DEBUG(3, ("Stripping execute bit from %s: (0x%x)\n", mapped_name,
			  open_access_mask));

		/* Strip execute. */
		open_access_mask &= ~FILE_EXECUTE;

		/*
		 * Add READ_DATA, so we're not left with desired_access=0. An
		 * execute call should imply the client will read the data.
		 */
		open_access_mask |= FILE_READ_DATA;

		DEBUGADD(3, ("New stripped access mask: 0x%x\n",
			     open_access_mask));
	}

	DEBUG(10,("onefs_sys_create_file: base_fd = %d, fname = %s "
		  "open_access_mask = 0x%x, flags = 0x%x, mode = 0%o, "
		  "desired_oplock = %s, id = 0x%x, secinfo = 0x%x, sd = %p, "
		  "dos_attributes = 0x%x, path = %s, "
		  "default_acl=%s\n", base_fd, mapped_name,
		  (unsigned int)open_access_mask,
		  (unsigned int)flags,
		  (unsigned int)mode,
		  onefs_oplock_str(onefs_oplock),
		  (unsigned int)id,
		  sec_info_effective, sd,
		  (unsigned int)onefs_dos_attributes, mapped_name,
		  cf_flags_and_bool(cf_flags, CF_FLAGS_DEFAULT_ACL) ?
		      "true" : "false"));

	/* Initialize smlock struct for files/dirs but not internal opens */
	if (!(oplock_request & INTERNAL_OPEN_ONLY)) {
		smlock_init(conn, &sml, is_executable(mapped_name), access_mask,
		    share_access, create_options);
		psml = &sml;
	}

	smlock_dump(10, psml);

	ret_fd = ifs_createfile(base_fd, mapped_name,
	    (enum ifs_ace_rights)open_access_mask, flags & ~O_ACCMODE, mode,
	    onefs_oplock, id, psml, sec_info_effective, pifs_sd,
	    onefs_dos_attributes, cf_flags, &onefs_granted_oplock);

	DEBUG(10,("onefs_sys_create_file(%s): ret_fd = %d, "
		  "onefs_granted_oplock = %s\n",
		  ret_fd < 0 ? strerror(errno) : "success", ret_fd,
		  onefs_oplock_str(onefs_granted_oplock)));

	if (granted_oplock) {
		*granted_oplock =
		    onefs_oplock_to_samba_oplock(onefs_granted_oplock);
	}

 out:
	END_PROFILE(syscall_createfile);
	aclu_free_sd(pifs_sd, false);
	TALLOC_FREE(mapped_name);

	return ret_fd;
}

/**
 * FreeBSD based sendfile implementation that allows for atomic semantics.
 */
static ssize_t onefs_sys_do_sendfile(int tofd, int fromfd,
    const DATA_BLOB *header, SMB_OFF_T offset, size_t count, bool atomic)
{
	size_t total=0;
	struct sf_hdtr hdr;
	struct iovec hdtrl;
	size_t hdr_len = 0;
	int flags = 0;

	if (atomic) {
		flags = SF_ATOMIC;
	}

	hdr.headers = &hdtrl;
	hdr.hdr_cnt = 1;
	hdr.trailers = NULL;
	hdr.trl_cnt = 0;

	/* Set up the header iovec. */
	if (header) {
		hdtrl.iov_base = (void *)header->data;
		hdtrl.iov_len = hdr_len = header->length;
	} else {
		hdtrl.iov_base = NULL;
		hdtrl.iov_len = 0;
	}

	total = count;
	while (total + hdtrl.iov_len) {
		SMB_OFF_T nwritten;
		int ret;

		/*
		 * FreeBSD sendfile returns 0 on success, -1 on error.
		 * Remember, the tofd and fromfd are reversed..... :-).
		 * nwritten includes the header data sent.
		 */

		do {
			ret = sendfile(fromfd, tofd, offset, total, &hdr,
				       &nwritten, flags);
#if defined(EWOULDBLOCK)
		} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
		} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif

		/* On error we're done. */
		if (ret == -1) {
			return -1;
		}

		/*
		 * If this was an ATOMIC sendfile, nwritten doesn't
		 * necessarily indicate an error.  It could mean count > than
		 * what sendfile can handle atomically (usually 64K) or that
		 * there was a short read due to the file being truncated.
		 */
		if (nwritten == 0) {
			return atomic ? 0 : -1;
		}

		/*
		 * An atomic sendfile should never send partial data!
		 */
		if (atomic && nwritten != total + hdtrl.iov_len) {
			DEBUG(0,("Atomic sendfile() sent partial data: "
				 "%llu of %d\n", nwritten,
				 total + hdtrl.iov_len));
			return -1;
		}

		/*
		 * If this was a short (signal interrupted) write we may need
		 * to subtract it from the header data, or null out the header
		 * data altogether if we wrote more than hdtrl.iov_len bytes.
		 * We change nwritten to be the number of file bytes written.
		 */

		if (hdtrl.iov_base && hdtrl.iov_len) {
			if (nwritten >= hdtrl.iov_len) {
				nwritten -= hdtrl.iov_len;
				hdtrl.iov_base = NULL;
				hdtrl.iov_len = 0;
			} else {
				hdtrl.iov_base =
				    (void *)((caddr_t)hdtrl.iov_base + nwritten);
				hdtrl.iov_len -= nwritten;
				nwritten = 0;
			}
		}
		total -= nwritten;
		offset += nwritten;
	}
	return count + hdr_len;
}

/**
 * Handles the subtleties of using sendfile with CIFS.
 */
ssize_t onefs_sys_sendfile(connection_struct *conn, int tofd, int fromfd,
			   const DATA_BLOB *header, SMB_OFF_T offset,
			   size_t count)
{
	bool atomic = false;
	ssize_t ret = 0;

	START_PROFILE_BYTES(syscall_sendfile, count);

	if (lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_ATOMIC_SENDFILE,
			 PARM_ATOMIC_SENDFILE_DEFAULT)) {
		atomic = true;
	}

	/* Try the sendfile */
	ret = onefs_sys_do_sendfile(tofd, fromfd, header, offset, count,
				    atomic);

	/* If the sendfile wasn't atomic, we're done. */
	if (!atomic) {
		DEBUG(10, ("non-atomic sendfile read %ul bytes\n", ret));
		END_PROFILE(syscall_sendfile);
		return ret;
	}

	/*
	 * Atomic sendfile takes care to not write anything to the socket
	 * until all of the requested bytes have been read from the file.
	 * There are two atomic cases that need to be handled.
	 *
	 *  1. The file was truncated causing less data to be read than was
	 *     requested.  In this case, we return back to the caller to
	 *     indicate 0 bytes were written to the socket.  This should
	 *     prompt the caller to fallback to the standard read path: read
	 *     the data, create a header that indicates how many bytes were
	 *     actually read, and send the header/data back to the client.
	 *
	 *     This saves us from standard sendfile behavior of sending a
	 *     header promising more data then will actually be sent.  The
	 *     only two options are to close the socket and kill the client
	 *     connection, or write a bunch of 0s.  Closing the client
	 *     connection is bad because there could actually be multiple
	 *     sessions multiplexed from the same client that are all dropped
	 *     because of a truncate.  Writing the remaining data as 0s also
	 *     isn't good, because the client will have an incorrect version
	 *     of the file.  If the file is written back to the server, the 0s
	 *     will be written back.  Fortunately, atomic sendfile allows us
	 *     to avoid making this choice in most cases.
	 *
	 *  2. One downside of atomic sendfile, is that there is a limit on
	 *     the number of bytes that can be sent atomically.  The kernel
	 *     has a limited amount of mbuf space that it can read file data
	 *     into without exhausting the system's mbufs, so a buffer of
	 *     length xfsize is used.  The xfsize at the time of writing this
	 *     is 64K.  xfsize bytes are read from the file, and subsequently
	 *     written to the socket.  This makes it impossible to do the
	 *     sendfile atomically for a byte count > xfsize.
	 *
	 *     To cope with large requests, atomic sendfile returns -1 with
	 *     errno set to E2BIG.  Since windows maxes out at 64K writes,
	 *     this is currently only a concern with non-windows clients.
	 *     Posix extensions allow the full 24bit bytecount field to be
	 *     used in ReadAndX, and clients such as smbclient and the linux
	 *     cifs client can request up to 16MB reads!  There are a few
	 *     options for handling large sendfile requests.
	 *
	 *	a. Fall back to the standard read path.  This is unacceptable
	 *         because it would require prohibitively large mallocs.
	 *
	 *	b. Fall back to using samba's fake_send_file which emulates
	 *	   the kernel sendfile in userspace.  This still has the same
	 *	   problem of sending the header before all of the data has
	 *	   been read, so it doesn't buy us anything, and has worse
	 *	   performance than the kernel's zero-copy sendfile.
	 *
	 *	c. Use non-atomic sendfile syscall to attempt a zero copy
	 *	   read, and hope that there isn't a short read due to
	 *	   truncation.  In the case of a short read, there are two
	 *	   options:
	 *
	 *	    1. Kill the client connection
	 *
	 *	    2. Write zeros to the socket for the remaining bytes
	 *	       promised in the header.
	 *
	 *	   It is safer from a data corruption perspective to kill the
	 *	   client connection, so this is our default behavior, but if
	 *	   this causes problems this can be configured to write zeros
	 *	   via smb.conf.
	 */

	/* Handle case 1: short read -> truncated file. */
	if (ret == 0) {
		END_PROFILE(syscall_sendfile);
		return ret;
	}

	/* Handle case 2: large read. */
	if (ret == -1 && errno == E2BIG) {

		if (!lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
				 PARM_SENDFILE_LARGE_READS,
				 PARM_SENDFILE_LARGE_READS_DEFAULT)) {
			DEBUG(3, ("Not attempting non-atomic large sendfile: "
				  "%lu bytes\n", count));
			END_PROFILE(syscall_sendfile);
			return 0;
		}

		if (count < 0x10000) {
			DEBUG(0, ("Count < 2^16 and E2BIG was returned! %lu\n",
				  count));
		}

		DEBUG(10, ("attempting non-atomic large sendfile: %lu bytes\n",
			   count));

		/* Try a non-atomic sendfile. */
		ret = onefs_sys_do_sendfile(tofd, fromfd, header, offset,
					    count, false);
		/* Real error: kill the client connection. */
		if (ret == -1) {
			DEBUG(1, ("error on non-atomic large sendfile "
				  "(%lu bytes): %s\n", count,
				  strerror(errno)));
			END_PROFILE(syscall_sendfile);
			return ret;
		}

		/* Short read: kill the client connection. */
		if (ret != count + header->length) {
			DEBUG(1, ("short read on non-atomic large sendfile "
				  "(%lu of %lu bytes): %s\n", ret, count,
				  strerror(errno)));

			/*
			 * Returning ret here would cause us to drop into the
			 * codepath that calls sendfile_short_send, which
			 * sends the client a bunch of zeros instead.
			 * Returning -1 kills the connection.
			 */
			if (lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
				PARM_SENDFILE_SAFE,
				PARM_SENDFILE_SAFE_DEFAULT)) {
				END_PROFILE(syscall_sendfile);
				return -1;
			}

			END_PROFILE(syscall_sendfile);
			return ret;
		}

		DEBUG(10, ("non-atomic large sendfile successful\n"));
	}

	/* There was error in the atomic sendfile. */
	if (ret == -1) {
		DEBUG(1, ("error on %s sendfile (%lu bytes): %s\n",
			  atomic ? "atomic" : "non-atomic",
			  count, strerror(errno)));
	}

	END_PROFILE(syscall_sendfile);
	return ret;
}

/**
 * Only talloc the spill buffer once (reallocing when necessary).
 */
static char *get_spill_buffer(size_t new_count)
{
	static int cur_count = 0;
	static char *spill_buffer = NULL;

	/* If a sufficiently sized buffer exists, just return. */
	if (new_count <= cur_count) {
		SMB_ASSERT(spill_buffer);
		return spill_buffer;
	}

	/* Allocate the first time. */
	if (cur_count == 0) {
		SMB_ASSERT(!spill_buffer);
		spill_buffer = talloc_array(NULL, char, new_count);
		if (spill_buffer) {
			cur_count = new_count;
		}
		return spill_buffer;
	}

	/* A buffer exists, but it's not big enough, so realloc. */
	SMB_ASSERT(spill_buffer);
	spill_buffer = talloc_realloc(NULL, spill_buffer, char, new_count);
	if (spill_buffer) {
		cur_count = new_count;
	}
	return spill_buffer;
}

/**
 * recvfile does zero-copy writes given an fd to write to, and a socket with
 * some data to write.  If recvfile read more than it was able to write, it
 * spills the data into a buffer.  After first reading any additional data
 * from the socket into the buffer, the spill buffer is then written with a
 * standard pwrite.
 */
ssize_t onefs_sys_recvfile(int fromfd, int tofd, SMB_OFF_T offset,
			   size_t count)
{
	char *spill_buffer = NULL;
	bool socket_drained = false;
	int ret;
	off_t total_rbytes = 0;
	off_t total_wbytes = 0;
	off_t rbytes;
	off_t wbytes;

	START_PROFILE_BYTES(syscall_recvfile, count);

	DEBUG(10,("onefs_recvfile: from = %d, to = %d, offset=%llu, count = "
		  "%lu\n", fromfd, tofd, offset, count));

	if (count == 0) {
		END_PROFILE(syscall_recvfile);
		return 0;
	}

	/*
	 * Setup up a buffer for recvfile to spill data that has been read
	 * from the socket but not written.
	 */
	spill_buffer = get_spill_buffer(count);
	if (spill_buffer == NULL) {
		ret = -1;
		goto out;
	}

	/*
	 * Keep trying recvfile until:
	 *  - There is no data left to read on the socket, or
	 *  - bytes read != bytes written, or
	 *  - An error is returned that isn't EINTR/EAGAIN
	 */
	do {
		/* Keep track of bytes read/written for recvfile */
		rbytes = 0;
		wbytes = 0;

		DEBUG(10, ("calling recvfile loop, offset + total_wbytes = "
			   "%llu, count - total_rbytes = %llu\n",
			   offset + total_wbytes, count - total_rbytes));

		ret = recvfile(tofd, fromfd, offset + total_wbytes,
			       count - total_wbytes, &rbytes, &wbytes, 0,
			       spill_buffer);

		DEBUG(10, ("recvfile ret = %d, errno = %d, rbytes = %llu, "
			   "wbytes = %llu\n", ret, ret >= 0 ? 0 : errno,
			   rbytes, wbytes));

		/* Update our progress so far */
		total_rbytes += rbytes;
		total_wbytes += wbytes;

	} while ((count - total_rbytes) && (rbytes == wbytes) &&
		 (ret == -1 && (errno == EINTR || errno == EAGAIN)));

	DEBUG(10, ("total_rbytes = %llu, total_wbytes = %llu\n",
		   total_rbytes, total_wbytes));

	/* Log if recvfile didn't write everything it read. */
	if (total_rbytes != total_wbytes) {
		DEBUG(3, ("partial recvfile: total_rbytes=%llu but "
			  "total_wbytes=%llu, diff = %llu\n", total_rbytes,
			  total_wbytes, total_rbytes - total_wbytes));
		SMB_ASSERT(total_rbytes > total_wbytes);
	}

	/*
	 * If there is still data on the socket, read it off.
	 */
	while (total_rbytes < count) {

		DEBUG(3, ("shallow recvfile (%s), reading %llu\n",
			  strerror(errno), count - total_rbytes));

		/*
		 * Read the remaining data into the spill buffer.  recvfile
		 * may already have some data in the spill buffer, so start
		 * filling the buffer at total_rbytes - total_wbytes.
		 */
		ret = sys_read(fromfd,
			       spill_buffer + (total_rbytes - total_wbytes),
			       count - total_rbytes);

		if (ret <= 0) {
			if (ret == 0) {
				DEBUG(0, ("shallow recvfile read: EOF\n"));
			} else {
				DEBUG(0, ("shallow recvfile read failed: %s\n",
					  strerror(errno)));
			}
			/* Socket is dead, so treat as if it were drained. */
			socket_drained = true;
			goto out;
		}

		/* Data was read so update the rbytes */
		total_rbytes += ret;
	}

	if (total_rbytes != count) {
		smb_panic("Unread recvfile data still on the socket!");
	}

	/*
	 * Now write any spilled data + the extra data read off the socket.
	 */
	while (total_wbytes < count) {

		DEBUG(3, ("partial recvfile, writing %llu\n", count - total_wbytes));

		ret = sys_pwrite(tofd, spill_buffer, count - total_wbytes,
				 offset + total_wbytes);

		if (ret == -1) {
			DEBUG(0, ("partial recvfile write failed: %s\n",
				  strerror(errno)));
			goto out;
		}

		/* Data was written so update the wbytes */
		total_wbytes += ret;
	}

	/* Success! */
	ret = total_wbytes;

out:

	END_PROFILE(syscall_recvfile);

	/* Make sure we always try to drain the socket. */
	if (!socket_drained && count - total_rbytes) {
		int saved_errno = errno;

		if (drain_socket(fromfd, count - total_rbytes) !=
		    count - total_rbytes) {
			/* Socket is dead! */
			DEBUG(0, ("drain socket failed: %d\n", errno));
		}
		errno = saved_errno;
	}

	return ret;
}

void init_stat_ex_from_onefs_stat(struct stat_ex *dst, const struct stat *src)
{
	ZERO_STRUCT(*dst);

	dst->st_ex_dev = src->st_dev;
	dst->st_ex_ino = src->st_ino;
	dst->st_ex_mode = src->st_mode;
	dst->st_ex_nlink = src->st_nlink;
	dst->st_ex_uid = src->st_uid;
	dst->st_ex_gid = src->st_gid;
	dst->st_ex_rdev = src->st_rdev;
	dst->st_ex_size = src->st_size;
	dst->st_ex_atime = src->st_atimespec;
	dst->st_ex_mtime = src->st_mtimespec;
	dst->st_ex_ctime = src->st_ctimespec;
	dst->st_ex_btime = src->st_birthtimespec;
	dst->st_ex_blksize = src->st_blksize;
	dst->st_ex_blocks = src->st_blocks;

	dst->st_ex_flags = src->st_flags;

	dst->vfs_private = src->st_snapid;
}

int onefs_sys_stat(const char *fname, SMB_STRUCT_STAT *sbuf)
{
	int ret;
	struct stat onefs_sbuf;

	ret = stat(fname, &onefs_sbuf);

	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(onefs_sbuf.st_mode)) {
			onefs_sbuf.st_size = 0;
		}
		init_stat_ex_from_onefs_stat(sbuf, &onefs_sbuf);
	}
	return ret;
}

int onefs_sys_fstat(int fd, SMB_STRUCT_STAT *sbuf)
{
	int ret;
	struct stat onefs_sbuf;

	ret = fstat(fd, &onefs_sbuf);

	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(onefs_sbuf.st_mode)) {
			onefs_sbuf.st_size = 0;
		}
		init_stat_ex_from_onefs_stat(sbuf, &onefs_sbuf);
	}
	return ret;
}

int onefs_sys_fstat_at(int base_fd, const char *fname, SMB_STRUCT_STAT *sbuf,
		       int flags)
{
	int ret;
	struct stat onefs_sbuf;

	ret = enc_fstatat(base_fd, fname, ENC_DEFAULT, &onefs_sbuf, flags);

	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(onefs_sbuf.st_mode)) {
			onefs_sbuf.st_size = 0;
		}
		init_stat_ex_from_onefs_stat(sbuf, &onefs_sbuf);
	}
	return ret;
}

int onefs_sys_lstat(const char *fname, SMB_STRUCT_STAT *sbuf)
{
	int ret;
	struct stat onefs_sbuf;

	ret = lstat(fname, &onefs_sbuf);

	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(onefs_sbuf.st_mode)) {
			onefs_sbuf.st_size = 0;
		}
		init_stat_ex_from_onefs_stat(sbuf, &onefs_sbuf);
	}
	return ret;
}

