/* Note : this particular snipset of code is available under
 * the LGPL, MPL or BSD license (at your choice).
 * Jean II
 */

// Require Wireless Tools 25 for sub-ioctl and addr support

/* --------------------------- INCLUDE --------------------------- */

#if WIRELESS_EXT <= 12
/* Wireless extensions backward compatibility */

/* We need the full definition for private ioctls */
struct iw_request_info
{
	__u16		cmd;		/* Wireless Extension command */
	__u16		flags;		/* More to come ;-) */
};
#endif /* WIRELESS_EXT <= 12 */

#ifndef IW_PRIV_TYPE_ADDR
#define IW_PRIV_TYPE_ADDR	0x6000
#endif	/* IW_PRIV_TYPE_ADDR */

/* --------------------------- HANDLERS --------------------------- */

/* First method : using sub-ioctls.
 * Note that sizeof(int + struct sockaddr) = 20 > 16, therefore the
 * data is passed in (char *) extra, and sub-ioctl in data->flags. */
static int sample_ioctl_set_mac(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_point *data,
				struct sockaddr *mac_addr)
{
	unsigned char *	addr = (char *) &mac_addr->sa_data;

	switch(data->flags) {
	case 0:
		printk(KERN_DEBUG "%s: mac_add %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	case 1:
		printk(KERN_DEBUG "%s: mac_del %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	case 2:
		printk(KERN_DEBUG "%s: mac_kick %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	default:
		printk(KERN_DEBUG "%s: mac_undefined %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	}

	return 0;
}

/* Second method : bind single handler to multiple ioctls.
 * Note that sizeof(struct sockaddr) = 16 <= 16, therefore the
 * data is passed in (struct iwreq) (and also mapped in extra).
 */
static int sample_ioctl_set_addr(struct net_device *dev,
				 struct iw_request_info *info,
				 struct sockaddr *mac_addr, char *extra)
{
	unsigned char *	addr = (char *) &mac_addr->sa_data;

	switch(info->cmd) {
	case SIOCIWFIRSTPRIV + 28:
		printk(KERN_DEBUG "%s: addr_add %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	case SIOCIWFIRSTPRIV + 30:
		printk(KERN_DEBUG "%s: addr_del %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	default:
		printk(KERN_DEBUG "%s: mac_undefined %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		break;
	}

	return 0;
}

// Extra fun for testing
static int sample_ioctl_get_mac(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_point *data,
				struct sockaddr *mac_addr)
{
	unsigned char	fake_addr[6];
	int		i;
	int		j;

	for(i = 0; i < 16; i++) {
		/* Create a fake address */
		for(j = 0; j < 6; j++)
			fake_addr[j] = (unsigned char) ((j << 4) + i);
		/* Put in in the table */
		memcpy(&(mac_addr[i]).sa_data, fake_addr, ETH_ALEN);
		mac_addr[i].sa_family = ARPHRD_ETHER;
	}
	data->length = 16;

	return 0;
}

static int sample_ioctl_set_float(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_freq *freq, char *extra)
{
	printk(KERN_DEBUG "%s: set_float %d;%d\n",
	       dev->name, freq->m, freq->e);

	return 0;
}

/* --------------------------- BINDING --------------------------- */

static const struct iw_priv_args sample_priv[] = {
	// *** Method 1 : using sub-ioctls ***
	/* --- sub-ioctls handler --- */
	{ SIOCIWFIRSTPRIV + 0,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "" },
	/* --- sub-ioctls definitions --- */
	{ 0,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "macadd" },
	{ 1,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "macdel" },
	{ 2,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "mackick" },
	// *** Method 2 : binding one handler to multiple ioctls ***
	{ SIOCIWFIRSTPRIV + 2,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "addradd" },
	{ SIOCIWFIRSTPRIV + 4,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "addrdel" },
	// *** Extra fun ***
	{ SIOCIWFIRSTPRIV + 1,
	  0, IW_PRIV_TYPE_ADDR | 16, "macget" },
	{ SIOCIWFIRSTPRIV + 6,
	  IW_PRIV_TYPE_FLOAT | IW_PRIV_SIZE_FIXED | 1, 0, "setfloat" },
};

static const iw_handler sample_private_handler[] =
{							/* SIOCIWFIRSTPRIV + */
#if WIRELESS_EXT >= 15
	/* Various little annoying bugs in the new API before
	 * version 15 make it difficult to use the new API for those ioctls.
	 * For example, it doesn't know about the new data type.
	 * Rather than littering the code with workarounds,
	 * let's use the regular ioctl handler. - Jean II */
	(iw_handler) sample_ioctl_set_mac,		/* 0 */
	(iw_handler) sample_ioctl_get_mac,		/* 1 */
	(iw_handler) sample_ioctl_set_addr,		/* 2 */
	(iw_handler) NULL,				/* 3 */
	(iw_handler) sample_ioctl_set_addr,		/* 4 */
	(iw_handler) NULL,				/* 5 */
	(iw_handler) sample_ioctl_set_float,		/* 6 */
#endif	/* WIRELESS_EXT >= 15 */
};

#if WIRELESS_EXT < 15
		/* Various little annoying bugs in the new API before
		 * version 15 make it difficult to use those ioctls.
		 * For example, it doesn't know about the new data type.
		 * Rather than littering the code with workarounds,
		 * let's use this code that just works. - Jean II */
	case SIOCIWFIRSTPRIV + 0:
		if (wrq->u.data.length > 1)
			ret = -E2BIG;
		else if (wrq->u.data.pointer) {
			struct sockaddr mac_addr;
			if (copy_from_user(&mac_addr, wrq->u.data.pointer,
					   sizeof(struct sockaddr))) {
				ret = -EFAULT;
				break;
			}
			ret = sample_ioctl_set_mac(dev, NULL, &wrq->u.data,
						   &mac_addr);
		}
		break;
	case SIOCIWFIRSTPRIV + 2:
	case SIOCIWFIRSTPRIV + 4:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else {
			struct iw_request_info info;
			info.cmd = cmd;
			ret = sample_ioctl_set_addr(dev, &info,
						    &wrq->u.ap_addr,
						    NULL);
		}
		break;
	case SIOCIWFIRSTPRIV + 1:
		if (wrq->u.essid.pointer) {
			struct sockaddr mac_addr[16];
			char nickbuf[IW_ESSID_MAX_SIZE + 1];
			ret = sample_ioctl_get_mac(dev, NULL, &wrq->u.data,
						   mac_addr);
			if (copy_to_user(wrq->u.data.pointer, nickbuf,
					 wrq->u.data.length *
					 sizeof(struct sockaddr)))
				ret = -EFAULT;
		}
		break;
	case SIOCIWFIRSTPRIV + 6:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else {
			ret = sample_ioctl_set_float(dev, NULL,
						     &wrq->u.freq,
						     NULL);
		}
		break;
#endif	/* WIRELESS_EXT < 15 */
