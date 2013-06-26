/*
   Unix SMB/CIFS implementation.

   endpoint server for the browser pipe

   Copyright (C) Stefan Metzmacher 2008

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
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_browser.h"


/*
  BrowserrServerEnum
*/
static void dcesrv_BrowserrServerEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrServerEnum *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrDebugCall
*/
static void dcesrv_BrowserrDebugCall(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrDebugCall *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrQueryOtherDomains
*/
static WERROR dcesrv_BrowserrQueryOtherDomains(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrQueryOtherDomains *r)
{
	struct BrowserrSrvInfo100Ctr *ctr100;

	switch (r->in.info->level) {
	case 100:
		if (!r->in.info->info.info100) {
			return WERR_INVALID_PARAM;
		}

		ctr100 = talloc(mem_ctx, struct BrowserrSrvInfo100Ctr);
		W_ERROR_HAVE_NO_MEMORY(ctr100);

		ctr100->entries_read = 0;
		ctr100->entries = talloc_zero_array(ctr100, struct srvsvc_NetSrvInfo100,
						    ctr100->entries_read);
		W_ERROR_HAVE_NO_MEMORY(ctr100->entries);

		r->out.info->info.info100 = ctr100;
		*r->out.total_entries = ctr100->entries_read;
		return WERR_OK;
	default:
		return WERR_UNKNOWN_LEVEL;
	}
}


/*
  BrowserrResetNetlogonState
*/
static void dcesrv_BrowserrResetNetlogonState(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrResetNetlogonState *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrDebugTrace
*/
static void dcesrv_BrowserrDebugTrace(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrDebugTrace *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrQueryStatistics
*/
static void dcesrv_BrowserrQueryStatistics(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrQueryStatistics *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserResetStatistics
*/
static void dcesrv_BrowserResetStatistics(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserResetStatistics *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  NetrBrowserStatisticsClear
*/
static void dcesrv_NetrBrowserStatisticsClear(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NetrBrowserStatisticsClear *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  NetrBrowserStatisticsGet
*/
static void dcesrv_NetrBrowserStatisticsGet(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NetrBrowserStatisticsGet *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrSetNetlogonState
*/
static void dcesrv_BrowserrSetNetlogonState(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrSetNetlogonState *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrQueryEmulatedDomains
*/
static void dcesrv_BrowserrQueryEmulatedDomains(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrQueryEmulatedDomains *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  BrowserrServerEnumEx
*/
static void dcesrv_BrowserrServerEnumEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct BrowserrServerEnumEx *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_browser_s.c"
