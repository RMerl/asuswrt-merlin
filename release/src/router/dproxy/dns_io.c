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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "dns_io.h"
#include "dns_decode.h"

/*****************************************************************************/
int dns_read_packet(int sock, dns_request_t *m)
{
  struct sockaddr_in sa;
  int salen;
  
  /* Read in the actual packet */
  salen = sizeof(sa);
  
  m->numread = recvfrom(sock, m->original_buf, sizeof(m->original_buf), 0,
		     (struct sockaddr *)&sa, &salen);
  
  if ( m->numread < 0) {
    debug_perror("dns_read_packet: recvfrom\n");
    return -1;
  }
  
  /* TODO: check source addr against list of allowed hosts */

  /* record where it came from */
  memcpy( (void *)&m->src_addr, (void *)&sa.sin_addr, sizeof(struct in_addr));
  m->src_port = ntohs( sa.sin_port );

  /* check that the message is long enough */
  if( m->numread < sizeof (m->message.header) ){
    debug("dns_read_packet: packet from '%s' to short to be dns packet", 
	  inet_ntoa (sa.sin_addr) );
    return -1;
  }

  /* pass on for full decode */
  dns_decode_request( m );

  return 0;
}
/*****************************************************************************/
int dns_write_packet(int sock, struct in_addr in, int port, dns_request_t *m)
{
  struct sockaddr_in sa;
  int retval;

  /* Zero it out */
  memset((void *)&sa, 0, sizeof(sa));

  /* Fill in the information */
  //inet_aton( "203.12.160.35", &in );
  memcpy( &sa.sin_addr.s_addr, &in, sizeof(in) );
  sa.sin_port = htons(port);
  sa.sin_family = AF_INET;

  retval = sendto(sock, m->original_buf, m->numread, 0, 
		(struct sockaddr *)&sa, sizeof(sa));
  
  if( retval < 0 ){
    debug_perror("dns_write_packet: sendto");
  }

  return retval;
}
