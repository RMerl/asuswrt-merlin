#include <string.h>
#include <stdio.h>

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "log.h"
#include "rrlist.h"

#include "llmnr-query-sched.h"
#include "dns.h"
#include "verify.h"

AvahiLLMNRQueryScheduler *avahi_llmnr_query_scheduler_new(AvahiInterface *i) {
    AvahiLLMNRQueryScheduler *s;
    assert(i);

    if (!(s = avahi_new(AvahiLLMNRQueryScheduler, 1)))
        return NULL;

    s->i = i;
    s->time_event_queue = i->monitor->server->time_event_queue;
    s->next_id = 1;
    AVAHI_LLIST_HEAD_INIT(AvahiLLMNRQueryJob, s->jobs);

    return s;
}

void avahi_llmnr_query_scheduler_free(AvahiLLMNRQueryScheduler *s) {
    assert(s);

    avahi_llmnr_query_scheduler_clear(s);
    avahi_free(s);
}

void avahi_llmnr_query_scheduler_clear(AvahiLLMNRQueryScheduler *s) {
    assert(s);

    while (s->jobs)
        avahi_llmnr_query_job_destroy(s, s->jobs);
}

void avahi_llmnr_query_job_destroy(AvahiLLMNRQueryScheduler *s, AvahiLLMNRQueryJob *qj) {
    assert(s);
    assert(qj);

     /* Free lq */
    avahi_llmnr_query_destroy(qj->lq);

    /* Free dns packet and time_event */
    if (qj->p)
        avahi_dns_packet_free(qj->p);

    if (qj->time_event)
        avahi_time_event_free(qj->time_event);

    /* Remove from the lists */
    AVAHI_LLIST_REMOVE(AvahiLLMNRQueryJob, jobs_by_scheduler, s->jobs, qj);
    AVAHI_LLIST_REMOVE(AvahiLLMNRQueryJob, jobs_by_interface, s->i->llmnr.queryjobs, qj);

    avahi_free(qj);
}

static void avahi_prepare_llmnr_query_job_packet(AvahiLLMNRQueryJob *qj) {
    assert(qj);

    /* New Packet*/
    qj->p = avahi_llmnr_packet_new_query(512 + AVAHI_DNS_PACKET_EXTRA_SIZE);

    /* Set ID*/
    avahi_dns_packet_set_field(qj->p, AVAHI_LLMNR_FIELD_ID, (uint16_t)(qj->lq->id));

    /*Append Key*/
    if (!avahi_llmnr_packet_append_key(qj->p, qj->lq->key))
        return;

    /* Set QDCOUNT */
    avahi_dns_packet_set_field(qj->p, AVAHI_LLMNR_FIELD_QDCOUNT, 1);

}

static AvahiLLMNRQueryJob *job_new(AvahiLLMNRQueryScheduler *s, AvahiLLMNRQuery *lq) {
    AvahiLLMNRQueryJob *qj;

    assert(s);
    assert(lq);

    if (!(qj = avahi_new(AvahiLLMNRQueryJob, 1)))
        return NULL;

    qj->scheduler = s;
    qj->lq = lq;

    /* Set lq parameters */
    lq->post_id_valid = 1;
    lq->post_id = s->next_id++;

    /* qj parameters */
    qj->n_sent = 0;
    qj->prev_scheduled = 0;
    qj->time_event = NULL;

    /* Prepend in Lists */
    AVAHI_LLIST_PREPEND(AvahiLLMNRQueryJob, jobs_by_scheduler, s->jobs, qj);
    AVAHI_LLIST_PREPEND(AvahiLLMNRQueryJob, jobs_by_interface, s->i->llmnr.queryjobs, qj);

    /*Just prepare dns packet, don't send it */
    avahi_prepare_llmnr_query_job_packet(qj);

    return qj;
}


static void resend_llmnr_query(AvahiLLMNRQueryJob *qj) {
    AvahiLLMNRQuery *lq = qj->lq;
    struct timeval tv;
    assert(qj);

    if (lq->type == AVAHI_LLMNR_SIMPLE_QUERY || lq->type == AVAHI_LLMNR_UNIQUENESS_VERIFICATION_QUERY) {
        /**Check whether we have already sent this query three times */
        if (qj->n_sent >= 3) {
            lq->callback(lq->interface->hardware->index, lq->interface->protocol, NULL, lq->userdata);
            avahi_llmnr_query_job_destroy(qj->scheduler, qj);
            /* Free Timeevent */
/*            avahi_time_event_free(qj->time_event);
            qj->time_event = NULL;*/

        } else {

            /* Send packet */
            avahi_interface_send_packet(lq->interface, qj->p, AVAHI_LLMNR);
            (qj->n_sent)++;
            /* Schedule further queries*/
            avahi_elapse_time(&tv, AVAHI_LLMNR_INTERVAL, 0);
            avahi_time_event_update(qj->time_event, &tv);
        }

    } else {

        assert(lq->type == AVAHI_LLMNR_CONFLICT_QUERY);
        assert(qj->n_sent == 1);

        /* Destroy this query */
        lq->callback(lq->interface->hardware->index, lq->interface->protocol, NULL, lq->userdata);
        avahi_llmnr_query_job_destroy(qj->scheduler, qj);
    }

    return;
}

static void reschedule_llmnr_query_job(AvahiLLMNRQueryJob *qj) {
    struct timeval tv;

    assert(qj);
    assert(!avahi_record_list_is_empty(qj->lq->c_bit_clear));

    if (!(qj->prev_scheduled)) {

        qj->prev_scheduled = 1;
        avahi_elapse_time(&tv, AVAHI_LLMNR_INTERVAL + AVAHI_LLMNR_JITTER, 0);
        avahi_time_event_update(qj->time_event, &tv);

    } else {
        /* We have already waited but we still have not received any response/s
        with 'c' bit clear. */
        qj->lq->callback(qj->lq->interface->hardware->index, qj->lq->interface->protocol, NULL, qj->lq->userdata);
        /*avahi_time_event_free(qj->time_event);
        qj->time_event = NULL;*/
        avahi_llmnr_query_job_destroy(qj->scheduler, qj);
    }
    return;
}

static void call_llmnr_query_callback(AvahiLLMNRQuery *lq) {
    AvahiRecord *r;

    assert(lq);
    assert(avahi_record_list_is_empty(lq->c_bit_set));

    while ((r = avahi_record_list_next(lq->c_bit_clear, NULL, NULL, NULL)))
        lq->callback(lq->interface->hardware->index, lq->interface->protocol, r, lq->userdata);

    return;
}

static void send_conflict_query(AvahiLLMNRQueryJob *qj) {
    /* Send the packet */
    AvahiRecord *r;
    AvahiLLMNRQuery *lq;
    assert(qj);

    /* We use the same lq */

    /*1. Change 'lq->type' and reset 'n_sent' */
    lq = qj->lq;
    lq->callback(lq->interface->hardware->index, lq->interface->protocol, NULL, lq->userdata);
    lq->type = AVAHI_LLMNR_CONFLICT_QUERY;
    qj->n_sent = 0;

    /* Reset 'c' bit in existing packet */
    avahi_dns_packet_set_field(qj->p, AVAHI_LLMNR_FIELD_FLAGS, AVAHI_LLMNR_FLAGS(0, 0, 1, 0, 0, 0, 0));

    /* Append records */
    while ((r = avahi_record_list_next(lq->c_bit_clear, NULL, NULL, NULL))) {
        /* Append Record TODO TTL*/
        if (!(avahi_llmnr_packet_append_record(qj->p, r, AVAHI_DEFAULT_TTL)))
        /* we send only those much of record which fits in */
            break;
        avahi_dns_packet_inc_field(qj->p, AVAHI_LLMNR_FIELD_ARCOUNT);
    }

    /* Send packet */
    if (avahi_dns_packet_get_field(qj->p, AVAHI_LLMNR_FIELD_ARCOUNT) != 0)
        avahi_interface_send_packet(lq->interface, qj->p, AVAHI_LLMNR);

    /* Destroy this AvahiLLMNRQueryJob */
    avahi_llmnr_query_job_destroy(qj->scheduler, qj);

    return;

}

static void elapse_timeout_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void *userdata) {
    AvahiLLMNRQueryJob *qj = userdata;
    AvahiLLMNRQuery *lq = qj->lq;
    int c_bit_set, c_bit_clear, counter;

    c_bit_set = avahi_record_list_is_empty(lq->c_bit_set);
    c_bit_clear = avahi_record_list_is_empty(lq->c_bit_clear);

    counter = (2*(!c_bit_set)) + (!c_bit_clear);

    switch (counter)/*TODO, clean it*/  {

        case 0 :
             /*We have not yet received any responses. Try to resend the same query*/
            resend_llmnr_query(qj);
            break;

        case 1 :
            /*We have received one or multiple reponse/s with 'c' bit clear
            and none with 'c' bit set within AVAHI_LLMNR_TIMEOUT. It means
            query has been replied. */
            call_llmnr_query_callback(lq);
            avahi_llmnr_query_job_destroy(qj->scheduler, qj);
            /*avahi_time_event_free(qj->time_event);
            qj->time_event = NULL;*/
            break;

        case 2 :
            /*We have received atleast one response with 'c' bit set but we didn't
            receive any response with 'c' bit clear. We don't want to send this query
            further but wait for atleast LLMNR_TIMEOUT + JITTER_INTERVAL to collect
            more responses, if any */
            reschedule_llmnr_query_job(qj);
            break;

        case 3 :
            /*This is conflict. Let us send AVAHI_LLMNR_CONFLICT_QUERY (with c bit set
            in query) along with responses*/
            avahi_log_info("CONFLICT..CONFLICT..CONFLICT");
            send_conflict_query(qj);
            avahi_llmnr_query_job_destroy(qj->scheduler, qj);
            break;
    }

    return;
}

int avahi_llmnr_query_scheduler_post(AvahiLLMNRQueryScheduler *s, AvahiLLMNRQuery *lq, int immediately) {
    AvahiLLMNRQueryJob *qj;
    struct timeval tv;

    assert(s);
    assert(lq);

    if (!(qj = job_new(s, lq)))
        return 0;

    qj->time_event = avahi_time_event_new(s->time_event_queue,
                                          avahi_elapse_time(&tv, 0, immediately ? 0 : AVAHI_LLMNR_JITTER),
                                          elapse_timeout_callback,
                                          qj);

    return 1;
}

int avahi_llmnr_query_scheduler_withdraw_by_id(AvahiLLMNRQueryScheduler *s, unsigned post_id) {
    AvahiLLMNRQueryJob *qj;

    assert(s);

    for (qj = s->jobs; qj; qj = qj->jobs_by_scheduler_next) {
        assert(!(qj->lq->dead));

        if (qj->lq->post_id == post_id) {
            avahi_llmnr_query_job_destroy(qj->scheduler, qj);
            return 1;
        }
    }

    return 0;
}

void avahi_llmnr_query_job_remove(AvahiInterface *i, AvahiKey *key) {
    AvahiLLMNRQueryJob *qj;

    assert(i);
    assert(key);

    if (!(qj = avahi_hashmap_lookup(i->llmnr.queryjobs_by_key, key)))
    /* No AvahiLLMNRQueryJob object present for this key*/
        return;

    avahi_llmnr_query_job_destroy(qj->scheduler, qj);

    return;
}
