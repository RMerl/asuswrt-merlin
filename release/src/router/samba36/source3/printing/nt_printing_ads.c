/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000.
 *  Copyright (C) Gerald Carter                2002-2005.
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
#include "../librpc/gen_ndr/spoolss.h"
#include "rpc_server/spoolss/srv_spoolss_util.h"
#include "nt_printing.h"
#include "ads.h"
#include "secrets.h"
#include "krb5_env.h"
#include "../libcli/registry/util_reg.h"
#include "auth.h"
#include "../librpc/ndr/libndr.h"
#include "rpc_client/cli_winreg_spoolss.h"

#ifdef HAVE_ADS
/*****************************************************************
 ****************************************************************/

static void store_printer_guid(struct messaging_context *msg_ctx,
			       const char *printer, struct GUID guid)
{
	TALLOC_CTX *tmp_ctx;
	struct auth_serversupplied_info *session_info = NULL;
	const char *guid_str;
	DATA_BLOB blob;
	NTSTATUS status;
	WERROR result;

	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) {
		DEBUG(0, ("store_printer_guid: Out of memory?!\n"));
		return;
	}

	status = make_session_info_system(tmp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("store_printer_guid: "
			  "Could not create system session_info\n"));
		goto done;
	}

	guid_str = GUID_string(tmp_ctx, &guid);
	if (!guid_str) {
		DEBUG(0, ("store_printer_guid: Out of memory?!\n"));
		goto done;
	}

	/* We used to store this as a REG_BINARY but that causes
	   Vista to whine */

	if (!push_reg_sz(tmp_ctx, &blob, guid_str)) {
		DEBUG(0, ("store_printer_guid: "
			  "Could not marshall string %s for objectGUID\n",
			  guid_str));
		goto done;
	}

	result = winreg_set_printer_dataex_internal(tmp_ctx, session_info, msg_ctx,
					   printer,
					   SPOOL_DSSPOOLER_KEY, "objectGUID",
					   REG_SZ, blob.data, blob.length);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("store_printer_guid: "
			  "Failed to store GUID for printer %s\n", printer));
	}

done:
	talloc_free(tmp_ctx);
}

WERROR nt_printer_guid_get(TALLOC_CTX *mem_ctx,
			   const struct auth_serversupplied_info *session_info,
			   struct messaging_context *msg_ctx,
			   const char *printer, struct GUID *guid)
{
	TALLOC_CTX *tmp_ctx;
	enum winreg_Type type;
	DATA_BLOB blob;
	uint32_t len;
	NTSTATUS status;
	WERROR result;

	tmp_ctx = talloc_new(mem_ctx);
	if (tmp_ctx == NULL) {
		DEBUG(0, ("out of memory?!\n"));
		return WERR_NOMEM;
	}

	result = winreg_get_printer_dataex_internal(tmp_ctx, session_info,
						    msg_ctx, printer,
						    SPOOL_DSSPOOLER_KEY,
						    "objectGUID",
						    &type,
						    &blob.data,
						    &len);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(0, ("Failed to get GUID for printer %s\n", printer));
		goto out_ctx_free;
	}
	blob.length = (size_t)len;

	/* We used to store the guid as REG_BINARY, then swapped
	   to REG_SZ for Vista compatibility so check for both */

	switch (type) {
	case REG_SZ: {
		bool ok;
		const char *guid_str;
		ok = pull_reg_sz(tmp_ctx, &blob, &guid_str);
		if (!ok) {
			DEBUG(0, ("Failed to unmarshall GUID for printer %s\n",
				  printer));
			result = WERR_REG_CORRUPT;
			goto out_ctx_free;
		}
		status = GUID_from_string(guid_str, guid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("bad GUID for printer %s\n", printer));
			result = ntstatus_to_werror(status);
			goto out_ctx_free;
		}
		break;
	}
	case REG_BINARY:
		if (blob.length != sizeof(struct GUID)) {
			DEBUG(0, ("bad GUID for printer %s\n", printer));
			result = WERR_REG_CORRUPT;
			goto out_ctx_free;
		}
		memcpy(guid, blob.data, sizeof(struct GUID));
		break;
	default:
		DEBUG(0,("GUID value stored as invalid type (%d)\n", type));
		result = WERR_REG_CORRUPT;
		goto out_ctx_free;
		break;
	}
	result = WERR_OK;

out_ctx_free:
	talloc_free(tmp_ctx);
	return result;
}

static WERROR nt_printer_info_to_mods(TALLOC_CTX *ctx,
				      struct spoolss_PrinterInfo2 *info2,
				      ADS_MODLIST *mods)
{
	char *info_str;

	ads_mod_str(ctx, mods, SPOOL_REG_PRINTERNAME, info2->sharename);
	ads_mod_str(ctx, mods, SPOOL_REG_SHORTSERVERNAME, global_myname());
	ads_mod_str(ctx, mods, SPOOL_REG_SERVERNAME, get_mydnsfullname());

	info_str = talloc_asprintf(ctx, "\\\\%s\\%s",
				   get_mydnsfullname(), info2->sharename);
	if (info_str == NULL) {
		return WERR_NOMEM;
	}
	ads_mod_str(ctx, mods, SPOOL_REG_UNCNAME, info_str);

	info_str = talloc_asprintf(ctx, "%d", 4);
	if (info_str == NULL) {
		return WERR_NOMEM;
	}
	ads_mod_str(ctx, mods, SPOOL_REG_VERSIONNUMBER, info_str);

	/* empty strings in the mods list result in an attrubute error */
	if (strlen(info2->drivername) != 0)
		ads_mod_str(ctx, mods, SPOOL_REG_DRIVERNAME, info2->drivername);
	if (strlen(info2->location) != 0)
		ads_mod_str(ctx, mods, SPOOL_REG_LOCATION, info2->location);
	if (strlen(info2->comment) != 0)
		ads_mod_str(ctx, mods, SPOOL_REG_DESCRIPTION, info2->comment);
	if (strlen(info2->portname) != 0)
		ads_mod_str(ctx, mods, SPOOL_REG_PORTNAME, info2->portname);
	if (strlen(info2->sepfile) != 0)
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTSEPARATORFILE, info2->sepfile);

	info_str = talloc_asprintf(ctx, "%u", info2->starttime);
	if (info_str == NULL) {
		return WERR_NOMEM;
	}
	ads_mod_str(ctx, mods, SPOOL_REG_PRINTSTARTTIME, info_str);

	info_str = talloc_asprintf(ctx, "%u", info2->untiltime);
	if (info_str == NULL) {
		return WERR_NOMEM;
	}
	ads_mod_str(ctx, mods, SPOOL_REG_PRINTENDTIME, info_str);

	info_str = talloc_asprintf(ctx, "%u", info2->priority);
	if (info_str == NULL) {
		return WERR_NOMEM;
	}
	ads_mod_str(ctx, mods, SPOOL_REG_PRIORITY, info_str);

	if (info2->attributes & PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS) {
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTKEEPPRINTEDJOBS, "TRUE");
	} else {
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTKEEPPRINTEDJOBS, "FALSE");
	}

	switch (info2->attributes & 0x3) {
	case 0:
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTSPOOLING,
			    SPOOL_REGVAL_PRINTWHILESPOOLING);
		break;
	case 1:
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTSPOOLING,
			    SPOOL_REGVAL_PRINTAFTERSPOOLED);
		break;
	case 2:
		ads_mod_str(ctx, mods, SPOOL_REG_PRINTSPOOLING,
			    SPOOL_REGVAL_PRINTDIRECT);
		break;
	default:
		DEBUG(3, ("unsupported printer attributes %x\n",
			  info2->attributes));
	}

	return WERR_OK;
}

static WERROR nt_printer_publish_ads(struct messaging_context *msg_ctx,
				     ADS_STRUCT *ads,
				     struct spoolss_PrinterInfo2 *pinfo2)
{
	ADS_STATUS ads_rc;
	LDAPMessage *res;
	char *prt_dn = NULL, *srv_dn, *srv_cn_0, *srv_cn_escaped, *sharename_escaped;
	char *srv_dn_utf8, **srv_cn_utf8;
	TALLOC_CTX *ctx;
	ADS_MODLIST mods;
	const char *attrs[] = {"objectGUID", NULL};
	struct GUID guid;
	WERROR win_rc = WERR_OK;
	size_t converted_size;
	const char *printer = pinfo2->sharename;

	/* build the ads mods */
	ctx = talloc_init("nt_printer_publish_ads");
	if (ctx == NULL) {
		return WERR_NOMEM;
	}

	DEBUG(5, ("publishing printer %s\n", printer));

	/* figure out where to publish */
	ads_rc = ads_find_machine_acct(ads, &res, global_myname());
	if (!ADS_ERR_OK(ads_rc)) {
		DEBUG(0, ("failed to find machine account for %s\n",
			  global_myname()));
		TALLOC_FREE(ctx);
		return WERR_NOT_FOUND;
	}

	/* We use ldap_get_dn here as we need the answer
	 * in utf8 to call ldap_explode_dn(). JRA. */

	srv_dn_utf8 = ldap_get_dn((LDAP *)ads->ldap.ld, (LDAPMessage *)res);
	ads_msgfree(ads, res);
	if (!srv_dn_utf8) {
		TALLOC_FREE(ctx);
		return WERR_SERVER_UNAVAILABLE;
	}
	srv_cn_utf8 = ldap_explode_dn(srv_dn_utf8, 1);
	if (!srv_cn_utf8) {
		TALLOC_FREE(ctx);
		ldap_memfree(srv_dn_utf8);
		return WERR_SERVER_UNAVAILABLE;
	}
	/* Now convert to CH_UNIX. */
	if (!pull_utf8_talloc(ctx, &srv_dn, srv_dn_utf8, &converted_size)) {
		TALLOC_FREE(ctx);
		ldap_memfree(srv_dn_utf8);
		ldap_memfree(srv_cn_utf8);
		return WERR_SERVER_UNAVAILABLE;
	}
	if (!pull_utf8_talloc(ctx, &srv_cn_0, srv_cn_utf8[0], &converted_size)) {
		TALLOC_FREE(ctx);
		ldap_memfree(srv_dn_utf8);
		ldap_memfree(srv_cn_utf8);
		TALLOC_FREE(srv_dn);
		return WERR_SERVER_UNAVAILABLE;
	}

	ldap_memfree(srv_dn_utf8);
	ldap_memfree(srv_cn_utf8);

	srv_cn_escaped = escape_rdn_val_string_alloc(srv_cn_0);
	if (!srv_cn_escaped) {
		TALLOC_FREE(ctx);
		return WERR_SERVER_UNAVAILABLE;
	}
	sharename_escaped = escape_rdn_val_string_alloc(printer);
	if (!sharename_escaped) {
		SAFE_FREE(srv_cn_escaped);
		TALLOC_FREE(ctx);
		return WERR_SERVER_UNAVAILABLE;
	}

	prt_dn = talloc_asprintf(ctx, "cn=%s-%s,%s", srv_cn_escaped, sharename_escaped, srv_dn);

	SAFE_FREE(srv_cn_escaped);
	SAFE_FREE(sharename_escaped);

	mods = ads_init_mods(ctx);

	if (mods == NULL) {
		TALLOC_FREE(ctx);
		return WERR_NOMEM;
	}

	win_rc = nt_printer_info_to_mods(ctx, pinfo2, &mods);
	if (!W_ERROR_IS_OK(win_rc)) {
		TALLOC_FREE(ctx);
		return win_rc;
	}

	/* publish it */
	ads_rc = ads_mod_printer_entry(ads, prt_dn, ctx, &mods);
	if (ads_rc.err.rc == LDAP_NO_SUCH_OBJECT) {
		int i;
		for (i=0; mods[i] != 0; i++)
			;
		mods[i] = (LDAPMod *)-1;
		ads_rc = ads_add_printer_entry(ads, prt_dn, ctx, &mods);
	}

	if (!ADS_ERR_OK(ads_rc)) {
		DEBUG(3, ("error publishing %s: %s\n",
			  printer, ads_errstr(ads_rc)));
	}

	/* retreive the guid and store it locally */
	if (ADS_ERR_OK(ads_search_dn(ads, &res, prt_dn, attrs))) {
		bool guid_ok;
		ZERO_STRUCT(guid);
		guid_ok = ads_pull_guid(ads, res, &guid);
		ads_msgfree(ads, res);
		if (guid_ok) {
			store_printer_guid(msg_ctx, printer, guid);
		}
	}
	TALLOC_FREE(ctx);

	return win_rc;
}

static WERROR nt_printer_unpublish_ads(ADS_STRUCT *ads,
                                       const char *printer)
{
	ADS_STATUS ads_rc;
	LDAPMessage *res = NULL;
	char *prt_dn = NULL;

	DEBUG(5, ("unpublishing printer %s\n", printer));

	/* remove the printer from the directory */
	ads_rc = ads_find_printer_on_server(ads, &res,
					    printer, global_myname());

	if (ADS_ERR_OK(ads_rc) && res && ads_count_replies(ads, res)) {
		prt_dn = ads_get_dn(ads, talloc_tos(), res);
		if (!prt_dn) {
			ads_msgfree(ads, res);
			return WERR_NOMEM;
		}
		ads_rc = ads_del_dn(ads, prt_dn);
		TALLOC_FREE(prt_dn);
	}

	if (res) {
		ads_msgfree(ads, res);
	}
	return WERR_OK;
}

/****************************************************************************
 * Publish a printer in the directory
 *
 * @param mem_ctx      memory context
 * @param session_info  session_info to access winreg pipe
 * @param pinfo2       printer information
 * @param action       publish/unpublish action
 * @return WERROR indicating status of publishing
 ***************************************************************************/

WERROR nt_printer_publish(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *session_info,
			  struct messaging_context *msg_ctx,
			  struct spoolss_PrinterInfo2 *pinfo2,
			  int action)
{
	uint32_t info2_mask = SPOOLSS_PRINTER_INFO_ATTRIBUTES;
	struct spoolss_SetPrinterInfo2 *sinfo2;
	ADS_STATUS ads_rc;
	ADS_STRUCT *ads = NULL;
	WERROR win_rc;

	sinfo2 = talloc_zero(mem_ctx, struct spoolss_SetPrinterInfo2);
	if (!sinfo2) {
		return WERR_NOMEM;
	}

	switch (action) {
	case DSPRINT_PUBLISH:
	case DSPRINT_UPDATE:
		pinfo2->attributes |= PRINTER_ATTRIBUTE_PUBLISHED;
		break;
	case DSPRINT_UNPUBLISH:
		pinfo2->attributes &= (~PRINTER_ATTRIBUTE_PUBLISHED);
		break;
	default:
		win_rc = WERR_NOT_SUPPORTED;
		goto done;
	}

	sinfo2->attributes = pinfo2->attributes;

	win_rc = winreg_update_printer_internal(mem_ctx, session_info, msg_ctx,
					pinfo2->sharename, info2_mask,
					sinfo2, NULL, NULL);
	if (!W_ERROR_IS_OK(win_rc)) {
		DEBUG(3, ("err %d saving data\n", W_ERROR_V(win_rc)));
		goto done;
	}

	TALLOC_FREE(sinfo2);

	ads = ads_init(lp_realm(), lp_workgroup(), NULL);
	if (!ads) {
		DEBUG(3, ("ads_init() failed\n"));
		win_rc = WERR_SERVER_UNAVAILABLE;
		goto done;
	}
	setenv(KRB5_ENV_CCNAME, "MEMORY:prtpub_cache", 1);
	SAFE_FREE(ads->auth.password);
	ads->auth.password = secrets_fetch_machine_password(lp_workgroup(),
		NULL, NULL);

	/* ads_connect() will find the DC for us */
	ads_rc = ads_connect(ads);
	if (!ADS_ERR_OK(ads_rc)) {
		DEBUG(3, ("ads_connect failed: %s\n", ads_errstr(ads_rc)));
		win_rc = WERR_ACCESS_DENIED;
		goto done;
	}

	switch (action) {
	case DSPRINT_PUBLISH:
	case DSPRINT_UPDATE:
		win_rc = nt_printer_publish_ads(msg_ctx, ads, pinfo2);
		break;
	case DSPRINT_UNPUBLISH:
		win_rc = nt_printer_unpublish_ads(ads, pinfo2->sharename);
		break;
	}

done:
	ads_destroy(&ads);
	return win_rc;
}

WERROR check_published_printers(struct messaging_context *msg_ctx)
{
	ADS_STATUS ads_rc;
	ADS_STRUCT *ads = NULL;
	int snum;
	int n_services = lp_numservices();
	TALLOC_CTX *tmp_ctx = NULL;
	struct auth_serversupplied_info *session_info = NULL;
	struct spoolss_PrinterInfo2 *pinfo2;
	NTSTATUS status;
	WERROR result;

	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) return WERR_NOMEM;

	ads = ads_init(lp_realm(), lp_workgroup(), NULL);
	if (!ads) {
		DEBUG(3, ("ads_init() failed\n"));
		return WERR_SERVER_UNAVAILABLE;
	}
	setenv(KRB5_ENV_CCNAME, "MEMORY:prtpub_cache", 1);
	SAFE_FREE(ads->auth.password);
	ads->auth.password = secrets_fetch_machine_password(lp_workgroup(),
		NULL, NULL);

	/* ads_connect() will find the DC for us */
	ads_rc = ads_connect(ads);
	if (!ADS_ERR_OK(ads_rc)) {
		DEBUG(3, ("ads_connect failed: %s\n", ads_errstr(ads_rc)));
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	status = make_session_info_system(tmp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("check_published_printers: "
			  "Could not create system session_info\n"));
		result = WERR_ACCESS_DENIED;
		goto done;
	}

	for (snum = 0; snum < n_services; snum++) {
		if (!lp_snum_ok(snum) || !lp_print_ok(snum)) {
			continue;
		}

		result = winreg_get_printer_internal(tmp_ctx, session_info, msg_ctx,
					    lp_servicename(snum),
					    &pinfo2);
		if (!W_ERROR_IS_OK(result)) {
			continue;
		}

		if (pinfo2->attributes & PRINTER_ATTRIBUTE_PUBLISHED) {
			nt_printer_publish_ads(msg_ctx, ads, pinfo2);
		}

		TALLOC_FREE(pinfo2);
	}

	result = WERR_OK;
done:
	ads_destroy(&ads);
	ads_kdestroy("MEMORY:prtpub_cache");
	talloc_free(tmp_ctx);
	return result;
}

bool is_printer_published(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *session_info,
			  struct messaging_context *msg_ctx,
			  const char *servername,
			  const char *printer,
			  struct spoolss_PrinterInfo2 **info2)
{
	struct spoolss_PrinterInfo2 *pinfo2 = NULL;
	WERROR result;
	struct dcerpc_binding_handle *b;

	result = winreg_printer_binding_handle(mem_ctx,
					       session_info,
					       msg_ctx,
					       &b);
	if (!W_ERROR_IS_OK(result)) {
		return false;
	}

	result = winreg_get_printer(mem_ctx, b,
				    printer, &pinfo2);
	if (!W_ERROR_IS_OK(result)) {
		return false;
	}

	if (!(pinfo2->attributes & PRINTER_ATTRIBUTE_PUBLISHED)) {
		TALLOC_FREE(pinfo2);
		return false;
	}

	if (info2) {
		*info2 = talloc_move(mem_ctx, &pinfo2);
	}
	talloc_free(pinfo2);
	return true;
}
#else
WERROR nt_printer_guid_get(TALLOC_CTX *mem_ctx,
			   const struct auth_serversupplied_info *session_info,
			   struct messaging_context *msg_ctx,
			   const char *printer, struct GUID *guid)
{
	return WERR_NOT_SUPPORTED;
}

WERROR nt_printer_publish(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *session_info,
			  struct messaging_context *msg_ctx,
			  struct spoolss_PrinterInfo2 *pinfo2,
			  int action)
{
	return WERR_OK;
}

WERROR check_published_printers(struct messaging_context *msg_ctx)
{
	return WERR_OK;
}

bool is_printer_published(TALLOC_CTX *mem_ctx,
			  const struct auth_serversupplied_info *session_info,
			  struct messaging_context *msg_ctx,
			  const char *servername,
			  const char *printer,
			  struct spoolss_PrinterInfo2 **info2)
{
	return False;
}
#endif /* HAVE_ADS */
