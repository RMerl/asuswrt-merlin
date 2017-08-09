/*
   Unix SMB/CIFS implementation.
   handle unexpected packets
   NBT netbios library routines
   Copyright (C) Andrew Tridgell 1994-1998, 2000
   Copyright (C) Jeremy Allison 2007

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

#ifndef _LIBSMB_NMBLIB_H_
#define _LIBSMB_NMBLIB_H_

/* The following definitions come from libsmb/unexpected.c  */

#include "nameserv.h"

struct nb_packet_server;
struct nb_packet_reader;

NTSTATUS nb_packet_server_create(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev,
				 int max_clients,
				 struct nb_packet_server **presult);
void nb_packet_dispatch(struct nb_packet_server *server,
			struct packet_struct *p);
struct tevent_req *nb_packet_reader_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 enum packet_type type,
					 int trn_id,
					 const char *mailslot_name);
NTSTATUS nb_packet_reader_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			       struct nb_packet_reader **preader);
struct tevent_req *nb_packet_read_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct nb_packet_reader *reader);
NTSTATUS nb_packet_read_recv(struct tevent_req *req,
			     struct packet_struct **ppacket);

/* The following definitions come from libsmb/nmblib.c  */

void debug_nmb_packet(struct packet_struct *p);
void put_name(char *dest, const char *name, int pad, unsigned int name_type);
char *nmb_namestr(const struct nmb_name *n);
struct packet_struct *copy_packet(struct packet_struct *packet);
void free_packet(struct packet_struct *packet);
int packet_trn_id(struct packet_struct *p);
struct packet_struct *parse_packet(char *buf,int length,
				   enum packet_type packet_type,
				   struct in_addr ip,
				   int port);
struct packet_struct *read_packet(int fd,enum packet_type packet_type);
void make_nmb_name( struct nmb_name *n, const char *name, int type);
bool nmb_name_equal(struct nmb_name *n1, struct nmb_name *n2);
int build_packet(char *buf, size_t buflen, struct packet_struct *p);
bool send_packet(struct packet_struct *p);
bool match_mailslot_name(struct packet_struct *p, const char *mailslot_name);
int matching_len_bits(unsigned char *p1, unsigned char *p2, size_t len);
void sort_query_replies(char *data, int n, struct in_addr ip);
char *name_mangle(TALLOC_CTX *mem_ctx, const char *In, char name_type);
int name_extract(unsigned char *buf,size_t buf_len, unsigned int ofs, fstring name);
int name_len(unsigned char *s1, size_t buf_len);

#endif /* _LIBSMB_NMBLIB_H_ */
