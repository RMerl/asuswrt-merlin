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

struct natnl_im_data {
	int inst_id;
	int rport;
	pjsip_rx_data *rdata;
	pj_str_t *body;
	pj_str_t result_body;
};


int im_handler_thread(void *arg);

int send_im(struct natnl_im_data *im_data, int wait_timeout_sec);

#endif /* IM_HANDLER_H */

