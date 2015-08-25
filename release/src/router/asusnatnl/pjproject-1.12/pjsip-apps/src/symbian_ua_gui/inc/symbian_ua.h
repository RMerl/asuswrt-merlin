/* $Id: ua.cpp 1793 2008-02-14 13:39:24Z bennylp $ */
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

#ifndef __SYMBIAN_UA_H__
#define __SYMBIAN_UA_H__

#include <stddef.h>

typedef struct 
{
    void (*on_info)(const wchar_t* buf);
    void (*on_incoming_call)(const wchar_t* caller_disp, const wchar_t* caller_uri);
    void (*on_call_end)(const wchar_t* reason);
    void (*on_reg_state)(bool success);
    void (*on_unreg_state)(bool success);
} symbian_ua_info_cb_t;

int symbian_ua_init();
int symbian_ua_destroy();

void symbian_ua_set_info_callback(const symbian_ua_info_cb_t *cb);

int symbian_ua_set_account(const char *domain, const char *username, 
			   const char *password,
			   bool use_srtp, bool use_ice);

bool symbian_ua_anycall();
int symbian_ua_makecall(const char* dest_url);
int symbian_ua_endcall();
int symbian_ua_answercall();

#endif

