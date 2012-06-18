/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996 - 1998
   
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

#include "includes.h"


/****************************************************************************
convert a share mode to a string
****************************************************************************/
char *get_file_mode_str(uint32 share_mode)
{
	static fstring mode;

	switch ((share_mode>>4)&0xF)
	{
		case DENY_NONE : fstrcpy(mode, "DENY_NONE  "); break;
		case DENY_ALL  : fstrcpy(mode, "DENY_ALL   "); break;
		case DENY_DOS  : fstrcpy(mode, "DENY_DOS   "); break;
		case DENY_READ : fstrcpy(mode, "DENY_READ  "); break;
		case DENY_WRITE: fstrcpy(mode, "DENY_WRITE "); break;
		default        : fstrcpy(mode, "DENY_????  "); break;
	}

	switch (share_mode & 0xF)
	{
		case 0 : fstrcat(mode, "RDONLY"); break;
		case 1 : fstrcat(mode, "WRONLY"); break;
		case 2 : fstrcat(mode, "RDWR  "); break;
		default: fstrcat(mode, "R??W??"); break;
	}

	return mode;
}

/****************************************************************************
convert an oplock mode to a string
****************************************************************************/
char *get_file_oplock_str(uint32 op_type)
{
	static fstring oplock;
	BOOL excl  = IS_BITS_SET_ALL(op_type, EXCLUSIVE_OPLOCK);
	BOOL batch = IS_BITS_SET_ALL(op_type, BATCH_OPLOCK    );

	oplock[0] = 0;

	if (excl           ) fstrcat(oplock, "EXCLUSIVE");
	if (excl  &&  batch) fstrcat(oplock, "+");
	if (          batch) fstrcat(oplock, "BATCH");
	if (!excl && !batch) fstrcat(oplock, "NONE");

	return oplock;
}

/****************************************************************************
convert a share type enum to a string
****************************************************************************/
char *get_share_type_str(uint32 type)
{
	static fstring typestr;

	switch (type)
	{
		case STYPE_DISKTREE: fstrcpy(typestr, "Disk"   ); break;
		case STYPE_PRINTQ  : fstrcpy(typestr, "Printer"); break;	      
		case STYPE_DEVICE  : fstrcpy(typestr, "Device" ); break;
		case STYPE_IPC     : fstrcpy(typestr, "IPC"    ); break;      
		default            : fstrcpy(typestr, "????"   ); break;      
	}
	return typestr;
}

/****************************************************************************
convert a server type enum to a string
****************************************************************************/
char *get_server_type_str(uint32 type)
{
	static fstring typestr;

	if (type == SV_TYPE_ALL)
	{
		fstrcpy(typestr, "All");
	}
	else
	{
		int i;
		typestr[0] = 0;
		for (i = 0; i < 32; i++)
		{
			if (IS_BITS_SET_ALL(type, 1 << i))
			{
				switch (((unsigned)1) << i)
				{
					case SV_TYPE_WORKSTATION      : fstrcat(typestr, "Wk " ); break;
					case SV_TYPE_SERVER           : fstrcat(typestr, "Sv " ); break;
					case SV_TYPE_SQLSERVER        : fstrcat(typestr, "Sql "); break;
					case SV_TYPE_DOMAIN_CTRL      : fstrcat(typestr, "PDC "); break;
					case SV_TYPE_DOMAIN_BAKCTRL   : fstrcat(typestr, "BDC "); break;
					case SV_TYPE_TIME_SOURCE      : fstrcat(typestr, "Tim "); break;
					case SV_TYPE_AFP              : fstrcat(typestr, "AFP "); break;
					case SV_TYPE_NOVELL           : fstrcat(typestr, "Nov "); break;
					case SV_TYPE_DOMAIN_MEMBER    : fstrcat(typestr, "Dom "); break;
					case SV_TYPE_PRINTQ_SERVER    : fstrcat(typestr, "PrQ "); break;
					case SV_TYPE_DIALIN_SERVER    : fstrcat(typestr, "Din "); break;
					case SV_TYPE_SERVER_UNIX      : fstrcat(typestr, "Unx "); break;
					case SV_TYPE_NT               : fstrcat(typestr, "NT " ); break;
					case SV_TYPE_WFW              : fstrcat(typestr, "Wfw "); break;
					case SV_TYPE_SERVER_MFPN      : fstrcat(typestr, "Mfp "); break;
					case SV_TYPE_SERVER_NT        : fstrcat(typestr, "SNT "); break;
					case SV_TYPE_POTENTIAL_BROWSER: fstrcat(typestr, "PtB "); break;
					case SV_TYPE_BACKUP_BROWSER   : fstrcat(typestr, "BMB "); break;
					case SV_TYPE_MASTER_BROWSER   : fstrcat(typestr, "LMB "); break;
					case SV_TYPE_DOMAIN_MASTER    : fstrcat(typestr, "DMB "); break;
					case SV_TYPE_SERVER_OSF       : fstrcat(typestr, "OSF "); break;
					case SV_TYPE_SERVER_VMS       : fstrcat(typestr, "VMS "); break;
					case SV_TYPE_WIN95_PLUS       : fstrcat(typestr, "W95 "); break;
					case SV_TYPE_ALTERNATE_XPORT  : fstrcat(typestr, "Xpt "); break;
					case SV_TYPE_LOCAL_LIST_ONLY  : fstrcat(typestr, "Dom "); break;
					case SV_TYPE_DOMAIN_ENUM      : fstrcat(typestr, "Loc "); break;
				}
			}
		}
		i = strlen(typestr)-1;
		if (typestr[i] == ' ') typestr[i] = 0;

	}
	return typestr;
}

/****************************************************************************
server info level 101 display function
****************************************************************************/
void display_srv_info_101(FILE *out_hnd, enum action_type action,
		SRV_INFO_101 *sv101)
{
	if (sv101 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Server Info Level 101:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring name;
			fstring comment;

			fstrcpy(name    , dos_unistrn2(sv101->uni_name    .buffer, sv101->uni_name    .uni_str_len));
			fstrcpy(comment , dos_unistrn2(sv101->uni_comment .buffer, sv101->uni_comment .uni_str_len));

			display_server(out_hnd, action, name, sv101->srv_type, comment);

			fprintf(out_hnd, "\tplatform_id     : %d\n"    , sv101->platform_id);
			fprintf(out_hnd, "\tos version      : %d.%d\n" , sv101->ver_major, sv101->ver_minor);

			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}

}

/****************************************************************************
server info level 102 display function
****************************************************************************/
void display_srv_info_102(FILE *out_hnd, enum action_type action,SRV_INFO_102 *sv102)
{
	if (sv102 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Server Info Level 102:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring name;
			fstring comment;
			fstring usr_path;

			fstrcpy(name    , dos_unistrn2(sv102->uni_name    .buffer, sv102->uni_name    .uni_str_len));
			fstrcpy(comment , dos_unistrn2(sv102->uni_comment .buffer, sv102->uni_comment .uni_str_len));
			fstrcpy(usr_path, dos_unistrn2(sv102->uni_usr_path.buffer, sv102->uni_usr_path.uni_str_len));

			display_server(out_hnd, action, name, sv102->srv_type, comment);

			fprintf(out_hnd, "\tplatform_id     : %d\n"    , sv102->platform_id);
			fprintf(out_hnd, "\tos version      : %d.%d\n" , sv102->ver_major, sv102->ver_minor);

			fprintf(out_hnd, "\tusers           : %x\n"    , sv102->users      );
			fprintf(out_hnd, "\tdisc, hidden    : %x,%x\n" , sv102->disc     , sv102->hidden   );
			fprintf(out_hnd, "\tannounce, delta : %d, %d\n", sv102->announce , sv102->ann_delta);
			fprintf(out_hnd, "\tlicenses        : %d\n"    , sv102->licenses   );
			fprintf(out_hnd, "\tuser path       : %s\n"    , usr_path);

			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
server info container display function
****************************************************************************/
void display_srv_info_ctr(FILE *out_hnd, enum action_type action,SRV_INFO_CTR *ctr)
{
	if (ctr == NULL || ctr->ptr_srv_ctr == 0)
	{
		fprintf(out_hnd, "Server Information: unavailable due to an error\n");
		return;
	}

	switch (ctr->switch_value)
	{
		case 101:
		{
			display_srv_info_101(out_hnd, action, &(ctr->srv.sv101));
			break;
		}
		case 102:
		{
			display_srv_info_102(out_hnd, action, &(ctr->srv.sv102));
			break;
		}
		default:
		{
			fprintf(out_hnd, "Server Information: Unknown Info Level\n");
			break;
		}
	}
}

/****************************************************************************
connection info level 0 display function
****************************************************************************/
void display_conn_info_0(FILE *out_hnd, enum action_type action,
		CONN_INFO_0 *info0)
{
	if (info0 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Connection Info Level 0:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\tid: %d\n", info0->id);

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}

}

/****************************************************************************
connection info level 1 display function
****************************************************************************/
void display_conn_info_1(FILE *out_hnd, enum action_type action,
		CONN_INFO_1 *info1, CONN_INFO_1_STR *str1)
{
	if (info1 == NULL || str1 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Connection Info Level 1:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring usr_name;
			fstring net_name;

			fstrcpy(usr_name, dos_unistrn2(str1->uni_usr_name.buffer, str1->uni_usr_name.uni_str_len));
			fstrcpy(net_name, dos_unistrn2(str1->uni_net_name.buffer, str1->uni_net_name.uni_str_len));

			fprintf(out_hnd, "\tid       : %d\n", info1->id);
			fprintf(out_hnd, "\ttype     : %s\n", get_share_type_str(info1->type));
			fprintf(out_hnd, "\tnum_opens: %d\n", info1->num_opens);
			fprintf(out_hnd, "\tnum_users: %d\n", info1->num_users);
			fprintf(out_hnd, "\topen_time: %d\n", info1->open_time);

			fprintf(out_hnd, "\tuser name: %s\n", usr_name);
			fprintf(out_hnd, "\tnet  name: %s\n", net_name);

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}

}

/****************************************************************************
connection info level 0 container display function
****************************************************************************/
void display_srv_conn_info_0_ctr(FILE *out_hnd, enum action_type action,
				SRV_CONN_INFO_0 *ctr)
{
	if (ctr == NULL)
	{
		fprintf(out_hnd, "display_srv_conn_info_0_ctr: unavailable due to an internal error\n");
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < ctr->num_entries_read; i++)
			{
				display_conn_info_0(out_hnd, ACTION_HEADER   , &(ctr->info_0[i]));
				display_conn_info_0(out_hnd, ACTION_ENUMERATE, &(ctr->info_0[i]));
				display_conn_info_0(out_hnd, ACTION_FOOTER   , &(ctr->info_0[i]));
			}
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
connection info level 1 container display function
****************************************************************************/
void display_srv_conn_info_1_ctr(FILE *out_hnd, enum action_type action,
				SRV_CONN_INFO_1 *ctr)
{
	if (ctr == NULL)
	{
		fprintf(out_hnd, "display_srv_conn_info_1_ctr: unavailable due to an internal error\n");
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < ctr->num_entries_read; i++)
			{
				display_conn_info_1(out_hnd, ACTION_HEADER   , &(ctr->info_1[i]), &(ctr->info_1_str[i]));
				display_conn_info_1(out_hnd, ACTION_ENUMERATE, &(ctr->info_1[i]), &(ctr->info_1_str[i]));
				display_conn_info_1(out_hnd, ACTION_FOOTER   , &(ctr->info_1[i]), &(ctr->info_1_str[i]));
			}
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
connection info container display function
****************************************************************************/
void display_srv_conn_info_ctr(FILE *out_hnd, enum action_type action,
				SRV_CONN_INFO_CTR *ctr)
{
	if (ctr == NULL || ctr->ptr_conn_ctr == 0)
	{
		fprintf(out_hnd, "display_srv_conn_info_ctr: unavailable due to an internal error\n");
		return;
	}

	switch (ctr->switch_value)
	{
		case 0:
		{
			display_srv_conn_info_0_ctr(out_hnd, action,
			                   &(ctr->conn.info0));
			break;
		}
		case 1:
		{
			display_srv_conn_info_1_ctr(out_hnd, action,
			                   &(ctr->conn.info1));
			break;
		}
		default:
		{
			fprintf(out_hnd, "display_srv_conn_info_ctr: Unknown Info Level\n");
			break;
		}
	}
}


/****************************************************************************
share info level 1 display function
****************************************************************************/
void display_share_info_1(FILE *out_hnd, enum action_type action,
			  SRV_SHARE_INFO_1 *info1)
{
	if (info1 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Share Info Level 1:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring remark  ;
			fstring net_name;

			fstrcpy(net_name, dos_unistrn2(info1->info_1_str.uni_netname.buffer, info1->info_1_str.uni_netname.uni_str_len));
			fstrcpy(remark  , dos_unistrn2(info1->info_1_str.uni_remark .buffer, info1->info_1_str.uni_remark .uni_str_len));

			display_share(out_hnd, action, net_name, info1->info_1.type, remark);

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}

}

/****************************************************************************
share info level 2 display function
****************************************************************************/
void display_share_info_2(FILE *out_hnd, enum action_type action,
			  SRV_SHARE_INFO_2 *info2)
{
	if (info2 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "Share Info Level 2:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring remark  ;
			fstring net_name;
			fstring path    ;
			fstring passwd  ;

			fstrcpy(net_name, dos_unistrn2(info2->info_2_str.uni_netname.buffer, info2->info_2_str.uni_netname.uni_str_len));
			fstrcpy(remark  , dos_unistrn2(info2->info_2_str.uni_remark .buffer, info2->info_2_str.uni_remark .uni_str_len));
			fstrcpy(path    , dos_unistrn2(info2->info_2_str.uni_path   .buffer, info2->info_2_str.uni_path   .uni_str_len));
			fstrcpy(passwd  , dos_unistrn2(info2->info_2_str.uni_passwd .buffer, info2->info_2_str.uni_passwd .uni_str_len));

			display_share2(out_hnd, action, net_name,
			      info2->info_2.type, remark, info2->info_2.perms,
			      info2->info_2.max_uses, info2->info_2.num_uses,
			      path, passwd);

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}

}

/****************************************************************************
share info container display function
****************************************************************************/
void display_srv_share_info_ctr(FILE *out_hnd, enum action_type action,
				SRV_SHARE_INFO_CTR *ctr)
{
	if (ctr == NULL)
	{
		fprintf(out_hnd, "display_srv_share_info_ctr: unavailable due to an internal error\n");
	return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < ctr->num_entries; i++)
			{
				switch (ctr->info_level) {
				case 1:
					display_share_info_1(out_hnd, ACTION_HEADER   , &(ctr->share.info1[i]));
					display_share_info_1(out_hnd, ACTION_ENUMERATE, &(ctr->share.info1[i]));
					display_share_info_1(out_hnd, ACTION_FOOTER   , &(ctr->share.info1[i]));
					break;
				case 2:
					display_share_info_2(out_hnd, ACTION_HEADER   , &(ctr->share.info2[i]));
					display_share_info_2(out_hnd, ACTION_ENUMERATE, &(ctr->share.info2[i]));
					display_share_info_2(out_hnd, ACTION_FOOTER   , &(ctr->share.info2[i]));
					break;
				default:
					fprintf(out_hnd, "display_srv_share_info_ctr: Unknown Info Level\n");
					break;
				}
			}
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
file info level 3 display function
****************************************************************************/
void display_file_info_3(FILE *out_hnd, enum action_type action,
		FILE_INFO_3 *info3, FILE_INFO_3_STR *str3)
{
	if (info3 == NULL || str3 == NULL)
	{
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "File Info Level 3:\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring path_name;
			fstring user_name;

			fstrcpy(path_name, dos_unistrn2(str3->uni_path_name.buffer, str3->uni_path_name.uni_str_len));
			fstrcpy(user_name, dos_unistrn2(str3->uni_user_name.buffer, str3->uni_user_name.uni_str_len));

			fprintf(out_hnd, "\tid       : %d\n", info3->id);
			fprintf(out_hnd, "\tperms    : %s\n", get_file_mode_str(info3->perms));
			fprintf(out_hnd, "\tnum_locks: %d\n", info3->num_locks);

			fprintf(out_hnd, "\tpath name: %s\n", path_name);
			fprintf(out_hnd, "\tuser name: %s\n", user_name);

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}

}

/****************************************************************************
file info level 3 container display function
****************************************************************************/
void display_srv_file_info_3_ctr(FILE *out_hnd, enum action_type action,
				SRV_FILE_INFO_3 *ctr)
{
	if (ctr == NULL)
	{
		fprintf(out_hnd, "display_srv_file_info_3_ctr: unavailable due to an internal error\n");
		return;
	}

	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < ctr->num_entries_read; i++)
			{
				display_file_info_3(out_hnd, ACTION_HEADER   , &(ctr->info_3[i]), &(ctr->info_3_str[i]));
				display_file_info_3(out_hnd, ACTION_ENUMERATE, &(ctr->info_3[i]), &(ctr->info_3_str[i]));
				display_file_info_3(out_hnd, ACTION_FOOTER   , &(ctr->info_3[i]), &(ctr->info_3_str[i]));
			}
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
file info container display function
****************************************************************************/
void display_srv_file_info_ctr(FILE *out_hnd, enum action_type action,
				SRV_FILE_INFO_CTR *ctr)
{
	if (ctr == NULL || ctr->ptr_file_ctr == 0)
	{
		fprintf(out_hnd, "display_srv_file_info_ctr: unavailable due to an internal error\n");
		return;
	}

	switch (ctr->switch_value)
	{
		case 3:
		{
			display_srv_file_info_3_ctr(out_hnd, action,
			                   &(ctr->file.info3));
			break;
		}
		default:
		{
			fprintf(out_hnd, "display_srv_file_info_ctr: Unknown Info Level\n");
			break;
		}
	}
}

/****************************************************************************
 print browse connection on a host
 ****************************************************************************/
void display_server(FILE *out_hnd, enum action_type action,
				char *sname, uint32 type, char *comment)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t%-15.15s%-20s %s\n",
			                 sname, get_server_type_str(type), comment);
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
print shares on a host
****************************************************************************/
void display_share(FILE *out_hnd, enum action_type action,
				char *sname, uint32 type, char *comment)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t%-15.15s%-10.10s%s\n",
			                 sname, get_share_type_str(type), comment);
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}


/****************************************************************************
print shares on a host, level 2
****************************************************************************/
void display_share2(FILE *out_hnd, enum action_type action,
				char *sname, uint32 type, char *comment,
				uint32 perms, uint32 max_uses, uint32 num_uses,
				char *path, char *passwd)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t%-15.15s%-10.10s%s %x %x %x %s %s\n",
			                 sname, get_share_type_str(type), comment,
			                 perms, max_uses, num_uses, path, passwd);
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}


/****************************************************************************
print name info
****************************************************************************/
void display_name(FILE *out_hnd, enum action_type action,
				char *sname)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t%-21.21s\n", sname);
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}


/****************************************************************************
 display group rid info
 ****************************************************************************/
void display_group_rid_info(FILE *out_hnd, enum action_type action,
				uint32 num_gids, DOM_GID *gid)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			if (num_gids == 0)
			{
				fprintf(out_hnd, "\tNo Groups\n");
			}
			else
			{
				fprintf(out_hnd, "\tGroup Info\n");
				fprintf(out_hnd, "\t----------\n");
			}
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < num_gids; i++)
			{
				fprintf(out_hnd, "\tGroup RID: %8x attr: %x\n",
								  gid[i].g_rid, gid[i].attr);
			}

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}


/****************************************************************************
 display alias name info
 ****************************************************************************/
void display_alias_name_info(FILE *out_hnd, enum action_type action,
				uint32 num_aliases, fstring *alias_name, uint32 *num_als_usrs)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			if (num_aliases == 0)
			{
				fprintf(out_hnd, "\tNo Aliases\n");
			}
			else
			{
				fprintf(out_hnd, "\tAlias Names\n");
				fprintf(out_hnd, "\t----------- \n");
			}
			break;
		}
		case ACTION_ENUMERATE:
		{
			int i;

			for (i = 0; i < num_aliases; i++)
			{
				fprintf(out_hnd, "\tAlias Name: %s Attributes: %3d\n",
								  alias_name[i], num_als_usrs[i]);
			}

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}


/****************************************************************************
 display sam_user_info_21 structure
 ****************************************************************************/
void display_sam_user_info_21(FILE *out_hnd, enum action_type action, SAM_USER_INFO_21 *usr)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "\tUser Info, Level 0x15\n");
			fprintf(out_hnd, "\t---------------------\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t\tUser Name   : %s\n", dos_unistrn2(usr->uni_user_name   .buffer, usr->uni_user_name   .uni_str_len)); /* username unicode string */
			fprintf(out_hnd, "\t\tFull Name   : %s\n", dos_unistrn2(usr->uni_full_name   .buffer, usr->uni_full_name   .uni_str_len)); /* user's full name unicode string */
			fprintf(out_hnd, "\t\tHome Drive  : %s\n", dos_unistrn2(usr->uni_home_dir    .buffer, usr->uni_home_dir    .uni_str_len)); /* home directory unicode string */
			fprintf(out_hnd, "\t\tDir Drive   : %s\n", dos_unistrn2(usr->uni_dir_drive   .buffer, usr->uni_dir_drive   .uni_str_len)); /* home directory drive unicode string */
			fprintf(out_hnd, "\t\tProfile Path: %s\n", dos_unistrn2(usr->uni_profile_path.buffer, usr->uni_profile_path.uni_str_len)); /* profile path unicode string */
			fprintf(out_hnd, "\t\tLogon Script: %s\n", dos_unistrn2(usr->uni_logon_script.buffer, usr->uni_logon_script.uni_str_len)); /* logon script unicode string */
			fprintf(out_hnd, "\t\tDescription : %s\n", dos_unistrn2(usr->uni_acct_desc   .buffer, usr->uni_acct_desc   .uni_str_len)); /* user description unicode string */
			fprintf(out_hnd, "\t\tWorkstations: %s\n", dos_unistrn2(usr->uni_workstations.buffer, usr->uni_workstations.uni_str_len)); /* workstaions unicode string */
			fprintf(out_hnd, "\t\tUnknown Str : %s\n", dos_unistrn2(usr->uni_unknown_str .buffer, usr->uni_unknown_str .uni_str_len)); /* unknown string unicode string */
			fprintf(out_hnd, "\t\tRemote Dial : %s\n", dos_unistrn2(usr->uni_munged_dial .buffer, usr->uni_munged_dial .uni_str_len)); /* munged remote access unicode string */

			fprintf(out_hnd, "\t\tLogon Time               : %s\n", http_timestring(nt_time_to_unix(&(usr->logon_time           ))));
			fprintf(out_hnd, "\t\tLogoff Time              : %s\n", http_timestring(nt_time_to_unix(&(usr->logoff_time          ))));
			fprintf(out_hnd, "\t\tKickoff Time             : %s\n", http_timestring(nt_time_to_unix(&(usr->kickoff_time         ))));
			fprintf(out_hnd, "\t\tPassword last set Time   : %s\n", http_timestring(nt_time_to_unix(&(usr->pass_last_set_time   ))));
			fprintf(out_hnd, "\t\tPassword can change Time : %s\n", http_timestring(nt_time_to_unix(&(usr->pass_can_change_time ))));
			fprintf(out_hnd, "\t\tPassword must change Time: %s\n", http_timestring(nt_time_to_unix(&(usr->pass_must_change_time))));
			
			fprintf(out_hnd, "\t\tunknown_2[0..31]...\n"); /* user passwords? */

			fprintf(out_hnd, "\t\tuser_rid : %x\n"  , usr->user_rid ); /* User ID */
			fprintf(out_hnd, "\t\tgroup_rid: %x\n"  , usr->group_rid); /* Group ID */
			fprintf(out_hnd, "\t\tacb_info : %04x\n", usr->acb_info ); /* Account Control Info */

			fprintf(out_hnd, "\t\tunknown_3: %08x\n", usr->unknown_3); /* 0x00ff ffff */
			fprintf(out_hnd, "\t\tlogon_divs: %d\n", usr->logon_divs); /* 0x0000 00a8 which is 168 which is num hrs in a week */
			fprintf(out_hnd, "\t\tunknown_5: %08x\n", usr->unknown_5); /* 0x0002 0000 */

			fprintf(out_hnd, "\t\tpadding1[0..7]...\n");

			if (usr->ptr_logon_hrs)
			{
				fprintf(out_hnd, "\t\tlogon_hrs[0..%d]...\n", usr->logon_hrs.len);
			}

			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}


/****************************************************************************
convert a security permissions into a string
****************************************************************************/
char *get_sec_mask_str(uint32 type)
{
	static fstring typestr;
	int i;

	switch (type)
	{
		case SEC_RIGHTS_FULL_CONTROL:
		{
			fstrcpy(typestr, "Full Control");
			return typestr;
		}

		case SEC_RIGHTS_READ:
		{
			fstrcpy(typestr, "Read");
			return typestr;
		}
		default:
		{
			break;
		}
	}

	typestr[0] = 0;
	for (i = 0; i < 32; i++)
	{
		if (IS_BITS_SET_ALL(type, 1 << i))
		{
			switch (((unsigned)1) << i)
			{
				case SEC_RIGHTS_QUERY_VALUE    : fstrcat(typestr, "Query " ); break;
				case SEC_RIGHTS_SET_VALUE      : fstrcat(typestr, "Set " ); break;
				case SEC_RIGHTS_CREATE_SUBKEY  : fstrcat(typestr, "Create "); break;
				case SEC_RIGHTS_ENUM_SUBKEYS   : fstrcat(typestr, "Enum "); break;
				case SEC_RIGHTS_NOTIFY         : fstrcat(typestr, "Notify "); break;
				case SEC_RIGHTS_CREATE_LINK    : fstrcat(typestr, "CreateLink "); break;
				case SEC_RIGHTS_DELETE         : fstrcat(typestr, "Delete "); break;
				case SEC_RIGHTS_READ_CONTROL   : fstrcat(typestr, "ReadControl "); break;
				case SEC_RIGHTS_WRITE_DAC      : fstrcat(typestr, "WriteDAC "); break;
				case SEC_RIGHTS_WRITE_OWNER    : fstrcat(typestr, "WriteOwner "); break;
			}
			type &= ~(1 << i);
		}
	}

	/* remaining bits get added on as-is */
	if (type != 0)
	{
		fstring tmp;
		slprintf(tmp, sizeof(tmp)-1, "[%08x]", type);
		fstrcat(typestr, tmp);
	}

	/* remove last space */
	i = strlen(typestr)-1;
	if (typestr[i] == ' ') typestr[i] = 0;

	return typestr;
}

/****************************************************************************
 display sec_access structure
 ****************************************************************************/
void display_sec_access(FILE *out_hnd, enum action_type action, SEC_ACCESS *info)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t\tPermissions: %s\n",
			        get_sec_mask_str(info->mask));
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
 display sec_ace structure
 ****************************************************************************/
void display_sec_ace(FILE *out_hnd, enum action_type action, SEC_ACE *ace)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "\tACE\n");
			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring sid_str;

			display_sec_access(out_hnd, ACTION_HEADER   , &ace->info);
			display_sec_access(out_hnd, ACTION_ENUMERATE, &ace->info);
			display_sec_access(out_hnd, ACTION_FOOTER   , &ace->info);

			sid_to_string(sid_str, &ace->sid);
			fprintf(out_hnd, "\t\tSID: %s\n", sid_str);
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
 display sec_acl structure
 ****************************************************************************/
void display_sec_acl(FILE *out_hnd, enum action_type action, SEC_ACL *sec_acl)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "\tACL\tNum ACEs:\t%d\trevision:\t%x\n",
			                 sec_acl->num_aces, sec_acl->revision); 
			fprintf(out_hnd, "\t---\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			if (sec_acl->size != 0 && sec_acl->num_aces != 0)
			{
				int i;
				for (i = 0; i < sec_acl->num_aces; i++)
				{
					display_sec_ace(out_hnd, ACTION_HEADER   , &sec_acl->ace_list[i]);
					display_sec_ace(out_hnd, ACTION_ENUMERATE, &sec_acl->ace_list[i]);
					display_sec_ace(out_hnd, ACTION_FOOTER   , &sec_acl->ace_list[i]);
				}
			}
				
			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}

/****************************************************************************
 display sec_desc structure
 ****************************************************************************/
void display_sec_desc(FILE *out_hnd, enum action_type action, SEC_DESC *sec)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "\tSecurity Descriptor\trevision:\t%x\ttype:\t%x\n",
			                 sec->revision, sec->type); 
			fprintf(out_hnd, "\t-------------------\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			fstring sid_str;

			if (sec->off_sacl != 0)
			{
				display_sec_acl(out_hnd, ACTION_HEADER   , sec->sacl);
				display_sec_acl(out_hnd, ACTION_ENUMERATE, sec->sacl);
				display_sec_acl(out_hnd, ACTION_FOOTER   , sec->sacl);
			}
			if (sec->off_dacl != 0)
			{
				display_sec_acl(out_hnd, ACTION_HEADER   , sec->dacl);
				display_sec_acl(out_hnd, ACTION_ENUMERATE, sec->dacl);
				display_sec_acl(out_hnd, ACTION_FOOTER   , sec->dacl);
			}
			if (sec->off_owner_sid != 0)
			{
				sid_to_string(sid_str, sec->owner_sid);
				fprintf(out_hnd, "\tOwner SID:\t%s\n", sid_str);
			}
			if (sec->off_grp_sid != 0)
			{
				sid_to_string(sid_str, sec->grp_sid);
				fprintf(out_hnd, "\tParent SID:\t%s\n", sid_str);
			}
				
			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}

/****************************************************************************
convert a security permissions into a string
****************************************************************************/
char *get_reg_val_type_str(uint32 type)
{
	static fstring typestr;

	switch (type)
	{
		case 0x01:
		{
			fstrcpy(typestr, "string");
			return typestr;
		}

		case 0x03:
		{
			fstrcpy(typestr, "bytes");
			return typestr;
		}

		case 0x04:
		{
			fstrcpy(typestr, "uint32");
			return typestr;
		}

		case 0x07:
		{
			fstrcpy(typestr, "multi");
			return typestr;
		}
		default:
		{
			break;
		}
	}
	slprintf(typestr, sizeof(typestr)-1, "[%d]", type);
	return typestr;
}


static void print_reg_value(FILE *out_hnd, char *val_name, uint32 val_type, BUFFER2 *value)
{
	fstring type;
	fstrcpy(type, get_reg_val_type_str(val_type));

	switch (val_type)
	{
		case 0x01: /* unistr */
		{
			fprintf(out_hnd,"\t%s:\t%s:\t%s\n", val_name, type, dos_buffer2_to_str(value));
			break;
		}

		default: /* unknown */
		case 0x03: /* bytes */
		{
			if (value->buf_len <= 8)
			{
				fprintf(out_hnd,"\t%s:\t%s:\t", val_name, type);
				out_data(out_hnd, (char*)value->buffer, value->buf_len, 8);
			}
			else
			{
				fprintf(out_hnd,"\t%s:\t%s:\n", val_name, type);
				out_data(out_hnd, (char*)value->buffer, value->buf_len, 16);
			}
			break;
		}

		case 0x04: /* uint32 */
		{
			fprintf(out_hnd,"\t%s:\t%s: 0x%08x\n", val_name, type, buffer2_to_uint32(value));
			break;
		}

		case 0x07: /* multiunistr */
		{
			fprintf(out_hnd,"\t%s:\t%s:\t%s\n", val_name, type, dos_buffer2_to_multistr(value));
			break;
		}
	}
}

/****************************************************************************
 display structure
 ****************************************************************************/
void display_reg_value_info(FILE *out_hnd, enum action_type action,
				char *val_name, uint32 val_type, BUFFER2 *value)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			print_reg_value(out_hnd, val_name, val_type, value);
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

/****************************************************************************
 display structure
 ****************************************************************************/
void display_reg_key_info(FILE *out_hnd, enum action_type action,
				char *key_name, time_t key_mod_time)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			break;
		}
		case ACTION_ENUMERATE:
		{
			fprintf(out_hnd, "\t%s\t(%s)\n",
			        key_name, http_timestring(key_mod_time));
			break;
		}
		case ACTION_FOOTER:
		{
			break;
		}
	}
}

#if COPY_THIS_TEMPLATE
/****************************************************************************
 display structure
 ****************************************************************************/
 void display_(FILE *out_hnd, enum action_type action, *)
{
	switch (action)
	{
		case ACTION_HEADER:
		{
			fprintf(out_hnd, "\t\n"); 
			fprintf(out_hnd, "\t-------------------\n");

			break;
		}
		case ACTION_ENUMERATE:
		{
			break;
		}
		case ACTION_FOOTER:
		{
			fprintf(out_hnd, "\n");
			break;
		}
	}
}

#endif
