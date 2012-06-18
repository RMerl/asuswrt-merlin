/*
 * $Id: uici.h,v 1.1 2009-06-30 02:31:09 steven Exp $
 * Sockets Library
 *
 * ** NOTICE **
 *
 * This code is written by (and is therefore copyright) Dr Kay Robbins 
 * (krobbins@cs.utsa.edu) and Dr. Steve Robbins (srobbins@cs.utsa.edu), 
 * and was released with unspecified licensing as part of their book 
 * _UNIX_Systems_Programming_ (Prentice Hall, ISBN: 0130424110).
 *
 * Dr. Steve Robbins was kind enough to allow me to re-license this 
 * software as GPL.  I would request that any bugs or problems with 
 * this code be brought to my attention (ron@pedde.com), and I will 
 * submit appropriate patches upstream, should the problem be with
 * the original code.
 *
 * ** NOTICE **
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/*********************************** uici.h **************************/
/*   Prototypes for the three public UICI functions                  */  
/*********************************************************************/
#define UPORT
typedef unsigned short u_port_t;
int u_open(u_port_t port);
int u_accept(int fd, char *hostn, int hostnsize);
int u_connect(u_port_t port, char *hostn);
