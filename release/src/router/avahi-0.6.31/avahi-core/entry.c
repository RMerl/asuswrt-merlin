/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <avahi-common/domain.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>
#include <avahi-common/defs.h>

#include "internal.h"
#include "iface.h"
#include "socket.h"
#include "browse.h"
#include "log.h"
#include "util.h"
#include "dns-srv-rr.h"
#include "rr-util.h"
#include "domain-util.h"

static void transport_flags_from_domain(AvahiServer *s, AvahiPublishFlags *flags, const char *domain, int check_for_llmnr) {
    assert(flags);
    assert(domain);
    assert(!((*flags & AVAHI_PUBLISH_USE_MULTICAST) && (*flags & AVAHI_PUBLISH_USE_WIDE_AREA)));

    if (check_for_llmnr) {
        assert(!((*flags & AVAHI_PUBLISH_USE_WIDE_AREA) && (*flags & AVAHI_PUBLISH_USE_LLMNR)));
        assert(!((*flags & AVAHI_PUBLISH_USE_MULTICAST) && (*flags & AVAHI_PUBLISH_USE_LLMNR)));

        if (*flags & (AVAHI_PUBLISH_USE_MULTICAST | AVAHI_PUBLISH_USE_WIDE_AREA | AVAHI_PUBLISH_USE_LLMNR))
            return;

    } else /*!check_for_llmnr*/
        if (*flags & (AVAHI_PUBLISH_USE_MULTICAST | AVAHI_PUBLISH_USE_WIDE_AREA))
            return;

    if (check_for_llmnr && avahi_is_valid_host_name(domain))
           *flags |= AVAHI_PUBLISH_USE_LLMNR;
    else if (!s->wide_area.wide_area_lookup_engine ||
             !avahi_wide_area_has_servers(s->wide_area.wide_area_lookup_engine) ||
             avahi_domain_ends_with(domain, AVAHI_MDNS_SUFFIX_LOCAL) ||
             avahi_domain_ends_with(domain, AVAHI_MDNS_SUFFIX_ADDR_IPV4) ||
             avahi_domain_ends_with(domain, AVAHI_MDNS_SUFFIX_ADDR_IPV6))
        *flags |= AVAHI_PUBLISH_USE_MULTICAST;
    else
        *flags |= AVAHI_PUBLISH_USE_WIDE_AREA;
}

void avahi_entry_free(AvahiServer*s, AvahiEntry *e) {
    AvahiEntry *t;
    AvahiHashmap *entries_by_key;

    assert(s);
    assert(e);

    if (e->type == AVAHI_ENTRY_MDNS) {
        avahi_goodbye_entry(s, e, 1, 1);
        AVAHI_LLIST_REMOVE(AvahiEntry, entries, s->mdns.entries, e);
        entries_by_key = s->mdns.entries_by_key;
    } else {
        assert (e->type == AVAHI_ENTRY_LLMNR);
        avahi_remove_verifiers (s, e);
        AVAHI_LLIST_REMOVE(AvahiEntry, entries, s->llmnr.entries, e);
        entries_by_key = s->llmnr.entries_by_key;
    }

    t = avahi_hashmap_lookup(entries_by_key, e->record->key);
    AVAHI_LLIST_REMOVE(AvahiEntry, by_key, t, e);
    if (t)
        avahi_hashmap_replace(entries_by_key, t->record->key, t);
    else
        avahi_hashmap_remove(entries_by_key, e->record->key);

    /* Remove from associated group */
    if (e->group)
        AVAHI_LLIST_REMOVE(AvahiEntry, by_group, e->group->entries, e);

    avahi_record_unref(e->record);
    avahi_free(e);
}

void avahi_entry_group_free(AvahiServer *s, AvahiSEntryGroup *g) {
    assert(s);
    assert(g);

    while (g->entries)
        avahi_entry_free(s, g->entries);

    if (g->type == AVAHI_GROUP_MDNS && g->proto.mdns.register_time_event)
        avahi_time_event_free(g->proto.mdns.register_time_event);

    if (g->type == AVAHI_GROUP_MDNS)
        AVAHI_LLIST_REMOVE(AvahiSEntryGroup, groups, s->mdns.groups, g);
    else
        AVAHI_LLIST_REMOVE(AvahiSEntryGroup, groups, s->llmnr.groups, g);

    avahi_free(g);
}

void avahi_cleanup_dead_entries(AvahiServer *s) {
    assert(s);

    if (s->mdns.need_group_cleanup) {
        AvahiSEntryGroup *g, *next;

        for (g = s->mdns.groups; g; g = next) {
            next = g->groups_next;

            if (g->dead)
                avahi_entry_group_free(s, g);
        }

        s->mdns.need_group_cleanup = 0;
    }

    if (s->mdns.need_entry_cleanup) {
        AvahiEntry *e, *next;

        for (e = s->mdns.entries; e; e = next) {
            next = e->entries_next;

            if (e->dead)
                avahi_entry_free(s, e);
        }

        s->mdns.need_entry_cleanup = 0;
    }

    if (s->need_browser_cleanup)
        avahi_browser_cleanup(s);

    if (s->cleanup_time_event) {
        avahi_time_event_free(s->cleanup_time_event);
        s->cleanup_time_event = NULL;
    }
}

static int check_record_conflict(AvahiServer *s, AvahiIfIndex interface, AvahiProtocol protocol, AvahiRecord *r, AvahiPublishFlags flags, AvahiPublishProtocol proto) {
    AvahiEntry *e;

    assert(s);
    assert(r);

    for (e = avahi_hashmap_lookup(proto == AVAHI_MDNS ? s->mdns.entries_by_key : s->llmnr.entries_by_key, r->key); e; e = e->by_key_next) {

        if (proto == AVAHI_MDNS)
            assert(e->type == AVAHI_ENTRY_MDNS);
        else
            assert(e->type == AVAHI_ENTRY_LLMNR);

        if (e->dead)
            continue;

        if (!(flags & AVAHI_PUBLISH_UNIQUE) && !(e->flags & AVAHI_PUBLISH_UNIQUE))
            continue;

        if ((flags & AVAHI_PUBLISH_ALLOW_MULTIPLE) && (e->flags & AVAHI_PUBLISH_ALLOW_MULTIPLE) )
            continue;

        if (avahi_record_equal_no_ttl(r, e->record)) {
            /* The records are the same, not a conflict in any case */
            continue;
        }

        if ((interface <= 0 ||
             e->interface <= 0 ||
             e->interface == interface) &&
            (protocol == AVAHI_PROTO_UNSPEC ||
             e->protocol == AVAHI_PROTO_UNSPEC ||
             e->protocol == protocol))

            return -1;
    }

    return 0;
}

static AvahiEntry *server_add_llmnr_internal(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    AvahiRecord *r) {

    AvahiEntry *e;

    assert(s);
    assert(r);

    /* Flags should be LLMNR flags only */
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, AVAHI_FLAGS_VALID(
                                    flags,
                                    AVAHI_PUBLISH_UNIQUE|
                                    AVAHI_PUBLISH_ALLOW_MULTIPLE|
                                    AVAHI_PUBLISH_UPDATE|
                                    AVAHI_PUBLISH_NO_VERIFY|
                                    AVAHI_PUBLISH_USE_LLMNR), AVAHI_ERR_INVALID_FLAGS);

    /* Publishing of only A/AAAA/PTR records is supported using LLMNR */
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                    (r->key->type == AVAHI_DNS_TYPE_A) ||
                                    (r->key->type == AVAHI_DNS_TYPE_AAAA) ||
				    (r->key->type == AVAHI_DNS_TYPE_CNAME) ||
                                    (r->key->type == AVAHI_DNS_TYPE_PTR), AVAHI_ERR_INVALID_DNS_TYPE);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                     !g ||
                                     (g->state != AVAHI_ENTRY_GROUP_LLMNR_ESTABLISHED &&
                                      g->state != AVAHI_ENTRY_GROUP_LLMNR_VERIFYING) ||
                                     (flags & AVAHI_PUBLISH_UPDATE), AVAHI_ERR_BAD_STATE);

    /* Copy Copy Copy. */
    if (flags & AVAHI_PUBLISH_UPDATE) {
        AvahiRecord *old_record;
        int is_first = 1;

        /* type can't be _UNSET*/
        AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !g || g->type == AVAHI_GROUP_LLMNR, AVAHI_ERR_INVALID_GROUP);

        /* Find the first matching entry */
        for (e = avahi_hashmap_lookup(s->llmnr.entries_by_key, r->key); e; e = e->by_key_next) {

            assert(e->type == AVAHI_ENTRY_LLMNR);
            if (!e->dead && e->group == g && e->interface == interface && e->protocol == protocol)
                break;

            is_first = 0;
        }

        if (!e) {
            avahi_server_set_errno(s, AVAHI_ERR_NOT_FOUND);
            return NULL;
        }

        /* Update the entry */
        old_record = e->record;
        e->record = avahi_record_ref(r);
        e->flags = flags;

        /* Reverify changes, if needed */
        if (!avahi_record_equal_no_ttl(old_record, r) && (!g || g->state != AVAHI_ENTRY_GROUP_LLMNR_UNCOMMITED))
            /* Reverify our updated entry */
            avahi_reverify_entry(s, e);

        /* If we were the first entry in the list, we need to update the key */
        if (is_first)
            avahi_hashmap_replace(s->llmnr.entries_by_key, e->record->key, e);

        avahi_record_unref(old_record);

    } else {
        AvahiEntry *t;

        AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                         !g ||
                                         g->type == AVAHI_GROUP_UNSET ||
                                         g->type == AVAHI_GROUP_LLMNR, AVAHI_ERR_INVALID_GROUP);

        if (g && g->type == AVAHI_GROUP_UNSET) {
            g->proto.llmnr.n_verifying = 0;
            g->proto.llmnr.n_entries = 0;
            g->type = AVAHI_GROUP_LLMNR;

            AVAHI_LLIST_HEAD_INIT(AvahiEntry, g->entries);
            AVAHI_LLIST_PREPEND(AvahiSEntryGroup, groups, s->llmnr.groups, g);
        }
        /* Check for the conflict */

        if (check_record_conflict(s, interface, protocol, r, flags, AVAHI_LLMNR) < 0) {
            avahi_server_set_errno(s, AVAHI_ERR_COLLISION);
            return NULL;
        }

        if (!(e = avahi_new(AvahiEntry, 1))) {
            avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            return NULL;
        }

        e->server = s;
        e->record = avahi_record_ref(r);
        e->group = g;
        e->interface = interface;
        e->protocol = protocol;
        e->flags = flags;
        e->dead = 0;
        e->type = AVAHI_ENTRY_LLMNR;
        AVAHI_LLIST_HEAD_INIT(AvahiLLMNREntryVerify, e->proto.llmnr.verifiers);

        AVAHI_LLIST_PREPEND(AvahiEntry, entries, s->llmnr.entries, e);

        /* Insert into hash table indexed by name */
        t = avahi_hashmap_lookup(s->llmnr.entries_by_key, r->key);
        AVAHI_LLIST_PREPEND(AvahiEntry, by_key, t, e);
        avahi_hashmap_replace(s->llmnr.entries_by_key, e->record->key, t);

        /* Insert into group list */
        if (g)
            AVAHI_LLIST_PREPEND(AvahiEntry, by_group, g->entries, e);
        else
            /* Verify now if it doesn't belong to any group otherwise entry will be verified
            when group will be commited */
            avahi_verify_entry(s, e);
      }

    return e;
}
static AvahiEntry * server_add_internal(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    AvahiRecord *r) {

    AvahiEntry *e;

    assert(s);
    assert(r);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, s->state != AVAHI_SERVER_FAILURE && s->state != AVAHI_SERVER_INVALID, AVAHI_ERR_BAD_STATE);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, AVAHI_PROTO_VALID(protocol), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, AVAHI_FLAGS_VALID(
                                         flags,
                                         AVAHI_PUBLISH_NO_ANNOUNCE|
                                         AVAHI_PUBLISH_NO_PROBE|
                                         AVAHI_PUBLISH_NO_VERIFY|
                                         AVAHI_PUBLISH_UNIQUE|
                                         AVAHI_PUBLISH_ALLOW_MULTIPLE|
                                         AVAHI_PUBLISH_UPDATE|
                                         AVAHI_PUBLISH_USE_WIDE_AREA|
                                         AVAHI_PUBLISH_USE_MULTICAST|
                                         AVAHI_PUBLISH_USE_LLMNR), AVAHI_ERR_INVALID_FLAGS);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, avahi_is_valid_domain_name(r->key->name), AVAHI_ERR_INVALID_HOST_NAME);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, r->ttl != 0, AVAHI_ERR_INVALID_TTL);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !avahi_key_is_pattern(r->key), AVAHI_ERR_IS_PATTERN);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, avahi_record_is_valid(r), AVAHI_ERR_INVALID_RECORD);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, r->key->clazz == AVAHI_DNS_CLASS_IN, AVAHI_ERR_INVALID_DNS_CLASS);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                     (r->key->type != 0) &&
                                     (r->key->type != AVAHI_DNS_TYPE_ANY) &&
                                     (r->key->type != AVAHI_DNS_TYPE_OPT) &&
                                     (r->key->type != AVAHI_DNS_TYPE_TKEY) &&
                                     (r->key->type != AVAHI_DNS_TYPE_TSIG) &&
                                     (r->key->type != AVAHI_DNS_TYPE_IXFR) &&
                                     (r->key->type != AVAHI_DNS_TYPE_AXFR), AVAHI_ERR_INVALID_DNS_TYPE);

    transport_flags_from_domain(s, &flags, r->key->name, 1);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, (flags & AVAHI_PUBLISH_USE_MULTICAST) || (flags & AVAHI_PUBLISH_USE_LLMNR), AVAHI_ERR_NOT_SUPPORTED);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !s->config.disable_publishing, AVAHI_ERR_NOT_PERMITTED);

    if (flags & AVAHI_PUBLISH_USE_LLMNR)
        return server_add_llmnr_internal(s, g, interface, protocol, flags, r);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !(flags & AVAHI_PUBLISH_NO_VERIFY), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                     !g ||
                                     (g->state != AVAHI_ENTRY_GROUP_ESTABLISHED && g->state != AVAHI_ENTRY_GROUP_REGISTERING) ||
                                     (flags & AVAHI_PUBLISH_UPDATE), AVAHI_ERR_BAD_STATE);

    if (flags & AVAHI_PUBLISH_UPDATE) {
        AvahiRecord *old_record;
        int is_first = 1;

        AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !g || g->type == AVAHI_GROUP_MDNS, AVAHI_ERR_INVALID_GROUP);

       /* Update an existing record */

       /* Find the first matching entry */
        for (e = avahi_hashmap_lookup(s->mdns.entries_by_key, r->key); e; e = e->by_key_next) {
            assert(e->type == AVAHI_ENTRY_MDNS);
            if (!e->dead && e->group == g && e->interface == interface && e->protocol == protocol)
                break;

            is_first = 0;
        }

        /* Hmm, nothing found? */
        if (!e) {
            avahi_server_set_errno(s, AVAHI_ERR_NOT_FOUND);
            return NULL;
        }

        /* Update the entry */
        old_record = e->record;
        e->record = avahi_record_ref(r);
        e->flags = flags;

        /* Announce our changes when needed */
        if (!avahi_record_equal_no_ttl(old_record, r) && (!g || g->state != AVAHI_ENTRY_GROUP_UNCOMMITED)) {

            /* Remove the old entry from all caches, if needed */
            if (!(e->flags & AVAHI_PUBLISH_UNIQUE))
                avahi_goodbye_entry(s, e, 1, 0);

            /* Reannounce our updated entry */
            avahi_reannounce_entry(s, e);
        }

        /* If we were the first entry in the list, we need to update the key */
        if (is_first)
            avahi_hashmap_replace(s->mdns.entries_by_key, e->record->key, e);

        avahi_record_unref(old_record);

    } else {
        AvahiEntry *t;

        AVAHI_CHECK_VALIDITY_RETURN_NULL(s,
                                         !g ||
                                         g->type == AVAHI_GROUP_UNSET ||
                                         g->type == AVAHI_GROUP_MDNS, AVAHI_ERR_INVALID_GROUP);

        if (g && g->type == AVAHI_GROUP_UNSET) {
            g->proto.mdns.n_probing = 0;
            g->proto.mdns.n_register_try = 0;
            g->proto.mdns.register_time_event = NULL;
            g->proto.mdns.register_time.tv_sec = 0;
            g->proto.mdns.register_time.tv_usec = 0;
            g->type = AVAHI_GROUP_MDNS;

            AVAHI_LLIST_HEAD_INIT(AvahiEntry, g->entries);
            AVAHI_LLIST_PREPEND(AvahiSEntryGroup, groups, s->mdns.groups, g);
        }
        /* Add a new record */

        if (check_record_conflict(s, interface, protocol, r, flags, AVAHI_MDNS) < 0) {
            avahi_server_set_errno(s, AVAHI_ERR_COLLISION);
            return NULL;
        }

        if (!(e = avahi_new(AvahiEntry, 1))) {
            avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            return NULL;
        }

        e->server = s;
        e->record = avahi_record_ref(r);
        e->group = g;
        e->interface = interface;
        e->protocol = protocol;
        e->flags = flags;
        e->dead = 0;
        e->type = AVAHI_ENTRY_MDNS;

        AVAHI_LLIST_HEAD_INIT(AvahiAnnouncer, e->proto.mdns.announcers);

        AVAHI_LLIST_PREPEND(AvahiEntry, entries, s->mdns.entries, e);

        /* Insert into hash table indexed by name */
        t = avahi_hashmap_lookup(s->mdns.entries_by_key, e->record->key);
        AVAHI_LLIST_PREPEND(AvahiEntry, by_key, t, e);
        avahi_hashmap_replace(s->mdns.entries_by_key, e->record->key, t);

        /* Insert into group list */
        if (g)
            AVAHI_LLIST_PREPEND(AvahiEntry, by_group, g->entries, e);

        avahi_announce_entry(s, e);
    }

    return e;
}

int avahi_server_add(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    AvahiRecord *r) {

    if (!server_add_internal(s, g, interface, protocol, flags, r))
        return avahi_server_errno(s);

    return AVAHI_OK;
}

const AvahiRecord *avahi_server_iterate(AvahiServer *s, AvahiSEntryGroup *g, void **state, AvahiPublishProtocol proto) {
    AvahiEntry **e = (AvahiEntry**) state;
    assert(s);
    assert(e);

    if (!*e)
        *e = g ? g->entries : (proto == AVAHI_MDNS ? s->mdns.entries : s->llmnr.entries);

    while (*e && (*e)->dead)
        *e = g ? (*e)->by_group_next : (*e)->entries_next;

    if (!*e)
        return NULL;

    return avahi_record_ref((*e)->record);
}

static void zone_dump(AvahiServer *s, AvahiDumpCallback callback, void* userdata, AvahiPublishProtocol proto) {
    AvahiEntry *e;

    for (e = ((proto == AVAHI_MDNS) ? (s->mdns.entries) : (s->llmnr.entries)); e; e = e->entries_next) {
        char *t;
        char ln[256];

        if (e->dead)
            continue;

        if (!(t = avahi_record_to_string(e->record))) {
            avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            return;
        }

        snprintf(ln, sizeof(ln), "%s ; iface=%i proto=%i", t, e->interface, e->protocol);
        avahi_free(t);

        callback(ln, userdata);
    }
}

int avahi_server_dump(AvahiServer *s, AvahiDumpCallback callback, void* userdata) {
    assert(s);
    assert(callback);

    callback(";;; mDNS ZONE DUMP FOLLOWS ;;;", userdata);
    zone_dump(s, callback, userdata, AVAHI_MDNS);
    avahi_dump_caches(s->monitor, callback, userdata);

    callback(";;; LLMNR ZONE DUMP FOLLOWS ;;;", userdata);
    zone_dump(s, callback, userdata, AVAHI_LLMNR);
    avahi_llmnr_cache_dump(s->llmnr.llmnr_lookup_engine, callback, userdata);

    if (s->wide_area.wide_area_lookup_engine)
        avahi_wide_area_cache_dump(s->wide_area.wide_area_lookup_engine, callback, userdata);

    return AVAHI_OK;
}

static AvahiEntry *server_add_ptr_internal(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    uint32_t ttl,
    const char *name,
    const char *dest) {

    AvahiRecord *r;
    AvahiEntry *e;

    assert(s);
    assert(dest);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !name || avahi_is_valid_domain_name(name), AVAHI_ERR_INVALID_HOST_NAME);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, avahi_is_valid_domain_name(dest), AVAHI_ERR_INVALID_HOST_NAME);

    if (!name)
        name = s->host_name_fqdn;

    if (!(r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_PTR, ttl))) {
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    r->data.ptr.name = avahi_normalize_name_strdup(dest);
    e = server_add_internal(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);
    return e;
}

int avahi_server_add_ptr(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    uint32_t ttl,
    const char *name,
    const char *dest) {

    AvahiEntry *e;

    assert(s);

    if (!(e = server_add_ptr_internal(s, g, interface, protocol, flags, ttl, name, dest)))
        return avahi_server_errno(s);

    return AVAHI_OK;
}

int avahi_server_add_cname(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    uint32_t ttl,
    const char *name) {

    AvahiRecord *r;
    AvahiEntry *e;
    char *fq;

    assert(s);
    assert(s->domain_name);
    assert(name);

    AVAHI_CHECK_VALIDITY(s, !name || avahi_is_valid_domain_name(name), AVAHI_ERR_INVALID_HOST_NAME);

    if (!(fq = avahi_strdup_printf("%s.%s", name, s->domain_name)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

    if (!(r = avahi_record_new_full(fq, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, ttl)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

    avahi_free(fq);

    r->data.cname.name = avahi_normalize_name_strdup(s->host_name_fqdn);
    e= server_add_internal(s, g, interface, protocol, (flags & ~ AVAHI_PUBLISH_NO_REVERSE) | AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_ALLOW_MULTIPLE, r);
    avahi_record_unref(r);

    if (!e)
	return avahi_server_errno(s);

    return AVAHI_OK;
}

int avahi_server_add_llmnr_cname(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    uint32_t ttl,
    const char *name) {

    AvahiRecord *r;
    AvahiEntry *e;
    char *fq;

    assert(s);
    assert(s->domain_name);
    assert(name);

    AVAHI_CHECK_VALIDITY(s, !name || avahi_is_valid_domain_name(name), AVAHI_ERR_INVALID_HOST_NAME);

    if (!(fq = avahi_strdup_printf("%s", name)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

    if (!(r = avahi_record_new_full(fq, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, ttl)))
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

    avahi_free(fq);

    r->data.cname.name = avahi_normalize_name_strdup(s->host_name_fqdn);
    e= server_add_internal(s, g, interface, protocol, (flags & ~ AVAHI_PUBLISH_NO_REVERSE) | AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_ALLOW_MULTIPLE, r);
    avahi_record_unref(r);

    if (!e)
	return avahi_server_errno(s);

    return AVAHI_OK;
}


static int server_add_address_internal (
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    AvahiAddress *a,
    uint32_t ttl) {

    AvahiRecord *r;
    int ret = AVAHI_OK;
    AvahiEntry *reverse = NULL, *entry = NULL;

    assert(s);
    assert(a);
    assert(name);

    /* Create the A/AAAA record */
    if (a->proto == AVAHI_PROTO_INET) {

        if (!(r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, ttl))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto finish;
        }

        r->data.a.address = a->data.ipv4;

    } else {
        assert(a->proto == AVAHI_PROTO_INET6);

        if (!(r = avahi_record_new_full(name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA, ttl))) {
            ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
            goto finish;
        }

        r->data.aaaa.address = a->data.ipv6;
    }

    entry = server_add_internal(s, g, interface, protocol, (flags & ~ AVAHI_PUBLISH_NO_REVERSE) | AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_ALLOW_MULTIPLE, r);
    avahi_record_unref(r);

    if (!entry) {
        ret = avahi_server_errno(s);
        goto finish;
    }

    /* Create the reverse lookup entry */

    if (!(flags & AVAHI_PUBLISH_NO_REVERSE)) {
        char reverse_n[AVAHI_DOMAIN_NAME_MAX];
        avahi_reverse_lookup_name(a, reverse_n, sizeof(reverse_n));

        if (!(reverse = server_add_ptr_internal(s, g, interface, protocol, flags | AVAHI_PUBLISH_UNIQUE, ttl, reverse_n, name))) {
            ret = avahi_server_errno(s);
            goto finish;
        }
    }

finish:

    if (ret != AVAHI_OK && !(flags & AVAHI_PUBLISH_UPDATE)) {
        if (entry)
            avahi_entry_free(s, entry);
        if (reverse)
            avahi_entry_free(s, reverse);
    }

    return ret;
}


int avahi_server_add_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    AvahiAddress *a) {

    char n[AVAHI_DOMAIN_NAME_MAX];
    int ret_fqdn, null_name = 0;

    assert(s);
    assert(a);
    /*assert(name || (flags & (AVAHI_PUBLISH_USE_MULTICAST | AVAHI_PUBLISH_USE_LLMNR | AVAHI_PUBLISH_USE_WIDE_AREA));*/

    AVAHI_CHECK_VALIDITY(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY(s, AVAHI_PROTO_VALID(protocol) && AVAHI_PROTO_VALID(a->proto), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY(s, AVAHI_FLAGS_VALID(flags,
                                              AVAHI_PUBLISH_NO_REVERSE|
                                              AVAHI_PUBLISH_NO_ANNOUNCE|
                                              AVAHI_PUBLISH_NO_PROBE|
                                              AVAHI_PUBLISH_NO_VERIFY|
                                              AVAHI_PUBLISH_UPDATE|
                                              AVAHI_PUBLISH_USE_WIDE_AREA|
                                              AVAHI_PUBLISH_USE_MULTICAST|
                                              AVAHI_PUBLISH_USE_LLMNR), AVAHI_ERR_INVALID_FLAGS);

    AVAHI_CHECK_VALIDITY(s,
                         !name ||
                         ((flags & AVAHI_PUBLISH_USE_MULTICAST) ? (avahi_is_valid_fqdn(name)) : (avahi_is_valid_domain_name(name))),
                         AVAHI_ERR_INVALID_HOST_NAME);

    /* Prepare the host name */
    if (!name) {
        name = s->host_name_fqdn;
        null_name = 1;
    } else {
        AVAHI_ASSERT_TRUE(avahi_normalize_name(name, n, sizeof(n)));
        name = n;
    }

    /* transport flags */
    transport_flags_from_domain(s, &flags, name, 1);
    AVAHI_CHECK_VALIDITY(s, (flags & (AVAHI_PUBLISH_USE_MULTICAST | AVAHI_PUBLISH_USE_LLMNR)), AVAHI_ERR_NOT_SUPPORTED);

    /* No matter mDNS or LLMNR */
    if (flags & AVAHI_PUBLISH_USE_MULTICAST)
        ret_fqdn = server_add_address_internal(s, g, interface, protocol, flags, name, a, AVAHI_DEFAULT_TTL_HOST_NAME);
    else
        ret_fqdn = server_add_address_internal(s, g,
                                               interface,
                                               protocol,
                                               (null_name == 1) ? (flags |= AVAHI_PUBLISH_NO_REVERSE) : (flags),
                                               name, a,
                                               AVAHI_DEFAULT_LLMNR_TTL_HOST_NAME);

    /* If previous entries have been added successfully && name parameter was NULL && we are using LLMNR,
    publish hostname -> A/AAAA entry and PTR entry*/
    if (ret_fqdn == AVAHI_OK && null_name == 1 && flags & AVAHI_PUBLISH_USE_LLMNR )
        return server_add_address_internal(s, g, interface, protocol, flags & ~ AVAHI_PUBLISH_NO_REVERSE, s->host_name, a, AVAHI_DEFAULT_LLMNR_TTL_HOST_NAME);

    return ret_fqdn;
}

static AvahiEntry *server_add_txt_strlst_nocopy(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    uint32_t ttl,
    const char *name,
    AvahiStringList *strlst) {

    AvahiRecord *r;
    AvahiEntry *e;

    assert(s);

    if (!(r = avahi_record_new_full(name ? name : s->host_name_fqdn, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, ttl))) {
        avahi_string_list_free(strlst);
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    r->data.txt.string_list = strlst;
    e = server_add_internal(s, g, interface, protocol, flags, r);
    avahi_record_unref(r);

    return e;
}

static AvahiStringList *add_magic_cookie(
    AvahiServer *s,
    AvahiStringList *strlst) {

    assert(s);

    if (!s->config.add_service_cookie)
        return strlst;

    if (avahi_string_list_find(strlst, AVAHI_SERVICE_COOKIE))
        /* This string list already contains a magic cookie */
        return strlst;

    return avahi_string_list_add_printf(strlst, AVAHI_SERVICE_COOKIE"=%u", s->local_service_cookie);
}

static int server_add_service_strlst_nocopy(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *strlst) {

    char ptr_name[AVAHI_DOMAIN_NAME_MAX], svc_name[AVAHI_DOMAIN_NAME_MAX], enum_ptr[AVAHI_DOMAIN_NAME_MAX], *h = NULL;
    AvahiRecord *r = NULL;
    int ret = AVAHI_OK;
    AvahiEntry *srv_entry = NULL, *txt_entry = NULL, *ptr_entry = NULL, *enum_entry = NULL;

    assert(s);
    assert(type);
    assert(name);

    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_PROTO_VALID(protocol), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_FLAGS_VALID(flags,
                                                                AVAHI_PUBLISH_NO_COOKIE|
                                                                AVAHI_PUBLISH_UPDATE|
                                                                AVAHI_PUBLISH_USE_WIDE_AREA|
                                                                AVAHI_PUBLISH_USE_MULTICAST), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_name(name), AVAHI_ERR_INVALID_SERVICE_NAME);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_type_strict(type), AVAHI_ERR_INVALID_SERVICE_TYPE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, !domain || avahi_is_valid_domain_name(domain), AVAHI_ERR_INVALID_DOMAIN_NAME);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, !host || avahi_is_valid_fqdn(host), AVAHI_ERR_INVALID_HOST_NAME);

    if (!domain)
        domain = s->domain_name;

    if (!host)
        host = s->host_name_fqdn;

    transport_flags_from_domain(s, &flags, domain, 0);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, flags & AVAHI_PUBLISH_USE_MULTICAST, AVAHI_ERR_NOT_SUPPORTED);

    if (!(h = avahi_normalize_name_strdup(host))) {
        ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    if ((ret = avahi_service_name_join(svc_name, sizeof(svc_name), name, type, domain)) < 0 ||
        (ret = avahi_service_name_join(ptr_name, sizeof(ptr_name), NULL, type, domain)) < 0 ||
        (ret = avahi_service_name_join(enum_ptr, sizeof(enum_ptr), NULL, "_services._dns-sd._udp", domain)) < 0) {
        avahi_server_set_errno(s, ret);
        goto fail;
    }

    /* Add service enumeration PTR record */

    if (!(ptr_entry = server_add_ptr_internal(s, g, interface, protocol, 0, AVAHI_DEFAULT_TTL, ptr_name, svc_name))) {
        ret = avahi_server_errno(s);
        goto fail;
    }

    /* Add SRV record */

    if (!(r = avahi_record_new_full(svc_name, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV, AVAHI_DEFAULT_TTL_HOST_NAME))) {
        ret = avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = h;
    h = NULL;
    srv_entry = server_add_internal(s, g, interface, protocol, AVAHI_PUBLISH_UNIQUE, r);
    avahi_record_unref(r);

    if (!srv_entry) {
        ret = avahi_server_errno(s);
        goto fail;
    }

    /* Add TXT record */

    if (!(flags & AVAHI_PUBLISH_NO_COOKIE))
        strlst = add_magic_cookie(s, strlst);

    txt_entry = server_add_txt_strlst_nocopy(s, g, interface, protocol, AVAHI_PUBLISH_UNIQUE, AVAHI_DEFAULT_TTL, svc_name, strlst);
    strlst = NULL;

    if (!txt_entry) {
        ret = avahi_server_errno(s);
        goto fail;
    }

    /* Add service type enumeration record */

    if (!(enum_entry = server_add_ptr_internal(s, g, interface, protocol, 0, AVAHI_DEFAULT_TTL, enum_ptr, ptr_name))) {
        ret = avahi_server_errno(s);
        goto fail;
    }

fail:
   if (ret != AVAHI_OK && !(flags & AVAHI_PUBLISH_UPDATE)) {
        if (srv_entry)
            avahi_entry_free(s, srv_entry);
        if (txt_entry)
            avahi_entry_free(s, txt_entry);
        if (ptr_entry)
            avahi_entry_free(s, ptr_entry);
        if (enum_entry)
            avahi_entry_free(s, enum_entry);
    }

    avahi_string_list_free(strlst);
    avahi_free(h);

    return ret;
}

int avahi_server_add_service_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    AvahiStringList *strlst) {

    assert(s);
    assert(type);
    assert(name);

    return server_add_service_strlst_nocopy(s, g, interface, protocol, flags, name, type, domain, host, port, avahi_string_list_copy(strlst));
}

int avahi_server_add_service(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *host,
    uint16_t port,
    ... ){

    va_list va;
    int ret;

    va_start(va, port);
    ret = server_add_service_strlst_nocopy(s, g, interface, protocol, flags, name, type, domain, host, port, avahi_string_list_new_va(va));
    va_end(va);

    return ret;
}

static int server_update_service_txt_strlst_nocopy(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    AvahiStringList *strlst) {

    char svc_name[AVAHI_DOMAIN_NAME_MAX];
    int ret = AVAHI_OK;
    AvahiEntry *e;

    assert(s);
    assert(type);
    assert(name);
    assert (flags & AVAHI_PUBLISH_USE_MULTICAST || flags & AVAHI_PUBLISH_USE_WIDE_AREA);

    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_PROTO_VALID(protocol), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_FLAGS_VALID(flags,
                                                                AVAHI_PUBLISH_NO_COOKIE|
                                                                AVAHI_PUBLISH_USE_WIDE_AREA|
                                                                AVAHI_PUBLISH_USE_MULTICAST), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_name(name), AVAHI_ERR_INVALID_SERVICE_NAME);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_type_strict(type), AVAHI_ERR_INVALID_SERVICE_TYPE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, !domain || avahi_is_valid_domain_name(domain), AVAHI_ERR_INVALID_DOMAIN_NAME);

    if (!domain)
        domain = s->domain_name;

    transport_flags_from_domain(s, &flags, domain, 0);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, flags & AVAHI_PUBLISH_USE_MULTICAST, AVAHI_ERR_NOT_SUPPORTED);

    if ((ret = avahi_service_name_join(svc_name, sizeof(svc_name), name, type, domain)) < 0) {
        avahi_server_set_errno(s, ret);
        goto fail;
    }

    /* Add TXT record */
    if (!(flags & AVAHI_PUBLISH_NO_COOKIE))
        strlst = add_magic_cookie(s, strlst);

    e = server_add_txt_strlst_nocopy(s, g, interface, protocol, AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_UPDATE, AVAHI_DEFAULT_TTL, svc_name, strlst);
    strlst = NULL;

    if (!e)
        ret = avahi_server_errno(s);

fail:

    avahi_string_list_free(strlst);

    return ret;
}

int avahi_server_update_service_txt_strlst(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    AvahiStringList *strlst) {

    return server_update_service_txt_strlst_nocopy(s, g, interface, protocol, flags, name, type, domain, avahi_string_list_copy(strlst));
}

/** Update the TXT record for a service with the NULL termonate list of strings */
int avahi_server_update_service_txt(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    ...) {

    va_list va;
    int ret;

    va_start(va, domain);
    ret = server_update_service_txt_strlst_nocopy(s, g, interface, protocol, flags, name, type, domain, avahi_string_list_new_va(va));
    va_end(va);

    return ret;
}

int avahi_server_add_service_subtype(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *name,
    const char *type,
    const char *domain,
    const char *subtype) {

    int ret = AVAHI_OK;
    char svc_name[AVAHI_DOMAIN_NAME_MAX], ptr_name[AVAHI_DOMAIN_NAME_MAX];

    assert(name);
    assert(type);
    assert(subtype);
    assert (flags & AVAHI_PUBLISH_USE_MULTICAST || flags & AVAHI_PUBLISH_USE_WIDE_AREA);

    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_PROTO_VALID(protocol), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, AVAHI_FLAGS_VALID(flags, AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_USE_WIDE_AREA), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_name(name), AVAHI_ERR_INVALID_SERVICE_NAME);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_type_strict(type), AVAHI_ERR_INVALID_SERVICE_TYPE);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, !domain || avahi_is_valid_domain_name(domain), AVAHI_ERR_INVALID_DOMAIN_NAME);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, avahi_is_valid_service_subtype(subtype), AVAHI_ERR_INVALID_SERVICE_SUBTYPE);

    if (!domain)
        domain = s->domain_name;

    transport_flags_from_domain(s, &flags, domain, 0);
    AVAHI_CHECK_VALIDITY_SET_RET_GOTO_FAIL(s, flags & AVAHI_PUBLISH_USE_MULTICAST, AVAHI_ERR_NOT_SUPPORTED);

    if ((ret = avahi_service_name_join(svc_name, sizeof(svc_name), name, type, domain)) < 0 ||
        (ret = avahi_service_name_join(ptr_name, sizeof(ptr_name), NULL, subtype, domain)) < 0) {
        avahi_server_set_errno(s, ret);
        goto fail;
    }

    if ((ret = avahi_server_add_ptr(s, g, interface, protocol, 0, AVAHI_DEFAULT_TTL, ptr_name, svc_name)) < 0)
        goto fail;

fail:

    return ret;
}

static void hexstring(char *s, size_t sl, const void *p, size_t pl) {
    static const char hex[] = "0123456789abcdef";
    int b = 0;
    const uint8_t *k = p;

    while (sl > 1 && pl > 0) {
        *(s++) = hex[(b ? *k : *k >> 4) & 0xF];

        if (b) {
            k++;
            pl--;
        }

        b = !b;

        sl--;
    }

    if (sl > 0)
        *s = 0;
}

static AvahiEntry *server_add_dns_server_name(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *domain,
    AvahiDNSServerType type,
    const char *name,
    uint16_t port /** should be 53 */) {

    AvahiEntry *e;
    char t[AVAHI_DOMAIN_NAME_MAX], normalized_d[AVAHI_DOMAIN_NAME_MAX], *n;

    AvahiRecord *r;

    assert(s);
    assert(name);
    assert (flags & AVAHI_PUBLISH_USE_MULTICAST || flags & AVAHI_PUBLISH_USE_WIDE_AREA);

    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, AVAHI_FLAGS_VALID(flags, AVAHI_PUBLISH_USE_WIDE_AREA|AVAHI_PUBLISH_USE_MULTICAST), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, type == AVAHI_DNS_SERVER_UPDATE || type == AVAHI_DNS_SERVER_RESOLVE, AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, port != 0, AVAHI_ERR_INVALID_PORT);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, avahi_is_valid_fqdn(name), AVAHI_ERR_INVALID_HOST_NAME);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, !domain || avahi_is_valid_domain_name(domain), AVAHI_ERR_INVALID_DOMAIN_NAME);

    if (!domain)
        domain = s->domain_name;

    transport_flags_from_domain(s, &flags, domain, 0);
    AVAHI_CHECK_VALIDITY_RETURN_NULL(s, flags & AVAHI_PUBLISH_USE_MULTICAST, AVAHI_ERR_NOT_SUPPORTED);

    if (!(n = avahi_normalize_name_strdup(name))) {
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    AVAHI_ASSERT_TRUE(avahi_normalize_name(domain, normalized_d, sizeof(normalized_d)));

    snprintf(t, sizeof(t), "%s.%s", type == AVAHI_DNS_SERVER_RESOLVE ? "_domain._udp" : "_dns-update._udp", normalized_d);

    if (!(r = avahi_record_new_full(t, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_SRV, AVAHI_DEFAULT_TTL_HOST_NAME))) {
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        avahi_free(n);
        return NULL;
    }

    r->data.srv.priority = 0;
    r->data.srv.weight = 0;
    r->data.srv.port = port;
    r->data.srv.name = n;
    e = server_add_internal(s, g, interface, protocol, 0, r);
    avahi_record_unref(r);

    return e;
}

int avahi_server_add_dns_server_address(
    AvahiServer *s,
    AvahiSEntryGroup *g,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiPublishFlags flags,
    const char *domain,
    AvahiDNSServerType type,
    const AvahiAddress *address,
    uint16_t port /** should be 53 */) {

    AvahiRecord *r;
    char n[64], h[64];
    AvahiEntry *a_entry, *s_entry;

    assert(s);
    assert(address);
    assert (flags & AVAHI_PUBLISH_USE_MULTICAST || flags & AVAHI_PUBLISH_USE_WIDE_AREA);

    AVAHI_CHECK_VALIDITY(s, AVAHI_IF_VALID(interface), AVAHI_ERR_INVALID_INTERFACE);
    AVAHI_CHECK_VALIDITY(s, AVAHI_PROTO_VALID(protocol) && AVAHI_PROTO_VALID(address->proto), AVAHI_ERR_INVALID_PROTOCOL);
    AVAHI_CHECK_VALIDITY(s, AVAHI_FLAGS_VALID(flags, AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_USE_WIDE_AREA), AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY(s, type == AVAHI_DNS_SERVER_UPDATE || type == AVAHI_DNS_SERVER_RESOLVE, AVAHI_ERR_INVALID_FLAGS);
    AVAHI_CHECK_VALIDITY(s, port != 0, AVAHI_ERR_INVALID_PORT);
    AVAHI_CHECK_VALIDITY(s, !domain || avahi_is_valid_domain_name(domain), AVAHI_ERR_INVALID_DOMAIN_NAME);

    if (!domain)
        domain = s->domain_name;

    transport_flags_from_domain(s, &flags, domain, 0);
    AVAHI_CHECK_VALIDITY(s, flags & AVAHI_PUBLISH_USE_MULTICAST, AVAHI_ERR_NOT_SUPPORTED);

    if (address->proto == AVAHI_PROTO_INET) {
        hexstring(h, sizeof(h), &address->data, sizeof(AvahiIPv4Address));
        snprintf(n, sizeof(n), "ip-%s.%s", h, domain);
        r = avahi_record_new_full(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_A, AVAHI_DEFAULT_TTL_HOST_NAME);
        r->data.a.address = address->data.ipv4;
    } else {
        hexstring(h, sizeof(h), &address->data, sizeof(AvahiIPv6Address));
        snprintf(n, sizeof(n), "ip6-%s.%s", h, domain);
        r = avahi_record_new_full(n, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA, AVAHI_DEFAULT_TTL_HOST_NAME);
        r->data.aaaa.address = address->data.ipv6;
    }

    if (!r)
        return avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);

    a_entry = server_add_internal(s, g, interface, protocol, AVAHI_PUBLISH_UNIQUE | AVAHI_PUBLISH_ALLOW_MULTIPLE, r);
    avahi_record_unref(r);

    if (!a_entry)
        return avahi_server_errno(s);

    if (!(s_entry = server_add_dns_server_name(s, g, interface, protocol, flags, domain, type, n, port))) {
        if (!(flags & AVAHI_PUBLISH_UPDATE))
            avahi_entry_free(s, a_entry);
        return avahi_server_errno(s);
    }

    return AVAHI_OK;
}

void avahi_s_entry_group_change_state(AvahiSEntryGroup *g, AvahiEntryGroupState state) {
    assert(g);
    assert(g->type != AVAHI_GROUP_UNSET);

    if (g->state == state)
        return;

    assert(state <= AVAHI_ENTRY_GROUP_COLLISION);

    if (g->state == AVAHI_ENTRY_GROUP_ESTABLISHED && g->type == AVAHI_GROUP_MDNS) {

        /* If the entry group was established for a time longer then
         * 5s, reset the establishment trial counter */

        if (avahi_age(&g->proto.mdns.established_at) > 5000000)
            g->proto.mdns.n_register_try = 0;
    } else if (g->state == AVAHI_ENTRY_GROUP_REGISTERING && g->type == AVAHI_GROUP_MDNS) {
        if (g->proto.mdns.register_time_event) {
            avahi_time_event_free(g->proto.mdns.register_time_event);
            g->proto.mdns.register_time_event = NULL;
        }
    }

    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED && g->type == AVAHI_GROUP_MDNS)

        /* If the entry group is now established, remember the time
         * this happened */

        gettimeofday(&g->proto.mdns.established_at, NULL);

    g->state = state;

    if (g->callback)
        g->callback(g->server, g, state, g->userdata);
}

AvahiSEntryGroup *avahi_s_entry_group_new(AvahiServer *s, AvahiSEntryGroupCallback callback, void* userdata) {
    AvahiSEntryGroup *g;

    assert(s);

    if (!(g = avahi_new(AvahiSEntryGroup, 1))) {
        avahi_server_set_errno(s, AVAHI_ERR_NO_MEMORY);
        return NULL;
    }

    g->server = s;
    g->callback = callback;
    g->userdata = userdata;
    g->dead = 0;
    g->state = AVAHI_ENTRY_GROUP_UNCOMMITED;
    g->type = AVAHI_GROUP_UNSET;

    return g;
}

static void cleanup_time_event_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void* userdata) {
    AvahiServer *s = userdata;

    assert(s);

    avahi_cleanup_dead_entries(s);
}

static void schedule_cleanup(AvahiServer *s) {
    struct timeval tv;

    assert(s);

    if (!s->cleanup_time_event)
        s->cleanup_time_event = avahi_time_event_new(s->time_event_queue, avahi_elapse_time(&tv, 1000, 0), &cleanup_time_event_callback, s);
}

void avahi_s_entry_group_free(AvahiSEntryGroup *g) {
    AvahiEntry *e;

    assert(g);
    assert(g->server);

    if (g->type == AVAHI_GROUP_UNSET) {
        avahi_free(g);

    } else if (g->type == AVAHI_GROUP_MDNS) {
        for (e = g->entries; e; e = e->by_group_next) {
            assert(e->type == AVAHI_ENTRY_MDNS);

            if (!e->dead) {
                avahi_goodbye_entry(g->server, e, 1, 1);
                e->dead = 1;
            }
        }

        if (g->proto.mdns.register_time_event) {
            avahi_time_event_free(g->proto.mdns.register_time_event);
            g->proto.mdns.register_time_event = NULL;
        }

        g->server->mdns.need_group_cleanup = 1;
        g->server->mdns.need_entry_cleanup = 1;

    } else {
        assert (g->type == AVAHI_GROUP_LLMNR);
        for (e = g->entries; e; e = e->by_group_next) {
            assert(e->type == AVAHI_ENTRY_LLMNR);

            if (!e->dead) {
                avahi_remove_verifiers(g->server, e);
                e->dead = 1;
            }
        }

        g->server->llmnr.need_entry_cleanup = 1;
        g->server->llmnr.need_group_cleanup = 1;
    }

    g->dead = 1;

    schedule_cleanup(g->server);
}

static void entry_group_commit_real(AvahiSEntryGroup *g) {
    assert(g);
    assert(g->type == AVAHI_GROUP_MDNS);

    gettimeofday(&g->proto.mdns.register_time, NULL);

    avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);

    if (g->dead)
        return;

    avahi_announce_group(g->server, g);
    avahi_s_entry_group_check_probed(g, 0);
}

static void entry_group_register_time_event_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void* userdata) {
    AvahiSEntryGroup *g = userdata;
    assert(g);
    assert(g->type == AVAHI_GROUP_MDNS);

    avahi_time_event_free(g->proto.mdns.register_time_event);
    g->proto.mdns.register_time_event = NULL;

    /* Holdoff time passed, so let's start probing */
    entry_group_commit_real(g);
}

int avahi_s_entry_group_commit(AvahiSEntryGroup *g) {
    struct timeval now;

    assert(g);
    assert(!g->dead);

    if (g->type == AVAHI_GROUP_UNSET)
        /* Group is empty */
        return avahi_server_set_errno(g->server, AVAHI_ERR_IS_EMPTY);

    if (g->type == AVAHI_GROUP_MDNS) {
        if (g->state != AVAHI_ENTRY_GROUP_UNCOMMITED && g->state != AVAHI_ENTRY_GROUP_COLLISION)
            return avahi_server_set_errno(g->server, AVAHI_ERR_BAD_STATE);

/*        if (avahi_s_entry_group_is_empty(g))
            return avahi_server_set_errno(g->server, AVAHI_ERR_IS_EMPTY);*/

        g->proto.mdns.n_register_try++;

        avahi_timeval_add(&g->proto.mdns.register_time,
                          1000*(g->proto.mdns.n_register_try >= AVAHI_RR_RATE_LIMIT_COUNT ?
                                AVAHI_RR_HOLDOFF_MSEC_RATE_LIMIT :
                                AVAHI_RR_HOLDOFF_MSEC));

        gettimeofday(&now, NULL);

        if (avahi_timeval_compare(&g->proto.mdns.register_time, &now) <= 0) {

            /* Holdoff time passed, so let's start probing */
            entry_group_commit_real(g);
        } else {

             /* Holdoff time has not yet passed, so let's wait */
            assert(!g->proto.mdns.register_time_event);
            g->proto.mdns.register_time_event = avahi_time_event_new(g->server->time_event_queue, &g->proto.mdns.register_time, entry_group_register_time_event_callback, g);

            avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_REGISTERING);
        }

    } else {
        assert(g->type == AVAHI_GROUP_LLMNR);

        if (g->state != AVAHI_ENTRY_GROUP_LLMNR_UNCOMMITED && g->state != AVAHI_ENTRY_GROUP_LLMNR_COLLISION)
            return avahi_server_set_errno(g->server, AVAHI_ERR_BAD_STATE);

/*        if (avahi_s_entry_group_is_empty(g))
            return avahi_server_set_errno(g->server, AVAHI_ERR_IS_EMPTY);*/

        avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_LLMNR_VERIFYING);
        avahi_verify_group(g->server, g);
    }

    return AVAHI_OK;
}

void avahi_s_entry_group_reset(AvahiSEntryGroup *g) {
    AvahiEntry *e;
    assert(g);

    if (g->type == AVAHI_GROUP_UNSET)
        return;

    if (g->type == AVAHI_GROUP_MDNS) {

        for (e = g->entries; e; e = e->by_group_next) {
            assert(e->type == AVAHI_ENTRY_MDNS);
            if (!e->dead) {
                avahi_goodbye_entry(g->server, e, 1, 1);
                e->dead = 1;
            }
        }
        g->server->mdns.need_entry_cleanup = 1;

        g->proto.mdns.n_probing = 0;

        avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_UNCOMMITED);

    } else {
        assert(g->type == AVAHI_GROUP_LLMNR);

        for (e = g->entries; e; e = e->by_group_next) {
            assert(e->type == AVAHI_ENTRY_LLMNR);
            if (!e->dead) {
                avahi_remove_verifiers(g->server, e);
                e->dead = 1;
            }
        }
        g->server->llmnr.need_entry_cleanup = 1;

        g->proto.llmnr.n_verifying = 0;

        avahi_s_entry_group_change_state(g, AVAHI_ENTRY_GROUP_LLMNR_UNCOMMITED);
    }

    schedule_cleanup(g->server);
}

int avahi_entry_is_commited(AvahiEntry *e) {
    assert(e);
    assert(!e->dead);

    if (e->type == AVAHI_ENTRY_MDNS)
        return !e->group ||
            e->group->state == AVAHI_ENTRY_GROUP_REGISTERING ||
            e->group->state == AVAHI_ENTRY_GROUP_ESTABLISHED;

    else
        return (!e->group ||
            e->group->state == AVAHI_ENTRY_GROUP_LLMNR_VERIFYING ||
            e->group->state == AVAHI_ENTRY_GROUP_LLMNR_ESTABLISHED );
}

AvahiEntryGroupState avahi_s_entry_group_get_state(AvahiSEntryGroup *g) {
    assert(g);
    assert(!g->dead);

    return g->state;
}

void avahi_s_entry_group_set_data(AvahiSEntryGroup *g, void* userdata) {
    assert(g);

    g->userdata = userdata;
}

void* avahi_s_entry_group_get_data(AvahiSEntryGroup *g) {
    assert(g);

    return g->userdata;
}

int avahi_s_entry_group_is_empty(AvahiSEntryGroup *g) {
    AvahiEntry *e;
    assert(g);

    if (g->type == AVAHI_GROUP_UNSET)
        /* There has not been any entry added in this group so far. */
        return 1;

    /* Look for an entry that is not dead */
    for (e = g->entries; e; e = e->by_group_next)
        if (!e->dead)
            return 0;

    return 1;
}
