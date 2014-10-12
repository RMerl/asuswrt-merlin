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
 * Copyright (C) Michael Adam 2008,2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "winbindd.h"
#include "../libds/common/flags.h"
#include "ads.h"
#include "libads/ldap_schema.h"
#include "nss_info.h"
#include "secrets.h"
#include "idmap.h"
#include "../libcli/ldap/ldap_ndr.h"
#include "../libcli/security/security.h"

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
	ADS_STRUCT *ads;
	struct posix_schema *ad_schema;
	enum wb_posix_mapping ad_map_type; /* WB_POSIX_MAP_UNKNOWN */
};

NTSTATUS init_module(void);

/************************************************************************
 ***********************************************************************/

static ADS_STATUS ad_idmap_cached_connection_internal(struct idmap_domain *dom)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	bool local = False;
	fstring dc_name;
	struct sockaddr_storage dc_ip;
	struct idmap_ad_context *ctx;
	char *ldap_server = NULL;
	char *realm = NULL;
	struct winbindd_domain *wb_dom;

	DEBUG(10, ("ad_idmap_cached_connection: called for domain '%s'\n",
		   dom->name));

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	if (ctx->ads != NULL) {

		time_t expire;
		time_t now = time(NULL);

		ads = ctx->ads;

		expire = MIN(ads->auth.tgt_expire, ads->auth.tgs_expire);

		/* check for a valid structure */
		DEBUG(7, ("Current tickets expire in %d seconds (at %d, time is now %d)\n",
			  (uint32)expire-(uint32)now, (uint32) expire, (uint32) now));

		if ( ads->config.realm && (expire > time(NULL))) {
			return ADS_SUCCESS;
		} else {
			/* we own this ADS_STRUCT so make sure it goes away */
			DEBUG(7,("Deleting expired krb5 credential cache\n"));
			ads->is_mine = True;
			ads_destroy( &ads );
			ads_kdestroy(WINBIND_CCACHE_NAME);
			ctx->ads = NULL;
		}
	}

	if (!local) {
		/* we don't want this to affect the users ccache */
		setenv("KRB5CCNAME", WINBIND_CCACHE_NAME, 1);
	}

	/*
	 * At this point we only have the NetBIOS domain name.
	 * Check if we can get server nam and realm from SAF cache
	 * and the domain list.
	 */
	ldap_server = saf_fetch(dom->name);
	DEBUG(10, ("ldap_server from saf cache: '%s'\n", ldap_server?ldap_server:""));

	wb_dom = find_domain_from_name_noinit(dom->name);
	if (wb_dom == NULL) {
		DEBUG(10, ("find_domain_from_name_noinit did not find domain '%s'\n",
			   dom->name));
		realm = NULL;
	} else {
		DEBUG(10, ("find_domain_from_name_noinit found realm '%s' for "
			  " domain '%s'\n", wb_dom->alt_name, dom->name));
		realm = wb_dom->alt_name;
	}

	if ( (ads = ads_init(realm, dom->name, ldap_server)) == NULL ) {
		DEBUG(1,("ads_init failed\n"));
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	/* the machine acct password might have change - fetch it every time */
	SAFE_FREE(ads->auth.password);
	ads->auth.password = secrets_fetch_machine_password(lp_workgroup(), NULL, NULL);

	SAFE_FREE(ads->auth.realm);
	ads->auth.realm = SMB_STRDUP(lp_realm());

	/* setup server affinity */

	get_dc_name(dom->name, realm, dc_name, &dc_ip );

	status = ads_connect(ads);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1, ("ad_idmap_cached_connection_internal: failed to "
			  "connect to AD\n"));
		ads_destroy(&ads);
		return status;
	}

	ads->is_mine = False;

	ctx->ads = ads;

	return ADS_SUCCESS;
}

/************************************************************************
 ***********************************************************************/

static ADS_STATUS ad_idmap_cached_connection(struct idmap_domain *dom)
{
	ADS_STATUS status;
	struct idmap_ad_context * ctx;

	status = ad_idmap_cached_connection_internal(dom);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	/* if we have a valid ADS_STRUCT and the schema model is
	   defined, then we can return here. */

	if ( ctx->ad_schema ) {
		return ADS_SUCCESS;
	}

	/* Otherwise, set the schema model */

	if ( (ctx->ad_map_type ==  WB_POSIX_MAP_SFU) ||
	     (ctx->ad_map_type ==  WB_POSIX_MAP_SFU20) ||
	     (ctx->ad_map_type ==  WB_POSIX_MAP_RFC2307) )
	{
		status = ads_check_posix_schema_mapping(
			ctx, ctx->ads, ctx->ad_map_type, &ctx->ad_schema);
		if ( !ADS_ERR_OK(status) ) {
			DEBUG(2,("ad_idmap_cached_connection: Failed to obtain schema details!\n"));
		}
	}

	return status;
}

static int idmap_ad_context_destructor(struct idmap_ad_context *ctx)
{
	if (ctx->ads != NULL) {
		/* we own this ADS_STRUCT so make sure it goes away */
		ctx->ads->is_mine = True;
		ads_destroy( &ctx->ads );
		ctx->ads = NULL;
	}
	return 0;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS idmap_ad_initialize(struct idmap_domain *dom)
{
	struct idmap_ad_context *ctx;
	char *config_option;
	const char *schema_mode = NULL;	

	ctx = TALLOC_ZERO_P(dom, struct idmap_ad_context);
	if (ctx == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	talloc_set_destructor(ctx, idmap_ad_context_destructor);

	config_option = talloc_asprintf(ctx, "idmap config %s", dom->name);
	if (config_option == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		talloc_free(ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* default map type */
	ctx->ad_map_type = WB_POSIX_MAP_RFC2307;

	/* schema mode */
	schema_mode = lp_parm_const_string(-1, config_option, "schema_mode", NULL);
	if ( schema_mode && schema_mode[0] ) {
		if ( strequal(schema_mode, "sfu") )
			ctx->ad_map_type = WB_POSIX_MAP_SFU;
		else if ( strequal(schema_mode, "sfu20" ) )
			ctx->ad_map_type = WB_POSIX_MAP_SFU20;
		else if ( strequal(schema_mode, "rfc2307" ) )
			ctx->ad_map_type = WB_POSIX_MAP_RFC2307;
		else
			DEBUG(0,("idmap_ad_initialize: Unknown schema_mode (%s)\n",
				 schema_mode));
	}

	dom->private_data = ctx;

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

static struct id_map *find_map_by_sid(struct id_map **maps, struct dom_sid *sid)
{
	int i;

	for (i = 0; maps[i] && i<IDMAP_AD_MAX_IDS; i++) {
		if (dom_sid_equal(maps[i]->sid, sid)) {
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

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	if ( (memctx = talloc_new(ctx)) == NULL ) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	rc = ad_idmap_cached_connection(dom);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ADS uninitialized: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		/* ret = ads_ntstatus(rc); */
		goto done;
	}

	attrs[2] = ctx->ad_schema->posix_uidnumber_attr;
	attrs[3] = ctx->ad_schema->posix_gidnumber_attr;

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
			u_filter = talloc_asprintf_append_buffer(u_filter, "(%s=%lu)",
							  ctx->ad_schema->posix_uidnumber_attr,
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
			g_filter = talloc_asprintf_append_buffer(g_filter, "(%s=%lu)",
							  ctx->ad_schema->posix_gidnumber_attr,
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
		filter = talloc_asprintf_append_buffer(filter, "%s))", u_filter);
		CHECK_ALLOC_DONE(filter);
			TALLOC_FREE(u_filter);
	}
	if ( g_filter) {
		filter = talloc_asprintf_append_buffer(filter, "%s))", g_filter);
		CHECK_ALLOC_DONE(filter);
		TALLOC_FREE(g_filter);
	}
	filter = talloc_asprintf_append_buffer(filter, ")");
	CHECK_ALLOC_DONE(filter);

	rc = ads_search_retry(ctx->ads, &res, filter, attrs);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ERROR: ads search returned: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if ( (count = ads_count_replies(ctx->ads, res)) == 0 ) {
		DEBUG(10, ("No IDs found\n"));
	}

	entry = res;
	for (i = 0; (i < count) && entry; i++) {
		struct dom_sid sid;
		enum id_type type;
		struct id_map *map;
		uint32_t id;
		uint32_t atype;

		if (i == 0) { /* first entry */
			entry = ads_first_entry(ctx->ads, entry);
		} else { /* following ones */
			entry = ads_next_entry(ctx->ads, entry);
		}

		if ( !entry ) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		if (!ads_pull_sid(ctx->ads, entry, "objectSid", &sid)) {
			DEBUG(2, ("Could not retrieve SID from entry\n"));
			continue;
		}

		/* get type */
		if (!ads_pull_uint32(ctx->ads, entry, "sAMAccountType", &atype)) {
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

		if (!ads_pull_uint32(ctx->ads, entry, (type==ID_TYPE_UID) ?
				                 ctx->ad_schema->posix_uidnumber_attr :
				                 ctx->ad_schema->posix_gidnumber_attr,
				     &id)) 
		{
			DEBUG(1, ("Could not get SID for unix ID %u\n", (unsigned) id));
			continue;
		}

		if (!idmap_unix_id_is_in_range(id, dom)) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, dom->low_id, dom->high_id));
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

		DEBUG(10, ("Mapped %s -> %lu (%d)\n", sid_string_dbg(map->sid),
			   (unsigned long)map->xid.id,
			   map->xid.type));
	}

	if (res) {
		ads_msgfree(ctx->ads, res);
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

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);	

	if ( (memctx = talloc_new(ctx)) == NULL ) {		
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	rc = ad_idmap_cached_connection(dom);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ADS uninitialized: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		/* ret = ads_ntstatus(rc); */
		goto done;
	}

	if (ctx->ad_schema == NULL) {
		DEBUG(0, ("haven't got ctx->ad_schema ! \n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	attrs[2] = ctx->ad_schema->posix_uidnumber_attr;
	attrs[3] = ctx->ad_schema->posix_gidnumber_attr;

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

		ids[idx]->status = ID_UNKNOWN;

		sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), ids[idx]->sid);
		filter = talloc_asprintf_append_buffer(filter, "(objectSid=%s)", sidstr);

		TALLOC_FREE(sidstr);
		CHECK_ALLOC_DONE(filter);
	}
	filter = talloc_asprintf_append_buffer(filter, "))");
	CHECK_ALLOC_DONE(filter);
	DEBUG(10, ("Filter: [%s]\n", filter));

	rc = ads_search_retry(ctx->ads, &res, filter, attrs);
	if (!ADS_ERR_OK(rc)) {
		DEBUG(1, ("ERROR: ads search returned: %s\n", ads_errstr(rc)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if ( (count = ads_count_replies(ctx->ads, res)) == 0 ) {
		DEBUG(10, ("No IDs found\n"));
	}

	entry = res;	
	for (i = 0; (i < count) && entry; i++) {
		struct dom_sid sid;
		enum id_type type;
		struct id_map *map;
		uint32_t id;
		uint32_t atype;

		if (i == 0) { /* first entry */
			entry = ads_first_entry(ctx->ads, entry);
		} else { /* following ones */
			entry = ads_next_entry(ctx->ads, entry);
		}

		if ( !entry ) {
			DEBUG(2, ("ERROR: Unable to fetch ldap entries from results\n"));
			break;
		}

		/* first check if the SID is present */
		if (!ads_pull_sid(ctx->ads, entry, "objectSid", &sid)) {
			DEBUG(2, ("Could not retrieve SID from entry\n"));
			continue;
		}

		map = find_map_by_sid(&ids[bidx], &sid);
		if (!map) {
			DEBUG(2, ("WARNING: couldn't match result with requested SID\n"));
			continue;
		}

		/* get type */
		if (!ads_pull_uint32(ctx->ads, entry, "sAMAccountType", &atype)) {
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

		if (!ads_pull_uint32(ctx->ads, entry, (type==ID_TYPE_UID) ?
				                 ctx->ad_schema->posix_uidnumber_attr :
				                 ctx->ad_schema->posix_gidnumber_attr,
				     &id)) 
		{
			DEBUG(1, ("Could not get unix ID for SID %s\n",
				sid_string_dbg(map->sid)));
			continue;
		}
		if (!idmap_unix_id_is_in_range(id, dom)) {
			DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				id, dom->low_id, dom->high_id));
			continue;
		}

		/* mapped */
		map->xid.type = type;
		map->xid.id = id;
		map->status = ID_MAPPED;

		DEBUG(10, ("Mapped %s -> %lu (%d)\n", sid_string_dbg(map->sid),
			   (unsigned long)map->xid.id,
			   map->xid.type));
	}

	if (res) {
		ads_msgfree(ctx->ads, res);
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

/*
 * nss_info_{sfu,sfu20,rfc2307}
 */

/************************************************************************
 Initialize the {sfu,sfu20,rfc2307} state
 ***********************************************************************/

static const char *wb_posix_map_unknown_string = "WB_POSIX_MAP_UNKNOWN";
static const char *wb_posix_map_template_string = "WB_POSIX_MAP_TEMPLATE";
static const char *wb_posix_map_sfu_string = "WB_POSIX_MAP_SFU";
static const char *wb_posix_map_sfu20_string = "WB_POSIX_MAP_SFU20";
static const char *wb_posix_map_rfc2307_string = "WB_POSIX_MAP_RFC2307";
static const char *wb_posix_map_unixinfo_string = "WB_POSIX_MAP_UNIXINFO";

static const char *ad_map_type_string(enum wb_posix_mapping map_type)
{
	switch (map_type) {
		case WB_POSIX_MAP_TEMPLATE:
			return wb_posix_map_template_string;
		case WB_POSIX_MAP_SFU:
			return wb_posix_map_sfu_string;
		case WB_POSIX_MAP_SFU20:
			return wb_posix_map_sfu20_string;
		case WB_POSIX_MAP_RFC2307:
			return wb_posix_map_rfc2307_string;
		case WB_POSIX_MAP_UNIXINFO:
			return wb_posix_map_unixinfo_string;
		default:
			return wb_posix_map_unknown_string;
	}
}

static NTSTATUS nss_ad_generic_init(struct nss_domain_entry *e,
				    enum wb_posix_mapping new_ad_map_type)
{
	struct idmap_domain *dom;
	struct idmap_ad_context *ctx;

	if (e->state != NULL) {
		dom = talloc_get_type(e->state, struct idmap_domain);
	} else {
		dom = TALLOC_ZERO_P(e, struct idmap_domain);
		if (dom == NULL) {
			DEBUG(0, ("Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}
		e->state = dom;
	}

	if (e->domain != NULL) {
		dom->name = talloc_strdup(dom, e->domain);
		if (dom->name == NULL) {
			DEBUG(0, ("Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (dom->private_data != NULL) {
		ctx = talloc_get_type(dom->private_data,
				      struct idmap_ad_context);
	} else {
		ctx = TALLOC_ZERO_P(dom, struct idmap_ad_context);
		if (ctx == NULL) {
			DEBUG(0, ("Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}
		ctx->ad_map_type = WB_POSIX_MAP_RFC2307;
		dom->private_data = ctx;
	}

	if ((ctx->ad_map_type != WB_POSIX_MAP_UNKNOWN) &&
	    (ctx->ad_map_type != new_ad_map_type))
	{
		DEBUG(2, ("nss_ad_generic_init: "
			  "Warning: overriding previously set posix map type "
			  "%s for domain %s with map type %s.\n",
			  ad_map_type_string(ctx->ad_map_type),
			  dom->name,
			  ad_map_type_string(new_ad_map_type)));
	}

	ctx->ad_map_type = new_ad_map_type;

	return NT_STATUS_OK;
}

static NTSTATUS nss_sfu_init( struct nss_domain_entry *e )
{
	return nss_ad_generic_init(e, WB_POSIX_MAP_SFU);
}

static NTSTATUS nss_sfu20_init( struct nss_domain_entry *e )
{
	return nss_ad_generic_init(e, WB_POSIX_MAP_SFU20);
}

static NTSTATUS nss_rfc2307_init( struct nss_domain_entry *e )
{
	return nss_ad_generic_init(e, WB_POSIX_MAP_RFC2307);
}


/************************************************************************
 ***********************************************************************/

static NTSTATUS nss_ad_get_info( struct nss_domain_entry *e, 
				  const struct dom_sid *sid,
				  TALLOC_CTX *mem_ctx,
				  const char **homedir,
				  const char **shell,
				  const char **gecos,
				  uint32 *gid )
{
	const char *attrs[] = {NULL, /* attr_homedir */
			       NULL, /* attr_shell */
			       NULL, /* attr_gecos */
			       NULL, /* attr_gidnumber */
			       NULL };
	char *filter = NULL;
	LDAPMessage *msg_internal = NULL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *sidstr = NULL;
	struct idmap_domain *dom;
	struct idmap_ad_context *ctx;

	DEBUG(10, ("nss_ad_get_info called for sid [%s] in domain '%s'\n",
		   sid_string_dbg(sid), e->domain?e->domain:"NULL"));

	/* Only do query if we are online */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	dom = talloc_get_type(e->state, struct idmap_domain);
	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	ads_status = ad_idmap_cached_connection(dom);
	if (!ADS_ERR_OK(ads_status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!ctx->ad_schema) {
		DEBUG(10, ("nss_ad_get_info: no ad_schema configured!\n"));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!sid || !homedir || !shell || !gecos) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Have to do our own query */

	DEBUG(10, ("nss_ad_get_info: no ads connection given, doing our "
		   "own query\n"));

	attrs[0] = ctx->ad_schema->posix_homedir_attr;
	attrs[1] = ctx->ad_schema->posix_shell_attr;
	attrs[2] = ctx->ad_schema->posix_gecos_attr;
	attrs[3] = ctx->ad_schema->posix_gidnumber_attr;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, sid);
	filter = talloc_asprintf(mem_ctx, "(objectSid=%s)", sidstr);
	TALLOC_FREE(sidstr);

	if (!filter) {
		nt_status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	ads_status = ads_search_retry(ctx->ads, &msg_internal, filter, attrs);
	if (!ADS_ERR_OK(ads_status)) {
		nt_status = ads_ntstatus(ads_status);
		goto done;
	}

	*homedir = ads_pull_string(ctx->ads, mem_ctx, msg_internal, ctx->ad_schema->posix_homedir_attr);
	*shell   = ads_pull_string(ctx->ads, mem_ctx, msg_internal, ctx->ad_schema->posix_shell_attr);
	*gecos   = ads_pull_string(ctx->ads, mem_ctx, msg_internal, ctx->ad_schema->posix_gecos_attr);

	if (gid) {
		if (!ads_pull_uint32(ctx->ads, msg_internal, ctx->ad_schema->posix_gidnumber_attr, gid))
			*gid = (uint32)-1;
	}

	nt_status = NT_STATUS_OK;

done:
	if (msg_internal) {
		ads_msgfree(ctx->ads, msg_internal);
	}

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS nss_ad_map_to_alias(TALLOC_CTX *mem_ctx,
				    struct nss_domain_entry *e,
				    const char *name,
				    char **alias)
{
	const char *attrs[] = {NULL, /* attr_uid */
			       NULL };
	char *filter = NULL;
	LDAPMessage *msg = NULL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct idmap_domain *dom;
	struct idmap_ad_context *ctx = NULL;

	/* Check incoming parameters */

	if ( !e || !e->domain || !name || !*alias) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	/* Only do query if we are online */

	if (idmap_is_offline())	{
		nt_status = NT_STATUS_FILE_IS_OFFLINE;
		goto done;
	}

	dom = talloc_get_type(e->state, struct idmap_domain);
	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	ads_status = ad_idmap_cached_connection(dom);
	if (!ADS_ERR_OK(ads_status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!ctx->ad_schema) {
		nt_status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
		goto done;
	}

	attrs[0] = ctx->ad_schema->posix_uid_attr;

	filter = talloc_asprintf(mem_ctx,
				 "(sAMAccountName=%s)",
				 name);
	if (!filter) {
		nt_status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	ads_status = ads_search_retry(ctx->ads, &msg, filter, attrs);
	if (!ADS_ERR_OK(ads_status)) {
		nt_status = ads_ntstatus(ads_status);
		goto done;
	}

	*alias = ads_pull_string(ctx->ads, mem_ctx, msg, ctx->ad_schema->posix_uid_attr);

	if (!*alias) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	nt_status = NT_STATUS_OK;

done:
	if (filter) {
		talloc_destroy(filter);
	}
	if (msg) {
		ads_msgfree(ctx->ads, msg);
	}

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS nss_ad_map_from_alias( TALLOC_CTX *mem_ctx,
					     struct nss_domain_entry *e,
					     const char *alias,
					     char **name )
{
	const char *attrs[] = {"sAMAccountName",
			       NULL };
	char *filter = NULL;
	LDAPMessage *msg = NULL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *username;
	struct idmap_domain *dom;
	struct idmap_ad_context *ctx = NULL;

	/* Check incoming parameters */

	if ( !alias || !name) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	/* Only do query if we are online */

	if (idmap_is_offline())	{
		nt_status = NT_STATUS_FILE_IS_OFFLINE;
		goto done;
	}

	dom = talloc_get_type(e->state, struct idmap_domain);
	ctx = talloc_get_type(dom->private_data, struct idmap_ad_context);

	ads_status = ad_idmap_cached_connection(dom);
	if (!ADS_ERR_OK(ads_status)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!ctx->ad_schema) {
		nt_status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
		goto done;
	}

	filter = talloc_asprintf(mem_ctx,
				 "(%s=%s)",
				 ctx->ad_schema->posix_uid_attr,
				 alias);
	if (!filter) {
		nt_status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	ads_status = ads_search_retry(ctx->ads, &msg, filter, attrs);
	if (!ADS_ERR_OK(ads_status)) {
		nt_status = ads_ntstatus(ads_status);
		goto done;
	}

	username = ads_pull_string(ctx->ads, mem_ctx, msg,
				   "sAMAccountName");
	if (!username) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	*name = talloc_asprintf(mem_ctx, "%s\\%s",
				lp_workgroup(),
				username);
	if (!*name) {
		nt_status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	nt_status = NT_STATUS_OK;

done:
	if (filter) {
		talloc_destroy(filter);
	}
	if (msg) {
		ads_msgfree(ctx->ads, msg);
	}

	return nt_status;
}

/************************************************************************
 Function dispatch tables for the idmap and nss plugins
 ***********************************************************************/

static struct idmap_methods ad_methods = {
	.init            = idmap_ad_initialize,
	.unixids_to_sids = idmap_ad_unixids_to_sids,
	.sids_to_unixids = idmap_ad_sids_to_unixids,
};

/* The SFU and RFC2307 NSS plugins share everything but the init
   function which sets the intended schema model to use */

static struct nss_info_methods nss_rfc2307_methods = {
	.init           = nss_rfc2307_init,
	.get_nss_info   = nss_ad_get_info,
	.map_to_alias   = nss_ad_map_to_alias,
	.map_from_alias = nss_ad_map_from_alias,
};

static struct nss_info_methods nss_sfu_methods = {
	.init           = nss_sfu_init,
	.get_nss_info   = nss_ad_get_info,
	.map_to_alias   = nss_ad_map_to_alias,
	.map_from_alias = nss_ad_map_from_alias,
};

static struct nss_info_methods nss_sfu20_methods = {
	.init           = nss_sfu20_init,
	.get_nss_info   = nss_ad_get_info,
	.map_to_alias   = nss_ad_map_to_alias,
	.map_from_alias = nss_ad_map_from_alias,
};



/************************************************************************
 Initialize the plugins
 ***********************************************************************/

NTSTATUS idmap_ad_init(void)
{
	static NTSTATUS status_idmap_ad = NT_STATUS_UNSUCCESSFUL;
	static NTSTATUS status_nss_rfc2307 = NT_STATUS_UNSUCCESSFUL;
	static NTSTATUS status_nss_sfu = NT_STATUS_UNSUCCESSFUL;
	static NTSTATUS status_nss_sfu20 = NT_STATUS_UNSUCCESSFUL;

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

	if ( !NT_STATUS_IS_OK( status_nss_sfu20 ) ) {
		status_nss_sfu20 = smb_register_idmap_nss(SMB_NSS_INFO_INTERFACE_VERSION,
							"sfu20",  &nss_sfu20_methods );		
		if ( !NT_STATUS_IS_OK(status_nss_sfu20) )
			return status_nss_sfu20;		
	}

	return NT_STATUS_OK;	
}

