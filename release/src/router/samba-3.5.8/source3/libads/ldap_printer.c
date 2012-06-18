/* 
   Unix SMB/CIFS implementation.
   ads (active directory) printer utility library
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   
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
#include "../librpc/gen_ndr/cli_spoolss.h"

#ifdef HAVE_ADS

/*
  find a printer given the name and the hostname
    Note that results "res" may be allocated on return so that the
    results can be used.  It should be freed using ads_msgfree.
*/
 ADS_STATUS ads_find_printer_on_server(ADS_STRUCT *ads, LDAPMessage **res,
				       const char *printer,
				       const char *servername)
{
	ADS_STATUS status;
	char *srv_dn, **srv_cn, *s = NULL;
	const char *attrs[] = {"*", "nTSecurityDescriptor", NULL};

	status = ads_find_machine_acct(ads, res, servername);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1, ("ads_find_printer_on_server: cannot find host %s in ads\n",
			  servername));
		return status;
	}
	if (ads_count_replies(ads, *res) != 1) {
		if (res) {
			ads_msgfree(ads, *res);
			*res = NULL;
		}
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}
	srv_dn = ldap_get_dn(ads->ldap.ld, *res);
	if (srv_dn == NULL) {
		if (res) {
			ads_msgfree(ads, *res);
			*res = NULL;
		}
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	srv_cn = ldap_explode_dn(srv_dn, 1);
	if (srv_cn == NULL) {
		ldap_memfree(srv_dn);
		if (res) {
			ads_msgfree(ads, *res);
			*res = NULL;
		}
		return ADS_ERROR(LDAP_INVALID_DN_SYNTAX);
	}
	if (res) {
		ads_msgfree(ads, *res);
		*res = NULL;
	}

	if (asprintf(&s, "(cn=%s-%s)", srv_cn[0], printer) == -1) {
		ldap_memfree(srv_dn);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	status = ads_search(ads, res, s, attrs);

	ldap_memfree(srv_dn);
	ldap_value_free(srv_cn);
	SAFE_FREE(s);
	return status;	
}

 ADS_STATUS ads_find_printers(ADS_STRUCT *ads, LDAPMessage **res)
{
	const char *ldap_expr;
	const char *attrs[] = { "objectClass", "printerName", "location", "driverName",
				"serverName", "description", NULL };

	/* For the moment only display all printers */

	ldap_expr = "(&(!(showInAdvancedViewOnly=TRUE))(uncName=*)"
		"(objectCategory=printQueue))";

	return ads_search(ads, res, ldap_expr, attrs);
}

/*
  modify a printer entry in the directory
*/
ADS_STATUS ads_mod_printer_entry(ADS_STRUCT *ads, char *prt_dn,
				 TALLOC_CTX *ctx, const ADS_MODLIST *mods)
{
	return ads_gen_mod(ads, prt_dn, *mods);
}

/*
  add a printer to the directory
*/
ADS_STATUS ads_add_printer_entry(ADS_STRUCT *ads, char *prt_dn,
					TALLOC_CTX *ctx, ADS_MODLIST *mods)
{
	ads_mod_str(ctx, mods, "objectClass", "printQueue");
	return ads_gen_add(ads, prt_dn, *mods);
}

/*
  map a REG_SZ to an ldap mod
*/
static bool map_sz(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
		   const struct regval_blob *value)
{
	char *str_value = NULL;
	size_t converted_size;
	ADS_STATUS status;

	if (value->type != REG_SZ)
		return false;

	if (value->size && *((smb_ucs2_t *) value->data_p)) {
		if (!pull_ucs2_talloc(ctx, &str_value,
				      (const smb_ucs2_t *) value->data_p,
				      &converted_size))
		{
			return false;
		}
		status = ads_mod_str(ctx, mods, value->valuename, str_value);
		return ADS_ERR_OK(status);
	}
	return true;
		
}

/*
  map a REG_DWORD to an ldap mod
*/
static bool map_dword(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
		      const struct regval_blob *value)
{
	char *str_value = NULL;
	ADS_STATUS status;

	if (value->type != REG_DWORD)
		return False;
	str_value = talloc_asprintf(ctx, "%d", *((uint32 *) value->data_p));
	if (!str_value) {
		return False;
	}
	status = ads_mod_str(ctx, mods, value->valuename, str_value);
	return ADS_ERR_OK(status);
}

/*
  map a boolean REG_BINARY to an ldap mod
*/
static bool map_bool(TALLOC_CTX *ctx, ADS_MODLIST *mods,
		     const struct regval_blob *value)
{
	char *str_value;
	ADS_STATUS status;

	if ((value->type != REG_BINARY) || (value->size != 1))
		return False;
	str_value =  talloc_asprintf(ctx, "%s", 
				     *(value->data_p) ? "TRUE" : "FALSE");
	if (!str_value) {
		return False;
	}
	status = ads_mod_str(ctx, mods, value->valuename, str_value);
	return ADS_ERR_OK(status);
}

/*
  map a REG_MULTI_SZ to an ldap mod
*/
static bool map_multi_sz(TALLOC_CTX *ctx, ADS_MODLIST *mods,
			 const struct regval_blob *value)
{
	char **str_values = NULL;
	size_t converted_size;
	smb_ucs2_t *cur_str = (smb_ucs2_t *) value->data_p;
        uint32 size = 0, num_vals = 0, i=0;
	ADS_STATUS status;

	if (value->type != REG_MULTI_SZ)
		return False;

	while(cur_str && *cur_str && (size < value->size)) {		
		size += 2 * (strlen_w(cur_str) + 1);
		cur_str += strlen_w(cur_str) + 1;
		num_vals++;
	};

	if (num_vals) {
		str_values = TALLOC_ARRAY(ctx, char *, num_vals + 1);
		if (!str_values) {
			return False;
		}
		memset(str_values, '\0', 
		       (num_vals + 1) * sizeof(char *));

		cur_str = (smb_ucs2_t *) value->data_p;
		for (i=0; i < num_vals; i++) {
			cur_str += pull_ucs2_talloc(ctx, &str_values[i],
						    cur_str, &converted_size) ?
			    converted_size : (size_t)-1;
		}

		status = ads_mod_strlist(ctx, mods, value->valuename, 
					 (const char **) str_values);
		return ADS_ERR_OK(status);
	} 
	return True;
}

struct valmap_to_ads {
	const char *valname;
	bool (*fn)(TALLOC_CTX *, ADS_MODLIST *, const struct regval_blob *);
};

/*
  map a REG_SZ to an ldap mod
*/
static void map_regval_to_ads(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
			      struct regval_blob *value)
{
	const struct valmap_to_ads map[] = {
		{SPOOL_REG_ASSETNUMBER, map_sz},
		{SPOOL_REG_BYTESPERMINUTE, map_dword},
		{SPOOL_REG_DEFAULTPRIORITY, map_dword},
		{SPOOL_REG_DESCRIPTION, map_sz},
		{SPOOL_REG_DRIVERNAME, map_sz},
		{SPOOL_REG_DRIVERVERSION, map_dword},
		{SPOOL_REG_FLAGS, map_dword},
		{SPOOL_REG_LOCATION, map_sz},
		{SPOOL_REG_OPERATINGSYSTEM, map_sz},
		{SPOOL_REG_OPERATINGSYSTEMHOTFIX, map_sz},
		{SPOOL_REG_OPERATINGSYSTEMSERVICEPACK, map_sz},
		{SPOOL_REG_OPERATINGSYSTEMVERSION, map_sz},
		{SPOOL_REG_PORTNAME, map_multi_sz},
		{SPOOL_REG_PRINTATTRIBUTES, map_dword},
		{SPOOL_REG_PRINTBINNAMES, map_multi_sz},
		{SPOOL_REG_PRINTCOLLATE, map_bool},
		{SPOOL_REG_PRINTCOLOR, map_bool},
		{SPOOL_REG_PRINTDUPLEXSUPPORTED, map_bool},
		{SPOOL_REG_PRINTENDTIME, map_dword},
		{SPOOL_REG_PRINTFORMNAME, map_sz},
		{SPOOL_REG_PRINTKEEPPRINTEDJOBS, map_bool},
		{SPOOL_REG_PRINTLANGUAGE, map_multi_sz},
		{SPOOL_REG_PRINTMACADDRESS, map_sz},
		{SPOOL_REG_PRINTMAXCOPIES, map_sz},
		{SPOOL_REG_PRINTMAXRESOLUTIONSUPPORTED, map_dword},
		{SPOOL_REG_PRINTMAXXEXTENT, map_dword},
		{SPOOL_REG_PRINTMAXYEXTENT, map_dword},
		{SPOOL_REG_PRINTMEDIAREADY, map_multi_sz},
		{SPOOL_REG_PRINTMEDIASUPPORTED, map_multi_sz},
		{SPOOL_REG_PRINTMEMORY, map_dword},
		{SPOOL_REG_PRINTMINXEXTENT, map_dword},
		{SPOOL_REG_PRINTMINYEXTENT, map_dword},
		{SPOOL_REG_PRINTNETWORKADDRESS, map_sz},
		{SPOOL_REG_PRINTNOTIFY, map_sz},
		{SPOOL_REG_PRINTNUMBERUP, map_dword},
		{SPOOL_REG_PRINTORIENTATIONSSUPPORTED, map_multi_sz},
		{SPOOL_REG_PRINTOWNER, map_sz},
		{SPOOL_REG_PRINTPAGESPERMINUTE, map_dword},
		{SPOOL_REG_PRINTRATE, map_dword},
		{SPOOL_REG_PRINTRATEUNIT, map_sz},
		{SPOOL_REG_PRINTSEPARATORFILE, map_sz},
		{SPOOL_REG_PRINTSHARENAME, map_sz},
		{SPOOL_REG_PRINTSPOOLING, map_sz},
		{SPOOL_REG_PRINTSTAPLINGSUPPORTED, map_bool},
		{SPOOL_REG_PRINTSTARTTIME, map_dword},
		{SPOOL_REG_PRINTSTATUS, map_sz},
		{SPOOL_REG_PRIORITY, map_dword},
		{SPOOL_REG_SERVERNAME, map_sz},
		{SPOOL_REG_SHORTSERVERNAME, map_sz},
		{SPOOL_REG_UNCNAME, map_sz},
		{SPOOL_REG_URL, map_sz},
		{SPOOL_REG_VERSIONNUMBER, map_dword},
		{NULL, NULL}
	};
	int i;

	for (i=0; map[i].valname; i++) {
		if (StrCaseCmp(map[i].valname, value->valuename) == 0) {
			if (!map[i].fn(ctx, mods, value)) {
				DEBUG(5, ("Add of value %s to modlist failed\n", value->valuename));
			} else {
				DEBUG(7, ("Mapped value %s\n", value->valuename));
			}
			
		}
	}
}


WERROR get_remote_printer_publishing_data(struct rpc_pipe_client *cli, 
					  TALLOC_CTX *mem_ctx,
					  ADS_MODLIST *mods,
					  const char *printer)
{
	WERROR result;
	char *printername;
	struct spoolss_PrinterEnumValues *info;
	uint32_t count;
	uint32 i;
	struct policy_handle pol;

	if ((asprintf(&printername, "%s\\%s", cli->srv_name_slash, printer) == -1)) {
		DEBUG(3, ("Insufficient memory\n"));
		return WERR_NOMEM;
	}

	result = rpccli_spoolss_openprinter_ex(cli, mem_ctx,
					       printername,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &pol);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(3, ("Unable to open printer %s, error is %s.\n",
			  printername, win_errstr(result)));
		SAFE_FREE(printername);
		return result;
	}

	result = rpccli_spoolss_enumprinterdataex(cli, mem_ctx, &pol,
						  SPOOL_DSDRIVER_KEY,
						  0,
						  &count,
						  &info);

	if (!W_ERROR_IS_OK(result)) {
		DEBUG(3, ("Unable to do enumdataex on %s, error is %s.\n",
			  printername, win_errstr(result)));
	} else {
		/* Have the data we need now, so start building */
		for (i=0; i < count; i++) {
			struct regval_blob v;

			fstrcpy(v.valuename, info[i].value_name);
			v.type = info[i].type;
			v.data_p = info[i].data->data;
			v.size = info[i].data->length;

			map_regval_to_ads(mem_ctx, mods, &v);
		}
	}

	result = rpccli_spoolss_enumprinterdataex(cli, mem_ctx, &pol,
						  SPOOL_DSSPOOLER_KEY,
						  0,
						  &count,
						  &info);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(3, ("Unable to do enumdataex on %s, error is %s.\n",
			  printername, win_errstr(result)));
	} else {
		for (i=0; i < count; i++) {
			struct regval_blob v;

			fstrcpy(v.valuename, info[i].value_name);
			v.type = info[i].type;
			v.data_p = info[i].data->data;
			v.size = info[i].data->length;

			map_regval_to_ads(mem_ctx, mods, &v);
		}
	}

	ads_mod_str(mem_ctx, mods, SPOOL_REG_PRINTERNAME, printer);

	rpccli_spoolss_ClosePrinter(cli, mem_ctx, &pol, NULL);
	SAFE_FREE(printername);

	return result;
}

bool get_local_printer_publishing_data(TALLOC_CTX *mem_ctx,
				       ADS_MODLIST *mods,
				       NT_PRINTER_DATA *data)
{
	uint32 key,val;

	for (key=0; key < data->num_keys; key++) {
		struct regval_ctr *ctr = data->keys[key].values;
		for (val=0; val < ctr->num_values; val++)
			map_regval_to_ads(mem_ctx, mods, ctr->values[val]);
	}
	return True;
}

#endif
