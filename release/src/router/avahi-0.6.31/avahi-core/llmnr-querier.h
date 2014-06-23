#ifndef foollmnrquerierbar
#define foollmnrquerierbar

typedef struct AvahiLLMNRQuery AvahiLLMNRQuery;

#include <sys/types.h>

#include <avahi-common/address.h>
#include <avahi-common/defs.h>

#include "core.h"
#include "publish.h"
#include "rrlist.h"
#include "internal.h"

typedef void (*AvahiLLMNRQueryCallback)(
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiRecord *r,
    void *userdata);

typedef enum {
    AVAHI_LLMNR_SIMPLE_QUERY,
    AVAHI_LLMNR_CONFLICT_QUERY,
    AVAHI_LLMNR_UNIQUENESS_VERIFICATION_QUERY
} AvahiLLMNRQueryType;


struct AvahiLLMNRQuery{
    int dead;

    /* 'e = interface->monitor->server->llmnr->llmnr_lookup_engine'
    maintains the hashmap for all queries by id. */
    uint32_t id;

    AvahiKey *key;
    AvahiInterface *interface;
    AvahiLLMNRQueryCallback callback;
    AvahiLLMNRQueryType type;

    AvahiRecordList *c_bit_set, *c_bit_clear;

    /* 'AvahiLLMNRQueryScheduler' next_id */
    unsigned post_id;
    int post_id_valid;

    void *userdata;
};

/* Issue an LLMNR Query on the specified interface and key*/
AvahiLLMNRQuery* avahi_llmnr_query_add(AvahiInterface *i, AvahiKey *key, AvahiLLMNRQueryType type, AvahiLLMNRQueryCallback callback, void *userdata);

/* Remove a scheduled LLMNR query for the specified key and interface*/
void avahi_llmnr_query_remove(AvahiInterface *i, AvahiKey *key);

/* Issue LLMNR queries for the specied key on all interfaces that match
idx and protocol. In this case callback function will be 'callback' for
all those queries. and it issues AVAHI_LLMNR_SIMPLE_QUERY type of query*/
void avahi_llmnr_query_add_for_all(AvahiServer *s, AvahiIfIndex idx, AvahiProtocol protocol, AvahiKey *key, AvahiLLMNRQueryCallback callback, void *userdata);

/* Remove LLMNR queries for the specified key on all interface that match*/
void avahi_llmnr_query_remove_for_all(AvahiServer *s, AvahiIfIndex idx, AvahiProtocol protocol, AvahiKey *key);

/* Remove specified query */
void avahi_llmnr_query_free(AvahiLLMNRQuery *lq);
void avahi_llmnr_query_destroy(AvahiLLMNRQuery *lq);

/** Remove all queries from specified interface **/
void avahi_llmnr_queries_free(AvahiInterface *i);

#endif /* foollmnrquerierbar */
