/* Virtual terminal interface shell.
 * Copyright (C) 2000 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef VTYSH_H
#define VTYSH_H

#define VTYSH_ZEBRA  0x01
#define VTYSH_RIPD   0x02
#define VTYSH_RIPNGD 0x04
#define VTYSH_OSPFD  0x08
#define VTYSH_OSPF6D 0x10
#define VTYSH_BGPD   0x20
#define VTYSH_ALL    VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_RIPNGD|VTYSH_OSPFD|VTYSH_OSPF6D|VTYSH_BGPD
#define VTYSH_RMAP   VTYSH_RIPD|VTYSH_RIPNGD|VTYSH_OSPFD|VTYSH_OSPF6D|VTYSH_BGPD

#define VTYSH_INDEX_ZEBRA 0
#define VTYSH_INDEX_RIP   1
#define VTYSH_INDEX_RIPNG 2
#define VTYSH_INDEX_OSPF  3
#define VTYSH_INDEX_OSPF6 4
#define VTYSH_INDEX_BGP   5
#define VTYSH_INDEX_MAX   6

/* UNIX domain socket path. */
#define ZEBRA_PATH "/tmp/.zebra"
#define RIP_PATH "/tmp/.ripd"
#define RIPNG_PATH "/tmp/.ripngd"
#define OSPF_PATH "/tmp/.ospfd"
#define OSPF6_PATH "/tmp/.ospf6d"
#define BGP_PATH "/tmp/.bgpd"

/* vtysh local configuration file. */
#define VTYSH_DEFAULT_CONFIG "vtysh.conf"

void vtysh_init_vty ();
void vtysh_init_cmd ();
void vtysh_connect_all ();
void vtysh_readline_init ();
void vtysh_user_init ();

void vtysh_execute (char *);
void vtysh_execute_no_pager (char *);

char *vtysh_prompt ();

void vtysh_config_write ();

int vtysh_config_from_file (struct vty *, FILE *);

void vtysh_read_config (char *, char *, char *);

void vtysh_config_parse (char *);

void vtysh_config_dump (FILE *);

void vtysh_config_init ();

void vtysh_pager_init ();

/* Child process execution flag. */
extern int execute_flag;

extern struct vty *vty;

#endif /* VTYSH_H */
