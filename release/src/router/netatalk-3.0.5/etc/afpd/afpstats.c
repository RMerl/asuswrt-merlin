/*
 * Copyright (c) 2013 Frank Lahm <franklahm@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <atalk/logger.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>
#include <atalk/server_child.h>

#include "afpstats_obj.h"
#include "afpstats_service_glue.h"

/*
 * Beware: this struct is accessed and modified from the main thread
 * and from this thread, thus be careful to lock and unlock the mutex.
 */
static server_child_t *childs;

static gpointer afpstats_thread(gpointer _data)
{
    DBusGConnection *bus;
    DBusGProxy *bus_proxy;
    GError *error = NULL;
    GMainContext *ctxt;
    GMainLoop *thread_loop;
    guint request_name_result;
    sigset_t sigs;

    /* Block all signals in this thread */
    sigfillset(&sigs);
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);

    ctxt = g_main_context_new();
    thread_loop = g_main_loop_new(ctxt, FALSE);

    dbus_g_object_type_install_info(AFPSTATS_TYPE_OBJECT, &dbus_glib_afpstats_obj_object_info);
   
    if (!(bus = dbus_g_bus_get_private(DBUS_BUS_SYSTEM, ctxt, &error))) {
        LOG(log_error, logtype_afpd,"Couldn't connect to system bus: %s", error->message);
        return NULL;
    }

    if (!(bus_proxy = dbus_g_proxy_new_for_name(bus, "org.freedesktop.DBus",
                                                "/org/freedesktop/DBus",
                                                "org.freedesktop.DBus"))) {
        LOG(log_error, logtype_afpd,"Couldn't create bus proxy");
        return NULL;
    }

    if (!dbus_g_proxy_call(bus_proxy, "RequestName", &error,
                           G_TYPE_STRING, "org.netatalk.AFPStats",
                           G_TYPE_UINT, 0,
                           G_TYPE_INVALID,
                           G_TYPE_UINT, &request_name_result,
                           G_TYPE_INVALID)) {
        LOG(log_error, logtype_afpd, "Failed to acquire DBUS name: %s", error->message);
        return NULL;
    }

    AFPStatsObj *obj = g_object_new(AFPSTATS_TYPE_OBJECT, NULL);
    dbus_g_connection_register_g_object(bus, "/org/netatalk/AFPStats", G_OBJECT(obj));

    g_main_loop_run(thread_loop);
    return thread_loop;
}

static void my_glib_log(const gchar *log_domain,
                        GLogLevelFlags log_level,
                        const gchar *message,
                        gpointer user_data)
{
    LOG(log_error, logtype_afpd, "%s: %s", log_domain, message);
}

server_child_t *afpstats_get_and_lock_childs(void)
{
    pthread_mutex_lock(&childs->servch_lock);
    return childs;
}

void afpstats_unlock_childs(void)
{
    pthread_mutex_unlock(&childs->servch_lock);
}

int afpstats_init(server_child_t *childs_in)
{
    GThread *thread;

    childs = childs_in;
    g_type_init();
    g_thread_init(NULL);
    dbus_g_thread_init();
    (void)g_log_set_default_handler(my_glib_log, NULL);

    thread = g_thread_create(afpstats_thread, NULL, TRUE, NULL);

    return 0;
}
