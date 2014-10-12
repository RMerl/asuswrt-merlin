/*
 * Unix SMB/CIFS implementation.
 * collected prototypes header
 *
 * frozen from "make proto" in May 2008
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SWAT_PROTO_H_
#define _SWAT_PROTO_H_


/* The following definitions come from web/cgi.c  */

void cgi_load_variables(void);
const char *cgi_variable(const char *name);
const char *cgi_variable_nonull(const char *name);
bool am_root(void);
char *cgi_user_name(void);
char *cgi_user_pass(void);
char *cgi_nonce(void);
void cgi_setup(const char *rootdir, int auth_required);
const char *cgi_baseurl(void);
const char *cgi_pathinfo(void);
const char *cgi_remote_host(void);
const char *cgi_remote_addr(void);
bool cgi_waspost(void);

/* The following definitions come from web/diagnose.c  */

bool winbindd_running(void);
bool nmbd_running(void);
bool smbd_running(void);

/* The following definitions come from web/neg_lang.c  */

int web_open(const char *fname, int flags, mode_t mode);
void web_set_lang(const char *lang_string);

/* The following definitions come from web/startstop.c  */

void start_smbd(void);
void start_nmbd(void);
void start_winbindd(void);
void stop_smbd(void);
void stop_nmbd(void);
void stop_winbindd(void);
void kill_pid(struct server_id pid);

/* The following definitions come from web/statuspage.c  */

void status_page(void);

/* The following definitions come from web/swat.c  */

const char *lang_msg_rotate(TALLOC_CTX *ctx, const char *msgid);
void get_xsrf_token(const char *username, const char *pass,
		    const char *formname, time_t xsrf_time, char token_str[33]);
void print_xsrf_token(const char *username, const char *pass,
		      const char *formname);
bool verify_xsrf_token(const char *formname);

#endif /*  _SWAT_PROTO_H_  */
