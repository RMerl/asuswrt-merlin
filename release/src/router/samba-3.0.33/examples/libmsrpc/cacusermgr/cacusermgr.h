/*
 * Unix SMB/CIFS implementation. 
 * cacusermgr definitions and includes.
 *
 * Copyright (C) Chris Nicholls     2005
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef CACUSERMGR_H_
#define CACUSERMGR_H_

#include "libmsrpc.h"
#include "includes.h"

/*used for the simple pager - mgr_page()*/
#define DEFAULT_SCREEN_LINES 20 

/**************
 * prototypes *
 **************/

/*util.c*/
void usage();
int process_cmd_line(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, int argc, char **argv);
void mgr_getline(fstring line);
void mgr_page(uint32 line_count);
uint32 rid_or_name(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, uint32 *rid, char **name);
char *get_new_password(TALLOC_CTX *mem_ctx);
void printerr(const char *msg, NTSTATUS status);
void print_rid_list(uint32 *rids, char **names, uint32 num_rids);
void print_lookup_records(CacLookupRidsRecord *map, uint32 num_rids);
int list_groups(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd);
void list_privs(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, CacUserInfo *info);
void add_privs(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, CacUserInfo *info);
void list_users(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd);

void mgr_GetAuthDataFn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword);


/*mgr_group.c*/
void group_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, POLICY_HND *group_hnd);

/*mgr_user.c*/
void user_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, POLICY_HND *user_hnd);

#endif /*CACUSERMGR_H_*/
