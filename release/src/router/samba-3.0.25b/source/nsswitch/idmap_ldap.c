/*
   Unix SMB/CIFS implementation.

   idmap LDAP backend

   Copyright (C) Tim Potter 		2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2003
   Copyright (C) Gerald Carter 		2003
   Copyright (C) Simo Sorce 		2003-2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

#include <lber.h>
#include <ldap.h>

#include "smbldap.h"

struct idmap_ldap_context {
	struct smbldap_state *smbldap_state;
	char *url;
	char *suffix;
	char *user_dn;
	uint32_t filter_low_id, filter_high_id;		/* Filter range */
	BOOL anon;
};

struct idmap_ldap_alloc_context {
	struct smbldap_state *smbldap_state;
	char *url;
	char *suffix;
	char *user_dn;
	uid_t low_uid, high_uid;      /* Range of uids */
	gid_t low_gid, high_gid;      /* Range of gids */

};

#define CHECK_ALLOC_DONE(mem) do { if (!mem) { DEBUG(0, ("Out of memory!\n")); ret = NT_STATUS_NO_MEMORY; goto done; } } while (0)

/**********************************************************************
 IDMAP ALLOC TDB BACKEND
**********************************************************************/
 
static struct idmap_ldap_alloc_context *idmap_alloc_ldap;

/*********************************************************************
 ********************************************************************/

static NTSTATUS get_credentials( TALLOC_CTX *mem_ctx, 
				 struct smbldap_state *ldap_state,
				 const char *config_option,
				 struct idmap_domain *dom,
				 char **dn )
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	char *user_dn = NULL;	
	char *secret = NULL;
	const char *tmp = NULL;
	
	/* assume anonymous if we don't have a specified user */

	tmp = lp_parm_const_string(-1, config_option, "ldap_user_dn", NULL);

	if ( tmp ) {
		if (!dom) {
			/* only the alloc backend is allowed to pass in a NULL dom */
			secret = idmap_fetch_secret("ldap", true, NULL, tmp);
		} else {
			secret = idmap_fetch_secret("ldap", false, dom->name, tmp);
		} 

		if (!secret) {
			DEBUG(0, ("get_credentials: Unable to fetch "
				  "auth credentials for %s in %s\n",
				  tmp, (dom==NULL)?"ALLOC":dom->name));
			ret = NT_STATUS_ACCESS_DENIED;
			goto done;
		} 		
		*dn = talloc_strdup(mem_ctx, tmp);
		CHECK_ALLOC_DONE(*dn);		
	} else {
		if ( !fetch_ldap_pw( &user_dn, &secret ) ) {
			DEBUG(2, ("get_credentials: Failed to lookup ldap "
				  "bind creds.  Using anonymous connection.\n"));
			*dn = talloc_strdup( mem_ctx, "" );			
		} else {
			*dn = talloc_strdup(mem_ctx, user_dn);
			SAFE_FREE( user_dn );		
			CHECK_ALLOC_DONE(*dn);
		}		
	}

	smbldap_set_creds(ldap_state, false, *dn, secret);
	ret = NT_STATUS_OK;
	
 done:
	SAFE_FREE( secret );

	return ret;	
}


/**********************************************************************
 Verify the sambaUnixIdPool entry in the directory.  
**********************************************************************/

static NTSTATUS verify_idpool(void)
{
	NTSTATUS ret;
	TALLOC_CTX *ctx;
	LDAPMessage *result = NULL;
	LDAPMod **mods = NULL;
	const char **attr_list;
	char *filter;
	int count;
	int rc;
	
	if ( ! idmap_alloc_ldap) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx = talloc_new(idmap_alloc_ldap);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	filter = talloc_asprintf(ctx, "(objectclass=%s)", LDAP_OBJ_IDPOOL);
	CHECK_ALLOC_DONE(filter);
	
	attr_list = get_attr_list(ctx, idpool_attr_list);
	CHECK_ALLOC_DONE(attr_list);

	rc = smbldap_search(idmap_alloc_ldap->smbldap_state,
				idmap_alloc_ldap->suffix, 
				LDAP_SCOPE_SUBTREE,
				filter,
				attr_list,
				0,
				&result);

	if (rc != LDAP_SUCCESS) {
		DEBUG(1, ("Unable to verify the idpool, cannot continue initialization!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	count = ldap_count_entries(idmap_alloc_ldap->smbldap_state->ldap_struct, result);

	ldap_msgfree(result);

	if ( count > 1 ) {
		DEBUG(0,("Multiple entries returned from %s (base == %s)\n",
			filter, idmap_alloc_ldap->suffix));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}
	else if (count == 0) {
		char *uid_str, *gid_str;
		
		uid_str = talloc_asprintf(ctx, "%lu", (unsigned long)idmap_alloc_ldap->low_uid);
		gid_str = talloc_asprintf(ctx, "%lu", (unsigned long)idmap_alloc_ldap->low_gid);

		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				"objectClass", LDAP_OBJ_IDPOOL);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, 
				get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER),
				uid_str);
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER),
				gid_str);
		if (mods) {
			rc = smbldap_modify(idmap_alloc_ldap->smbldap_state,
						idmap_alloc_ldap->suffix,
						mods);
			ldap_mods_free(mods, True);
		} else {
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	ret = (rc == LDAP_SUCCESS)?NT_STATUS_OK:NT_STATUS_UNSUCCESSFUL;
done:
	talloc_free(ctx);
	return ret;
}

/*****************************************************************************
 Initialise idmap database. 
*****************************************************************************/

static NTSTATUS idmap_ldap_alloc_init(const char *params)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;	
	const char *range;
	const char *tmp;
	uid_t low_uid = 0;
	uid_t high_uid = 0;
	gid_t low_gid = 0;
	gid_t high_gid = 0;

	/* Only do init if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	idmap_alloc_ldap = TALLOC_ZERO_P(NULL, struct idmap_ldap_alloc_context);
        CHECK_ALLOC_DONE( idmap_alloc_ldap );
	
	/* load ranges */

	idmap_alloc_ldap->low_uid = 0;
	idmap_alloc_ldap->high_uid = 0;
	idmap_alloc_ldap->low_gid = 0;
	idmap_alloc_ldap->high_gid = 0;

	range = lp_parm_const_string(-1, "idmap alloc config", "range", NULL);
	if (range && range[0]) {
		unsigned low_id, high_id;

		if (sscanf(range, "%u - %u", &low_id, &high_id) == 2) {
			if (low_id < high_id) {
				idmap_alloc_ldap->low_gid = idmap_alloc_ldap->low_uid = low_id;
				idmap_alloc_ldap->high_gid = idmap_alloc_ldap->high_uid = high_id;
			} else {
				DEBUG(1, ("ERROR: invalid idmap alloc range [%s]", range));
			}
		} else {
			DEBUG(1, ("ERROR: invalid syntax for idmap alloc config:range [%s]", range));
		}
	}

	if (lp_idmap_uid(&low_uid, &high_uid)) {
		idmap_alloc_ldap->low_uid = low_uid;
		idmap_alloc_ldap->high_uid = high_uid;
	}

	if (lp_idmap_gid(&low_gid, &high_gid)) {
		idmap_alloc_ldap->low_gid = low_gid;
		idmap_alloc_ldap->high_gid= high_gid;
	}

	if (idmap_alloc_ldap->high_uid <= idmap_alloc_ldap->low_uid) {
		DEBUG(1, ("idmap uid range missing or invalid\n"));
		DEBUGADD(1, ("idmap will be unable to map foreign SIDs\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (idmap_alloc_ldap->high_gid <= idmap_alloc_ldap->low_gid) {
		DEBUG(1, ("idmap gid range missing or invalid\n"));
		DEBUGADD(1, ("idmap will be unable to map foreign SIDs\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (params && *params) {
		/* assume location is the only parameter */
		idmap_alloc_ldap->url = talloc_strdup(idmap_alloc_ldap, params);
	} else {
		tmp = lp_parm_const_string(-1, "idmap alloc config", "ldap_url", NULL);

		if ( ! tmp) {
			DEBUG(1, ("ERROR: missing idmap ldap url\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		
		idmap_alloc_ldap->url = talloc_strdup(idmap_alloc_ldap, tmp);
	}
	CHECK_ALLOC_DONE( idmap_alloc_ldap->url );

	tmp = lp_ldap_idmap_suffix();
	if ( ! tmp || ! *tmp) {
		tmp = lp_parm_const_string(-1, "idmap alloc config", "ldap_base_dn", NULL);
	}
	if ( ! tmp) {
		tmp = lp_ldap_suffix();
		if (tmp) {
			DEBUG(1, ("WARNING: Trying to use the global ldap suffix(%s)\n", tmp));
			DEBUGADD(1, ("as suffix. This may not be what you want!\n"));
		}
		if ( ! tmp) {
			DEBUG(1, ("ERROR: missing idmap ldap suffix\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	idmap_alloc_ldap->suffix = talloc_strdup(idmap_alloc_ldap, tmp);
	CHECK_ALLOC_DONE( idmap_alloc_ldap->suffix );
	
	ret = smbldap_init(idmap_alloc_ldap, idmap_alloc_ldap->url,
				 &idmap_alloc_ldap->smbldap_state);	
	if (!NT_STATUS_IS_OK(ret)) { 
		DEBUG(1, ("ERROR: smbldap_init (%s) failed!\n", 
			  idmap_alloc_ldap->url));
		goto done;		
	}

        ret = get_credentials( idmap_alloc_ldap, 
			       idmap_alloc_ldap->smbldap_state, 
			       "idmap alloc config", NULL,
			       &idmap_alloc_ldap->user_dn );
	if ( !NT_STATUS_IS_OK(ret) ) {
		DEBUG(1,("idmap_ldap_alloc_init: Failed to get connection "
			 "credentials (%s)\n", nt_errstr(ret)));
		goto done;
	}	

	/* see if the idmap suffix and sub entries exists */

	ret = verify_idpool();	

 done:
	if ( !NT_STATUS_IS_OK( ret ) )
		TALLOC_FREE( idmap_alloc_ldap );
	
	return ret;
}

/********************************
 Allocate a new uid or gid
********************************/

static NTSTATUS idmap_ldap_allocate_id(struct unixid *xid)
{
	TALLOC_CTX *ctx;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	int rc = LDAP_SERVER_DOWN;
	int count = 0;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	char *id_str;
	char *new_id_str;
	char *filter = NULL;
	const char *dn = NULL;
	const char **attr_list;
	const char *type;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	if ( ! idmap_alloc_ldap) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx = talloc_new(idmap_alloc_ldap);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* get type */
	switch (xid->type) {

	case ID_TYPE_UID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER);
	       	break;

	case ID_TYPE_GID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER);
		break;

	default:
		DEBUG(2, ("Invalid ID type (0x%x)\n", xid->type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	filter = talloc_asprintf(ctx, "(objectClass=%s)", LDAP_OBJ_IDPOOL);
	CHECK_ALLOC_DONE(filter);

	attr_list = get_attr_list(ctx, idpool_attr_list);
	CHECK_ALLOC_DONE(attr_list);

	DEBUG(10, ("Search of the id pool (filter: %s)\n", filter));

	rc = smbldap_search(idmap_alloc_ldap->smbldap_state,
				idmap_alloc_ldap->suffix,
			       LDAP_SCOPE_SUBTREE, filter,
			       attr_list, 0, &result);
	 
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("%s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	talloc_autofree_ldapmsg(ctx, result);
	
	count = ldap_count_entries(idmap_alloc_ldap->smbldap_state->ldap_struct, result);
	if (count != 1) {
		DEBUG(0,("Single %s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	entry = ldap_first_entry(idmap_alloc_ldap->smbldap_state->ldap_struct, result);

	dn = smbldap_talloc_dn(ctx, idmap_alloc_ldap->smbldap_state->ldap_struct, entry);
	if ( ! dn) {
		goto done;
	}

	if ( ! (id_str = smbldap_talloc_single_attribute(idmap_alloc_ldap->smbldap_state->ldap_struct,
				entry, type, ctx))) {
		DEBUG(0,("%s attribute not found\n", type));
		goto done;
	}
	if ( ! id_str) {
		DEBUG(0,("Out of memory\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	xid->id = strtoul(id_str, NULL, 10);

	/* make sure we still have room to grow */

	switch (xid->type) {
	case ID_TYPE_UID:
		if (xid->id > idmap_alloc_ldap->high_uid) {
			DEBUG(0,("Cannot allocate uid above %lu!\n", 
				 (unsigned long)idmap_alloc_ldap->high_uid));
			goto done;
		}
		break;
		
	case ID_TYPE_GID: 
		if (xid->id > idmap_alloc_ldap->high_gid) {
			DEBUG(0,("Cannot allocate gid above %lu!\n", 
				 (unsigned long)idmap_alloc_ldap->high_uid));
			goto done;
		}
		break;

	default:
		/* impossible */
		goto done;
	}
	
	new_id_str = talloc_asprintf(ctx, "%lu", (unsigned long)xid->id + 1);
	if ( ! new_id_str) {
		DEBUG(0,("Out of memory\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}
		 
	smbldap_set_mod(&mods, LDAP_MOD_DELETE, type, id_str);
	smbldap_set_mod(&mods, LDAP_MOD_ADD, type, new_id_str);

	if (mods == NULL) {
		DEBUG(0,("smbldap_set_mod() failed.\n"));
		goto done;		
	}

	DEBUG(10, ("Try to atomically increment the id (%s -> %s)\n", id_str, new_id_str));

	rc = smbldap_modify(idmap_alloc_ldap->smbldap_state, dn, mods);

	ldap_mods_free(mods, True);

	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("Failed to allocate new %s. smbldap_modify() failed.\n", type));
		goto done;
	}
	
	ret = NT_STATUS_OK;

done:
	talloc_free(ctx);
	return ret;
}

/**********************************
 Get current highest id. 
**********************************/

static NTSTATUS idmap_ldap_get_hwm(struct unixid *xid)
{
	TALLOC_CTX *memctx;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	int rc = LDAP_SERVER_DOWN;
	int count = 0;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	char *id_str;
	char *filter = NULL;
	const char **attr_list;
	const char *type;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	if ( ! idmap_alloc_ldap) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	memctx = talloc_new(idmap_alloc_ldap);
	if ( ! memctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* get type */
	switch (xid->type) {

	case ID_TYPE_UID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER);
	       	break;

	case ID_TYPE_GID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER);
		break;

	default:
		DEBUG(2, ("Invalid ID type (0x%x)\n", xid->type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	filter = talloc_asprintf(memctx, "(objectClass=%s)", LDAP_OBJ_IDPOOL);
	CHECK_ALLOC_DONE(filter);

	attr_list = get_attr_list(memctx, idpool_attr_list);
	CHECK_ALLOC_DONE(attr_list);

	rc = smbldap_search(idmap_alloc_ldap->smbldap_state,
				idmap_alloc_ldap->suffix,
			       LDAP_SCOPE_SUBTREE, filter,
			       attr_list, 0, &result);
	 
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("%s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	talloc_autofree_ldapmsg(memctx, result);
	
	count = ldap_count_entries(idmap_alloc_ldap->smbldap_state->ldap_struct, result);
	if (count != 1) {
		DEBUG(0,("Single %s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	entry = ldap_first_entry(idmap_alloc_ldap->smbldap_state->ldap_struct, result);

	id_str = smbldap_talloc_single_attribute(idmap_alloc_ldap->smbldap_state->ldap_struct,
			entry, type, memctx);
	if ( ! id_str) {
		DEBUG(0,("%s attribute not found\n", type));
		goto done;
	}
	if ( ! id_str) {
		DEBUG(0,("Out of memory\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	xid->id	= strtoul(id_str, NULL, 10);
	
	ret = NT_STATUS_OK;
done:
	talloc_free(memctx);
	return ret;
}
/**********************************
 Set highest id. 
**********************************/

static NTSTATUS idmap_ldap_set_hwm(struct unixid *xid)
{
	TALLOC_CTX *ctx;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	int rc = LDAP_SERVER_DOWN;
	int count = 0;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	char *new_id_str;
	char *filter = NULL;
	const char *dn = NULL;
	const char **attr_list;
	const char *type;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	if ( ! idmap_alloc_ldap) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx = talloc_new(idmap_alloc_ldap);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* get type */
	switch (xid->type) {

	case ID_TYPE_UID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER);
	       	break;

	case ID_TYPE_GID:
		type = get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER);
		break;

	default:
		DEBUG(2, ("Invalid ID type (0x%x)\n", xid->type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	filter = talloc_asprintf(ctx, "(objectClass=%s)", LDAP_OBJ_IDPOOL);
	CHECK_ALLOC_DONE(filter);

	attr_list = get_attr_list(ctx, idpool_attr_list);
	CHECK_ALLOC_DONE(attr_list);

	rc = smbldap_search(idmap_alloc_ldap->smbldap_state,
				idmap_alloc_ldap->suffix,
			       LDAP_SCOPE_SUBTREE, filter,
			       attr_list, 0, &result);
	 
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("%s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	talloc_autofree_ldapmsg(ctx, result);
	
	count = ldap_count_entries(idmap_alloc_ldap->smbldap_state->ldap_struct, result);
	if (count != 1) {
		DEBUG(0,("Single %s object not found\n", LDAP_OBJ_IDPOOL));
		goto done;
	}

	entry = ldap_first_entry(idmap_alloc_ldap->smbldap_state->ldap_struct, result);

	dn = smbldap_talloc_dn(ctx, idmap_alloc_ldap->smbldap_state->ldap_struct, entry);
	if ( ! dn) {
		goto done;
	}

	new_id_str = talloc_asprintf(ctx, "%lu", (unsigned long)xid->id);
	if ( ! new_id_str) {
		DEBUG(0,("Out of memory\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}
		 
	smbldap_set_mod(&mods, LDAP_MOD_REPLACE, type, new_id_str);

	if (mods == NULL) {
		DEBUG(0,("smbldap_set_mod() failed.\n"));
		goto done;		
	}

	rc = smbldap_modify(idmap_alloc_ldap->smbldap_state, dn, mods);

	ldap_mods_free(mods, True);

	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("Failed to allocate new %s. smbldap_modify() failed.\n", type));
		goto done;
	}
	
	ret = NT_STATUS_OK;

done:
	talloc_free(ctx);
	return ret;
}

/**********************************
 Close idmap ldap alloc
**********************************/

static NTSTATUS idmap_ldap_alloc_close(void)
{
	if (idmap_alloc_ldap) {
		smbldap_free_struct(&idmap_alloc_ldap->smbldap_state);
		DEBUG(5,("The connection to the LDAP server was closed\n"));
		/* maybe free the results here --metze */
		TALLOC_FREE(idmap_alloc_ldap);
	}
	return NT_STATUS_OK;
}


/**********************************************************************
 IDMAP MAPPING LDAP BACKEND
**********************************************************************/
 
static int idmap_ldap_close_destructor(struct idmap_ldap_context *ctx)
{
	smbldap_free_struct(&ctx->smbldap_state);
	DEBUG(5,("The connection to the LDAP server was closed\n"));
	/* maybe free the results here --metze */

	return 0;
}

/********************************
 Initialise idmap database. 
********************************/

static NTSTATUS idmap_ldap_db_init(struct idmap_domain *dom)
{
	NTSTATUS ret;
	struct idmap_ldap_context *ctx = NULL;
	char *config_option = NULL;
	const char *range = NULL;
	const char *tmp = NULL;

	/* Only do init if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	ctx = TALLOC_ZERO_P(dom, struct idmap_ldap_context);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	config_option = talloc_asprintf(ctx, "idmap config %s", dom->name);
	if ( ! config_option) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* load ranges */
	range = lp_parm_const_string(-1, config_option, "range", NULL);
	if (range && range[0]) {
		if ((sscanf(range, "%u - %u", &ctx->filter_low_id, &ctx->filter_high_id) != 2) ||
		    (ctx->filter_low_id > ctx->filter_high_id)) {
			DEBUG(1, ("ERROR: invalid filter range [%s]", range));
			ctx->filter_low_id = 0;
			ctx->filter_high_id = 0;
		}
	}

	if (dom->params && *(dom->params)) {
		/* assume location is the only parameter */
		ctx->url = talloc_strdup(ctx, dom->params);
	} else {
		tmp = lp_parm_const_string(-1, config_option, "ldap_url", NULL);

		if ( ! tmp) {
			DEBUG(1, ("ERROR: missing idmap ldap url\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		
		ctx->url = talloc_strdup(ctx, tmp);
	}
	CHECK_ALLOC_DONE(ctx->url);

	tmp = lp_ldap_idmap_suffix();
	if ( ! tmp || ! *tmp) {
		tmp = lp_parm_const_string(-1, config_option, "ldap_base_dn", NULL);
	}
	if ( ! tmp) {
		tmp = lp_ldap_suffix();
		if (tmp) {
			DEBUG(1, ("WARNING: Trying to use the global ldap suffix(%s)\n", tmp));
			DEBUGADD(1, ("as suffix. This may not be what you want!\n"));
		} else {
			DEBUG(1, ("ERROR: missing idmap ldap suffix\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}		
	}
	ctx->suffix = talloc_strdup(ctx, tmp);
	CHECK_ALLOC_DONE(ctx->suffix);

	ret = smbldap_init(ctx, ctx->url, &ctx->smbldap_state);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(1, ("ERROR: smbldap_init (%s) failed!\n", ctx->url));
		goto done;
	}

        ret = get_credentials( ctx, ctx->smbldap_state, config_option, 
			       dom, &ctx->user_dn );
	if ( !NT_STATUS_IS_OK(ret) ) {
		DEBUG(1,("idmap_ldap_db_init: Failed to get connection "
			 "credentials (%s)\n", nt_errstr(ret)));
		goto done;
	}	
	
	/* set the destructor on the context, so that resource are properly
	   freed if the contexts is released */

	talloc_set_destructor(ctx, idmap_ldap_close_destructor);

	dom->private_data = ctx;
	dom->initialized = True;

	talloc_free(config_option);
	return NT_STATUS_OK;

/*failed */
done:
	talloc_free(ctx);
	return ret;
}

/* max number of ids requested per batch query */
#define IDMAP_LDAP_MAX_IDS 30 

/**********************************
 lookup a set of unix ids. 
**********************************/

/* this function searches up to IDMAP_LDAP_MAX_IDS entries in maps for a match */
static struct id_map *find_map_by_id(struct id_map **maps, enum id_type type, uint32_t id)
{
	int i;

	for (i = 0; i < IDMAP_LDAP_MAX_IDS; i++) {
		if (maps[i] == NULL) { /* end of the run */
			return NULL;
		}
		if ((maps[i]->xid.type == type) && (maps[i]->xid.id == id)) {
			return maps[i];
		}
	}

	return NULL;	
}

static NTSTATUS idmap_ldap_unixids_to_sids(struct idmap_domain *dom, struct id_map **ids)
{
	NTSTATUS ret;
	TALLOC_CTX *memctx;
	struct idmap_ldap_context *ctx;
	LDAPMessage *result = NULL;
	const char *uidNumber;
	const char *gidNumber;
	const char **attr_list;
	char *filter = NULL;
	BOOL multi = False;
	int idx = 0;
	int bidx = 0;
	int count;
	int rc;
	int i;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* Initilization my have been deferred because we were offline */
	if ( ! dom->initialized) {
		ret = idmap_ldap_db_init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			return ret;
		}
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ldap_context);	

	memctx = talloc_new(ctx);
	if ( ! memctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	uidNumber = get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER);
	gidNumber = get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER);

	attr_list = get_attr_list(ctx, sidmap_attr_list);

	if ( ! ids[1]) {
		/* if we are requested just one mapping use the simple filter */

		filter = talloc_asprintf(memctx, "(&(objectClass=%s)(%s=%lu))",
				LDAP_OBJ_IDMAP_ENTRY,
				(ids[0]->xid.type==ID_TYPE_UID)?uidNumber:gidNumber,
				(unsigned long)ids[0]->xid.id);
		CHECK_ALLOC_DONE(filter);
		DEBUG(10, ("Filter: [%s]\n", filter));
	} else {
		/* multiple mappings */
		multi = True;
	}

again:
	if (multi) {

		talloc_free(filter);
		filter = talloc_asprintf(memctx, "(&(objectClass=%s)(|", LDAP_OBJ_IDMAP_ENTRY);
		CHECK_ALLOC_DONE(filter);

		bidx = idx;
		for (i = 0; (i < IDMAP_LDAP_MAX_IDS) && ids[idx]; i++, idx++) {
			filter = talloc_asprintf_append(filter, "(%s=%lu)",
					(ids[idx]->xid.type==ID_TYPE_UID)?uidNumber:gidNumber,
					(unsigned long)ids[idx]->xid.id);
			CHECK_ALLOC_DONE(filter);
		}
		filter = talloc_asprintf_append(filter, "))");
		CHECK_ALLOC_DONE(filter);
		DEBUG(10, ("Filter: [%s]\n", filter));
	} else {
		bidx = 0;
		idx = 1;
	}

	rc = smbldap_search(ctx->smbldap_state, ctx->suffix, LDAP_SCOPE_SUBTREE, 
		filter, attr_list, 0, &result);

	if (rc != LDAP_SUCCESS) {
		DEBUG(3,("Failure looking up ids (%s)\n", ldap_err2string(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	count = ldap_count_entries(ctx->smbldap_state->ldap_struct, result);

	if (count == 0) {
		DEBUG(10, ("NO SIDs found\n"));
	}

	for (i = 0; i < count; i++) {
		LDAPMessage *entry = NULL;
		char *sidstr = NULL;
	       	char *tmp = NULL;
		enum id_type type;
		struct id_map *map;
		uint32_t id;

		if (i == 0) { /* first entry */
			entry = ldap_first_entry(ctx->smbldap_state->ldap_struct, result);
		} else { /* following ones */
			entry = ldap_next_entry(ctx->smbldap_state->ldap_struct, entry);
		}
		if ( ! entry) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		sidstr = smbldap_talloc_single_attribute(
				ctx->smbldap_state->ldap_struct,
				entry, LDAP_ATTRIBUTE_SID, memctx);
		if ( ! sidstr) { /* no sid, skip entry */
			DEBUG(2, ("WARNING SID not found on entry\n"));
			continue;
		}

		/* now try to see if it is a uid, if not try with a gid
		 * (gid is more common, but in case both uidNumber and
		 * gidNumber are returned the SID is mapped to the uid not the gid) */
		type = ID_TYPE_UID;
		tmp = smbldap_talloc_single_attribute(
				ctx->smbldap_state->ldap_struct,
				entry, uidNumber, memctx);
		if ( ! tmp) {
			type = ID_TYPE_GID;
			tmp = smbldap_talloc_single_attribute(
					ctx->smbldap_state->ldap_struct,
					entry, gidNumber, memctx);
		}
		if ( ! tmp) { /* wow very strange entry, how did it match ? */
			DEBUG(5, ("Unprobable match on (%s), no uidNumber, nor gidNumber returned\n", sidstr));
			TALLOC_FREE(sidstr);
			continue;
		}

		id = strtoul(tmp, NULL, 10);
		if ((id == 0) ||
		    (ctx->filter_low_id && (id < ctx->filter_low_id)) ||
		    (ctx->filter_high_id && (id > ctx->filter_high_id))) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, ctx->filter_low_id, ctx->filter_high_id));
			TALLOC_FREE(sidstr);
			TALLOC_FREE(tmp);
			continue;
		}
		TALLOC_FREE(tmp);

		map = find_map_by_id(&ids[bidx], type, id);
		if (!map) {
			DEBUG(2, ("WARNING: couldn't match sid (%s) with requested ids\n", sidstr));
			TALLOC_FREE(sidstr);
			continue;
		}

		if ( ! string_to_sid(map->sid, sidstr)) {
			DEBUG(2, ("ERROR: Invalid SID on entry\n"));
			TALLOC_FREE(sidstr);
			continue;
		}
		TALLOC_FREE(sidstr);

		/* mapped */
		map->status = ID_MAPPED;

		DEBUG(10, ("Mapped %s -> %lu (%d)\n", sid_string_static(map->sid), (unsigned long)map->xid.id, map->xid.type));
	}

	/* free the ldap results */
	if (result) {
		ldap_msgfree(result);
		result = NULL;
	}

	if (multi && ids[idx]) { /* still some values to map */
		goto again;
	}

	ret = NT_STATUS_OK;

	/* mark all unknwon/expired ones as unmapped */
	for (i = 0; ids[i]; i++) {
		if (ids[i]->status != ID_MAPPED)
			ids[i]->status = ID_UNMAPPED;
	}

done:
	talloc_free(memctx);
	return ret;
}

/**********************************
 lookup a set of sids. 
**********************************/

/* this function searches up to IDMAP_LDAP_MAX_IDS entries in maps for a match */
static struct id_map *find_map_by_sid(struct id_map **maps, DOM_SID *sid)
{
	int i;

	for (i = 0; i < IDMAP_LDAP_MAX_IDS; i++) {
		if (maps[i] == NULL) { /* end of the run */
			return NULL;
		}
		if (sid_equal(maps[i]->sid, sid)) {
			return maps[i];
		}
	}

	return NULL;	
}

static NTSTATUS idmap_ldap_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
       	LDAPMessage *entry = NULL;
	NTSTATUS ret;
	TALLOC_CTX *memctx;
	struct idmap_ldap_context *ctx;
	LDAPMessage *result = NULL;
	const char *uidNumber;
	const char *gidNumber;
	const char **attr_list;
	char *filter = NULL;
	BOOL multi = False;
	int idx = 0;
	int bidx = 0;
	int count;
	int rc;
	int i;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* Initilization my have been deferred because we were offline */
	if ( ! dom->initialized) {
		ret = idmap_ldap_db_init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			return ret;
		}
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ldap_context);	

	memctx = talloc_new(ctx);
	if ( ! memctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	uidNumber = get_attr_key2string(idpool_attr_list, LDAP_ATTR_UIDNUMBER);
	gidNumber = get_attr_key2string(idpool_attr_list, LDAP_ATTR_GIDNUMBER);

	attr_list = get_attr_list(ctx, sidmap_attr_list);

	if ( ! ids[1]) {
		/* if we are requested just one mapping use the simple filter */

		filter = talloc_asprintf(memctx, "(&(objectClass=%s)(%s=%s))",
				LDAP_OBJ_IDMAP_ENTRY,
				LDAP_ATTRIBUTE_SID,
				sid_string_static(ids[0]->sid));
		CHECK_ALLOC_DONE(filter);
		DEBUG(10, ("Filter: [%s]\n", filter));
	} else {
		/* multiple mappings */
		multi = True;
	}

again:
	if (multi) {

		TALLOC_FREE(filter);
		filter = talloc_asprintf(memctx, "(&(objectClass=%s)(|", LDAP_OBJ_IDMAP_ENTRY);
		CHECK_ALLOC_DONE(filter);

		bidx = idx;
		for (i = 0; (i < IDMAP_LDAP_MAX_IDS) && ids[idx]; i++, idx++) {
			filter = talloc_asprintf_append(filter, "(%s=%s)",
					LDAP_ATTRIBUTE_SID,
					sid_string_static(ids[idx]->sid));
			CHECK_ALLOC_DONE(filter);
		}
		filter = talloc_asprintf_append(filter, "))");
		CHECK_ALLOC_DONE(filter);
		DEBUG(10, ("Filter: [%s]", filter));
	} else {
		bidx = 0;
		idx = 1;
	}

	rc = smbldap_search(ctx->smbldap_state, ctx->suffix, LDAP_SCOPE_SUBTREE, 
		filter, attr_list, 0, &result);

	if (rc != LDAP_SUCCESS) {
		DEBUG(3,("Failure looking up sids (%s)\n", ldap_err2string(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	count = ldap_count_entries(ctx->smbldap_state->ldap_struct, result);

	if (count == 0) {
		DEBUG(10, ("NO SIDs found\n"));
	}

	for (i = 0; i < count; i++) {
		char *sidstr = NULL;
		char *tmp = NULL;
		enum id_type type;
		struct id_map *map;
		DOM_SID sid;
		uint32_t id;

		if (i == 0) { /* first entry */
			entry = ldap_first_entry(ctx->smbldap_state->ldap_struct, result);
		} else { /* following ones */
			entry = ldap_next_entry(ctx->smbldap_state->ldap_struct, entry);
		}
		if ( ! entry) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		sidstr = smbldap_talloc_single_attribute(
				ctx->smbldap_state->ldap_struct,
				entry, LDAP_ATTRIBUTE_SID, memctx);
		if ( ! sidstr) { /* no sid ??, skip entry */
			DEBUG(2, ("WARNING SID not found on entry\n"));
			continue;
		}

		if ( ! string_to_sid(&sid, sidstr)) {
			DEBUG(2, ("ERROR: Invalid SID on entry\n"));
			TALLOC_FREE(sidstr);
			continue;
		}

		map = find_map_by_sid(&ids[bidx], &sid);
		if (!map) {
			DEBUG(2, ("WARNING: couldn't find entry sid (%s) in ids", sidstr));
			TALLOC_FREE(sidstr);
			continue;
		}

		TALLOC_FREE(sidstr);

		/* now try to see if it is a uid, if not try with a gid
		 * (gid is more common, but in case both uidNumber and
		 * gidNumber are returned the SID is mapped to the uid not the gid) */
		type = ID_TYPE_UID;
		tmp = smbldap_talloc_single_attribute(
				ctx->smbldap_state->ldap_struct,
				entry, uidNumber, memctx);
		if ( ! tmp) {
			type = ID_TYPE_GID;
			tmp = smbldap_talloc_single_attribute(
					ctx->smbldap_state->ldap_struct,
					entry, gidNumber, memctx);
		}
		if ( ! tmp) { /* no ids ?? */
			DEBUG(5, ("no uidNumber, nor gidNumber attributes found\n"));
			continue;
		}

		id = strtoul(tmp, NULL, 10);
		if ((id == 0) ||
		    (ctx->filter_low_id && (id < ctx->filter_low_id)) ||
		    (ctx->filter_high_id && (id > ctx->filter_high_id))) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, ctx->filter_low_id, ctx->filter_high_id));
			TALLOC_FREE(tmp);
			continue;
		}
		TALLOC_FREE(tmp);

		/* mapped */
		map->xid.type = type;
		map->xid.id = id;
		map->status = ID_MAPPED;
		
		DEBUG(10, ("Mapped %s -> %lu (%d)\n", sid_string_static(map->sid), (unsigned long)map->xid.id, map->xid.type));
	}

	/* free the ldap results */
	if (result) {
		ldap_msgfree(result);
		result = NULL;
	}

	if (multi && ids[idx]) { /* still some values to map */
		goto again;
	}

	ret = NT_STATUS_OK;

	/* mark all unknwon/expired ones as unmapped */
	for (i = 0; ids[i]; i++) {
		if (ids[i]->status != ID_MAPPED)
			ids[i]->status = ID_UNMAPPED;
	}

done:
	talloc_free(memctx);
	return ret;
}

/**********************************
 set a mapping. 
**********************************/

/* TODO: change this:  This function cannot be called to modify a mapping, only set a new one */

static NTSTATUS idmap_ldap_set_mapping(struct idmap_domain *dom, const struct id_map *map)
{
	NTSTATUS ret;
	TALLOC_CTX *memctx;
	struct idmap_ldap_context *ctx;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	const char *type;
	char *id_str;
	char *sid;
	char *dn;
	int rc = -1;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* Initilization my have been deferred because we were offline */
	if ( ! dom->initialized) {
		ret = idmap_ldap_db_init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			return ret;
		}
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ldap_context);	

	switch(map->xid.type) {
	case ID_TYPE_UID:
		type = get_attr_key2string(sidmap_attr_list, LDAP_ATTR_UIDNUMBER);
		break;

	case ID_TYPE_GID:
		type = get_attr_key2string(sidmap_attr_list, LDAP_ATTR_GIDNUMBER);
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	memctx = talloc_new(ctx);
	if ( ! memctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	id_str = talloc_asprintf(memctx, "%lu", (unsigned long)map->xid.id);
	CHECK_ALLOC_DONE(id_str);

	sid = talloc_strdup(memctx, sid_string_static(map->sid));
	CHECK_ALLOC_DONE(sid);

	dn = talloc_asprintf(memctx, "%s=%s,%s",
			get_attr_key2string(sidmap_attr_list, LDAP_ATTR_SID),
			sid,
			ctx->suffix);
	CHECK_ALLOC_DONE(dn);

	smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_IDMAP_ENTRY);

	smbldap_make_mod(ctx->smbldap_state->ldap_struct, entry, &mods, type, id_str);

	smbldap_make_mod(ctx->smbldap_state->ldap_struct, entry, &mods,  
			  get_attr_key2string(sidmap_attr_list, LDAP_ATTR_SID), sid);

	if ( ! mods) {
		DEBUG(2, ("ERROR: No mods?\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* TODO: remove conflicting mappings! */

	smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_SID_ENTRY);

	DEBUG(10, ("Set DN %s (%s -> %s)\n", dn, sid, id_str));

	rc = smbldap_add(ctx->smbldap_state, dn, mods);
	ldap_mods_free(mods, True);	

	if (rc != LDAP_SUCCESS) {
		char *ld_error = NULL;
		ldap_get_option(ctx->smbldap_state->ldap_struct, LDAP_OPT_ERROR_STRING, &ld_error);
		DEBUG(0,("ldap_set_mapping_internals: Failed to add %s to %lu mapping [%s]\n",
			 sid, (unsigned long)map->xid.id, type));
		DEBUG(0, ("ldap_set_mapping_internals: Error was: %s (%s)\n", 
			ld_error ? ld_error : "(NULL)", ldap_err2string (rc)));
		if (ld_error) {
			ldap_memfree(ld_error);
		}
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}
		
	DEBUG(10,("ldap_set_mapping: Successfully created mapping from %s to %lu [%s]\n",
		sid, (unsigned long)map->xid.id, type));

	ret = NT_STATUS_OK;

done:
	talloc_free(memctx);
	return ret;
}

/**********************************
 Close the idmap ldap instance 
**********************************/

static NTSTATUS idmap_ldap_close(struct idmap_domain *dom)
{
	struct idmap_ldap_context *ctx;

	if (dom->private_data) {
		ctx = talloc_get_type(dom->private_data, struct idmap_ldap_context);

		talloc_free(ctx);
		dom->private_data = NULL;
	}
	
	return NT_STATUS_OK;
}

static struct idmap_methods idmap_ldap_methods = {

	.init = idmap_ldap_db_init,
	.unixids_to_sids = idmap_ldap_unixids_to_sids,
	.sids_to_unixids = idmap_ldap_sids_to_unixids,
	.set_mapping = idmap_ldap_set_mapping,
	.close_fn = idmap_ldap_close
};

static struct idmap_alloc_methods idmap_ldap_alloc_methods = {

	.init = idmap_ldap_alloc_init,
	.allocate_id = idmap_ldap_allocate_id,
	.get_id_hwm = idmap_ldap_get_hwm,
	.set_id_hwm = idmap_ldap_set_hwm,
	.close_fn = idmap_ldap_alloc_close,
	/* .dump_data = TODO */
};

NTSTATUS idmap_alloc_ldap_init(void)
{
	return smb_register_idmap_alloc(SMB_IDMAP_INTERFACE_VERSION, "ldap", &idmap_ldap_alloc_methods);
}

NTSTATUS idmap_ldap_init(void)
{
	NTSTATUS ret;

	/* FIXME: bad hack to actually register also the alloc_ldap module without changining configure.in */
	ret = idmap_alloc_ldap_init();
	if (! NT_STATUS_IS_OK(ret)) {
		return ret;
	}
	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, "ldap", &idmap_ldap_methods);
}

