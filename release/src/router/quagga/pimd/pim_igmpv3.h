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

#ifndef PIM_IGMPV3_H
#define PIM_IGMPV3_H

#include <zebra.h>
#include "if.h"

#define IGMP_V3_CHECKSUM_OFFSET            (2)
#define IGMP_V3_REPORT_NUMGROUPS_OFFSET    (6)
#define IGMP_V3_REPORT_GROUPPRECORD_OFFSET (8)
#define IGMP_V3_NUMSOURCES_OFFSET          (10)
#define IGMP_V3_SOURCES_OFFSET             (12)

/* GMI: Group Membership Interval */
#define PIM_IGMP_GMI_MSEC(qrv,qqi,qri_dsec) ((qrv) * (1000 * (qqi)) + 100 * (qri_dsec))

/* OQPI: Other Querier Present Interval */
#define PIM_IGMP_OQPI_MSEC(qrv,qqi,qri_dsec) ((qrv) * (1000 * (qqi)) + 100 * ((qri_dsec) >> 1))

/* SQI: Startup Query Interval */
#define PIM_IGMP_SQI(qi) (((qi) < 4) ? 1 : ((qi) >> 2))

/* LMQT: Last Member Query Time */
#define PIM_IGMP_LMQT_MSEC(lmqi_dsec, lmqc) ((lmqc) * (100 * (lmqi_dsec)))

/* OHPI: Older Host Present Interval */
#define PIM_IGMP_OHPI_DSEC(qrv,qqi,qri_dsec) ((qrv) * (10 * (qqi)) + (qri_dsec))

void igmp_group_reset_gmi(struct igmp_group *group);
void igmp_source_reset_gmi(struct igmp_sock *igmp,
			   struct igmp_group *group,
			   struct igmp_source *source);

void igmp_source_free(struct igmp_source *source);
void igmp_source_delete(struct igmp_source *source);
void igmp_source_delete_expired(struct list *source_list);

int igmp_group_compat_mode(const struct igmp_sock *igmp,
			   const struct igmp_group *group);

void igmpv3_report_isin(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources);
void igmpv3_report_isex(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources);
void igmpv3_report_toin(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources);
void igmpv3_report_toex(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources);
void igmpv3_report_allow(struct igmp_sock *igmp, struct in_addr from,
			 struct in_addr group_addr,
			 int num_sources, struct in_addr *sources);
void igmpv3_report_block(struct igmp_sock *igmp, struct in_addr from,
			 struct in_addr group_addr,
			 int num_sources, struct in_addr *sources);

void igmp_group_timer_lower_to_lmqt(struct igmp_group *group);
void igmp_source_timer_lower_to_lmqt(struct igmp_source *source);

struct igmp_source *igmp_find_source_by_addr(struct igmp_group *group,
					     struct in_addr src_addr);

void pim_igmp_send_membership_query(struct igmp_group *group,
				    int fd,
				    const char *ifname,
				    char *query_buf,
				    int query_buf_size,
				    int num_sources,
				    struct in_addr dst_addr,
				    struct in_addr group_addr,
				    int query_max_response_time_dsec,
				    uint8_t s_flag,
				    uint8_t querier_robustness_variable,
				    uint16_t querier_query_interval);

#endif /* PIM_IGMPV3_H */
