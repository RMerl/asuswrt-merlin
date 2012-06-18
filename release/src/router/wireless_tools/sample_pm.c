/* Note : this particular snipset of code is available under
 * the LGPL, MPL or BSD license (at your choice).
 * Jean II
 */

/* --------------------------- INCLUDE --------------------------- */

/* Backward compatibility for Wireless Extension 9 */
#ifndef IW_POWER_MODIFIER
#define IW_POWER_MODIFIER	0x000F	/* Modify a parameter */
#define IW_POWER_MIN		0x0001	/* Value is a minimum  */
#define IW_POWER_MAX		0x0002	/* Value is a maximum */
#define IW_POWER_RELATIVE	0x0004	/* Value is not in seconds/ms/us */
#endif IW_POWER_MODIFIER

struct net_local {
  int		pm_on;		// Power Management enabled
  int		pm_multi;	// Receive multicasts
  int		pm_period;	// Power Management period
  int		pm_period_auto;	// Power Management auto mode
  int		pm_max_period;	// Power Management max period
  int		pm_min_period;	// Power Management min period
  int		pm_timeout;	// Power Management timeout
};

/* --------------------------- HANDLERS --------------------------- */

static int ioctl_set_power(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *prq,
			   char *extra)
{
  /* Disable it ? */
  if(prq->disabled)
    {
      local->pm_on = 0;
    }
  else
    {
      /* Check mode */
      switch(prq->flags & IW_POWER_MODE)
	{
	case IW_POWER_UNICAST_R:
	  local->pm_multi = 0;
	  local->need_commit = 1;
	  break;
	case IW_POWER_ALL_R:
	  local->pm_multi = 1;
	  local->need_commit = 1;
	  break;
	case IW_POWER_ON:	/* None = ok */
	  break;
	default:	/* Invalid */
	  return(-EINVAL);
	}
      /* Set period */
      if(prq->flags & IW_POWER_PERIOD)
	{
	  int	period = prq->value;
#if WIRELESS_EXT < 21
	  period /= 1000000;
#endif
	  /* Hum: check if within bounds... */

	  /* Activate PM */
	  local->pm_on = 1;
	  local->need_commit = 1;

	  /* Check min value */
	  if(prq->flags & IW_POWER_MIN)
	    {
	      local->pm_min_period = period;
	      local->pm_period_auto = 1;
	    }
	  else
	    /* Check max value */
	    if(prq->flags & IW_POWER_MAX)
	      {
		local->pm_max_period = period;
		local->pm_period_auto = 1;
	      }
	    else
	      {
		/* Fixed value */
		local->pm_period = period;
		local->pm_period_auto = 0;
	      }
	}
      /* Set timeout */
      if(prq->flags & IW_POWER_TIMEOUT)
	{
	  /* Activate PM */
	  local->pm_on = 1;
	  local->need_commit = 1;
	  /* Fixed value in ms */
	  local->pm_timeout = prq->value/1000;
	}
    }

  return(0);
}

static int ioctl_get_power(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *prq,
			   char *extra)
{
  prq->disabled = !local->pm_on;
  /* By default, display the period */
  if(!(prq->flags & IW_POWER_TIMEOUT))
    {
      int	inc_flags = prq->flags;
      prq->flags = IW_POWER_PERIOD | IW_POWER_RELATIVE;
      /* Check if auto */
      if(local->pm_period_auto)
	{
	  /* By default, the min */
	  if(!(inc_flags & IW_POWER_MAX))
	    {
	      prq->value = local->pm_min_period;
#if WIRELESS_EXT < 21
	      prq->value *= 1000000;
#endif
	      prq->flags |= IW_POWER_MIN;
	    }
	  else
	    {
	      prq->value = local->pm_max_period;
#if WIRELESS_EXT < 21
	      prq->value *= 1000000;
#endif
	      prq->flags |= IW_POWER_MAX;
	    }
	}
      else
	{
	  /* Fixed value. Check the flags */
	  if(inc_flags & (IW_POWER_MIN | IW_POWER_MAX))
	    return(-EINVAL);
	  else
	    {
	      prq->value = local->pm_period;
#if WIRELESS_EXT < 21
	      prq->value *= 1000000;
#endif
	    }
	}
    }
  else
    {
      /* Deal with the timeout - always fixed */
      prq->flags = IW_POWER_TIMEOUT;
      prq->value = local->pm_timeout * 1000;
    }
  if(local->pm_multi)
    prq->flags |= IW_POWER_ALL_R;
  else
    prq->flags |= IW_POWER_UNICAST_R;

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

#if WIRELESS_EXT > 9
#if WIRELESS_EXT < 21
      range.min_pmp = 1000000;	/* 1 units */
      range.max_pmp = 12000000;	/* 12 units */
#else
      range.min_pmp = 1;	/* 1 units */
      range.max_pmp = 12;	/* 12 units */
#endif
      range.min_pmt = 1000;	/* 1 ms */
      range.max_pmt = 1000000;	/* 1 s */
      range.pmp_flags = IW_POWER_PERIOD | IW_POWER_RELATIVE |
        IW_POWER_MIN | IW_POWER_MAX;
      range.pmt_flags = IW_POWER_TIMEOUT;
      range.pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_UNICAST_R;
#endif /* WIRELESS_EXT > 9 */
  return(0);
}

/* --------------------------- BINDING --------------------------- */

#if WIRELESS_EXT > 12
/* Use the new driver API, save overhead */
static const iw_handler		handler_table[] =
{
	...
	(iw_handler) ioctl_set_power,		/* SIOCSIWPOWER */
	(iw_handler) ioctl_get_power,		/* SIOCGIWPOWER */
};
#else	/* WIRELESS_EXT < 12 */
/* Use old API in the ioctl handler */
static int
do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
  struct iwreq *wrq = (struct iwreq *) ifr;
  int		err = 0;

  switch (cmd)
    {
#if WIRELESS_EXT > 8
      /* Set the desired Power Management mode */
    case SIOCSIWPOWER:
      err = ioctl_set_power(dev, NULL, &(wrq->u.power), NULL);
      break;

      /* Get the power management settings */
    case SIOCGIWPOWER:
      err = ioctl_get_power(dev, NULL, &(wrq->u.power), NULL);
      break;
#endif	/* WIRELESS_EXT > 8 */
    }
  return(err);
}
#endif	/* WIRELESS_EXT < 12 */

