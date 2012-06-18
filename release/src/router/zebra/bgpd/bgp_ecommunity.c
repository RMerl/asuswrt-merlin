/* BGP Extended Communities Attribute
   Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "hash.h"
#include "memory.h"
#include "prefix.h"
#include "command.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_ecommunity.h"

/* Hash of community attribute. */
struct hash *ecomhash;

/* Allocate a new ecommunities.  */
struct ecommunity *
ecommunity_new ()
{
  return (struct ecommunity *) XCALLOC (MTYPE_ECOMMUNITY,
					sizeof (struct ecommunity));
}

/* Allocate ecommunities.  */
void
ecommunity_free (struct ecommunity *ecom)
{
  if (ecom->val)
    XFREE (MTYPE_ECOMMUNITY_VAL, ecom->val);
  if (ecom->str)
    XFREE (MTYPE_ECOMMUNITY_STR, ecom->str);
  XFREE (MTYPE_ECOMMUNITY, ecom);
}

/* Add a new Extended Communities value to Extended Communities
   Attribute structure.  When the value is already exists in the
   structure, we don't add the value.  Newly added value is sorted by
   numerical order.  When the value is added to the structure return 1
   else return 0.  */
static int
ecommunity_add_val (struct ecommunity *ecom, struct ecommunity_val *eval)
{
  u_char *p;
  int ret;
  int c;

  /* When this is fist value, just add it.  */
  if (ecom->val == NULL)
    {
      ecom->size++;
      ecom->val = XMALLOC (MTYPE_ECOMMUNITY_VAL, ecom_length (ecom));
      memcpy (ecom->val, eval->val, ECOMMUNITY_SIZE);
      return 1;
    }

  /* If the value already exists in the structure return 0.  */
  c = 0;
  for (p = ecom->val; c < ecom->size; p += ECOMMUNITY_SIZE, c++)
    {
      ret = memcmp (p, eval->val, ECOMMUNITY_SIZE);
      if (ret == 0)
        return 0;
      if (ret > 0)
        break;
    }

  /* Add the value to the structure with numerical sorting.  */
  ecom->size++;
  ecom->val = XREALLOC (MTYPE_ECOMMUNITY_VAL, ecom->val, ecom_length (ecom));

  memmove (ecom->val + (c + 1) * ECOMMUNITY_SIZE,
	   ecom->val + c * ECOMMUNITY_SIZE,
	   (ecom->size - 1 - c) * ECOMMUNITY_SIZE);
  memcpy (ecom->val + c * ECOMMUNITY_SIZE, eval->val, ECOMMUNITY_SIZE);

  return 1;
}

/* This function takes pointer to Extended Communites strucutre then
   create a new Extended Communities structure by uniq and sort each
   Exteneded Communities value.  */
struct ecommunity *
ecommunity_uniq_sort (struct ecommunity *ecom)
{
  int i;
  struct ecommunity *new;
  struct ecommunity_val *eval;
  
  if (! ecom)
    return NULL;
  
  new = ecommunity_new ();;
  
  for (i = 0; i < ecom->size; i++)
    {
      eval = (struct ecommunity_val *) (ecom->val + (i * ECOMMUNITY_SIZE));
      ecommunity_add_val (new, eval);
    }
  return new;
}

/* Parse Extended Communites Attribute in BGP packet.  */
struct ecommunity *
ecommunity_parse (char *pnt, u_short length)
{
  struct ecommunity tmp;
  struct ecommunity *new;

  /* Length check.  */
  if (length % ECOMMUNITY_SIZE)
    return NULL;

  /* Prepare tmporary structure for making a new Extended Communities
     Attribute.  */
  tmp.size = length / ECOMMUNITY_SIZE;
  tmp.val = pnt;

  /* Create a new Extended Communities Attribute by uniq and sort each
     Extended Communities value  */
  new = ecommunity_uniq_sort (&tmp);

  return ecommunity_intern (new);
}

/* Duplicate the Extended Communities Attribute structure.  */
struct ecommunity *
ecommunity_dup (struct ecommunity *ecom)
{
  struct ecommunity *new;

  new = XCALLOC (MTYPE_ECOMMUNITY, sizeof (struct ecommunity));
  new->size = ecom->size;
  if (new->size)
    {
      new->val = XMALLOC (MTYPE_ECOMMUNITY_VAL, ecom->size * ECOMMUNITY_SIZE);
      memcpy (new->val, ecom->val, ecom->size * ECOMMUNITY_SIZE);
    }
  else
    new->val = NULL;
  return new;
}

/* Retrun string representation of communities attribute. */
char *
ecommunity_str (struct ecommunity *ecom)
{
  if (! ecom->str)
    ecom->str = ecommunity_ecom2str (ecom, ECOMMUNITY_FORMAT_DISPLAY);
  return ecom->str;
}

/* Merge two Extended Communities Attribute structure.  */
struct ecommunity *
ecommunity_merge (struct ecommunity *ecom1, struct ecommunity *ecom2)
{
  if (ecom1->val)
    ecom1->val = XREALLOC (MTYPE_ECOMMUNITY_VAL, ecom1->val, 
			   (ecom1->size + ecom2->size) * ECOMMUNITY_SIZE);
  else
    ecom1->val = XMALLOC (MTYPE_ECOMMUNITY_VAL,
			  (ecom1->size + ecom2->size) * ECOMMUNITY_SIZE);

  memcpy (ecom1->val + (ecom1->size * ECOMMUNITY_SIZE),
	  ecom2->val, ecom2->size * ECOMMUNITY_SIZE);
  ecom1->size += ecom2->size;

  return ecom1;
}

/* Intern Extended Communities Attribute.  */
struct ecommunity *
ecommunity_intern (struct ecommunity *ecom)
{
  struct ecommunity *find;

  assert (ecom->refcnt == 0);

  find = (struct ecommunity *) hash_get (ecomhash, ecom, hash_alloc_intern);

  if (find != ecom)
    ecommunity_free (ecom);

  find->refcnt++;

  if (! find->str)
    find->str = ecommunity_ecom2str (find, ECOMMUNITY_FORMAT_DISPLAY);

  return find;
}

/* Unintern Extended Communities Attribute.  */
void
ecommunity_unintern (struct ecommunity *ecom)
{
  struct ecommunity *ret;

  if (ecom->refcnt)
    ecom->refcnt--;

  /* Pull off from hash.  */
  if (ecom->refcnt == 0)
    {
      /* Extended community must be in the hash.  */
      ret = (struct ecommunity *) hash_release (ecomhash, ecom);
      assert (ret != NULL);

      ecommunity_free (ecom);
    }
}

/* Utinity function to make hash key.  */
unsigned int
ecommunity_hash_make (struct ecommunity *ecom)
{
  int c;
  unsigned int key;
  unsigned char *pnt;

  key = 0;
  pnt = ecom->val;
  
  for (c = 0; c < ecom->size * ECOMMUNITY_SIZE; c++)
    key += pnt[c];

  return key;
}

/* Compare two Extended Communities Attribute structure.  */
int
ecommunity_cmp (struct ecommunity *ecom1, struct ecommunity *ecom2)
{
  if (ecom1->size == ecom2->size
      && memcmp (ecom1->val, ecom2->val, ecom1->size * ECOMMUNITY_SIZE) == 0)
    return 1;
  return 0;
}

/* Initialize Extended Comminities related hash. */
void
ecommunity_init ()
{
  ecomhash = hash_create (ecommunity_hash_make, ecommunity_cmp);
}

/* Extended Communities token enum. */
enum ecommunity_token
{
  ecommunity_token_rt,
  ecommunity_token_soo,
  ecommunity_token_val,
  ecommunity_token_unknown
};

/* Get next Extended Communities token from the string. */
char *
ecommunity_gettoken (char *str, struct ecommunity_val *eval,
		     enum ecommunity_token *token)
{
  int ret;
  int dot = 0;
  int digit = 0;
  int separator = 0;
  u_int32_t val_low = 0;
  u_int32_t val_high = 0;
  char *p = str;
  struct in_addr ip;
  char ipstr[INET_ADDRSTRLEN + 1];

  /* Skip white space. */
  while (isspace ((int) *p))
    {
      p++;
      str++;
    }

  /* Check the end of the line. */
  if (*p == '\0')
    return NULL;

  /* "rt" and "soo" keyword parse. */
  if (! isdigit ((int) *p)) 
    {
      /* "rt" match check.  */
      if (tolower ((int) *p) == 'r')
	{
	  p++;
 	  if (tolower ((int) *p) == 't')
	    {
	      p++;
	      *token = ecommunity_token_rt;
	      return p;
	    }
	  if (isspace ((int) *p) || *p == '\0')
	    {
	      *token = ecommunity_token_rt;
	      return p;
	    }
	  goto error;
	}
      /* "soo" match check.  */
      else if (tolower ((int) *p) == 's')
	{
	  p++;
 	  if (tolower ((int) *p) == 'o')
	    {
	      p++;
	      if (tolower ((int) *p) == 'o')
		{
		  p++;
		  *token = ecommunity_token_soo;
		  return p;
		}
	      if (isspace ((int) *p) || *p == '\0')
		{
		  *token = ecommunity_token_soo;
		  return p;
		}
	      goto error;
	    }
	  if (isspace ((int) *p) || *p == '\0')
	    {
	      *token = ecommunity_token_soo;
	      return p;
	    }
	  goto error;
	}
      goto error;
    }
  
  while (isdigit ((int) *p) || *p == ':' || *p == '.') 
    {
      if (*p == ':') 
	{
	  if (separator)
	    goto error;

	  separator = 1;
	  digit = 0;

	  if (dot)
	    {
	      if ((p - str) > INET_ADDRSTRLEN)
		goto error;

	      memset (ipstr, 0, INET_ADDRSTRLEN + 1);
	      memcpy (ipstr, str, p - str);

	      ret = inet_aton (ipstr, &ip);
	      if (ret == 0)
		goto error;
	    }
	  else
	    val_high = val_low;

	  val_low = 0;
	}
      else if (*p == '.')
	{
	  if (separator)
	    goto error;
	  dot++;
	  if (dot > 4)
	    goto error;
	}
      else
	{
	  digit = 1;
	  val_low *= 10;
	  val_low += (*p - '0');
	}
      p++;
    }

  /* Low digit part must be there. */
  if (! digit || ! separator)
    goto error;

  /* Encode result into routing distinguisher.  */
  if (dot)
    {
      eval->val[0] = ECOMMUNITY_ENCODE_IP;
      eval->val[1] = 0;
      memcpy (&eval->val[2], &ip, sizeof (struct in_addr));
      eval->val[6] = (val_low >> 8) & 0xff;
      eval->val[7] = val_low & 0xff;
    }
  else
    {
      eval->val[0] = ECOMMUNITY_ENCODE_AS;
      eval->val[1] = 0;
      eval->val[2] = (val_high >>8) & 0xff;
      eval->val[3] = val_high & 0xff;
      eval->val[4] = (val_low >>24) & 0xff;
      eval->val[5] = (val_low >>16) & 0xff;
      eval->val[6] = (val_low >>8) & 0xff;
      eval->val[7] = val_low & 0xff;
    }
  *token = ecommunity_token_val;
  return p;

 error:
  *token = ecommunity_token_unknown;
  return p;
}

/* Convert string to extended community attribute. 

   When type is already known, please specify both str and type.  str
   should not include keyword such as "rt" and "soo".  Type is
   ECOMMUNITY_TYPE_ROUTE_TARGET or ECOMMUNITY_TYPE_SITE_ORIGIN.
   keyword_included should be zero.

   For example route-map's "set extcommunity" command case:

   "rt 100:1 100:2 100:3"        -> str = "100:1 100:2 100:3"
                                    type = ECOMMUNITY_TYPE_ROUTE_TARGET
                                    keyword_included = 0

   "soo 100:1"                   -> str = "100:1"
                                    type = ECOMMUNITY_TYPE_SITE_ORIGIN
                                    keyword_included = 0

   When string includes keyword for each extended community value.
   Please specify keyword_included as non-zero value.

   For example standard extcommunity-list case:

   "rt 100:1 rt 100:2 soo 100:1" -> str = "rt 100:1 rt 100:2 soo 100:1"
                                    type = 0
                                    keyword_include = 1
*/
struct ecommunity *
ecommunity_str2com (char *str, int type, int keyword_included)
{
  struct ecommunity *ecom = NULL;
  enum ecommunity_token token;
  struct ecommunity_val eval;
  int keyword = 0;

  while ((str = ecommunity_gettoken (str, &eval, &token)))
    {
      switch (token)
	{
	case ecommunity_token_rt:
	case ecommunity_token_soo:
	  if (! keyword_included || keyword)
	    {
	      if (ecom)
		ecommunity_free (ecom);
	      return NULL;
	    }
	  keyword = 1;

	  if (token == ecommunity_token_rt)
	    {
	      type = ECOMMUNITY_TYPE_ROUTE_TARGET;
	    }
	  if (token == ecommunity_token_soo)
	    {
	      type = ECOMMUNITY_TYPE_SITE_ORIGIN;
	    }
	  break;
	case ecommunity_token_val:
	  if (keyword_included)
	    {
	      if (! keyword)
		{
		  if (ecom)
		    ecommunity_free (ecom);
		  return NULL;
		}
	      keyword = 0;
	    }
	  if (ecom == NULL)
	    ecom = ecommunity_new ();
	  eval.val[1] = type;
	  ecommunity_add_val (ecom, &eval);
	  break;
	case ecommunity_token_unknown:
	default:
	  if (ecom)
	    ecommunity_free (ecom);
	  return NULL;
	  break;
	}
    }
  return ecom;
}

struct ecommunity *
ecommunity_cost_str2com (char *str, u_char poi)
{
  struct ecommunity *ecom = NULL;
  struct ecommunity_cost ecost;
  u_int32_t id = 0;
  u_int32_t val = 0;
  char *p = str;

  while (isspace ((int) *p))
    p++;

  while (isdigit ((int) *p))
    {
      id *= 10;
      id += (*p - '0');
      p++;
    }

  while (isspace ((int) *p))
    p++;

  while (isdigit ((int) *p))
    {
      val *= 10;
      val += (*p - '0');
      p++;
    }

  ecost.type = ntohs (ECOMMUNITY_COST_COMMUNITY);
  ecost.poi = poi;
  ecost.id = id;
  ecost.val = ntohl (val);

  ecom = ecommunity_new ();
  ecommunity_add_val (ecom, (struct ecommunity_val *)&ecost);

  return ecom;
}

/* Convert extended community attribute to string.  

   Due to historical reason of industry standard implementation, there
   are three types of format.

   route-map set extcommunity format
        "rt 100:1 100:2"
        "soo 100:3"

   extcommunity-list
        "rt 100:1 rt 100:2 soo 100:3"

   "show ip bgp" and extcommunity-list regular expression matching
        "RT:100:1 RT:100:2 SoO:100:3"

   For each formath please use below definition for format:

   ECOMMUNITY_FORMAT_CONFIG
   ECOMMUNITY_FORMAT_DISPLAY
*/


char *
ecommunity_cost_poi_print (u_char poi)
{
  if (poi == ECOMMUNITY_COST_POI_IGP)
    return "igp";

  return "?";
}

char *
ecommunity_ecom2str (struct ecommunity *ecom, int format)
{
  int i;
  u_char *pnt;
  int encode = 0;
  int encode_check = 0;
  int type = 0;
#define ECOMMUNITY_STR_DEFAULT_LEN  26
  int str_size;
  int str_pnt;
  u_char *str_buf;
  char *prefix;
  int len = 0;
  int first = 1;

  /* For parse Extended Community attribute tupple. */
  struct ecommunity_as
  {
    as_t as;
    u_int32_t val;
  } eas;

  struct ecommunity_ip
  {
    struct in_addr ip;
    u_int16_t val;
  } eip;

  if (ecom->size == 0)
    {
      str_buf = XMALLOC (MTYPE_ECOMMUNITY_STR, 1);
      str_buf[0] = '\0';
      return str_buf;
    }

  /* Prepare buffer.  */
  str_buf = XMALLOC (MTYPE_ECOMMUNITY_STR, ECOMMUNITY_STR_DEFAULT_LEN + 1);
  str_size = ECOMMUNITY_STR_DEFAULT_LEN + 1;
  str_pnt = 0;

  for (i = 0; i < ecom->size; i++)
    {
      /* Make it sure size is enough.  */
      while (str_pnt + ECOMMUNITY_STR_DEFAULT_LEN >= str_size)
	{
	  str_size *= 2;
	  str_buf = XREALLOC (MTYPE_ECOMMUNITY_STR, str_buf, str_size);
	}

      /* Space between each value.  */
      if (! first)
	str_buf[str_pnt++] = ' ';

      pnt = ecom->val + (i * 8);

      /* High-order octet of type. */
      encode = *pnt++;
      encode_check = encode & ~ECOMMUNITY_FLAG_NON_TRANSITIVE;

      if (encode_check != ECOMMUNITY_ENCODE_AS
          && encode_check != ECOMMUNITY_ENCODE_IP
          && encode_check != ECOMMUNITY_ENCODE_OPAQUE)
	{
	  len = sprintf (str_buf + str_pnt, "?");
	  str_pnt += len;
	  first = 0;
	  continue;
	}
      
      /* Low-order octet of type. */
      type = *pnt++;
      if ((encode_check == ECOMMUNITY_ENCODE_AS
	   || encode_check == ECOMMUNITY_ENCODE_IP
	   || encode_check == ECOMMUNITY_ENCODE_4OCTET_AS)
	  && (type == ECOMMUNITY_TYPE_ROUTE_TARGET || type == ECOMMUNITY_TYPE_SITE_ORIGIN)
	  && ! CHECK_FLAG (encode, ECOMMUNITY_FLAG_NON_TRANSITIVE))
	{
	  switch (format)
	    {
	      case ECOMMUNITY_FORMAT_CONFIG:
		prefix = (type == ECOMMUNITY_TYPE_ROUTE_TARGET ? "rt " : "soo ");
		break;
	      case ECOMMUNITY_FORMAT_DISPLAY:
		prefix = (type == ECOMMUNITY_TYPE_ROUTE_TARGET ? "RT:" : "SoO:");
		break;
	      default:
		prefix = "";
		break;
	    }
	}
      else if (encode_check == ECOMMUNITY_ENCODE_OPAQUE
	       && type == ECOMMUNITY_TYPE_COST_COMMUNITY
	       && CHECK_FLAG (encode, ECOMMUNITY_FLAG_NON_TRANSITIVE))
	{
	  u_int32_t val;
	  u_char poi;
	  u_char id;

          poi = *pnt++;
	  id =  *pnt++;
	  memcpy (&val, pnt, 4);

	  len = sprintf (str_buf + str_pnt, "Cost:%s:%d:%u",
			 ecommunity_cost_poi_print (poi), id, ntohl (val));
	  str_pnt += len;
	  first = 0;
	  continue;
	}
      else
	{
	  len = sprintf (str_buf + str_pnt, "?");
	  str_pnt += len;
	  first = 0;
	  continue;
	}

      /* Put string into buffer.  */
      if (encode_check == ECOMMUNITY_ENCODE_AS)
	{
	  eas.as = (*pnt++ << 8);
	  eas.as |= (*pnt++);

	  eas.val = (*pnt++ << 24);
	  eas.val |= (*pnt++ << 16);
	  eas.val |= (*pnt++ << 8);
	  eas.val |= (*pnt++);

	  len = sprintf (str_buf + str_pnt, "%s%d:%d", prefix,
			 eas.as, eas.val);
	  str_pnt += len;
	  first = 0;
	}
      else if (encode_check == ECOMMUNITY_ENCODE_IP)
	{
	  memcpy (&eip.ip, pnt, 4);
	  pnt += 4;
	  eip.val = (*pnt++ << 8);
	  eip.val |= (*pnt++);

	  len = sprintf (str_buf + str_pnt, "%s%s:%d", prefix,
			 inet_ntoa (eip.ip), eip.val);
	  str_pnt += len;
	  first = 0;
	}
    }
  return str_buf;
}

int
ecommunity_match (struct ecommunity *ecom1, struct ecommunity *ecom2)
{
  int i = 0;
  int j = 0;

  if (ecom1 == NULL && ecom2 == NULL)
    return 1;

  if (ecom1 == NULL || ecom2 == NULL)
    return 0;

  if (ecom1->size < ecom2->size)
    return 0;

  /* Every community on com2 needs to be on com1 for this to match */
  while (i < ecom1->size && j < ecom2->size)
    {
      if (memcmp (ecom1->val + i, ecom2->val + j, ECOMMUNITY_SIZE) == 0)
        j++;
      i++;
    }

  if (j == ecom2->size)
    return 1;
  else
    return 0;
}

int
ecommunity_cost_cmp (struct ecommunity *ecom1, struct ecommunity *ecom2, u_char poi)
{
  int i = 0;
  int j = 0;
  int find1 = 0;
  int find2 = 0;
  u_char id1 = 0;
  u_char id2 = 0;
  u_int32_t val1 = 0;
  u_int32_t val2 = 0;

  /* For parse Extended Community attribute tupple. */
  struct ecommunity_cost
  {
    u_int16_t type;
    u_char poi;
    u_char id;
    u_int32_t val;
  } ecost;

  if (ecom1 == NULL && ecom2 == NULL)
    return 0;

  while (1)
    {
      while (ecom1 && i < ecom1->size)
	{
	  memcpy (&ecost, (ecom1->val + (i * 8)), sizeof (struct ecommunity_cost));

	  if (ntohs (ecost.type) == ECOMMUNITY_COST_COMMUNITY
	      && ecost.poi == poi)
	    {
	      id1 = ecost.id;
	      val1 = ntohl (ecost.val);
	      find1 = 1;
	      break;
	    }
	  i++;
	}

      while (ecom2 && j < ecom2->size)
	{
	  memcpy (&ecost, ecom2->val + (j * 8), sizeof (struct ecommunity_cost));
	  if (ntohs (ecost.type) == ECOMMUNITY_COST_COMMUNITY
	      && ecost.poi == poi)
	    {
	      id2 = ecost.id;
	      val2 = ntohl (ecost.val);
	      find2 = 1;
	      break;
	    }
	  j++;
	}

      if (! find1 || ! find2)
	break;
      if (id1 != id2)
	break;
      if (val1 != val2)
	break;

      find1 = 0;
      find2 = 0;
      i++;
      j++;
    }

  if (! find1)
    val1 = COST_COMMUNITY_DEFAULT_COST;
  if (! find2)
    val2 = COST_COMMUNITY_DEFAULT_COST;

  if (find1 && find2)
    {
      if (id1 < id2)
	return 1;
      if (id1 > id2)
	return -1;
    }
  if (val1 < val2)
    return 1;
  if (val1 > val2)
    return -1;

  return 0;
}
