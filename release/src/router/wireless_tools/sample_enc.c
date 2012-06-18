/* Note : this particular snipset of code is available under
 * the LGPL, MPL or BSD license (at your choice).
 * Jean II
 */

/* --------------------------- INCLUDE --------------------------- */

#define MAX_KEY_SIZE	16
#define	MAX_KEYS	8
int	key_on = 0;
int	key_open = 1;
int	key_current = 0;
char	key_table[MAX_KEYS][MAX_KEY_SIZE];
int	key_size[MAX_KEYS];

/* --------------------------- HANDLERS --------------------------- */

static int ioctl_set_encode(struct net_device *dev,
			    struct iw_request_info *info,
			    struct iw_point *erq,
			    char *key)
{
  int	index = (erq->flags & IW_ENCODE_INDEX) - 1;

  if (erq->length > 0)
    {
      /* Check the size of the key */
      if(erq->length > MAX_KEY_SIZE)
	return(-EINVAL);

      /* Check the index */
      if((index < 0) || (index >= MAX_KEYS))
	index = key_current;

      /* Copy the key in the driver */
      memcpy(key_table[index], key, erq->length);
      key_size[index] = erq->length;
      key_on = 1;
    }
  else
    {
      /* Do we want to just set the current key ? */
      if((index >= 0) && (index < MAX_KEYS))
	{
	  if(key_size[index] > 0)
	    {
	      key_current = index;
	      key_on = 1;
	    }
	  else
	    return(-EINVAL);
	}
    }

  /* Read the flags */
  if(erq->flags & IW_ENCODE_DISABLED)
    key_on = 0;		/* disable encryption */
  if(erq->flags & IW_ENCODE_RESTRICTED)
    key_open = 0;	/* disable open mode */
  if(erq->flags & IW_ENCODE_OPEN)
    key_open = 1;	/* enable open mode */

  return(0);
}

static int ioctl_get_encode(struct net_device *dev,
			    struct iw_request_info *info,
			    struct iw_point *erq,
			    char *key)
{
  int	index = (erq->flags & IW_ENCODE_INDEX) - 1;

  /* Set the flags */
  erq->flags = 0;
  if(key_on == 0)
    erq->flags |= IW_ENCODE_DISABLED;
  if(key_open == 0)
    erq->flags |= IW_ENCODE_RESTRICTED;
  else
    erq->flags |= IW_ENCODE_OPEN;

  /* Which key do we want */
  if((index < 0) || (index >= MAX_KEYS))
    index = key_current;
  erq->flags |= index + 1;

  /* Copy the key to the user buffer */
  erq->length = key_size[index];
  memcpy(key, key_table[index], key_size[index]);

  return(0);
}

static int ioctl_get_range(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *rrq,
			   char *extra)
{
  struct iw_range *range = (struct iw_range *) extra;

  rrq->length = sizeof(struct iw_range);

  memset(range, 0, sizeof(struct iw_range));

#if WIRELESS_EXT > 10
  /* Version we are compiled with */
  range->we_version_compiled = WIRELESS_EXT;
  /* Minimum version we recommend */
  range->we_version_source = 8;
#endif /* WIRELESS_EXT > 10 */

#if WIRELESS_EXT > 8
  range->encoding_size[0] = 8;	/* DES = 64 bits key */
  range->encoding_size[1] = 16;
  range->num_encoding_sizes = 2;
  range->max_encoding_tokens = 8;
#endif /* WIRELESS_EXT > 8 */
  return(0);
}

/* --------------------------- BINDING --------------------------- */

#if WIRELESS_EXT > 12
static const iw_handler		handler_table[] =
{
	...
	(iw_handler) ioctl_set_encode,		/* SIOCSIWENCODE */
	(iw_handler) ioctl_get_encode,		/* SIOCGIWENCODE */
};
#else	/* WIRELESS_EXT < 12 */
static int
do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
  struct iwreq *wrq = (struct iwreq *) ifr;
  int		err = 0;

  switch (cmd)
    {
#if WIRELESS_EXT > 8
    case SIOCSIWENCODE:
      {
	char keybuf[MAX_KEY_SIZE];
	if(wrq->u.encoding.pointer)
	  {
	    /* We actually have a key to set */
	    if(wrq->u.encoding.length > MAX_KEY_SIZE)
	      {
		err = -E2BIG;
		break;
	      }
	    if(copy_from_user(keybuf, wrq->u.encoding.pointer,
			      wrq->u.encoding.length))
	      {
		err = -EFAULT;
		break;
	      }
	  }
	else
	  if(wrq->u.encoding.length != 0)
	    {
	      err = -EINVAL;
	      break;
	    }
	err = ioctl_set_encode(dev, NULL, &(wrq->u.encoding), keybuf);
      }
      break;

    case SIOCGIWENCODE:
      /* only super-user can see encryption key */
      if(! capable(CAP_NET_ADMIN))
	{
	  err = -EPERM;
	  break;
	}
      {
	char keybuf[MAX_KEY_SIZE];
	err = ioctl_get_encode(dev, NULL, &(wrq->u.encoding), keybuf);
	if(wrq->u.encoding.pointer)
	  {
	    if (copy_to_user(wrq->u.encoding.pointer, keybuf,
			     wrq->u.encoding.length))
	      err= -EFAULT;
	  }
      }
      break;
#endif	/* WIRELESS_EXT > 8 */
    }
  return(err);
}
#endif	/* WIRELESS_EXT < 12 */

