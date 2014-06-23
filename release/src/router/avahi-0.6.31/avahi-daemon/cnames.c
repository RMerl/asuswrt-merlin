/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <avahi-common/llist.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-core/log.h>
#include <avahi-core/publish.h>

#include "main.h"
#include "cnames.h"

typedef struct CName CName;

struct CName {
    AvahiSEntryGroup *group;
    int iteration;

    char *cname;

    int publish_proto;	/*0/mDNS/LLMNR*/

    AVAHI_LLIST_FIELDS(CName, cnames);
};

static AVAHI_LLIST_HEAD(CName, cnames) = NULL;
static int current_iteration = 0;

static void add_cname_to_server(CName *c);
static void remove_cname_from_server(CName *c);

static void entry_group_callback(AvahiServer *s, AVAHI_GCC_UNUSED AvahiSEntryGroup *eg, AvahiEntryGroupState state, void* userdata) {
    CName *c;

    assert(s);
    assert(eg);

    c = userdata;

    switch (state) {

        case AVAHI_ENTRY_GROUP_COLLISION:
            avahi_log_error("Conflict for alias \"%s\", not established.", c->cname);
            break;

        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            avahi_log_notice ("Alias \"%s\" successfully established.", c->cname);
            break;

        case AVAHI_ENTRY_GROUP_FAILURE:
            avahi_log_notice ("Failed to establish alias \"%s\": %s.", c->cname, avahi_strerror (avahi_server_errno (s)));
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

static CName *cname_new(void) {
    CName *c;

    c = avahi_new(CName, 1);

    c->group = NULL;
    c->cname = NULL;
    c->iteration = current_iteration;

    AVAHI_LLIST_PREPEND(CName, cnames, cnames, c);

    return c;
}

static void cname_free(CName *c) {
    assert(c);

    AVAHI_LLIST_REMOVE(CName, cnames, cnames, c);

    if (c->group)
        avahi_s_entry_group_free(c->group);

    avahi_free(c->cname);

    avahi_free(c);
}

static CName *cname_find(const char *name) {
    CName *c;

    assert(name);

    for (c = cnames; c; c = c->cnames_next)
        if (!strcmp(c->cname, name))
            return c;

    return NULL;
}

static void add_cname_to_server(CName *c)
{
            avahi_log_error("add_cname_to_server start.");
    if (!c->group)
        if (!(c->group = avahi_s_entry_group_new (avahi_server, entry_group_callback, c))) {
            avahi_log_error("avahi_s_entry_group_new() failed: %s", avahi_strerror(avahi_server_errno(avahi_server)));
            return;
        }

    if (avahi_s_entry_group_is_empty(c->group)) {
        int err;

	c->publish_proto = AVAHI_PUBLISH_USE_MULTICAST;

        if ((err = avahi_server_add_cname(avahi_server, c->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, c->publish_proto, AVAHI_DEFAULT_TTL_HOST_NAME, c->cname)) < 0) {
            avahi_log_error ("Alias %s: avahi_server_add_cname failure: %s", c->cname, avahi_strerror(err));
            return;
        }

        avahi_s_entry_group_commit (c->group);
    }
}

static void remove_cname_from_server(CName *c)
{
    if (c->group)
        avahi_s_entry_group_reset (c->group);
}

void cnames_add_to_server(void) {
    CName *c;

    for (c = cnames; c; c = c->cnames_next){
            avahi_log_error("cnames_add_to_server start.");
        add_cname_to_server(c);
    }
}

void cnames_remove_from_server(void) {
    CName *c;

    for (c = cnames; c; c = c->cnames_next)
        remove_cname_from_server(c);
}

void cnames_register(char **aliases) {
    CName *c, *next;
    char **name;

    current_iteration++;

    for (name = aliases; name && *name; name++) {
        if (!(c = cname_find(*name))) {
            c = cname_new();
            c->cname = *name;

            avahi_log_info("Loading new alias %s.", c->cname);
        }

        c->iteration = current_iteration;
    }

    for (c = cnames; c; c = next) {
        next = c->cnames_next;

        if (c->iteration != current_iteration) {
            avahi_log_info("Alias %s vanished, removing.", c->cname);
            cname_free(c);
        }
    }
}

void cnames_free_all (void)
{
    while(cnames)
        cname_free(cnames);
}

typedef struct LLMNR_CName LLMNR_CName;

struct LLMNR_CName {
    AvahiSEntryGroup *llmnr_group;
    int iteration;

    char *llmnr_cname;

    int publish_proto;	/*0/mDNS/LLMNR*/

    AVAHI_LLIST_FIELDS(LLMNR_CName, llmnr_cnames);
};

static AVAHI_LLIST_HEAD(LLMNR_CName, llmnr_cnames) = NULL;
static int llmnr_current_iteration = 0;

static void add_llmnr_cname_to_server(LLMNR_CName *lc);
static void remove_llmnr_cname_from_server(LLMNR_CName *lc);

static void llmnr_entry_group_callback(AvahiServer *s, AVAHI_GCC_UNUSED AvahiSEntryGroup *eg, AvahiEntryGroupState state, void* userdata) {
    LLMNR_CName *lc;

    assert(s);
    assert(eg);

    lc = userdata;

    switch (state) {

        case AVAHI_ENTRY_GROUP_COLLISION:
            avahi_log_error("Conflict for llmnr alias \"%s\", not established.", lc->llmnr_cname);
            break;

        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            avahi_log_notice ("LLMNR Alias \"%s\" successfully established.", lc->llmnr_cname);
            break;

        case AVAHI_ENTRY_GROUP_FAILURE:
            avahi_log_notice ("Failed to establish llmnr alias \"%s\": %s.", lc->llmnr_cname, avahi_strerror (avahi_server_errno (s)));
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

static LLMNR_CName *llmnr_cname_new(void) {
    LLMNR_CName *lc;

    lc = avahi_new(LLMNR_CName, 1);

    lc->llmnr_group = NULL;
    lc->llmnr_cname = NULL;
    lc->iteration = llmnr_current_iteration;

    AVAHI_LLIST_PREPEND(LLMNR_CName, llmnr_cnames, llmnr_cnames, lc);

    return lc;
}

static void llmnr_cname_free(LLMNR_CName *lc) {
    assert(lc);

    AVAHI_LLIST_REMOVE(LLMNR_CName, llmnr_cnames, llmnr_cnames, lc);

    if (lc->llmnr_group)
        avahi_s_entry_group_free(lc->llmnr_group);

    avahi_free(lc->llmnr_cname);

    avahi_free(lc);
}

static LLMNR_CName *llmnr_cname_find(const char *llmnr_name) {
    LLMNR_CName *lc;

    assert(llmnr_name);

    for (lc = llmnr_cnames; lc; lc = lc->llmnr_cnames_next)
        if (!strcmp(lc->llmnr_cname, llmnr_name))
            return lc;

    return NULL;
}

static void add_llmnr_cname_to_server(LLMNR_CName *lc)
{
            avahi_log_error("add_llmnr_cname_to_server start.");
    if (!lc->llmnr_group)
        if (!(lc->llmnr_group = avahi_s_entry_group_new (avahi_server, llmnr_entry_group_callback, lc))) {
            avahi_log_error("avahi_s_llmnr_entry_group_new() failed: %s", avahi_strerror(avahi_server_errno(avahi_server)));
            return;
        }

    if (avahi_s_entry_group_is_empty(lc->llmnr_group)) {
        int err;

	lc->publish_proto = AVAHI_PUBLISH_USE_LLMNR;

        if ((err = avahi_server_add_llmnr_cname(avahi_server, lc->llmnr_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, lc->publish_proto, AVAHI_DEFAULT_TTL_HOST_NAME, lc->llmnr_cname)) < 0) {
            avahi_log_error ("Alias %s: avahi_server_add_llmnr_cname failure: %s", lc->llmnr_cname, avahi_strerror(err));
            return;
        }

        avahi_s_entry_group_commit (lc->llmnr_group);
    }
}

static void remove_llmnr_cname_from_server(LLMNR_CName *lc)
{
    if (lc->llmnr_group)
        avahi_s_entry_group_reset (lc->llmnr_group);
}

void llmnr_cnames_add_to_server(void) {
    LLMNR_CName *lc;

    for (lc = llmnr_cnames; lc; lc = lc->llmnr_cnames_next){
            avahi_log_error("llmnr_cnames_add_to_server start.");
        add_llmnr_cname_to_server(lc);
    }
}

void llmnr_cnames_remove_from_server(void) {
    LLMNR_CName *lc;

    for (lc = llmnr_cnames; lc; lc = lc->llmnr_cnames_next)
        remove_llmnr_cname_from_server(lc);
}

void llmnr_cnames_register(char **aliases_llmnr) {
    LLMNR_CName *lc, *llmnr_next;
    char **llmnr_name;

    llmnr_current_iteration++;

    for (llmnr_name = aliases_llmnr; llmnr_name && *llmnr_name; llmnr_name++) {
        if (!(lc = llmnr_cname_find(*llmnr_name))) {
            lc = llmnr_cname_new();
            lc->llmnr_cname = *llmnr_name;

            avahi_log_info("Loading new alias_llmnr %s.", lc->llmnr_cname);
        }

        lc->iteration = llmnr_current_iteration;
    }

    for (lc = llmnr_cnames; lc; lc = llmnr_next) {
        llmnr_next = lc->llmnr_cnames_next;

        if (lc->iteration != llmnr_current_iteration) {
            avahi_log_info("Alias_llmnr %s vanished, removing.", lc->llmnr_cname);
            llmnr_cname_free(lc);
        }
    }
}

void llmnr_cnames_free_all (void)
{
    while(llmnr_cnames)
        llmnr_cname_free(llmnr_cnames);
}

