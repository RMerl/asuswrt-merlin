/* $Id: sock_qos_wm.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/sock_qos.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>

#include <winsock.h>

/* QoS implementation for Windows Mobile 6, must be enabled explicitly
 * (this is controlled by pjlib's config.h)
 */
#if defined(PJ_QOS_IMPLEMENTATION) && PJ_QOS_IMPLEMENTATION==PJ_QOS_WM

#define THIS_FILE   "sock_qos_wm.c"

/* Mapping between our traffic type and WM's DSCP traffic types */
static const int dscp_map[] = 
{
    DSCPBestEffort,
    DSCPBackground,
    DSCPVideo,
    DSCPAudio,
    DSCPControl
};

PJ_DEF(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
					   pj_qos_params *param)
{
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(param);

    PJ_LOG(4,(THIS_FILE, "pj_sock_set_qos_params() is not implemented "
			 "for this platform"));
    return PJ_ENOTSUP;
}

PJ_DEF(pj_status_t) pj_sock_set_qos_type(pj_sock_t sock,
					 pj_qos_type type)
{
    int value;

    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(dscp_map), PJ_EINVAL);

    value = dscp_map[type];
    return pj_sock_setsockopt(sock, IPPROTO_IP, IP_DSCP_TRAFFIC_TYPE,
			      &value, sizeof(value));
}


PJ_DEF(pj_status_t) pj_sock_get_qos_params(pj_sock_t sock,
					   pj_qos_params *p_param)
{
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(p_param);

    PJ_LOG(4,(THIS_FILE, "pj_sock_get_qos_params() is not implemented "
			 "for this platform"));
    return PJ_ENOTSUP;
}

PJ_DEF(pj_status_t) pj_sock_get_qos_type(pj_sock_t sock,
					 pj_qos_type *p_type)
{
    pj_status_t status;
    int value, optlen;
    unsigned i;

    optlen = sizeof(value);
    value = 0;
    status = pj_sock_getsockopt(sock, IPPROTO_IP, IP_DSCP_TRAFFIC_TYPE,
			        &value, &optlen);
    if (status != PJ_SUCCESS)
	return status;

    *p_type = PJ_QOS_TYPE_BEST_EFFORT;
    for (i=0; i<PJ_ARRAY_SIZE(dscp_map); ++i) {
	if (value == dscp_map[i]) {
	    *p_type = (pj_qos_type)i;
	    break;
	}
    }

    return PJ_SUCCESS;
}

#endif	/* PJ_QOS_IMPLEMENTATION */
