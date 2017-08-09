/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008

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
#include "libsmb/libsmb.h"
#include "libsmbclient.h"
#include "libsmb_internal.h"


/*
 * Open a print file to be written to by other calls
 */

SMBCFILE *
SMBC_open_print_job_ctx(SMBCCTX *context,
                        const char *fname)
{
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        DEBUG(4, ("SMBC_open_print_job_ctx(%s)\n", fname));

        if (SMBC_parse_path(frame,
                            context,
                            fname,
                            NULL,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return NULL;
        }

        /* What if the path is empty, or the file exists? */

	TALLOC_FREE(frame);
        return smbc_getFunctionOpen(context)(context, fname, O_WRONLY, 666);
}

/*
 * Routine to print a file on a remote server ...
 *
 * We open the file, which we assume to be on a remote server, and then
 * copy it to a print file on the share specified by printq.
 */

int
SMBC_print_file_ctx(SMBCCTX *c_file,
                    const char *fname,
                    SMBCCTX *c_print,
                    const char *printq)
{
        SMBCFILE *fid1;
        SMBCFILE *fid2;
        int bytes;
        int saverr;
        int tot_bytes = 0;
        char buf[4096];
	TALLOC_CTX *frame = talloc_stackframe();

        if (!c_file || !c_file->internal->initialized ||
            !c_print || !c_print->internal->initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname && !printq) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        /* Try to open the file for reading ... */

        if ((long)(fid1 = smbc_getFunctionOpen(c_file)(c_file, fname,
                                                       O_RDONLY, 0666)) < 0) {
                DEBUG(3, ("Error, fname=%s, errno=%i\n", fname, errno));
		TALLOC_FREE(frame);
                return -1;  /* smbc_open sets errno */
        }

        /* Now, try to open the printer file for writing */

        if ((long)(fid2 = smbc_getFunctionOpenPrintJob(c_print)(c_print,
                                                                printq)) < 0) {
                saverr = errno;  /* Save errno */
                smbc_getFunctionClose(c_file)(c_file, fid1);
                errno = saverr;
		TALLOC_FREE(frame);
                return -1;
        }

        while ((bytes = smbc_getFunctionRead(c_file)(c_file, fid1,
                                                     buf, sizeof(buf))) > 0) {
                tot_bytes += bytes;

                if ((smbc_getFunctionWrite(c_print)(c_print, fid2,
                                                    buf, bytes)) < 0) {
                        saverr = errno;
                        smbc_getFunctionClose(c_file)(c_file, fid1);
                        smbc_getFunctionClose(c_print)(c_print, fid2);
                        errno = saverr;
                }
        }

        saverr = errno;

        smbc_getFunctionClose(c_file)(c_file, fid1);
        smbc_getFunctionClose(c_print)(c_print, fid2);

        if (bytes < 0) {
                errno = saverr;
		TALLOC_FREE(frame);
                return -1;
        }

	TALLOC_FREE(frame);
        return tot_bytes;
}

/*
 * Routine to list print jobs on a printer share ...
 */

int
SMBC_list_print_jobs_ctx(SMBCCTX *context,
                         const char *fname,
                         smbc_list_print_job_fn fn)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_list_print_jobs(%s)\n", fname));

        if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by SMBC_server */
        }

        if (cli_print_queue(srv->cli,
                            (void (*)(struct print_job_info *))fn) < 0) {
                errno = SMBC_errno(context, srv->cli);
		TALLOC_FREE(frame);
                return -1;
        }

	TALLOC_FREE(frame);
        return 0;
}

/*
 * Delete a print job from a remote printer share
 */

int
SMBC_unlink_print_job_ctx(SMBCCTX *context,
                          const char *fname,
                          int id)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        int err;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        if (!fname) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

        DEBUG(4, ("smbc_unlink_print_job(%s)\n", fname));

        if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

        if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

        srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

        if (!srv) {
		TALLOC_FREE(frame);
                return -1;  /* errno set by SMBC_server */
        }

        if ((err = cli_printjob_del(srv->cli, id)) != 0) {
                if (err < 0)
                        errno = SMBC_errno(context, srv->cli);
                else if (err == ERRnosuchprintjob)
                        errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	TALLOC_FREE(frame);
        return 0;
}

