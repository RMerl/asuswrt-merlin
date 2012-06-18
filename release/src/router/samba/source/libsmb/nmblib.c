/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios library routines
   Copyright (C) Andrew Tridgell 1994-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
*/

#include "includes.h"

extern int DEBUGLEVEL;

int num_good_sends = 0;
int num_good_receives = 0;

static struct opcode_names {
	char *nmb_opcode_name;
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
 * Lookup a nmb opcode name.
 ****************************************************************************/
static char *lookup_opcode_name( int opcode )
{
  struct opcode_names *op_namep;
  int i;

  for(i = 0; nmb_header_opcode_names[i].nmb_opcode_name != 0; i++) {
    op_namep = &nmb_header_opcode_names[i];
    if(opcode == op_namep->opcode)
      return op_namep->nmb_opcode_name;
  }
  return "<unknown opcode>";
}

/****************************************************************************
  print out a res_rec structure
  ****************************************************************************/
static void debug_nmb_res_rec(struct res_rec *res, char *hdr)
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

  for (i = 0; i < res->rdlength; i+= 16)
    {
      DEBUGADD(4, ("    %s %3x char ", hdr, i));

      for (j = 0; j < 16; j++)
	{
	  unsigned char x = res->rdata[i+j];
	  if (x < 32 || x > 127) x = '.';
	  
	  if (i+j >= res->rdlength) break;
	  DEBUGADD(4, ("%c", x));
	}
      
      DEBUGADD(4, ("   hex "));

      for (j = 0; j < 16; j++)
	{
	  if (i+j >= res->rdlength) break;
	  DEBUGADD(4, ("%02X", (unsigned char)res->rdata[i+j]));
	}
      
      DEBUGADD(4, ("\n"));
    }
}

/****************************************************************************
  process a nmb packet
  ****************************************************************************/
void debug_nmb_packet(struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;

  if( DEBUGLVL( 4 ) )
    {
    dbgtext( "nmb packet from %s(%d) header: id=%d opcode=%s(%d) response=%s\n",
             inet_ntoa(p->ip), p->port,
             nmb->header.name_trn_id,
             lookup_opcode_name(nmb->header.opcode),
             nmb->header.opcode,
             BOOLSTR(nmb->header.response) );
    dbgtext( "    header: flags: bcast=%s rec_avail=%s rec_des=%s trunc=%s auth=%s\n",
             BOOLSTR(nmb->header.nm_flags.bcast),
             BOOLSTR(nmb->header.nm_flags.recursion_available),
             BOOLSTR(nmb->header.nm_flags.recursion_desired),
             BOOLSTR(nmb->header.nm_flags.trunc),
             BOOLSTR(nmb->header.nm_flags.authoritative) );
    dbgtext( "    header: rcode=%d qdcount=%d ancount=%d nscount=%d arcount=%d\n",
             nmb->header.rcode,
             nmb->header.qdcount,
             nmb->header.ancount,
             nmb->header.nscount,
             nmb->header.arcount );
    }

  if (nmb->header.qdcount)
    {
      DEBUGADD( 4, ( "    question: q_name=%s q_type=%d q_class=%d\n",
                     nmb_namestr(&nmb->question.question_name),
                     nmb->question.question_type,
                     nmb->question.question_class) );
    }

  if (nmb->answers && nmb->header.ancount)
    {
      debug_nmb_res_rec(nmb->answers,"answers");
    }
  if (nmb->nsrecs && nmb->header.nscount)
    {
      debug_nmb_res_rec(nmb->nsrecs,"nsrecs");
    }
  if (nmb->additional && nmb->header.arcount)
    {
      debug_nmb_res_rec(nmb->additional,"additional");
    }
}

/*******************************************************************
  handle "compressed" name pointers
  ******************************************************************/
static BOOL handle_name_ptrs(unsigned char *ubuf,int *offset,int length,
			     BOOL *got_pointer,int *ret)
{
  int loop_count=0;
  
  while ((ubuf[*offset] & 0xC0) == 0xC0) {
    if (!*got_pointer) (*ret) += 2;
    (*got_pointer)=True;
    (*offset) = ((ubuf[*offset] & ~0xC0)<<8) | ubuf[(*offset)+1];
    if (loop_count++ == 10 || (*offset) < 0 || (*offset)>(length-2)) {
      return(False);
    }
  }
  return(True);
}

/*******************************************************************
  parse a nmb name from "compressed" format to something readable
  return the space taken by the name, or 0 if the name is invalid
  ******************************************************************/
static int parse_nmb_name(char *inbuf,int offset,int length, struct nmb_name *name)
{
  int m,n=0;
  unsigned char *ubuf = (unsigned char *)inbuf;
  int ret = 0;
  BOOL got_pointer=False;
  int loop_count=0;

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

  if (n==16) {
    /* parse out the name type, 
       its always in the 16th byte of the name */
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


/*******************************************************************
  put a compressed nmb name into a buffer. return the length of the
  compressed name

  compressed names are really weird. The "compression" doubles the
  size. The idea is that it also means that compressed names conform
  to the doman name system. See RFC1002.
  ******************************************************************/
static int put_nmb_name(char *buf,int offset,struct nmb_name *name)
{
  int ret,m;
  fstring buf1;
  char *p;

  if (strcmp(name->name,"*") == 0) {
    /* special case for wildcard name */
    memset(buf1,'\0',20);
    buf1[0] = '*';
    buf1[15] = name->name_type;
  } else {
    slprintf(buf1, sizeof(buf1) - 1,"%-15.15s%c",name->name,name->name_type);
  }

  buf[offset] = 0x20;

  ret = 34;

  for (m=0;m<16;m++) {
    buf[offset+1+2*m] = 'A' + ((buf1[m]>>4)&0xF);
    buf[offset+2+2*m] = 'A' + (buf1[m]&0xF);
  }
  offset += 33;

  buf[offset] = 0;

  if (name->scope[0]) {
    /* XXXX this scope handling needs testing */
    ret += strlen(name->scope) + 1;
    pstrcpy(&buf[offset+1],name->scope);  
  
    p = &buf[offset+1];
    while ((p = strchr(p,'.'))) {
      buf[offset] = PTR_DIFF(p,&buf[offset+1]);
      offset += (buf[offset] + 1);
      p = &buf[offset+1];
    }
    buf[offset] = strlen(&buf[offset+1]);
  }

  return(ret);
}

/*******************************************************************
  useful for debugging messages
  ******************************************************************/
char *nmb_namestr(struct nmb_name *n)
{
  static int i=0;
  static fstring ret[4];
  char *p = ret[i];

  if (!n->scope[0])
    slprintf(p,sizeof(fstring)-1, "%s<%02x>",n->name,n->name_type);
  else
    slprintf(p,sizeof(fstring)-1, "%s<%02x>.%s",n->name,n->name_type,n->scope);

  i = (i+1)%4;
  return(p);
}

/*******************************************************************
  allocate and parse some resource records
  ******************************************************************/
static BOOL parse_alloc_res_rec(char *inbuf,int *offset,int length,
				struct res_rec **recs, int count)
{
  int i;
  *recs = (struct res_rec *)malloc(sizeof(**recs)*count);
  if (!*recs) return(False);

  memset((char *)*recs,'\0',sizeof(**recs)*count);

  for (i=0;i<count;i++) {
    int l = parse_nmb_name(inbuf,*offset,length,&(*recs)[i].rr_name);
    (*offset) += l;
    if (!l || (*offset)+10 > length) {
      free(*recs);
      *recs = NULL;
      return(False);
    }
    (*recs)[i].rr_type = RSVAL(inbuf,(*offset));
    (*recs)[i].rr_class = RSVAL(inbuf,(*offset)+2);
    (*recs)[i].ttl = RIVAL(inbuf,(*offset)+4);
    (*recs)[i].rdlength = RSVAL(inbuf,(*offset)+8);
    (*offset) += 10;
    if ((*recs)[i].rdlength>sizeof((*recs)[i].rdata) || 
	(*offset)+(*recs)[i].rdlength > length) {
      free(*recs);
      *recs = NULL;
      return(False);
    }
    memcpy((*recs)[i].rdata,inbuf+(*offset),(*recs)[i].rdlength);
    (*offset) += (*recs)[i].rdlength;    
  }
  return(True);
}

/*******************************************************************
  put a resource record into a packet
  ******************************************************************/
static int put_res_rec(char *buf,int offset,struct res_rec *recs,int count)
{
  int ret=0;
  int i;

  for (i=0;i<count;i++) {
    int l = put_nmb_name(buf,offset,&recs[i].rr_name);
    offset += l;
    ret += l;
    RSSVAL(buf,offset,recs[i].rr_type);
    RSSVAL(buf,offset+2,recs[i].rr_class);
    RSIVAL(buf,offset+4,recs[i].ttl);
    RSSVAL(buf,offset+8,recs[i].rdlength);
    memcpy(buf+offset+10,recs[i].rdata,recs[i].rdlength);
    offset += 10+recs[i].rdlength;
    ret += 10+recs[i].rdlength;
  }

  return(ret);
}

/*******************************************************************
  put a compressed name pointer record into a packet
  ******************************************************************/
static int put_compressed_name_ptr(unsigned char *buf,int offset,struct res_rec *rec,int ptr_offset)
{  
  int ret=0;
  buf[offset] = (0xC0 | ((ptr_offset >> 8) & 0xFF));
  buf[offset+1] = (ptr_offset & 0xFF);
  offset += 2;
  ret += 2;
  RSSVAL(buf,offset,rec->rr_type);
  RSSVAL(buf,offset+2,rec->rr_class);
  RSIVAL(buf,offset+4,rec->ttl);
  RSSVAL(buf,offset+8,rec->rdlength);
  memcpy(buf+offset+10,rec->rdata,rec->rdlength);
  offset += 10+rec->rdlength;
  ret += 10+rec->rdlength;
    
  return(ret);
}

/*******************************************************************
  parse a dgram packet. Return False if the packet can't be parsed 
  or is invalid for some reason, True otherwise 

  this is documented in section 4.4.1 of RFC1002
  ******************************************************************/
static BOOL parse_dgram(char *inbuf,int length,struct dgram_packet *dgram)
{
  int offset;
  int flags;

  memset((char *)dgram,'\0',sizeof(*dgram));

  if (length < 14) return(False);

  dgram->header.msg_type = CVAL(inbuf,0);
  flags = CVAL(inbuf,1);
  dgram->header.flags.node_type = (enum node_type)((flags>>2)&3);
  if (flags & 1) dgram->header.flags.more = True;
  if (flags & 2) dgram->header.flags.first = True;
  dgram->header.dgm_id = RSVAL(inbuf,2);
  putip((char *)&dgram->header.source_ip,inbuf+4);
  dgram->header.source_port = RSVAL(inbuf,8);
  dgram->header.dgm_length = RSVAL(inbuf,10);
  dgram->header.packet_offset = RSVAL(inbuf,12);

  offset = 14;

  if (dgram->header.msg_type == 0x10 ||
      dgram->header.msg_type == 0x11 ||
      dgram->header.msg_type == 0x12) {      
    offset += parse_nmb_name(inbuf,offset,length,&dgram->source_name);
    offset += parse_nmb_name(inbuf,offset,length,&dgram->dest_name);
  }

  if (offset >= length || (length-offset > sizeof(dgram->data))) 
    return(False);

  dgram->datasize = length-offset;
  memcpy(dgram->data,inbuf+offset,dgram->datasize);

  return(True);
}


/*******************************************************************
  parse a nmb packet. Return False if the packet can't be parsed 
  or is invalid for some reason, True otherwise 
  ******************************************************************/
static BOOL parse_nmb(char *inbuf,int length,struct nmb_packet *nmb)
{
  int nm_flags,offset;

  memset((char *)nmb,'\0',sizeof(*nmb));

  if (length < 12) return(False);

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
    offset = parse_nmb_name(inbuf,12,length,&nmb->question.question_name);
    if (!offset) return(False);

    if (length - (12+offset) < 4) return(False);
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
      !parse_alloc_res_rec(inbuf,&offset,length,&nmb->additional,
			   nmb->header.arcount))
    return(False);

  return(True);
}

/*******************************************************************
  'Copy constructor' for an nmb packet
  ******************************************************************/
static struct packet_struct *copy_nmb_packet(struct packet_struct *packet)
{  
  struct nmb_packet *nmb;
  struct nmb_packet *copy_nmb;
  struct packet_struct *pkt_copy;

  if(( pkt_copy = (struct packet_struct *)malloc(sizeof(*packet))) == NULL)
  {
    DEBUG(0,("copy_nmb_packet: malloc fail.\n"));
    return NULL;
  }

  /* Structure copy of entire thing. */

  *pkt_copy = *packet;

  /* Ensure this copy is not locked. */
  pkt_copy->locked = False;

  /* Ensure this copy has no resource records. */
  nmb = &packet->packet.nmb;
  copy_nmb = &pkt_copy->packet.nmb;

  copy_nmb->answers = NULL;
  copy_nmb->nsrecs = NULL;
  copy_nmb->additional = NULL;

  /* Now copy any resource records. */

  if (nmb->answers)
  {
    if((copy_nmb->answers = (struct res_rec *)
                  malloc(nmb->header.ancount * sizeof(struct res_rec))) == NULL)
      goto free_and_exit;
    memcpy((char *)copy_nmb->answers, (char *)nmb->answers, 
           nmb->header.ancount * sizeof(struct res_rec));
  }
  if (nmb->nsrecs)
  {
    if((copy_nmb->nsrecs = (struct res_rec *)
                  malloc(nmb->header.nscount * sizeof(struct res_rec))) == NULL)
      goto free_and_exit;
    memcpy((char *)copy_nmb->nsrecs, (char *)nmb->nsrecs, 
           nmb->header.nscount * sizeof(struct res_rec));
  }
  if (nmb->additional)
  {
    if((copy_nmb->additional = (struct res_rec *)
                  malloc(nmb->header.arcount * sizeof(struct res_rec))) == NULL)
      goto free_and_exit;
    memcpy((char *)copy_nmb->additional, (char *)nmb->additional, 
           nmb->header.arcount * sizeof(struct res_rec));
  }

  return pkt_copy;

free_and_exit:

  if(copy_nmb->answers) {
    free((char *)copy_nmb->answers);
    copy_nmb->answers = NULL;
  }
  if(copy_nmb->nsrecs) {
    free((char *)copy_nmb->nsrecs);
    copy_nmb->nsrecs = NULL;
  }
  if(copy_nmb->additional) {
    free((char *)copy_nmb->additional);
    copy_nmb->additional = NULL;
  }
  free((char *)pkt_copy);

  DEBUG(0,("copy_nmb_packet: malloc fail in resource records.\n"));
  return NULL;
}

/*******************************************************************
  'Copy constructor' for a dgram packet
  ******************************************************************/
static struct packet_struct *copy_dgram_packet(struct packet_struct *packet)
{ 
  struct packet_struct *pkt_copy;

  if(( pkt_copy = (struct packet_struct *)malloc(sizeof(*packet))) == NULL)
  {
    DEBUG(0,("copy_dgram_packet: malloc fail.\n"));
    return NULL;
  }

  /* Structure copy of entire thing. */

  *pkt_copy = *packet;

  /* Ensure this copy is not locked. */
  pkt_copy->locked = False;

  /* There are no additional pointers in a dgram packet,
     we are finished. */
  return pkt_copy;
}

/*******************************************************************
  'Copy constructor' for a generic packet
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
  free up any resources associated with an nmb packet
  ******************************************************************/
static void free_nmb_packet(struct nmb_packet *nmb)
{  
  if (nmb->answers) {
    free(nmb->answers);
    nmb->answers = NULL;
  }
  if (nmb->nsrecs) {
    free(nmb->nsrecs);
    nmb->nsrecs = NULL;
  }
  if (nmb->additional) {
    free(nmb->additional);
    nmb->additional = NULL;
  }
}

/*******************************************************************
  free up any resources associated with a dgram packet
  ******************************************************************/
static void free_dgram_packet(struct dgram_packet *nmb)
{  
  /* We have nothing to do for a dgram packet. */
}

/*******************************************************************
  free up any resources associated with a packet
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
  free(packet);
}

/*******************************************************************
  read a packet from a socket and parse it, returning a packet ready
  to be used or put on the queue. This assumes a UDP socket
  ******************************************************************/
struct packet_struct *read_packet(int fd,enum packet_type packet_type)
{
  extern struct in_addr lastip;
  extern int lastport;
  struct packet_struct *packet;
  char buf[MAX_DGRAM_SIZE];
  int length;
  BOOL ok=False;
  
  length = read_udp_socket(fd,buf,sizeof(buf));
  if (length < MIN_DGRAM_SIZE) return(NULL);

  packet = (struct packet_struct *)malloc(sizeof(*packet));
  if (!packet) return(NULL);

  packet->next = NULL;
  packet->prev = NULL;
  packet->ip = lastip;
  packet->port = lastport;
  packet->fd = fd;
  packet->locked = False;
  packet->timestamp = time(NULL);
  packet->packet_type = packet_type;
  switch (packet_type) 
    {
    case NMB_PACKET:
      ok = parse_nmb(buf,length,&packet->packet.nmb);
      break;

    case DGRAM_PACKET:
      ok = parse_dgram(buf,length,&packet->packet.dgram);
      break;
    }
  if (!ok) {
    DEBUG(10,("read_packet: discarding packet id = %d\n", 
                 packet->packet.nmb.header.name_trn_id));
    free_packet(packet);
    return(NULL);
  }

  num_good_receives++;

  DEBUG(5,("Received a packet of len %d from (%s) port %d\n",
	    length, inet_ntoa(packet->ip), packet->port ) );

  return(packet);
}
					 

/*******************************************************************
  send a udp packet on a already open socket
  ******************************************************************/
static BOOL send_udp(int fd,char *buf,int len,struct in_addr ip,int port)
{
  BOOL ret;
  struct sockaddr_in sock_out;

  /* set the address and port */
  memset((char *)&sock_out,'\0',sizeof(sock_out));
  putip((char *)&sock_out.sin_addr,(char *)&ip);
  sock_out.sin_port = htons( port );
  sock_out.sin_family = AF_INET;
  
  DEBUG( 5, ( "Sending a packet of len %d to (%s) on port %d\n",
	      len, inet_ntoa(ip), port ) );
	
  ret = (sendto(fd,buf,len,0,(struct sockaddr *)&sock_out,
		sizeof(sock_out)) >= 0);

  if (!ret)
    DEBUG(0,("Packet send failed to %s(%d) ERRNO=%s\n",
	     inet_ntoa(ip),port,strerror(errno)));

  if (ret)
    num_good_sends++;

  return(ret);
}

/*******************************************************************
  build a dgram packet ready for sending

  XXXX This currently doesn't handle packets too big for one
  datagram. It should split them and use the packet_offset, more and
  first flags to handle the fragmentation. Yuck.
  ******************************************************************/
static int build_dgram(char *buf,struct packet_struct *p)
{
  struct dgram_packet *dgram = &p->packet.dgram;
  unsigned char *ubuf = (unsigned char *)buf;
  int offset=0;

  /* put in the header */
  ubuf[0] = dgram->header.msg_type;
  ubuf[1] = (((int)dgram->header.flags.node_type)<<2);
  if (dgram->header.flags.more) ubuf[1] |= 1;
  if (dgram->header.flags.first) ubuf[1] |= 2;
  RSSVAL(ubuf,2,dgram->header.dgm_id);
  putip(ubuf+4,(char *)&dgram->header.source_ip);
  RSSVAL(ubuf,8,dgram->header.source_port);
  RSSVAL(ubuf,12,dgram->header.packet_offset);

  offset = 14;

  if (dgram->header.msg_type == 0x10 ||
      dgram->header.msg_type == 0x11 ||
      dgram->header.msg_type == 0x12) {      
    offset += put_nmb_name((char *)ubuf,offset,&dgram->source_name);
    offset += put_nmb_name((char *)ubuf,offset,&dgram->dest_name);
  }

  memcpy(ubuf+offset,dgram->data,dgram->datasize);
  offset += dgram->datasize;

  /* automatically set the dgm_length */
  dgram->header.dgm_length = offset;
  RSSVAL(ubuf,10,dgram->header.dgm_length); 

  return(offset);
}

/*******************************************************************
  build a nmb name
 *******************************************************************/
void make_nmb_name( struct nmb_name *n, const char *name, int type )
{
	extern pstring global_scope;
	memset( (char *)n, '\0', sizeof(struct nmb_name) );
	StrnCpy( n->name, name, 15 );
	strupper( n->name );
	n->name_type = (unsigned int)type & 0xFF;
	StrnCpy( n->scope, global_scope, 63 );
	strupper( n->scope );
}

/*******************************************************************
  Compare two nmb names
  ******************************************************************/

BOOL nmb_name_equal(struct nmb_name *n1, struct nmb_name *n2)
{
  return ((n1->name_type == n2->name_type) &&
         strequal(n1->name ,n2->name ) &&
         strequal(n1->scope,n2->scope));
}

/*******************************************************************
  build a nmb packet ready for sending

  XXXX this currently relies on not being passed something that expands
  to a packet too big for the buffer. Eventually this should be
  changed to set the trunc bit so the receiver can request the rest
  via tcp (when that becomes supported)
  ******************************************************************/
static int build_nmb(char *buf,struct packet_struct *p)
{
  struct nmb_packet *nmb = &p->packet.nmb;
  unsigned char *ubuf = (unsigned char *)buf;
  int offset=0;

  /* put in the header */
  RSSVAL(ubuf,offset,nmb->header.name_trn_id);
  ubuf[offset+2] = (nmb->header.opcode & 0xF) << 3;
  if (nmb->header.response) ubuf[offset+2] |= (1<<7);
  if (nmb->header.nm_flags.authoritative && 
      nmb->header.response) ubuf[offset+2] |= 0x4;
  if (nmb->header.nm_flags.trunc) ubuf[offset+2] |= 0x2;
  if (nmb->header.nm_flags.recursion_desired) ubuf[offset+2] |= 0x1;
  if (nmb->header.nm_flags.recursion_available &&
      nmb->header.response) ubuf[offset+3] |= 0x80;
  if (nmb->header.nm_flags.bcast) ubuf[offset+3] |= 0x10;
  ubuf[offset+3] |= (nmb->header.rcode & 0xF);

  RSSVAL(ubuf,offset+4,nmb->header.qdcount);
  RSSVAL(ubuf,offset+6,nmb->header.ancount);
  RSSVAL(ubuf,offset+8,nmb->header.nscount);
  RSSVAL(ubuf,offset+10,nmb->header.arcount);
  
  offset += 12;
  if (nmb->header.qdcount) {
    /* XXXX this doesn't handle a qdcount of > 1 */
    offset += put_nmb_name((char *)ubuf,offset,&nmb->question.question_name);
    RSSVAL(ubuf,offset,nmb->question.question_type);
    RSSVAL(ubuf,offset+2,nmb->question.question_class);
    offset += 4;
  }

  if (nmb->header.ancount)
    offset += put_res_rec((char *)ubuf,offset,nmb->answers,
			  nmb->header.ancount);

  if (nmb->header.nscount)
    offset += put_res_rec((char *)ubuf,offset,nmb->nsrecs,
			  nmb->header.nscount);

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

    offset += put_compressed_name_ptr(ubuf,offset,nmb->additional,12);

  } else if (nmb->header.arcount) {
    offset += put_res_rec((char *)ubuf,offset,nmb->additional,
			  nmb->header.arcount);  
  }
  return(offset);
}


/*******************************************************************
  send a packet_struct
  ******************************************************************/
BOOL send_packet(struct packet_struct *p)
{
  char buf[1024];
  int len=0;

  memset(buf,'\0',sizeof(buf));

  switch (p->packet_type) 
    {
    case NMB_PACKET:
      len = build_nmb(buf,p);
      debug_nmb_packet(p);
      break;

    case DGRAM_PACKET:
      len = build_dgram(buf,p);
      break;
    }

  if (!len) return(False);

  return(send_udp(p->fd,buf,len,p->ip,p->port));
}

/****************************************************************************
  receive a packet with timeout on a open UDP filedescriptor
  The timeout is in milliseconds
  ***************************************************************************/
struct packet_struct *receive_packet(int fd,enum packet_type type,int t)
{
  fd_set fds;
  struct timeval timeout;

  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  timeout.tv_sec = t/1000;
  timeout.tv_usec = 1000*(t%1000);

  sys_select(fd+1,&fds,&timeout);

  if (FD_ISSET(fd,&fds)) 
    return(read_packet(fd,type));

  return(NULL);
}


/****************************************************************************
return the number of bits that match between two 4 character buffers
  ***************************************************************************/
static int matching_bits(uchar *p1, uchar *p2)
{
	int i, j, ret = 0;
	for (i=0; i<4; i++) {
		if (p1[i] != p2[i]) break;
		ret += 8;
	}

	if (i==4) return ret;

	for (j=0; j<8; j++) {
		if ((p1[i] & (1<<(7-j))) != (p2[i] & (1<<(7-j)))) break;
		ret++;
	}	
	
	return ret;
}


static uchar sort_ip[4];

/****************************************************************************
compare two query reply records
  ***************************************************************************/
static int name_query_comp(uchar *p1, uchar *p2)
{
	return matching_bits(p2+2, sort_ip) - matching_bits(p1+2, sort_ip);
}

/****************************************************************************
sort a set of 6 byte name query response records so that the IPs that
have the most leading bits in common with the specified address come first
  ***************************************************************************/
void sort_query_replies(char *data, int n, struct in_addr ip)
{
	if (n <= 1) return;

	putip(sort_ip, (char *)&ip);

	qsort(data, n, 6, QSORT_CAST name_query_comp);
}
