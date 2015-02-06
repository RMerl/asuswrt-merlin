/* dnsmasq is Copyright (c) 2000-2014 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

int extract_name(struct dns_header *header, size_t plen, unsigned char **pp, 
		 char *name, int isExtract, int extrabytes)
{
  unsigned char *cp = (unsigned char *)name, *p = *pp, *p1 = NULL;
  unsigned int j, l, hops = 0;
  int retvalue = 1;
  
  if (isExtract)
    *cp = 0;

  while (1)
    { 
      unsigned int label_type;

      if (!CHECK_LEN(header, p, plen, 1))
	return 0;
      
      if ((l = *p++) == 0) 
	/* end marker */
	{
	  /* check that there are the correct no of bytes after the name */
	  if (!CHECK_LEN(header, p, plen, extrabytes))
	    return 0;
	  
	  if (isExtract)
	    {
	      if (cp != (unsigned char *)name)
		cp--;
	      *cp = 0; /* terminate: lose final period */
	    }
	  else if (*cp != 0)
	    retvalue = 2;
	  
	  if (p1) /* we jumped via compression */
	    *pp = p1;
	  else
	    *pp = p;
	  
	  return retvalue;
	}

      label_type = l & 0xc0;
      
      if (label_type == 0xc0) /* pointer */
	{ 
	  if (!CHECK_LEN(header, p, plen, 1))
	    return 0;
	      
	  /* get offset */
	  l = (l&0x3f) << 8;
	  l |= *p++;
	  
	  if (!p1) /* first jump, save location to go back to */
	    p1 = p;
	      
	  hops++; /* break malicious infinite loops */
	  if (hops > 255)
	    return 0;
	  
	  p = l + (unsigned char *)header;
	}
      else if (label_type == 0x80)
	return 0; /* reserved */
      else if (label_type == 0x40)
	{ /* ELT */
	  unsigned int count, digs;
	  
	  if ((l & 0x3f) != 1)
	    return 0; /* we only understand bitstrings */

	  if (!isExtract)
	    return 0; /* Cannot compare bitsrings */
	  
	  count = *p++;
	  if (count == 0)
	    count = 256;
	  digs = ((count-1)>>2)+1;
	  
	  /* output is \[x<hex>/siz]. which is digs+9 chars */
	  if (cp - (unsigned char *)name + digs + 9 >= MAXDNAME)
	    return 0;
	  if (!CHECK_LEN(header, p, plen, (count-1)>>3))
	    return 0;

	  *cp++ = '\\';
	  *cp++ = '[';
	  *cp++ = 'x';
	  for (j=0; j<digs; j++)
	    {
	      unsigned int dig;
	      if (j%2 == 0)
		dig = *p >> 4;
	      else
		dig = *p++ & 0x0f;
	      
	      *cp++ = dig < 10 ? dig + '0' : dig + 'A' - 10;
	    } 
	  cp += sprintf((char *)cp, "/%d]", count);
	  /* do this here to overwrite the zero char from sprintf */
	  *cp++ = '.';
	}
      else 
	{ /* label_type = 0 -> label. */
	  if (cp - (unsigned char *)name + l + 1 >= MAXDNAME)
	    return 0;
	  if (!CHECK_LEN(header, p, plen, l))
	    return 0;
	  
	  for(j=0; j<l; j++, p++)
	    if (isExtract)
	      {
		unsigned char c = *p;
		if (isascii(c) && !iscntrl(c) && c != '.')
		  *cp++ = *p;
		else
		  return 0;
	      }
	    else 
	      {
		unsigned char c1 = *cp, c2 = *p;
		
		if (c1 == 0)
		  retvalue = 2;
		else 
		  {
		    cp++;
		    if (c1 >= 'A' && c1 <= 'Z')
		      c1 += 'a' - 'A';
		    if (c2 >= 'A' && c2 <= 'Z')
		      c2 += 'a' - 'A';
		    
		    if (c1 != c2)
		      retvalue =  2;
		  }
	      }
	  
	  if (isExtract)
	    *cp++ = '.';
	  else if (*cp != 0 && *cp++ != '.')
	    retvalue = 2;
	}
    }
}
 
/* Max size of input string (for IPv6) is 75 chars.) */
#define MAXARPANAME 75
int in_arpa_name_2_addr(char *namein, struct all_addr *addrp)
{
  int j;
  char name[MAXARPANAME+1], *cp1;
  unsigned char *addr = (unsigned char *)addrp;
  char *lastchunk = NULL, *penchunk = NULL;
  
  if (strlen(namein) > MAXARPANAME)
    return 0;

  memset(addrp, 0, sizeof(struct all_addr));

  /* turn name into a series of asciiz strings */
  /* j counts no of labels */
  for(j = 1,cp1 = name; *namein; cp1++, namein++)
    if (*namein == '.')
      {
	penchunk = lastchunk;
        lastchunk = cp1 + 1;
	*cp1 = 0;
	j++;
      }
    else
      *cp1 = *namein;
  
  *cp1 = 0;

  if (j<3)
    return 0;

  if (hostname_isequal(lastchunk, "arpa") && hostname_isequal(penchunk, "in-addr"))
    {
      /* IP v4 */
      /* address arives as a name of the form
	 www.xxx.yyy.zzz.in-addr.arpa
	 some of the low order address octets might be missing
	 and should be set to zero. */
      for (cp1 = name; cp1 != penchunk; cp1 += strlen(cp1)+1)
	{
	  /* check for digits only (weeds out things like
	     50.0/24.67.28.64.in-addr.arpa which are used 
	     as CNAME targets according to RFC 2317 */
	  char *cp;
	  for (cp = cp1; *cp; cp++)
	    if (!isdigit((unsigned char)*cp))
	      return 0;
	  
	  addr[3] = addr[2];
	  addr[2] = addr[1];
	  addr[1] = addr[0];
	  addr[0] = atoi(cp1);
	}

      return F_IPV4;
    }
#ifdef HAVE_IPV6
  else if (hostname_isequal(penchunk, "ip6") && 
	   (hostname_isequal(lastchunk, "int") || hostname_isequal(lastchunk, "arpa")))
    {
      /* IP v6:
         Address arrives as 0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.ip6.[int|arpa]
    	 or \[xfedcba9876543210fedcba9876543210/128].ip6.[int|arpa]
      
	 Note that most of these the various reprentations are obsolete and 
	 left-over from the many DNS-for-IPv6 wars. We support all the formats
	 that we can since there is no reason not to.
      */

      if (*name == '\\' && *(name+1) == '[' && 
	  (*(name+2) == 'x' || *(name+2) == 'X'))
	{	  
	  for (j = 0, cp1 = name+3; *cp1 && isxdigit((unsigned char) *cp1) && j < 32; cp1++, j++)
	    {
	      char xdig[2];
	      xdig[0] = *cp1;
	      xdig[1] = 0;
	      if (j%2)
		addr[j/2] |= strtol(xdig, NULL, 16);
	      else
		addr[j/2] = strtol(xdig, NULL, 16) << 4;
	    }
	  
	  if (*cp1 == '/' && j == 32)
	    return F_IPV6;
	}
      else
	{
	  for (cp1 = name; cp1 != penchunk; cp1 += strlen(cp1)+1)
	    {
	      if (*(cp1+1) || !isxdigit((unsigned char)*cp1))
		return 0;
	      
	      for (j = sizeof(struct all_addr)-1; j>0; j--)
		addr[j] = (addr[j] >> 4) | (addr[j-1] << 4);
	      addr[0] = (addr[0] >> 4) | (strtol(cp1, NULL, 16) << 4);
	    }
	  
	  return F_IPV6;
	}
    }
#endif
  
  return 0;
}

unsigned char *skip_name(unsigned char *ansp, struct dns_header *header, size_t plen, int extrabytes)
{
  while(1)
    {
      unsigned int label_type;
      
      if (!CHECK_LEN(header, ansp, plen, 1))
	return NULL;
      
      label_type = (*ansp) & 0xc0;

      if (label_type == 0xc0)
	{
	  /* pointer for compression. */
	  ansp += 2;	
	  break;
	}
      else if (label_type == 0x80)
	return NULL; /* reserved */
      else if (label_type == 0x40)
	{
	  /* Extended label type */
	  unsigned int count;
	  
	  if (!CHECK_LEN(header, ansp, plen, 2))
	    return NULL;
	  
	  if (((*ansp++) & 0x3f) != 1)
	    return NULL; /* we only understand bitstrings */
	  
	  count = *(ansp++); /* Bits in bitstring */
	  
	  if (count == 0) /* count == 0 means 256 bits */
	    ansp += 32;
	  else
	    ansp += ((count-1)>>3)+1;
	}
      else
	{ /* label type == 0 Bottom six bits is length */
	  unsigned int len = (*ansp++) & 0x3f;
	  
	  if (!ADD_RDLEN(header, ansp, plen, len))
	    return NULL;

	  if (len == 0)
	    break; /* zero length label marks the end. */
	}
    }

  if (!CHECK_LEN(header, ansp, plen, extrabytes))
    return NULL;
  
  return ansp;
}

unsigned char *skip_questions(struct dns_header *header, size_t plen)
{
  int q;
  unsigned char *ansp = (unsigned char *)(header+1);

  for (q = ntohs(header->qdcount); q != 0; q--)
    {
      if (!(ansp = skip_name(ansp, header, plen, 4)))
	return NULL;
      ansp += 4; /* class and type */
    }
  
  return ansp;
}

unsigned char *skip_section(unsigned char *ansp, int count, struct dns_header *header, size_t plen)
{
  int i, rdlen;
  
  for (i = 0; i < count; i++)
    {
      if (!(ansp = skip_name(ansp, header, plen, 10)))
	return NULL; 
      ansp += 8; /* type, class, TTL */
      GETSHORT(rdlen, ansp);
      if (!ADD_RDLEN(header, ansp, plen, rdlen))
	return NULL;
    }

  return ansp;
}

/* CRC the question section. This is used to safely detect query 
   retransmision and to detect answers to questions we didn't ask, which 
   might be poisoning attacks. Note that we decode the name rather 
   than CRC the raw bytes, since replies might be compressed differently. 
   We ignore case in the names for the same reason. Return all-ones
   if there is not question section. */
#ifndef HAVE_DNSSEC
unsigned int questions_crc(struct dns_header *header, size_t plen, char *name)
{
  int q;
  unsigned int crc = 0xffffffff;
  unsigned char *p1, *p = (unsigned char *)(header+1);

  for (q = ntohs(header->qdcount); q != 0; q--) 
    {
      if (!extract_name(header, plen, &p, name, 1, 4))
	return crc; /* bad packet */
      
      for (p1 = (unsigned char *)name; *p1; p1++)
	{
	  int i = 8;
	  char c = *p1;

	  if (c >= 'A' && c <= 'Z')
	    c += 'a' - 'A';

	  crc ^= c << 24;
	  while (i--)
	    crc = crc & 0x80000000 ? (crc << 1) ^ 0x04c11db7 : crc << 1;
	}
      
      /* CRC the class and type as well */
      for (p1 = p; p1 < p+4; p1++)
	{
	  int i = 8;
	  crc ^= *p1 << 24;
	  while (i--)
	    crc = crc & 0x80000000 ? (crc << 1) ^ 0x04c11db7 : crc << 1;
	}

      p += 4;
      if (!CHECK_LEN(header, p, plen, 0))
	return crc; /* bad packet */
    }

  return crc;
}
#endif

size_t resize_packet(struct dns_header *header, size_t plen, unsigned char *pheader, size_t hlen)
{
  unsigned char *ansp = skip_questions(header, plen);
    
  /* if packet is malformed, just return as-is. */
  if (!ansp)
    return plen;
  
  if (!(ansp = skip_section(ansp, ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount),
			    header, plen)))
    return plen;
    
  /* restore pseudoheader */
  if (pheader && ntohs(header->arcount) == 0)
    {
      /* must use memmove, may overlap */
      memmove(ansp, pheader, hlen);
      header->arcount = htons(1);
      ansp += hlen;
    }

  return ansp - (unsigned char *)header;
}

unsigned char *find_pseudoheader(struct dns_header *header, size_t plen, size_t  *len, unsigned char **p, int *is_sign)
{
  /* See if packet has an RFC2671 pseudoheader, and if so return a pointer to it. 
     also return length of pseudoheader in *len and pointer to the UDP size in *p
     Finally, check to see if a packet is signed. If it is we cannot change a single bit before
     forwarding. We look for SIG and TSIG in the addition section, and TKEY queries (for GSS-TSIG) */
  
  int i, arcount = ntohs(header->arcount);
  unsigned char *ansp = (unsigned char *)(header+1);
  unsigned short rdlen, type, class;
  unsigned char *ret = NULL;

  if (is_sign)
    {
      *is_sign = 0;

      if (OPCODE(header) == QUERY)
	{
	  for (i = ntohs(header->qdcount); i != 0; i--)
	    {
	      if (!(ansp = skip_name(ansp, header, plen, 4)))
		return NULL;
	      
	      GETSHORT(type, ansp); 
	      GETSHORT(class, ansp);
	      
	      if (class == C_IN && type == T_TKEY)
		*is_sign = 1;
	    }
	}
    }
  else
    {
      if (!(ansp = skip_questions(header, plen)))
	return NULL;
    }
    
  if (arcount == 0)
    return NULL;
  
  if (!(ansp = skip_section(ansp, ntohs(header->ancount) + ntohs(header->nscount), header, plen)))
    return NULL; 
  
  for (i = 0; i < arcount; i++)
    {
      unsigned char *save, *start = ansp;
      if (!(ansp = skip_name(ansp, header, plen, 10)))
	return NULL; 

      GETSHORT(type, ansp);
      save = ansp;
      GETSHORT(class, ansp);
      ansp += 4; /* TTL */
      GETSHORT(rdlen, ansp);
      if (!ADD_RDLEN(header, ansp, plen, rdlen))
	return NULL;
      if (type == T_OPT)
	{
	  if (len)
	    *len = ansp - start;
	  if (p)
	    *p = save;
	  ret = start;
	}
      else if (is_sign && 
	       i == arcount - 1 && 
	       class == C_ANY && 
	       type == T_TSIG)
	*is_sign = 1;
    }
  
  return ret;
}

struct macparm {
  unsigned char *limit;
  struct dns_header *header;
  size_t plen;
  union mysockaddr *l3;
};
 
static size_t add_pseudoheader(struct dns_header *header, size_t plen, unsigned char *limit, 
			       int optno, unsigned char *opt, size_t optlen, int set_do)
{ 
  unsigned char *lenp, *datap, *p;
  int rdlen, is_sign;
  
  if (!(p = find_pseudoheader(header, plen, NULL, NULL, &is_sign)))
    {
      if (is_sign)
	return plen;

      /* We are adding the pseudoheader */
      if (!(p = skip_questions(header, plen)) ||
	  !(p = skip_section(p, 
			     ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount), 
			     header, plen)))
	return plen;
      *p++ = 0; /* empty name */
      PUTSHORT(T_OPT, p);
      PUTSHORT(daemon->edns_pktsz, p); /* max packet length */
      PUTSHORT(0, p);    /* extended RCODE and version */
      PUTSHORT(set_do ? 0x8000 : 0, p); /* DO flag */
      lenp = p;
      PUTSHORT(0, p);    /* RDLEN */
      rdlen = 0;
      if (((ssize_t)optlen) > (limit - (p + 4)))
	return plen; /* Too big */
      header->arcount = htons(ntohs(header->arcount) + 1);
      datap = p;
    }
  else
    {
      int i;
      unsigned short code, len, flags;
      
      /* Must be at the end, if exists */
      if (ntohs(header->arcount) != 1 ||
	  is_sign ||
	  (!(p = skip_name(p, header, plen, 10))))
	return plen;
      
      p += 6; /* skip UDP length and RCODE */
      GETSHORT(flags, p);
      if (set_do)
	{
	  p -=2;
	  PUTSHORT(flags | 0x8000, p);
	}

      lenp = p;
      GETSHORT(rdlen, p);
      if (!CHECK_LEN(header, p, plen, rdlen))
	return plen; /* bad packet */
      datap = p;

       /* no option to add */
      if (optno == 0)
	return plen;
      	  
      /* check if option already there */
      for (i = 0; i + 4 < rdlen; i += len + 4)
	{
	  GETSHORT(code, p);
	  GETSHORT(len, p);
	  if (code == optno)
	    return plen;
	  p += len;
	}
      
      if (((ssize_t)optlen) > (limit - (p + 4)))
	return plen; /* Too big */
    }
  
  if (optno != 0)
    {
      PUTSHORT(optno, p);
      PUTSHORT(optlen, p);
      memcpy(p, opt, optlen);
      p += optlen;  
    }

  PUTSHORT(p - datap, lenp);
  return p - (unsigned char *)header;
  
}

static int filter_mac(int family, char *addrp, char *mac, size_t maclen, void *parmv)
{
  struct macparm *parm = parmv;
  int match = 0;
    
  if (family == parm->l3->sa.sa_family)
    {
      if (family == AF_INET && memcmp(&parm->l3->in.sin_addr, addrp, INADDRSZ) == 0)
	match = 1;
#ifdef HAVE_IPV6
      else
	if (family == AF_INET6 && memcmp(&parm->l3->in6.sin6_addr, addrp, IN6ADDRSZ) == 0)
	  match = 1;
#endif
    }
 
  if (!match)
    return 1; /* continue */

  parm->plen = add_pseudoheader(parm->header, parm->plen, parm->limit,  EDNS0_OPTION_MAC, (unsigned char *)mac, maclen, 0);
  
  return 0; /* done */
}	      
     
size_t add_mac(struct dns_header *header, size_t plen, char *limit, union mysockaddr *l3)
{
  struct macparm parm;
     
/* Must have an existing pseudoheader as the only ar-record, 
   or have no ar-records. Must also not be signed */
   
  if (ntohs(header->arcount) > 1)
    return plen;

  parm.header = header;
  parm.limit = (unsigned char *)limit;
  parm.plen = plen;
  parm.l3 = l3;

  iface_enumerate(AF_UNSPEC, &parm, filter_mac);
  
  return parm.plen; 
}

struct subnet_opt {
  u16 family;
  u8 source_netmask, scope_netmask;
#ifdef HAVE_IPV6 
  u8 addr[IN6ADDRSZ];
#else
  u8 addr[INADDRSZ];
#endif
};

static size_t calc_subnet_opt(struct subnet_opt *opt, union mysockaddr *source)
{
  /* http://tools.ietf.org/html/draft-vandergaast-edns-client-subnet-02 */
  
  int len;
  void *addrp;

#ifdef HAVE_IPV6
  if (source->sa.sa_family == AF_INET6)
    {
      opt->family = htons(2);
      opt->source_netmask = daemon->addr6_netmask;
      addrp = &source->in6.sin6_addr;
    }
  else
#endif
    {
      opt->family = htons(1);
      opt->source_netmask = daemon->addr4_netmask;
      addrp = &source->in.sin_addr;
    }
  
  opt->scope_netmask = 0;
  len = 0;
  
  if (opt->source_netmask != 0)
    {
      len = ((opt->source_netmask - 1) >> 3) + 1;
      memcpy(opt->addr, addrp, len);
      if (opt->source_netmask & 7)
	opt->addr[len-1] &= 0xff << (8 - (opt->source_netmask & 7));
    }

  return len + 4;
}
 
size_t add_source_addr(struct dns_header *header, size_t plen, char *limit, union mysockaddr *source)
{
  /* http://tools.ietf.org/html/draft-vandergaast-edns-client-subnet-02 */
  
  int len;
  struct subnet_opt opt;
  
  len = calc_subnet_opt(&opt, source);
  return add_pseudoheader(header, plen, (unsigned char *)limit, EDNS0_OPTION_CLIENT_SUBNET, (unsigned char *)&opt, len, 0);
}

#ifdef HAVE_DNSSEC
size_t add_do_bit(struct dns_header *header, size_t plen, char *limit)
{
  return add_pseudoheader(header, plen, (unsigned char *)limit, 0, NULL, 0, 1);
}
#endif

int check_source(struct dns_header *header, size_t plen, unsigned char *pseudoheader, union mysockaddr *peer)
{
  /* Section 9.2, Check that subnet option in reply matches. */


 int len, calc_len;
  struct subnet_opt opt;
  unsigned char *p;
  int code, i, rdlen;
  
   calc_len = calc_subnet_opt(&opt, peer);
   
   if (!(p = skip_name(pseudoheader, header, plen, 10)))
     return 1;
   
   p += 8; /* skip UDP length and RCODE */
   
   GETSHORT(rdlen, p);
   if (!CHECK_LEN(header, p, plen, rdlen))
     return 1; /* bad packet */
   
   /* check if option there */
   for (i = 0; i + 4 < rdlen; i += len + 4)
     {
       GETSHORT(code, p);
       GETSHORT(len, p);
       if (code == EDNS0_OPTION_CLIENT_SUBNET)
	 {
	   /* make sure this doesn't mismatch. */
	   opt.scope_netmask = p[3];
	   if (len != calc_len || memcmp(p, &opt, len) != 0)
	     return 0;
	 }
       p += len;
     }
   
   return 1;
}

/* is addr in the non-globally-routed IP space? */ 
int private_net(struct in_addr addr, int ban_localhost) 
{
  in_addr_t ip_addr = ntohl(addr.s_addr);

  return
    (((ip_addr & 0xFF000000) == 0x7F000000) && ban_localhost)  /* 127.0.0.0/8    (loopback) */ || 
    ((ip_addr & 0xFFFF0000) == 0xC0A80000)  /* 192.168.0.0/16 (private)  */ ||
    ((ip_addr & 0xFF000000) == 0x0A000000)  /* 10.0.0.0/8     (private)  */ ||
    ((ip_addr & 0xFFF00000) == 0xAC100000)  /* 172.16.0.0/12  (private)  */ ||
    ((ip_addr & 0xFFFF0000) == 0xA9FE0000)  /* 169.254.0.0/16 (zeroconf) */ ;
}

static unsigned char *do_doctor(unsigned char *p, int count, struct dns_header *header, size_t qlen, char *name, int *doctored)
{
  int i, qtype, qclass, rdlen;

  for (i = count; i != 0; i--)
    {
      if (name && option_bool(OPT_LOG))
	{
	  if (!extract_name(header, qlen, &p, name, 1, 10))
	    return 0;
	}
      else if (!(p = skip_name(p, header, qlen, 10)))
	return 0; /* bad packet */
      
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      p += 4; /* ttl */
      GETSHORT(rdlen, p);
      
      if (qclass == C_IN && qtype == T_A)
	{
	  struct doctor *doctor;
	  struct in_addr addr;
	  
	  if (!CHECK_LEN(header, p, qlen, INADDRSZ))
	    return 0;
	  
	  /* alignment */
	  memcpy(&addr, p, INADDRSZ);
	  
	  for (doctor = daemon->doctors; doctor; doctor = doctor->next)
	    {
	      if (doctor->end.s_addr == 0)
		{
		  if (!is_same_net(doctor->in, addr, doctor->mask))
		    continue;
		}
	      else if (ntohl(doctor->in.s_addr) > ntohl(addr.s_addr) || 
		       ntohl(doctor->end.s_addr) < ntohl(addr.s_addr))
		continue;
	      
	      addr.s_addr &= ~doctor->mask.s_addr;
	      addr.s_addr |= (doctor->out.s_addr & doctor->mask.s_addr);
	      /* Since we munged the data, the server it came from is no longer authoritative */
	      header->hb3 &= ~HB3_AA;
	      *doctored = 1;
	      memcpy(p, &addr, INADDRSZ);
	      break;
	    }
	}
      else if (qtype == T_TXT && name && option_bool(OPT_LOG))
	{
	  unsigned char *p1 = p;
	  if (!CHECK_LEN(header, p1, qlen, rdlen))
	    return 0;
	  while ((p1 - p) < rdlen)
	    {
	      unsigned int i, len = *p1;
	      unsigned char *p2 = p1;
	      /* make counted string zero-term  and sanitise */
	      for (i = 0; i < len; i++)
		{
		  if (!isprint((int)*(p2+1)))
		    break;
		  
		  *p2 = *(p2+1);
		  p2++;
		}
	      *p2 = 0;
	      my_syslog(LOG_INFO, "reply %s is %s", name, p1);
	      /* restore */
	      memmove(p1 + 1, p1, i);
	      *p1 = len;
	      p1 += len+1;
	    }
	}		  
      
      if (!ADD_RDLEN(header, p, qlen, rdlen))
	 return 0; /* bad packet */
    }
  
  return p; 
}

static int find_soa(struct dns_header *header, size_t qlen, char *name, int *doctored)
{
  unsigned char *p;
  int qtype, qclass, rdlen;
  unsigned long ttl, minttl = ULONG_MAX;
  int i, found_soa = 0;
  
  /* first move to NS section and find TTL from any SOA section */
  if (!(p = skip_questions(header, qlen)) ||
      !(p = do_doctor(p, ntohs(header->ancount), header, qlen, name, doctored)))
    return 0;  /* bad packet */
  
  for (i = ntohs(header->nscount); i != 0; i--)
    {
      if (!(p = skip_name(p, header, qlen, 10)))
	return 0; /* bad packet */
      
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      GETLONG(ttl, p);
      GETSHORT(rdlen, p);
      
      if ((qclass == C_IN) && (qtype == T_SOA))
	{
	  found_soa = 1;
	  if (ttl < minttl)
	    minttl = ttl;

	  /* MNAME */
	  if (!(p = skip_name(p, header, qlen, 0)))
	    return 0;
	  /* RNAME */
	  if (!(p = skip_name(p, header, qlen, 20)))
	    return 0;
	  p += 16; /* SERIAL REFRESH RETRY EXPIRE */
	  
	  GETLONG(ttl, p); /* minTTL */
	  if (ttl < minttl)
	    minttl = ttl;
	}
      else if (!ADD_RDLEN(header, p, qlen, rdlen))
	return 0; /* bad packet */
    }
  
  /* rewrite addresses in additional section too */
  if (!do_doctor(p, ntohs(header->arcount), header, qlen, NULL, doctored))
    return 0;
  
  if (!found_soa)
    minttl = daemon->neg_ttl;

  return minttl;
}

/* Note that the following code can create CNAME chains that don't point to a real record,
   either because of lack of memory, or lack of SOA records.  These are treated by the cache code as 
   expired and cleaned out that way. 
   Return 1 if we reject an address because it look like part of dns-rebinding attack. */
int extract_addresses(struct dns_header *header, size_t qlen, char *name, time_t now, 
		      char **ipsets, int is_sign, int check_rebind, int no_cache_dnssec, int secure, int *doctored)
{
  unsigned char *p, *p1, *endrr, *namep;
  int i, j, qtype, qclass, aqtype, aqclass, ardlen, res, searched_soa = 0;
  unsigned long ttl = 0;
  struct all_addr addr;
#ifdef HAVE_IPSET
  char **ipsets_cur;
#else
  (void)ipsets; /* unused */
#endif
  
  cache_start_insert();

  /* find_soa is needed for dns_doctor and logging side-effects, so don't call it lazily if there are any. */
  if (daemon->doctors || option_bool(OPT_LOG) || option_bool(OPT_DNSSEC_VALID))
    {
      searched_soa = 1;
      ttl = find_soa(header, qlen, name, doctored);
#ifdef HAVE_DNSSEC
      if (*doctored && secure)
	return 0;
#endif
    }
  
  /* go through the questions. */
  p = (unsigned char *)(header+1);
  
  for (i = ntohs(header->qdcount); i != 0; i--)
    {
      int found = 0, cname_count = CNAME_CHAIN;
      struct crec *cpp = NULL;
      int flags = RCODE(header) == NXDOMAIN ? F_NXDOMAIN : 0;
      int secflag = secure ?  F_DNSSECOK : 0;
      unsigned long cttl = ULONG_MAX, attl;

      namep = p;
      if (!extract_name(header, qlen, &p, name, 1, 4))
	return 0; /* bad packet */
           
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      
      if (qclass != C_IN)
	continue;

      /* PTRs: we chase CNAMEs here, since we have no way to 
	 represent them in the cache. */
      if (qtype == T_PTR)
	{ 
	  int name_encoding = in_arpa_name_2_addr(name, &addr);
	  
	  if (!name_encoding)
	    continue;

	  if (!(flags & F_NXDOMAIN))
	    {
	    cname_loop:
	      if (!(p1 = skip_questions(header, qlen)))
		return 0;
	      
	      for (j = ntohs(header->ancount); j != 0; j--) 
		{
		  unsigned char *tmp = namep;
		  /* the loop body overwrites the original name, so get it back here. */
		  if (!extract_name(header, qlen, &tmp, name, 1, 0) ||
		      !(res = extract_name(header, qlen, &p1, name, 0, 10)))
		    return 0; /* bad packet */
		  
		  GETSHORT(aqtype, p1); 
		  GETSHORT(aqclass, p1);
		  GETLONG(attl, p1);
		  if ((daemon->max_ttl != 0) && (attl > daemon->max_ttl) && !is_sign)
		    {
		      (p1) -= 4;
		      PUTLONG(daemon->max_ttl, p1);
		    }
		  GETSHORT(ardlen, p1);
		  endrr = p1+ardlen;
		  
		  /* TTL of record is minimum of CNAMES and PTR */
		  if (attl < cttl)
		    cttl = attl;

		  if (aqclass == C_IN && res != 2 && (aqtype == T_CNAME || aqtype == T_PTR))
		    {
		      if (!extract_name(header, qlen, &p1, name, 1, 0))
			return 0;
		      
		      if (aqtype == T_CNAME)
			{
			  if (!cname_count-- || secure)
			    return 0; /* looped CNAMES, or DNSSEC, which we can't cache. */
			  goto cname_loop;
			}
		      
		      cache_insert(name, &addr, now, cttl, name_encoding | secflag | F_REVERSE);
		      found = 1; 
		    }
		  
		  p1 = endrr;
		  if (!CHECK_LEN(header, p1, qlen, 0))
		    return 0; /* bad packet */
		}
	    }
	  
	   if (!found && !option_bool(OPT_NO_NEG))
	    {
	      if (!searched_soa)
		{
		  searched_soa = 1;
		  ttl = find_soa(header, qlen, NULL, doctored);
		}
	      if (ttl)
		cache_insert(NULL, &addr, now, ttl, name_encoding | F_REVERSE | F_NEG | flags | secflag);	
	    }
	}
      else
	{
	  /* everything other than PTR */
	  struct crec *newc;
	  int addrlen;

	  if (qtype == T_A)
	    {
	      addrlen = INADDRSZ;
	      flags |= F_IPV4;
	    }
#ifdef HAVE_IPV6
	  else if (qtype == T_AAAA)
	    {
	      addrlen = IN6ADDRSZ;
	      flags |= F_IPV6;
	    }
#endif
	  else 
	    continue;
	    
	cname_loop1:
	  if (!(p1 = skip_questions(header, qlen)))
	    return 0;
	  
	  for (j = ntohs(header->ancount); j != 0; j--) 
	    {
	      if (!(res = extract_name(header, qlen, &p1, name, 0, 10)))
		return 0; /* bad packet */
	      
	      GETSHORT(aqtype, p1); 
	      GETSHORT(aqclass, p1);
	      GETLONG(attl, p1);
	      if ((daemon->max_ttl != 0) && (attl > daemon->max_ttl) && !is_sign)
		{
		  (p1) -= 4;
		  PUTLONG(daemon->max_ttl, p1);
		}
	      GETSHORT(ardlen, p1);
	      endrr = p1+ardlen;
	      
	      if (aqclass == C_IN && res != 2 && (aqtype == T_CNAME || aqtype == qtype))
		{
		  if (aqtype == T_CNAME)
		    {
		      if (!cname_count--)
			return 0; /* looped CNAMES */
		      newc = cache_insert(name, NULL, now, attl, F_CNAME | F_FORWARD | secflag);
		      if (newc)
			{
			  newc->addr.cname.target.cache = NULL;
			  /* anything other than zero, to avoid being mistaken for CNAME to interface-name */ 
			  newc->addr.cname.uid = 1; 
			  if (cpp)
			    {
			      cpp->addr.cname.target.cache = newc;
			      cpp->addr.cname.uid = newc->uid;
			    }
			}
		      
		      cpp = newc;
		      if (attl < cttl)
			cttl = attl;
		      
		      if (!extract_name(header, qlen, &p1, name, 1, 0))
			return 0;
		      goto cname_loop1;
		    }
		  else if (!(flags & F_NXDOMAIN))
		    {
		      found = 1;
		      
		      /* copy address into aligned storage */
		      if (!CHECK_LEN(header, p1, qlen, addrlen))
			return 0; /* bad packet */
		      memcpy(&addr, p1, addrlen);
		      
		      /* check for returned address in private space */
		      if (check_rebind &&
			  (flags & F_IPV4) &&
			  private_net(addr.addr.addr4, !option_bool(OPT_LOCAL_REBIND)))
			return 1;
		      
#ifdef HAVE_IPSET
		      if (ipsets && (flags & (F_IPV4 | F_IPV6)))
			{
			  ipsets_cur = ipsets;
			  while (*ipsets_cur)
			    {
			      log_query((flags & (F_IPV4 | F_IPV6)) | F_IPSET, name, &addr, *ipsets_cur);
			      add_to_ipset(*ipsets_cur++, &addr, flags, 0);
			    }
			}
#endif
		      
		      newc = cache_insert(name, &addr, now, attl, flags | F_FORWARD | secflag);
		      if (newc && cpp)
			{
			  cpp->addr.cname.target.cache = newc;
			  cpp->addr.cname.uid = newc->uid;
			}
		      cpp = NULL;
		    }
		}
	      
	      p1 = endrr;
	      if (!CHECK_LEN(header, p1, qlen, 0))
		return 0; /* bad packet */
	    }
	  
	  if (!found && !option_bool(OPT_NO_NEG))
	    {
	      if (!searched_soa)
		{
		  searched_soa = 1;
		  ttl = find_soa(header, qlen, NULL, doctored);
		}
	      /* If there's no SOA to get the TTL from, but there is a CNAME 
		 pointing at this, inherit its TTL */
	      if (ttl || cpp)
		{
		  newc = cache_insert(name, NULL, now, ttl ? ttl : cttl, F_FORWARD | F_NEG | flags | secflag);	
		  if (newc && cpp)
		    {
		      cpp->addr.cname.target.cache = newc;
		      cpp->addr.cname.uid = newc->uid;
		    }
		}
	    }
	}
    }
  
  /* Don't put stuff from a truncated packet into the cache.
     Don't cache replies from non-recursive nameservers, since we may get a 
     reply containing a CNAME but not its target, even though the target 
     does exist. */
  if (!(header->hb3 & HB3_TC) && 
      !(header->hb4 & HB4_CD) &&
      (header->hb4 & HB4_RA) &&
      !no_cache_dnssec)
    cache_end_insert();

  return 0;
}

/* If the packet holds exactly one query
   return F_IPV4 or F_IPV6  and leave the name from the query in name */
unsigned int extract_request(struct dns_header *header, size_t qlen, char *name, unsigned short *typep)
{
  unsigned char *p = (unsigned char *)(header+1);
  int qtype, qclass;

  if (typep)
    *typep = 0;

  if (ntohs(header->qdcount) != 1 || OPCODE(header) != QUERY)
    return 0; /* must be exactly one query. */
  
  if (!extract_name(header, qlen, &p, name, 1, 4))
    return 0; /* bad packet */
   
  GETSHORT(qtype, p); 
  GETSHORT(qclass, p);

  if (typep)
    *typep = qtype;

  if (qclass == C_IN)
    {
      if (qtype == T_A)
	return F_IPV4;
      if (qtype == T_AAAA)
	return F_IPV6;
      if (qtype == T_ANY)
	return  F_IPV4 | F_IPV6;
    }
  
  return F_QUERY;
}


size_t setup_reply(struct dns_header *header, size_t qlen,
		struct all_addr *addrp, unsigned int flags, unsigned long ttl)
{
  unsigned char *p = skip_questions(header, qlen);
  
  /* clear authoritative and truncated flags, set QR flag */
  header->hb3 = (header->hb3 & ~(HB3_AA | HB3_TC)) | HB3_QR;
  /* set RA flag */
  header->hb4 |= HB4_RA;

  header->nscount = htons(0);
  header->arcount = htons(0);
  header->ancount = htons(0); /* no answers unless changed below */
  if (flags == F_NEG)
    SET_RCODE(header, SERVFAIL); /* couldn't get memory */
  else if (flags == F_NOERR)
    SET_RCODE(header, NOERROR); /* empty domain */
  else if (flags == F_NXDOMAIN)
    SET_RCODE(header, NXDOMAIN);
  else if (p && flags == F_IPV4)
    { /* we know the address */
      SET_RCODE(header, NOERROR);
      header->ancount = htons(1);
      header->hb3 |= HB3_AA;
      add_resource_record(header, NULL, NULL, sizeof(struct dns_header), &p, ttl, NULL, T_A, C_IN, "4", addrp);
    }
#ifdef HAVE_IPV6
  else if (p && flags == F_IPV6)
    {
      SET_RCODE(header, NOERROR);
      header->ancount = htons(1);
      header->hb3 |= HB3_AA;
      add_resource_record(header, NULL, NULL, sizeof(struct dns_header), &p, ttl, NULL, T_AAAA, C_IN, "6", addrp);
    }
#endif
  else /* nowhere to forward to */
    SET_RCODE(header, REFUSED);
 
  return p - (unsigned char *)header;
}

/* check if name matches local names ie from /etc/hosts or DHCP or local mx names. */
int check_for_local_domain(char *name, time_t now)
{
  struct crec *crecp;
  struct mx_srv_record *mx;
  struct txt_record *txt;
  struct interface_name *intr;
  struct ptr_record *ptr;
  struct naptr *naptr;

  /* Note: the call to cache_find_by_name is intended to find any record which matches
     ie A, AAAA, CNAME, DS. Because RRSIG records are marked by setting both F_DS and F_DNSKEY,
     cache_find_by name ordinarily only returns records with an exact match on those bits (ie
     for the call below, only DS records). The F_NSIGMATCH bit changes this behaviour */

  if ((crecp = cache_find_by_name(NULL, name, now, F_IPV4 | F_IPV6 | F_CNAME | F_DS | F_NO_RR | F_NSIGMATCH)) &&
      (crecp->flags & (F_HOSTS | F_DHCP | F_CONFIG)))
    return 1;
  
  for (naptr = daemon->naptr; naptr; naptr = naptr->next)
     if (hostname_isequal(name, naptr->name))
      return 1;

   for (mx = daemon->mxnames; mx; mx = mx->next)
    if (hostname_isequal(name, mx->name))
      return 1;

  for (txt = daemon->txt; txt; txt = txt->next)
    if (hostname_isequal(name, txt->name))
      return 1;

  for (intr = daemon->int_names; intr; intr = intr->next)
    if (hostname_isequal(name, intr->name))
      return 1;

  for (ptr = daemon->ptr; ptr; ptr = ptr->next)
    if (hostname_isequal(name, ptr->name))
      return 1;
 
  return 0;
}

/* Is the packet a reply with the answer address equal to addr?
   If so mung is into an NXDOMAIN reply and also put that information
   in the cache. */
int check_for_bogus_wildcard(struct dns_header *header, size_t qlen, char *name, 
			     struct bogus_addr *baddr, time_t now)
{
  unsigned char *p;
  int i, qtype, qclass, rdlen;
  unsigned long ttl;
  struct bogus_addr *baddrp;

  /* skip over questions */
  if (!(p = skip_questions(header, qlen)))
    return 0; /* bad packet */

  for (i = ntohs(header->ancount); i != 0; i--)
    {
      if (!extract_name(header, qlen, &p, name, 1, 10))
	return 0; /* bad packet */
  
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      GETLONG(ttl, p);
      GETSHORT(rdlen, p);
      
      if (qclass == C_IN && qtype == T_A)
	{
	  if (!CHECK_LEN(header, p, qlen, INADDRSZ))
	    return 0;
	  
	  for (baddrp = baddr; baddrp; baddrp = baddrp->next)
	    if (memcmp(&baddrp->addr, p, INADDRSZ) == 0)
	      {
		/* Found a bogus address. Insert that info here, since there no SOA record
		   to get the ttl from in the normal processing */
		cache_start_insert();
		cache_insert(name, NULL, now, ttl, F_IPV4 | F_FORWARD | F_NEG | F_NXDOMAIN);
		cache_end_insert();
		
		return 1;
	      }
	}
      
      if (!ADD_RDLEN(header, p, qlen, rdlen))
	return 0;
    }
  
  return 0;
}

int check_for_ignored_address(struct dns_header *header, size_t qlen, struct bogus_addr *baddr)
{
  unsigned char *p;
  int i, qtype, qclass, rdlen;
  struct bogus_addr *baddrp;

  /* skip over questions */
  if (!(p = skip_questions(header, qlen)))
    return 0; /* bad packet */

  for (i = ntohs(header->ancount); i != 0; i--)
    {
      if (!(p = skip_name(p, header, qlen, 10)))
	return 0; /* bad packet */
      
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);
      
      if (qclass == C_IN && qtype == T_A)
	{
	  if (!CHECK_LEN(header, p, qlen, INADDRSZ))
	    return 0;
	  
	  for (baddrp = baddr; baddrp; baddrp = baddrp->next)
	    if (memcmp(&baddrp->addr, p, INADDRSZ) == 0)
	      return 1;
	}
      
      if (!ADD_RDLEN(header, p, qlen, rdlen))
	return 0;
    }
  
  return 0;
}

int add_resource_record(struct dns_header *header, char *limit, int *truncp, int nameoffset, unsigned char **pp, 
			unsigned long ttl, int *offset, unsigned short type, unsigned short class, char *format, ...)
{
  va_list ap;
  unsigned char *sav, *p = *pp;
  int j;
  unsigned short usval;
  long lval;
  char *sval;

  if (truncp && *truncp)
    return 0;
 
  va_start(ap, format);   /* make ap point to 1st unamed argument */
  
  if (nameoffset > 0)
    {
      PUTSHORT(nameoffset | 0xc000, p);
    }
  else
    {
      char *name = va_arg(ap, char *);
      if (name)
	p = do_rfc1035_name(p, name);
      if (nameoffset < 0)
	{
	  PUTSHORT(-nameoffset | 0xc000, p);
	}
      else
	*p++ = 0;
    }

  PUTSHORT(type, p);
  PUTSHORT(class, p);
  PUTLONG(ttl, p);      /* TTL */

  sav = p;              /* Save pointer to RDLength field */
  PUTSHORT(0, p);       /* Placeholder RDLength */

  for (; *format; format++)
    switch (*format)
      {
#ifdef HAVE_IPV6
      case '6':
	sval = va_arg(ap, char *); 
	memcpy(p, sval, IN6ADDRSZ);
	p += IN6ADDRSZ;
	break;
#endif
	
      case '4':
	sval = va_arg(ap, char *); 
	memcpy(p, sval, INADDRSZ);
	p += INADDRSZ;
	break;
	
      case 'b':
	usval = va_arg(ap, int);
	*p++ = usval;
	break;
	
      case 's':
	usval = va_arg(ap, int);
	PUTSHORT(usval, p);
	break;
	
      case 'l':
	lval = va_arg(ap, long);
	PUTLONG(lval, p);
	break;
	
      case 'd':
	/* get domain-name answer arg and store it in RDATA field */
	if (offset)
	  *offset = p - (unsigned char *)header;
	p = do_rfc1035_name(p, va_arg(ap, char *));
	*p++ = 0;
	break;
	
      case 't':
	usval = va_arg(ap, int);
	sval = va_arg(ap, char *);
	if (usval != 0)
	  memcpy(p, sval, usval);
	p += usval;
	break;

      case 'z':
	sval = va_arg(ap, char *);
	usval = sval ? strlen(sval) : 0;
	if (usval > 255)
	  usval = 255;
	*p++ = (unsigned char)usval;
	memcpy(p, sval, usval);
	p += usval;
	break;
      }

  va_end(ap);	/* clean up variable argument pointer */
  
  j = p - sav - 2;
  PUTSHORT(j, sav);     /* Now, store real RDLength */
  
  /* check for overflow of buffer */
  if (limit && ((unsigned char *)limit - p) < 0)
    {
      if (truncp)
	*truncp = 1;
      return 0;
    }
  
  *pp = p;
  return 1;
}

static unsigned long crec_ttl(struct crec *crecp, time_t now)
{
  /* Return 0 ttl for DHCP entries, which might change
     before the lease expires. */

  if  (crecp->flags & (F_IMMORTAL | F_DHCP))
    return daemon->local_ttl;
  
  /* Return the Max TTL value if it is lower then the actual TTL */
  if (daemon->max_ttl == 0 || ((unsigned)(crecp->ttd - now) < daemon->max_ttl))
    return crecp->ttd - now;
  else
    return daemon->max_ttl;
}
  

/* return zero if we can't answer from cache, or packet size if we can */
size_t answer_request(struct dns_header *header, char *limit, size_t qlen,  
		      struct in_addr local_addr, struct in_addr local_netmask, 
		      time_t now, int *ad_reqd, int *do_bit) 
{
  char *name = daemon->namebuff;
  unsigned char *p, *ansp, *pheader;
  unsigned int qtype, qclass;
  struct all_addr addr;
  int nameoffset;
  unsigned short flag;
  int q, ans, anscount = 0, addncount = 0;
  int dryrun = 0, sec_reqd = 0, have_pseudoheader = 0;
  int is_sign;
  struct crec *crecp;
  int nxdomain = 0, auth = 1, trunc = 0, sec_data = 1;
  struct mx_srv_record *rec;
  size_t len;
 
  /* Don't return AD set if checking disabled. */
  if (header->hb4 & HB4_CD)
    sec_data = 0;
  
  /* RFC 6840 5.7 */
  *ad_reqd = header->hb4 & HB4_AD;
  *do_bit = 0;

  /* If there is an RFC2671 pseudoheader then it will be overwritten by
     partial replies, so we have to do a dry run to see if we can answer
     the query. We check to see if the do bit is set, if so we always
     forward rather than answering from the cache, which doesn't include
     security information, unless we're in DNSSEC validation mode. */

  if (find_pseudoheader(header, qlen, NULL, &pheader, &is_sign))
    { 
      unsigned short udpsz, flags;
      unsigned char *psave = pheader;

      have_pseudoheader = 1;

      GETSHORT(udpsz, pheader);
      pheader += 2; /* ext_rcode */
      GETSHORT(flags, pheader);
      
      if ((sec_reqd = flags & 0x8000))
	*do_bit = 1;/* do bit */ 
      *ad_reqd = 1;

      /* If our client is advertising a larger UDP packet size
	 than we allow, trim it so that we don't get an overlarge
	 response from upstream */

      if (!is_sign && (udpsz > daemon->edns_pktsz))
	PUTSHORT(daemon->edns_pktsz, psave); 

      dryrun = 1;
    }

  if (ntohs(header->qdcount) == 0 || OPCODE(header) != QUERY )
    return 0;
  
  for (rec = daemon->mxnames; rec; rec = rec->next)
    rec->offset = 0;
  
 rerun:
  /* determine end of question section (we put answers there) */
  if (!(ansp = skip_questions(header, qlen)))
    return 0; /* bad packet */
   
  /* now process each question, answers go in RRs after the question */
  p = (unsigned char *)(header+1);

  for (q = ntohs(header->qdcount); q != 0; q--)
    {
      /* save pointer to name for copying into answers */
      nameoffset = p - (unsigned char *)header;

      /* now extract name as .-concatenated string into name */
      if (!extract_name(header, qlen, &p, name, 1, 4))
	return 0; /* bad packet */
            
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);

      ans = 0; /* have we answered this question */
      
      if (qtype == T_TXT || qtype == T_ANY)
	{
	  struct txt_record *t;
	  for(t = daemon->txt; t ; t = t->next)
	    {
	      if (t->class == qclass && hostname_isequal(name, t->name))
		{
		  ans = 1;
		  if (!dryrun)
		    {
		      unsigned long ttl = daemon->local_ttl;
		      int ok = 1;
		      log_query(F_CONFIG | F_RRNAME, name, NULL, "<TXT>");
		      /* Dynamically generate stat record */
		      if (t->stat != 0)
			{
			  ttl = 0;
			  if (!cache_make_stat(t))
			    ok = 0;
			}
		      
		      if (ok && add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						    ttl, NULL,
						    T_TXT, t->class, "t", t->len, t->txt))
			anscount++;

		    }
		}
	    }
	}

#ifdef HAVE_DNSSEC
      if (option_bool(OPT_DNSSEC_VALID) && (qtype == T_DNSKEY || qtype == T_DS))
	{
	  int gotone = 0;
	  struct blockdata *keydata;

	  /* Do we have RRSIG? Can't do DS or DNSKEY otherwise. */
	  if (sec_reqd)
	    {
	      crecp = NULL;
	      while ((crecp = cache_find_by_name(crecp, name, now, F_DNSKEY | F_DS)))
		if (crecp->uid == qclass && crecp->addr.sig.type_covered == qtype)
		  break;
	    }
	  
	  if (!sec_reqd || crecp)
	    {
	      if (qtype == T_DS)
		{
		  crecp = NULL;
		  while ((crecp = cache_find_by_name(crecp, name, now, F_DS)))
		    if (crecp->uid == qclass)
		      {
			gotone = 1; 
			if (!dryrun)
			  {
			    if (crecp->flags & F_NEG)
			      {
				if (crecp->flags & F_NXDOMAIN)
				  nxdomain = 1;
				log_query(F_UPSTREAM, name, NULL, "no DS");	
			      }
			    else if ((keydata = blockdata_retrieve(crecp->addr.ds.keydata, crecp->addr.ds.keylen, NULL)))
			      {			     			      
				struct all_addr a;
				a.addr.keytag =  crecp->addr.ds.keytag;
				log_query(F_KEYTAG | (crecp->flags & F_CONFIG), name, &a, "DS keytag %u");
				if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							crec_ttl(crecp, now), &nameoffset,
							T_DS, qclass, "sbbt", 
							crecp->addr.ds.keytag, crecp->addr.ds.algo, 
							crecp->addr.ds.digest, crecp->addr.ds.keylen, keydata))
				  anscount++;
				
			      } 
			  }
		      }
		}
	      else /* DNSKEY */
		{
		  crecp = NULL;
		  while ((crecp = cache_find_by_name(crecp, name, now, F_DNSKEY)))
		    if (crecp->uid == qclass)
		      {
			gotone = 1;
			if (!dryrun && (keydata = blockdata_retrieve(crecp->addr.key.keydata, crecp->addr.key.keylen, NULL)))
			  {			     			      
			    struct all_addr a;
			    a.addr.keytag =  crecp->addr.key.keytag;
			    log_query(F_KEYTAG | (crecp->flags & F_CONFIG), name, &a, "DNSKEY keytag %u");
			    if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						    crec_ttl(crecp, now), &nameoffset,
						    T_DNSKEY, qclass, "sbbt", 
						    crecp->addr.key.flags, 3, crecp->addr.key.algo, crecp->addr.key.keylen, keydata))
			      anscount++;
			  }
		      }
		}
	    }
	  
	  /* Now do RRSIGs */
	  if (gotone)
	    {
	      ans = 1;
	      auth = 0;
	      if (!dryrun && sec_reqd)
		{
		  crecp = NULL;
		  while ((crecp = cache_find_by_name(crecp, name, now, F_DNSKEY | F_DS)))
		    if (crecp->uid == qclass && crecp->addr.sig.type_covered == qtype &&
			(keydata = blockdata_retrieve(crecp->addr.sig.keydata, crecp->addr.sig.keylen, NULL)))
		      {
			add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					    crec_ttl(crecp, now), &nameoffset,
					    T_RRSIG, qclass, "t", crecp->addr.sig.keylen, keydata);
			anscount++;
		      }
		}
	    }
	}
#endif	     
      
      if (qclass == C_IN)
	{
	  struct txt_record *t;

	  for (t = daemon->rr; t; t = t->next)
	    if ((t->class == qtype || qtype == T_ANY) && hostname_isequal(name, t->name))
	      {
		ans = 1;
		if (!dryrun)
		  {
		    log_query(F_CONFIG | F_RRNAME, name, NULL, "<RR>");
		    if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					    daemon->local_ttl, NULL,
					    t->class, C_IN, "t", t->len, t->txt))
		      anscount ++;
		  }
	      }
		
	  if (qtype == T_PTR || qtype == T_ANY)
	    {
	      /* see if it's w.z.y.z.in-addr.arpa format */
	      int is_arpa = in_arpa_name_2_addr(name, &addr);
	      struct ptr_record *ptr;
	      struct interface_name* intr = NULL;

	      for (ptr = daemon->ptr; ptr; ptr = ptr->next)
		if (hostname_isequal(name, ptr->name))
		  break;

	      if (is_arpa == F_IPV4)
		for (intr = daemon->int_names; intr; intr = intr->next)
		  {
		    struct addrlist *addrlist;
		    
		    for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		      if (!(addrlist->flags & ADDRLIST_IPV6) && addr.addr.addr4.s_addr == addrlist->addr.addr.addr4.s_addr)
			break;
		    
		    if (addrlist)
		      break;
		    else
		      while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
			intr = intr->next;
		  }
#ifdef HAVE_IPV6
	      else if (is_arpa == F_IPV6)
		for (intr = daemon->int_names; intr; intr = intr->next)
		  {
		    struct addrlist *addrlist;
		    
		    for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		      if ((addrlist->flags & ADDRLIST_IPV6) && IN6_ARE_ADDR_EQUAL(&addr.addr.addr6, &addrlist->addr.addr.addr6))
			break;
		    
		    if (addrlist)
		      break;
		    else
		      while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
			intr = intr->next;
		  }
#endif
	      
	      if (intr)
		{
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(is_arpa | F_REVERSE | F_CONFIG, intr->name, &addr, NULL);
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					      daemon->local_ttl, NULL,
					      T_PTR, C_IN, "d", intr->name))
			anscount++;
		    }
		}
	      else if (ptr)
		{
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(F_CONFIG | F_RRNAME, name, NULL, "<PTR>");
		      for (ptr = daemon->ptr; ptr; ptr = ptr->next)
			if (hostname_isequal(name, ptr->name) &&
			    add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						daemon->local_ttl, NULL,
						T_PTR, C_IN, "d", ptr->ptr))
			  anscount++;
			 
		    }
		}
	      else if ((crecp = cache_find_by_addr(NULL, &addr, now, is_arpa)))
		{
		  if (!(crecp->flags & (F_HOSTS | F_DHCP | F_CONFIG)) && sec_reqd)
		    {
		      if (!option_bool(OPT_DNSSEC_VALID) || ((crecp->flags & F_NEG) && (crecp->flags & F_DNSSECOK)))
			crecp = NULL;
#ifdef HAVE_DNSSEC
		      else if (crecp->flags & F_DNSSECOK)
			{
			  int gotsig = 0;
			  struct crec *rr_crec = NULL;

			  while ((rr_crec = cache_find_by_name(rr_crec, name, now, F_DS | F_DNSKEY)))
			    {
			      if (rr_crec->addr.sig.type_covered == T_PTR && rr_crec->uid == C_IN)
				{
				  char *sigdata = blockdata_retrieve(rr_crec->addr.sig.keydata, rr_crec->addr.sig.keylen, NULL);
				  gotsig = 1;
				  
				  if (!dryrun && 
				      add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							  rr_crec->ttd - now, &nameoffset,
							  T_RRSIG, C_IN, "t", crecp->addr.sig.keylen, sigdata))
				    anscount++;
				}
			    } 
			  
			  if (!gotsig)
			    crecp = NULL;
			}
#endif
		    }

		  if (crecp)
		    {
		      do 
			{ 
			  /* don't answer wildcard queries with data not from /etc/hosts or dhcp leases */
			  if (qtype == T_ANY && !(crecp->flags & (F_HOSTS | F_DHCP)))
			    continue;
			  
			  if (!(crecp->flags & F_DNSSECOK))
			    sec_data = 0;
			  
			  if (crecp->flags & F_NEG)
			    {
			      ans = 1;
			      auth = 0;
			      if (crecp->flags & F_NXDOMAIN)
				nxdomain = 1;
			      if (!dryrun)
				log_query(crecp->flags & ~F_FORWARD, name, &addr, NULL);
			    }
			  else if ((crecp->flags & (F_HOSTS | F_DHCP)) || !sec_reqd || option_bool(OPT_DNSSEC_VALID))
			    {
			      ans = 1;
			      if (!(crecp->flags & (F_HOSTS | F_DHCP)))
				auth = 0;
			      if (!dryrun)
				{
				  log_query(crecp->flags & ~F_FORWARD, cache_get_name(crecp), &addr, 
					    record_source(crecp->uid));
				  
				  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							  crec_ttl(crecp, now), NULL,
							  T_PTR, C_IN, "d", cache_get_name(crecp)))
				    anscount++;
				}
			    }
			} while ((crecp = cache_find_by_addr(crecp, &addr, now, is_arpa)));
		    }
		}
	      else if (is_rev_synth(is_arpa, &addr, name))
		{
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(F_CONFIG | F_REVERSE | is_arpa, name, &addr, NULL); 
		      
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					      daemon->local_ttl, NULL,
					      T_PTR, C_IN, "d", name))
			      anscount++;
		    }
		}
	      else if (is_arpa == F_IPV4 && 
		       option_bool(OPT_BOGUSPRIV) && 
		       private_net(addr.addr.addr4, 1))
		{
		  /* if not in cache, enabled and private IPV4 address, return NXDOMAIN */
		  ans = 1;
		  nxdomain = 1;
		  if (!dryrun)
		    log_query(F_CONFIG | F_REVERSE | F_IPV4 | F_NEG | F_NXDOMAIN, 
			      name, &addr, NULL);
		}
	    }
	    
	  for (flag = F_IPV4; flag; flag = (flag == F_IPV4) ? F_IPV6 : 0)
	    {
	      unsigned short type = T_A;
	      struct interface_name *intr;

	      if (flag == F_IPV6)
#ifdef HAVE_IPV6
		type = T_AAAA;
#else
	        break;
#endif
	      
	      if (qtype != type && qtype != T_ANY)
		continue;
	      
	      /* Check for "A for A"  queries; be rather conservative 
		 about what looks like dotted-quad.  */
	      if (qtype == T_A)
		{
		  char *cp;
		  unsigned int i, a;
		  int x;

		  for (cp = name, i = 0, a = 0; *cp; i++)
		    {
		      if (!isdigit((unsigned char)*cp) || (x = strtol(cp, &cp, 10)) > 255) 
			{
			  i = 5;
			  break;
			}
		      
		      a = (a << 8) + x;
		      
		      if (*cp == '.') 
			cp++;
		    }
		  
		  if (i == 4)
		    {
		      ans = 1;
		      if (!dryrun)
			{
			  addr.addr.addr4.s_addr = htonl(a);
			  log_query(F_FORWARD | F_CONFIG | F_IPV4, name, &addr, NULL);
			  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						  daemon->local_ttl, NULL, type, C_IN, "4", &addr))
			    anscount++;
			}
		      continue;
		    }
		}

	      /* interface name stuff */
	    intname_restart:
	      for (intr = daemon->int_names; intr; intr = intr->next)
		if (hostname_isequal(name, intr->name))
		  break;
	      
	      if (intr)
		{
		  struct addrlist *addrlist;
		  int gotit = 0;

		  enumerate_interfaces(0);
		  
		  for (intr = daemon->int_names; intr; intr = intr->next)
		    if (hostname_isequal(name, intr->name))
		      {
			for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
#ifdef HAVE_IPV6
			  if (((addrlist->flags & ADDRLIST_IPV6) ? T_AAAA : T_A) == type)
#endif
			    {
#ifdef HAVE_IPV6
			      if (addrlist->flags & ADDRLIST_REVONLY)
				continue;
#endif	
			      ans = 1;	
			      if (!dryrun)
				{
				  gotit = 1;
				  log_query(F_FORWARD | F_CONFIG | flag, name, &addrlist->addr, NULL);
				  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							  daemon->local_ttl, NULL, type, C_IN, 
							  type == T_A ? "4" : "6", &addrlist->addr))
				    anscount++;
				}
			    }
		      }
		  
		  if (!dryrun && !gotit)
		    log_query(F_FORWARD | F_CONFIG | flag | F_NEG, name, NULL, NULL);
		     
		  continue;
		}

	    cname_restart:
	      if ((crecp = cache_find_by_name(NULL, name, now, flag | F_CNAME | (dryrun ? F_NO_RR : 0))))
		{
		  int localise = 0;
		  
		  /* See if a putative address is on the network from which we recieved
		     the query, is so we'll filter other answers. */
		  if (local_addr.s_addr != 0 && option_bool(OPT_LOCALISE) && flag == F_IPV4)
		    {
		      struct crec *save = crecp;
		      do {
			if ((crecp->flags & F_HOSTS) &&
			    is_same_net(*((struct in_addr *)&crecp->addr), local_addr, local_netmask))
			  {
			    localise = 1;
			    break;
			  } 
			} while ((crecp = cache_find_by_name(crecp, name, now, flag | F_CNAME)));
		      crecp = save;
		    }

		  /* If the client asked for DNSSEC and we can't provide RRSIGs, either
		     because we've not doing DNSSEC or the cached answer is signed by negative,
		     don't answer from the cache, forward instead. */
		  if (!(crecp->flags & (F_HOSTS | F_DHCP | F_CONFIG)) && sec_reqd)
		    {
		      if (!option_bool(OPT_DNSSEC_VALID) || ((crecp->flags & F_NEG) && (crecp->flags & F_DNSSECOK)))
			crecp = NULL;
#ifdef HAVE_DNSSEC
		      else if (crecp->flags & F_DNSSECOK)
			{
			  /* We're returning validated data, need to return the RRSIG too. */
			  struct crec *rr_crec = NULL;
			  int sigtype = type;
			  /* The signature may have expired even though the data is still in cache, 
			     forward instead of answering from cache if so. */
			  int gotsig = 0;
			  
			  if (crecp->flags & F_CNAME)
			    sigtype = T_CNAME;
			  
			  while ((rr_crec = cache_find_by_name(rr_crec, name, now, F_DS | F_DNSKEY)))
			    {
			      if (rr_crec->addr.sig.type_covered == sigtype && rr_crec->uid == C_IN)
				{
				  char *sigdata = blockdata_retrieve(rr_crec->addr.sig.keydata, rr_crec->addr.sig.keylen, NULL);
				  gotsig = 1;
				  
				  if (!dryrun && 
				      add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							  rr_crec->ttd - now, &nameoffset,
							  T_RRSIG, C_IN, "t", rr_crec->addr.sig.keylen, sigdata))
				    anscount++;
				}
			    }
			  
			  if (!gotsig)
			    crecp = NULL;
			}
#endif
		    }		 

		  if (crecp)
		    do
		      { 
			/* don't answer wildcard queries with data not from /etc/hosts
			   or DHCP leases */
			if (qtype == T_ANY && !(crecp->flags & (F_HOSTS | F_DHCP | F_CONFIG)))
			  break;
			
			if (!(crecp->flags & F_DNSSECOK))
			  sec_data = 0;
			
			if (crecp->flags & F_CNAME)
			  {
			    char *cname_target = cache_get_cname_target(crecp);
			    
			    if (!dryrun)
			      {
				log_query(crecp->flags, name, NULL, record_source(crecp->uid));
				if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							crec_ttl(crecp, now), &nameoffset,
							T_CNAME, C_IN, "d", cname_target))
				  anscount++;
			      }
			    
			    strcpy(name, cname_target);
			    /* check if target interface_name */
			    if (crecp->addr.cname.uid == SRC_INTERFACE)
			      goto intname_restart;
			    else
			      goto cname_restart;
			  }
			
			if (crecp->flags & F_NEG)
			  {
			    /* We don't cache NSEC records, so if a DNSSEC-validated negative answer
			       is cached and the client wants DNSSEC, forward rather than answering from the cache */
			    if (!sec_reqd || !(crecp->flags & F_DNSSECOK))
			      {
				ans = 1;
				auth = 0;
				if (crecp->flags & F_NXDOMAIN)
				  nxdomain = 1;
				if (!dryrun)
				  log_query(crecp->flags, name, NULL, NULL);
			      }
			  }
			else 
			  {
			    /* If we are returning local answers depending on network,
			       filter here. */
			    if (localise && 
				(crecp->flags & F_HOSTS) &&
				!is_same_net(*((struct in_addr *)&crecp->addr), local_addr, local_netmask))
			      continue;
			    
			    if (!(crecp->flags & (F_HOSTS | F_DHCP)))
			      auth = 0;
			    
			    ans = 1;
			    if (!dryrun)
			      {
				log_query(crecp->flags & ~F_REVERSE, name, &crecp->addr.addr,
					  record_source(crecp->uid));
				
				if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
							crec_ttl(crecp, now), NULL, type, C_IN, 
							type == T_A ? "4" : "6", &crecp->addr))
				  anscount++;
			      }
			  }
		      } while ((crecp = cache_find_by_name(crecp, name, now, flag | F_CNAME)));
		}
	      else if (is_name_synthetic(flag, name, &addr))
		{
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(F_FORWARD | F_CONFIG | flag, name, &addr, NULL);
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					      daemon->local_ttl, NULL, type, C_IN, type == T_A ? "4" : "6", &addr))
			anscount++;
		    }
		}
	    }

	  if (qtype == T_CNAME || qtype == T_ANY)
	    {
	      if ((crecp = cache_find_by_name(NULL, name, now, F_CNAME)) &&
		  (qtype == T_CNAME || (crecp->flags & (F_HOSTS | F_DHCP | F_CONFIG  | (dryrun ? F_NO_RR : 0)))))
		{
		  if (!(crecp->flags & F_DNSSECOK))
		    sec_data = 0;
		  
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(crecp->flags, name, NULL, record_source(crecp->uid));
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					      crec_ttl(crecp, now), &nameoffset,
					      T_CNAME, C_IN, "d", cache_get_cname_target(crecp)))
			anscount++;
		    }
		}
	    }

	  if (qtype == T_MX || qtype == T_ANY)
	    {
	      int found = 0;
	      for (rec = daemon->mxnames; rec; rec = rec->next)
		if (!rec->issrv && hostname_isequal(name, rec->name))
		  {
		  ans = found = 1;
		  if (!dryrun)
		    {
		      int offset;
		      log_query(F_CONFIG | F_RRNAME, name, NULL, "<MX>");
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->local_ttl,
					      &offset, T_MX, C_IN, "sd", rec->weight, rec->target))
			{
			  anscount++;
			  if (rec->target)
			    rec->offset = offset;
			}
		    }
		  }
	      
	      if (!found && (option_bool(OPT_SELFMX) || option_bool(OPT_LOCALMX)) && 
		  cache_find_by_name(NULL, name, now, F_HOSTS | F_DHCP | F_NO_RR))
		{ 
		  ans = 1;
		  if (!dryrun)
		    {
		      log_query(F_CONFIG | F_RRNAME, name, NULL, "<MX>");
		      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->local_ttl, NULL, 
					      T_MX, C_IN, "sd", 1, 
					      option_bool(OPT_SELFMX) ? name : daemon->mxtarget))
			anscount++;
		    }
		}
	    }
	  	  
	  if (qtype == T_SRV || qtype == T_ANY)
	    {
	      int found = 0;
	      struct mx_srv_record *move = NULL, **up = &daemon->mxnames;

	      for (rec = daemon->mxnames; rec; rec = rec->next)
		if (rec->issrv && hostname_isequal(name, rec->name))
		  {
		    found = ans = 1;
		    if (!dryrun)
		      {
			int offset;
			log_query(F_CONFIG | F_RRNAME, name, NULL, "<SRV>");
			if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->local_ttl, 
						&offset, T_SRV, C_IN, "sssd", 
						rec->priority, rec->weight, rec->srvport, rec->target))
			  {
			    anscount++;
			    if (rec->target)
			      rec->offset = offset;
			  }
		      }
		    
		    /* unlink first SRV record found */
		    if (!move)
		      {
			move = rec;
			*up = rec->next;
		      }
		    else
		      up = &rec->next;      
		  }
		else
		  up = &rec->next;

	      /* put first SRV record back at the end. */
	      if (move)
		{
		  *up = move;
		  move->next = NULL;
		}
	      
	      if (!found && option_bool(OPT_FILTER) && (qtype == T_SRV || (qtype == T_ANY && strchr(name, '_'))))
		{
		  ans = 1;
		  if (!dryrun)
		    log_query(F_CONFIG | F_NEG, name, NULL, NULL);
		}
	    }

	  if (qtype == T_NAPTR || qtype == T_ANY)
	    {
	      struct naptr *na;
	      for (na = daemon->naptr; na; na = na->next)
		if (hostname_isequal(name, na->name))
		  {
		    ans = 1;
		    if (!dryrun)
		      {
			log_query(F_CONFIG | F_RRNAME, name, NULL, "<NAPTR>");
			if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->local_ttl, 
						NULL, T_NAPTR, C_IN, "sszzzd", 
						na->order, na->pref, na->flags, na->services, na->regexp, na->replace))
			  anscount++;
		      }
		  }
	    }
	  
	  if (qtype == T_MAILB)
	    ans = 1, nxdomain = 1;

	  if (qtype == T_SOA && option_bool(OPT_FILTER))
	    {
	      ans = 1; 
	      if (!dryrun)
		log_query(F_CONFIG | F_NEG, name, &addr, NULL);
	    }
	}

      if (!ans)
	return 0; /* failed to answer a question */
    }
  
  if (dryrun)
    {
      dryrun = 0;
      goto rerun;
    }
  
  /* create an additional data section, for stuff in SRV and MX record replies. */
  for (rec = daemon->mxnames; rec; rec = rec->next)
    if (rec->offset != 0)
      {
	/* squash dupes */
	struct mx_srv_record *tmp;
	for (tmp = rec->next; tmp; tmp = tmp->next)
	  if (tmp->offset != 0 && hostname_isequal(rec->target, tmp->target))
	    tmp->offset = 0;
	
	crecp = NULL;
	while ((crecp = cache_find_by_name(crecp, rec->target, now, F_IPV4 | F_IPV6)))
	  {
#ifdef HAVE_IPV6
	    int type =  crecp->flags & F_IPV4 ? T_A : T_AAAA;
#else
	    int type = T_A;
#endif
	    if (crecp->flags & F_NEG)
	      continue;

	    if (add_resource_record(header, limit, NULL, rec->offset, &ansp, 
				    crec_ttl(crecp, now), NULL, type, C_IN, 
				    crecp->flags & F_IPV4 ? "4" : "6", &crecp->addr))
	      addncount++;
	  }
      }
  
  /* done all questions, set up header and return length of result */
  /* clear authoritative and truncated flags, set QR flag */
  header->hb3 = (header->hb3 & ~(HB3_AA | HB3_TC)) | HB3_QR;
  /* set RA flag */
  header->hb4 |= HB4_RA;
   
  /* authoritive - only hosts and DHCP derived names. */
  if (auth)
    header->hb3 |= HB3_AA;
  
  /* truncation */
  if (trunc)
    header->hb3 |= HB3_TC;
  
  if (nxdomain)
    SET_RCODE(header, NXDOMAIN);
  else
    SET_RCODE(header, NOERROR); /* no error */
  header->ancount = htons(anscount);
  header->nscount = htons(0);
  header->arcount = htons(addncount);

  len = ansp - (unsigned char *)header;
  
  if (have_pseudoheader)
    len = add_pseudoheader(header, len, (unsigned char *)limit, 0, NULL, 0, sec_reqd);
  
  if (*ad_reqd && sec_data)
    header->hb4 |= HB4_AD;
  else
    header->hb4 &= ~HB4_AD;
  
  return len;
}

