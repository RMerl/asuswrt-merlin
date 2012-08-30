/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2010 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * plug-in support, using dynamically loaded libraries
 */

#ifndef OPENVPN_PLUGIN_H
#define OPENVPN_PLUGIN_H

#include "openvpn-plugin.h"

#ifdef ENABLE_PLUGIN

#include "misc.h"

#define MAX_PLUGINS 16

struct plugin_option {
  const char *so_pathname;
  const char **argv;
};

struct plugin_option_list {
  int n;
  struct plugin_option plugins[MAX_PLUGINS];
};

struct plugin {
  bool initialized;
  const char *so_pathname;
  unsigned int plugin_type_mask;
  int requested_initialization_point;

#if defined(USE_LIBDL)
  void *handle;
#elif defined(USE_LOAD_LIBRARY)
  HMODULE module;
#endif

  openvpn_plugin_open_v1 open1;
  openvpn_plugin_open_v2 open2;
  openvpn_plugin_func_v1 func1;
  openvpn_plugin_func_v2 func2;
  openvpn_plugin_close_v1 close;
  openvpn_plugin_abort_v1 abort;
  openvpn_plugin_client_constructor_v1 client_constructor;
  openvpn_plugin_client_destructor_v1 client_destructor;
  openvpn_plugin_min_version_required_v1 min_version_required;
  openvpn_plugin_select_initialization_point_v1 initialization_point;

  openvpn_plugin_handle_t plugin_handle;
};

struct plugin_per_client
{
  void *per_client_context[MAX_PLUGINS];
};

struct plugin_common
{
  int n;
  struct plugin plugins[MAX_PLUGINS];
};

struct plugin_list
{
  struct plugin_per_client per_client;
  struct plugin_common *common;
  bool common_owned;
};

struct plugin_return
{
  int n;
  struct openvpn_plugin_string_list *list[MAX_PLUGINS];
};

struct plugin_option_list *plugin_option_list_new (struct gc_arena *gc);
bool plugin_option_list_add (struct plugin_option_list *list, char **p, struct gc_arena *gc);

#ifdef ENABLE_DEBUG
void plugin_option_list_print (const struct plugin_option_list *list, int msglevel);
#endif

struct plugin_list *plugin_list_init (const struct plugin_option_list *list);

void plugin_list_open (struct plugin_list *pl,
		       const struct plugin_option_list *list,
		       struct plugin_return *pr,
		       const struct env_set *es,
		       const int init_point);

struct plugin_list *plugin_list_inherit (const struct plugin_list *src);

int plugin_call (const struct plugin_list *pl,
		 const int type,
		 const struct argv *av,
		 struct plugin_return *pr,
		 struct env_set *es);

void plugin_list_close (struct plugin_list *pl);
bool plugin_defined (const struct plugin_list *pl, const int type);

void plugin_return_get_column (const struct plugin_return *src,
			       struct plugin_return *dest,
			       const char *colname);

void plugin_return_free (struct plugin_return *pr);

#ifdef ENABLE_DEBUG
void plugin_return_print (const int msglevel, const char *prefix, const struct plugin_return *pr);
#endif

static inline int
plugin_n (const struct plugin_list *pl)
{
  if (pl && pl->common)
    return pl->common->n;
  else
    return 0;
}

static inline bool
plugin_return_defined (const struct plugin_return *pr)
{
  return pr->n >= 0;
}

static inline void
plugin_return_init (struct plugin_return *pr)
{
  pr->n = 0;
}

#else

struct plugin_list { int dummy; };
struct plugin_return { int dummy; };

static inline bool
plugin_defined (const struct plugin_list *pl, const int type)
{
  return false;
}

static inline int
plugin_call (const struct plugin_list *pl,
	     const int type,
	     const struct argv *av,
	     struct plugin_return *pr,
	     struct env_set *es)
{
  return 0;
}

#endif /* ENABLE_PLUGIN */

#endif /* OPENVPN_PLUGIN_H */
