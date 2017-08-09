/*
   Unix SMB/CIFS implementation.
   Headers for the async winbind client library
   Copyright (C) Volker Lendecke 2008

     ** NOTE! The following LGPL license applies to the wbclient
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WBC_ASYNC_H_
#define _WBC_ASYNC_H_

#include <talloc.h>
#include <tevent.h>
#include "nsswitch/libwbclient/wbclient.h"
#include "nsswitch/wb_reqtrans.h"

struct wb_context;
struct winbindd_request;
struct winbindd_response;

enum wbcDebugLevel {
	WBC_DEBUG_FATAL,
	WBC_DEBUG_ERROR,
	WBC_DEBUG_WARNING,
	WBC_DEBUG_TRACE
};

struct tevent_req *wb_trans_send(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev,
				 struct wb_context *wb_ctx, bool need_priv,
				 struct winbindd_request *wb_req);
wbcErr wb_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
		     struct winbindd_response **presponse);
struct wb_context *wb_context_init(TALLOC_CTX *mem_ctx, const char* dir);
int wbcSetDebug(struct wb_context *wb_ctx,
		void (*debug)(void *context,
			      enum wbcDebugLevel level,
			      const char *fmt,
			      va_list ap) PRINTF_ATTRIBUTE(3,0),
		void *context);
int wbcSetDebugStderr(struct wb_context *wb_ctx);
void wbcDebug(struct wb_context *wb_ctx, enum wbcDebugLevel level,
	      const char *fmt, ...) PRINTF_ATTRIBUTE(3,0);

/* Definitions from wb_reqtrans.c */
wbcErr map_wbc_err_from_errno(int error);

bool tevent_req_is_wbcerr(struct tevent_req *req, wbcErr *pwbc_err);
wbcErr tevent_req_simple_recv_wbcerr(struct tevent_req *req);

/* Async functions from wbc_idmap.c */

struct tevent_req *wbcSidToUid_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct wb_context *wb_ctx,
				    const struct wbcDomainSid *sid);
wbcErr wbcSidToUid_recv(struct tevent_req *req, uid_t *puid);

struct tevent_req *wbcUidToSid_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct wb_context *wb_ctx,
				    uid_t uid);
wbcErr wbcUidToSid_recv(struct tevent_req *req, struct wbcDomainSid *psid);

struct tevent_req *wbcSidToGid_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct wb_context *wb_ctx,
				    const struct wbcDomainSid *sid);
wbcErr wbcSidToGid_recv(struct tevent_req *req, gid_t *pgid);

struct tevent_req *wbcGidToSid_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct wb_context *wb_ctx,
				    gid_t gid);
wbcErr wbcGidToSid_recv(struct tevent_req *req, struct wbcDomainSid *psid);

/* Async functions from wbc_pam.c */
struct tevent_req *wbcAuthenticateUserEx_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct wb_context *wb_ctx,
					const struct wbcAuthUserParams *params);
wbcErr wbcAuthenticateUserEx_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  struct wbcAuthUserInfo **info,
				  struct wbcAuthErrorInfo **error);

/* Async functions from wbc_sid.c */
struct tevent_req *wbcLookupName_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wb_context *wb_ctx,
				      const char *domain,
				      const char *name);
wbcErr wbcLookupName_recv(struct tevent_req *req,
			  struct wbcDomainSid *sid,
			  enum wbcSidType *name_type);
struct tevent_req *wbcLookupSid_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct wb_context *wb_ctx,
				     const struct wbcDomainSid *sid);
wbcErr wbcLookupSid_recv(struct tevent_req *req,
			 TALLOC_CTX *mem_ctx,
			 char **pdomain,
			 char **pname,
			 enum wbcSidType *pname_type);

/* Async functions from wbc_util.c */

struct tevent_req *wbcPing_send(TALLOC_CTX *mem_ctx,
			       struct tevent_context *ev,
			       struct wb_context *wb_ctx);
wbcErr wbcPing_recv(struct tevent_req *req);

struct tevent_req *wbcInterfaceVersion_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct wb_context *wb_ctx);
wbcErr wbcInterfaceVersion_recv(struct tevent_req *req,
			        uint32_t *interface_version);

struct tevent_req *wbcInfo_send(TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				struct wb_context *wb_ctx);
wbcErr wbcInfo_recv(struct tevent_req *req,
		    TALLOC_CTX *mem_ctx,
		    char *winbind_separator,
		    char **version_string);

struct tevent_req *wbcNetbiosName_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct wb_context *wb_ctx);
wbcErr wbcNetbiosName_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   char **netbios_name);

struct tevent_req *wbcDomainName_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wb_context *wb_ctx);
wbcErr wbcDomainName_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  char **netbios_name);

struct tevent_req *wbcInterfaceDetails_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct wb_context *wb_ctx);
wbcErr wbcInterfaceDetails_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				struct wbcInterfaceDetails **details);

struct tevent_req *wbcDomainInfo_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wb_context *wb_ctx,
				      const char *domain);
wbcErr wbcDomainInfo_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  struct wbcDomainInfo **dinfo);

#endif /*_WBC_ASYNC_H_*/
