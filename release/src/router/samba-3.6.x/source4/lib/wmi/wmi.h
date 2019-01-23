/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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

#ifndef _WMI_H_
#define _WMI_H_

#include "librpc/gen_ndr/com_wmi.h"

/* The following definitions come from lib/wmi/wmicore.c  */


/** FIXME: Use credentials struct rather than user/password here */
WERROR WBEM_ConnectServer(struct com_context *ctx, const char *server, const char *nspace, 
			  struct cli_credentials *credentials,
			  const char *locale, uint32_t flags, const char *authority, 
			  struct IWbemContext* wbem_ctx, struct IWbemServices** services);
const char *wmi_errstr(WERROR werror);

/* The following definitions come from lib/wmi/wbemdata.c  */

WERROR IWbemClassObject_GetMethod(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, struct IWbemClassObject **in, struct IWbemClassObject **out);
void WbemClassObject_CreateInstance(struct IWbemClassObject *wco);
WERROR IWbemClassObject_Clone(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject **copy);
WERROR IWbemClassObject_SpawnInstance(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, uint32_t flags, struct IWbemClassObject **instance);
WERROR IWbemClassObject_Get(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, union CIMVAR *val, enum CIMTYPE_ENUMERATION *cimtype, uint32_t *flavor);
WERROR IWbemClassObject_Put(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, union CIMVAR *val, enum CIMTYPE_ENUMERATION cimtype);
WERROR IEnumWbemClassObject_SmartNext(struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, uint32_t uCount, struct IWbemClassObject **apObjects, uint32_t *puReturned);
struct composite_context *dcom_proxy_IEnumWbemClassObject_Release_send(struct IUnknown *d, TALLOC_CTX *mem_ctx);

void wmi_init(struct com_context **ctx, struct cli_credentials *credentials);

#endif
