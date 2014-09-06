#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <limits.h>

netsnmp_feature_require(container_fifo)

#include "deliverByNotify.h"

/* we should never split beyond this */
#define MAX_MESSAGE_COUNT 128
#define BASE_PACKET_SIZE 100 /* should be enough to store SNMPv3 msg headers */

/* if v is !NULL, then estimate it's likely size */
#define ESTIMATE_VAR_SIZE(v) (v?(v->name_length + v->val_len + 8):0)

void parse_deliver_config(const char *, char *);
void parse_deliver_maxsize_config(const char *, char *);
void parse_data_notification_oid_config(const char *, char *);
void parse_periodic_time_oid_config(const char *, char *);
void parse_message_number_oid_config(const char *, char *);
void parse_max_message_number_oid_config(const char *, char *);
void free_deliver_config(void);

static void _schedule_next_execute_time(void);

oid    data_notification_oid[MAX_OID_LEN]
       = { 1, 3, 6, 1, 4, 1, 8072, 3, 1, 5, 4, 0, 1 };
size_t data_notification_oid_len = 13;

oid    netsnmp_periodic_time_oid[MAX_OID_LEN]
       = { 1, 3, 6, 1, 4, 1, 8072, 3, 1, 5, 3, 1, 0 };
size_t netsnmp_periodic_time_oid_len = 13;

oid    netsnmp_message_number_oid[MAX_OID_LEN]
       = { 1, 3, 6, 1, 4, 1, 8072, 3, 1, 5, 3, 2, 0 };
size_t netsnmp_message_number_oid_len = 13;

oid    netsnmp_max_message_number_oid[MAX_OID_LEN]
       = { 1, 3, 6, 1, 4, 1, 8072, 3, 1, 5, 3, 3, 0 };
size_t netsnmp_max_message_number_oid_len = 13;

oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };

#define DEFAULT_MAX_DELIVER_SIZE -1;
static int default_max_size;
unsigned int alarm_reg;
static netsnmp_container *deliver_container;

static int
_deliver_compare(deliver_by_notify *lhs, deliver_by_notify *rhs) {
    /* sort by the next_run time */
    if (lhs->next_run < rhs->next_run)
        return -1;
    else
        return 1;
}

/** Initializes the mteTrigger module */
void
init_deliverByNotify(void)
{
    /* register the config tokens */
    snmpd_register_config_handler("deliverByNotify",
                                  &parse_deliver_config, &free_deliver_config,
                                  "[-p] [-m] [-s maxsize] FREQUENCY OID");

    snmpd_register_config_handler("deliverByNotifyMaxPacketSize",
                                  &parse_deliver_maxsize_config, NULL,
                                  "sizeInBytes");

    snmpd_register_config_handler("deliverByNotifyOid",
                                  &parse_data_notification_oid_config,
                                  NULL, /* XXX: reset to default */
                                  "OID");

    snmpd_register_config_handler("deliverByNotifyFrequencyOid",
                                  &parse_periodic_time_oid_config,
                                  NULL, /* XXX: reset to default */
                                  "OID");

    snmpd_register_config_handler("deliverByNotifyMessageNumberOid",
                                  &parse_message_number_oid_config,
                                  NULL, /* XXX: reset to default */
                                  "OID");

    snmpd_register_config_handler("deliverByNotifyMaxMessageNumberOid",
                                  &parse_max_message_number_oid_config,
                                  NULL, /* XXX: reset to default */
                                  "OID");

    /* create the container to store the config objects*/
    deliver_container = netsnmp_container_find("deliverByNotify:fifo");
    if (NULL == deliver_container) {
        snmp_log(LOG_ERR,
                 "deliverByNotify: failed to initialize our data container\n");
        return;
    }
    deliver_container->container_name = strdup("deliverByNotify");
    deliver_container->compare = (netsnmp_container_compare *) _deliver_compare;

    /* set the defaults */
    default_max_size = DEFAULT_MAX_DELIVER_SIZE;

    alarm_reg = 0;
}

void
_parse_config_oid(const char *token, char *line,
                  oid *oid_store, size_t *oid_store_len) {
    size_t tmp_len = MAX_OID_LEN;
    /* parse the OID given */

    if (!snmp_parse_oid(line, oid_store, &tmp_len)) {
        char buf[SPRINT_MAX_LEN];
        snprintf(buf, SPRINT_MAX_LEN-1, "unknown %s OID: %s", token, line);
        config_perror(buf);
        return;
    }

    *oid_store_len = tmp_len;
}

void
parse_data_notification_oid_config(const char *token, char *line) {
    _parse_config_oid(token, line,
                      data_notification_oid, &data_notification_oid_len);
}

void
parse_periodic_time_oid_config(const char *token, char *line) {
    _parse_config_oid(token, line,
                      netsnmp_periodic_time_oid, &netsnmp_periodic_time_oid_len);
}

void
parse_message_number_oid_config(const char *token, char *line) {
    _parse_config_oid(token, line,
                      netsnmp_message_number_oid,
                      &netsnmp_message_number_oid_len);
}

void
parse_max_message_number_oid_config(const char *token, char *line) {
    _parse_config_oid(token, line,
                      netsnmp_max_message_number_oid,
                      &netsnmp_max_message_number_oid_len);
}

void
parse_deliver_config(const char *token, char *line) {
    const char *cp = line;
    char buf[SPRINT_MAX_LEN];
    size_t buf_len = SPRINT_MAX_LEN;
    int max_size = DEFAULT_MAX_DELIVER_SIZE;
    int frequency;
    oid target_oid[MAX_OID_LEN];
    size_t target_oid_len = MAX_OID_LEN;
    deliver_by_notify *new_notify = NULL;
    int flags = 0;

    while(cp && *cp == '-') {
        switch (*(cp+1)) {
        case 's':
            cp = skip_token_const(cp);
            if (!cp) {
                config_perror("no argument given to -s");
                return;
            }
            max_size = atoi(cp);
            break;

        case 'p':
            flags = flags | NETSNMP_DELIVER_NO_PERIOD_OID;
            break;

        case 'm':
            flags = flags | NETSNMP_DELIVER_NO_MSG_COUNTS;
            break;

        default:
            config_perror("unknown flag");
            return;
        }
        cp = skip_token_const(cp);
    }

    if (!cp) {
        config_perror("no frequency given");
        return;
    }
    copy_nword(cp, buf, buf_len);
    frequency = netsnmp_string_time_to_secs(buf);
    cp = skip_token_const(cp);

    if (frequency <= 0) {
        config_perror("illegal frequency given");
        return;
    }

    if (!cp) {
        config_perror("no OID given");
        return;
    }

    /* parse the OID given */
    if (!snmp_parse_oid(cp, target_oid, &target_oid_len)) {
        config_perror("unknown deliverByNotify OID");
        DEBUGMSGTL(("deliverByNotify", "The OID with the problem: %s\n", cp));
        return;
    }

    /* set up the object to store all the data */
    new_notify = SNMP_MALLOC_TYPEDEF(deliver_by_notify);
    new_notify->frequency = frequency;
    new_notify->max_packet_size = max_size;
    new_notify->last_run = time(NULL);
    new_notify->next_run = new_notify->last_run + frequency;
    new_notify->flags = flags;

    new_notify->target = malloc(target_oid_len * sizeof(oid));
    new_notify->target_len = target_oid_len;
    memcpy(new_notify->target, target_oid, target_oid_len*sizeof(oid));

    /* XXX: need to do the whole container */
    snmp_alarm_register(calculate_time_until_next_run(new_notify, NULL), 0, 
                        &deliver_execute, NULL);

    /* add it to the container */
    CONTAINER_INSERT(deliver_container, new_notify);
    _schedule_next_execute_time();
}

void
parse_deliver_maxsize_config(const char *token, char *line) {
    default_max_size = atoi(line);
}

static void
_free_deliver_obj(deliver_by_notify *obj, void *context) {
    netsnmp_assert_or_return(obj != NULL, );
    SNMP_FREE(obj->target);
    SNMP_FREE(obj);
}

void
free_deliver_config(void) {
    default_max_size = DEFAULT_MAX_DELIVER_SIZE;
    CONTAINER_CLEAR(deliver_container,
                    (netsnmp_container_obj_func *) _free_deliver_obj, NULL);
    if (alarm_reg) {
        snmp_alarm_unregister(alarm_reg);
        alarm_reg = 0;
    }
}

void
deliver_execute(unsigned int clientreg, void *clientarg) {
    netsnmp_variable_list *vars, *walker, *deliver_notification, *vartmp;
    netsnmp_variable_list *ready_for_delivery[MAX_MESSAGE_COUNT];
    netsnmp_session *sess;
    int rc, i;
    deliver_by_notify *obj;
    netsnmp_iterator *iterator;
    time_t            now = time(NULL);
    u_long            message_count, max_message_count, tmp_long;
    u_long           *max_message_count_ptrs[MAX_MESSAGE_COUNT];
    size_t            estimated_pkt_size;

    DEBUGMSGTL(("deliverByNotify", "Starting the execute routine\n"));

    /* XXX: need to do the whole container */
    iterator = CONTAINER_ITERATOR(deliver_container);
    netsnmp_assert_or_return(iterator != NULL,);

    sess = netsnmp_query_get_default_session();

    for(obj = ITERATOR_FIRST(iterator); obj;
        obj = ITERATOR_NEXT(iterator)) {

        /* check if we need to run this one yet */
        if (obj->next_run > now)
            continue;

        max_message_count = 1;
        message_count = 0;

        /* fill the varbind list with the target object */
        vars = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
        snmp_set_var_objid( vars, obj->target, obj->target_len );
        vars->type = ASN_NULL;

        /* walk the OID tree for the data */
        rc = netsnmp_query_walk(vars, sess);
        if (rc != SNMP_ERR_NOERROR) {
            /* XXX: disable? and reset the next query time point! */
            snmp_log(LOG_ERR, "deliverByNotify: failed to issue the query");
            ITERATOR_RELEASE(iterator);
            return;
        }

        walker = vars;

        while (walker) {

            /* Set up the notification itself */
            deliver_notification = NULL;
            estimated_pkt_size = BASE_PACKET_SIZE;

            /* add in the notification type */
            snmp_varlist_add_variable(&deliver_notification,
                                      objid_snmptrap, OID_LENGTH(objid_snmptrap),
                                      ASN_OBJECT_ID,
                                      data_notification_oid,
                                      data_notification_oid_len * sizeof(oid));
            estimated_pkt_size += ESTIMATE_VAR_SIZE(deliver_notification);

            /* add in the current message number in this sequence */
            if (!(obj->flags & NETSNMP_DELIVER_NO_PERIOD_OID)) {
                tmp_long = obj->frequency;
                snmp_varlist_add_variable(&deliver_notification,
                                          netsnmp_periodic_time_oid,
                                          netsnmp_periodic_time_oid_len,
                                          ASN_UNSIGNED,
                                          (const void *) &tmp_long,
                                          sizeof(tmp_long));
                estimated_pkt_size += ESTIMATE_VAR_SIZE(deliver_notification);
            }

            /* add in the current message number in this sequence */
            message_count++;
            if (message_count > MAX_MESSAGE_COUNT) {
                snmp_log(LOG_ERR, "delivery construct grew too large...  giving up\n");
                /* XXX: disable it */
                /* XXX: send a notification about it? */
                ITERATOR_RELEASE(iterator);
                return;
            }

            /* store this for later updating and sending */
            ready_for_delivery[message_count-1] = deliver_notification;

            if (!(obj->flags & NETSNMP_DELIVER_NO_MSG_COUNTS)) {
                snmp_varlist_add_variable(&deliver_notification,
                                          netsnmp_message_number_oid,
                                          netsnmp_message_number_oid_len,
                                          ASN_UNSIGNED,
                                          (const void *) &message_count,
                                          sizeof(message_count));
                estimated_pkt_size += ESTIMATE_VAR_SIZE(deliver_notification);

                /* add in the max message number count for this sequence */
                vartmp = snmp_varlist_add_variable(&deliver_notification,
                                                   netsnmp_max_message_number_oid,
                                                   netsnmp_max_message_number_oid_len,
                                                   ASN_UNSIGNED,
                                                   (const void *) &max_message_count,
                                                   sizeof(max_message_count));
                estimated_pkt_size += ESTIMATE_VAR_SIZE(deliver_notification);

                /* we'll need to update this counter later */
                max_message_count_ptrs[message_count-1] =
                    (u_long *) vartmp->val.integer;
            } else {
                /* just to be sure */
                max_message_count_ptrs[message_count-1] = NULL;
            }
            
            /* copy in the collected data */
            while(walker) {
                snmp_varlist_add_variable(&deliver_notification,
                                          walker->name, walker->name_length,
                                          walker->type,
                                          walker->val.string, walker->val_len);

                /* 8 byte padding for ASN encodings an a few extra OID bytes */
                estimated_pkt_size += ESTIMATE_VAR_SIZE(walker);

                walker = walker->next_variable;

                /* if the current size PLUS the next one (which is now
                   in 'walker') is greater than the limet then we stop here */
                if (obj->max_packet_size > 0 &&
                    estimated_pkt_size +
                    ESTIMATE_VAR_SIZE(walker) >=
                    obj->max_packet_size) {
                    break;
                }
            }

            /* send out the notification */
            send_v2trap(deliver_notification);
        }

        for(i = 0; i < message_count; i++) {
            /* update the max count pointer */
            if (max_message_count_ptrs[i])
                *(max_message_count_ptrs[i]) = message_count;
            
            send_v2trap(ready_for_delivery[i]);
            snmp_free_varbind(ready_for_delivery[i]);
        }

        snmp_free_varbind(vars);

        /* record this as the time processed */
        /* XXX: this may creep by a few seconds when processing and maybe we want
           to do the time stamp at the beginning? */
        obj->last_run = time(NULL);
    }
    ITERATOR_RELEASE(iterator);

    /* calculate the next time to sleep for */
    _schedule_next_execute_time();
}

int
calculate_time_until_next_run(deliver_by_notify *it, time_t *now) {
    time_t          local_now;

    /* if we weren't passed a valid time, fake it */
    if (NULL == now) {
        now = &local_now;
        time(&local_now);
    }

    netsnmp_assert_or_return(it->last_run != 0, -1);

    /* set the timestamp for the next run */
    it->next_run = it->last_run + it->frequency;

    /* how long until the next run? */
    return it->next_run - *now;
}

static void
_schedule_next_execute_time(void) {
    time_t             local_now = time(NULL);
    int                sleep_for = INT_MAX;
    int                next_time;
    netsnmp_iterator  *iterator;
    deliver_by_notify *obj;

    DEBUGMSGTL(("deliverByNotify", "Calculating scheduling needed\n"));

    if (alarm_reg) {
        snmp_alarm_unregister(alarm_reg);
        alarm_reg = 0;
    }

    iterator = CONTAINER_ITERATOR(deliver_container);
    if (NULL == iterator)
        return;

    for(obj = ITERATOR_FIRST(iterator); obj;
        obj = ITERATOR_NEXT(iterator)) {
        next_time = calculate_time_until_next_run(obj, &local_now);
        DEBUGMSGTL(("deliverByNotify", "  obj: %d (last=%d, next_run=%d)\n", next_time, obj->last_run, obj->next_run));
        if (next_time < sleep_for)
            sleep_for = next_time;
    }

    if (sleep_for != INT_MAX) {
        if (sleep_for < 1)
            sleep_for = 1; /* give at least a small pause */
        DEBUGMSGTL(("deliverByNotify", "Next execution in %d (max = %d) seconds\n",
                    sleep_for, INT_MAX));
        alarm_reg = snmp_alarm_register(sleep_for, 0, &deliver_execute, NULL);
    }

    ITERATOR_RELEASE(iterator);
}

