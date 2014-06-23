#ifndef foollmnrresponsehbar
#define foollmnrresponsehbar

typedef struct AvahiLLMNRResponseJob AvahiLLMNRResponseJob;
typedef struct AvahiLLMNRResponseScheduler AvahiLLMNRResponseScheduler;

#include <avahi-common/llist.h>
#include <avahi-common/timeval.h>

#include "iface.h"
#include "dns.h"

struct AvahiLLMNRResponseJob {
    AvahiLLMNRResponseScheduler *s;

    AvahiTimeEvent *time_event;
    AvahiDnsPacket *reply;

    AvahiAddress address;
    uint16_t port;

    AVAHI_LLIST_FIELDS(AvahiLLMNRResponseJob, jobs);
};

struct AvahiLLMNRResponseScheduler {
    AvahiInterface *interface;
    AvahiTimeEventQueue *time_event_queue;

    AVAHI_LLIST_HEAD(AvahiLLMNRResponseJob, jobs);
};

AvahiLLMNRResponseScheduler *avahi_llmnr_response_scheduler_new(AvahiInterface *i);
void avahi_llmnr_response_scheduler_free(AvahiLLMNRResponseScheduler *s);
void avahi_llmnr_response_scheduler_clear(AvahiLLMNRResponseScheduler *s);

int avahi_post_llmnr_response(AvahiLLMNRResponseScheduler *s, AvahiDnsPacket *p, const AvahiAddress *a, uint16_t port);
void avahi_llmnr_response_job_free(AvahiLLMNRResponseScheduler *s, AvahiLLMNRResponseJob *rj);

#endif

