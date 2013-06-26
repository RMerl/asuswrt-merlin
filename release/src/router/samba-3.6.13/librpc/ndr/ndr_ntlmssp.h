/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special ntlmssp structures

   Copyright (C) Guenther Deschner 2009

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

_PUBLIC_ size_t ndr_ntlmssp_string_length(uint32_t negotiate_flags, const char *s);
_PUBLIC_ uint32_t ndr_ntlmssp_negotiated_string_flags(uint32_t negotiate_flags);
_PUBLIC_ enum ndr_err_code ndr_push_AV_PAIR_LIST(struct ndr_push *ndr, int ndr_flags, const struct AV_PAIR_LIST *r);
_PUBLIC_ enum ndr_err_code ndr_pull_AV_PAIR_LIST(struct ndr_pull *ndr, int ndr_flags, struct AV_PAIR_LIST *r);
_PUBLIC_ void ndr_print_ntlmssp_nt_response(TALLOC_CTX *mem_ctx,
					    const DATA_BLOB *nt_response,
					    bool ntlmv2);
_PUBLIC_ void ndr_print_ntlmssp_lm_response(TALLOC_CTX *mem_ctx,
					    const DATA_BLOB *lm_response,
					    bool ntlmv2);
_PUBLIC_ void ndr_print_ntlmssp_Version(struct ndr_print *ndr, const char *name, const union ntlmssp_Version *r);

