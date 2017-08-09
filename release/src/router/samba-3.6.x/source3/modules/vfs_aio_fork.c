/*
 * Simulate the Posix AIO using mmap/fork
 *
 * Copyright (C) Volker Lendecke 2008
 * Copyright (C) Jeremy Allison 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "system/filesys.h"
#include "system/shmem.h"
#include "smbd/smbd.h"

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

struct mmap_area {
	size_t size;
	volatile void *ptr;
};

static int mmap_area_destructor(struct mmap_area *area)
{
	munmap((void *)area->ptr, area->size);
	return 0;
}

static struct mmap_area *mmap_area_init(TALLOC_CTX *mem_ctx, size_t size)
{
	struct mmap_area *result;
	int fd;

	result = talloc(mem_ctx, struct mmap_area);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	fd = open("/dev/zero", O_RDWR);
	if (fd == -1) {
		DEBUG(3, ("open(\"/dev/zero\") failed: %s\n",
			  strerror(errno)));
		goto fail;
	}

	result->ptr = mmap(NULL, size, PROT_READ|PROT_WRITE,
			   MAP_SHARED|MAP_FILE, fd, 0);
	if (result->ptr == MAP_FAILED) {
		DEBUG(1, ("mmap failed: %s\n", strerror(errno)));
		goto fail;
	}

	close(fd);

	result->size = size;
	talloc_set_destructor(result, mmap_area_destructor);

	return result;

fail:
	TALLOC_FREE(result);
	return NULL;
}

struct rw_cmd {
	size_t n;
	SMB_OFF_T offset;
	bool read_cmd;
};

struct rw_ret {
	ssize_t size;
	int ret_errno;
};

struct aio_child_list;

struct aio_child {
	struct aio_child *prev, *next;
	struct aio_child_list *list;
	SMB_STRUCT_AIOCB *aiocb;
	pid_t pid;
	int sockfd;
	struct fd_event *sock_event;
	struct rw_ret retval;
	struct mmap_area *map;	/* ==NULL means write request */
	bool dont_delete;	/* Marked as in use since last cleanup */
	bool cancelled;
	bool read_cmd;
	bool called_from_suspend;
	bool completion_done;
};

struct aio_child_list {
	struct aio_child *children;
	struct timed_event *cleanup_event;
};

static void free_aio_children(void **p)
{
	TALLOC_FREE(*p);
}

static ssize_t read_fd(int fd, void *ptr, size_t nbytes, int *recvfd)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t n;
#ifndef HAVE_MSGHDR_MSG_CONTROL
	int newfd;
#endif

#ifdef	HAVE_MSGHDR_MSG_CONTROL
	union {
	  struct cmsghdr	cm;
	  char				control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr	*cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
#else
#if HAVE_MSGHDR_MSG_ACCTRIGHTS
	msg.msg_accrights = (caddr_t) &newfd;
	msg.msg_accrightslen = sizeof(int);
#else
#error Can not pass file descriptors
#endif
#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = (void *)ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ( (n = recvmsg(fd, &msg, 0)) <= 0) {
		return(n);
	}

#ifdef	HAVE_MSGHDR_MSG_CONTROL
	if ((cmptr = CMSG_FIRSTHDR(&msg)) != NULL
	    && cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
		if (cmptr->cmsg_level != SOL_SOCKET) {
			DEBUG(10, ("control level != SOL_SOCKET"));
			errno = EINVAL;
			return -1;
		}
		if (cmptr->cmsg_type != SCM_RIGHTS) {
			DEBUG(10, ("control type != SCM_RIGHTS"));
			errno = EINVAL;
			return -1;
		}
		*recvfd = *((int *) CMSG_DATA(cmptr));
	} else {
		*recvfd = -1;		/* descriptor was not passed */
	}
#else
	if (msg.msg_accrightslen == sizeof(int)) {
		*recvfd = newfd;
	}
	else {
		*recvfd = -1;		/* descriptor was not passed */
	}
#endif

	return(n);
}

static ssize_t write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
	struct msghdr	msg;
	struct iovec	iov[1];

#ifdef	HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr	cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr	*cmptr;

	ZERO_STRUCT(msg);
	ZERO_STRUCT(control_un);

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = sendfd;
#else
	ZERO_STRUCT(msg);
	msg.msg_accrights = (caddr_t) &sendfd;
	msg.msg_accrightslen = sizeof(int);
#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	ZERO_STRUCT(iov);
	iov[0].iov_base = (void *)ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	return (sendmsg(fd, &msg, 0));
}

static void aio_child_cleanup(struct event_context *event_ctx,
			      struct timed_event *te,
			      struct timeval now,
			      void *private_data)
{
	struct aio_child_list *list = talloc_get_type_abort(
		private_data, struct aio_child_list);
	struct aio_child *child, *next;

	TALLOC_FREE(list->cleanup_event);

	for (child = list->children; child != NULL; child = next) {
		next = child->next;

		if (child->aiocb != NULL) {
			DEBUG(10, ("child %d currently active\n",
				   (int)child->pid));
			continue;
		}

		if (child->dont_delete) {
			DEBUG(10, ("Child %d was active since last cleanup\n",
				   (int)child->pid));
			child->dont_delete = false;
			continue;
		}

		DEBUG(10, ("Child %d idle for more than 30 seconds, "
			   "deleting\n", (int)child->pid));

		TALLOC_FREE(child);
		child = next;
	}

	if (list->children != NULL) {
		/*
		 * Re-schedule the next cleanup round
		 */
		list->cleanup_event = event_add_timed(smbd_event_context(), list,
						      timeval_add(&now, 30, 0),
						      aio_child_cleanup, list);

	}
}

static struct aio_child_list *init_aio_children(struct vfs_handle_struct *handle)
{
	struct aio_child_list *data = NULL;

	if (SMB_VFS_HANDLE_TEST_DATA(handle)) {
		SMB_VFS_HANDLE_GET_DATA(handle, data, struct aio_child_list,
					return NULL);
	}

	if (data == NULL) {
		data = TALLOC_ZERO_P(NULL, struct aio_child_list);
		if (data == NULL) {
			return NULL;
		}
	}

	/*
	 * Regardless of whether the child_list had been around or not, make
	 * sure that we have a cleanup timed event. This timed event will
	 * delete itself when it finds that no children are around anymore.
	 */

	if (data->cleanup_event == NULL) {
		data->cleanup_event = event_add_timed(smbd_event_context(), data,
						      timeval_current_ofs(30, 0),
						      aio_child_cleanup, data);
		if (data->cleanup_event == NULL) {
			TALLOC_FREE(data);
			return NULL;
		}
	}

	if (!SMB_VFS_HANDLE_TEST_DATA(handle)) {
		SMB_VFS_HANDLE_SET_DATA(handle, data, free_aio_children,
					struct aio_child_list, return False);
	}

	return data;
}

static void aio_child_loop(int sockfd, struct mmap_area *map)
{
	while (true) {
		int fd = -1;
		ssize_t ret;
		struct rw_cmd cmd_struct;
		struct rw_ret ret_struct;

		ret = read_fd(sockfd, &cmd_struct, sizeof(cmd_struct), &fd);
		if (ret != sizeof(cmd_struct)) {
			DEBUG(10, ("read_fd returned %d: %s\n", (int)ret,
				   strerror(errno)));
			exit(1);
		}

		DEBUG(10, ("aio_child_loop: %s %d bytes at %d from fd %d\n",
			   cmd_struct.read_cmd ? "read" : "write",
			   (int)cmd_struct.n, (int)cmd_struct.offset, fd));

#ifdef ENABLE_BUILD_FARM_HACKS
		{
			/*
			 * In the build farm, we want erratic behaviour for
			 * async I/O times
			 */
			uint8_t randval;
			unsigned msecs;
			/*
			 * use generate_random_buffer, we just forked from a
			 * common parent state
			 */
			generate_random_buffer(&randval, sizeof(randval));
			msecs = randval + 20;
			DEBUG(10, ("delaying for %u msecs\n", msecs));
			smb_msleep(msecs);
		}
#endif


		ZERO_STRUCT(ret_struct);

		if (cmd_struct.read_cmd) {
			ret_struct.size = sys_pread(
				fd, (void *)map->ptr, cmd_struct.n,
				cmd_struct.offset);
#if 0
/* This breaks "make test" when run with aio_fork module. */
#ifdef ENABLE_BUILD_FARM_HACKS
			ret_struct.size = MAX(1, ret_struct.size * 0.9);
#endif
#endif
		}
		else {
			ret_struct.size = sys_pwrite(
				fd, (void *)map->ptr, cmd_struct.n,
				cmd_struct.offset);
		}

		DEBUG(10, ("aio_child_loop: syscall returned %d\n",
			   (int)ret_struct.size));

		if (ret_struct.size == -1) {
			ret_struct.ret_errno = errno;
		}

		/*
		 * Close the fd before telling our parent we're done. The
		 * parent might close and re-open the file very quickly, and
		 * with system-level share modes (GPFS) we would get an
		 * unjustified SHARING_VIOLATION.
		 */
		close(fd);

		ret = write_data(sockfd, (char *)&ret_struct,
				 sizeof(ret_struct));
		if (ret != sizeof(ret_struct)) {
			DEBUG(10, ("could not write ret_struct: %s\n",
				   strerror(errno)));
			exit(2);
		}
	}
}

static void handle_aio_completion(struct event_context *event_ctx,
				  struct fd_event *event, uint16 flags,
				  void *p)
{
	struct aio_extra *aio_ex = NULL;
	struct aio_child *child = (struct aio_child *)p;
	NTSTATUS status;

	DEBUG(10, ("handle_aio_completion called with flags=%d\n", flags));

	if ((flags & EVENT_FD_READ) == 0) {
		return;
	}

	status = read_data(child->sockfd, (char *)&child->retval,
			   sizeof(child->retval));

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("aio child %d died: %s\n", (int)child->pid,
			  nt_errstr(status)));
		child->retval.size = -1;
		child->retval.ret_errno = EIO;
	}

	if (child->aiocb == NULL) {
		DEBUG(1, ("Inactive child died\n"));
		TALLOC_FREE(child);
		return;
	}

	if (child->cancelled) {
		child->aiocb = NULL;
		child->cancelled = false;
		return;
	}

	if (child->read_cmd && (child->retval.size > 0)) {
		SMB_ASSERT(child->retval.size <= child->aiocb->aio_nbytes);
		memcpy((void *)child->aiocb->aio_buf, (void *)child->map->ptr,
		       child->retval.size);
	}

	if (child->called_from_suspend) {
		child->completion_done = true;
		return;
	}
	aio_ex = (struct aio_extra *)child->aiocb->aio_sigevent.sigev_value.sival_ptr;
	smbd_aio_complete_aio_ex(aio_ex);
	TALLOC_FREE(aio_ex);
}

static int aio_child_destructor(struct aio_child *child)
{
	char c=0;

	SMB_ASSERT((child->aiocb == NULL) || child->cancelled);

	DEBUG(10, ("aio_child_destructor: removing child %d on fd %d\n",
			child->pid, child->sockfd));

	/*
	 * closing the sockfd makes the child not return from recvmsg() on RHEL
	 * 5.5 so instead force the child to exit by writing bad data to it
	 */
	write(child->sockfd, &c, sizeof(c));
	close(child->sockfd);
	DLIST_REMOVE(child->list->children, child);
	return 0;
}

/*
 * We have to close all fd's in open files, we might incorrectly hold a system
 * level share mode on a file.
 */

static struct files_struct *close_fsp_fd(struct files_struct *fsp,
					 void *private_data)
{
	if ((fsp->fh != NULL) && (fsp->fh->fd != -1)) {
		close(fsp->fh->fd);
		fsp->fh->fd = -1;
	}
	return NULL;
}

static NTSTATUS create_aio_child(struct smbd_server_connection *sconn,
				 struct aio_child_list *children,
				 size_t map_size,
				 struct aio_child **presult)
{
	struct aio_child *result;
	int fdpair[2];
	NTSTATUS status;

	fdpair[0] = fdpair[1] = -1;

	result = TALLOC_ZERO_P(children, struct aio_child);
	NT_STATUS_HAVE_NO_MEMORY(result);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpair) == -1) {
		status = map_nt_error_from_unix(errno);
		DEBUG(10, ("socketpair() failed: %s\n", strerror(errno)));
		goto fail;
	}

	DEBUG(10, ("fdpair = %d/%d\n", fdpair[0], fdpair[1]));

	result->map = mmap_area_init(result, map_size);
	if (result->map == NULL) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0, ("Could not create mmap area\n"));
		goto fail;
	}

	result->pid = sys_fork();
	if (result->pid == -1) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0, ("fork failed: %s\n", strerror(errno)));
		goto fail;
	}

	if (result->pid == 0) {
		close(fdpair[0]);
		result->sockfd = fdpair[1];
		files_forall(sconn, close_fsp_fd, NULL);
		aio_child_loop(result->sockfd, result->map);
	}

	DEBUG(10, ("Child %d created with sockfd %d\n",
			result->pid, fdpair[0]));

	result->sockfd = fdpair[0];
	close(fdpair[1]);

	result->sock_event = event_add_fd(smbd_event_context(), result,
					  result->sockfd, EVENT_FD_READ,
					  handle_aio_completion,
					  result);
	if (result->sock_event == NULL) {
		status = NT_STATUS_NO_MEMORY;
		DEBUG(0, ("event_add_fd failed\n"));
		goto fail;
	}

	result->list = children;
	DLIST_ADD(children->children, result);

	talloc_set_destructor(result, aio_child_destructor);

	*presult = result;

	return NT_STATUS_OK;

 fail:
	if (fdpair[0] != -1) close(fdpair[0]);
	if (fdpair[1] != -1) close(fdpair[1]);
	TALLOC_FREE(result);

	return status;
}

static NTSTATUS get_idle_child(struct vfs_handle_struct *handle,
			       struct aio_child **pchild)
{
	struct aio_child_list *children;
	struct aio_child *child;
	NTSTATUS status;

	children = init_aio_children(handle);
	if (children == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (child = children->children; child != NULL; child = child->next) {
		if (child->aiocb == NULL) {
			/* idle */
			break;
		}
	}

	if (child == NULL) {
		DEBUG(10, ("no idle child found, creating new one\n"));

		status = create_aio_child(handle->conn->sconn, children,
					  128*1024, &child);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("create_aio_child failed: %s\n",
				   nt_errstr(status)));
			return status;
		}
	}

	child->dont_delete = true;

	*pchild = child;
	return NT_STATUS_OK;
}

static int aio_fork_read(struct vfs_handle_struct *handle,
			 struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child *child;
	struct rw_cmd cmd;
	ssize_t ret;
	NTSTATUS status;

	if (aiocb->aio_nbytes > 128*1024) {
		/* TODO: support variable buffers */
		errno = EINVAL;
		return -1;
	}

	status = get_idle_child(handle, &child);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not get an idle child\n"));
		return -1;
	}

	child->read_cmd = true;
	child->aiocb = aiocb;
	child->retval.ret_errno = EINPROGRESS;

	ZERO_STRUCT(cmd);
	cmd.n = aiocb->aio_nbytes;
	cmd.offset = aiocb->aio_offset;
	cmd.read_cmd = child->read_cmd;

	DEBUG(10, ("sending fd %d to child %d\n", fsp->fh->fd,
		   (int)child->pid));

	ret = write_fd(child->sockfd, &cmd, sizeof(cmd), fsp->fh->fd);
	if (ret == -1) {
		DEBUG(10, ("write_fd failed: %s\n", strerror(errno)));
		return -1;
	}

	return 0;
}

static int aio_fork_write(struct vfs_handle_struct *handle,
			  struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child *child;
	struct rw_cmd cmd;
	ssize_t ret;
	NTSTATUS status;

	if (aiocb->aio_nbytes > 128*1024) {
		/* TODO: support variable buffers */
		errno = EINVAL;
		return -1;
	}

	status = get_idle_child(handle, &child);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not get an idle child\n"));
		return -1;
	}

	child->read_cmd = false;
	child->aiocb = aiocb;
	child->retval.ret_errno = EINPROGRESS;

	memcpy((void *)child->map->ptr, (void *)aiocb->aio_buf,
	       aiocb->aio_nbytes);

	ZERO_STRUCT(cmd);
	cmd.n = aiocb->aio_nbytes;
	cmd.offset = aiocb->aio_offset;
	cmd.read_cmd = child->read_cmd;

	DEBUG(10, ("sending fd %d to child %d\n", fsp->fh->fd,
		   (int)child->pid));

	ret = write_fd(child->sockfd, &cmd, sizeof(cmd), fsp->fh->fd);
	if (ret == -1) {
		DEBUG(10, ("write_fd failed: %s\n", strerror(errno)));
		return -1;
	}

	return 0;
}

static struct aio_child *aio_fork_find_child(struct vfs_handle_struct *handle,
					     SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child_list *children;
	struct aio_child *child;

	children = init_aio_children(handle);
	if (children == NULL) {
		return NULL;
	}

	for (child = children->children; child != NULL; child = child->next) {
		if (child->aiocb == aiocb) {
			return child;
		}
	}

	return NULL;
}

static ssize_t aio_fork_return_fn(struct vfs_handle_struct *handle,
				  struct files_struct *fsp,
				  SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child *child = aio_fork_find_child(handle, aiocb);

	if (child == NULL) {
		errno = EINVAL;
		DEBUG(0, ("returning EINVAL\n"));
		return -1;
	}

	child->aiocb = NULL;

	if (child->retval.size == -1) {
		errno = child->retval.ret_errno;
	}

	return child->retval.size;
}

static int aio_fork_cancel(struct vfs_handle_struct *handle,
			   struct files_struct *fsp,
			   SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child_list *children;
	struct aio_child *child;

	children = init_aio_children(handle);
	if (children == NULL) {
		errno = EINVAL;
		return -1;
	}

	for (child = children->children; child != NULL; child = child->next) {
		if (child->aiocb == NULL) {
			continue;
		}
		if (child->aiocb->aio_fildes != fsp->fh->fd) {
			continue;
		}
		if ((aiocb != NULL) && (child->aiocb != aiocb)) {
			continue;
		}

		/*
		 * We let the child do its job, but we discard the result when
		 * it's finished.
		 */

		child->cancelled = true;
	}

	return AIO_CANCELED;
}

static int aio_fork_error_fn(struct vfs_handle_struct *handle,
			     struct files_struct *fsp,
			     SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_child *child = aio_fork_find_child(handle, aiocb);

	if (child == NULL) {
		errno = EINVAL;
		return -1;
	}

	return child->retval.ret_errno;
}

static void aio_fork_suspend_timed_out(struct tevent_context *event_ctx,
					struct tevent_timer *te,
					struct timeval now,
					void *private_data)
{
	bool *timed_out = (bool *)private_data;
	/* Remove this timed event handler. */
	TALLOC_FREE(te);
	*timed_out = true;
}

static int aio_fork_suspend(struct vfs_handle_struct *handle,
			struct files_struct *fsp,
			const SMB_STRUCT_AIOCB * const aiocb_array[],
			int n,
			const struct timespec *timeout)
{
	struct aio_child_list *children = NULL;
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	int i;
	int ret = -1;
	bool timed_out = false;

	children = init_aio_children(handle);
	if (children == NULL) {
		errno = EINVAL;
		goto out;
	}

	/* This is a blocking call, and has to use a sub-event loop. */
	ev = event_context_init(frame);
	if (ev == NULL) {
		errno = ENOMEM;
		goto out;
	}

	if (timeout) {
		struct timeval tv = convert_timespec_to_timeval(*timeout);
		struct tevent_timer *te = tevent_add_timer(ev,
						frame,
						timeval_current_ofs(tv.tv_sec,
								    tv.tv_usec),
						aio_fork_suspend_timed_out,
						&timed_out);
		if (!te) {
			errno = ENOMEM;
			goto out;
		}
	}

	for (i = 0; i < n; i++) {
		struct aio_child *child = NULL;
		const SMB_STRUCT_AIOCB *aiocb = aiocb_array[i];

		if (!aiocb) {
			continue;
		}

		/*
		 * We're going to cheat here. We know that smbd/aio.c
		 * only calls this when it's waiting for every single
		 * outstanding call to finish on a close, so just wait
		 * individually for each IO to complete. We don't care
		 * what order they finish - only that they all do. JRA.
		 */

		for (child = children->children; child != NULL; child = child->next) {
			struct tevent_fd *event;

			if (child->aiocb == NULL) {
				continue;
			}
			if (child->aiocb->aio_fildes != fsp->fh->fd) {
				continue;
			}
			if (child->aiocb != aiocb) {
				continue;
			}

			if (child->aiocb->aio_sigevent.sigev_value.sival_ptr == NULL) {
				continue;
			}

			event = event_add_fd(ev,
					     frame,
					     child->sockfd,
					     EVENT_FD_READ,
					     handle_aio_completion,
					     child);

			child->called_from_suspend = true;

			while (!child->completion_done) {
				if (tevent_loop_once(ev) == -1) {
					goto out;
				}

				if (timed_out) {
					errno = EAGAIN;
					goto out;
				}
			}
		}
	}

	ret = 0;

  out:

	TALLOC_FREE(frame);
	return ret;
}

static struct vfs_fn_pointers vfs_aio_fork_fns = {
	.aio_read = aio_fork_read,
	.aio_write = aio_fork_write,
	.aio_return_fn = aio_fork_return_fn,
	.aio_cancel = aio_fork_cancel,
	.aio_error_fn = aio_fork_error_fn,
	.aio_suspend = aio_fork_suspend,
};

NTSTATUS vfs_aio_fork_init(void);
NTSTATUS vfs_aio_fork_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"aio_fork", &vfs_aio_fork_fns);
}
