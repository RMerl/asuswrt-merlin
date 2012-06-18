/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996-1998
   Copyright (C) Jeremy Allison 1998

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _RPCCLIENT_H
#define _RPCCLIENT_H

struct tar_client_info
{
    int blocksize;
    BOOL inc;
    BOOL reset;
    BOOL excl;
    char type;
    int attrib;
    char **cliplist;
    int clipn;
    int tp;
    int num_files;
    int buf_size;
    int bytes_written;
    char *buf;
    int handle;
    int print_mode;
    char *file_mode;
};

struct nt_client_info
{
    /************* \PIPE\NETLOGON stuff ******************/

    fstring mach_acct;

    uint8 sess_key[16];
    DOM_CRED clnt_cred;
    DOM_CRED rtn_cred;

    NET_ID_INFO_CTR ctr;
    NET_USER_INFO_3 user_info3;

    /************** \PIPE\winreg stuff ********************/

    POLICY_HND reg_pol_connect;

    /************** \PIPE\lsarpc stuff ********************/

    POLICY_HND lsa_info_pol;

    /* domain member */
    DOM_SID level3_sid;
    DOM_SID level5_sid;

    /* domain controller */
    fstring level3_dom;
    fstring level5_dom;

    /************** \PIPE\samr stuff  ********************/

    POLICY_HND samr_pol_connect;
    POLICY_HND samr_pol_open_domain;
    POLICY_HND samr_pol_open_user;

    struct acct_info *sam;
    int num_sam_entries;
};

struct client_info
{
    struct in_addr dest_ip;
    fstring dest_host;
    fstring query_host;
    uint8 name_type;

    fstring myhostname;
    fstring mach_acct;

    pstring cur_dir;
    pstring base_dir;
    pstring file_sel;

    fstring service;
    fstring share;
    fstring svc_type;

    time_t newer_than;
    int archive_level;
    int dir_total;
    int put_total_time_ms;
    int put_total_size;
    int get_total_time_ms;
    int get_total_size;
    int print_mode;
    BOOL translation;
    BOOL recurse_dir;
    BOOL prompt;
    BOOL lowercase;
    BOOL abort_mget;

    struct tar_client_info tar;
    struct nt_client_info dom;
};

enum action_type {ACTION_HEADER, ACTION_ENUMERATE, ACTION_FOOTER};

#endif /* _RPCCLIENT_H */
