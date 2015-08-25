/* $Id: sock_qos_dummy.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/errno.h>
#include <pj/log.h>

/* Dummy implementation of QoS API. 
 * (this is controlled by pjlib's config.h)
 */
#if defined(PJ_QOS_IMPLEMENTATION) && PJ_QOS_IMPLEMENTATION==PJ_QOS_DUMMY

#define THIS_FILE   "sock_qos_dummy.c"


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
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(type);

    PJ_LOG(4,(THIS_FILE, "pj_sock_set_qos_type() is not implemented "
			 "for this platform"));
    return PJ_ENOTSUP;
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
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(p_type);

    PJ_LOG(4,(THIS_FILE, "pj_sock_get_qos_type() is not implemented "
			 "for this platform"));
    return PJ_ENOTSUP;
}

#endif	/* PJ_QOS_DUMMY */
