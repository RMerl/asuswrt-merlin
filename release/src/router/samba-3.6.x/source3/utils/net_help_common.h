/*
   Samba Unix/Linux SMB client library
   net help commands
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

#ifndef _NET_HELP_COMMON_H_
#define _NET_HELP_COMMON_H_

/**
 * Get help for common methods.
 *
 * This will output some help for using the ADS/RPC/RAP transports.
 *
 * @param c	A net_context structure
 * @param argc	Normal argc with previous parameters removed
 * @param argv	Normal argv with previous parameters removed
 * @return	0 on success, nonzero on failure.
 */
int net_common_methods_usage(struct net_context *c, int argc, const char**argv);

/**
 * Get help for common flags.
 *
 * This will output some help for using common flags.
 *
 * @param c	A net_context structure
 * @param argc	Normal argc with previous parameters removed
 * @param argv	Normal argv with previous parameters removed
 * @return	0 on success, nonzero on failure.
 */
int net_common_flags_usage(struct net_context *c, int argc, const char **argv);


#endif /* _NET_HELP_COMMON_H_*/

