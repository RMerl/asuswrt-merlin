#ifndef foollmnrqueryschedhbar
#define foollmnrqueryschedhbar

typedef struct AvahiLLMNRQueryJob AvahiLLMNRQueryJob;
typedef struct AvahiLLMNRQueryScheduler AvahiLLMNRQueryScheduler;

#include "iface.h"

#define AVAHI_LLMNR_INTERVAL 1000
#define AVAHI_LLMNR_JITTER 100

struct AvahiLLMNRQueryJob {
    AvahiLLMNRQueryScheduler *scheduler;
    AvahiLLMNRQuery *lq;

    int n_sent, prev_scheduled;
    AvahiDnsPacket *p;

    AvahiTimeEvent *time_event;

    /* List of jobs maintained by AvahiLLMNRQueryScheduler*/
    AVAHI_LLIST_FIELDS(AvahiLLMNRQueryJob, jobs_by_scheduler);

    /* List of jobs maintained by Interface. */
    /* Both contain the same data. */
    AVAHI_LLIST_FIELDS(AvahiLLMNRQueryJob, jobs_by_interface);
};

struct AvahiLLMNRQueryScheduler {
    AvahiInterface *i;
    AvahiTimeEventQueue *time_event_queue;

    /* For 'lq' lq->post_id */
    unsigned int next_id;

    AVAHI_LLIST_HEAD(AvahiLLMNRQueryJob, jobs);
};

/* Add new query scheduler for specified Interface*/
AvahiLLMNRQueryScheduler *avahi_llmnr_query_scheduler_new(AvahiInterface *i);

/* This would destroy 'qj' as well as corresponding 'lq' object. */
void avahi_llmnr_query_job_destroy(AvahiLLMNRQueryScheduler *s, AvahiLLMNRQueryJob *qj);

void avahi_llmnr_query_scheduler_free(AvahiLLMNRQueryScheduler *s);
void avahi_llmnr_query_scheduler_clear(AvahiLLMNRQueryScheduler *s);

/* Issue an 'AvahiLLMNRQueryJob' for 'AvahiLLMNRQuery' object(lq) on specified scheduler */
/* This would fill 'lq->post_id' */
int avahi_llmnr_query_scheduler_post(AvahiLLMNRQueryScheduler *s, AvahiLLMNRQuery *lq, int immediately);

/* Remove AvahiLLMNRQuery and AvahiLLMNRQueryJob for this id.
(id == ls->post_id, which was created during avahi_llmnr_query_scheduler_post(s, lq, immediately)) */
int avahi_llmnr_query_scheduler_withdraw_by_id(AvahiLLMNRQueryScheduler *s, unsigned id);
void avahi_llmnr_query_job_remove(AvahiInterface *i, AvahiKey *key);

#endif /*foollmnrquerychedhbar*/
