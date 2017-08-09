/*
   Unix SMB/CIFS implementation.
   DNS-SD registration
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

#include <includes.h>
#include "smbd/smbd.h"

/* Uses DNS service discovery (libdns_sd) to
 * register the SMB service. SMB service is registered
 * on ".local" domain via Multicast DNS & any
 * other unicast DNS domains available.
 *
 * Users use the smbclient -B (Browse) option to
 * browse for advertised SMB services.
 */

#define DNS_REG_RETRY_INTERVAL (5*60)  /* in seconds */

#ifdef WITH_DNSSD_SUPPORT

#include <dns_sd.h>

struct dns_reg_state {
	struct tevent_context *event_ctx;
	uint16_t port;
	DNSServiceRef srv_ref;
	struct tevent_timer *te;
	int fd;
	struct tevent_fd *fde;
};

static int dns_reg_state_destructor(struct dns_reg_state *dns_state)
{
	if (dns_state->srv_ref != NULL) {
		/* Close connection to the mDNS daemon */
		DNSServiceRefDeallocate(dns_state->srv_ref);
		dns_state->srv_ref = NULL;
	}

	/* Clear event handler */
	TALLOC_FREE(dns_state->te);
	TALLOC_FREE(dns_state->fde);
	dns_state->fd = -1;

	return 0;
}

static void dns_register_smbd_retry(struct tevent_context *ctx,
				    struct tevent_timer *te,
				    struct timeval now,
				    void *private_data);
static void dns_register_smbd_fde_handler(struct tevent_context *ev,
					  struct tevent_fd *fde,
					  uint16_t flags,
					  void *private_data);

static bool dns_register_smbd_schedule(struct dns_reg_state *dns_state,
				       struct timeval tval)
{
	dns_reg_state_destructor(dns_state);

	dns_state->te = tevent_add_timer(dns_state->event_ctx,
					 dns_state,
					 tval,
					 dns_register_smbd_retry,
					 dns_state);
	if (!dns_state->te) {
		return false;
	}

	return true;
}

static void dns_register_smbd_retry(struct tevent_context *ctx,
				    struct tevent_timer *te,
				    struct timeval now,
				    void *private_data)
{
	struct dns_reg_state *dns_state = talloc_get_type_abort(private_data,
					  struct dns_reg_state);
	DNSServiceErrorType err;

	dns_reg_state_destructor(dns_state);

	DEBUG(6, ("registering _smb._tcp service on port %d\n",
		  dns_state->port));

	/* Register service with DNS. Connects with the mDNS
	 * daemon running on the local system to perform DNS
	 * service registration.
	 */
	err = DNSServiceRegister(&dns_state->srv_ref, 0 /* flags */,
			kDNSServiceInterfaceIndexAny,
			NULL /* service name */,
			"_smb._tcp" /* service type */,
			NULL /* domain */,
			"" /* SRV target host name */,
			htons(dns_state->port),
			0 /* TXT record len */,
			NULL /* TXT record data */,
			NULL /* callback func */,
			NULL /* callback context */);

	if (err != kDNSServiceErr_NoError) {
		/* Failed to register service. Schedule a re-try attempt.
		 */
		DEBUG(3, ("unable to register with mDNS (err %d)\n", err));
		goto retry;
	}

	dns_state->fd = DNSServiceRefSockFD(dns_state->srv_ref);
	if (dns_state->fd == -1) {
		goto retry;
	}

	dns_state->fde = tevent_add_fd(dns_state->event_ctx,
				       dns_state,
				       dns_state->fd,
				       TEVENT_FD_READ,
				       dns_register_smbd_fde_handler,
				       dns_state);
	if (!dns_state->fde) {
		goto retry;
	}

	return;
 retry:
	dns_register_smbd_schedule(dns_state,
		timeval_current_ofs(DNS_REG_RETRY_INTERVAL, 0));
}

/* Processes reply from mDNS daemon. Returns true if a reply was received */
static void dns_register_smbd_fde_handler(struct tevent_context *ev,
					  struct tevent_fd *fde,
					  uint16_t flags,
					  void *private_data)
{
	struct dns_reg_state *dns_state = talloc_get_type_abort(private_data,
					  struct dns_reg_state);
	DNSServiceErrorType err;

	err = DNSServiceProcessResult(dns_state->srv_ref);
	if (err != kDNSServiceErr_NoError) {
		DEBUG(3, ("failed to process mDNS result (err %d), re-trying\n",
			    err));
		goto retry;
	}

	talloc_free(dns_state);
	return;

 retry:
	dns_register_smbd_schedule(dns_state,
		timeval_current_ofs(DNS_REG_RETRY_INTERVAL, 0));
}

bool smbd_setup_mdns_registration(struct tevent_context *ev,
				  TALLOC_CTX *mem_ctx,
				  uint16_t port)
{
	struct dns_reg_state *dns_state;

	dns_state = talloc_zero(mem_ctx, struct dns_reg_state);
	if (dns_state == NULL) {
		return false;
	}
	dns_state->event_ctx = ev;
	dns_state->port = port;
	dns_state->fd = -1;

	talloc_set_destructor(dns_state, dns_reg_state_destructor);

	return dns_register_smbd_schedule(dns_state, timeval_zero());
}

#else /* WITH_DNSSD_SUPPORT */

bool smbd_setup_mdns_registration(struct tevent_context *ev,
				  TALLOC_CTX *mem_ctx,
				  uint16_t port)
{
	return true;
}

#endif /* WITH_DNSSD_SUPPORT */
