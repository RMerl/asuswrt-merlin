/*
   Unix SMB/CIFS implementation.
   Copyright (C) Guenther Deschner    2009

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
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/auth/schannel_state.h"

/******************************************************************************
 Open or create the schannel session store tdb.
*******************************************************************************/

#define SCHANNEL_STORE_VERSION_1 1
#define SCHANNEL_STORE_VERSION_2 2 /* should not be used */
#define SCHANNEL_STORE_VERSION_CURRENT SCHANNEL_STORE_VERSION_1

TDB_CONTEXT *open_schannel_session_store(TALLOC_CTX *mem_ctx)
{
	TDB_DATA vers;
	uint32 ver;
	TDB_CONTEXT *tdb_sc = NULL;
	char *fname = talloc_asprintf(mem_ctx, "%s/schannel_store.tdb", lp_private_dir());

	if (!fname) {
		return NULL;
	}

	tdb_sc = tdb_open_log(fname, 0, TDB_DEFAULT, O_RDWR|O_CREAT, 0600);

	if (!tdb_sc) {
		DEBUG(0,("open_schannel_session_store: Failed to open %s\n", fname));
		TALLOC_FREE(fname);
		return NULL;
	}

 again:
	vers = tdb_fetch_bystring(tdb_sc, "SCHANNEL_STORE_VERSION");
	if (vers.dptr == NULL) {
		/* First opener, no version. */
		SIVAL(&ver,0,SCHANNEL_STORE_VERSION_CURRENT);
		vers.dptr = (uint8 *)&ver;
		vers.dsize = 4;
		tdb_store_bystring(tdb_sc, "SCHANNEL_STORE_VERSION", vers, TDB_REPLACE);
		vers.dptr = NULL;
	} else if (vers.dsize == 4) {
		ver = IVAL(vers.dptr,0);
		if (ver == SCHANNEL_STORE_VERSION_2) {
			DEBUG(0,("open_schannel_session_store: wrong version number %d in %s\n",
				(int)ver, fname ));
			tdb_wipe_all(tdb_sc);
			goto again;
		}
		if (ver != SCHANNEL_STORE_VERSION_CURRENT) {
			DEBUG(0,("open_schannel_session_store: wrong version number %d in %s\n",
				(int)ver, fname ));
			tdb_close(tdb_sc);
			tdb_sc = NULL;
		}
	} else {
		tdb_close(tdb_sc);
		tdb_sc = NULL;
		DEBUG(0,("open_schannel_session_store: wrong version number size %d in %s\n",
			(int)vers.dsize, fname ));
	}

	SAFE_FREE(vers.dptr);
	TALLOC_FREE(fname);

	return tdb_sc;
}

/******************************************************************************
 Wrapper around schannel_fetch_session_key_tdb()
 Note we must be root here.
*******************************************************************************/

NTSTATUS schannel_fetch_session_key(TALLOC_CTX *mem_ctx,
				    const char *computer_name,
				    struct netlogon_creds_CredentialState **pcreds)
{
	struct tdb_context *tdb;
	NTSTATUS status;

	tdb = open_schannel_session_store(mem_ctx);
	if (!tdb) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = schannel_fetch_session_key_tdb(tdb, mem_ctx, computer_name, pcreds);

	tdb_close(tdb);

	return status;
}

/******************************************************************************
 Wrapper around schannel_store_session_key_tdb()
 Note we must be root here.
*******************************************************************************/

NTSTATUS schannel_store_session_key(TALLOC_CTX *mem_ctx,
				    struct netlogon_creds_CredentialState *creds)
{
	struct tdb_context *tdb;
	NTSTATUS status;

	tdb = open_schannel_session_store(mem_ctx);
	if (!tdb) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = schannel_store_session_key_tdb(tdb, mem_ctx, creds);

	tdb_close(tdb);

	return status;
}
