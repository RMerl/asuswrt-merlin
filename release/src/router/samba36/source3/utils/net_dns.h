/*
   Samba Unix/Linux Dynamic DNS Update
   net ads commands

   Copyright (C) Krishna Ganugapati (krishnag@centeris.com)         2006
   Copyright (C) Gerald Carter                                      2006

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

/* flags for DoDNSUpdate */

#define	DNS_UPDATE_SIGNED		0x01
#define	DNS_UPDATE_SIGNED_SUFFICIENT	0x02
#define	DNS_UPDATE_UNSIGNED		0x04
#define	DNS_UPDATE_UNSIGNED_SUFFICIENT	0x08
#define	DNS_UPDATE_PROBE		0x10
#define	DNS_UPDATE_PROBE_SUFFICIENT	0x20

#if defined(WITH_DNS_UPDATES)

#include "../lib/addns/dns.h"

DNS_ERROR DoDNSUpdate(char *pszServerName,
		      const char *pszDomainName, const char *pszHostName,
		      const struct sockaddr_storage *sslist,
		      size_t num_addrs,
		      uint32_t flags);

DNS_ERROR do_gethostbyname(const char *server, const char *host);

#endif /* defined(WITH_DNS_UPDATES) */
