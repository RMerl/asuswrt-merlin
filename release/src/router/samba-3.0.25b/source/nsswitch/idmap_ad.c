/*
 *  idmap_ad: map between Active Directory and RFC 2307 or "Services for Unix" (SFU) Accounts
 *
 * Unix SMB/CIFS implementation.
 *
 * Winbind ADS backend functions
 *
 * Copyright (C) Andrew Tridgell 2001
 * Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003
 * Copyright (C) Gerald (Jerry) Carter 2004-2007
 * Copyright (C) Luke Howard 2001-2004
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

#define WINBIND_CCACHE_NAME "MEMORY:winbind_ccache"

#define IDMAP_AD_MAX_IDS 30
#define CHECK_ALLOC_DONE(mem) do { \
     if (!mem) { \
           DEBUG(0, ("Out of memory!\n")); \
           ret = NT_STATUS_NO_MEMORY; \
           goto done; \
      } \
} while (0)

struct idmap_ad_context {
	uint32_t filter_low_id;
	uint32_t filter_high_id;
};

NTSTATUS init_module(void);

static ADS_STRUCT *ad_idmap_ads = NULL;
static struct posix_schema *ad_schema = NULL;
static enum wb_posix_mapping ad_map_type = WB_POSIX_MAP_UNKNOWN;

/************************************************************************
 ***********************************************************************/

static ADS_STRUCT *ad_idmap_cached_connection_internal(void)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	BOOL local = False;
	fstring dc_name;
	struct in_addr dc_ip;	

	if (ad_idmap_ads != NULL) {

		time_t expire;
		time_t now = time(NULL);

		ads = ad_idmap_ads;

		expire = MIN(ads->auth.tgt_expire, ads->auth.tgs_expire);

		/* check for a valid structure */
		DEBUG(7, ("Current tickets expire in %d seconds (at %d, time is now %d)\n",
			  (uint32)expire-(uint32)now, (uint32) expire, (uint32) now));

		if ( ads->config.realm && (expire > time(NULL))) {
			return ads;
		} else {
			/* we own this ADS_STRUCT so make sure it goes away */
			DEBUG(7,("Deleting expired krb5 credential cache\n"));
			ads->is_mine = True;
			ads_destroy( &ads );
			ads_kdestroy(WINBIND_CCACHE_NAME);
			ad_idmap_ads = NULL;
			TALLOC_FREE( ad_schema );			
		}
	}

	if (!local) {
		/* we don't want this to affect the users ccache */
		setenv("KRB5CCNAME", WINBIND_CCACHE_NAME, 1);
	}

	if ( (ads = ads_init(lp_realm(), lp_workgroup(), NULL)) == NULL ) {
		DEBUG(1,("ads_init failed\n"));
		return NULL;
	}

	/* the machine acct password might have change - fetch it every time */
	SAFE_FREE(ads->auth.password);
	ads->auth.password = secrets_fetch_machine_password(lp_workgroup(), NULL, NULL);

	SAFE_FREE(ads->auth.realm);
	ads->auth.realm = SMB_STRDUP(lp_realm());

	/* setup server affinity */

	get_dc_name( NULL, ads->auth.realm, dc_name, &dc_ip );
	
	status = ads_connect(ads);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1, ("ad_idmap_init: failed to connect to AD\n"));
		ads_destroy(&ads);
		return NULL;
	}

	ads->is_mine = False;

	ad_idmap_ads = ads;

	return ads;
}

/************************************************************************
 ***********************************************************************/

static ADS_STRUCT *ad_idmap_cached_connection(void)
{
	ADS_STRUCT *ads = ad_idmap_cached_connection_internal();
	
	if ( !ads )
		return NULL;

	/* if we have a valid ADS_STRUCT and the schema model is
	   defined, then we can return here. */

	if ( ad_schema )
		return ads;

	/* Otherwise, set the schema model */

	if ( (ad_map_type ==  WB_POSIX_MAP_SFU) ||
	     (ad_map_type ==  WB_POSIX_MAP_RFC2307) ) 
	{
		ADS_STATUS schema_status;
		
		schema_status = ads_check_posix_schema_mapping( NULL, ads, ad_map_type, &ad_schema);
		if ( !ADS_ERR_OK(schema_status) ) {
			DEBUG(2,("ad_idmap_cached_connection: Failed to obtain schema details!\n"));
			return NULL;			
		}
	}
	
	return ads;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS idmap_ad_initialize(struct idmap_domain *dom)
{
	struct idmap_ad_context *ctx;
	char *config_option;
	const char *range = NULL;
	const char *schema_mode = NULL;	

	if ( (ctx = TALLOC_ZERO_P(dom, struct idmap_ad_context)) == NULL ) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if ( (config_option = talloc_asprintf(ctx, "idmap config %s", dom->name)) == NULL ) {
		DEBUG(0, ("Out of memory!\n"));
		talloc_free(ctx);
		return NT_STATUS_NO_MEMORY;
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

	/* schema mode */
	if ( ad_map_type == WB_POSIX_MAP_UNKNOWN )
		ad_map_type = WB_POSIX_MAP_RFC2307;
	schema_mode = lp_parm_const_string(-1, config_option, "schema_mode", NULL);
	if ( schema_mode && schema_mode[0] ) {
		if ( strequal(schema_mode, "sfu") )
			ad_map_type = WB_POSIX_MAP_SFU;
		else if ( strequal(schema_mode, "rfc2307" ) )
			ad_map_type = WB_POSIX_MAP_RFC2307;
		else
			DEBUG(0,("idmap_ad_initialize: Unknown schema_mode (%s)\n",
				 schema_mode));
	}

	dom->private_data = ctx;
	dom->initialized = True;

	talloc_free(config_option);

	return NT_STATUS_OK;
}

/************************************************************************
 Search up to IDMAP_AD_MAX_IDS entries in maps for a match.
 ***********************************************************************/

static struct id_map *find_map_by_id(struct id_map **maps, enum id_type type, uint32_t id)
{
	int i;

	for (i = 0; maps[i] && i<IDMAP_AD_MAX_IDS; i++) {
		if ((maps[i]->xid.type == type) && (maps[i]->xid.id == id)) {
			return maps[i];
		}
	}

	return NULL;	
}

/************************************************************************
 Search up to IDMAP_AD_MAX_IDS entries in maps for a match
 ***********************************************************************/

static struct id_map *find_map_by_sid(struct id_map **maps, DOM_SID *sid)
{
	int i;

	for (i = 0; maps[i] && i<IDMAP_AD_MAX_IDS; i++) {
		if (sid_equal(maps[i]->sid, sid)) {
			return maps[i];
		}
	}

	return NULL;	
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS idmap_ad_unixids_to_sids(struct idmap_domain *dom, struct id_map **ids)
{
	NTSTATUS ret;
	TALLOC_CTX *memctx;
	struct idmap_ad_context *ctx;
	ADS_STATUS rc;
	ADS_STRUCT *ads;
	const char *attrs[] = { "sAMAccountType", 
				"objectSid",
				NULL, /* uidnumber */
				NULL, /* gidnumber */
				NULL };
	LDAPMessage *res = NULL;
	LDAPMessage *entry = NULL;
	char *filter = NULL;
	int idx = 0;
	int bidx = 0;
	int count;
	int i;
	char *u_filter = NULL;
	char *g_filter = NULL;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* Initilization my have been deferred because we were offline */
	if ( ! dom->initialized) {
		ret = idmap_ad_initialize(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			return ret;
		}
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	if ( (memctx = talloc_new(ctx)) == NULL ) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if ( (ads = ad_idmap_cached_connection()) == NULL ) {
		DEBUG(1, ("ADS uninitialized\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	attrs[2] = ad_schema->posix_uidnumber_attr;
	attrs[3] = ad_schema->posix_gidnumber_attr;

again:
	bidx = idx;
	for (i = 0; (i < IDMAP_AD_MAX_IDS) && ids[idx]; i++, idx++) {
		switch (ids[idx]->xid.type) {
		case ID_TYPE_UID:     
			if ( ! u_filter) {
				u_filter = talloc_asprintf(memctx, "(&(|"
							   "(sAMAccountType=%d)"
							   "(sAMAccountType=%d)"
							   "(sAMAccountType=%d))(|",
							   ATYPE_NORMAL_ACCOUNT,
							   ATYPE_WORKSTATION_TRUST,
							   ATYPE_INTERDOMAIN_TRUST);
			}
			u_filter = talloc_asprintf_append(u_filter, "(%s=%lu)",
							  ad_schema->posix_uidnumber_attr,
							  (unsigned long)ids[idx]->xid.id);
			CHECK_ALLOC_DONE(u_filter);
			break;
				
		case ID_TYPE_GID:
			if ( ! g_filter) {
				g_filter = talloc_asprintf(memctx, "(&(|"
							   "(sAMAccountType=%d)"
							   "(sAMAccountType=%d))(|",
							   ATYPE_SECURITY_GLOBAL_GROUP,
							   ATYPE_SECURITY_LOCAL_GROUP);
			}
			g_filter = talloc_asprintf_append(g_filter, "(%s=%lu)",
							  ad_schema->posix_gidnumber_attr,
							  (unsigned long)ids[idx]->xid.id);
			CHECK_ALLOC_DONE(g_filter);
			break;

		default:
			DEBUG(3, ("Error: mapping requested but Unknown ID type\n"));
			ids[idx]->status = ID_UNKNOWN;
			continue;
		}
	}
	filter = talloc_asprintf(memctx, "(|");
	CHECK_ALLOC_DONE(filter);
	if ( u_filter) {
		filter = talloc_asprintf_append(filter, "%s))", u_filter);
		CHECK_ALLOC_DONE(filter);
			TALLOC_FREE(u_filter);
	}
	if ( g_filter) {
		filter = talloc_asprintf_append(filter, "%s))", g_filter);
		CHECK_ALLOC_DONE(filter);
		TALLOC_FREE(g_filter);
	}
	filter = talloc_asprintf_append(filter, ")");
	CHECK_ALLOC_DONE(filter);

	rc = ads_search_retry(ads, &res, filter, attrs);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ERROR: ads search returned: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if ( (count = ads_count_replies(ads, res)) == 0 ) {
		DEBUG(10, ("No IDs found\n"));
	}

	entry = res;
	for (i = 0; (i < count) && entry; i++) {
		DOM_SID sid;
		enum id_type type;
		struct id_map *map;
		uint32_t id;
		uint32_t atype;

		if (i == 0) { /* first entry */
			entry = ads_first_entry(ads, entry);
		} else { /* following ones */
			entry = ads_next_entry(ads, entry);
		}

		if ( !entry ) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		if (!ads_pull_sid(ads, entry, "objectSid", &sid)) {
			DEBUG(2, ("Could not retrieve SID from entry\n"));
			continue;
		}

		/* get type */
		if (!ads_pull_uint32(ads, entry, "sAMAccountType", &atype)) {
			DEBUG(1, ("could not get SAM account type\n"));
			continue;
		}

		switch (atype & 0xF0000000) {
		case ATYPE_SECURITY_GLOBAL_GROUP:
		case ATYPE_SECURITY_LOCAL_GROUP:
			type = ID_TYPE_GID;
			break;
		case ATYPE_NORMAL_ACCOUNT:
		case ATYPE_WORKSTATION_TRUST:
		case ATYPE_INTERDOMAIN_TRUST:
			type = ID_TYPE_UID;
			break;
		default:
			DEBUG(1, ("unrecognized SAM account type %08x\n", atype));
			continue;
		}

		if (!ads_pull_uint32(ads, entry, (type==ID_TYPE_UID) ? 
				                 ad_schema->posix_uidnumber_attr : 
				                 ad_schema->posix_gidnumber_attr, 
				     &id)) 
		{
			DEBUG(1, ("Could not get unix ID\n"));
			continue;
		}

		if ((id == 0) ||
		    (ctx->filter_low_id && (id < ctx->filter_low_id)) ||
		    (ctx->filter_high_id && (id > ctx->filter_high_id))) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, ctx->filter_low_id, ctx->filter_high_id));
			continue;
		}

		map = find_map_by_id(&ids[bidx], type, id);
		if (!map) {
			DEBUG(2, ("WARNING: couldn't match result with requested ID\n"));
			continue;
		}

		sid_copy(map->sid, &sid);

		/* mapped */
		map->status = ID_MAPPED;

		DEBUG(10, ("Mapped %s -> %lu (%d)\n",
			   sid_string_static(map->sid),
			   (unsigned long)map->xid.id,
			   map->xid.type));
	}

	if (res) {
		ads_msgfree(ads, res);
	}

	if (ids[idx]) { /* still some values to map */
		goto again;
	}

	ret = NT_STATUS_OK;

	/* mark all unknown/expired ones as unmapped */
	for (i = 0; ids[i]; i++) {
		if (ids[i]->status != ID_MAPPED) 
			ids[i]->status = ID_UNMAPPED;
	}

done:
	talloc_free(memctx);
	return ret;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS idmap_ad_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
	NTSTATUS ret;
	TALLOC_CTX *memctx;
	struct idmap_ad_context *ctx;
	ADS_STATUS rc;
	ADS_STRUCT *ads;
	const char *attrs[] = { "sAMAccountType", 
				"objectSid",
				NULL, /* attr_uidnumber */
				NULL, /* attr_gidnumber */
				NULL };
	LDAPMessage *res = NULL;
	LDAPMessage *entry = NULL;
	char *filter = NULL;
	int idx = 0;
	int bidx = 0;
	int count;
	int i;
	char *sidstr;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* Initilization my have been deferred because we were offline */
	if ( ! dom->initialized) {
		ret = idmap_ad_initialize(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			return ret;
		}
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);	

	if ( (memctx = talloc_new(ctx)) == NULL ) {		
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if ( (ads = ad_idmap_cached_connection()) == NULL ) {
		DEBUG(1, ("ADS uninitialized\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	attrs[2] = ad_schema->posix_uidnumber_attr;
	attrs[3] = ad_schema->posix_gidnumber_attr;

again:
	filter = talloc_asprintf(memctx, "(&(|"
				 "(sAMAccountType=%d)(sAMAccountType=%d)(sAMAccountType=%d)" /* user account types */
				 "(sAMAccountType=%d)(sAMAccountType=%d)" /* group account types */
				 ")(|",
				 ATYPE_NORMAL_ACCOUNT, ATYPE_WORKSTATION_TRUST, ATYPE_INTERDOMAIN_TRUST,
				 ATYPE_SECURITY_GLOBAL_GROUP, ATYPE_SECURITY_LOCAL_GROUP);
		
	CHECK_ALLOC_DONE(filter);

	bidx = idx;
	for (i = 0; (i < IDMAP_AD_MAX_IDS) && ids[idx]; i++, idx++) {

		sidstr = sid_binstring(ids[idx]->sid);
		filter = talloc_asprintf_append(filter, "(objectSid=%s)", sidstr);
			
		free(sidstr);
		CHECK_ALLOC_DONE(filter);
	}
	filter = talloc_asprintf_append(filter, "))");
	CHECK_ALLOC_DONE(filter);
	DEBUG(10, ("Filter: [%s]\n", filter));

	rc = ads_search_retry(ads, &res, filter, attrs);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ERROR: ads search returned: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if ( (count = ads_count_replies(ads, res)) == 0 ) {
		DEBUG(10, ("No IDs found\n"));
	}

	entry = res;	
	for (i = 0; (i < count) && entry; i++) {
		DOM_SID sid;
		enum id_type type;
		struct id_map *map;
		uint32_t id;
		uint32_t atype;

		if (i == 0) { /* first entry */
			entry = ads_first_entry(ads, entry);
		} else { /* following ones */
			entry = ads_next_entry(ads, entry);
		}

		if ( !entry ) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		if (!ads_pull_sid(ads, entry, "objectSid", &sid)) {
			DEBUG(2, ("Could not retrieve SID from entry\n"));
			continue;
		}

		map = find_map_by_sid(&ids[bidx], &sid);
		if (!map) {
			DEBUG(2, ("WARNING: couldn't match result with requested SID\n"));
			continue;
		}

		/* get type */
		if (!ads_pull_uint32(ads, entry, "sAMAccountType", &atype)) {
			DEBUG(1, ("could not get SAM account type\n"));
			continue;
		}

		switch (atype & 0xF0000000) {
		case ATYPE_SECURITY_GLOBAL_GROUP:
		case ATYPE_SECURITY_LOCAL_GROUP:
			type = ID_TYPE_GID;
			break;
		case ATYPE_NORMAL_ACCOUNT:
		case ATYPE_WORKSTATION_TRUST:
		case ATYPE_INTERDOMAIN_TRUST:
			type = ID_TYPE_UID;
			break;
		default:
			DEBUG(1, ("unrecognized SAM account type %08x\n", atype));
			continue;
		}

		if (!ads_pull_uint32(ads, entry, (type==ID_TYPE_UID) ? 
				                 ad_schema->posix_uidnumber_attr : 
				                 ad_schema->posix_gidnumber_attr, 
				     &id)) 
		{
			DEBUG(1, ("Could not get unix ID\n"));
			continue;
		}
		if ((id == 0) ||
		    (ctx->filter_low_id && (id < ctx->filter_low_id)) ||
		    (ctx->filter_high_id && (id > ctx->filter_high_id))) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, ctx->filter_low_id, ctx->filter_high_id));
			continue;
		}

		/* mapped */
		map->xid.type = type;
		map->xid.id = id;
		map->status = ID_MAPPED;

		DEBUG(10, ("Mapped %s -> %lu (%d)\n",
			   sid_string_static(map->sid),
			   (unsigned long)map->xid.id,
			   map->xid.type));
	}

	if (res) {
		ads_msgfree(ads, res);
	}

	if (ids[idx]) { /* still some values to map */
		goto again;
	}

	ret = NT_STATUS_OK;

	/* mark all unknwoni/expired ones as unmapped */
	for (i = 0; ids[i]; i++) {
		if (ids[i]->status != ID_MAPPED) 
			ids[i]->status = ID_UNMAPPED;
	}

done:
	talloc_free(memctx);
	return ret;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS idmap_ad_close(struct idmap_domain *dom)
{
	ADS_STRUCT *ads = ad_idmap_ads;

	if (ads != NULL) {
		/* we own this ADS_STRUCT so make sure it goes away */
		ads->is_mine = True;
		ads_destroy( &ads );
		ad_idmap_ads = NULL;
	}

	TALLOC_FREE( ad_schema );
	
	return NT_STATUS_OK;
}

/*
 * nss_info_{sfu,rfc2307}
 */

/************************************************************************
 Initialize the {sfu,rfc2307} state
 ***********************************************************************/

static NTSTATUS nss_sfu_init( struct nss_domain_entry *e )
{
	/* Sanity check if we have previously been called with a
	   different schema model */

	if ( (ad_map_type != WB_POSIX_MAP_UNKNOWN) &&
	     (ad_map_type != WB_POSIX_MAP_SFU) ) 
	{
		DEBUG(0,("nss_sfu_init: Posix Map type has already been set.  "
			 "Mixed schema models not supported!\n"));
		return NT_STATUS_NOT_SUPPORTED;
	}
	
	ad_map_type =  WB_POSIX_MAP_SFU;	

	return NT_STATUS_OK;
}

static NTSTATUS nss_rfc2307_init( struct nss_domain_entry *e )
{
	/* Sanity check if we have previously been called with a
	   different schema model */
	 
	if ( (ad_map_type != WB_POSIX_MAP_UNKNOWN) &&
	     (ad_map_type != WB_POSIX_MAP_RFC2307) ) 
	{
		DEBUG(0,("nss_rfc2307_init: Posix Map type has already been set.  "
			 "Mixed schema models not supported!\n"));
		return NT_STATUS_NOT_SUPPORTED;
	}
	
	ad_map_type =  WB_POSIX_MAP_RFC2307;

	return NT_STATUS_OK;
}


/************************************************************************
 ***********************************************************************/
static NTSTATUS nss_ad_get_info( struct nss_domain_entry *e, 
				  const DOM_SID *sid, 
				  TALLOC_CTX *ctx,
				  ADS_STRUCT *ads, 
				  LDAPMessage *msg,
				  char **homedir,
				  char **shell, 
				  char **gecos,
				  uint32 *gid )
{
	ADS_STRUCT *ads_internal = NULL;

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	/* We are assuming that the internal ADS_STRUCT is for the 
	   same forest as the incoming *ads pointer */

	ads_internal = ad_idmap_cached_connection();

	if ( !ads_internal || !ad_schema )
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	
	if ( !homedir || !shell || !gecos )
		return NT_STATUS_INVALID_PARAMETER;

	*homedir = ads_pull_string( ads, ctx, msg, ad_schema->posix_homedir_attr );
	*shell   = ads_pull_string( ads, ctx, msg, ad_schema->posix_shell_attr );
	*gecos   = ads_pull_string( ads, ctx, msg, ad_schema->posix_gecos_attr );
       
	if ( gid ) {		
		if ( !ads_pull_uint32(ads, msg, ad_schema->posix_gidnumber_attr, gid ) )
			*gid = 0;		
	}
		
	return NT_STATUS_OK;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS nss_ad_close( void )
{
	/* nothing to do.  All memory is free()'d by the idmap close_fn() */

	return NT_STATUS_OK;
}

/************************************************************************
 Function dispatch tables for the idmap and nss plugins
 ***********************************************************************/

static struct idmap_methods ad_methods = {
	.init            = idmap_ad_initialize,
	.unixids_to_sids = idmap_ad_unixids_to_sids,
	.sids_to_unixids = idmap_ad_sids_to_unixids,
	.close_fn        = idmap_ad_close
};

/* The SFU and RFC2307 NSS plugins share everything but the init
   function which sets the intended schema model to use */
  
static struct nss_info_methods nss_rfc2307_methods = {
	.init         = nss_rfc2307_init,
	.get_nss_info =	nss_ad_get_info,
	.close_fn     = nss_ad_close
};

static struct nss_info_methods nss_sfu_methods = {
	.init         = nss_sfu_init,
	.get_nss_info =	nss_ad_get_info,
	.close_fn     = nss_ad_close
};


/************************************************************************
 Initialize the plugins
 ***********************************************************************/

NTSTATUS idmap_ad_init(void)
{
	static NTSTATUS status_idmap_ad = NT_STATUS_UNSUCCESSFUL;
	static NTSTATUS status_nss_rfc2307 = NT_STATUS_UNSUCCESSFUL;
	static NTSTATUS status_nss_sfu = NT_STATUS_UNSUCCESSFUL;

	/* Always register the AD method first in order to get the
	   idmap_domain interface called */

	if ( !NT_STATUS_IS_OK(status_idmap_ad) ) {
		status_idmap_ad = smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, 
						     "ad", &ad_methods);
		if ( !NT_STATUS_IS_OK(status_idmap_ad) )
			return status_idmap_ad;		
	}
	
	if ( !NT_STATUS_IS_OK( status_nss_rfc2307 ) ) {
		status_nss_rfc2307 = smb_register_idmap_nss(SMB_NSS_INFO_INTERFACE_VERSION,
							    "rfc2307",  &nss_rfc2307_methods );		
		if ( !NT_STATUS_IS_OK(status_nss_rfc2307) )
			return status_nss_rfc2307;
	}

	if ( !NT_STATUS_IS_OK( status_nss_sfu ) ) {
		status_nss_sfu = smb_register_idmap_nss(SMB_NSS_INFO_INTERFACE_VERSION,
							"sfu",  &nss_sfu_methods );		
		if ( !NT_STATUS_IS_OK(status_nss_sfu) )
			return status_nss_sfu;		
	}

	return NT_STATUS_OK;	
}

