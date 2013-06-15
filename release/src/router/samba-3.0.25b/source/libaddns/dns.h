/*
  Linux DNS client library implementation

  Copyright (C) 2006 Krishna Ganugapati <krishnag@centeris.com>
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
  02110-1301  USA
*/

#ifndef _DNS_H
#define _DNS_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>

#ifdef HAVE_UUID_UUID_H
#include <uuid/uuid.h>
#endif

#ifdef HAVE_KRB5_H
#include <krb5.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>

#ifndef int16
#define int16 int16_t
#endif

#ifndef uint16
#define uint16 uint16_t
#endif

#ifndef int32
#define int32 int32_t
#endif

#ifndef uint32
#define uint32 uint32_t
#endif
#endif

#ifdef HAVE_KRB5_H
#include <krb5.h>
#endif

#if HAVE_GSSAPI_H
#include <gssapi.h>
#elif HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#elif HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#endif

#if defined(HAVE_GSSAPI_H) || defined(HAVE_GSSAPI_GSSAPI_H) || defined(HAVE_GSSAPI_GSSAPI_GENERIC_H)
#define HAVE_GSSAPI_SUPPORT    1
#endif

#include <talloc.h>

void *_talloc_zero_zeronull(const void *ctx, size_t size, const char *name);
void *_talloc_memdup_zeronull(const void *t, const void *p, size_t size, const char *name);
void *_talloc_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name);
void *_talloc_zero_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name);
void *talloc_zeronull(const void *context, size_t size, const char *name);

#define TALLOC(ctx, size) talloc_zeronull(ctx, size, __location__)
#define TALLOC_P(ctx, type) (type *)talloc_zeronull(ctx, sizeof(type), #type)
#define TALLOC_ARRAY(ctx, type, count) (type *)_talloc_array_zeronull(ctx, sizeof(type), count, #type)
#define TALLOC_MEMDUP(ctx, ptr, size) _talloc_memdup_zeronull(ctx, ptr, size, __location__)
#define TALLOC_ZERO(ctx, size) _talloc_zero_zeronull(ctx, size, __location__)
#define TALLOC_ZERO_P(ctx, type) (type *)_talloc_zero_zeronull(ctx, sizeof(type), #type)
#define TALLOC_ZERO_ARRAY(ctx, type, count) (type *)_talloc_zero_array_zeronull(ctx, sizeof(type), count, #type)
#define TALLOC_REALLOC(ctx, ptr, count) _talloc_realloc(ctx, ptr, count, __location__)
#define TALLOC_REALLOC_ARRAY(ctx, ptr, type, count) (type *)_talloc_realloc_array(ctx, ptr, sizeof(type), count, #type)
#define talloc_destroy(ctx) talloc_free(ctx)
#define TALLOC_FREE(ctx) do { if ((ctx) != NULL) {talloc_free(ctx); ctx=NULL;} } while(0)
#define TALLOC_SIZE(ctx, size) talloc_zeronull(ctx, size, __location__)
#define TALLOC_ZERO_SIZE(ctx, size) _talloc_zero_zeronull(ctx, size, __location__)

/*******************************************************************
   Type definitions for int16, int32, uint16 and uint32.  Needed
   for Samba coding style
*******************************************************************/

#ifndef uint8
#  define uint8 unsigned char
#endif

#if !defined(int16) && !defined(HAVE_INT16_FROM_RPC_RPC_H)
#  if (SIZEOF_SHORT == 4)
#    define int16 __ERROR___CANNOT_DETERMINE_TYPE_FOR_INT16;
#  else /* SIZEOF_SHORT != 4 */
#    define int16 short
#  endif /* SIZEOF_SHORT != 4 */
   /* needed to work around compile issue on HP-UX 11.x */
#  define _INT16        1
#endif

/*
 * Note we duplicate the size tests in the unsigned
 * case as int16 may be a typedef from rpc/rpc.h
 */

#if !defined(uint16) && !defined(HAVE_UINT16_FROM_RPC_RPC_H)
#  if (SIZEOF_SHORT == 4)
#    define uint16 __ERROR___CANNOT_DETERMINE_TYPE_FOR_INT16;
#  else /* SIZEOF_SHORT != 4 */
#    define uint16 unsigned short
#  endif /* SIZEOF_SHORT != 4 */
#endif

#if !defined(int32) && !defined(HAVE_INT32_FROM_RPC_RPC_H)
#  if (SIZEOF_INT == 4)
#    define int32 int
#  elif (SIZEOF_LONG == 4)
#    define int32 long
#  elif (SIZEOF_SHORT == 4)
#    define int32 short
#  else
     /* uggh - no 32 bit type?? probably a CRAY. just hope this works ... */
#    define int32 int
#  endif
   /* needed to work around compile issue on HP-UX 11.x */
#  define _INT32        1
#endif

/*
 * Note we duplicate the size tests in the unsigned
 * case as int32 may be a typedef from rpc/rpc.h
 */

#if !defined(uint32) && !defined(HAVE_UINT32_FROM_RPC_RPC_H)
#  if (SIZEOF_INT == 4)
#    define uint32 unsigned int
#  elif (SIZEOF_LONG == 4)
#    define uint32 unsigned long
#  elif (SIZEOF_SHORT == 4)
#    define uint32 unsigned short
#  else
      /* uggh - no 32 bit type?? probably a CRAY. just hope this works ... */
#    define uint32 unsigned
#  endif
#endif

/*
 * check for 8 byte long long
 */

#if !defined(uint64)
#  if (SIZEOF_LONG == 8)
#    define uint64 unsigned long
#  elif (SIZEOF_LONG_LONG == 8)
#    define uint64 unsigned long long
#  endif /* don't lie.  If we don't have it, then don't use it */
#endif

/* needed on Sun boxes */
#ifndef INADDR_NONE
#define INADDR_NONE          0xFFFFFFFF
#endif

#include "dnserr.h"


#define DNS_TCP			1
#define DNS_UDP			2

#define DNS_OPCODE_UPDATE	1

/* DNS Class Types */

#define DNS_CLASS_IN		1
#define DNS_CLASS_ANY		255
#define DNS_CLASS_NONE		254

/* DNS RR Types */

#define DNS_RR_A		1

#define DNS_TCP_PORT		53
#define DNS_UDP_PORT		53

#define QTYPE_A         1
#define QTYPE_NS        2
#define QTYPE_MD        3
#define QTYPE_CNAME	5
#define QTYPE_SOA	6
#define QTYPE_ANY	255
#define	QTYPE_TKEY	249
#define QTYPE_TSIG	250

/*
MF              4 a mail forwarder (Obsolete - use MX)
CNAME           5 the canonical name for an alias
SOA             6 marks the start of a zone of authority
MB              7 a mailbox domain name (EXPERIMENTAL)
MG              8 a mail group member (EXPERIMENTAL)
MR              9 a mail rename domain name (EXPERIMENTAL)
NULL            10 a null RR (EXPERIMENTAL)
WKS             11 a well known service description
PTR             12 a domain name pointer
HINFO           13 host information
MINFO           14 mailbox or mail list information
MX              15 mail exchange
TXT             16 text strings
*/

#define QR_QUERY	 0x0000
#define QR_RESPONSE	 0x0001

#define OPCODE_QUERY 0x00
#define OPCODE_IQUERY	0x01
#define OPCODE_STATUS	0x02

#define AA			1

#define RECURSION_DESIRED	0x01

#define RCODE_NOERROR          0
#define RCODE_FORMATERROR      1
#define RCODE_SERVER_FAILURE   2
#define RCODE_NAME_ERROR       3
#define RCODE_NOTIMPLEMENTED   4
#define RCODE_REFUSED          5

#define SENDBUFFER_SIZE		65536
#define RECVBUFFER_SIZE		65536

/*
 * TKEY Modes from rfc2930
 */

#define DNS_TKEY_MODE_SERVER   1
#define DNS_TKEY_MODE_DH       2
#define DNS_TKEY_MODE_GSSAPI   3
#define DNS_TKEY_MODE_RESOLVER 4
#define DNS_TKEY_MODE_DELETE   5


#define DNS_ONE_DAY_IN_SECS	86400
#define DNS_TEN_HOURS_IN_SECS	36000

#define SOCKET_ERROR 		-1
#define INVALID_SOCKET		-1

#define  DNS_NO_ERROR		0
#define  DNS_FORMAT_ERROR	1
#define  DNS_SERVER_FAILURE	2
#define  DNS_NAME_ERROR		3
#define  DNS_NOT_IMPLEMENTED	4
#define  DNS_REFUSED		5

typedef long HANDLE;

#ifndef _UPPER_BOOL
typedef int BOOL;
#define _UPPER_BOOL
#endif


enum dns_ServerType { DNS_SRV_ANY, DNS_SRV_WIN2000, DNS_SRV_WIN2003 };

struct dns_domain_label {
	struct dns_domain_label *next;
	char *label;
	size_t len;
};

struct dns_domain_name {
	struct dns_domain_label *pLabelList;
};

struct dns_question {
	struct dns_domain_name *name;
	uint16 q_type;
	uint16 q_class;
};

/*
 * Before changing the definition of dns_zone, look
 * dns_marshall_update_request(), we rely on this being the same as
 * dns_question right now.
 */

struct dns_zone {
	struct dns_domain_name *name;
	uint16 z_type;
	uint16 z_class;
};

struct dns_rrec {
	struct dns_domain_name *name;
	uint16 type;
	uint16 r_class;
	uint32 ttl;
	uint16 data_length;
	uint8 *data;
};

struct dns_tkey_record {
	struct dns_domain_name *algorithm;
	time_t inception;
	time_t expiration;
	uint16 mode;
	uint16 error;
	uint16 key_length;
	uint8 *key;
};

struct dns_request {
	uint16 id;
	uint16 flags;
	uint16 num_questions;
	uint16 num_answers;
	uint16 num_auths;
	uint16 num_additionals;
	struct dns_question **questions;
	struct dns_rrec **answers;
	struct dns_rrec **auths;
	struct dns_rrec **additionals;
};

/*
 * Before changing the definition of dns_update_request, look
 * dns_marshall_update_request(), we rely on this being the same as
 * dns_request right now.
 */

struct dns_update_request {
	uint16 id;
	uint16 flags;
	uint16 num_zones;
	uint16 num_preqs;
	uint16 num_updates;
	uint16 num_additionals;
	struct dns_zone **zones;
	struct dns_rrec **preqs;
	struct dns_rrec **updates;
	struct dns_rrec **additionals;
};

struct dns_connection {
	int32 hType;
	int s;
	struct sockaddr RecvAddr;
};

struct dns_buffer {
	uint8 *data;
	size_t size;
	size_t offset;
	DNS_ERROR error;
};

/* from dnsutils.c */

DNS_ERROR dns_domain_name_from_string( TALLOC_CTX *mem_ctx,
				       const char *pszDomainName,
				       struct dns_domain_name **presult );
char *dns_generate_keyname( TALLOC_CTX *mem_ctx );

/* from dnsrecord.c */

DNS_ERROR dns_create_query( TALLOC_CTX *mem_ctx, const char *name,
			    uint16 q_type, uint16 q_class,
			    struct dns_request **preq );
DNS_ERROR dns_create_update( TALLOC_CTX *mem_ctx, const char *name,
			     struct dns_update_request **preq );
DNS_ERROR dns_create_probe(TALLOC_CTX *mem_ctx, const char *zone,
			   const char *host, int num_ips,
			   const struct in_addr *iplist,
			   struct dns_update_request **preq);
DNS_ERROR dns_create_rrec(TALLOC_CTX *mem_ctx, const char *name,
			  uint16 type, uint16 r_class, uint32 ttl,
			  uint16 data_length, uint8 *data,
			  struct dns_rrec **prec);
DNS_ERROR dns_add_rrec(TALLOC_CTX *mem_ctx, struct dns_rrec *rec,
		       uint16 *num_records, struct dns_rrec ***records);
DNS_ERROR dns_create_tkey_record(TALLOC_CTX *mem_ctx, const char *keyname,
				 const char *algorithm_name, time_t inception,
				 time_t expiration, uint16 mode, uint16 error,
				 uint16 key_length, const uint8 *key,
				 struct dns_rrec **prec);
DNS_ERROR dns_create_name_in_use_record(TALLOC_CTX *mem_ctx,
					const char *name,
					const struct in_addr *ip,
					struct dns_rrec **prec);
DNS_ERROR dns_create_delete_record(TALLOC_CTX *mem_ctx, const char *name,
				   uint16 type, uint16 r_class,
				   struct dns_rrec **prec);
DNS_ERROR dns_create_name_not_in_use_record(TALLOC_CTX *mem_ctx,
					    const char *name, uint32 type,
					    struct dns_rrec **prec);
DNS_ERROR dns_create_a_record(TALLOC_CTX *mem_ctx, const char *host,
			      uint32 ttl, struct in_addr ip,
			      struct dns_rrec **prec);
DNS_ERROR dns_unmarshall_tkey_record(TALLOC_CTX *mem_ctx, struct dns_rrec *rec,
				     struct dns_tkey_record **ptkey);
DNS_ERROR dns_create_tsig_record(TALLOC_CTX *mem_ctx, const char *keyname,
				 const char *algorithm_name,
				 time_t time_signed, uint16 fudge,
				 uint16 mac_length, const uint8 *mac,
				 uint16 original_id, uint16 error,
				 struct dns_rrec **prec);
DNS_ERROR dns_add_rrec(TALLOC_CTX *mem_ctx, struct dns_rrec *rec,
		       uint16 *num_records, struct dns_rrec ***records);

/* from dnssock.c */

DNS_ERROR dns_open_connection( const char *nameserver, int32 dwType,
		    TALLOC_CTX *mem_ctx,
		    struct dns_connection **conn );
DNS_ERROR dns_send(struct dns_connection *conn, const struct dns_buffer *buf);
DNS_ERROR dns_receive(TALLOC_CTX *mem_ctx, struct dns_connection *conn,
		      struct dns_buffer **presult);
DNS_ERROR dns_transaction(TALLOC_CTX *mem_ctx, struct dns_connection *conn,
			  const struct dns_request *req,
			  struct dns_request **resp);
DNS_ERROR dns_update_transaction(TALLOC_CTX *mem_ctx,
				 struct dns_connection *conn,
				 struct dns_update_request *up_req,
				 struct dns_update_request **up_resp);

/* from dnsmarshall.c */

struct dns_buffer *dns_create_buffer(TALLOC_CTX *mem_ctx);
void dns_marshall_buffer(struct dns_buffer *buf, const uint8 *data,
			 size_t len);
void dns_marshall_uint16(struct dns_buffer *buf, uint16 val);
void dns_marshall_uint32(struct dns_buffer *buf, uint32 val);
void dns_unmarshall_buffer(struct dns_buffer *buf, uint8 *data,
			   size_t len);
void dns_unmarshall_uint16(struct dns_buffer *buf, uint16 *val);
void dns_unmarshall_uint32(struct dns_buffer *buf, uint32 *val);
void dns_unmarshall_domain_name(TALLOC_CTX *mem_ctx,
				struct dns_buffer *buf,
				struct dns_domain_name **pname);
void dns_marshall_domain_name(struct dns_buffer *buf,
			      const struct dns_domain_name *name);
void dns_unmarshall_domain_name(TALLOC_CTX *mem_ctx,
				struct dns_buffer *buf,
				struct dns_domain_name **pname);
DNS_ERROR dns_marshall_request(TALLOC_CTX *mem_ctx,
			       const struct dns_request *req,
			       struct dns_buffer **pbuf);
DNS_ERROR dns_unmarshall_request(TALLOC_CTX *mem_ctx,
				 struct dns_buffer *buf,
				 struct dns_request **preq);
DNS_ERROR dns_marshall_update_request(TALLOC_CTX *mem_ctx,
				      struct dns_update_request *update,
				      struct dns_buffer **pbuf);
DNS_ERROR dns_unmarshall_update_request(TALLOC_CTX *mem_ctx,
					struct dns_buffer *buf,
					struct dns_update_request **pupreq);
struct dns_request *dns_update2request(struct dns_update_request *update);
struct dns_update_request *dns_request2update(struct dns_request *request);
uint16 dns_response_code(uint16 flags);

/* from dnsgss.c */

#ifdef HAVE_GSSAPI_SUPPORT

void display_status( const char *msg, OM_uint32 maj_stat, OM_uint32 min_stat ); 
DNS_ERROR dns_negotiate_sec_ctx( const char *target_realm,
				 const char *servername,
				 const char *keyname,
				 gss_ctx_id_t *gss_ctx,
				 enum dns_ServerType srv_type );
DNS_ERROR dns_sign_update(struct dns_update_request *req,
			  gss_ctx_id_t gss_ctx,
			  const char *keyname,
			  const char *algorithmname,
			  time_t time_signed, uint16 fudge);
DNS_ERROR dns_create_update_request(TALLOC_CTX *mem_ctx,
				    const char *domainname,
				    const char *hostname,
				    const struct in_addr *ip_addr,
				    size_t num_adds,
				    struct dns_update_request **preq);

#endif	/* HAVE_GSSAPI_SUPPORT */

#endif	/* _DNS_H */
