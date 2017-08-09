/* 
   Unix SMB/CIFS implementation.
   client file operations
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jeremy Allison 2001-2009

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
#include "system/filesys.h"
#include "libsmb/libsmb.h"
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "libsmb/clirap.h"
#include "trans2.h"
#include "ntioctl.h"

/***********************************************************
 Common function for pushing stings, used by smb_bytes_push_str()
 and trans_bytes_push_str(). Only difference is the align_odd
 parameter setting.
***********************************************************/

static uint8_t *internal_bytes_push_str(uint8_t *buf, bool ucs2,
				const char *str, size_t str_len,
				bool align_odd,
				size_t *pconverted_size)
{
	size_t buflen;
	char *converted;
	size_t converted_size;

	if (buf == NULL) {
		return NULL;
	}

	buflen = talloc_get_size(buf);

	if (align_odd && ucs2 && (buflen % 2 == 0)) {
		/*
		 * We're pushing into an SMB buffer, align odd
		 */
		buf = TALLOC_REALLOC_ARRAY(NULL, buf, uint8_t, buflen + 1);
		if (buf == NULL) {
			return NULL;
		}
		buf[buflen] = '\0';
		buflen += 1;
	}

	if (!convert_string_talloc(talloc_tos(), CH_UNIX,
				   ucs2 ? CH_UTF16LE : CH_DOS,
				   str, str_len, &converted,
				   &converted_size, true)) {
		return NULL;
	}

	buf = TALLOC_REALLOC_ARRAY(NULL, buf, uint8_t,
				   buflen + converted_size);
	if (buf == NULL) {
		TALLOC_FREE(converted);
		return NULL;
	}

	memcpy(buf + buflen, converted, converted_size);

	TALLOC_FREE(converted);

	if (pconverted_size) {
		*pconverted_size = converted_size;
	}

	return buf;
}

/***********************************************************
 Push a string into an SMB buffer, with odd byte alignment
 if it's a UCS2 string.
***********************************************************/

uint8_t *smb_bytes_push_str(uint8_t *buf, bool ucs2,
			    const char *str, size_t str_len,
			    size_t *pconverted_size)
{
	return internal_bytes_push_str(buf, ucs2, str, str_len,
			true, pconverted_size);
}

uint8_t *smb_bytes_push_bytes(uint8_t *buf, uint8_t prefix,
			      const uint8_t *bytes, size_t num_bytes)
{
	size_t buflen;

	if (buf == NULL) {
		return NULL;
	}
	buflen = talloc_get_size(buf);

	buf = TALLOC_REALLOC_ARRAY(NULL, buf, uint8_t,
				   buflen + 1 + num_bytes);
	if (buf == NULL) {
		return NULL;
	}
	buf[buflen] = prefix;
	memcpy(&buf[buflen+1], bytes, num_bytes);
	return buf;
}

/***********************************************************
 Same as smb_bytes_push_str(), but without the odd byte
 align for ucs2 (we're pushing into a param or data block).
 static for now, although this will probably change when
 other modules use async trans calls.
***********************************************************/

static uint8_t *trans2_bytes_push_str(uint8_t *buf, bool ucs2,
			    const char *str, size_t str_len,
			    size_t *pconverted_size)
{
	return internal_bytes_push_str(buf, ucs2, str, str_len,
			false, pconverted_size);
}

struct cli_setpathinfo_state {
	uint16_t setup;
	uint8_t *param;
};

static void cli_setpathinfo_done(struct tevent_req *subreq);

struct tevent_req *cli_setpathinfo_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct cli_state *cli,
					uint16_t level,
					const char *path,
					uint8_t *data,
					size_t data_len)
{
	struct tevent_req *req, *subreq;
	struct cli_setpathinfo_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_setpathinfo_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup setup word. */
	SSVAL(&state->setup, 0, TRANSACT2_SETPATHINFO);

	/* Setup param array. */
	state->param = TALLOC_ZERO_ARRAY(state, uint8_t, 6);
	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}
	SSVAL(state->param, 0, level);

	state->param = trans2_bytes_push_str(
		state->param, cli_ucs2(cli), path, strlen(path)+1, NULL);
	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_trans_send(
		state,			/* mem ctx. */
		ev,			/* event ctx. */
		cli,			/* cli_state. */
		SMBtrans2,		/* cmd. */
		NULL,			/* pipe name. */
		-1,			/* fid. */
		0,			/* function. */
		0,			/* flags. */
		&state->setup,		/* setup. */
		1,			/* num setup uint16_t words. */
		0,			/* max returned setup. */
		state->param,		/* param. */
		talloc_get_size(state->param),	/* num param. */
		2,			/* max returned param. */
		data,			/* data. */
		data_len,		/* num data. */
		0);			/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_setpathinfo_done, req);
	return req;
}

static void cli_setpathinfo_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_trans_recv(subreq, NULL, NULL, NULL, 0, NULL,
					 NULL, 0, NULL, NULL, 0, NULL);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

NTSTATUS cli_setpathinfo_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_setpathinfo(struct cli_state *cli,
			 uint16_t level,
			 const char *path,
			 uint8_t *data,
			 size_t data_len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = tevent_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_setpathinfo_send(ev, ev, cli, level, path, data, data_len);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_setpathinfo_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Hard/Symlink a file (UNIX extensions).
 Creates new name (sym)linked to oldname.
****************************************************************************/

struct cli_posix_link_internal_state {
	uint8_t *data;
};

static void cli_posix_link_internal_done(struct tevent_req *subreq);

static struct tevent_req *cli_posix_link_internal_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t level,
					const char *oldname,
					const char *newname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_posix_link_internal_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_posix_link_internal_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup data array. */
	state->data = talloc_array(state, uint8_t, 0);
	if (tevent_req_nomem(state->data, req)) {
		return tevent_req_post(req, ev);
	}
	state->data = trans2_bytes_push_str(
		state->data, cli_ucs2(cli), oldname, strlen(oldname)+1, NULL);

	subreq = cli_setpathinfo_send(
		state, ev, cli, level, newname,
		state->data, talloc_get_size(state->data));
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_link_internal_done, req);
	return req;
}

static void cli_posix_link_internal_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_setpathinfo_recv(subreq);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

/****************************************************************************
 Symlink a file (UNIX extensions).
****************************************************************************/

struct tevent_req *cli_posix_symlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *oldname,
					const char *newname)
{
	return cli_posix_link_internal_send(
		mem_ctx, ev, cli, SMB_SET_FILE_UNIX_LINK, oldname, newname);
}

NTSTATUS cli_posix_symlink_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_symlink(struct cli_state *cli,
			const char *oldname,
			const char *newname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_symlink_send(frame,
				ev,
				cli,
				oldname,
				newname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_symlink_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Read a POSIX symlink.
****************************************************************************/

struct readlink_state {
	uint8_t *data;
	uint32_t num_data;
};

static void cli_posix_readlink_done(struct tevent_req *subreq);

struct tevent_req *cli_posix_readlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					size_t len)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct readlink_state *state = NULL;
	uint32_t maxbytelen = (uint32_t)(cli_ucs2(cli) ? len*3 : len);

	req = tevent_req_create(mem_ctx, &state, struct readlink_state);
	if (req == NULL) {
		return NULL;
	}

	/*
	 * Len is in bytes, we need it in UCS2 units.
	 */
	if ((2*len < len) || (maxbytelen < len)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_QUERY_FILE_UNIX_LINK, 1, maxbytelen);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_readlink_done, req);
	return req;
}

static void cli_posix_readlink_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct readlink_state *state = tevent_req_data(
		req, struct readlink_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	/*
	 * num_data is > 1, we've given 1 as minimum to cli_qpathinfo_send
	 */
	if (state->data[state->num_data-1] != '\0') {
		tevent_req_nterror(req, NT_STATUS_DATA_ERROR);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_posix_readlink_recv(struct tevent_req *req, struct cli_state *cli,
				char *retpath, size_t len)
{
	NTSTATUS status;
	char *converted = NULL;
	size_t converted_size = 0;
	struct readlink_state *state = tevent_req_data(req, struct readlink_state);

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	/* The returned data is a pushed string, not raw data. */
	if (!convert_string_talloc(state,
				cli_ucs2(cli) ? CH_UTF16LE : CH_DOS, 
				CH_UNIX,
				state->data,
				state->num_data,
				&converted,
				&converted_size,
				true)) {
		return NT_STATUS_NO_MEMORY;
	}

	len = MIN(len,converted_size);
	if (len == 0) {
		return NT_STATUS_DATA_ERROR;
	}
	memcpy(retpath, converted, len);
	return NT_STATUS_OK;
}

NTSTATUS cli_posix_readlink(struct cli_state *cli, const char *fname,
				char *linkpath, size_t len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_readlink_send(frame,
				ev,
				cli,
				fname,
				len);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_readlink_recv(req, cli, linkpath, len);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Hard link a file (UNIX extensions).
****************************************************************************/

struct tevent_req *cli_posix_hardlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *oldname,
					const char *newname)
{
	return cli_posix_link_internal_send(
		mem_ctx, ev, cli, SMB_SET_FILE_UNIX_HLINK, oldname, newname);
}

NTSTATUS cli_posix_hardlink_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_hardlink(struct cli_state *cli,
			const char *oldname,
			const char *newname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_hardlink_send(frame,
				ev,
				cli,
				oldname,
				newname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_hardlink_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Map standard UNIX permissions onto wire representations.
****************************************************************************/

uint32_t unix_perms_to_wire(mode_t perms)
{
        unsigned int ret = 0;

        ret |= ((perms & S_IXOTH) ?  UNIX_X_OTH : 0);
        ret |= ((perms & S_IWOTH) ?  UNIX_W_OTH : 0);
        ret |= ((perms & S_IROTH) ?  UNIX_R_OTH : 0);
        ret |= ((perms & S_IXGRP) ?  UNIX_X_GRP : 0);
        ret |= ((perms & S_IWGRP) ?  UNIX_W_GRP : 0);
        ret |= ((perms & S_IRGRP) ?  UNIX_R_GRP : 0);
        ret |= ((perms & S_IXUSR) ?  UNIX_X_USR : 0);
        ret |= ((perms & S_IWUSR) ?  UNIX_W_USR : 0);
        ret |= ((perms & S_IRUSR) ?  UNIX_R_USR : 0);
#ifdef S_ISVTX
        ret |= ((perms & S_ISVTX) ?  UNIX_STICKY : 0);
#endif
#ifdef S_ISGID
        ret |= ((perms & S_ISGID) ?  UNIX_SET_GID : 0);
#endif
#ifdef S_ISUID
        ret |= ((perms & S_ISUID) ?  UNIX_SET_UID : 0);
#endif
        return ret;
}

/****************************************************************************
 Map wire permissions to standard UNIX.
****************************************************************************/

mode_t wire_perms_to_unix(uint32_t perms)
{
        mode_t ret = (mode_t)0;

        ret |= ((perms & UNIX_X_OTH) ? S_IXOTH : 0);
        ret |= ((perms & UNIX_W_OTH) ? S_IWOTH : 0);
        ret |= ((perms & UNIX_R_OTH) ? S_IROTH : 0);
        ret |= ((perms & UNIX_X_GRP) ? S_IXGRP : 0);
        ret |= ((perms & UNIX_W_GRP) ? S_IWGRP : 0);
        ret |= ((perms & UNIX_R_GRP) ? S_IRGRP : 0);
        ret |= ((perms & UNIX_X_USR) ? S_IXUSR : 0);
        ret |= ((perms & UNIX_W_USR) ? S_IWUSR : 0);
        ret |= ((perms & UNIX_R_USR) ? S_IRUSR : 0);
#ifdef S_ISVTX
        ret |= ((perms & UNIX_STICKY) ? S_ISVTX : 0);
#endif
#ifdef S_ISGID
        ret |= ((perms & UNIX_SET_GID) ? S_ISGID : 0);
#endif
#ifdef S_ISUID
        ret |= ((perms & UNIX_SET_UID) ? S_ISUID : 0);
#endif
        return ret;
}

/****************************************************************************
 Return the file type from the wire filetype for UNIX extensions.
****************************************************************************/

static mode_t unix_filetype_from_wire(uint32_t wire_type)
{
	switch (wire_type) {
		case UNIX_TYPE_FILE:
			return S_IFREG;
		case UNIX_TYPE_DIR:
			return S_IFDIR;
#ifdef S_IFLNK
		case UNIX_TYPE_SYMLINK:
			return S_IFLNK;
#endif
#ifdef S_IFCHR
		case UNIX_TYPE_CHARDEV:
			return S_IFCHR;
#endif
#ifdef S_IFBLK
		case UNIX_TYPE_BLKDEV:
			return S_IFBLK;
#endif
#ifdef S_IFIFO
		case UNIX_TYPE_FIFO:
			return S_IFIFO;
#endif
#ifdef S_IFSOCK
		case UNIX_TYPE_SOCKET:
			return S_IFSOCK;
#endif
		default:
			return (mode_t)0;
	}
}

/****************************************************************************
 Do a POSIX getfacl (UNIX extensions).
****************************************************************************/

struct getfacl_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_posix_getfacl_done(struct tevent_req *subreq);

struct tevent_req *cli_posix_getfacl_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct getfacl_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct getfacl_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname, SMB_QUERY_POSIX_ACL,
				    0, cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_getfacl_done, req);
	return req;
}

static void cli_posix_getfacl_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct getfacl_state *state = tevent_req_data(
		req, struct getfacl_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_posix_getfacl_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				size_t *prb_size,
				char **retbuf)
{
	struct getfacl_state *state = tevent_req_data(req, struct getfacl_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*prb_size = (size_t)state->num_data;
	*retbuf = (char *)talloc_move(mem_ctx, &state->data);
	return NT_STATUS_OK;
}

NTSTATUS cli_posix_getfacl(struct cli_state *cli,
			const char *fname,
			TALLOC_CTX *mem_ctx,
			size_t *prb_size,
			char **retbuf)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_getfacl_send(frame,
				ev,
				cli,
				fname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_getfacl_recv(req, mem_ctx, prb_size, retbuf);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Stat a file (UNIX extensions).
****************************************************************************/

struct stat_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_posix_stat_done(struct tevent_req *subreq);

struct tevent_req *cli_posix_stat_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct stat_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct stat_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_QUERY_FILE_UNIX_BASIC, 100, 100);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_stat_done, req);
	return req;
}

static void cli_posix_stat_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	struct stat_state *state = tevent_req_data(req, struct stat_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_posix_stat_recv(struct tevent_req *req,
				SMB_STRUCT_STAT *sbuf)
{
	struct stat_state *state = tevent_req_data(req, struct stat_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	sbuf->st_ex_size = IVAL2_TO_SMB_BIG_UINT(state->data,0);     /* total size, in bytes */
	sbuf->st_ex_blocks = IVAL2_TO_SMB_BIG_UINT(state->data,8);   /* number of blocks allocated */
#if defined (HAVE_STAT_ST_BLOCKS) && defined(STAT_ST_BLOCKSIZE)
	sbuf->st_ex_blocks /= STAT_ST_BLOCKSIZE;
#else
	/* assume 512 byte blocks */
	sbuf->st_ex_blocks /= 512;
#endif
	sbuf->st_ex_ctime = interpret_long_date((char *)(state->data + 16));    /* time of last change */
	sbuf->st_ex_atime = interpret_long_date((char *)(state->data + 24));    /* time of last access */
	sbuf->st_ex_mtime = interpret_long_date((char *)(state->data + 32));    /* time of last modification */

	sbuf->st_ex_uid = (uid_t) IVAL(state->data,40);      /* user ID of owner */
	sbuf->st_ex_gid = (gid_t) IVAL(state->data,48);      /* group ID of owner */
	sbuf->st_ex_mode = unix_filetype_from_wire(IVAL(state->data, 56));
#if defined(HAVE_MAKEDEV)
	{
		uint32_t dev_major = IVAL(state->data,60);
		uint32_t dev_minor = IVAL(state->data,68);
		sbuf->st_ex_rdev = makedev(dev_major, dev_minor);
	}
#endif
	sbuf->st_ex_ino = (SMB_INO_T)IVAL2_TO_SMB_BIG_UINT(state->data,76);      /* inode */
	sbuf->st_ex_mode |= wire_perms_to_unix(IVAL(state->data,84));     /* protection */
	sbuf->st_ex_nlink = BIG_UINT(state->data,92); /* number of hard links */

	return NT_STATUS_OK;
}

NTSTATUS cli_posix_stat(struct cli_state *cli,
			const char *fname,
			SMB_STRUCT_STAT *sbuf)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_stat_send(frame,
				ev,
				cli,
				fname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_stat_recv(req, sbuf);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Chmod or chown a file internal (UNIX extensions).
****************************************************************************/

struct cli_posix_chown_chmod_internal_state {
	uint8_t data[100];
};

static void cli_posix_chown_chmod_internal_done(struct tevent_req *subreq);

static struct tevent_req *cli_posix_chown_chmod_internal_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					uint32_t mode,
					uint32_t uid,
					uint32_t gid)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_posix_chown_chmod_internal_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_posix_chown_chmod_internal_state);
	if (req == NULL) {
		return NULL;
	}

	memset(state->data, 0xff, 40); /* Set all sizes/times to no change. */
	memset(&state->data[40], '\0', 60);
	SIVAL(state->data,40,uid);
	SIVAL(state->data,48,gid);
	SIVAL(state->data,84,mode);

	subreq = cli_setpathinfo_send(state, ev, cli, SMB_SET_FILE_UNIX_BASIC,
				      fname, state->data, sizeof(state->data));
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_chown_chmod_internal_done,
				req);
	return req;
}

static void cli_posix_chown_chmod_internal_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_setpathinfo_recv(subreq);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

/****************************************************************************
 chmod a file (UNIX extensions).
****************************************************************************/

struct tevent_req *cli_posix_chmod_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					mode_t mode)
{
	return cli_posix_chown_chmod_internal_send(mem_ctx, ev, cli,
			fname,
			unix_perms_to_wire(mode),
			SMB_UID_NO_CHANGE,
			SMB_GID_NO_CHANGE);
}

NTSTATUS cli_posix_chmod_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_chmod(struct cli_state *cli, const char *fname, mode_t mode)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_chmod_send(frame,
				ev,
				cli,
				fname,
				mode);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_chmod_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 chown a file (UNIX extensions).
****************************************************************************/

struct tevent_req *cli_posix_chown_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					uid_t uid,
					gid_t gid)
{
	return cli_posix_chown_chmod_internal_send(mem_ctx, ev, cli,
			fname,
			SMB_MODE_NO_CHANGE,
			(uint32_t)uid,
			(uint32_t)gid);
}

NTSTATUS cli_posix_chown_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_chown(struct cli_state *cli,
			const char *fname,
			uid_t uid,
			gid_t gid)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_chown_send(frame,
				ev,
				cli,
				fname,
				uid,
				gid);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_chown_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Rename a file.
****************************************************************************/

static void cli_rename_done(struct tevent_req *subreq);

struct cli_rename_state {
	uint16_t vwv[1];
};

struct tevent_req *cli_rename_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname_src,
				const char *fname_dst)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_rename_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_rename_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname_src,
				   strlen(fname_src)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	bytes = TALLOC_REALLOC_ARRAY(state, bytes, uint8_t,
			talloc_get_size(bytes)+1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	bytes[talloc_get_size(bytes)-1] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname_dst,
				   strlen(fname_dst)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBmv, additional_flags,
			      1, state->vwv, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_rename_done, req);
	return req;
}

static void cli_rename_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_rename_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_rename(struct cli_state *cli, const char *fname_src, const char *fname_dst)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_rename_send(frame, ev, cli, fname_src, fname_dst);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_rename_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 NT Rename a file.
****************************************************************************/

static void cli_ntrename_internal_done(struct tevent_req *subreq);

struct cli_ntrename_internal_state {
	uint16_t vwv[4];
};

static struct tevent_req *cli_ntrename_internal_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname_src,
				const char *fname_dst,
				uint16_t rename_flag)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_ntrename_internal_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_ntrename_internal_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0 ,FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);
	SSVAL(state->vwv+1, 0, rename_flag);

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname_src,
				   strlen(fname_src)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	bytes = TALLOC_REALLOC_ARRAY(state, bytes, uint8_t,
			talloc_get_size(bytes)+1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	bytes[talloc_get_size(bytes)-1] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname_dst,
				   strlen(fname_dst)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBntrename, additional_flags,
			      4, state->vwv, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_ntrename_internal_done, req);
	return req;
}

static void cli_ntrename_internal_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS cli_ntrename_internal_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

struct tevent_req *cli_ntrename_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname_src,
				const char *fname_dst)
{
	return cli_ntrename_internal_send(mem_ctx,
					  ev,
					  cli,
					  fname_src,
					  fname_dst,
					  RENAME_FLAG_RENAME);
}

NTSTATUS cli_ntrename_recv(struct tevent_req *req)
{
	return cli_ntrename_internal_recv(req);
}

NTSTATUS cli_ntrename(struct cli_state *cli, const char *fname_src, const char *fname_dst)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_ntrename_send(frame, ev, cli, fname_src, fname_dst);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_ntrename_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 NT hardlink a file.
****************************************************************************/

struct tevent_req *cli_nt_hardlink_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname_src,
				const char *fname_dst)
{
	return cli_ntrename_internal_send(mem_ctx,
					  ev,
					  cli,
					  fname_src,
					  fname_dst,
					  RENAME_FLAG_HARD_LINK);
}

NTSTATUS cli_nt_hardlink_recv(struct tevent_req *req)
{
	return cli_ntrename_internal_recv(req);
}

NTSTATUS cli_nt_hardlink(struct cli_state *cli, const char *fname_src, const char *fname_dst)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_nt_hardlink_send(frame, ev, cli, fname_src, fname_dst);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_nt_hardlink_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Delete a file.
****************************************************************************/

static void cli_unlink_done(struct tevent_req *subreq);

struct cli_unlink_state {
	uint16_t vwv[1];
};

struct tevent_req *cli_unlink_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname,
				uint16_t mayhave_attrs)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_unlink_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_unlink_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0, mayhave_attrs);

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBunlink, additional_flags,
				1, state->vwv, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_unlink_done, req);
	return req;
}

static void cli_unlink_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_unlink_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_unlink(struct cli_state *cli, const char *fname, uint16_t mayhave_attrs)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_unlink_send(frame, ev, cli, fname, mayhave_attrs);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_unlink_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Create a directory.
****************************************************************************/

static void cli_mkdir_done(struct tevent_req *subreq);

struct cli_mkdir_state {
	int dummy;
};

struct tevent_req *cli_mkdir_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *dname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_mkdir_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_mkdir_state);
	if (req == NULL) {
		return NULL;
	}

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), dname,
				   strlen(dname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBmkdir, additional_flags,
			      0, NULL, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_mkdir_done, req);
	return req;
}

static void cli_mkdir_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_mkdir_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_mkdir(struct cli_state *cli, const char *dname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_mkdir_send(frame, ev, cli, dname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_mkdir_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Remove a directory.
****************************************************************************/

static void cli_rmdir_done(struct tevent_req *subreq);

struct cli_rmdir_state {
	int dummy;
};

struct tevent_req *cli_rmdir_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *dname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_rmdir_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_rmdir_state);
	if (req == NULL) {
		return NULL;
	}

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), dname,
				   strlen(dname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBrmdir, additional_flags,
			      0, NULL, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_rmdir_done, req);
	return req;
}

static void cli_rmdir_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_rmdir_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_rmdir(struct cli_state *cli, const char *dname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_rmdir_send(frame, ev, cli, dname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_rmdir_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Set or clear the delete on close flag.
****************************************************************************/

struct doc_state {
	uint16_t setup;
	uint8_t param[6];
	uint8_t data[1];
};

static void cli_nt_delete_on_close_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_trans_recv(subreq, NULL, NULL, NULL, 0, NULL,
					 NULL, 0, NULL, NULL, 0, NULL);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

struct tevent_req *cli_nt_delete_on_close_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					bool flag)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct doc_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct doc_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup setup word. */
	SSVAL(&state->setup, 0, TRANSACT2_SETFILEINFO);

	/* Setup param array. */
	SSVAL(state->param,0,fnum);
	SSVAL(state->param,2,SMB_SET_FILE_DISPOSITION_INFO);

	/* Setup data array. */
	SCVAL(&state->data[0], 0, flag ? 1 : 0);

	subreq = cli_trans_send(state,			/* mem ctx. */
				ev,			/* event ctx. */
				cli,			/* cli_state. */
				SMBtrans2,		/* cmd. */
				NULL,			/* pipe name. */
				-1,			/* fid. */
				0,			/* function. */
				0,			/* flags. */
				&state->setup,		/* setup. */
				1,			/* num setup uint16_t words. */
				0,			/* max returned setup. */
				state->param,		/* param. */
				6,			/* num param. */
				2,			/* max returned param. */
				state->data,		/* data. */
				1,			/* num data. */
				0);			/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_nt_delete_on_close_done, req);
	return req;
}

NTSTATUS cli_nt_delete_on_close_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_nt_delete_on_close(struct cli_state *cli, uint16_t fnum, bool flag)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_nt_delete_on_close_send(frame,
				ev,
				cli,
				fnum,
				flag);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_nt_delete_on_close_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_ntcreate_state {
	uint16_t vwv[24];
	uint16_t fnum;
};

static void cli_ntcreate_done(struct tevent_req *subreq);

struct tevent_req *cli_ntcreate_send(TALLOC_CTX *mem_ctx,
				     struct event_context *ev,
				     struct cli_state *cli,
				     const char *fname,
				     uint32_t CreatFlags,
				     uint32_t DesiredAccess,
				     uint32_t FileAttributes,
				     uint32_t ShareAccess,
				     uint32_t CreateDisposition,
				     uint32_t CreateOptions,
				     uint8_t SecurityFlags)
{
	struct tevent_req *req, *subreq;
	struct cli_ntcreate_state *state;
	uint16_t *vwv;
	uint8_t *bytes;
	size_t converted_len;

	req = tevent_req_create(mem_ctx, &state, struct cli_ntcreate_state);
	if (req == NULL) {
		return NULL;
	}

	vwv = state->vwv;

	SCVAL(vwv+0, 0, 0xFF);
	SCVAL(vwv+0, 1, 0);
	SSVAL(vwv+1, 0, 0);
	SCVAL(vwv+2, 0, 0);

	if (cli->use_oplocks) {
		CreatFlags |= (REQUEST_OPLOCK|REQUEST_BATCH_OPLOCK);
	}
	SIVAL(vwv+3, 1, CreatFlags);
	SIVAL(vwv+5, 1, 0x0);	/* RootDirectoryFid */
	SIVAL(vwv+7, 1, DesiredAccess);
	SIVAL(vwv+9, 1, 0x0);	/* AllocationSize */
	SIVAL(vwv+11, 1, 0x0);	/* AllocationSize */
	SIVAL(vwv+13, 1, FileAttributes);
	SIVAL(vwv+15, 1, ShareAccess);
	SIVAL(vwv+17, 1, CreateDisposition);
	SIVAL(vwv+19, 1, CreateOptions);
	SIVAL(vwv+21, 1, 0x02);	/* ImpersonationLevel */
	SCVAL(vwv+23, 1, SecurityFlags);

	bytes = talloc_array(state, uint8_t, 0);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli),
				   fname, strlen(fname)+1,
				   &converted_len);

	/* sigh. this copes with broken netapp filer behaviour */
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "", 1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	SSVAL(vwv+2, 1, converted_len);

	subreq = cli_smb_send(state, ev, cli, SMBntcreateX, 0, 24, vwv,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_ntcreate_done, req);
	return req;
}

static void cli_ntcreate_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_ntcreate_state *state = tevent_req_data(
		req, struct cli_ntcreate_state);
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 3, &wct, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	state->fnum = SVAL(vwv+2, 1);
	tevent_req_done(req);
}

NTSTATUS cli_ntcreate_recv(struct tevent_req *req, uint16_t *pfnum)
{
	struct cli_ntcreate_state *state = tevent_req_data(
		req, struct cli_ntcreate_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfnum = state->fnum;
	return NT_STATUS_OK;
}

NTSTATUS cli_ntcreate(struct cli_state *cli,
		      const char *fname,
		      uint32_t CreatFlags,
		      uint32_t DesiredAccess,
		      uint32_t FileAttributes,
		      uint32_t ShareAccess,
		      uint32_t CreateDisposition,
		      uint32_t CreateOptions,
		      uint8_t SecurityFlags,
		      uint16_t *pfid)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_ntcreate_send(frame, ev, cli, fname, CreatFlags,
				DesiredAccess, FileAttributes, ShareAccess,
				CreateDisposition, CreateOptions,
				SecurityFlags);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_ntcreate_recv(req, pfid);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Open a file
 WARNING: if you open with O_WRONLY then getattrE won't work!
****************************************************************************/

struct cli_open_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	const char *fname;
	uint16_t vwv[15];
	uint16_t fnum;
	unsigned openfn;
	unsigned dos_deny;
	uint8_t additional_flags;
	struct iovec bytes;
};

static void cli_open_done(struct tevent_req *subreq);
static void cli_open_ntcreate_done(struct tevent_req *subreq);

struct tevent_req *cli_open_create(TALLOC_CTX *mem_ctx,
				   struct event_context *ev,
				   struct cli_state *cli, const char *fname,
				   int flags, int share_mode,
				   struct tevent_req **psmbreq)
{
	struct tevent_req *req, *subreq;
	struct cli_open_state *state;
	uint8_t *bytes;

	req = tevent_req_create(mem_ctx, &state, struct cli_open_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->fname = fname;

	if (flags & O_CREAT) {
		state->openfn |= (1<<4);
	}
	if (!(flags & O_EXCL)) {
		if (flags & O_TRUNC)
			state->openfn |= (1<<1);
		else
			state->openfn |= (1<<0);
	}

	state->dos_deny = (share_mode<<4);

	if ((flags & O_ACCMODE) == O_RDWR) {
		state->dos_deny |= 2;
	} else if ((flags & O_ACCMODE) == O_WRONLY) {
		state->dos_deny |= 1;
	}

#if defined(O_SYNC)
	if ((flags & O_SYNC) == O_SYNC) {
		state->dos_deny |= (1<<14);
	}
#endif /* O_SYNC */

	if (share_mode == DENY_FCB) {
		state->dos_deny = 0xFF;
	}

	SCVAL(state->vwv + 0, 0, 0xFF);
	SCVAL(state->vwv + 0, 1, 0);
	SSVAL(state->vwv + 1, 0, 0);
	SSVAL(state->vwv + 2, 0, 0);  /* no additional info */
	SSVAL(state->vwv + 3, 0, state->dos_deny);
	SSVAL(state->vwv + 4, 0, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	SSVAL(state->vwv + 5, 0, 0);
	SIVAL(state->vwv + 6, 0, 0);
	SSVAL(state->vwv + 8, 0, state->openfn);
	SIVAL(state->vwv + 9, 0, 0);
	SIVAL(state->vwv + 11, 0, 0);
	SIVAL(state->vwv + 13, 0, 0);

	if (cli->use_oplocks) {
		/* if using oplocks then ask for a batch oplock via
                   core and extended methods */
		state->additional_flags =
			FLAG_REQUEST_OPLOCK|FLAG_REQUEST_BATCH_OPLOCK;
		SSVAL(state->vwv+2, 0, SVAL(state->vwv+2, 0) | 6);
	}

	bytes = talloc_array(state, uint8_t, 0);
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	state->bytes.iov_base = (void *)bytes;
	state->bytes.iov_len = talloc_get_size(bytes);

	subreq = cli_smb_req_create(state, ev, cli, SMBopenX,
				    state->additional_flags,
				    15, state->vwv, 1, &state->bytes);
	if (subreq == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	tevent_req_set_callback(subreq, cli_open_done, req);
	*psmbreq = subreq;
	return req;
}

struct tevent_req *cli_open_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli, const char *fname,
				 int flags, int share_mode)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_open_create(mem_ctx, ev, cli, fname, flags, share_mode,
			      &subreq);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(subreq);
	if (tevent_req_nterror(req, status)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_open_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_open_state *state = tevent_req_data(
		req, struct cli_open_state);
	uint8_t wct;
	uint16_t *vwv;
	uint8_t *inbuf;
	NTSTATUS status;
	uint32_t access_mask, share_mode, create_disposition, create_options;

	status = cli_smb_recv(subreq, state, &inbuf, 3, &wct, &vwv, NULL,
			      NULL);
	TALLOC_FREE(subreq);

	if (NT_STATUS_IS_OK(status)) {
		state->fnum = SVAL(vwv+2, 0);
		tevent_req_done(req);
		return;
	}

	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
		tevent_req_nterror(req, status);
		return;
	}

	/*
	 * For the new shiny OS/X Lion SMB server, try a ntcreate
	 * fallback.
	 */

	if (!map_open_params_to_ntcreate(state->fname, state->dos_deny,
					 state->openfn, &access_mask,
					 &share_mode, &create_disposition,
					 &create_options, NULL)) {
		tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return;
	}

	subreq = cli_ntcreate_send(state, state->ev, state->cli,
				   state->fname, 0, access_mask,
				   0, share_mode, create_disposition,
				   create_options, 0);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_open_ntcreate_done, req);
}

static void cli_open_ntcreate_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_open_state *state = tevent_req_data(
		req, struct cli_open_state);
	NTSTATUS status;

	status = cli_ntcreate_recv(subreq, &state->fnum);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_open_recv(struct tevent_req *req, uint16_t *pfnum)
{
	struct cli_open_state *state = tevent_req_data(
		req, struct cli_open_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfnum = state->fnum;
	return NT_STATUS_OK;
}

NTSTATUS cli_open(struct cli_state *cli, const char *fname, int flags,
	     int share_mode, uint16_t *pfnum)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_open_send(frame, ev, cli, fname, flags, share_mode);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_open_recv(req, pfnum);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Close a file.
****************************************************************************/

struct cli_close_state {
	uint16_t vwv[3];
};

static void cli_close_done(struct tevent_req *subreq);

struct tevent_req *cli_close_create(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum,
				struct tevent_req **psubreq)
{
	struct tevent_req *req, *subreq;
	struct cli_close_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_close_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0, fnum);
	SIVALS(state->vwv+1, 0, -1);

	subreq = cli_smb_req_create(state, ev, cli, SMBclose, 0, 3, state->vwv,
				    0, NULL);
	if (subreq == NULL) {
		TALLOC_FREE(req);
		return NULL;
	}
	tevent_req_set_callback(subreq, cli_close_done, req);
	*psubreq = subreq;
	return req;
}

struct tevent_req *cli_close_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum)
{
	struct tevent_req *req, *subreq;
	NTSTATUS status;

	req = cli_close_create(mem_ctx, ev, cli, fnum, &subreq);
	if (req == NULL) {
		return NULL;
	}

	status = cli_smb_req_send(subreq);
	if (tevent_req_nterror(req, status)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void cli_close_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_close_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_close(struct cli_state *cli, uint16_t fnum)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_close_send(frame, ev, cli, fnum);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_close_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Truncate a file to a specified size
****************************************************************************/

struct ftrunc_state {
	uint16_t setup;
	uint8_t param[6];
	uint8_t data[8];
};

static void cli_ftruncate_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_trans_recv(subreq, NULL, NULL, NULL, 0, NULL,
					 NULL, 0, NULL, NULL, 0, NULL);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

struct tevent_req *cli_ftruncate_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					uint64_t size)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct ftrunc_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct ftrunc_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup setup word. */
	SSVAL(&state->setup, 0, TRANSACT2_SETFILEINFO);

	/* Setup param array. */
	SSVAL(state->param,0,fnum);
	SSVAL(state->param,2,SMB_SET_FILE_END_OF_FILE_INFO);
	SSVAL(state->param,4,0);

	/* Setup data array. */
        SBVAL(state->data, 0, size);

	subreq = cli_trans_send(state,			/* mem ctx. */
				ev,			/* event ctx. */
				cli,			/* cli_state. */
				SMBtrans2,		/* cmd. */
				NULL,			/* pipe name. */
				-1,			/* fid. */
				0,			/* function. */
				0,			/* flags. */
				&state->setup,		/* setup. */
				1,			/* num setup uint16_t words. */
				0,			/* max returned setup. */
				state->param,		/* param. */
				6,			/* num param. */
				2,			/* max returned param. */
				state->data,		/* data. */
				8,			/* num data. */
				0);			/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_ftruncate_done, req);
	return req;
}

NTSTATUS cli_ftruncate_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_ftruncate(struct cli_state *cli, uint16_t fnum, uint64_t size)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_ftruncate_send(frame,
				ev,
				cli,
				fnum,
				size);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_ftruncate_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 send a lock with a specified locktype
 this is used for testing LOCKING_ANDX_CANCEL_LOCK
****************************************************************************/

NTSTATUS cli_locktype(struct cli_state *cli, uint16_t fnum,
		      uint32_t offset, uint32_t len,
		      int timeout, unsigned char locktype)
{
	uint16_t vwv[8];
	uint8_t bytes[10];
	NTSTATUS status;
	int saved_timeout;

	SCVAL(vwv + 0, 0, 0xff);
	SCVAL(vwv + 0, 1, 0);
	SSVAL(vwv + 1, 0, 0);
	SSVAL(vwv + 2, 0, fnum);
	SCVAL(vwv + 3, 0, locktype);
	SCVAL(vwv + 3, 1, 0);
	SIVALS(vwv + 4, 0, timeout);
	SSVAL(vwv + 6, 0, 0);
	SSVAL(vwv + 7, 0, 1);

	SSVAL(bytes, 0, cli->pid);
	SIVAL(bytes, 2, offset);
	SIVAL(bytes, 6, len);

	saved_timeout = cli->timeout;

	if (timeout != 0) {
		cli->timeout = (timeout == -1)
			? 0x7FFFFFFF : (timeout + 2*1000);
	}

	status = cli_smb(talloc_tos(), cli, SMBlockingX, 0, 8, vwv,
			 10, bytes, NULL, 0, NULL, NULL, NULL, NULL);

	cli->timeout = saved_timeout;

	return status;
}

/****************************************************************************
 Lock a file.
 note that timeout is in units of 2 milliseconds
****************************************************************************/

bool cli_lock(struct cli_state *cli, uint16_t fnum,
		  uint32_t offset, uint32_t len, int timeout,
		  enum brl_type lock_type)
{
	NTSTATUS status;

	status = cli_locktype(cli, fnum, offset, len, timeout,
			      (lock_type == READ_LOCK? 1 : 0));
	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
 Unlock a file.
****************************************************************************/

struct cli_unlock_state {
	uint16_t vwv[8];
	uint8_t data[10];
};

static void cli_unlock_done(struct tevent_req *subreq);

struct tevent_req *cli_unlock_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum,
				uint64_t offset,
				uint64_t len)

{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_unlock_state *state = NULL;
	uint8_t additional_flags = 0;

	req = tevent_req_create(mem_ctx, &state, struct cli_unlock_state);
	if (req == NULL) {
		return NULL;
	}

	SCVAL(state->vwv+0, 0, 0xFF);
	SSVAL(state->vwv+2, 0, fnum);
	SCVAL(state->vwv+3, 0, 0);
	SIVALS(state->vwv+4, 0, 0);
	SSVAL(state->vwv+6, 0, 1);
	SSVAL(state->vwv+7, 0, 0);

	SSVAL(state->data, 0, cli->pid);
	SIVAL(state->data, 2, offset);
	SIVAL(state->data, 6, len);

	subreq = cli_smb_send(state, ev, cli, SMBlockingX, additional_flags,
				8, state->vwv, 10, state->data);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_unlock_done, req);
	return req;
}

static void cli_unlock_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_unlock_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_unlock(struct cli_state *cli,
			uint16_t fnum,
			uint32_t offset,
			uint32_t len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_unlock_send(frame, ev, cli,
			fnum, offset, len);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_unlock_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Lock a file with 64 bit offsets.
****************************************************************************/

bool cli_lock64(struct cli_state *cli, uint16_t fnum,
		uint64_t offset, uint64_t len, int timeout,
		enum brl_type lock_type)
{
	uint16_t vwv[8];
	uint8_t bytes[20];
        int saved_timeout = cli->timeout;
	int ltype;
	NTSTATUS status;

	if (! (cli->capabilities & CAP_LARGE_FILES)) {
		return cli_lock(cli, fnum, offset, len, timeout, lock_type);
	}

	ltype = (lock_type == READ_LOCK? 1 : 0);
	ltype |= LOCKING_ANDX_LARGE_FILES;

	SCVAL(vwv + 0, 0, 0xff);
	SCVAL(vwv + 0, 1, 0);
	SSVAL(vwv + 1, 0, 0);
	SSVAL(vwv + 2, 0, fnum);
	SCVAL(vwv + 3, 0, ltype);
	SCVAL(vwv + 3, 1, 0);
	SIVALS(vwv + 4, 0, timeout);
	SSVAL(vwv + 6, 0, 0);
	SSVAL(vwv + 7, 0, 1);

	SIVAL(bytes, 0, cli->pid);
	SOFF_T_R(bytes, 4, offset);
	SOFF_T_R(bytes, 12, len);

	saved_timeout = cli->timeout;

	if (timeout != 0) {
		cli->timeout = (timeout == -1)
			? 0x7FFFFFFF : (timeout + 2*1000);
	}

	status = cli_smb(talloc_tos(), cli, SMBlockingX, 0, 8, vwv,
			 20, bytes, NULL, 0, NULL, NULL, NULL, NULL);

	cli->timeout = saved_timeout;

	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
 Unlock a file with 64 bit offsets.
****************************************************************************/

struct cli_unlock64_state {
	uint16_t vwv[8];
	uint8_t data[20];
};

static void cli_unlock64_done(struct tevent_req *subreq);

struct tevent_req *cli_unlock64_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum,
				uint64_t offset,
				uint64_t len)

{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_unlock64_state *state = NULL;
	uint8_t additional_flags = 0;

	req = tevent_req_create(mem_ctx, &state, struct cli_unlock64_state);
	if (req == NULL) {
		return NULL;
	}

        SCVAL(state->vwv+0, 0, 0xff);
	SSVAL(state->vwv+2, 0, fnum);
	SCVAL(state->vwv+3, 0,LOCKING_ANDX_LARGE_FILES);
	SIVALS(state->vwv+4, 0, 0);
	SSVAL(state->vwv+6, 0, 1);
	SSVAL(state->vwv+7, 0, 0);

	SIVAL(state->data, 0, cli->pid);
	SOFF_T_R(state->data, 4, offset);
	SOFF_T_R(state->data, 12, len);

	subreq = cli_smb_send(state, ev, cli, SMBlockingX, additional_flags,
				8, state->vwv, 20, state->data);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_unlock64_done, req);
	return req;
}

static void cli_unlock64_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_unlock64_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_unlock64(struct cli_state *cli,
				uint16_t fnum,
				uint64_t offset,
				uint64_t len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (! (cli->capabilities & CAP_LARGE_FILES)) {
		return cli_unlock(cli, fnum, offset, len);
	}

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_unlock64_send(frame, ev, cli,
			fnum, offset, len);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_unlock64_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Get/unlock a POSIX lock on a file - internal function.
****************************************************************************/

struct posix_lock_state {
        uint16_t setup;
	uint8_t param[4];
        uint8_t data[POSIX_LOCK_DATA_SIZE];
};

static void cli_posix_unlock_internal_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_trans_recv(subreq, NULL, NULL, NULL, 0, NULL,
					 NULL, 0, NULL, NULL, 0, NULL);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

static struct tevent_req *cli_posix_lock_internal_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					uint64_t offset,
					uint64_t len,
					bool wait_lock,
					enum brl_type lock_type)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct posix_lock_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct posix_lock_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup setup word. */
	SSVAL(&state->setup, 0, TRANSACT2_SETFILEINFO);

	/* Setup param array. */
	SSVAL(&state->param, 0, fnum);
	SSVAL(&state->param, 2, SMB_SET_POSIX_LOCK);

	/* Setup data array. */
	switch (lock_type) {
		case READ_LOCK:
			SSVAL(&state->data, POSIX_LOCK_TYPE_OFFSET,
				POSIX_LOCK_TYPE_READ);
			break;
		case WRITE_LOCK:
			SSVAL(&state->data, POSIX_LOCK_TYPE_OFFSET,
				POSIX_LOCK_TYPE_WRITE);
			break;
		case UNLOCK_LOCK:
			SSVAL(&state->data, POSIX_LOCK_TYPE_OFFSET,
				POSIX_LOCK_TYPE_UNLOCK);
			break;
		default:
			return NULL;
	}

	if (wait_lock) {
		SSVAL(&state->data, POSIX_LOCK_FLAGS_OFFSET,
				POSIX_LOCK_FLAG_WAIT);
	} else {
		SSVAL(state->data, POSIX_LOCK_FLAGS_OFFSET,
				POSIX_LOCK_FLAG_NOWAIT);
	}

	SIVAL(&state->data, POSIX_LOCK_PID_OFFSET, cli->pid);
	SOFF_T(&state->data, POSIX_LOCK_START_OFFSET, offset);
	SOFF_T(&state->data, POSIX_LOCK_LEN_OFFSET, len);

	subreq = cli_trans_send(state,                  /* mem ctx. */
				ev,                     /* event ctx. */
				cli,                    /* cli_state. */
				SMBtrans2,              /* cmd. */
				NULL,                   /* pipe name. */
				-1,                     /* fid. */
				0,                      /* function. */
				0,                      /* flags. */
				&state->setup,          /* setup. */
				1,                      /* num setup uint16_t words. */
				0,                      /* max returned setup. */
				state->param,           /* param. */
				4,			/* num param. */
				2,                      /* max returned param. */
				state->data,            /* data. */
				POSIX_LOCK_DATA_SIZE,   /* num data. */
				0);                     /* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_unlock_internal_done, req);
	return req;
}

/****************************************************************************
 POSIX Lock a file.
****************************************************************************/

struct tevent_req *cli_posix_lock_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					uint64_t offset,
					uint64_t len,
					bool wait_lock,
					enum brl_type lock_type)
{
	return cli_posix_lock_internal_send(mem_ctx, ev, cli, fnum, offset, len,
					wait_lock, lock_type);
}

NTSTATUS cli_posix_lock_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_lock(struct cli_state *cli, uint16_t fnum,
			uint64_t offset, uint64_t len,
			bool wait_lock, enum brl_type lock_type)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	if (lock_type != READ_LOCK && lock_type != WRITE_LOCK) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_lock_send(frame,
				ev,
				cli,
				fnum,
				offset,
				len,
				wait_lock,
				lock_type);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_lock_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 POSIX Unlock a file.
****************************************************************************/

struct tevent_req *cli_posix_unlock_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					uint64_t offset,
					uint64_t len)
{
	return cli_posix_lock_internal_send(mem_ctx, ev, cli, fnum, offset, len,
					false, UNLOCK_LOCK);
}

NTSTATUS cli_posix_unlock_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_unlock(struct cli_state *cli, uint16_t fnum, uint64_t offset, uint64_t len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_unlock_send(frame,
				ev,
				cli,
				fnum,
				offset,
				len);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_unlock_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Do a SMBgetattrE call.
****************************************************************************/

static void cli_getattrE_done(struct tevent_req *subreq);

struct cli_getattrE_state {
	uint16_t vwv[1];
	int zone_offset;
	uint16_t attr;
	SMB_OFF_T size;
	time_t change_time;
	time_t access_time;
	time_t write_time;
};

struct tevent_req *cli_getattrE_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_getattrE_state *state = NULL;
	uint8_t additional_flags = 0;

	req = tevent_req_create(mem_ctx, &state, struct cli_getattrE_state);
	if (req == NULL) {
		return NULL;
	}

	state->zone_offset = cli->serverzone;
	SSVAL(state->vwv+0,0,fnum);

	subreq = cli_smb_send(state, ev, cli, SMBgetattrE, additional_flags,
			      1, state->vwv, 0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_getattrE_done, req);
	return req;
}

static void cli_getattrE_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_getattrE_state *state = tevent_req_data(
		req, struct cli_getattrE_state);
	uint8_t wct;
	uint16_t *vwv = NULL;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 11, &wct, &vwv,
			      NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	state->size = (SMB_OFF_T)IVAL(vwv+6,0);
	state->attr = SVAL(vwv+10,0);
	state->change_time = make_unix_date2(vwv+0, state->zone_offset);
	state->access_time = make_unix_date2(vwv+2, state->zone_offset);
	state->write_time = make_unix_date2(vwv+4, state->zone_offset);

	tevent_req_done(req);
}

NTSTATUS cli_getattrE_recv(struct tevent_req *req,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time)
{
	struct cli_getattrE_state *state = tevent_req_data(
				req, struct cli_getattrE_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (attr) {
		*attr = state->attr;
	}
	if (size) {
		*size = state->size;
	}
	if (change_time) {
		*change_time = state->change_time;
	}
	if (access_time) {
		*access_time = state->access_time;
	}
	if (write_time) {
		*write_time = state->write_time;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_getattrE(struct cli_state *cli,
			uint16_t fnum,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_getattrE_send(frame, ev, cli, fnum);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_getattrE_recv(req,
					attr,
					size,
					change_time,
					access_time,
					write_time);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Do a SMBgetatr call
****************************************************************************/

static void cli_getatr_done(struct tevent_req *subreq);

struct cli_getatr_state {
	int zone_offset;
	uint16_t attr;
	SMB_OFF_T size;
	time_t write_time;
};

struct tevent_req *cli_getatr_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_getatr_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_getatr_state);
	if (req == NULL) {
		return NULL;
	}

	state->zone_offset = cli->serverzone;

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBgetatr, additional_flags,
			      0, NULL, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_getatr_done, req);
	return req;
}

static void cli_getatr_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_getatr_state *state = tevent_req_data(
		req, struct cli_getatr_state);
	uint8_t wct;
	uint16_t *vwv = NULL;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 4, &wct, &vwv, NULL,
			      NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	state->attr = SVAL(vwv+0,0);
	state->size = (SMB_OFF_T)IVAL(vwv+3,0);
	state->write_time = make_unix_date3(vwv+1, state->zone_offset);

	tevent_req_done(req);
}

NTSTATUS cli_getatr_recv(struct tevent_req *req,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *write_time)
{
	struct cli_getatr_state *state = tevent_req_data(
				req, struct cli_getatr_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (attr) {
		*attr = state->attr;
	}
	if (size) {
		*size = state->size;
	}
	if (write_time) {
		*write_time = state->write_time;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_getatr(struct cli_state *cli,
			const char *fname,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *write_time)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_getatr_send(frame, ev, cli, fname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_getatr_recv(req,
				attr,
				size,
				write_time);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Do a SMBsetattrE call.
****************************************************************************/

static void cli_setattrE_done(struct tevent_req *subreq);

struct cli_setattrE_state {
	uint16_t vwv[7];
};

struct tevent_req *cli_setattrE_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum,
				time_t change_time,
				time_t access_time,
				time_t write_time)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_setattrE_state *state = NULL;
	uint8_t additional_flags = 0;

	req = tevent_req_create(mem_ctx, &state, struct cli_setattrE_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0, fnum);
	push_dos_date2((uint8_t *)&state->vwv[1], 0, change_time,
		       cli->serverzone);
	push_dos_date2((uint8_t *)&state->vwv[3], 0, access_time,
		       cli->serverzone);
	push_dos_date2((uint8_t *)&state->vwv[5], 0, write_time,
		       cli->serverzone);

	subreq = cli_smb_send(state, ev, cli, SMBsetattrE, additional_flags,
			      7, state->vwv, 0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_setattrE_done, req);
	return req;
}

static void cli_setattrE_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_setattrE_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_setattrE(struct cli_state *cli,
			uint16_t fnum,
			time_t change_time,
			time_t access_time,
			time_t write_time)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_setattrE_send(frame, ev,
			cli,
			fnum,
			change_time,
			access_time,
			write_time);

	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_setattrE_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Do a SMBsetatr call.
****************************************************************************/

static void cli_setatr_done(struct tevent_req *subreq);

struct cli_setatr_state {
	uint16_t vwv[8];
};

struct tevent_req *cli_setatr_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname,
				uint16_t attr,
				time_t mtime)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_setatr_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_setatr_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv+0, 0, attr);
	push_dos_date3((uint8_t *)&state->vwv[1], 0, mtime, cli->serverzone);

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes = TALLOC_REALLOC_ARRAY(state, bytes, uint8_t,
			talloc_get_size(bytes)+1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	bytes[talloc_get_size(bytes)-1] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), "",
				   1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBsetatr, additional_flags,
			      8, state->vwv, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_setatr_done, req);
	return req;
}

static void cli_setatr_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_setatr_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_setatr(struct cli_state *cli,
		const char *fname,
		uint16_t attr,
		time_t mtime)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_setatr_send(frame, ev, cli, fname, attr, mtime);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_setatr_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Check for existance of a dir.
****************************************************************************/

static void cli_chkpath_done(struct tevent_req *subreq);

struct cli_chkpath_state {
	int dummy;
};

struct tevent_req *cli_chkpath_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_chkpath_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_chkpath_state);
	if (req == NULL) {
		return NULL;
	}

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);

	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBcheckpath, additional_flags,
			      0, NULL, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_chkpath_done, req);
	return req;
}

static void cli_chkpath_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_chkpath_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_chkpath(struct cli_state *cli, const char *path)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	char *path2 = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	path2 = talloc_strdup(frame, path);
	if (!path2) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	trim_char(path2,'\0','\\');
	if (!*path2) {
		path2 = talloc_strdup(frame, "\\");
		if (!path2) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_chkpath_send(frame, ev, cli, path2);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_chkpath_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Query disk space.
****************************************************************************/

static void cli_dskattr_done(struct tevent_req *subreq);

struct cli_dskattr_state {
	int bsize;
	int total;
	int avail;
};

struct tevent_req *cli_dskattr_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_dskattr_state *state = NULL;
	uint8_t additional_flags = 0;

	req = tevent_req_create(mem_ctx, &state, struct cli_dskattr_state);
	if (req == NULL) {
		return NULL;
	}

	subreq = cli_smb_send(state, ev, cli, SMBdskattr, additional_flags,
			      0, NULL, 0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_dskattr_done, req);
	return req;
}

static void cli_dskattr_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_dskattr_state *state = tevent_req_data(
		req, struct cli_dskattr_state);
	uint8_t wct;
	uint16_t *vwv = NULL;
	uint8_t *inbuf;
	NTSTATUS status;

	status = cli_smb_recv(subreq, state, &inbuf, 4, &wct, &vwv, NULL,
			      NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	state->bsize = SVAL(vwv+1, 0)*SVAL(vwv+2,0);
	state->total = SVAL(vwv+0, 0);
	state->avail = SVAL(vwv+3, 0);
	tevent_req_done(req);
}

NTSTATUS cli_dskattr_recv(struct tevent_req *req, int *bsize, int *total, int *avail)
{
	struct cli_dskattr_state *state = tevent_req_data(
				req, struct cli_dskattr_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*bsize = state->bsize;
	*total = state->total;
	*avail = state->avail;
	return NT_STATUS_OK;
}

NTSTATUS cli_dskattr(struct cli_state *cli, int *bsize, int *total, int *avail)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_dskattr_send(frame, ev, cli);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_dskattr_recv(req, bsize, total, avail);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Create and open a temporary file.
****************************************************************************/

static void cli_ctemp_done(struct tevent_req *subreq);

struct ctemp_state {
	uint16_t vwv[3];
	char *ret_path;
	uint16_t fnum;
};

struct tevent_req *cli_ctemp_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *path)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct ctemp_state *state = NULL;
	uint8_t additional_flags = 0;
	uint8_t *bytes = NULL;

	req = tevent_req_create(mem_ctx, &state, struct ctemp_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(state->vwv,0,0);
	SIVALS(state->vwv+1,0,-1);

	bytes = talloc_array(state, uint8_t, 1);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}
	bytes[0] = 4;
	bytes = smb_bytes_push_str(bytes, cli_ucs2(cli), path,
				   strlen(path)+1, NULL);
	if (tevent_req_nomem(bytes, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_smb_send(state, ev, cli, SMBctemp, additional_flags,
			      3, state->vwv, talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_ctemp_done, req);
	return req;
}

static void cli_ctemp_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	struct ctemp_state *state = tevent_req_data(
				req, struct ctemp_state);
	NTSTATUS status;
	uint8_t wcnt;
	uint16_t *vwv;
	uint32_t num_bytes = 0;
	uint8_t *bytes = NULL;
	uint8_t *inbuf;

	status = cli_smb_recv(subreq, state, &inbuf, 1, &wcnt, &vwv,
			      &num_bytes, &bytes);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	state->fnum = SVAL(vwv+0, 0);

	/* From W2K3, the result is just the ASCII name */
	if (num_bytes < 2) {
		tevent_req_nterror(req, NT_STATUS_DATA_ERROR);
		return;
	}

	if (pull_string_talloc(state,
			NULL,
			0,
			&state->ret_path,
			bytes,
			num_bytes,
			STR_ASCII) == 0) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_ctemp_recv(struct tevent_req *req,
			TALLOC_CTX *ctx,
			uint16_t *pfnum,
			char **outfile)
{
	struct ctemp_state *state = tevent_req_data(req,
			struct ctemp_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfnum = state->fnum;
	*outfile = talloc_strdup(ctx, state->ret_path);
	if (!*outfile) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_ctemp(struct cli_state *cli,
			TALLOC_CTX *ctx,
			const char *path,
			uint16_t *pfnum,
			char **out_path)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_ctemp_send(frame, ev, cli, path);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_ctemp_recv(req, ctx, pfnum, out_path);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/*
   send a raw ioctl - used by the torture code
*/
NTSTATUS cli_raw_ioctl(struct cli_state *cli, uint16_t fnum, uint32_t code, DATA_BLOB *blob)
{
	uint16_t vwv[3];
	NTSTATUS status;

	SSVAL(vwv+0, 0, fnum);
	SSVAL(vwv+1, 0, code>>16);
	SSVAL(vwv+2, 0, (code&0xFFFF));

	status = cli_smb(talloc_tos(), cli, SMBioctl, 0, 3, vwv, 0, NULL,
			 NULL, 0, NULL, NULL, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	*blob = data_blob_null;
	return NT_STATUS_OK;
}

/*********************************************************
 Set an extended attribute utility fn.
*********************************************************/

static NTSTATUS cli_set_ea(struct cli_state *cli, uint16_t setup_val,
			   uint8_t *param, unsigned int param_len,
			   const char *ea_name,
			   const char *ea_val, size_t ea_len)
{
	uint16_t setup[1];
	unsigned int data_len = 0;
	uint8_t *data = NULL;
	char *p;
	size_t ea_namelen = strlen(ea_name);
	NTSTATUS status;

	SSVAL(setup, 0, setup_val);

	if (ea_namelen == 0 && ea_len == 0) {
		data_len = 4;
		data = (uint8_t *)SMB_MALLOC(data_len);
		if (!data) {
			return NT_STATUS_NO_MEMORY;
		}
		p = (char *)data;
		SIVAL(p,0,data_len);
	} else {
		data_len = 4 + 4 + ea_namelen + 1 + ea_len;
		data = (uint8_t *)SMB_MALLOC(data_len);
		if (!data) {
			return NT_STATUS_NO_MEMORY;
		}
		p = (char *)data;
		SIVAL(p,0,data_len);
		p += 4;
		SCVAL(p, 0, 0); /* EA flags. */
		SCVAL(p, 1, ea_namelen);
		SSVAL(p, 2, ea_len);
		memcpy(p+4, ea_name, ea_namelen+1); /* Copy in the name. */
		memcpy(p+4+ea_namelen+1, ea_val, ea_len);
	}

	status = cli_trans(talloc_tos(), cli, SMBtrans2, NULL, -1, 0, 0,
			   setup, 1, 0,
			   param, param_len, 2,
			   data,  data_len, cli->max_xmit,
			   NULL,
			   NULL, 0, NULL, /* rsetup */
			   NULL, 0, NULL, /* rparam */
			   NULL, 0, NULL); /* rdata */
	SAFE_FREE(data);
	return status;
}

/*********************************************************
 Set an extended attribute on a pathname.
*********************************************************/

NTSTATUS cli_set_ea_path(struct cli_state *cli, const char *path,
			 const char *ea_name, const char *ea_val,
			 size_t ea_len)
{
	unsigned int param_len = 0;
	uint8_t *param;
	size_t srclen = 2*(strlen(path)+1);
	char *p;
	NTSTATUS status;

	param = SMB_MALLOC_ARRAY(uint8_t, 6+srclen+2);
	if (!param) {
		return NT_STATUS_NO_MEMORY;
	}
	memset(param, '\0', 6);
	SSVAL(param,0,SMB_INFO_SET_EA);
	p = (char *)(&param[6]);

	p += clistr_push(cli, p, path, srclen, STR_TERMINATE);
	param_len = PTR_DIFF(p, param);

	status = cli_set_ea(cli, TRANSACT2_SETPATHINFO, param, param_len,
			    ea_name, ea_val, ea_len);
	SAFE_FREE(param);
	return status;
}

/*********************************************************
 Set an extended attribute on an fnum.
*********************************************************/

NTSTATUS cli_set_ea_fnum(struct cli_state *cli, uint16_t fnum,
			 const char *ea_name, const char *ea_val,
			 size_t ea_len)
{
	uint8_t param[6];

	memset(param, 0, 6);
	SSVAL(param,0,fnum);
	SSVAL(param,2,SMB_INFO_SET_EA);

	return cli_set_ea(cli, TRANSACT2_SETFILEINFO, param, 6,
			  ea_name, ea_val, ea_len);
}

/*********************************************************
 Get an extended attribute list utility fn.
*********************************************************/

static bool parse_ea_blob(TALLOC_CTX *ctx, const uint8_t *rdata,
			  size_t rdata_len,
			  size_t *pnum_eas, struct ea_struct **pea_list)
{
	struct ea_struct *ea_list = NULL;
	size_t num_eas;
	size_t ea_size;
	const uint8_t *p;

	if (rdata_len < 4) {
		return false;
	}

	ea_size = (size_t)IVAL(rdata,0);
	if (ea_size > rdata_len) {
		return false;
	}

	if (ea_size == 0) {
		/* No EA's present. */
		*pnum_eas = 0;
		*pea_list = NULL;
		return true;
	}

	p = rdata + 4;
	ea_size -= 4;

	/* Validate the EA list and count it. */
	for (num_eas = 0; ea_size >= 4; num_eas++) {
		unsigned int ea_namelen = CVAL(p,1);
		unsigned int ea_valuelen = SVAL(p,2);
		if (ea_namelen == 0) {
			return false;
		}
		if (4 + ea_namelen + 1 + ea_valuelen > ea_size) {
			return false;
		}
		ea_size -= 4 + ea_namelen + 1 + ea_valuelen;
		p += 4 + ea_namelen + 1 + ea_valuelen;
	}

	if (num_eas == 0) {
		*pnum_eas = 0;
		*pea_list = NULL;
		return true;
	}

	*pnum_eas = num_eas;
	if (!pea_list) {
		/* Caller only wants number of EA's. */
		return true;
	}

	ea_list = TALLOC_ARRAY(ctx, struct ea_struct, num_eas);
	if (!ea_list) {
		return false;
	}

	ea_size = (size_t)IVAL(rdata,0);
	p = rdata + 4;

	for (num_eas = 0; num_eas < *pnum_eas; num_eas++ ) {
		struct ea_struct *ea = &ea_list[num_eas];
		fstring unix_ea_name;
		unsigned int ea_namelen = CVAL(p,1);
		unsigned int ea_valuelen = SVAL(p,2);

		ea->flags = CVAL(p,0);
		unix_ea_name[0] = '\0';
		pull_ascii_fstring(unix_ea_name, p + 4);
		ea->name = talloc_strdup(ea_list, unix_ea_name);
		if (!ea->name) {
			goto fail;
		}
		/* Ensure the value is null terminated (in case it's a string). */
		ea->value = data_blob_talloc(ea_list, NULL, ea_valuelen + 1);
		if (!ea->value.data) {
			goto fail;
		}
		if (ea_valuelen) {
			memcpy(ea->value.data, p+4+ea_namelen+1, ea_valuelen);
		}
		ea->value.data[ea_valuelen] = 0;
		ea->value.length--;
		p += 4 + ea_namelen + 1 + ea_valuelen;
	}

	*pea_list = ea_list;
	return true;

fail:
	TALLOC_FREE(ea_list);
	return false;
}

/*********************************************************
 Get an extended attribute list from a pathname.
*********************************************************/

struct cli_get_ea_list_path_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_get_ea_list_path_done(struct tevent_req *subreq);

struct tevent_req *cli_get_ea_list_path_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli,
					     const char *fname)
{
	struct tevent_req *req, *subreq;
	struct cli_get_ea_list_path_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_get_ea_list_path_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_INFO_QUERY_ALL_EAS, 4,
				    cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_get_ea_list_path_done, req);
	return req;
}

static void cli_get_ea_list_path_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	struct cli_get_ea_list_path_state *state = tevent_req_data(
		req, struct cli_get_ea_list_path_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_get_ea_list_path_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				   size_t *pnum_eas, struct ea_struct **peas)
{
	struct cli_get_ea_list_path_state *state = tevent_req_data(
		req, struct cli_get_ea_list_path_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (!parse_ea_blob(mem_ctx, state->data, state->num_data,
			   pnum_eas, peas)) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_get_ea_list_path(struct cli_state *cli, const char *path,
		TALLOC_CTX *ctx,
		size_t *pnum_eas,
		struct ea_struct **pea_list)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_get_ea_list_path_send(frame, ev, cli, path);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_get_ea_list_path_recv(req, ctx, pnum_eas, pea_list);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Convert open "flags" arg to uint32_t on wire.
****************************************************************************/

static uint32_t open_flags_to_wire(int flags)
{
	int open_mode = flags & O_ACCMODE;
	uint32_t ret = 0;

	switch (open_mode) {
		case O_WRONLY:
			ret |= SMB_O_WRONLY;
			break;
		case O_RDWR:
			ret |= SMB_O_RDWR;
			break;
		default:
		case O_RDONLY:
			ret |= SMB_O_RDONLY;
			break;
	}

	if (flags & O_CREAT) {
		ret |= SMB_O_CREAT;
	}
	if (flags & O_EXCL) {
		ret |= SMB_O_EXCL;
	}
	if (flags & O_TRUNC) {
		ret |= SMB_O_TRUNC;
	}
#if defined(O_SYNC)
	if (flags & O_SYNC) {
		ret |= SMB_O_SYNC;
	}
#endif /* O_SYNC */
	if (flags & O_APPEND) {
		ret |= SMB_O_APPEND;
	}
#if defined(O_DIRECT)
	if (flags & O_DIRECT) {
		ret |= SMB_O_DIRECT;
	}
#endif
#if defined(O_DIRECTORY)
	if (flags & O_DIRECTORY) {
		ret |= SMB_O_DIRECTORY;
	}
#endif
	return ret;
}

/****************************************************************************
 Open a file - POSIX semantics. Returns fnum. Doesn't request oplock.
****************************************************************************/

struct posix_open_state {
	uint16_t setup;
	uint8_t *param;
	uint8_t data[18];
	uint16_t fnum; /* Out */
};

static void cli_posix_open_internal_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
				subreq, struct tevent_req);
	struct posix_open_state *state = tevent_req_data(req, struct posix_open_state);
	NTSTATUS status;
	uint8_t *data;
	uint32_t num_data;

	status = cli_trans_recv(subreq, state, NULL, NULL, 0, NULL,
				NULL, 0, NULL, &data, 12, &num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	state->fnum = SVAL(data,2);
	tevent_req_done(req);
}

static struct tevent_req *cli_posix_open_internal_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					int flags,
					mode_t mode,
					bool is_dir)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct posix_open_state *state = NULL;
	uint32_t wire_flags = open_flags_to_wire(flags);

	req = tevent_req_create(mem_ctx, &state, struct posix_open_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup setup word. */
	SSVAL(&state->setup, 0, TRANSACT2_SETPATHINFO);

	/* Setup param array. */
	state->param = talloc_array(state, uint8_t, 6);
	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}
	memset(state->param, '\0', 6);
	SSVAL(state->param, 0, SMB_POSIX_PATH_OPEN);

	state->param = trans2_bytes_push_str(state->param, cli_ucs2(cli), fname,
				   strlen(fname)+1, NULL);

	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}

	/* Setup data words. */
	if (is_dir) {
		wire_flags |= SMB_O_DIRECTORY;
	}

	SIVAL(state->data,0,0); /* No oplock. */
	SIVAL(state->data,4,wire_flags);
	SIVAL(state->data,8,unix_perms_to_wire(mode));
	SIVAL(state->data,12,0); /* Top bits of perms currently undefined. */
	SSVAL(state->data,16,SMB_NO_INFO_LEVEL_RETURNED); /* No info level returned. */

	subreq = cli_trans_send(state,			/* mem ctx. */
				ev,			/* event ctx. */
				cli,			/* cli_state. */
				SMBtrans2,		/* cmd. */
				NULL,			/* pipe name. */
				-1,			/* fid. */
				0,			/* function. */
				0,			/* flags. */
				&state->setup,		/* setup. */
				1,			/* num setup uint16_t words. */
				0,			/* max returned setup. */
				state->param,		/* param. */
				talloc_get_size(state->param),/* num param. */
				2,			/* max returned param. */
				state->data,		/* data. */
				18,			/* num data. */
				12);			/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_open_internal_done, req);
	return req;
}

struct tevent_req *cli_posix_open_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					int flags,
					mode_t mode)
{
	return cli_posix_open_internal_send(mem_ctx, ev,
				cli, fname, flags, mode, false);
}

NTSTATUS cli_posix_open_recv(struct tevent_req *req, uint16_t *pfnum)
{
	struct posix_open_state *state = tevent_req_data(req, struct posix_open_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pfnum = state->fnum;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open - POSIX semantics. Doesn't request oplock.
****************************************************************************/

NTSTATUS cli_posix_open(struct cli_state *cli, const char *fname,
			int flags, mode_t mode, uint16_t *pfnum)
{

	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_open_send(frame,
				ev,
				cli,
				fname,
				flags,
				mode);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_open_recv(req, pfnum);

 fail:
	TALLOC_FREE(frame);
	return status;
}

struct tevent_req *cli_posix_mkdir_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					mode_t mode)
{
	return cli_posix_open_internal_send(mem_ctx, ev,
				cli, fname, O_CREAT, mode, true);
}

NTSTATUS cli_posix_mkdir_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_mkdir(struct cli_state *cli, const char *fname, mode_t mode)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_mkdir_send(frame,
				ev,
				cli,
				fname,
				mode);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_mkdir_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 unlink or rmdir - POSIX semantics.
****************************************************************************/

struct cli_posix_unlink_internal_state {
	uint8_t data[2];
};

static void cli_posix_unlink_internal_done(struct tevent_req *subreq);

static struct tevent_req *cli_posix_unlink_internal_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					uint16_t level)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_posix_unlink_internal_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_posix_unlink_internal_state);
	if (req == NULL) {
		return NULL;
	}

	/* Setup data word. */
	SSVAL(state->data, 0, level);

	subreq = cli_setpathinfo_send(state, ev, cli,
				      SMB_POSIX_PATH_UNLINK,
				      fname,
				      state->data, sizeof(state->data));
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_posix_unlink_internal_done, req);
	return req;
}

static void cli_posix_unlink_internal_done(struct tevent_req *subreq)
{
	NTSTATUS status = cli_setpathinfo_recv(subreq);
	tevent_req_simple_finish_ntstatus(subreq, status);
}

struct tevent_req *cli_posix_unlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname)
{
	return cli_posix_unlink_internal_send(mem_ctx, ev, cli, fname,
					      SMB_POSIX_UNLINK_FILE_TARGET);
}

NTSTATUS cli_posix_unlink_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

/****************************************************************************
 unlink - POSIX semantics.
****************************************************************************/

NTSTATUS cli_posix_unlink(struct cli_state *cli, const char *fname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_unlink_send(frame,
				ev,
				cli,
				fname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_unlink_recv(req);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 rmdir - POSIX semantics.
****************************************************************************/

struct tevent_req *cli_posix_rmdir_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname)
{
	return cli_posix_unlink_internal_send(
		mem_ctx, ev, cli, fname,
		SMB_POSIX_UNLINK_DIRECTORY_TARGET);
}

NTSTATUS cli_posix_rmdir_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_posix_rmdir(struct cli_state *cli, const char *fname)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev = NULL;
	struct tevent_req *req = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_posix_rmdir_send(frame,
				ev,
				cli,
				fname);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_posix_rmdir_recv(req, frame);

 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 filechangenotify
****************************************************************************/

struct cli_notify_state {
	uint8_t setup[8];
	uint32_t num_changes;
	struct notify_change *changes;
};

static void cli_notify_done(struct tevent_req *subreq);

struct tevent_req *cli_notify_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   struct cli_state *cli, uint16_t fnum,
				   uint32_t buffer_size,
				   uint32_t completion_filter, bool recursive)
{
	struct tevent_req *req, *subreq;
	struct cli_notify_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_notify_state);
	if (req == NULL) {
		return NULL;
	}

	SIVAL(state->setup, 0, completion_filter);
	SSVAL(state->setup, 4, fnum);
	SSVAL(state->setup, 6, recursive);

	subreq = cli_trans_send(
		state,			/* mem ctx. */
		ev,			/* event ctx. */
		cli,			/* cli_state. */
		SMBnttrans,		/* cmd. */
		NULL,			/* pipe name. */
		-1,			/* fid. */
		NT_TRANSACT_NOTIFY_CHANGE, /* function. */
		0,			/* flags. */
		(uint16_t *)state->setup, /* setup. */
		4,			/* num setup uint16_t words. */
		0,			/* max returned setup. */
		NULL,			/* param. */
		0,			/* num param. */
		buffer_size,		/* max returned param. */
		NULL,			/* data. */
		0,			/* num data. */
		0);			/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_notify_done, req);
	return req;
}

static void cli_notify_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_notify_state *state = tevent_req_data(
		req, struct cli_notify_state);
	NTSTATUS status;
	uint8_t *params;
	uint32_t i, ofs, num_params;
	uint16_t flags2;

	status = cli_trans_recv(subreq, talloc_tos(), &flags2, NULL, 0, NULL,
				&params, 0, &num_params, NULL, 0, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		DEBUG(10, ("cli_trans_recv returned %s\n", nt_errstr(status)));
		return;
	}

	state->num_changes = 0;
	ofs = 0;

	while (num_params - ofs > 12) {
		uint32_t len = IVAL(params, ofs);
		state->num_changes += 1;

		if ((len == 0) || (ofs+len >= num_params)) {
			break;
		}
		ofs += len;
	}

	state->changes = talloc_array(state, struct notify_change,
				      state->num_changes);
	if (tevent_req_nomem(state->changes, req)) {
		TALLOC_FREE(params);
		return;
	}

	ofs = 0;

	for (i=0; i<state->num_changes; i++) {
		uint32_t next = IVAL(params, ofs);
		uint32_t len = IVAL(params, ofs+8);
		ssize_t ret;
		char *name;

		if ((next != 0) && (len+12 != next)) {
			TALLOC_FREE(params);
			tevent_req_nterror(
				req, NT_STATUS_INVALID_NETWORK_RESPONSE);
			return;
		}

		state->changes[i].action = IVAL(params, ofs+4);
		ret = clistr_pull_talloc(params, (char *)params, flags2,
					 &name, params+ofs+12, len,
					 STR_TERMINATE|STR_UNICODE);
		if (ret == -1) {
			TALLOC_FREE(params);
			tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
			return;
		}
		state->changes[i].name = name;
		ofs += next;
	}

	TALLOC_FREE(params);
	tevent_req_done(req);
}

NTSTATUS cli_notify_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 uint32_t *pnum_changes,
			 struct notify_change **pchanges)
{
	struct cli_notify_state *state = tevent_req_data(
		req, struct cli_notify_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*pnum_changes = state->num_changes;
	*pchanges = talloc_move(mem_ctx, &state->changes);
	return NT_STATUS_OK;
}

struct cli_qpathinfo_state {
	uint8_t *param;
	uint8_t *data;
	uint16_t setup[1];
	uint32_t min_rdata;
	uint8_t *rdata;
	uint32_t num_rdata;
};

static void cli_qpathinfo_done(struct tevent_req *subreq);

struct tevent_req *cli_qpathinfo_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct cli_state *cli, const char *fname,
				      uint16_t level, uint32_t min_rdata,
				      uint32_t max_rdata)
{
	struct tevent_req *req, *subreq;
	struct cli_qpathinfo_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_qpathinfo_state);
	if (req == NULL) {
		return NULL;
	}
	state->min_rdata = min_rdata;
	SSVAL(state->setup, 0, TRANSACT2_QPATHINFO);

	state->param = talloc_zero_array(state, uint8_t, 6);
	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}
	SSVAL(state->param, 0, level);
	state->param = trans2_bytes_push_str(
		state->param, cli_ucs2(cli), fname, strlen(fname)+1, NULL);
	if (tevent_req_nomem(state->param, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = cli_trans_send(
		state,			/* mem ctx. */
		ev,			/* event ctx. */
		cli,			/* cli_state. */
		SMBtrans2,		/* cmd. */
		NULL,			/* pipe name. */
		-1,			/* fid. */
		0,			/* function. */
		0,			/* flags. */
		state->setup,		/* setup. */
		1,			/* num setup uint16_t words. */
		0,			/* max returned setup. */
		state->param,		/* param. */
		talloc_get_size(state->param),	/* num param. */
		2,			/* max returned param. */
		NULL,			/* data. */
		0,			/* num data. */
		max_rdata);		/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qpathinfo_done, req);
	return req;
}

static void cli_qpathinfo_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qpathinfo_state *state = tevent_req_data(
		req, struct cli_qpathinfo_state);
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL, NULL, 0, NULL,
				NULL, 0, NULL,
				&state->rdata, state->min_rdata,
				&state->num_rdata);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qpathinfo_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    uint8_t **rdata, uint32_t *num_rdata)
{
	struct cli_qpathinfo_state *state = tevent_req_data(
		req, struct cli_qpathinfo_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (rdata != NULL) {
		*rdata = talloc_move(mem_ctx, &state->rdata);
	} else {
		TALLOC_FREE(state->rdata);
	}
	if (num_rdata != NULL) {
		*num_rdata = state->num_rdata;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_qpathinfo(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		       const char *fname, uint16_t level, uint32_t min_rdata,
		       uint32_t max_rdata,
		       uint8_t **rdata, uint32_t *num_rdata)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qpathinfo_send(frame, ev, cli, fname, level, min_rdata,
				 max_rdata);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qpathinfo_recv(req, mem_ctx, rdata, num_rdata);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_qfileinfo_state {
	uint16_t setup[1];
	uint8_t param[4];
	uint8_t *data;
	uint32_t min_rdata;
	uint8_t *rdata;
	uint32_t num_rdata;
};

static void cli_qfileinfo_done(struct tevent_req *subreq);

struct tevent_req *cli_qfileinfo_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct cli_state *cli, uint16_t fnum,
				      uint16_t level, uint32_t min_rdata,
				      uint32_t max_rdata)
{
	struct tevent_req *req, *subreq;
	struct cli_qfileinfo_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_qfileinfo_state);
	if (req == NULL) {
		return NULL;
	}
	state->min_rdata = min_rdata;
	SSVAL(state->param, 0, fnum);
	SSVAL(state->param, 2, level);
	SSVAL(state->setup, 0, TRANSACT2_QFILEINFO);

	subreq = cli_trans_send(
		state,			/* mem ctx. */
		ev,			/* event ctx. */
		cli,			/* cli_state. */
		SMBtrans2,		/* cmd. */
		NULL,			/* pipe name. */
		-1,			/* fid. */
		0,			/* function. */
		0,			/* flags. */
		state->setup,		/* setup. */
		1,			/* num setup uint16_t words. */
		0,			/* max returned setup. */
		state->param,		/* param. */
		sizeof(state->param),	/* num param. */
		2,			/* max returned param. */
		NULL,			/* data. */
		0,			/* num data. */
		max_rdata);		/* max returned data. */

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qfileinfo_done, req);
	return req;
}

static void cli_qfileinfo_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qfileinfo_state *state = tevent_req_data(
		req, struct cli_qfileinfo_state);
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL, NULL, 0, NULL,
				NULL, 0, NULL,
				&state->rdata, state->min_rdata,
				&state->num_rdata);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qfileinfo_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    uint8_t **rdata, uint32_t *num_rdata)
{
	struct cli_qfileinfo_state *state = tevent_req_data(
		req, struct cli_qfileinfo_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (rdata != NULL) {
		*rdata = talloc_move(mem_ctx, &state->rdata);
	} else {
		TALLOC_FREE(state->rdata);
	}
	if (num_rdata != NULL) {
		*num_rdata = state->num_rdata;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_qfileinfo(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		       uint16_t fnum, uint16_t level, uint32_t min_rdata,
		       uint32_t max_rdata,
		       uint8_t **rdata, uint32_t *num_rdata)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qfileinfo_send(frame, ev, cli, fnum, level, min_rdata,
				 max_rdata);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qfileinfo_recv(req, mem_ctx, rdata, num_rdata);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_flush_state {
	uint16_t vwv[1];
};

static void cli_flush_done(struct tevent_req *subreq);

struct tevent_req *cli_flush_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  uint16_t fnum)
{
	struct tevent_req *req, *subreq;
	struct cli_flush_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_flush_state);
	if (req == NULL) {
		return NULL;
	}
	SSVAL(state->vwv + 0, 0, fnum);

	subreq = cli_smb_send(state, ev, cli, SMBflush, 0, 1, state->vwv,
			      0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_flush_done, req);
	return req;
}

static void cli_flush_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_flush_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_flush(TALLOC_CTX *mem_ctx, struct cli_state *cli, uint16_t fnum)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_flush_send(frame, ev, cli, fnum);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_flush_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_shadow_copy_data_state {
	uint16_t setup[4];
	uint8_t *data;
	uint32_t num_data;
	bool get_names;
};

static void cli_shadow_copy_data_done(struct tevent_req *subreq);

struct tevent_req *cli_shadow_copy_data_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli,
					     uint16_t fnum,
					     bool get_names)
{
	struct tevent_req *req, *subreq;
	struct cli_shadow_copy_data_state *state;
	uint32_t ret_size;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_shadow_copy_data_state);
	if (req == NULL) {
		return NULL;
	}
	state->get_names = get_names;
	ret_size = get_names ? cli->max_xmit : 16;

	SIVAL(state->setup + 0, 0, FSCTL_GET_SHADOW_COPY_DATA);
	SSVAL(state->setup + 2, 0, fnum);
	SCVAL(state->setup + 3, 0, 1); /* isFsctl */
	SCVAL(state->setup + 3, 1, 0); /* compfilter, isFlags (WSSP) */

	subreq = cli_trans_send(
		state, ev, cli, SMBnttrans, NULL, 0, NT_TRANSACT_IOCTL, 0,
		state->setup, ARRAY_SIZE(state->setup), 0,
		NULL, 0, 0,
		NULL, 0, ret_size);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_shadow_copy_data_done, req);
	return req;
}

static void cli_shadow_copy_data_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_shadow_copy_data_state *state = tevent_req_data(
		req, struct cli_shadow_copy_data_state);
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL,
				NULL, 0, NULL, /* setup */
				NULL, 0, NULL, /* param */
				&state->data, 12, &state->num_data);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_shadow_copy_data_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				   char ***pnames, int *pnum_names)
{
	struct cli_shadow_copy_data_state *state = tevent_req_data(
		req, struct cli_shadow_copy_data_state);
	char **names;
	int i, num_names;
	uint32_t dlength;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	num_names = IVAL(state->data, 4);
	dlength = IVAL(state->data, 8);

	if (!state->get_names) {
		*pnum_names = num_names;
		return NT_STATUS_OK;
	}

	if (dlength+12 > state->num_data) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	names = talloc_array(mem_ctx, char *, num_names);
	if (names == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_names; i++) {
		bool ret;
		uint8_t *src;
		size_t converted_size;

		src = state->data + 12 + i * 2 * sizeof(SHADOW_COPY_LABEL);
		ret = convert_string_talloc(
			names, CH_UTF16LE, CH_UNIX,
			src, 2 * sizeof(SHADOW_COPY_LABEL),
			&names[i], &converted_size, True);
		if (!ret) {
			TALLOC_FREE(names);
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
	}
	*pnum_names = num_names;
	*pnames = names;
	return NT_STATUS_OK;
}

NTSTATUS cli_shadow_copy_data(TALLOC_CTX *mem_ctx, struct cli_state *cli,
			      uint16_t fnum, bool get_names,
			      char ***pnames, int *pnum_names)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_shadow_copy_data_send(frame, ev, cli, fnum, get_names);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_shadow_copy_data_recv(req, mem_ctx, pnames, pnum_names);
 fail:
	TALLOC_FREE(frame);
	return status;
}
