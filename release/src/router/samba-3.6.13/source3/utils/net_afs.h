/*
   Samba Unix/Linux SMB client library
   net afs commands
   Copyright (C) 2008  Kai Blin  (kai@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _NET_AFS_H_
#define _NET_AFS_H_

int net_afs_usage(struct net_context *c, int argc, const char **argv);
int net_afs_key(struct net_context *c, int argc, const char **argv);
int net_afs_impersonate(struct net_context *c, int argc,
			       const char **argv);
int net_afs(struct net_context *c, int argc, const char **argv);

#endif /*_NET_AFS_H_*/
