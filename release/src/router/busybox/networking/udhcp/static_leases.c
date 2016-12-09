/* vi: set sw=4 ts=4: */
/*
 * Storing and retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "common.h"
#include "dhcpd.h"

/* Takes the address of the pointer to the static_leases linked list,
 * address to a 6 byte mac address,
 * 4 byte IP address */
void FAST_FUNC add_static_lease(struct static_lease **st_lease_pp,
		uint8_t *mac,
		uint32_t nip)
{
	struct static_lease *st_lease;

	/* Find the tail of the list */
	while ((st_lease = *st_lease_pp) != NULL) {
		st_lease_pp = &st_lease->next;
	}

	/* Add new node */
	*st_lease_pp = st_lease = xzalloc(sizeof(*st_lease));
	memcpy(st_lease->mac, mac, 6);
	st_lease->nip = nip;
	/*st_lease->next = NULL;*/
}

/* Find static lease IP by mac */
uint32_t FAST_FUNC get_static_nip_by_mac(struct static_lease *st_lease, void *mac)
{
	while (st_lease) {
		if (memcmp(st_lease->mac, mac, 6) == 0)
			return st_lease->nip;
		st_lease = st_lease->next;
	}

	return 0;
}

/* Check to see if an IP is reserved as a static IP */
int FAST_FUNC is_nip_reserved(struct static_lease *st_lease, uint32_t nip)
{
	while (st_lease) {
		if (st_lease->nip == nip)
			return 1;
		st_lease = st_lease->next;
	}

	return 0;
}

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 2
/* Print out static leases just to check what's going on */
/* Takes the address of the pointer to the static_leases linked list */
void FAST_FUNC log_static_leases(struct static_lease **st_lease_pp)
{
	struct static_lease *cur;

	if (dhcp_verbose < 2)
		return;

	cur = *st_lease_pp;
	while (cur) {
		bb_error_msg("static lease: mac:%02x:%02x:%02x:%02x:%02x:%02x nip:%x",
			cur->mac[0], cur->mac[1], cur->mac[2],
			cur->mac[3], cur->mac[4], cur->mac[5],
			cur->nip
		);
		cur = cur->next;
	}
}
#endif
