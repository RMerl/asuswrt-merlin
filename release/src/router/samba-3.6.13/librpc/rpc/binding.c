/* 
   Unix SMB/CIFS implementation.

   dcerpc utility functions

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Jelmer Vernooij 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Rafal Szczesniak 2006

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
#include "../../lib/util/util_net.h"
#include "librpc/gen_ndr/ndr_epmapper.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/rpc/dcerpc.h"
#include "rpc_common.h"

#undef strcasecmp
#undef strncasecmp

#define MAX_PROTSEQ		10

static const struct {
	const char *name;
	enum dcerpc_transport_t transport;
	int num_protocols;
	enum epm_protocol protseq[MAX_PROTSEQ];
} transports[] = {
	{ "ncacn_np",     NCACN_NP, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_SMB, EPM_PROTOCOL_NETBIOS }},
	{ "ncacn_ip_tcp", NCACN_IP_TCP, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_TCP, EPM_PROTOCOL_IP } }, 
	{ "ncacn_http", NCACN_HTTP, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_HTTP, EPM_PROTOCOL_IP } }, 
	{ "ncadg_ip_udp", NCACN_IP_UDP, 3, 
		{ EPM_PROTOCOL_NCADG, EPM_PROTOCOL_UDP, EPM_PROTOCOL_IP } },
	{ "ncalrpc", NCALRPC, 2, 
		{ EPM_PROTOCOL_NCALRPC, EPM_PROTOCOL_NAMED_PIPE } },
	{ "ncacn_unix_stream", NCACN_UNIX_STREAM, 2, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_UNIX_DS } },
	{ "ncadg_unix_dgram", NCADG_UNIX_DGRAM, 2, 
		{ EPM_PROTOCOL_NCADG, EPM_PROTOCOL_UNIX_DS } },
	{ "ncacn_at_dsp", NCACN_AT_DSP, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_APPLETALK, EPM_PROTOCOL_DSP } },
	{ "ncadg_at_ddp", NCADG_AT_DDP, 3, 
		{ EPM_PROTOCOL_NCADG, EPM_PROTOCOL_APPLETALK, EPM_PROTOCOL_DDP } },
	{ "ncacn_vns_ssp", NCACN_VNS_SPP, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_STREETTALK, EPM_PROTOCOL_VINES_SPP } },
	{ "ncacn_vns_ipc", NCACN_VNS_IPC, 3, 
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_STREETTALK, EPM_PROTOCOL_VINES_IPC }, },
	{ "ncadg_ipx", NCADG_IPX, 2,
		{ EPM_PROTOCOL_NCADG, EPM_PROTOCOL_IPX },
	},
	{ "ncacn_spx", NCACN_SPX, 3,
		/* I guess some MS programmer confused the identifier for 
		 * EPM_PROTOCOL_UUID (0x0D or 13) with the one for 
		 * EPM_PROTOCOL_SPX (0x13) here. -- jelmer*/
		{ EPM_PROTOCOL_NCACN, EPM_PROTOCOL_NCALRPC, EPM_PROTOCOL_UUID },
	},
};

static const struct {
	const char *name;
	uint32_t flag;
} ncacn_options[] = {
	{"sign", DCERPC_SIGN},
	{"seal", DCERPC_SEAL},
	{"connect", DCERPC_CONNECT},
	{"spnego", DCERPC_AUTH_SPNEGO},
	{"ntlm", DCERPC_AUTH_NTLM},
	{"krb5", DCERPC_AUTH_KRB5},
	{"validate", DCERPC_DEBUG_VALIDATE_BOTH},
	{"print", DCERPC_DEBUG_PRINT_BOTH},
	{"padcheck", DCERPC_DEBUG_PAD_CHECK},
	{"bigendian", DCERPC_PUSH_BIGENDIAN},
	{"smb2", DCERPC_SMB2},
	{"hdrsign", DCERPC_HEADER_SIGNING},
	{"ndr64", DCERPC_NDR64},
	{"localaddress", DCERPC_LOCALADDRESS}
};

const char *epm_floor_string(TALLOC_CTX *mem_ctx, struct epm_floor *epm_floor)
{
	struct ndr_syntax_id syntax;
	NTSTATUS status;

	switch(epm_floor->lhs.protocol) {
		case EPM_PROTOCOL_UUID:
			status = dcerpc_floor_get_lhs_data(epm_floor, &syntax);
			if (NT_STATUS_IS_OK(status)) {
				/* lhs is used: UUID */
				char *uuidstr;

				if (GUID_equal(&syntax.uuid, &ndr_transfer_syntax.uuid)) {
					return "NDR";
				} 

				if (GUID_equal(&syntax.uuid, &ndr64_transfer_syntax.uuid)) {
					return "NDR64";
				} 

				uuidstr = GUID_string(mem_ctx, &syntax.uuid);

				return talloc_asprintf(mem_ctx, " uuid %s/0x%02x", uuidstr, syntax.if_version);
			} else { /* IPX */
				return talloc_asprintf(mem_ctx, "IPX:%s", 
						data_blob_hex_string_upper(mem_ctx, &epm_floor->rhs.uuid.unknown));
			}

		case EPM_PROTOCOL_NCACN:
			return "RPC-C";

		case EPM_PROTOCOL_NCADG:
			return "RPC";

		case EPM_PROTOCOL_NCALRPC:
			return "NCALRPC";

		case EPM_PROTOCOL_DNET_NSP:
			return "DNET/NSP";

		case EPM_PROTOCOL_IP:
			return talloc_asprintf(mem_ctx, "IP:%s", epm_floor->rhs.ip.ipaddr);

		case EPM_PROTOCOL_NAMED_PIPE:
			return talloc_asprintf(mem_ctx, "NAMED-PIPE:%s", epm_floor->rhs.named_pipe.path);

		case EPM_PROTOCOL_SMB:
			return talloc_asprintf(mem_ctx, "SMB:%s", epm_floor->rhs.smb.unc);

		case EPM_PROTOCOL_UNIX_DS:
			return talloc_asprintf(mem_ctx, "Unix:%s", epm_floor->rhs.unix_ds.path);

		case EPM_PROTOCOL_NETBIOS:
			return talloc_asprintf(mem_ctx, "NetBIOS:%s", epm_floor->rhs.netbios.name);

		case EPM_PROTOCOL_NETBEUI:
			return "NETBeui";

		case EPM_PROTOCOL_SPX:
			return "SPX";

		case EPM_PROTOCOL_NB_IPX:
			return "NB_IPX";

		case EPM_PROTOCOL_HTTP:
			return talloc_asprintf(mem_ctx, "HTTP:%d", epm_floor->rhs.http.port);

		case EPM_PROTOCOL_TCP:
			return talloc_asprintf(mem_ctx, "TCP:%d", epm_floor->rhs.tcp.port);

		case EPM_PROTOCOL_UDP:
			return talloc_asprintf(mem_ctx, "UDP:%d", epm_floor->rhs.udp.port);

		default:
			return talloc_asprintf(mem_ctx, "UNK(%02x):", epm_floor->lhs.protocol);
	}
}


/*
  form a binding string from a binding structure
*/
_PUBLIC_ char *dcerpc_binding_string(TALLOC_CTX *mem_ctx, const struct dcerpc_binding *b)
{
	char *s = talloc_strdup(mem_ctx, "");
	int i;
	const char *t_name = NULL;

	if (b->transport != NCA_UNKNOWN) {
		t_name = derpc_transport_string_by_transport(b->transport);
		if (!t_name) {
			return NULL;
		}
	}

	if (!GUID_all_zero(&b->object.uuid)) { 
		s = talloc_asprintf(s, "%s@",
				    GUID_string(mem_ctx, &b->object.uuid));
	}

	if (t_name != NULL) {
		s = talloc_asprintf_append_buffer(s, "%s:", t_name);
		if (s == NULL) {
			return NULL;
		}
	}

	if (b->host) {
		s = talloc_asprintf_append_buffer(s, "%s", b->host);
	}

	if (!b->endpoint && !b->options && !b->flags) {
		return s;
	}

	s = talloc_asprintf_append_buffer(s, "[");

	if (b->endpoint) {
		s = talloc_asprintf_append_buffer(s, "%s", b->endpoint);
	}

	/* this is a *really* inefficent way of dealing with strings,
	   but this is rarely called and the strings are always short,
	   so I don't care */
	for (i=0;b->options && b->options[i];i++) {
		s = talloc_asprintf_append_buffer(s, ",%s", b->options[i]);
		if (!s) return NULL;
	}

	for (i=0;i<ARRAY_SIZE(ncacn_options);i++) {
		if (b->flags & ncacn_options[i].flag) {
			if (ncacn_options[i].flag == DCERPC_LOCALADDRESS && b->localaddress) {
				s = talloc_asprintf_append_buffer(s, ",%s=%s", ncacn_options[i].name,
								  b->localaddress);
			} else {
				s = talloc_asprintf_append_buffer(s, ",%s", ncacn_options[i].name);
			}
			if (!s) return NULL;
		}
	}

	s = talloc_asprintf_append_buffer(s, "]");

	return s;
}

/*
  parse a binding string into a dcerpc_binding structure
*/
_PUBLIC_ NTSTATUS dcerpc_parse_binding(TALLOC_CTX *mem_ctx, const char *s, struct dcerpc_binding **b_out)
{
	struct dcerpc_binding *b;
	char *options;
	char *p;
	int i, j, comma_count;

	b = talloc_zero(mem_ctx, struct dcerpc_binding);
	if (!b) {
		return NT_STATUS_NO_MEMORY;
	}

	p = strchr(s, '@');

	if (p && PTR_DIFF(p, s) == 36) { /* 36 is the length of a UUID */
		NTSTATUS status;
		DATA_BLOB blob = data_blob(s, 36);
		status = GUID_from_data_blob(&blob, &b->object.uuid);

		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(0, ("Failed parsing UUID\n"));
			return status;
		}

		s = p + 1;
	} else {
		ZERO_STRUCT(b->object);
	}

	b->object.if_version = 0;

	p = strchr(s, ':');

	if (p == NULL) {
		b->transport = NCA_UNKNOWN;
	} else {
		char *type = talloc_strndup(mem_ctx, s, PTR_DIFF(p, s));
		if (!type) {
			return NT_STATUS_NO_MEMORY;
		}

		for (i=0;i<ARRAY_SIZE(transports);i++) {
			if (strcasecmp(type, transports[i].name) == 0) {
				b->transport = transports[i].transport;
				break;
			}
		}

		if (i==ARRAY_SIZE(transports)) {
			DEBUG(0,("Unknown dcerpc transport '%s'\n", type));
			return NT_STATUS_INVALID_PARAMETER;
		}

		talloc_free(type);

		s = p+1;
	}

	p = strchr(s, '[');
	if (p) {
		b->host = talloc_strndup(b, s, PTR_DIFF(p, s));
		options = talloc_strdup(mem_ctx, p+1);
		if (options[strlen(options)-1] != ']') {
			return NT_STATUS_INVALID_PARAMETER;
		}
		options[strlen(options)-1] = 0;
	} else {
		b->host = talloc_strdup(b, s);
		options = NULL;
	}
	if (!b->host) {
		return NT_STATUS_NO_MEMORY;
	}

	b->target_hostname = b->host;

	b->options = NULL;
	b->flags = 0;
	b->assoc_group_id = 0;
	b->endpoint = NULL;
	b->localaddress = NULL;

	if (!options) {
		*b_out = b;
		return NT_STATUS_OK;
	}

	comma_count = count_chars(options, ',');

	b->options = talloc_array(b, const char *, comma_count+2);
	if (!b->options) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; (p = strchr(options, ',')); i++) {
		b->options[i] = talloc_strndup(b, options, PTR_DIFF(p, options));
		if (!b->options[i]) {
			return NT_STATUS_NO_MEMORY;
		}
		options = p+1;
	}
	b->options[i] = options;
	b->options[i+1] = NULL;

	/* some options are pre-parsed for convenience */
	for (i=0;b->options[i];i++) {
		for (j=0;j<ARRAY_SIZE(ncacn_options);j++) {
			size_t opt_len = strlen(ncacn_options[j].name);
			if (strncasecmp(ncacn_options[j].name, b->options[i], opt_len) == 0) {
				int k;
				char c = b->options[i][opt_len];

				if (ncacn_options[j].flag == DCERPC_LOCALADDRESS && c == '=') {
					b->localaddress = talloc_strdup(b, &b->options[i][opt_len+1]);
				} else if (c != 0) {
					continue;
				}

				b->flags |= ncacn_options[j].flag;
				for (k=i;b->options[k];k++) {
					b->options[k] = b->options[k+1];
				}
				i--;
				break;
			}
		}
	}

	if (b->options[0]) {
		/* Endpoint is first option */
		b->endpoint = b->options[0];
		if (strlen(b->endpoint) == 0) b->endpoint = NULL;

		for (i=0;b->options[i];i++) {
			b->options[i] = b->options[i+1];
		}
	}

	if (b->options[0] == NULL)
		b->options = NULL;

	*b_out = b;
	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS dcerpc_floor_get_lhs_data(const struct epm_floor *epm_floor,
					    struct ndr_syntax_id *syntax)
{
	TALLOC_CTX *mem_ctx = talloc_init("floor_get_lhs_data");
	struct ndr_pull *ndr;
	enum ndr_err_code ndr_err;
	uint16_t if_version=0;

	ndr = ndr_pull_init_blob(&epm_floor->lhs.lhs_data, mem_ctx);
	if (ndr == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	ndr->flags |= LIBNDR_FLAG_NOALIGN;

	ndr_err = ndr_pull_GUID(ndr, NDR_SCALARS | NDR_BUFFERS, &syntax->uuid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(mem_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	ndr_err = ndr_pull_uint16(ndr, NDR_SCALARS, &if_version);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(mem_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	syntax->if_version = if_version;

	talloc_free(mem_ctx);

	return NT_STATUS_OK;
}

static DATA_BLOB dcerpc_floor_pack_lhs_data(TALLOC_CTX *mem_ctx, const struct ndr_syntax_id *syntax)
{
	DATA_BLOB blob;
	struct ndr_push *ndr = ndr_push_init_ctx(mem_ctx);

	ndr->flags |= LIBNDR_FLAG_NOALIGN;

	ndr_push_GUID(ndr, NDR_SCALARS | NDR_BUFFERS, &syntax->uuid);
	ndr_push_uint16(ndr, NDR_SCALARS, syntax->if_version);

	blob = ndr_push_blob(ndr);
	talloc_steal(mem_ctx, blob.data);
	talloc_free(ndr);
	return blob;
}

static bool dcerpc_floor_pack_rhs_if_version_data(
	TALLOC_CTX *mem_ctx, const struct ndr_syntax_id *syntax,
	DATA_BLOB *pblob)
{
	DATA_BLOB blob;
	struct ndr_push *ndr = ndr_push_init_ctx(mem_ctx);
	enum ndr_err_code ndr_err;

	if (ndr == NULL) {
		return false;
	}

	ndr->flags |= LIBNDR_FLAG_NOALIGN;

	ndr_err = ndr_push_uint16(ndr, NDR_SCALARS, syntax->if_version >> 16);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	blob = ndr_push_blob(ndr);
	talloc_steal(mem_ctx, blob.data);
	talloc_free(ndr);
	*pblob = blob;
	return true;
}

const char *dcerpc_floor_get_rhs_data(TALLOC_CTX *mem_ctx, struct epm_floor *epm_floor)
{
	switch (epm_floor->lhs.protocol) {
	case EPM_PROTOCOL_TCP:
		if (epm_floor->rhs.tcp.port == 0) return NULL;
		return talloc_asprintf(mem_ctx, "%d", epm_floor->rhs.tcp.port);

	case EPM_PROTOCOL_UDP:
		if (epm_floor->rhs.udp.port == 0) return NULL;
		return talloc_asprintf(mem_ctx, "%d", epm_floor->rhs.udp.port);

	case EPM_PROTOCOL_HTTP:
		if (epm_floor->rhs.http.port == 0) return NULL;
		return talloc_asprintf(mem_ctx, "%d", epm_floor->rhs.http.port);

	case EPM_PROTOCOL_IP:
		return talloc_strdup(mem_ctx, epm_floor->rhs.ip.ipaddr);

	case EPM_PROTOCOL_NCACN:
		return NULL;

	case EPM_PROTOCOL_NCADG:
		return NULL;

	case EPM_PROTOCOL_SMB:
		if (strlen(epm_floor->rhs.smb.unc) == 0) return NULL;
		return talloc_strdup(mem_ctx, epm_floor->rhs.smb.unc);

	case EPM_PROTOCOL_NAMED_PIPE:
		if (strlen(epm_floor->rhs.named_pipe.path) == 0) return NULL;
		return talloc_strdup(mem_ctx, epm_floor->rhs.named_pipe.path);

	case EPM_PROTOCOL_NETBIOS:
		if (strlen(epm_floor->rhs.netbios.name) == 0) return NULL;
		return talloc_strdup(mem_ctx, epm_floor->rhs.netbios.name);

	case EPM_PROTOCOL_NCALRPC:
		return NULL;

	case EPM_PROTOCOL_VINES_SPP:
		return talloc_asprintf(mem_ctx, "%d", epm_floor->rhs.vines_spp.port);

	case EPM_PROTOCOL_VINES_IPC:
		return talloc_asprintf(mem_ctx, "%d", epm_floor->rhs.vines_ipc.port);

	case EPM_PROTOCOL_STREETTALK:
		return talloc_strdup(mem_ctx, epm_floor->rhs.streettalk.streettalk);

	case EPM_PROTOCOL_UNIX_DS:
		if (strlen(epm_floor->rhs.unix_ds.path) == 0) return NULL;
		return talloc_strdup(mem_ctx, epm_floor->rhs.unix_ds.path);

	case EPM_PROTOCOL_NULL:
		return NULL;

	default:
		DEBUG(0,("Unsupported lhs protocol %d\n", epm_floor->lhs.protocol));
		break;
	}

	return NULL;
}

static NTSTATUS dcerpc_floor_set_rhs_data(TALLOC_CTX *mem_ctx, 
					  struct epm_floor *epm_floor,  
					  const char *data)
{
	switch (epm_floor->lhs.protocol) {
	case EPM_PROTOCOL_TCP:
		epm_floor->rhs.tcp.port = atoi(data);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_UDP:
		epm_floor->rhs.udp.port = atoi(data);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_HTTP:
		epm_floor->rhs.http.port = atoi(data);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_IP:
		epm_floor->rhs.ip.ipaddr = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.ip.ipaddr);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NCACN:
		epm_floor->rhs.ncacn.minor_version = 0;
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NCADG:
		epm_floor->rhs.ncadg.minor_version = 0;
		return NT_STATUS_OK;

	case EPM_PROTOCOL_SMB:
		epm_floor->rhs.smb.unc = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.smb.unc);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NAMED_PIPE:
		epm_floor->rhs.named_pipe.path = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.named_pipe.path);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NETBIOS:
		epm_floor->rhs.netbios.name = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.netbios.name);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NCALRPC:
		return NT_STATUS_OK;

	case EPM_PROTOCOL_VINES_SPP:
		epm_floor->rhs.vines_spp.port = atoi(data);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_VINES_IPC:
		epm_floor->rhs.vines_ipc.port = atoi(data);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_STREETTALK:
		epm_floor->rhs.streettalk.streettalk = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.streettalk.streettalk);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_UNIX_DS:
		epm_floor->rhs.unix_ds.path = talloc_strdup(mem_ctx, data);
		NT_STATUS_HAVE_NO_MEMORY(epm_floor->rhs.unix_ds.path);
		return NT_STATUS_OK;

	case EPM_PROTOCOL_NULL:
		return NT_STATUS_OK;

	default:
		DEBUG(0,("Unsupported lhs protocol %d\n", epm_floor->lhs.protocol));
		break;
	}

	return NT_STATUS_NOT_SUPPORTED;
}

enum dcerpc_transport_t dcerpc_transport_by_endpoint_protocol(int prot)
{
	int i;

	/* Find a transport that has 'prot' as 4th protocol */
	for (i=0;i<ARRAY_SIZE(transports);i++) {
		if (transports[i].num_protocols >= 2 && 
			transports[i].protseq[1] == prot) {
			return transports[i].transport;
		}
	}

	/* Unknown transport */
	return (unsigned int)-1;
}

_PUBLIC_ enum dcerpc_transport_t dcerpc_transport_by_tower(const struct epm_tower *tower)
{
	int i;

	/* Find a transport that matches this tower */
	for (i=0;i<ARRAY_SIZE(transports);i++) {
		int j;
		if (transports[i].num_protocols != tower->num_floors - 2) {
			continue; 
		}

		for (j = 0; j < transports[i].num_protocols; j++) {
			if (transports[i].protseq[j] != tower->floors[j+2].lhs.protocol) {
				break;
			}
		}

		if (j == transports[i].num_protocols) {
			return transports[i].transport;
		}
	}

	/* Unknown transport */
	return (unsigned int)-1;
}

_PUBLIC_ const char *derpc_transport_string_by_transport(enum dcerpc_transport_t t)
{
	int i;

	for (i=0; i<ARRAY_SIZE(transports); i++) {
		if (t == transports[i].transport) {
			return transports[i].name;
		}
	}
	return NULL;
}

_PUBLIC_ NTSTATUS dcerpc_binding_from_tower(TALLOC_CTX *mem_ctx,
					    struct epm_tower *tower,
					    struct dcerpc_binding **b_out)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;

	/*
	 * A tower needs to have at least 4 floors to carry useful
	 * information. Floor 3 is the transport identifier which defines
	 * how many floors are required at least.
	 */
	if (tower->num_floors < 4) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	binding = talloc_zero(mem_ctx, struct dcerpc_binding);
	NT_STATUS_HAVE_NO_MEMORY(binding);

	ZERO_STRUCT(binding->object);
	binding->options = NULL;
	binding->host = NULL;
	binding->target_hostname = NULL;
	binding->flags = 0;
	binding->assoc_group_id = 0;

	binding->transport = dcerpc_transport_by_tower(tower);

	if (binding->transport == (unsigned int)-1) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	/* Set object uuid */
	status = dcerpc_floor_get_lhs_data(&tower->floors[0], &binding->object);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Error pulling object uuid and version: %s", nt_errstr(status)));
		return status;
	}

	/* Ignore floor 1, it contains the NDR version info */

	binding->options = NULL;

	/* Set endpoint */
	if (tower->num_floors >= 4) {
		binding->endpoint = dcerpc_floor_get_rhs_data(binding, &tower->floors[3]);
	} else {
		binding->endpoint = NULL;
	}

	/* Set network address */
	if (tower->num_floors >= 5) {
		binding->host = dcerpc_floor_get_rhs_data(binding, &tower->floors[4]);
		NT_STATUS_HAVE_NO_MEMORY(binding->host);
		binding->target_hostname = binding->host;
	}
	*b_out = binding;
	return NT_STATUS_OK;
}

_PUBLIC_ struct dcerpc_binding *dcerpc_binding_dup(TALLOC_CTX *mem_ctx,
						   const struct dcerpc_binding *b)
{
	struct dcerpc_binding *n;
	uint32_t count;

	n = talloc_zero(mem_ctx, struct dcerpc_binding);
	if (n == NULL) {
		return NULL;
	}

	n->transport = b->transport;
	n->object = b->object;
	n->flags = b->flags;
	n->assoc_group_id = b->assoc_group_id;

	if (b->host != NULL) {
		n->host = talloc_strdup(n, b->host);
		if (n->host == NULL) {
			talloc_free(n);
			return NULL;
		}
	}

	if (b->target_hostname != NULL) {
		n->target_hostname = talloc_strdup(n, b->target_hostname);
		if (n->target_hostname == NULL) {
			talloc_free(n);
			return NULL;
		}
	}

	if (b->target_principal != NULL) {
		n->target_principal = talloc_strdup(n, b->target_principal);
		if (n->target_principal == NULL) {
			talloc_free(n);
			return NULL;
		}
	}

	if (b->localaddress != NULL) {
		n->localaddress = talloc_strdup(n, b->localaddress);
		if (n->localaddress == NULL) {
			talloc_free(n);
			return NULL;
		}
	}

	if (b->endpoint != NULL) {
		n->endpoint = talloc_strdup(n, b->endpoint);
		if (n->endpoint == NULL) {
			talloc_free(n);
			return NULL;
		}
	}

	for (count = 0; b->options && b->options[count]; count++);

	if (count > 0) {
		uint32_t i;

		n->options = talloc_array(n, const char *, count + 1);
		if (n->options == NULL) {
			talloc_free(n);
			return NULL;
		}

		for (i = 0; i < count; i++) {
			n->options[i] = talloc_strdup(n->options, b->options[i]);
			if (n->options[i] == NULL) {
				talloc_free(n);
				return NULL;
			}
		}
		n->options[count] = NULL;
	}

	return n;
}

_PUBLIC_ NTSTATUS dcerpc_binding_build_tower(TALLOC_CTX *mem_ctx,
					     const struct dcerpc_binding *binding,
					     struct epm_tower *tower)
{
	const enum epm_protocol *protseq = NULL;
	int num_protocols = -1, i;
	NTSTATUS status;

	/* Find transport */
	for (i=0;i<ARRAY_SIZE(transports);i++) {
		if (transports[i].transport == binding->transport) {
			protseq = transports[i].protseq;
			num_protocols = transports[i].num_protocols;
			break;
		}
	}

	if (num_protocols == -1) {
		DEBUG(0, ("Unable to find transport with id '%d'\n", binding->transport));
		return NT_STATUS_UNSUCCESSFUL;
	}

	tower->num_floors = 2 + num_protocols;
	tower->floors = talloc_array(mem_ctx, struct epm_floor, tower->num_floors);

	/* Floor 0 */
	tower->floors[0].lhs.protocol = EPM_PROTOCOL_UUID;

	tower->floors[0].lhs.lhs_data = dcerpc_floor_pack_lhs_data(tower->floors, &binding->object);

	if (!dcerpc_floor_pack_rhs_if_version_data(
		    tower->floors, &binding->object,
		    &tower->floors[0].rhs.uuid.unknown)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Floor 1 */
	tower->floors[1].lhs.protocol = EPM_PROTOCOL_UUID;

	tower->floors[1].lhs.lhs_data = dcerpc_floor_pack_lhs_data(tower->floors, 
								&ndr_transfer_syntax);

	tower->floors[1].rhs.uuid.unknown = data_blob_talloc_zero(tower->floors, 2);

	/* Floor 2 to num_protocols */
	for (i = 0; i < num_protocols; i++) {
		tower->floors[2 + i].lhs.protocol = protseq[i];
		tower->floors[2 + i].lhs.lhs_data = data_blob_talloc(tower->floors, NULL, 0);
		ZERO_STRUCT(tower->floors[2 + i].rhs);
		dcerpc_floor_set_rhs_data(tower->floors, &tower->floors[2 + i], "");
	}

	/* The 4th floor contains the endpoint */
	if (num_protocols >= 2 && binding->endpoint) {
		status = dcerpc_floor_set_rhs_data(tower->floors, &tower->floors[3], binding->endpoint);
		if (NT_STATUS_IS_ERR(status)) {
			return status;
		}
	}

	/* The 5th contains the network address */
	if (num_protocols >= 3 && binding->host) {
		if (is_ipaddress(binding->host) ||
		    (binding->host[0] == '\\' && binding->host[1] == '\\')) {
			status = dcerpc_floor_set_rhs_data(tower->floors, &tower->floors[4], 
							   binding->host);
		} else {
			/* note that we don't attempt to resolve the
			   name here - when we get a hostname here we
			   are in the client code, and want to put in
			   a wildcard all-zeros IP for the server to
			   fill in */
			status = dcerpc_floor_set_rhs_data(tower->floors, &tower->floors[4], 
							   "0.0.0.0");
		}
		if (NT_STATUS_IS_ERR(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}
