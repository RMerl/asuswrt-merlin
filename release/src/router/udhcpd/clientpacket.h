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
#ifndef _CLIENTPACKET_H
#define _CLIENTPACKET_H

unsigned long random_xid(void);
int send_discover(unsigned long xid, unsigned long requested);
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_release(unsigned long server, unsigned long ciaddr);
int get_raw_packet(struct dhcpMessage *payload, int fd);

#endif
