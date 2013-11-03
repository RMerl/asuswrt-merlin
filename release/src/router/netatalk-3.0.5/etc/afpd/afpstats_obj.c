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

#include <strings.h>
#include <pwd.h>

#include <glib-object.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <atalk/logger.h>
#include <atalk/dsi.h>

#include "afpstats.h"
#include "afpstats_obj.h"

struct AFPStatsObj
{
  GObject parent;
};

struct AFPStatsObjClass
{
  GObjectClass parent;
};

static void afpstats_obj_init(AFPStatsObj *obj)
{
}

static void afpstats_obj_class_init(AFPStatsObjClass *klass)
{
}

static gpointer afpstats_obj_parent_class = NULL;

static void afpstats_obj_class_intern_init(gpointer klass)
{
	afpstats_obj_parent_class = g_type_class_peek_parent(klass);
	afpstats_obj_class_init((AFPStatsObjClass *)klass);
}

GType afpstats_obj_get_type(void)
{
	static volatile gsize g_define_type_id__volatile = 0;
	if (g_once_init_enter(&g_define_type_id__volatile)) {
		GType g_define_type_id = g_type_register_static_simple(
            G_TYPE_OBJECT,
            g_intern_static_string("AFPStatsObj"),
            sizeof(AFPStatsObjClass),
            (GClassInitFunc)afpstats_obj_class_intern_init,
			sizeof(AFPStatsObj),
			(GInstanceInitFunc)afpstats_obj_init,
			(GTypeFlags)0);
		g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
	}
	return g_define_type_id__volatile;
}

gboolean afpstats_obj_get_users(AFPStatsObj *obj, gchar ***ret, GError **error)
{
    gchar **names;
    server_child_t *childs = afpstats_get_and_lock_childs();
    afp_child_t *child;
    struct passwd *pw;
    int i = 0, j;
    char buf[256];

    names = g_new(char *, childs->servch_count + 1);

    for (j = 0; j < CHILD_HASHSIZE && i < childs->servch_count; j++) {
        child = childs->servch_table[j];
        while (child) {
            if (child->afpch_valid && (pw = getpwuid(child->afpch_uid))) {
                time_t time = child->afpch_logintime;
                strftime(buf, sizeof(buf), "%b %d %H:%M:%S", localtime(&time));
                names[i++] = g_strdup_printf("name: %s, pid: %d, logintime: %s, state: %s, volumes: %s",
                                             pw->pw_name, child->afpch_pid, buf,
                                             child->afpch_state == DSI_RUNNING ? "active" :
                                             child->afpch_state == DSI_SLEEPING ? "sleeping" :
                                             child->afpch_state == DSI_EXTSLEEP ? "sleeping" :
                                             child->afpch_state == DSI_DISCONNECTED ? "disconnected" :
                                             "unknown",
                                             child->afpch_volumes ? child->afpch_volumes : "-"); 
            }
            child = child->afpch_next;
        }
    }
    names[i] = NULL;
    *ret = names;

    afpstats_unlock_childs();

    return TRUE;
}
