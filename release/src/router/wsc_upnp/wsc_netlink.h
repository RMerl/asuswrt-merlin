/*
	header file for wsc_netlink.c
*/

#ifndef __WSC_NETLINK_H__
#define __WSC_NETLINK_H__

#include <linux/wireless.h>

#define NETLINK_ROUTE 0
#define RTMGRP_LINK 1
#ifndef RTM_BASE
#define RTM_BASE 0x10
#endif

#if 0	//already defined in <linux/rtnetlink.h>
#define RTM_NEWLINK (RTM_BASE + 0)
#define RTM_DELLINK (RTM_BASE + 1)

#define RTA_ALIGNTO 4
#define RTA_ALIGN(len) (((len) + RTA_ALIGNTO - 1) & ~(RTA_ALIGNTO - 1))
#define RTA_NEXT(rta,attrlen) \
	((attrlen) -= RTA_ALIGN((rta)->rta_len), \
	(struct rtattr *) (((char *)(rta)) + RTA_ALIGN((rta)->rta_len)))

#define NLMSG_ALIGNTO 4
#define NLMSG_ALIGN(len) (((len) + NLMSG_ALIGNTO - 1) & ~(NLMSG_ALIGNTO - 1))
#define NLMSG_LENGTH(len) ((len) + NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_DATA(nlh) ((void*) (((char*) nlh) + NLMSG_LENGTH(0)))
#endif

#ifndef RTA_OK
#define RTA_OK(rta,len) \
	((len) > 0 && (rta)->rta_len >= sizeof(struct rtattr) && \
	(rta)->rta_len <= (len))
#endif

#if (WIRELESS_EXT < 15)
/* ----------------------- WIRELESS EVENTS ----------------------- */
/*
 * Wireless events are carried through the rtnetlink socket to user
 * space. They are encapsulated in the IFLA_WIRELESS field of
 * a RTM_NEWLINK message.
 */

/*
 * A Wireless Event. Contains basically the same data as the ioctl...
 */
struct iw_event
{
	unsigned short		len;            /* Real lenght of this stuff */
    unsigned short		cmd;            /* Wireless IOCTL */
	union iwreq_data	u;              /* IOCTL fixed payload */
};

/* Size of the Event prefix (including padding and alignement junk) */
#define IW_EV_LCP_LEN   (sizeof(struct iw_event) - sizeof(union iwreq_data))

/* Size of the various events */
#define IW_EV_CHAR_LEN  (IW_EV_LCP_LEN + IFNAMSIZ)
#define IW_EV_UINT_LEN  (IW_EV_LCP_LEN + sizeof(__u32))
#define IW_EV_FREQ_LEN  (IW_EV_LCP_LEN + sizeof(struct iw_freq))
#define IW_EV_POINT_LEN (IW_EV_LCP_LEN + sizeof(struct iw_point))
#define IW_EV_PARAM_LEN (IW_EV_LCP_LEN + sizeof(struct iw_param))
#define IW_EV_ADDR_LEN  (IW_EV_LCP_LEN + sizeof(struct sockaddr))
#define IW_EV_QUAL_LEN  (IW_EV_LCP_LEN + sizeof(struct iw_quality))

/* Note : in the case of iw_point, the extra data will come at the
 * end of the event */

#endif  /* _LINUX_WIRELESS_H */


#define IWEVCUSTOM	0x8C02		/* Driver specific ascii string */
#define IWEVFIRST	0x8C00
#define IW_EVENT_IDX(cmd)	((cmd) - IWEVFIRST)

/* iw_point events are special. First, the payload (extra data) come at
 * the end of the event, so they are bigger than IW_EV_POINT_LEN. Second,
 * we omit the pointer, so start at an offset. */
#define IW_EV_POINT_OFF (((char *) &(((struct iw_point *) NULL)->length)) - (char *) NULL)

//wpa_supplicant event flags.
#define	RT_ASSOC_EVENT_FLAG                         0x0101
#define	RT_DISASSOC_EVENT_FLAG                      0x0102
#define	RT_REQIE_EVENT_FLAG                         0x0103
#define	RT_RESPIE_EVENT_FLAG                        0x0104
#define	RT_ASSOCINFO_EVENT_FLAG                     0x0105
#define RT_PMKIDCAND_FLAG                           0x0106
#endif
