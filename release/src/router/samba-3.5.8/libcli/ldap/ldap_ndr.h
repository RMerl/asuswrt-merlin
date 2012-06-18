#ifndef __LIBCLI_LDAP_LDAP_NDR_H__
#define __LIBCLI_LDAP_LDAP_NDR_H__

#include "librpc/gen_ndr/ndr_misc.h"

char *ldap_encode_ndr_uint32(TALLOC_CTX *mem_ctx, uint32_t value);
char *ldap_encode_ndr_dom_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);
char *ldap_encode_ndr_GUID(TALLOC_CTX *mem_ctx, struct GUID *guid);
NTSTATUS ldap_decode_ndr_GUID(TALLOC_CTX *mem_ctx, struct ldb_val val, struct GUID *guid);

#endif /* __LIBCLI_LDAP_LDAP_NDR_H__ */

