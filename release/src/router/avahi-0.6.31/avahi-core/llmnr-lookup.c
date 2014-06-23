#include <stdlib.h>

#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>
#include <avahi-common/defs.h>
#include <avahi-common/error.h>

#include "internal.h"
#include "browse.h"
#include "socket.h"
#include "log.h"
#include "hashmap.h"
#include "rr-util.h"

#include "llmnr-lookup.h"

/* Lookups Function */

/* AvahiLLMNRQuery Callback */
static void query_callback (AvahiIfIndex idx, AvahiProtocol protocol, AvahiRecord *r, void *userdata);

static void elapse_timeout_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void *userdata);

/* New lookup */
AvahiLLMNRLookup* avahi_llmnr_lookup_new(
    AvahiLLMNRLookupEngine *e,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiKey *key,
    AvahiLLMNRLookupCallback callback,
    void *userdata) {

    struct timeval tv;
    AvahiLLMNRLookup *l, *t;

    assert(e);
    assert(AVAHI_IF_VALID(interface));
    assert(AVAHI_PROTO_VALID(protocol));
    assert(key);
    assert(callback);

    if (!(l = avahi_new(AvahiLLMNRLookup, 1)))
        return NULL;

    /* Initialize parameters */
    l->engine = e;
    l->dead = 0;
    l->key = avahi_key_ref(key);
    /* TODO*/
    l->cname_key =  avahi_key_new_cname(l->key);
    l->interface = interface;
    l->protocol = protocol;
    l->callback = callback;
    l->userdata = userdata;
    l->queries_issued = 0;
    l->time_event = NULL;

    /* Prepend in list */
    AVAHI_LLIST_PREPEND(AvahiLLMNRLookup, lookups, e->lookups, l);

    /* In hashmap */
    t = avahi_hashmap_lookup(e->lookups_by_key, key);
    AVAHI_LLIST_PREPEND(AvahiLLMNRLookup, by_key, t, l);
    avahi_hashmap_replace(e->lookups_by_key, avahi_key_ref(key), t);

    /* Issue LLMNR Queries on all interfaces that match the id and protocol*/
    avahi_llmnr_query_add_for_all(e->s, interface, protocol, key, query_callback, l);
    l->queries_issued = 1;

    /* All queries are issued right now. To the max we get response from each interface maximum
    after 3 seconds (3 queries). still we keep it 4 seconds .AVAHI_BROWSER_ALL_FOR_NOW event */
    gettimeofday(&tv, NULL);
    avahi_timeval_add(&tv, 4000000);

    l->time_event = avahi_time_event_new(e->s->time_event_queue, &tv, elapse_timeout_callback, l);

    return l;
}

/* Stop lookup */
static void lookup_stop(AvahiLLMNRLookup *l) {
    assert(l);

    l->callback = NULL;

    if (l->queries_issued) {
        avahi_llmnr_query_remove_for_all(l->engine->s, l->interface, l->protocol, l->key);
        l->queries_issued = 0;
    }

    if (l->time_event) {
        avahi_time_event_free(l->time_event);
        l->time_event = NULL;
    }
}

/* Destroy Lookup */
static void lookup_destroy(AvahiLLMNRLookup *l) {
    AvahiLLMNRLookup *t;
    assert(l);

    lookup_stop(l);

    t = avahi_hashmap_lookup(l->engine->lookups_by_key, l->key);
    AVAHI_LLIST_REMOVE(AvahiLLMNRLookup, by_key, t, l);

    if (t)
        avahi_hashmap_replace(l->engine->lookups_by_key, avahi_key_ref(l->key), t);
    else
        avahi_hashmap_remove(l->engine->lookups_by_key, l->key);

    AVAHI_LLIST_REMOVE(AvahiLLMNRLookup, lookups, l->engine->lookups, l);

    if (l->key)
        avahi_key_unref(l->key);


    /* What this CNAME key is for? :( */
    if (l->cname_key)
        avahi_key_unref(l->cname_key);

    avahi_free(l);
}

/* Free lookup (and stop) */
void avahi_llmnr_lookup_free(AvahiLLMNRLookup *l) {
    assert(l);

    if (l->dead)
        return;

    l->dead = 1;
    l->engine->cleanup_dead = 1;
    lookup_stop(l);
}

static void elapse_timeout_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void *userdata) {
    AvahiLLMNRLookup *l = userdata;

    l->callback(l->engine, l->interface, l->protocol, AVAHI_BROWSER_ALL_FOR_NOW, AVAHI_LOOKUP_RESULT_LLMNR, NULL, l->userdata);

    if (l->time_event) {
        avahi_time_event_free(l->time_event);
        l->time_event = NULL;
    }

    lookup_stop(l);

    return;
}

/* Cache Functions */

/* Run callbacks for all lookups belong to e and were issued for r->key, idx and proto */
static void run_callbacks(AvahiLLMNRLookupEngine *e, AvahiIfIndex idx, AvahiProtocol proto, AvahiRecord *r, AvahiBrowserEvent event) {
    AvahiLLMNRLookup *l;

    assert(e);
    assert(r);


    for (l = avahi_hashmap_lookup(e->lookups_by_key, r->key); l; l = l->by_key_next)
        if ( !(l->dead) &&
            (l->callback) &&
            /* lookups can have UNSPEC iface or protocol */
            (l->interface < 0 || (l->interface == idx)) &&
            (l->protocol < 0 || (l->protocol == proto)) )

            /* Call callback funtion */
            l->callback(e, idx, proto, event, AVAHI_LOOKUP_RESULT_LLMNR, r, l->userdata);


    if (r->key->clazz == AVAHI_DNS_CLASS_IN && r->key->type == AVAHI_DNS_TYPE_CNAME) {

        for (l = e->lookups; l; l = l->lookups_next) {
            AvahiKey *key;

            if (l->dead || !l->callback)
                continue;

            if ((key = avahi_key_new_cname(l->key))) {

                if (avahi_key_equal(r->key, key) &&
                    (l->interface < 0 || l->interface == idx) &&
                    (l->protocol < 0 || l->protocol == proto) )
                    /* Call callback */
                    l->callback(e, idx, proto, AVAHI_BROWSER_NEW, AVAHI_LOOKUP_RESULT_LLMNR, r, l->userdata);
                avahi_key_unref(key);
            }
        }
    }
}

/* find cache entry (idx, protocol, r)*/
static AvahiLLMNRCacheEntry *find_cache_entry(AvahiLLMNRLookupEngine *e, AvahiIfIndex idx, AvahiProtocol protocol, AvahiRecord *r) {
    AvahiLLMNRCacheEntry *c;

    assert(e);
    assert(r);

    assert(idx != AVAHI_IF_UNSPEC);
    assert(protocol != AVAHI_PROTO_UNSPEC);

    for (c = avahi_hashmap_lookup(e->cache_by_key, r->key); c; c = c->cache_next) {

        if ( (c->interface == idx) &&
            (c->protocol == protocol) &&
            avahi_record_equal_no_ttl(c->record, r) )

            return c;
    }

    return NULL;
}

/* remove/destroy cache entry */
static void destroy_cache_entry(AvahiLLMNRCacheEntry *c) {

    AvahiLLMNRCacheEntry *t;
    assert(c);

    if (c->time_event)
        avahi_time_event_free(c->time_event);

    AVAHI_LLIST_REMOVE(AvahiLLMNRCacheEntry, cache, c->engine->cache, c);

    t = avahi_hashmap_lookup(c->engine->cache_by_key, c->record->key);
    AVAHI_LLIST_REMOVE(AvahiLLMNRCacheEntry, by_key, t, c);
    if (t)
        avahi_hashmap_replace(c->engine->cache_by_key, avahi_key_ref(c->record->key), t);
    else
        avahi_hashmap_remove(c->engine->cache_by_key, c->record->key);

    /* Run callbacks that entry has been deleted */
    /* run_callbacks(c->engine, c->interface, c->protocol, c->record, AVAHI_BROWSER_REMOVE); */

    assert(c->engine->n_cache_entries > 0);
    c->engine->n_cache_entries--;

    avahi_record_unref(c->record);

    avahi_free(c);
}

/* callback funtion when cache entry timeout reaches */
static void cache_entry_timeout(AvahiTimeEvent *e, void *userdata) {
    AvahiLLMNRCacheEntry *c = userdata;

    assert(e);
    assert(c);

    destroy_cache_entry(c);
}

/* Update cache */
static void update_cache(AvahiLLMNRLookupEngine *e, AvahiIfIndex idx, AvahiProtocol protocol, AvahiRecord *r) {
    AvahiLLMNRCacheEntry *c;
    int new = 1;

    assert(e);
    assert(r);

    if ((c = find_cache_entry(e, idx, protocol, r))) {
/*        new = 0;*/

        /* Update the existine entry. Remove c->record
        point c->record to r*/
        avahi_record_unref(c->record);
    } else {
        AvahiLLMNRCacheEntry *t;

/*        new = 1;*/

        /* Check for the cache limit */
        if (e->n_cache_entries >= LLMNR_CACHE_ENTRIES_MAX)
            goto finish;

        c = avahi_new(AvahiLLMNRCacheEntry, 1);
        c->engine = e;
        c->interface = idx;
        c->protocol = protocol;
        c->time_event = NULL;

        AVAHI_LLIST_PREPEND(AvahiLLMNRCacheEntry, cache, e->cache, c);

        t = avahi_hashmap_lookup(e->cache_by_key, r->key);
        AVAHI_LLIST_PREPEND(AvahiLLMNRCacheEntry, by_key, t, c);
        avahi_hashmap_replace(e->cache_by_key, avahi_key_ref(r->key), t);

        e->n_cache_entries++;
    }

    /* Now update record */
    c->record = avahi_record_ref(r);

    /* Scedule the expiry time of this CacheEntry */
    gettimeofday(&c->timestamp, NULL);
    c->expiry = c->timestamp;
    avahi_timeval_add(&c->expiry, r->ttl * 1000000);

    if (c->time_event)
        avahi_time_event_update(c->time_event, &c->expiry);
    else
        c->time_event = avahi_time_event_new(e->s->time_event_queue, &c->expiry, cache_entry_timeout, c);

    finish:
    if (new)
        /* Whether the record is new or old we run callbacks for every update */
        run_callbacks(e, idx, protocol, r, AVAHI_BROWSER_NEW);
}

/* Define callback function */
static void query_callback(
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiRecord *r,
    void *userdata) {

    AvahiLLMNRLookup *lookup = userdata;
    assert(AVAHI_IF_VALID(idx) && idx != -1);
    assert(AVAHI_PROTO_VALID(protocol) && (protocol != AVAHI_PROTO_UNSPEC));

    if (r)
        /* Update cache */
        update_cache(lookup->engine, idx, protocol, r);
    /*else
        This query was issued by 'lookup'. So call lookup->callback specifying that
        on specified interface and protocol we have no records available
        lookup->callback(lookup->engine, idx, protocol, AVAHI_BROWSER_FAILURE, AVAHI_LOOKUP_RESULT_LLMNR, NULL, lookup->userdata); */

    /* We can't stop the lookup right away because we may have some more responses coming
    up from more interfaces */
    return;
}

/* Scan LLMNR cache */
unsigned avahi_scan_llmnr_cache(
    AvahiLLMNRLookupEngine *e,
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiKey *key,
    AvahiLLMNRLookupCallback callback,
    void *userdata) {

    AvahiLLMNRCacheEntry *c;
    AvahiKey *cname_key;
    unsigned n = 0;

    assert(e);
    assert(key);
    assert(callback);

    assert(AVAHI_IF_VALID(idx));
    assert(AVAHI_PROTO_VALID(protocol));

    for (c = avahi_hashmap_lookup(e->cache_by_key, key); c; c = c->by_key_next)
        if (((idx < 0) || (c->interface == idx)) &&
            ((protocol < 0) || (c->protocol == protocol)) ) {
                /* Call Callback Function */
                callback(e,
                         c->interface,
                         c->protocol,
                         AVAHI_BROWSER_NEW,
                         AVAHI_LOOKUP_RESULT_CACHED | AVAHI_LOOKUP_RESULT_LLMNR,
                         c->record,
                         userdata);
                /* Increase 'n'*/
                n++;
        }

    if ((cname_key = avahi_key_new_cname(key))) {

        for (c = avahi_hashmap_lookup(e->cache_by_key, cname_key); c; c = c->by_key_next)
            if ( ( (idx < 0) || (c->interface == idx) ) &&
             ( (protocol < 0) || (c->protocol == protocol) )) {
                callback(e, c->interface, c->protocol, AVAHI_BROWSER_NEW, AVAHI_LOOKUP_RESULT_CACHED | AVAHI_LOOKUP_RESULT_LLMNR, c->record, userdata);
                n++;
        }

        avahi_key_unref(cname_key);
    }

    return n;
}

/* Clear all cache entries belong to this interface */
void avahi_llmnr_clear_cache(AvahiLLMNRLookupEngine *e, AvahiInterface *i) {
    AvahiLLMNRCacheEntry *c;

    assert(e);
    assert(i);

    for (c = e->cache; c; c = c->cache_next)
        if (avahi_interface_match(i, c->interface, c->protocol))
            /*Destroy this cache entry  */
            destroy_cache_entry(c);
}

/* Cache dump */
void avahi_llmnr_cache_dump(AvahiLLMNRLookupEngine *e, AvahiDumpCallback callback, void *userdata) {
    AvahiLLMNRCacheEntry *c;

    assert(e);
    assert(callback);

    callback(";;; LLMNR CACHE ;;;", userdata);

    for (c = e->cache; c; c = c->cache_next) {
        if (avahi_interface_is_relevant(avahi_interface_monitor_get_interface(c->engine->s->monitor, c->interface, c->protocol))) {
            char *t = avahi_record_to_string(c->record);
            callback(t, userdata);
            avahi_free(t);
        }
    }
}

/* LLMNRLookupEngine functions */
void avahi_llmnr_lookup_engine_cleanup(AvahiLLMNRLookupEngine *e) {
    AvahiLLMNRLookup *l, *n;
    assert(e);

    while (e->cleanup_dead) {
        e->cleanup_dead = 0;

        for (l = e->lookups; l; l = n) {
            n = l->lookups_next;

            if (l->dead)
                lookup_destroy(l);
        }
    }
}

/* The interface is new so issue query for all lookups those are not dead */
void avahi_llmnr_lookup_engine_new_interface(AvahiLLMNRLookupEngine *e, AvahiInterface *i) {
    AvahiLLMNRLookup *l;

    assert(e);
    assert(i);
    for (l = e->lookups; l; l = l->lookups_next) {

        if (l->dead || !l->callback)
            continue;

        if (l->queries_issued && avahi_interface_match(i, l->interface, l->protocol))
            /* Issue LLMNR query */
            avahi_llmnr_query_add(i, l->key, AVAHI_LLMNR_SIMPLE_QUERY, query_callback, l);
    }
}

/* New lookup engine */
AvahiLLMNRLookupEngine* avahi_llmnr_lookup_engine_new(AvahiServer *s) {
    AvahiLLMNRLookupEngine *e;

    assert(s);

    e = avahi_new(AvahiLLMNRLookupEngine, 1);
    e->s = s;
    e->next_id = 0;
    e->cleanup_dead = 0;
    e->n_cache_entries = 0;

    /* Queries and lookups */
    e->lookups_by_key = avahi_hashmap_new((AvahiHashFunc) avahi_key_hash, (AvahiEqualFunc) avahi_key_equal, (AvahiFreeFunc) avahi_key_unref, NULL);
    e->queries_by_id = avahi_hashmap_new((AvahiHashFunc) avahi_int_hash, (AvahiEqualFunc) avahi_int_equal, NULL, NULL);
    AVAHI_LLIST_HEAD_INIT(AvahiLLMNRLookup, e->lookups);

    /* Cache*/
    e->cache_by_key = avahi_hashmap_new((AvahiHashFunc) avahi_key_hash, (AvahiEqualFunc) avahi_key_equal, (AvahiFreeFunc) avahi_key_unref, NULL);
    AVAHI_LLIST_HEAD_INIT(AvahiLLMNRCacheEntry, e->cache);

    return e;
}

void avahi_llmnr_lookup_engine_free(AvahiLLMNRLookupEngine *e) {
    assert(e);

    while (e->cache)
        destroy_cache_entry(e->cache);

    while (e->lookups)
        lookup_destroy(e->lookups);

    assert(e->n_cache_entries == 0);

    /* Clear all hashmap's*/
    avahi_hashmap_free(e->lookups_by_key);
    avahi_hashmap_free(e->queries_by_id);
    avahi_hashmap_free(e->cache_by_key);

    avahi_free(e);
}
