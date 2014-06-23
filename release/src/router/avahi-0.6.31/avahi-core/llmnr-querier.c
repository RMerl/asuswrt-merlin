#include <stdlib.h>
#include <stdio.h>

#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include "log.h"

#include "llmnr-querier.h"
#include "llmnr-query-sched.h"
#include "llmnr-lookup.h"

static AvahiLLMNRQuery* find_query(AvahiLLMNRLookupEngine *e, uint16_t id) {
    AvahiLLMNRQuery *lq;
    /* Convert to 32 bit integer*/
    uint32_t i = (uint32_t) id;

    assert(e);

    if (!(lq = avahi_hashmap_lookup(e->queries_by_id, &i)))
        return NULL;

    assert(lq->id == id);

    if (lq->dead)
        return NULL;

    return lq;
}

AvahiLLMNRQuery* avahi_llmnr_query_add(AvahiInterface *i, AvahiKey *key, AvahiLLMNRQueryType type, AvahiLLMNRQueryCallback callback, void *userdata) {
    AvahiLLMNRQuery *lq;
    int c;

    /* Engine maintains a hashmap of 'AvahiLLMNRQuery' objects by id */
    AvahiLLMNRLookupEngine *e = i->monitor->server->llmnr.llmnr_lookup_engine;

    assert(i);
    assert(key);
    assert(callback);

    if (!(lq = avahi_new(AvahiLLMNRQuery, 1)))
        return NULL;

    /* Initialize parameters */
    lq->dead = 0;
    lq->key = avahi_key_ref(key);
    lq->type = type;
    lq->interface = i;
    lq->callback = callback;
    lq->post_id_valid = 0;
    lq->post_id = 0;
    lq->userdata = userdata;

    /* Initialize record lists for this query object */
    lq->c_bit_set = avahi_record_list_new();
    lq->c_bit_clear = avahi_record_list_new();

    c = avahi_record_list_is_empty(lq->c_bit_set);
    assert(c);
    assert(lq->c_bit_clear);

    /* Set ID */
    /* KEEP IT 32 BIT for HASHMAP */
    for (;; e->next_id++)
        if (!(find_query(e, e->next_id)))
            break;

    lq->id = e->next_id++;

    /* AvahiLLMNRLookupEngine hashmap of queries by ID,
    'engine-AvahiLLMNRQuery'. (lq->id and lq) */
    avahi_hashmap_insert(e->queries_by_id, &(lq->id), lq);

    /* Schedule this LLMNR query. This will create an
    'AvahiLLMNRQueryJob' for this query and will issue
    further queries there only. im0 */
    if (!avahi_interface_post_llmnr_query(i, lq, 0))
        return NULL;

    return lq;

}

void avahi_llmnr_query_destroy(AvahiLLMNRQuery *lq) {
    AvahiLLMNRLookupEngine *e;
    e = lq->interface->monitor->server->llmnr.llmnr_lookup_engine;

    assert(lq);

    avahi_hashmap_remove(e->queries_by_id, &(lq->id));

    avahi_key_unref(lq->key);
    avahi_free(lq);

    return;
}

void avahi_llmnr_query_remove(AvahiInterface *i, AvahiKey *key) {
    /* First find for the AvahiLLMNRQueryJob object for this 'i' and 'key' */
    AvahiLLMNRQueryJob *qj;

    assert(i);
    assert(key);

    /* Interface is maintaing hashmap of AvahiLLMNRQueryJob's by key*/
    if (!(qj = avahi_hashmap_lookup(i->llmnr.queryjobs_by_key, key)))
        return;

    /* get 'qj' -> destroy(s, qj) -> destroy(lq) */
    avahi_llmnr_query_job_destroy(qj->scheduler, qj);

    return;
}

struct cbdata {
    AvahiLLMNRQueryCallback callback;
    AvahiKey *key;
    void *userdata;
};

static void llmnr_query_add_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void *userdata) {
    struct cbdata *cbdata;

    assert(m);
    assert(i);
    assert(userdata);

    cbdata = (struct cbdata *)(userdata);

    /* Issue AVAHI_LLMNR_SIMPLE_QUERY type Query*/
    if (!avahi_llmnr_query_add( i, (AvahiKey*)(cbdata->key), AVAHI_LLMNR_SIMPLE_QUERY, cbdata->callback, cbdata->userdata))
        /* set errno*/

    return;
}


void avahi_llmnr_query_add_for_all(
    AvahiServer *s,
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiKey *key,
    AvahiLLMNRQueryCallback callback,
    void *userdata) {

    struct cbdata cbdata;

    assert(s);
    assert(callback);
    assert(AVAHI_IF_VALID(idx));
    assert(AVAHI_PROTO_VALID(protocol));
    assert(key);

    cbdata.callback = callback;
    cbdata.key = key;
    cbdata.userdata = userdata;

    avahi_interface_monitor_walk(s->monitor, idx, protocol, llmnr_query_add_callback, &cbdata);
}

void avahi_llmnr_query_free(AvahiLLMNRQuery *lq) {
    assert(lq);

    avahi_llmnr_query_scheduler_withdraw_by_id(lq->interface->llmnr.query_scheduler, lq->post_id);
}

static void remove_llmnr_query_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void *userdata) {

    assert(m);
    assert(i);
    assert(userdata);

    /* Remove Query */
    avahi_llmnr_query_remove(i, (AvahiKey*) userdata);
}

void avahi_llmnr_query_remove_for_all(AvahiServer *s, AvahiIfIndex idx, AvahiProtocol protocol, AvahiKey *key) {

    assert(s);
    assert(AVAHI_IF_VALID(idx));
    assert(AVAHI_PROTO_VALID(protocol));
    assert(key);

    avahi_interface_monitor_walk(s->monitor, idx, protocol, remove_llmnr_query_callback, key);
}

void avahi_llmnr_queries_free(AvahiInterface *i) {
    AvahiLLMNRQueryJob *qj;
    assert(i);

    for (qj = i->llmnr.queryjobs; qj; qj = qj->jobs_by_interface_next)
        avahi_llmnr_query_job_destroy(qj->scheduler, qj);
}
