/* $Id: sock_qos_bsd.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/string.h>

/* This is the implementation of QoS with BSD socket's setsockopt(),
 * using IP_TOS and SO_PRIORITY
 */ 
#if !defined(PJ_QOS_IMPLEMENTATION) || PJ_QOS_IMPLEMENTATION==PJ_QOS_BSD

PJ_DEF(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
					   pj_qos_params *param)
{
    pj_status_t last_err = PJ_ENOTSUP;
    pj_status_t status;

    /* No op? */
    if (!param->flags)
	return PJ_SUCCESS;

    /* Clear WMM field since we don't support it */
    param->flags &= ~(PJ_QOS_PARAM_HAS_WMM);

    /* Set TOS/DSCP */
    if (param->flags & PJ_QOS_PARAM_HAS_DSCP) {
	/* Value is dscp_val << 2 */
	int val = (param->dscp_val << 2);
	status = pj_sock_setsockopt(sock, pj_SOL_IP(), pj_IP_TOS(), 
				    &val, sizeof(val));
	if (status != PJ_SUCCESS) {
	    param->flags &= ~(PJ_QOS_PARAM_HAS_DSCP);
	    last_err = status;
	}
    }

    /* Set SO_PRIORITY */
    if (param->flags & PJ_QOS_PARAM_HAS_SO_PRIO) {
	int val = param->so_prio;
	status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_PRIORITY(),
				    &val, sizeof(val));
	if (status != PJ_SUCCESS) {
	    param->flags &= ~(PJ_QOS_PARAM_HAS_SO_PRIO);
	    last_err = status;
	}
    }

    return param->flags ? PJ_SUCCESS : last_err;
}

PJ_DEF(pj_status_t) pj_sock_set_qos_type(pj_sock_t sock,
					 pj_qos_type type)
{
    pj_qos_params param;
    pj_status_t status;

    status = pj_qos_get_params(type, &param);
    if (status != PJ_SUCCESS)
	return status;

    return pj_sock_set_qos_params(sock, &param);
}


PJ_DEF(pj_status_t) pj_sock_get_qos_params(pj_sock_t sock,
					   pj_qos_params *p_param)
{
    pj_status_t last_err = PJ_ENOTSUP;
    int val, optlen;
    pj_status_t status;

    pj_bzero(p_param, sizeof(*p_param));

    /* Get DSCP/TOS value */
    optlen = sizeof(val);
    status = pj_sock_getsockopt(sock, pj_SOL_IP(), pj_IP_TOS(), 
				&val, &optlen);
    if (status == PJ_SUCCESS) {
	p_param->flags |= PJ_QOS_PARAM_HAS_DSCP;
	p_param->dscp_val = (pj_uint8_t)(val >> 2);
    } else {
	last_err = status;
    }

    /* Get SO_PRIORITY */
    optlen = sizeof(val);
    status = pj_sock_getsockopt(sock, pj_SOL_SOCKET(), pj_SO_PRIORITY(),
				&val, &optlen);
    if (status == PJ_SUCCESS) {
	p_param->flags |= PJ_QOS_PARAM_HAS_SO_PRIO;
	p_param->so_prio = (pj_uint8_t)val;
    } else {
	last_err = status;
    }

    /* WMM is not supported */

    return p_param->flags ? PJ_SUCCESS : last_err;
}

PJ_DEF(pj_status_t) pj_sock_get_qos_type(pj_sock_t sock,
					 pj_qos_type *p_type)
{
    pj_qos_params param;
    pj_status_t status;

    status = pj_sock_get_qos_params(sock, &param);
    if (status != PJ_SUCCESS)
	return status;

    return pj_qos_get_type(&param, p_type);
}

#endif	/* PJ_QOS_IMPLEMENTATION */

