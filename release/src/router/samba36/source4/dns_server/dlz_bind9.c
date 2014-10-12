/*
   Unix SMB/CIFS implementation.

   bind9 dlz driver for Samba

   Copyright (C) 2010 Andrew Tridgell

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
#include "talloc.h"
#include "param/param.h"
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "auth/session.h"
#include "auth/gensec/gensec.h"
#include "gen_ndr/ndr_dnsp.h"
#include "lib/cmdline/popt_common.h"
#include "lib/cmdline/popt_credentials.h"
#include "ldb_module.h"
#include "dlz_minimal.h"

struct dlz_bind9_data {
	struct ldb_context *samdb;
	struct tevent_context *ev_ctx;
	struct loadparm_context *lp;
	int *transaction_token;
	uint32_t soa_serial;

	/* helper functions from the dlz_dlopen driver */
	void (*log)(int level, const char *fmt, ...);
	isc_result_t (*putrr)(dns_sdlzlookup_t *handle, const char *type,
			      dns_ttl_t ttl, const char *data);
	isc_result_t (*putnamedrr)(dns_sdlzlookup_t *handle, const char *name,
				   const char *type, dns_ttl_t ttl, const char *data);
	isc_result_t (*writeable_zone)(dns_view_t *view, const char *zone_name);
};


static const char *zone_prefixes[] = {
	"CN=MicrosoftDNS,DC=DomainDnsZones",
	"CN=MicrosoftDNS,DC=ForestDnsZones",
	NULL
};

/*
  return the version of the API
 */
_PUBLIC_ int dlz_version(unsigned int *flags)
{
	return DLZ_DLOPEN_VERSION;
}

/*
   remember a helper function from the bind9 dlz_dlopen driver
 */
static void b9_add_helper(struct dlz_bind9_data *state, const char *helper_name, void *ptr)
{
	if (strcmp(helper_name, "log") == 0) {
		state->log = ptr;
	}
	if (strcmp(helper_name, "putrr") == 0) {
		state->putrr = ptr;
	}
	if (strcmp(helper_name, "putnamedrr") == 0) {
		state->putnamedrr = ptr;
	}
	if (strcmp(helper_name, "writeable_zone") == 0) {
		state->writeable_zone = ptr;
	}
}

/*
  format a record for bind9
 */
static bool b9_format(struct dlz_bind9_data *state,
		      TALLOC_CTX *mem_ctx,
		      struct dnsp_DnssrvRpcRecord *rec,
		      const char **type, const char **data)
{
	switch (rec->wType) {
	case DNS_TYPE_A:
		*type = "a";
		*data = rec->data.ipv4;
		break;

	case DNS_TYPE_AAAA:
		*type = "aaaa";
		*data = rec->data.ipv6;
		break;

	case DNS_TYPE_CNAME:
		*type = "cname";
		*data = rec->data.cname;
		break;

	case DNS_TYPE_TXT:
		*type = "txt";
		*data = rec->data.txt;
		break;

	case DNS_TYPE_PTR:
		*type = "ptr";
		*data = rec->data.ptr;
		break;

	case DNS_TYPE_SRV:
		*type = "srv";
		*data = talloc_asprintf(mem_ctx, "%u %u %u %s",
					rec->data.srv.wPriority,
					rec->data.srv.wWeight,
					rec->data.srv.wPort,
					rec->data.srv.nameTarget);
		break;

	case DNS_TYPE_MX:
		*type = "mx";
		*data = talloc_asprintf(mem_ctx, "%u %s",
					rec->data.mx.wPriority,
					rec->data.mx.nameTarget);
		break;

	case DNS_TYPE_HINFO:
		*type = "hinfo";
		*data = talloc_asprintf(mem_ctx, "%s %s",
					rec->data.hinfo.cpu,
					rec->data.hinfo.os);
		break;

	case DNS_TYPE_NS:
		*type = "ns";
		*data = rec->data.ns;
		break;

	case DNS_TYPE_SOA: {
		const char *mname;
		*type = "soa";

		/* we need to fake the authoritative nameserver to
		 * point at ourselves. This is how AD DNS servers
		 * force clients to send updates to the right local DC
		 */
		mname = talloc_asprintf(mem_ctx, "%s.%s",
					lpcfg_netbios_name(state->lp), lpcfg_dnsdomain(state->lp));
		if (mname == NULL) {
			return false;
		}
		mname = strlower_talloc(mem_ctx, mname);
		if (mname == NULL) {
			return false;
		}

		state->soa_serial = rec->data.soa.serial;

		*data = talloc_asprintf(mem_ctx, "%s %s %u %u %u %u %u",
					mname,
					rec->data.soa.rname,
					rec->data.soa.serial,
					rec->data.soa.refresh,
					rec->data.soa.retry,
					rec->data.soa.expire,
					rec->data.soa.minimum);
		break;
	}

	default:
		state->log(ISC_LOG_ERROR, "samba b9_putrr: unhandled record type %u",
			   rec->wType);
		return false;
	}

	return true;
}

static const struct {
	enum dns_record_type dns_type;
	const char *typestr;
	bool single_valued;
} dns_typemap[] = {
	{ DNS_TYPE_A,     "A"     , false},
	{ DNS_TYPE_AAAA,  "AAAA"  , false},
	{ DNS_TYPE_CNAME, "CNAME" , true},
	{ DNS_TYPE_TXT,   "TXT"   , false},
	{ DNS_TYPE_PTR,   "PTR"   , false},
	{ DNS_TYPE_SRV,   "SRV"   , false},
	{ DNS_TYPE_MX,    "MX"    , false},
	{ DNS_TYPE_HINFO, "HINFO" , false},
	{ DNS_TYPE_NS,    "NS"    , false},
	{ DNS_TYPE_SOA,   "SOA"   , true},
};


/*
  see if a DNS type is single valued
 */
static bool b9_single_valued(enum dns_record_type dns_type)
{
	int i;
	for (i=0; i<ARRAY_SIZE(dns_typemap); i++) {
		if (dns_typemap[i].dns_type == dns_type) {
			return dns_typemap[i].single_valued;
		}
	}
	return false;
}

/*
  see if a DNS type is single valued
 */
static bool b9_dns_type(const char *type, enum dns_record_type *dtype)
{
	int i;
	for (i=0; i<ARRAY_SIZE(dns_typemap); i++) {
		if (strcasecmp(dns_typemap[i].typestr, type) == 0) {
			*dtype = dns_typemap[i].dns_type;
			return true;
		}
	}
	return false;
}


#define DNS_PARSE_STR(ret, str, sep, saveptr) do {	\
	(ret) = strtok_r(str, sep, &saveptr); \
	if ((ret) == NULL) return false; \
	} while (0)

#define DNS_PARSE_UINT(ret, str, sep, saveptr) do {  \
	char *istr = strtok_r(str, sep, &saveptr); \
	if ((istr) == NULL) return false; \
	(ret) = strtoul(istr, NULL, 10); \
	} while (0)

/*
  parse a record from bind9
 */
static bool b9_parse(struct dlz_bind9_data *state,
		     const char *rdatastr,
		     struct dnsp_DnssrvRpcRecord *rec)
{
	char *full_name, *dclass, *type;
	char *str, *saveptr=NULL;
	int i;

	str = talloc_strdup(rec, rdatastr);
	if (str == NULL) {
		return false;
	}

	/* parse the SDLZ string form */
	DNS_PARSE_STR(full_name, str, "\t", saveptr);
	DNS_PARSE_UINT(rec->dwTtlSeconds, NULL, "\t", saveptr);
	DNS_PARSE_STR(dclass, NULL, "\t", saveptr);
	DNS_PARSE_STR(type, NULL, "\t", saveptr);

	/* construct the record */
	for (i=0; i<ARRAY_SIZE(dns_typemap); i++) {
		if (strcasecmp(type, dns_typemap[i].typestr) == 0) {
			rec->wType = dns_typemap[i].dns_type;
			break;
		}
	}
	if (i == ARRAY_SIZE(dns_typemap)) {
		state->log(ISC_LOG_ERROR, "samba_dlz: unsupported record type '%s' for '%s'",
			   type, full_name);
		return false;
	}

	switch (rec->wType) {
	case DNS_TYPE_A:
		DNS_PARSE_STR(rec->data.ipv4, NULL, " ", saveptr);
		break;

	case DNS_TYPE_AAAA:
		DNS_PARSE_STR(rec->data.ipv6, NULL, " ", saveptr);
		break;

	case DNS_TYPE_CNAME:
		DNS_PARSE_STR(rec->data.cname, NULL, " ", saveptr);
		break;

	case DNS_TYPE_TXT:
		DNS_PARSE_STR(rec->data.txt, NULL, "\t", saveptr);
		break;

	case DNS_TYPE_PTR:
		DNS_PARSE_STR(rec->data.ptr, NULL, " ", saveptr);
		break;

	case DNS_TYPE_SRV:
		DNS_PARSE_UINT(rec->data.srv.wPriority, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.srv.wWeight, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.srv.wPort, NULL, " ", saveptr);
		DNS_PARSE_STR(rec->data.srv.nameTarget, NULL, " ", saveptr);
		break;

	case DNS_TYPE_MX:
		DNS_PARSE_UINT(rec->data.mx.wPriority, NULL, " ", saveptr);
		DNS_PARSE_STR(rec->data.mx.nameTarget, NULL, " ", saveptr);
		break;

	case DNS_TYPE_HINFO:
		DNS_PARSE_STR(rec->data.hinfo.cpu, NULL, " ", saveptr);
		DNS_PARSE_STR(rec->data.hinfo.os, NULL, " ", saveptr);
		break;

	case DNS_TYPE_NS:
		DNS_PARSE_STR(rec->data.ns, NULL, " ", saveptr);
		break;

	case DNS_TYPE_SOA:
		DNS_PARSE_STR(rec->data.soa.mname, NULL, " ", saveptr);
		DNS_PARSE_STR(rec->data.soa.rname, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.soa.serial, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.soa.refresh, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.soa.retry, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.soa.expire, NULL, " ", saveptr);
		DNS_PARSE_UINT(rec->data.soa.minimum, NULL, " ", saveptr);
		break;

	default:
		state->log(ISC_LOG_ERROR, "samba b9_parse: unhandled record type %u",
			   rec->wType);
		return false;
	}

	/* we should be at the end of the buffer now */
	if (strtok_r(NULL, "\t ", &saveptr) != NULL) {
		state->log(ISC_LOG_ERROR, "samba b9_parse: expected data at end of string for '%s'");
		return false;
	}

	return true;
}

/*
  send a resource recond to bind9
 */
static isc_result_t b9_putrr(struct dlz_bind9_data *state,
			     void *handle, struct dnsp_DnssrvRpcRecord *rec,
			     const char **types)
{
	isc_result_t result;
	const char *type, *data;
	TALLOC_CTX *tmp_ctx = talloc_new(state);

	if (!b9_format(state, tmp_ctx, rec, &type, &data)) {
		return ISC_R_FAILURE;
	}

	if (data == NULL) {
		talloc_free(tmp_ctx);
		return ISC_R_NOMEMORY;
	}

	if (types) {
		int i;
		for (i=0; types[i]; i++) {
			if (strcmp(types[i], type) == 0) break;
		}
		if (types[i] == NULL) {
			/* skip it */
			return ISC_R_SUCCESS;
		}
	}

	result = state->putrr(handle, type, rec->dwTtlSeconds, data);
	if (result != ISC_R_SUCCESS) {
		state->log(ISC_LOG_ERROR, "Failed to put rr");
	}
	talloc_free(tmp_ctx);
	return result;
}


/*
  send a named resource recond to bind9
 */
static isc_result_t b9_putnamedrr(struct dlz_bind9_data *state,
				  void *handle, const char *name,
				  struct dnsp_DnssrvRpcRecord *rec)
{
	isc_result_t result;
	const char *type, *data;
	TALLOC_CTX *tmp_ctx = talloc_new(state);

	if (!b9_format(state, tmp_ctx, rec, &type, &data)) {
		return ISC_R_FAILURE;
	}

	if (data == NULL) {
		talloc_free(tmp_ctx);
		return ISC_R_NOMEMORY;
	}

	result = state->putnamedrr(handle, name, type, rec->dwTtlSeconds, data);
	if (result != ISC_R_SUCCESS) {
		state->log(ISC_LOG_ERROR, "Failed to put named rr '%s'", name);
	}
	talloc_free(tmp_ctx);
	return result;
}

struct b9_options {
	const char *url;
};

/*
   parse options
 */
static isc_result_t parse_options(struct dlz_bind9_data *state,
				  unsigned int argc, char *argv[],
				  struct b9_options *options)
{
	int opt;
	poptContext pc;
	struct poptOption long_options[] = {
		{ "url",       'H', POPT_ARG_STRING, &options->url, 0, "database URL", "URL" },
		{ NULL }
	};
	struct poptOption **popt_options;
	int ret;

	fault_setup_disable();

	popt_options = ldb_module_popt_options(state->samdb);
	(*popt_options) = long_options;

	ret = ldb_modules_hook(state->samdb, LDB_MODULE_HOOK_CMDLINE_OPTIONS);
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "dlz samba: failed cmdline hook");
		return ISC_R_FAILURE;
	}

	pc = poptGetContext("dlz_bind9", argc, (const char **)argv, *popt_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		default:
			state->log(ISC_LOG_ERROR, "dlz samba: Invalid option %s: %s",
				   poptBadOption(pc, 0), poptStrerror(opt));
			return ISC_R_FAILURE;
		}
	}

	ret = ldb_modules_hook(state->samdb, LDB_MODULE_HOOK_CMDLINE_PRECONNECT);
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "dlz samba: failed cmdline preconnect");
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}


/*
  called to initialise the driver
 */
_PUBLIC_ isc_result_t dlz_create(const char *dlzname,
				 unsigned int argc, char *argv[],
				 void **dbdata, ...)
{
	struct dlz_bind9_data *state;
	const char *helper_name;
	va_list ap;
	isc_result_t result;
	TALLOC_CTX *tmp_ctx;
	int ret;
	struct ldb_dn *dn;
	struct b9_options options;

	ZERO_STRUCT(options);

	state = talloc_zero(NULL, struct dlz_bind9_data);
	if (state == NULL) {
		return ISC_R_NOMEMORY;
	}

	tmp_ctx = talloc_new(state);

	/* fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char *)) != NULL) {
		b9_add_helper(state, helper_name, va_arg(ap, void*));
	}
	va_end(ap);

	state->ev_ctx = s4_event_context_init(state);
	if (state->ev_ctx == NULL) {
		result = ISC_R_NOMEMORY;
		goto failed;
	}

	state->samdb = ldb_init(state, state->ev_ctx);
	if (state->samdb == NULL) {
		state->log(ISC_LOG_ERROR, "samba_dlz: Failed to create ldb");
		result = ISC_R_FAILURE;
		goto failed;
	}

	result = parse_options(state, argc, argv, &options);
	if (result != ISC_R_SUCCESS) {
		goto failed;
	}

	state->lp = loadparm_init_global(true);
	if (state->lp == NULL) {
		result = ISC_R_NOMEMORY;
		goto failed;
	}

	if (options.url == NULL) {
		options.url = talloc_asprintf(tmp_ctx, "ldapi://%s",
					      private_path(tmp_ctx, state->lp, "ldap_priv/ldapi"));
		if (options.url == NULL) {
			result = ISC_R_NOMEMORY;
			goto failed;
		}
	}

	ret = ldb_connect(state->samdb, options.url, 0, NULL);
	if (ret == -1) {
		state->log(ISC_LOG_ERROR, "samba_dlz: Failed to connect to %s - %s",
			   options.url, ldb_errstring(state->samdb));
		result = ISC_R_FAILURE;
		goto failed;
	}

	ret = ldb_modules_hook(state->samdb, LDB_MODULE_HOOK_CMDLINE_POSTCONNECT);
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "samba_dlz: Failed postconnect for %s - %s",
			   options.url, ldb_errstring(state->samdb));
		result = ISC_R_FAILURE;
		goto failed;
	}

	dn = ldb_get_default_basedn(state->samdb);
	if (dn == NULL) {
		state->log(ISC_LOG_ERROR, "samba_dlz: Unable to get basedn for %s - %s",
			   options.url, ldb_errstring(state->samdb));
		result = ISC_R_FAILURE;
		goto failed;
	}

	state->log(ISC_LOG_INFO, "samba_dlz: started for DN %s",
		   ldb_dn_get_linearized(dn));

	*dbdata = state;

	talloc_free(tmp_ctx);
	return ISC_R_SUCCESS;

failed:
	talloc_free(state);
	return result;
}

/*
  shutdown the backend
 */
_PUBLIC_ void dlz_destroy(void *dbdata)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	state->log(ISC_LOG_INFO, "samba_dlz: shutting down");
	talloc_free(state);
}


/*
  return the base DN for a zone
 */
static isc_result_t b9_find_zone_dn(struct dlz_bind9_data *state, const char *zone_name,
				    TALLOC_CTX *mem_ctx, struct ldb_dn **zone_dn)
{
	int ret;
	TALLOC_CTX *tmp_ctx = talloc_new(state);
	const char *attrs[] = { NULL };
	int i;

	for (i=0; zone_prefixes[i]; i++) {
		struct ldb_dn *dn;
		struct ldb_result *res;

		dn = ldb_dn_copy(tmp_ctx, ldb_get_default_basedn(state->samdb));
		if (dn == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		if (!ldb_dn_add_child_fmt(dn, "DC=%s,%s", zone_name, zone_prefixes[i])) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, attrs, "objectClass=dnsZone");
		if (ret == LDB_SUCCESS) {
			if (zone_dn != NULL) {
				*zone_dn = talloc_steal(mem_ctx, dn);
			}
			talloc_free(tmp_ctx);
			return ISC_R_SUCCESS;
		}
		talloc_free(dn);
	}

	talloc_free(tmp_ctx);
	return ISC_R_NOTFOUND;
}


/*
  return the DN for a name. The record does not need to exist, but the
  zone must exist
 */
static isc_result_t b9_find_name_dn(struct dlz_bind9_data *state, const char *name,
				    TALLOC_CTX *mem_ctx, struct ldb_dn **dn)
{
	const char *p;

	/* work through the name piece by piece, until we find a zone */
	for (p=name; p; ) {
		isc_result_t result;
		result = b9_find_zone_dn(state, p, mem_ctx, dn);
		if (result == ISC_R_SUCCESS) {
			/* we found a zone, now extend the DN to get
			 * the full DN
			 */
			bool ret;
			if (p == name) {
				ret = ldb_dn_add_child_fmt(*dn, "DC=@");
			} else {
				ret = ldb_dn_add_child_fmt(*dn, "DC=%.*s", (int)(p-name)-1, name);
			}
			if (!ret) {
				talloc_free(*dn);
				return ISC_R_NOMEMORY;
			}
			return ISC_R_SUCCESS;
		}
		p = strchr(p, '.');
		if (p == NULL) {
			break;
		}
		p++;
	}
	return ISC_R_NOTFOUND;
}


/*
  see if we handle a given zone
 */
_PUBLIC_ isc_result_t dlz_findzonedb(void *dbdata, const char *name)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	return b9_find_zone_dn(state, name, NULL, NULL);
}


/*
  lookup one record
 */
static isc_result_t dlz_lookup_types(struct dlz_bind9_data *state,
				     const char *zone, const char *name,
				     dns_sdlzlookup_t *lookup,
				     const char **types)
{
	TALLOC_CTX *tmp_ctx = talloc_new(state);
	const char *attrs[] = { "dnsRecord", NULL };
	int ret = LDB_SUCCESS, i;
	struct ldb_result *res;
	struct ldb_message_element *el;
	struct ldb_dn *dn;

	for (i=0; zone_prefixes[i]; i++) {
		dn = ldb_dn_copy(tmp_ctx, ldb_get_default_basedn(state->samdb));
		if (dn == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		if (!ldb_dn_add_child_fmt(dn, "DC=%s,DC=%s,%s", name, zone, zone_prefixes[i])) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_BASE,
				 attrs, "objectClass=dnsNode");
		if (ret == LDB_SUCCESS) {
			break;
		}
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ISC_R_NOTFOUND;
	}

	el = ldb_msg_find_element(res->msgs[0], "dnsRecord");
	if (el == NULL || el->num_values == 0) {
		talloc_free(tmp_ctx);
		return ISC_R_NOTFOUND;
	}

	for (i=0; i<el->num_values; i++) {
		struct dnsp_DnssrvRpcRecord rec;
		enum ndr_err_code ndr_err;
		isc_result_t result;

		ndr_err = ndr_pull_struct_blob(&el->values[i], tmp_ctx, &rec,
					       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			state->log(ISC_LOG_ERROR, "samba_dlz: failed to parse dnsRecord for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(tmp_ctx);
			return ISC_R_FAILURE;
		}

		result = b9_putrr(state, lookup, &rec, types);
		if (result != ISC_R_SUCCESS) {
			talloc_free(tmp_ctx);
			return result;
		}
	}

	talloc_free(tmp_ctx);
	return ISC_R_SUCCESS;
}

/*
  lookup one record
 */
_PUBLIC_ isc_result_t dlz_lookup(const char *zone, const char *name,
				 void *dbdata, dns_sdlzlookup_t *lookup)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	return dlz_lookup_types(state, zone, name, lookup, NULL);
}


/*
  see if a zone transfer is allowed
 */
_PUBLIC_ isc_result_t dlz_allowzonexfr(void *dbdata, const char *name, const char *client)
{
	/* just say yes for all our zones for now */
	return dlz_findzonedb(dbdata, name);
}

/*
  perform a zone transfer
 */
_PUBLIC_ isc_result_t dlz_allnodes(const char *zone, void *dbdata,
				   dns_sdlzallnodes_t *allnodes)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	const char *attrs[] = { "dnsRecord", NULL };
	int ret = LDB_SUCCESS, i, j;
	struct ldb_dn *dn;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx = talloc_new(state);

	for (i=0; zone_prefixes[i]; i++) {
		dn = ldb_dn_copy(tmp_ctx, ldb_get_default_basedn(state->samdb));
		if (dn == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		if (!ldb_dn_add_child_fmt(dn, "DC=%s,%s", zone, zone_prefixes[i])) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_SUBTREE,
				 attrs, "objectClass=dnsNode");
		if (ret == LDB_SUCCESS) {
			break;
		}
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ISC_R_NOTFOUND;
	}

	for (i=0; i<res->count; i++) {
		struct ldb_message_element *el;
		TALLOC_CTX *el_ctx = talloc_new(tmp_ctx);
		const char *rdn, *name;
		const struct ldb_val *v;

		el = ldb_msg_find_element(res->msgs[i], "dnsRecord");
		if (el == NULL || el->num_values == 0) {
			state->log(ISC_LOG_INFO, "failed to find dnsRecord for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(el_ctx);
			continue;
		}

		v = ldb_dn_get_rdn_val(res->msgs[i]->dn);
		if (v == NULL) {
			state->log(ISC_LOG_INFO, "failed to find RDN for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(el_ctx);
			continue;
		}

		rdn = talloc_strndup(el_ctx, (char *)v->data, v->length);
		if (rdn == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		if (strcmp(rdn, "@") == 0) {
			name = zone;
		} else {
			name = talloc_asprintf(el_ctx, "%s.%s", rdn, zone);
		}
		if (name == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		for (j=0; j<el->num_values; j++) {
			struct dnsp_DnssrvRpcRecord rec;
			enum ndr_err_code ndr_err;
			isc_result_t result;

			ndr_err = ndr_pull_struct_blob(&el->values[j], el_ctx, &rec,
						       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				state->log(ISC_LOG_ERROR, "samba_dlz: failed to parse dnsRecord for %s",
					   ldb_dn_get_linearized(dn));
				continue;
			}

			result = b9_putnamedrr(state, allnodes, name, &rec);
			if (result != ISC_R_SUCCESS) {
				continue;
			}
		}
	}

	talloc_free(tmp_ctx);

	return ISC_R_SUCCESS;
}


/*
  start a transaction
 */
_PUBLIC_ isc_result_t dlz_newversion(const char *zone, void *dbdata, void **versionp)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);

	state->log(ISC_LOG_INFO, "samba_dlz: starting transaction on zone %s", zone);

	if (state->transaction_token != NULL) {
		state->log(ISC_LOG_INFO, "samba_dlz: transaction already started for zone %s", zone);
		return ISC_R_FAILURE;
	}

	state->transaction_token = talloc_zero(state, int);
	if (state->transaction_token == NULL) {
		return ISC_R_NOMEMORY;
	}

	if (ldb_transaction_start(state->samdb) != LDB_SUCCESS) {
		state->log(ISC_LOG_INFO, "samba_dlz: failed to start a transaction for zone %s", zone);
		talloc_free(state->transaction_token);
		state->transaction_token = NULL;
		return ISC_R_FAILURE;
	}

	*versionp = (void *)state->transaction_token;

	return ISC_R_SUCCESS;
}

/*
  end a transaction
 */
_PUBLIC_ void dlz_closeversion(const char *zone, isc_boolean_t commit,
			       void *dbdata, void **versionp)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);

	if (state->transaction_token != (int *)*versionp) {
		state->log(ISC_LOG_INFO, "samba_dlz: transaction not started for zone %s", zone);
		return;
	}

	if (commit) {
		if (ldb_transaction_commit(state->samdb) != LDB_SUCCESS) {
			state->log(ISC_LOG_INFO, "samba_dlz: failed to commit a transaction for zone %s", zone);
			return;
		}
		state->log(ISC_LOG_INFO, "samba_dlz: committed transaction on zone %s", zone);
	} else {
		if (ldb_transaction_cancel(state->samdb) != LDB_SUCCESS) {
			state->log(ISC_LOG_INFO, "samba_dlz: failed to cancel a transaction for zone %s", zone);
			return;
		}
		state->log(ISC_LOG_INFO, "samba_dlz: cancelling transaction on zone %s", zone);
	}

	talloc_free(state->transaction_token);
	state->transaction_token = NULL;
	*versionp = NULL;
}


/*
  see if there is a SOA record for a zone
 */
static bool b9_has_soa(struct dlz_bind9_data *state, struct ldb_dn *dn, const char *zone)
{
	const char *attrs[] = { "dnsRecord", NULL };
	struct ldb_result *res;
	struct ldb_message_element *el;
	TALLOC_CTX *tmp_ctx = talloc_new(state);
	int ret, i;

	if (!ldb_dn_add_child_fmt(dn, "DC=@,DC=%s", zone)) {
		talloc_free(tmp_ctx);
		return false;
	}

	ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_BASE,
			 attrs, "objectClass=dnsNode");
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return false;
	}

	el = ldb_msg_find_element(res->msgs[0], "dnsRecord");
	if (el == NULL) {
		talloc_free(tmp_ctx);
		return false;
	}
	for (i=0; i<el->num_values; i++) {
		struct dnsp_DnssrvRpcRecord rec;
		enum ndr_err_code ndr_err;

		ndr_err = ndr_pull_struct_blob(&el->values[i], tmp_ctx, &rec,
					       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			continue;
		}
		if (rec.wType == DNS_TYPE_SOA) {
			talloc_free(tmp_ctx);
			return true;
		}
	}

	talloc_free(tmp_ctx);
	return false;
}

/*
  configure a writeable zone
 */
_PUBLIC_ isc_result_t dlz_configure(dns_view_t *view, void *dbdata)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *dn;
	int i;

	state->log(ISC_LOG_INFO, "samba_dlz: starting configure");
	if (state->writeable_zone == NULL) {
		state->log(ISC_LOG_INFO, "samba_dlz: no writeable_zone method available");
		return ISC_R_FAILURE;
	}

	tmp_ctx = talloc_new(state);

	for (i=0; zone_prefixes[i]; i++) {
		const char *attrs[] = { "name", NULL };
		int j, ret;
		struct ldb_result *res;

		dn = ldb_dn_copy(tmp_ctx, ldb_get_default_basedn(state->samdb));
		if (dn == NULL) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		if (!ldb_dn_add_child_fmt(dn, "%s", zone_prefixes[i])) {
			talloc_free(tmp_ctx);
			return ISC_R_NOMEMORY;
		}

		ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_SUBTREE,
				 attrs, "objectClass=dnsZone");
		if (ret != LDB_SUCCESS) {
			continue;
		}

		for (j=0; j<res->count; j++) {
			isc_result_t result;
			const char *zone = ldb_msg_find_attr_as_string(res->msgs[j], "name", NULL);
			if (zone == NULL) {
				continue;
			}
			if (!b9_has_soa(state, dn, zone)) {
				continue;
			}
			result = state->writeable_zone(view, zone);
			if (result != ISC_R_SUCCESS) {
				state->log(ISC_LOG_ERROR, "samba_dlz: Failed to configure zone '%s'",
					   zone);
				talloc_free(tmp_ctx);
				return result;
			}
			state->log(ISC_LOG_INFO, "samba_dlz: configured writeable zone '%s'", zone);
		}
	}

	talloc_free(tmp_ctx);
	return ISC_R_SUCCESS;
}

/*
  authorize a zone update
 */
_PUBLIC_ isc_boolean_t dlz_ssumatch(const char *signer, const char *name, const char *tcpaddr,
				    const char *type, const char *key, uint32_t keydatalen, uint8_t *keydata,
				    void *dbdata)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);

	state->log(ISC_LOG_INFO, "samba_dlz: allowing update of signer=%s name=%s tcpaddr=%s type=%s key=%s keydatalen=%u",
		   signer, name, tcpaddr, type, key, keydatalen);
	return true;
}


/*
  add a new record
 */
static isc_result_t b9_add_record(struct dlz_bind9_data *state, const char *name,
				  struct ldb_dn *dn,
				  struct dnsp_DnssrvRpcRecord *rec)
{
	struct ldb_message *msg;
	enum ndr_err_code ndr_err;
	struct ldb_val v;
	int ret;

	msg = ldb_msg_new(rec);
	if (msg == NULL) {
		return ISC_R_NOMEMORY;
	}
	msg->dn = dn;
	ret = ldb_msg_add_string(msg, "objectClass", "dnsNode");
	if (ret != LDB_SUCCESS) {
		return ISC_R_FAILURE;
	}

	ndr_err = ndr_push_struct_blob(&v, rec, rec, (ndr_push_flags_fn_t)ndr_push_dnsp_DnssrvRpcRecord);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ISC_R_FAILURE;
	}
	ret = ldb_msg_add_value(msg, "dnsRecord", &v, NULL);
	if (ret != LDB_SUCCESS) {
		return ISC_R_FAILURE;
	}

	ret = ldb_add(state->samdb, msg);
	if (ret != LDB_SUCCESS) {
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}

/*
  see if two DNS names are the same
 */
static bool dns_name_equal(const char *name1, const char *name2)
{
	size_t len1 = strlen(name1);
	size_t len2 = strlen(name2);
	if (name1[len1-1] == '.') len1--;
	if (name2[len2-1] == '.') len2--;
	if (len1 != len2) {
		return false;
	}
	return strncasecmp_m(name1, name2, len1) == 0;
}


/*
  see if two dns records match
 */
static bool b9_record_match(struct dlz_bind9_data *state,
			    struct dnsp_DnssrvRpcRecord *rec1, struct dnsp_DnssrvRpcRecord *rec2)
{
	if (rec1->wType != rec2->wType) {
		return false;
	}
	/* see if this type is single valued */
	if (b9_single_valued(rec1->wType)) {
		return true;
	}

	/* see if the data matches */
	switch (rec1->wType) {
	case DNS_TYPE_A:
		return strcmp(rec1->data.ipv4, rec2->data.ipv4) == 0;
	case DNS_TYPE_AAAA:
		return strcmp(rec1->data.ipv6, rec2->data.ipv6) == 0;
	case DNS_TYPE_CNAME:
		return dns_name_equal(rec1->data.cname, rec2->data.cname);
	case DNS_TYPE_TXT:
		return strcmp(rec1->data.txt, rec2->data.txt) == 0;
	case DNS_TYPE_PTR:
		return strcmp(rec1->data.ptr, rec2->data.ptr) == 0;
	case DNS_TYPE_NS:
		return dns_name_equal(rec1->data.ns, rec2->data.ns);

	case DNS_TYPE_SRV:
		return rec1->data.srv.wPriority == rec2->data.srv.wPriority &&
			rec1->data.srv.wWeight  == rec2->data.srv.wWeight &&
			rec1->data.srv.wPort    == rec2->data.srv.wPort &&
			dns_name_equal(rec1->data.srv.nameTarget, rec2->data.srv.nameTarget);

	case DNS_TYPE_MX:
		return rec1->data.mx.wPriority == rec2->data.mx.wPriority &&
			dns_name_equal(rec1->data.mx.nameTarget, rec2->data.mx.nameTarget);

	case DNS_TYPE_HINFO:
		return strcmp(rec1->data.hinfo.cpu, rec2->data.hinfo.cpu) == 0 &&
			strcmp(rec1->data.hinfo.os, rec2->data.hinfo.os) == 0;

	case DNS_TYPE_SOA:
		return dns_name_equal(rec1->data.soa.mname, rec2->data.soa.mname) &&
			dns_name_equal(rec1->data.soa.rname, rec2->data.soa.rname) &&
			rec1->data.soa.serial == rec2->data.soa.serial &&
			rec1->data.soa.refresh == rec2->data.soa.refresh &&
			rec1->data.soa.retry == rec2->data.soa.retry &&
			rec1->data.soa.expire == rec2->data.soa.expire &&
			rec1->data.soa.minimum == rec2->data.soa.minimum;
	default:
		state->log(ISC_LOG_ERROR, "samba b9_putrr: unhandled record type %u",
			   rec1->wType);
		break;
	}

	return false;
}


/*
  add or modify a rdataset
 */
_PUBLIC_ isc_result_t dlz_addrdataset(const char *name, const char *rdatastr, void *dbdata, void *version)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	struct dnsp_DnssrvRpcRecord *rec;
	struct ldb_dn *dn;
	isc_result_t result;
	struct ldb_result *res;
	const char *attrs[] = { "dnsRecord", NULL };
	int ret, i;
	struct ldb_message_element *el;
	enum ndr_err_code ndr_err;
	NTTIME t;

	if (state->transaction_token != (void*)version) {
		state->log(ISC_LOG_INFO, "samba_dlz: bad transaction version");
		return ISC_R_FAILURE;
	}

	rec = talloc_zero(state, struct dnsp_DnssrvRpcRecord);
	if (rec == NULL) {
		return ISC_R_NOMEMORY;
	}

	unix_to_nt_time(&t, time(NULL));
	t /= 10*1000*1000; /* convert to seconds (NT time is in 100ns units) */
	t /= 3600;         /* convert to hours */

	rec->rank        = DNS_RANK_ZONE;
	rec->dwSerial    = state->soa_serial;
	rec->dwTimeStamp = (uint32_t)t;

	if (!b9_parse(state, rdatastr, rec)) {
		state->log(ISC_LOG_INFO, "samba_dlz: failed to parse rdataset '%s'", rdatastr);
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	/* find the DN of the record */
	result = b9_find_name_dn(state, name, rec, &dn);
	if (result != ISC_R_SUCCESS) {
		talloc_free(rec);
		return result;
	}

	/* get any existing records */
	ret = ldb_search(state->samdb, rec, &res, dn, LDB_SCOPE_BASE, attrs, "objectClass=dnsNode");
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		result = b9_add_record(state, name, dn, rec);
		talloc_free(rec);
		if (result == ISC_R_SUCCESS) {
			state->log(ISC_LOG_ERROR, "samba_dlz: added %s %s", name, rdatastr);
		}
		return result;
	}

	/* there are existing records. We need to see if this will
	 * replace a record or add to it
	 */
	el = ldb_msg_find_element(res->msgs[0], "dnsRecord");
	if (el == NULL) {
		state->log(ISC_LOG_ERROR, "samba_dlz: no dnsRecord attribute for %s",
			   ldb_dn_get_linearized(dn));
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	for (i=0; i<el->num_values; i++) {
		struct dnsp_DnssrvRpcRecord rec2;

		ndr_err = ndr_pull_struct_blob(&el->values[i], rec, &rec2,
					       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			state->log(ISC_LOG_ERROR, "samba_dlz: failed to parse dnsRecord for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(rec);
			return ISC_R_FAILURE;
		}

		if (b9_record_match(state, rec, &rec2)) {
			break;
		}
	}
	if (i == el->num_values) {
		/* adding a new value */
		el->values = talloc_realloc(el, el->values, struct ldb_val, el->num_values+1);
		if (el->values == NULL) {
			talloc_free(rec);
			return ISC_R_NOMEMORY;
		}
		el->num_values++;
	}

	ndr_err = ndr_push_struct_blob(&el->values[i], rec, rec,
				       (ndr_push_flags_fn_t)ndr_push_dnsp_DnssrvRpcRecord);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		state->log(ISC_LOG_ERROR, "samba_dlz: failed to push dnsRecord for %s",
			   ldb_dn_get_linearized(dn));
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	/* modify the record */
	el->flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(state->samdb, res->msgs[0]);
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "samba_dlz: failed to modify %s - %s",
			   ldb_dn_get_linearized(dn), ldb_errstring(state->samdb));
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	state->log(ISC_LOG_INFO, "samba_dlz: added rdataset %s '%s'", name, rdatastr);

	talloc_free(rec);
	return ISC_R_SUCCESS;
}

/*
  remove a rdataset
 */
_PUBLIC_ isc_result_t dlz_subrdataset(const char *name, const char *rdatastr, void *dbdata, void *version)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	struct dnsp_DnssrvRpcRecord *rec;
	struct ldb_dn *dn;
	isc_result_t result;
	struct ldb_result *res;
	const char *attrs[] = { "dnsRecord", NULL };
	int ret, i;
	struct ldb_message_element *el;
	enum ndr_err_code ndr_err;

	if (state->transaction_token != (void*)version) {
		state->log(ISC_LOG_INFO, "samba_dlz: bad transaction version");
		return ISC_R_FAILURE;
	}

	rec = talloc_zero(state, struct dnsp_DnssrvRpcRecord);
	if (rec == NULL) {
		return ISC_R_NOMEMORY;
	}

	if (!b9_parse(state, rdatastr, rec)) {
		state->log(ISC_LOG_INFO, "samba_dlz: failed to parse rdataset '%s'", rdatastr);
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	/* find the DN of the record */
	result = b9_find_name_dn(state, name, rec, &dn);
	if (result != ISC_R_SUCCESS) {
		talloc_free(rec);
		return result;
	}

	/* get the existing records */
	ret = ldb_search(state->samdb, rec, &res, dn, LDB_SCOPE_BASE, attrs, "objectClass=dnsNode");
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		talloc_free(rec);
		return ISC_R_NOTFOUND;
	}

	/* there are existing records. We need to see if any match
	 */
	el = ldb_msg_find_element(res->msgs[0], "dnsRecord");
	if (el == NULL || el->num_values == 0) {
		state->log(ISC_LOG_ERROR, "samba_dlz: no dnsRecord attribute for %s",
			   ldb_dn_get_linearized(dn));
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	for (i=0; i<el->num_values; i++) {
		struct dnsp_DnssrvRpcRecord rec2;

		ndr_err = ndr_pull_struct_blob(&el->values[i], rec, &rec2,
					       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			state->log(ISC_LOG_ERROR, "samba_dlz: failed to parse dnsRecord for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(rec);
			return ISC_R_FAILURE;
		}

		if (b9_record_match(state, rec, &rec2)) {
			break;
		}
	}
	if (i == el->num_values) {
		talloc_free(rec);
		return ISC_R_NOTFOUND;
	}

	if (i < el->num_values-1) {
		memmove(&el->values[i], &el->values[i+1], sizeof(el->values[0])*((el->num_values-1)-i));
	}
	el->num_values--;

	if (el->num_values == 0) {
		/* delete the record */
		ret = ldb_delete(state->samdb, dn);
	} else {
		/* modify the record */
		el->flags = LDB_FLAG_MOD_REPLACE;
		ret = ldb_modify(state->samdb, res->msgs[0]);
	}
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "samba_dlz: failed to modify %s - %s",
			   ldb_dn_get_linearized(dn), ldb_errstring(state->samdb));
		talloc_free(rec);
		return ISC_R_FAILURE;
	}

	state->log(ISC_LOG_INFO, "samba_dlz: subtracted rdataset %s '%s'", name, rdatastr);

	talloc_free(rec);
	return ISC_R_SUCCESS;
}


/*
  delete all records of the given type
 */
_PUBLIC_ isc_result_t dlz_delrdataset(const char *name, const char *type, void *dbdata, void *version)
{
	struct dlz_bind9_data *state = talloc_get_type_abort(dbdata, struct dlz_bind9_data);
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *dn;
	isc_result_t result;
	struct ldb_result *res;
	const char *attrs[] = { "dnsRecord", NULL };
	int ret, i;
	struct ldb_message_element *el;
	enum ndr_err_code ndr_err;
	enum dns_record_type dns_type;
	bool found = false;

	if (state->transaction_token != (void*)version) {
		state->log(ISC_LOG_INFO, "samba_dlz: bad transaction version");
		return ISC_R_FAILURE;
	}

	if (!b9_dns_type(type, &dns_type)) {
		state->log(ISC_LOG_INFO, "samba_dlz: bad dns type %s in delete", type);
		return ISC_R_FAILURE;
	}

	tmp_ctx = talloc_new(state);

	/* find the DN of the record */
	result = b9_find_name_dn(state, name, tmp_ctx, &dn);
	if (result != ISC_R_SUCCESS) {
		talloc_free(tmp_ctx);
		return result;
	}

	/* get the existing records */
	ret = ldb_search(state->samdb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, attrs, "objectClass=dnsNode");
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		talloc_free(tmp_ctx);
		return ISC_R_NOTFOUND;
	}

	/* there are existing records. We need to see if any match the type
	 */
	el = ldb_msg_find_element(res->msgs[0], "dnsRecord");
	if (el == NULL || el->num_values == 0) {
		talloc_free(tmp_ctx);
		return ISC_R_NOTFOUND;
	}

	for (i=0; i<el->num_values; i++) {
		struct dnsp_DnssrvRpcRecord rec2;

		ndr_err = ndr_pull_struct_blob(&el->values[i], tmp_ctx, &rec2,
					       (ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			state->log(ISC_LOG_ERROR, "samba_dlz: failed to parse dnsRecord for %s",
				   ldb_dn_get_linearized(dn));
			talloc_free(tmp_ctx);
			return ISC_R_FAILURE;
		}

		if (dns_type == rec2.wType) {
			if (i < el->num_values-1) {
				memmove(&el->values[i], &el->values[i+1],
					sizeof(el->values[0])*((el->num_values-1)-i));
			}
			el->num_values--;
			i--;
			found = true;
		}
	}

	if (!found) {
		talloc_free(tmp_ctx);
		return ISC_R_FAILURE;
	}

	if (el->num_values == 0) {
		/* delete the record */
		ret = ldb_delete(state->samdb, dn);
	} else {
		/* modify the record */
		el->flags = LDB_FLAG_MOD_REPLACE;
		ret = ldb_modify(state->samdb, res->msgs[0]);
	}
	if (ret != LDB_SUCCESS) {
		state->log(ISC_LOG_ERROR, "samba_dlz: failed to delete type %s in %s - %s",
			   type, ldb_dn_get_linearized(dn), ldb_errstring(state->samdb));
		talloc_free(tmp_ctx);
		return ISC_R_FAILURE;
	}

	state->log(ISC_LOG_INFO, "samba_dlz: deleted rdataset %s of type %s", name, type);

	talloc_free(tmp_ctx);
	return ISC_R_SUCCESS;
}
