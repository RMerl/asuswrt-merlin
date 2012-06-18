/* 
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007

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
#include "events/events.h"
#include "ldb.h"
#include "ldb_errors.h"
#include "../lib/util/util_ldb.h"
#include "../lib/crypto/crypto.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "../libds/common/flags.h"
#include "dsdb/common/proto.h"
#include "libcli/ldap/ldap_ndr.h"
#include "param/param.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

/*
  search the sam for the specified attributes in a specific domain, filter on
  objectSid being in domain_sid.
*/
int samdb_search_domain(struct ldb_context *sam_ldb,
			TALLOC_CTX *mem_ctx, 
			struct ldb_dn *basedn,
			struct ldb_message ***res,
			const char * const *attrs,
			const struct dom_sid *domain_sid,
			const char *format, ...)  _PRINTF_ATTRIBUTE(7,8)
{
	va_list ap;
	int i, count;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn,
			       res, attrs, format, ap);
	va_end(ap);

	i=0;

	while (i<count) {
		struct dom_sid *entry_sid;

		entry_sid = samdb_result_dom_sid(mem_ctx, (*res)[i], "objectSid");

		if ((entry_sid == NULL) ||
		    (!dom_sid_in_domain(domain_sid, entry_sid))) {
			/* Delete that entry from the result set */
			(*res)[i] = (*res)[count-1];
			count -= 1;
			talloc_free(entry_sid);
			continue;
		}
		talloc_free(entry_sid);
		i += 1;
	}

	return count;
}

/*
  search the sam for a single string attribute in exactly 1 record
*/
const char *samdb_search_string_v(struct ldb_context *sam_ldb,
				  TALLOC_CTX *mem_ctx,
				  struct ldb_dn *basedn,
				  const char *attr_name,
				  const char *format, va_list ap) _PRINTF_ATTRIBUTE(5,0)
{
	int count;
	const char *attrs[2] = { NULL, NULL };
	struct ldb_message **res = NULL;

	attrs[0] = attr_name;

	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	if (count > 1) {		
		DEBUG(1,("samdb: search for %s %s not single valued (count=%d)\n", 
			 attr_name, format, count));
	}
	if (count != 1) {
		talloc_free(res);
		return NULL;
	}

	return samdb_result_string(res[0], attr_name, NULL);
}

/*
  search the sam for a single string attribute in exactly 1 record
*/
const char *samdb_search_string(struct ldb_context *sam_ldb,
				TALLOC_CTX *mem_ctx,
				struct ldb_dn *basedn,
				const char *attr_name,
				const char *format, ...) _PRINTF_ATTRIBUTE(5,6)
{
	va_list ap;
	const char *str;

	va_start(ap, format);
	str = samdb_search_string_v(sam_ldb, mem_ctx, basedn, attr_name, format, ap);
	va_end(ap);

	return str;
}

struct ldb_dn *samdb_search_dn(struct ldb_context *sam_ldb,
			       TALLOC_CTX *mem_ctx,
			       struct ldb_dn *basedn,
			       const char *format, ...) _PRINTF_ATTRIBUTE(4,5)
{
	va_list ap;
	struct ldb_dn *ret;
	struct ldb_message **res = NULL;
	int count;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, NULL, format, ap);
	va_end(ap);

	if (count != 1) return NULL;

	ret = talloc_steal(mem_ctx, res[0]->dn);
	talloc_free(res);

	return ret;
}

/*
  search the sam for a dom_sid attribute in exactly 1 record
*/
struct dom_sid *samdb_search_dom_sid(struct ldb_context *sam_ldb,
				     TALLOC_CTX *mem_ctx,
				     struct ldb_dn *basedn,
				     const char *attr_name,
				     const char *format, ...) _PRINTF_ATTRIBUTE(5,6)
{
	va_list ap;
	int count;
	struct ldb_message **res;
	const char *attrs[2] = { NULL, NULL };
	struct dom_sid *sid;

	attrs[0] = attr_name;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	va_end(ap);
	if (count > 1) {		
		DEBUG(1,("samdb: search for %s %s not single valued (count=%d)\n", 
			 attr_name, format, count));
	}
	if (count != 1) {
		talloc_free(res);
		return NULL;
	}
	sid = samdb_result_dom_sid(mem_ctx, res[0], attr_name);
	talloc_free(res);
	return sid;	
}

/*
  return the count of the number of records in the sam matching the query
*/
int samdb_search_count(struct ldb_context *sam_ldb,
		       TALLOC_CTX *mem_ctx,
		       struct ldb_dn *basedn,
		       const char *format, ...) _PRINTF_ATTRIBUTE(4,5)
{
	va_list ap;
	struct ldb_message **res;
	const char * const attrs[] = { NULL };
	int ret;

	va_start(ap, format);
	ret = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	va_end(ap);

	return ret;
}


/*
  search the sam for a single integer attribute in exactly 1 record
*/
uint_t samdb_search_uint(struct ldb_context *sam_ldb,
			 TALLOC_CTX *mem_ctx,
			 uint_t default_value,
			 struct ldb_dn *basedn,
			 const char *attr_name,
			 const char *format, ...) _PRINTF_ATTRIBUTE(6,7)
{
	va_list ap;
	int count;
	struct ldb_message **res;
	const char *attrs[2] = { NULL, NULL };

	attrs[0] = attr_name;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	va_end(ap);

	if (count != 1) {
		return default_value;
	}

	return samdb_result_uint(res[0], attr_name, default_value);
}

/*
  search the sam for a single signed 64 bit integer attribute in exactly 1 record
*/
int64_t samdb_search_int64(struct ldb_context *sam_ldb,
			   TALLOC_CTX *mem_ctx,
			   int64_t default_value,
			   struct ldb_dn *basedn,
			   const char *attr_name,
			   const char *format, ...) _PRINTF_ATTRIBUTE(6,7)
{
	va_list ap;
	int count;
	struct ldb_message **res;
	const char *attrs[2] = { NULL, NULL };

	attrs[0] = attr_name;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	va_end(ap);

	if (count != 1) {
		return default_value;
	}

	return samdb_result_int64(res[0], attr_name, default_value);
}

/*
  search the sam for multipe records each giving a single string attribute
  return the number of matches, or -1 on error
*/
int samdb_search_string_multiple(struct ldb_context *sam_ldb,
				 TALLOC_CTX *mem_ctx,
				 struct ldb_dn *basedn,
				 const char ***strs,
				 const char *attr_name,
				 const char *format, ...) _PRINTF_ATTRIBUTE(6,7)
{
	va_list ap;
	int count, i;
	const char *attrs[2] = { NULL, NULL };
	struct ldb_message **res = NULL;

	attrs[0] = attr_name;

	va_start(ap, format);
	count = gendb_search_v(sam_ldb, mem_ctx, basedn, &res, attrs, format, ap);
	va_end(ap);

	if (count <= 0) {
		return count;
	}

	/* make sure its single valued */
	for (i=0;i<count;i++) {
		if (res[i]->num_elements != 1) {
			DEBUG(1,("samdb: search for %s %s not single valued\n", 
				 attr_name, format));
			talloc_free(res);
			return -1;
		}
	}

	*strs = talloc_array(mem_ctx, const char *, count+1);
	if (! *strs) {
		talloc_free(res);
		return -1;
	}

	for (i=0;i<count;i++) {
		(*strs)[i] = samdb_result_string(res[i], attr_name, NULL);
	}
	(*strs)[count] = NULL;

	return count;
}

/*
  pull a uint from a result set. 
*/
uint_t samdb_result_uint(const struct ldb_message *msg, const char *attr, uint_t default_value)
{
	return ldb_msg_find_attr_as_uint(msg, attr, default_value);
}

/*
  pull a (signed) int64 from a result set. 
*/
int64_t samdb_result_int64(const struct ldb_message *msg, const char *attr, int64_t default_value)
{
	return ldb_msg_find_attr_as_int64(msg, attr, default_value);
}

/*
  pull a string from a result set. 
*/
const char *samdb_result_string(const struct ldb_message *msg, const char *attr, 
				const char *default_value)
{
	return ldb_msg_find_attr_as_string(msg, attr, default_value);
}

struct ldb_dn *samdb_result_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const struct ldb_message *msg,
			       const char *attr, struct ldb_dn *default_value)
{
	struct ldb_dn *ret_dn = ldb_msg_find_attr_as_dn(ldb, mem_ctx, msg, attr);
	if (!ret_dn) {
		return default_value;
	}
	return ret_dn;
}

/*
  pull a rid from a objectSid in a result set. 
*/
uint32_t samdb_result_rid_from_sid(TALLOC_CTX *mem_ctx, const struct ldb_message *msg, 
				   const char *attr, uint32_t default_value)
{
	struct dom_sid *sid;
	uint32_t rid;

	sid = samdb_result_dom_sid(mem_ctx, msg, attr);
	if (sid == NULL) {
		return default_value;
	}
	rid = sid->sub_auths[sid->num_auths-1];
	talloc_free(sid);
	return rid;
}

/*
  pull a dom_sid structure from a objectSid in a result set. 
*/
struct dom_sid *samdb_result_dom_sid(TALLOC_CTX *mem_ctx, const struct ldb_message *msg, 
				     const char *attr)
{
	const struct ldb_val *v;
	struct dom_sid *sid;
	enum ndr_err_code ndr_err;
	v = ldb_msg_find_ldb_val(msg, attr);
	if (v == NULL) {
		return NULL;
	}
	sid = talloc(mem_ctx, struct dom_sid);
	if (sid == NULL) {
		return NULL;
	}
	ndr_err = ndr_pull_struct_blob(v, sid, NULL, sid,
				       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sid);
		return NULL;
	}
	return sid;
}

/*
  pull a guid structure from a objectGUID in a result set. 
*/
struct GUID samdb_result_guid(const struct ldb_message *msg, const char *attr)
{
	const struct ldb_val *v;
	enum ndr_err_code ndr_err;
	struct GUID guid;
	TALLOC_CTX *mem_ctx;

	ZERO_STRUCT(guid);

	v = ldb_msg_find_ldb_val(msg, attr);
	if (!v) return guid;

	mem_ctx = talloc_named_const(NULL, 0, "samdb_result_guid");
	if (!mem_ctx) return guid;
	ndr_err = ndr_pull_struct_blob(v, mem_ctx, NULL, &guid,
				       (ndr_pull_flags_fn_t)ndr_pull_GUID);
	talloc_free(mem_ctx);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return guid;
	}

	return guid;
}

/*
  pull a sid prefix from a objectSid in a result set. 
  this is used to find the domain sid for a user
*/
struct dom_sid *samdb_result_sid_prefix(TALLOC_CTX *mem_ctx, const struct ldb_message *msg, 
					const char *attr)
{
	struct dom_sid *sid = samdb_result_dom_sid(mem_ctx, msg, attr);
	if (!sid || sid->num_auths < 1) return NULL;
	sid->num_auths--;
	return sid;
}

/*
  pull a NTTIME in a result set. 
*/
NTTIME samdb_result_nttime(struct ldb_message *msg, const char *attr, NTTIME default_value)
{
	return ldb_msg_find_attr_as_uint64(msg, attr, default_value);
}

/*
 * Windows stores 0 for lastLogoff.
 * But when a MS DC return the lastLogoff (as Logoff Time)
 * it returns 0x7FFFFFFFFFFFFFFF, not returning this value in this case
 * cause windows 2008 and newer version to fail for SMB requests
 */
NTTIME samdb_result_last_logoff(struct ldb_message *msg)
{
	NTTIME ret = ldb_msg_find_attr_as_uint64(msg, "lastLogoff",0);

	if (ret == 0)
		ret = 0x7FFFFFFFFFFFFFFFULL;

	return ret;
}

/*
 * Windows uses both 0 and 9223372036854775807 (0x7FFFFFFFFFFFFFFFULL) to
 * indicate an account doesn't expire.
 *
 * When Windows initially creates an account, it sets
 * accountExpires = 9223372036854775807 (0x7FFFFFFFFFFFFFFF).  However,
 * when changing from an account having a specific expiration date to
 * that account never expiring, it sets accountExpires = 0.
 *
 * Consolidate that logic here to allow clearer logic for account expiry in
 * the rest of the code.
 */
NTTIME samdb_result_account_expires(struct ldb_message *msg)
{
	NTTIME ret = ldb_msg_find_attr_as_uint64(msg, "accountExpires",
						 0);

	if (ret == 0)
		ret = 0x7FFFFFFFFFFFFFFFULL;

	return ret;
}

/*
  pull a uint64_t from a result set. 
*/
uint64_t samdb_result_uint64(struct ldb_message *msg, const char *attr, uint64_t default_value)
{
	return ldb_msg_find_attr_as_uint64(msg, attr, default_value);
}


/*
  construct the allow_password_change field from the PwdLastSet attribute and the 
  domain password settings
*/
NTTIME samdb_result_allow_password_change(struct ldb_context *sam_ldb, 
					  TALLOC_CTX *mem_ctx, 
					  struct ldb_dn *domain_dn, 
					  struct ldb_message *msg, 
					  const char *attr)
{
	uint64_t attr_time = samdb_result_uint64(msg, attr, 0);
	int64_t minPwdAge;

	if (attr_time == 0) {
		return 0;
	}

	minPwdAge = samdb_search_int64(sam_ldb, mem_ctx, 0, domain_dn, "minPwdAge", NULL);

	/* yes, this is a -= not a += as minPwdAge is stored as the negative
	   of the number of 100-nano-seconds */
	attr_time -= minPwdAge;

	return attr_time;
}

/*
  construct the force_password_change field from the PwdLastSet
  attribute, the userAccountControl and the domain password settings
*/
NTTIME samdb_result_force_password_change(struct ldb_context *sam_ldb, 
					  TALLOC_CTX *mem_ctx, 
					  struct ldb_dn *domain_dn, 
					  struct ldb_message *msg)
{
	uint64_t attr_time = samdb_result_uint64(msg, "pwdLastSet", 0);
	uint32_t userAccountControl = samdb_result_uint64(msg, "userAccountControl", 0);
	int64_t maxPwdAge;

	/* Machine accounts don't expire, and there is a flag for 'no expiry' */
	if (!(userAccountControl & UF_NORMAL_ACCOUNT)
	    || (userAccountControl & UF_DONT_EXPIRE_PASSWD)) {
		return 0x7FFFFFFFFFFFFFFFULL;
	}

	if (attr_time == 0) {
		return 0;
	}

	maxPwdAge = samdb_search_int64(sam_ldb, mem_ctx, 0, domain_dn, "maxPwdAge", NULL);
	if (maxPwdAge == 0) {
		return 0x7FFFFFFFFFFFFFFFULL;
	} else {
		attr_time -= maxPwdAge;
	}

	return attr_time;
}

/*
  pull a samr_Password structutre from a result set. 
*/
struct samr_Password *samdb_result_hash(TALLOC_CTX *mem_ctx, const struct ldb_message *msg, const char *attr)
{
	struct samr_Password *hash = NULL;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);
	if (val && (val->length >= sizeof(hash->hash))) {
		hash = talloc(mem_ctx, struct samr_Password);
		memcpy(hash->hash, val->data, MIN(val->length, sizeof(hash->hash)));
	}
	return hash;
}

/*
  pull an array of samr_Password structutres from a result set. 
*/
uint_t samdb_result_hashes(TALLOC_CTX *mem_ctx, const struct ldb_message *msg, 
			   const char *attr, struct samr_Password **hashes)
{
	uint_t count = 0;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);
	int i;

	*hashes = NULL;
	if (!val) {
		return 0;
	}
	count = val->length / 16;
	if (count == 0) {
		return 0;
	}

	*hashes = talloc_array(mem_ctx, struct samr_Password, count);
	if (! *hashes) {
		return 0;
	}

	for (i=0;i<count;i++) {
		memcpy((*hashes)[i].hash, (i*16)+(char *)val->data, 16);
	}

	return count;
}

NTSTATUS samdb_result_passwords(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, struct ldb_message *msg, 
				struct samr_Password **lm_pwd, struct samr_Password **nt_pwd) 
{
	struct samr_Password *lmPwdHash, *ntPwdHash;
	if (nt_pwd) {
		int num_nt;
		num_nt = samdb_result_hashes(mem_ctx, msg, "unicodePwd", &ntPwdHash);
		if (num_nt == 0) {
			*nt_pwd = NULL;
		} else if (num_nt > 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else {
			*nt_pwd = &ntPwdHash[0];
		}
	}
	if (lm_pwd) {
		/* Ensure that if we have turned off LM
		 * authentication, that we never use the LM hash, even
		 * if we store it */
		if (lp_lanman_auth(lp_ctx)) {
			int num_lm;
			num_lm = samdb_result_hashes(mem_ctx, msg, "dBCSPwd", &lmPwdHash);
			if (num_lm == 0) {
				*lm_pwd = NULL;
			} else if (num_lm > 1) {
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			} else {
				*lm_pwd = &lmPwdHash[0];
			}
		} else {
			*lm_pwd = NULL;
		}
	}
	return NT_STATUS_OK;
}

/*
  pull a samr_LogonHours structutre from a result set. 
*/
struct samr_LogonHours samdb_result_logon_hours(TALLOC_CTX *mem_ctx, struct ldb_message *msg, const char *attr)
{
	struct samr_LogonHours hours;
	const int units_per_week = 168;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);
	ZERO_STRUCT(hours);
	hours.bits = talloc_array(mem_ctx, uint8_t, units_per_week);
	if (!hours.bits) {
		return hours;
	}
	hours.units_per_week = units_per_week;
	memset(hours.bits, 0xFF, units_per_week);
	if (val) {
		memcpy(hours.bits, val->data, MIN(val->length, units_per_week));
	}
	return hours;
}

/*
  pull a set of account_flags from a result set. 

  This requires that the attributes: 
   pwdLastSet
   userAccountControl
  be included in 'msg'
*/
uint32_t samdb_result_acct_flags(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
				 struct ldb_message *msg, struct ldb_dn *domain_dn)
{
	uint32_t userAccountControl = ldb_msg_find_attr_as_uint(msg, "userAccountControl", 0);
	uint32_t acct_flags = ds_uf2acb(userAccountControl);
	NTTIME must_change_time;
	NTTIME now;

	must_change_time = samdb_result_force_password_change(sam_ctx, mem_ctx, 
							      domain_dn, msg);

	/* Test account expire time */
	unix_to_nt_time(&now, time(NULL));
	/* check for expired password */
	if (must_change_time < now) {
		acct_flags |= ACB_PW_EXPIRED;
	}
	return acct_flags;
}

struct lsa_BinaryString samdb_result_parameters(TALLOC_CTX *mem_ctx,
						struct ldb_message *msg,
						const char *attr)
{
	struct lsa_BinaryString s;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);

	ZERO_STRUCT(s);

	if (!val) {
		return s;
	}

	s.array = talloc_array(mem_ctx, uint16_t, val->length/2);
	if (!s.array) {
		return s;
	}
	s.length = s.size = val->length/2;
	memcpy(s.array, val->data, val->length);

	return s;
}

/* Find an attribute, with a particular value */

/* The current callers of this function expect a very specific
 * behaviour: In particular, objectClass subclass equivilance is not
 * wanted.  This means that we should not lookup the schema for the
 * comparison function */
struct ldb_message_element *samdb_find_attribute(struct ldb_context *ldb, 
						 const struct ldb_message *msg, 
						 const char *name, const char *value)
{
	int i;
	struct ldb_message_element *el = ldb_msg_find_element(msg, name);

	if (!el) {
		return NULL;
	}

	for (i=0;i<el->num_values;i++) {
		if (ldb_attr_cmp(value, (char *)el->values[i].data) == 0) {
			return el;
		}
	}

	return NULL;
}

int samdb_find_or_add_value(struct ldb_context *ldb, struct ldb_message *msg, const char *name, const char *set_value)
{
	if (samdb_find_attribute(ldb, msg, name, set_value) == NULL) {
		return samdb_msg_add_string(ldb, msg, msg, name, set_value);
	}
	return LDB_SUCCESS;
}

int samdb_find_or_add_attribute(struct ldb_context *ldb, struct ldb_message *msg, const char *name, const char *set_value)
{
	struct ldb_message_element *el;

       	el = ldb_msg_find_element(msg, name);
	if (el) {
		return LDB_SUCCESS;
	}

	return samdb_msg_add_string(ldb, msg, msg, name, set_value);
}



/*
  add a string element to a message
*/
int samdb_msg_add_string(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, const char *str)
{
	char *s = talloc_strdup(mem_ctx, str);
	char *a = talloc_strdup(mem_ctx, attr_name);
	if (s == NULL || a == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return ldb_msg_add_string(msg, a, s);
}

/*
  add a dom_sid element to a message
*/
int samdb_msg_add_dom_sid(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, struct dom_sid *sid)
{
	struct ldb_val v;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&v, mem_ctx, 
				       lp_iconv_convenience(ldb_get_opaque(sam_ldb, "loadparm")),
				       sid,
				       (ndr_push_flags_fn_t)ndr_push_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return ldb_msg_add_value(msg, attr_name, &v, NULL);
}


/*
  add a delete element operation to a message
*/
int samdb_msg_add_delete(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name)
{
	/* we use an empty replace rather than a delete, as it allows for 
	   samdb_replace() to be used everywhere */
	return ldb_msg_add_empty(msg, attr_name, LDB_FLAG_MOD_REPLACE, NULL);
}

/*
  add a add attribute value to a message
*/
int samdb_msg_add_addval(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, const char *value)
{
	struct ldb_message_element *el;
	char *a, *v;
	int ret;
	a = talloc_strdup(mem_ctx, attr_name);
	if (a == NULL)
		return -1;
	v = talloc_strdup(mem_ctx, value);
	if (v == NULL)
		return -1;
	ret = ldb_msg_add_string(msg, a, v);
	if (ret != 0)
		return ret;
	el = ldb_msg_find_element(msg, a);
	if (el == NULL)
		return -1;
	el->flags = LDB_FLAG_MOD_ADD;
	return 0;
}

/*
  add a delete attribute value to a message
*/
int samdb_msg_add_delval(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, const char *value)
{
	struct ldb_message_element *el;
	char *a, *v;
	int ret;
	a = talloc_strdup(mem_ctx, attr_name);
	if (a == NULL)
		return -1;
	v = talloc_strdup(mem_ctx, value);
	if (v == NULL)
		return -1;
	ret = ldb_msg_add_string(msg, a, v);
	if (ret != 0)
		return ret;
	el = ldb_msg_find_element(msg, a);
	if (el == NULL)
		return -1;
	el->flags = LDB_FLAG_MOD_DELETE;
	return 0;
}

/*
  add a int element to a message
*/
int samdb_msg_add_int(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, int v)
{
	const char *s = talloc_asprintf(mem_ctx, "%d", v);
	return samdb_msg_add_string(sam_ldb, mem_ctx, msg, attr_name, s);
}

/*
  add a uint_t element to a message
*/
int samdb_msg_add_uint(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, uint_t v)
{
	const char *s = talloc_asprintf(mem_ctx, "%u", v);
	return samdb_msg_add_string(sam_ldb, mem_ctx, msg, attr_name, s);
}

/*
  add a (signed) int64_t element to a message
*/
int samdb_msg_add_int64(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			const char *attr_name, int64_t v)
{
	const char *s = talloc_asprintf(mem_ctx, "%lld", (long long)v);
	return samdb_msg_add_string(sam_ldb, mem_ctx, msg, attr_name, s);
}

/*
  add a uint64_t element to a message
*/
int samdb_msg_add_uint64(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			const char *attr_name, uint64_t v)
{
	const char *s = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)v);
	return samdb_msg_add_string(sam_ldb, mem_ctx, msg, attr_name, s);
}

/*
  add a samr_Password element to a message
*/
int samdb_msg_add_hash(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, struct samr_Password *hash)
{
	struct ldb_val val;
	val.data = talloc_memdup(mem_ctx, hash->hash, 16);
	if (!val.data) {
		return -1;
	}
	val.length = 16;
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/*
  add a samr_Password array to a message
*/
int samdb_msg_add_hashes(TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, struct samr_Password *hashes, uint_t count)
{
	struct ldb_val val;
	int i;
	val.data = talloc_array_size(mem_ctx, 16, count);
	val.length = count*16;
	if (!val.data) {
		return -1;
	}
	for (i=0;i<count;i++) {
		memcpy(i*16 + (char *)val.data, hashes[i].hash, 16);
	}
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/*
  add a acct_flags element to a message
*/
int samdb_msg_add_acct_flags(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			     const char *attr_name, uint32_t v)
{
	return samdb_msg_add_uint(sam_ldb, mem_ctx, msg, attr_name, ds_acb2uf(v));
}

/*
  add a logon_hours element to a message
*/
int samdb_msg_add_logon_hours(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			      const char *attr_name, struct samr_LogonHours *hours)
{
	struct ldb_val val;
	val.length = hours->units_per_week / 8;
	val.data = hours->bits;
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/*
  add a parameters element to a message
*/
int samdb_msg_add_parameters(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			     const char *attr_name, struct lsa_BinaryString *parameters)
{
	struct ldb_val val;
	val.length = parameters->length * 2;
	val.data = (uint8_t *)parameters->array;
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}
/*
  add a general value element to a message
*/
int samdb_msg_add_value(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			      const char *attr_name, const struct ldb_val *val)
{
	return ldb_msg_add_value(msg, attr_name, val, NULL);
}

/*
  sets a general value element to a message
*/
int samdb_msg_set_value(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			const char *attr_name, const struct ldb_val *val)
{
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, attr_name);
	if (el) {
		el->num_values = 0;
	}
	return ldb_msg_add_value(msg, attr_name, val, NULL);
}

/*
  set a string element in a message
*/
int samdb_msg_set_string(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, const char *str)
{
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, attr_name);
	if (el) {
		el->num_values = 0;
	}
	return samdb_msg_add_string(sam_ldb, mem_ctx, msg, attr_name, str);
}

/*
  replace elements in a record
*/
int samdb_replace(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg)
{
	int i;

	/* mark all the message elements as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	/* modify the samdb record */
	return ldb_modify(sam_ldb, msg);
}

/*
  return a default security descriptor
*/
struct security_descriptor *samdb_default_security_descriptor(TALLOC_CTX *mem_ctx)
{
	struct security_descriptor *sd;

	sd = security_descriptor_initialise(mem_ctx);

	return sd;
}

struct ldb_dn *samdb_base_dn(struct ldb_context *sam_ctx) 
{
	return ldb_get_default_basedn(sam_ctx);
}

struct ldb_dn *samdb_config_dn(struct ldb_context *sam_ctx) 
{
	return ldb_get_config_basedn(sam_ctx);
}

struct ldb_dn *samdb_schema_dn(struct ldb_context *sam_ctx) 
{
	return ldb_get_schema_basedn(sam_ctx);
}

struct ldb_dn *samdb_root_dn(struct ldb_context *sam_ctx) 
{
	return ldb_get_root_basedn(sam_ctx);
}

struct ldb_dn *samdb_partitions_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *new_dn;

	new_dn = ldb_dn_copy(mem_ctx, samdb_config_dn(sam_ctx));
	if ( ! ldb_dn_add_child_fmt(new_dn, "CN=Partitions")) {
		talloc_free(new_dn);
		return NULL;
	}
	return new_dn;
}

struct ldb_dn *samdb_sites_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *new_dn;

	new_dn = ldb_dn_copy(mem_ctx, samdb_config_dn(sam_ctx));
	if ( ! ldb_dn_add_child_fmt(new_dn, "CN=Sites")) {
		talloc_free(new_dn);
		return NULL;
	}
	return new_dn;
}

/*
  work out the domain sid for the current open ldb
*/
const struct dom_sid *samdb_domain_sid(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	const struct dom_sid *domain_sid;
	const char *attrs[] = {
		"objectSid",
		NULL
	};
	struct ldb_result *res;
	int ret;

	/* see if we have a cached copy */
	domain_sid = (struct dom_sid *)ldb_get_opaque(ldb, "cache.domain_sid");
	if (domain_sid) {
		return domain_sid;
	}

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, ldb_get_default_basedn(ldb), LDB_SCOPE_BASE, attrs, "objectSid=*");

	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	domain_sid = samdb_result_dom_sid(tmp_ctx, res->msgs[0], "objectSid");
	if (domain_sid == NULL) {
		goto failed;
	}

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.domain_sid", discard_const_p(struct dom_sid, domain_sid)) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, domain_sid);
	talloc_free(tmp_ctx);

	return domain_sid;

failed:
	DEBUG(1,("Failed to find domain_sid for open ldb\n"));
	talloc_free(tmp_ctx);
	return NULL;
}

bool samdb_set_domain_sid(struct ldb_context *ldb, const struct dom_sid *dom_sid_in)
{
	TALLOC_CTX *tmp_ctx;
	struct dom_sid *dom_sid_new;
	struct dom_sid *dom_sid_old;

	/* see if we have a cached copy */
	dom_sid_old = talloc_get_type(ldb_get_opaque(ldb, 
						     "cache.domain_sid"), struct dom_sid);

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	dom_sid_new = dom_sid_dup(tmp_ctx, dom_sid_in);
	if (!dom_sid_new) {
		goto failed;
	}

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.domain_sid", dom_sid_new) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, dom_sid_new);
	talloc_free(tmp_ctx);
	talloc_free(dom_sid_old);

	return true;

failed:
	DEBUG(1,("Failed to set our own cached domain SID in the ldb!\n"));
	talloc_free(tmp_ctx);
	return false;
}

/* Obtain the short name of the flexible single master operator
 * (FSMO), such as the PDC Emulator */
const char *samdb_result_fsmo_name(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const struct ldb_message *msg, 
			     const char *attr)
{
	/* Format is cn=NTDS Settings,cn=<NETBIOS name of FSMO>,.... */
	struct ldb_dn *fsmo_dn = ldb_msg_find_attr_as_dn(ldb, mem_ctx, msg, attr);
	const struct ldb_val *val = ldb_dn_get_component_val(fsmo_dn, 1);
	const char *name = ldb_dn_get_component_name(fsmo_dn, 1);

	if (!name || (ldb_attr_cmp(name, "cn") != 0)) {
		/* Ensure this matches the format.  This gives us a
		 * bit more confidence that a 'cn' value will be a
		 * ascii string */
		return NULL;
	}
	if (val) {
		return (char *)val->data;
	}
	return NULL;
}

/*
  work out the ntds settings dn for the current open ldb
*/
struct ldb_dn *samdb_ntds_settings_dn(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	const char *root_attrs[] = { "dsServiceName", NULL };
	int ret;
	struct ldb_result *root_res;
	struct ldb_dn *settings_dn;

	/* see if we have a cached copy */
	settings_dn = (struct ldb_dn *)ldb_get_opaque(ldb, "cache.settings_dn");
	if (settings_dn) {
		return settings_dn;
	}

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &root_res, ldb_dn_new(tmp_ctx, ldb, ""), LDB_SCOPE_BASE, root_attrs, NULL);
	if (ret) {
		DEBUG(1,("Searching for dsServiceName in rootDSE failed: %s\n", 
			 ldb_errstring(ldb)));
		goto failed;
	}

	if (root_res->count != 1) {
		goto failed;
	}

	settings_dn = ldb_msg_find_attr_as_dn(ldb, tmp_ctx, root_res->msgs[0], "dsServiceName");

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.settings_dn", settings_dn) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, settings_dn);
	talloc_free(tmp_ctx);

	return settings_dn;

failed:
	DEBUG(1,("Failed to find our own NTDS Settings DN in the ldb!\n"));
	talloc_free(tmp_ctx);
	return NULL;
}

/*
  work out the ntds settings invocationId for the current open ldb
*/
const struct GUID *samdb_ntds_invocation_id(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	const char *attrs[] = { "invocationId", NULL };
	int ret;
	struct ldb_result *res;
	struct GUID *invocation_id;

	/* see if we have a cached copy */
	invocation_id = (struct GUID *)ldb_get_opaque(ldb, "cache.invocation_id");
	if (invocation_id) {
		return invocation_id;
	}

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	invocation_id = talloc(tmp_ctx, struct GUID);
	if (!invocation_id) {
		goto failed;
	}

	*invocation_id = samdb_result_guid(res->msgs[0], "invocationId");

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.invocation_id", invocation_id) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, invocation_id);
	talloc_free(tmp_ctx);

	return invocation_id;

failed:
	DEBUG(1,("Failed to find our own NTDS Settings invocationId in the ldb!\n"));
	talloc_free(tmp_ctx);
	return NULL;
}

bool samdb_set_ntds_invocation_id(struct ldb_context *ldb, const struct GUID *invocation_id_in)
{
	TALLOC_CTX *tmp_ctx;
	struct GUID *invocation_id_new;
	struct GUID *invocation_id_old;

	/* see if we have a cached copy */
	invocation_id_old = (struct GUID *)ldb_get_opaque(ldb, 
							 "cache.invocation_id");

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	invocation_id_new = talloc(tmp_ctx, struct GUID);
	if (!invocation_id_new) {
		goto failed;
	}

	*invocation_id_new = *invocation_id_in;

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.invocation_id", invocation_id_new) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, invocation_id_new);
	talloc_free(tmp_ctx);
	talloc_free(invocation_id_old);

	return true;

failed:
	DEBUG(1,("Failed to set our own cached invocationId in the ldb!\n"));
	talloc_free(tmp_ctx);
	return false;
}

/*
  work out the ntds settings objectGUID for the current open ldb
*/
const struct GUID *samdb_ntds_objectGUID(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	const char *attrs[] = { "objectGUID", NULL };
	int ret;
	struct ldb_result *res;
	struct GUID *ntds_guid;

	/* see if we have a cached copy */
	ntds_guid = (struct GUID *)ldb_get_opaque(ldb, "cache.ntds_guid");
	if (ntds_guid) {
		return ntds_guid;
	}

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	ntds_guid = talloc(tmp_ctx, struct GUID);
	if (!ntds_guid) {
		goto failed;
	}

	*ntds_guid = samdb_result_guid(res->msgs[0], "objectGUID");

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.ntds_guid", ntds_guid) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, ntds_guid);
	talloc_free(tmp_ctx);

	return ntds_guid;

failed:
	DEBUG(1,("Failed to find our own NTDS Settings objectGUID in the ldb!\n"));
	talloc_free(tmp_ctx);
	return NULL;
}

bool samdb_set_ntds_objectGUID(struct ldb_context *ldb, const struct GUID *ntds_guid_in)
{
	TALLOC_CTX *tmp_ctx;
	struct GUID *ntds_guid_new;
	struct GUID *ntds_guid_old;

	/* see if we have a cached copy */
	ntds_guid_old = (struct GUID *)ldb_get_opaque(ldb, "cache.ntds_guid");

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ntds_guid_new = talloc(tmp_ctx, struct GUID);
	if (!ntds_guid_new) {
		goto failed;
	}

	*ntds_guid_new = *ntds_guid_in;

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.ntds_guid", ntds_guid_new) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, ntds_guid_new);
	talloc_free(tmp_ctx);
	talloc_free(ntds_guid_old);

	return true;

failed:
	DEBUG(1,("Failed to set our own cached invocationId in the ldb!\n"));
	talloc_free(tmp_ctx);
	return false;
}

/*
  work out the server dn for the current open ldb
*/
struct ldb_dn *samdb_server_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx)
{
	return ldb_dn_get_parent(mem_ctx, samdb_ntds_settings_dn(ldb));
}

/*
  work out the server dn for the current open ldb
*/
struct ldb_dn *samdb_server_site_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *server_dn;
	struct ldb_dn *server_site_dn;

	server_dn = samdb_server_dn(ldb, mem_ctx);
	if (!server_dn) return NULL;

	server_site_dn = ldb_dn_get_parent(mem_ctx, server_dn);

	talloc_free(server_dn);
	return server_site_dn;
}

/*
 * This works out if we are running on a supported forest/domain function
 * level. Basically this means that we don't support mixed/interim (NT 4 DC
 * support) levels.
 * If errmsg isn't NULL we write in an adequate error message for printing out
 * to the screen.
 */
bool samdb_is_capable_dc(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
	char **errmsg)
{
	int32_t level_forest, level_domain, level_domain_mixed;
	bool ret = true;

	level_forest = (int32_t) samdb_search_int64(ldb, mem_ctx, -1,
		samdb_partitions_dn(ldb, mem_ctx), "msDS-Behavior-Version",
		NULL);
	level_domain = (int32_t) samdb_search_int64(ldb, mem_ctx, -1,
		samdb_base_dn(ldb), "msDS-Behavior-Version", NULL);
	level_domain_mixed = (int32_t) samdb_search_int64(ldb, mem_ctx, -1,
		samdb_base_dn(ldb), "nTMixedDomain", NULL);

	if (errmsg != NULL)
		*errmsg = talloc_strdup(mem_ctx, "");

	if (level_forest == -1 || level_domain == -1 || level_domain_mixed == -1) {
		ret = false;
		if (errmsg != NULL)
			*errmsg = talloc_strdup_append(*errmsg,
				"\nATTENTION: Invalid values for forest and/or domain function level!"
			);
	}

	if (level_forest == DS_DOMAIN_FUNCTION_2003_MIXED) {
		ret = false;
		if (errmsg != NULL)
			*errmsg = talloc_strdup_append(*errmsg,
				"\nATTENTION: You run SAMBA 4 on the 2003 with mixed domains (NT4 DC support) forest level. This isn't supported!"
			);
	}
	if ((level_domain == DS_DOMAIN_FUNCTION_2000 && level_domain_mixed != 0)
		|| level_domain == DS_DOMAIN_FUNCTION_2003_MIXED) {
		ret = false;
		if (errmsg != NULL)
			*errmsg = talloc_strdup_append(*errmsg,
				"\nATTENTION: You run SAMBA 4 on a mixed/interim (NT4 DC support) domain level. This isn't supported!"
			);
	}

	if ((!ret) && (errmsg != NULL)) {
		*errmsg = talloc_strdup_append(*errmsg,
			"\nPlease raise the domain and/or forest level to an adequate value. Use for this the 'domainlevel' tool, the MS AD MMC tools or manipulate the needed attributes directly."
		);
	}

	return ret;
}

/*
  work out if we are the PDC for the domain of the current open ldb
*/
bool samdb_is_pdc(struct ldb_context *ldb)
{
	const char *dom_attrs[] = { "fSMORoleOwner", NULL };
	int ret;
	struct ldb_result *dom_res;
	TALLOC_CTX *tmp_ctx;
	bool is_pdc;
	struct ldb_dn *pdc;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		DEBUG(1, ("talloc_new failed in samdb_is_pdc"));
		return false;
	}

	ret = ldb_search(ldb, tmp_ctx, &dom_res, ldb_get_default_basedn(ldb), LDB_SCOPE_BASE, dom_attrs, NULL);
	if (ret) {
		DEBUG(1,("Searching for fSMORoleOwner in %s failed: %s\n", 
			 ldb_dn_get_linearized(ldb_get_default_basedn(ldb)), 
			 ldb_errstring(ldb)));
		goto failed;
	}
	if (dom_res->count != 1) {
		goto failed;
	}

	pdc = ldb_msg_find_attr_as_dn(ldb, tmp_ctx, dom_res->msgs[0], "fSMORoleOwner");

	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), pdc) == 0) {
		is_pdc = true;
	} else {
		is_pdc = false;
	}

	talloc_free(tmp_ctx);

	return is_pdc;

failed:
	DEBUG(1,("Failed to find if we are the PDC for this ldb\n"));
	talloc_free(tmp_ctx);
	return false;
}

/*
  work out if we are a Global Catalog server for the domain of the current open ldb
*/
bool samdb_is_gc(struct ldb_context *ldb)
{
	const char *attrs[] = { "options", NULL };
	int ret, options;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		DEBUG(1, ("talloc_new failed in samdb_is_pdc"));
		return false;
	}

	/* Query cn=ntds settings,.... */
	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret) {
		talloc_free(tmp_ctx);
		return false;
	}
	if (res->count != 1) {
		talloc_free(tmp_ctx);
		return false;
	}

	options = ldb_msg_find_attr_as_int(res->msgs[0], "options", 0);
	talloc_free(tmp_ctx);

	/* if options attribute has the 0x00000001 flag set, then enable the global catlog */
	if (options & 0x000000001) {
		return true;
	}
	return false;
}

/* Find a domain object in the parents of a particular DN.  */
int samdb_search_for_parent_domain(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_dn *dn,
				   struct ldb_dn **parent_dn, const char **errstring)
{
	TALLOC_CTX *local_ctx;
	struct ldb_dn *sdn = dn;
	struct ldb_result *res = NULL;
	int ret = 0;
	const char *attrs[] = { NULL };

	local_ctx = talloc_new(mem_ctx);
	if (local_ctx == NULL) return LDB_ERR_OPERATIONS_ERROR;

	while ((sdn = ldb_dn_get_parent(local_ctx, sdn))) {
		ret = ldb_search(ldb, local_ctx, &res, sdn, LDB_SCOPE_BASE, attrs,
				 "(|(|(objectClass=domain)(objectClass=builtinDomain))(objectClass=samba4LocalDomain))");
		if (ret == LDB_SUCCESS) {
			if (res->count == 1) {
				break;
			}
		} else {
			break;
		}
	}

	if (ret != LDB_SUCCESS) {
		*errstring = talloc_asprintf(mem_ctx, "Error searching for parent domain of %s, failed searching for %s: %s",
					     ldb_dn_get_linearized(dn),
					     ldb_dn_get_linearized(sdn),
					     ldb_errstring(ldb));
		talloc_free(local_ctx);
		return ret;
	}
	if (res->count != 1) {
		*errstring = talloc_asprintf(mem_ctx, "Invalid dn (%s), not child of a domain object",
					     ldb_dn_get_linearized(dn));
		DEBUG(0,(__location__ ": %s\n", *errstring));
		talloc_free(local_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	*parent_dn = talloc_steal(mem_ctx, res->msgs[0]->dn);
	talloc_free(local_ctx);
	return ret;
}


/*
  set the user password using plaintext, obeying any user or domain
  password restrictions

  note that this function doesn't actually store the result in the
  database, it just fills in the "mod" structure with ldb modify
  elements to setup the correct change when samdb_replace() is
  called. This allows the caller to combine the change with other
  changes (as is needed by some of the set user info levels)

  The caller should probably have a transaction wrapping this
*/
NTSTATUS samdb_set_password(struct ldb_context *ctx, TALLOC_CTX *mem_ctx,
			    struct ldb_dn *user_dn,
			    struct ldb_dn *domain_dn,
			    struct ldb_message *mod,
			    const DATA_BLOB *new_password,
			    struct samr_Password *param_lmNewHash,
			    struct samr_Password *param_ntNewHash,
			    bool user_change,
			    enum samr_RejectReason *reject_reason,
			    struct samr_DomInfo1 **_dominfo)
{
	const char * const user_attrs[] = { "userAccountControl", "lmPwdHistory", 
					    "ntPwdHistory", 
					    "dBCSPwd", "unicodePwd", 
					    "objectSid", 
					    "pwdLastSet", NULL };
	const char * const domain_attrs[] = { "pwdProperties", "pwdHistoryLength", 
					      "maxPwdAge", "minPwdAge", 
					      "minPwdLength", NULL };
	NTTIME pwdLastSet;
	int64_t minPwdAge;
	uint_t minPwdLength, pwdProperties, pwdHistoryLength;
	uint_t userAccountControl;
	struct samr_Password *sambaLMPwdHistory, *sambaNTPwdHistory,
		*lmPwdHash, *ntPwdHash, *lmNewHash, *ntNewHash;
	struct samr_Password local_lmNewHash, local_ntNewHash;
	int sambaLMPwdHistory_len, sambaNTPwdHistory_len;
	struct dom_sid *domain_sid;
	struct ldb_message **res;
	bool restrictions;
	int count;
	time_t now = time(NULL);
	NTTIME now_nt;
	int i;

	/* we need to know the time to compute password age */
	unix_to_nt_time(&now_nt, now);

	/* pull all the user parameters */
	count = gendb_search_dn(ctx, mem_ctx, user_dn, &res, user_attrs);
	if (count != 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	userAccountControl = samdb_result_uint(res[0],   "userAccountControl", 0);
	sambaLMPwdHistory_len =   samdb_result_hashes(mem_ctx, res[0], 
						 "lmPwdHistory", &sambaLMPwdHistory);
	sambaNTPwdHistory_len =   samdb_result_hashes(mem_ctx, res[0], 
						 "ntPwdHistory", &sambaNTPwdHistory);
	lmPwdHash =          samdb_result_hash(mem_ctx, res[0],   "dBCSPwd");
	ntPwdHash =          samdb_result_hash(mem_ctx, res[0],   "unicodePwd");
	pwdLastSet =         samdb_result_uint64(res[0], "pwdLastSet", 0);

	/* Copy parameters */
	lmNewHash = param_lmNewHash;
	ntNewHash = param_ntNewHash;

	/* Only non-trust accounts have restrictions (possibly this
	 * test is the wrong way around, but I like to be restrictive
	 * if possible */
	restrictions = !(userAccountControl & (UF_INTERDOMAIN_TRUST_ACCOUNT
					       |UF_WORKSTATION_TRUST_ACCOUNT
					       |UF_SERVER_TRUST_ACCOUNT)); 

	if (domain_dn) {
		/* pull the domain parameters */
		count = gendb_search_dn(ctx, mem_ctx, domain_dn, &res, domain_attrs);
		if (count != 1) {
			DEBUG(2, ("samdb_set_password: Domain DN %s is invalid, for user %s\n", 
				  ldb_dn_get_linearized(domain_dn),
				  ldb_dn_get_linearized(user_dn)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
	} else {
		/* work out the domain sid, and pull the domain from there */
		domain_sid =         samdb_result_sid_prefix(mem_ctx, res[0], "objectSid");
		if (domain_sid == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		count = gendb_search(ctx, mem_ctx, NULL, &res, domain_attrs, 
				     "(objectSid=%s)", 
				     ldap_encode_ndr_dom_sid(mem_ctx, domain_sid));
		if (count != 1) {
			DEBUG(2, ("samdb_set_password: Could not find domain to match SID: %s, for user %s\n", 
				  dom_sid_string(mem_ctx, domain_sid),
				  ldb_dn_get_linearized(user_dn)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
	}

	pwdProperties =    samdb_result_uint(res[0],   "pwdProperties", 0);
	pwdHistoryLength = samdb_result_uint(res[0],   "pwdHistoryLength", 0);
	minPwdLength =     samdb_result_uint(res[0],   "minPwdLength", 0);
	minPwdAge =        samdb_result_int64(res[0],  "minPwdAge", 0);

	if (userAccountControl & UF_PASSWD_NOTREQD) {
		/* see [MS-ADTS] 2.2.15 */
		minPwdLength = 0;
	}

	if (_dominfo) {
		struct samr_DomInfo1 *dominfo;
		/* on failure we need to fill in the reject reasons */
		dominfo = talloc(mem_ctx, struct samr_DomInfo1);
		if (dominfo == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		dominfo->min_password_length     = minPwdLength;
		dominfo->password_properties     = pwdProperties;
		dominfo->password_history_length = pwdHistoryLength;
		dominfo->max_password_age        = minPwdAge;
		dominfo->min_password_age        = minPwdAge;
		*_dominfo = dominfo;
	}

	if (restrictions && new_password) {
		char *new_pass;

		/* check the various password restrictions */
		if (restrictions && minPwdLength > utf16_len_n(new_password->data, new_password->length) / 2) {
			if (reject_reason) {
				*reject_reason = SAMR_REJECT_TOO_SHORT;
			}
			return NT_STATUS_PASSWORD_RESTRICTION;
		}

		/* Create the NT hash */
		mdfour(local_ntNewHash.hash, new_password->data, new_password->length);

		ntNewHash = &local_ntNewHash;

		/* Only check complexity if we can convert it at all.  Assuming unconvertable passwords are 'strong' */
		if (convert_string_talloc_convenience(mem_ctx,
			  lp_iconv_convenience(ldb_get_opaque(ctx, "loadparm")),
			  CH_UTF16, CH_UNIX,
			  new_password->data, new_password->length,
			  (void **)&new_pass, NULL, false)) {

			/* possibly check password complexity */
			if (restrictions && (pwdProperties & DOMAIN_PASSWORD_COMPLEX) &&
			    !check_password_quality(new_pass)) {
				if (reject_reason) {
					*reject_reason = SAMR_REJECT_COMPLEXITY;
				}
				return NT_STATUS_PASSWORD_RESTRICTION;
			}

			/* compute the new lm hashes (for checking history - case insenitivly!) */
			if (E_deshash(new_pass, local_lmNewHash.hash)) {
				lmNewHash = &local_lmNewHash;
			}
		}
	}

	if (restrictions && user_change) {
		/* are all password changes disallowed? */
		if (pwdProperties & DOMAIN_REFUSE_PASSWORD_CHANGE) {
			if (reject_reason) {
				*reject_reason = SAMR_REJECT_OTHER;
			}
			return NT_STATUS_PASSWORD_RESTRICTION;
		}

		/* can this user change password? */
		if (userAccountControl & UF_PASSWD_CANT_CHANGE) {
			if (reject_reason) {
				*reject_reason = SAMR_REJECT_OTHER;
			}
			return NT_STATUS_PASSWORD_RESTRICTION;
		}

		/* yes, this is a minus. The ages are in negative 100nsec units! */
		if (pwdLastSet - minPwdAge > now_nt) {
			if (reject_reason) {
				*reject_reason = SAMR_REJECT_OTHER;
			}
			return NT_STATUS_PASSWORD_RESTRICTION;
		}

		/* check the immediately past password */
		if (pwdHistoryLength > 0) {
			if (lmNewHash && lmPwdHash && memcmp(lmNewHash->hash, lmPwdHash->hash, 16) == 0) {
				if (reject_reason) {
					*reject_reason = SAMR_REJECT_IN_HISTORY;
				}
				return NT_STATUS_PASSWORD_RESTRICTION;
			}
			if (ntNewHash && ntPwdHash && memcmp(ntNewHash->hash, ntPwdHash->hash, 16) == 0) {
				if (reject_reason) {
					*reject_reason = SAMR_REJECT_IN_HISTORY;
				}
				return NT_STATUS_PASSWORD_RESTRICTION;
			}
		}

		/* check the password history */
		sambaLMPwdHistory_len = MIN(sambaLMPwdHistory_len, pwdHistoryLength);
		sambaNTPwdHistory_len = MIN(sambaNTPwdHistory_len, pwdHistoryLength);

		for (i=0; lmNewHash && i<sambaLMPwdHistory_len;i++) {
			if (memcmp(lmNewHash->hash, sambaLMPwdHistory[i].hash, 16) == 0) {
				if (reject_reason) {
					*reject_reason = SAMR_REJECT_IN_HISTORY;
				}
				return NT_STATUS_PASSWORD_RESTRICTION;
			}
		}
		for (i=0; ntNewHash && i<sambaNTPwdHistory_len;i++) {
			if (memcmp(ntNewHash->hash, sambaNTPwdHistory[i].hash, 16) == 0) {
				if (reject_reason) {
					*reject_reason = SAMR_REJECT_IN_HISTORY;
				}
				return NT_STATUS_PASSWORD_RESTRICTION;
			}
		}
	}

#define CHECK_RET(x) do { if (x != 0) return NT_STATUS_NO_MEMORY; } while(0)

	/* the password is acceptable. Start forming the new fields */
	if (new_password) {
		/* if we know the cleartext UTF16 password, then set it.
		 * Modules in ldb will set all the appropriate
		 * hashes */
		CHECK_RET(ldb_msg_add_value(mod, "clearTextPassword", new_password, NULL));
	} else {
		/* We don't have the cleartext, so delete the old one
		 * and set what we have of the hashes */
		CHECK_RET(samdb_msg_add_delete(ctx, mem_ctx, mod, "clearTextPassword"));

		if (lmNewHash) {
			CHECK_RET(samdb_msg_add_hash(ctx, mem_ctx, mod, "dBCSPwd", lmNewHash));
		} else {
			CHECK_RET(samdb_msg_add_delete(ctx, mem_ctx, mod, "dBCSPwd"));
		}

		if (ntNewHash) {
			CHECK_RET(samdb_msg_add_hash(ctx, mem_ctx, mod, "unicodePwd", ntNewHash));
		} else {
			CHECK_RET(samdb_msg_add_delete(ctx, mem_ctx, mod, "unicodePwd"));
		}
	}

	return NT_STATUS_OK;
}


/*
  set the user password using plaintext, obeying any user or domain
  password restrictions

  This wrapper function takes a SID as input, rather than a user DN,
  and actually performs the password change

*/
NTSTATUS samdb_set_password_sid(struct ldb_context *ctx, TALLOC_CTX *mem_ctx,
				const struct dom_sid *user_sid,
				const DATA_BLOB *new_pass,
				struct samr_Password *lmNewHash, 
				struct samr_Password *ntNewHash,
				bool user_change,
				enum samr_RejectReason *reject_reason,
				struct samr_DomInfo1 **_dominfo) 
{
	NTSTATUS nt_status;
	struct ldb_dn *user_dn;
	struct ldb_message *msg;
	int ret;

	ret = ldb_transaction_start(ctx);
	if (ret) {
		DEBUG(1, ("Failed to start transaction: %s\n", ldb_errstring(ctx)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	user_dn = samdb_search_dn(ctx, mem_ctx, NULL, 
				  "(&(objectSid=%s)(objectClass=user))", 
				  ldap_encode_ndr_dom_sid(mem_ctx, user_sid));
	if (!user_dn) {
		ldb_transaction_cancel(ctx);
		DEBUG(3, ("samdb_set_password_sid: SID %s not found in samdb, returning NO_SUCH_USER\n",
			  dom_sid_string(mem_ctx, user_sid)));
		return NT_STATUS_NO_SUCH_USER;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		ldb_transaction_cancel(ctx);
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = ldb_dn_copy(msg, user_dn);
	if (!msg->dn) {
		ldb_transaction_cancel(ctx);
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = samdb_set_password(ctx, mem_ctx,
				       user_dn, NULL,
				       msg, new_pass, 
				       lmNewHash, ntNewHash,
				       user_change, /* This is a password set, not change */
				       reject_reason, _dominfo);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ldb_transaction_cancel(ctx);
		return nt_status;
	}

	/* modify the samdb record */
	ret = samdb_replace(ctx, mem_ctx, msg);
	if (ret != 0) {
		ldb_transaction_cancel(ctx);
		return NT_STATUS_ACCESS_DENIED;
	}

	ret = ldb_transaction_commit(ctx);
	if (ret != 0) {
		DEBUG(0,("Failed to commit transaction to change password on %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ctx)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}
	return NT_STATUS_OK;
}



NTSTATUS samdb_create_foreign_security_principal(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
						 struct dom_sid *sid, struct ldb_dn **ret_dn) 
{
	struct ldb_message *msg;
	struct ldb_dn *basedn;
	const char *sidstr;
	int ret;

	sidstr = dom_sid_string(mem_ctx, sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	/* We might have to create a ForeignSecurityPrincipal, even if this user
	 * is in our own domain */

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* TODO: Hmmm. This feels wrong. How do I find the base dn to
	 * put the ForeignSecurityPrincipals? d_state->domain_dn does
	 * not work, this is wrong for the Builtin domain, there's no
	 * cn=For...,cn=Builtin,dc={BASEDN}.  -- vl
	 */

	basedn = samdb_search_dn(sam_ctx, mem_ctx, NULL,
				 "(&(objectClass=container)(cn=ForeignSecurityPrincipals))");

	if (basedn == NULL) {
		DEBUG(0, ("Failed to find DN for "
			  "ForeignSecurityPrincipal container\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* add core elements to the ldb_message for the alias */
	msg->dn = ldb_dn_copy(mem_ctx, basedn);
	if ( ! ldb_dn_add_child_fmt(msg->dn, "CN=%s", sidstr))
		return NT_STATUS_NO_MEMORY;

	samdb_msg_add_string(sam_ctx, mem_ctx, msg,
			     "objectClass",
			     "foreignSecurityPrincipal");

	/* create the alias */
	ret = ldb_add(sam_ctx, msg);
	if (ret != 0) {
		DEBUG(0,("Failed to create foreignSecurityPrincipal "
			 "record %s: %s\n", 
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ctx)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	*ret_dn = msg->dn;
	return NT_STATUS_OK;
}


/*
  Find the DN of a domain, assuming it to be a dotted.dns name
*/

struct ldb_dn *samdb_dns_domain_to_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *dns_domain) 
{
	int i;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	const char *binary_encoded;
	const char **split_realm;
	struct ldb_dn *dn;

	if (!tmp_ctx) {
		return NULL;
	}

	split_realm = (const char **)str_list_make(tmp_ctx, dns_domain, ".");
	if (!split_realm) {
		talloc_free(tmp_ctx);
		return NULL;
	}
	dn = ldb_dn_new(mem_ctx, ldb, NULL);
	for (i=0; split_realm[i]; i++) {
		binary_encoded = ldb_binary_encode_string(tmp_ctx, split_realm[i]);
		if (!ldb_dn_add_base_fmt(dn, "dc=%s", binary_encoded)) {
			DEBUG(2, ("Failed to add dc=%s element to DN %s\n",
				  binary_encoded, ldb_dn_get_linearized(dn)));
			talloc_free(tmp_ctx);
			return NULL;
		}
	}
	if (!ldb_dn_validate(dn)) {
		DEBUG(2, ("Failed to validated DN %s\n",
			  ldb_dn_get_linearized(dn)));
		return NULL;
	}
	return dn;
}
/*
  Find the DN of a domain, be it the netbios or DNS name 
*/

struct ldb_dn *samdb_domain_to_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, 
				  const char *domain_name) 
{
	const char * const domain_ref_attrs[] = {
		"ncName", NULL
	};
	const char * const domain_ref2_attrs[] = {
		NULL
	};
	struct ldb_result *res_domain_ref;
	char *escaped_domain = ldb_binary_encode_string(mem_ctx, domain_name);
	/* find the domain's DN */
	int ret_domain = ldb_search(ldb, mem_ctx,
					    &res_domain_ref, 
					    samdb_partitions_dn(ldb, mem_ctx), 
					    LDB_SCOPE_ONELEVEL, 
					    domain_ref_attrs,
					    "(&(nETBIOSName=%s)(objectclass=crossRef))", 
					    escaped_domain);
	if (ret_domain != 0) {
		return NULL;
	}

	if (res_domain_ref->count == 0) {
		ret_domain = ldb_search(ldb, mem_ctx,
						&res_domain_ref, 
						samdb_dns_domain_to_dn(ldb, mem_ctx, domain_name),
						LDB_SCOPE_BASE,
						domain_ref2_attrs,
						"(objectclass=domain)");
		if (ret_domain != 0) {
			return NULL;
		}

		if (res_domain_ref->count == 1) {
			return res_domain_ref->msgs[0]->dn;
		}
		return NULL;
	}

	if (res_domain_ref->count > 1) {
		DEBUG(0,("Found %d records matching domain [%s]\n", 
			 ret_domain, domain_name));
		return NULL;
	}

	return samdb_result_dn(ldb, mem_ctx, res_domain_ref->msgs[0], "nCName", NULL);

}


/*
  use a GUID to find a DN
 */
int dsdb_find_dn_by_guid(struct ldb_context *ldb, 
			 TALLOC_CTX *mem_ctx,
			 const char *guid_str, struct ldb_dn **dn)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { NULL };
	struct ldb_request *search_req;
	char *expression;
	struct ldb_search_options_control *options;

	expression = talloc_asprintf(mem_ctx, "objectGUID=%s", guid_str);
	if (!expression) {
		DEBUG(0, (__location__ ": out of memory\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	res = talloc_zero(mem_ctx, struct ldb_result);
	if (!res) {
		DEBUG(0, (__location__ ": out of memory\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&search_req, ldb, mem_ctx,
				   ldb_get_default_basedn(ldb),
				   LDB_SCOPE_SUBTREE,
				   expression, attrs,
				   NULL,
				   res, ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* we need to cope with cross-partition links, so search for
	   the GUID over all partitions */
	options = talloc(search_req, struct ldb_search_options_control);
	if (options == NULL) {
		DEBUG(0, (__location__ ": out of memory\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	options->search_options = LDB_SEARCH_OPTION_PHANTOM_ROOT;

	ret = ldb_request_add_control(search_req, LDB_CONTROL_EXTENDED_DN_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request_add_control(search_req,
				      LDB_CONTROL_SEARCH_OPTIONS_OID,
				      true, options);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, search_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_wait(search_req->handle, LDB_WAIT_ALL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* this really should be exactly 1, but there is a bug in the
	   partitions module that can return two here with the
	   search_options control set */
	if (res->count < 1) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	*dn = res->msgs[0]->dn;

	return LDB_SUCCESS;
}

/*
  search for attrs on one DN, allowing for deleted objects
 */
int dsdb_search_dn_with_deleted(struct ldb_context *ldb,
				TALLOC_CTX *mem_ctx,
				struct ldb_result **_res,
				struct ldb_dn *basedn,
				const char * const *attrs)
{
	int ret;
	struct ldb_request *req;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;

	tmp_ctx = talloc_new(mem_ctx);

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&req, ldb, tmp_ctx,
				   basedn,
				   LDB_SCOPE_BASE,
				   NULL,
				   attrs,
				   NULL,
				   res,
				   ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = ldb_request_add_control(req, LDB_CONTROL_SHOW_DELETED_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);
	*_res = talloc_steal(mem_ctx, res);
	return ret;
}


/*
  use a DN to find a GUID
 */
int dsdb_find_guid_by_dn(struct ldb_context *ldb, 
			 struct ldb_dn *dn, struct GUID *guid)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { "objectGUID", NULL };
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);

	ret = dsdb_search_dn_with_deleted(ldb, tmp_ctx, &res, dn, attrs);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count < 1) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	*guid = samdb_result_guid(res->msgs[0], "objectGUID");
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

/*
  use a DN to find a SID
 */
int dsdb_find_sid_by_dn(struct ldb_context *ldb, 
			struct ldb_dn *dn, struct dom_sid *sid)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { "objectSID", NULL };
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	struct dom_sid *s;

	ZERO_STRUCTP(sid);

	ret = dsdb_search_dn_with_deleted(ldb, tmp_ctx, &res, dn, attrs);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count < 1) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	s = samdb_result_dom_sid(tmp_ctx, res->msgs[0], "objectSID");
	if (s == NULL) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	*sid = *s;
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}



/*
  load a repsFromTo blob list for a given partition GUID
  attr must be "repsFrom" or "repsTo"
 */
WERROR dsdb_loadreps(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, struct ldb_dn *dn,
		     const char *attr, struct repsFromToBlob **r, uint32_t *count)
{
	const char *attrs[] = { attr, NULL };
	struct ldb_result *res = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	int i;
	struct ldb_message_element *el;

	*r = NULL;
	*count = 0;

	if (ldb_search(sam_ctx, tmp_ctx, &res, dn, LDB_SCOPE_BASE, attrs, NULL) != LDB_SUCCESS ||
	    res->count < 1) {
		DEBUG(0,("dsdb_loadreps: failed to read partition object\n"));
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	el = ldb_msg_find_element(res->msgs[0], attr);
	if (el == NULL) {
		/* it's OK to be empty */
		talloc_free(tmp_ctx);
		return WERR_OK;
	}

	*count = el->num_values;
	*r = talloc_array(mem_ctx, struct repsFromToBlob, *count);
	if (*r == NULL) {
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	for (i=0; i<(*count); i++) {
		enum ndr_err_code ndr_err;
		ndr_err = ndr_pull_struct_blob(&el->values[i], 
					       mem_ctx, lp_iconv_convenience(ldb_get_opaque(sam_ctx, "loadparm")),
					       &(*r)[i], 
					       (ndr_pull_flags_fn_t)ndr_pull_repsFromToBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
	}

	talloc_free(tmp_ctx);
	
	return WERR_OK;
}

/*
  save the repsFromTo blob list for a given partition GUID
  attr must be "repsFrom" or "repsTo"
 */
WERROR dsdb_savereps(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, struct ldb_dn *dn,
		     const char *attr, struct repsFromToBlob *r, uint32_t count)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct ldb_message *msg;
	struct ldb_message_element *el;
	int i;

	msg = ldb_msg_new(tmp_ctx);
	msg->dn = dn;
	if (ldb_msg_add_empty(msg, attr, LDB_FLAG_MOD_REPLACE, &el) != LDB_SUCCESS) {
		goto failed;
	}

	el->values = talloc_array(msg, struct ldb_val, count);
	if (!el->values) {
		goto failed;
	}

	for (i=0; i<count; i++) {
		struct ldb_val v;
		enum ndr_err_code ndr_err;

		ndr_err = ndr_push_struct_blob(&v, tmp_ctx, lp_iconv_convenience(ldb_get_opaque(sam_ctx, "loadparm")),
					       &r[i], 
					       (ndr_push_flags_fn_t)ndr_push_repsFromToBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			goto failed;
		}

		el->num_values++;
		el->values[i] = v;
	}

	if (ldb_modify(sam_ctx, msg) != LDB_SUCCESS) {
		DEBUG(0,("Failed to store %s - %s\n", attr, ldb_errstring(sam_ctx)));
		goto failed;
	}

	talloc_free(tmp_ctx);
	
	return WERR_OK;

failed:
	talloc_free(tmp_ctx);
	return WERR_DS_DRA_INTERNAL_ERROR;
}


/*
  load the uSNHighest attribute from the @REPLCHANGED object for a
  partition
 */
int dsdb_load_partition_usn(struct ldb_context *ldb, struct ldb_dn *dn, uint64_t *uSN)
{
	struct ldb_request *req;
	int ret;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	struct dsdb_control_current_partition *p_ctrl;
	struct ldb_result *res;

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&req, ldb, tmp_ctx,
				   ldb_dn_new(tmp_ctx, ldb, "@REPLCHANGED"),
				   LDB_SCOPE_BASE,
				   NULL, NULL,
				   NULL,
				   res, ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	p_ctrl = talloc(req, struct dsdb_control_current_partition);
	if (p_ctrl == NULL) {
		talloc_free(res);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	p_ctrl->version = DSDB_CONTROL_CURRENT_PARTITION_VERSION;
	p_ctrl->dn = dn;
	

	ret = ldb_request_add_control(req,
				      DSDB_CONTROL_CURRENT_PARTITION_OID,
				      false, p_ctrl);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	
	/* Run the new request */
	ret = ldb_request(ldb, req);
	
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		/* it hasn't been created yet, which means
		   an implicit value of zero */
		*uSN = 0;
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (res->count < 1) {
		*uSN = 0;
	} else {
		*uSN = ldb_msg_find_attr_as_uint64(res->msgs[0], "uSNHighest", 0);
	}

	talloc_free(tmp_ctx);

	return LDB_SUCCESS;	
}

/*
  save the uSNHighest attribute in the @REPLCHANGED object for a
  partition
 */
int dsdb_save_partition_usn(struct ldb_context *ldb, struct ldb_dn *dn, uint64_t uSN)
{
	struct ldb_request *req;
	struct ldb_message *msg;
	struct dsdb_control_current_partition *p_ctrl;
	int ret;

	msg = ldb_msg_new(ldb);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = ldb_dn_new(msg, ldb, "@REPLCHANGED");
	if (msg->dn == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	ret = ldb_msg_add_fmt(msg, "uSNHighest", "%llu", (unsigned long long)uSN);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return ret;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	

	p_ctrl = talloc(msg, struct dsdb_control_current_partition);
	if (p_ctrl == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	p_ctrl->version = DSDB_CONTROL_CURRENT_PARTITION_VERSION;
	p_ctrl->dn = dn;

	ret = ldb_build_mod_req(&req, ldb, msg,
				msg,
				NULL,
				NULL, ldb_op_default_callback,
				NULL);
again:
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return ret;
	}
	
	ret = ldb_request_add_control(req,
				      DSDB_CONTROL_CURRENT_PARTITION_OID,
				      false, p_ctrl);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return ret;
	}
	
	/* Run the new request */
	ret = ldb_request(ldb, req);
	
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = ldb_build_add_req(&req, ldb, msg,
					msg,
					NULL,
					NULL, ldb_op_default_callback,
					NULL);
		goto again;
	}
	
	talloc_free(msg);
	
	return ret;
}

int drsuapi_DsReplicaCursor2_compare(const struct drsuapi_DsReplicaCursor2 *c1,
						   const struct drsuapi_DsReplicaCursor2 *c2)
{
	return GUID_compare(&c1->source_dsa_invocation_id, &c2->source_dsa_invocation_id);
}
