/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"
#include "memory.h"

#include "pimd.h"
#include "pim_cmd.h"
#include "pim_iface.h"
#include "pim_zebra.h"
#include "pim_str.h"
#include "pim_oil.h"
#include "pim_pim.h"
#include "pim_upstream.h"
#include "pim_rand.h"
#include "pim_rpf.h"
#include "pim_ssmpingd.h"

const char *const PIM_ALL_SYSTEMS      = MCAST_ALL_SYSTEMS;
const char *const PIM_ALL_ROUTERS      = MCAST_ALL_ROUTERS;
const char *const PIM_ALL_PIM_ROUTERS  = MCAST_ALL_PIM_ROUTERS;
const char *const PIM_ALL_IGMP_ROUTERS = MCAST_ALL_IGMP_ROUTERS;

struct thread_master     *master = 0;
uint32_t                  qpim_debugs = 0;
int                       qpim_mroute_socket_fd = -1;
int64_t                   qpim_mroute_socket_creation = 0; /* timestamp of creation */
struct thread            *qpim_mroute_socket_reader = 0;
int                       qpim_mroute_oif_highest_vif_index = -1;
struct list              *qpim_channel_oil_list = 0;
int                       qpim_t_periodic = PIM_DEFAULT_T_PERIODIC; /* Period between Join/Prune Messages */
struct list              *qpim_upstream_list = 0;
struct zclient           *qpim_zclient_update = 0;
struct zclient           *qpim_zclient_lookup = 0;
struct pim_assert_metric  qpim_infinite_assert_metric;
long                      qpim_rpf_cache_refresh_delay_msec = 10000;
struct thread            *qpim_rpf_cache_refresher = 0;
int64_t                   qpim_rpf_cache_refresh_requests = 0;
int64_t                   qpim_rpf_cache_refresh_events = 0;
int64_t                   qpim_rpf_cache_refresh_last =  0;
struct in_addr            qpim_inaddr_any;
struct list              *qpim_ssmpingd_list = 0;
struct in_addr            qpim_ssmpingd_group_addr;
int64_t                   qpim_scan_oil_events = 0;
int64_t                   qpim_scan_oil_last = 0;
int64_t                   qpim_mroute_add_events = 0;
int64_t                   qpim_mroute_add_last = 0;
int64_t                   qpim_mroute_del_events = 0;
int64_t                   qpim_mroute_del_last = 0;

static void pim_free()
{
  pim_ssmpingd_destroy();

  if (qpim_channel_oil_list)
    list_free(qpim_channel_oil_list);

  if (qpim_upstream_list)
    list_free(qpim_upstream_list);
}

void pim_init()
{
  pim_rand_init();

  if (!inet_aton(PIM_ALL_PIM_ROUTERS, &qpim_all_pim_routers_addr)) {
    zlog_err("%s %s: could not solve %s to group address: errno=%d: %s",
	     __FILE__, __PRETTY_FUNCTION__,
	     PIM_ALL_PIM_ROUTERS, errno, safe_strerror(errno));
    zassert(0);
    return;
  }

  qpim_channel_oil_list = list_new();
  if (!qpim_channel_oil_list) {
    zlog_err("%s %s: failure: channel_oil_list=list_new()",
	     __FILE__, __PRETTY_FUNCTION__);
    return;
  }
  qpim_channel_oil_list->del = (void (*)(void *)) pim_channel_oil_free;

  qpim_upstream_list = list_new();
  if (!qpim_upstream_list) {
    zlog_err("%s %s: failure: upstream_list=list_new()",
	     __FILE__, __PRETTY_FUNCTION__);
    pim_free();
    return;
  }
  qpim_upstream_list->del = (void (*)(void *)) pim_upstream_free;

  qpim_mroute_socket_fd = -1; /* mark mroute as disabled */
  qpim_mroute_oif_highest_vif_index = -1;

  zassert(!qpim_debugs);
  zassert(!PIM_MROUTE_IS_ENABLED);

  qpim_inaddr_any.s_addr = PIM_NET_INADDR_ANY;

  /*
    RFC 4601: 4.6.3.  Assert Metrics

    assert_metric
    infinite_assert_metric() {
    return {1,infinity,infinity,0}
    }
  */
  qpim_infinite_assert_metric.rpt_bit_flag      = 1;
  qpim_infinite_assert_metric.metric_preference = PIM_ASSERT_METRIC_PREFERENCE_MAX;
  qpim_infinite_assert_metric.route_metric      = PIM_ASSERT_ROUTE_METRIC_MAX;
  qpim_infinite_assert_metric.ip_address        = qpim_inaddr_any;

  pim_if_init();
  pim_cmd_init();
  pim_ssmpingd_init();
}

void pim_terminate()
{
  pim_free();
}
