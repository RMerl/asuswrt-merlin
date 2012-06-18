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
#include "dproxy.h"

/*
 * Add a node to the list pointed to by 'list'. Copy 'm' into that node.
 * RETURNS: pointer to start of list 
 */
dns_request_t *dns_list_add(dns_request_t *list, dns_request_t *m);
/*
 * Scans each node in the list pointed to by 'list' and compare the id field
 * of each node with that of the id fields of 'm'.
 * RETURNS: pointer to the first node that matches else NULL.
 */
dns_request_t *dns_list_find_by_id(dns_request_t *list, dns_request_t *m);
/*
 * Removes and frees the node pointed to by m
 */
dns_request_t *dns_list_remove(dns_request_t *list, dns_request_t *m );
/*
 * Print out the list for debuging purposes
 */
void dns_list_print(dns_request_t *list);
