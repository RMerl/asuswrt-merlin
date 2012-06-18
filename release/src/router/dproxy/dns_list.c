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
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#include "dns_list.h"

static int	dns_list_size=0;	//added by CMC 8/4/2001
/*****************************************************************************/
dns_request_t *dns_list_add(dns_request_t *list, dns_request_t *m)
{
  dns_request_t *new_node, *node;

  if ( dns_list_size >= MAX_LIST_SIZE && list ){	//modified by CMC 12/27/2001
	node = list;
	while ( node->next )
		node = node->next;
	list = dns_list_remove( list, node );
  }
  //new_node = (dns_request_t *)malloc( sizeof(dns_request_t) * 10 );  //???
  new_node = (dns_request_t *)malloc( sizeof(dns_request_t) );
  if( !new_node) return list;

  dns_list_size++;			//added by CMC 8/4/2001

  debug( "Addr: %x\n", new_node);

  memcpy( new_node, m, sizeof (*m) );
  new_node->time_pending = 0;		//added by CMC 8/3/2001
  new_node->duplicate_queries = 0;	//added by CMC 8/4/2001

  new_node->next = list;
  return new_node;
}
/*****************************************************************************/
dns_request_t *dns_list_find_by_id(dns_request_t *list, dns_request_t *m)
{
#if DNS_DEBUG
  dns_list_print( list );
#endif

  while( list != NULL){
    debug("1. Name: %s  ....  %d  --- 2. %d\n", list->message.question[0].name,
	  list->message.header.id, m->message.header.id );
    if( list->message.header.id == m->message.header.id )
      return list;
    list = list->next;
  }
  return NULL;
}
/*****************************************************************************/
dns_request_t *dns_list_remove(dns_request_t *list, dns_request_t *m )
{
  dns_request_t *prev, *retval;

  if( !m )return list;

  prev = list;
  while (prev){
    if( prev->next == m)break;
    prev = prev->next;
  }

  if( !prev ){ //must be at start of list
    retval = m->next;
    free( m );
  }else{
    prev->next = m->next;
    retval = list;
    free( m );
  }
  --dns_list_size;	//added by CMC 8/4/2001
  return retval;
}
/*****************************************************************************/
void dns_list_print(dns_request_t *list)
{
  debug("Dumping list:\n");

  while(list){
    debug("    ID: %d ... Name: %s ---- IP: %s\n", list->message.header.id,
	  list->cname, list->ip );
    list = list->next;
  }

}




