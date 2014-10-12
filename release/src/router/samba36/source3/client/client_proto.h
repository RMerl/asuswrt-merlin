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

#ifndef _CLIENT_PROTO_H_
#define _CLIENT_PROTO_H_

struct cli_state;
struct file_info;

/* The following definitions come from client/client.c  */

const char *client_get_cur_dir(void);
const char *client_set_cur_dir(const char *newdir);
NTSTATUS do_list(const char *mask,
			uint16 attribute,
			NTSTATUS (*fn)(struct cli_state *cli_state, struct file_info *,
				   const char *dir),
			bool rec,
			bool dirs);
int cmd_iosize(void);

/* The following definitions come from client/clitar.c  */

int cmd_block(void);
int cmd_tarmode(void);
int cmd_setmode(void);
int cmd_tar(void);
int process_tar(void);
int tar_parseargs(int argc, char *argv[], const char *Optarg, int Optind);

/* The following definitions come from client/dnsbrowse.c  */

int do_smb_browse(void);
int do_smb_browse(void);

#endif /*  _CLIENT_PROTO_H_  */
