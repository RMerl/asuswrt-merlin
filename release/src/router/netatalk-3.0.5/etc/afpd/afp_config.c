/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/dsi.h>
#include <atalk/afp.h>
#include <atalk/compat.h>
#include <atalk/server_child.h>
#include <atalk/globals.h>
#include <atalk/errchk.h>
#include <atalk/netatalk_conf.h>
#include <atalk/fce_api.h>

#ifdef HAVE_LDAP
#include <atalk/ldapconfig.h>
#endif

#include "afp_config.h"
#include "uam_auth.h"
#include "status.h"
#include "volume.h"
#include "afp_zeroconf.h"

/*!
 * Free and cleanup config and DSI
 *
 * "dsi" can be NULL in which case all DSI objects and the config object is freed,
 * otherwise its an afpd session child and only any unneeded DSI objects are freed
 */
void configfree(AFPObj *obj, DSI *dsi)
{
    DSI *p, *q;

    if (!dsi) {
        /* Master afpd reloading config */
        auth_unload();
        if (! (obj->options.flags & OPTION_NOZEROCONF)) {
            zeroconf_deregister();
        }
    }

    unload_volumes(obj);

    /* Master and child releasing unneeded DSI handles */
    for (p = obj->dsi; p; p = q) {
        q = p->next;
        if (p == dsi)
            continue;
        dsi_free(p);
        free(p);
    }
    obj->dsi = NULL;

    /* afpd session child passes dsi handle to obj handle */
    if (dsi) {
        dsi->next = NULL;
        obj->dsi = dsi;
    }
}

/*!
 * Get everything running
 */
int configinit(AFPObj *obj)
{
    EC_INIT;
    DSI *dsi = NULL;
    DSI **next = &obj->dsi;
    char *p = NULL, *q = NULL, *savep;
    const char *r;
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    static char interfaddr[NI_MAXHOST];

    auth_load(obj->options.uampath, obj->options.uamlist);
    set_signature(&obj->options);
#ifdef HAVE_LDAP
    acl_ldap_freeconfig();
#endif /* HAVE_LDAP */

    LOG(log_debug, logtype_afpd, "DSIConfigInit: hostname: %s, listen: %s, interfaces: %s, port: %s",
        obj->options.hostname,
        obj->options.listen ? obj->options.listen : "-",
        obj->options.interfaces ? obj->options.interfaces : "-",
        obj->options.port);

    /*
     * Setup addresses we listen on from hostname and/or "afp listen" option
     */
    if (obj->options.listen) {
        EC_NULL( q = p = strdup(obj->options.listen) );
        EC_NULL( p = strtok_r(p, ", ", &savep) );
        while (p) {
            if ((dsi = dsi_init(obj, obj->options.hostname, p, obj->options.port)) == NULL)
                break;

            status_init(obj, dsi);
            *next = dsi;
            next = &dsi->next;
            dsi->AFPobj = obj;

            LOG(log_note, logtype_afpd, "Netatalk AFP/TCP listening on %s:%d",
                getip_string((struct sockaddr *)&dsi->server),
                getip_port((struct sockaddr *)&dsi->server));

            p = strtok_r(NULL, ", ", &savep);
        }
        if (q) {
            free(q);
            q = NULL;
        }
    }

   /*
    * Setup addresses we listen on from "afp interfaces".
    * We use getifaddrs() instead of if_nameindex() because the latter appears still
    * to be unable to return ipv4 addresses
    */
    if (obj->options.interfaces) {
#ifndef HAVE_GETIFADDRS
        LOG(log_error, logtype_afpd, "option \"afp interfaces\" not supported");
#else
        if (getifaddrs(&ifaddr) == -1) {
            LOG(log_error, logtype_afpd, "getinterfaddr: getifaddrs() failed: %s", strerror(errno));
            EC_FAIL;
        }

        EC_NULL( q = p = strdup(obj->options.interfaces) );
        EC_NULL( p = strtok_r(p, ", ", &savep) );
        while (p) {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL)
                    continue;
                if (STRCMP(ifa->ifa_name, !=, p))
                    continue;

                family = ifa->ifa_addr->sa_family;
                if (family == AF_INET || family == AF_INET6) {
                    if (getnameinfo(ifa->ifa_addr,
                                    (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                    interfaddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0) {
                        LOG(log_error, logtype_afpd, "getinterfaddr: getnameinfo() failed %s", gai_strerror(errno));
                        continue;
                    }

                    if ((dsi = dsi_init(obj, obj->options.hostname, interfaddr, obj->options.port)) == NULL)
                        continue;

                    status_init(obj, dsi);
                    *next = dsi;
                    next = &dsi->next;
                    dsi->AFPobj = obj;

                    LOG(log_note, logtype_afpd, "Netatalk AFP/TCP listening on interface %s with address %s:%d",
                        p,
                        getip_string((struct sockaddr *)&dsi->server),
                        getip_port((struct sockaddr *)&dsi->server));
                } /* if (family == AF_INET || family == AF_INET6) */
            } /* for (ifa != NULL) */
            p = strtok_r(NULL, ", ", &savep);
        }
        freeifaddrs(ifaddr);
#endif
    }

    /*
     * Check whether we got a valid DSI from options.listen or options.interfaces,
     * if not add a DSI that accepts all connections and goes though the list of
     * network interaces for determining an IP we can advertise in DSIStatus
     */
    if (dsi == NULL) {
        if ((dsi = dsi_init(obj, obj->options.hostname, NULL, obj->options.port)) == NULL)
            EC_FAIL_LOG("no suitable network address found, use \"afp listen\" or \"afp interfaces\"", 0);
        status_init(obj, dsi);
        *next = dsi;
        next = &dsi->next;
        dsi->AFPobj = obj;

        LOG(log_note, logtype_afpd, "Netatalk AFP/TCP listening on %s:%d",
            getip_string((struct sockaddr *)&dsi->server),
            getip_port((struct sockaddr *)&dsi->server));
    }

#ifdef HAVE_LDAP
    /* Parse afp.conf */
    acl_ldap_readconfig(obj->iniconfig);
#endif /* HAVE_LDAP */

    /* Now register with zeroconf, we also need the volumes for that */
    if (! (obj->options.flags & OPTION_NOZEROCONF)) {
        load_volumes(obj);
        zeroconf_register(obj);
    }

    if ((r = atalk_iniparser_getstring(obj->iniconfig, INISEC_GLOBAL, "fce listener", NULL))) {
		LOG(log_note, logtype_afpd, "Adding FCE listener: %s", r);
		fce_add_udp_socket(r);
    }
    if ((r = atalk_iniparser_getstring(obj->iniconfig, INISEC_GLOBAL, "fce coalesce", NULL))) {
		LOG(log_note, logtype_afpd, "Fce coalesce: %s", r);
		fce_set_coalesce(r);
    }
    if ((r = atalk_iniparser_getstring(obj->iniconfig, INISEC_GLOBAL, "fce events", NULL))) {
		LOG(log_note, logtype_afpd, "Fce events: %s", r);
		fce_set_events(r);
    }

EC_CLEANUP:
    if (q)
        free(q);
    EC_EXIT;
}
