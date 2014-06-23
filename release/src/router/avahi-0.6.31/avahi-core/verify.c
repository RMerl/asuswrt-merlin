#include <string.h>
#include <stdio.h>

#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "log.h"
#include "rr-util.h"

#include "verify.h"

static void check_established (AvahiSEntryGroup *group) {
    assert(group);
    assert(group->type == AVAHI_GROUP_LLMNR);

    /* Check for the state */
    if (group->proto.llmnr.n_verifying == 0) 
        avahi_s_entry_group_change_state(group, AVAHI_ENTRY_GROUP_LLMNR_ESTABLISHED);
}

/* Decrease the n_verifying counter and free the 'lq' object */
static void decrease_n_verify(AvahiLLMNREntryVerify *ev) {
    assert(ev);
    assert(ev->e->group->type == AVAHI_GROUP_LLMNR);

    /* 
     * 1. Group is new and presently the state is verifying and decreasing the counter
     *    may enter the group in established state.
     * 2. Group is already established and we are registering a new entry, we should have 
     *    n_verifying greater than zero 
     */
    assert((ev->e->group->state == AVAHI_ENTRY_GROUP_LLMNR_VERIFYING) ||
           (ev->e->group->proto.llmnr.n_verifying > 0));

    --ev->e->group->proto.llmnr.n_verifying;
    check_established(ev->e->group);
}

static void withdraw_llmnr_entry(AvahiServer *s, AvahiEntry *e) {
    assert(s);
    assert(e);
    assert(e->type == AVAHI_ENTRY_LLMNR);

    /* Withdraw the specified entry, and if is part of an entry group,
     * put that into LLMNR_COLLISION state */
    if (e->dead) 
        return;

    if (e->group) {
        AvahiEntry *k;       
        assert(e->group->type == AVAHI_GROUP_LLMNR);

        /* Withdraw all entries of that group */
        for (k = e->group->entries; k; k = k->by_group_next) {
            assert(k->type == AVAHI_ENTRY_LLMNR);
            if (!k->dead) 
                k->dead = 1;
            }

            e->group->proto.llmnr.n_verifying = 0;
            avahi_s_entry_group_change_state(e->group, AVAHI_ENTRY_GROUP_LLMNR_COLLISION);
        } else 
            e->dead = 1;

    s->llmnr.need_entry_cleanup = 1;
    return;
}

/* This function is called when 'AvahiLLMNRQuery' has been processed completely.
Based on the data we get in 'userdata', state of the 'AvahiLLMNREntryVerifier' is decided*/
static void query_callback(
    AvahiIfIndex idx,
    AvahiProtocol protocol,
    AvahiRecord *r,
    void *userdata) {
    /* Get AvahiLLMNREntryVerify object, responder address and 'T' bit info, if any*/

    AvahiVerifierData *vdata = userdata;
    int win, t_bit ;

    AvahiLLMNREntryVerify *ev;
    const AvahiAddress *address;

    win = 0;
    ev = (vdata->ev);
    address = (vdata->address);
    t_bit = (vdata->t_bit);

    /* May be by the time we come back in this function due to conflict in some other entry
    of the group this entry belongs to, this entry has already been put up in the dead state.*/
    if (ev->e->dead) 
        return;

    assert(AVAHI_IF_VALID(idx));
    assert(AVAHI_PROTO_VALID(protocol));
    assert(userdata);
    assert(ev->e->type == AVAHI_ENTRY_LLMNR);
    assert(!ev->e->dead);

    /* Start comparing values.*/
    if (ev->state == AVAHI_CONFLICT) {
        /* Set by handle_response function*/
        /* There is already a conflict about this key or name is not UNIQUE */
        /* Withdraw this Entry*/
        withdraw_llmnr_entry(ev->s, ev->e);
    } else {
        if (!r && !address) {
            /* No record found for this key. Our name is unique. */
            /*char *t;
            t = avahi_record_to_string(ev->e->record);
            avahi_log_info("LLMNR Entry Verified\n %s on interface index (%d) and protocol : %s",
                t, 
                ev->interface->hardware->index, 
                avahi_proto_to_string(ev->interface->protocol));*/

            ev->state = AVAHI_VERIFIED;

            if (ev->e->group) 
                decrease_n_verify(ev);
        } else {
            /* We may have two cases now !r && address || r && !address
            r && !address when T bit is clear
            !r && address when T bit is set */

            /*  So check 'T' bit first*/
            if (!(t_bit)) {
                /* Assert that we had a response for this key */
                assert(r);

                /*Another host has already claimed for this name and is using it.*/
                ev->state = AVAHI_CONFLICT;

                /* withdraw this entry */
                withdraw_llmnr_entry(ev->s, ev->e);

            } else {
                /* 'T' bit is set. Now we compare two IP addresses lexicographically*/
                assert(protocol == address->proto);

                /* Source IP address = which is being used by interface to join LLMNR group
                We compare that address and responder address */
                assert(ev->interface->llmnr.llmnr_joined);

                if ( (protocol == AVAHI_PROTO_INET) && 
                    (memcmp(&(ev->interface->llmnr.local_llmnr_address.data.ipv4.address), &(address->data.ipv4.address), sizeof(AvahiIPv4Address))) )
                    win = 1;
                else /*( protocol == AVAHI_PROTO_INET6) */
                    if (memcmp(&(ev->interface->llmnr.local_llmnr_address.data.ipv6.address), &(address->data.ipv6.address), sizeof(AvahiIPv6Address)))
                    win = 1;

                if (win) {
                    /* We can claim this name */
                    ev->state = AVAHI_ESTABLISHED;
                    decrease_n_verify(ev);
                } else {
                    ev->state = AVAHI_CONFLICT;
                    withdraw_llmnr_entry(ev->s, ev->e);
                }
            }
        }
    }

    ev->lq = NULL;
    return;
}

static void remove_verifier(AvahiServer *s, AvahiLLMNREntryVerify *ev) {
    assert(s);
    assert(ev);
    assert(ev->e->type == AVAHI_ENTRY_LLMNR);

    if (ev->lq)
        avahi_llmnr_query_scheduler_withdraw_by_id(ev->interface->llmnr.query_scheduler, ev->lq->post_id);

    AVAHI_LLIST_REMOVE(AvahiLLMNREntryVerify, by_interface, ev->interface->llmnr.verifiers, ev);
    AVAHI_LLIST_REMOVE(AvahiLLMNREntryVerify, by_entry, ev->e->proto.llmnr.verifiers, ev);

    avahi_free(ev);
}

static AvahiLLMNREntryVerify *get_verifier(AvahiServer *s, AvahiInterface *i, AvahiEntry *e) {
    AvahiLLMNREntryVerify *ev;

    assert(s);
    assert(i);
    assert(e);
    assert(e->type == AVAHI_ENTRY_LLMNR);

    for (ev = e->proto.llmnr.verifiers; ev; ev = ev->by_entry_next) {
        if (ev->interface == i) 
            return ev;
    }

    return NULL;
}

static void set_state(AvahiLLMNREntryVerify *ev) {
    AvahiEntry *e;
    assert(ev);

    e = ev->e;
    assert(e->type == AVAHI_ENTRY_LLMNR);

    if ((e->flags & AVAHI_PUBLISH_UNIQUE) && !(e->flags & AVAHI_PUBLISH_NO_VERIFY))
        ev->state = AVAHI_VERIFYING;
    else
        ev->state = AVAHI_ESTABLISHED;

    if (ev->state == AVAHI_VERIFYING) {
        /* Structure to send in AvahiLLMNRQuery*/
        struct AvahiVerifierData *vdata;

        vdata = avahi_new(AvahiVerifierData, 1);
        /* Fill AvahiLLMNREntryVerify*/
        vdata->ev = ev;

        /* Both these fields are filled by handle_response*/
        vdata->address = NULL;

        /* If we get the response with t bit clear we don't touch this entry*/
        /* If we get the response with t bit set we set it so bt edfault keep it zero.*/
        vdata->t_bit = 0;

        /* Initiate Query */
        ev->lq = avahi_llmnr_query_add(ev->interface, e->record->key, AVAHI_LLMNR_UNIQUENESS_VERIFICATION_QUERY, query_callback, vdata);

        /* Increase n_verify */
        if (e->group) {
            assert(e->group->type == AVAHI_GROUP_LLMNR);
            e->group->proto.llmnr.n_verifying++;
        }

    } else { /*ev->state == AVAHI_ESTABLISHED */
        if (ev->e->group)
            check_established(ev->e->group);
    }
}

static void new_verifier(AvahiServer *s, AvahiInterface *i, AvahiEntry *e) {
    AvahiLLMNREntryVerify *ev;

    assert(s);
    assert(i);
    assert(e);
    assert(!e->dead);
    assert(e->type == AVAHI_ENTRY_LLMNR);  

    if ( !avahi_interface_match (i, e->interface, e->protocol) || 
        !i->llmnr.verifying || 
        !avahi_entry_is_commited(e) )
        /* start verifying rr's only when group has been commited */
        return;

    if (get_verifier(s, i, e))
        return;

    if (!(ev = avahi_new(AvahiLLMNREntryVerify, 1)))
        return;

    ev->s= s;
    ev->interface = i;
    ev->e = e;
    ev->lq = NULL;

    AVAHI_LLIST_PREPEND(AvahiLLMNREntryVerify, by_interface, i->llmnr.verifiers, ev);
    AVAHI_LLIST_PREPEND(AvahiLLMNREntryVerify, by_entry, e->proto.llmnr.verifiers, ev);

    set_state(ev);
}

void avahi_verify_interface(AvahiServer *s, AvahiInterface *i) {   
    AvahiEntry *e;

    assert(s);
    assert(i);

    if (!i->llmnr.verifying)
        return;

    for (e = s->llmnr.entries; e;e =  e->entries_next)
        if (!e->dead)
            new_verifier(s, i, e);
}

static void verify_entry_walk_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void * userdata) {
    AvahiEntry *e = userdata;

    assert(m);
    assert(i);  
    assert(e);
    assert(!e->dead);

    new_verifier(m->server, i, e);
}

void avahi_verify_entry(AvahiServer *s, AvahiEntry *e) {
    assert(s);
    assert(e);
    assert((!e->dead) && (e->type == AVAHI_ENTRY_LLMNR));

    avahi_interface_monitor_walk(s->monitor, e->interface, e->protocol, verify_entry_walk_callback, e);
}

void avahi_verify_group(AvahiServer *s, AvahiSEntryGroup *g) {
    AvahiEntry *e;

    assert(s);
    assert(g);
    assert(g->type == AVAHI_GROUP_LLMNR);

    for (e = g->entries; e; e = e->by_group_next) {
        if (!e->dead)
            avahi_verify_entry(s, e);
    }
}

int avahi_llmnr_entry_is_verifying(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) { 
    AvahiLLMNREntryVerify *ev;

    assert(s);
    assert(e);
    assert(i);
    assert((!e->dead) && (e->type == AVAHI_ENTRY_LLMNR));

    if ( !(ev = get_verifier(s, i, e)) ||
        (ev->state == AVAHI_CONFLICT))
        return -1;

    if (ev->state == AVAHI_VERIFYING)
        return 1;

    assert(ev->state == AVAHI_VERIFIED);

    return 0;
}

void avahi_llmnr_entry_return_to_initial_state(AvahiServer *s, AvahiEntry *e, AvahiInterface *i) {
    AvahiLLMNREntryVerify *ev;

    assert(s);
    assert(e);
    assert(i);
    assert((!e->dead) && (e->type == AVAHI_ENTRY_LLMNR));

    if (!(ev = get_verifier(s, i, e)))
        return;

    if (ev->state == AVAHI_VERIFYING) {
        if (ev->e->group) {
            avahi_llmnr_query_destroy(ev->lq);
            ev->lq = NULL;

            assert(ev->e->group->type == AVAHI_GROUP_LLMNR);
            ev->e->group->proto.llmnr.n_verifying--;
        }
    }

    set_state(ev);
}

static void avahi_reverify(AvahiLLMNREntryVerify *ev) {
    AvahiEntry *e;
    AvahiVerifierData *vdata;

    assert(ev);

    if (!(vdata = avahi_new(AvahiVerifierData, 1)))
        return;

    e = ev->e;

    if (e->group)
        assert(e->group->type == AVAHI_GROUP_LLMNR);

    /* Group has not been commited yet, nothing to reverify*/
    if (e->group && (e->group->state = AVAHI_ENTRY_GROUP_LLMNR_UNCOMMITED || e->group->state == AVAHI_ENTRY_GROUP_LLMNR_COLLISION))
        return;

    /* STATE == AVAHI_VERIFYING : Free the lq object and if entry belongs to
    a group decrease the n_verifying counter. NEW STATE : _VERIFYING*/
    if (ev->state == AVAHI_VERIFYING) {
        if (e->group)
            e->group->proto.llmnr.n_verifying--;

    } else if ((ev->state == AVAHI_LLMNR_ESTABLISHED) && (e->flags & AVAHI_PUBLISH_UNIQUE) && !(e->flags & AVAHI_PUBLISH_NO_VERIFY))
        /* _ESTABLISHED but _VERIFY again*/
        ev->state = AVAHI_VERIFYING;

    else
        ev->state = AVAHI_ESTABLISHED;

    /* New state has been decided */

    if (ev->state == AVAHI_VERIFYING) {

        vdata->ev = ev;
        vdata->address = NULL;
        vdata->t_bit = 0;

        /* Start the queries */
        avahi_llmnr_query_add(ev->interface, e->record->key, AVAHI_LLMNR_UNIQUENESS_VERIFICATION_QUERY, query_callback, vdata);

        /* Increase the counter if it belongs to a group */
        if (e->group) 
            e->group->proto.llmnr.n_verifying++;

    } else 
        if (e->group)
            check_established(e->group);
}

static void reannounce_walk_callback(AvahiInterfaceMonitor *m, AvahiInterface *i, void* userdata) {
    AvahiEntry *e = userdata;
    AvahiLLMNREntryVerify *ev;

    assert(m);
    assert(i);
    assert(e);
    assert((!e->dead) && (e->type == AVAHI_ENTRY_LLMNR));

    if (!(ev = get_verifier(m->server, i, e)))
        return;

    avahi_reverify(ev);
}

void avahi_reverify_entry(AvahiServer *s, AvahiEntry *e) {

    assert(s);
    assert(e);
    assert(!e->dead && e->type == AVAHI_ENTRY_LLMNR);

    avahi_interface_monitor_walk(s->monitor, e->interface, e->protocol, reannounce_walk_callback, e);
}

void avahi_remove_verifiers(AvahiServer *s, AvahiEntry *e) {
    assert(s);
    assert(e);
    assert(e->type == AVAHI_ENTRY_LLMNR);

    while (e->proto.llmnr.verifiers)
        remove_verifier(s, e->proto.llmnr.verifiers);
}

AvahiLLMNREntryVerifyState avahi_llmnr_entry_verify_state(AvahiLLMNREntryVerify *ev) {
    assert(ev);

    return (ev->state);
}

void avahi_remove_interface_verifiers(AvahiServer *s, AvahiInterface *i) {
    assert(s);
    assert(i);

    while (i->llmnr.verifiers)
        remove_verifier(s, i->llmnr.verifiers);
}

