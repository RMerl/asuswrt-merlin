/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Guenther Deschner 2005-2007
   Copyright (C) Gerald (Jerry) Carter 2006
   
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
#include "ads.h"
#include "libads/ldap_schema.h"
#include "../libcli/ldap/ldap_ndr.h"

#ifdef HAVE_LDAP

static ADS_STATUS ads_get_attrnames_by_oids(ADS_STRUCT *ads,
					    TALLOC_CTX *mem_ctx,
					    const char *schema_path,
					    const char **OIDs,
					    size_t num_OIDs,
					    char ***OIDs_out, char ***names,
					    size_t *count)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	LDAPMessage *msg;
	char *expr = NULL;
	const char *attrs[] = { "lDAPDisplayName", "attributeId", NULL };
	int i = 0, p = 0;
	
	if (!ads || !mem_ctx || !names || !count || !OIDs || !OIDs_out) {
		return ADS_ERROR(LDAP_PARAM_ERROR);
	}

	if (num_OIDs == 0 || OIDs[0] == NULL) {
		return ADS_ERROR_NT(NT_STATUS_NONE_MAPPED);
	}

	if ((expr = talloc_asprintf(mem_ctx, "(|")) == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	for (i=0; i<num_OIDs; i++) {

		if ((expr = talloc_asprintf_append_buffer(expr, "(attributeId=%s)", 
						   OIDs[i])) == NULL) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	if ((expr = talloc_asprintf_append_buffer(expr, ")")) == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	status = ads_do_search_retry(ads, schema_path, 
				     LDAP_SCOPE_SUBTREE, expr, attrs, &res);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	*count = ads_count_replies(ads, res);
	if (*count == 0 || !res) {
		status = ADS_ERROR_NT(NT_STATUS_NONE_MAPPED);
		goto out;
	}

	if (((*names) = TALLOC_ARRAY(mem_ctx, char *, *count)) == NULL) {
		status = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}
	if (((*OIDs_out) = TALLOC_ARRAY(mem_ctx, char *, *count)) == NULL) {
		status = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}

	for (msg = ads_first_entry(ads, res); msg != NULL; 
	     msg = ads_next_entry(ads, msg)) {

		(*names)[p] 	= ads_pull_string(ads, mem_ctx, msg, 
						  "lDAPDisplayName");
		(*OIDs_out)[p] 	= ads_pull_string(ads, mem_ctx, msg, 
						  "attributeId");
		if (((*names)[p] == NULL) || ((*OIDs_out)[p] == NULL)) {
			status = ADS_ERROR(LDAP_NO_MEMORY);
			goto out;
		}

		p++;
	}

	if (*count < num_OIDs) {
		status = ADS_ERROR_NT(STATUS_SOME_UNMAPPED);
		goto out;
	}

	status = ADS_ERROR(LDAP_SUCCESS);
out:
	ads_msgfree(ads, res);

	return status;
}

const char *ads_get_attrname_by_guid(ADS_STRUCT *ads, 
				     const char *schema_path, 
				     TALLOC_CTX *mem_ctx, 
				     const struct GUID *schema_guid)
{
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *expr = NULL;
	const char *attrs[] = { "lDAPDisplayName", NULL };
	const char *result = NULL;
	char *guid_bin = NULL;

	if (!ads || !mem_ctx || !schema_guid) {
		goto done;
	}

	guid_bin = ldap_encode_ndr_GUID(mem_ctx, schema_guid);
	if (!guid_bin) {
		goto done;
	}

	expr = talloc_asprintf(mem_ctx, "(schemaIDGUID=%s)", guid_bin);
	if (!expr) {
		goto done;
	}

	rc = ads_do_search_retry(ads, schema_path, LDAP_SCOPE_SUBTREE, 
				 expr, attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		goto done;
	}

	if (ads_count_replies(ads, res) != 1) {
		goto done;
	}

	result = ads_pull_string(ads, mem_ctx, res, "lDAPDisplayName");

 done:
	TALLOC_FREE(guid_bin);
	ads_msgfree(ads, res);
	return result;
	
}

/*********************************************************************
*********************************************************************/

ADS_STATUS ads_schema_path(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, char **schema_path)
{
	ADS_STATUS status;
	LDAPMessage *res;
	const char *schema;
	const char *attrs[] = { "schemaNamingContext", NULL };

	status = ads_do_search(ads, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if ( (schema = ads_pull_string(ads, mem_ctx, res, "schemaNamingContext")) == NULL ) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
	}

	if ( (*schema_path = talloc_strdup(mem_ctx, schema)) == NULL ) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	ads_msgfree(ads, res);

	return status;
}

/**
 * Check for "Services for Unix" or rfc2307 Schema and load some attributes into the ADS_STRUCT
 * @param ads connection to ads server
 * @param enum mapping type
 * @return ADS_STATUS status of search (False if one or more attributes couldn't be
 * found in Active Directory)
 **/ 
ADS_STATUS ads_check_posix_schema_mapping(TALLOC_CTX *mem_ctx,
					  ADS_STRUCT *ads,
					  enum wb_posix_mapping map_type,
					  struct posix_schema **s ) 
{
	TALLOC_CTX *ctx = NULL; 
	ADS_STATUS status;
	char **oids_out, **names_out;
	size_t num_names;
	char *schema_path = NULL;
	int i;
	struct posix_schema *schema = NULL;

	const char *oids_sfu[] = { 	ADS_ATTR_SFU_UIDNUMBER_OID,
					ADS_ATTR_SFU_GIDNUMBER_OID,
					ADS_ATTR_SFU_HOMEDIR_OID,
					ADS_ATTR_SFU_SHELL_OID,
					ADS_ATTR_SFU_GECOS_OID,
					ADS_ATTR_SFU_UID_OID };

	const char *oids_sfu20[] = { 	ADS_ATTR_SFU20_UIDNUMBER_OID,
					ADS_ATTR_SFU20_GIDNUMBER_OID,
					ADS_ATTR_SFU20_HOMEDIR_OID,
					ADS_ATTR_SFU20_SHELL_OID,
					ADS_ATTR_SFU20_GECOS_OID,
					ADS_ATTR_SFU20_UID_OID };

	const char *oids_rfc2307[] = {	ADS_ATTR_RFC2307_UIDNUMBER_OID,
					ADS_ATTR_RFC2307_GIDNUMBER_OID,
					ADS_ATTR_RFC2307_HOMEDIR_OID,
					ADS_ATTR_RFC2307_SHELL_OID,
					ADS_ATTR_RFC2307_GECOS_OID,
					ADS_ATTR_RFC2307_UID_OID };

	DEBUG(10,("ads_check_posix_schema_mapping for schema mode: %d\n", map_type));

	switch (map_type) {
	
		case WB_POSIX_MAP_TEMPLATE:
		case WB_POSIX_MAP_UNIXINFO:
			DEBUG(10,("ads_check_posix_schema_mapping: nothing to do\n"));
			return ADS_ERROR(LDAP_SUCCESS);

		case WB_POSIX_MAP_SFU:
		case WB_POSIX_MAP_SFU20:
		case WB_POSIX_MAP_RFC2307:
			break;

		default:
			DEBUG(0,("ads_check_posix_schema_mapping: "
				 "unknown enum %d\n", map_type));
			return ADS_ERROR(LDAP_PARAM_ERROR);
	}

	if ( (ctx = talloc_init("ads_check_posix_schema_mapping")) == NULL ) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	if ( (schema = TALLOC_P(mem_ctx, struct posix_schema)) == NULL ) {
		TALLOC_FREE( ctx );
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	status = ads_schema_path(ads, ctx, &schema_path);
	if (!ADS_ERR_OK(status)) {
		DEBUG(3,("ads_check_posix_mapping: Unable to retrieve schema DN!\n"));
		goto done;
	}

	switch (map_type) {
		case WB_POSIX_MAP_SFU:
			status = ads_get_attrnames_by_oids(ads, ctx, schema_path, oids_sfu, 
							   ARRAY_SIZE(oids_sfu), 
							   &oids_out, &names_out, &num_names);
			break;
		case WB_POSIX_MAP_SFU20:
			status = ads_get_attrnames_by_oids(ads, ctx, schema_path, oids_sfu20, 
							   ARRAY_SIZE(oids_sfu20), 
							   &oids_out, &names_out, &num_names);
			break;
		case WB_POSIX_MAP_RFC2307:
			status = ads_get_attrnames_by_oids(ads, ctx, schema_path, oids_rfc2307, 
							   ARRAY_SIZE(oids_rfc2307), 
							   &oids_out, &names_out, &num_names);
			break;
		default:
			status = ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
			break;
	}

	if (!ADS_ERR_OK(status)) {
		DEBUG(3,("ads_check_posix_schema_mapping: failed %s\n", 
			ads_errstr(status)));
		goto done;
	}

	for (i=0; i<num_names; i++) {

		DEBUGADD(10,("\tOID %s has name: %s\n", oids_out[i], names_out[i]));

		if (strequal(ADS_ATTR_RFC2307_UIDNUMBER_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_UIDNUMBER_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_UIDNUMBER_OID, oids_out[i])) {
			schema->posix_uidnumber_attr = talloc_strdup(schema, names_out[i]);
			continue;		       
		}

		if (strequal(ADS_ATTR_RFC2307_GIDNUMBER_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_GIDNUMBER_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_GIDNUMBER_OID, oids_out[i])) {
			schema->posix_gidnumber_attr = talloc_strdup(schema, names_out[i]);
			continue;		
		}

		if (strequal(ADS_ATTR_RFC2307_HOMEDIR_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_HOMEDIR_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_HOMEDIR_OID, oids_out[i])) {
			schema->posix_homedir_attr = talloc_strdup(schema, names_out[i]);
			continue;			
		}

		if (strequal(ADS_ATTR_RFC2307_SHELL_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_SHELL_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_SHELL_OID, oids_out[i])) {
			schema->posix_shell_attr = talloc_strdup(schema, names_out[i]);
			continue;			
		}

		if (strequal(ADS_ATTR_RFC2307_GECOS_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_GECOS_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_GECOS_OID, oids_out[i])) {
			schema->posix_gecos_attr = talloc_strdup(schema, names_out[i]);
		}

		if (strequal(ADS_ATTR_RFC2307_UID_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU_UID_OID, oids_out[i]) ||
		    strequal(ADS_ATTR_SFU20_UID_OID, oids_out[i])) {
			schema->posix_uid_attr = talloc_strdup(schema, names_out[i]);
		}
	}

	if (!schema->posix_uidnumber_attr ||
	    !schema->posix_gidnumber_attr ||
	    !schema->posix_homedir_attr ||
	    !schema->posix_shell_attr ||
	    !schema->posix_gecos_attr) {
	    	status = ADS_ERROR(LDAP_NO_MEMORY);
	        TALLOC_FREE( schema );		
	    	goto done;
	}

	*s = schema;
	
	status = ADS_ERROR(LDAP_SUCCESS);
	
done:
	TALLOC_FREE(ctx);

	return status;
}

#endif
