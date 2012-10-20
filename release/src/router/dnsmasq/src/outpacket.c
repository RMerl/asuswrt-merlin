/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

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
 
#ifdef HAVE_DHCP6

static size_t outpacket_counter;

void end_opt6(int container)
{
   void *p = daemon->outpacket.iov_base + container + 2;
   u16 len = outpacket_counter - container - 4 ;
   
   PUTSHORT(len, p);
}

int save_counter(int newval)
{
  int ret = outpacket_counter;
  if (newval != -1)
    outpacket_counter = newval;

  return ret;
}

void *expand(size_t headroom)
{
  void *ret;

  if (expand_buf(&daemon->outpacket, outpacket_counter + headroom))
    {
      ret = daemon->outpacket.iov_base + outpacket_counter;
      outpacket_counter += headroom;
      return ret;
    }
  
  return NULL;
}
    
int new_opt6(int opt)
{
  int ret = outpacket_counter;
  void *p;

  if ((p = expand(4)))
    {
      PUTSHORT(opt, p);
      PUTSHORT(0, p);
    }

  return ret;
}

void *put_opt6(void *data, size_t len)
{
  void *p;

  if ((p = expand(len)))
    memcpy(p, data, len);   

  return p;
}
  
void put_opt6_long(unsigned int val)
{
  void *p;
  
  if ((p = expand(4)))  
    PUTLONG(val, p);
}

void put_opt6_short(unsigned int val)
{
  void *p;

  if ((p = expand(2)))
    PUTSHORT(val, p);   
}

void put_opt6_char(unsigned int val)
{
  unsigned char *p;

  if ((p = expand(1)))
    *p = val;   
}

void put_opt6_string(char *s)
{
  put_opt6(s, strlen(s));
}

#endif
