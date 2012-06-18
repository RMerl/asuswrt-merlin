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
 * Queries are encoded such that there is and integer specifying how long 
 * the next block of text will be before the actuall text. For eaxmple:
 *             www.linux.com => \03www\05linux\03com\0
 * This function assumes that buf points to an encoded name.
 * The name is decoded into name. Name should be at least 255 bytes long.
 * RETURNS: The length of the string including the '\0' terminator.
 */
void dns_decode_name(char *name, char **buf);
/*
 * Decodes the raw packet in buf to create a rr. Assumes buf points to the 
 * start of a rr. 
 * Note: Question rrs dont have rdatalen or rdata. Set is_question when
 * decoding question rrs, else clear is_question
 * RETURNS: the amount that buf should be incremented
 */
void dns_decode_rr(struct dns_rr *rr, char **buf,int is_question,char *header);

/*
 * A raw packet pointed to by buf is decoded in the assumed to be alloced 
 * dns_message structure.
 * RETURNS: 0
 */
int dns_decode_message(struct dns_message *m, char **buf);

/* 
 * 
 */
void dns_decode_request(dns_request_t *m);





