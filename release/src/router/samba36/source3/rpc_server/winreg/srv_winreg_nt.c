/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (C) Gerald Carter                 2002-2006.
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

/* Implementation of registry functions. */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_winreg.h"
#include "registry.h"
#include "registry/reg_api.h"
#include "registry/reg_api_regf.h"
#include "registry/reg_perfcount.h"
#include "rpc_misc.h"
#include "auth.h"
#include "lib/privileges.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/******************************************************************
 Find a registry key handle and return a struct registry_key *
 *****************************************************************/

static struct registry_key *find_regkey_by_hnd(struct pipes_struct *p,
					       struct policy_handle *hnd)
{
	struct registry_key *regkey = NULL;

	if(!find_policy_by_hnd(p,hnd,(void **)(void *)&regkey)) {
		DEBUG(2,("find_regkey_index_by_hnd: Registry Key not found: "));
		return NULL;
	}

	return regkey;
}

/*******************************************************************
 Function for open a new registry handle and creating a handle
 Note that P should be valid & hnd should already have space

 When we open a key, we store the full path to the key as
 HK[LM|U]\<key>\<key>\...
 *******************************************************************/

static WERROR open_registry_key(struct pipes_struct *p,
				struct policy_handle *hnd,
				struct registry_key *parent,
				const char *subkeyname,
				uint32_t access_desired)
{
	WERROR result = WERR_OK;
	struct registry_key *key;

	if (parent == NULL) {
		result = reg_openhive(p->mem_ctx, subkeyname, access_desired,
				      p->session_info->security_token, &key);
	}
	else {
		result = reg_openkey(p->mem_ctx, parent, subkeyname,
				     access_desired, &key);
	}

	if ( !W_ERROR_IS_OK(result) ) {
		return result;
	}

	if ( !create_policy_hnd( p, hnd, key ) ) {
		return WERR_BADFILE;
	}

	return WERR_OK;
}

/*******************************************************************
 Function for open a new registry handle and creating a handle
 Note that P should be valid & hnd should already have space
 *******************************************************************/

static bool close_registry_key(struct pipes_struct *p,
			       struct policy_handle *hnd)
{
	struct registry_key *regkey = find_regkey_by_hnd(p, hnd);

	if ( !regkey ) {
		DEBUG(2,("close_registry_key: Invalid handle (%s:%u:%u)\n",
			 OUR_HANDLE(hnd)));
		return False;
	}

	close_policy_hnd(p, hnd);

	return True;
}

/********************************************************************
 _winreg_CloseKey
 ********************************************************************/

WERROR _winreg_CloseKey(struct pipes_struct *p,
			struct winreg_CloseKey *r)
{
	/* close the policy handle */

	if (!close_registry_key(p, r->in.handle))
		return WERR_BADFID;

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}

/*******************************************************************
 _winreg_OpenHKLM
 ********************************************************************/

WERROR _winreg_OpenHKLM(struct pipes_struct *p,
			struct winreg_OpenHKLM *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKLM, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKPD
 ********************************************************************/

WERROR _winreg_OpenHKPD(struct pipes_struct *p,
			struct winreg_OpenHKPD *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKPD, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKPT
 ********************************************************************/

WERROR _winreg_OpenHKPT(struct pipes_struct *p,
			struct winreg_OpenHKPT *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKPT, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKCR
 ********************************************************************/

WERROR _winreg_OpenHKCR(struct pipes_struct *p,
			struct winreg_OpenHKCR *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKCR, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKU
 ********************************************************************/

WERROR _winreg_OpenHKU(struct pipes_struct *p,
		       struct winreg_OpenHKU *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKU, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKCU
 ********************************************************************/

WERROR _winreg_OpenHKCU(struct pipes_struct *p,
			struct winreg_OpenHKCU *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKCU, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKCC
 ********************************************************************/

WERROR _winreg_OpenHKCC(struct pipes_struct *p,
			struct winreg_OpenHKCC *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKCC, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKDD
 ********************************************************************/

WERROR _winreg_OpenHKDD(struct pipes_struct *p,
			struct winreg_OpenHKDD *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKDD, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenHKPN
 ********************************************************************/

WERROR _winreg_OpenHKPN(struct pipes_struct *p,
			struct winreg_OpenHKPN *r)
{
	return open_registry_key(p, r->out.handle, NULL, KEY_HKPN, r->in.access_mask);
}

/*******************************************************************
 _winreg_OpenKey
 ********************************************************************/

WERROR _winreg_OpenKey(struct pipes_struct *p,
		       struct winreg_OpenKey *r)
{
	struct registry_key *parent = find_regkey_by_hnd(p, r->in.parent_handle );

	if ( !parent )
		return WERR_BADFID;

	return open_registry_key(p, r->out.handle, parent, r->in.keyname.name, r->in.access_mask);
}

/*******************************************************************
 _winreg_QueryValue
 ********************************************************************/

WERROR _winreg_QueryValue(struct pipes_struct *p,
			  struct winreg_QueryValue *r)
{
	WERROR        status = WERR_BADFILE;
	struct registry_key *regkey = find_regkey_by_hnd( p, r->in.handle );
	prs_struct    prs_hkpd;

	uint8_t *outbuf = NULL;
	uint32_t outbuf_size = 0;

	bool free_buf = False;
	bool free_prs = False;

	if ( !regkey )
		return WERR_BADFID;

	if (r->in.value_name->name == NULL) {
		return WERR_INVALID_PARAM;
	}

	if ((r->out.data_length == NULL) || (r->out.type == NULL) || (r->out.data_size == NULL)) {
		return WERR_INVALID_PARAM;
	}

	DEBUG(7,("_winreg_QueryValue: policy key name = [%s]\n", regkey->key->name));
	DEBUG(7,("_winreg_QueryValue: policy key type = [%08x]\n", regkey->key->type));

	/* Handle QueryValue calls on HKEY_PERFORMANCE_DATA */
	if(regkey->key->type == REG_KEY_HKPD)
	{
		if (strequal(r->in.value_name->name, "Global"))	{
			if (!prs_init(&prs_hkpd, *r->in.data_size, p->mem_ctx, MARSHALL))
				return WERR_NOMEM;
			status = reg_perfcount_get_hkpd(
				&prs_hkpd, *r->in.data_size, &outbuf_size, NULL);
			outbuf = (uint8_t *)prs_hkpd.data_p;
			free_prs = True;
		}
		else if (strequal(r->in.value_name->name, "Counter 009")) {
			outbuf_size = reg_perfcount_get_counter_names(
				reg_perfcount_get_base_index(),
				(char **)(void *)&outbuf);
			free_buf = True;
		}
		else if (strequal(r->in.value_name->name, "Explain 009")) {
			outbuf_size = reg_perfcount_get_counter_help(
				reg_perfcount_get_base_index(),
				(char **)(void *)&outbuf);
			free_buf = True;
		}
		else if (isdigit(r->in.value_name->name[0])) {
			/* we probably have a request for a specific object
			 * here */
			if (!prs_init(&prs_hkpd, *r->in.data_size, p->mem_ctx, MARSHALL))
				return WERR_NOMEM;
			status = reg_perfcount_get_hkpd(
				&prs_hkpd, *r->in.data_size, &outbuf_size,
				r->in.value_name->name);
			outbuf = (uint8_t *)prs_hkpd.data_p;
			free_prs = True;
		}
		else {
			DEBUG(3,("Unsupported key name [%s] for HKPD.\n",
				 r->in.value_name->name));
			return WERR_BADFILE;
		}

		*r->out.type = REG_BINARY;
	}
	else {
		struct registry_value *val;

		status = reg_queryvalue(p->mem_ctx, regkey, r->in.value_name->name,
					&val);
		if (!W_ERROR_IS_OK(status)) {

			DEBUG(10,("_winreg_QueryValue: reg_queryvalue failed with: %s\n",
				win_errstr(status)));

			if (r->out.data_size) {
				*r->out.data_size = 0;
			}
			if (r->out.data_length) {
				*r->out.data_length = 0;
			}
			return status;
		}

		outbuf = val->data.data;
		outbuf_size = val->data.length;
		*r->out.type = val->type;
	}

	status = WERR_BADFILE;

	if (*r->in.data_size < outbuf_size) {
		*r->out.data_size = outbuf_size;
		status = r->in.data ? WERR_MORE_DATA : WERR_OK;
	} else {
		*r->out.data_length = outbuf_size;
		*r->out.data_size = outbuf_size;
		if (r->out.data) {
			memcpy(r->out.data, outbuf, outbuf_size);
		}
		status = WERR_OK;
	}

	if (free_prs) prs_mem_free(&prs_hkpd);
	if (free_buf) SAFE_FREE(outbuf);

	return status;
}

/*****************************************************************************
 _winreg_QueryInfoKey
 ****************************************************************************/

WERROR _winreg_QueryInfoKey(struct pipes_struct *p,
			    struct winreg_QueryInfoKey *r)
{
	WERROR 	status = WERR_OK;
	struct registry_key *regkey = find_regkey_by_hnd( p, r->in.handle );

	if ( !regkey )
		return WERR_BADFID;

	r->out.classname->name = NULL;

	status = reg_queryinfokey(regkey, r->out.num_subkeys, r->out.max_subkeylen,
				  r->out.max_classlen, r->out.num_values, r->out.max_valnamelen,
				  r->out.max_valbufsize, r->out.secdescsize,
				  r->out.last_changed_time);
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	/*
	 * These calculations account for the registry buffers being
	 * UTF-16. They are inexact at best, but so far they worked.
	 */

	*r->out.max_subkeylen *= 2;

	*r->out.max_valnamelen += 1;
	*r->out.max_valnamelen *= 2;

	return WERR_OK;
}


/*****************************************************************************
 _winreg_GetVersion
 ****************************************************************************/

WERROR _winreg_GetVersion(struct pipes_struct *p,
			  struct winreg_GetVersion *r)
{
	struct registry_key *regkey = find_regkey_by_hnd( p, r->in.handle );

	if ( !regkey )
		return WERR_BADFID;

	return reg_getversion(r->out.version);
}


/*****************************************************************************
 _winreg_EnumKey
 ****************************************************************************/

WERROR _winreg_EnumKey(struct pipes_struct *p,
		       struct winreg_EnumKey *r)
{
	WERROR err = WERR_OK;
	struct registry_key *key = find_regkey_by_hnd( p, r->in.handle );

	if ( !key )
		return WERR_BADFID;

	if ( !r->in.name || !r->in.keyclass )
		return WERR_INVALID_PARAM;

	DEBUG(8,("_winreg_EnumKey: enumerating key [%s]\n", key->key->name));

	err = reg_enumkey(p->mem_ctx, key, r->in.enum_index, (char **)&r->out.name->name,
			  r->out.last_changed_time);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}
	r->out.keyclass->name = "";
	return WERR_OK;
}

/*****************************************************************************
 _winreg_EnumValue
 ****************************************************************************/

WERROR _winreg_EnumValue(struct pipes_struct *p,
			 struct winreg_EnumValue *r)
{
	WERROR err = WERR_OK;
	struct registry_key *key = find_regkey_by_hnd( p, r->in.handle );
	char *valname = NULL;
	struct registry_value *val = NULL;

	if ( !key )
		return WERR_BADFID;

	if ( !r->in.name )
		return WERR_INVALID_PARAM;

	DEBUG(8,("_winreg_EnumValue: enumerating values for key [%s]\n",
		 key->key->name));

	err = reg_enumvalue(p->mem_ctx, key, r->in.enum_index, &valname, &val);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	if (r->out.name != NULL) {
		r->out.name->name = valname;
	}

	if (r->out.type != NULL) {
		*r->out.type = val->type;
	}

	if (r->out.value != NULL) {
		if ((r->out.size == NULL) || (r->out.length == NULL)) {
			return WERR_INVALID_PARAM;
		}

		if (val->data.length > *r->out.size) {
			return WERR_MORE_DATA;
		}

		memcpy( r->out.value, val->data.data, val->data.length );
	}

	if (r->out.length != NULL) {
		*r->out.length = val->data.length;
	}
	if (r->out.size != NULL) {
		*r->out.size = val->data.length;
	}

	return WERR_OK;
}

/*******************************************************************
 _winreg_InitiateSystemShutdown
 ********************************************************************/

WERROR _winreg_InitiateSystemShutdown(struct pipes_struct *p,
				      struct winreg_InitiateSystemShutdown *r)
{
	struct winreg_InitiateSystemShutdownEx s;

	s.in.hostname = r->in.hostname;
	s.in.message = r->in.message;
	s.in.timeout = r->in.timeout;
	s.in.force_apps = r->in.force_apps;
	s.in.do_reboot = r->in.do_reboot;
	s.in.reason = 0;

	/* thunk down to _winreg_InitiateSystemShutdownEx()
	   (just returns a status) */

	return _winreg_InitiateSystemShutdownEx( p, &s );
}

/*******************************************************************
 _winreg_InitiateSystemShutdownEx
 ********************************************************************/

#define SHUTDOWN_R_STRING "-r"
#define SHUTDOWN_F_STRING "-f"


WERROR _winreg_InitiateSystemShutdownEx(struct pipes_struct *p,
					struct winreg_InitiateSystemShutdownEx *r)
{
	char *shutdown_script = NULL;
	char *msg = NULL;
	char *chkmsg = NULL;
	fstring str_timeout;
	fstring str_reason;
	fstring do_reboot;
	fstring f;
	int ret = -1;
	bool can_shutdown = false;

	shutdown_script = talloc_strdup(p->mem_ctx, lp_shutdown_script());
	if (!shutdown_script) {
		return WERR_NOMEM;
	}
	if (!*shutdown_script) {
		return WERR_ACCESS_DENIED;
	}

	/* pull the message string and perform necessary sanity checks on it */

	if ( r->in.message && r->in.message->string ) {
		if ( (msg = talloc_strdup(p->mem_ctx, r->in.message->string )) == NULL ) {
			return WERR_NOMEM;
		}
		chkmsg = TALLOC_ARRAY(p->mem_ctx, char, strlen(msg)+1);
		if (!chkmsg) {
			return WERR_NOMEM;
		}
		alpha_strcpy(chkmsg, msg, NULL, strlen(msg)+1);
	}

	fstr_sprintf(str_timeout, "%d", r->in.timeout);
	fstr_sprintf(do_reboot, r->in.do_reboot ? SHUTDOWN_R_STRING : "");
	fstr_sprintf(f, r->in.force_apps ? SHUTDOWN_F_STRING : "");
	fstr_sprintf(str_reason, "%d", r->in.reason );

	shutdown_script = talloc_all_string_sub(p->mem_ctx,
				shutdown_script, "%z", chkmsg ? chkmsg : "");
	if (!shutdown_script) {
		return WERR_NOMEM;
	}
	shutdown_script = talloc_all_string_sub(p->mem_ctx,
					shutdown_script, "%t", str_timeout);
	if (!shutdown_script) {
		return WERR_NOMEM;
	}
	shutdown_script = talloc_all_string_sub(p->mem_ctx,
						shutdown_script, "%r", do_reboot);
	if (!shutdown_script) {
		return WERR_NOMEM;
	}
	shutdown_script = talloc_all_string_sub(p->mem_ctx,
						shutdown_script, "%f", f);
	if (!shutdown_script) {
		return WERR_NOMEM;
	}
	shutdown_script = talloc_all_string_sub(p->mem_ctx,
					shutdown_script, "%x", str_reason);
	if (!shutdown_script) {
		return WERR_NOMEM;
	}

	can_shutdown = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_REMOTE_SHUTDOWN);

	/* IF someone has privs, run the shutdown script as root. OTHERWISE run it as not root
	   Take the error return from the script and provide it as the Windows return code. */

	/********** BEGIN SeRemoteShutdownPrivilege BLOCK **********/

	if ( can_shutdown )
		become_root();

	ret = smbrun( shutdown_script, NULL );

	if ( can_shutdown )
		unbecome_root();

	/********** END SeRemoteShutdownPrivilege BLOCK **********/

	DEBUG(3,("_reg_shutdown_ex: Running the command `%s' gave %d\n",
		shutdown_script, ret));

	return (ret == 0) ? WERR_OK : WERR_ACCESS_DENIED;
}

/*******************************************************************
 _winreg_AbortSystemShutdown
 ********************************************************************/

WERROR _winreg_AbortSystemShutdown(struct pipes_struct *p,
				   struct winreg_AbortSystemShutdown *r)
{
	const char *abort_shutdown_script = lp_abort_shutdown_script();
	int ret = -1;
	bool can_shutdown = false;

	if (!*abort_shutdown_script)
		return WERR_ACCESS_DENIED;

	can_shutdown = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_REMOTE_SHUTDOWN);

	/********** BEGIN SeRemoteShutdownPrivilege BLOCK **********/

	if ( can_shutdown )
		become_root();

	ret = smbrun( abort_shutdown_script, NULL );

	if ( can_shutdown )
		unbecome_root();

	/********** END SeRemoteShutdownPrivilege BLOCK **********/

	DEBUG(3,("_winreg_AbortSystemShutdown: Running the command `%s' gave %d\n",
		abort_shutdown_script, ret));

	return (ret == 0) ? WERR_OK : WERR_ACCESS_DENIED;
}

/*******************************************************************
 ********************************************************************/

static int validate_reg_filename(TALLOC_CTX *ctx, char **pp_fname )
{
	char *p = NULL;
	int num_services = lp_numservices();
	int snum = -1;
	const char *share_path = NULL;
	char *fname = *pp_fname;

	/* convert to a unix path, stripping the C:\ along the way */

	if (!(p = valid_share_pathname(ctx, fname))) {
		return -1;
	}

	/* has to exist within a valid file share */

	for (snum=0; snum<num_services; snum++) {
		if (!lp_snum_ok(snum) || lp_print_ok(snum)) {
			continue;
		}

		share_path = lp_pathname(snum);

		/* make sure we have a path (e.g. [homes] ) */
		if (strlen(share_path) == 0) {
			continue;
		}

		if (strncmp(share_path, p, strlen(share_path)) == 0) {
			break;
		}
	}

	*pp_fname = p;
	return (snum < num_services) ? snum : -1;
}

/*******************************************************************
 _winreg_RestoreKey
 ********************************************************************/

WERROR _winreg_RestoreKey(struct pipes_struct *p,
			  struct winreg_RestoreKey *r)
{
	struct registry_key *regkey = find_regkey_by_hnd( p, r->in.handle );
	char *fname = NULL;
	int snum = -1;

	if ( !regkey )
		return WERR_BADFID;

	if ( !r->in.filename || !r->in.filename->name )
		return WERR_INVALID_PARAM;

	fname = talloc_strdup(p->mem_ctx, r->in.filename->name);
	if (!fname) {
		return WERR_NOMEM;
	}

	DEBUG(8,("_winreg_RestoreKey: verifying restore of key [%s] from "
		 "\"%s\"\n", regkey->key->name, fname));

	if ((snum = validate_reg_filename(p->mem_ctx, &fname)) == -1)
		return WERR_OBJECT_PATH_INVALID;

	/* user must posses SeRestorePrivilege for this this proceed */

	if ( !security_token_has_privilege(p->session_info->security_token, SEC_PRIV_RESTORE)) {
		return WERR_ACCESS_DENIED;
	}

	DEBUG(2,("_winreg_RestoreKey: Restoring [%s] from %s in share %s\n",
		 regkey->key->name, fname, lp_servicename(snum) ));

	return reg_restorekey(regkey, fname);
}

/*******************************************************************
 _winreg_SaveKey
 ********************************************************************/

WERROR _winreg_SaveKey(struct pipes_struct *p,
		       struct winreg_SaveKey *r)
{
	struct registry_key *regkey = find_regkey_by_hnd( p, r->in.handle );
	char *fname = NULL;
	int snum = -1;

	if ( !regkey )
		return WERR_BADFID;

	if ( !r->in.filename || !r->in.filename->name )
		return WERR_INVALID_PARAM;

	fname = talloc_strdup(p->mem_ctx, r->in.filename->name);
	if (!fname) {
		return WERR_NOMEM;
	}

	DEBUG(8,("_winreg_SaveKey: verifying backup of key [%s] to \"%s\"\n",
		 regkey->key->name, fname));

	if ((snum = validate_reg_filename(p->mem_ctx, &fname)) == -1 )
		return WERR_OBJECT_PATH_INVALID;

	DEBUG(2,("_winreg_SaveKey: Saving [%s] to %s in share %s\n",
		 regkey->key->name, fname, lp_servicename(snum) ));

	return reg_savekey(regkey, fname);
}

/*******************************************************************
 _winreg_SaveKeyEx
 ********************************************************************/

WERROR _winreg_SaveKeyEx(struct pipes_struct *p,
			 struct winreg_SaveKeyEx *r)
{
	/* fill in your code here if you think this call should
	   do anything */

	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/*******************************************************************
 _winreg_CreateKey
 ********************************************************************/

WERROR _winreg_CreateKey(struct pipes_struct *p,
			 struct winreg_CreateKey *r)
{
	struct registry_key *parent = find_regkey_by_hnd(p, r->in.handle);
	struct registry_key *new_key = NULL;
	WERROR result = WERR_OK;

	if ( !parent )
		return WERR_BADFID;

	DEBUG(10, ("_winreg_CreateKey called with parent key '%s' and "
		   "subkey name '%s'\n", parent->key->name, r->in.name.name));

	result = reg_createkey(NULL, parent, r->in.name.name, r->in.access_mask,
			       &new_key, r->out.action_taken);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	if (!create_policy_hnd(p, r->out.new_handle, new_key)) {
		TALLOC_FREE(new_key);
		return WERR_BADFILE;
	}

	return WERR_OK;
}

/*******************************************************************
 _winreg_SetValue
 ********************************************************************/

WERROR _winreg_SetValue(struct pipes_struct *p,
			struct winreg_SetValue *r)
{
	struct registry_key *key = find_regkey_by_hnd(p, r->in.handle);
	struct registry_value *val = NULL;

	if ( !key )
		return WERR_BADFID;

	DEBUG(8,("_winreg_SetValue: Setting value for [%s:%s]\n",
			 key->key->name, r->in.name.name));

	val = talloc_zero(p->mem_ctx, struct registry_value);
	if (val == NULL) {
		return WERR_NOMEM;
	}

	val->type = r->in.type;
	val->data = data_blob_talloc(p->mem_ctx, r->in.data, r->in.size);

	return reg_setvalue(key, r->in.name.name, val);
}

/*******************************************************************
 _winreg_DeleteKey
 ********************************************************************/

WERROR _winreg_DeleteKey(struct pipes_struct *p,
			 struct winreg_DeleteKey *r)
{
	struct registry_key *parent = find_regkey_by_hnd(p, r->in.handle);

	if ( !parent )
		return WERR_BADFID;

	return reg_deletekey(parent, r->in.key.name);
}


/*******************************************************************
 _winreg_DeleteValue
 ********************************************************************/

WERROR _winreg_DeleteValue(struct pipes_struct *p,
			   struct winreg_DeleteValue *r)
{
	struct registry_key *key = find_regkey_by_hnd(p, r->in.handle);

	if ( !key )
		return WERR_BADFID;

	return reg_deletevalue(key, r->in.value.name);
}

/*******************************************************************
 _winreg_GetKeySecurity
 ********************************************************************/

WERROR _winreg_GetKeySecurity(struct pipes_struct *p,
			      struct winreg_GetKeySecurity *r)
{
	struct registry_key *key = find_regkey_by_hnd(p, r->in.handle);
	WERROR err = WERR_OK;
	struct security_descriptor *secdesc = NULL;
	uint8 *data = NULL;
	size_t len = 0;

	if ( !key )
		return WERR_BADFID;

	/* access checks first */

	if ( !(key->key->access_granted & SEC_STD_READ_CONTROL) )
		return WERR_ACCESS_DENIED;

	err = reg_getkeysecurity(p->mem_ctx, key, &secdesc);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	err = ntstatus_to_werror(marshall_sec_desc(p->mem_ctx, secdesc,
						   &data, &len));
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	if (len > r->out.sd->size) {
		r->out.sd->size = len;
		return WERR_INSUFFICIENT_BUFFER;
	}

	r->out.sd->size = len;
	r->out.sd->len = len;
	r->out.sd->data = data;

	return WERR_OK;
}

/*******************************************************************
 _winreg_SetKeySecurity
 ********************************************************************/

WERROR _winreg_SetKeySecurity(struct pipes_struct *p,
			      struct winreg_SetKeySecurity *r)
{
	struct registry_key *key = find_regkey_by_hnd(p, r->in.handle);
	struct security_descriptor *secdesc = NULL;
	WERROR err = WERR_OK;

	if ( !key )
		return WERR_BADFID;

	/* access checks first */

	if ( !(key->key->access_granted & SEC_STD_WRITE_DAC) )
		return WERR_ACCESS_DENIED;

	err = ntstatus_to_werror(unmarshall_sec_desc(p->mem_ctx, r->in.sd->data,
						     r->in.sd->len, &secdesc));
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	return reg_setkeysecurity(key, secdesc);
}

/*******************************************************************
 _winreg_FlushKey
 ********************************************************************/

WERROR _winreg_FlushKey(struct pipes_struct *p,
			struct winreg_FlushKey *r)
{
	/* I'm just replying OK because there's not a lot
	   here I see to do i  --jerry */

	return WERR_OK;
}

/*******************************************************************
 _winreg_UnLoadKey
 ********************************************************************/

WERROR _winreg_UnLoadKey(struct pipes_struct *p,
			 struct winreg_UnLoadKey *r)
{
	/* fill in your code here if you think this call should
	   do anything */

	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/*******************************************************************
 _winreg_ReplaceKey
 ********************************************************************/

WERROR _winreg_ReplaceKey(struct pipes_struct *p,
			  struct winreg_ReplaceKey *r)
{
	/* fill in your code here if you think this call should
	   do anything */

	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/*******************************************************************
 _winreg_LoadKey
 ********************************************************************/

WERROR _winreg_LoadKey(struct pipes_struct *p,
		       struct winreg_LoadKey *r)
{
	/* fill in your code here if you think this call should
	   do anything */

	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/*******************************************************************
 _winreg_NotifyChangeKeyValue
 ********************************************************************/

WERROR _winreg_NotifyChangeKeyValue(struct pipes_struct *p,
				    struct winreg_NotifyChangeKeyValue *r)
{
	return WERR_NOT_SUPPORTED;
}

/*******************************************************************
 _winreg_QueryMultipleValues
 ********************************************************************/

WERROR _winreg_QueryMultipleValues(struct pipes_struct *p,
				   struct winreg_QueryMultipleValues *r)
{
	struct winreg_QueryMultipleValues2 r2;
	uint32_t needed = 0;

	r2.in.key_handle	= r->in.key_handle;
	r2.in.values_in		= r->in.values_in;
	r2.in.num_values	= r->in.num_values;
	r2.in.offered		= r->in.buffer_size;
	r2.in.buffer		= r->in.buffer;
	r2.out.values_out	= r->out.values_out;
	r2.out.needed		= &needed;
	r2.out.buffer		= r->out.buffer;

	return _winreg_QueryMultipleValues2(p, &r2);
}

/*******************************************************************
 ********************************************************************/

static WERROR construct_multiple_entry(TALLOC_CTX *mem_ctx,
				       const char *valuename,
				       uint32_t value_length,
				       uint32_t offset,
				       enum winreg_Type type,
				       struct QueryMultipleValue *r)
{
	r->ve_valuename = talloc_zero(mem_ctx, struct winreg_ValNameBuf);
	if (r->ve_valuename == NULL) {
		return WERR_NOMEM;
	}

	r->ve_valuename->name = talloc_strdup(r->ve_valuename, valuename ? valuename : "");
	if (r->ve_valuename->name == NULL) {
		return WERR_NOMEM;
	}

	r->ve_valuename->size = strlen_m_term(r->ve_valuename->name)*2;
	r->ve_valuelen = value_length;
	r->ve_valueptr = offset;
	r->ve_type = type;

	return WERR_OK;
}

/*******************************************************************
 _winreg_QueryMultipleValues2
 ********************************************************************/

WERROR _winreg_QueryMultipleValues2(struct pipes_struct *p,
				    struct winreg_QueryMultipleValues2 *r)
{
	struct registry_key *regkey = find_regkey_by_hnd(p, r->in.key_handle);
	struct registry_value *vals = NULL;
	const char **names = NULL;
	uint32_t offset = 0, num_vals = 0;
	DATA_BLOB result = data_blob_null;
	int i = 0;
	WERROR err = WERR_OK;

	if (!regkey) {
		return WERR_BADFID;
	}

	names = talloc_zero_array(p->mem_ctx, const char *, r->in.num_values);
	if (names == NULL) {
		return WERR_NOMEM;
	}

	for (i=0; i < r->in.num_values; i++) {
		if (r->in.values_in[i].ve_valuename &&
		    r->in.values_in[i].ve_valuename->name) {
			names[i] = talloc_strdup(names,
				r->in.values_in[i].ve_valuename->name);
			if (names[i] == NULL) {
				return WERR_NOMEM;
			}
		}
	}

	err = reg_querymultiplevalues(p->mem_ctx, regkey,
				      r->in.num_values, names,
				      &num_vals, &vals);
	if (!W_ERROR_IS_OK(err)) {
		return err;
	}

	result = data_blob_talloc(p->mem_ctx, NULL, 0);

	for (i=0; i < r->in.num_values; i++) {
		const char *valuename = NULL;

		if (vals[i].data.length > 0) {
			if (!data_blob_append(p->mem_ctx, &result,
					      vals[i].data.data,
					      vals[i].data.length)) {
				return WERR_NOMEM;
			}
		}

		if (r->in.values_in[i].ve_valuename &&
		    r->in.values_in[i].ve_valuename->name) {
			valuename = r->in.values_in[i].ve_valuename->name;
		}

		err = construct_multiple_entry(r->out.values_out,
					       valuename,
					       vals[i].data.length,
					       offset,
					       vals[i].type,
					       &r->out.values_out[i]);
		if (!W_ERROR_IS_OK(err)) {
			return err;
		}

		offset += vals[i].data.length;
	}

	*r->out.needed = result.length;

	if (r->in.num_values != num_vals) {
		return WERR_BADFILE;
	}

	if (*r->in.offered >= *r->out.needed) {
		if (r->out.buffer) {
			memcpy(r->out.buffer, result.data, MIN(result.length, *r->in.offered));
		}
		return WERR_OK;
	} else {
		return WERR_MORE_DATA;
	}
}

/*******************************************************************
 _winreg_DeleteKeyEx
 ********************************************************************/

WERROR _winreg_DeleteKeyEx(struct pipes_struct *p,
			   struct winreg_DeleteKeyEx *r)
{
	/* fill in your code here if you think this call should
	   do anything */

	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}
