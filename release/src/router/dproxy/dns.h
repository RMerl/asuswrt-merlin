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


#define NUM_RRS 5
/*****************************************************************************/
struct dns_rr{
  char name[NAME_SIZE];
  uint16 type;
  uint16 class;
  uint32 ttl;
  uint16 rdatalen;
  char data[NAME_SIZE];
} __attribute__((packed));
/*****************************************************************************/
union header_flags {
  uint16 flags;

  struct {
    unsigned short int rcode:4;
    unsigned short int unused:3;
    unsigned short int recursion_avail:1;
    unsigned short int want_recursion:1;
    unsigned short int truncated:1;
    unsigned short int authorative:1;
    unsigned short int opcode:4;
    unsigned short int question:1;
  } __attribute__((packed)) f;
} __attribute__((packed));
/*****************************************************************************/
struct dns_header_s{
  uint16 id;
  union header_flags flags;
  uint16 qdcount;
  uint16 ancount;
  uint16 nscount;
  uint16 arcount;
} __attribute__((packed));
/*****************************************************************************/
struct dns_message{
  struct dns_header_s header;
  struct dns_rr question[NUM_RRS];
  struct dns_rr answer[NUM_RRS];
};
/*****************************************************************************/
typedef struct dns_request_s{
  char cname[NAME_SIZE];
  char ip[20];
  int ttl;
  int time_pending; /* request age in seconds */
  int duplicate_queries; /* duplicate query number */

  /* the actual dns request that was recieved */
  struct dns_message message;

  /* where the request came from */
  struct in_addr src_addr;
  int src_port;

  /* the orginal packet */
  char original_buf[MAX_PACKET_SIZE];
  int numread;
  char *here;

  /* next node in list */
  struct dns_request_s *next;
}dns_request_t;
/*****************************************************************************/
/* TYPE values */
enum{ A = 1,      /* a host address */
	NS,       /* an authoritative name server */
	MD,       /* a mail destination (Obsolete - use MX) */
	MF,       /* */
	CNAME,    /* the canonical name for an alias */
	SOA,      /* marks the start of a zone of authority  */
	MB,       /* a mailbox domain name (EXPERIMENTAL) */
	MG,       /* */
	MR,       /* */
	NUL,      /* */
	WKS,      /* a well known service description */
	PTR,      /* a domain name pointer */
	HINFO,    /* host information */
	MINFO,    /* mailbox or mail list information */
	MX,       /* mail exchange */
	TXT,      /* text strings */

	AAA = 0x1c /* IPv6 A */
	};

/* CLASS values */
enum{
  IN = 1,         /* the Internet */
    CS,
    CH,
    HS
};

/* OPCODE values */
enum{
  QUERY,
    IQUERY,
    STATUS
};
