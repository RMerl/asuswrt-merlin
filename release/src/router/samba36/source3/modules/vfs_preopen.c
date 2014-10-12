/*
 * Force a readahead of files by opening them and reading the first bytes
 *
 * Copyright (C) Volker Lendecke 2008
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
#include "smbd/smbd.h"

struct preopen_state;

struct preopen_helper {
	struct preopen_state *state;
	struct fd_event *fde;
	pid_t pid;
	int fd;
	bool busy;
};

struct preopen_state {
	int num_helpers;
	struct preopen_helper *helpers;

	size_t to_read;		/* How many bytes to read in children? */
	int queue_max;

	char *template_fname;	/* Filename to be sent to children */
	size_t number_start;	/* start offset into "template_fname" */
	int num_digits;		/* How many digits is the number long? */

	int fnum_sent;		/* last fname sent to children */

	int fnum_queue_end;	/* last fname to be sent, based on
				 * last open call + preopen:queuelen
				 */

	name_compare_entry *preopen_names;
};

static void preopen_helper_destroy(struct preopen_helper *c)
{
	int status;
	close(c->fd);
	c->fd = -1;
	kill(c->pid, SIGKILL);
	waitpid(c->pid, &status, 0);
	c->busy = true;
}

static void preopen_queue_run(struct preopen_state *state)
{
	char *pdelimiter;
	char delimiter;

	pdelimiter = state->template_fname + state->number_start
		+ state->num_digits;
	delimiter = *pdelimiter;

	while (state->fnum_sent < state->fnum_queue_end) {

		ssize_t written;
		size_t to_write;
		int helper;

		for (helper=0; helper<state->num_helpers; helper++) {
			if (state->helpers[helper].busy) {
				continue;
			}
			break;
		}
		if (helper == state->num_helpers) {
			/* everyone is busy */
			return;
		}

		snprintf(state->template_fname + state->number_start,
			 state->num_digits + 1,
			 "%.*lu", state->num_digits,
			 (long unsigned int)(state->fnum_sent + 1));
		*pdelimiter = delimiter;

		to_write = talloc_get_size(state->template_fname);
		written = write_data(state->helpers[helper].fd,
				     state->template_fname, to_write);
		state->helpers[helper].busy = true;

		if (written != to_write) {
			preopen_helper_destroy(&state->helpers[helper]);
		}
		state->fnum_sent += 1;
	}
}

static void preopen_helper_readable(struct event_context *ev,
				    struct fd_event *fde, uint16_t flags,
				    void *priv)
{
	struct preopen_helper *helper = (struct preopen_helper *)priv;
	struct preopen_state *state = helper->state;
	ssize_t nread;
	char c;

	if ((flags & EVENT_FD_READ) == 0) {
		return;
	}

	nread = read(helper->fd, &c, 1);
	if (nread <= 0) {
		preopen_helper_destroy(helper);
		return;
	}

	helper->busy = false;

	preopen_queue_run(state);
}

static int preopen_helpers_destructor(struct preopen_state *c)
{
	int i;

	for (i=0; i<c->num_helpers; i++) {
		if (c->helpers[i].fd == -1) {
			continue;
		}
		preopen_helper_destroy(&c->helpers[i]);
	}

	return 0;
}

static bool preopen_helper_open_one(int sock_fd, char **pnamebuf,
				    size_t to_read, void *filebuf)
{
	char *namebuf = *pnamebuf;
	ssize_t nwritten, nread;
	char c = 0;
	int fd;

	nread = 0;

	while ((nread == 0) || (namebuf[nread-1] != '\0')) {
		ssize_t thistime;

		thistime = read(sock_fd, namebuf + nread,
				talloc_get_size(namebuf) - nread);
		if (thistime <= 0) {
			return false;
		}

		nread += thistime;

		if (nread == talloc_get_size(namebuf)) {
			namebuf = TALLOC_REALLOC_ARRAY(
				NULL, namebuf, char,
				talloc_get_size(namebuf) * 2);
			if (namebuf == NULL) {
				return false;
			}
			*pnamebuf = namebuf;
		}
	}

	fd = open(namebuf, O_RDONLY);
	if (fd == -1) {
		goto done;
	}
	nread = read(fd, filebuf, to_read);
	close(fd);

 done:
	nwritten = write(sock_fd, &c, 1);
	return true;
}

static bool preopen_helper(int fd, size_t to_read)
{
	char *namebuf;
	void *readbuf;

	namebuf = TALLOC_ARRAY(NULL, char, 1024);
	if (namebuf == NULL) {
		return false;
	}

	readbuf = talloc_size(NULL, to_read);
	if (readbuf == NULL) {
		TALLOC_FREE(namebuf);
		return false;
	}

	while (preopen_helper_open_one(fd, &namebuf, to_read, readbuf)) {
		;
	}

	TALLOC_FREE(readbuf);
	TALLOC_FREE(namebuf);
	return false;
}

static NTSTATUS preopen_init_helper(struct preopen_helper *h)
{
	int fdpair[2];
	NTSTATUS status;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpair) == -1) {
		status = map_nt_error_from_unix(errno);
		DEBUG(10, ("socketpair() failed: %s\n", strerror(errno)));
		return status;
	}

	h->pid = sys_fork();

	if (h->pid == -1) {
		return map_nt_error_from_unix(errno);
	}

	if (h->pid == 0) {
		close(fdpair[0]);
		preopen_helper(fdpair[1], h->state->to_read);
		exit(0);
	}
	close(fdpair[1]);
	h->fd = fdpair[0];
	h->fde = event_add_fd(smbd_event_context(), h->state, h->fd,
			      EVENT_FD_READ, preopen_helper_readable, h);
	if (h->fde == NULL) {
		close(h->fd);
		h->fd = -1;
		return NT_STATUS_NO_MEMORY;
	}
	h->busy = false;
	return NT_STATUS_OK;
}

static NTSTATUS preopen_init_helpers(TALLOC_CTX *mem_ctx, size_t to_read,
				     int num_helpers, int queue_max,
				     struct preopen_state **presult)
{
	struct preopen_state *result;
	int i;

	result = talloc(mem_ctx, struct preopen_state);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->num_helpers = num_helpers;
	result->helpers = TALLOC_ARRAY(result, struct preopen_helper,
				       num_helpers);
	if (result->helpers == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	result->to_read = to_read;
	result->queue_max = queue_max;
	result->template_fname = NULL;
	result->fnum_sent = 0;

	for (i=0; i<num_helpers; i++) {
		result->helpers[i].state = result;
		result->helpers[i].fd = -1;
	}

	talloc_set_destructor(result, preopen_helpers_destructor);

	for (i=0; i<num_helpers; i++) {
		preopen_init_helper(&result->helpers[i]);
	}

	*presult = result;
	return NT_STATUS_OK;
}

static void preopen_free_helpers(void **ptr)
{
	TALLOC_FREE(*ptr);
}

static struct preopen_state *preopen_state_get(vfs_handle_struct *handle)
{
	struct preopen_state *state;
	NTSTATUS status;
	const char *namelist;

	if (SMB_VFS_HANDLE_TEST_DATA(handle)) {
		SMB_VFS_HANDLE_GET_DATA(handle, state, struct preopen_state,
					return NULL);
		return state;
	}

	namelist = lp_parm_const_string(SNUM(handle->conn), "preopen", "names",
					NULL);

	if (namelist == NULL) {
		return NULL;
	}

	status = preopen_init_helpers(
		NULL,
		lp_parm_int(SNUM(handle->conn), "preopen", "num_bytes", 1),
		lp_parm_int(SNUM(handle->conn), "preopen", "helpers", 1),
		lp_parm_int(SNUM(handle->conn), "preopen", "queuelen", 10),
		&state);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	set_namearray(&state->preopen_names, (char *)namelist);

	if (state->preopen_names == NULL) {
		TALLOC_FREE(state);
		return NULL;
	}

	if (!SMB_VFS_HANDLE_TEST_DATA(handle)) {
		SMB_VFS_HANDLE_SET_DATA(handle, state, preopen_free_helpers,
					struct preopen_state, return NULL);
	}

	return state;
}

static bool preopen_parse_fname(const char *fname, unsigned long *pnum,
				size_t *pstart_idx, int *pnum_digits)
{
	const char *p, *q;
	unsigned long num;

	p = strrchr_m(fname, '/');
	if (p == NULL) {
		p = fname;
	}

	p += 1;
	while (p[0] != '\0') {
		if (isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2])) {
			break;
		}
		p += 1;
	}
	if (*p == '\0') {
		/* no digits around */
		return false;
	}

	num = strtoul(p, (char **)&q, 10);

	if (num+1 < num) {
		/* overflow */
		return false;
	}

	*pnum = num;
	*pstart_idx = (p - fname);
	*pnum_digits = (q - p);
	return true;
}

static int preopen_open(vfs_handle_struct *handle,
			struct smb_filename *smb_fname, files_struct *fsp,
			int flags, mode_t mode)
{
	struct preopen_state *state;
	int res;
	unsigned long num;

	DEBUG(10, ("preopen_open called on %s\n", smb_fname_str_dbg(smb_fname)));

	state = preopen_state_get(handle);
	if (state == NULL) {
		return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	}

	res = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	if (res == -1) {
		return -1;
	}

	if (flags != O_RDONLY) {
		return res;
	}

	if (!is_in_path(smb_fname->base_name, state->preopen_names, true)) {
		DEBUG(10, ("%s does not match the preopen:names list\n",
			   smb_fname_str_dbg(smb_fname)));
		return res;
	}

	TALLOC_FREE(state->template_fname);
	state->template_fname = talloc_asprintf(
		state, "%s/%s", fsp->conn->connectpath, smb_fname->base_name);

	if (state->template_fname == NULL) {
		return res;
	}

	if (!preopen_parse_fname(state->template_fname, &num,
				 &state->number_start, &state->num_digits)) {
		TALLOC_FREE(state->template_fname);
		return res;
	}

	if (num > state->fnum_sent) {
		/*
		 * Helpers were too slow, there's no point in reading
		 * files in helpers that we already read in the
		 * parent.
		 */
		state->fnum_sent = num;
	}

	if ((state->fnum_queue_end != 0) /* Something was started earlier */
	    && (num < (state->fnum_queue_end - state->queue_max))) {
		/*
		 * "num" is before the queue we announced. This means
		 * a new run is started.
		 */
		state->fnum_sent = num;
	}

	state->fnum_queue_end = num + state->queue_max;

	preopen_queue_run(state);

	return res;
}

static struct vfs_fn_pointers vfs_preopen_fns = {
	.open_fn = preopen_open
};

NTSTATUS vfs_preopen_init(void);
NTSTATUS vfs_preopen_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"preopen", &vfs_preopen_fns);
}
