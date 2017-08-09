/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (c) Andreas Schneider            2010.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/filesys.h"
#include "printing/nt_printing_migrate.h"
#include "printing/nt_printing_migrate_internal.h"

#include "rpc_client/rpc_client.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_winreg.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "auth.h"
#include "util_tdb.h"

#define FORMS_PREFIX "FORMS/"
#define DRIVERS_PREFIX "DRIVERS/"
#define PRINTERS_PREFIX "PRINTERS/"
#define SECDESC_PREFIX "SECDESC/"

static int rename_file_with_suffix(TALLOC_CTX *mem_ctx,
				   const char *path,
				   const char *suffix)
{
	int rc = -1;
	char *dst_path;

	dst_path = talloc_asprintf(mem_ctx, "%s%s", path, suffix);
	if (dst_path == NULL) {
		DEBUG(3, ("error out of memory\n"));
		return rc;
	}

	rc = (rename(path, dst_path) != 0);

	if (rc == 0) {
		DEBUG(5, ("moved '%s' to '%s'\n", path, dst_path));
	} else if (errno == ENOENT) {
		DEBUG(3, ("file '%s' does not exist - so not moved\n", path));
		rc = 0;
	} else {
		DEBUG(3, ("error renaming %s to %s: %s\n", path, dst_path,
			  strerror(errno)));
	}

	TALLOC_FREE(dst_path);
	return rc;
}

static NTSTATUS migrate_internal(TALLOC_CTX *mem_ctx,
				 const char *tdb_path,
				 struct rpc_pipe_client *winreg_pipe)
{
	const char *backup_suffix = ".bak";
	TDB_DATA kbuf, newkey, dbuf;
	TDB_CONTEXT *tdb;
	NTSTATUS status;
	int rc;

	tdb = tdb_open_log(tdb_path, 0, TDB_DEFAULT, O_RDONLY, 0600);
	if (tdb == NULL && errno == ENOENT) {
		/* if we have no printers database then migration is
		   considered successful */
		DEBUG(4, ("No printers database to migrate in %s\n", tdb_path));
		return NT_STATUS_OK;
	}
	if (tdb == NULL) {
		DEBUG(2, ("Failed to open tdb file: %s\n", tdb_path));
		return NT_STATUS_NO_SUCH_FILE;
	}

	for (kbuf = tdb_firstkey(tdb);
	     kbuf.dptr;
	     newkey = tdb_nextkey(tdb, kbuf), free(kbuf.dptr), kbuf = newkey)
	{
		dbuf = tdb_fetch(tdb, kbuf);
		if (!dbuf.dptr) {
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, FORMS_PREFIX, strlen(FORMS_PREFIX)) == 0) {
			status = printing_tdb_migrate_form(mem_ctx,
					      winreg_pipe,
					      (const char *) kbuf.dptr + strlen(FORMS_PREFIX),
					      dbuf.dptr,
					      dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			if (!NT_STATUS_IS_OK(status)) {
				tdb_close(tdb);
				return status;
			}
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, DRIVERS_PREFIX, strlen(DRIVERS_PREFIX)) == 0) {
			status = printing_tdb_migrate_driver(mem_ctx,
						winreg_pipe,
						(const char *) kbuf.dptr + strlen(DRIVERS_PREFIX),
						dbuf.dptr,
						dbuf.dsize,
						false);
			SAFE_FREE(dbuf.dptr);
			if (!NT_STATUS_IS_OK(status)) {
				tdb_close(tdb);
				return status;
			}
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, PRINTERS_PREFIX, strlen(PRINTERS_PREFIX)) == 0) {
			const char *printer_name = (const char *)(kbuf.dptr
						    + strlen(PRINTERS_PREFIX));
			status = printing_tdb_migrate_printer(mem_ctx,
						 winreg_pipe,
						 printer_name,
						 dbuf.dptr,
						 dbuf.dsize,
						 false);
			SAFE_FREE(dbuf.dptr);
			if (!NT_STATUS_IS_OK(status)) {
				tdb_close(tdb);
				return status;
			}
			continue;
		}
		SAFE_FREE(dbuf.dptr);
	}

	for (kbuf = tdb_firstkey(tdb);
	     kbuf.dptr;
	     newkey = tdb_nextkey(tdb, kbuf), free(kbuf.dptr), kbuf = newkey)
	{
		dbuf = tdb_fetch(tdb, kbuf);
		if (!dbuf.dptr) {
			continue;
		}

		if (strncmp((const char *) kbuf.dptr, SECDESC_PREFIX, strlen(SECDESC_PREFIX)) == 0) {
			const char *secdesc_name = (const char *)(kbuf.dptr
						    + strlen(SECDESC_PREFIX));
			status = printing_tdb_migrate_secdesc(mem_ctx,
						 winreg_pipe,
						 secdesc_name,
						 dbuf.dptr,
						 dbuf.dsize);
			SAFE_FREE(dbuf.dptr);
			if (NT_STATUS_EQUAL(status, werror_to_ntstatus(WERR_BADFILE))) {
				DEBUG(2, ("Skipping secdesc migration for non-existent "
						"printer: %s\n", secdesc_name));
			} else if (!NT_STATUS_IS_OK(status)) {
				tdb_close(tdb);
				return status;
			}
			continue;
		}
		SAFE_FREE(dbuf.dptr);
	}

	tdb_close(tdb);

	rc = rename_file_with_suffix(mem_ctx, tdb_path, backup_suffix);
	if (rc != 0) {
		DEBUG(0, ("Error moving tdb to '%s%s'\n",
			  tdb_path, backup_suffix));
	}

	return NT_STATUS_OK;
}

bool nt_printing_tdb_migrate(struct messaging_context *msg_ctx)
{
	const char *drivers_path = state_path("ntdrivers.tdb");
	const char *printers_path = state_path("ntprinters.tdb");
	const char *forms_path = state_path("ntforms.tdb");
	bool drivers_exists = file_exist(drivers_path);
	bool printers_exists = file_exist(printers_path);
	bool forms_exists = file_exist(forms_path);
	struct auth_serversupplied_info *session_info;
	struct rpc_pipe_client *winreg_pipe = NULL;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	NTSTATUS status;

	if (!drivers_exists && !printers_exists && !forms_exists) {
		return true;
	}

	status = make_session_info_system(tmp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Couldn't create session_info: %s\n",
			  nt_errstr(status)));
		talloc_free(tmp_ctx);
		return false;
	}

	status = rpc_pipe_open_interface(tmp_ctx,
					&ndr_table_winreg.syntax_id,
					session_info,
					NULL,
					msg_ctx,
					&winreg_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Couldn't open internal winreg pipe: %s\n",
			  nt_errstr(status)));
		talloc_free(tmp_ctx);
		return false;
	}

	if (drivers_exists) {
		status = migrate_internal(tmp_ctx, drivers_path, winreg_pipe);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Couldn't migrate drivers tdb file: %s\n",
			  nt_errstr(status)));
			talloc_free(tmp_ctx);
			return false;
		}
	}

	if (printers_exists) {
		status = migrate_internal(tmp_ctx, printers_path, winreg_pipe);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Couldn't migrate printers tdb file: %s\n",
				  nt_errstr(status)));
			talloc_free(tmp_ctx);
			return false;
		}
	}

	if (forms_exists) {
		status = migrate_internal(tmp_ctx, forms_path, winreg_pipe);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Couldn't migrate forms tdb file: %s\n",
				  nt_errstr(status)));
			talloc_free(tmp_ctx);
			return false;
		}
	}

	talloc_free(tmp_ctx);
	return true;
}
