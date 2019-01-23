/* 
   Unix SMB/CIFS implementation.

   core wins server handling

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Stefan Metzmacher	2005
      
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
#include "lib/util/dlinklist.h"
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsdb.h"
#include "nbt_server/wins/winsserver.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "system/time.h"
#include "libcli/composite/composite.h"
#include "smbd/service_task.h"
#include "system/network.h"
#include "lib/socket/socket.h"
#include "lib/socket/netif.h"
#include <ldb.h>
#include "param/param.h"
#include "libcli/resolve/resolve.h"
#include "lib/util/util_net.h"

/*
  work out the ttl we will use given a client requested ttl
*/
uint32_t wins_server_ttl(struct wins_server *winssrv, uint32_t ttl)
{
	ttl = MIN(ttl, winssrv->config.max_renew_interval);
	ttl = MAX(ttl, winssrv->config.min_renew_interval);
	return ttl;
}

static enum wrepl_name_type wrepl_type(uint16_t nb_flags, struct nbt_name *name, bool mhomed)
{
	/* this copes with the nasty hack that is the type 0x1c name */
	if (name->type == NBT_NAME_LOGON) {
		return WREPL_TYPE_SGROUP;
	}
	if (nb_flags & NBT_NM_GROUP) {
		return WREPL_TYPE_GROUP;
	}
	if (mhomed) {
		return WREPL_TYPE_MHOMED;
	}
	return WREPL_TYPE_UNIQUE;
}

/*
  register a new name with WINS
*/
static uint8_t wins_register_new(struct nbt_name_socket *nbtsock, 
				 struct nbt_name_packet *packet, 
				 const struct socket_address *src,
				 enum wrepl_name_type type)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	struct nbt_name *name = &packet->questions[0].name;
	uint32_t ttl = wins_server_ttl(winssrv, packet->additional[0].ttl);
	uint16_t nb_flags = packet->additional[0].rdata.netbios.addresses[0].nb_flags;
	const char *address = packet->additional[0].rdata.netbios.addresses[0].ipaddr;
	struct winsdb_record rec;
	enum wrepl_name_node node;

#define WREPL_NODE_NBT_FLAGS(nb_flags) \
	((nb_flags & NBT_NM_OWNER_TYPE)>>13)

	node	= WREPL_NODE_NBT_FLAGS(nb_flags);

	rec.name		= name;
	rec.type		= type;
	rec.state		= WREPL_STATE_ACTIVE;
	rec.node		= node;
	rec.is_static		= false;
	rec.expire_time		= time(NULL) + ttl;
	rec.version		= 0; /* will be allocated later */
	rec.wins_owner		= NULL; /* will be set later */
	rec.registered_by	= src->addr;
	rec.addresses		= winsdb_addr_list_make(packet);
	if (rec.addresses == NULL) return NBT_RCODE_SVR;

	rec.addresses     = winsdb_addr_list_add(winssrv->wins_db,
						 &rec, rec.addresses,
						 address,
						 winssrv->wins_db->local_owner,
						 rec.expire_time,
						 true);
	if (rec.addresses == NULL) return NBT_RCODE_SVR;

	DEBUG(4,("WINS: accepted registration of %s with address %s\n",
		 nbt_name_string(packet, name), rec.addresses[0]->address));
	
	return winsdb_add(winssrv->wins_db, &rec, WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP);
}


/*
  update the ttl on an existing record
*/
static uint8_t wins_update_ttl(struct nbt_name_socket *nbtsock, 
			       struct nbt_name_packet *packet, 
			       struct winsdb_record *rec,
			       struct winsdb_addr *winsdb_addr,
			       const struct socket_address *src)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	uint32_t ttl = wins_server_ttl(winssrv, packet->additional[0].ttl);
	const char *address = packet->additional[0].rdata.netbios.addresses[0].ipaddr;
	uint32_t modify_flags = 0;

	rec->expire_time   = time(NULL) + ttl;
	rec->registered_by = src->addr;

	if (winsdb_addr) {
		rec->addresses = winsdb_addr_list_add(winssrv->wins_db,
						      rec, rec->addresses,
						      winsdb_addr->address,
						      winssrv->wins_db->local_owner,
						      rec->expire_time,
						      true);
		if (rec->addresses == NULL) return NBT_RCODE_SVR;
	}

	if (strcmp(winssrv->wins_db->local_owner, rec->wins_owner) != 0) {
		modify_flags = WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
	}

	DEBUG(5,("WINS: refreshed registration of %s at %s\n",
		 nbt_name_string(packet, rec->name), address));
	
	return winsdb_modify(winssrv->wins_db, rec, modify_flags);
}

/*
  do a sgroup merge
*/
static uint8_t wins_sgroup_merge(struct nbt_name_socket *nbtsock, 
				 struct nbt_name_packet *packet, 
			         struct winsdb_record *rec,
			         const char *address,
			         const struct socket_address *src)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	uint32_t ttl = wins_server_ttl(winssrv, packet->additional[0].ttl);

	rec->expire_time   = time(NULL) + ttl;
	rec->registered_by = src->addr;

	rec->addresses     = winsdb_addr_list_add(winssrv->wins_db,
						  rec, rec->addresses,
						  address,
						  winssrv->wins_db->local_owner,
						  rec->expire_time,
						  true);
	if (rec->addresses == NULL) return NBT_RCODE_SVR;

	DEBUG(5,("WINS: sgroup merge of %s at %s\n",
		 nbt_name_string(packet, rec->name), address));
	
	return winsdb_modify(winssrv->wins_db, rec, WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP);
}

struct nbtd_wins_wack_state {
	struct nbtd_wins_wack_state *prev, *next;
	struct wins_server *winssrv;
	struct nbt_name_socket *nbtsock;
	struct nbtd_interface *iface;
	struct nbt_name_packet *request_packet;
	struct winsdb_record *rec;
	struct socket_address *src;
	const char *reg_address;
	enum wrepl_name_type new_type;
	struct wins_challenge_io io;
	NTSTATUS status;
};

static int nbtd_wins_wack_state_destructor(struct nbtd_wins_wack_state *s)
{
	DLIST_REMOVE(s->iface->wack_queue, s);
	return 0;
}

static bool wins_check_wack_queue(struct nbtd_interface *iface,
				  struct nbt_name_packet *packet,
				  struct socket_address *src)
{
	struct nbtd_wins_wack_state *s;

	for (s= iface->wack_queue; s; s = s->next) {
		if (packet->name_trn_id != s->request_packet->name_trn_id) {
			continue;
		}
		if (packet->operation != s->request_packet->operation) {
			continue;
		}
		if (src->port != s->src->port) {
			continue;
		}
		if (strcmp(src->addr, s->src->addr) != 0) {
			continue;
		}

		return true;
	}

	return false;
}

/*
  deny a registration request
*/
static void wins_wack_deny(struct nbtd_wins_wack_state *s)
{
	nbtd_name_registration_reply(s->nbtsock, s->request_packet, 
				     s->src, NBT_RCODE_ACT);
	DEBUG(4,("WINS: denied name registration request for %s from %s:%d\n",
		 nbt_name_string(s, s->rec->name), s->src->addr, s->src->port));
	talloc_free(s);
}

/*
  allow a registration request
*/
static void wins_wack_allow(struct nbtd_wins_wack_state *s)
{
	NTSTATUS status;
	uint32_t ttl = wins_server_ttl(s->winssrv, s->request_packet->additional[0].ttl);
	struct winsdb_record *rec = s->rec, *rec2;
	uint32_t i,j;

	status = winsdb_lookup(s->winssrv->wins_db, rec->name, s, &rec2);
	if (!NT_STATUS_IS_OK(status) ||
	    rec2->version != rec->version ||
	    strcmp(rec2->wins_owner, rec->wins_owner) != 0) {
		DEBUG(5,("WINS: record %s changed during WACK - failing registration\n",
			 nbt_name_string(s, rec->name)));
		wins_wack_deny(s);
		return;
	}

	/*
	 * if the old name owner doesn't hold the name anymore
	 * handle the request as new registration for the new name owner
	 */
	if (!NT_STATUS_IS_OK(s->status)) {
		uint8_t rcode;

		winsdb_delete(s->winssrv->wins_db, rec);
		rcode = wins_register_new(s->nbtsock, s->request_packet, s->src, s->new_type);
		if (rcode != NBT_RCODE_OK) {
			DEBUG(1,("WINS: record %s failed to register as new during WACK\n",
				 nbt_name_string(s, rec->name)));
			wins_wack_deny(s);
			return;
		}
		goto done;
	}

	rec->expire_time = time(NULL) + ttl;
	rec->registered_by = s->src->addr;

	/*
	 * now remove all addresses that the client doesn't hold anymore
	 * and update the time stamp and owner for the ones that are still there
	 */
	for (i=0; rec->addresses[i]; i++) {
		bool found = false;
		for (j=0; j < s->io.out.num_addresses; j++) {
			if (strcmp(rec->addresses[i]->address, s->io.out.addresses[j]) != 0) continue;

			found = true;
			break;
		}
		if (found) {
			rec->addresses = winsdb_addr_list_add(s->winssrv->wins_db,
							      rec, rec->addresses,
							      s->reg_address,
							      s->winssrv->wins_db->local_owner,
							      rec->expire_time,
							      true);
			if (rec->addresses == NULL) goto failed;
			continue;
		}

		winsdb_addr_list_remove(rec->addresses, rec->addresses[i]->address);
	}

	rec->addresses = winsdb_addr_list_add(s->winssrv->wins_db,
					      rec, rec->addresses,
					      s->reg_address,
					      s->winssrv->wins_db->local_owner,
					      rec->expire_time,
					      true);
	if (rec->addresses == NULL) goto failed;

	/* if we have more than one address, this becomes implicit a MHOMED record */
	if (winsdb_addr_list_length(rec->addresses) > 1) {
		rec->type = WREPL_TYPE_MHOMED;
	}

	winsdb_modify(s->winssrv->wins_db, rec, WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP);

	DEBUG(4,("WINS: accepted registration of %s with address %s\n",
		 nbt_name_string(s, rec->name), s->reg_address));

done:
	nbtd_name_registration_reply(s->nbtsock, s->request_packet, 
				     s->src, NBT_RCODE_OK);
failed:
	talloc_free(s);
}

/*
  called when a name query to a current owner completes
*/
static void wack_wins_challenge_handler(struct composite_context *c_req)
{
	struct nbtd_wins_wack_state *s = talloc_get_type(c_req->async.private_data,
					 struct nbtd_wins_wack_state);
	bool found;
	uint32_t i;

	s->status = wins_challenge_recv(c_req, s, &s->io);

	/*
	 * if the owner denies it holds the name, then allow
	 * the registration
	 */
	if (!NT_STATUS_IS_OK(s->status)) {
		wins_wack_allow(s);
		return;
	}

	if (s->new_type == WREPL_TYPE_GROUP || s->new_type == WREPL_TYPE_SGROUP) {
		DEBUG(1,("WINS: record %s failed to register as group type(%u) during WACK, it's still type(%u)\n",
			 nbt_name_string(s, s->rec->name), s->new_type, s->rec->type));
		wins_wack_deny(s);
		return;
	}

	/*
	 * if the owner still wants the name and doesn't reply
	 * with the address trying to be registered, then deny
	 * the registration
	 */
	found = false;
	for (i=0; i < s->io.out.num_addresses; i++) {
		if (strcmp(s->reg_address, s->io.out.addresses[i]) != 0) continue;

		found = true;
		break;
	}
	if (!found) {
		wins_wack_deny(s);
		return;
	}

	wins_wack_allow(s);
	return;
}


/*
  a client has asked to register a unique name that someone else owns. We
  need to ask each of the current owners if they still want it. If they do
  then reject the registration, otherwise allow it
*/
static void wins_register_wack(struct nbt_name_socket *nbtsock, 
			       struct nbt_name_packet *packet, 
			       struct winsdb_record *rec,
			       struct socket_address *src,
			       enum wrepl_name_type new_type)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	struct nbtd_wins_wack_state *s;
	struct composite_context *c_req;
	uint32_t ttl;

	s = talloc_zero(nbtsock, struct nbtd_wins_wack_state);
	if (s == NULL) goto failed;

	/* package up the state variables for this wack request */
	s->winssrv		= winssrv;
	s->nbtsock		= nbtsock;
	s->iface		= iface;
	s->request_packet	= talloc_steal(s, packet);
	s->rec			= talloc_steal(s, rec);
	s->reg_address		= packet->additional[0].rdata.netbios.addresses[0].ipaddr;
	s->new_type		= new_type;
	s->src		        = src;
	if (talloc_reference(s, src) == NULL) goto failed;

	s->io.in.nbtd_server	= iface->nbtsrv;
	s->io.in.nbt_port       = lpcfg_nbt_port(iface->nbtsrv->task->lp_ctx);
	s->io.in.event_ctx	= iface->nbtsrv->task->event_ctx;
	s->io.in.name		= rec->name;
	s->io.in.num_addresses	= winsdb_addr_list_length(rec->addresses);
	s->io.in.addresses	= winsdb_addr_string_list(s, rec->addresses);
	if (s->io.in.addresses == NULL) goto failed;

	DLIST_ADD_END(iface->wack_queue, s, struct nbtd_wins_wack_state *);

	talloc_set_destructor(s, nbtd_wins_wack_state_destructor);

	/*
	 * send a WACK to the client, specifying the maximum time it could
         * take to check with the owner, plus some slack
	 */
	ttl = 5 + 4 * winsdb_addr_list_length(rec->addresses);
	nbtd_wack_reply(nbtsock, packet, src, ttl);

	/*
	 * send the challenge to the old addresses
	 */
	c_req = wins_challenge_send(s, &s->io);
	if (c_req == NULL) goto failed;

	c_req->async.fn			= wack_wins_challenge_handler;
	c_req->async.private_data	= s;
	return;

failed:
	talloc_free(s);
	nbtd_name_registration_reply(nbtsock, packet, src, NBT_RCODE_SVR);
}

/*
  register a name
*/
static void nbtd_winsserver_register(struct nbt_name_socket *nbtsock, 
				     struct nbt_name_packet *packet, 
				     struct socket_address *src)
{
	NTSTATUS status;
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	struct nbt_name *name = &packet->questions[0].name;
	struct winsdb_record *rec;
	uint8_t rcode = NBT_RCODE_OK;
	uint16_t nb_flags = packet->additional[0].rdata.netbios.addresses[0].nb_flags;
	const char *address = packet->additional[0].rdata.netbios.addresses[0].ipaddr;
	bool mhomed = ((packet->operation & NBT_OPCODE) == NBT_OPCODE_MULTI_HOME_REG);
	enum wrepl_name_type new_type = wrepl_type(nb_flags, name, mhomed);
	struct winsdb_addr *winsdb_addr = NULL;
	bool duplicate_packet;

	/*
	 * as a special case, the local master browser name is always accepted
	 * for registration, but never stored, but w2k3 stores it if it's registered
	 * as a group name, (but a query for the 0x1D name still returns not found!)
	 */
	if (name->type == NBT_NAME_MASTER && !(nb_flags & NBT_NM_GROUP)) {
		rcode = NBT_RCODE_OK;
		goto done;
	}

	/* w2k3 refuses 0x1B names with marked as group */
	if (name->type == NBT_NAME_PDC && (nb_flags & NBT_NM_GROUP)) {
		rcode = NBT_RCODE_RFS;
		goto done;
	}

	/* w2k3 refuses 0x1C names with out marked as group */
	if (name->type == NBT_NAME_LOGON && !(nb_flags & NBT_NM_GROUP)) {
		rcode = NBT_RCODE_RFS;
		goto done;
	}

	/* w2k3 refuses 0x1E names with out marked as group */
	if (name->type == NBT_NAME_BROWSER && !(nb_flags & NBT_NM_GROUP)) {
		rcode = NBT_RCODE_RFS;
		goto done;
	}

	if (name->scope && strlen(name->scope) > 237) {
		rcode = NBT_RCODE_SVR;
		goto done;
	}

	duplicate_packet = wins_check_wack_queue(iface, packet, src);
	if (duplicate_packet) {
		/* just ignore the packet */
		DEBUG(5,("Ignoring duplicate packet while WACK is pending from %s:%d\n",
			 src->addr, src->port));
		return;
	}

	status = winsdb_lookup(winssrv->wins_db, name, packet, &rec);
	if (NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_NOT_FOUND, status)) {
		rcode = wins_register_new(nbtsock, packet, src, new_type);
		goto done;
	} else if (!NT_STATUS_IS_OK(status)) {
		rcode = NBT_RCODE_SVR;
		goto done;
	} else if (rec->is_static) {
		if (rec->type == WREPL_TYPE_GROUP || rec->type == WREPL_TYPE_SGROUP) {
			rcode = NBT_RCODE_OK;
			goto done;
		}
		rcode = NBT_RCODE_ACT;
		goto done;
	}

	if (rec->type == WREPL_TYPE_GROUP) {
		if (new_type != WREPL_TYPE_GROUP) {
			DEBUG(2,("WINS: Attempt to register name %s as non normal group(%u)"
				 " while a normal group is already there\n",
				 nbt_name_string(packet, name), new_type));
			rcode = NBT_RCODE_ACT;
			goto done;
		}

		if (rec->state == WREPL_STATE_ACTIVE) {
			/* TODO: is this correct? */
			rcode = wins_update_ttl(nbtsock, packet, rec, NULL, src);
			goto done;
		}

		/* TODO: is this correct? */
		winsdb_delete(winssrv->wins_db, rec);
		rcode = wins_register_new(nbtsock, packet, src, new_type);
		goto done;
	}

	if (rec->state != WREPL_STATE_ACTIVE) {
		winsdb_delete(winssrv->wins_db, rec);
		rcode = wins_register_new(nbtsock, packet, src, new_type);
		goto done;
	}

	switch (rec->type) {
	case WREPL_TYPE_UNIQUE:
	case WREPL_TYPE_MHOMED:
		/* 
		 * if its an active unique name, and the registration is for a group, then
		 * see if the unique name owner still wants the name
		 * TODO: is this correct?
		 */
		if (new_type == WREPL_TYPE_GROUP || new_type == WREPL_TYPE_GROUP) {
			wins_register_wack(nbtsock, packet, rec, src, new_type);
			return;
		}

		/* 
		 * if the registration is for an address that is currently active, then 
		 * just update the expiry time of the record and the address
		 */
		winsdb_addr = winsdb_addr_list_check(rec->addresses, address);
		if (winsdb_addr) {
			rcode = wins_update_ttl(nbtsock, packet, rec, winsdb_addr, src);
			goto done;
		}

		/*
		 * we have to do a WACK to see if the current owner is willing
		 * to give up its claim
		 */
		wins_register_wack(nbtsock, packet, rec, src, new_type);
		return;

	case WREPL_TYPE_GROUP:
		/* this should not be reached as normal groups are handled above */
		DEBUG(0,("BUG at %s\n",__location__));
		rcode = NBT_RCODE_ACT;
		goto done;

	case WREPL_TYPE_SGROUP:
		/* if the new record isn't also a special group, refuse the registration */ 
		if (new_type != WREPL_TYPE_SGROUP) {
			DEBUG(2,("WINS: Attempt to register name %s as non special group(%u)"
				 " while a special group is already there\n",
				 nbt_name_string(packet, name), new_type));
			rcode = NBT_RCODE_ACT;
			goto done;
		}

		/* 
		 * if the registration is for an address that is currently active, then 
		 * just update the expiry time of the record and the address
		 */
		winsdb_addr = winsdb_addr_list_check(rec->addresses, address);
		if (winsdb_addr) {
			rcode = wins_update_ttl(nbtsock, packet, rec, winsdb_addr, src);
			goto done;
		}

		rcode = wins_sgroup_merge(nbtsock, packet, rec, address, src);
		goto done;
	}

done:
	nbtd_name_registration_reply(nbtsock, packet, src, rcode);
}

static uint32_t ipv4_match_bits(struct in_addr ip1, struct in_addr ip2)
{
	uint32_t i, j, match=0;
	uint8_t *p1, *p2;

	p1 = (uint8_t *)&ip1.s_addr;
	p2 = (uint8_t *)&ip2.s_addr;

	for (i=0; i<4; i++) {
		if (p1[i] != p2[i]) break;
		match += 8;
	}

	if (i==4) return match;

	for (j=0; j<8; j++) {
		if ((p1[i] & (1<<(7-j))) != (p2[i] & (1<<(7-j))))
			break;
		match++;
	}

	return match;
}

static int nbtd_wins_randomize1Clist_sort(void *p1,/* (const char **) */
					  void *p2,/* (const char **) */
					  struct socket_address *src)
{
	const char *a1 = (const char *)*(const char **)p1;
	const char *a2 = (const char *)*(const char **)p2;
	uint32_t match_bits1;
	uint32_t match_bits2;

	match_bits1 = ipv4_match_bits(interpret_addr2(a1), interpret_addr2(src->addr));
	match_bits2 = ipv4_match_bits(interpret_addr2(a2), interpret_addr2(src->addr));

	return match_bits2 - match_bits1;
}

static void nbtd_wins_randomize1Clist(struct loadparm_context *lp_ctx,
				      const char **addresses, struct socket_address *src)
{
	const char *mask;
	const char *tmp;
	uint32_t num_addrs;
	uint32_t idx, sidx;
	int r;

	for (num_addrs=0; addresses[num_addrs]; num_addrs++) { /* noop */ }

	if (num_addrs <= 1) return; /* nothing to do */

	/* first sort the addresses depending on the matching to the client */
	LDB_TYPESAFE_QSORT(addresses, num_addrs, src, nbtd_wins_randomize1Clist_sort);

	mask = lpcfg_parm_string(lp_ctx, NULL, "nbtd", "wins_randomize1Clist_mask");
	if (!mask) {
		mask = "255.255.255.0";
	}

	/* 
	 * choose a random address to be the first in the response to the client,
	 * preferr the addresses inside the nbtd:wins_randomize1Clist_mask netmask
	 */
	r = random();
	idx = sidx = r % num_addrs;

	while (1) {
		bool same;

		/* if the current one is in the same subnet, use it */
		same = iface_same_net(addresses[idx], src->addr, mask);
		if (same) {
			sidx = idx;
			break;
		}

		/* we need to check for idx == 0, after checking for the same net */
		if (idx == 0) break;
		/* 
		 * if we haven't found an address in the same subnet, search in ones
		 * which match the client more
		 *
		 * some notes:
		 *
		 * it's not "idx = idx % r" but "idx = r % idx"
		 * because in "a % b" b is the allowed range
		 * and b-1 is the maximum possible result, so it must be decreasing
		 * and the above idx == 0 check breaks the while(1) loop.
		 */
		idx = r % idx;
	}

	/* note sidx == 0 is also valid here ... */
	tmp		= addresses[0];
	addresses[0]	= addresses[sidx];
	addresses[sidx]	= tmp;
}

/*
  query a name
*/
static void nbtd_winsserver_query(struct loadparm_context *lp_ctx,
				  struct nbt_name_socket *nbtsock, 
				  struct nbt_name_packet *packet, 
				  struct socket_address *src)
{
	NTSTATUS status;
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	struct nbt_name *name = &packet->questions[0].name;
	struct winsdb_record *rec;
	struct winsdb_record *rec_1b = NULL;
	const char **addresses;
	const char **addresses_1b = NULL;
	uint16_t nb_flags = 0;

	if (name->type == NBT_NAME_MASTER) {
		goto notfound;
	}

	/*
	 * w2k3 returns the first address of the 0x1B record as first address
	 * to a 0x1C query
	 *
	 * since Windows 2000 Service Pack 2 there's on option to trigger this behavior:
	 *
	 * HKEY_LOCAL_MACHINE\System\CurrentControlset\Services\WINS\Parameters\Prepend1BTo1CQueries
	 * Typ: Daten REG_DWORD
	 * Value: 0 = deactivated, 1 = activated
	 */
	if (name->type == NBT_NAME_LOGON && 
	    lpcfg_parm_bool(lp_ctx, NULL, "nbtd", "wins_prepend1Bto1Cqueries", true)) {
		struct nbt_name name_1b;

		name_1b = *name;
		name_1b.type = NBT_NAME_PDC;

		status = winsdb_lookup(winssrv->wins_db, &name_1b, packet, &rec_1b);
		if (NT_STATUS_IS_OK(status)) {
			addresses_1b = winsdb_addr_string_list(packet, rec_1b->addresses);
		}
	}

	status = winsdb_lookup(winssrv->wins_db, name, packet, &rec);
	if (!NT_STATUS_IS_OK(status)) {
		if (!lpcfg_wins_dns_proxy(lp_ctx)) {
			goto notfound;
		}

		if (name->type != NBT_NAME_CLIENT && name->type != NBT_NAME_SERVER) {
			goto notfound;
		}

		nbtd_wins_dns_proxy_query(nbtsock, packet, src);
		return;
	}

	/*
	 * for group's we always reply with
	 * 255.255.255.255 as address, even if
	 * the record is released or tombstoned
	 */
	if (rec->type == WREPL_TYPE_GROUP) {
		addresses = str_list_add(NULL, "255.255.255.255");
		talloc_steal(packet, addresses);
		if (!addresses) {
			goto notfound;
		}
		nb_flags |= NBT_NM_GROUP;
		goto found;
	}

	if (rec->state != WREPL_STATE_ACTIVE) {
		goto notfound;
	}

	addresses = winsdb_addr_string_list(packet, rec->addresses);
	if (!addresses) {
		goto notfound;
	}

	/* 
	 * if addresses_1b isn't NULL, we have a 0x1C query and need to return the
	 * first 0x1B address as first address
	 */
	if (addresses_1b && addresses_1b[0]) {
		const char **addresses_1c = addresses;
		uint32_t i;
		uint32_t num_addrs;

		addresses = str_list_add(NULL, addresses_1b[0]);
		if (!addresses) {
			goto notfound;
		}
		talloc_steal(packet, addresses);
		num_addrs = 1;

		for (i=0; addresses_1c[i]; i++) {
			if (strcmp(addresses_1b[0], addresses_1c[i]) == 0) continue;

			/*
			 * stop when we already have 25 addresses
			 */
			if (num_addrs >= 25) break;

			num_addrs++;			
			addresses = str_list_add(addresses, addresses_1c[i]);
			if (!addresses) {
				goto notfound;
			}
		}
	}

	if (rec->type == WREPL_TYPE_SGROUP) {
		nb_flags |= NBT_NM_GROUP;
	} else {
		nb_flags |= (rec->node <<13);
	}

	/*
	 * since Windows 2000 Service Pack 2 there's on option to trigger this behavior:
	 *
	 * HKEY_LOCAL_MACHINE\System\CurrentControlset\Services\WINS\Parameters\Randomize1CList
	 * Typ: Daten REG_DWORD
	 * Value: 0 = deactivated, 1 = activated
	 */
	if (name->type == NBT_NAME_LOGON && 
	    lpcfg_parm_bool(lp_ctx, NULL, "nbtd", "wins_randomize1Clist", false)) {
		nbtd_wins_randomize1Clist(lp_ctx, addresses, src);
	}

found:
	nbtd_name_query_reply(nbtsock, packet, src, name, 
			      0, nb_flags, addresses);
	return;

notfound:
	nbtd_negative_name_query_reply(nbtsock, packet, src);
}

/*
  release a name
*/
static void nbtd_winsserver_release(struct nbt_name_socket *nbtsock, 
				    struct nbt_name_packet *packet, 
				    struct socket_address *src)
{
	NTSTATUS status;
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	struct nbt_name *name = &packet->questions[0].name;
	struct winsdb_record *rec;
	uint32_t modify_flags = 0;
	uint8_t ret;

	if (name->type == NBT_NAME_MASTER) {
		goto done;
	}

	if (name->scope && strlen(name->scope) > 237) {
		goto done;
	}

	status = winsdb_lookup(winssrv->wins_db, name, packet, &rec);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (rec->is_static) {
		if (rec->type == WREPL_TYPE_UNIQUE || rec->type == WREPL_TYPE_MHOMED) {
			goto done;
		}
		nbtd_name_release_reply(nbtsock, packet, src, NBT_RCODE_ACT);
		return;
	}

	if (rec->state != WREPL_STATE_ACTIVE) {
		goto done;
	}

	/* 
	 * TODO: do we need to check if
	 *       src->addr matches packet->additional[0].rdata.netbios.addresses[0].ipaddr
	 *       here?
	 */

	/* 
	 * we only allow releases from an owner - other releases are
	 * silently ignored
	 */
	if (!winsdb_addr_list_check(rec->addresses, src->addr)) {
		int i;
		DEBUG(4,("WINS: silently ignoring attempted name release on %s from %s\n", nbt_name_string(rec, rec->name), src->addr));
		DEBUGADD(4, ("Registered Addresses: \n"));
		for (i=0; rec->addresses && rec->addresses[i]; i++) {
			DEBUGADD(4, ("%s\n", rec->addresses[i]->address));
		}
		goto done;
	}

	DEBUG(4,("WINS: released name %s from %s\n", nbt_name_string(rec, rec->name), src->addr));

	switch (rec->type) {
	case WREPL_TYPE_UNIQUE:
		rec->state = WREPL_STATE_RELEASED;
		break;

	case WREPL_TYPE_GROUP:
		rec->state = WREPL_STATE_RELEASED;
		break;

	case WREPL_TYPE_SGROUP:
		winsdb_addr_list_remove(rec->addresses, src->addr);
		/* TODO: do we need to take the ownership here? */
		if (winsdb_addr_list_length(rec->addresses) == 0) {
			rec->state = WREPL_STATE_RELEASED;
		}
		break;

	case WREPL_TYPE_MHOMED:
		winsdb_addr_list_remove(rec->addresses, src->addr);
		/* TODO: do we need to take the ownership here? */
		if (winsdb_addr_list_length(rec->addresses) == 0) {
			rec->state = WREPL_STATE_RELEASED;
		}
		break;
	}

	if (rec->state == WREPL_STATE_ACTIVE) {
		/*
		 * If the record is still active, we need to update the
		 * expire_time.
		 *
		 * if we're not the owner, we need to take the ownership.
		 */
		rec->expire_time= time(NULL) + winssrv->config.max_renew_interval;
		if (strcmp(rec->wins_owner, winssrv->wins_db->local_owner) != 0) {
			modify_flags = WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
		}
		if (lpcfg_parm_bool(iface->nbtsrv->task->lp_ctx, NULL, "wreplsrv", "propagate name releases", false)) {
			/*
			 * We have an option to propagate every name release,
			 * this is off by default to match windows servers
			 */
			modify_flags = WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
		}
	} else if (rec->state == WREPL_STATE_RELEASED) {
		/*
		 * if we're not the owner, we need to take the owner ship
		 * and make the record tombstone, but expire after
		 * tombstone_interval + tombstone_timeout and not only after tombstone_timeout
		 * like for normal tombstone records.
		 * This is to replicate the record directly to the original owner,
		 * where the record is still active
		 */ 
		if (strcmp(rec->wins_owner, winssrv->wins_db->local_owner) == 0) {
			rec->expire_time= time(NULL) + winssrv->config.tombstone_interval;
		} else {
			rec->state	= WREPL_STATE_TOMBSTONE;
			rec->expire_time= time(NULL) + 
					  winssrv->config.tombstone_interval +
					  winssrv->config.tombstone_timeout;
			modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
		}
	}

	ret = winsdb_modify(winssrv->wins_db, rec, modify_flags);
	if (ret != NBT_RCODE_OK) {
		DEBUG(1,("WINS: FAILED: released name %s at %s: error:%u\n",
			nbt_name_string(rec, rec->name), src->addr, ret));
	}
done:
	/* we match w2k3 by always giving a positive reply to name releases. */
	nbtd_name_release_reply(nbtsock, packet, src, NBT_RCODE_OK);
}


/*
  answer a name query
*/
void nbtd_winsserver_request(struct nbt_name_socket *nbtsock, 
			     struct nbt_name_packet *packet, 
			     struct socket_address *src)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private_data,
						       struct nbtd_interface);
	struct wins_server *winssrv = iface->nbtsrv->winssrv;
	if ((packet->operation & NBT_FLAG_BROADCAST) || winssrv == NULL) {
		return;
	}

	switch (packet->operation & NBT_OPCODE) {
	case NBT_OPCODE_QUERY:
		nbtd_winsserver_query(iface->nbtsrv->task->lp_ctx, nbtsock, packet, src);
		break;

	case NBT_OPCODE_REGISTER:
	case NBT_OPCODE_REFRESH:
	case NBT_OPCODE_REFRESH2:
	case NBT_OPCODE_MULTI_HOME_REG:
		nbtd_winsserver_register(nbtsock, packet, src);
		break;

	case NBT_OPCODE_RELEASE:
		nbtd_winsserver_release(nbtsock, packet, src);
		break;
	}

}

/*
  startup the WINS server, if configured
*/
NTSTATUS nbtd_winsserver_init(struct nbtd_server *nbtsrv)
{
	uint32_t tmp;
	const char *owner;

	if (!lpcfg_wins_support(nbtsrv->task->lp_ctx)) {
		nbtsrv->winssrv = NULL;
		return NT_STATUS_OK;
	}

	nbtsrv->winssrv = talloc_zero(nbtsrv, struct wins_server);
	NT_STATUS_HAVE_NO_MEMORY(nbtsrv->winssrv);

	nbtsrv->winssrv->config.max_renew_interval = lpcfg_max_wins_ttl(nbtsrv->task->lp_ctx);
	nbtsrv->winssrv->config.min_renew_interval = lpcfg_min_wins_ttl(nbtsrv->task->lp_ctx);
	tmp = lpcfg_parm_int(nbtsrv->task->lp_ctx, NULL, "wreplsrv", "tombstone_interval", 6*24*60*60);
	nbtsrv->winssrv->config.tombstone_interval = tmp;
	tmp = lpcfg_parm_int(nbtsrv->task->lp_ctx, NULL, "wreplsrv"," tombstone_timeout", 1*24*60*60);
	nbtsrv->winssrv->config.tombstone_timeout = tmp;

	owner = lpcfg_parm_string(nbtsrv->task->lp_ctx, NULL, "winsdb", "local_owner");

	if (owner == NULL) {
		struct interface *ifaces;
		load_interfaces(nbtsrv->task, lpcfg_interfaces(nbtsrv->task->lp_ctx), &ifaces);
		owner = iface_n_ip(ifaces, 0);
	}

	nbtsrv->winssrv->wins_db     = winsdb_connect(nbtsrv->winssrv, nbtsrv->task->event_ctx, 
						      nbtsrv->task->lp_ctx,
						      owner, WINSDB_HANDLE_CALLER_NBTD);
	if (!nbtsrv->winssrv->wins_db) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	irpc_add_name(nbtsrv->task->msg_ctx, "wins_server");

	return NT_STATUS_OK;
}
