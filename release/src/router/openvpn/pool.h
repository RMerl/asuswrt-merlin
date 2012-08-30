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

#ifndef POOL_H
#define POOL_H

#if P2MP

/*#define IFCONFIG_POOL_TEST*/

#include "basic.h"
#include "status.h"

#define IFCONFIG_POOL_MAX         65536
#define IFCONFIG_POOL_MIN_NETBITS    16

#define IFCONFIG_POOL_30NET   0
#define IFCONFIG_POOL_INDIV   1

struct ifconfig_pool_entry
{
  bool in_use;
  char *common_name;
  time_t last_release;
  bool fixed;
};

struct ifconfig_pool
{
  in_addr_t base;
  int size;
  int type;
  bool duplicate_cn;
  struct ifconfig_pool_entry *list;
};

struct ifconfig_pool_persist
{
  struct status_output *file;
  bool fixed;
};

typedef int ifconfig_pool_handle;

struct ifconfig_pool *ifconfig_pool_init (int type, in_addr_t start, in_addr_t end, const bool duplicate_cn);

void ifconfig_pool_free (struct ifconfig_pool *pool);

bool ifconfig_pool_verify_range (const int msglevel, const in_addr_t start, const in_addr_t end);

ifconfig_pool_handle ifconfig_pool_acquire (struct ifconfig_pool *pool, in_addr_t *local, in_addr_t *remote, const char *common_name);

bool ifconfig_pool_release (struct ifconfig_pool* pool, ifconfig_pool_handle hand, const bool hard);

struct ifconfig_pool_persist *ifconfig_pool_persist_init (const char *filename, int refresh_freq);
void ifconfig_pool_persist_close (struct ifconfig_pool_persist *persist);
bool ifconfig_pool_write_trigger (struct ifconfig_pool_persist *persist);

void ifconfig_pool_read (struct ifconfig_pool_persist *persist, struct ifconfig_pool *pool);
void ifconfig_pool_write (struct ifconfig_pool_persist *persist, const struct ifconfig_pool *pool);

#ifdef IFCONFIG_POOL_TEST
void ifconfig_pool_test (in_addr_t start, in_addr_t end);
#endif

#endif
#endif
