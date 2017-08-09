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
#include "ldb_module.h"
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
#include "system/locale.h"
#include "lib/util/tsort.h"
#include "dsdb/common/util.h"
#include "lib/socket/socket.h"
#include "librpc/gen_ndr/irpc.h"
#include "libds/common/flag_mapping.h"

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

	return ldb_msg_find_attr_as_string(res[0], attr_name, NULL);
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
	const char *attrs[] = { NULL };
	int ret;

	va_start(ap, format);
	ret = gendb_search_v(sam_ldb, mem_ctx, basedn, NULL, attrs, format, ap);
	va_end(ap);

	return ret;
}


/*
  search the sam for a single integer attribute in exactly 1 record
*/
unsigned int samdb_search_uint(struct ldb_context *sam_ldb,
			 TALLOC_CTX *mem_ctx,
			 unsigned int default_value,
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

	return ldb_msg_find_attr_as_uint(res[0], attr_name, default_value);
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

	return ldb_msg_find_attr_as_int64(res[0], attr_name, default_value);
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
		(*strs)[i] = ldb_msg_find_attr_as_string(res[i], attr_name, NULL);
	}
	(*strs)[count] = NULL;

	return count;
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
	bool ok;
	const struct ldb_val *v;
	struct dom_sid *sid;
	v = ldb_msg_find_ldb_val(msg, attr);
	if (v == NULL) {
		return NULL;
	}
	sid = talloc(mem_ctx, struct dom_sid);
	if (sid == NULL) {
		return NULL;
	}
	ok = sid_blob_parse(*v, sid);
	if (!ok) {
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
	struct GUID guid;
	NTSTATUS status;

	v = ldb_msg_find_ldb_val(msg, attr);
	if (!v) return GUID_zero();

	status = GUID_from_ndr_blob(v, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return GUID_zero();
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
NTTIME samdb_result_nttime(const struct ldb_message *msg, const char *attr,
			   NTTIME default_value)
{
	return ldb_msg_find_attr_as_uint64(msg, attr, default_value);
}

/*
 * Windows stores 0 for lastLogoff.
 * But when a MS DC return the lastLogoff (as Logoff Time)
 * it returns 0x7FFFFFFFFFFFFFFF, not returning this value in this case
 * cause windows 2008 and newer version to fail for SMB requests
 */
NTTIME samdb_result_last_logoff(const struct ldb_message *msg)
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
NTTIME samdb_result_account_expires(const struct ldb_message *msg)
{
	NTTIME ret = ldb_msg_find_attr_as_uint64(msg, "accountExpires",
						 0);

	if (ret == 0)
		ret = 0x7FFFFFFFFFFFFFFFULL;

	return ret;
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
	uint64_t attr_time = ldb_msg_find_attr_as_uint64(msg, attr, 0);
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
	int64_t attr_time = ldb_msg_find_attr_as_int64(msg, "pwdLastSet", 0);
	uint32_t userAccountControl = ldb_msg_find_attr_as_uint(msg,
								"userAccountControl",
								0);
	int64_t maxPwdAge;

	/* Machine accounts don't expire, and there is a flag for 'no expiry' */
	if (!(userAccountControl & UF_NORMAL_ACCOUNT)
	    || (userAccountControl & UF_DONT_EXPIRE_PASSWD)) {
		return 0x7FFFFFFFFFFFFFFFULL;
	}

	if (attr_time == 0) {
		return 0;
	}
	if (attr_time == -1) {
		return 0x7FFFFFFFFFFFFFFFULL;
	}

	maxPwdAge = samdb_search_int64(sam_ldb, mem_ctx, 0, domain_dn,
				       "maxPwdAge", NULL);
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
  pull an array of samr_Password structures from a result set.
*/
unsigned int samdb_result_hashes(TALLOC_CTX *mem_ctx, const struct ldb_message *msg,
			   const char *attr, struct samr_Password **hashes)
{
	unsigned int count, i;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);

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
		unsigned int num_nt;
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
		if (lpcfg_lanman_auth(lp_ctx)) {
			unsigned int num_lm;
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
	size_t units_per_week = 168;
	const struct ldb_val *val = ldb_msg_find_ldb_val(msg, attr);

	ZERO_STRUCT(hours);

	if (val) {
		units_per_week = val->length * 8;
	}

	hours.bits = talloc_array(mem_ctx, uint8_t, units_per_week/8);
	if (!hours.bits) {
		return hours;
	}
	hours.units_per_week = units_per_week;
	memset(hours.bits, 0xFF, units_per_week/8);
	if (val) {
		memcpy(hours.bits, val->data, val->length);
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
	s.length = s.size = val->length;
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
	unsigned int i;
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

int samdb_find_or_add_attribute(struct ldb_context *ldb, struct ldb_message *msg, const char *name, const char *set_value)
{
	struct ldb_message_element *el;

       	el = ldb_msg_find_element(msg, name);
	if (el) {
		return LDB_SUCCESS;
	}

	return ldb_msg_add_string(msg, name, set_value);
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
				       sid,
				       (ndr_push_flags_fn_t)ndr_push_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ldb_operr(sam_ldb);
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
	   dsdb_replace() to be used everywhere */
	return ldb_msg_add_empty(msg, attr_name, LDB_FLAG_MOD_REPLACE, NULL);
}

/*
  add an add attribute value to a message or enhance an existing attribute
  which has the same name and the add flag set.
*/
int samdb_msg_add_addval(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx,
			 struct ldb_message *msg, const char *attr_name,
			 const char *value)
{
	struct ldb_message_element *el;
	struct ldb_val val, *vals;
	char *v;
	unsigned int i;
	bool found = false;
	int ret;

	v = talloc_strdup(mem_ctx, value);
	if (v == NULL) {
		return ldb_oom(sam_ldb);
	}

	val.data = (uint8_t *) v;
	val.length = strlen(v);

	if (val.length == 0) {
		/* allow empty strings as non-existent attributes */
		return LDB_SUCCESS;
	}

	for (i = 0; i < msg->num_elements; i++) {
		el = &msg->elements[i];
		if ((ldb_attr_cmp(el->name, attr_name) == 0) &&
		    (LDB_FLAG_MOD_TYPE(el->flags) == LDB_FLAG_MOD_ADD)) {
			found = true;
			break;
		}
	}
	if (!found) {
		ret = ldb_msg_add_empty(msg, attr_name, LDB_FLAG_MOD_ADD,
					&el);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	vals = talloc_realloc(msg->elements, el->values, struct ldb_val,
			      el->num_values + 1);
	if (vals == NULL) {
		return ldb_oom(sam_ldb);
	}
	el->values = vals;
	el->values[el->num_values] = val;
	++(el->num_values);

	return LDB_SUCCESS;
}

/*
  add a delete attribute value to a message or enhance an existing attribute
  which has the same name and the delete flag set.
*/
int samdb_msg_add_delval(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx,
			 struct ldb_message *msg, const char *attr_name,
			 const char *value)
{
	struct ldb_message_element *el;
	struct ldb_val val, *vals;
	char *v;
	unsigned int i;
	bool found = false;
	int ret;

	v = talloc_strdup(mem_ctx, value);
	if (v == NULL) {
		return ldb_oom(sam_ldb);
	}

	val.data = (uint8_t *) v;
	val.length = strlen(v);

	if (val.length == 0) {
		/* allow empty strings as non-existent attributes */
		return LDB_SUCCESS;
	}

	for (i = 0; i < msg->num_elements; i++) {
		el = &msg->elements[i];
		if ((ldb_attr_cmp(el->name, attr_name) == 0) &&
		    (LDB_FLAG_MOD_TYPE(el->flags) == LDB_FLAG_MOD_DELETE)) {
			found = true;
			break;
		}
	}
	if (!found) {
		ret = ldb_msg_add_empty(msg, attr_name, LDB_FLAG_MOD_DELETE,
					&el);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	vals = talloc_realloc(msg->elements, el->values, struct ldb_val,
			      el->num_values + 1);
	if (vals == NULL) {
		return ldb_oom(sam_ldb);
	}
	el->values = vals;
	el->values[el->num_values] = val;
	++(el->num_values);

	return LDB_SUCCESS;
}

/*
  add a int element to a message
*/
int samdb_msg_add_int(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, int v)
{
	const char *s = talloc_asprintf(mem_ctx, "%d", v);
	if (s == NULL) {
		return ldb_oom(sam_ldb);
	}
	return ldb_msg_add_string(msg, attr_name, s);
}

/*
 * Add an unsigned int element to a message
 *
 * The issue here is that we have not yet first cast to int32_t explicitly,
 * before we cast to an signed int to printf() into the %d or cast to a
 * int64_t before we then cast to a long long to printf into a %lld.
 *
 * There are *no* unsigned integers in Active Directory LDAP, even the RID
 * allocations and ms-DS-Secondary-KrbTgt-Number are *signed* quantities.
 * (See the schema, and the syntax definitions in schema_syntax.c).
 *
 */
int samdb_msg_add_uint(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, unsigned int v)
{
	return samdb_msg_add_int(sam_ldb, mem_ctx, msg, attr_name, (int)v);
}

/*
  add a (signed) int64_t element to a message
*/
int samdb_msg_add_int64(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			const char *attr_name, int64_t v)
{
	const char *s = talloc_asprintf(mem_ctx, "%lld", (long long)v);
	if (s == NULL) {
		return ldb_oom(sam_ldb);
	}
	return ldb_msg_add_string(msg, attr_name, s);
}

/*
 * Add an unsigned int64_t (uint64_t) element to a message
 *
 * The issue here is that we have not yet first cast to int32_t explicitly,
 * before we cast to an signed int to printf() into the %d or cast to a
 * int64_t before we then cast to a long long to printf into a %lld.
 *
 * There are *no* unsigned integers in Active Directory LDAP, even the RID
 * allocations and ms-DS-Secondary-KrbTgt-Number are *signed* quantities.
 * (See the schema, and the syntax definitions in schema_syntax.c).
 *
 */
int samdb_msg_add_uint64(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			const char *attr_name, uint64_t v)
{
	return samdb_msg_add_int64(sam_ldb, mem_ctx, msg, attr_name, (int64_t)v);
}

/*
  add a samr_Password element to a message
*/
int samdb_msg_add_hash(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		       const char *attr_name, const struct samr_Password *hash)
{
	struct ldb_val val;
	val.data = talloc_memdup(mem_ctx, hash->hash, 16);
	if (!val.data) {
		return ldb_oom(sam_ldb);
	}
	val.length = 16;
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/*
  add a samr_Password array to a message
*/
int samdb_msg_add_hashes(struct ldb_context *ldb,
			 TALLOC_CTX *mem_ctx, struct ldb_message *msg,
			 const char *attr_name, struct samr_Password *hashes,
			 unsigned int count)
{
	struct ldb_val val;
	unsigned int i;
	val.data = talloc_array_size(mem_ctx, 16, count);
	val.length = count*16;
	if (!val.data) {
		return ldb_oom(ldb);
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
	val.length = parameters->length;
	val.data = (uint8_t *)parameters->array;
	return ldb_msg_add_value(msg, attr_name, &val, NULL);
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
	return ldb_msg_add_string(msg, attr_name, str);
}

/*
 * sets a signed integer in a message
 */
int samdb_msg_set_int(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx,
		      struct ldb_message *msg, const char *attr_name, int v)
{
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, attr_name);
	if (el) {
		el->num_values = 0;
	}
	return samdb_msg_add_int(sam_ldb, mem_ctx, msg, attr_name, v);
}

/*
 * Sets an unsigned int element in a message
 *
 * The issue here is that we have not yet first cast to int32_t explicitly,
 * before we cast to an signed int to printf() into the %d or cast to a
 * int64_t before we then cast to a long long to printf into a %lld.
 *
 * There are *no* unsigned integers in Active Directory LDAP, even the RID
 * allocations and ms-DS-Secondary-KrbTgt-Number are *signed* quantities.
 * (See the schema, and the syntax definitions in schema_syntax.c).
 *
 */
int samdb_msg_set_uint(struct ldb_context *sam_ldb, TALLOC_CTX *mem_ctx,
		       struct ldb_message *msg, const char *attr_name,
		       unsigned int v)
{
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, attr_name);
	if (el) {
		el->num_values = 0;
	}
	return samdb_msg_add_uint(sam_ldb, mem_ctx, msg, attr_name, v);
}

/*
 * Handle ldb_request in transaction
 */
static int dsdb_autotransaction_request(struct ldb_context *sam_ldb,
					struct ldb_request *req)
{
	int ret;

	ret = ldb_transaction_start(sam_ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(sam_ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		return ldb_transaction_commit(sam_ldb);
	}
	ldb_transaction_cancel(sam_ldb);

	return ret;
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

struct ldb_dn *samdb_aggregate_schema_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx) 
{
	struct ldb_dn *schema_dn = ldb_get_schema_basedn(sam_ctx);
	struct ldb_dn *aggregate_dn;
	if (!schema_dn) {
		return NULL;
	}

	aggregate_dn = ldb_dn_copy(mem_ctx, schema_dn);
	if (!aggregate_dn) {
		return NULL;
	}
	if (!ldb_dn_add_child_fmt(aggregate_dn, "CN=Aggregate")) {
		return NULL;
	}
	return aggregate_dn;
}

struct ldb_dn *samdb_partitions_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *new_dn;

	new_dn = ldb_dn_copy(mem_ctx, ldb_get_config_basedn(sam_ctx));
	if ( ! ldb_dn_add_child_fmt(new_dn, "CN=Partitions")) {
		talloc_free(new_dn);
		return NULL;
	}
	return new_dn;
}

struct ldb_dn *samdb_infrastructure_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx)
{
       struct ldb_dn *new_dn;

       new_dn = ldb_dn_copy(mem_ctx, ldb_get_default_basedn(sam_ctx));
       if ( ! ldb_dn_add_child_fmt(new_dn, "CN=Infrastructure")) {
               talloc_free(new_dn);
               return NULL;
       }
       return new_dn;
}

struct ldb_dn *samdb_sites_dn(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx)
{
	struct ldb_dn *new_dn;

	new_dn = ldb_dn_copy(mem_ctx, ldb_get_config_basedn(sam_ctx));
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
	talloc_free(tmp_ctx);
	return NULL;
}

/*
  get domain sid from cache
*/
const struct dom_sid *samdb_domain_sid_cache_only(struct ldb_context *ldb)
{
	return (struct dom_sid *)ldb_get_opaque(ldb, "cache.domain_sid");
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

bool samdb_set_ntds_settings_dn(struct ldb_context *ldb, struct ldb_dn *ntds_settings_dn_in)
{
	TALLOC_CTX *tmp_ctx;
	struct ldb_dn *ntds_settings_dn_new;
	struct ldb_dn *ntds_settings_dn_old;

	/* see if we have a cached copy */
	ntds_settings_dn_old = talloc_get_type(ldb_get_opaque(ldb, 
							      "cache.ntds_settings_dn"), struct ldb_dn);

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ntds_settings_dn_new = ldb_dn_copy(tmp_ctx, ntds_settings_dn_in);
	if (!ntds_settings_dn_new) {
		goto failed;
	}

	/* cache the domain_sid in the ldb */
	if (ldb_set_opaque(ldb, "cache.ntds_settings_dn", ntds_settings_dn_new) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, ntds_settings_dn_new);
	talloc_free(tmp_ctx);
	talloc_free(ntds_settings_dn_old);

	return true;

failed:
	DEBUG(1,("Failed to set our NTDS Settings DN in the ldb!\n"));
	talloc_free(tmp_ctx);
	return false;
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
	settings_dn = (struct ldb_dn *)ldb_get_opaque(ldb, "cache.ntds_settings_dn");
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
	if (ldb_set_opaque(ldb, "cache.ntds_settings_dn", settings_dn) != LDB_SUCCESS) {
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
	struct ldb_dn *servers_dn;
	struct ldb_dn *server_site_dn;

	/* TODO: there must be a saner way to do this!! */
	server_dn = samdb_server_dn(ldb, mem_ctx);
	if (!server_dn) return NULL;

	servers_dn = ldb_dn_get_parent(mem_ctx, server_dn);
	talloc_free(server_dn);
	if (!servers_dn) return NULL;

	server_site_dn = ldb_dn_get_parent(mem_ctx, servers_dn);
	talloc_free(servers_dn);

	return server_site_dn;
}

/*
  find the site name from a computers DN record
 */
int samdb_find_site_for_computer(struct ldb_context *ldb,
				 TALLOC_CTX *mem_ctx, struct ldb_dn *computer_dn,
				 const char **site_name)
{
	int ret;
	struct ldb_dn *dn;
	const struct ldb_val *rdn_val;

	*site_name = NULL;

	ret = samdb_reference_dn(ldb, mem_ctx, computer_dn, "serverReferenceBL", &dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!ldb_dn_remove_child_components(dn, 2)) {
		talloc_free(dn);
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	rdn_val = ldb_dn_get_rdn_val(dn);
	if (rdn_val == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	(*site_name) = talloc_strndup(mem_ctx, (const char *)rdn_val->data, rdn_val->length);
	talloc_free(dn);
	if (!*site_name) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return LDB_SUCCESS;
}

/*
  find the NTDS GUID from a computers DN record
 */
int samdb_find_ntdsguid_for_computer(struct ldb_context *ldb, struct ldb_dn *computer_dn,
				     struct GUID *ntds_guid)
{
	int ret;
	struct ldb_dn *dn;

	*ntds_guid = GUID_zero();

	ret = samdb_reference_dn(ldb, ldb, computer_dn, "serverReferenceBL", &dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!ldb_dn_add_child_fmt(dn, "CN=NTDS Settings")) {
		talloc_free(dn);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = dsdb_find_guid_by_dn(ldb, dn, ntds_guid);
	talloc_free(dn);
	return ret;
}

/*
  find a 'reference' DN that points at another object
  (eg. serverReference, rIDManagerReference etc)
 */
int samdb_reference_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_dn *base,
		       const char *attribute, struct ldb_dn **dn)
{
	const char *attrs[2];
	struct ldb_result *res;
	int ret;

	attrs[0] = attribute;
	attrs[1] = NULL;

	ret = dsdb_search(ldb, mem_ctx, &res, base, LDB_SCOPE_BASE, attrs, DSDB_SEARCH_ONE_ONLY, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	*dn = ldb_msg_find_attr_as_dn(ldb, mem_ctx, res->msgs[0], attribute);
	if (!*dn) {
		if (!ldb_msg_find_element(res->msgs[0], attribute)) {
			ldb_asprintf_errstring(ldb, "Cannot find attribute %s of %s to calculate reference dn", attribute,
					       ldb_dn_get_linearized(base));
		} else {
			ldb_asprintf_errstring(ldb, "Cannot interpret attribute %s of %s as a dn", attribute,
					       ldb_dn_get_linearized(base));
		}
		talloc_free(res);
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	talloc_free(res);
	return LDB_SUCCESS;
}

/*
  find our machine account via the serverReference attribute in the
  server DN
 */
int samdb_server_reference_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_dn **dn)
{
	struct ldb_dn *server_dn;
	int ret;

	server_dn = samdb_server_dn(ldb, mem_ctx);
	if (server_dn == NULL) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	ret = samdb_reference_dn(ldb, mem_ctx, server_dn, "serverReference", dn);
	talloc_free(server_dn);

	return ret;
}

/*
  find the RID Manager$ DN via the rIDManagerReference attribute in the
  base DN
 */
int samdb_rid_manager_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_dn **dn)
{
	return samdb_reference_dn(ldb, mem_ctx, ldb_get_default_basedn(ldb),
				  "rIDManagerReference", dn);
}

/*
  find the RID Set DN via the rIDSetReferences attribute in our
  machine account DN
 */
int samdb_rid_set_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_dn **dn)
{
	struct ldb_dn *server_ref_dn;
	int ret;

	ret = samdb_server_reference_dn(ldb, mem_ctx, &server_ref_dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = samdb_reference_dn(ldb, mem_ctx, server_ref_dn, "rIDSetReferences", dn);
	talloc_free(server_ref_dn);
	return ret;
}

const char *samdb_server_site_name(struct ldb_context *ldb, TALLOC_CTX *mem_ctx)
{
	const struct ldb_val *val = ldb_dn_get_rdn_val(samdb_server_site_dn(ldb,
									    mem_ctx));

	if (val == NULL) {
		return NULL;
	}

	return (const char *) val->data;
}

/*
 * Finds the client site by using the client's IP address.
 * The "subnet_name" returns the name of the subnet if parameter != NULL
 */
const char *samdb_client_site_name(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
				   const char *ip_address, char **subnet_name)
{
	const char *attrs[] = { "cn", "siteObject", NULL };
	struct ldb_dn *sites_container_dn, *subnets_dn, *sites_dn;
	struct ldb_result *res;
	const struct ldb_val *val;
	const char *site_name = NULL, *l_subnet_name = NULL;
	const char *allow_list[2] = { NULL, NULL };
	unsigned int i, count;
	int cnt, ret;

	/*
	 * if we don't have a client ip e.g. ncalrpc
	 * the server site is the client site
	 */
	if (ip_address == NULL) {
		return samdb_server_site_name(ldb, mem_ctx);
	}

	sites_container_dn = samdb_sites_dn(ldb, mem_ctx);
	if (sites_container_dn == NULL) {
		return NULL;
	}

	subnets_dn = ldb_dn_copy(mem_ctx, sites_container_dn);
	if ( ! ldb_dn_add_child_fmt(subnets_dn, "CN=Subnets")) {
		talloc_free(sites_container_dn);
		talloc_free(subnets_dn);
		return NULL;
	}

	ret = ldb_search(ldb, mem_ctx, &res, subnets_dn, LDB_SCOPE_ONELEVEL,
			 attrs, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		count = 0;
	} else if (ret != LDB_SUCCESS) {
		talloc_free(sites_container_dn);
		talloc_free(subnets_dn);
		return NULL;
	} else {
		count = res->count;
	}

	for (i = 0; i < count; i++) {
		l_subnet_name = ldb_msg_find_attr_as_string(res->msgs[i], "cn",
							    NULL);

		allow_list[0] = l_subnet_name;

		if (allow_access(mem_ctx, NULL, allow_list, "", ip_address)) {
			sites_dn = ldb_msg_find_attr_as_dn(ldb, mem_ctx,
							   res->msgs[i],
							   "siteObject");
			if (sites_dn == NULL) {
				/* No reference, maybe another subnet matches */
				continue;
			}

			/* "val" cannot be NULL here since "sites_dn" != NULL */
			val = ldb_dn_get_rdn_val(sites_dn);
			site_name = talloc_strdup(mem_ctx,
						  (const char *) val->data);

			talloc_free(sites_dn);

			break;
		}
	}

	if (site_name == NULL) {
		/* This is the Windows Server fallback rule: when no subnet
		 * exists and we have only one site available then use it (it
		 * is for sure the same as our server site). If more sites do
		 * exist then we don't know which one to use and set the site
		 * name to "". */
		cnt = samdb_search_count(ldb, mem_ctx, sites_container_dn,
					 "(objectClass=site)");
		if (cnt == 1) {
			site_name = samdb_server_site_name(ldb, mem_ctx);
		} else {
			site_name = talloc_strdup(mem_ctx, "");
		}
		l_subnet_name = NULL;
	}

	if (subnet_name != NULL) {
		*subnet_name = talloc_strdup(mem_ctx, l_subnet_name);
	}

	talloc_free(sites_container_dn);
	talloc_free(subnets_dn);
	talloc_free(res);

	return site_name;
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
	if (ret != LDB_SUCCESS) {
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
	uint32_t options;
	int ret;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		DEBUG(1, ("talloc_new failed in samdb_is_pdc"));
		return false;
	}

	/* Query cn=ntds settings,.... */
	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return false;
	}
	if (res->count != 1) {
		talloc_free(tmp_ctx);
		return false;
	}

	options = ldb_msg_find_attr_as_uint(res->msgs[0], "options", 0);
	talloc_free(tmp_ctx);

	/* if options attribute has the 0x00000001 flag set, then enable the global catlog */
	if (options & DS_NTDSDSA_OPT_IS_GC) {
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
	int ret = LDB_SUCCESS;
	const char *attrs[] = { NULL };

	local_ctx = talloc_new(mem_ctx);
	if (local_ctx == NULL) return ldb_oom(ldb);

	while ((sdn = ldb_dn_get_parent(local_ctx, sdn))) {
		ret = ldb_search(ldb, local_ctx, &res, sdn, LDB_SCOPE_BASE, attrs,
				 "(|(objectClass=domain)(objectClass=builtinDomain))");
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
 * Performs checks on a user password (plaintext UNIX format - attribute
 * "password"). The remaining parameters have to be extracted from the domain
 * object in the AD.
 *
 * Result codes from "enum samr_ValidationStatus" (consider "samr.idl")
 */
enum samr_ValidationStatus samdb_check_password(const DATA_BLOB *password,
						const uint32_t pwdProperties,
						const uint32_t minPwdLength)
{
	/* checks if the "minPwdLength" property is satisfied */
	if (minPwdLength > password->length)
		return SAMR_VALIDATION_STATUS_PWD_TOO_SHORT;

	/* checks the password complexity */
	if (((pwdProperties & DOMAIN_PASSWORD_COMPLEX) != 0)
			&& (password->data != NULL)
			&& (!check_password_quality((const char *) password->data)))
		return SAMR_VALIDATION_STATUS_NOT_COMPLEX_ENOUGH;

	return SAMR_VALIDATION_STATUS_SUCCESS;
}

/*
 * Callback for "samdb_set_password" password change
 */
int samdb_set_password_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS) {
		ret = ares->error;
		req->context = talloc_steal(req,
					    ldb_reply_get_control(ares, DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID));
		talloc_free(ares);
		return ldb_request_done(req, ret);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	req->context = talloc_steal(req,
				    ldb_reply_get_control(ares, DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID));
	talloc_free(ares);
	return ldb_request_done(req, LDB_SUCCESS);
}

/*
 * Sets the user password using plaintext UTF16 (attribute "new_password") or
 * LM (attribute "lmNewHash") or NT (attribute "ntNewHash") hash. Also pass
 * the old LM and/or NT hash (attributes "lmOldHash"/"ntOldHash") if it is a
 * user change or not. The "rejectReason" gives some more information if the
 * change failed.
 *
 * Results: NT_STATUS_OK, NT_STATUS_INVALID_PARAMETER, NT_STATUS_UNSUCCESSFUL,
 *   NT_STATUS_WRONG_PASSWORD, NT_STATUS_PASSWORD_RESTRICTION
 */
NTSTATUS samdb_set_password(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
			    struct ldb_dn *user_dn, struct ldb_dn *domain_dn,
			    const DATA_BLOB *new_password,
			    const struct samr_Password *lmNewHash,
			    const struct samr_Password *ntNewHash,
			    const struct samr_Password *lmOldHash,
			    const struct samr_Password *ntOldHash,
			    enum samPwdChangeReason *reject_reason,
			    struct samr_DomInfo1 **_dominfo)
{
	struct ldb_message *msg;
	struct ldb_message_element *el;
	struct ldb_request *req;
	struct dsdb_control_password_change_status *pwd_stat = NULL;
	int ret;
	NTSTATUS status = NT_STATUS_OK;

#define CHECK_RET(x) \
	if (x != LDB_SUCCESS) { \
		talloc_free(msg); \
		return NT_STATUS_NO_MEMORY; \
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = user_dn;
	if ((new_password != NULL)
			&& ((lmNewHash == NULL) && (ntNewHash == NULL))) {
		/* we have the password as plaintext UTF16 */
		CHECK_RET(ldb_msg_add_value(msg, "clearTextPassword",
					    new_password, NULL));
		el = ldb_msg_find_element(msg, "clearTextPassword");
		el->flags = LDB_FLAG_MOD_REPLACE;
	} else if ((new_password == NULL)
			&& ((lmNewHash != NULL) || (ntNewHash != NULL))) {
		/* we have a password as LM and/or NT hash */
		if (lmNewHash != NULL) {
			CHECK_RET(samdb_msg_add_hash(ldb, mem_ctx, msg,
				"dBCSPwd", lmNewHash));
			el = ldb_msg_find_element(msg, "dBCSPwd");
			el->flags = LDB_FLAG_MOD_REPLACE;
		}
		if (ntNewHash != NULL) {
			CHECK_RET(samdb_msg_add_hash(ldb, mem_ctx, msg,
				"unicodePwd", ntNewHash));
			el = ldb_msg_find_element(msg, "unicodePwd");
			el->flags = LDB_FLAG_MOD_REPLACE;
		}
	} else {
		/* the password wasn't specified correctly */
		talloc_free(msg);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* build modify request */
	ret = ldb_build_mod_req(&req, ldb, mem_ctx, msg, NULL, NULL,
				samdb_set_password_callback, NULL);
        if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
        }

	/* A password change operation */
	if ((ntOldHash != NULL) || (lmOldHash != NULL)) {
		struct dsdb_control_password_change *change;

		change = talloc(req, struct dsdb_control_password_change);
		if (change == NULL) {
			talloc_free(req);
			talloc_free(msg);
			return NT_STATUS_NO_MEMORY;
		}

		change->old_nt_pwd_hash = ntOldHash;
		change->old_lm_pwd_hash = lmOldHash;

		ret = ldb_request_add_control(req,
					      DSDB_CONTROL_PASSWORD_CHANGE_OID,
					      true, change);
		if (ret != LDB_SUCCESS) {
			talloc_free(req);
			talloc_free(msg);
			return NT_STATUS_NO_MEMORY;
		}
	}
	ret = ldb_request_add_control(req,
				      DSDB_CONTROL_PASSWORD_HASH_VALUES_OID,
				      true, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(req);
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}
	ret = ldb_request_add_control(req,
				      DSDB_CONTROL_PASSWORD_CHANGE_STATUS_OID,
				      true, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(req);
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	ret = dsdb_autotransaction_request(ldb, req);

	if (req->context != NULL) {
		pwd_stat = talloc_steal(mem_ctx,
					((struct ldb_control *)req->context)->data);
	}

	talloc_free(req);
	talloc_free(msg);

	/* Sets the domain info (if requested) */
	if (_dominfo != NULL) {
		struct samr_DomInfo1 *dominfo;

		dominfo = talloc_zero(mem_ctx, struct samr_DomInfo1);
		if (dominfo == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		if (pwd_stat != NULL) {
			dominfo->min_password_length     = pwd_stat->domain_data.minPwdLength;
			dominfo->password_properties     = pwd_stat->domain_data.pwdProperties;
			dominfo->password_history_length = pwd_stat->domain_data.pwdHistoryLength;
			dominfo->max_password_age        = pwd_stat->domain_data.maxPwdAge;
			dominfo->min_password_age        = pwd_stat->domain_data.minPwdAge;
		}

		*_dominfo = dominfo;
	}

	if (reject_reason != NULL) {
		if (pwd_stat != NULL) {
			*reject_reason = pwd_stat->reject_reason;
		} else {
			*reject_reason = SAM_PWD_CHANGE_NO_ERROR;
		}
	}

	if (pwd_stat != NULL) {
		talloc_free(pwd_stat);
	}

	if (ret == LDB_ERR_CONSTRAINT_VIOLATION) {
		const char *errmsg = ldb_errstring(ldb);
		char *endptr = NULL;
		WERROR werr = WERR_GENERAL_FAILURE;
		status = NT_STATUS_UNSUCCESSFUL;
		if (errmsg != NULL) {
			werr = W_ERROR(strtol(errmsg, &endptr, 16));
		}
		if (endptr != errmsg) {
			if (W_ERROR_EQUAL(werr, WERR_INVALID_PASSWORD)) {
				status = NT_STATUS_WRONG_PASSWORD;
			}
			if (W_ERROR_EQUAL(werr, WERR_PASSWORD_RESTRICTION)) {
				status = NT_STATUS_PASSWORD_RESTRICTION;
			}
		}
	} else if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		/* don't let the caller know if an account doesn't exist */
		status = NT_STATUS_WRONG_PASSWORD;
	} else if (ret != LDB_SUCCESS) {
		status = NT_STATUS_UNSUCCESSFUL;
	}

	return status;
}

/*
 * Sets the user password using plaintext UTF16 (attribute "new_password") or
 * LM (attribute "lmNewHash") or NT (attribute "ntNewHash") hash. Also pass
 * the old LM and/or NT hash (attributes "lmOldHash"/"ntOldHash") if it is a
 * user change or not. The "rejectReason" gives some more information if the
 * change failed.
 *
 * This wrapper function for "samdb_set_password" takes a SID as input rather
 * than a user DN.
 *
 * This call encapsulates a new LDB transaction for changing the password;
 * therefore the user hasn't to start a new one.
 *
 * Results: NT_STATUS_OK, NT_STATUS_INTERNAL_DB_CORRUPTION,
 *   NT_STATUS_INVALID_PARAMETER, NT_STATUS_UNSUCCESSFUL,
 *   NT_STATUS_WRONG_PASSWORD, NT_STATUS_PASSWORD_RESTRICTION,
 *   NT_STATUS_TRANSACTION_ABORTED, NT_STATUS_NO_SUCH_USER
 */
NTSTATUS samdb_set_password_sid(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
				const struct dom_sid *user_sid,
				const DATA_BLOB *new_password,
				const struct samr_Password *lmNewHash,
				const struct samr_Password *ntNewHash,
				const struct samr_Password *lmOldHash,
				const struct samr_Password *ntOldHash,
				enum samPwdChangeReason *reject_reason,
				struct samr_DomInfo1 **_dominfo) 
{
	NTSTATUS nt_status;
	struct ldb_dn *user_dn;
	int ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Failed to start transaction: %s\n", ldb_errstring(ldb)));
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	user_dn = samdb_search_dn(ldb, mem_ctx, NULL,
				  "(&(objectSid=%s)(objectClass=user))", 
				  ldap_encode_ndr_dom_sid(mem_ctx, user_sid));
	if (!user_dn) {
		ldb_transaction_cancel(ldb);
		DEBUG(3, ("samdb_set_password_sid: SID %s not found in samdb, returning NO_SUCH_USER\n",
			  dom_sid_string(mem_ctx, user_sid)));
		return NT_STATUS_NO_SUCH_USER;
	}

	nt_status = samdb_set_password(ldb, mem_ctx,
				       user_dn, NULL,
				       new_password,
				       lmNewHash, ntNewHash,
				       lmOldHash, ntOldHash,
				       reject_reason, _dominfo);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ldb_transaction_cancel(ldb);
		talloc_free(user_dn);
		return nt_status;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to commit transaction to change password on %s: %s\n",
			 ldb_dn_get_linearized(user_dn),
			 ldb_errstring(ldb)));
		talloc_free(user_dn);
		return NT_STATUS_TRANSACTION_ABORTED;
	}

	talloc_free(user_dn);
	return NT_STATUS_OK;
}


NTSTATUS samdb_create_foreign_security_principal(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
						 struct dom_sid *sid, struct ldb_dn **ret_dn) 
{
	struct ldb_message *msg;
	struct ldb_dn *basedn;
	char *sidstr;
	int ret;

	sidstr = dom_sid_string(mem_ctx, sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	/* We might have to create a ForeignSecurityPrincipal, even if this user
	 * is in our own domain */

	msg = ldb_msg_new(sidstr);
	if (msg == NULL) {
		talloc_free(sidstr);
		return NT_STATUS_NO_MEMORY;
	}

	ret = dsdb_wellknown_dn(sam_ctx, sidstr,
				ldb_get_default_basedn(sam_ctx),
				DS_GUID_FOREIGNSECURITYPRINCIPALS_CONTAINER,
				&basedn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("Failed to find DN for "
			  "ForeignSecurityPrincipal container - %s\n", ldb_errstring(sam_ctx)));
		talloc_free(sidstr);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* add core elements to the ldb_message for the alias */
	msg->dn = basedn;
	if ( ! ldb_dn_add_child_fmt(msg->dn, "CN=%s", sidstr)) {
		talloc_free(sidstr);
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_msg_add_string(msg, "objectClass",
				 "foreignSecurityPrincipal");
	if (ret != LDB_SUCCESS) {
		talloc_free(sidstr);
		return NT_STATUS_NO_MEMORY;
	}

	/* create the alias */
	ret = ldb_add(sam_ctx, msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to create foreignSecurityPrincipal "
			 "record %s: %s\n", 
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(sam_ctx)));
		talloc_free(sidstr);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	*ret_dn = talloc_steal(mem_ctx, msg->dn);
	talloc_free(sidstr);

	return NT_STATUS_OK;
}


/*
  Find the DN of a domain, assuming it to be a dotted.dns name
*/

struct ldb_dn *samdb_dns_domain_to_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *dns_domain) 
{
	unsigned int i;
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
		talloc_free(tmp_ctx);
		return NULL;
	}
	talloc_free(tmp_ctx);
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
	if (ret_domain != LDB_SUCCESS) {
		return NULL;
	}

	if (res_domain_ref->count == 0) {
		ret_domain = ldb_search(ldb, mem_ctx,
						&res_domain_ref, 
						samdb_dns_domain_to_dn(ldb, mem_ctx, domain_name),
						LDB_SCOPE_BASE,
						domain_ref2_attrs,
						"(objectclass=domain)");
		if (ret_domain != LDB_SUCCESS) {
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
			 const struct GUID *guid, struct ldb_dn **dn)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { NULL };
	char *guid_str = GUID_string(mem_ctx, guid);

	if (!guid_str) {
		return ldb_operr(ldb);
	}

	ret = dsdb_search(ldb, mem_ctx, &res, NULL, LDB_SCOPE_SUBTREE, attrs,
			  DSDB_SEARCH_SEARCH_ALL_PARTITIONS |
			  DSDB_SEARCH_SHOW_EXTENDED_DN |
			  DSDB_SEARCH_ONE_ONLY,
			  "objectGUID=%s", guid_str);
	talloc_free(guid_str);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	*dn = talloc_steal(mem_ctx, res->msgs[0]->dn);
	talloc_free(res);

	return LDB_SUCCESS;
}

/*
  use a DN to find a GUID with a given attribute name
 */
int dsdb_find_guid_attr_by_dn(struct ldb_context *ldb,
			      struct ldb_dn *dn, const char *attribute,
			      struct GUID *guid)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[2];
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);

	attrs[0] = attribute;
	attrs[1] = NULL;

	ret = dsdb_search_dn(ldb, tmp_ctx, &res, dn, attrs,
			     DSDB_SEARCH_SHOW_DELETED |
			     DSDB_SEARCH_SHOW_RECYCLED);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count < 1) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	*guid = samdb_result_guid(res->msgs[0], attribute);
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

/*
  use a DN to find a GUID
 */
int dsdb_find_guid_by_dn(struct ldb_context *ldb,
			 struct ldb_dn *dn, struct GUID *guid)
{
	return dsdb_find_guid_attr_by_dn(ldb, dn, "objectGUID", guid);
}



/*
 adds the given GUID to the given ldb_message. This value is added
 for the given attr_name (may be either "objectGUID" or "parentGUID").
 */
int dsdb_msg_add_guid(struct ldb_message *msg,
		struct GUID *guid,
		const char *attr_name)
{
	int ret;
	struct ldb_val v;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx =  talloc_init("dsdb_msg_add_guid");

	status = GUID_to_ndr_blob(guid, tmp_ctx, &v);
	if (!NT_STATUS_IS_OK(status)) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	ret = ldb_msg_add_steal_value(msg, attr_name, &v);
	if (ret != LDB_SUCCESS) {
		DEBUG(4,(__location__ ": Failed to add %s to the message\n",
					 attr_name));
		goto done;
	}

	ret = LDB_SUCCESS;

done:
	talloc_free(tmp_ctx);
	return ret;

}


/*
  use a DN to find a SID
 */
int dsdb_find_sid_by_dn(struct ldb_context *ldb, 
			struct ldb_dn *dn, struct dom_sid *sid)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { "objectSid", NULL };
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	struct dom_sid *s;

	ZERO_STRUCTP(sid);

	ret = dsdb_search_dn(ldb, tmp_ctx, &res, dn, attrs,
			     DSDB_SEARCH_SHOW_DELETED |
			     DSDB_SEARCH_SHOW_RECYCLED);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count < 1) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	s = samdb_result_dom_sid(tmp_ctx, res->msgs[0], "objectSid");
	if (s == NULL) {
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	*sid = *s;
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

/*
  use a SID to find a DN
 */
int dsdb_find_dn_by_sid(struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct dom_sid *sid, struct ldb_dn **dn)
{
	int ret;
	struct ldb_result *res;
	const char *attrs[] = { NULL };
	char *sid_str = ldap_encode_ndr_dom_sid(mem_ctx, sid);

	if (!sid_str) {
		return ldb_operr(ldb);
	}

	ret = dsdb_search(ldb, mem_ctx, &res, NULL, LDB_SCOPE_SUBTREE, attrs,
			  DSDB_SEARCH_SEARCH_ALL_PARTITIONS |
			  DSDB_SEARCH_SHOW_EXTENDED_DN |
			  DSDB_SEARCH_ONE_ONLY,
			  "objectSid=%s", sid_str);
	talloc_free(sid_str);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	*dn = talloc_steal(mem_ctx, res->msgs[0]->dn);
	talloc_free(res);

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
	unsigned int i;
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
					       mem_ctx, 
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
	unsigned int i;

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

		ndr_err = ndr_push_struct_blob(&v, tmp_ctx, 
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
  load the uSNHighest and the uSNUrgent attributes from the @REPLCHANGED
  object for a partition
 */
int dsdb_load_partition_usn(struct ldb_context *ldb, struct ldb_dn *dn,
				uint64_t *uSN, uint64_t *urgent_uSN)
{
	struct ldb_request *req;
	int ret;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	struct dsdb_control_current_partition *p_ctrl;
	struct ldb_result *res;

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
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
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
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

	if (ret == LDB_ERR_NO_SUCH_OBJECT || ret == LDB_ERR_INVALID_DN_SYNTAX) {
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
		if (urgent_uSN) {
			*urgent_uSN = 0;
		}
	} else {
		*uSN = ldb_msg_find_attr_as_uint64(res->msgs[0], "uSNHighest", 0);
		if (urgent_uSN) {
			*urgent_uSN = ldb_msg_find_attr_as_uint64(res->msgs[0], "uSNUrgent", 0);
		}
	}

	talloc_free(tmp_ctx);

	return LDB_SUCCESS;	
}

int drsuapi_DsReplicaCursor2_compare(const struct drsuapi_DsReplicaCursor2 *c1,
						   const struct drsuapi_DsReplicaCursor2 *c2)
{
	return GUID_compare(&c1->source_dsa_invocation_id, &c2->source_dsa_invocation_id);
}

int drsuapi_DsReplicaCursor_compare(const struct drsuapi_DsReplicaCursor *c1,
				    const struct drsuapi_DsReplicaCursor *c2)
{
	return GUID_compare(&c1->source_dsa_invocation_id, &c2->source_dsa_invocation_id);
}


/*
  see if a computer identified by its invocationId is a RODC
*/
int samdb_is_rodc(struct ldb_context *sam_ctx, const struct GUID *objectGUID, bool *is_rodc)
{
	/* 1) find the DN for this servers NTDSDSA object
	   2) search for the msDS-isRODC attribute
	   3) if not present then not a RODC
	   4) if present and TRUE then is a RODC
	*/
	struct ldb_dn *config_dn;
	const char *attrs[] = { "msDS-isRODC", NULL };
	int ret;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx = talloc_new(sam_ctx);

	config_dn = ldb_get_config_basedn(sam_ctx);
	if (!config_dn) {
		talloc_free(tmp_ctx);
		return ldb_operr(sam_ctx);
	}

	ret = dsdb_search(sam_ctx, tmp_ctx, &res, config_dn, LDB_SCOPE_SUBTREE, attrs,
			  DSDB_SEARCH_ONE_ONLY, "objectGUID=%s", GUID_string(tmp_ctx, objectGUID));

	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		*is_rodc = false;
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}

	if (ret != LDB_SUCCESS) {
		DEBUG(1,(("Failed to find our own NTDS Settings object by objectGUID=%s!\n"),
			 GUID_string(tmp_ctx, objectGUID)));
		*is_rodc = false;
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = ldb_msg_find_attr_as_bool(res->msgs[0], "msDS-isRODC", 0);
	*is_rodc = (ret == 1);

	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}


/*
  see if we are a RODC
*/
int samdb_rodc(struct ldb_context *sam_ctx, bool *am_rodc)
{
	const struct GUID *objectGUID;
	int ret;
	bool *cached;

	/* see if we have a cached copy */
	cached = (bool *)ldb_get_opaque(sam_ctx, "cache.am_rodc");
	if (cached) {
		*am_rodc = *cached;
		return LDB_SUCCESS;
	}

	objectGUID = samdb_ntds_objectGUID(sam_ctx);
	if (!objectGUID) {
		return ldb_operr(sam_ctx);
	}

	ret = samdb_is_rodc(sam_ctx, objectGUID, am_rodc);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	cached = talloc(sam_ctx, bool);
	if (cached == NULL) {
		return ldb_oom(sam_ctx);
	}
	*cached = *am_rodc;

	ret = ldb_set_opaque(sam_ctx, "cache.am_rodc", cached);
	if (ret != LDB_SUCCESS) {
		talloc_free(cached);
		return ldb_operr(sam_ctx);
	}

	return LDB_SUCCESS;
}

bool samdb_set_am_rodc(struct ldb_context *ldb, bool am_rodc)
{
	TALLOC_CTX *tmp_ctx;
	bool *cached;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	cached = talloc(tmp_ctx, bool);
	if (!cached) {
		goto failed;
	}

	*cached = am_rodc;
	if (ldb_set_opaque(ldb, "cache.am_rodc", cached) != LDB_SUCCESS) {
		goto failed;
	}

	talloc_steal(ldb, cached);
	talloc_free(tmp_ctx);
	return true;

failed:
	DEBUG(1,("Failed to set our own cached am_rodc in the ldb!\n"));
	talloc_free(tmp_ctx);
	return false;
}


/*
  return NTDS options flags. See MS-ADTS 7.1.1.2.2.1.2.1.1 

  flags are DS_NTDS_OPTION_*
*/
int samdb_ntds_options(struct ldb_context *ldb, uint32_t *options)
{
	TALLOC_CTX *tmp_ctx;
	const char *attrs[] = { "options", NULL };
	int ret;
	struct ldb_result *res;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	*options = ldb_msg_find_attr_as_uint(res->msgs[0], "options", 0);

	talloc_free(tmp_ctx);

	return LDB_SUCCESS;

failed:
	DEBUG(1,("Failed to find our own NTDS Settings options in the ldb!\n"));
	talloc_free(tmp_ctx);
	return LDB_ERR_NO_SUCH_OBJECT;
}

const char* samdb_ntds_object_category(TALLOC_CTX *tmp_ctx, struct ldb_context *ldb)
{
	const char *attrs[] = { "objectCategory", NULL };
	int ret;
	struct ldb_result *res;

	ret = ldb_search(ldb, tmp_ctx, &res, samdb_ntds_settings_dn(ldb), LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	return ldb_msg_find_attr_as_string(res->msgs[0], "objectCategory", NULL);

failed:
	DEBUG(1,("Failed to find our own NTDS Settings objectCategory in the ldb!\n"));
	return NULL;
}

/*
 * Function which generates a "lDAPDisplayName" attribute from a "CN" one.
 * Algorithm implemented according to MS-ADTS 3.1.1.2.3.4
 */
const char *samdb_cn_to_lDAPDisplayName(TALLOC_CTX *mem_ctx, const char *cn)
{
	char **tokens, *ret;
	size_t i;

	tokens = str_list_make(mem_ctx, cn, " -_");
	if (tokens == NULL)
		return NULL;

	/* "tolower()" and "toupper()" should also work properly on 0x00 */
	tokens[0][0] = tolower(tokens[0][0]);
	for (i = 1; i < str_list_length((const char **)tokens); i++)
		tokens[i][0] = toupper(tokens[i][0]);

	ret = talloc_strdup(mem_ctx, tokens[0]);
	for (i = 1; i < str_list_length((const char **)tokens); i++)
		ret = talloc_asprintf_append_buffer(ret, "%s", tokens[i]);

	talloc_free(tokens);

	return ret;
}

/*
 * This detects and returns the domain functional level (DS_DOMAIN_FUNCTION_*)
 */
int dsdb_functional_level(struct ldb_context *ldb)
{
	int *domainFunctionality =
		talloc_get_type(ldb_get_opaque(ldb, "domainFunctionality"), int);
	if (!domainFunctionality) {
		/* this is expected during initial provision */
		DEBUG(4,(__location__ ": WARNING: domainFunctionality not setup\n"));
		return DS_DOMAIN_FUNCTION_2000;
	}
	return *domainFunctionality;
}

/*
 * This detects and returns the forest functional level (DS_DOMAIN_FUNCTION_*)
 */
int dsdb_forest_functional_level(struct ldb_context *ldb)
{
	int *forestFunctionality =
		talloc_get_type(ldb_get_opaque(ldb, "forestFunctionality"), int);
	if (!forestFunctionality) {
		DEBUG(0,(__location__ ": WARNING: forestFunctionality not setup\n"));
		return DS_DOMAIN_FUNCTION_2000;
	}
	return *forestFunctionality;
}

/*
  set a GUID in an extended DN structure
 */
int dsdb_set_extended_dn_guid(struct ldb_dn *dn, const struct GUID *guid, const char *component_name)
{
	struct ldb_val v;
	NTSTATUS status;
	int ret;

	status = GUID_to_ndr_blob(guid, dn, &v);
	if (!NT_STATUS_IS_OK(status)) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	ret = ldb_dn_set_extended_component(dn, component_name, &v);
	data_blob_free(&v);
	return ret;
}

/*
  return a GUID from a extended DN structure
 */
NTSTATUS dsdb_get_extended_dn_guid(struct ldb_dn *dn, struct GUID *guid, const char *component_name)
{
	const struct ldb_val *v;

	v = ldb_dn_get_extended_component(dn, component_name);
	if (v == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return GUID_from_ndr_blob(v, guid);
}

/*
  return a uint64_t from a extended DN structure
 */
NTSTATUS dsdb_get_extended_dn_uint64(struct ldb_dn *dn, uint64_t *val, const char *component_name)
{
	const struct ldb_val *v;
	char *s;

	v = ldb_dn_get_extended_component(dn, component_name);
	if (v == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	s = talloc_strndup(dn, (const char *)v->data, v->length);
	NT_STATUS_HAVE_NO_MEMORY(s);

	*val = strtoull(s, NULL, 0);

	talloc_free(s);
	return NT_STATUS_OK;
}

/*
  return a NTTIME from a extended DN structure
 */
NTSTATUS dsdb_get_extended_dn_nttime(struct ldb_dn *dn, NTTIME *nttime, const char *component_name)
{
	return dsdb_get_extended_dn_uint64(dn, nttime, component_name);
}

/*
  return a uint32_t from a extended DN structure
 */
NTSTATUS dsdb_get_extended_dn_uint32(struct ldb_dn *dn, uint32_t *val, const char *component_name)
{
	const struct ldb_val *v;
	char *s;

	v = ldb_dn_get_extended_component(dn, component_name);
	if (v == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	s = talloc_strndup(dn, (const char *)v->data, v->length);
	NT_STATUS_HAVE_NO_MEMORY(s);

	*val = strtoul(s, NULL, 0);

	talloc_free(s);
	return NT_STATUS_OK;
}

/*
  return a dom_sid from a extended DN structure
 */
NTSTATUS dsdb_get_extended_dn_sid(struct ldb_dn *dn, struct dom_sid *sid, const char *component_name)
{
	const struct ldb_val *sid_blob;
	struct TALLOC_CTX *tmp_ctx;
	enum ndr_err_code ndr_err;

	sid_blob = ldb_dn_get_extended_component(dn, component_name);
	if (!sid_blob) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	tmp_ctx = talloc_new(NULL);

	ndr_err = ndr_pull_struct_blob_all(sid_blob, tmp_ctx, sid,
					   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		talloc_free(tmp_ctx);
		return status;
	}

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}


/*
  return RMD_FLAGS directly from a ldb_dn
  returns 0 if not found
 */
uint32_t dsdb_dn_rmd_flags(struct ldb_dn *dn)
{
	const struct ldb_val *v;
	char buf[32];
	v = ldb_dn_get_extended_component(dn, "RMD_FLAGS");
	if (!v || v->length > sizeof(buf)-1) return 0;
	strncpy(buf, (const char *)v->data, v->length);
	buf[v->length] = 0;
	return strtoul(buf, NULL, 10);
}

/*
  return RMD_FLAGS directly from a ldb_val for a DN
  returns 0 if RMD_FLAGS is not found
 */
uint32_t dsdb_dn_val_rmd_flags(const struct ldb_val *val)
{
	const char *p;
	uint32_t flags;
	char *end;

	if (val->length < 13) {
		return 0;
	}
	p = memmem(val->data, val->length, "<RMD_FLAGS=", 11);
	if (!p) {
		return 0;
	}
	flags = strtoul(p+11, &end, 10);
	if (!end || *end != '>') {
		/* it must end in a > */
		return 0;
	}
	return flags;
}

/*
  return true if a ldb_val containing a DN in storage form is deleted
 */
bool dsdb_dn_is_deleted_val(const struct ldb_val *val)
{
	return (dsdb_dn_val_rmd_flags(val) & DSDB_RMD_FLAG_DELETED) != 0;
}

/*
  return true if a ldb_val containing a DN in storage form is
  in the upgraded w2k3 linked attribute format
 */
bool dsdb_dn_is_upgraded_link_val(struct ldb_val *val)
{
	return memmem(val->data, val->length, "<RMD_ADDTIME=", 13) != NULL;
}

/*
  return a DN for a wellknown GUID
 */
int dsdb_wellknown_dn(struct ldb_context *samdb, TALLOC_CTX *mem_ctx,
		      struct ldb_dn *nc_root, const char *wk_guid,
		      struct ldb_dn **wkguid_dn)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	const char *attrs[] = { NULL };
	int ret;
	struct ldb_dn *dn;
	struct ldb_result *res;

	/* construct the magic WKGUID DN */
	dn = ldb_dn_new_fmt(tmp_ctx, samdb, "<WKGUID=%s,%s>",
			    wk_guid, ldb_dn_get_linearized(nc_root));
	if (!wkguid_dn) {
		talloc_free(tmp_ctx);
		return ldb_operr(samdb);
	}

	ret = dsdb_search_dn(samdb, tmp_ctx, &res, dn, attrs,
			     DSDB_SEARCH_SHOW_DELETED |
			     DSDB_SEARCH_SHOW_RECYCLED);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	(*wkguid_dn) = talloc_steal(mem_ctx, res->msgs[0]->dn);
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}


static int dsdb_dn_compare_ptrs(struct ldb_dn **dn1, struct ldb_dn **dn2)
{
	return ldb_dn_compare(*dn1, *dn2);
}

/*
  find a NC root given a DN within the NC
 */
int dsdb_find_nc_root(struct ldb_context *samdb, TALLOC_CTX *mem_ctx, struct ldb_dn *dn,
		      struct ldb_dn **nc_root)
{
	const char *root_attrs[] = { "namingContexts", NULL };
	TALLOC_CTX *tmp_ctx;
	int ret;
	struct ldb_message_element *el;
	struct ldb_result *root_res;
	unsigned int i;
	struct ldb_dn **nc_dns;

	tmp_ctx = talloc_new(samdb);
	if (tmp_ctx == NULL) {
		return ldb_oom(samdb);
	}

	ret = ldb_search(samdb, tmp_ctx, &root_res,
			 ldb_dn_new(tmp_ctx, samdb, ""), LDB_SCOPE_BASE, root_attrs, NULL);
	if (ret != LDB_SUCCESS) {
		DEBUG(1,("Searching for namingContexts in rootDSE failed: %s\n", ldb_errstring(samdb)));
		talloc_free(tmp_ctx);
		return ret;
	}

	el = ldb_msg_find_element(root_res->msgs[0], "namingContexts");
	if ((el == NULL) || (el->num_values < 3)) {
		struct ldb_message *tmp_msg;

		DEBUG(5,("dsdb_find_nc_root: Finding a valid 'namingContexts' element in the RootDSE failed. Using a temporary list."));

		/* This generates a temporary list of NCs in order to let the
		 * provisioning work. */
		tmp_msg = ldb_msg_new(tmp_ctx);
		if (tmp_msg == NULL) {
			talloc_free(tmp_ctx);
			return ldb_oom(samdb);
		}
		ret = ldb_msg_add_steal_string(tmp_msg, "namingContexts",
					       ldb_dn_alloc_linearized(tmp_msg, ldb_get_schema_basedn(samdb)));
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
		ret = ldb_msg_add_steal_string(tmp_msg, "namingContexts",
					       ldb_dn_alloc_linearized(tmp_msg, ldb_get_config_basedn(samdb)));
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
		ret = ldb_msg_add_steal_string(tmp_msg, "namingContexts",
					       ldb_dn_alloc_linearized(tmp_msg, ldb_get_default_basedn(samdb)));
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
		el = &tmp_msg->elements[0];
	}

       nc_dns = talloc_array(tmp_ctx, struct ldb_dn *, el->num_values);
       if (!nc_dns) {
	       talloc_free(tmp_ctx);
	       return ldb_oom(samdb);
       }

       for (i=0; i<el->num_values; i++) {
	       nc_dns[i] = ldb_dn_from_ldb_val(nc_dns, samdb, &el->values[i]);
	       if (nc_dns[i] == NULL) {
		       talloc_free(tmp_ctx);
		       return ldb_operr(samdb);
	       }
       }

       TYPESAFE_QSORT(nc_dns, el->num_values, dsdb_dn_compare_ptrs);

       for (i=0; i<el->num_values; i++) {
               if (ldb_dn_compare_base(nc_dns[i], dn) == 0) {
		       (*nc_root) = talloc_steal(mem_ctx, nc_dns[i]);
                       talloc_free(tmp_ctx);
                       return LDB_SUCCESS;
               }
       }

       talloc_free(tmp_ctx);
       return LDB_ERR_NO_SUCH_OBJECT;
}


/*
  find the deleted objects DN for any object, by looking for the NC
  root, then looking up the wellknown GUID
 */
int dsdb_get_deleted_objects_dn(struct ldb_context *ldb,
				TALLOC_CTX *mem_ctx, struct ldb_dn *obj_dn,
				struct ldb_dn **do_dn)
{
	struct ldb_dn *nc_root;
	int ret;

	ret = dsdb_find_nc_root(ldb, mem_ctx, obj_dn, &nc_root);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = dsdb_wellknown_dn(ldb, mem_ctx, nc_root, DS_GUID_DELETED_OBJECTS_CONTAINER, do_dn);
	talloc_free(nc_root);
	return ret;
}

/*
  return the tombstoneLifetime, in days
 */
int dsdb_tombstone_lifetime(struct ldb_context *ldb, uint32_t *lifetime)
{
	struct ldb_dn *dn;
	dn = ldb_get_config_basedn(ldb);
	if (!dn) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	dn = ldb_dn_copy(ldb, dn);
	if (!dn) {
		return ldb_operr(ldb);
	}
	/* see MS-ADTS section 7.1.1.2.4.1.1. There doesn't appear to
	 be a wellknown GUID for this */
	if (!ldb_dn_add_child_fmt(dn, "CN=Directory Service,CN=Windows NT,CN=Services")) {
		talloc_free(dn);
		return ldb_operr(ldb);
	}

	*lifetime = samdb_search_uint(ldb, dn, 180, dn, "tombstoneLifetime", "objectClass=nTDSService");
	talloc_free(dn);
	return LDB_SUCCESS;
}

/*
  compare a ldb_val to a string case insensitively
 */
int samdb_ldb_val_case_cmp(const char *s, struct ldb_val *v)
{
	size_t len = strlen(s);
	int ret;
	if (len > v->length) return 1;
	ret = strncasecmp(s, (const char *)v->data, v->length);
	if (ret != 0) return ret;
	if (v->length > len && v->data[len] != 0) {
		return -1;
	}
	return 0;
}


/*
  load the UDV for a partition in v2 format
  The list is returned sorted, and with our local cursor added
 */
int dsdb_load_udv_v2(struct ldb_context *samdb, struct ldb_dn *dn, TALLOC_CTX *mem_ctx,
		     struct drsuapi_DsReplicaCursor2 **cursors, uint32_t *count)
{
	static const char *attrs[] = { "replUpToDateVector", NULL };
	struct ldb_result *r;
	const struct ldb_val *ouv_value;
	unsigned int i;
	int ret;
	uint64_t highest_usn;
	const struct GUID *our_invocation_id;
	struct timeval now = timeval_current();

	ret = ldb_search(samdb, mem_ctx, &r, dn, LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ouv_value = ldb_msg_find_ldb_val(r->msgs[0], "replUpToDateVector");
	if (ouv_value) {
		enum ndr_err_code ndr_err;
		struct replUpToDateVectorBlob ouv;

		ndr_err = ndr_pull_struct_blob(ouv_value, r, &ouv,
					       (ndr_pull_flags_fn_t)ndr_pull_replUpToDateVectorBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			talloc_free(r);
			return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
		}
		if (ouv.version != 2) {
			/* we always store as version 2, and
			 * replUpToDateVector is not replicated
			 */
			return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
		}

		*count = ouv.ctr.ctr2.count;
		*cursors = talloc_steal(mem_ctx, ouv.ctr.ctr2.cursors);
	} else {
		*count = 0;
		*cursors = NULL;
	}

	talloc_free(r);

	our_invocation_id = samdb_ntds_invocation_id(samdb);
	if (!our_invocation_id) {
		DEBUG(0,(__location__ ": No invocationID on samdb - %s\n", ldb_errstring(samdb)));
		talloc_free(*cursors);
		return ldb_operr(samdb);
	}

	ret = dsdb_load_partition_usn(samdb, dn, &highest_usn, NULL);
	if (ret != LDB_SUCCESS) {
		/* nothing to add - this can happen after a vampire */
		TYPESAFE_QSORT(*cursors, *count, drsuapi_DsReplicaCursor2_compare);
		return LDB_SUCCESS;
	}

	for (i=0; i<*count; i++) {
		if (GUID_equal(our_invocation_id, &(*cursors)[i].source_dsa_invocation_id)) {
			(*cursors)[i].highest_usn = highest_usn;
			(*cursors)[i].last_sync_success = timeval_to_nttime(&now);
			TYPESAFE_QSORT(*cursors, *count, drsuapi_DsReplicaCursor2_compare);
			return LDB_SUCCESS;
		}
	}

	(*cursors) = talloc_realloc(mem_ctx, *cursors, struct drsuapi_DsReplicaCursor2, (*count)+1);
	if (! *cursors) {
		return ldb_oom(samdb);
	}

	(*cursors)[*count].source_dsa_invocation_id = *our_invocation_id;
	(*cursors)[*count].highest_usn = highest_usn;
	(*cursors)[*count].last_sync_success = timeval_to_nttime(&now);
	(*count)++;

	TYPESAFE_QSORT(*cursors, *count, drsuapi_DsReplicaCursor2_compare);

	return LDB_SUCCESS;
}

/*
  load the UDV for a partition in version 1 format
  The list is returned sorted, and with our local cursor added
 */
int dsdb_load_udv_v1(struct ldb_context *samdb, struct ldb_dn *dn, TALLOC_CTX *mem_ctx,
		     struct drsuapi_DsReplicaCursor **cursors, uint32_t *count)
{
	struct drsuapi_DsReplicaCursor2 *v2;
	uint32_t i;
	int ret;

	ret = dsdb_load_udv_v2(samdb, dn, mem_ctx, &v2, count);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (*count == 0) {
		talloc_free(v2);
		*cursors = NULL;
		return LDB_SUCCESS;
	}

	*cursors = talloc_array(mem_ctx, struct drsuapi_DsReplicaCursor, *count);
	if (*cursors == NULL) {
		talloc_free(v2);
		return ldb_oom(samdb);
	}

	for (i=0; i<*count; i++) {
		(*cursors)[i].source_dsa_invocation_id = v2[i].source_dsa_invocation_id;
		(*cursors)[i].highest_usn = v2[i].highest_usn;
	}
	talloc_free(v2);
	return LDB_SUCCESS;
}

/*
  add a set of controls to a ldb_request structure based on a set of
  flags. See util.h for a list of available flags
 */
int dsdb_request_add_controls(struct ldb_request *req, uint32_t dsdb_flags)
{
	int ret;
	if (dsdb_flags & DSDB_SEARCH_SEARCH_ALL_PARTITIONS) {
		struct ldb_search_options_control *options;
		/* Using the phantom root control allows us to search all partitions */
		options = talloc(req, struct ldb_search_options_control);
		if (options == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		options->search_options = LDB_SEARCH_OPTION_PHANTOM_ROOT;

		ret = ldb_request_add_control(req,
					      LDB_CONTROL_SEARCH_OPTIONS_OID,
					      true, options);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_SEARCH_SHOW_DELETED) {
		ret = ldb_request_add_control(req, LDB_CONTROL_SHOW_DELETED_OID, true, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_SEARCH_SHOW_RECYCLED) {
		ret = ldb_request_add_control(req, LDB_CONTROL_SHOW_RECYCLED_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT) {
		ret = ldb_request_add_control(req, DSDB_CONTROL_DN_STORAGE_FORMAT_OID, true, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_SEARCH_SHOW_EXTENDED_DN) {
		struct ldb_extended_dn_control *extended_ctrl = talloc(req, struct ldb_extended_dn_control);
		if (!extended_ctrl) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		extended_ctrl->type = 1;

		ret = ldb_request_add_control(req, LDB_CONTROL_EXTENDED_DN_OID, true, extended_ctrl);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_SEARCH_REVEAL_INTERNALS) {
		ret = ldb_request_add_control(req, LDB_CONTROL_REVEAL_INTERNALS, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_MODIFY_RELAX) {
		ret = ldb_request_add_control(req, LDB_CONTROL_RELAX_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_MODIFY_PERMISSIVE) {
		ret = ldb_request_add_control(req, LDB_CONTROL_PERMISSIVE_MODIFY_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_FLAG_AS_SYSTEM) {
		ret = ldb_request_add_control(req, LDB_CONTROL_AS_SYSTEM_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_TREE_DELETE) {
		ret = ldb_request_add_control(req, LDB_CONTROL_TREE_DELETE_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	if (dsdb_flags & DSDB_PROVISION) {
		ret = ldb_request_add_control(req, LDB_CONTROL_PROVISION_OID, false, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

/*
  an add with a set of controls
*/
int dsdb_add(struct ldb_context *ldb, const struct ldb_message *message,
	     uint32_t dsdb_flags)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_add_req(&req, ldb, ldb,
				message,
				NULL,
				NULL,
				ldb_op_default_callback,
				NULL);

	if (ret != LDB_SUCCESS) return ret;

	ret = dsdb_request_add_controls(req, dsdb_flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(req);
		return ret;
	}

	ret = dsdb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  a modify with a set of controls
*/
int dsdb_modify(struct ldb_context *ldb, const struct ldb_message *message,
		uint32_t dsdb_flags)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_mod_req(&req, ldb, ldb,
				message,
				NULL,
				NULL,
				ldb_op_default_callback,
				NULL);

	if (ret != LDB_SUCCESS) return ret;

	ret = dsdb_request_add_controls(req, dsdb_flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(req);
		return ret;
	}

	ret = dsdb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  like dsdb_modify() but set all the element flags to
  LDB_FLAG_MOD_REPLACE
 */
int dsdb_replace(struct ldb_context *ldb, struct ldb_message *msg, uint32_t dsdb_flags)
{
	unsigned int i;

	/* mark all the message elements as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	return dsdb_modify(ldb, msg, dsdb_flags);
}


/*
  search for attrs on one DN, allowing for dsdb_flags controls
 */
int dsdb_search_dn(struct ldb_context *ldb,
		   TALLOC_CTX *mem_ctx,
		   struct ldb_result **_res,
		   struct ldb_dn *basedn,
		   const char * const *attrs,
		   uint32_t dsdb_flags)
{
	int ret;
	struct ldb_request *req;
	struct ldb_result *res;

	res = talloc_zero(mem_ctx, struct ldb_result);
	if (!res) {
		return ldb_oom(ldb);
	}

	ret = ldb_build_search_req(&req, ldb, res,
				   basedn,
				   LDB_SCOPE_BASE,
				   NULL,
				   attrs,
				   NULL,
				   res,
				   ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return ret;
	}

	ret = dsdb_request_add_controls(req, dsdb_flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return ret;
	}

	*_res = res;
	return LDB_SUCCESS;
}

/*
  search for attrs on one DN, by the GUID of the DN, allowing for
  dsdb_flags controls
 */
int dsdb_search_by_dn_guid(struct ldb_context *ldb,
			   TALLOC_CTX *mem_ctx,
			   struct ldb_result **_res,
			   const struct GUID *guid,
			   const char * const *attrs,
			   uint32_t dsdb_flags)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct ldb_dn *dn;
	int ret;

	dn = ldb_dn_new_fmt(tmp_ctx, ldb, "<GUID=%s>", GUID_string(tmp_ctx, guid));
	if (dn == NULL) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	ret = dsdb_search_dn(ldb, mem_ctx, _res, dn, attrs, dsdb_flags);
	talloc_free(tmp_ctx);
	return ret;
}

/*
  general search with dsdb_flags for controls
 */
int dsdb_search(struct ldb_context *ldb,
		TALLOC_CTX *mem_ctx,
		struct ldb_result **_res,
		struct ldb_dn *basedn,
		enum ldb_scope scope,
		const char * const *attrs,
		uint32_t dsdb_flags,
		const char *exp_fmt, ...) _PRINTF_ATTRIBUTE(8, 9)
{
	int ret;
	struct ldb_request *req;
	struct ldb_result *res;
	va_list ap;
	char *expression = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		expression = talloc_vasprintf(tmp_ctx, exp_fmt, ap);
		va_end(ap);

		if (!expression) {
			talloc_free(tmp_ctx);
			return ldb_oom(ldb);
		}
	}

	ret = ldb_build_search_req(&req, ldb, tmp_ctx,
				   basedn,
				   scope,
				   expression,
				   attrs,
				   NULL,
				   res,
				   ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = dsdb_request_add_controls(req, dsdb_flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		ldb_reset_err_string(ldb);
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (dsdb_flags & DSDB_SEARCH_ONE_ONLY) {
		if (res->count == 0) {
			talloc_free(tmp_ctx);
			ldb_reset_err_string(ldb);
			return LDB_ERR_NO_SUCH_OBJECT;
		}
		if (res->count != 1) {
			talloc_free(tmp_ctx);
			ldb_reset_err_string(ldb);
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
	}

	*_res = talloc_steal(mem_ctx, res);
	talloc_free(tmp_ctx);

	return LDB_SUCCESS;
}


/*
  general search with dsdb_flags for controls
  returns exactly 1 record or an error
 */
int dsdb_search_one(struct ldb_context *ldb,
		    TALLOC_CTX *mem_ctx,
		    struct ldb_message **msg,
		    struct ldb_dn *basedn,
		    enum ldb_scope scope,
		    const char * const *attrs,
		    uint32_t dsdb_flags,
		    const char *exp_fmt, ...) _PRINTF_ATTRIBUTE(8, 9)
{
	int ret;
	struct ldb_result *res;
	va_list ap;
	char *expression = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	dsdb_flags |= DSDB_SEARCH_ONE_ONLY;

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		expression = talloc_vasprintf(tmp_ctx, exp_fmt, ap);
		va_end(ap);

		if (!expression) {
			talloc_free(tmp_ctx);
			return ldb_oom(ldb);
		}
		ret = dsdb_search(ldb, tmp_ctx, &res, basedn, scope, attrs,
				  dsdb_flags, "%s", expression);
	} else {
		ret = dsdb_search(ldb, tmp_ctx, &res, basedn, scope, attrs,
				  dsdb_flags, NULL);
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	*msg = talloc_steal(mem_ctx, res->msgs[0]);
	talloc_free(tmp_ctx);

	return LDB_SUCCESS;
}

/* returns back the forest DNS name */
const char *samdb_forest_name(struct ldb_context *ldb, TALLOC_CTX *mem_ctx)
{
	const char *forest_name = ldb_dn_canonical_string(mem_ctx,
							  ldb_get_root_basedn(ldb));
	char *p;

	if (forest_name == NULL) {
		return NULL;
	}

	p = strchr(forest_name, '/');
	if (p) {
		*p = '\0';
	}

	return forest_name;
}

/* returns back the default domain DNS name */
const char *samdb_default_domain_name(struct ldb_context *ldb, TALLOC_CTX *mem_ctx)
{
	const char *domain_name = ldb_dn_canonical_string(mem_ctx,
							  ldb_get_default_basedn(ldb));
	char *p;

	if (domain_name == NULL) {
		return NULL;
	}

	p = strchr(domain_name, '/');
	if (p) {
		*p = '\0';
	}

	return domain_name;
}

/*
   validate that an DSA GUID belongs to the specified user sid.
   The user SID must be a domain controller account (either RODC or
   RWDC)
 */
int dsdb_validate_dsa_guid(struct ldb_context *ldb,
			   const struct GUID *dsa_guid,
			   const struct dom_sid *sid)
{
	/* strategy:
	    - find DN of record with the DSA GUID in the
	      configuration partition (objectGUID)
            - remove "NTDS Settings" component from DN
	    - do a base search on that DN for serverReference with
	      extended-dn enabled
            - extract objectSid from resulting serverReference
              attribute
	    - check this sid matches the sid argument
	*/
	struct ldb_dn *config_dn;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	struct ldb_message *msg;
	const char *attrs1[] = { NULL };
	const char *attrs2[] = { "serverReference", NULL };
	int ret;
	struct ldb_dn *dn, *account_dn;
	struct dom_sid sid2;
	NTSTATUS status;

	config_dn = ldb_get_config_basedn(ldb);

	ret = dsdb_search_one(ldb, tmp_ctx, &msg, config_dn, LDB_SCOPE_SUBTREE,
			      attrs1, 0, "(&(objectGUID=%s)(objectClass=nTDSDSA))", GUID_string(tmp_ctx, dsa_guid));
	if (ret != LDB_SUCCESS) {
		DEBUG(1,(__location__ ": Failed to find DSA objectGUID %s for sid %s\n",
			 GUID_string(tmp_ctx, dsa_guid), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}
	dn = msg->dn;

	if (!ldb_dn_remove_child_components(dn, 1)) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	ret = dsdb_search_one(ldb, tmp_ctx, &msg, dn, LDB_SCOPE_BASE,
			      attrs2, DSDB_SEARCH_SHOW_EXTENDED_DN,
			      "(objectClass=server)");
	if (ret != LDB_SUCCESS) {
		DEBUG(1,(__location__ ": Failed to find server record for DSA with objectGUID %s, sid %s\n",
			 GUID_string(tmp_ctx, dsa_guid), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	account_dn = ldb_msg_find_attr_as_dn(ldb, tmp_ctx, msg, "serverReference");
	if (account_dn == NULL) {
		DEBUG(1,(__location__ ": Failed to find account_dn for DSA with objectGUID %s, sid %s\n",
			 GUID_string(tmp_ctx, dsa_guid), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	status = dsdb_get_extended_dn_sid(account_dn, &sid2, "SID");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,(__location__ ": Failed to find SID for DSA with objectGUID %s, sid %s\n",
			 GUID_string(tmp_ctx, dsa_guid), dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	if (!dom_sid_equal(sid, &sid2)) {
		/* someone is trying to spoof another account */
		DEBUG(0,(__location__ ": Bad DSA objectGUID %s for sid %s - expected sid %s\n",
			 GUID_string(tmp_ctx, dsa_guid),
			 dom_sid_string(tmp_ctx, sid),
			 dom_sid_string(tmp_ctx, &sid2)));
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

static const char * const secret_attributes[] = {
	DSDB_SECRET_ATTRIBUTES,
	NULL
};

/*
  check if the attribute belongs to the RODC filtered attribute set
  Note that attributes that are in the filtered attribute set are the
  ones that _are_ always sent to a RODC
*/
bool dsdb_attr_in_rodc_fas(const struct dsdb_attribute *sa)
{
	/* they never get secret attributes */
	if (is_attr_in_list(secret_attributes, sa->lDAPDisplayName)) {
		return false;
	}

	/* they do get non-secret critical attributes */
	if (sa->schemaFlagsEx & SCHEMA_FLAG_ATTR_IS_CRITICAL) {
		return true;
	}

	/* they do get non-secret attributes marked as being in the FAS  */
	if (sa->searchFlags & SEARCH_FLAG_RODC_ATTRIBUTE) {
		return true;
	}

	/* other attributes are denied */
	return false;
}

/* return fsmo role dn and role owner dn for a particular role*/
WERROR dsdb_get_fsmo_role_info(TALLOC_CTX *tmp_ctx,
			       struct ldb_context *ldb,
			       uint32_t role,
			       struct ldb_dn **fsmo_role_dn,
			       struct ldb_dn **role_owner_dn)
{
	int ret;
	switch (role) {
	case DREPL_NAMING_MASTER:
		*fsmo_role_dn = samdb_partitions_dn(ldb, tmp_ctx);
		ret = samdb_reference_dn(ldb, tmp_ctx, *fsmo_role_dn, "fSMORoleOwner", role_owner_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in Naming Master object - %s",
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		break;
	case DREPL_INFRASTRUCTURE_MASTER:
		*fsmo_role_dn = samdb_infrastructure_dn(ldb, tmp_ctx);
		ret = samdb_reference_dn(ldb, tmp_ctx, *fsmo_role_dn, "fSMORoleOwner", role_owner_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in Schema Master object - %s",
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		break;
	case DREPL_RID_MASTER:
		ret = samdb_rid_manager_dn(ldb, tmp_ctx, fsmo_role_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0, (__location__ ": Failed to find RID Manager object - %s", ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}

		ret = samdb_reference_dn(ldb, tmp_ctx, *fsmo_role_dn, "fSMORoleOwner", role_owner_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in RID Manager object - %s",
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		break;
	case DREPL_SCHEMA_MASTER:
		*fsmo_role_dn = ldb_get_schema_basedn(ldb);
		ret = samdb_reference_dn(ldb, tmp_ctx, *fsmo_role_dn, "fSMORoleOwner", role_owner_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in Schema Master object - %s",
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		break;
	case DREPL_PDC_MASTER:
		*fsmo_role_dn = ldb_get_default_basedn(ldb);
		ret = samdb_reference_dn(ldb, tmp_ctx, *fsmo_role_dn, "fSMORoleOwner", role_owner_dn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in Pd Master object - %s",
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return WERR_DS_DRA_INTERNAL_ERROR;
		}
		break;
	default:
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	return WERR_OK;
}

const char *samdb_dn_to_dnshostname(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_dn *server_dn)
{
	int ldb_ret;
	struct ldb_result *res = NULL;
	const char * const attrs[] = { "dNSHostName", NULL};

	ldb_ret = ldb_search(ldb, mem_ctx, &res,
			     server_dn,
			     LDB_SCOPE_BASE,
			     attrs, NULL);
	if (ldb_ret != LDB_SUCCESS) {
		DEBUG(4, ("Failed to find dNSHostName for dn %s, ldb error: %s",
			  ldb_dn_get_linearized(server_dn), ldb_errstring(ldb)));
		return NULL;
	}

	return ldb_msg_find_attr_as_string(res->msgs[0], "dNSHostName", NULL);
}

/*
  returns true if an attribute is in the filter,
  false otherwise, provided that attribute value is provided with the expression
*/
bool dsdb_attr_in_parse_tree(struct ldb_parse_tree *tree,
			     const char *attr)
{
       unsigned int i;
       switch (tree->operation) {
       case LDB_OP_AND:
       case LDB_OP_OR:
               for (i=0;i<tree->u.list.num_elements;i++) {
                       if (dsdb_attr_in_parse_tree(tree->u.list.elements[i],
                                                       attr))
                               return true;
               }
               return false;
       case LDB_OP_NOT:
               return dsdb_attr_in_parse_tree(tree->u.isnot.child, attr);
       case LDB_OP_EQUALITY:
       case LDB_OP_GREATER:
       case LDB_OP_LESS:
       case LDB_OP_APPROX:
               if (ldb_attr_cmp(tree->u.equality.attr, attr) == 0) {
                       return true;
               }
               return false;
       case LDB_OP_SUBSTRING:
               if (ldb_attr_cmp(tree->u.substring.attr, attr) == 0) {
                       return true;
               }
               return false;
       case LDB_OP_PRESENT:
	       /* (attrname=*) is not filtered out */
               return false;
       case LDB_OP_EXTENDED:
               if (tree->u.extended.attr &&
                   ldb_attr_cmp(tree->u.extended.attr, attr) == 0) {
		       return true;
               }
               return false;
       }
       return false;
}

bool is_attr_in_list(const char * const * attrs, const char *attr)
{
	unsigned int i;

	for (i = 0; attrs[i]; i++) {
		if (ldb_attr_cmp(attrs[i], attr) == 0)
			return true;
	}

	return false;
}

