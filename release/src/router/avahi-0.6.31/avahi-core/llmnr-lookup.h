#ifndef foollmnrlookuphbar
#define foollmnrlookuphbar

#include <avahi-common/llist.h>
#include <avahi-common/address.h>

#include "hashmap.h"
#include "lookup.h"
#include "rr.h"

#include "llmnr-querier.h"

#define LLMNR_CACHE_ENTRIES_MAX 500

typedef struct AvahiLLMNRLookup AvahiLLMNRLookup;
typedef struct AvahiLLMNRLookupEngine AvahiLLMNRLookupEngine;
typedef struct AvahiLLMNRCacheEntry AvahiLLMNRCacheEntry;

/* AvahiLLMNRLookup callback prototype */
typedef void (*AvahiLLMNRLookupCallback)(
    AvahiLLMNRLookupEngine *e,
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    AvahiLookupResultFlags flags,
    AvahiRecord *r,
    void *userdata);

/* LLMNR Cache entry */
struct AvahiLLMNRCacheEntry {
    AvahiLLMNRLookupEngine *engine;

    /* interface and protocol can't be
    AVAHI_IF_UNSPEC and AVAHI_PROTO_UNSPEC resp.*/
    AvahiIfIndex interface;
    AvahiProtocol protocol;

    AvahiRecord *record;

    struct timeval timestamp;
    struct timeval expiry;

    AvahiTimeEvent *time_event;

    AVAHI_LLIST_FIELDS(AvahiLLMNRCacheEntry, by_key);
    AVAHI_LLIST_FIELDS(AvahiLLMNRCacheEntry, cache);
};

/* LLMNR Lookup object */
struct AvahiLLMNRLookup {
    AvahiLLMNRLookupEngine *engine;
    int dead;

    AvahiKey *key, *cname_key;
    AvahiTimeEvent *time_event;

    AvahiIfIndex interface;
    AvahiProtocol protocol;

    AvahiLLMNRLookupCallback callback;
    void *userdata;

    AVAHI_LLIST_FIELDS(AvahiLLMNRLookup, lookups);
    AVAHI_LLIST_FIELDS(AvahiLLMNRLookup, by_key);

    int queries_issued;

    AVAHI_LLIST_HEAD(AvahiLLMNRQuery, queries);
};


/* LLMNR lookup engine */
struct AvahiLLMNRLookupEngine {
    AvahiServer *s;

    /* For 'AvahiLLMNRQuery' object*/
    uint16_t next_id;

    /*lookups and queries */
    AVAHI_LLIST_HEAD(AvahiLLMNRLookup, lookups);
    AvahiHashmap *lookups_by_key;
    AvahiHashmap *queries_by_id;

    /* Cache */
    AVAHI_LLIST_HEAD(AvahiLLMNRCacheEntry, cache);
    AvahiHashmap *cache_by_key;
    unsigned n_cache_entries;

    int cleanup_dead;
};

/*** Engine functions ***/

AvahiLLMNRLookupEngine* avahi_llmnr_lookup_engine_new(AvahiServer *sl);
void avahi_llmnr_lookup_engine_free(AvahiLLMNRLookupEngine *e);
void avahi_llmnr_lookup_engine_cleanup(AvahiLLMNRLookupEngine *e);

/* when new interface come up, call this function to issue queries */
void avahi_llmnr_lookup_engine_new_interface(AvahiLLMNRLookupEngine *e, AvahiInterface *i);

/* remove all cache entries from this interface */
void avahi_llmnr_clear_cache(AvahiLLMNRLookupEngine *e, AvahiInterface *i);

/*** AvahiLLMNRLookup functions ***/

AvahiLLMNRLookup* avahi_llmnr_lookup_new(
    AvahiLLMNRLookupEngine *e,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiKey *key,
    AvahiLLMNRLookupCallback callback,
    void *userdata);

void avahi_llmnr_lookup_free(AvahiLLMNRLookup *l);

/*** Cache functions ***/
void avahi_llmnr_cache_dump(AvahiLLMNRLookupEngine *e, AvahiDumpCallback callback, void *userdata);

unsigned avahi_scan_llmnr_cache(
    AvahiLLMNRLookupEngine *e,
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiKey *key,
    AvahiLLMNRLookupCallback callback,
    void *userdata
);
#endif /* foollmnrlookuphbar */
