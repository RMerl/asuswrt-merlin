/* 
   Unix SMB/CIFS implementation.

   Simple LDB NTPTR backend

   Copyright (C) Stefan (metze) Metzmacher 2005

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
/*
  This implements a NTPTR backend that store
  all objects (Printers, Ports, Monitors, PrinterDrivers ...)
  in a ldb database, but doesn't do real printing.

  This is just used for testing how some of
  the SPOOLSS protocol details should work
*/

#include "includes.h"
#include "ntptr/ntptr.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include <ldb.h>
#include "auth/auth.h"
#include "dsdb/samdb/samdb.h"
#include "ldb_wrap.h"
#include "../lib/util/util_ldb.h"
#include "librpc/gen_ndr/dcerpc.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/common/common.h"
#include "param/param.h"

/*
  connect to the SPOOLSS database
  return a ldb_context pointer on success, or NULL on failure
 */
static struct ldb_context *sptr_db_connect(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx, struct loadparm_context *lp_ctx)
{
	return ldb_wrap_connect(mem_ctx, ev_ctx, lp_ctx, lpcfg_spoolss_url(lp_ctx), system_session(lp_ctx),
				NULL, 0);
}

static int sptr_db_search(struct ldb_context *ldb,
			  TALLOC_CTX *mem_ctx,
			  struct ldb_dn *basedn,
			  struct ldb_message ***res,
			  const char * const *attrs,
			  const char *format, ...) PRINTF_ATTRIBUTE(6,7);

static int sptr_db_search(struct ldb_context *ldb,
			  TALLOC_CTX *mem_ctx,
			  struct ldb_dn *basedn,
			  struct ldb_message ***res,
			  const char * const *attrs,
			  const char *format, ...)
{
	va_list ap;
	int count;

	va_start(ap, format);
	count = gendb_search_v(ldb, mem_ctx, basedn, res, attrs, format, ap);
	va_end(ap);

	return count;
}

#define SET_STRING(ldb, mod, attr, value) do { \
	if (value == NULL) return WERR_INVALID_PARAM; \
	if (ldb_msg_add_string(mod, attr, value) != LDB_SUCCESS) { \
		return WERR_NOMEM; \
	} \
} while (0)

#define SET_UINT(ldb, mod, attr, value) do { \
	if (samdb_msg_add_uint(ldb, (TALLOC_CTX *)mod, mod, attr, value) != LDB_SUCCESS) { \
		return WERR_NOMEM; \
	} \
} while (0)

static NTSTATUS sptr_init_context(struct ntptr_context *ntptr)
{
	struct ldb_context *sptr_db = sptr_db_connect(ntptr, ntptr->ev_ctx, ntptr->lp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(sptr_db);

	ntptr->private_data = sptr_db;

	return NT_STATUS_OK;
}

/* PrintServer functions */
static WERROR sptr_OpenPrintServer(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				   struct spoolss_OpenPrinterEx *r,
				   const char *server_name,
				   struct ntptr_GenericHandle **_server)
{
	struct ntptr_GenericHandle *server;

	/* TODO: do access check here! */

	server = talloc(mem_ctx, struct ntptr_GenericHandle);
	W_ERROR_HAVE_NO_MEMORY(server);

	server->type		= NTPTR_HANDLE_SERVER;
	server->ntptr		= ntptr;
	server->object_name	= talloc_strdup(server, server_name);
	W_ERROR_HAVE_NO_MEMORY(server->object_name);
	server->access_mask	= 0;
	server->private_data	= NULL;

	*_server = server;
	return WERR_OK;
}

/*
 * PrintServer PrinterData functions
 */

static WERROR sptr_PrintServerData(struct ntptr_GenericHandle *server,
				   TALLOC_CTX *mem_ctx,
				   const char *value_name,
				   union spoolss_PrinterData *r,
				   enum winreg_Type *type)
{
	struct dcerpc_server_info *server_info = lpcfg_dcerpc_server_info(mem_ctx, server->ntptr->lp_ctx);
	if (strcmp("W3SvcInstalled", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return WERR_OK;
	} else if (strcmp("BeepEnabled", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return WERR_OK;
	} else if (strcmp("EventLog", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return WERR_OK;
	} else if (strcmp("NetPopup", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return WERR_OK;
	} else if (strcmp("NetPopupToComputer", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return  WERR_OK;
	} else if (strcmp("MajorVersion", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 3;
		return WERR_OK;
	} else if (strcmp("MinorVersion", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 0;
		return WERR_OK;
	} else if (strcmp("DefaultSpoolDirectory", value_name) == 0) {
		*type		= REG_SZ;
		r->string	= "C:\\PRINTERS";
		return  WERR_OK;
	} else if (strcmp("Architecture", value_name) == 0) {
		*type		= REG_SZ;
		r->string	= SPOOLSS_ARCHITECTURE_NT_X86;
		return  WERR_OK;
	} else if (strcmp("DsPresent", value_name) == 0) {
		*type		= REG_DWORD;
		r->value	= 1;
		return WERR_OK;
	} else if (strcmp("OSVersion", value_name) == 0) {
		DATA_BLOB blob;
		enum ndr_err_code ndr_err;
		struct spoolss_OSVersion os;

		os.major		= server_info->version_major;
		os.minor		= server_info->version_minor;
		os.build		= server_info->version_build;
		os.extra_string		= "";

		ndr_err = ndr_push_struct_blob(&blob, mem_ctx, &os, (ndr_push_flags_fn_t)ndr_push_spoolss_OSVersion);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return WERR_GENERAL_FAILURE;
		}

		*type		= REG_BINARY;
		r->binary	= blob;
		return WERR_OK;
	} else if (strcmp("OSVersionEx", value_name) == 0) {
		DATA_BLOB blob;
		enum ndr_err_code ndr_err;
		struct spoolss_OSVersionEx os_ex;

		os_ex.major		= server_info->version_major;
		os_ex.minor		= server_info->version_minor;
		os_ex.build		= server_info->version_build;
		os_ex.extra_string	= "";
		os_ex.service_pack_major= 0;
		os_ex.service_pack_minor= 0;
		os_ex.suite_mask	= 0;
		os_ex.product_type	= 0;
		os_ex.reserved		= 0;

		ndr_err = ndr_push_struct_blob(&blob, mem_ctx, &os_ex, (ndr_push_flags_fn_t)ndr_push_spoolss_OSVersionEx);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return WERR_GENERAL_FAILURE;
		}

		*type		= REG_BINARY;
		r->binary	= blob;
		return WERR_OK;
	} else if (strcmp("DNSMachineName", value_name) == 0) {
		const char *dnsdomain = lpcfg_dnsdomain(server->ntptr->lp_ctx);

		if (dnsdomain == NULL) return WERR_INVALID_PARAM;

		*type		= REG_SZ;
		r->string	= talloc_asprintf(mem_ctx, "%s.%s",
							  lpcfg_netbios_name(server->ntptr->lp_ctx),
							  dnsdomain);
		W_ERROR_HAVE_NO_MEMORY(r->string);
		return WERR_OK;
	}

	return WERR_INVALID_PARAM;
}

static WERROR sptr_GetPrintServerData(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				      struct spoolss_GetPrinterData *r)
{
	WERROR result;
	union spoolss_PrinterData data;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	result = sptr_PrintServerData(server, mem_ctx, r->in.value_name, &data, r->out.type);
	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	ndr_err = ndr_push_union_blob(&blob, mem_ctx, 
				      &data, *r->out.type, (ndr_push_flags_fn_t)ndr_push_spoolss_PrinterData);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return WERR_GENERAL_FAILURE;
	}

	*r->out.needed = blob.length;

	if (r->in.offered >= *r->out.needed) {
		memcpy(r->out.data, blob.data, blob.length);
	}

	return WERR_OK;
}

/* PrintServer Form functions */
static WERROR sptr_EnumPrintServerForms(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
					struct spoolss_EnumForms *r)
{
	struct ldb_context *sptr_db = talloc_get_type(server->ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	int count;
	int i;
	union spoolss_FormInfo *info;

	count = sptr_db_search(sptr_db, mem_ctx,
				ldb_dn_new(mem_ctx, sptr_db, "CN=Forms,CN=PrintServer"),
				&msgs, NULL, "(&(objectClass=form))");

	if (count == 0) return WERR_OK;
	if (count < 0) return WERR_GENERAL_FAILURE;

	info = talloc_array(mem_ctx, union spoolss_FormInfo, count);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (r->in.level) {
	case 1:
		for (i=0; i < count; i++) {
			info[i].info1.flags		= ldb_msg_find_attr_as_uint(msgs[i], "flags", SPOOLSS_FORM_BUILTIN);

			info[i].info1.form_name		= ldb_msg_find_attr_as_string(msgs[i], "form-name", NULL);
			W_ERROR_HAVE_NO_MEMORY(info[i].info1.form_name);

			info[i].info1.size.width	= ldb_msg_find_attr_as_uint(msgs[i], "size-width", 0);
			info[i].info1.size.height	= ldb_msg_find_attr_as_uint(msgs[i], "size-height", 0);

			info[i].info1.area.left		= ldb_msg_find_attr_as_uint(msgs[i], "area-left", 0);
			info[i].info1.area.top		= ldb_msg_find_attr_as_uint(msgs[i], "area-top", 0);
			info[i].info1.area.right	= ldb_msg_find_attr_as_uint(msgs[i], "area-right", 0);
			info[i].info1.area.bottom	= ldb_msg_find_attr_as_uint(msgs[i], "area-bottom", 0);
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.info	= info;
	*r->out.count	= count;
	return WERR_OK;
}

static WERROR sptr_AddPrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				      struct spoolss_AddForm *r)
{
	struct ldb_context *sptr_db = talloc_get_type(server->ntptr->private_data, struct ldb_context);
	struct ldb_message *msg,**msgs;
	const char * const attrs[] = {"flags", NULL };
	int count, ret;

	/* TODO: do checks access here
	 * if (!(server->access_mask & desired_access)) {
	 *	return WERR_FOOBAR;
	 * }
	 */

	switch (r->in.level) {
	case 1:
		if (!r->in.info.info1) {
			return WERR_FOOBAR;
		}
		count = sptr_db_search(sptr_db, mem_ctx,
				       ldb_dn_new(mem_ctx, sptr_db, "CN=Forms,CN=PrintServer"),
				       &msgs, attrs, "(&(form-name=%s)(objectClass=form))",
				       r->in.info.info1->form_name);

		if (count == 1) return WERR_FOOBAR;
		if (count > 1) return WERR_FOOBAR;
		if (count < 0) return WERR_GENERAL_FAILURE;

		if (r->in.info.info1->flags != SPOOLSS_FORM_USER) {
			return WERR_FOOBAR;
		}

		msg = ldb_msg_new(mem_ctx);
		W_ERROR_HAVE_NO_MEMORY(msg);

		/* add core elements to the ldb_message for the Form */
		msg->dn = ldb_dn_new_fmt(msg, sptr_db, "form-name=%s,CN=Forms,CN=PrintServer", r->in.info.info1->form_name);
		SET_STRING(sptr_db, msg, "objectClass", "form");

		SET_UINT(sptr_db, msg, "flags", r->in.info.info1->flags);

		SET_STRING(sptr_db, msg, "form-name", r->in.info.info1->form_name);

		SET_UINT(sptr_db, msg, "size-width", r->in.info.info1->size.width);
		SET_UINT(sptr_db, msg, "size-height", r->in.info.info1->size.height);

		SET_UINT(sptr_db, msg, "area-left", r->in.info.info1->area.left);
		SET_UINT(sptr_db, msg, "area-top", r->in.info.info1->area.top);
		SET_UINT(sptr_db, msg, "area-right", r->in.info.info1->area.right);
		SET_UINT(sptr_db, msg, "area-bottom", r->in.info.info1->area.bottom);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	ret = ldb_add(sptr_db, msg);
	if (ret != 0) {
		return WERR_FOOBAR;
	}

	return WERR_OK;
}

static WERROR sptr_SetPrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				      struct spoolss_SetForm *r)
{
	struct ldb_context *sptr_db = talloc_get_type(server->ntptr->private_data, struct ldb_context);
	struct ldb_message *msg,**msgs;
	const char * const attrs[] = { "flags", NULL};
	int count, ret;
	enum spoolss_FormFlags flags;

	/* TODO: do checks access here
	 * if (!(server->access_mask & desired_access)) {
	 *	return WERR_FOOBAR;
	 * }
	 */

	switch (r->in.level) {
	case 1:
		if (!r->in.info.info1) {
			return WERR_FOOBAR;
		}

		count = sptr_db_search(sptr_db, mem_ctx,
				       ldb_dn_new(mem_ctx, sptr_db, "CN=Forms,CN=PrintServer"),
				       &msgs, attrs, "(&(form-name=%s)(objectClass=form))",
				       r->in.info.info1->form_name);

		if (count == 0) return WERR_FOOBAR;
		if (count > 1) return WERR_FOOBAR;
		if (count < 0) return WERR_GENERAL_FAILURE;

		flags = ldb_msg_find_attr_as_uint(msgs[0], "flags", SPOOLSS_FORM_BUILTIN);
		if (flags != SPOOLSS_FORM_USER) {
			return WERR_FOOBAR;
		}

		msg = ldb_msg_new(mem_ctx);
		W_ERROR_HAVE_NO_MEMORY(msg);

		/* add core elements to the ldb_message for the user */
		msg->dn = msgs[0]->dn;

		SET_UINT(sptr_db, msg, "flags", r->in.info.info1->flags);

		SET_STRING(sptr_db, msg, "form-name", r->in.info.info1->form_name);

		SET_UINT(sptr_db, msg, "size-width", r->in.info.info1->size.width);
		SET_UINT(sptr_db, msg, "size-height", r->in.info.info1->size.height);

		SET_UINT(sptr_db, msg, "area-left", r->in.info.info1->area.left);
		SET_UINT(sptr_db, msg, "area-top", r->in.info.info1->area.top);
		SET_UINT(sptr_db, msg, "area-right", r->in.info.info1->area.right);
		SET_UINT(sptr_db, msg, "area-bottom", r->in.info.info1->area.bottom);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	ret = dsdb_replace(sptr_db, msg, 0);
	if (ret != 0) {
		return WERR_FOOBAR;
	}

	return WERR_OK;
}

static WERROR sptr_DeletePrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
					 struct spoolss_DeleteForm *r)
{
	struct ldb_context *sptr_db = talloc_get_type(server->ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	const char * const attrs[] = { "flags", NULL};
	int count, ret;
	enum spoolss_FormFlags flags;

	/* TODO: do checks access here
	 * if (!(server->access_mask & desired_access)) {
	 *	return WERR_FOOBAR;
	 * }
	 */

	if (!r->in.form_name) {
		return WERR_FOOBAR;
	}

	count = sptr_db_search(sptr_db, mem_ctx,
			       ldb_dn_new(mem_ctx, sptr_db, "CN=Forms,CN=PrintServer"),
			       &msgs, attrs, "(&(form-name=%s)(objectclass=form))",
			       r->in.form_name);

	if (count == 0) return WERR_FOOBAR;
	if (count > 1) return WERR_FOOBAR;
	if (count < 0) return WERR_GENERAL_FAILURE;

	flags = ldb_msg_find_attr_as_uint(msgs[0], "flags", SPOOLSS_FORM_BUILTIN);
	if (flags != SPOOLSS_FORM_USER) {
		return WERR_FOOBAR;
	}

	ret = ldb_delete(sptr_db, msgs[0]->dn);
	if (ret != 0) {
		return WERR_FOOBAR;
	}

	return WERR_OK;
}

/* PrintServer Driver functions */
static WERROR sptr_EnumPrinterDrivers(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				      struct spoolss_EnumPrinterDrivers *r)
{
	return WERR_OK;
}

static WERROR sptr_GetPrinterDriverDirectory(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
					     struct spoolss_GetPrinterDriverDirectory *r)
{
	union spoolss_DriverDirectoryInfo *info;
	const char *prefix;
	const char *postfix;

	/*
	 * NOTE: normally r->in.level is 1, but both w2k3 and nt4 sp6a
	 *        are ignoring the r->in.level completely, so we do :-)
	 */
       
	/*
	 * TODO: check the server name is ours
	 * - if it's a invalid UNC then return WERR_INVALID_NAME
	 * - if it's the wrong host name return WERR_INVALID_PARAM
	 * - if it's "" then we need to return a local WINDOWS path
	 */
	if (!r->in.server || !r->in.server[0]) {
		prefix = "C:\\DRIVERS";
	} else {
		prefix = talloc_asprintf(mem_ctx, "%s\\print$", r->in.server);
		W_ERROR_HAVE_NO_MEMORY(prefix);
	}

	if (r->in.environment && strcmp(SPOOLSS_ARCHITECTURE_NT_X86, r->in.environment) == 0) {
		postfix = "W32X86";
	} else {
		return WERR_INVALID_ENVIRONMENT;
	}

	info = talloc(mem_ctx, union spoolss_DriverDirectoryInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	info->info1.directory_name	= talloc_asprintf(mem_ctx, "%s\\%s", prefix, postfix);
	W_ERROR_HAVE_NO_MEMORY(info->info1.directory_name);

	r->out.info = info;
	return WERR_OK;
}

/* Printer functions */
static WERROR sptr_EnumPrinters(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				struct spoolss_EnumPrinters *r)
{
	struct ldb_context *sptr_db = talloc_get_type(ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	int count;
	int i;
	union spoolss_PrinterInfo *info;

	count = sptr_db_search(sptr_db, mem_ctx, NULL, &msgs, NULL,
			       "(&(objectclass=printer))");

	if (count == 0) return WERR_OK;
	if (count < 0) return WERR_GENERAL_FAILURE;

	info = talloc_array(mem_ctx, union spoolss_PrinterInfo, count);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch(r->in.level) {
	case 1:
		for (i = 0; i < count; i++) {
			info[i].info1.flags		= ldb_msg_find_attr_as_uint(msgs[i], "flags", 0);

			info[i].info1.name		= ldb_msg_find_attr_as_string(msgs[i], "name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info1.name);

			info[i].info1.description	= ldb_msg_find_attr_as_string(msgs[i], "description", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info1.description);

			info[i].info1.comment		= ldb_msg_find_attr_as_string(msgs[i], "comment", NULL);
		}
		break;
	case 2:
		for (i = 0; i < count; i++) {
			info[i].info2.servername	= ldb_msg_find_attr_as_string(msgs[i], "servername", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.servername);

			info[i].info2.printername	= ldb_msg_find_attr_as_string(msgs[i], "printername", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.printername);

			info[i].info2.sharename		= ldb_msg_find_attr_as_string(msgs[i], "sharename", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.sharename);

			info[i].info2.portname		= ldb_msg_find_attr_as_string(msgs[i], "portname", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.portname);

			info[i].info2.drivername	= ldb_msg_find_attr_as_string(msgs[i], "drivername", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.drivername);

			info[i].info2.comment		= ldb_msg_find_attr_as_string(msgs[i], "comment", NULL);

			info[i].info2.location		= ldb_msg_find_attr_as_string(msgs[i], "location", NULL);

			info[i].info2.devmode		= NULL;

			info[i].info2.sepfile		= ldb_msg_find_attr_as_string(msgs[i], "sepfile", NULL);

			info[i].info2.printprocessor	= ldb_msg_find_attr_as_string(msgs[i], "printprocessor", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.printprocessor);

			info[i].info2.datatype		= ldb_msg_find_attr_as_string(msgs[i], "datatype", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.datatype);

			info[i].info2.parameters	= ldb_msg_find_attr_as_string(msgs[i], "parameters", NULL);

			info[i].info2.secdesc		= NULL;

			info[i].info2.attributes	= ldb_msg_find_attr_as_uint(msgs[i], "attributes", 0);
			info[i].info2.priority		= ldb_msg_find_attr_as_uint(msgs[i], "priority", 0);
			info[i].info2.defaultpriority	= ldb_msg_find_attr_as_uint(msgs[i], "defaultpriority", 0);
			info[i].info2.starttime		= ldb_msg_find_attr_as_uint(msgs[i], "starttime", 0);
			info[i].info2.untiltime		= ldb_msg_find_attr_as_uint(msgs[i], "untiltime", 0);
			info[i].info2.status		= ldb_msg_find_attr_as_uint(msgs[i], "status", 0);
			info[i].info2.cjobs		= ldb_msg_find_attr_as_uint(msgs[i], "cjobs", 0);
			info[i].info2.averageppm	= ldb_msg_find_attr_as_uint(msgs[i], "averageppm", 0);
		}
		break;
	case 4:
		for (i = 0; i < count; i++) {
			info[i].info4.printername	= ldb_msg_find_attr_as_string(msgs[i], "printername", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.printername);

			info[i].info4.servername	= ldb_msg_find_attr_as_string(msgs[i], "servername", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.servername);

			info[i].info4.attributes	= ldb_msg_find_attr_as_uint(msgs[i], "attributes", 0);
		}
		break;
	case 5:
		for (i = 0; i < count; i++) {
			info[i].info5.printername	= ldb_msg_find_attr_as_string(msgs[i], "name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info5.printername);

			info[i].info5.portname		= ldb_msg_find_attr_as_string(msgs[i], "port", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info5.portname);

			info[i].info5.attributes	= ldb_msg_find_attr_as_uint(msgs[i], "attributes", 0);
			info[i].info5.device_not_selected_timeout = ldb_msg_find_attr_as_uint(msgs[i], "device_not_selected_timeout", 0);
			info[i].info5.transmission_retry_timeout  = ldb_msg_find_attr_as_uint(msgs[i], "transmission_retry_timeout", 0);
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.info	= info;
	*r->out.count	= count;
	return WERR_OK;
}

static WERROR sptr_OpenPrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			       struct spoolss_OpenPrinterEx *r,
			       const char *printer_name,
			       struct ntptr_GenericHandle **printer)
{
	return WERR_INVALID_PRINTER_NAME;
}

/* port functions */
static WERROR sptr_EnumPorts(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			     struct spoolss_EnumPorts *r)
{
	struct ldb_context *sptr_db = talloc_get_type(ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	int count;
	int i;
	union spoolss_PortInfo *info;

	count = sptr_db_search(sptr_db, mem_ctx, NULL, &msgs, NULL,
			       "(&(objectclass=port))");

	if (count == 0) return WERR_OK;
	if (count < 0) return WERR_GENERAL_FAILURE;

	info = talloc_array(mem_ctx, union spoolss_PortInfo, count);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (r->in.level) {
	case 1:
		for (i = 0; i < count; i++) {
			info[i].info1.port_name		= ldb_msg_find_attr_as_string(msgs[i], "port-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info1.port_name);
		}
		break;
	case 2:
		for (i=0; i < count; i++) {
			info[i].info2.port_name		= ldb_msg_find_attr_as_string(msgs[i], "port-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.port_name);

			info[i].info2.monitor_name	= ldb_msg_find_attr_as_string(msgs[i], "monitor-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.monitor_name);

			info[i].info2.description	= ldb_msg_find_attr_as_string(msgs[i], "description", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.description);

			info[i].info2.port_type		= ldb_msg_find_attr_as_uint(msgs[i], "port-type", SPOOLSS_PORT_TYPE_WRITE);
			info[i].info2.reserved		= ldb_msg_find_attr_as_uint(msgs[i], "reserved", 0);
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.info	= info;
	*r->out.count	= count;
	return WERR_OK;
}

/* monitor functions */
static WERROR sptr_EnumMonitors(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				struct spoolss_EnumMonitors *r)
{
	struct ldb_context *sptr_db = talloc_get_type(ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	int count;
	int i;
	union spoolss_MonitorInfo *info;

	count = sptr_db_search(sptr_db, mem_ctx, NULL, &msgs, NULL,
			       "(&(objectclass=monitor))");

	if (count == 0) return WERR_OK;
	if (count < 0) return WERR_GENERAL_FAILURE;

	info = talloc_array(mem_ctx, union spoolss_MonitorInfo, count);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (r->in.level) {
	case 1:
		for (i = 0; i < count; i++) {
			info[i].info1.monitor_name	= ldb_msg_find_attr_as_string(msgs[i], "monitor-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info1.monitor_name);
		}
		break;
	case 2:
		for (i=0; i < count; i++) {
			info[i].info2.monitor_name	= ldb_msg_find_attr_as_string(msgs[i], "monitor-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.monitor_name);

			info[i].info2.environment	= ldb_msg_find_attr_as_string(msgs[i], "environment", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.environment);

			info[i].info2.dll_name		= ldb_msg_find_attr_as_string(msgs[i], "dll-name", "");
			W_ERROR_HAVE_NO_MEMORY(info[i].info2.dll_name);
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	*r->out.info	= info;
	*r->out.count	= count;
	return WERR_OK;
}

/* Printer Form functions */
static WERROR sptr_GetPrinterForm(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				  struct spoolss_GetForm *r)
{
	struct ldb_context *sptr_db = talloc_get_type(printer->ntptr->private_data, struct ldb_context);
	struct ldb_message **msgs;
	struct ldb_dn *base_dn;
	int count;
	union spoolss_FormInfo *info;

	/* TODO: do checks access here
	 * if (!(printer->access_mask & desired_access)) {
	 *	return WERR_FOOBAR;
	 * }
	 */

	base_dn = ldb_dn_new_fmt(mem_ctx, sptr_db, "CN=Forms,CN=%s,CN=Printers", printer->object_name);
	W_ERROR_HAVE_NO_MEMORY(base_dn);

	count = sptr_db_search(sptr_db, mem_ctx, base_dn, &msgs, NULL,
			       "(&(form-name=%s)(objectClass=form))",
			       r->in.form_name);

	if (count == 0) return WERR_FOOBAR;
	if (count > 1) return WERR_FOOBAR;
	if (count < 0) return WERR_GENERAL_FAILURE;

	info = talloc(mem_ctx, union spoolss_FormInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	switch (r->in.level) {
	case 1:
		info->info1.flags	= ldb_msg_find_attr_as_uint(msgs[0], "flags", SPOOLSS_FORM_BUILTIN);

		info->info1.form_name	= ldb_msg_find_attr_as_string(msgs[0], "form-name", NULL);
		W_ERROR_HAVE_NO_MEMORY(info->info1.form_name);

		info->info1.size.width	= ldb_msg_find_attr_as_uint(msgs[0], "size-width", 0);
		info->info1.size.height	= ldb_msg_find_attr_as_uint(msgs[0], "size-height", 0);

		info->info1.area.left	= ldb_msg_find_attr_as_uint(msgs[0], "area-left", 0);
		info->info1.area.top	= ldb_msg_find_attr_as_uint(msgs[0], "area-top", 0);
		info->info1.area.right	= ldb_msg_find_attr_as_uint(msgs[0], "area-right", 0);
		info->info1.area.bottom	= ldb_msg_find_attr_as_uint(msgs[0], "area-bottom", 0);
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	r->out.info	= info;
	return WERR_OK;
}

static WERROR sptr_GetPrintProcessorDirectory(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
					      struct spoolss_GetPrintProcessorDirectory *r)
{
	union spoolss_PrintProcessorDirectoryInfo *info;
	const char *prefix;
	const char *postfix;

	/*
	 * NOTE: normally r->in.level is 1, but both w2k3 and nt4 sp6a
	 *        are ignoring the r->in.level completely, so we do :-)
	 */

	/*
	 * TODO: check the server name is ours
	 * - if it's a invalid UNC then return WERR_INVALID_NAME
	 * - if it's the wrong host name return WERR_INVALID_PARAM
	 * - if it's "" then we need to return a local WINDOWS path
	 */
	if (!r->in.server || !r->in.server[0]) {
		prefix = "C:\\PRTPROCS";
	} else {
		prefix = talloc_asprintf(mem_ctx, "%s\\prnproc$", r->in.server);
		W_ERROR_HAVE_NO_MEMORY(prefix);
	}

	if (r->in.environment && strcmp(SPOOLSS_ARCHITECTURE_NT_X86, r->in.environment) == 0) {
		postfix = "W32X86";
	} else {
		return WERR_INVALID_ENVIRONMENT;
	}

	info = talloc(mem_ctx, union spoolss_PrintProcessorDirectoryInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	info->info1.directory_name	= talloc_asprintf(mem_ctx, "%s\\%s", prefix, postfix);
	W_ERROR_HAVE_NO_MEMORY(info->info1.directory_name);

	r->out.info = info;
	return WERR_OK;
}


/*
  initialialise the simble ldb backend, registering ourselves with the ntptr subsystem
 */
static const struct ntptr_ops ntptr_simple_ldb_ops = {
	.name				= "simple_ldb",
	.init_context			= sptr_init_context,

	/* PrintServer functions */
	.OpenPrintServer		= sptr_OpenPrintServer,
/*	.XcvDataPrintServer		= sptr_XcvDataPrintServer,
*/
	/* PrintServer PrinterData functions */
/*	.EnumPrintServerData		= sptr_EnumPrintServerData,
*/	.GetPrintServerData		= sptr_GetPrintServerData,
/*	.SetPrintServerData		= sptr_SetPrintServerData,
	.DeletePrintServerData		= sptr_DeletePrintServerData,
*/
	/* PrintServer Form functions */
	.EnumPrintServerForms		= sptr_EnumPrintServerForms,
	.AddPrintServerForm		= sptr_AddPrintServerForm,
	.SetPrintServerForm		= sptr_SetPrintServerForm,
	.DeletePrintServerForm		= sptr_DeletePrintServerForm,

	/* PrintServer Driver functions */
	.EnumPrinterDrivers		= sptr_EnumPrinterDrivers,
/*	.AddPrinterDriver		= sptr_AddPrinterDriver,
	.DeletePrinterDriver		= sptr_DeletePrinterDriver,
*/	.GetPrinterDriverDirectory	= sptr_GetPrinterDriverDirectory,

	/* Port functions */
	.EnumPorts			= sptr_EnumPorts,
/*	.OpenPort			= sptr_OpenPort,
	.XcvDataPort			= sptr_XcvDataPort,
*/
	/* Monitor functions */
	.EnumMonitors			= sptr_EnumMonitors,
/*	.OpenMonitor			= sptr_OpenMonitor,
	.XcvDataMonitor			= sptr_XcvDataMonitor,
*/
	/* PrintProcessor functions */
/*	.EnumPrintProcessors		= sptr_EnumPrintProcessors,
*/
	.GetPrintProcessorDirectory	= sptr_GetPrintProcessorDirectory,

	/* Printer functions */
	.EnumPrinters			= sptr_EnumPrinters,
	.OpenPrinter			= sptr_OpenPrinter,
/*	.AddPrinter			= sptr_AddPrinter,
	.GetPrinter			= sptr_GetPrinter,
	.SetPrinter			= sptr_SetPrinter,
	.DeletePrinter			= sptr_DeletePrinter,
	.XcvDataPrinter			= sptr_XcvDataPrinter,
*/
	/* Printer Driver functions */
/*	.GetPrinterDriver		= sptr_GetPrinterDriver,
*/
	/* Printer PrinterData functions */
/*	.EnumPrinterData		= sptr_EnumPrinterData,
	.GetPrinterData			= sptr_GetPrinterData,
	.SetPrinterData			= sptr_SetPrinterData,
	.DeletePrinterData		= sptr_DeletePrinterData,
*/
	/* Printer Form functions */
/*	.EnumPrinterForms		= sptr_EnumPrinterForms,
	.AddPrinterForm			= sptr_AddPrinterForm,
*/	.GetPrinterForm			= sptr_GetPrinterForm,
/*	.SetPrinterForm			= sptr_SetPrinterForm,
	.DeletePrinterForm		= sptr_DeletePrinterForm,
*/
	/* Printer Job functions */
/*	.EnumJobs			= sptr_EnumJobs,
	.AddJob				= sptr_AddJob,
	.ScheduleJob			= sptr_ScheduleJob,
	.GetJob				= sptr_GetJob,
	.SetJob				= sptr_SetJob,
*/
	/* Printer Printing functions */
/*	.StartDocPrinter		= sptr_StartDocPrinter,
	.EndDocPrinter			= sptr_EndDocPrinter,
	.StartPagePrinter		= sptr_StartPagePrinter,
	.EndPagePrinter			= sptr_EndPagePrinter,
	.WritePrinter			= sptr_WritePrinter,
	.ReadPrinter			= sptr_ReadPrinter,
*/};

NTSTATUS ntptr_simple_ldb_init(void)
{
	NTSTATUS ret;

	ret = ntptr_register(&ntptr_simple_ldb_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register NTPTR '%s' backend!\n",
			 ntptr_simple_ldb_ops.name));
	}

	return ret;
}
