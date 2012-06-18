
#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include "../dsl_define.h"

#include "ra_ioctl.h"
#include "ra_reg_rw_ate.h"


static int esw_fd = 0;

int switch_bridge_mode()
{
    char ioctl_read_buf[1];
    struct ifreq ifr;
#if DSL_N55U == 1
    strncpy(ifr.ifr_name, "eth2", 5);
#else
	#error "new model"
#endif
    ifr.ifr_data = ioctl_read_buf;
    if (-1 == ioctl(esw_fd, RAETH_START_BRIDGE_MODE, &ifr))
    {
        perror("ioctl");
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
    if (esw_fd !=0)
    {
        close(esw_fd);
    }        
}


#endif
