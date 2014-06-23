#include <stdlib.h>
#include <avahi-common/malloc.h>
#include <avahi-core/log.h>

#include "llmnr-response.h"

AvahiLLMNRResponseScheduler *avahi_llmnr_response_scheduler_new(AvahiInterface *i) {
    AvahiLLMNRResponseScheduler *s;

    assert(i);

    if (!(s = avahi_new(AvahiLLMNRResponseScheduler, 1)))
        return NULL;

    s->interface = i;
    s->time_event_queue = i->monitor->server->time_event_queue;

    AVAHI_LLIST_HEAD_INIT(AvahiLLMNRResponseJob, s->jobs);

    return s;
}

static AvahiLLMNRResponseJob *job_new(
    AvahiLLMNRResponseScheduler *s,
    AvahiDnsPacket *p,
    const AvahiAddress *a,
    uint16_t port) {
    AvahiLLMNRResponseJob *rj;

    assert(a->proto == s->interface->protocol);

    if (!(rj = avahi_new(AvahiLLMNRResponseJob, 1)))
        return NULL;

    rj->s = s;
    rj->reply = p;
    rj->address = *a;
    rj->port = port;
    rj->time_event = NULL;

    AVAHI_LLIST_PREPEND(AvahiLLMNRResponseJob, jobs, s->jobs , rj);

    return rj;
}

void avahi_llmnr_response_job_free(AvahiLLMNRResponseScheduler *s, AvahiLLMNRResponseJob *rj) {
    assert(s);
    assert(rj);

    if (rj->time_event)
        avahi_time_event_free(rj->time_event);

    if (rj->reply)
        avahi_dns_packet_free(rj->reply);

    AVAHI_LLIST_REMOVE(AvahiLLMNRResponseJob, jobs, s->jobs, rj);

    avahi_free(rj);
}


void avahi_llmnr_response_scheduler_clear(AvahiLLMNRResponseScheduler *s) {
    assert(s);

    while (s->jobs)
        avahi_llmnr_response_job_free(s, s->jobs);
}

void avahi_llmnr_response_scheduler_free(AvahiLLMNRResponseScheduler *s) {
    assert(s);

    avahi_llmnr_response_scheduler_clear(s);
    avahi_free(s);
}

static void elapse_response_callback(AVAHI_GCC_UNUSED AvahiTimeEvent *e, void *userdata) {
    AvahiLLMNRResponseJob *rj = userdata;

    assert(rj);

    /* Send Packet */
    avahi_interface_send_packet_unicast(rj->s->interface, rj->reply, &rj->address, rj->port, AVAHI_LLMNR);
    avahi_llmnr_response_job_free(rj->s, rj);
}

int avahi_post_llmnr_response(AvahiLLMNRResponseScheduler *s, AvahiDnsPacket *p, const AvahiAddress *a, uint16_t port) {
    AvahiLLMNRResponseJob *rj;
    struct timeval tv;

    assert(s);
    assert(p);
    assert(a);
    assert(port);

    assert(a->proto == s->interface->protocol);

    if (!s->interface->llmnr.verifying)
        return 0;

    if (!(rj = job_new(s, p, a, port)))
        return 0;

    avahi_elapse_time(&tv, 0, AVAHI_LLMNR_JITTER);
    rj->time_event = avahi_time_event_new(s->time_event_queue, &tv, elapse_response_callback, rj);

    return 1;
}


