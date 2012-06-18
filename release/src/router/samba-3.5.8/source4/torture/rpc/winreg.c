/*
   Unix SMB/CIFS implementation.
   test suite for winreg rpc operations

   Copyright (C) Tim Potter 2003
   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Günther Deschner 2007

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
#include "torture/torture.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "torture/rpc/rpc.h"

#define TEST_KEY_BASE "smbtorture test"
#define TEST_KEY1 TEST_KEY_BASE "\\spottyfoot"
#define TEST_KEY2 TEST_KEY_BASE "\\with a SD (#1)"
#define TEST_KEY3 TEST_KEY_BASE "\\with a subkey"
#define TEST_KEY4 TEST_KEY_BASE "\\sd_tests"
#define TEST_SUBKEY TEST_KEY3 "\\subkey"
#define TEST_SUBKEY_SD  TEST_KEY4 "\\subkey_sd"
#define TEST_SUBSUBKEY_SD TEST_KEY4 "\\subkey_sd\\subsubkey_sd"

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

static bool test_GetVersion(struct dcerpc_pipe *p,
			    struct torture_context *tctx,
			    struct policy_handle *handle)
{
	struct winreg_GetVersion r;
	uint32_t v;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.out.version = &v;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_GetVersion(p, tctx, &r),
				   "GetVersion failed");

	torture_assert_werr_ok(tctx, r.out.result, "GetVersion failed");

	return true;
}

static bool test_NotifyChangeKeyValue(struct dcerpc_pipe *p,
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
				   dcerpc_winreg_NotifyChangeKeyValue(p, tctx, &r),
				   "NotifyChangeKeyValue failed");

	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_comment(tctx,
				"NotifyChangeKeyValue failed - %s - not considering\n",
				win_errstr(r.out.result));
		return true;
	}

	return true;
}

static bool test_CreateKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			   struct policy_handle *handle, const char *name,
			   const char *kclass)
{
	struct winreg_CreateKey r;
	struct policy_handle newhandle;
	enum winreg_CreateAction action_taken = 0;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.out.new_handle = &newhandle;
	init_winreg_String(&r.in.name, name);
	init_winreg_String(&r.in.keyclass, kclass);
	r.in.options = 0x0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.action_taken = r.out.action_taken = &action_taken;
	r.in.secdesc = NULL;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CreateKey(p, tctx, &r),
				   "CreateKey failed");

	torture_assert_werr_ok(tctx,  r.out.result, "CreateKey failed");

	return true;
}


/*
  createkey testing with a SD
*/
static bool test_CreateKey_sd(struct dcerpc_pipe *p,
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
		ndr_push_struct_blob(&sdblob, tctx, NULL, sd,
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

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CreateKey(p, tctx, &r),
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
				   dcerpc_winreg_GetKeySecurity(p, tctx, &r),
				   "GetKeySecurity failed");

	torture_assert_werr_equal(tctx, r.out.result, get_werr,
				  "GetKeySecurity failed");

	sdblob.data = r.out.sd->data;
	sdblob.length = r.out.sd->len;

	sd = talloc_zero(tctx, struct security_descriptor);

	torture_assert_ndr_success(tctx,
		ndr_pull_struct_blob(&sdblob, tctx, NULL, sd,
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

	ZERO_STRUCT(r);

	if (sd && (p->conn->flags & DCERPC_DEBUG_PRINT_OUT)) {
		NDR_PRINT_DEBUG(security_descriptor, sd);
	}

	torture_assert_ndr_success(tctx,
		ndr_push_struct_blob(&sdblob, tctx, NULL, sd,
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
				   dcerpc_winreg_SetKeySecurity(p, tctx, &r),
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

static bool test_CloseKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			  struct policy_handle *handle)
{
	struct winreg_CloseKey r;

	ZERO_STRUCT(r);
	r.in.handle = r.out.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_CloseKey(p, tctx, &r),
				   "CloseKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "CloseKey failed");

	return true;
}

static bool test_FlushKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			  struct policy_handle *handle)
{
	struct winreg_FlushKey r;

	ZERO_STRUCT(r);
	r.in.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_FlushKey(p, tctx, &r),
				   "FlushKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "FlushKey failed");

	return true;
}

static bool _test_OpenKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			  struct policy_handle *hive_handle,
			  const char *keyname, uint32_t access_mask,
			  struct policy_handle *key_handle,
			  WERROR open_werr,
			  bool *success)
{
	struct winreg_OpenKey r;

	ZERO_STRUCT(r);
	r.in.parent_handle = hive_handle;
	init_winreg_String(&r.in.keyname, keyname);
	r.in.unknown = 0x00000000;
	r.in.access_mask = access_mask;
	r.out.handle = key_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_winreg_OpenKey(p, tctx, &r),
				   "OpenKey failed");

	torture_assert_werr_equal(tctx, r.out.result, open_werr,
				  "OpenKey failed");

	if (success && W_ERROR_EQUAL(r.out.result, WERR_OK)) {
		*success = true;
	}

	return true;
}

static bool test_OpenKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			 struct policy_handle *hive_handle,
			 const char *keyname, struct policy_handle *key_handle)
{
	return _test_OpenKey(p, tctx, hive_handle, keyname,
			     SEC_FLAG_MAXIMUM_ALLOWED, key_handle,
			     WERR_OK, NULL);
}

static bool test_Cleanup(struct dcerpc_pipe *p, struct torture_context *tctx,
			 struct policy_handle *handle, const char *key)
{
	struct winreg_DeleteKey r;

	ZERO_STRUCT(r);
	r.in.handle = handle;

	init_winreg_String(&r.in.key, key);
	dcerpc_winreg_DeleteKey(p, tctx, &r);

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

	torture_comment(tctx, "SecurityDescriptor get & set\n");

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!_test_GetSetSecurityDescriptor(p, tctx, &new_handle,
					    WERR_OK, WERR_OK)) {
		ret = false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
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
	bool got_key = false;

	if (!_test_OpenKey(p, tctx, handle, key, access_mask, &new_handle,
			   open_werr, &got_key)) {
		return false;
	}

	if (!got_key) {
		return true;
	}

	if (!_test_GetSetSecurityDescriptor(p, tctx, &new_handle,
					    get_werr, set_werr)) {
		ret = false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
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

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_dacl_trustee_present(p, tctx, &new_handle, sid);

	test_CloseKey(p, tctx, &new_handle);

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

	if (!_test_OpenKey(p, tctx, handle, key, SEC_FLAG_SYSTEM_SECURITY,
			   &new_handle, WERR_OK, NULL)) {
		return false;
	}

	ret = test_sacl_trustee_present(p, tctx, &new_handle, sid);

	test_CloseKey(p, tctx, &new_handle);

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

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_owner_present(p, tctx, &new_handle, sid);

	test_CloseKey(p, tctx, &new_handle);

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

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	ret = test_group_present(p, tctx, &new_handle, sid);

	test_CloseKey(p, tctx, &new_handle);

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

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!test_SetKeySecurity(p, tctx, &new_handle, sd)) {
		ret = false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
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

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
		return false;
	}

	if (!test_GetKeySecurity(p, tctx, &new_handle, sd)) {
		ret = false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
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
	NTSTATUS status;
	bool ret = true;

	torture_comment(tctx, "SecurityDescriptor inheritance\n");

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
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

	status = security_descriptor_dacl_add(sd, ace);
	if (!NT_STATUS_IS_OK(status)) {
		printf("failed to add ace: %s\n", nt_errstr(status));
		return false;
	}

	/* FIXME: add further tests for these flags */
	sd->type |= SEC_DESC_DACL_AUTO_INHERIT_REQ |
		    SEC_DESC_SACL_AUTO_INHERITED;

	if (!test_SetKeySecurity(p, tctx, &new_handle, sd)) {
		return false;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("new ACE not present!\n");
		return false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
		return false;
	}

	if (!test_CreateKey(p, tctx, handle, TEST_SUBKEY_SD, NULL)) {
		ret = false;
		goto out;
	}

	if (!test_OpenKey(p, tctx, handle, TEST_SUBKEY_SD, &new_handle)) {
		ret = false;
		goto out;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("inherited ACE not present!\n");
		ret = false;
		goto out;
	}

	test_CloseKey(p, tctx, &new_handle);
	if (!test_CreateKey(p, tctx, handle, TEST_SUBSUBKEY_SD, NULL)) {
		ret = false;
		goto out;
	}

	if (!test_OpenKey(p, tctx, handle, TEST_SUBSUBKEY_SD, &new_handle)) {
		ret = false;
		goto out;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("inherited ACE not present!\n");
		ret = false;
		goto out;
	}

 out:
	test_CloseKey(p, tctx, &new_handle);
	test_Cleanup(p, tctx, handle, TEST_SUBKEY_SD);
	test_RestoreSecurity(p, tctx, handle, key, sd_orig);

	return true;
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
	NTSTATUS status;
	bool ret = true;
	uint8_t ace_flags = 0x0;

	torture_comment(tctx, "SecurityDescriptor inheritance block\n");

	if (!test_OpenKey(p, tctx, handle, key, &new_handle)) {
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

	status = security_descriptor_dacl_add(sd, ace);
	if (!NT_STATUS_IS_OK(status)) {
		printf("failed to add ace: %s\n", nt_errstr(status));
		return false;
	}

	if (!_test_SetKeySecurity(p, tctx, &new_handle, NULL, sd, WERR_OK)) {
		return false;
	}

	if (!test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("new ACE not present!\n");
		return false;
	}

	if (!test_CloseKey(p, tctx, &new_handle)) {
		return false;
	}

	if (!test_CreateKey(p, tctx, handle, TEST_SUBSUBKEY_SD, NULL)) {
		return false;
	}

	if (!test_OpenKey(p, tctx, handle, TEST_SUBSUBKEY_SD, &new_handle)) {
		ret = false;
		goto out;
	}

	if (test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("inherited ACE present but should not!\n");
		ret = false;
		goto out;
	}

	sid = dom_sid_parse_talloc(tctx, TEST_SID);
	if (sid == NULL) {
		return false;
	}

	if (test_dacl_trustee_present(p, tctx, &new_handle, sid)) {
		printf("inherited trustee SID present but should not!\n");
		ret = false;
		goto out;
	}

	test_CloseKey(p, tctx, &new_handle);

	if (!test_OpenKey(p, tctx, handle, TEST_SUBKEY_SD, &new_handle)) {
		ret = false;
		goto out;
	}

	if (test_dacl_ace_present(p, tctx, &new_handle, ace)) {
		printf("inherited ACE present but should not!\n");
		ret = false;
		goto out;
	}

	if (!test_dacl_trustee_flags_present(p, tctx, &new_handle, sid, ace_flags)) {
		printf("inherited trustee SID with flags 0x%02x not present!\n",
			ace_flags);
		ret = false;
		goto out;
	}

 out:
	test_CloseKey(p, tctx, &new_handle);
	test_Cleanup(p, tctx, handle, TEST_SUBKEY_SD);
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
	bool open_success = false;

	torture_comment(tctx, "SecurityDescriptor (%s) sets for secinfo: "
			"0x%08x, access_mask: 0x%08x\n",
			test, sec_info, access_mask);

	if (!_test_OpenKey(p, tctx, handle, key,
			   access_mask,
			   &new_handle,
			   WERR_OK,
			   &open_success)) {
		return false;
	}

	if (!open_success) {
		printf("key did not open\n");
		test_CloseKey(p, tctx, &new_handle);
		return false;
	}

	if (!_test_SetKeySecurity(p, tctx, &new_handle, &sec_info,
				  sd,
				  set_werr)) {
		torture_warning(tctx,
				"SetKeySecurity with secinfo: 0x%08x has failed\n",
				sec_info);
		smb_panic("");
		test_CloseKey(p, tctx, &new_handle);
		return false;
	}

	test_CloseKey(p, tctx, &new_handle);

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
				printf("test_SetSecurityDescriptor_SecInfo failed for OWNER\n");
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
				printf("test_SetSecurityDescriptor_SecInfo failed for GROUP\n");
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
				printf("test_SetSecurityDescriptor_SecInfo failed for DACL\n");
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
				printf("test_SetSecurityDescriptor_SecInfo failed for SACL\n");
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
		printf("test_SecurityDescriptor failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorInheritance(p, tctx, handle, key)) {
		printf("test_SecurityDescriptorInheritance failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorBlockInheritance(p, tctx, handle, key)) {
		printf("test_SecurityDescriptorBlockInheritance failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorsSecInfo(p, tctx, handle, key)) {
		printf("test_SecurityDescriptorsSecInfo failed\n");
		ret = false;
	}

	if (!test_SecurityDescriptorsMasks(p, tctx, handle, key)) {
		printf("test_SecurityDescriptorsMasks failed\n");
		ret = false;
	}

	return ret;
}

static bool test_DeleteKey(struct dcerpc_pipe *p, struct torture_context *tctx,
			   struct policy_handle *handle, const char *key)
{
	NTSTATUS status;
	struct winreg_DeleteKey r;

	r.in.handle = handle;
	init_winreg_String(&r.in.key, key);

	status = dcerpc_winreg_DeleteKey(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "DeleteKey failed");
	torture_assert_werr_ok(tctx, r.out.result, "DeleteKey failed");

	return true;
}

static bool test_QueryInfoKey(struct dcerpc_pipe *p,
			      struct torture_context *tctx,
			      struct policy_handle *handle, char *kclass)
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
				   dcerpc_winreg_QueryInfoKey(p, tctx, &r),
				   "QueryInfoKey failed");

	torture_assert_werr_ok(tctx, r.out.result, "QueryInfoKey failed");

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

		status = dcerpc_winreg_EnumKey(p, tctx, &r);

		if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(r.out.result)) {
			struct policy_handle key_handle;

			torture_comment(tctx, "EnumKey: %d: %s\n",
					r.in.enum_index,
					r.out.name->name);

			if (!test_OpenKey(p, tctx, handle, r.out.name->name,
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

static bool test_QueryMultipleValues(struct dcerpc_pipe *p,
				     struct torture_context *tctx,
				     struct policy_handle *handle,
				     const char *valuename)
{
	struct winreg_QueryMultipleValues r;
	NTSTATUS status;
	uint32_t bufsize=0;

	ZERO_STRUCT(r);
	r.in.key_handle = handle;
	r.in.values = r.out.values = talloc_array(tctx, struct QueryMultipleValue, 1);
	r.in.values[0].name = talloc(tctx, struct winreg_String);
	r.in.values[0].name->name = valuename;
	r.in.values[0].offset = 0;
	r.in.values[0].length = 0;
	r.in.values[0].type = 0;

	r.in.num_values = 1;
	r.in.buffer_size = r.out.buffer_size = talloc(tctx, uint32_t);
	*r.in.buffer_size = bufsize;
	do {
		*r.in.buffer_size = bufsize;
		r.in.buffer = r.out.buffer = talloc_zero_array(tctx, uint8_t,
							       *r.in.buffer_size);

		status = dcerpc_winreg_QueryMultipleValues(p, tctx, &r);

		if(NT_STATUS_IS_ERR(status))
			torture_fail(tctx, "QueryMultipleValues failed");

		talloc_free(r.in.buffer);
		bufsize += 0x20;
	} while (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA));

	torture_assert_werr_ok(tctx, r.out.result, "QueryMultipleValues failed");

	return true;
}

static bool test_QueryValue(struct dcerpc_pipe *p,
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

	status = dcerpc_winreg_QueryValue(p, tctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		torture_fail(tctx, "QueryValue failed");
	}

	torture_assert_werr_ok(tctx, r.out.result, "QueryValue failed");

	return true;
}

static bool test_EnumValue(struct dcerpc_pipe *p, struct torture_context *tctx,
			   struct policy_handle *handle, int max_valnamelen,
			   int max_valbufsize)
{
	struct winreg_EnumValue r;
	enum winreg_Type type = 0;
	uint32_t size = max_valbufsize, zero = 0;
	bool ret = true;
	uint8_t buf8;
	struct winreg_ValNameBuf name;

	name.name   = "";
	name.size   = 1024;

	ZERO_STRUCT(r);
	r.in.handle = handle;
	r.in.enum_index = 0;
	r.in.name = &name;
	r.out.name = &name;
	r.in.type = &type;
	r.in.value = &buf8;
	r.in.length = &zero;
	r.in.size = &size;

	do {
		torture_assert_ntstatus_ok(tctx,
					   dcerpc_winreg_EnumValue(p, tctx, &r),
					   "EnumValue failed");

		if (W_ERROR_IS_OK(r.out.result)) {
			ret &= test_QueryValue(p, tctx, handle,
					       r.out.name->name);
			ret &= test_QueryMultipleValues(p, tctx, handle,
							r.out.name->name);
		}

		r.in.enum_index++;
	} while (W_ERROR_IS_OK(r.out.result));

	torture_assert_werr_equal(tctx, r.out.result, WERR_NO_MORE_ITEMS,
				  "EnumValue failed");

	return ret;
}

static bool test_AbortSystemShutdown(struct dcerpc_pipe *p,
				     struct torture_context *tctx)
{
	struct winreg_AbortSystemShutdown r;
	uint16_t server = 0x0;

	ZERO_STRUCT(r);
	r.in.server = &server;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_AbortSystemShutdown(p, tctx, &r),
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

	ZERO_STRUCT(r);
	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_winreg_InitiateSystemShutdown(p, tctx, &r),
				   "InitiateSystemShutdown failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "InitiateSystemShutdown failed");

	return test_AbortSystemShutdown(p, tctx);
}


static bool test_InitiateSystemShutdownEx(struct torture_context *tctx,
					  struct dcerpc_pipe *p)
{
	struct winreg_InitiateSystemShutdownEx r;
	uint16_t hostname = 0x0;

	ZERO_STRUCT(r);
	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;
	r.in.reason = 0;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_InitiateSystemShutdownEx(p, tctx, &r),
		"InitiateSystemShutdownEx failed");

	torture_assert_werr_ok(tctx, r.out.result,
			       "InitiateSystemShutdownEx failed");

	return test_AbortSystemShutdown(p, tctx);
}
#define MAX_DEPTH 2		/* Only go this far down the tree */

static bool test_key(struct dcerpc_pipe *p, struct torture_context *tctx,
		     struct policy_handle *handle, int depth,
		     bool test_security)
{
	if (depth == MAX_DEPTH)
		return true;

	if (!test_QueryInfoKey(p, tctx, handle, NULL)) {
	}

	if (!test_NotifyChangeKeyValue(p, tctx, handle)) {
	}

	if (test_security && !test_GetKeySecurity(p, tctx, handle, NULL)) {
	}

	if (!test_EnumKey(p, tctx, handle, depth, test_security)) {
	}

	if (!test_EnumValue(p, tctx, handle, 0xFF, 0xFFFF)) {
	}

	test_CloseKey(p, tctx, handle);

	return true;
}

typedef NTSTATUS (*winreg_open_fn)(struct dcerpc_pipe *, TALLOC_CTX *, void *);

static bool test_Open_Security(struct torture_context *tctx,
			       struct dcerpc_pipe *p, void *userdata)
{
	struct policy_handle handle, newhandle;
	bool ret = true, created2 = false;
	bool created4 = false;
	struct winreg_OpenHKLM r;

	winreg_open_fn open_fn = userdata;

	ZERO_STRUCT(r);
	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx, open_fn(p, tctx, &r),
				   "open");

	test_Cleanup(p, tctx, &handle, TEST_KEY_BASE);

	if (!test_CreateKey(p, tctx, &handle, TEST_KEY_BASE, NULL)) {
		torture_comment(tctx,
				"CreateKey (TEST_KEY_BASE) failed\n");
	}

	if (test_CreateKey_sd(p, tctx, &handle, TEST_KEY2,
			      NULL, &newhandle)) {
		created2 = true;
	}

	if (created2 && !test_CloseKey(p, tctx, &newhandle)) {
		printf("CloseKey failed\n");
		ret = false;
	}

	if (test_CreateKey_sd(p, tctx, &handle, TEST_KEY4, NULL, &newhandle)) {
		created4 = true;
	}

	if (created4 && !test_CloseKey(p, tctx, &newhandle)) {
		printf("CloseKey failed\n");
		ret = false;
	}

	if (created4 && !test_SecurityDescriptors(p, tctx, &handle, TEST_KEY4)) {
		ret = false;
	}

	if (created4 && !test_DeleteKey(p, tctx, &handle, TEST_KEY4)) {
		printf("DeleteKey failed\n");
		ret = false;
	}

	if (created2 && !test_DeleteKey(p, tctx, &handle, TEST_KEY2)) {
		printf("DeleteKey failed\n");
		ret = false;
	}

	/* The HKCR hive has a very large fanout */
	if (open_fn == (void *)dcerpc_winreg_OpenHKCR) {
		if(!test_key(p, tctx, &handle, MAX_DEPTH - 1, true)) {
			ret = false;
		}
	} else {
		if (!test_key(p, tctx, &handle, 0, true)) {
			ret = false;
		}
	}

	test_Cleanup(p, tctx, &handle, TEST_KEY_BASE);

	return ret;
}

static bool test_Open(struct torture_context *tctx, struct dcerpc_pipe *p,
		      void *userdata)
{
	struct policy_handle handle, newhandle;
	bool ret = true, created = false, deleted = false;
	bool created3 = false, created_subkey = false;
	struct winreg_OpenHKLM r;

	winreg_open_fn open_fn = userdata;

	ZERO_STRUCT(r);
	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx, open_fn(p, tctx, &r),
				   "open");

	test_Cleanup(p, tctx, &handle, TEST_KEY_BASE);

	if (!test_CreateKey(p, tctx, &handle, TEST_KEY_BASE, NULL)) {
		torture_comment(tctx,
				"CreateKey (TEST_KEY_BASE) failed\n");
	}

	if (!test_CreateKey(p, tctx, &handle, TEST_KEY1, NULL)) {
		torture_comment(tctx,
				"CreateKey failed - not considering a failure\n");
	} else {
		created = true;
	}

	if (created && !test_FlushKey(p, tctx, &handle)) {
		torture_comment(tctx, "FlushKey failed\n");
		ret = false;
	}

	if (created && !test_OpenKey(p, tctx, &handle, TEST_KEY1, &newhandle))
		torture_fail(tctx,
			     "CreateKey failed (OpenKey after Create didn't work)\n");

	if (created && !test_CloseKey(p, tctx, &newhandle))
		torture_fail(tctx,
			     "CreateKey failed (CloseKey after Open didn't work)\n");

	if (created && !test_DeleteKey(p, tctx, &handle, TEST_KEY1)) {
		torture_comment(tctx, "DeleteKey failed\n");
		ret = false;
	} else {
		deleted = true;
	}

	if (created && !test_FlushKey(p, tctx, &handle)) {
		torture_comment(tctx, "FlushKey failed\n");
		ret = false;
	}

	if (created && deleted &&
	    !_test_OpenKey(p, tctx, &handle, TEST_KEY1,
			   SEC_FLAG_MAXIMUM_ALLOWED, &newhandle,
			   WERR_BADFILE, NULL)) {
		torture_comment(tctx,
				"DeleteKey failed (OpenKey after Delete "
				"did not return WERR_BADFILE)\n");
		ret = false;
	}

	if (!test_GetVersion(p, tctx, &handle)) {
		torture_comment(tctx, "GetVersion failed\n");
		ret = false;
	}

	if (created && test_CreateKey(p, tctx, &handle, TEST_KEY3, NULL)) {
		created3 = true;
	}

	if (created3 &&
	    test_CreateKey(p, tctx, &handle, TEST_SUBKEY, NULL)) {
		created_subkey = true;
	}

	if (created_subkey &&
	    !test_DeleteKey(p, tctx, &handle, TEST_KEY3)) {
		printf("DeleteKey failed\n");
		ret = false;
	}

	/* The HKCR hive has a very large fanout */
	if (open_fn == (void *)dcerpc_winreg_OpenHKCR) {
		if(!test_key(p, tctx, &handle, MAX_DEPTH - 1, false)) {
			ret = false;
		}
	} else {
		if (!test_key(p, tctx, &handle, 0, false)) {
			ret = false;
		}
	}

	test_Cleanup(p, tctx, &handle, TEST_KEY_BASE);

	return ret;
}

struct torture_suite *torture_rpc_winreg(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "WINREG");
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "winreg",
						  &ndr_table_winreg);

	test = torture_rpc_tcase_add_test(tcase, "InitiateSystemShutdown",
					  test_InitiateSystemShutdown);
	test->dangerous = true;

	test = torture_rpc_tcase_add_test(tcase, "InitiateSystemShutdownEx",
					  test_InitiateSystemShutdownEx);
	test->dangerous = true;

	/* Basic tests without security descriptors */
	torture_rpc_tcase_add_test_ex(tcase, "HKLM-basic",
				      test_Open,
				      (winreg_open_fn)dcerpc_winreg_OpenHKLM);
	torture_rpc_tcase_add_test_ex(tcase, "HKU-basic",
				      test_Open,
				      (winreg_open_fn)dcerpc_winreg_OpenHKU);
	torture_rpc_tcase_add_test_ex(tcase, "HKCR-basic",
				      test_Open,
				      (winreg_open_fn)dcerpc_winreg_OpenHKCR);
	torture_rpc_tcase_add_test_ex(tcase, "HKCU-basic",
				      test_Open,
				      (winreg_open_fn)dcerpc_winreg_OpenHKCU);

	/* Security descriptor tests */
	torture_rpc_tcase_add_test_ex(tcase, "HKLM-security",
				      test_Open_Security,
				      (winreg_open_fn)dcerpc_winreg_OpenHKLM);
	torture_rpc_tcase_add_test_ex(tcase, "HKU-security",
				      test_Open_Security,
				      (winreg_open_fn)dcerpc_winreg_OpenHKU);
	torture_rpc_tcase_add_test_ex(tcase, "HKCR-security",
				      test_Open_Security,
				      (winreg_open_fn)dcerpc_winreg_OpenHKCR);
	torture_rpc_tcase_add_test_ex(tcase, "HKCU-security",
				      test_Open_Security,
				      (winreg_open_fn)dcerpc_winreg_OpenHKCU);

	return suite;
}
