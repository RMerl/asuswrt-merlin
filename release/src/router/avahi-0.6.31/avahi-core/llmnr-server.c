#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <avahi-common/domain.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>

#include "iface.h"
#include "socket.h"
#include "log.h"
#include "addr-util.h"
#include "domain-util.h"
#include "rr-util.h"
#include "hashmap.h"

#include "llmnr-querier.h"
#include "llmnr-query-sched.h"
#include "verify.h"
#include "llmnr-response.h"
#include "llmnr-lookup.h"
#include "dns.h"

/* Server methods */
static void avahi_llmnr_server_prepare_response(AvahiServer *s, AvahiInterface *i, AvahiEntry *e) {
    assert(s);
    assert(i);
    assert(e);
    assert(e->type == AVAHI_ENTRY_LLMNR);

    avahi_record_list_push(s->llmnr.record_list,
                           e->record,
                           e->flags & AVAHI_PUBLISH_UNIQUE,
                           avahi_llmnr_entry_is_verifying(s, e, i),
                           0);
    assert(!avahi_record_list_is_empty(s->llmnr.record_list));
}

static void avahi_llmnr_server_prepare_matching_responses(AvahiServer *s, AvahiInterface *i, AvahiKey *key) {
    assert(s);
    assert(i);
    assert(key);

    /* ANY queries*/
    if (avahi_key_is_pattern(key)) {
        AvahiEntry *e;

        for (e = s->llmnr.entries; e; e = e->entries_next)
            if ((!e->dead && avahi_key_pattern_match(key, e->record->key) &&
                (avahi_llmnr_entry_is_verifying(s, e, i) != -1)) )

                avahi_llmnr_server_prepare_response(s, i, e);

    } else {
        AvahiEntry *e;

        /* Other queries */

        for (e = avahi_hashmap_lookup(s->llmnr.entries_by_key, key); e; e = e->by_key_next) {

            if (!e->dead &&
                (avahi_llmnr_entry_is_verifying(s, e, i) != -1) )
                avahi_llmnr_server_prepare_response(s, i, e);
        }
    }
    /* Look for CNAME records */

    if ((key->clazz == AVAHI_DNS_CLASS_IN || key->clazz == AVAHI_DNS_CLASS_ANY)
        && key->type != AVAHI_DNS_TYPE_CNAME && key->type != AVAHI_DNS_TYPE_ANY) {
        AvahiKey *cname_key;
        if (!(cname_key = avahi_key_new(key->name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME)))
            return;

        avahi_llmnr_server_prepare_matching_responses(s, i, cname_key);
        avahi_key_unref(cname_key);
    }

}

static void avahi_s_cleanup_dead_entries(AvahiServer *s) {
    assert(s);

    /* Cleanup dead groups (AvahiSEntryGroup) */
    if (s->llmnr.need_group_cleanup) {
        AvahiSEntryGroup *g;

        for (g = s->llmnr.groups; g; g = g->groups_next) {
            assert(g->type == AVAHI_GROUP_LLMNR);

            if (g->dead)
                avahi_entry_group_free(s, g);
        }

        s->llmnr.need_group_cleanup = 0;
    }

    if (s->llmnr.need_entry_cleanup) {
        AvahiEntry *e;

        for (e = s->llmnr.entries; e; e = e->entries_next)
            if (e->dead)
                avahi_entry_free(s, e);

        s->llmnr.need_entry_cleanup = 0;
    }
}

static void llmnr_append_aux_callback(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata) {
    int *unicast_response = userdata;

    assert(s);
    assert(r);
    assert(unicast_response);
    avahi_record_list_push(s->llmnr.record_list, r, flush_cache, *unicast_response, 1);
}

static void llmnr_append_aux_records_to_list(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, int unicast_response) {
    assert(s);
    assert(r);
    llmnr_server_enumerate_aux_records(s, i, r, llmnr_append_aux_callback, &unicast_response);
}

static void llmnr_enum_aux_records(AvahiServer *s, AvahiInterface *i, const char *name, uint16_t type, void (*callback)(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata), void* userdata) {
    assert(s);
    assert(i);
    assert(name);
    assert(callback);

    if (type == AVAHI_DNS_TYPE_ANY) {
        AvahiEntry *e;
        for (e = s->llmnr.entries; e; e = e->entries_next)
            if (!e->dead && (e->type == AVAHI_ENTRY_LLMNR) && (avahi_llmnr_entry_is_verifying(s, e, i) != -1) &&
		(e->record->key->clazz == AVAHI_DNS_CLASS_IN)&& avahi_domain_equal(name, e->record->key->name))	//Edison
                callback(s, e->record, e->flags & AVAHI_PUBLISH_UNIQUE, userdata);
    } else {
        AvahiEntry *e;
        AvahiKey *k;

        if (!(k = avahi_key_new(name, AVAHI_DNS_CLASS_IN, type)))
            return; /** OOM */
        for (e = avahi_hashmap_lookup(s->llmnr.entries_by_key, k); e; e = e->by_key_next)
	    if (!e->dead && e->type == AVAHI_ENTRY_LLMNR && (avahi_llmnr_entry_is_verifying(s, e, i) != -1))
                callback(s, e->record, e->flags & AVAHI_PUBLISH_UNIQUE, userdata);

        avahi_key_unref(k);
    }
}

void llmnr_server_enumerate_aux_records(AvahiServer *s, AvahiInterface *i, AvahiRecord *r, void (*callback)(AvahiServer *s, AvahiRecord *r, int flush_cache, void* userdata), void* userdata) {
    assert(s);
    assert(i);
    assert(r);
    assert(callback);
    /* Call the specified callback far all records referenced by the one specified in *r */

    if (r->key->clazz == AVAHI_DNS_CLASS_IN) {
  	if (r->key->type == AVAHI_DNS_TYPE_CNAME)
            llmnr_enum_aux_records(s, i, r->data.cname.name, AVAHI_DNS_TYPE_ANY, callback, userdata);
    }
}

static void avahi_llmnr_server_generate_response(
    AvahiServer *s,
    AvahiDnsPacket *p,
    AvahiInterface *i,
    const AvahiAddress *address,
    uint16_t port,
    int empty_response) {

    /* empty_response == 0 */
    /* 1. If the query was for A/AAAA or PTR records we must have only single RR.
    2. If the query was for ANY type we *should* have only single RR. If more than
    single RR, we send them in different packets as what should be the value of 'c'
    and 't' bit in case of multiple records in a single packet is not specified in RFC */

    /* empty_response == 1 */
    /* Ignore the RR's in sl->record_list, just send an empty RR */

    AvahiDnsPacket *reply = NULL;
    AvahiRecord *r;
    int c, t, send_packet = 0;

    if (!empty_response) {
        /* Get a reply packet */
        reply = avahi_llmnr_packet_new_reply(p, 512 + AVAHI_DNS_PACKET_EXTRA_SIZE, 1, !c);

        while ((r = avahi_record_list_next(s->llmnr.record_list, &c, &t, NULL))) {
		
		llmnr_append_aux_records_to_list(s, i, r, t);	//Edison

            /* Append Record */
            if (!avahi_llmnr_packet_append_record(reply, r, 30)) {

                /*Send this packet*/
                if (avahi_dns_packet_get_field(reply, AVAHI_LLMNR_FIELD_ANCOUNT != 0)) {
                        send_packet = 1;
                        /* If we send rest of the records in a new packet that will be discarded
                           So there is no point sending that. We should have set the T bit and wait
                           for the TCP query but we do not support TCP processing yet. Break*/
                        /* TODO set errno*/
                        break;
                }

            } else {
                send_packet = 1;
                /* Increment field */
                avahi_dns_packet_inc_field(reply, AVAHI_LLMNR_FIELD_ANCOUNT);
                avahi_record_unref(r);
            }
        }

    } else {
        reply = avahi_llmnr_packet_new_reply(p, 512 + AVAHI_DNS_PACKET_EXTRA_SIZE, 1, !c);
        send_packet = 1;
    }

    avahi_record_list_flush(s->llmnr.record_list);

    if (send_packet) {
        assert(reply);
        if (!avahi_schedule_llmnr_response_job(i->llmnr.response_scheduler, reply, address, port))
            return;
    }

    return;
}

static void prepend_response_packet(int c_bit, AvahiLLMNRQuery *lq, AvahiDnsPacket *p) {
    AvahiRecord *r = NULL;
    AvahiKey *key;
    int n_records;

    assert(lq);
    assert(p);

    /*TODO Duplicate records*/
    n_records = avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_ANCOUNT);

    /* Consume Key first */
    key = avahi_llmnr_packet_consume_key(p);

    assert(avahi_key_equal(key, lq->key));

    if (c_bit) {
        /* Prepend in C cit set list */
        for (; n_records ; n_records--) {
            if (!(r = avahi_llmnr_packet_consume_record(p)))
                return;

            avahi_record_list_push(lq->c_bit_set, r, 1, 0, 0);
        }

    } else {
        /* Prepend in C bit clear list*/
        for (; n_records ; n_records--) {

            if (!(r = avahi_llmnr_packet_consume_record(p)))
                return;

            avahi_record_list_push(lq->c_bit_clear, r, 0, 0, 0);
        }
    }
}

static void prepend_uniqueness_verification_packet(AvahiLLMNRQuery *lq, const AvahiAddress *src_address) {
    AvahiVerifierData *vdata;

    assert(lq);
    assert(src_address);

    vdata = (AvahiVerifierData *)(lq->userdata);

    if (vdata->address)
    {
        assert((vdata->t_bit));
        /* vdata->address is not NULL => we have already filled this field.    Since we
        issue uniqueness verification query only for those entries that are meant to
        be UNIQUE, this should either be a duplicate response or entry is not UNIQUE.
        In any case we put AvahiLLMNREntryVerify object state into AVAHI_CONFLICT. */
        if (avahi_address_cmp(vdata->address, src_address) == 0)
            /* Addresses match, means a duplicate packet */
            return;
        else /* Address don't match =>     AVAHI_CONFLICT */
            vdata->ev->state = AVAHI_CONFLICT;

    } else {
        /* Set 't' bit */
        (vdata->t_bit) = 1;
        vdata->address = src_address;
    }

    return;
}

static void handle_conflict_query (AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i) {
    AvahiKey *key;

    assert(s);
    assert(p);
    assert(i);

    if (!(key = avahi_llmnr_packet_consume_key(p)))
        return;

    if (avahi_key_is_pattern(key)) {
       AvahiEntry *e;

       for (e = s->llmnr.entries; e; e = e->entries_next)
            if ((!e->dead && avahi_key_pattern_match(key, e->record->key) &&
                (avahi_llmnr_entry_is_verifying(s, e, i) == 0)) )
                /* Reverify query */
                avahi_reverify_entry(s, e);

    } else {
        AvahiEntry *e;
        /* Other queries */
        for (e = avahi_hashmap_lookup(s->llmnr.entries_by_key, key); e; e = e->by_key_next)
            if (!e->dead && (avahi_llmnr_entry_is_verifying(s, e, i) == 0))
                /* Reverify query */
                avahi_reverify_entry(s, e);
    }

    return;
}

static void handle_response_packet (AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *src_address, uint16_t port) {
    uint16_t flags;
    uint32_t id;
    AvahiLLMNRQuery *lq;

    assert(s);
    assert(i);
    assert(p);
    assert(src_address);
    assert(port);

    id = (uint32_t) (avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_ID));
    flags = avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_FLAGS);

    /* AvahiLLMNRQuery object that issued this query */
    if ( !(lq = avahi_hashmap_lookup(s->llmnr.llmnr_lookup_engine->queries_by_id, &id)) ||
        (lq->dead) )
        goto finish;

    assert(lq->type != AVAHI_LLMNR_CONFLICT_QUERY);

    /* Check for the 'tc' bit */
    if (flags & AVAHI_LLMNR_FLAG_TC) {
        /*TODO: TCP Query processing .Not yet supported
         * Not supported by vista machines either. ;)*/
        /**    send_tcp_query(sl, i, p, src_address); **/
        return;
    }

    switch(lq->type) {

        case AVAHI_LLMNR_SIMPLE_QUERY :
             /* Check for 't' bit. For simple queries should be clear*/
            if ((flags & AVAHI_LLMNR_FLAG_T) == 1)
                goto finish;
            else
                prepend_response_packet(flags & AVAHI_LLMNR_FLAG_C, lq, p);
            break;

        case AVAHI_LLMNR_UNIQUENESS_VERIFICATION_QUERY :
            /* If 't' bit is clear, we will prepend in list according to 'c' bit
            and will check at last whether there is already a conflict about this entry*/
            if (!(flags & AVAHI_LLMNR_FLAG_T))
                prepend_response_packet(flags & AVAHI_LLMNR_FLAG_C, lq, p);
            else /* 't' bit is set => we are not concerned with 'c' bit now*/
                prepend_uniqueness_verification_packet(lq, src_address);
            break;

        default :
            break;
    }
    return;

finish:
    avahi_dns_packet_free(p);
    return;
}

static void handle_query_packet(AvahiServer *s, AvahiDnsPacket *p, AvahiInterface *i, const AvahiAddress *src_address, uint16_t port) {
    AvahiKey *key, *new_key;

    assert(s);
    assert(i);
    assert(p);
    assert(src_address);
    assert(port);
    assert(avahi_record_list_is_empty(s->llmnr.record_list));

    if (!(key = avahi_llmnr_packet_consume_key(p)))
        return;

    /* Fill sl->record_list with valid RR/'s, if any */
    avahi_llmnr_server_prepare_matching_responses(s, i, key);

    if (!avahi_record_list_is_empty(s->llmnr.record_list)) {
        avahi_llmnr_server_generate_response(s, p, i, src_address, port, 0);
        avahi_key_unref(key);

    } else if (!(key->type == AVAHI_DNS_TYPE_ANY)) {

        /* We don't have any valid verified record for this key.
        Now we check for records with 'type = AVAHI_DNS_TYPE_ANY'
        with same name to see if we are autoritative for that name
        for another type of query. In that case we send an empty
        response packet. */
        new_key = avahi_key_new(key->name, key->clazz, AVAHI_DNS_TYPE_ANY);
        avahi_llmnr_server_prepare_matching_responses(s, i, new_key);
        avahi_key_unref(new_key);

        if (!avahi_record_list_is_empty(s->llmnr.record_list))
            /* We do have record for type ANY. Send an empty response */
            avahi_llmnr_server_generate_response(s, p, i, src_address, port, 1);
    }

    return;
}

static int originates_from_local_iface(AvahiServer *s, AvahiIfIndex iface, const AvahiAddress *a, uint16_t port) {

    assert(s);
    assert(iface != AVAHI_IF_UNSPEC);
    assert(a);

    if (port != AVAHI_LLMNR_PORT)
        return 0;

    return avahi_interface_has_address(s->monitor, iface, a);
}


static int is_llmnr_mcast_address(const AvahiAddress *a) {
    AvahiAddress b;
    assert(a);

    avahi_address_parse(a->proto == AVAHI_PROTO_INET ? AVAHI_IPV4_LLMNR_GROUP : AVAHI_IPV6_LLMNR_GROUP, a->proto, &b);

    return avahi_address_cmp(a, &b) == 0;
}

static void dispatch_packet (
    AvahiServer *s,
    AvahiDnsPacket *p,
    const AvahiAddress *src_address,
    uint16_t port,
    const AvahiAddress *dst_address,
    AvahiIfIndex iface,
    int ttl) {

    /* Get Interface here*/
    AvahiInterface *i;
    uint16_t flags;

    assert(s);
    assert(p);
    assert(src_address);
    assert(dst_address);
    assert(iface > 0);
    assert(src_address->proto == dst_address->proto);


    if (!(i = avahi_interface_monitor_get_interface(s->monitor, iface, src_address->proto)) || !i->llmnr.verifying)
        return;

    /* Discard our own packets */
    if (originates_from_local_iface(s, iface, src_address, port))
        return;

    if (avahi_llmnr_packet_check_valid(p) < 0)
        return;

    flags = avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_FLAGS);

    if (!(flags & AVAHI_LLMNR_FLAG_QR)) {

        /* UDP queries should be multicast (Queries can be sent over any port)
        So we don't check for the port here.*/
        if (!(is_llmnr_mcast_address(dst_address)))
            return;

        /* Make sure 'tc' bit is clear. Ignore 'rcode' and 't' bitsTODO, TC bit set in Vista Packets*/
        if ( (flags & AVAHI_LLMNR_FLAG_TC) ||
            (avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_QDCOUNT) != 1 ) ||
            (avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_ANCOUNT) != 0) ||
            (avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_NSCOUNT) != 0) )
                return;

        /* Check for the 'c' bit */
        if (!(flags & AVAHI_LLMNR_FLAG_C))
        /* Simple_LLMNR_Query packet*/
            handle_query_packet(s, p, i, src_address, port);

        /* Received a conflict query. We don't respond to this query.
        Rather start uniquness verification process.*/
        else
            handle_conflict_query(s, p, i);

    } else /* Response packet */ {

        /* Response must be unicast and must be sent through standard
        LLMNR port*/
        if ((is_llmnr_mcast_address(dst_address)) &&
           (port != AVAHI_LLMNR_PORT))
            return;

        /* RCODE must be clear for LLMNR unicast UDP responses */
        if ((flags & AVAHI_LLMNR_FLAG_RCODE))
            return;

        /* sl->server->config.check_response_ttl, same goes for LLMNR*/
        if (ttl != 255 && s->config.check_response_ttl) {
            avahi_log_warn("Received response with invalid TTL %u on interface '%s.%i'.", ttl, i->hardware->name, i->protocol);
            return;
        }

        if (avahi_dns_packet_get_field(p, AVAHI_LLMNR_FIELD_QDCOUNT) != 1)
            return;

        handle_response_packet(s, p, i, src_address, port);
    }

    return;
}


static void llmnr_socket_event(AvahiWatch *w, int fd, AvahiWatchEvent event, void *userdata) {
    AvahiServer *s = userdata;
    AvahiAddress dest, src;
    AvahiDnsPacket *p = NULL;
    /* Pass the Interface Index from this function */
    AvahiIfIndex iface;

    uint16_t port;
    uint8_t ttl;

    assert(w);
    assert(fd >= 0);
    assert(event & AVAHI_WATCH_IN);

    if (fd == s->llmnr.fd_ipv4) {
        dest.proto = src.proto = AVAHI_PROTO_INET;
        p = avahi_recv_dns_packet_ipv4(s->llmnr.fd_ipv4, &src.data.ipv4, &port, &dest.data.ipv4, &iface, &ttl);

    } else {
        assert(fd == s->llmnr.fd_ipv6) ;
        dest.proto = src.proto = AVAHI_PROTO_INET6 ;
        p = avahi_recv_dns_packet_ipv6(s->llmnr.fd_ipv6, &src.data.ipv6, &port, &dest.data.ipv6, &iface, &ttl);
    }

    if (p) {
        /* Find Interface Index if iface == AVAHI_IF_UNSPEC presently with the help of dest address */
        if (iface == AVAHI_IF_UNSPEC)
            iface = avahi_find_interface_for_address(s->monitor, &dest);

        /* we should have some specific value in iface otherwise we can't dispatch the packet */
        if (iface != AVAHI_IF_UNSPEC)
            dispatch_packet(s, p, &src, port, &dest, iface, ttl);
        else
            avahi_log_error("Invalid address");

        avahi_dns_packet_free(p);

        /* TODO clean-up */
        avahi_s_cleanup_dead_entries(s);
    }
}

int setup_llmnr_sockets(AvahiServer *s) {
    assert(s);

    s->llmnr.fd_ipv4 = s->config.use_ipv4 ? avahi_open_socket_ipv4(s->config.disallow_other_stacks, AVAHI_LLMNR) : -1;
    s->llmnr.fd_ipv6 = s->config.use_ipv6 ? avahi_open_socket_ipv6(s->config.disallow_other_stacks, AVAHI_LLMNR) : -1;

    if (s->llmnr.fd_ipv6 < 0 && s->llmnr.fd_ipv4 < 0)
        return AVAHI_ERR_NO_NETWORK;

    if (s->llmnr.fd_ipv4 < 0 && s->config.use_ipv4)
        avahi_log_notice("Failed to create IPv4 LLMNR socket, proceeding in IPv6 only mode");
    else if (s->llmnr.fd_ipv6 < 0 && s->config.use_ipv6)
        avahi_log_notice("Failed to create IPv6 LLMNR socket, proceeding in IPv4 only mode");

    s->llmnr.watch_ipv4 = s->llmnr.watch_ipv6 = NULL;

    if (s->llmnr.fd_ipv4 >= 0)
        s->llmnr.watch_ipv4 = s->poll_api->watch_new(s->poll_api, s->llmnr.fd_ipv4, AVAHI_WATCH_IN, llmnr_socket_event, s);
    if (s->llmnr.fd_ipv6 >= 0)
        s->llmnr.watch_ipv6 = s->poll_api->watch_new(s->poll_api, s->llmnr.fd_ipv6, AVAHI_WATCH_IN, llmnr_socket_event, s);

    return 0;
}

