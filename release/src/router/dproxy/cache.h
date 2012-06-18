/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
  **
  ** cache.h - function prototypes for the cache handling routines
  **
  ** Part of the drpoxy package by Matthew Pratt.
  **
  ** Copyright 1999 Matthew Pratt <mattpratt@yahoo.com>
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

#include "dproxy.h" /* for BUF_SIZE */

/* We assume cache entries are in the follwing form:

<name> <ip> <time_created>

Where "time_created" in the time the entry was created (seconds since 1/1/1970)
*/

/*
 * Search the cache "filename" for the first entry with name in the first field
 * Copies the corresponding IP into the char array "ip".
 * RETURNS: 0 if matching entry not found else 1.
 */
int cache_lookup_name(char *name, char ip[BUF_SIZE]);

/*
 * Search the cache "filename" for the first entry with ip in the second field.
 * Copies the corresponding Name into the char array "result".
 * RETURNS: 0 if matching entry not found else 1.
 */
int cache_lookup_ip(char *ip, char result[BUF_SIZE]);
/*
 * Appends the "name" and "ip" as well as the current time (seconds
 * since 1/1/1970), to the cache file "filename".
 */
void cache_name_append(char *name, char *ip);
/*
 * Scans the cache file "filename" for entries whose time_created fields
 * is greater than the current time minus the time they were created, and
 * removes them from the cache.
 */
void cache_purge(int older_than);
/*
 * Reads the hosts file given by HOSTS_FILE and add any entries in standard
 * cache file format to the cache_file stream.
 */
void cache_add_hosts_entries(FILE *cache_file);
/*
 * Added by CMC to serach name keyword in deny file
 * return 1 if match, else 0
 */
int deny_lookup_name(char *name);

/* EOF */
