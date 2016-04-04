/*
 * Project: asusnatnl
 * File: im_handler.h
 *
 * Copyright (C) 2014 ASUSTek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IM_HANDLER_H
#define IM_HANDLER_H

#include <pjlib.h>
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#define session_set_status(session, st) (session->status = (client_status)st)
#define session_is_status(session, st) (session->status == (client_status)st)

struct natnl_im_data {
	int inst_id;
	int rport;
	pjsip_rx_data *rdata;
	pj_str_t *r_msg;
	pj_str_t *body;
	int timeout_sec;
	pj_sem_t *sem;
	char result_body[NATNL_PKT_MAX_LEN];
};

enum im_session_status {
	IM_SESSION_STATUS_NONE,
	IM_SESSION_STATUS_READY,
	IM_SESSION_STATUS_LO_DATA_GOT,
	IM_SESSION_STATUS_REQUEST_SENT,
	IM_SESSION_STATUS_RESPONSE_GOT,
	IM_SESSION_STATUS_LO_DATA_SENT,
	IM_SESSION_STATUS_DESTROYING,
	IM_SESSION_STATUS_DESTROYED,
};


int im_handler_thread(void *arg);

int send_im(struct natnl_im_data *im_data, int wait_timeout_sec);

int im_init(int inst_id);

int im_destroy(int inst_id);

int im_srv_socket_init(int inst_id, 
					   char *des_device_id, 
					   char *lport, 
					   char *rip,
					   char *rport,
					   int timeout_sec);

int im_srv_socket_destroy(int inst_id, 
						  char *lport, 
						  char *rport);

int im_recv_resp_msg(void *user_data, pjsip_rx_data *rdata, int status) ;

#endif /* IM_HANDLER_H */

