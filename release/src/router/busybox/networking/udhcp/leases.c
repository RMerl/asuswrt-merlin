/* vi: set sw=4 ts=4: */
/*
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "common.h"
#include "dhcpd.h"

/* Find the oldest expired lease, NULL if there are no expired leases */
static struct dyn_lease *oldest_expired_lease(void)
{
	struct dyn_lease *oldest_lease = NULL;
	leasetime_t oldest_time = time(NULL);
	unsigned i;

	/* Unexpired leases have g_leases[i].expires >= current time
	 * and therefore can't ever match */
	for (i = 0; i < server_config.max_leases; i++) {
		if (g_leases[i].expires < oldest_time) {
			oldest_time = g_leases[i].expires;
			oldest_lease = &g_leases[i];
		}
	}
	return oldest_lease;
}

/* Clear out all leases with matching nonzero chaddr OR yiaddr.
 * If chaddr == NULL, this is a conflict lease.
 */
static void clear_leases(const uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++) {
		if ((chaddr && memcmp(g_leases[i].lease_mac, chaddr, 6) == 0)
		 || (yiaddr && g_leases[i].lease_nip == yiaddr)
		) {
			memset(&g_leases[i], 0, sizeof(g_leases[i]));
		}
	}
}

/* Add a lease into the table, clearing out any old ones.
 * If chaddr == NULL, this is a conflict lease.
 */
struct dyn_lease* FAST_FUNC add_lease(
		const uint8_t *chaddr, uint32_t yiaddr,
		leasetime_t leasetime,
		const char *hostname, int hostname_len)
{
	struct dyn_lease *oldest;

	/* clean out any old ones */
	clear_leases(chaddr, yiaddr);

	oldest = oldest_expired_lease();

	if (oldest) {
		memset(oldest, 0, sizeof(*oldest));
		if (hostname) {
			char *p;

			hostname_len++; /* include NUL */
			if (hostname_len > sizeof(oldest->hostname))
				hostname_len = sizeof(oldest->hostname);
			p = safe_strncpy(oldest->hostname, hostname, hostname_len);
			/* sanitization (s/non-ASCII/^/g) */
			while (*p) {
				if (*p < ' ' || *p > 126)
					*p = '^';
				p++;
			}
		}
		if (chaddr)
			memcpy(oldest->lease_mac, chaddr, 6);
		oldest->lease_nip = yiaddr;
		oldest->expires = time(NULL) + leasetime;
	}

	return oldest;
}

/* True if a lease has expired */
int FAST_FUNC is_expired_lease(struct dyn_lease *lease)
{
	return (lease->expires < (leasetime_t) time(NULL));
}

/* Find the first lease that matches MAC, NULL if no match */
struct dyn_lease* FAST_FUNC find_lease_by_mac(const uint8_t *mac)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (memcmp(g_leases[i].lease_mac, mac, 6) == 0)
			return &g_leases[i];

	return NULL;
}

/* Find the first lease that matches IP, NULL is no match */
struct dyn_lease* FAST_FUNC find_lease_by_nip(uint32_t nip)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (g_leases[i].lease_nip == nip)
			return &g_leases[i];

	return NULL;
}

/* Check if the IP is taken; if it is, add it to the lease table */
static int nobody_responds_to_arp(uint32_t nip, const uint8_t *safe_mac)
{
	struct in_addr temp;
	int r;

	r = arpping(nip, safe_mac,
			server_config.server_nip,
			server_config.server_mac,
			server_config.interface);
	if (r)
		return r;

	temp.s_addr = nip;
	bb_info_msg("%s belongs to someone, reserving it for %u seconds",
		inet_ntoa(temp), (unsigned)server_config.conflict_time);
	add_lease(NULL, nip, server_config.conflict_time, NULL, 0);
	return 0;
}

/* Find a new usable (we think) address */
uint32_t FAST_FUNC find_free_or_expired_nip(const uint8_t *safe_mac)
{
	uint32_t addr;
	struct dyn_lease *oldest_lease = NULL;

	addr = server_config.start_ip; /* addr is in host order here */
	for (; addr <= server_config.end_ip; addr++) {
		uint32_t nip;
		struct dyn_lease *lease;

		/* ie, 192.168.55.0 */
		if ((addr & 0xff) == 0)
			continue;
		/* ie, 192.168.55.255 */
		if ((addr & 0xff) == 0xff)
			continue;
		nip = htonl(addr);
		/* is this a static lease addr? */
		if (is_nip_reserved(server_config.static_leases, nip))
			continue;

		lease = find_lease_by_nip(nip);
		if (!lease) {
//TODO: DHCP servers do not always sit on the same subnet as clients: should *ping*, not arp-ping!
			if (nobody_responds_to_arp(nip, safe_mac))
				return nip;
		} else {
			if (!oldest_lease || lease->expires < oldest_lease->expires)
				oldest_lease = lease;
		}
	}

	if (oldest_lease
	 && is_expired_lease(oldest_lease)
	 && nobody_responds_to_arp(oldest_lease->lease_nip, safe_mac)
	) {
		return oldest_lease->lease_nip;
	}

	return 0;
}
