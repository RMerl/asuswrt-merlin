/*
   Unix SMB/CIFS implementation.

   CLDAP server structures

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008

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

#ifndef __LIBCLI_NETLOGON_H__
#define __LIBCLI_NETLOGON_H__

#include "librpc/gen_ndr/ndr_nbt.h"

#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_svcctl.h"
#include "librpc/gen_ndr/ndr_samr.h"

struct netlogon_samlogon_response
{
	uint32_t ntver;
	union {
		struct NETLOGON_SAM_LOGON_RESPONSE_NT40 nt4;
		struct NETLOGON_SAM_LOGON_RESPONSE nt5;
		struct NETLOGON_SAM_LOGON_RESPONSE_EX nt5_ex;
	} data;

};

struct nbt_netlogon_response
{
	enum {NETLOGON_GET_PDC, NETLOGON_SAMLOGON} response_type;
	union {
		struct nbt_netlogon_response_from_pdc get_pdc;
		struct netlogon_samlogon_response samlogon;
	} data;
};

#include "../libcli/netlogon_proto.h"
#include "../libcli/ndr_netlogon_proto.h"
#endif /* __CLDAP_SERVER_PROTO_H__ */
