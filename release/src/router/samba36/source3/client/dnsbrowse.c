/*
   Unix SMB/CIFS implementation.
   DNS-SD browse client
   Copyright (C) Rishi Srivatsavai 2007

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
#include "client/client_proto.h"

#ifdef WITH_DNSSD_SUPPORT

#include <dns_sd.h>
#include "system/select.h"

/* Holds service instances found during DNS browse */
struct mdns_smbsrv_result
{
	char *serviceName;
	char *regType;
	char *domain;
	uint32_t ifIndex;
	struct mdns_smbsrv_result *nextResult;
};

/* Maintains state during DNS browse */
struct mdns_browse_state
{
	struct mdns_smbsrv_result *listhead; /* Browse result list head */
	int browseDone;

};


static void
do_smb_resolve_reply (DNSServiceRef sdRef, DNSServiceFlags flags,
		uint32_t interfaceIndex, DNSServiceErrorType errorCode,
		const char *fullname, const char *hosttarget, uint16_t port,
		uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
	printf("SMB service available on %s port %u\n",
		hosttarget, ntohs(port));
}


static void do_smb_resolve(struct mdns_smbsrv_result *browsesrv)
{
	DNSServiceRef mdns_conn_sdref = NULL;
	int mdnsfd;
	int fdsetsz;
	int ret;
	struct timeval tv;
	DNSServiceErrorType err;

	TALLOC_CTX * ctx = talloc_tos();

	err = DNSServiceResolve(&mdns_conn_sdref, 0 /* flags */,
		browsesrv->ifIndex,
		browsesrv->serviceName, browsesrv->regType, browsesrv->domain,
		do_smb_resolve_reply, NULL);

	if (err != kDNSServiceErr_NoError) {
		return;
	}

	mdnsfd = DNSServiceRefSockFD(mdns_conn_sdref);
	for (;;)  {
		int revents;

		ret = poll_one_fd(mdnsfd, POLLIN|POLLHUP, 1000, &revents);
		if (ret <= 0 && errno != EINTR) {
			break;
		}

		if (revents & (POLLIN|POLLHUP|POLLERR)) {
			/* Invoke callback function */
			DNSServiceProcessResult(mdns_conn_sdref);
			break;
		}
	}

	TALLOC_FREE(fdset);
	DNSServiceRefDeallocate(mdns_conn_sdref);
}


static void
do_smb_browse_reply(DNSServiceRef sdRef, DNSServiceFlags flags,
        uint32_t interfaceIndex, DNSServiceErrorType errorCode,
        const char  *serviceName, const char *regtype,
        const char  *replyDomain, void  *context)
{
	struct mdns_browse_state *bstatep = (struct mdns_browse_state *)context;
	struct mdns_smbsrv_result *bresult;

	if (bstatep == NULL) {
		return;
	}

	if (errorCode != kDNSServiceErr_NoError) {
		bstatep->browseDone = 1;
		return;
	}

	if (flags & kDNSServiceFlagsMoreComing) {
		bstatep->browseDone = 0;
	} else {
		bstatep->browseDone = 1;
	}

	if (!(flags & kDNSServiceFlagsAdd)) {
		return;
	}

	bresult = TALLOC_ARRAY(talloc_tos(), struct mdns_smbsrv_result, 1);
	if (bresult == NULL) {
		return;
	}

	if (bstatep->listhead != NULL) {
		bresult->nextResult = bstatep->listhead;
	}

	bresult->serviceName = talloc_strdup(talloc_tos(), serviceName);
	bresult->regType = talloc_strdup(talloc_tos(), regtype);
	bresult->domain = talloc_strdup(talloc_tos(), replyDomain);
	bresult->ifIndex = interfaceIndex;
	bstatep->listhead = bresult;
}

int do_smb_browse(void)
{
	int mdnsfd;
	int fdsetsz;
	int ret;
	struct mdns_browse_state bstate;
	struct mdns_smbsrv_result *resptr;
	struct timeval tv;
	DNSServiceRef mdns_conn_sdref = NULL;
	DNSServiceErrorType err;

	TALLOC_CTX * ctx = talloc_stackframe();

	ZERO_STRUCT(bstate);

	err = DNSServiceBrowse(&mdns_conn_sdref, 0, 0, "_smb._tcp", "",
			do_smb_browse_reply, &bstate);

	if (err != kDNSServiceErr_NoError) {
		d_printf("Error connecting to the Multicast DNS daemon\n");
		TALLOC_FREE(ctx);
		return 1;
	}

	mdnsfd = DNSServiceRefSockFD(mdns_conn_sdref);
	for (;;)  {
		int revents;

		ret = poll_one_fd(mdnsfd, POLLIN|POLLHUP, &revents, 1000);
		if (ret <= 0 && errno != EINTR) {
			break;
		}

		if (revents & (POLLIN|POLLHUP|POLLERR)) {
			/* Invoke callback function */
			if (DNSServiceProcessResult(mdns_conn_sdref)) {
				break;
			}
			if (bstate.browseDone) {
				break;
			}
		}
	}

	DNSServiceRefDeallocate(mdns_conn_sdref);

	if (bstate.listhead != NULL) {
		resptr = bstate.listhead;
		while (resptr != NULL) {
			struct mdns_smbsrv_result *oldresptr;
			oldresptr = resptr;

			/* Resolve smb service instance */
			do_smb_resolve(resptr);

			resptr = resptr->nextResult;
		}
	}

	TALLOC_FREE(ctx);
	return 0;
}

#else /* WITH_DNSSD_SUPPORT */

int do_smb_browse(void)
{
    d_printf("DNS-SD browsing is not supported on this platform\n");
    return 1;
}

#endif /* WITH_DNSSD_SUPPORT */


