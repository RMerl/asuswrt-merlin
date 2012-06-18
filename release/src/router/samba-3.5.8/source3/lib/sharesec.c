/* 
 *  Unix SMB/Netbios implementation.
 *  SEC_DESC handling functions
 *  Copyright (C) Jeremy R. Allison            1995-2003.
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

/*******************************************************************
 Create the share security tdb.
 ********************************************************************/

static struct db_context *share_db; /* used for share security descriptors */
#define SHARE_DATABASE_VERSION_V1 1
#define SHARE_DATABASE_VERSION_V2 2 /* version id in little endian. */

/* Map generic permissions to file object specific permissions */

extern const struct generic_mapping file_generic_mapping;

static int delete_fn(struct db_record *rec, void *priv)
{
	rec->delete_rec(rec);
	return 0;
}

bool share_info_db_init(void)
{
	const char *vstring = "INFO/version";
	int32 vers_id;

	if (share_db != NULL) {
		return True;
	}

	share_db = db_open(NULL, state_path("share_info.tdb"), 0,
				 TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
	if (share_db == NULL) {
		DEBUG(0,("Failed to open share info database %s (%s)\n",
			state_path("share_info.tdb"), strerror(errno) ));
		return False;
	}

	vers_id = dbwrap_fetch_int32(share_db, vstring);
	if (vers_id == SHARE_DATABASE_VERSION_V2) {
		return true;
	}

	if (share_db->transaction_start(share_db) != 0) {
		DEBUG(0, ("transaction_start failed\n"));
		TALLOC_FREE(share_db);
		return false;
	}

	vers_id = dbwrap_fetch_int32(share_db, vstring);
	if (vers_id == SHARE_DATABASE_VERSION_V2) {
		/*
		 * Race condition
		 */
		if (share_db->transaction_cancel(share_db)) {
			smb_panic("transaction_cancel failed");
		}
		return true;
	}

	/* Cope with byte-reversed older versions of the db. */
	if ((vers_id == SHARE_DATABASE_VERSION_V1) || (IREV(vers_id) == SHARE_DATABASE_VERSION_V1)) {
		/* Written on a bigendian machine with old fetch_int code. Save as le. */

		if (dbwrap_store_int32(share_db, vstring,
				       SHARE_DATABASE_VERSION_V2) != 0) {
			DEBUG(0, ("dbwrap_store_int32 failed\n"));
			goto cancel;
		}
		vers_id = SHARE_DATABASE_VERSION_V2;
	}

	if (vers_id != SHARE_DATABASE_VERSION_V2) {
		int ret;
		ret = share_db->traverse(share_db, delete_fn, NULL);
		if (ret < 0) {
			DEBUG(0, ("traverse failed\n"));
			goto cancel;
		}
		if (dbwrap_store_int32(share_db, vstring,
				       SHARE_DATABASE_VERSION_V2) != 0) {
			DEBUG(0, ("dbwrap_store_int32 failed\n"));
			goto cancel;
		}
	}

	if (share_db->transaction_commit(share_db) != 0) {
		DEBUG(0, ("transaction_commit failed\n"));
		return false;
	}

	return true;

 cancel:
	if (share_db->transaction_cancel(share_db)) {
		smb_panic("transaction_cancel failed");
	}

	return false;
}

/*******************************************************************
 Fake up a Everyone, default access as a default.
 def_access is a GENERIC_XXX access mode.
 ********************************************************************/

SEC_DESC *get_share_security_default( TALLOC_CTX *ctx, size_t *psize, uint32 def_access)
{
	uint32_t sa;
	SEC_ACE ace;
	SEC_ACL *psa = NULL;
	SEC_DESC *psd = NULL;
	uint32 spec_access = def_access;

	se_map_generic(&spec_access, &file_generic_mapping);

	sa = (def_access | spec_access );
	init_sec_ace(&ace, &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED, sa, 0);

	if ((psa = make_sec_acl(ctx, NT4_ACL_REVISION, 1, &ace)) != NULL) {
		psd = make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
				    SEC_DESC_SELF_RELATIVE, NULL, NULL, NULL,
				    psa, psize);
	}

	if (!psd) {
		DEBUG(0,("get_share_security: Failed to make SEC_DESC.\n"));
		return NULL;
	}

	return psd;
}

/*******************************************************************
 Pull a security descriptor from the share tdb.
 ********************************************************************/

SEC_DESC *get_share_security( TALLOC_CTX *ctx, const char *servicename,
			      size_t *psize)
{
	char *key;
	SEC_DESC *psd = NULL;
	TDB_DATA data;
	NTSTATUS status;

	if (!share_info_db_init()) {
		return NULL;
	}

	if (!(key = talloc_asprintf(ctx, "SECDESC/%s", servicename))) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		return NULL;
	}

	data = dbwrap_fetch_bystring(share_db, talloc_tos(), key);

	TALLOC_FREE(key);

	if (data.dptr == NULL) {
		return get_share_security_default(ctx, psize,
						  GENERIC_ALL_ACCESS);
	}

	status = unmarshall_sec_desc(ctx, data.dptr, data.dsize, &psd);

	TALLOC_FREE(data.dptr);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("unmarshall_sec_desc failed: %s\n",
			  nt_errstr(status)));
		return NULL;
	}

	if (psd)
		*psize = ndr_size_security_descriptor(psd, NULL, 0);

	return psd;
}

/*******************************************************************
 Store a security descriptor in the share db.
 ********************************************************************/

bool set_share_security(const char *share_name, SEC_DESC *psd)
{
	TALLOC_CTX *frame;
	char *key;
	bool ret = False;
	TDB_DATA blob;
	NTSTATUS status;

	if (!share_info_db_init()) {
		return False;
	}

	frame = talloc_stackframe();

	status = marshall_sec_desc(frame, psd, &blob.dptr, &blob.dsize);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("marshall_sec_desc failed: %s\n",
			  nt_errstr(status)));
		goto out;
	}

	if (!(key = talloc_asprintf(frame, "SECDESC/%s", share_name))) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		goto out;
	}

	status = dbwrap_trans_store(share_db, string_term_tdb_data(key), blob,
				    TDB_REPLACE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("set_share_security: Failed to store secdesc for "
			  "%s: %s\n", share_name, nt_errstr(status)));
		goto out;
	}

	DEBUG(5,("set_share_security: stored secdesc for %s\n", share_name ));
	ret = True;

 out:
	TALLOC_FREE(frame);
	return ret;
}

/*******************************************************************
 Delete a security descriptor.
********************************************************************/

bool delete_share_security(const char *servicename)
{
	TDB_DATA kbuf;
	char *key;
	NTSTATUS status;

	if (!share_info_db_init()) {
		return False;
	}

	if (!(key = talloc_asprintf(talloc_tos(), "SECDESC/%s",
				    servicename))) {
		return False;
	}
	kbuf = string_term_tdb_data(key);

	status = dbwrap_trans_delete(share_db, kbuf);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("delete_share_security: Failed to delete entry for "
			  "share %s: %s\n", servicename, nt_errstr(status)));
		return False;
	}

	return True;
}

/*******************************************************************
 Can this user access with share with the required permissions ?
********************************************************************/

bool share_access_check(const NT_USER_TOKEN *token, const char *sharename,
			uint32 desired_access)
{
	uint32 granted;
	NTSTATUS status;
	SEC_DESC *psd = NULL;
	size_t sd_size;

	psd = get_share_security(talloc_tos(), sharename, &sd_size);

	if (!psd) {
		return True;
	}

	status = se_access_check(psd, token, desired_access, &granted);

	TALLOC_FREE(psd);

	return NT_STATUS_IS_OK(status);
}

/***************************************************************************
 Parse the contents of an acl string from a usershare file.
***************************************************************************/

bool parse_usershare_acl(TALLOC_CTX *ctx, const char *acl_str, SEC_DESC **ppsd)
{
	size_t s_size = 0;
	const char *pacl = acl_str;
	int num_aces = 0;
	SEC_ACE *ace_list = NULL;
	SEC_ACL *psa = NULL;
	SEC_DESC *psd = NULL;
	size_t sd_size = 0;
	int i;

	*ppsd = NULL;

	/* If the acl string is blank return "Everyone:R" */
	if (!*acl_str) {
		SEC_DESC *default_psd = get_share_security_default(ctx, &s_size, GENERIC_READ_ACCESS);
		if (!default_psd) {
			return False;
		}
		*ppsd = default_psd;
		return True;
	}

	num_aces = 1;

	/* Add the number of ',' characters to get the number of aces. */
	num_aces += count_chars(pacl,',');

	ace_list = TALLOC_ARRAY(ctx, SEC_ACE, num_aces);
	if (!ace_list) {
		return False;
	}

	for (i = 0; i < num_aces; i++) {
		uint32_t sa;
		uint32 g_access;
		uint32 s_access;
		DOM_SID sid;
		char *sidstr;
		enum security_ace_type type = SEC_ACE_TYPE_ACCESS_ALLOWED;

		if (!next_token_talloc(ctx, &pacl, &sidstr, ":")) {
			DEBUG(0,("parse_usershare_acl: malformed usershare acl looking "
				"for ':' in string '%s'\n", pacl));
			return False;
		}

		if (!string_to_sid(&sid, sidstr)) {
			DEBUG(0,("parse_usershare_acl: failed to convert %s to sid.\n",
				sidstr ));
			return False;
		}

		switch (*pacl) {
			case 'F': /* Full Control, ie. R+W */
			case 'f': /* Full Control, ie. R+W */
				s_access = g_access = GENERIC_ALL_ACCESS;
				break;
			case 'R': /* Read only. */
			case 'r': /* Read only. */
				s_access = g_access = GENERIC_READ_ACCESS;
				break;
			case 'D': /* Deny all to this SID. */
			case 'd': /* Deny all to this SID. */
				type = SEC_ACE_TYPE_ACCESS_DENIED;
				s_access = g_access = GENERIC_ALL_ACCESS;
				break;
			default:
				DEBUG(0,("parse_usershare_acl: unknown acl type at %s.\n",
					pacl ));
				return False;
		}

		pacl++;
		if (*pacl && *pacl != ',') {
			DEBUG(0,("parse_usershare_acl: bad acl string at %s.\n",
				pacl ));
			return False;
		}
		pacl++; /* Go past any ',' */

		se_map_generic(&s_access, &file_generic_mapping);
		sa = (g_access | s_access);
		init_sec_ace(&ace_list[i], &sid, type, sa, 0);
	}

	if ((psa = make_sec_acl(ctx, NT4_ACL_REVISION, num_aces, ace_list)) != NULL) {
		psd = make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
				    SEC_DESC_SELF_RELATIVE, NULL, NULL, NULL,
				    psa, &sd_size);
	}

	if (!psd) {
		DEBUG(0,("parse_usershare_acl: Failed to make SEC_DESC.\n"));
		return False;
	}

	*ppsd = psd;
	return True;
}
