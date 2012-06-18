
#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include "../dsl_define.h"

#include "ra_ioctl.h"
#include "ra_reg_rw.h"

static int esw_fd;

int switch_rcv_okt(unsigned char* prcv_buf, unsigned short max_rcv_buf_size, unsigned short* prcv_buf_len)
{
    struct ifreq ifr;
    unsigned char ioctl_read_buf[MAX_TC_RESP_BUF_LEN+2];
#if DSL_N55U == 1
    strncpy(ifr.ifr_name, "eth2", 5);
#else    
	#error "new model"
#endif    
    ifr.ifr_data = ioctl_read_buf;
    if (-1 == ioctl(esw_fd, RAETH_GET_TC_RESP, &ifr))
    {
        perror("ioctl");
        return -1;
    }
							
    unsigned short* pWord;
    pWord=(unsigned short*)(ioctl_read_buf);
    unsigned short pkt_len;
    pkt_len = *pWord++;
    *prcv_buf_len = 0;    
    
    if (pkt_len > 0)
    {
        if (max_rcv_buf_size >= pkt_len)
        {
            memcpy(prcv_buf, pWord,pkt_len);
            *prcv_buf_len = pkt_len;
        }
    }
    
    return 0;    							
}							

int
switch_init(void)
{
    esw_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (esw_fd < 0)
    {
        perror("socket");
        return -1;
    }
    return 0;
}

void
switch_fini(void)
{
    close(esw_fd);
}


#endif
