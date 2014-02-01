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

/* The SURF random number generator was taken from djbdns-1.05, by 
   Daniel J Bernstein, which is public domain. */


#include "dnsmasq.h"

#ifdef HAVE_BROKEN_RTC
#include <sys/times.h>
#endif

#if defined(LOCALEDIR) || defined(HAVE_IDN)
#include <idna.h>
#endif

/* SURF random number generator */

static u32 seed[32];
static u32 in[12];
static u32 out[8];
static int outleft = 0;

void rand_init()
{
  int fd = open(RANDFILE, O_RDONLY);
  
  if (fd == -1 ||
      !read_write(fd, (unsigned char *)&seed, sizeof(seed), 1) ||
      !read_write(fd, (unsigned char *)&in, sizeof(in), 1))
    die(_("failed to seed the random number generator: %s"), NULL, EC_MISC);
  
  close(fd);
}

#define ROTATE(x,b) (((x) << (b)) | ((x) >> (32 - (b))))
#define MUSH(i,b) x = t[i] += (((x ^ seed[i]) + sum) ^ ROTATE(x,b));

static void surf(void)
{
  u32 t[12]; u32 x; u32 sum = 0;
  int r; int i; int loop;

  for (i = 0;i < 12;++i) t[i] = in[i] ^ seed[12 + i];
  for (i = 0;i < 8;++i) out[i] = seed[24 + i];
  x = t[11];
  for (loop = 0;loop < 2;++loop) {
    for (r = 0;r < 16;++r) {
      sum += 0x9e3779b9;
      MUSH(0,5) MUSH(1,7) MUSH(2,9) MUSH(3,13)
      MUSH(4,5) MUSH(5,7) MUSH(6,9) MUSH(7,13)
      MUSH(8,5) MUSH(9,7) MUSH(10,9) MUSH(11,13)
    }
    for (i = 0;i < 8;++i) out[i] ^= t[i + 4];
  }
}

unsigned short rand16(void)
{
  if (!outleft) 
    {
      if (!++in[0]) if (!++in[1]) if (!++in[2]) ++in[3];
      surf();
      outleft = 8;
    }
  
  return (unsigned short) out[--outleft];
}

u64 rand64(void)
{
  static int outleft = 0;

  if (outleft < 2)
    {
      if (!++in[0]) if (!++in[1]) if (!++in[2]) ++in[3];
      surf();
      outleft = 8;
    }
  
  outleft -= 2;

  return (u64)out[outleft+1] + (((u64)out[outleft]) << 32);
}

static int check_name(char *in)
{
  /* remove trailing . 
     also fail empty string and label > 63 chars */
  size_t dotgap = 0, l = strlen(in);
  char c;
  int nowhite = 0;
  
  if (l == 0 || l > MAXDNAME) return 0;
  
  if (in[l-1] == '.')
    {
      in[l-1] = 0;
      nowhite = 1;
    }

  for (; (c = *in); in++)
    {
      if (c == '.')
	dotgap = 0;
      else if (++dotgap > MAXLABEL)
	return 0;
      else if (isascii((unsigned char)c) && iscntrl((unsigned char)c)) 
	/* iscntrl only gives expected results for ascii */
	return 0;
#if !defined(LOCALEDIR) && !defined(HAVE_IDN)
      else if (!isascii((unsigned char)c))
	return 0;
#endif
      else if (c != ' ')
	nowhite = 1;
    }

  if (!nowhite)
    return 0;

  return 1;
}

/* Hostnames have a more limited valid charset than domain names
   so check for legal char a-z A-Z 0-9 - _ 
   Note that this may receive a FQDN, so only check the first label 
   for the tighter criteria. */
int legal_hostname(char *name)
{
  char c;
  int first;

  if (!check_name(name))
    return 0;

  for (first = 1; (c = *name); name++, first = 0)
    /* check for legal char a-z A-Z 0-9 - _ . */
    {
      if ((c >= 'A' && c <= 'Z') ||
	  (c >= 'a' && c <= 'z') ||
	  (c >= '0' && c <= '9'))
	continue;

      if (!first && (c == '-' || c == '_'))
	continue;
      
      /* end of hostname part */
      if (c == '.')
	return 1;
      
      return 0;
    }
  
  return 1;
}
  
char *canonicalise(char *in, int *nomem)
{
  char *ret = NULL;
#if defined(LOCALEDIR) || defined(HAVE_IDN)
  int rc;
#endif

  if (nomem)
    *nomem = 0;
  
  if (!check_name(in))
    return NULL;
  
#if defined(LOCALEDIR) || defined(HAVE_IDN)
  if ((rc = idna_to_ascii_lz(in, &ret, 0)) != IDNA_SUCCESS)
    {
      if (ret)
	free(ret);

      if (nomem && (rc == IDNA_MALLOC_ERROR || rc == IDNA_DLOPEN_ERROR))
	{
	  my_syslog(LOG_ERR, _("failed to allocate memory"));
	  *nomem = 1;
	}
    
      return NULL;
    }
#else
  if ((ret = whine_malloc(strlen(in)+1)))
    strcpy(ret, in);
  else if (nomem)
    *nomem = 1;    
#endif

  return ret;
}

unsigned char *do_rfc1035_name(unsigned char *p, char *sval)
{
  int j;
  
  while (sval && *sval)
    {
      unsigned char *cp = p++;
      for (j = 0; *sval && (*sval != '.'); sval++, j++)
	*p++ = *sval;
      *cp  = j;
      if (*sval)
	sval++;
    }
  return p;
}

/* for use during startup */
void *safe_malloc(size_t size)
{
  void *ret = malloc(size);
  
  if (!ret)
    die(_("could not get memory"), NULL, EC_NOMEM);
     
  return ret;
}    

void safe_pipe(int *fd, int read_noblock)
{
  if (pipe(fd) == -1 || 
      !fix_fd(fd[1]) ||
      (read_noblock && !fix_fd(fd[0])))
    die(_("cannot create pipe: %s"), NULL, EC_MISC);
}

void *whine_malloc(size_t size)
{
  void *ret = malloc(size);

  if (!ret)
    my_syslog(LOG_ERR, _("failed to allocate %d bytes"), (int) size);

  return ret;
}

int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2)
{
  if (s1->sa.sa_family == s2->sa.sa_family)
    { 
      if (s1->sa.sa_family == AF_INET &&
	  s1->in.sin_port == s2->in.sin_port &&
	  s1->in.sin_addr.s_addr == s2->in.sin_addr.s_addr)
	return 1;
#ifdef HAVE_IPV6      
      if (s1->sa.sa_family == AF_INET6 &&
	  s1->in6.sin6_port == s2->in6.sin6_port &&
	  IN6_ARE_ADDR_EQUAL(&s1->in6.sin6_addr, &s2->in6.sin6_addr))
	return 1;
#endif
    }
  return 0;
}

int sa_len(union mysockaddr *addr)
{
#ifdef HAVE_SOCKADDR_SA_LEN
  return addr->sa.sa_len;
#else
#ifdef HAVE_IPV6
  if (addr->sa.sa_family == AF_INET6)
    return sizeof(addr->in6);
  else
#endif
    return sizeof(addr->in); 
#endif
}

/* don't use strcasecmp and friends here - they may be messed up by LOCALE */
int hostname_isequal(const char *a, const char *b)
{
  unsigned int c1, c2;
  
  do {
    c1 = (unsigned char) *a++;
    c2 = (unsigned char) *b++;
    
    if (c1 >= 'A' && c1 <= 'Z')
      c1 += 'a' - 'A';
    if (c2 >= 'A' && c2 <= 'Z')
      c2 += 'a' - 'A';
    
    if (c1 != c2)
      return 0;
  } while (c1);
  
  return 1;
}
    
time_t dnsmasq_time(void)
{
#ifdef HAVE_BROKEN_RTC
  struct tms dummy;
  static long tps = 0;

  if (tps == 0)
    tps = sysconf(_SC_CLK_TCK);

  return (time_t)(times(&dummy)/tps);
#else
  return time(NULL);
#endif
}

int is_same_net(struct in_addr a, struct in_addr b, struct in_addr mask)
{
  return (a.s_addr & mask.s_addr) == (b.s_addr & mask.s_addr);
} 

#ifdef HAVE_IPV6
int is_same_net6(struct in6_addr *a, struct in6_addr *b, int prefixlen)
{
  int pfbytes = prefixlen >> 3;
  int pfbits = prefixlen & 7;

  if (memcmp(&a->s6_addr, &b->s6_addr, pfbytes) != 0)
    return 0;

  if (pfbits == 0 ||
      (a->s6_addr[pfbytes] >> (8 - pfbits) == b->s6_addr[pfbytes] >> (8 - pfbits)))
    return 1;

  return 0;
}

/* return least signigicant 64 bits if IPv6 address */
u64 addr6part(struct in6_addr *addr)
{
  int i;
  u64 ret = 0;

  for (i = 8; i < 16; i++)
    ret = (ret << 8) + addr->s6_addr[i];

  return ret;
}

void setaddr6part(struct in6_addr *addr, u64 host)
{
  int i;

  for (i = 15; i >= 8; i--)
    {
      addr->s6_addr[i] = host;
      host = host >> 8;
    }
}

#endif
 

/* returns port number from address */
int prettyprint_addr(union mysockaddr *addr, char *buf)
{
  int port = 0;
  
#ifdef HAVE_IPV6
  if (addr->sa.sa_family == AF_INET)
    {
      inet_ntop(AF_INET, &addr->in.sin_addr, buf, ADDRSTRLEN);
      port = ntohs(addr->in.sin_port);
    }
  else if (addr->sa.sa_family == AF_INET6)
    {
      char name[IF_NAMESIZE];
      inet_ntop(AF_INET6, &addr->in6.sin6_addr, buf, ADDRSTRLEN);
      if (addr->in6.sin6_scope_id != 0 &&
	  if_indextoname(addr->in6.sin6_scope_id, name) &&
	  strlen(buf) + strlen(name) + 2 <= ADDRSTRLEN)
	{
	  strcat(buf, "%");
	  strcat(buf, name);
	}
      port = ntohs(addr->in6.sin6_port);
    }
#else
  strcpy(buf, inet_ntoa(addr->in.sin_addr));
  port = ntohs(addr->in.sin_port); 
#endif
  
  return port;
}

void prettyprint_time(char *buf, unsigned int t)
{
  if (t == 0xffffffff)
    sprintf(buf, _("infinite"));
  else
    {
      unsigned int x, p = 0;
       if ((x = t/86400))
	p += sprintf(&buf[p], "%dd", x);
       if ((x = (t/3600)%24))
	p += sprintf(&buf[p], "%dh", x);
      if ((x = (t/60)%60))
	p += sprintf(&buf[p], "%dm", x);
      if ((x = t%60))
	p += sprintf(&buf[p], "%ds", x);
    }
}


/* in may equal out, when maxlen may be -1 (No max len). 
   Return -1 for extraneous no-hex chars found. */
int parse_hex(char *in, unsigned char *out, int maxlen, 
	      unsigned int *wildcard_mask, int *mac_type)
{
  int mask = 0, i = 0;
  char *r;
    
  if (mac_type)
    *mac_type = 0;
  
  while (maxlen == -1 || i < maxlen)
    {
      for (r = in; *r != 0 && *r != ':' && *r != '-' && *r != ' '; r++)
	if (*r != '*' && !isxdigit((unsigned char)*r))
	  return -1;
      
      if (*r == 0)
	maxlen = i;
      
      if (r != in )
	{
	  if (*r == '-' && i == 0 && mac_type)
	   {
	      *r = 0;
	      *mac_type = strtol(in, NULL, 16);
	      mac_type = NULL;
	   }
	  else
	    {
	      *r = 0;
	      if (strcmp(in, "*") == 0)
		{
		  mask = (mask << 1) | 1;
		  i++;
		}
	      else
		{
		  int j, bytes = (1 + (r - in))/2;
		  for (j = 0; j < bytes; j++)
		    { 
		      char sav = sav;
		      if (j < bytes - 1)
			{
			  sav = in[(j+1)*2];
			  in[(j+1)*2] = 0;
			}
		      out[i] = strtol(&in[j*2], NULL, 16);
		      mask = mask << 1;
		      i++;
		      if (j < bytes - 1)
			in[(j+1)*2] = sav;
		    }
		}
	    }
	}
      in = r+1;
    }
  
  if (wildcard_mask)
    *wildcard_mask = mask;

  return i;
}

#ifdef HAVE_DNSSEC
static int charval(char c)
{
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;

  if (c >= '0' && c <= '9')
    return c - '0' + 52;

  if (c == '+')
    return 62;
  
  if (c == '/')
    return 63;

  if (c == '=')
    return -1;

  return -2;
}

int parse_base64(char *in, char *out)
{
  char *p = out;
  int i, val[4];

  while (*in)
    {
      for (i = 0; i < 4; i++)
	{ 
	  while (*in == ' ') 
	    in++;
	  if (*in == 0)
	    return -1;
	  if ((val[i] = charval(*in++)) == -2)
	    return -1;
	}
          
      while (*in == ' ') 
	in++;
      
      if (val[1] == -1)
	return -1; /* too much padding */
      
      *p++ = (val[0] << 2) | (val[1] >> 4);
      
      if (val[2] != -1)
	*p++ = (val[1] << 4) | ( val[2] >> 2);

      if (val[3] != -1)
	*p++ = (val[2] << 6) | val[3];
    }

  return p - out;
}
#endif

/* return 0 for no match, or (no matched octets) + 1 */
int memcmp_masked(unsigned char *a, unsigned char *b, int len, unsigned int mask)
{
  int i, count;
  for (count = 1, i = len - 1; i >= 0; i--, mask = mask >> 1)
    if (!(mask & 1))
      {
	if (a[i] == b[i])
	  count++;
	else
	  return 0;
      }
  return count;
}

/* _note_ may copy buffer */
int expand_buf(struct iovec *iov, size_t size)
{
  void *new;

  if (size <= (size_t)iov->iov_len)
    return 1;

  if (!(new = whine_malloc(size)))
    {
      errno = ENOMEM;
      return 0;
    }

  if (iov->iov_base)
    {
      memcpy(new, iov->iov_base, iov->iov_len);
      free(iov->iov_base);
    }

  iov->iov_base = new;
  iov->iov_len = size;

  return 1;
}

char *print_mac(char *buff, unsigned char *mac, int len)
{
  char *p = buff;
  int i;
   
  if (len == 0)
    sprintf(p, "<null>");
  else
    for (i = 0; i < len; i++)
      p += sprintf(p, "%.2x%s", mac[i], (i == len - 1) ? "" : ":");
  
  return buff;
}

void bump_maxfd(int fd, int *max)
{
  if (fd > *max)
    *max = fd;
}

int retry_send(void)
{
   struct timespec waiter;
   if (errno == EAGAIN || errno == EWOULDBLOCK)
     {
       waiter.tv_sec = 0;
       waiter.tv_nsec = 10000;
       nanosleep(&waiter, NULL);
       return 1;
     }
   
   if (errno == EINTR)
     return 1;

   return 0;
}

int read_write(int fd, unsigned char *packet, int size, int rw)
{
  ssize_t n, done;
  
  for (done = 0; done < size; done += n)
    {
    retry:
      if (rw)
        n = read(fd, &packet[done], (size_t)(size - done));
      else
        n = write(fd, &packet[done], (size_t)(size - done));

      if (n == 0)
        return 0;
      else if (n == -1)
        {
          if (retry_send() || errno == ENOMEM || errno == ENOBUFS)
            goto retry;
          else
            return 0;
        }
    }
  return 1;
}

/* Basically match a string value against a wildcard pattern.  */
int wildcard_match(const char* wildcard, const char* match)
{
  while (*wildcard && *match)
    {
      if (*wildcard == '*')
        return 1;

      if (*wildcard != *match)
        return 0; 

      ++wildcard;
      ++match;
    }

  return *wildcard == *match;
}
