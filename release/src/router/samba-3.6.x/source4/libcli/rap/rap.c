/*
   Unix SMB/CIFS implementation.
   RAP client
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Tim Potter 2005
   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Guenther Deschner 2010-2011

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
#include "libcli/libcli.h"
#include "../librpc/gen_ndr/ndr_rap.h"
#include "libcli/rap/rap.h"
#include "librpc/ndr/libndr.h"

struct rap_call *new_rap_cli_call(TALLOC_CTX *mem_ctx, uint16_t callno)
{
	struct rap_call *call;

	call = talloc(mem_ctx, struct rap_call);

	if (call == NULL)
		return NULL;

	call->callno = callno;
	call->rcv_paramlen = 4;

	call->paramdesc = NULL;
	call->datadesc = NULL;
	call->auxdatadesc = NULL;

	call->ndr_push_param = ndr_push_init_ctx(mem_ctx);
	call->ndr_push_param->flags = RAPNDR_FLAGS;

	call->ndr_push_data = ndr_push_init_ctx(mem_ctx);
	call->ndr_push_data->flags = RAPNDR_FLAGS;

	return call;
}

static void rap_cli_push_paramdesc(struct rap_call *call, char desc)
{
	int len = 0;

	if (call->paramdesc != NULL)
		len = strlen(call->paramdesc);

	call->paramdesc = talloc_realloc(call,
					 call->paramdesc,
					 char,
					 len+2);

	call->paramdesc[len] = desc;
	call->paramdesc[len+1] = '\0';
}

static void rap_cli_push_word(struct rap_call *call, uint16_t val)
{
	rap_cli_push_paramdesc(call, 'W');
	ndr_push_uint16(call->ndr_push_param, NDR_SCALARS, val);
}

static void rap_cli_push_dword(struct rap_call *call, uint32_t val)
{
	rap_cli_push_paramdesc(call, 'D');
	ndr_push_uint32(call->ndr_push_param, NDR_SCALARS, val);
}

static void rap_cli_push_rcvbuf(struct rap_call *call, int len)
{
	rap_cli_push_paramdesc(call, 'r');
	rap_cli_push_paramdesc(call, 'L');
	ndr_push_uint16(call->ndr_push_param, NDR_SCALARS, len);
	call->rcv_datalen = len;
}

static void rap_cli_push_sendbuf(struct rap_call *call, int len)
{
	rap_cli_push_paramdesc(call, 's');
	rap_cli_push_paramdesc(call, 'T');
	ndr_push_uint16(call->ndr_push_param, NDR_SCALARS, len);
}

static void rap_cli_push_param(struct rap_call *call, uint16_t val)
{
	rap_cli_push_paramdesc(call, 'P');
	ndr_push_uint16(call->ndr_push_param, NDR_SCALARS, val);
}

static void rap_cli_expect_multiple_entries(struct rap_call *call)
{
	rap_cli_push_paramdesc(call, 'e');
	rap_cli_push_paramdesc(call, 'h');
	call->rcv_paramlen += 4; /* uint16_t entry count, uint16_t total */
}

static void rap_cli_expect_word(struct rap_call *call)
{
	rap_cli_push_paramdesc(call, 'h');
	call->rcv_paramlen += 2;
}

static void rap_cli_push_string(struct rap_call *call, const char *str)
{
	if (str == NULL) {
		rap_cli_push_paramdesc(call, 'O');
		return;
	}
	rap_cli_push_paramdesc(call, 'z');
	ndr_push_string(call->ndr_push_param, NDR_SCALARS, str);
}

static void rap_cli_expect_format(struct rap_call *call, const char *format)
{
	call->datadesc = format;
}

static void rap_cli_expect_extra_format(struct rap_call *call, const char *format)
{
	call->auxdatadesc = format;
}

static NTSTATUS rap_pull_string(TALLOC_CTX *mem_ctx, struct ndr_pull *ndr,
				uint16_t convert, const char **dest)
{
	uint16_t string_offset;
	uint16_t ignore;
	const char *p;
	size_t len;

	NDR_RETURN(ndr_pull_uint16(ndr, NDR_SCALARS, &string_offset));
	NDR_RETURN(ndr_pull_uint16(ndr, NDR_SCALARS, &ignore));

	string_offset -= convert;

	if (string_offset+1 > ndr->data_size)
		return NT_STATUS_INVALID_PARAMETER;

	p = (const char *)(ndr->data + string_offset);
	len = strnlen(p, ndr->data_size-string_offset);

	if ( string_offset + len + 1 >  ndr->data_size )
		return NT_STATUS_INVALID_PARAMETER;

	*dest = talloc_zero_array(mem_ctx, char, len+1);
	pull_string(discard_const_p(char, *dest), p, len+1, len, STR_ASCII);

	return NT_STATUS_OK;
}

NTSTATUS rap_cli_do_call(struct smbcli_tree *tree,
			 struct rap_call *call)
{
	NTSTATUS result;
	DATA_BLOB param_blob;
	DATA_BLOB data_blob;
	struct ndr_push *params;
	struct ndr_push *data;
	struct smb_trans2 trans;

	params = ndr_push_init_ctx(call);

	if (params == NULL)
		return NT_STATUS_NO_MEMORY;

	params->flags = RAPNDR_FLAGS;

	data = ndr_push_init_ctx(call);

	if (data == NULL)
		return NT_STATUS_NO_MEMORY;

	data->flags = RAPNDR_FLAGS;

	trans.in.max_param = call->rcv_paramlen;
	trans.in.max_data = call->rcv_datalen;
	trans.in.max_setup = 0;
	trans.in.flags = 0;
	trans.in.timeout = 0;
	trans.in.setup_count = 0;
	trans.in.setup = NULL;
	trans.in.trans_name = "\\PIPE\\LANMAN";

	NDR_RETURN(ndr_push_uint16(params, NDR_SCALARS, call->callno));
	if (call->paramdesc)
		NDR_RETURN(ndr_push_string(params, NDR_SCALARS, call->paramdesc));
	if (call->datadesc)
		NDR_RETURN(ndr_push_string(params, NDR_SCALARS, call->datadesc));

	param_blob = ndr_push_blob(call->ndr_push_param);
	NDR_RETURN(ndr_push_bytes(params, param_blob.data,
				 param_blob.length));

	data_blob = ndr_push_blob(call->ndr_push_data);
	NDR_RETURN(ndr_push_bytes(data, data_blob.data,
				 data_blob.length));

	if (call->auxdatadesc)
		NDR_RETURN(ndr_push_string(params, NDR_SCALARS, call->auxdatadesc));

	trans.in.params = ndr_push_blob(params);
	trans.in.data = ndr_push_blob(data);

	result = smb_raw_trans(tree, call, &trans);

	if (!NT_STATUS_IS_OK(result))
		return result;

	call->ndr_pull_param = ndr_pull_init_blob(&trans.out.params, call);
	call->ndr_pull_param->flags = RAPNDR_FLAGS;

	call->ndr_pull_data = ndr_pull_init_blob(&trans.out.data, call);
	call->ndr_pull_data->flags = RAPNDR_FLAGS;

	return result;
}


NTSTATUS smbcli_rap_netshareenum(struct smbcli_tree *tree,
				 TALLOC_CTX *mem_ctx,
				 struct rap_NetShareEnum *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	int i;

	call = new_rap_cli_call(tree, RAP_WshareEnum);

	if (call == NULL)
		return NT_STATUS_NO_MEMORY;

	rap_cli_push_word(call, r->in.level); /* Level */
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B13");
		break;
	case 1:
		rap_cli_expect_format(call, "B13BWz");
		break;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetShareEnum, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	r->out.info = talloc_array(mem_ctx, union rap_share_info, r->out.count);

	if (r->out.info == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<r->out.count; i++) {
		switch(r->in.level) {
		case 0:
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
						r->out.info[i].info0.share_name, 13));
			break;
		case 1:
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
						r->out.info[i].info1.share_name, 13));
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
					        &r->out.info[i].info1.reserved1, 1));
			NDR_GOTO(ndr_pull_uint16(call->ndr_pull_data,
					       NDR_SCALARS, &r->out.info[i].info1.share_type));
			RAP_GOTO(rap_pull_string(mem_ctx, call->ndr_pull_data,
					       r->out.convert,
					       &r->out.info[i].info1.comment));
			break;
		}
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetShareEnum, r);
	}
	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netserverenum2(struct smbcli_tree *tree,
				   TALLOC_CTX *mem_ctx,
				   struct rap_NetServerEnum2 *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	int i;

	call = new_rap_cli_call(mem_ctx, RAP_NetServerEnum2);

	if (call == NULL)
		return NT_STATUS_NO_MEMORY;

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);
	rap_cli_push_dword(call, r->in.servertype);
	rap_cli_push_string(call, r->in.domain);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B16");
		break;
	case 1:
		rap_cli_expect_format(call, "B16BBDz");
		break;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetServerEnum2, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	r->out.info = talloc_array(mem_ctx, union rap_server_info, r->out.count);

	if (r->out.info == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<r->out.count; i++) {
		switch(r->in.level) {
		case 0:
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
						r->out.info[i].info0.name, 16));
			break;
		case 1:
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
						r->out.info[i].info1.name, 16));
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
					      &r->out.info[i].info1.version_major, 1));
			NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
					      &r->out.info[i].info1.version_minor, 1));
			NDR_GOTO(ndr_pull_uint32(call->ndr_pull_data,
					       NDR_SCALARS, &r->out.info[i].info1.servertype));
			RAP_GOTO(rap_pull_string(mem_ctx, call->ndr_pull_data,
					       r->out.convert,
					       &r->out.info[i].info1.comment));
		}
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetServerEnum2, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netservergetinfo(struct smbcli_tree *tree,
				     TALLOC_CTX *mem_ctx,
				     struct rap_WserverGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WserverGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B16");
		break;
	case 1:
		rap_cli_expect_format(call, "B16BBDz");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_WserverGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	switch(r->in.level) {
	case 0:
		NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
					r->out.info.info0.name, 16));
		break;
	case 1:
		NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
					r->out.info.info1.name, 16));
		NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
				      &r->out.info.info1.version_major, 1));
		NDR_GOTO(ndr_pull_bytes(call->ndr_pull_data,
				      &r->out.info.info1.version_minor, 1));
		NDR_GOTO(ndr_pull_uint32(call->ndr_pull_data,
				       NDR_SCALARS, &r->out.info.info1.servertype));
		RAP_GOTO(rap_pull_string(mem_ctx, call->ndr_pull_data,
				       r->out.convert,
				       &r->out.info.info1.comment));
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_WserverGetInfo, r);
	}
 done:
	talloc_free(call);
	return result;
}

static enum ndr_err_code ndr_pull_rap_NetPrintQEnum_data(struct ndr_pull *ndr, struct rap_NetPrintQEnum *r)
{
	uint32_t cntr_info_0;
	TALLOC_CTX *_mem_save_info_0;

	NDR_PULL_ALLOC_N(ndr, r->out.info, r->out.count);
	_mem_save_info_0 = NDR_PULL_GET_MEM_CTX(ndr);
	NDR_PULL_SET_MEM_CTX(ndr, r->out.info, 0);
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->out.info[cntr_info_0], r->in.level));
		NDR_CHECK(ndr_pull_rap_printq_info(ndr, NDR_SCALARS, &r->out.info[cntr_info_0]));
	}
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_rap_printq_info(ndr, NDR_BUFFERS, &r->out.info[cntr_info_0]));
	}
	NDR_PULL_SET_MEM_CTX(ndr, _mem_save_info_0, 0);

	return NDR_ERR_SUCCESS;
}

NTSTATUS smbcli_rap_netprintqenum(struct smbcli_tree *tree,
				  TALLOC_CTX *mem_ctx,
				  struct rap_NetPrintQEnum *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintQEnum))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B13");
		break;
	case 1:
		rap_cli_expect_format(call, "B13BWWWzzzzzWW");
		break;
	case 2:
		rap_cli_expect_format(call, "B13BWWWzzzzzWN");
		rap_cli_expect_extra_format(call, "WB21BB16B10zWWzDDz");
		break;
	case 3:
		rap_cli_expect_format(call, "zWWWWzzzzWWzzl");
		break;
	case 4:
		rap_cli_expect_format(call, "zWWWWzzzzWNzzl");
		rap_cli_expect_extra_format(call, "WWzWWDDzz");
		/* no mention of extra format in MS-RAP */
		break;
	case 5:
		rap_cli_expect_format(call, "z");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintQEnum, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_rap_NetPrintQEnum_data(call->ndr_pull_data, r));

	r->out.info = talloc_steal(mem_ctx, r->out.info);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintQEnum, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintqgetinfo(struct smbcli_tree *tree,
				     TALLOC_CTX *mem_ctx,
				     struct rap_NetPrintQGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintQGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintQueueName);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B13");
		break;
	case 1:
		rap_cli_expect_format(call, "B13BWWWzzzzzWW");
		break;
	case 2:
		rap_cli_expect_format(call, "B13BWWWzzzzzWN");
		rap_cli_expect_extra_format(call, "WB21BB16B10zWWzDDz");
		break;
	case 3:
		rap_cli_expect_format(call, "zWWWWzzzzWWzzl");
		break;
	case 4:
		rap_cli_expect_format(call, "zWWWWzzzzWNzzl");
		rap_cli_expect_extra_format(call, "WWzWWDDzz");
		/* no mention of extra format in MS-RAP */
		break;
	case 5:
		rap_cli_expect_format(call, "z");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintQGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	ZERO_STRUCT(r->out);

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	if (r->out.status == 0) {
		call->ndr_pull_data->relative_rap_convert = r->out.convert;

		NDR_GOTO(ndr_pull_set_switch_value(call->ndr_pull_data, &r->out.info, r->in.level));
		NDR_GOTO(ndr_pull_rap_printq_info(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.info));
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintQGetInfo, r);
	}

	result = NT_STATUS_OK;
 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintjobpause(struct smbcli_tree *tree,
				     TALLOC_CTX *mem_ctx,
				     struct rap_NetPrintJobPause *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobPause))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.JobID);

	rap_cli_expect_format(call, "W");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobPause, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobPause, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintjobcontinue(struct smbcli_tree *tree,
					TALLOC_CTX *mem_ctx,
					struct rap_NetPrintJobContinue *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobContinue))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.JobID);

	rap_cli_expect_format(call, "W");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobContinue, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobContinue, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintjobdelete(struct smbcli_tree *tree,
				      TALLOC_CTX *mem_ctx,
				      struct rap_NetPrintJobDelete *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobDel))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.JobID);

	rap_cli_expect_format(call, "W");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobDelete, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobDelete, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintqueuepause(struct smbcli_tree *tree,
				       TALLOC_CTX *mem_ctx,
				       struct rap_NetPrintQueuePause *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintQPause))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintQueueName);

	rap_cli_expect_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintQueuePause, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintQueuePause, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintqueueresume(struct smbcli_tree *tree,
					TALLOC_CTX *mem_ctx,
					struct rap_NetPrintQueueResume *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintQContinue))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintQueueName);

	rap_cli_expect_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintQueueResume, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintQueueResume, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintqueuepurge(struct smbcli_tree *tree,
				       TALLOC_CTX *mem_ctx,
				       struct rap_NetPrintQueuePurge *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintQPurge))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintQueueName);

	rap_cli_expect_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintQueuePurge, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintQueuePurge, r);
	}

 done:
	talloc_free(call);
	return result;
}

static enum ndr_err_code ndr_pull_rap_NetPrintJobEnum_data(struct ndr_pull *ndr, struct rap_NetPrintJobEnum *r)
{
	uint32_t cntr_info_0;
	TALLOC_CTX *_mem_save_info_0;

	NDR_PULL_ALLOC_N(ndr, r->out.info, r->out.count);
	_mem_save_info_0 = NDR_PULL_GET_MEM_CTX(ndr);
	NDR_PULL_SET_MEM_CTX(ndr, r->out.info, 0);
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->out.info[cntr_info_0], r->in.level));
		NDR_CHECK(ndr_pull_rap_printj_info(ndr, NDR_SCALARS, &r->out.info[cntr_info_0]));
	}
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_rap_printj_info(ndr, NDR_BUFFERS, &r->out.info[cntr_info_0]));
	}
	NDR_PULL_SET_MEM_CTX(ndr, _mem_save_info_0, 0);

	return NDR_ERR_SUCCESS;
}

NTSTATUS smbcli_rap_netprintjobenum(struct smbcli_tree *tree,
				    TALLOC_CTX *mem_ctx,
				    struct rap_NetPrintJobEnum *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobEnum))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintQueueName);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "W");
		break;
	case 1:
		rap_cli_expect_format(call, "WB21BB16B10zWWzDDz");
		break;
	case 2:
		rap_cli_expect_format(call, "WWzWWDDzz");
		break;
	case 3:
		rap_cli_expect_format(call, "WWzWWDDzzzzzzzzzzlz");
		break;
	case 4:
		rap_cli_expect_format(call, "WWzWWDDzzzzzDDDDDDD");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobEnum, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_rap_NetPrintJobEnum_data(call->ndr_pull_data, r));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobEnum, r);
	}

	r->out.info = talloc_steal(mem_ctx, r->out.info);

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintjobgetinfo(struct smbcli_tree *tree,
				       TALLOC_CTX *mem_ctx,
				       struct rap_NetPrintJobGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.JobID);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "W");
		break;
	case 1:
		rap_cli_expect_format(call, "WB21BB16B10zWWzDDz");
		break;
	case 2:
		rap_cli_expect_format(call, "WWzWWDDzz");
		break;
	case 3:
		rap_cli_expect_format(call, "WWzWWDDzzzzzzzzzzlz");
		break;
	case 4:
		rap_cli_expect_format(call, "WWzWWDDzzzzzDDDDDDD");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_set_switch_value(call->ndr_pull_data, &r->out.info, r->in.level));
	NDR_GOTO(ndr_pull_rap_printj_info(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.info));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobGetInfo, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintjobsetinfo(struct smbcli_tree *tree,
				       TALLOC_CTX *mem_ctx,
				       struct rap_NetPrintJobSetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintJobSetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.JobID);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_sendbuf(call, r->in.bufsize);
	rap_cli_push_param(call, r->in.ParamNum);

	switch (r->in.ParamNum) {
	case RAP_PARAM_JOBNUM:
	case RAP_PARAM_JOBPOSITION:
	case RAP_PARAM_JOBSTATUS:
		NDR_GOTO(ndr_push_uint16(call->ndr_push_param, NDR_SCALARS, r->in.Param.value));
		break;
	case RAP_PARAM_USERNAME:
	case RAP_PARAM_NOTIFYNAME:
	case RAP_PARAM_DATATYPE:
	case RAP_PARAM_PARAMETERS_STRING:
	case RAP_PARAM_JOBSTATUSSTR:
	case RAP_PARAM_JOBCOMMENT:
		NDR_GOTO(ndr_push_string(call->ndr_push_param, NDR_SCALARS, r->in.Param.string));
		break;
	case RAP_PARAM_TIMESUBMITTED:
	case RAP_PARAM_JOBSIZE:
		NDR_GOTO(ndr_push_uint32(call->ndr_push_param, NDR_SCALARS, r->in.Param.value4));
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	/* not really sure if this is correct */
	rap_cli_expect_format(call, "WB21BB16B10zWWzDDz");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintJobSetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintJobSetInfo, r);
	}

 done:
	talloc_free(call);
	return result;
}

static enum ndr_err_code ndr_pull_rap_NetPrintDestEnum_data(struct ndr_pull *ndr, struct rap_NetPrintDestEnum *r)
{
	uint32_t cntr_info_0;
	TALLOC_CTX *_mem_save_info_0;

	NDR_PULL_ALLOC_N(ndr, r->out.info, r->out.count);
	_mem_save_info_0 = NDR_PULL_GET_MEM_CTX(ndr);
	NDR_PULL_SET_MEM_CTX(ndr, r->out.info, 0);
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->out.info[cntr_info_0], r->in.level));
		NDR_CHECK(ndr_pull_rap_printdest_info(ndr, NDR_SCALARS, &r->out.info[cntr_info_0]));
	}
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_rap_printdest_info(ndr, NDR_BUFFERS, &r->out.info[cntr_info_0]));
	}
	NDR_PULL_SET_MEM_CTX(ndr, _mem_save_info_0, 0);

	return NDR_ERR_SUCCESS;
}


NTSTATUS smbcli_rap_netprintdestenum(struct smbcli_tree *tree,
				     TALLOC_CTX *mem_ctx,
				     struct rap_NetPrintDestEnum *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintDestEnum))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B9");
		break;
	case 1:
		rap_cli_expect_format(call, "B9B21WWzW");
		break;
	case 2:
		rap_cli_expect_format(call, "z");
		break;
	case 3:
		rap_cli_expect_format(call, "zzzWWzzzWW");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintDestEnum, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_rap_NetPrintDestEnum_data(call->ndr_pull_data, r));

	r->out.info = talloc_steal(mem_ctx, r->out.info);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintDestEnum, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netprintdestgetinfo(struct smbcli_tree *tree,
					TALLOC_CTX *mem_ctx,
					struct rap_NetPrintDestGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WPrintDestGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.PrintDestName);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B9");
		break;
	case 1:
		rap_cli_expect_format(call, "B9B21WWzW");
		break;
	case 2:
		rap_cli_expect_format(call, "z");
		break;
	case 3:
		rap_cli_expect_format(call, "zzzWWzzzWW");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetPrintDestGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_set_switch_value(call->ndr_pull_data, &r->out.info, r->in.level));
	NDR_GOTO(ndr_pull_rap_printdest_info(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.info));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetPrintDestGetInfo, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netuserpasswordset2(struct smbcli_tree *tree,
					TALLOC_CTX *mem_ctx,
					struct rap_NetUserPasswordSet2 *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WUserPasswordSet2))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.UserName);
	rap_cli_push_paramdesc(call, 'b');
	rap_cli_push_paramdesc(call, '1');
	rap_cli_push_paramdesc(call, '6');
	ndr_push_array_uint8(call->ndr_push_param, NDR_SCALARS, r->in.OldPassword, 16);
	rap_cli_push_paramdesc(call, 'b');
	rap_cli_push_paramdesc(call, '1');
	rap_cli_push_paramdesc(call, '6');
	ndr_push_array_uint8(call->ndr_push_param, NDR_SCALARS, r->in.NewPassword, 16);
	rap_cli_push_word(call, r->in.EncryptedPassword);
	rap_cli_push_word(call, r->in.RealPasswordLength);

	rap_cli_expect_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetUserPasswordSet2, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetUserPasswordSet2, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netoemchangepassword(struct smbcli_tree *tree,
					 TALLOC_CTX *mem_ctx,
					 struct rap_NetOEMChangePassword *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_SamOEMChgPasswordUser2_P))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.UserName);
	rap_cli_push_sendbuf(call, 532);
	ndr_push_array_uint8(call->ndr_push_data, NDR_SCALARS, r->in.crypt_password, 516);
	ndr_push_array_uint8(call->ndr_push_data, NDR_SCALARS, r->in.password_hash, 16);

	rap_cli_expect_format(call, "B516B16");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetOEMChangePassword, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetOEMChangePassword, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netusergetinfo(struct smbcli_tree *tree,
				   TALLOC_CTX *mem_ctx,
				   struct rap_NetUserGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WUserGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.UserName);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 0:
		rap_cli_expect_format(call, "B21");
		break;
	case 1:
		rap_cli_expect_format(call, "B21BB16DWzzWz");
		break;
	case 2:
		rap_cli_expect_format(call, "B21BB16DWzzWzDzzzzDDDDWb21WWzWW");
		break;
	case 10:
		rap_cli_expect_format(call, "B21Bzzz");
		break;
	case 11:
		rap_cli_expect_format(call, "B21BzzzWDDzzDDWWzWzDWb21W");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetUserGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_set_switch_value(call->ndr_pull_data, &r->out.info, r->in.level));
	NDR_GOTO(ndr_pull_rap_netuser_info(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.info));

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetUserGetInfo, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}


static enum ndr_err_code ndr_pull_rap_NetSessionEnum_data(struct ndr_pull *ndr, struct rap_NetSessionEnum *r)
{
	uint32_t cntr_info_0;
	TALLOC_CTX *_mem_save_info_0;

	NDR_PULL_ALLOC_N(ndr, r->out.info, r->out.count);
	_mem_save_info_0 = NDR_PULL_GET_MEM_CTX(ndr);
	NDR_PULL_SET_MEM_CTX(ndr, r->out.info, 0);
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->out.info[cntr_info_0], r->in.level));
		NDR_CHECK(ndr_pull_rap_session_info(ndr, NDR_SCALARS, &r->out.info[cntr_info_0]));
	}
	for (cntr_info_0 = 0; cntr_info_0 < r->out.count; cntr_info_0++) {
		NDR_CHECK(ndr_pull_rap_session_info(ndr, NDR_BUFFERS, &r->out.info[cntr_info_0]));
	}
	NDR_PULL_SET_MEM_CTX(ndr, _mem_save_info_0, 0);

	return NDR_ERR_SUCCESS;
}


NTSTATUS smbcli_rap_netsessionenum(struct smbcli_tree *tree,
				   TALLOC_CTX *mem_ctx,
				   struct rap_NetSessionEnum *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	call = new_rap_cli_call(tree, RAP_WsessionEnum);

	if (call == NULL)
		return NT_STATUS_NO_MEMORY;

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_multiple_entries(call);

	switch(r->in.level) {
	case 2:
		rap_cli_expect_format(call, "zzWWWDDDz");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetSessionEnum, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.count));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	call->ndr_pull_data->relative_rap_convert = r->out.convert;

	NDR_GOTO(ndr_pull_rap_NetSessionEnum_data(call->ndr_pull_data, r));

	r->out.info = talloc_steal(mem_ctx, r->out.info);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetSessionEnum, r);
	}

	result = NT_STATUS_OK;

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netsessiongetinfo(struct smbcli_tree *tree,
				      TALLOC_CTX *mem_ctx,
				      struct rap_NetSessionGetInfo *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WsessionGetInfo))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.SessionName);
	rap_cli_push_word(call, r->in.level);
	rap_cli_push_rcvbuf(call, r->in.bufsize);
	rap_cli_expect_word(call);

	switch(r->in.level) {
	case 2:
		rap_cli_expect_format(call, "zzWWWDDDz");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetSessionGetInfo, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	ZERO_STRUCT(r->out);

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.available));

	if (r->out.status == 0 && r->out.available) {
		call->ndr_pull_data->relative_rap_convert = r->out.convert;

		NDR_GOTO(ndr_pull_set_switch_value(call->ndr_pull_data, &r->out.info, r->in.level));
		NDR_GOTO(ndr_pull_rap_session_info(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.info));
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetSessionGetInfo, r);
	}

	result = NT_STATUS_OK;
 done:
	talloc_free(call);
	return result;
}


NTSTATUS smbcli_rap_netuseradd(struct smbcli_tree *tree,
			       TALLOC_CTX *mem_ctx,
			       struct rap_NetUserAdd *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WUserAdd2))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_word(call, r->in.level);
	rap_cli_push_sendbuf(call, r->in.bufsize);
	rap_cli_push_word(call, r->in.pwdlength);
	rap_cli_push_word(call, r->in.unknown);

	switch (r->in.level) {
	case 1:
		rap_cli_expect_format(call, "B21BB16DWzzWz");
		break;
	default:
		result = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetUserAdd, r);
	}

	NDR_GOTO(ndr_push_set_switch_value(call->ndr_push_data, &r->in.info, r->in.level));
	NDR_GOTO(ndr_push_rap_netuser_info(call->ndr_push_data, NDR_SCALARS|NDR_BUFFERS, &r->in.info));

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetUserAdd, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netuserdelete(struct smbcli_tree *tree,
				  TALLOC_CTX *mem_ctx,
				  struct rap_NetUserDelete *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_WUserDel))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_string(call, r->in.UserName);

	rap_cli_expect_format(call, "");
	rap_cli_expect_extra_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetUserDelete, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetUserDelete, r);
	}

 done:
	talloc_free(call);
	return result;
}

NTSTATUS smbcli_rap_netremotetod(struct smbcli_tree *tree,
				  TALLOC_CTX *mem_ctx,
				  struct rap_NetRemoteTOD *r)
{
	struct rap_call *call;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (!(call = new_rap_cli_call(mem_ctx, RAP_NetRemoteTOD))) {
		return NT_STATUS_NO_MEMORY;
	}

	rap_cli_push_rcvbuf(call, r->in.bufsize);

	rap_cli_expect_format(call, "DDBBBBWWBBWB");
	rap_cli_expect_extra_format(call, "");

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(rap_NetRemoteTOD, r);
	}

	result = rap_cli_do_call(tree, call);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = NT_STATUS_INVALID_PARAMETER;

	NDR_GOTO(ndr_pull_rap_status(call->ndr_pull_param, NDR_SCALARS, &r->out.status));
	NDR_GOTO(ndr_pull_uint16(call->ndr_pull_param, NDR_SCALARS, &r->out.convert));

	NDR_GOTO(ndr_pull_rap_TimeOfDayInfo(call->ndr_pull_data, NDR_SCALARS|NDR_BUFFERS, &r->out.tod));

	result = NT_STATUS_OK;

	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(rap_NetRemoteTOD, r);
	}

 done:
	talloc_free(call);
	return result;
}
