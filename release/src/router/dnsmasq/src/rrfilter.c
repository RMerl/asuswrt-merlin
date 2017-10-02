/* dnsmasq is Copyright (c) 2000-2017 Simon Kelley

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

/* Code to safely remove RRs from an DNS answer */ 

#include "dnsmasq.h"

/* Go through a domain name, find "pointers" and fix them up based on how many bytes
   we've chopped out of the packet, or check they don't point into an elided part.  */
static int check_name(unsigned char **namep, struct dns_header *header, size_t plen, int fixup, unsigned char **rrs, int rr_count)
{
  unsigned char *ansp = *namep;

  while(1)
    {
      unsigned int label_type;
      
      if (!CHECK_LEN(header, ansp, plen, 1))
	return 0;
      
      label_type = (*ansp) & 0xc0;

      if (label_type == 0xc0)
	{
	  /* pointer for compression. */
	  unsigned int offset;
	  int i;
	  unsigned char *p;
	  
	  if (!CHECK_LEN(header, ansp, plen, 2))
	    return 0;

	  offset = ((*ansp++) & 0x3f) << 8;
	  offset |= *ansp++;

	  p = offset + (unsigned char *)header;
	  
	  for (i = 0; i < rr_count; i++)
	    if (p < rrs[i])
	      break;
	    else
	      if (i & 1)
		offset -= rrs[i] - rrs[i-1];

	  /* does the pointer end up in an elided RR? */
	  if (i & 1)
	    return 0;

	  /* No, scale the pointer */
	  if (fixup)
	    {
	      ansp -= 2;
	      *ansp++ = (offset >> 8) | 0xc0;
	      *ansp++ = offset & 0xff;
	    }
	  break;
	}
      else if (label_type == 0x80)
	return 0; /* reserved */
      else if (label_type == 0x40)
	{
	  /* Extended label type */
	  unsigned int count;
	  
	  if (!CHECK_LEN(header, ansp, plen, 2))
	    return 0;
	  
	  if (((*ansp++) & 0x3f) != 1)
	    return 0; /* we only understand bitstrings */
	  
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
	    return 0;

	  if (len == 0)
	    break; /* zero length label marks the end. */
	}
    }

  *namep = ansp;

  return 1;
}

/* Go through RRs and check or fixup the domain names contained within */
static int check_rrs(unsigned char *p, struct dns_header *header, size_t plen, int fixup, unsigned char **rrs, int rr_count)
{
  int i, j, type, class, rdlen;
  unsigned char *pp;
  
  for (i = 0; i < ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount); i++)
    {
      pp = p;

      if (!(p = skip_name(p, header, plen, 10)))
	return 0;
      
      GETSHORT(type, p); 
      GETSHORT(class, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);

      /* If this RR is to be elided, don't fix up its contents */
      for (j = 0; j < rr_count; j += 2)
	if (rrs[j] == pp)
	  break;

      if (j >= rr_count)
	{
	  /* fixup name of RR */
	  if (!check_name(&pp, header, plen, fixup, rrs, rr_count))
	    return 0;
	  
	  if (class == C_IN)
	    {
	      u16 *d;
 
	      for (pp = p, d = rrfilter_desc(type); *d != (u16)-1; d++)
		{
		  if (*d != 0)
		    pp += *d;
		  else if (!check_name(&pp, header, plen, fixup, rrs, rr_count))
		    return 0;
		}
	    }
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return 0;
    }
  
  return 1;
}
	

/* mode is 0 to remove EDNS0, 1 to filter DNSSEC RRs */
size_t rrfilter(struct dns_header *header, size_t plen, int mode)
{
  static unsigned char **rrs;
  static int rr_sz = 0;

  unsigned char *p = (unsigned char *)(header+1);
  int i, rdlen, qtype, qclass, rr_found, chop_an, chop_ns, chop_ar;

  if (ntohs(header->qdcount) != 1 ||
      !(p = skip_name(p, header, plen, 4)))
    return plen;
  
  GETSHORT(qtype, p);
  GETSHORT(qclass, p);

  /* First pass, find pointers to start and end of all the records we wish to elide:
     records added for DNSSEC, unless explicitly queried for */
  for (rr_found = 0, chop_ns = 0, chop_an = 0, chop_ar = 0, i = 0; 
       i < ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount);
       i++)
    {
      unsigned char *pstart = p;
      int type, class;

      if (!(p = skip_name(p, header, plen, 10)))
	return plen;
      
      GETSHORT(type, p); 
      GETSHORT(class, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);
        
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return plen;

      /* Don't remove the answer. */
      if (i < ntohs(header->ancount) && type == qtype && class == qclass)
	continue;
      
      if (mode == 0) /* EDNS */
	{
	  /* EDNS mode, remove T_OPT from additional section only */
	  if (i < (ntohs(header->nscount) + ntohs(header->ancount)) || type != T_OPT)
	    continue;
	}
      else if (type != T_NSEC && type != T_NSEC3 && type != T_RRSIG)
	/* DNSSEC mode, remove SIGs and NSECs from all three sections. */
	continue;
      
      
      if (!expand_workspace(&rrs, &rr_sz, rr_found + 1))
	return plen; 
      
      rrs[rr_found++] = pstart;
      rrs[rr_found++] = p;
      
      if (i < ntohs(header->ancount))
	chop_an++;
      else if (i < (ntohs(header->nscount) + ntohs(header->ancount)))
	chop_ns++;
      else
	chop_ar++;
    }
  
  /* Nothing to do. */
  if (rr_found == 0)
    return plen;

  /* Second pass, look for pointers in names in the records we're keeping and make sure they don't
     point to records we're going to elide. This is theoretically possible, but unlikely. If
     it happens, we give up and leave the answer unchanged. */
  p = (unsigned char *)(header+1);
  
  /* question first */
  if (!check_name(&p, header, plen, 0, rrs, rr_found))
    return plen;
  p += 4; /* qclass, qtype */
  
  /* Now answers and NS */
  if (!check_rrs(p, header, plen, 0, rrs, rr_found))
    return plen;
  
  /* Third pass, actually fix up pointers in the records */
  p = (unsigned char *)(header+1);
  
  check_name(&p, header, plen, 1, rrs, rr_found);
  p += 4; /* qclass, qtype */
  
  check_rrs(p, header, plen, 1, rrs, rr_found);

  /*  Fouth pass, elide records */
  for (p = rrs[0], i = 1; i < rr_found; i += 2)
    {
      unsigned char *start = rrs[i];
      unsigned char *end = (i != rr_found - 1) ? rrs[i+1] : ((unsigned char *)header) + plen;
      
      memmove(p, start, end-start);
      p += end-start;
    }
     
  plen = p - (unsigned char *)header;
  header->ancount = htons(ntohs(header->ancount) - chop_an);
  header->nscount = htons(ntohs(header->nscount) - chop_ns);
  header->arcount = htons(ntohs(header->arcount) - chop_ar);

  return plen;
}

/* This is used in the DNSSEC code too, hence it's exported */
u16 *rrfilter_desc(int type)
{
  /* List of RRtypes which include domains in the data.
     0 -> domain
     integer -> no of plain bytes
     -1 -> end

     zero is not a valid RRtype, so the final entry is returned for
     anything which needs no mangling.
  */
  
  static u16 rr_desc[] = 
    { 
      T_NS, 0, -1, 
      T_MD, 0, -1,
      T_MF, 0, -1,
      T_CNAME, 0, -1,
      T_SOA, 0, 0, -1,
      T_MB, 0, -1,
      T_MG, 0, -1,
      T_MR, 0, -1,
      T_PTR, 0, -1,
      T_MINFO, 0, 0, -1,
      T_MX, 2, 0, -1,
      T_RP, 0, 0, -1,
      T_AFSDB, 2, 0, -1,
      T_RT, 2, 0, -1,
      T_SIG, 18, 0, -1,
      T_PX, 2, 0, 0, -1,
      T_NXT, 0, -1,
      T_KX, 2, 0, -1,
      T_SRV, 6, 0, -1,
      T_DNAME, 0, -1,
      0, -1 /* wildcard/catchall */
    }; 
  
  u16 *p = rr_desc;
  
  while (*p != type && *p != 0)
    while (*p++ != (u16)-1);

  return p+1;
}

int expand_workspace(unsigned char ***wkspc, int *szp, int new)
{
  unsigned char **p;
  int old = *szp;

  if (old >= new+1)
    return 1;

  if (new >= 100)
    return 0;

  new += 5;
  
  if (!(p = whine_malloc(new * sizeof(unsigned char *))))
    return 0;  
  
  if (old != 0 && *wkspc)
    {
      memcpy(p, *wkspc, old * sizeof(unsigned char *));
      free(*wkspc);
    }
  
  *wkspc = p;
  *szp = new;

  return 1;
}
