/*
   Unix SMB/CIFS implementation.
   test suite for winreg rpc operations

   Copyright (C) Tim Potter 2003
   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Günther Deschner 2007,2010

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
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"
#include "lib/registry/registry.h"

#define TEST_KEY_BASE "winreg_torture_test"
#define TEST_KEY1 "spottyfoot"
#define TEST_KEY2 "with a SD (#1)"
#define TEST_KEY3 "with a subkey"
#define TEST_KEY4 "sd_tests"
#define TEST_SUBKEY "subkey"
#define TEST_SUBKEY_SD  "subkey_sd"
#define TEST_SUBSUBKEY_SD "subkey_sd\\subsubkey_sd"
#define TEST_VALUE "torture_value_name"
#define TEST_KEY_VOLATILE "torture_volatile_key"
#define TEST_SUBKEY_VOLATILE "torture_volatile_subkey"
#define TEST_KEY_SYMLINK "torture_symlink_key"
#define TEST_KEY_SYMLINK_DEST "torture_symlink_dest"

#define TEST_SID "S-1-5-21-1234567890-1234567890-1234567890-500"

static void init_lsa_StringLarge(struct lsa_StringLarge *name, const char *s)
{
	name->string = s;
}

static void init_winreg_String(struct winreg_String *name, const char *s)
{
	name->name = s;
	if (s) {
		name->name_len = 2 * (strlen_m(s) + 1);
		name->name_size = name->name_len;
	} else {
		name->name_len = 0;
		name->name_size = 0;
	}
}

static bool test_GetVersion(struct dcerpc_binding_handle *b,
			    struct torture_context *tctx,
			    struct policy_handle *handle)
{
	struct winreg_GetVersion r;
	uint32_t v;

	torture_comment(tctx, "Testing GetVersion\n");

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.out.version = &v;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_GetVersion_r(b, tctx, &r),
				   "GetVersion failed");

	torture_assert_werr_ok(tctx, r.out.result, "GetVersion failed");

	return true;
}

static bool test_NotifyChangeKeyValue(struct dcerpc_binding_handle *b,
				      struct torture_context *tctx,
				      struct policy_handle *handle)
{
	struct winreg_NotifyChangeKeyValue r;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.watch_subtree = true;
	r.in.notify_filter = 0;
	r.in.unknown = r.in.unknown2 = 0;
	init_winreg_String(&r.in.string1, NULL);
	init_winreg_String(&r.in.string2, NULL);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_NotifyChangeKeyValue_r(b, tctx, &r),
				   "NotifyChangeKeyValue failed");

	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_comment(tctx,
				"NotifyChangeKeyValue failed - %s - not considering\n",
				win_errstr(r.out.result));
		return true;
	}

	return true;
}

static bool test_CreateKey_opts(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle,
				const char *name,
				const char *kclass,
				uint32_t options,
				uint32_t access_mask,
				struct winreg_SecBuf *secdesc,
				WERROR expected_result,
				enum winreg_CreateAction *action_taken_p,
				struct policy_handle *new_handle_p)
{
	struct winreg_CreateKey r;
	struct policy_handle newhandle;
	enum winreg_CreateAction action_taken = 0;

	torture_comment(tctx, "Testing CreateKey(%s)\n", name);

	ZERO_STRUCT(r);
	r.in.handle = handle;
	init_winreg_String(&r.in.name, name);
	init_winreg_String(&r.in.keyclass, kclass);
	r.in.options = options;
	r.in.access_mask = access_mask;
	r.in.action_taken = &action_taken;
	r.in.secdesc = secdesc;
	r.out.new_handle = &newhandle;
	r.out.action_taken = &action_taken;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CreateKey_r(b, tctx, &r),
				   "CreateKey failed");

	torture_assert_werr_equal(tctx, r.out.result, expected_result, "CreateKey failed");

	if (new_handle_p) {
		*new_handle_p = newhandle;
	}
	if (action_taken_p) {
		*action_taken_p = *r.out.action_taken;
	}

	return true;
}

static bool test_CreateKey(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *handle, const char *name,
			   const char *kclass)
{
	return test_CreateKey_opts(tctx, b, handle, name, kclass,
				   REG_OPTION_NON_VOLATILE,
				   SEC_FLAG_MAXIMUM_ALLOWED,
				   NULL, /* secdesc */
				   WERR_OK,
				   NULL, /* action_taken */
				   NULL /* new_handle */);
}

/*
  createkey testing with a SD
*/
static bool test_CreateKey_sd(struct dcerpc_binding_handle *b,
			      struct torture_context *tctx,
			      struct policy_handle *handle, const char *name,
			      const char *kclass,
			      struct policy_handle *newhandle)
{
	struct winreg_CreateKey r;
	enum winreg_CreateAction action_taken = 0;
	struct security_descriptor *sd;
	DATA_BLOB sdblob;
	struct winreg_SecBuf secbuf;

	sd = security_descriptor_dacl_create(tctx,
					0,
					NULL, NULL,
					SID_NT_AUTHENTICATED_USERS,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_GENERIC_ALL,
					SEC_ACE_FLAG_OBJECT_INHERIT |
					SEC_ACE_FLAG_CONTAINER_INHERIT,
					NULL);

	torture_assert_ndr_success(tctx,
		ndr_push_struct_blob(&sdblob, tctx, sd,
				     (ndr_push_flags_fn_t)ndr_push_security_descriptor),
				     "Failed to push security_descriptor ?!\n");

	secbuf.sd.data = sdblob.data;
	secbuf.sd.len = sdblob.length;
	secbuf.sd.size = sdblob.length;
	secbuf.length = sdblob.length-10;
	secbuf.inherit = 0;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.out.new_handle = newhandle;
	init_winreg_String(&r.in.name, name);
	init_winreg_String(&r.in.keyclass, kclass);
	r.in.options = 0x0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.action_taken = r.out.action_taken = &action_taken;
	r.in.secdesc = &secbuf;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CreateKey_r(b, tctx, &r),
				   "CreateKey with sd failed");

	torture_assert_werr_ok(tctx, r.out.result, "CreateKey with sd failed");

	return true;
}

static bool _test_GetKeySecurity(struct dcerpc_pipe *p,
				 struct torture_context *tctx,
				 struct policy_handle *handle,
				 uint32_t *sec_info_ptr,
				 WERROR get_werr,
				 struct security_descriptor **sd_out)
{
	struct winreg_GetKeySecurity r;
	struct security_descriptor *sd = NULL;
	uint32_t sec_info;
	DATA_BLOB sdblob;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (sec_info_ptr) {
		sec_info = *sec_info_ptr;
	} else {
		sec_info = SECINFO_OWNER | SECINFO_GROUP | SECINFO_DACL;
	}

	ZERO_STRUCT(r);

	r.in.handle = handle;
	r.in.sec_info = sec_info;
	r.in.sd = r.out.sd = talloc_zero(tctx, struct KeySecurityData);
	r.in.sd->size = 0x1000;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_GetKeySecurity_r(b, tctx, &r),
				   "GetKeySecurity failed");

	torture_assert_werr_equal(tctx, r.out.result, get_werr,
				  "GetKeySecurity failed");

	sdblob.data = r.out.sd->data;
	sdblob.length = r.out.sd->len;

	sd = talloc_zero(tctx, struct security_descriptor);

	torture_assert_ndr_success(tctx,
		ndr_pull_struct_blob(&sdblob, tctx, sd,
				     (ndr_pull_flags_fn_t)ndr_pull_security_descriptor),
				     "pull_security_descriptor failed");

	if (p->conn->flags & DCERPC_DEBUG_PRINT_OUT) {
		NDR_PRINT_DEBUG(security_descriptor, sd);
	}

	if (sd_out) {
		*sd_out = sd;
	} else {
		talloc_free(sd);
	}

	return true;
}

static bool test_GetKeySecurity(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				struct policy_handle *handle,
				struct security_descriptor **sd_out)
{
	return _test_GetKeySecurity(p, tctx, handle, NULL, WERR_OK, sd_out);
}

static bool _test_SetKeySecurity(struct dcerpc_pipe *p,
				 struct torture_context *tctx,
				 struct policy_handle *handle,
				 uint32_t *sec_info_ptr,
				 struct security_descriptor *sd,
				 WERROR werr)
{
	struct winreg_SetKeySecurity r;
	struct KeySecurityData *sdata = NULL;
	DATA_BLOB sdblob;
	uint32_t sec_info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r);

	if (sd && (p->conn->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_DEBUG(security_descriptor, sd);
	}

	torture_assert_ndr_success(tctx,
		ndr_push_struct_blob(&sdblob, tctx, sd,
				     (ndr_push_flags_fn_t)ndr_push_security_descriptor),
				     "push_security_descriptor failed");

	sdata = talloc_zero(tctx, struct KeySecurityData);
	sdata->data = sdblob.data;
	sdata->size = sdblob.length;
	sdata->len = sdblob.length;

	if (sec_info_ptr) {
		sec_info = *sec_info_ptr;
	} else {
		sec_info = SECINFO_UNPROTECTED_SACL |
			   SECINFO_UNPROTECTED_DACL;
		if (sd->owner_sid) {
			sec_info |= SECINFO_OWNER;
		}
		if (sd->group_sid) {
			sec_info |= SECINFO_GROUP;
		}
		if (sd->sacl) {
			sec_info |= SECINFO_SACL;
		}
		if (sd->dacl) {
			sec_info |= SECINFO_DACL;
		}
	}

	r.in.handle = handle;
	r.in.sec_info = sec_info;
	r.in.sd = sdata;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_SetKeySecurity_r(b, tctx, &r),
				   "SetKeySecurity failed");

	torture_assert_werr_equal(tctx, r.out.result, werr,
				  "SetKeySecurity failed");

	return true;
}

static bool test_SetKeySecurity(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				struct policy_handle *handle,
				struct security_descriptor *sd)
{
	return _test_SetKeySecurity(p, tctx, handle, NULL, sd, WERR_OK);
}

static bool test_CloseKey(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct policy_handle *handle)
{
	struct winreg_CloseKey r;

	ZERO_STRUCT(r);
	r.in.handle = r.out.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CloseKey_r(b, tctx, &r),
				   "CloseKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "CloseKey failed");

	return true;
}

static bool test_FlushKey(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct policy_handle *handle)
{
	struct winreg_FlushKey r;

	ZERO_STRUCT(r);
	r.in.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_FlushKey_r(b, tctx, &r),
				   "FlushKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "FlushKey failed");

	return true;
}

static bool test_OpenKey_opts(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *hive_handle,
			      const char *keyname,
			      uint32_t options,
			      uint32_t access_mask,
			      struct policy_handle *key_handle,
			      WERROR expected_result)
{
	struct winreg_OpenKey r;

	ZERO_STRUCT(r);
	r.in.parent_handle = hive_handle;
	init_winreg_String(&r.in.keyname, keyname);
	r.in.options = options;
	r.in.access_mask = access_mask;
	r.out.handle = key_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_OpenKey_r(b, tctx, &r),
				   "OpenKey failed");

	torture_assert_werr_equal(tctx, r.out.result, expected_result,
				  "OpenKey failed");

	return true;
}

static bool test_OpenKey(struct dcerpc_binding_handle *b,
			 struct torture_context *tctx,
			 struct policy_handle *hive_handle,
			 const char *keyname, struct policy_handle *key_handle)
{
	return test_OpenKey_opts(tctx, b, hive_handle, keyname,
				 REG_OPTION_NON_VOLATILE,
				 SEC_FLAG_MAXIMUM_ALLOWED,
				 key_handle,
				 WERR_OK);
}

static bool test_Cleanup(struct dcerpc_binding_handle *b,
			 struct torture_context *tctx,
			 struct policy_handle *handle, const char *key)
{
	struct winreg_DeleteKey r;

	ZERO_STRUCT(r);
	r.in.handle = handle;

	init_winreg_String(&r.in.key, key);
	dcerpc_winreg_DeleteKey_r(b, tctx, &r);

	return true;
}

static bool _test_GetSetSecurityDescriptor(struct dcerpc_pipe *p,
					   struct torture_context *tctx,
					   struct policy_handle *handle,
					   WERROR get_werr,
					   WERROR set_werr)
{
	struct security_descriptor *sd = NULL;

	if (!_test_GetKeySecurity(p, tctx, handle, NULL, get_werr, &sd)) {
		return false;
	}

	if (!_test_SetKeySecurity(p, tctx, handle, NULL, sd, set_werr)) {
		return false;
	}

	return true;
}

static bool test_SecurityDescriptor(struct dcerpc_pipe *p,
				    struct torture_context *tctx,
				    struct policy_handle *handle,
				    const char *key)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "SecurityDescriptor get & set\n");

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!_test_GetSetSecurityDescriptor(p, tctx, &new_handle,
					    WERR_OK, WERR_OK)) {
		ret = false;
	}

	if (!test_CloseKey(b, tctx, &new_handle)) {
		return false;
	}

	return ret;
}

static bool _test_SecurityDescriptor(struct dcerpc_pipe *p,
				     struct torture_context *tctx,
				     struct policy_handle *handle,
				     uint32_t access_mask,
				     const char *key,
				     WERROR open_werr,
				     WERROR get_werr,
				     WERROR set_werr)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, key,
				  REG_OPTION_NON_VOLATILE,
				  access_mask,
				  &new_handle,
				  open_werr),
		"failed to open key");

	if (!W_ERROR_IS_OK(open_werr)) {
		return true;
	}

	if (!_test_GetSetSecurityDescriptor(p, tctx, &new_handle,
					    get_werr, set_werr)) {
		ret = false;
	}

	if (!test_CloseKey(b, tctx, &new_handle)) {
		return false;
	}

	return ret;
}

static bool test_dacl_trustee_present(struct dcerpc_pipe *p,
				      struct torture_context *tctx,
				      struct policy_handle *handle,
				      const struct dom_sid *sid)
{
	struct security_descriptor *sd = NULL;
	int i;

	if (!test_GetKeySecurity(p, tctx, handle, &sd)) {
		return false;
	}

	if (!sd || !sd->dacl) {
		return false;
	}

	for (i = 0; i < sd->dacl->num_aces; i++) {
		if (dom_sid_equal(&sd->dacl->aces[i].trustee, sid)) {
			return true;
		}
	}

	return false;
}

static bool _test_dacl_trustee_present(struct dcerpc_pipe *p,
				       struct torture_context *tctx,
				       struct policy_handle *handle,
				       const char *key,
				       const struct dom_sid *sid)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_dacl_trustee_present(p, tctx, &new_handle, sid);

	test_CloseKey(b, tctx, &new_handle);

	return ret;
}

static bool test_sacl_trustee_present(struct dcerpc_pipe *p,
				      struct torture_context *tctx,
				      struct policy_handle *handle,
				      const struct dom_sid *sid)
{
	struct security_descriptor *sd = NULL;
	int i;
	uint32_t sec_info = SECINFO_SACL;

	if (!_test_GetKeySecurity(p, tctx, handle, &sec_info, WERR_OK, &sd)) {
		return false;
	}

	if (!sd || !sd->sacl) {
		return false;
	}

	for (i = 0; i < sd->sacl->num_aces; i++) {
		if (dom_sid_equal(&sd->sacl->aces[i].trustee, sid)) {
			return true;
		}
	}

	return false;
}

static bool _test_sacl_trustee_present(struct dcerpc_pipe *p,
				       struct torture_context *tctx,
				       struct policy_handle *handle,
				       const char *key,
				       const struct dom_sid *sid)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, key,
				  REG_OPTION_NON_VOLATILE,
				  SEC_FLAG_SYSTEM_SECURITY,
				  &new_handle,
				  WERR_OK),
		"failed to open key");

	ret = test_sacl_trustee_present(p, tctx, &new_handle, sid);

	test_CloseKey(b, tctx, &new_handle);

	return ret;
}

static bool test_owner_present(struct dcerpc_pipe *p,
			       struct torture_context *tctx,
			       struct policy_handle *handle,
			       const struct dom_sid *sid)
{
	struct security_descriptor *sd = NULL;
	uint32_t sec_info = SECINFO_OWNER;

	if (!_test_GetKeySecurity(p, tctx, handle, &sec_info, WERR_OK, &sd)) {
		return false;
	}

	if (!sd || !sd->owner_sid) {
		return false;
	}

	return dom_sid_equal(sd->owner_sid, sid);
}

static bool _test_owner_present(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				struct policy_handle *handle,
				const char *key,
				const struct dom_sid *sid)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_owner_present(p, tctx, &new_handle, sid);

	test_CloseKey(b, tctx, &new_handle);

	return ret;
}

static bool test_group_present(struct dcerpc_pipe *p,
			       struct torture_context *tctx,
			       struct policy_handle *handle,
			       const struct dom_sid *sid)
{
	struct security_descriptor *sd = NULL;
	uint32_t sec_info = SECINFO_GROUP;

	if (!_test_GetKeySecurity(p, tctx, handle, &sec_info, WERR_OK, &sd)) {
		return false;
	}

	if (!sd || !sd->group_sid) {
		return false;
	}

	return dom_sid_equal(sd->group_sid, sid);
}

static bool _test_group_present(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				struct policy_handle *handle,
				const char *key,
				const struct dom_sid *sid)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_group_present(p, tctx, &new_handle, sid);

	test_CloseKey(b, tctx, &new_handle);

	return ret;
}

static bool test_dacl_trustee_flags_present(struct dcerpc_pipe *p,
					    struct torture_context *tctx,
					    struct policy_handle *handle,
					    const struct dom_sid *sid,
					    uint8_t flags)
{
	struct security_descriptor *sd = NULL;
	int i;

	if (!test_GetKeySecurity(p, tctx, handle, &sd)) {
		return false;
	}

	if (!sd || !sd->dacl) {
		return false;
	}

	for (i = 0; i < sd->dacl->num_aces; i++) {
		if ((dom_sid_equal(&sd->dacl->aces[i].trustee, sid)) &&
		    (sd->dacl->aces[i].flags == flags)) {
			return true;
		}
	}

	return false;
}

static bool test_dacl_ace_present(struct dcerpc_pipe *p,
				  struct torture_context *tctx,
				  struct policy_handle *handle,
				  const struct security_ace *ace)
{
	struct security_descriptor *sd = NULL;
	int i;

	if (!test_GetKeySecurity(p, tctx, handle, &sd)) {
		return false;
	}

	if (!sd || !sd->dacl) {
		return false;
	}

	for (i = 0; i < sd->dacl->num_aces; i++) {
		if (security_ace_equal(&sd->dacl->aces[i], ace)) {
			return true;
		}
	}

	return false;
}

static bool test_RestoreSecurity(struct dcerpc_pipe *p,
				 struct torture_context *tctx,
				 struct policy_handle *handle,
				 const char *key,
				 struct security_descriptor *sd)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!test_SetKeySecurity(p, tctx, &new_handle, sd)) {
		ret = false;
	}

	if (!test_CloseKey(b, tctx, &new_handle)) {
		ret = false;
	}

	return ret;
}

static bool test_BackupSecurity(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				struct policy_handle *handle,
				const char *key,
				struct security_descriptor **sd)
{
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!test_GetKeySecurity(p, tctx, &new_handle, sd)) {
		ret = false;
	}

	if (!test_CloseKey(b, tctx, &new_handle)) {
		ret = false;
	}

	return ret;
}

static bool test_SecurityDescriptorInheritance(struct dcerpc_pipe *p,
					       struct torture_context *tctx,
					       struct policy_handle *handle,
					       const char *key)
{
	/* get sd
	   add ace SEC_ACE_FLAG_CONTAINER_INHERIT
	   set sd
	   get sd
	   check ace
	   add subkey
	   get sd
	   check ace
	   add subsubkey
	   get sd
	   check ace
	   del subsubkey
	   del subkey
	   reset sd
	*/

	struct security_descriptor *sd = NULL;
	struct security_descriptor *sd_orig = NULL;
	struct security_ace *ace = NULL;
	struct policy_handle new_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *test_subkey_sd;
	const char *test_subsubkey_sd;

	torture_comment(tctx, "SecurityDescriptor inheritance\n");

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!_test_GetKeySecurity(p, tctx, &new_handle, NULL, WERR_OK, &sd)) {
		return false;
	}

	sd_orig = security_descriptor_copy(tctx, sd);
	if (sd_orig == NULL) {
		return false;
	}

	ace = security_ace_create(tctx,
				  TEST_SID,
				  SEC_ACE_TYPE_ACCESS_ALLOWED,
				  SEC_STD_REQUIRED,
				  SEC_ACE_FLAG_CONTAINER_INHERIT);

	torture_assert_ntstatus_ok(tctx,
		security_descriptor_dacl_add(sd, ace),
		"failed to add ace");

	/* FIXME: add further tests for these flags */
	sd->type |= SEC_DESC_DACL_AUTO_INHERIT_REQ |
		    SEC_DESC_SACL_AUTO_INHERITED;

	if (!test_SetKeySecurity(p, tctx, &new_handle, sd)) {
		return false;
	}

	torture_assert(tctx,
		test_dacl_ace_present(p, tctx, &new_handle, ace),
		"new ACE not present!");

	if (!test_CloseKey(b, tctx, &new_handle)) {
		return false;
	}

	test_subkey_sd = talloc_asprintf(tctx, "%s\\%s", key, TEST_SUBKEY_SD);

	if (!test_CreateKey(b, tctx, handle, test_subkey_sd, NULL)) {
		ret = false;
		goto out;
	}

	if (!test_OpenKey(b, tctx, handle, test_subkey_sd, &new_handle)) {
		ret = false;
		goto out;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		torture_comment(tctx, "inherited ACE not present!\n");
		ret = false;
		goto out;
	}

	test_subsubkey_sd = talloc_asprintf(tctx, "%s\\%s", key, TEST_SUBSUBKEY_SD);

	test_CloseKey(b, tctx, &new_handle);
	if (!test_CreateKey(b, tctx, handle, test_subsubkey_sd, NULL)) {
		ret = false;
		goto out;
	}

	if (!test_OpenKey(b, tctx, handle, test_subsubkey_sd, &new_handle)) {
		ret = false;
		goto out;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		torture_comment(tctx, "inherited ACE not present!\n");
		ret = false;
		goto out;
	}

 out:
	test_CloseKey(b, tctx, &new_handle);
	test_Cleanup(b, tctx, handle, test_subkey_sd);
	test_RestoreSecurity(p, tctx, handle, key, sd_orig);

	return ret;
}

static bool test_SecurityDescriptorBlockInheritance(struct dcerpc_pipe *p,
						    struct torture_context *tctx,
						    struct policy_handle *handle,
						    const char *key)
{
	/* get sd
	   add ace SEC_ACE_FLAG_NO_PROPAGATE_INHERIT
	   set sd
	   add subkey/subkey
	   get sd
	   check ace
	   get sd from subkey
	   check ace
	   del subkey/subkey
	   del subkey
	   reset sd
	*/

	struct security_descriptor *sd = NULL;
	struct security_descriptor *sd_orig = NULL;
	struct security_ace *ace = NULL;
	struct policy_handle new_handle;
	struct dom_sid *sid = NULL;
	bool ret = true;
	uint8_t ace_flags = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *test_subkey_sd;
	const char *test_subsubkey_sd;

	torture_comment(tctx, "SecurityDescriptor inheritance block\n");

	if (!test_OpenKey(b, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!_test_GetKeySecurity(p, tctx, &new_handle, NULL, WERR_OK, &sd)) {
		return false;
	}

	sd_orig = security_descriptor_copy(tctx, sd);
	if (sd_orig == NULL) {
		return false;
	}

	ace = security_ace_create(tctx,
				  TEST_SID,
				  SEC_ACE_TYPE_ACCESS_ALLOWED,
				  SEC_STD_REQUIRED,
				  SEC_ACE_FLAG_CONTAINER_INHERIT |
				  SEC_ACE_FLAG_NO_PROPAGATE_INHERIT);

	torture_assert_ntstatus_ok(tctx,
		security_descriptor_dacl_add(sd, ace),
		"failed to add ace");

	if (!_test_SetKeySecurity(p, tctx, &new_handle, NULL, sd, WERR_OK)) {
		return false;
	}

	torture_assert(tctx,
		test_dacl_ace_present(p, tctx, &new_handle, ace),
		"new ACE not present!");

	if (!test_CloseKey(b, tctx, &new_handle)) {
		return false;
	}

	test_subkey_sd = talloc_asprintf(tctx, "%s\\%s", key, TEST_SUBKEY_SD);
	test_subsubkey_sd = talloc_asprintf(tctx, "%s\\%s", key, TEST_SUBSUBKEY_SD);

	if (!test_CreateKey(b, tctx, handle, test_subsubkey_sd, NULL)) {
		return false;
	}

	if (!test_OpenKey(b, tctx, handle, test_subsubkey_sd, &new_handle)) {
		ret = false;
		goto out;
	}

	if (test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		torture_comment(tctx, "inherited ACE present but should not!\n");
		ret = false;
		goto out;
	}

	sid = dom_sid_parse_talloc(tctx, TEST_SID);
	if (sid == NULL) {
		return false;
	}

	if (test_dacl_trustee_present(p, tctx, &new_handle, sid)) {
		torture_comment(tctx, "inherited trustee SID present but should not!\n");
		ret = false;
		goto out;
	}

	test_CloseKey(b, tctx, &new_handle);

	if (!test_OpenKey(b, tctx, handle, test_subkey_sd, &new_handle)) {
		ret = false;
		goto out;
	}

	if (test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		torture_comment(tctx, "inherited ACE present but should not!\n");
		ret = false;
		goto out;
	}

	if (!test_dacl_trustee_flags_present(p, tctx, &new_handle, sid, ace_flags)) {
		torture_comment(tctx, "inherited trustee SID with flags 0x%02x not present!\n",
			ace_flags);
		ret = false;
		goto out;
	}

 out:
	test_CloseKey(b, tctx, &new_handle);
	test_Cleanup(b, tctx, handle, test_subkey_sd);
	test_RestoreSecurity(p, tctx, handle, key, sd_orig);

	return ret;
}

static bool test_SecurityDescriptorsMasks(struct dcerpc_pipe *p,
					  struct torture_context *tctx,
					  struct policy_handle *handle,
					  const char *key)
{
	bool ret = true;
	int i;

	struct winreg_mask_result_table {
		uint32_t access_mask;
		WERROR open_werr;
		WERROR get_werr;
		WERROR set_werr;
	} sd_mask_tests[] = {
		{ 0,
			WERR_ACCESS_DENIED, WERR_BADFILE, WERR_FOOBAR },
		{ SEC_FLAG_MAXIMUM_ALLOWED,
			WERR_OK, WERR_OK, WERR_OK },
		{ SEC_STD_WRITE_DAC,
			WERR_OK, WERR_ACCESS_DENIED, WERR_FOOBAR },
		{ SEC_FLAG_SYSTEM_SECURITY,
			WERR_OK, WERR_ACCESS_DENIED, WERR_FOOBAR }
	};

	/* FIXME: before this test can ever run successfully we need a way to
	 * correctly read a NULL security_descritpor in ndr, get the required
	 * length, requery, etc.
	 */

	return true;

	for (i=0; i < ARRAY_SIZE(sd_mask_tests); i++) {

		torture_comment(tctx,
				"SecurityDescriptor get & set with access_mask: 0x%08x\n",
				sd_mask_tests[i].access_mask);
		torture_comment(tctx,
				"expecting: open %s, get: %s, set: %s\n",
				win_errstr(sd_mask_tests[i].open_werr),
				win_errstr(sd_mask_tests[i].get_werr),
				win_errstr(sd_mask_tests[i].set_werr));

		if (_test_SecurityDescriptor(p, tctx, handle,
					     sd_mask_tests[i].access_mask, key,
					     sd_mask_tests[i].open_werr,
					     sd_mask_tests[i].get_werr,
					     sd_mask_tests[i].set_werr)) {
			ret = false;
		}
	}

	return ret;
}

typedef bool (*secinfo_verify_fn)(struct dcerpc_pipe *,
				  struct torture_context *,
				  struct policy_handle *,
				  const char *,
				  const struct dom_sid *);

static bool test_SetSecurityDescriptor_SecInfo(struct dcerpc_pipe *p,
					       struct torture_context *tctx,
					       struct policy_handle *handle,
					       const char *key,
					       const char *test,
					       uint32_t access_mask,
					       uint32_t sec_info,
					       struct security_descriptor *sd,
					       WERROR set_werr,
					       bool expect_present,
					       bool (*fn) (struct dcerpc_pipe *,
							   struct torture_context *,
							   struct policy_handle *,
							   const char *,
							   const struct dom_sid *),
					       const struct dom_sid *sid)
{
	struct policy_handle new_handle;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "SecurityDescriptor (%s) sets for secinfo: "
			"0x%08x, access_mask: 0x%08x\n",
			test, sec_info, access_mask);

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, key,
				  REG_OPTION_NON_VOLATILE,
				  access_mask,
				  &new_handle,
				  WERR_OK),
		"failed to open key");

	if (!_test_SetKeySecurity(p, tctx, &new_handle, &sec_info,
				  sd,
				  set_werr)) {
		torture_warning(tctx,
				"SetKeySecurity with secinfo: 0x%08x has failed\n",
				sec_info);
		smb_panic("");
		test_CloseKey(b, tctx, &new_handle);
		return false;
	}

	test_CloseKey(b, tctx, &new_handle);

	if (W_ERROR_IS_OK(set_werr)) {
		bool present;
		present = fn(p, tctx, handle, key, sid);
		if ((expect_present) && (!present)) {
			torture_warning(tctx,
					"%s sid is not present!\n",
					test);
			return false;
		}
		if ((!expect_present) && (present)) {
			torture_warning(tctx,
					"%s sid is present but not expected!\n",
					test);
			return false;
		}
	}

	return true;
}

static bool test_SecurityDescriptorsSecInfo(struct dcerpc_pipe *p,
					    struct torture_context *tctx,
					    struct policy_handle *handle,
					    const char *key)
{
	struct security_descriptor *sd_orig = NULL;
	struct dom_sid *sid = NULL;
	bool ret = true;
	int i, a;

	struct security_descriptor *sd_owner =
		security_descriptor_dacl_create(tctx,
						0,
						TEST_SID, NULL, NULL);

	struct security_descriptor *sd_group =
		security_descriptor_dacl_create(tctx,
						0,
						NULL, TEST_SID, NULL);

	struct security_descriptor *sd_dacl =
		security_descriptor_dacl_create(tctx,
						0,
						NULL, NULL,
						TEST_SID,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						SEC_GENERIC_ALL,
						0,
						SID_NT_AUTHENTICATED_USERS,
						SEC_ACE_TYPE_ACCESS_ALLOWED,
						SEC_GENERIC_ALL,
						0,
						NULL);

	struct security_descriptor *sd_sacl =
		security_descriptor_sacl_create(tctx,
						0,
						NULL, NULL,
						TEST_SID,
						SEC_ACE_TYPE_SYSTEM_AUDIT,
						SEC_GENERIC_ALL,
						SEC_ACE_FLAG_SUCCESSFUL_ACCESS,
						NULL);

	struct winreg_secinfo_table {
		struct security_descriptor *sd;
		uint32_t sec_info;
		WERROR set_werr;
		bool sid_present;
		secinfo_verify_fn fn;
	};

	struct winreg_secinfo_table sec_info_owner_tests[] = {
		{ sd_owner, 0, WERR_OK,
			false, (secinfo_verify_fn)_test_owner_present },
		{ sd_owner, SECINFO_OWNER, WERR_OK,
			true, (secinfo_verify_fn)_test_owner_present },
		{ sd_owner, SECINFO_GROUP, WERR_INVALID_PARAM },
		{ sd_owner, SECINFO_DACL, WERR_OK,
			true, (secinfo_verify_fn)_test_owner_present },
		{ sd_owner, SECINFO_SACL, WERR_ACCESS_DENIED },
	};

	uint32_t sd_owner_good_access_masks[] = {
		SEC_FLAG_MAXIMUM_ALLOWED,
		/* SEC_STD_WRITE_OWNER, */
	};

	struct winreg_secinfo_table sec_info_group_tests[] = {
		{ sd_group, 0, WERR_OK,
			false, (secinfo_verify_fn)_test_group_present },
		{ sd_group, SECINFO_OWNER, WERR_INVALID_PARAM },
		{ sd_group, SECINFO_GROUP, WERR_OK,
			true, (secinfo_verify_fn)_test_group_present },
		{ sd_group, SECINFO_DACL, WERR_OK,
			true, (secinfo_verify_fn)_test_group_present },
		{ sd_group, SECINFO_SACL, WERR_ACCESS_DENIED },
	};

	uint32_t sd_group_good_access_masks[] = {
		SEC_FLAG_MAXIMUM_ALLOWED,
	};

	struct winreg_secinfo_table sec_info_dacl_tests[] = {
		{ sd_dacl, 0, WERR_OK,
			false, (secinfo_verify_fn)_test_dacl_trustee_present },
		{ sd_dacl, SECINFO_OWNER, WERR_INVALID_PARAM },
		{ sd_dacl, SECINFO_GROUP, WERR_INVALID_PARAM },
		{ sd_dacl, SECINFO_DACL, WERR_OK,
			true, (secinfo_verify_fn)_test_dacl_trustee_present },
		{ sd_dacl, SECINFO_SACL, WERR_ACCESS_DENIED },
	};

	uint32_t sd_dacl_good_access_masks[] = {
		SEC_FLAG_MAXIMUM_ALLOWED,
		SEC_STD_WRITE_DAC,
	};

	struct winreg_secinfo_table sec_info_sacl_tests[] = {
		{ sd_sacl, 0, WERR_OK,
			false, (secinfo_verify_fn)_test_sacl_trustee_present },
		{ sd_sacl, SECINFO_OWNER, WERR_INVALID_PARAM },
		{ sd_sacl, SECINFO_GROUP, WERR_INVALID_PARAM },
		{ sd_sacl, SECINFO_DACL, WERR_OK,
			false, (secinfo_verify_fn)_test_sacl_trustee_present },
		{ sd_sacl, SECINFO_SACL, WERR_OK,
			true, (secinfo_verify_fn)_test_sacl_trustee_present },
	};

	uint32_t sd_sacl_good_access_masks[] = {
		SEC_FLAG_MAXIMUM_ALLOWED | SEC_FLAG_SYSTEM_SECURITY,
		/* SEC_FLAG_SYSTEM_SECURITY, */
	};

	sid = dom_sid_parse_talloc(tctx, TEST_SID);
	if (sid == NULL) {
		return false;
	}

	if (!test_BackupSecurity(p, tctx, handle, key, &sd_orig)) {
		return false;
	}

	/* OWNER */

	for (i=0; i < ARRAY_SIZE(sec_info_owner_tests); i++) {

		for (a=0; a < ARRAY_SIZE(sd_owner_good_access_masks); a++) {

			if (!test_SetSecurityDescriptor_SecInfo(p, tctx, handle,
					key,
					"OWNER",
					sd_owner_good_access_masks[a],
					sec_info_owner_tests[i].sec_info,
					sec_info_owner_tests[i].sd,
					sec_info_owner_tests[i].set_werr,
					sec_info_owner_tests[i].sid_present,
					sec_info_owner_tests[i].fn,
					sid))
			{
				torture_comment(tctx, "test_SetSecurityDescriptor_SecInfo failed for OWNER\n");
				ret = false;
				goto out;
			}
		}
	}

	/* GROUP */

	for (i=0; i < ARRAY_SIZE(sec_info_group_tests); i++) {

		for (a=0; a < ARRAY_SIZE(sd_group_good_access_masks); a++) {

			if (!test_SetSecurityDescriptor_SecInfo(p, tctx, handle,
					key,
					"GROUP",
					sd_group_good_access_masks[a],
					sec_info_group_tests[i].sec_info,
					sec_info_group_tests[i].sd,
					sec_info_group_tests[i].set_werr,
					sec_info_group_tests[i].sid_present,
					sec_info_group_tests[i].fn,
					sid))
			{
				torture_comment(tctx, "test_SetSecurityDescriptor_SecInfo failed for GROUP\n");
				ret = false;
				goto out;
			}
		}
	}

	/* DACL */

	for (i=0; i < ARRAY_SIZE(sec_info_dacl_tests); i++) {

		for (a=0; a < ARRAY_SIZE(sd_dacl_good_access_masks); a++) {

			if (!test_SetSecurityDescriptor_SecInfo(p, tctx, handle,
					key,
					"DACL",
					sd_dacl_good_access_masks[a],
					sec_info_dacl_tests[i].sec_info,
					sec_info_dacl_tests[i].sd,
					sec_info_dacl_tests[i].set_werr,
					sec_info_dacl_tests[i].sid_present,
					sec_info_dacl_tests[i].fn,
					sid))
			{
				torture_comment(tctx, "test_SetSecurityDescriptor_SecInfo failed for DACL\n");
				ret = false;
				goto out;
			}
		}
	}

	/* SACL */

	for (i=0; i < ARRAY_SIZE(sec_info_sacl_tests); i++) {

		for (a=0; a < ARRAY_SIZE(sd_sacl_good_access_masks); a++) {

			if (!test_SetSecurityDescriptor_SecInfo(p, tctx, handle,
					key,
					"SACL",
					sd_sacl_good_access_masks[a],
					sec_info_sacl_tests[i].sec_info,
					sec_info_sacl_tests[i].sd,
					sec_info_sacl_tests[i].set_werr,
					sec_info_sacl_tests[i].sid_present,
					sec_info_sacl_tests[i].fn,
					sid))
			{
				torture_comment(tctx, "test_SetSecurityDescriptor_SecInfo failed for SACL\n");
				ret = false;
				goto out;
			}
		}
	}

 out:
	test_RestoreSecurity(p, tctx, handle, key, sd_orig);

	return ret;
}

static bool test_SecurityDescriptors(struct dcerpc_pipe *p,
				     struct torture_context *tctx,
				     struct policy_handle *handle,
				     const char *key)
{
	bool ret = true;

	if (!test_SecurityDescriptor(p, tctx, handle, key)) {
		torture_comment(tctx, "test_SecurityDescriptor failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorInheritance(p, tctx, handle, key)) {
		torture_comment(tctx, "test_SecurityDescriptorInheritance failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorBlockInheritance(p, tctx, handle, key)) {
		torture_comment(tctx, "test_SecurityDescriptorBlockInheritance failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorsSecInfo(p, tctx, handle, key)) {
		torture_comment(tctx, "test_SecurityDescriptorsSecInfo failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorsMasks(p, tctx, handle, key)) {
		torture_comment(tctx, "test_SecurityDescriptorsMasks failed\n");
		ret = false;
	}

	return ret;
}

static bool test_DeleteKey_opts(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle,
				const char *key,
				WERROR expected_result)
{
	struct winreg_DeleteKey r;

	torture_comment(tctx, "Testing DeleteKey(%s)\n", key);

	r.in.handle = handle;
	init_winreg_String(&r.in.key, key);

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_DeleteKey_r(b, tctx, &r),
		"Delete Key failed");
	torture_assert_werr_equal(tctx, r.out.result, expected_result,
		"DeleteKey failed");

	return true;
}

static bool test_DeleteKey(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *handle, const char *key)
{
	return test_DeleteKey_opts(b, tctx, handle, key, WERR_OK);
}

static bool test_QueryInfoKey(struct dcerpc_binding_handle *b,
			      struct torture_context *tctx,
			      struct policy_handle *handle,
			      char *kclass,
			      uint32_t *pmax_valnamelen,
			      uint32_t *pmax_valbufsize)
{
	struct winreg_QueryInfoKey r;
	uint32_t num_subkeys, max_subkeylen, max_classlen,
		num_values, max_valnamelen, max_valbufsize,
		secdescsize;
	NTTIME last_changed_time;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.out.num_subkeys = &num_subkeys;
	r.out.max_subkeylen = &max_subkeylen;
	r.out.max_classlen = &max_classlen;
	r.out.num_values = &num_values;
	r.out.max_valnamelen = &max_valnamelen;
	r.out.max_valbufsize = &max_valbufsize;
	r.out.secdescsize = &secdescsize;
	r.out.last_changed_time = &last_changed_time;

	r.out.classname = talloc(tctx, struct winreg_String);

	r.in.classname = talloc(tctx, struct winreg_String);
	init_winreg_String(r.in.classname, kclass);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_QueryInfoKey_r(b, tctx, &r),
				   "QueryInfoKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "QueryInfoKey failed");

	if (pmax_valnamelen) {
		*pmax_valnamelen = max_valnamelen;
	}

	if (pmax_valbufsize) {
		*pmax_valbufsize = max_valbufsize;
	}

	return true;
}

static bool test_SetValue(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct policy_handle *handle,
			  const char *value_name,
			  enum winreg_Type type,
			  uint8_t *data,
			  uint32_t size)
{
	struct winreg_SetValue r;
	struct winreg_String name;

	torture_comment(tctx, "Testing SetValue(%s), type: %s, offered: 0x%08x)\n",
		value_name, str_regtype(type), size);

	init_winreg_String(&name, value_name);

	r.in.handle = handle;
	r.in.name = name;
	r.in.type = type;
	r.in.data = data;
	r.in.size = size;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_SetValue_r(b, tctx, &r),
		"winreg_SetValue failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"winreg_SetValue failed");

	return true;
}

static bool test_DeleteValue(struct dcerpc_binding_handle *b,
			     struct torture_context *tctx,
			     struct policy_handle *handle,
			     const char *value_name)
{
	struct winreg_DeleteValue r;
	struct winreg_String value;

	torture_comment(tctx, "Testing DeleteValue(%s)\n", value_name);

	init_winreg_String(&value, value_name);

	r.in.handle = handle;
	r.in.value = value;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_DeleteValue_r(b, tctx, &r),
		"winreg_DeleteValue failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"winreg_DeleteValue failed");

	return true;
}

static bool test_key(struct dcerpc_pipe *p, struct torture_context *tctx,
		     struct policy_handle *handle, int depth,
		     bool test_security);

static bool test_EnumKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			 struct policy_handle *handle, int depth,
			 bool test_security)
{
	struct winreg_EnumKey r;
	struct winreg_StringBuf kclass, name;
	NTSTATUS status;
	NTTIME t = 0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	kclass.name   = "";
	kclass.size   = 1024;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.enum_index = 0;
	r.in.name = &name;
	r.in.keyclass = &kclass;
	r.out.name = &name;
	r.in.last_changed_time = &t;

	do {
		name.name   = NULL;
		name.size   = 1024;

		status = dcerpc_winreg_EnumKey_r(b, tctx, &r);

		if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(r.out.result)) {
			struct policy_handle key_handle;

			torture_comment(tctx, "EnumKey: %d: %s\n",
					r.in.enum_index,
					r.out.name->name);

			if (!test_OpenKey(b, tctx, handle, r.out.name->name,
					  &key_handle)) {
			} else {
				test_key(p, tctx, &key_handle,
					 depth + 1, test_security);
			}
		}

		r.in.enum_index++;

	} while (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(r.out.result));

	torture_assert_ntstatus_ok(tctx, status, "EnumKey failed");

	if (!W_ERROR_IS_OK(r.out.result) &&
		!W_ERROR_EQUAL(r.out.result, WERR_NO_MORE_ITEMS)) {
		torture_fail(tctx, "EnumKey failed");
	}

	return true;
}

static bool test_QueryMultipleValues(struct dcerpc_binding_handle *b,
				     struct torture_context *tctx,
				     struct policy_handle *handle,
				     const char *valuename)
{
	struct winreg_QueryMultipleValues r;
	uint32_t bufsize=0;

	ZERO_STRUCT(r);

	r.in.key_handle = handle;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, 1);
	r.in.values_in[0].ve_valuename = talloc(tctx, struct winreg_ValNameBuf);
	r.in.values_in[0].ve_valuename->name = valuename;
	/* size needs to be set manually for winreg_ValNameBuf */
	r.in.values_in[0].ve_valuename->size = strlen_m_term(valuename)*2;

	r.in.num_values = 1;
	r.in.buffer_size = r.out.buffer_size = talloc(tctx, uint32_t);
	*r.in.buffer_size = bufsize;
	do {
		*r.in.buffer_size = bufsize;
		r.in.buffer = r.out.buffer = talloc_zero_array(tctx, uint8_t,
							       *r.in.buffer_size);

		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_QueryMultipleValues_r(b, tctx, &r),
			"QueryMultipleValues failed");

		talloc_free(r.in.buffer);
		bufsize += 0x20;
	} while (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA));

	torture_assert_werr_ok(tctx, r.out.result, "QueryMultipleValues failed");

	return true;
}

static bool test_QueryMultipleValues_full(struct dcerpc_binding_handle *b,
					  struct torture_context *tctx,
					  struct policy_handle *handle,
					  uint32_t num_values,
					  const char * const *valuenames,
					  bool existing_value)
{
	struct winreg_QueryMultipleValues r;
	uint32_t bufsize = 0;
	int i;

	torture_comment(tctx, "Testing QueryMultipleValues\n");

	ZERO_STRUCT(r);

	r.in.key_handle = handle;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, 0);
	r.in.buffer_size = r.out.buffer_size = &bufsize;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues_r(b, tctx, &r),
		"QueryMultipleValues failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues failed");

	/* this test crashes w2k8 remote registry */
#if 0
	r.in.num_values = num_values;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, num_values);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues_r(b, tctx, &r),
		"QueryMultipleValues failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues failed");
#endif
	r.in.num_values = num_values;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, num_values);
	for (i=0; i < r.in.num_values; i++) {
		r.in.values_in[i].ve_valuename = talloc_zero(tctx, struct winreg_ValNameBuf);
		r.in.values_in[i].ve_valuename->name = talloc_strdup(tctx, valuenames[i]);
		r.in.values_in[i].ve_valuename->size = strlen_m_term(r.in.values_in[i].ve_valuename->name)*2;
	}

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues_r(b, tctx, &r),
		"QueryMultipleValues failed");
	torture_assert_werr_equal(tctx, r.out.result, existing_value ? WERR_MORE_DATA : WERR_BADFILE,
		"QueryMultipleValues failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_BADFILE)) {
		return true;
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		*r.in.buffer_size = 0xff;
		r.in.buffer = r.out.buffer = talloc_zero_array(tctx, uint8_t, *r.in.buffer_size);

		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_QueryMultipleValues_r(b, tctx, &r),
			"QueryMultipleValues failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues failed");

	return true;
}


static bool test_QueryMultipleValues2_full(struct dcerpc_binding_handle *b,
					   struct torture_context *tctx,
					   struct policy_handle *handle,
					   uint32_t num_values,
					   const char * const *valuenames,
					   bool existing_value)
{
	struct winreg_QueryMultipleValues2 r;
	uint32_t offered = 0, needed;
	int i;

	torture_comment(tctx, "Testing QueryMultipleValues2\n");

	ZERO_STRUCT(r);

	r.in.key_handle = handle;
	r.in.offered = &offered;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, 0);
	r.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
		"QueryMultipleValues2 failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues2 failed");

	/* this test crashes w2k8 remote registry */
#if 0
	r.in.num_values = num_values;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, num_values);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
		"QueryMultipleValues2 failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues2 failed");
#endif
	r.in.num_values = num_values;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, num_values);
	for (i=0; i < r.in.num_values; i++) {
		r.in.values_in[i].ve_valuename = talloc_zero(tctx, struct winreg_ValNameBuf);
		r.in.values_in[i].ve_valuename->name = talloc_strdup(tctx, valuenames[i]);
		r.in.values_in[i].ve_valuename->size = strlen_m_term(r.in.values_in[i].ve_valuename->name)*2;
	}

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
		"QueryMultipleValues2 failed");
	torture_assert_werr_equal(tctx, r.out.result, existing_value ? WERR_MORE_DATA : WERR_BADFILE,
		"QueryMultipleValues2 failed");

	if (W_ERROR_EQUAL(r.out.result, WERR_BADFILE)) {
		return true;
	}

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		*r.in.offered = *r.out.needed;
		r.in.buffer = r.out.buffer = talloc_zero_array(tctx, uint8_t, *r.in.offered);

		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
			"QueryMultipleValues2 failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues2 failed");

	return true;
}

static bool test_QueryMultipleValues2(struct dcerpc_binding_handle *b,
				      struct torture_context *tctx,
				      struct policy_handle *handle,
				      const char *valuename)
{
	struct winreg_QueryMultipleValues2 r;
	uint32_t offered = 0, needed;

	ZERO_STRUCT(r);

	r.in.key_handle = handle;
	r.in.values_in = r.out.values_out = talloc_zero_array(tctx, struct QueryMultipleValue, 1);
	r.in.values_in[0].ve_valuename = talloc(tctx, struct winreg_ValNameBuf);
	r.in.values_in[0].ve_valuename->name = valuename;
	/* size needs to be set manually for winreg_ValNameBuf */
	r.in.values_in[0].ve_valuename->size = strlen_m_term(valuename)*2;

	r.in.num_values = 1;
	r.in.offered = &offered;
	r.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
		"QueryMultipleValues2 failed");
	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		*r.in.offered = *r.out.needed;
		r.in.buffer = r.out.buffer = talloc_zero_array(tctx, uint8_t, *r.in.offered);

		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_QueryMultipleValues2_r(b, tctx, &r),
			"QueryMultipleValues2 failed");
	}

	torture_assert_werr_ok(tctx, r.out.result,
		"QueryMultipleValues2 failed");

	return true;
}

static bool test_QueryValue(struct dcerpc_binding_handle *b,
			    struct torture_context *tctx,
			    struct policy_handle *handle,
			    const char *valuename)
{
	struct winreg_QueryValue r;
	NTSTATUS status;
	enum winreg_Type zero_type = 0;
	uint32_t offered = 0xfff;
	uint32_t zero = 0;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.data = NULL;
	r.in.value_name = talloc_zero(tctx, struct winreg_String);
	r.in.value_name->name = valuename;
	r.in.type = &zero_type;
	r.in.data_size = &offered;
	r.in.data_length = &zero;

	status = dcerpc_winreg_QueryValue_r(b, tctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		torture_fail(tctx, "QueryValue failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "QueryValue failed");

	return true;
}

static bool test_QueryValue_full(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *handle,
				 const char *valuename,
				 bool existing_value)
{
	struct winreg_QueryValue r;
	struct winreg_String value_name;
	enum winreg_Type type = REG_NONE;
	uint32_t data_size = 0;
	uint32_t real_data_size = 0;
	uint32_t data_length = 0;
	uint8_t *data = NULL;
	WERROR expected_error = WERR_BADFILE;
	const char *errmsg_nonexisting = "expected WERR_BADFILE for nonexisting value";

	if (valuename == NULL) {
		expected_error = WERR_INVALID_PARAM;
		errmsg_nonexisting = "expected WERR_INVALID_PARAM for NULL valuename";
	}

	ZERO_STRUCT(r);

	init_winreg_String(&value_name, NULL);

	torture_comment(tctx, "Testing QueryValue(%s)\n", valuename);

	r.in.handle = handle;
	r.in.value_name = &value_name;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r), "QueryValue failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"expected WERR_INVALID_PARAM for NULL winreg_String.name");

	init_winreg_String(&value_name, valuename);
	r.in.value_name = &value_name;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"expected WERR_INVALID_PARAM for missing type length and size");

	r.in.type = &type;
	r.out.type = &type;
	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"expected WERR_INVALID_PARAM for missing length and size");

	r.in.data_length = &data_length;
	r.out.data_length = &data_length;
	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"expected WERR_INVALID_PARAM for missing size");

	r.in.data_size = &data_size;
	r.out.data_size = &data_size;
	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	if (existing_value) {
		torture_assert_werr_ok(tctx, r.out.result,
			"QueryValue failed");
	} else {
		torture_assert_werr_equal(tctx, r.out.result, expected_error,
			errmsg_nonexisting);
	}

	real_data_size = *r.out.data_size;

	data = talloc_zero_array(tctx, uint8_t, 0);
	r.in.data = data;
	r.out.data = data;
	*r.in.data_size = 0;
	*r.out.data_size = 0;
	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	if (existing_value) {
		torture_assert_werr_equal(tctx, r.out.result, WERR_MORE_DATA,
			"expected WERR_MORE_DATA for query with too small buffer");
	} else {
		torture_assert_werr_equal(tctx, r.out.result, expected_error,
			errmsg_nonexisting);
	}

	data = talloc_zero_array(tctx, uint8_t, real_data_size);
	r.in.data = data;
	r.out.data = data;
	r.in.data_size = &real_data_size;
	r.out.data_size = &real_data_size;
	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_QueryValue_r(b, tctx, &r),
		"QueryValue failed");
	if (existing_value) {
		torture_assert_werr_ok(tctx, r.out.result,
			"QueryValue failed");
	} else {
		torture_assert_werr_equal(tctx, r.out.result, expected_error,
			errmsg_nonexisting);
	}

	return true;
}

static bool test_EnumValue(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *handle, int max_valnamelen,
			   int max_valbufsize)
{
	struct winreg_EnumValue r;
	enum winreg_Type type = 0;
	uint32_t size = max_valbufsize, zero = 0;
	bool ret = true;
	uint8_t *data = NULL;
	struct winreg_ValNameBuf name;
	char n = '\0';

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.enum_index = 0;
	r.in.name = &name;
	r.out.name = &name;
	r.in.type = &type;
	r.in.length = &zero;
	r.in.size = &size;

	do {
		name.name = &n;
		name.size = max_valnamelen + 2;
		name.length = 0;

		data = NULL;
		if (size) {
			data = (uint8_t *) talloc_array(tctx, uint8_t *, size);
		}
		r.in.value = data;

		torture_assert_ntstatus_ok(tctx,
					   dcerpc_winreg_EnumValue_r(b, tctx, &r),
					   "EnumValue failed");

		if (W_ERROR_IS_OK(r.out.result)) {
			ret &= test_QueryValue(b, tctx, handle,
					       r.out.name->name);
			ret &= test_QueryMultipleValues(b, tctx, handle,
							r.out.name->name);
			ret &= test_QueryMultipleValues2(b, tctx, handle,
							 r.out.name->name);
		}

		talloc_free(data);

		r.in.enum_index++;
	} while (W_ERROR_IS_OK(r.out.result));

	torture_assert_werr_equal(tctx, r.out.result, WERR_NO_MORE_ITEMS,
				  "EnumValue failed");

	return ret;
}

static bool test_AbortSystemShutdown(struct dcerpc_binding_handle *b,
				     struct torture_context *tctx)
{
	struct winreg_AbortSystemShutdown r;
	uint16_t server = 0x0;

	ZERO_STRUCT(r);
	r.in.server = &server;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_AbortSystemShutdown_r(b, tctx, &r),
				   "AbortSystemShutdown failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "AbortSystemShutdown failed");

	return true;
}

static bool test_InitiateSystemShutdown(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	struct winreg_InitiateSystemShutdown r;
	uint16_t hostname = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r);
	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_InitiateSystemShutdown_r(b, tctx, &r),
				   "InitiateSystemShutdown failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "InitiateSystemShutdown failed");

	return test_AbortSystemShutdown(b, tctx);
}


static bool test_InitiateSystemShutdownEx(struct torture_context *tctx,
					  struct dcerpc_pipe *p)
{
	struct winreg_InitiateSystemShutdownEx r;
	uint16_t hostname = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(r);
	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;
	r.in.reason = 0;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_InitiateSystemShutdownEx_r(b, tctx, &r),
		"InitiateSystemShutdownEx failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "InitiateSystemShutdownEx failed");

	return test_AbortSystemShutdown(b, tctx);
}
#define MAX_DEPTH 2		/* Only go this far down the tree */

static bool test_key(struct dcerpc_pipe *p, struct torture_context *tctx,
		     struct policy_handle *handle, int depth,
		     bool test_security)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint32_t max_valnamelen = 0;
	uint32_t max_valbufsize = 0;

	if (depth == MAX_DEPTH)
		return true;

	if (!test_QueryInfoKey(b, tctx, handle, NULL,
			       &max_valnamelen, &max_valbufsize)) {
	}

	if (!test_NotifyChangeKeyValue(b, tctx, handle)) {
	}

	if (test_security && !test_GetKeySecurity(p, tctx, handle, NULL)) {
	}

	if (!test_EnumKey(p, tctx, handle, depth, test_security)) {
	}

	if (!test_EnumValue(b, tctx, handle, max_valnamelen, max_valbufsize)) {
	}

	if (!test_EnumValue(b, tctx, handle, max_valnamelen, 0xFFFF)) {
	}

	test_CloseKey(b, tctx, handle);

	return true;
}

static bool test_SetValue_simple(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *handle)
{
	const char *value_name = TEST_VALUE;
	uint32_t value = 0x12345678;
	uint64_t value2 = 0x12345678;
	const char *string = "torture";
	const char *array[2];
	DATA_BLOB blob;
	enum winreg_Type types[] = {
		REG_DWORD,
		REG_DWORD_BIG_ENDIAN,
		REG_QWORD,
		REG_BINARY,
		REG_SZ,
		REG_MULTI_SZ
	};
	int t;

	array[0] = "array0";
	array[1] = NULL;

	torture_comment(tctx, "Testing SetValue (standard formats)\n");

	for (t=0; t < ARRAY_SIZE(types); t++) {

		enum winreg_Type w_type;
		uint32_t w_size, w_length;
		uint8_t *w_data;

		switch (types[t]) {
		case REG_DWORD:
		case REG_DWORD_BIG_ENDIAN:
			blob = data_blob_talloc_zero(tctx, 4);
			SIVAL(blob.data, 0, value);
			break;
		case REG_QWORD:
			blob = data_blob_talloc_zero(tctx, 8);
			SBVAL(blob.data, 0, value2);
			break;
		case REG_BINARY:
			blob = data_blob_string_const("binary_blob");
			break;
		case REG_SZ:
			torture_assert(tctx, push_reg_sz(tctx, &blob, string), "failed to push REG_SZ");
			break;
		case REG_MULTI_SZ:
			torture_assert(tctx, push_reg_multi_sz(tctx, &blob, array), "failed to push REG_MULTI_SZ");
			break;
		default:
			break;
		}

		torture_assert(tctx,
			test_SetValue(b, tctx, handle, value_name, types[t], blob.data, blob.length),
			"test_SetValue failed");
		torture_assert(tctx,
			test_QueryValue_full(b, tctx, handle, value_name, true),
			talloc_asprintf(tctx, "test_QueryValue_full for %s value failed", value_name));
		torture_assert(tctx,
			test_winreg_QueryValue(tctx, b, handle, value_name, &w_type, &w_size, &w_length, &w_data),
			"test_winreg_QueryValue failed");
		torture_assert(tctx,
			test_DeleteValue(b, tctx, handle, value_name),
			"test_DeleteValue failed");

		torture_assert_int_equal(tctx, w_type, types[t], "winreg type mismatch");
		torture_assert_int_equal(tctx, w_size, blob.length, "winreg size mismatch");
		torture_assert_int_equal(tctx, w_length, blob.length, "winreg length mismatch");
		torture_assert_mem_equal(tctx, w_data, blob.data, blob.length, "winreg buffer mismatch");
	}

	torture_comment(tctx, "Testing SetValue (standard formats) succeeded\n");

	return true;
}

static bool test_SetValue_values(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *handle)
{
	DATA_BLOB blob;
	const char *values[] = {
		"torture_value",
		"torture value",
		"torture,value",
		"torture;value",
		"torture/value",
		"torture\\value",
		"torture_value_name",
		"torture value name",
		"torture,value,name",
		"torture;value;name",
		"torture/value/name",
		"torture\\value\\name",
	};
	int i;

	torture_comment(tctx, "Testing SetValue (values)\n");

	for (i=0; i < ARRAY_SIZE(values); i++) {

		enum winreg_Type w_type;
		uint32_t w_size, w_length;
		uint8_t *w_data;

		blob = data_blob_talloc(tctx, NULL, 32);

		generate_random_buffer(blob.data, 32);

		torture_assert(tctx,
			test_SetValue(b, tctx, handle, values[i], REG_BINARY, blob.data, blob.length),
			"test_SetValue failed");
		torture_assert(tctx,
			test_QueryValue_full(b, tctx, handle, values[i], true),
			talloc_asprintf(tctx, "test_QueryValue_full for %s value failed", values[i]));
		torture_assert(tctx,
			test_winreg_QueryValue(tctx, b, handle, values[i], &w_type, &w_size, &w_length, &w_data),
			"test_winreg_QueryValue failed");
		torture_assert(tctx,
			test_DeleteValue(b, tctx, handle, values[i]),
			"test_DeleteValue failed");

		torture_assert_int_equal(tctx, w_type, REG_BINARY, "winreg type mismatch");
		torture_assert_int_equal(tctx, w_size, blob.length, "winreg size mismatch");
		torture_assert_int_equal(tctx, w_length, blob.length, "winreg length mismatch");
		torture_assert_mem_equal(tctx, w_data, blob.data, blob.length, "winreg buffer mismatch");
	}

	torture_comment(tctx, "Testing SetValue (values) succeeded\n");

	return true;
}

typedef NTSTATUS (*winreg_open_fn)(struct dcerpc_binding_handle *, TALLOC_CTX *, void *);

static bool test_SetValue_extended(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *handle)
{
	const char *value_name = TEST_VALUE;
	enum winreg_Type types[] = {
		REG_NONE,
		REG_SZ,
		REG_EXPAND_SZ,
		REG_BINARY,
		REG_DWORD,
		REG_DWORD_BIG_ENDIAN,
		REG_LINK,
		REG_MULTI_SZ,
		REG_RESOURCE_LIST,
		REG_FULL_RESOURCE_DESCRIPTOR,
		REG_RESOURCE_REQUIREMENTS_LIST,
		REG_QWORD,
		12,
		13,
		14,
		55,
		123456,
		653210,
		__LINE__
	};
	int t, l;

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_skip(tctx, "skipping extended SetValue test against Samba4");
	}

	torture_comment(tctx, "Testing SetValue (extended formats)\n");

	for (t=0; t < ARRAY_SIZE(types); t++) {
	for (l=0; l < 16; l++) {

		enum winreg_Type w_type;
		uint32_t w_size, w_length;
		uint8_t *w_data;

		uint32_t size;
		uint8_t *data;

		size = l;
		data = talloc_array(tctx, uint8_t, size);

		generate_random_buffer(data, size);

		torture_assert(tctx,
			test_SetValue(b, tctx, handle, value_name, types[t], data, size),
			"test_SetValue failed");

		torture_assert(tctx,
			test_winreg_QueryValue(tctx, b, handle, value_name, &w_type, &w_size, &w_length, &w_data),
			"test_winreg_QueryValue failed");

		torture_assert(tctx,
			test_DeleteValue(b, tctx, handle, value_name),
			"test_DeleteValue failed");

		torture_assert_int_equal(tctx, w_type, types[t], "winreg type mismatch");
		torture_assert_int_equal(tctx, w_size, size, "winreg size mismatch");
		torture_assert_int_equal(tctx, w_length, size, "winreg length mismatch");
		torture_assert_mem_equal(tctx, w_data, data, size, "winreg buffer mismatch");
	}
	}

	torture_comment(tctx, "Testing SetValue (extended formats) succeeded\n");

	return true;
}

static bool test_create_keynames(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *handle)
{
	const char *keys[] = {
		"torture_key",
		"torture key",
		"torture,key",
		"torture/key",
		"torture\\key",
	};
	int i;

	for (i=0; i < ARRAY_SIZE(keys); i++) {

		enum winreg_CreateAction action_taken;
		struct policy_handle new_handle;
		char *q, *tmp;

		torture_assert(tctx,
			test_CreateKey_opts(tctx, b, handle, keys[i], NULL,
					    REG_OPTION_NON_VOLATILE,
					    SEC_FLAG_MAXIMUM_ALLOWED,
					    NULL,
					    WERR_OK,
					    &action_taken,
					    &new_handle),
			talloc_asprintf(tctx, "failed to create '%s' key", keys[i]));

		torture_assert_int_equal(tctx, action_taken, REG_CREATED_NEW_KEY, "unexpected action");

		torture_assert(tctx,
			test_DeleteKey_opts(b, tctx, handle, keys[i], WERR_OK),
			"failed to delete key");

		torture_assert(tctx,
			test_DeleteKey_opts(b, tctx, handle, keys[i], WERR_BADFILE),
			"failed 2nd delete key");

		tmp = talloc_strdup(tctx, keys[i]);

		q = strchr(tmp, '\\');
		if (q != NULL) {
			*q = '\0';
			q++;

			torture_assert(tctx,
				test_DeleteKey_opts(b, tctx, handle, tmp, WERR_OK),
				"failed to delete key");

			torture_assert(tctx,
				test_DeleteKey_opts(b, tctx, handle, tmp, WERR_BADFILE),
				"failed 2nd delete key");
		}
	}

	return true;
}

#define KEY_CURRENT_VERSION "SOFTWARE\\MICROSOFT\\WINDOWS NT\\CURRENTVERSION"
#define VALUE_CURRENT_VERSION "CurrentVersion"
#define VALUE_SYSTEM_ROOT "SystemRoot"

static const struct {
	const char *values[3];
	uint32_t num_values;
	bool existing_value;
	const char *error_message;
} multiple_values_tests[] = {
	{
		.values = { VALUE_CURRENT_VERSION, NULL, NULL },
		.num_values = 1,
		.existing_value = true,
		.error_message = NULL
	},{
		.values = { VALUE_SYSTEM_ROOT, NULL, NULL },
		.num_values = 1,
		.existing_value = true,
		.error_message = NULL
	},{
		.values = { VALUE_CURRENT_VERSION, VALUE_SYSTEM_ROOT, NULL },
		.num_values = 2,
		.existing_value = true,
		.error_message = NULL
	},{
		.values = { VALUE_CURRENT_VERSION, VALUE_SYSTEM_ROOT,
			    VALUE_CURRENT_VERSION },
		.num_values = 3,
		.existing_value = true,
		.error_message = NULL
	},{
		.values = { VALUE_CURRENT_VERSION, NULL, VALUE_SYSTEM_ROOT },
		.num_values = 3,
		.existing_value = false,
		.error_message = NULL
	},{
		.values = { VALUE_CURRENT_VERSION, "", VALUE_SYSTEM_ROOT },
		.num_values = 3,
		.existing_value = false,
		.error_message = NULL
	},{
		.values = { "IDoNotExist", NULL, NULL },
		.num_values = 1,
		.existing_value = false,
		.error_message = NULL
	},{
		.values = { "IDoNotExist", VALUE_CURRENT_VERSION, NULL },
		.num_values = 2,
		.existing_value = false,
		.error_message = NULL
	},{
		.values = { VALUE_CURRENT_VERSION, "IDoNotExist", NULL },
		.num_values = 2,
		.existing_value = false,
		.error_message = NULL
	}
};

static bool test_HKLM_wellknown(struct torture_context *tctx,
				struct dcerpc_binding_handle *b,
				struct policy_handle *handle)
{
	struct policy_handle newhandle;
	int i;

	/* FIXME: s3 does not support SEC_FLAG_MAXIMUM_ALLOWED yet */
	if (torture_setting_bool(tctx, "samba3", false)) {
		torture_assert(tctx, test_OpenKey_opts(tctx, b, handle,
			       KEY_CURRENT_VERSION,
			       REG_OPTION_NON_VOLATILE,
			       KEY_QUERY_VALUE,
			       &newhandle,
			       WERR_OK),
			"failed to open current version key");
	} else {
		torture_assert(tctx, test_OpenKey(b, tctx, handle, KEY_CURRENT_VERSION, &newhandle),
			"failed to open current version key");
	}

	torture_assert(tctx, test_QueryValue_full(b, tctx, &newhandle, VALUE_CURRENT_VERSION, true),
		"failed to query current version");
	torture_assert(tctx, test_QueryValue_full(b, tctx, &newhandle, "IDoNotExist", false),
		"succeeded to query nonexistent value");
	torture_assert(tctx, test_QueryValue_full(b, tctx, &newhandle, NULL, false),
		"succeeded to query value with NULL name");
	torture_assert(tctx, test_QueryValue_full(b, tctx, &newhandle, "", false),
		"succeeded to query nonexistent default value (\"\")");

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping QueryMultipleValues{2} tests against Samba4\n");
		goto close_key;
	}

	for (i=0; i < ARRAY_SIZE(multiple_values_tests); i++) {
		const char *msg;
		msg = talloc_asprintf(tctx,
				"failed to query %d %sexisting values\n",
					multiple_values_tests[i].num_values,
					multiple_values_tests[i].existing_value ? "":"non");

		torture_assert(tctx,
			test_QueryMultipleValues_full(b, tctx, &newhandle,
						      multiple_values_tests[i].num_values,
						      multiple_values_tests[i].values,
						      multiple_values_tests[i].existing_value),
			msg);
		torture_assert(tctx,
			test_QueryMultipleValues2_full(b, tctx, &newhandle,
						       multiple_values_tests[i].num_values,
						       multiple_values_tests[i].values,
						       multiple_values_tests[i].existing_value),
			msg);
	}

 close_key:
	torture_assert(tctx, test_CloseKey(b, tctx, &newhandle),
		"failed to close current version key");

	return true;
}

static bool test_OpenHive(struct torture_context *tctx,
			  struct dcerpc_binding_handle *b,
			  struct policy_handle *handle,
			  int hkey)
{
	struct winreg_OpenHKLM r;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = handle;

	switch (hkey) {
	case HKEY_LOCAL_MACHINE:
		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_OpenHKLM_r(b, tctx, &r),
			"failed to open HKLM");
		torture_assert_werr_ok(tctx, r.out.result,
			"failed to open HKLM");
		break;
	case HKEY_CURRENT_USER:
		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_OpenHKCU_r(b, tctx, (struct winreg_OpenHKCU *)(void *)&r),
			"failed to open HKCU");
		torture_assert_werr_ok(tctx, r.out.result,
			"failed to open HKCU");
		break;
	case HKEY_USERS:
		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_OpenHKU_r(b, tctx, (struct winreg_OpenHKU *)(void *)&r),
			"failed to open HKU");
		torture_assert_werr_ok(tctx, r.out.result,
			"failed to open HKU");
		break;
	case HKEY_CLASSES_ROOT:
		torture_assert_ntstatus_ok(tctx,
			dcerpc_winreg_OpenHKCR_r(b, tctx, (struct winreg_OpenHKCR *)(void *)&r),
			"failed to open HKCR");
		torture_assert_werr_ok(tctx, r.out.result,
			"failed to open HKCR");
		break;
	default:
		torture_warning(tctx, "unsupported hkey: 0x%08x\n", hkey);
		return false;
	}

	return true;
}

static bool test_volatile_keys(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       struct policy_handle *handle,
			       int hkey)
{
	struct policy_handle new_handle, hive_handle;
	enum winreg_CreateAction action_taken;

	torture_comment(tctx, "Testing VOLATILE key\n");

	test_DeleteKey(b, tctx, handle, TEST_KEY_VOLATILE);

	torture_assert(tctx,
		test_CreateKey_opts(tctx, b, handle, TEST_KEY_VOLATILE, NULL,
				    REG_OPTION_VOLATILE,
				    SEC_FLAG_MAXIMUM_ALLOWED,
				    NULL,
				    WERR_OK,
				    &action_taken,
				    &new_handle),
		"failed to create REG_OPTION_VOLATILE type key");

	torture_assert_int_equal(tctx, action_taken, REG_CREATED_NEW_KEY, "unexpected action");

	torture_assert(tctx,
		test_CreateKey_opts(tctx, b, &new_handle, TEST_SUBKEY_VOLATILE, NULL,
				    REG_OPTION_NON_VOLATILE,
				    SEC_FLAG_MAXIMUM_ALLOWED,
				    NULL,
				    WERR_CHILD_MUST_BE_VOLATILE,
				    NULL,
				    NULL),
		"failed to fail create REG_OPTION_VOLATILE type key");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, TEST_KEY_VOLATILE,
				  REG_OPTION_NON_VOLATILE,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_OK),
		"failed to open volatile key");

	torture_assert(tctx,
		test_DeleteKey(b, tctx, handle, TEST_KEY_VOLATILE),
		"failed to delete key");

	torture_assert(tctx,
		test_CreateKey_opts(tctx, b, handle, TEST_KEY_VOLATILE, NULL,
				    REG_OPTION_VOLATILE,
				    SEC_FLAG_MAXIMUM_ALLOWED,
				    NULL,
				    WERR_OK,
				    &action_taken,
				    &new_handle),
		"failed to create REG_OPTION_VOLATILE type key");

	torture_assert_int_equal(tctx, action_taken, REG_CREATED_NEW_KEY, "unexpected action");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, TEST_KEY_VOLATILE,
				  REG_OPTION_VOLATILE,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_OK),
		"failed to open volatile key");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	torture_assert(tctx,
		test_OpenHive(tctx, b, &hive_handle, hkey),
		"failed top open hive");

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, &hive_handle, TEST_KEY_VOLATILE,
				  REG_OPTION_VOLATILE,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_BADFILE),
		"failed to open volatile key");

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, &hive_handle, TEST_KEY_VOLATILE,
				  REG_OPTION_NON_VOLATILE,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_BADFILE),
		"failed to open volatile key");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &hive_handle),
		"failed to close");

	torture_assert(tctx,
		test_DeleteKey(b, tctx, handle, TEST_KEY_VOLATILE),
		"failed to delete key");


	torture_comment(tctx, "Testing VOLATILE key succeeded\n");

	return true;
}

static const char *kernel_mode_registry_path(struct torture_context *tctx,
					     int hkey,
					     const char *sid_string,
					     const char *path)
{
	switch (hkey) {
	case HKEY_LOCAL_MACHINE:
		return talloc_asprintf(tctx, "\\Registry\\MACHINE\\%s", path);
	case HKEY_CURRENT_USER:
		return talloc_asprintf(tctx, "\\Registry\\USER\\%s\\%s", sid_string, path);
	case HKEY_USERS:
		return talloc_asprintf(tctx, "\\Registry\\USER\\%s", path);
	case HKEY_CLASSES_ROOT:
		return talloc_asprintf(tctx, "\\Registry\\MACHINE\\Software\\Classes\\%s", path);
	default:
		torture_warning(tctx, "unsupported hkey: 0x%08x\n", hkey);
		return NULL;
	}
}

static bool test_symlink_keys(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      struct policy_handle *handle,
			      const char *key,
			      int hkey)
{
	struct policy_handle new_handle;
	enum winreg_CreateAction action_taken;
	DATA_BLOB blob;
	uint32_t value = 42;
	const char *test_key_symlink_dest;
	const char *test_key_symlink;
	const char *kernel_mode_path;

	/* disable until we know how to delete a symbolic link */
	torture_skip(tctx, "symlink test disabled");

	torture_comment(tctx, "Testing REG_OPTION_CREATE_LINK key\n");

	/* create destination key with testvalue */
	test_key_symlink = talloc_asprintf(tctx, "%s\\%s",
			key, TEST_KEY_SYMLINK);
	test_key_symlink_dest = talloc_asprintf(tctx, "%s\\%s",
			key, TEST_KEY_SYMLINK_DEST);

	test_DeleteKey(b, tctx, handle, test_key_symlink);

	torture_assert(tctx,
		test_CreateKey_opts(tctx, b, handle, test_key_symlink_dest, NULL,
				    0,
				    SEC_FLAG_MAXIMUM_ALLOWED,
				    NULL,
				    WERR_OK,
				    &action_taken,
				    &new_handle),
		"failed to create symlink destination");

	blob = data_blob_talloc_zero(tctx, 4);
	SIVAL(blob.data, 0, value);

	torture_assert(tctx,
		test_SetValue(b, tctx, &new_handle, "TestValue", REG_DWORD, blob.data, blob.length),
		"failed to create TestValue");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	/* create symlink */

	torture_assert(tctx,
		test_CreateKey_opts(tctx, b, handle, test_key_symlink, NULL,
				    REG_OPTION_CREATE_LINK | REG_OPTION_VOLATILE,
				    SEC_FLAG_MAXIMUM_ALLOWED,
				    NULL,
				    WERR_OK,
				    &action_taken,
				    &new_handle),
		"failed to create REG_OPTION_CREATE_LINK type key");

	torture_assert_int_equal(tctx, action_taken, REG_CREATED_NEW_KEY, "unexpected action");

	kernel_mode_path = kernel_mode_registry_path(tctx, hkey, NULL, test_key_symlink_dest);

	torture_assert(tctx,
		convert_string_talloc(tctx, CH_UNIX, CH_UTF16,
				      kernel_mode_path,
				      strlen(kernel_mode_path), /* not NULL terminated */
				      &blob.data, &blob.length,
				      false),
		"failed to convert");

	torture_assert(tctx,
		test_SetValue(b, tctx, &new_handle, "SymbolicLinkValue", REG_LINK, blob.data, blob.length),
		"failed to create SymbolicLinkValue value");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	/* test follow symlink */

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, test_key_symlink,
				  0,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_OK),
		"failed to follow symlink key");

	torture_assert(tctx,
		test_QueryValue(b, tctx, &new_handle, "TestValue"),
		"failed to query value");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	/* delete link */

	torture_assert(tctx,
		test_OpenKey_opts(tctx, b, handle, test_key_symlink,
				  REG_OPTION_OPEN_LINK | REG_OPTION_VOLATILE,
				  SEC_FLAG_MAXIMUM_ALLOWED,
				  &new_handle,
				  WERR_OK),
		"failed to open symlink key");

	torture_assert(tctx,
		test_DeleteValue(b, tctx, &new_handle, "SymbolicLinkValue"),
		"failed to delete value SymbolicLinkValue");

	torture_assert(tctx,
		test_CloseKey(b, tctx, &new_handle),
		"failed to close");

	torture_assert(tctx,
		test_DeleteKey(b, tctx, handle, test_key_symlink),
		"failed to delete key");

	/* delete destination */

	torture_assert(tctx,
		test_DeleteKey(b, tctx, handle, test_key_symlink_dest),
		"failed to delete key");

	return true;
}

static bool test_CreateKey_keytypes(struct torture_context *tctx,
				    struct dcerpc_binding_handle *b,
				    struct policy_handle *handle,
				    const char *key,
				    int hkey)
{

	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		torture_skip(tctx, "skipping CreateKey keytypes test against Samba");
	}

	torture_assert(tctx,
		test_volatile_keys(tctx, b, handle, hkey),
		"failed to test volatile keys");

	torture_assert(tctx,
		test_symlink_keys(tctx, b, handle, key, hkey),
		"failed to test symlink keys");

	return true;
}

static bool test_key_base(struct torture_context *tctx,
			  struct dcerpc_binding_handle *b,
			  struct policy_handle *handle,
			  const char *base_key,
			  int hkey)
{
	struct policy_handle newhandle;
	bool ret = true, created = false, deleted = false;
	bool created3 = false;
	const char *test_key1;
	const char *test_key3;
	const char *test_subkey;

	test_Cleanup(b, tctx, handle, base_key);

	if (!test_CreateKey(b, tctx, handle, base_key, NULL)) {
		torture_comment(tctx,
				"CreateKey(%s) failed\n", base_key);
	}

	test_key1 = talloc_asprintf(tctx, "%s\\%s", base_key, TEST_KEY1);

	if (!test_CreateKey(b, tctx, handle, test_key1, NULL)) {
		torture_comment(tctx,
				"CreateKey failed - not considering a failure\n");
	} else {
		created = true;
	}

	if (created) {
		if (!test_FlushKey(b, tctx, handle)) {
			torture_comment(tctx, "FlushKey failed\n");
			ret = false;
		}

		if (!test_OpenKey(b, tctx, handle, test_key1, &newhandle)) {
			torture_fail(tctx,
				     "CreateKey failed (OpenKey after Create didn't work)\n");
		}

		if (hkey == HKEY_CURRENT_USER) {
			torture_assert(tctx, test_SetValue_simple(b, tctx, &newhandle),
				"simple SetValue test failed");
			torture_assert(tctx, test_SetValue_values(b, tctx, &newhandle),
				"values SetValue test failed");
			torture_assert(tctx, test_SetValue_extended(b, tctx, &newhandle),
				"extended SetValue test failed");
			torture_assert(tctx, test_create_keynames(b, tctx, &newhandle),
				"keyname CreateKey test failed");
		} else {
			torture_assert(tctx, test_CreateKey_keytypes(tctx, b, &newhandle, test_key1, hkey),
				"keytype test failed");
		}

		if (!test_CloseKey(b, tctx, &newhandle)) {
			torture_fail(tctx,
				     "CreateKey failed (CloseKey after Open didn't work)\n");
		}

		if (!test_DeleteKey(b, tctx, handle, test_key1)) {
			torture_comment(tctx, "DeleteKey(%s) failed\n",
					      test_key1);
			ret = false;
		} else {
			deleted = true;
		}

		if (!test_FlushKey(b, tctx, handle)) {
			torture_comment(tctx, "FlushKey failed\n");
			ret = false;
		}

		if (deleted) {
			if (!test_OpenKey_opts(tctx, b, handle, test_key1,
					       REG_OPTION_NON_VOLATILE,
					       SEC_FLAG_MAXIMUM_ALLOWED,
					       &newhandle,
					       WERR_BADFILE)) {
				torture_comment(tctx,
						"DeleteKey failed (OpenKey after Delete "
						"did not return WERR_BADFILE)\n");
				ret = false;
			}
		}

		test_key3 = talloc_asprintf(tctx, "%s\\%s", base_key, TEST_KEY3);

		if (test_CreateKey(b, tctx, handle, test_key3, NULL)) {
			created3 = true;
		}

		test_subkey = talloc_asprintf(tctx, "%s\\%s", test_key3, TEST_SUBKEY);

		if (created3) {
			if (test_CreateKey(b, tctx, handle, test_subkey, NULL)) {
				if (!test_DeleteKey(b, tctx, handle, test_subkey)) {
					torture_comment(tctx, "DeleteKey(%s) failed\n", test_subkey);
					ret = false;
				}
			}

			if (!test_DeleteKey(b, tctx, handle, test_key3)) {
				torture_comment(tctx, "DeleteKey(%s) failed\n", test_key3);
				ret = false;
			}
		}
	}

	test_Cleanup(b, tctx, handle, base_key);

	return ret;
}

static bool test_key_base_sd(struct torture_context *tctx,
			     struct dcerpc_pipe *p,
			     struct policy_handle *handle,
			     const char *base_key)
{
	struct policy_handle newhandle;
	bool ret = true, created2 = false, created4 = false;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *test_key2;
	const char *test_key4;

	torture_skip(tctx, "security descriptor test disabled\n");

	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		torture_skip(tctx, "skipping security descriptor tests against Samba");
	}

	test_Cleanup(b, tctx, handle, base_key);

	if (!test_CreateKey(b, tctx, handle, base_key, NULL)) {
		torture_comment(tctx,
				"CreateKey(%s) failed\n", base_key);
	}

	test_key2 = talloc_asprintf(tctx, "%s\\%s", base_key, TEST_KEY2);

	if (test_CreateKey_sd(b, tctx, handle, test_key2,
			      NULL, &newhandle)) {
		created2 = true;
	}

	if (created2 && !test_CloseKey(b, tctx, &newhandle)) {
		torture_comment(tctx, "CloseKey failed\n");
		ret = false;
	}

	test_key4 = talloc_asprintf(tctx, "%s\\%s", base_key, TEST_KEY4);

	if (test_CreateKey_sd(b, tctx, handle, test_key4, NULL, &newhandle)) {
		created4 = true;
	}

	if (created4 && !test_CloseKey(b, tctx, &newhandle)) {
		torture_comment(tctx, "CloseKey failed\n");
		ret = false;
	}

	if (created4 && !test_SecurityDescriptors(p, tctx, handle, test_key4)) {
		ret = false;
	}

	if (created4 && !test_DeleteKey(b, tctx, handle, test_key4)) {
		torture_comment(tctx, "DeleteKey(%s) failed\n", test_key4);
		ret = false;
	}

	if (created2 && !test_DeleteKey(b, tctx, handle, test_key4)) {
		torture_comment(tctx, "DeleteKey(%s) failed\n", test_key4);
		ret = false;
	}

	test_Cleanup(b, tctx, handle, base_key);

	return ret;
}

static bool test_Open(struct torture_context *tctx, struct dcerpc_pipe *p,
		      void *userdata)
{
	struct policy_handle handle;
	bool ret = true;
	struct winreg_OpenHKLM r;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *torture_base_key;
	int hkey = 0;

	winreg_open_fn open_fn = (winreg_open_fn)userdata;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx, open_fn(b, tctx, &r),
				   "open");

	if (!test_GetVersion(b, tctx, &handle)) {
		torture_comment(tctx, "GetVersion failed\n");
		ret = false;
	}

	if (open_fn == (winreg_open_fn)dcerpc_winreg_OpenHKLM_r) {
		hkey = HKEY_LOCAL_MACHINE;
		torture_base_key = "SOFTWARE\\Samba\\" TEST_KEY_BASE;
	} else if (open_fn == (winreg_open_fn)dcerpc_winreg_OpenHKU_r) {
		hkey = HKEY_USERS;
		torture_base_key = TEST_KEY_BASE;
	} else if (open_fn == (winreg_open_fn)dcerpc_winreg_OpenHKCR_r) {
		hkey = HKEY_CLASSES_ROOT;
		torture_base_key = TEST_KEY_BASE;
	} else if (open_fn == (winreg_open_fn)dcerpc_winreg_OpenHKCU_r) {
		hkey = HKEY_CURRENT_USER;
		torture_base_key = TEST_KEY_BASE;
	} else {
		torture_fail(tctx, "unsupported hkey");
	}

	if (hkey == HKEY_LOCAL_MACHINE) {
		torture_assert(tctx,
			test_HKLM_wellknown(tctx, b, &handle),
			"failed to test HKLM wellknown keys");
	}

	if (!test_key_base(tctx, b, &handle, torture_base_key, hkey)) {
		torture_warning(tctx, "failed to test TEST_KEY_BASE(%s)",
				torture_base_key);
		ret = false;
	}

	if (!test_key_base_sd(tctx, p, &handle, torture_base_key)) {
		torture_warning(tctx, "failed to test TEST_KEY_BASE(%s) sd",
				torture_base_key);
		ret = false;
	}

	/* The HKCR hive has a very large fanout */
	if (hkey == HKEY_CLASSES_ROOT) {
		if(!test_key(p, tctx, &handle, MAX_DEPTH - 1, false)) {
			ret = false;
		}
	} else if (hkey == HKEY_LOCAL_MACHINE) {
		/* FIXME we are not allowed to enum values in the HKLM root */
	} else {
		if (!test_key(p, tctx, &handle, 0, false)) {
			ret = false;
		}
	}

	return ret;
}

struct torture_suite *torture_rpc_winreg(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "winreg");
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "winreg",
						  &ndr_table_winreg);

	test = torture_rpc_tcase_add_test(tcase, "InitiateSystemShutdown",
					  test_InitiateSystemShutdown);
	test->dangerous = true;

	test = torture_rpc_tcase_add_test(tcase, "InitiateSystemShutdownEx",
					  test_InitiateSystemShutdownEx);
	test->dangerous = true;

	torture_rpc_tcase_add_test_ex(tcase, "HKLM",
				      test_Open,
				      (void *)dcerpc_winreg_OpenHKLM_r);
	torture_rpc_tcase_add_test_ex(tcase, "HKU",
				      test_Open,
				      (void *)dcerpc_winreg_OpenHKU_r);
	torture_rpc_tcase_add_test_ex(tcase, "HKCR",
				      test_Open,
				      (void *)dcerpc_winreg_OpenHKCR_r);
	torture_rpc_tcase_add_test_ex(tcase, "HKCU",
				      test_Open,
				      (void *)dcerpc_winreg_OpenHKCU_r);

	return suite;
}
