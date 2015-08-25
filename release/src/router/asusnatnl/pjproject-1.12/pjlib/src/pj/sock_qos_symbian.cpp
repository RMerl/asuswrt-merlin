/* $Id: sock_qos_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "os_symbian.h"

PJ_DEF(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
					   pj_qos_params *param)
{
    PJ_ASSERT_RETURN(sock!=0 && sock!=PJ_INVALID_SOCKET, PJ_EINVAL);
    
    CPjSocket *pjsock = (CPjSocket*)sock;
    RSocket & rsock = pjsock->Socket();
    pj_status_t last_err = PJ_ENOTSUP;
    
    /* SO_PRIORITY and WMM are not supported */
    param->flags &= ~(PJ_QOS_PARAM_HAS_SO_PRIO | PJ_QOS_PARAM_HAS_WMM);
    
    if (param->flags & PJ_QOS_PARAM_HAS_DSCP) {
	TInt err;
	
	err = rsock.SetOpt(KSoIpTOS, KProtocolInetIp,
		           (param->dscp_val << 2));
	if (err != KErrNone) {
	    last_err = PJ_RETURN_OS_ERROR(err);
	    param->flags &= ~(PJ_QOS_PARAM_HAS_DSCP);
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
    PJ_ASSERT_RETURN(sock!=0 && sock!=PJ_INVALID_SOCKET, PJ_EINVAL);
    
    CPjSocket *pjsock = (CPjSocket*)sock;
    RSocket & rsock = pjsock->Socket();
    TInt err, dscp;
    
    pj_bzero(p_param, sizeof(*p_param));

    err = rsock.GetOpt(KSoIpTOS, KProtocolInetIp, dscp);
    if (err == KErrNone) {
	p_param->flags |= PJ_QOS_PARAM_HAS_DSCP;
	p_param->dscp_val = (dscp >> 2);
	return PJ_SUCCESS;
    } else {
	return PJ_RETURN_OS_ERROR(err);
    }
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

