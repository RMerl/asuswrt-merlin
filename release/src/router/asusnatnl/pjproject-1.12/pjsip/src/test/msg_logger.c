/* $Id: msg_logger.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "msg_logger.c"

static pj_bool_t msg_log_enabled;

static pj_bool_t on_rx_msg(pjsip_rx_data *rdata)
{
    if (msg_log_enabled) {
	PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s:%s:%d:\n"
			     "%.*s\n"
			     "--end msg--",
			     rdata->msg_info.len,
			     pjsip_rx_data_get_info(rdata),
			     rdata->tp_info.transport->type_name,
			     rdata->pkt_info.src_name,
			     rdata->pkt_info.src_port,
			     rdata->msg_info.len,
			     rdata->msg_info.msg_buf));
    }

    return PJ_FALSE;
}

static pj_status_t on_tx_msg(pjsip_tx_data *tdata)
{
    if (msg_log_enabled) {
	PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s:%s:%d:\n"
			     "%.*s\n"
			     "--end msg--",
			     (tdata->buf.cur - tdata->buf.start),
			     pjsip_tx_data_get_info(tdata),
			     tdata->tp_info.transport->type_name,
			     tdata->tp_info.dst_name,
			     tdata->tp_info.dst_port,
			     (tdata->buf.cur - tdata->buf.start),
			     tdata->buf.start));
    }
    return PJ_SUCCESS;
}


/* Message logger module. */
static pjsip_module mod_msg_logger = 
{
    NULL, NULL,				/* prev and next	*/
    { "mod-msg-logger", 14},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &on_rx_msg,				/* on_rx_request()	*/
    &on_rx_msg,				/* on_rx_response()	*/
    &on_tx_msg,				/* on_tx_request()	*/
    &on_tx_msg,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

int init_msg_logger(void)
{
    pj_status_t status;

    if (mod_msg_logger.id != -1)
	return 0;

    status = pjsip_endpt_register_module(endpt, &mod_msg_logger);
    if (status != PJ_SUCCESS) {
	app_perror("  error registering module", status);
	return -10;
    }

    return 0;
}

int msg_logger_set_enabled(pj_bool_t enabled)
{
    int val = msg_log_enabled;
    msg_log_enabled = enabled;
    return val;
}

