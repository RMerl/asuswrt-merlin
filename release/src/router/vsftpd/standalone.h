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
#ifndef VSF_STANDALONE_H
#define VSF_STANDALONE_H

struct vsf_client_launch
{
  unsigned int num_children;
  unsigned int num_this_ip;
};

/* vsf_standalone_main()
 * PURPOSE
 * This function starts listening on the network for incoming FTP connections.
 * When it gets one, it returns to the caller in a new process, with file
 * descriptor 0, 1 and 2 set to the network socket of the new client.
 *
 * RETURNS
 * Returns a structure representing the current number of clients, and
 * instances for this IP addresss.
 */
struct vsf_client_launch vsf_standalone_main(void);

#endif /* VSF_STANDALONE_H */

