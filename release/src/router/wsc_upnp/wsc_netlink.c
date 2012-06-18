/*
	function handler for linux netlink socket!
 */
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "wsc_common.h"
#include "wsc_netlink.h"
#include "wsc_msg.h"

int we_version_compiled = WIRELESS_EXT;
static int netlink_sock = -1;

static void wscEventHandler(char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom;
//	char *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end)
	{
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		//DBGPRINTF(RT_DBG_INFO, "Wireless event: cmd=0x%x len=%d, we_version_compiled=%d!\n", iwe->cmd, iwe->len, we_version_compiled);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if(iwe->cmd == IWEVCUSTOM)
		{
			if (we_version_compiled > 18)
			{			
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN, sizeof(struct iw_event) - dlen);
		}
		else
		{
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			}	

			if (custom + iwe->u.data.length > end){
				DBGPRINTF(RT_DBG_INFO, "custom(0x%x) + iwe->u.data.length(0x%x) > end(0x%x)!\n", custom, iwe->u.data.length, end);
				return;
			}
#if 0
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;
				
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
#endif
			//DBGPRINTF(RT_DBG_INFO, "iwe->u.data.flags=0x%x!\n", iwe->u.data.flags);
			switch(iwe->u.data.flags)
			{
				case RT_ASSOC_EVENT_FLAG:
				case RT_DISASSOC_EVENT_FLAG:
				case RT_REQIE_EVENT_FLAG:
				case RT_RESPIE_EVENT_FLAG:
				case RT_ASSOCINFO_EVENT_FLAG:
				case RT_PMKIDCAND_FLAG:
					break;
				default:
					if(strncmp(custom, "RAWSCMSG", 8) == 0)
					{	
						DBGPRINTF(RT_DBG_INFO, "Recevive a RAWSCMSG segment\n");
						WscRTMPMsgHandler(custom, iwe->u.data.length);
					}
					break;
			}
//			free(buf);
		}
		pos += iwe->len;
	}
	
}


static void wscNLEventRTMNewlinkHandle(struct nlmsghdr *nlmsgHdr, int len)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr * attr;

    DBGPRINTF(RT_DBG_INFO, "%s", __FUNCTION__);

	if (len < sizeof(struct ifinfomsg))
		return;

	ifi = NLMSG_DATA(nlmsgHdr);
    //wsc_hexdump("ifi: ", (char *)ifi, sizeof(struct ifinfomsg));
	
	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));
       
	attrlen = nlmsgHdr->nlmsg_len - nlmsg_len;
//	DBGPRINTF("attrlen=%d\n",attrlen);
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);
    //wsc_hexdump("rtattr: ", (char *)attr,sizeof(struct rtattr));
	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	//wsc_hexdump("rtattr: ", (char *)attr, rta_len);
	while (RTA_OK(attr, attrlen))
	{
		if (attr->rta_type == IFLA_WIRELESS)
		{
			wscEventHandler(((char *) attr) + rta_len, attr->rta_len - rta_len);
		} 
		attr = RTA_NEXT(attr, attrlen);
		//wsc_hexdump("rta_type: ", (char *)attr,sizeof(struct rtattr));
	}
}

void wscNLSockRecv(int sock)
{
	char buf[8192];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *nlmsgHdr;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from, &fromlen);

	if (left < 0)
	{
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	nlmsgHdr = (struct nlmsghdr *)buf;
	//wsc_hexdump("nlmsgHdr: ", (char *)nlmsgHdr, nlmsgHdr->nlmsg_len);

	while (left >= sizeof(*nlmsgHdr))
	{
		int len, plen;

		len = nlmsgHdr->nlmsg_len;
		plen = len - sizeof(*nlmsgHdr);
		if (len > left || plen < 0)
		{
			DBGPRINTF(RT_DBG_INFO, "Malformed netlink message: len=%d left=%d plen=%d", len, left, plen);
			break;
		}

		switch (nlmsgHdr->nlmsg_type)
		{
			case RTM_NEWLINK:
				wscNLEventRTMNewlinkHandle(nlmsgHdr, plen);
				break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		nlmsgHdr = (struct nlmsghdr *) ((char *) nlmsgHdr + len);
	}

	if (left > 0)
	{
		DBGPRINTF(RT_DBG_INFO, "%d extra bytes in the end of netlink message", left);
	}

}

/******************************************************************************
 * wscDevNLHandle
 *
 * Description: 
 *       Function that receives netlink socket msg from the kernel
 *       during the lifetime of the device, and send to appropriate
 *       msg queue and trigger the callback handler.  
 *
 * Parameters:
 *    None
 *
 * Return:
 *    None
 *****************************************************************************/
void *wscDevNLHandle(void *args)
{
	fd_set rfds;
	int retVal;
	int flags;
	
	DBGPRINTF(RT_DBG_INFO, "Pthread(%s)Now waiting for the netlink socket incoming message!\n", __FUNCTION__);

	flags = fcntl(netlink_sock, F_GETFL, 0); 
	if (flags == -1) 
    	goto done; 
    fcntl(netlink_sock, F_SETFL, flags | O_NONBLOCK);  
		
	while(!stopThread)
	{	
		FD_ZERO(&rfds);
		FD_SET(netlink_sock, &rfds);

    	retVal = select(netlink_sock + 1, &rfds, NULL, NULL, NULL);
		
		/* Don¡¦t rely on the value of Wsc now! */
		if (retVal == -1)
			perror("select()");
    	else if (retVal) {
			DBGPRINTF(RT_DBG_INFO, "(%s):netlink socket data is available now.\n", __FUNCTION__);
			if(FD_ISSET(netlink_sock, &rfds)){
				wscNLSockRecv(netlink_sock);
			}
		}
	}

done:	
	if(netlink_sock >= 0)
		close(netlink_sock);
		
	
	pthread_exit(NULL);
}

/******************************************************************************
 * wscK2UModuleInit
 *
 * Description: 
 *       The Kernel space 2 user space msg subsystem entry point.
 *	  In Linux system, we use netlink socket to receive the specific type msg
 *	  send from wireless driver.
 *		 This function mainly create a posix thread and recvive msg then dispatch
 *	  to coressponding handler.
 *
 * Parameters:
 *    None
 * 
 * Return:
 *    success: 1
 *    fail   : 0
 *****************************************************************************/
int wscK2UModuleInit(void)
{
	int sock, retVal;
    struct sockaddr_nl local;
	pthread_t wscNLHandle_thread;
	
	//Create netlink socket
	sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock < 0)
	{
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		return WSC_SYS_ERROR;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;

	if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0)
	{
		perror("bind(netlink)");
		close(sock);
		return WSC_SYS_ERROR;
	}

    /* 
    	start a netlink socket receiver handle thread  
    */
    DBGPRINTF(RT_DBG_INFO, "sock=%d!(0x%p)\n", sock, &sock);
	netlink_sock = sock;
	retVal = pthread_create(&wscNLHandle_thread, NULL, wscDevNLHandle, NULL);
	if(retVal == 0)
		pthread_detach(wscNLHandle_thread);
	
	return WSC_SYS_SUCCESS;
}

