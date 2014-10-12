/* 
   Unix SMB/CIFS implementation.

   code to manipulate domain credentials

   Copyright (C) Andrew Tridgell 2004
   
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

#include "librpc/gen_ndr/netlogon.h"

/* The 7 here seems to be required to get Win2k not to downgrade us
   to NT4.  Actually, anything other than 1ff would seem to do... */
#define NETLOGON_NEG_AUTH2_FLAGS 0x000701ff
/*
	(NETLOGON_NEG_ACCOUNT_LOCKOUT |
	 NETLOGON_NEG_PERSISTENT_SAMREPL |
	 NETLOGON_NEG_ARCFOUR |
	 NETLOGON_NEG_PROMOTION_COUNT |
	 NETLOGON_NEG_CHANGELOG_BDC |
	 NETLOGON_NEG_FULL_SYNC_REPL |
	 NETLOGON_NEG_MULTIPLE_SIDS |
	 NETLOGON_NEG_REDO |
	 NETLOGON_NEG_PASSWORD_CHANGE_REFUSAL |
	 NETLOGON_NEG_DNS_DOMAIN_TRUSTS |
	 NETLOGON_NEG_PASSWORD_SET2 |
	 NETLOGON_NEG_GETDOMAININFO)
*/
#define NETLOGON_NEG_DOMAIN_TRUST_ACCOUNT	0x2010b000

/* these are the flags that ADS clients use */
/*
	(NETLOGON_NEG_ACCOUNT_LOCKOUT |
	 NETLOGON_NEG_PERSISTENT_SAMREPL |
	 NETLOGON_NEG_ARCFOUR |
	 NETLOGON_NEG_PROMOTION_COUNT |
	 NETLOGON_NEG_CHANGELOG_BDC |
	 NETLOGON_NEG_FULL_SYNC_REPL |
	 NETLOGON_NEG_MULTIPLE_SIDS |
	 NETLOGON_NEG_REDO |
	 NETLOGON_NEG_PASSWORD_CHANGE_REFUSAL |
	 NETLOGON_NEG_SEND_PASSWORD_INFO_PDC |
	 NETLOGON_NEG_GENERIC_PASSTHROUGH |
	 NETLOGON_NEG_CONCURRENT_RPC |
	 NETLOGON_NEG_AVOID_ACCOUNT_DB_REPL |
	 NETLOGON_NEG_AVOID_SECURITYAUTH_DB_REPL |
	 NETLOGON_NEG_128BIT |
	 NETLOGON_NEG_TRANSITIVE_TRUSTS |
	 NETLOGON_NEG_DNS_DOMAIN_TRUSTS |
	 NETLOGON_NEG_PASSWORD_SET2 |
	 NETLOGON_NEG_GETDOMAININFO |
	 NETLOGON_NEG_CROSS_FOREST_TRUSTS |
	 NETLOGON_NEG_AUTHENTICATED_RPC_LSASS |
	 NETLOGON_NEG_SCHANNEL)
*/

#define NETLOGON_NEG_AUTH2_ADS_FLAGS (0x200fbffb | NETLOGON_NEG_ARCFOUR | NETLOGON_NEG_128BIT | NETLOGON_NEG_SCHANNEL)

#define NETLOGON_NEG_AUTH2_RODC_FLAGS (NETLOGON_NEG_AUTH2_ADS_FLAGS | NETLOGON_NEG_RODC_PASSTHROUGH)

