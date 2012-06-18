/*
	function handler for ioctl handler
*/
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)		// for 3052 missing include file? --YY
#include <net/if.h>
#include <linux/types.h>
#endif

#include <linux/wireless.h>
#include <netinet/in.h>
#include "wsc_common.h"
#include "wsc_ioctl.h"


/******************************************************************************
 * wsc_set_oid
 *
 * Description: 
 *       Send Wsc Message to kernel space by ioctl function call.
 *
 * Parameters:
 *    uint16 oid - The ioctl flag type for the ioctl function call.
 *    char *data - The data message want to send by ioctl
 *    int len    - The length of data 
 *  
 * Return Value:
 *    Indicate if the ioctl success or failure.
 *    	-1 = failure.
 *    	0  = If success. 
 *****************************************************************************/
int wsc_set_oid(uint16 oid, char *data, int len)
{
	char *buf;
	struct iwreq iwr;
	
	if(ioctl_sock < 0)
		return -1;
		
	if((buf = malloc(len)) == NULL)
		return -1;
	
	memset(buf, 0,len);
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, WSC_IOCTL_IF, IFNAMSIZ);
	iwr.u.data.flags = oid;
	iwr.u.data.flags |= OID_GET_SET_TOGGLE;
		
	if (data)
		memcpy(buf, data, len);

	iwr.u.data.pointer = (caddr_t) buf;
	iwr.u.data.length = len;

	if (ioctl(ioctl_sock, RT_PRIV_IOCTL, &iwr) < 0) {
		DBGPRINTF(RT_DBG_INFO, "%s: oid=0x%x len (%d) failed", __FUNCTION__, oid, len);
		free(buf);
		return -1;
	}
	free(buf);
	return 0;
}


/******************************************************************************
 * wsc_get_oid
 *
 * Description: 
 *       Get Wsc Message from kernel space by ioctl function call.
 *
 * Parameters:
 *    uint16 oid - The ioctl flag type for the ioctl function call.
 *    char *data - The data pointer want to receive msg from ioctl.
 *    int len    - The length of returned data.
 *  
 * Return Value:
 *    Indicate if the ioctl success or failure.
 *    	-1 = failure.
 *    	0  = If success. 
 *****************************************************************************/
int wsc_get_oid(uint16 oid, char *data, int *len)
{
	struct iwreq iwr;

	if(ioctl_sock < 0 || data == NULL)
		return -1;
	
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, WSC_IOCTL_IF, IFNAMSIZ);
	iwr.u.data.flags = oid;

	iwr.u.data.pointer = (caddr_t)data;
	if (ioctl(ioctl_sock, RT_PRIV_IOCTL, &iwr) < 0) {
		DBGPRINTF(RT_DBG_ERROR, "%s: oid=0x%x failed", __FUNCTION__, oid);
		return -1;
	}
	*len = iwr.u.data.length;
	
	return 0;
}


/******************************************************************************
 * wscU2KModuleInit
 *
 * Description: 
 *       Initialize the U2K Message subsystem(i.e. it's ioctl sub-system now).
 *
 * Parameters:
 *    void
 *  
 * Return Value:
 *    The created ioctl socket id or -1 to indicate if the initialize success or failure.
 *    	-1 		  		 = failure.
 *    	ioctl socket id  = If success. 
 *****************************************************************************/
int wscU2KModuleInit(void)
{
	int s;
	struct ifreq ifr;
	
	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		DBGPRINTF(RT_DBG_ERROR, "ioctl socket create failed! errno=%d!\n", errno);
		return -1;
	}
	/* do it */
	strncpy(ifr.ifr_name, WSC_IOCTL_IF, IFNAMSIZ);

	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
	{
		DBGPRINTF(RT_DBG_ERROR, "ioctl to get the interface(%s) index failed! errno=%d!\n" , ifr.ifr_name, errno);
		return -1;
	}

	return s;

}


