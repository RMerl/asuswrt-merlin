/*
   Unix SMB/CIFS implementation.
   NBT netbios library routines
   Copyright (C) Andrew Tridgell 1994-1998
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

#include "includes.h"
#include "libsmb/nmblib.h"

static const struct opcode_names {
	const char *nmb_opcode_name;
	int opcode;
} nmb_header_opcode_names[] = {
	{"Query",           0 },
	{"Registration",      5 },
	{"Release",           6 },
	{"WACK",              7 },
	{"Refresh",           8 },
	{"Refresh(altcode)",  9 },
	{"Multi-homed Registration", 15 },
	{0, -1 }
};

/****************************************************************************
 Lookup a nmb opcode name.
****************************************************************************/

static const char *lookup_opcode_name( int opcode )
{
	const struct opcode_names *op_namep;
	int i;

	for(i = 0; nmb_header_opcode_names[i].nmb_opcode_name != 0; i++) {
		op_namep = &nmb_header_opcode_names[i];
		if(opcode == op_namep->opcode)
			return op_namep->nmb_opcode_name;
	}
	return "<unknown opcode>";
}

/****************************************************************************
 Print out a res_rec structure.
****************************************************************************/

static void debug_nmb_res_rec(struct res_rec *res, const char *hdr)
{
	int i, j;

	DEBUGADD( 4, ( "    %s: nmb_name=%s rr_type=%d rr_class=%d ttl=%d\n",
		hdr,
		nmb_namestr(&res->rr_name),
		res->rr_type,
		res->rr_class,
		res->ttl ) );

	if( res->rdlength == 0 || res->rdata == NULL )
		return;

	for (i = 0; i < res->rdlength; i+= MAX_NETBIOSNAME_LEN) {
		DEBUGADD(4, ("    %s %3x char ", hdr, i));

		for (j = 0; j < MAX_NETBIOSNAME_LEN; j++) {
			unsigned char x = res->rdata[i+j];
			if (x < 32 || x > 127)
				x = '.';

			if (i+j >= res->rdlength)
				break;
			DEBUGADD(4, ("%c", x));
		}

		DEBUGADD(4, ("   hex "));

		for (j = 0; j < MAX_NETBIOSNAME_LEN; j++) {
			if (i+j >= res->rdlength)
				break;
			DEBUGADD(4, ("%02X", (unsigned char)res->rdata[i+j]));
		}

		DEBUGADD(4, ("\n"));
	}
}

/****************************************************************************
 Process a nmb packet.
****************************************************************************/

void debug_nmb_packet(struct packet_struct *p)
{
	struct nmb_packet *nmb = &p->packet.nmb;

	if( DEBUGLVL( 4 ) ) {
		dbgtext( "nmb packet from %s(%d) header: id=%d "
				"opcode=%s(%d) response=%s\n",
			inet_ntoa(p->ip), p->port,
			nmb->header.name_trn_id,
			lookup_opcode_name(nmb->header.opcode),
			nmb->header.opcode,
			BOOLSTR(nmb->header.response) );
		dbgtext( "    header: flags: bcast=%s rec_avail=%s "
				"rec_des=%s trunc=%s auth=%s\n",
			BOOLSTR(nmb->header.nm_flags.bcast),
			BOOLSTR(nmb->header.nm_flags.recursion_available),
			BOOLSTR(nmb->header.nm_flags.recursion_desired),
			BOOLSTR(nmb->header.nm_flags.trunc),
			BOOLSTR(nmb->header.nm_flags.authoritative) );
		dbgtext( "    header: rcode=%d qdcount=%d ancount=%d "
				"nscount=%d arcount=%d\n",
			nmb->header.rcode,
			nmb->header.qdcount,
			nmb->header.ancount,
			nmb->header.nscount,
			nmb->header.arcount );
	}

	if (nmb->header.qdcount) {
		DEBUGADD( 4, ( "    question: q_name=%s q_type=%d q_class=%d\n",
			nmb_namestr(&nmb->question.question_name),
			nmb->question.question_type,
			nmb->question.question_class) );
	}

	if (nmb->answers && nmb->header.ancount) {
		debug_nmb_res_rec(nmb->answers,"answers");
	}
	if (nmb->nsrecs && nmb->header.nscount) {
		debug_nmb_res_rec(nmb->nsrecs,"nsrecs");
	}
	if (nmb->additional && nmb->header.arcount) {
		debug_nmb_res_rec(nmb->additional,"additional");
	}
}

/*******************************************************************
 Handle "compressed" name pointers.
******************************************************************/

static bool handle_name_ptrs(unsigned char *ubuf,int *offset,int length,
			     bool *got_pointer,int *ret)
{
	int loop_count=0;

	while ((ubuf[*offset] & 0xC0) == 0xC0) {
		if (!*got_pointer)
			(*ret) += 2;
		(*got_pointer)=True;
		(*offset) = ((ubuf[*offset] & ~0xC0)<<8) | ubuf[(*offset)+1];
		if (loop_count++ == 10 ||
				(*offset) < 0 || (*offset)>(length-2)) {
			return False;
		}
	}
	return True;
}

/*******************************************************************
 Parse a nmb name from "compressed" format to something readable
 return the space taken by the name, or 0 if the name is invalid
******************************************************************/

static int parse_nmb_name(char *inbuf,int ofs,int length, struct nmb_name *name)
{
	int m,n=0;
	unsigned char *ubuf = (unsigned char *)inbuf;
	int ret = 0;
	bool got_pointer=False;
	int loop_count=0;
	int offset = ofs;

	if (length - offset < 2)
		return(0);

	/* handle initial name pointers */
	if (!handle_name_ptrs(ubuf,&offset,length,&got_pointer,&ret))
		return(0);

	m = ubuf[offset];

	if (!m)
		return(0);
	if ((m & 0xC0) || offset+m+2 > length)
		return(0);

	memset((char *)name,'\0',sizeof(*name));

	/* the "compressed" part */
	if (!got_pointer)
		ret += m + 2;
	offset++;
	while (m > 0) {
		unsigned char c1,c2;
		c1 = ubuf[offset++]-'A';
		c2 = ubuf[offset++]-'A';
		if ((c1 & 0xF0) || (c2 & 0xF0) || (n > sizeof(name->name)-1))
			return(0);
		name->name[n++] = (c1<<4) | c2;
		m -= 2;
	}
	name->name[n] = 0;

	if (n==MAX_NETBIOSNAME_LEN) {
		/* parse out the name type, its always
		 * in the 16th byte of the name */
		name->name_type = ((unsigned char)name->name[15]) & 0xff;

		/* remove trailing spaces */
		name->name[15] = 0;
		n = 14;
		while (n && name->name[n]==' ')
			name->name[n--] = 0;
	}

	/* now the domain parts (if any) */
	n = 0;
	while (ubuf[offset]) {
		/* we can have pointers within the domain part as well */
		if (!handle_name_ptrs(ubuf,&offset,length,&got_pointer,&ret))
			return(0);

		m = ubuf[offset];
		/*
		 * Don't allow null domain parts.
		 */
		if (!m)
			return(0);
		if (!got_pointer)
			ret += m+1;
		if (n)
			name->scope[n++] = '.';
		if (m+2+offset>length || n+m+1>sizeof(name->scope))
			return(0);
		offset++;
		while (m--)
			name->scope[n++] = (char)ubuf[offset++];

		/*
		 * Watch for malicious loops.
		 */
		if (loop_count++ == 10)
			return 0;
	}
	name->scope[n++] = 0;

	return(ret);
}

/****************************************************************************
 Put a netbios name, padding(s) and a name type into a 16 character buffer.
 name is already in DOS charset.
 [15 bytes name + padding][1 byte name type].
****************************************************************************/

void put_name(char *dest, const char *name, int pad, unsigned int name_type)
{
	size_t len = strlen(name);

	memcpy(dest, name, (len < MAX_NETBIOSNAME_LEN) ?
			len : MAX_NETBIOSNAME_LEN - 1);
	if (len < MAX_NETBIOSNAME_LEN - 1) {
		memset(dest + len, pad, MAX_NETBIOSNAME_LEN - 1 - len);
	}
	dest[MAX_NETBIOSNAME_LEN - 1] = name_type;
}

/*******************************************************************
 Put a compressed nmb name into a buffer. Return the length of the
 compressed name.

 Compressed names are really weird. The "compression" doubles the
 size. The idea is that it also means that compressed names conform
 to the doman name system. See RFC1002.

 If buf == NULL this is a length calculation.
******************************************************************/

static int put_nmb_name(char *buf,int offset,struct nmb_name *name)
{
	int ret,m;
	nstring buf1;
	char *p;

	if (strcmp(name->name,"*") == 0) {
		/* special case for wildcard name */
		put_name(buf1, "*", '\0', name->name_type);
	} else {
		put_name(buf1, name->name, ' ', name->name_type);
	}

	if (buf) {
		buf[offset] = 0x20;
	}

	ret = 34;

	for (m=0;m<MAX_NETBIOSNAME_LEN;m++) {
		if (buf) {
			buf[offset+1+2*m] = 'A' + ((buf1[m]>>4)&0xF);
			buf[offset+2+2*m] = 'A' + (buf1[m]&0xF);
		}
	}
	offset += 33;

	if (buf) {
		buf[offset] = 0;
	}

	if (name->scope[0]) {
		/* XXXX this scope handling needs testing */
		ret += strlen(name->scope) + 1;
		if (buf) {
			safe_strcpy(&buf[offset+1],name->scope,
					sizeof(name->scope));

			p = &buf[offset+1];
			while ((p = strchr_m(p,'.'))) {
				buf[offset] = PTR_DIFF(p,&buf[offset+1]);
				offset += (buf[offset] + 1);
				p = &buf[offset+1];
			}
			buf[offset] = strlen(&buf[offset+1]);
		}
	}

	return ret;
}

/*******************************************************************
 Useful for debugging messages.
******************************************************************/

char *nmb_namestr(const struct nmb_name *n)
{
	fstring name;
	char *result;

	pull_ascii_fstring(name, n->name);
	if (!n->scope[0])
		result = talloc_asprintf(talloc_tos(), "%s<%02x>", name,
					 n->name_type);
	else
		result = talloc_asprintf(talloc_tos(), "%s<%02x>.%s", name,
					 n->name_type, n->scope);

	SMB_ASSERT(result != NULL);
	return result;
}

/*******************************************************************
 Allocate and parse some resource records.
******************************************************************/

static bool parse_alloc_res_rec(char *inbuf,int *offset,int length,
				struct res_rec **recs, int count)
{
	int i;

	*recs = SMB_MALLOC_ARRAY(struct res_rec, count);
	if (!*recs)
		return(False);

	memset((char *)*recs,'\0',sizeof(**recs)*count);

	for (i=0;i<count;i++) {
		int l = parse_nmb_name(inbuf,*offset,length,
				&(*recs)[i].rr_name);
		(*offset) += l;
		if (!l || (*offset)+10 > length) {
			SAFE_FREE(*recs);
			return(False);
		}
		(*recs)[i].rr_type = RSVAL(inbuf,(*offset));
		(*recs)[i].rr_class = RSVAL(inbuf,(*offset)+2);
		(*recs)[i].ttl = RIVAL(inbuf,(*offset)+4);
		(*recs)[i].rdlength = RSVAL(inbuf,(*offset)+8);
		(*offset) += 10;
		if ((*recs)[i].rdlength>sizeof((*recs)[i].rdata) ||
				(*offset)+(*recs)[i].rdlength > length) {
			SAFE_FREE(*recs);
			return(False);
		}
		memcpy((*recs)[i].rdata,inbuf+(*offset),(*recs)[i].rdlength);
		(*offset) += (*recs)[i].rdlength;
	}
	return(True);
}

/*******************************************************************
 Put a resource record into a packet.
 If buf == NULL this is a length calculation.
******************************************************************/

static int put_res_rec(char *buf,int offset,struct res_rec *recs,int count)
{
	int ret=0;
	int i;

	for (i=0;i<count;i++) {
		int l = put_nmb_name(buf,offset,&recs[i].rr_name);
		offset += l;
		ret += l;
		if (buf) {
			RSSVAL(buf,offset,recs[i].rr_type);
			RSSVAL(buf,offset+2,recs[i].rr_class);
			RSIVAL(buf,offset+4,recs[i].ttl);
			RSSVAL(buf,offset+8,recs[i].rdlength);
			memcpy(buf+offset+10,recs[i].rdata,recs[i].rdlength);
		}
		offset += 10+recs[i].rdlength;
		ret += 10+recs[i].rdlength;
	}

	return ret;
}

/*******************************************************************
 Put a compressed name pointer record into a packet.
 If buf == NULL this is a length calculation.
******************************************************************/

static int put_compressed_name_ptr(unsigned char *buf,
				int offset,
				struct res_rec *rec,
				int ptr_offset)
{
	int ret=0;
	if (buf) {
		buf[offset] = (0xC0 | ((ptr_offset >> 8) & 0xFF));
		buf[offset+1] = (ptr_offset & 0xFF);
	}
	offset += 2;
	ret += 2;
	if (buf) {
		RSSVAL(buf,offset,rec->rr_type);
		RSSVAL(buf,offset+2,rec->rr_class);
		RSIVAL(buf,offset+4,rec->ttl);
		RSSVAL(buf,offset+8,rec->rdlength);
		memcpy(buf+offset+10,rec->rdata,rec->rdlength);
	}
	offset += 10+rec->rdlength;
	ret += 10+rec->rdlength;

	return ret;
}

/*******************************************************************
 Parse a dgram packet. Return False if the packet can't be parsed
 or is invalid for some reason, True otherwise.

 This is documented in section 4.4.1 of RFC1002.
******************************************************************/

static bool parse_dgram(char *inbuf,int length,struct dgram_packet *dgram)
{
	int offset;
	int flags;

	memset((char *)dgram,'\0',sizeof(*dgram));

	if (length < 14)
		return(False);

	dgram->header.msg_type = CVAL(inbuf,0);
	flags = CVAL(inbuf,1);
	dgram->header.flags.node_type = (enum node_type)((flags>>2)&3);
	if (flags & 1)
		dgram->header.flags.more = True;
	if (flags & 2)
		dgram->header.flags.first = True;
	dgram->header.dgm_id = RSVAL(inbuf,2);
	putip((char *)&dgram->header.source_ip,inbuf+4);
	dgram->header.source_port = RSVAL(inbuf,8);
	dgram->header.dgm_length = RSVAL(inbuf,10);
	dgram->header.packet_offset = RSVAL(inbuf,12);

	offset = 14;

	if (dgram->header.msg_type == 0x10 ||
			dgram->header.msg_type == 0x11 ||
			dgram->header.msg_type == 0x12) {
		offset += parse_nmb_name(inbuf,offset,length,
				&dgram->source_name);
		offset += parse_nmb_name(inbuf,offset,length,
				&dgram->dest_name);
	}

	if (offset >= length || (length-offset > sizeof(dgram->data)))
		return(False);

	dgram->datasize = length-offset;
	memcpy(dgram->data,inbuf+offset,dgram->datasize);

	/* Paranioa. Ensure the last 2 bytes in the dgram buffer are
	   zero. This should be true anyway, just enforce it for
	   paranioa sake. JRA. */
	SMB_ASSERT(dgram->datasize <= (sizeof(dgram->data)-2));
	memset(&dgram->data[sizeof(dgram->data)-2], '\0', 2);

	return(True);
}

/*******************************************************************
 Parse a nmb packet. Return False if the packet can't be parsed
 or is invalid for some reason, True otherwise.
******************************************************************/

static bool parse_nmb(char *inbuf,int length,struct nmb_packet *nmb)
{
	int nm_flags,offset;

	memset((char *)nmb,'\0',sizeof(*nmb));

	if (length < 12)
		return(False);

	/* parse the header */
	nmb->header.name_trn_id = RSVAL(inbuf,0);

	DEBUG(10,("parse_nmb: packet id = %d\n", nmb->header.name_trn_id));

	nmb->header.opcode = (CVAL(inbuf,2) >> 3) & 0xF;
	nmb->header.response = ((CVAL(inbuf,2)>>7)&1)?True:False;
	nm_flags = ((CVAL(inbuf,2) & 0x7) << 4) + (CVAL(inbuf,3)>>4);
	nmb->header.nm_flags.bcast = (nm_flags&1)?True:False;
	nmb->header.nm_flags.recursion_available = (nm_flags&8)?True:False;
	nmb->header.nm_flags.recursion_desired = (nm_flags&0x10)?True:False;
	nmb->header.nm_flags.trunc = (nm_flags&0x20)?True:False;
	nmb->header.nm_flags.authoritative = (nm_flags&0x40)?True:False;
	nmb->header.rcode = CVAL(inbuf,3) & 0xF;
	nmb->header.qdcount = RSVAL(inbuf,4);
	nmb->header.ancount = RSVAL(inbuf,6);
	nmb->header.nscount = RSVAL(inbuf,8);
	nmb->header.arcount = RSVAL(inbuf,10);

	if (nmb->header.qdcount) {
		offset = parse_nmb_name(inbuf,12,length,
				&nmb->question.question_name);
		if (!offset)
			return(False);

		if (length - (12+offset) < 4)
			return(False);
		nmb->question.question_type = RSVAL(inbuf,12+offset);
		nmb->question.question_class = RSVAL(inbuf,12+offset+2);

		offset += 12+4;
	} else {
		offset = 12;
	}

	/* and any resource records */
	if (nmb->header.ancount &&
			!parse_alloc_res_rec(inbuf,&offset,length,&nmb->answers,
					nmb->header.ancount))
		return(False);

	if (nmb->header.nscount &&
			!parse_alloc_res_rec(inbuf,&offset,length,&nmb->nsrecs,
					nmb->header.nscount))
		return(False);

	if (nmb->header.arcount &&
			!parse_alloc_res_rec(inbuf,&offset,length,
				&nmb->additional, nmb->header.arcount))
		return(False);

	return(True);
}

/*******************************************************************
 'Copy constructor' for an nmb packet.
******************************************************************/

static struct packet_struct *copy_nmb_packet(struct packet_struct *packet)
{
	struct nmb_packet *nmb;
	struct nmb_packet *copy_nmb;
	struct packet_struct *pkt_copy;

	if(( pkt_copy = SMB_MALLOC_P(struct packet_struct)) == NULL) {
		DEBUG(0,("copy_nmb_packet: malloc fail.\n"));
		return NULL;
	}

	/* Structure copy of entire thing. */

	*pkt_copy = *packet;

	/* Ensure this copy is not locked. */
	pkt_copy->locked = False;
	pkt_copy->recv_fd = -1;
	pkt_copy->send_fd = -1;

	/* Ensure this copy has no resource records. */
	nmb = &packet->packet.nmb;
	copy_nmb = &pkt_copy->packet.nmb;

	copy_nmb->answers = NULL;
	copy_nmb->nsrecs = NULL;
	copy_nmb->additional = NULL;

	/* Now copy any resource records. */

	if (nmb->answers) {
		if((copy_nmb->answers = SMB_MALLOC_ARRAY(
				struct res_rec,nmb->header.ancount)) == NULL)
			goto free_and_exit;
		memcpy((char *)copy_nmb->answers, (char *)nmb->answers,
				nmb->header.ancount * sizeof(struct res_rec));
	}
	if (nmb->nsrecs) {
		if((copy_nmb->nsrecs = SMB_MALLOC_ARRAY(
				struct res_rec, nmb->header.nscount)) == NULL)
			goto free_and_exit;
		memcpy((char *)copy_nmb->nsrecs, (char *)nmb->nsrecs,
				nmb->header.nscount * sizeof(struct res_rec));
	}
	if (nmb->additional) {
		if((copy_nmb->additional = SMB_MALLOC_ARRAY(
				struct res_rec, nmb->header.arcount)) == NULL)
			goto free_and_exit;
		memcpy((char *)copy_nmb->additional, (char *)nmb->additional,
				nmb->header.arcount * sizeof(struct res_rec));
	}

	return pkt_copy;

 free_and_exit:

	SAFE_FREE(copy_nmb->answers);
	SAFE_FREE(copy_nmb->nsrecs);
	SAFE_FREE(copy_nmb->additional);
	SAFE_FREE(pkt_copy);

	DEBUG(0,("copy_nmb_packet: malloc fail in resource records.\n"));
	return NULL;
}

/*******************************************************************
  'Copy constructor' for a dgram packet.
******************************************************************/

static struct packet_struct *copy_dgram_packet(struct packet_struct *packet)
{
	struct packet_struct *pkt_copy;

	if(( pkt_copy = SMB_MALLOC_P(struct packet_struct)) == NULL) {
		DEBUG(0,("copy_dgram_packet: malloc fail.\n"));
		return NULL;
	}

	/* Structure copy of entire thing. */

	*pkt_copy = *packet;

	/* Ensure this copy is not locked. */
	pkt_copy->locked = False;
	pkt_copy->recv_fd = -1;
	pkt_copy->send_fd = -1;

	/* There are no additional pointers in a dgram packet,
		we are finished. */
	return pkt_copy;
}

/*******************************************************************
 'Copy constructor' for a generic packet.
******************************************************************/

struct packet_struct *copy_packet(struct packet_struct *packet)
{
	if(packet->packet_type == NMB_PACKET)
		return copy_nmb_packet(packet);
	else if (packet->packet_type == DGRAM_PACKET)
		return copy_dgram_packet(packet);
	return NULL;
}

/*******************************************************************
 Free up any resources associated with an nmb packet.
******************************************************************/

static void free_nmb_packet(struct nmb_packet *nmb)
{
	SAFE_FREE(nmb->answers);
	SAFE_FREE(nmb->nsrecs);
	SAFE_FREE(nmb->additional);
}

/*******************************************************************
 Free up any resources associated with a dgram packet.
******************************************************************/

static void free_dgram_packet(struct dgram_packet *nmb)
{
	/* We have nothing to do for a dgram packet. */
}

/*******************************************************************
 Free up any resources associated with a packet.
******************************************************************/

void free_packet(struct packet_struct *packet)
{
	if (packet->locked)
		return;
	if (packet->packet_type == NMB_PACKET)
		free_nmb_packet(&packet->packet.nmb);
	else if (packet->packet_type == DGRAM_PACKET)
		free_dgram_packet(&packet->packet.dgram);
	ZERO_STRUCTPN(packet);
	SAFE_FREE(packet);
}

int packet_trn_id(struct packet_struct *p)
{
	int result;
	switch (p->packet_type) {
	case NMB_PACKET:
		result = p->packet.nmb.header.name_trn_id;
		break;
	case DGRAM_PACKET:
		result = p->packet.dgram.header.dgm_id;
		break;
	default:
		result = -1;
	}
	return result;
}

/*******************************************************************
 Parse a packet buffer into a packet structure.
******************************************************************/

struct packet_struct *parse_packet(char *buf,int length,
				   enum packet_type packet_type,
				   struct in_addr ip,
				   int port)
{
	struct packet_struct *p;
	bool ok=False;

	p = SMB_MALLOC_P(struct packet_struct);
	if (!p)
		return(NULL);

	ZERO_STRUCTP(p);	/* initialize for possible padding */

	p->next = NULL;
	p->prev = NULL;
	p->ip = ip;
	p->port = port;
	p->locked = False;
	p->timestamp = time(NULL);
	p->packet_type = packet_type;

	switch (packet_type) {
	case NMB_PACKET:
		ok = parse_nmb(buf,length,&p->packet.nmb);
		break;

	case DGRAM_PACKET:
		ok = parse_dgram(buf,length,&p->packet.dgram);
		break;
	}

	if (!ok) {
		free_packet(p);
		return NULL;
	}

	return p;
}

/*******************************************************************
 Read a packet from a socket and parse it, returning a packet ready
 to be used or put on the queue. This assumes a UDP socket.
******************************************************************/

struct packet_struct *read_packet(int fd,enum packet_type packet_type)
{
	struct packet_struct *packet;
	struct sockaddr_storage sa;
	struct sockaddr_in *si = (struct sockaddr_in *)&sa;
	char buf[MAX_DGRAM_SIZE];
	int length;

	length = read_udp_v4_socket(fd,buf,sizeof(buf),&sa);
	if (length < MIN_DGRAM_SIZE || sa.ss_family != AF_INET) {
		return NULL;
	}

	packet = parse_packet(buf,
			length,
			packet_type,
			si->sin_addr,
			ntohs(si->sin_port));
	if (!packet)
		return NULL;

	packet->recv_fd = fd;
	packet->send_fd = -1;

	DEBUG(5,("Received a packet of len %d from (%s) port %d\n",
		 length, inet_ntoa(packet->ip), packet->port ) );

	return(packet);
}

/*******************************************************************
 Send a udp packet on a already open socket.
******************************************************************/

static bool send_udp(int fd,char *buf,int len,struct in_addr ip,int port)
{
	bool ret = False;
	int i;
	struct sockaddr_in sock_out;

	/* set the address and port */
	memset((char *)&sock_out,'\0',sizeof(sock_out));
	putip((char *)&sock_out.sin_addr,(char *)&ip);
	sock_out.sin_port = htons( port );
	sock_out.sin_family = AF_INET;

	DEBUG( 5, ( "Sending a packet of len %d to (%s) on port %d\n",
			len, inet_ntoa(ip), port ) );

	/*
	 * Patch to fix asynch error notifications from Linux kernel.
	 */

	for (i = 0; i < 5; i++) {
		ret = (sendto(fd,buf,len,0,(struct sockaddr *)&sock_out,
					sizeof(sock_out)) >= 0);
		if (ret || errno != ECONNREFUSED)
			break;
	}

	if (!ret)
		DEBUG(0,("Packet send failed to %s(%d) ERRNO=%s\n",
			inet_ntoa(ip),port,strerror(errno)));

	return(ret);
}

/*******************************************************************
 Build a dgram packet ready for sending.
 If buf == NULL this is a length calculation.
******************************************************************/

static int build_dgram(char *buf, size_t len, struct dgram_packet *dgram)
{
	unsigned char *ubuf = (unsigned char *)buf;
	int offset=0;

	/* put in the header */
	if (buf) {
		ubuf[0] = dgram->header.msg_type;
		ubuf[1] = (((int)dgram->header.flags.node_type)<<2);
		if (dgram->header.flags.more)
			ubuf[1] |= 1;
		if (dgram->header.flags.first)
			ubuf[1] |= 2;
		RSSVAL(ubuf,2,dgram->header.dgm_id);
		putip(ubuf+4,(char *)&dgram->header.source_ip);
		RSSVAL(ubuf,8,dgram->header.source_port);
		RSSVAL(ubuf,12,dgram->header.packet_offset);
	}

	offset = 14;

	if (dgram->header.msg_type == 0x10 ||
			dgram->header.msg_type == 0x11 ||
			dgram->header.msg_type == 0x12) {
		offset += put_nmb_name((char *)ubuf,offset,&dgram->source_name);
		offset += put_nmb_name((char *)ubuf,offset,&dgram->dest_name);
	}

	if (buf) {
		memcpy(ubuf+offset,dgram->data,dgram->datasize);
	}
	offset += dgram->datasize;

	/* automatically set the dgm_length
	 * NOTE: RFC1002 says the dgm_length does *not*
	 *       include the fourteen-byte header. crh
	 */
	dgram->header.dgm_length = (offset - 14);
	if (buf) {
		RSSVAL(ubuf,10,dgram->header.dgm_length);
	}

	return offset;
}

/*******************************************************************
 Build a nmb name
*******************************************************************/

void make_nmb_name( struct nmb_name *n, const char *name, int type)
{
	fstring unix_name;
	memset( (char *)n, '\0', sizeof(struct nmb_name) );
	fstrcpy(unix_name, name);
	strupper_m(unix_name);
	push_ascii(n->name, unix_name, sizeof(n->name), STR_TERMINATE);
	n->name_type = (unsigned int)type & 0xFF;
	push_ascii(n->scope,  global_scope(), 64, STR_TERMINATE);
}

/*******************************************************************
  Compare two nmb names
******************************************************************/

bool nmb_name_equal(struct nmb_name *n1, struct nmb_name *n2)
{
	return ((n1->name_type == n2->name_type) &&
		strequal(n1->name ,n2->name ) &&
		strequal(n1->scope,n2->scope));
}

/*******************************************************************
 Build a nmb packet ready for sending.
 If buf == NULL this is a length calculation.
******************************************************************/

static int build_nmb(char *buf, size_t len, struct nmb_packet *nmb)
{
	unsigned char *ubuf = (unsigned char *)buf;
	int offset=0;

	if (len && len < 12) {
		return 0;
	}

	/* put in the header */
	if (buf) {
		RSSVAL(ubuf,offset,nmb->header.name_trn_id);
		ubuf[offset+2] = (nmb->header.opcode & 0xF) << 3;
		if (nmb->header.response)
			ubuf[offset+2] |= (1<<7);
		if (nmb->header.nm_flags.authoritative &&
				nmb->header.response)
			ubuf[offset+2] |= 0x4;
		if (nmb->header.nm_flags.trunc)
			ubuf[offset+2] |= 0x2;
		if (nmb->header.nm_flags.recursion_desired)
			ubuf[offset+2] |= 0x1;
		if (nmb->header.nm_flags.recursion_available &&
				nmb->header.response)
			ubuf[offset+3] |= 0x80;
		if (nmb->header.nm_flags.bcast)
			ubuf[offset+3] |= 0x10;
		ubuf[offset+3] |= (nmb->header.rcode & 0xF);

		RSSVAL(ubuf,offset+4,nmb->header.qdcount);
		RSSVAL(ubuf,offset+6,nmb->header.ancount);
		RSSVAL(ubuf,offset+8,nmb->header.nscount);
		RSSVAL(ubuf,offset+10,nmb->header.arcount);
	}

	offset += 12;
	if (nmb->header.qdcount) {
		/* XXXX this doesn't handle a qdcount of > 1 */
		if (len) {
			/* Length check. */
			int extra = put_nmb_name(NULL,offset,
					&nmb->question.question_name);
			if (offset + extra > len) {
				return 0;
			}
		}
		offset += put_nmb_name((char *)ubuf,offset,
				&nmb->question.question_name);
		if (buf) {
			RSSVAL(ubuf,offset,nmb->question.question_type);
			RSSVAL(ubuf,offset+2,nmb->question.question_class);
		}
		offset += 4;
	}

	if (nmb->header.ancount) {
		if (len) {
			/* Length check. */
			int extra = put_res_rec(NULL,offset,nmb->answers,
					nmb->header.ancount);
			if (offset + extra > len) {
				return 0;
			}
		}
		offset += put_res_rec((char *)ubuf,offset,nmb->answers,
				nmb->header.ancount);
	}

	if (nmb->header.nscount) {
		if (len) {
			/* Length check. */
			int extra = put_res_rec(NULL,offset,nmb->nsrecs,
				nmb->header.nscount);
			if (offset + extra > len) {
				return 0;
			}
		}
		offset += put_res_rec((char *)ubuf,offset,nmb->nsrecs,
				nmb->header.nscount);
	}

	/*
	 * The spec says we must put compressed name pointers
	 * in the following outgoing packets :
	 * NAME_REGISTRATION_REQUEST, NAME_REFRESH_REQUEST,
	 * NAME_RELEASE_REQUEST.
	 */

	if((nmb->header.response == False) &&
		((nmb->header.opcode == NMB_NAME_REG_OPCODE) ||
		(nmb->header.opcode == NMB_NAME_RELEASE_OPCODE) ||
		(nmb->header.opcode == NMB_NAME_REFRESH_OPCODE_8) ||
		(nmb->header.opcode == NMB_NAME_REFRESH_OPCODE_9) ||
		(nmb->header.opcode == NMB_NAME_MULTIHOMED_REG_OPCODE)) &&
		(nmb->header.arcount == 1)) {

		if (len) {
			/* Length check. */
			int extra = put_compressed_name_ptr(NULL,offset,
					nmb->additional,12);
			if (offset + extra > len) {
				return 0;
			}
		}
		offset += put_compressed_name_ptr(ubuf,offset,
				nmb->additional,12);
	} else if (nmb->header.arcount) {
		if (len) {
			/* Length check. */
			int extra = put_res_rec(NULL,offset,nmb->additional,
				nmb->header.arcount);
			if (offset + extra > len) {
				return 0;
			}
		}
		offset += put_res_rec((char *)ubuf,offset,nmb->additional,
			nmb->header.arcount);
	}
	return offset;
}

/*******************************************************************
 Linearise a packet.
******************************************************************/

int build_packet(char *buf, size_t buflen, struct packet_struct *p)
{
	int len = 0;

	switch (p->packet_type) {
	case NMB_PACKET:
		len = build_nmb(buf,buflen,&p->packet.nmb);
		break;

	case DGRAM_PACKET:
		len = build_dgram(buf,buflen,&p->packet.dgram);
		break;
	}

	return len;
}

/*******************************************************************
 Send a packet_struct.
******************************************************************/

bool send_packet(struct packet_struct *p)
{
	char buf[1024];
	int len=0;

	memset(buf,'\0',sizeof(buf));

	len = build_packet(buf, sizeof(buf), p);

	if (!len)
		return(False);

	return(send_udp(p->send_fd,buf,len,p->ip,p->port));
}

/****************************************************************************
 Receive a UDP/138 packet either via UDP or from the unexpected packet
 queue. The packet must be a reply packet and have the specified mailslot name
 The timeout is in milliseconds.
***************************************************************************/

/****************************************************************************
 See if a datagram has the right mailslot name.
***************************************************************************/

bool match_mailslot_name(struct packet_struct *p, const char *mailslot_name)
{
	struct dgram_packet *dgram = &p->packet.dgram;
	char *buf;

	buf = &dgram->data[0];
	buf -= 4;

	buf = smb_buf(buf);

	if (memcmp(buf, mailslot_name, strlen(mailslot_name)+1) == 0) {
		return True;
	}

	return False;
}

/****************************************************************************
 Return the number of bits that match between two len character buffers
***************************************************************************/

int matching_len_bits(unsigned char *p1, unsigned char *p2, size_t len)
{
	size_t i, j;
	int ret = 0;
	for (i=0; i<len; i++) {
		if (p1[i] != p2[i])
			break;
		ret += 8;
	}

	if (i==len)
		return ret;

	for (j=0; j<8; j++) {
		if ((p1[i] & (1<<(7-j))) != (p2[i] & (1<<(7-j))))
			break;
		ret++;
	}

	return ret;
}

static unsigned char sort_ip[4];

/****************************************************************************
 Compare two query reply records.
***************************************************************************/

static int name_query_comp(unsigned char *p1, unsigned char *p2)
{
	return matching_len_bits(p2+2, sort_ip, 4) -
		matching_len_bits(p1+2, sort_ip, 4);
}

/****************************************************************************
 Sort a set of 6 byte name query response records so that the IPs that
 have the most leading bits in common with the specified address come first.
***************************************************************************/

void sort_query_replies(char *data, int n, struct in_addr ip)
{
	if (n <= 1)
		return;

	putip(sort_ip, (char *)&ip);

	/* TODO:
	   this can't use TYPESAFE_QSORT() as the types are wrong.
	   It should be fixed to use a real type instead of char*
	*/
	qsort(data, n, 6, QSORT_CAST name_query_comp);
}

/****************************************************************************
 Interpret the weird netbios "name" into a unix fstring. Return the name type.
 Returns -1 on error.
****************************************************************************/

static int name_interpret(unsigned char *buf, size_t buf_len,
		unsigned char *in, fstring name)
{
	unsigned char *end_ptr = buf + buf_len;
	int ret;
	unsigned int len;
	fstring out_string;
	unsigned char *out = (unsigned char *)out_string;

	*out=0;

	if (in >= end_ptr) {
		return -1;
	}
	len = (*in++) / 2;

	if (len<1) {
		return -1;
	}

	while (len--) {
		if (&in[1] >= end_ptr) {
			return -1;
		}
		if (in[0] < 'A' || in[0] > 'P' || in[1] < 'A' || in[1] > 'P') {
			*out = 0;
			return(0);
		}
		*out = ((in[0]-'A')<<4) + (in[1]-'A');
		in += 2;
		out++;
		if (PTR_DIFF(out,out_string) >= sizeof(fstring)) {
			return -1;
		}
	}
	ret = out[-1];
	out[-1] = 0;

	pull_ascii_fstring(name, out_string);

	return(ret);
}

/****************************************************************************
 Mangle a name into netbios format.
 Note:  <Out> must be (33 + strlen(scope) + 2) bytes long, at minimum.
****************************************************************************/

char *name_mangle(TALLOC_CTX *mem_ctx, const char *In, char name_type)
{
	int   i;
	int   len;
	nstring buf;
	char *result;
	char *p;

	result = talloc_array(mem_ctx, char, 33 + strlen(global_scope()) + 2);
	if (result == NULL) {
		return NULL;
	}
	p = result;

	/* Safely copy the input string, In, into buf[]. */
	if (strcmp(In,"*") == 0)
		put_name(buf, "*", '\0', 0x00);
	else {
		/* We use an fstring here as mb dos names can expend x3 when
		   going to utf8. */
		fstring buf_unix;
		nstring buf_dos;

		pull_ascii_fstring(buf_unix, In);
		strupper_m(buf_unix);

		push_ascii_nstring(buf_dos, buf_unix);
		put_name(buf, buf_dos, ' ', name_type);
	}

	/* Place the length of the first field into the output buffer. */
	p[0] = 32;
	p++;

	/* Now convert the name to the rfc1001/1002 format. */
	for( i = 0; i < MAX_NETBIOSNAME_LEN; i++ ) {
		p[i*2]     = ( (buf[i] >> 4) & 0x000F ) + 'A';
		p[(i*2)+1] = (buf[i] & 0x000F) + 'A';
	}
	p += 32;
	p[0] = '\0';

	/* Add the scope string. */
	for( i = 0, len = 0; *(global_scope()) != '\0'; i++, len++ ) {
		switch( (global_scope())[i] ) {
			case '\0':
				p[0] = len;
				if( len > 0 )
					p[len+1] = 0;
				return result;
			case '.':
				p[0] = len;
				p   += (len + 1);
				len  = -1;
				break;
			default:
				p[len+1] = (global_scope())[i];
				break;
		}
	}

	return result;
}

/****************************************************************************
 Find a pointer to a netbios name.
****************************************************************************/

static unsigned char *name_ptr(unsigned char *buf, size_t buf_len, unsigned int ofs)
{
	unsigned char c = 0;

	if (ofs > buf_len || buf_len < 1) {
		return NULL;
	}

	c = *(unsigned char *)(buf+ofs);
	if ((c & 0xC0) == 0xC0) {
		uint16 l = 0;

		if (ofs > buf_len - 1) {
			return NULL;
		}
		l = RSVAL(buf, ofs) & 0x3FFF;
		if (l > buf_len) {
			return NULL;
		}
		DEBUG(5,("name ptr to pos %d from %d is %s\n",l,ofs,buf+l));
		return(buf + l);
	} else {
		return(buf+ofs);
	}
}

/****************************************************************************
 Extract a netbios name from a buf (into a unix string) return name type.
 Returns -1 on error.
****************************************************************************/

int name_extract(unsigned char *buf, size_t buf_len, unsigned int ofs, fstring name)
{
	unsigned char *p = name_ptr(buf,buf_len,ofs);

	name[0] = '\0';
	if (p == NULL) {
		return -1;
	}
	return(name_interpret(buf,buf_len,p,name));
}

/****************************************************************************
 Return the total storage length of a mangled name.
 Returns -1 on error.
****************************************************************************/

int name_len(unsigned char *s1, size_t buf_len)
{
	/* NOTE: this argument _must_ be unsigned */
	unsigned char *s = (unsigned char *)s1;
	int len = 0;

	if (buf_len < 1) {
		return -1;
	}
	/* If the two high bits of the byte are set, return 2. */
	if (0xC0 == (*s & 0xC0)) {
		if (buf_len < 2) {
			return -1;
		}
		return(2);
	}

	/* Add up the length bytes. */
	for (len = 1; (*s); s += (*s) + 1) {
		len += *s + 1;
		if (len > buf_len) {
			return -1;
		}
	}

	return(len);
}
