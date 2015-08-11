/* dnssec.c is Copyright (c) 2012 Giovanni Bajo <rasky@develer.com>
           and Copyright (c) 2012-2015 Simon Kelley

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

#ifdef HAVE_DNSSEC

#include <nettle/rsa.h>
#include <nettle/dsa.h>
#ifndef NO_NETTLE_ECC
#  include <nettle/ecdsa.h>
#  include <nettle/ecc-curve.h>
#endif
#include <nettle/nettle-meta.h>
#include <nettle/bignum.h>

/* Nettle-3.0 moved to a new API for DSA. We use a name that's defined in the new API
   to detect Nettle-3, and invoke the backwards compatibility mode. */
#ifdef dsa_params_init
#include <nettle/dsa-compat.h>
#endif

#define SERIAL_UNDEF  -100
#define SERIAL_EQ        0
#define SERIAL_LT       -1
#define SERIAL_GT        1

/* http://www.iana.org/assignments/ds-rr-types/ds-rr-types.xhtml */
static char *ds_digest_name(int digest)
{
  switch (digest)
    {
    case 1: return "sha1";
    case 2: return "sha256";
    case 3: return "gosthash94";
    case 4: return "sha384";
    default: return NULL;
    }
}
 
/* http://www.iana.org/assignments/dns-sec-alg-numbers/dns-sec-alg-numbers.xhtml */
static char *algo_digest_name(int algo)
{
  switch (algo)
    {
    case 1: return "md5";
    case 3: return "sha1";
    case 5: return "sha1";
    case 6: return "sha1";
    case 7: return "sha1";
    case 8: return "sha256";
    case 10: return "sha512";
    case 12: return "gosthash94";
    case 13: return "sha256";
    case 14: return "sha384";
    default: return NULL;
    }
}
      
/* Find pointer to correct hash function in nettle library */
static const struct nettle_hash *hash_find(char *name)
{
  int i;
  
  if (!name)
    return NULL;
  
  for (i = 0; nettle_hashes[i]; i++)
    {
      if (strcmp(nettle_hashes[i]->name, name) == 0)
	return nettle_hashes[i];
    }

  return NULL;
}

/* expand ctx and digest memory allocations if necessary and init hash function */
static int hash_init(const struct nettle_hash *hash, void **ctxp, unsigned char **digestp)
{
  static void *ctx = NULL;
  static unsigned char *digest = NULL;
  static unsigned int ctx_sz = 0;
  static unsigned int digest_sz = 0;

  void *new;

  if (ctx_sz < hash->context_size)
    {
      if (!(new = whine_malloc(hash->context_size)))
	return 0;
      if (ctx)
	free(ctx);
      ctx = new;
      ctx_sz = hash->context_size;
    }
  
  if (digest_sz < hash->digest_size)
    {
      if (!(new = whine_malloc(hash->digest_size)))
	return 0;
      if (digest)
	free(digest);
      digest = new;
      digest_sz = hash->digest_size;
    }

  *ctxp = ctx;
  *digestp = digest;

  hash->init(ctx);

  return 1;
}
  
static int dnsmasq_rsa_verify(struct blockdata *key_data, unsigned int key_len, unsigned char *sig, size_t sig_len,
			      unsigned char *digest, int algo)
{
  unsigned char *p;
  size_t exp_len;
  
  static struct rsa_public_key *key = NULL;
  static mpz_t sig_mpz;
  
  if (key == NULL)
    {
      if (!(key = whine_malloc(sizeof(struct rsa_public_key))))
	return 0;
      
      nettle_rsa_public_key_init(key);
      mpz_init(sig_mpz);
    }
  
  if ((key_len < 3) || !(p = blockdata_retrieve(key_data, key_len, NULL)))
    return 0;
  
  key_len--;
  if ((exp_len = *p++) == 0)
    {
      GETSHORT(exp_len, p);
      key_len -= 2;
    }
  
  if (exp_len >= key_len)
    return 0;
  
  key->size =  key_len - exp_len;
  mpz_import(key->e, exp_len, 1, 1, 0, 0, p);
  mpz_import(key->n, key->size, 1, 1, 0, 0, p + exp_len);

  mpz_import(sig_mpz, sig_len, 1, 1, 0, 0, sig);
  
  switch (algo)
    {
    case 1:
      return nettle_rsa_md5_verify_digest(key, digest, sig_mpz);
    case 5: case 7:
      return nettle_rsa_sha1_verify_digest(key, digest, sig_mpz);
    case 8:
      return nettle_rsa_sha256_verify_digest(key, digest, sig_mpz);
    case 10:
      return nettle_rsa_sha512_verify_digest(key, digest, sig_mpz);
    }

  return 0;
}  

static int dnsmasq_dsa_verify(struct blockdata *key_data, unsigned int key_len, unsigned char *sig, size_t sig_len,
			      unsigned char *digest, int algo)
{
  unsigned char *p;
  unsigned int t;
  
  static struct dsa_public_key *key = NULL;
  static struct dsa_signature *sig_struct;
  
  if (key == NULL)
    {
      if (!(sig_struct = whine_malloc(sizeof(struct dsa_signature))) || 
	  !(key = whine_malloc(sizeof(struct dsa_public_key)))) 
	return 0;
      
      nettle_dsa_public_key_init(key);
      nettle_dsa_signature_init(sig_struct);
    }
  
  if ((sig_len < 41) || !(p = blockdata_retrieve(key_data, key_len, NULL)))
    return 0;
  
  t = *p++;
  
  if (key_len < (213 + (t * 24)))
    return 0;
  
  mpz_import(key->q, 20, 1, 1, 0, 0, p); p += 20;
  mpz_import(key->p, 64 + (t*8), 1, 1, 0, 0, p); p += 64 + (t*8);
  mpz_import(key->g, 64 + (t*8), 1, 1, 0, 0, p); p += 64 + (t*8);
  mpz_import(key->y, 64 + (t*8), 1, 1, 0, 0, p); p += 64 + (t*8);
  
  mpz_import(sig_struct->r, 20, 1, 1, 0, 0, sig+1);
  mpz_import(sig_struct->s, 20, 1, 1, 0, 0, sig+21);
  
  (void)algo;
  
  return nettle_dsa_sha1_verify_digest(key, digest, sig_struct);
} 
 
#ifndef NO_NETTLE_ECC
static int dnsmasq_ecdsa_verify(struct blockdata *key_data, unsigned int key_len, 
				unsigned char *sig, size_t sig_len,
				unsigned char *digest, size_t digest_len, int algo)
{
  unsigned char *p;
  unsigned int t;
  struct ecc_point *key;

  static struct ecc_point *key_256 = NULL, *key_384 = NULL;
  static mpz_t x, y;
  static struct dsa_signature *sig_struct;
  
  if (!sig_struct)
    {
      if (!(sig_struct = whine_malloc(sizeof(struct dsa_signature))))
	return 0;
      
      nettle_dsa_signature_init(sig_struct);
      mpz_init(x);
      mpz_init(y);
    }
  
  switch (algo)
    {
    case 13:
      if (!key_256)
	{
	  if (!(key_256 = whine_malloc(sizeof(struct ecc_point))))
	    return 0;
	  
	  nettle_ecc_point_init(key_256, &nettle_secp_256r1);
	}
      
      key = key_256;
      t = 32;
      break;
      
    case 14:
      if (!key_384)
	{
	  if (!(key_384 = whine_malloc(sizeof(struct ecc_point))))
	    return 0;
	  
	  nettle_ecc_point_init(key_384, &nettle_secp_384r1);
	}
      
      key = key_384;
      t = 48;
      break;
        
    default:
      return 0;
    }
  
  if (sig_len != 2*t || key_len != 2*t ||
      !(p = blockdata_retrieve(key_data, key_len, NULL)))
    return 0;
  
  mpz_import(x, t , 1, 1, 0, 0, p);
  mpz_import(y, t , 1, 1, 0, 0, p + t);

  if (!ecc_point_set(key, x, y))
    return 0;
  
  mpz_import(sig_struct->r, t, 1, 1, 0, 0, sig);
  mpz_import(sig_struct->s, t, 1, 1, 0, 0, sig + t);
  
  return nettle_ecdsa_verify(key, digest_len, digest, sig_struct);
} 
#endif 

static int verify(struct blockdata *key_data, unsigned int key_len, unsigned char *sig, size_t sig_len,
		  unsigned char *digest, size_t digest_len, int algo)
{
  (void)digest_len;

  switch (algo)
    {
    case 1: case 5: case 7: case 8: case 10:
      return dnsmasq_rsa_verify(key_data, key_len, sig, sig_len, digest, algo);
      
    case 3: case 6: 
      return dnsmasq_dsa_verify(key_data, key_len, sig, sig_len, digest, algo);
 
#ifndef NO_NETTLE_ECC   
    case 13: case 14:
      return dnsmasq_ecdsa_verify(key_data, key_len, sig, sig_len, digest, digest_len, algo);
#endif
    }
  
  return 0;
}

/* Convert from presentation format to wire format, in place.
   Also map UC -> LC.
   Note that using extract_name to get presentation format
   then calling to_wire() removes compression and maps case,
   thus generating names in canonical form.
   Calling to_wire followed by from_wire is almost an identity,
   except that the UC remains mapped to LC. 

   Note that both /000 and '.' are allowed within labels. These get
   represented in presentation format using NAME_ESCAPE as an escape
   character. In theory, if all the characters in a name were /000 or
   '.' or NAME_ESCAPE then all would have to be escaped, so the 
   presentation format would be twice as long as the spec (1024). 
   The buffers are all delcared as 2049 (allowing for the trailing zero) 
   for this reason.
*/
static int to_wire(char *name)
{
  unsigned char *l, *p, *q, term;
  int len;

  for (l = (unsigned char*)name; *l != 0; l = p)
    {
      for (p = l; *p != '.' && *p != 0; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p = *p - 'A' + 'a';
	else if (*p == NAME_ESCAPE)
	  {
	    for (q = p; *q; q++)
	      *q = *(q+1);
	    (*p)--;
	  }
      term = *p;
      
      if ((len = p - l) != 0)
	memmove(l+1, l, len);
      *l = len;
      
      p++;
      
      if (term == 0)
	*p = 0;
    }
  
  return l + 1 - (unsigned char *)name;
}

/* Note: no compression  allowed in input. */
static void from_wire(char *name)
{
  unsigned char *l, *p, *last;
  int len;
  
  for (last = (unsigned char *)name; *last != 0; last += *last+1);
  
  for (l = (unsigned char *)name; *l != 0; l += len+1)
    {
      len = *l;
      memmove(l, l+1, len);
      for (p = l; p < l + len; p++)
	if (*p == '.' || *p == 0 || *p == NAME_ESCAPE)
	  {
	    memmove(p+1, p, 1 + last - p);
	    len++;
	    *p++ = NAME_ESCAPE; 
	    (*p)++;
	  }
	
      l[len] = '.';
    }

  if ((char *)l != name)
    *(l-1) = 0;
}

/* Input in presentation format */
static int count_labels(char *name)
{
  int i;

  if (*name == 0)
    return 0;

  for (i = 0; *name; name++)
    if (*name == '.')
      i++;

  return i+1;
}

/* Implement RFC1982 wrapped compare for 32-bit numbers */
static int serial_compare_32(unsigned long s1, unsigned long s2)
{
  if (s1 == s2)
    return SERIAL_EQ;

  if ((s1 < s2 && (s2 - s1) < (1UL<<31)) ||
      (s1 > s2 && (s1 - s2) > (1UL<<31)))
    return SERIAL_LT;
  if ((s1 < s2 && (s2 - s1) > (1UL<<31)) ||
      (s1 > s2 && (s1 - s2) < (1UL<<31)))
    return SERIAL_GT;
  return SERIAL_UNDEF;
}

/* Called at startup. If the timestamp file is configured and exists, put its mtime on
   timestamp_time. If it doesn't exist, create it, and set the mtime to 1-1-2015.
   return -1 -> Cannot create file.
           0 -> not using timestamp, or timestamp exists and is in past.
           1 -> timestamp exists and is in future.
*/

static time_t timestamp_time;

int setup_timestamp(void)
{
  struct stat statbuf;
  
  daemon->back_to_the_future = 0;
  
  if (!daemon->timestamp_file)
    return 0;
  
  if (stat(daemon->timestamp_file, &statbuf) != -1)
    {
      timestamp_time = statbuf.st_mtime;
    check_and_exit:
      if (difftime(timestamp_time, time(0)) <=  0)
	{
	  /* time already OK, update timestamp, and do key checking from the start. */
	  if (utime(daemon->timestamp_file, NULL) == -1)
	    my_syslog(LOG_ERR, _("failed to update mtime on %s: %s"), daemon->timestamp_file, strerror(errno));
	  daemon->back_to_the_future = 1;
	  return 0;
	}
      return 1;
    }
  
  if (errno == ENOENT)
    {
      /* NB. for explanation of O_EXCL flag, see comment on pidfile in dnsmasq.c */ 
      int fd = open(daemon->timestamp_file, O_WRONLY | O_CREAT | O_NONBLOCK | O_EXCL, 0666);
      if (fd != -1)
	{
	  struct utimbuf timbuf;

	  close(fd);
	  
	  timestamp_time = timbuf.actime = timbuf.modtime = 1420070400; /* 1-1-2015 */
	  if (utime(daemon->timestamp_file, &timbuf) == 0)
	    goto check_and_exit;
	}
    }

  return -1;
}

/* Check whether today/now is between date_start and date_end */
static int check_date_range(unsigned long date_start, unsigned long date_end)
{
  unsigned long curtime = time(0);
 
  /* Checking timestamps may be temporarily disabled */
    
  /* If the current time if _before_ the timestamp
     on our persistent timestamp file, then assume the
     time if not yet correct, and don't check the
     key timestamps. As soon as the current time is
     later then the timestamp, update the timestamp
     and start checking keys */
  if (daemon->timestamp_file)
    {
      if (daemon->back_to_the_future == 0 && difftime(timestamp_time, curtime) <= 0)
	{
	  if (utime(daemon->timestamp_file, NULL) != 0)
	    my_syslog(LOG_ERR, _("failed to update mtime on %s: %s"), daemon->timestamp_file, strerror(errno));
	  
	  daemon->back_to_the_future = 1;
	  set_option_bool(OPT_DNSSEC_TIME);
	  queue_event(EVENT_RELOAD); /* purge cache */
	} 

      if (daemon->back_to_the_future == 0)
	return 1;
    }
  else if (option_bool(OPT_DNSSEC_TIME))
    return 1;
  
  /* We must explicitly check against wanted values, because of SERIAL_UNDEF */
  return serial_compare_32(curtime, date_start) == SERIAL_GT
    && serial_compare_32(curtime, date_end) == SERIAL_LT;
}

static u16 *get_desc(int type)
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

/* Return bytes of canonicalised rdata, when the return value is zero, the remaining 
   data, pointed to by *p, should be used raw. */
static int get_rdata(struct dns_header *header, size_t plen, unsigned char *end, char *buff, int bufflen,
		     unsigned char **p, u16 **desc)
{
  int d = **desc;
  
  /* No more data needs mangling */
  if (d == (u16)-1)
    {
      /* If there's more data than we have space for, just return what fits,
	 we'll get called again for more chunks */
      if (end - *p > bufflen)
	{
	  memcpy(buff, *p, bufflen);
	  *p += bufflen;
	  return bufflen;
	}
      
      return 0;
    }
 
  (*desc)++;
  
  if (d == 0 && extract_name(header, plen, p, buff, 1, 0))
    /* domain-name, canonicalise */
    return to_wire(buff);
  else
    { 
      /* plain data preceding a domain-name, don't run off the end of the data */
      if ((end - *p) < d)
	d = end - *p;
      
      if (d != 0)
	{
	  memcpy(buff, *p, d);
	  *p += d;
	}
      
      return d;
    }
}

static int expand_workspace(unsigned char ***wkspc, int *sz, int new)
{
  unsigned char **p;
  int new_sz = *sz;
  
  if (new_sz > new)
    return 1;

  if (new >= 100)
    return 0;

  new_sz += 5;
  
  if (!(p = whine_malloc((new_sz) * sizeof(unsigned char **))))
    return 0;  
  
  if (*wkspc)
    {
      memcpy(p, *wkspc, *sz * sizeof(unsigned char **));
      free(*wkspc);
    }
  
  *wkspc = p;
  *sz = new_sz;

  return 1;
}

/* Bubble sort the RRset into the canonical order. 
   Note that the byte-streams from two RRs may get unsynced: consider 
   RRs which have two domain-names at the start and then other data.
   The domain-names may have different lengths in each RR, but sort equal

   ------------
   |abcde|fghi|
   ------------
   |abcd|efghi|
   ------------

   leaving the following bytes as deciding the order. Hence the nasty left1 and left2 variables.
*/

static void sort_rrset(struct dns_header *header, size_t plen, u16 *rr_desc, int rrsetidx, 
		       unsigned char **rrset, char *buff1, char *buff2)
{
  int swap, quit, i;
  
  do
    {
      for (swap = 0, i = 0; i < rrsetidx-1; i++)
	{
	  int rdlen1, rdlen2, left1, left2, len1, len2, len, rc;
	  u16 *dp1, *dp2;
	  unsigned char *end1, *end2;
	  /* Note that these have been determined to be OK previously,
	     so we don't need to check for NULL return here. */
	  unsigned char *p1 = skip_name(rrset[i], header, plen, 10);
	  unsigned char *p2 = skip_name(rrset[i+1], header, plen, 10);
	  
	  p1 += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen1, p1);
	  end1 = p1 + rdlen1;
	  
	  p2 += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen2, p2);
	  end2 = p2 + rdlen2; 
	  
	  dp1 = dp2 = rr_desc;
	  
	  for (quit = 0, left1 = 0, left2 = 0, len1 = 0, len2 = 0; !quit;)
	    {
	      if (left1 != 0)
		memmove(buff1, buff1 + len1 - left1, left1);
	      
	      if ((len1 = get_rdata(header, plen, end1, buff1 + left1, (MAXDNAME * 2) - left1, &p1, &dp1)) == 0)
		{
		  quit = 1;
		  len1 = end1 - p1;
		  memcpy(buff1 + left1, p1, len1);
		}
	      len1 += left1;
	      
	      if (left2 != 0)
		memmove(buff2, buff2 + len2 - left2, left2);
	      
	      if ((len2 = get_rdata(header, plen, end2, buff2 + left2, (MAXDNAME *2) - left2, &p2, &dp2)) == 0)
		{
		  quit = 1;
		  len2 = end2 - p2;
		  memcpy(buff2 + left2, p2, len2);
		}
	      len2 += left2;
	       
	      if (len1 > len2)
		left1 = len1 - len2, left2 = 0, len = len2;
	      else
		left2 = len2 - len1, left1 = 0, len = len1;
	      
	      rc = (len == 0) ? 0 : memcmp(buff1, buff2, len);
	      
	      if (rc > 0 || (rc == 0 && quit && len1 > len2))
		{
		  unsigned char *tmp = rrset[i+1];
		  rrset[i+1] = rrset[i];
		  rrset[i] = tmp;
		  swap = quit = 1;
		}
	      else if (rc < 0)
		quit = 1;
	    }
	}
    } while (swap);
}

/* Validate a single RRset (class, type, name) in the supplied DNS reply 
   Return code:
   STAT_SECURE   if it validates.
   STAT_SECURE_WILDCARD if it validates and is the result of wildcard expansion.
   (In this case *wildcard_out points to the "body" of the wildcard within name.) 
   STAT_NO_SIG no RRsigs found.
   STAT_INSECURE RRset empty.
   STAT_BOGUS    signature is wrong, bad packet.
   STAT_NEED_KEY need DNSKEY to complete validation (name is returned in keyname)

   if key is non-NULL, use that key, which has the algo and tag given in the params of those names,
   otherwise find the key in the cache.

   name is unchanged on exit. keyname is used as workspace and trashed.
*/
static int validate_rrset(time_t now, struct dns_header *header, size_t plen, int class, int type, 
			  char *name, char *keyname, char **wildcard_out, struct blockdata *key, int keylen, int algo_in, int keytag_in)
{
  static unsigned char **rrset = NULL, **sigs = NULL;
  static int rrset_sz = 0, sig_sz = 0;
  
  unsigned char *p;
  int rrsetidx, sigidx, res, rdlen, j, name_labels;
  struct crec *crecp = NULL;
  int type_covered, algo, labels, orig_ttl, sig_expiration, sig_inception, key_tag;
  u16 *rr_desc = get_desc(type);
 
  if (wildcard_out)
    *wildcard_out = NULL;
  
  if (!(p = skip_questions(header, plen)))
    return STAT_BOGUS;
  
  name_labels = count_labels(name); /* For 4035 5.3.2 check */

  /* look for RRSIGs for this RRset and get pointers to each RR in the set. */
  for (rrsetidx = 0, sigidx = 0, j = ntohs(header->ancount) + ntohs(header->nscount); 
       j != 0; j--) 
    {
      unsigned char *pstart, *pdata;
      int stype, sclass;

      pstart = p;
      
      if (!(res = extract_name(header, plen, &p, name, 0, 10)))
	return STAT_BOGUS; /* bad packet */
      
      GETSHORT(stype, p);
      GETSHORT(sclass, p);
      p += 4; /* TTL */
      
      pdata = p;

      GETSHORT(rdlen, p);
      
      if (!CHECK_LEN(header, p, plen, rdlen))
	return STAT_BOGUS; 
      
      if (res == 1 && sclass == class)
	{
	  if (stype == type)
	    {
	      if (!expand_workspace(&rrset, &rrset_sz, rrsetidx))
		return STAT_BOGUS; 
	      
	      rrset[rrsetidx++] = pstart;
	    }
	  
	  if (stype == T_RRSIG)
	    {
	      if (rdlen < 18)
		return STAT_BOGUS; /* bad packet */ 
	      
	      GETSHORT(type_covered, p);
	      
	      if (type_covered == type)
		{
		  if (!expand_workspace(&sigs, &sig_sz, sigidx))
		    return STAT_BOGUS; 
		  
		  sigs[sigidx++] = pdata;
		} 
	      
	      p = pdata + 2; /* restore for ADD_RDLEN */
	    }
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return STAT_BOGUS;
    }
  
  /* RRset empty */
  if (rrsetidx == 0)
    return STAT_INSECURE; 

  /* no RRSIGs */
  if (sigidx == 0)
    return STAT_NO_SIG; 
  
  /* Sort RRset records into canonical order. 
     Note that at this point keyname and daemon->workspacename buffs are
     unused, and used as workspace by the sort. */
  sort_rrset(header, plen, rr_desc, rrsetidx, rrset, daemon->workspacename, keyname);
         
  /* Now try all the sigs to try and find one which validates */
  for (j = 0; j <sigidx; j++)
    {
      unsigned char *psav, *sig, *digest;
      int i, wire_len, sig_len;
      const struct nettle_hash *hash;
      void *ctx;
      char *name_start;
      u32 nsigttl;
      
      p = sigs[j];
      GETSHORT(rdlen, p); /* rdlen >= 18 checked previously */
      psav = p;
      
      p += 2; /* type_covered - already checked */
      algo = *p++;
      labels = *p++;
      GETLONG(orig_ttl, p);
      GETLONG(sig_expiration, p);
      GETLONG(sig_inception, p);
      GETSHORT(key_tag, p);
      
      if (!extract_name(header, plen, &p, keyname, 1, 0))
	return STAT_BOGUS;

      /* RFC 4035 5.3.1 says that the Signer's Name field MUST equal
	 the name of the zone containing the RRset. We can't tell that
	 for certain, but we can check that  the RRset name is equal to
	 or encloses the signers name, which should be enough to stop 
	 an attacker using signatures made with the key of an unrelated 
	 zone he controls. Note that the root key is always allowed. */
      if (*keyname != 0)
	{
	  int failed = 0;
	  
	  for (name_start = name; !hostname_isequal(name_start, keyname); )
	    if ((name_start = strchr(name_start, '.')))
	      name_start++; /* chop a label off and try again */
	    else
	      {
		failed = 1;
		break;
	      }

	  /* Bad sig, try another */
	  if (failed)
	    continue;
	}
      
      /* Other 5.3.1 checks */
      if (!check_date_range(sig_inception, sig_expiration) ||
	  labels > name_labels ||
	  !(hash = hash_find(algo_digest_name(algo))) ||
	  !hash_init(hash, &ctx, &digest))
	continue;
	
      /* OK, we have the signature record, see if the relevant DNSKEY is in the cache. */
      if (!key && !(crecp = cache_find_by_name(NULL, keyname, now, F_DNSKEY)))
	return STAT_NEED_KEY;
      
      sig = p;
      sig_len = rdlen - (p - psav);
              
      nsigttl = htonl(orig_ttl);
      
      hash->update(ctx, 18, psav);
      wire_len = to_wire(keyname);
      hash->update(ctx, (unsigned int)wire_len, (unsigned char*)keyname);
      from_wire(keyname);
      
      for (i = 0; i < rrsetidx; ++i)
	{
	  int seg;
	  unsigned char *end, *cp;
	  u16 len, *dp;
	  
	  p = rrset[i];
	  if (!extract_name(header, plen, &p, name, 1, 10)) 
	    return STAT_BOGUS;

	  name_start = name;
	  
	  /* if more labels than in RRsig name, hash *.<no labels in rrsig labels field>  4035 5.3.2 */
	  if (labels < name_labels)
	    {
	      int k;
	      for (k = name_labels - labels; k != 0; k--)
		{
		  while (*name_start != '.' && *name_start != 0)
		    name_start++;
		  if (k != 1 && *name_start == '.')
		    name_start++;
		}
	      
	      if (wildcard_out)
		*wildcard_out = name_start+1;

	      name_start--;
	      *name_start = '*';
	    }
	  
	  wire_len = to_wire(name_start);
	  hash->update(ctx, (unsigned int)wire_len, (unsigned char *)name_start);
	  hash->update(ctx, 4, p); /* class and type */
	  hash->update(ctx, 4, (unsigned char *)&nsigttl);
	  
	  p += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen, p);
	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; 
	  
	  end = p + rdlen;
	  
	  /* canonicalise rdata and calculate length of same, use name buffer as workspace.
	     Note that name buffer is twice MAXDNAME long in DNSSEC mode. */
	  cp = p;
	  dp = rr_desc;
	  for (len = 0; (seg = get_rdata(header, plen, end, name, MAXDNAME * 2, &cp, &dp)) != 0; len += seg);
	  len += end - cp;
	  len = htons(len);
	  hash->update(ctx, 2, (unsigned char *)&len); 
	  
	  /* Now canonicalise again and digest. */
	  cp = p;
	  dp = rr_desc;
	  while ((seg = get_rdata(header, plen, end, name, MAXDNAME * 2, &cp, &dp)))
	    hash->update(ctx, seg, (unsigned char *)name);
	  if (cp != end)
	    hash->update(ctx, end - cp, cp);
	}
     
      hash->digest(ctx, hash->digest_size, digest);
      
      /* namebuff used for workspace above, restore to leave unchanged on exit */
      p = (unsigned char*)(rrset[0]);
      extract_name(header, plen, &p, name, 1, 0);

      if (key)
	{
	  if (algo_in == algo && keytag_in == key_tag &&
	      verify(key, keylen, sig, sig_len, digest, hash->digest_size, algo))
	    return STAT_SECURE;
	}
      else
	{
	  /* iterate through all possible keys 4035 5.3.1 */
	  for (; crecp; crecp = cache_find_by_name(crecp, keyname, now, F_DNSKEY))
	    if (crecp->addr.key.algo == algo && 
		crecp->addr.key.keytag == key_tag &&
		crecp->uid == (unsigned int)class &&
		verify(crecp->addr.key.keydata, crecp->addr.key.keylen, sig, sig_len, digest, hash->digest_size, algo))
	      return (labels < name_labels) ? STAT_SECURE_WILDCARD : STAT_SECURE;
	}
    }

  return STAT_BOGUS;
}
 
/* The DNS packet is expected to contain the answer to a DNSKEY query.
   Put all DNSKEYs in the answer which are valid into the cache.
   return codes:
         STAT_SECURE   At least one valid DNSKEY found and in cache.
	 STAT_BOGUS    No DNSKEYs found, which  can be validated with DS,
	               or self-sign for DNSKEY RRset is not valid, bad packet.
	 STAT_NEED_DS  DS records to validate a key not found, name in keyname 
*/
int dnssec_validate_by_ds(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, int class)
{
  unsigned char *psave, *p = (unsigned char *)(header+1);
  struct crec *crecp, *recp1;
  int rc, j, qtype, qclass, ttl, rdlen, flags, algo, valid, keytag, type_covered;
  struct blockdata *key;
  struct all_addr a;

  if (ntohs(header->qdcount) != 1 ||
      !extract_name(header, plen, &p, name, 1, 4))
    return STAT_BOGUS;

  GETSHORT(qtype, p);
  GETSHORT(qclass, p);
  
  if (qtype != T_DNSKEY || qclass != class || ntohs(header->ancount) == 0)
    return STAT_BOGUS;

  /* See if we have cached a DS record which validates this key */
  if (!(crecp = cache_find_by_name(NULL, name, now, F_DS)))
    {
      strcpy(keyname, name);
      return STAT_NEED_DS;
    }
  
  /* If we've cached that DS provably doesn't exist, result must be INSECURE */
  if (crecp->flags & F_NEG)
    return STAT_INSECURE_DS;
  
  /* NOTE, we need to find ONE DNSKEY which matches the DS */
  for (valid = 0, j = ntohs(header->ancount); j != 0 && !valid; j--) 
    {
      /* Ensure we have type, class  TTL and length */
      if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	return STAT_BOGUS; /* bad packet */
  
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      GETLONG(ttl, p);
      GETSHORT(rdlen, p);
 
      if (!CHECK_LEN(header, p, plen, rdlen) || rdlen < 4)
	return STAT_BOGUS; /* bad packet */
      
      if (qclass != class || qtype != T_DNSKEY || rc == 2)
	{
	  p += rdlen;
	  continue;
	}
            
      psave = p;
      
      GETSHORT(flags, p);
      if (*p++ != 3)
	return STAT_BOGUS;
      algo = *p++;
      keytag = dnskey_keytag(algo, flags, p, rdlen - 4);
      key = NULL;
      
      /* key must have zone key flag set */
      if (flags & 0x100)
	key = blockdata_alloc((char*)p, rdlen - 4);
      
      p = psave;
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	{
	  if (key)
	    blockdata_free(key);
	  return STAT_BOGUS; /* bad packet */
	}

      /* No zone key flag or malloc failure */
      if (!key)
	continue;
      
      for (recp1 = crecp; recp1; recp1 = cache_find_by_name(recp1, name, now, F_DS))
	{
	  void *ctx;
	  unsigned char *digest, *ds_digest;
	  const struct nettle_hash *hash;
	  
	  if (recp1->addr.ds.algo == algo && 
	      recp1->addr.ds.keytag == keytag &&
	      recp1->uid == (unsigned int)class &&
	      (hash = hash_find(ds_digest_name(recp1->addr.ds.digest))) &&
	      hash_init(hash, &ctx, &digest))
	    
	    {
	      int wire_len = to_wire(name);
	      
	      /* Note that digest may be different between DSs, so 
		 we can't move this outside the loop. */
	      hash->update(ctx, (unsigned int)wire_len, (unsigned char *)name);
	      hash->update(ctx, (unsigned int)rdlen, psave);
	      hash->digest(ctx, hash->digest_size, digest);
	      
	      from_wire(name);
	      
	      if (recp1->addr.ds.keylen == (int)hash->digest_size &&
		  (ds_digest = blockdata_retrieve(recp1->addr.key.keydata, recp1->addr.ds.keylen, NULL)) &&
		  memcmp(ds_digest, digest, recp1->addr.ds.keylen) == 0 &&
		  validate_rrset(now, header, plen, class, T_DNSKEY, name, keyname, NULL, key, rdlen - 4, algo, keytag) == STAT_SECURE)
		{
		  valid = 1;
		  break;
		}
	    }
	}
      blockdata_free(key);
    }

  if (valid)
    {
      /* DNSKEY RRset determined to be OK, now cache it and the RRsigs that sign it. */
      cache_start_insert();
      
      p = skip_questions(header, plen);

      for (j = ntohs(header->ancount); j != 0; j--) 
	{
	  /* Ensure we have type, class  TTL and length */
	  if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	    return STAT_INSECURE; /* bad packet */
	  
	  GETSHORT(qtype, p); 
	  GETSHORT(qclass, p);
	  GETLONG(ttl, p);
	  GETSHORT(rdlen, p);
	    
	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	  
	  if (qclass == class && rc == 1)
	    {
	      psave = p;
	      
	      if (qtype == T_DNSKEY)
		{
		  if (rdlen < 4)
		    return STAT_BOGUS; /* bad packet */
		  
		  GETSHORT(flags, p);
		  if (*p++ != 3)
		    return STAT_BOGUS;
		  algo = *p++;
		  keytag = dnskey_keytag(algo, flags, p, rdlen - 4);
		  
		  /* Cache needs to known class for DNSSEC stuff */
		  a.addr.dnssec.class = class;
		  
		  if ((key = blockdata_alloc((char*)p, rdlen - 4)))
		    {
		      if (!(recp1 = cache_insert(name, &a, now, ttl, F_FORWARD | F_DNSKEY | F_DNSSECOK)))
			blockdata_free(key);
		      else
			{
			  a.addr.keytag = keytag;
			  log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DNSKEY keytag %u");
			  
			  recp1->addr.key.keylen = rdlen - 4;
			  recp1->addr.key.keydata = key;
			  recp1->addr.key.algo = algo;
			  recp1->addr.key.keytag = keytag;
			  recp1->addr.key.flags = flags;
			}
		    }
		}
	      else if (qtype == T_RRSIG)
		{
		  /* RRSIG, cache if covers DNSKEY RRset */
		  if (rdlen < 18)
		    return STAT_BOGUS; /* bad packet */
		  
		  GETSHORT(type_covered, p);
		  
		  if (type_covered == T_DNSKEY)
		    {
		      a.addr.dnssec.class = class;
		      a.addr.dnssec.type = type_covered;
		      
		      algo = *p++;
		      p += 13; /* labels, orig_ttl, expiration, inception */
		      GETSHORT(keytag, p);	
		      if ((key = blockdata_alloc((char*)psave, rdlen)))
			{
			  if (!(crecp = cache_insert(name, &a, now, ttl,  F_FORWARD | F_DNSKEY | F_DS)))
			    blockdata_free(key);
			  else
			    {
			      crecp->addr.sig.keydata = key;
			      crecp->addr.sig.keylen = rdlen;
			      crecp->addr.sig.keytag = keytag;
			      crecp->addr.sig.type_covered = type_covered;
			      crecp->addr.sig.algo = algo;
			    }
			}
		    }
		}
	      
	      p = psave;
	    }

	  if (!ADD_RDLEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	}
      
      /* commit cache insert. */
      cache_end_insert();
      return STAT_SECURE;
    }

  log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, "BOGUS DNSKEY");
  return STAT_BOGUS;
}

/* The DNS packet is expected to contain the answer to a DS query
   Put all DSs in the answer which are valid into the cache.
   return codes:
   STAT_SECURE      At least one valid DS found and in cache.
   STAT_NO_DS       It's proved there's no DS here.
   STAT_NO_NS       It's proved there's no DS _or_ NS here.
   STAT_BOGUS       no DS in reply or not signed, fails validation, bad packet.
   STAT_NEED_KEY    DNSKEY records to validate a DS not found, name in keyname
*/

int dnssec_validate_ds(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, int class)
{
  unsigned char *p = (unsigned char *)(header+1);
  int qtype, qclass, val, i, neganswer, nons;

  if (ntohs(header->qdcount) != 1 ||
      !(p = skip_name(p, header, plen, 4)))
    return STAT_BOGUS;
  
  GETSHORT(qtype, p);
  GETSHORT(qclass, p);

  if (qtype != T_DS || qclass != class)
    val = STAT_BOGUS;
  else
    val = dnssec_validate_reply(now, header, plen, name, keyname, NULL, &neganswer, &nons);
  /* Note dnssec_validate_reply() will have cached positive answers */
  
  if (val == STAT_INSECURE)
    val = STAT_BOGUS;

  p = (unsigned char *)(header+1);
  extract_name(header, plen, &p, name, 1, 4);
  p += 4; /* qtype, qclass */
  
  if (!(p = skip_section(p, ntohs(header->ancount), header, plen)))
    val = STAT_BOGUS;
   
  /* If we return STAT_NO_SIG, name contains the name of the DS query */
  if (val == STAT_NO_SIG)
    {
      *keyname = 0;
      return val;
    }  

  /* If the key needed to validate the DS is on the same domain as the DS, we'll
     loop getting nowhere. Stop that now. This can happen of the DS answer comes
     from the DS's zone, and not the parent zone. */
  if (val == STAT_BOGUS ||  (val == STAT_NEED_KEY && hostname_isequal(name, keyname)))
    {
      log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, "BOGUS DS");
      return STAT_BOGUS;
    }

  /* By here, the answer is proved secure, and a positive answer has been cached. */
  if (val == STAT_SECURE && neganswer)
    {
      int rdlen, flags = F_FORWARD | F_DS | F_NEG | F_DNSSECOK;
      unsigned long ttl, minttl = ULONG_MAX;
      struct all_addr a;

      if (RCODE(header) == NXDOMAIN)
	flags |= F_NXDOMAIN;
      
      /* We only cache validated DS records, DNSSECOK flag hijacked 
	 to store presence/absence of NS. */
      if (nons)
	flags &= ~F_DNSSECOK;
      
      for (i = ntohs(header->nscount); i != 0; i--)
	{
	  if (!(p = skip_name(p, header, plen, 0)))
	    return STAT_BOGUS;
	  
	  GETSHORT(qtype, p); 
	  GETSHORT(qclass, p);
	  GETLONG(ttl, p);
	  GETSHORT(rdlen, p);

	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	    
	  if (qclass != class || qtype != T_SOA)
	    {
	      p += rdlen;
	      continue;
	    }
           
	  if (ttl < minttl)
	    minttl = ttl;
	  
	  /* MNAME */
	  if (!(p = skip_name(p, header, plen, 0)))
	    return STAT_BOGUS;
	  /* RNAME */
	  if (!(p = skip_name(p, header, plen, 20)))
	    return STAT_BOGUS;
	  p += 16; /* SERIAL REFRESH RETRY EXPIRE */
	  
	  GETLONG(ttl, p); /* minTTL */
	  if (ttl < minttl)
	    minttl = ttl;
	  
	  break;
	}
      
      if (i != 0)
	{
	  cache_start_insert();
	  
	  a.addr.dnssec.class = class;
	  cache_insert(name, &a, now, ttl, flags);
	  
	  cache_end_insert();  
	  
	  log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, nons ? "no delegation" : "no DS");
	}

      return nons ? STAT_NO_NS : STAT_NO_DS; 
    }

  return val;
}

/* 4034 6.1 */
static int hostname_cmp(const char *a, const char *b)
{
  char *sa, *ea, *ca, *sb, *eb, *cb;
  unsigned char ac, bc;
  
  sa = ea = (char *)a + strlen(a);
  sb = eb = (char *)b + strlen(b);
 
  while (1)
    {
      while (sa != a && *(sa-1) != '.')
	sa--;
      
      while (sb != b && *(sb-1) != '.')
	sb--;

      ca = sa;
      cb = sb;

      while (1) 
	{
	  if (ca == ea)
	    {
	      if (cb == eb)
		break;
	      
	      return -1;
	    }
	  
	  if (cb == eb)
	    return 1;
	  
	  ac = (unsigned char) *ca++;
	  bc = (unsigned char) *cb++;
	  
	  if (ac >= 'A' && ac <= 'Z')
	    ac += 'a' - 'A';
	  if (bc >= 'A' && bc <= 'Z')
	    bc += 'a' - 'A';
	  
	  if (ac < bc)
	    return -1;
	  else if (ac != bc)
	    return 1;
	}

     
      if (sa == a)
	{
	  if (sb == b)
	    return 0;
	  
	  return -1;
	}
      
      if (sb == b)
	return 1;
      
      ea = sa--;
      eb = sb--;
    }
}

/* Find all the NSEC or NSEC3 records in a reply.
   return an array of pointers to them. */
static int find_nsec_records(struct dns_header *header, size_t plen, unsigned char ***nsecsetp, int *nsecsetl, int class_reqd)
{
  static unsigned char **nsecset = NULL;
  static int nsecset_sz = 0;
  
  int type_found = 0;
  unsigned char *p = skip_questions(header, plen);
  int type, class, rdlen, i, nsecs_found;

  /* Move to NS section */
  if (!p || !(p = skip_section(p, ntohs(header->ancount), header, plen)))
    return 0;
  
  for (nsecs_found = 0, i = ntohs(header->nscount); i != 0; i--)
    {
      unsigned char *pstart = p;
      
      if (!(p = skip_name(p, header, plen, 10)))
	return 0;
      
      GETSHORT(type, p); 
      GETSHORT(class, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);

      if (class == class_reqd && (type == T_NSEC || type == T_NSEC3))
	{
	  /* No mixed NSECing 'round here, thankyouverymuch */
	  if (type_found == T_NSEC && type == T_NSEC3)
	    return 0;
	  if (type_found == T_NSEC3 && type == T_NSEC)
	    return 0;

	  type_found = type;

	  if (!expand_workspace(&nsecset, &nsecset_sz, nsecs_found))
	    return 0; 
	  
	  nsecset[nsecs_found++] = pstart;
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return 0;
    }
  
  *nsecsetp = nsecset;
  *nsecsetl = nsecs_found;
  
  return type_found;
}

static int prove_non_existence_nsec(struct dns_header *header, size_t plen, unsigned char **nsecs, int nsec_count,
				    char *workspace1, char *workspace2, char *name, int type, int *nons)
{
  int i, rc, rdlen;
  unsigned char *p, *psave;
  int offset = (type & 0xff) >> 3;
  int mask = 0x80 >> (type & 0x07);

  if (nons)
    *nons = 0;
  
  /* Find NSEC record that proves name doesn't exist */
  for (i = 0; i < nsec_count; i++)
    {
      p = nsecs[i];
      if (!extract_name(header, plen, &p, workspace1, 1, 10))
	return STAT_BOGUS;
      p += 8; /* class, type, TTL */
      GETSHORT(rdlen, p);
      psave = p;
      if (!extract_name(header, plen, &p, workspace2, 1, 10))
	return STAT_BOGUS;
      
      rc = hostname_cmp(workspace1, name);
      
      if (rc == 0)
	{
	  /* 4035 para 5.4. Last sentence */
	  if (type == T_NSEC || type == T_RRSIG)
	    return STAT_SECURE;

	  /* NSEC with the same name as the RR we're testing, check
	     that the type in question doesn't appear in the type map */
	  rdlen -= p - psave;
	  /* rdlen is now length of type map, and p points to it */
	  
	  /* If we can prove that there's no NS record, return that information. */
	  if (nons && rdlen >= 2 && p[0] == 0 && (p[2] & (0x80 >> T_NS)) == 0)
	    *nons = 1;
	  
	  while (rdlen >= 2)
	    {
	      if (!CHECK_LEN(header, p, plen, rdlen))
		return STAT_BOGUS;
	      
	      if (p[0] == type >> 8)
		{
		  /* Does the NSEC say our type exists? */
		  if (offset < p[1] && (p[offset+2] & mask) != 0)
		    return STAT_BOGUS;
		  
		  break; /* finshed checking */
		}
	      
	      rdlen -= p[1];
	      p +=  p[1];
	    }
	  
	  return STAT_SECURE;
	}
      else if (rc == -1)
	{
	  /* Normal case, name falls between NSEC name and next domain name,
	     wrap around case, name falls between NSEC name (rc == -1) and end */
	  if (hostname_cmp(workspace2, name) >= 0 || hostname_cmp(workspace1, workspace2) >= 0)
	    return STAT_SECURE;
	}
      else 
	{
	  /* wrap around case, name falls between start and next domain name */
	  if (hostname_cmp(workspace1, workspace2) >= 0 && hostname_cmp(workspace2, name) >=0 )
	    return STAT_SECURE;
	}
    }
  
  return STAT_BOGUS;
}

/* return digest length, or zero on error */
static int hash_name(char *in, unsigned char **out, struct nettle_hash const *hash, 
		     unsigned char *salt, int salt_len, int iterations)
{
  void *ctx;
  unsigned char *digest;
  int i;

  if (!hash_init(hash, &ctx, &digest))
    return 0;
 
  hash->update(ctx, to_wire(in), (unsigned char *)in);
  hash->update(ctx, salt_len, salt);
  hash->digest(ctx, hash->digest_size, digest);

  for(i = 0; i < iterations; i++)
    {
      hash->update(ctx, hash->digest_size, digest);
      hash->update(ctx, salt_len, salt);
      hash->digest(ctx, hash->digest_size, digest);
    }
   
  from_wire(in);

  *out = digest;
  return hash->digest_size;
}

/* Decode base32 to first "." or end of string */
static int base32_decode(char *in, unsigned char *out)
{
  int oc, on, c, mask, i;
  unsigned char *p = out;
 
  for (c = *in, oc = 0, on = 0; c != 0 && c != '.'; c = *++in) 
    {
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'a' && c <= 'v')
	c -= 'a', c += 10;
      else if (c >= 'A' && c <= 'V')
	c -= 'A', c += 10;
      else
	return 0;
      
      for (mask = 0x10, i = 0; i < 5; i++)
        {
	  if (c & mask)
	    oc |= 1;
	  mask = mask >> 1;
	  if (((++on) & 7) == 0)
	    *p++ = oc;
	  oc = oc << 1;
	}
    }
  
  if ((on & 7) != 0)
    return 0;

  return p - out;
}

static int check_nsec3_coverage(struct dns_header *header, size_t plen, int digest_len, unsigned char *digest, int type,
				char *workspace1, char *workspace2, unsigned char **nsecs, int nsec_count, int *nons)
{
  int i, hash_len, salt_len, base32_len, rdlen;
  unsigned char *p, *psave;

  for (i = 0; i < nsec_count; i++)
    if ((p = nsecs[i]))
      {
       	if (!extract_name(header, plen, &p, workspace1, 1, 0) ||
	    !(base32_len = base32_decode(workspace1, (unsigned char *)workspace2)))
	  return 0;
	
	p += 8; /* class, type, TTL */
	GETSHORT(rdlen, p);
	psave = p;
	p += 4; /* algo, flags, iterations */
	salt_len = *p++; /* salt_len */
	p += salt_len; /* salt */
	hash_len = *p++; /* p now points to next hashed name */
	
	if (!CHECK_LEN(header, p, plen, hash_len))
	  return 0;
	
	if (digest_len == base32_len && hash_len == base32_len)
	  {
	    int rc = memcmp(workspace2, digest, digest_len);

	    if (rc == 0)
	      {
		/* We found an NSEC3 whose hashed name exactly matches the query, so
		   we just need to check the type map. p points to the RR data for the record. */
		
		int offset = (type & 0xff) >> 3;
		int mask = 0x80 >> (type & 0x07);
		
		p += hash_len; /* skip next-domain hash */
		rdlen -= p - psave;

		if (!CHECK_LEN(header, p, plen, rdlen))
		  return 0;
		
		/* If we can prove that there's no NS record, return that information. */
		if (nons && rdlen >= 2 && p[0] == 0 && (p[2] & (0x80 >> T_NS)) == 0)
		  *nons = 1;
		
		while (rdlen >= 2)
		  {
		    if (p[0] == type >> 8)
		      {
			/* Does the NSEC3 say our type exists? */
			if (offset < p[1] && (p[offset+2] & mask) != 0)
			  return STAT_BOGUS;
			
			break; /* finshed checking */
		      }
		    
		    rdlen -= p[1];
		    p +=  p[1];
		  }

		return 1;
	      }
	    else if (rc < 0)
	      {
		/* Normal case, hash falls between NSEC3 name-hash and next domain name-hash,
		   wrap around case, name-hash falls between NSEC3 name-hash and end */
		if (memcmp(p, digest, digest_len) >= 0 || memcmp(workspace2, p, digest_len) >= 0)
		  return 1;
	      }
	    else 
	      {
		/* wrap around case, name falls between start and next domain name */
		if (memcmp(workspace2, p, digest_len) >= 0 && memcmp(p, digest, digest_len) >= 0)
		  return 1;
	      }
	  }
      }
  return 0;
}

static int prove_non_existence_nsec3(struct dns_header *header, size_t plen, unsigned char **nsecs, int nsec_count,
				     char *workspace1, char *workspace2, char *name, int type, char *wildname, int *nons)
{
  unsigned char *salt, *p, *digest;
  int digest_len, i, iterations, salt_len, base32_len, algo = 0;
  struct nettle_hash const *hash;
  char *closest_encloser, *next_closest, *wildcard;
  
  if (nons)
    *nons = 0;
  
  /* Look though the NSEC3 records to find the first one with 
     an algorithm we support (currently only algo == 1).

     Take the algo, iterations, and salt of that record
     as the ones we're going to use, and prune any 
     that don't match. */
  
  for (i = 0; i < nsec_count; i++)
    {
      if (!(p = skip_name(nsecs[i], header, plen, 15)))
	return STAT_BOGUS; /* bad packet */
      
      p += 10; /* type, class, TTL, rdlen */
      algo = *p++;
      
      if (algo == 1)
	break; /* known algo */
    }

  /* No usable NSEC3s */
  if (i == nsec_count)
    return STAT_BOGUS;

  p++; /* flags */
  GETSHORT (iterations, p);
  salt_len = *p++;
  salt = p;
  if (!CHECK_LEN(header, salt, plen, salt_len))
    return STAT_BOGUS; /* bad packet */
    
  /* Now prune so we only have NSEC3 records with same iterations, salt and algo */
  for (i = 0; i < nsec_count; i++)
    {
      unsigned char *nsec3p = nsecs[i];
      int this_iter;

      nsecs[i] = NULL; /* Speculative, will be restored if OK. */
      
      if (!(p = skip_name(nsec3p, header, plen, 15)))
	return STAT_BOGUS; /* bad packet */
      
      p += 10; /* type, class, TTL, rdlen */
      
      if (*p++ != algo)
	continue;
 
      p++; /* flags */
      
      GETSHORT(this_iter, p);
      if (this_iter != iterations)
	continue;

      if (salt_len != *p++)
	continue;
      
      if (!CHECK_LEN(header, p, plen, salt_len))
	return STAT_BOGUS; /* bad packet */

      if (memcmp(p, salt, salt_len) != 0)
	continue;

      /* All match, put the pointer back */
      nsecs[i] = nsec3p;
    }

  /* Algo is checked as 1 above */
  if (!(hash = hash_find("sha1")))
    return STAT_BOGUS;

  if ((digest_len = hash_name(name, &digest, hash, salt, salt_len, iterations)) == 0)
    return STAT_BOGUS;
  
  if (check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, nons))
    return STAT_SECURE;

  /* Can't find an NSEC3 which covers the name directly, we need the "closest encloser NSEC3" 
     or an answer inferred from a wildcard record. */
  closest_encloser = name;
  next_closest = NULL;

  do
    {
      if (*closest_encloser == '.')
	closest_encloser++;

      if (wildname && hostname_isequal(closest_encloser, wildname))
	break;

      if ((digest_len = hash_name(closest_encloser, &digest, hash, salt, salt_len, iterations)) == 0)
	return STAT_BOGUS;
      
      for (i = 0; i < nsec_count; i++)
	if ((p = nsecs[i]))
	  {
	    if (!extract_name(header, plen, &p, workspace1, 1, 0) ||
		!(base32_len = base32_decode(workspace1, (unsigned char *)workspace2)))
	      return STAT_BOGUS;
	  
	    if (digest_len == base32_len &&
		memcmp(digest, workspace2, digest_len) == 0)
	      break; /* Gotit */
	  }
      
      if (i != nsec_count)
	break;
      
      next_closest = closest_encloser;
    }
  while ((closest_encloser = strchr(closest_encloser, '.')));
  
  if (!closest_encloser)
    return STAT_BOGUS;
  
  /* Look for NSEC3 that proves the non-existence of the next-closest encloser */
  if ((digest_len = hash_name(next_closest, &digest, hash, salt, salt_len, iterations)) == 0)
    return STAT_BOGUS;

  if (!check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, NULL))
    return STAT_BOGUS;
  
  /* Finally, check that there's no seat of wildcard synthesis */
  if (!wildname)
    {
      if (!(wildcard = strchr(next_closest, '.')) || wildcard == next_closest)
	return STAT_BOGUS;
      
      wildcard--;
      *wildcard = '*';
      
      if ((digest_len = hash_name(wildcard, &digest, hash, salt, salt_len, iterations)) == 0)
	return STAT_BOGUS;
      
      if (!check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, NULL))
	return STAT_BOGUS;
    }
  
  return STAT_SECURE;
}
    
/* Validate all the RRsets in the answer and authority sections of the reply (4035:3.2.3) */
/* Returns are the same as validate_rrset, plus the class if the missing key is in *class */
int dnssec_validate_reply(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, 
			  int *class, int *neganswer, int *nons)
{
  unsigned char *ans_start, *qname, *p1, *p2, **nsecs;
  int type1, class1, rdlen1, type2, class2, rdlen2, qclass, qtype;
  int i, j, rc, nsec_count, cname_count = CNAME_CHAIN;
  int nsec_type = 0, have_answer = 0;

  if (neganswer)
    *neganswer = 0;
  
  if (RCODE(header) == SERVFAIL || ntohs(header->qdcount) != 1)
    return STAT_BOGUS;
  
  if (RCODE(header) != NXDOMAIN && RCODE(header) != NOERROR)
    return STAT_INSECURE;

  qname = p1 = (unsigned char *)(header+1);
  
  if (!extract_name(header, plen, &p1, name, 1, 4))
    return STAT_BOGUS;

  GETSHORT(qtype, p1);
  GETSHORT(qclass, p1);
  ans_start = p1;

  if (qtype == T_ANY)
    have_answer = 1;
 
  /* Can't validate an RRISG query */
  if (qtype == T_RRSIG)
    return STAT_INSECURE;
 
 cname_loop:
  for (j = ntohs(header->ancount); j != 0; j--) 
    {
      /* leave pointer to missing name in qname */
           
      if (!(rc = extract_name(header, plen, &p1, name, 0, 10)))
	return STAT_BOGUS; /* bad packet */
      
      GETSHORT(type2, p1); 
      GETSHORT(class2, p1);
      p1 += 4; /* TTL */
      GETSHORT(rdlen2, p1);

      if (rc == 1 && qclass == class2)
	{
	  /* Do we have an answer for the question? */
	  if (type2 == qtype)
	    {
	      have_answer = 1;
	      break;
	    }
	  else if (type2 == T_CNAME)
	    {
	      qname = p1;
	      
	      /* looped CNAMES */
	      if (!cname_count-- || !extract_name(header, plen, &p1, name, 1, 0))
		return STAT_BOGUS;
	       
	      p1 = ans_start;
	      goto cname_loop;
	    }
	} 

      if (!ADD_RDLEN(header, p1, plen, rdlen2))
	return STAT_BOGUS;
    }
   
  if (neganswer && !have_answer)
    *neganswer = 1;
  
  /* No data, therefore no sigs */
  if (ntohs(header->ancount) + ntohs(header->nscount) == 0)
    {
      *keyname = 0;
      return STAT_NO_SIG;
    }

  for (p1 = ans_start, i = 0; i < ntohs(header->ancount) + ntohs(header->nscount); i++)
    {
      if (!extract_name(header, plen, &p1, name, 1, 10))
	return STAT_BOGUS; /* bad packet */
      
      GETSHORT(type1, p1);
      GETSHORT(class1, p1);
      p1 += 4; /* TTL */
      GETSHORT(rdlen1, p1);
      
      /* Don't try and validate RRSIGs! */
      if (type1 != T_RRSIG)
	{
	  /* Check if we've done this RRset already */
	  for (p2 = ans_start, j = 0; j < i; j++)
	    {
	      if (!(rc = extract_name(header, plen, &p2, name, 0, 10)))
		return STAT_BOGUS; /* bad packet */
	      
	      GETSHORT(type2, p2);
	      GETSHORT(class2, p2);
	      p2 += 4; /* TTL */
	      GETSHORT(rdlen2, p2);
	      
	      if (type2 == type1 && class2 == class1 && rc == 1)
		break; /* Done it before: name, type, class all match. */
	      
	      if (!ADD_RDLEN(header, p2, plen, rdlen2))
		return STAT_BOGUS;
	    }
	  
	  /* Not done, validate now */
	  if (j == i)
	    {
	      int ttl, keytag, algo, digest, type_covered;
	      unsigned char *psave;
	      struct all_addr a;
	      struct blockdata *key;
	      struct crec *crecp;
	      char *wildname;
	      int have_wildcard = 0;

	      rc = validate_rrset(now, header, plen, class1, type1, name, keyname, &wildname, NULL, 0, 0, 0);
	      
	      if (rc == STAT_SECURE_WILDCARD)
		{
		  have_wildcard = 1;

		  /* An attacker replay a wildcard answer with a different
		     answer and overlay a genuine RR. To prove this
		     hasn't happened, the answer must prove that
		     the gennuine record doesn't exist. Check that here. */
		  if (!nsec_type && !(nsec_type = find_nsec_records(header, plen, &nsecs, &nsec_count, class1)))
		    return STAT_BOGUS; /* No NSECs or bad packet */
		  
		  if (nsec_type == T_NSEC)
		    rc = prove_non_existence_nsec(header, plen, nsecs, nsec_count, daemon->workspacename, keyname, name, type1, NULL);
		  else
		    rc = prove_non_existence_nsec3(header, plen, nsecs, nsec_count, daemon->workspacename, 
						   keyname, name, type1, wildname, NULL);
		  
		  if (rc != STAT_SECURE)
		    return rc;
		} 
	      else if (rc != STAT_SECURE)
		{
		  if (class)
		    *class = class1; /* Class for DS or DNSKEY */

		  if (rc == STAT_NO_SIG)
		    {
		      /* If we dropped off the end of a CNAME chain, return
			 STAT_NO_SIG and the last name is keyname. This is used for proving non-existence
			 if DS records in CNAME chains. */
		      if (cname_count == CNAME_CHAIN || i < ntohs(header->ancount)) 
			/* No CNAME chain, or no sig in answer section, return empty name. */
			*keyname = 0;
		      else if (!extract_name(header, plen, &qname, keyname, 1, 0))
			return STAT_BOGUS;
		    }
 
		  return rc;
		}
	      
	      /* Cache RRsigs in answer section, and if we just validated a DS RRset, cache it */
	      cache_start_insert();
	      
	      for (p2 = ans_start, j = 0; j < ntohs(header->ancount); j++)
		{
		  if (!(rc = extract_name(header, plen, &p2, name, 0, 10)))
		    return STAT_BOGUS; /* bad packet */
		      
		  GETSHORT(type2, p2);
		  GETSHORT(class2, p2);
		  GETLONG(ttl, p2);
		  GETSHORT(rdlen2, p2);
		       
		  if (!CHECK_LEN(header, p2, plen, rdlen2))
		    return STAT_BOGUS; /* bad packet */
		  
		  if (class2 == class1 && rc == 1)
		    { 
		      psave = p2;

		      if (type1 == T_DS && type2 == T_DS)
			{
			  if (rdlen2 < 4)
			    return STAT_BOGUS; /* bad packet */
			  
			  GETSHORT(keytag, p2);
			  algo = *p2++;
			  digest = *p2++;
			  
			  /* Cache needs to known class for DNSSEC stuff */
			  a.addr.dnssec.class = class2;
			  
			  if ((key = blockdata_alloc((char*)p2, rdlen2 - 4)))
			    {
			      if (!(crecp = cache_insert(name, &a, now, ttl, F_FORWARD | F_DS | F_DNSSECOK)))
				blockdata_free(key);
			      else
				{
				  a.addr.keytag = keytag;
				  log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DS keytag %u");
				  crecp->addr.ds.digest = digest;
				  crecp->addr.ds.keydata = key;
				  crecp->addr.ds.algo = algo;
				  crecp->addr.ds.keytag = keytag;
				  crecp->addr.ds.keylen = rdlen2 - 4; 
				} 
			    }
			}
		      else if (type2 == T_RRSIG)
			{
			  if (rdlen2 < 18)
			    return STAT_BOGUS; /* bad packet */
			  
			  GETSHORT(type_covered, p2);

			  if (type_covered == type1 && 
			      (type_covered == T_A || type_covered == T_AAAA ||
			       type_covered == T_CNAME || type_covered == T_DS || 
			       type_covered == T_DNSKEY || type_covered == T_PTR)) 
			    {
			      a.addr.dnssec.type = type_covered;
			      a.addr.dnssec.class = class1;
			      
			      algo = *p2++;
			      p2 += 13; /* labels, orig_ttl, expiration, inception */
			      GETSHORT(keytag, p2);
			      
			      /* We don't cache sigs for wildcard answers, because to reproduce the
				 answer from the cache will require one or more NSEC/NSEC3 records 
				 which we don't cache. The lack of the RRSIG ensures that a query for
				 this RRset asking for a secure answer will always be forwarded. */
			      if (!have_wildcard && (key = blockdata_alloc((char*)psave, rdlen2)))
				{
				  if (!(crecp = cache_insert(name, &a, now, ttl,  F_FORWARD | F_DNSKEY | F_DS)))
				    blockdata_free(key);
				  else
				    {
				      crecp->addr.sig.keydata = key;
				      crecp->addr.sig.keylen = rdlen2;
				      crecp->addr.sig.keytag = keytag;
				      crecp->addr.sig.type_covered = type_covered;
				      crecp->addr.sig.algo = algo;
				    }
				}
			    }
			}
		      
		      p2 = psave;
		    }
		  
		  if (!ADD_RDLEN(header, p2, plen, rdlen2))
		    return STAT_BOGUS; /* bad packet */
		}
		  
	      cache_end_insert();
	    }
	}

      if (!ADD_RDLEN(header, p1, plen, rdlen1))
	return STAT_BOGUS;
    }

  /* OK, all the RRsets validate, now see if we have a NODATA or NXDOMAIN reply */
  if (have_answer)
    return STAT_SECURE;
     
  /* NXDOMAIN or NODATA reply, prove that (name, class1, type1) can't exist */
  /* First marshall the NSEC records, if we've not done it previously */
  if (!nsec_type && !(nsec_type = find_nsec_records(header, plen, &nsecs, &nsec_count, qclass)))
    {
      /* No NSEC records. If we dropped off the end of a CNAME chain, return
	 STAT_NO_SIG and the last name is keyname. This is used for proving non-existence
	 if DS records in CNAME chains. */
      if (cname_count == CNAME_CHAIN) /* No CNAME chain, return empty name. */
	*keyname = 0;
      else if (!extract_name(header, plen, &qname, keyname, 1, 0))
	return STAT_BOGUS;
      return STAT_NO_SIG; /* No NSECs, this is probably a dangling CNAME pointing into
			     an unsigned zone. Return STAT_NO_SIG to cause this to be proved. */
    }
   
  /* Get name of missing answer */
  if (!extract_name(header, plen, &qname, name, 1, 0))
    return STAT_BOGUS;
  
  if (nsec_type == T_NSEC)
    return prove_non_existence_nsec(header, plen, nsecs, nsec_count, daemon->workspacename, keyname, name, qtype, nons);
  else
    return prove_non_existence_nsec3(header, plen, nsecs, nsec_count, daemon->workspacename, keyname, name, qtype, NULL, nons);
}

/* Chase the CNAME chain in the packet until the first record which _doesn't validate.
   Needed for proving answer in unsigned space.
   Return STAT_NEED_* 
          STAT_BOGUS - error
          STAT_INSECURE - name of first non-secure record in name 
*/
int dnssec_chase_cname(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname)
{
  unsigned char *p = (unsigned char *)(header+1);
  int type, class, qclass, rdlen, j, rc;
  int cname_count = CNAME_CHAIN;
  char *wildname;

  /* Get question */
  if (!extract_name(header, plen, &p, name, 1, 4))
    return STAT_BOGUS;
  
  p +=2; /* type */
  GETSHORT(qclass, p);

  while (1)
    {
      for (j = ntohs(header->ancount); j != 0; j--) 
	{
	  if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	    return STAT_BOGUS; /* bad packet */
	  
	  GETSHORT(type, p); 
	  GETSHORT(class, p);
	  p += 4; /* TTL */
	  GETSHORT(rdlen, p);

	  /* Not target, loop */
	  if (rc == 2 || qclass != class)
	    {
	      if (!ADD_RDLEN(header, p, plen, rdlen))
		return STAT_BOGUS;
	      continue;
	    }
	  
	  /* Got to end of CNAME chain. */
	  if (type != T_CNAME)
	    return STAT_INSECURE;
	  
	  /* validate CNAME chain, return if insecure or need more data */
	  rc = validate_rrset(now, header, plen, class, type, name, keyname, &wildname, NULL, 0, 0, 0);
	   
	  if (rc == STAT_SECURE_WILDCARD)
	    {
	      int nsec_type, nsec_count, i;
	      unsigned char **nsecs;

	      /* An attacker can replay a wildcard answer with a different
		 answer and overlay a genuine RR. To prove this
		 hasn't happened, the answer must prove that
		 the genuine record doesn't exist. Check that here. */
	      if (!(nsec_type = find_nsec_records(header, plen, &nsecs, &nsec_count, class)))
		return STAT_BOGUS; /* No NSECs or bad packet */
	      
	      /* Note that we're called here because something didn't validate in validate_reply,
		 so we can't assume that any NSEC records have been validated. We do them by steam here */

	      for (i = 0; i < nsec_count; i++)
		{
		  unsigned char *p1 = nsecs[i];
		  
		  if (!extract_name(header, plen, &p1, daemon->workspacename, 1, 0))
		    return STAT_BOGUS;

		  rc = validate_rrset(now, header, plen, class, nsec_type, daemon->workspacename, keyname, NULL, NULL, 0, 0, 0);

		  /* NSECs can't be wildcards. */
		  if (rc == STAT_SECURE_WILDCARD)
		    rc = STAT_BOGUS;

		  if (rc != STAT_SECURE)
		    return rc;
		}

	      if (nsec_type == T_NSEC)
		rc = prove_non_existence_nsec(header, plen, nsecs, nsec_count, daemon->workspacename, keyname, name, type, NULL);
	      else
		rc = prove_non_existence_nsec3(header, plen, nsecs, nsec_count, daemon->workspacename, 
					       keyname, name, type, wildname, NULL);
	      
	      if (rc != STAT_SECURE)
		return rc;
	    }
	  
	  if (rc != STAT_SECURE)
	    {
	      if (rc == STAT_NO_SIG)
		rc = STAT_INSECURE;
	      return rc;
	    }

	  /* Loop down CNAME chain/ */
	  if (!cname_count-- || 
	      !extract_name(header, plen, &p, name, 1, 0) ||
	      !(p = skip_questions(header, plen)))
	    return STAT_BOGUS;
	  
	  break;
	}

      /* End of CNAME chain */
      return STAT_INSECURE;	
    }
}


/* Compute keytag (checksum to quickly index a key). See RFC4034 */
int dnskey_keytag(int alg, int flags, unsigned char *key, int keylen)
{
  if (alg == 1)
    {
      /* Algorithm 1 (RSAMD5) has a different (older) keytag calculation algorithm.
         See RFC4034, Appendix B.1 */
      return key[keylen-4] * 256 + key[keylen-3];
    }
  else
    {
      unsigned long ac = flags + 0x300 + alg;
      int i;

      for (i = 0; i < keylen; ++i)
        ac += (i & 1) ? key[i] : key[i] << 8;

      ac += (ac >> 16) & 0xffff;
      return ac & 0xffff;
    }
}

size_t dnssec_generate_query(struct dns_header *header, char *end, char *name, int class, 
			     int type, union mysockaddr *addr, int edns_pktsz)
{
  unsigned char *p;
  char *types = querystr("dnssec-query", type);
  size_t ret;

  if (addr->sa.sa_family == AF_INET) 
    log_query(F_NOEXTRA | F_DNSSEC | F_IPV4, name, (struct all_addr *)&addr->in.sin_addr, types);
#ifdef HAVE_IPV6
  else
    log_query(F_NOEXTRA | F_DNSSEC | F_IPV6, name, (struct all_addr *)&addr->in6.sin6_addr, types);
#endif
  
  header->qdcount = htons(1);
  header->ancount = htons(0);
  header->nscount = htons(0);
  header->arcount = htons(0);

  header->hb3 = HB3_RD; 
  SET_OPCODE(header, QUERY);
  /* For debugging, set Checking Disabled, otherwise, have the upstream check too,
     this allows it to select auth servers when one is returning bad data. */
  header->hb4 = option_bool(OPT_DNSSEC_DEBUG) ? HB4_CD : 0;

  /* ID filled in later */

  p = (unsigned char *)(header+1);
	
  p = do_rfc1035_name(p, name);
  *p++ = 0;
  PUTSHORT(type, p);
  PUTSHORT(class, p);

  ret = add_do_bit(header, p - (unsigned char *)header, end);

  if (find_pseudoheader(header, ret, NULL, &p, NULL))
    PUTSHORT(edns_pktsz, p);

  return ret;
}

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
  int i, type, class, rdlen;
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

      if (type != T_NSEC && type != T_NSEC3 && type != T_RRSIG)
	{
	  /* fixup name of RR */
	  if (!check_name(&pp, header, plen, fixup, rrs, rr_count))
	    return 0;
	  
	  if (class == C_IN)
	    {
	      u16 *d;
 
	      for (pp = p, d = get_desc(type); *d != (u16)-1; d++)
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
	

size_t filter_rrsigs(struct dns_header *header, size_t plen)
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
     records added for DNSSEC, unless explicity queried for */
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
      
      if ((type == T_NSEC || type == T_NSEC3 || type == T_RRSIG) && 
	  (type != qtype || class != qclass))
	{
	  if (!expand_workspace(&rrs, &rr_sz, rr_found + 1))
	    return plen; 
	  
	  rrs[rr_found++] = pstart;

	  if (!ADD_RDLEN(header, p, plen, rdlen))
	    return plen;
	  
	  rrs[rr_found++] = p;
	  
	  if (i < ntohs(header->ancount))
	    chop_an++;
	  else if (i < (ntohs(header->nscount) + ntohs(header->ancount)))
	    chop_ns++;
	  else
	    chop_ar++;
	}
      else if (!ADD_RDLEN(header, p, plen, rdlen))
	return plen;
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
  
  /* Third pass, elide records */
  for (p = rrs[0], i = 1; i < rr_found; i += 2)
    {
      unsigned char *start = rrs[i];
      unsigned char *end = (i != rr_found - 1) ? rrs[i+1] : ((unsigned char *)(header+1)) + plen;
      
      memmove(p, start, end-start);
      p += end-start;
    }
     
  plen = p - (unsigned char *)header;
  header->ancount = htons(ntohs(header->ancount) - chop_an);
  header->nscount = htons(ntohs(header->nscount) - chop_ns);
  header->arcount = htons(ntohs(header->arcount) - chop_ar);

  /* Fourth pass, fix up pointers in the remaining records */
  p = (unsigned char *)(header+1);
  
  check_name(&p, header, plen, 1, rrs, rr_found);
  p += 4; /* qclass, qtype */
  
  check_rrs(p, header, plen, 1, rrs, rr_found);
  
  return plen;
}

unsigned char* hash_questions(struct dns_header *header, size_t plen, char *name)
{
  int q;
  unsigned int len;
  unsigned char *p = (unsigned char *)(header+1);
  const struct nettle_hash *hash;
  void *ctx;
  unsigned char *digest;
  
  if (!(hash = hash_find("sha1")) || !hash_init(hash, &ctx, &digest))
    return NULL;
  
  for (q = ntohs(header->qdcount); q != 0; q--) 
    {
      if (!extract_name(header, plen, &p, name, 1, 4))
	break; /* bad packet */
      
      len = to_wire(name);
      hash->update(ctx, len, (unsigned char *)name);
      /* CRC the class and type as well */
      hash->update(ctx, 4, p);

      p += 4;
      if (!CHECK_LEN(header, p, plen, 0))
	break; /* bad packet */
    }
  
  hash->digest(ctx, hash->digest_size, digest);
  return digest;
}

#endif /* HAVE_DNSSEC */
