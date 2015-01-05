/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
 *
 */ 

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <getopt.h>
#include <errno.h>
#include <err.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <fcntl.h>
#include "busybox.h"
#include "athrs_ctrl.h"
#define MAX_SIZ 10

struct eth_cfg_params {
     int  cmd;
     char    ad_name[IFNAMSIZ];      /* if name, e.g. "eth0" */
     uint16_t vlanid;
     uint16_t portnum;
     uint32_t phy_reg;
     uint32_t tos;
     uint32_t val;
     uint8_t duplex;
     uint8_t  mac_addr[6];
     struct rx_stats rxcntr;
     struct tx_stats txcntr;
     struct tx_mac_stats txmac;
     struct rx_mac_stats rxmac;
     phystats phy_st;

};

struct ifreq ifr;
struct eth_cfg_params etd;
int s,opt_force = 0,duplex = 1;
const char *progname;
static void rx_stats(void)
{
        //printf ("\n\n%s\n", __func__);
        printf ("\t%d\t port%d :Rx bcast cntr\n", etd.rxcntr.rx_broad, etd.portnum);
        printf ("\t%d\t port%d :Rx pause cntr\n", etd.rxcntr.rx_pause, etd.portnum);
        printf ("\t%d\t port%d :Rx multi frames rcvd\n", etd.rxcntr.rx_multi, etd.portnum);
        printf ("\t%d\t port%d :Rx fcs err cntr\n", etd.rxcntr.rx_fcserr,  etd.portnum);
        printf ("\t%d\t port%d :Rx allign err cntr\n", etd.rxcntr.rx_allignerr, etd.portnum);
        printf ("\t%d\t port%d :Rx runt cntr \n", etd.rxcntr.rx_runt, etd.portnum);
        printf ("\t%d\t port%d :Rx fragment cntr\n", etd.rxcntr.rx_frag, etd.portnum);
        printf ("\t%d\t port%d :Rx 64b byte cntr\n", etd.rxcntr.rx_64b, etd.portnum);
        printf ("\t%d\t port%d :Rx 128b byte cntr\n", etd.rxcntr.rx_128b, etd.portnum);
        printf ("\t%d\t port%d :Rx 256b byte cntr\n", etd.rxcntr.rx_256b, etd.portnum);
        printf ("\t%d\t port%d :Rx 512b byte cntr\n", etd.rxcntr.rx_512b, etd.portnum);
        printf ("\t%d\t port%d :Rx 1024b byte cntr\n", etd.rxcntr.rx_1024b, etd.portnum);
        printf ("\t%d\t port%d :Rx 1518b byte cntr\n ", etd.rxcntr.rx_1518b, etd.portnum);
        printf ("\t%d\t port%d :Rx total pkt rcvd\n", (etd.rxcntr.rx_64b + etd.rxcntr.rx_128b + etd.rxcntr.rx_256b +
                                etd.rxcntr.rx_512b + etd.rxcntr.rx_1024b + etd.rxcntr.rx_1518b), etd.portnum);
        printf ("\t%d\t port%d :Rx maxb cntr\n", etd.rxcntr.rx_maxb, etd.portnum);
        printf ("\t%d\t port%d :Rx too long cntr\n", etd.rxcntr.rx_tool, etd.portnum);
        printf ("\t%d\t port%d :Rx byte_l\n", etd.rxcntr.rx_goodbl, etd.portnum);
        printf ("\t%d\t port%d :Rx byte_h\n", etd.rxcntr.rx_goodbh, etd.portnum);
        printf ("\t%d\t port%d :Rx overflow cntr\n", etd.rxcntr.rx_overflow, etd.portnum);
        printf ("\t%d\t port%d :Rx bad byte_l cntr\n", etd.rxcntr.rx_badbl, etd.portnum);
        printf ("\t%d\t port%d :Rx bad byte_u cntr\n", etd.rxcntr.rx_badbu, etd.portnum);
}

static void tx_stats(void)
{
        printf ("\n\n%s\n", __func__);
        printf ("\t%d\t port%d : Tx bcast cntr \n", etd.txcntr.tx_broad, etd.portnum);
        printf ("\t%d\t port%d : Tx pause cntr\n", etd.txcntr.tx_pause, etd.portnum);
        printf ("\t%d\t port%d : Tx multi cntr\n", etd.txcntr.tx_multi, etd.portnum);
        printf ("\t%d\t port%d : Tx under run cntr\n", etd.txcntr.tx_underrun, etd.portnum);
        printf ("\t%d\t port%d : Tx 64b byte cntr\n", etd.txcntr.tx_64b, etd.portnum);
        printf ("\t%d\t port%d : Tx 128b byte cntr\n", etd.txcntr.tx_128b, etd.portnum);
        printf ("\t%d\t port%d : Tx 256b byte cntr\n", etd.txcntr.tx_256b, etd.portnum);
        printf ("\t%d\t port%d : Tx 512b byte cntr\n", etd.txcntr.tx_512b, etd.portnum);
        printf ("\t%d\t port%d : Tx 1024b byte cntr\n", etd.txcntr.tx_1024b, etd.portnum);
        printf ("\t%d\t port%d : Tx 1518b byte cntr\n", etd.txcntr.tx_1518b, etd.portnum);
        printf ("\t%d\t port%d : Tx total pkt txmtd cntr\n", (etd.txcntr.tx_64b + etd.txcntr.tx_128b
                                      + etd.txcntr.tx_256b+ etd.txcntr.tx_512b + etd.txcntr.tx_1024b
                                      + etd.txcntr.tx_1518b), etd.portnum);
        printf ("\t%d\t port%d : Tx max byte cntr\n", etd.txcntr.tx_maxb, etd.portnum);
        printf ("\t%d\t port%d : Tx oversize \n", etd.txcntr.tx_oversiz, etd.portnum);
        printf ("\t%d\t port%d : Tx byte _l \n", etd.txcntr.tx_bytel, etd.portnum);
        printf ("\t%d\t port%d : Tx byte _h \n", etd.txcntr.tx_byteh, etd.portnum);
        printf ("\t%d\t port%d : Tx collision err cntr\n", etd.txcntr.tx_collision, etd.portnum);
        printf ("\t%d\t port%d : Tx abort collision err cntr\n", etd.txcntr.tx_abortcol, etd.portnum);
        printf ("\t%d\t port%d : Tx multi collision err cntr\n", etd.txcntr.tx_multicol, etd.portnum);
        printf ("\t%d\t port%d : Tx single collision err cntr\n", etd.txcntr.tx_singalcol, etd.portnum);
        printf ("\t%d\t port%d : Tx exec deffer err cntr\n", etd.txcntr.tx_execdefer, etd.portnum);
        printf ("\t%d\t port%d : Tx defer err cntr\n", etd.txcntr.tx_defer, etd.portnum);
        printf ("\t%d\t port%d : Tx late collision err cntr\n", etd.txcntr.tx_latecol, etd.portnum);

}
static void tx_mac_stats(void)
{
        printf ("\n\n%s\n", __func__);
        printf ("\t%d\t : Tx pkt cntr\n", etd.txmac.pkt_cntr);
        printf ("\t%d\t : Tx byte cntr\n", etd.txmac.byte_cntr);
        printf ("\t%d\t : Tx mcast pkt cntr\n", etd.txmac.mcast_cntr);
        printf ("\t%d\t : Tx bcast pkt cntr\n", etd.txmac.bcast_cntr);
        printf ("\t%d\t : Tx pause frame pkt cntr\n", etd.txmac.pctrlframe_cntr);
        printf ("\t%d\t : Tx deferal pkt cntr\n", etd.txmac.deferal_cntr);
        printf ("\t%d\t : Tx excessive deferal pkt cntr\n", etd.txmac.excess_deferal_cntr);
        printf ("\t%d\t : Tx single collision pkt cntr\n", etd.txmac.single_col_cntr);
        printf ("\t%d\t : Tx multiple collision pkt cntr\n", etd.txmac.multi_col_cntr);
        printf ("\t%d\t : Tx late collision pkt cntr\n", etd.txmac.late_col_cntr);
        printf ("\t%d\t : Tx excessive collison pkt cntr\n", etd.txmac.excess_col_cntr);
        printf ("\t%d\t : Tx total collison pkt cntr\n", etd.txmac.total_col_cntr);
        printf ("\t%d\t : Tx drop frame cntr\n", etd.txmac.dropframe_cntr);
        printf ("\t%d\t : Tx jabber frame cntr\n", etd.txmac.jabberframe_cntr);
        printf ("\t%d\t : Tx fcs err cntr\n", etd.txmac.fcserr_cntr);
        printf ("\t%d\t : Tx control frame cntr\n", etd.txmac.ctrlframe_cntr);
        printf ("\t%d\t : Tx oversize frame cntr\n", etd.txmac.oz_frame_cntr);
        printf ("\t%d\t : Tx undersize frame cntr\n", etd.txmac.us_frame_cntr);
        printf ("\t%d\t : Tx fragments frame cntr\n", etd.txmac.frag_frame_cntr);

}
static void rx_mac_stats (void)
{
        printf ("\n\n%s\n", __func__);
        printf ("\t%d\t: Rx byte cntr\n", etd.rxmac.byte_cntr);
        printf ("\t%d\t: Rx pkt cntr\n", etd.rxmac.pkt_cntr);
        printf ("\t%d\t: Rx fcs err cntr\n", etd.rxmac.fcserr_cntr);
        printf ("\t%d\t: Rx mcast pkt cntr\n", etd.rxmac.mcast_cntr);
        printf ("\t%d\t: Rx bcast pkt cntr\n", etd.rxmac.bcast_cntr);
        printf ("\t%d\t: Rx ctrl frame cntr\n", etd.rxmac.ctrlframe_cntr);
        printf ("\t%d\t: Rx pause frame pkt cntr\n", etd.rxmac.pausefr_cntr);
        printf ("\t%d\t: Rx unknown opcode cntr\n", etd.rxmac.unknownop_cntr);
        printf ("\t%d\t: Rx alignment err cntr\n", etd.rxmac.allignerr_cntr);
        printf ("\t%d\t: Rx frame length err cntr\n", etd.rxmac.framelerr_cntr);
        printf ("\t%d\t: Rx code err cntr\n", etd.rxmac.codeerr_cntr);
        printf ("\t%d\t: Rx carrier sense err cntr\n", etd.rxmac.carriersenseerr_cntr);
        printf ("\t%d\t: Rx under sz pkt cntr\n", etd.rxmac.underszpkt_cntr);
        printf ("\t%d\t: Rx over sz pkt cntr\n", etd.rxmac.ozpkt_cntr);
        printf ("\t%d\t: Rx fragment cntr\n", etd.rxmac.fragment_cntr);
        printf ("\t%d\t: Rx jabber cntr\n", etd.rxmac.jabber_cntr);
        printf ("\t%d\t: RX drop cntr\n",etd.rxmac.rcvdrop_cntr);
        printf ("\t%u\t: Rx overfl cntr\n",etd.rxmac.rxoverfl);

}



u_int32_t
regread(u_int32_t phy_reg,u_int16_t portno)
{

	etd.phy_reg = phy_reg;
	etd.cmd     = ATHR_PHY_RD;
	etd.portnum = portno;
	if (ioctl(s,ATHR_PHY_CTRL_IOC, &ifr) < 0)
        	err(1, etd.ad_name);
    	return etd.val;
}
static void athr_en_jumboframe(int value)
{
	etd.cmd = ATHR_JUMBO_FRAME;
	etd.val=value;
	if (ioctl(s,ATHR_GMAC_CTRL_IOC, &ifr) < 0)
        	err(1,etd.ad_name);
}
static void athr_set_framesize(int sz)
{
	etd.cmd = ATHR_FRAME_SIZE_CTL;
    	etd.val = sz;
    	if (ioctl(s,ATHR_GMAC_CTRL_IOC, &ifr) < 0)
        	err(1,etd.ad_name);
  
}
static void athr_commit_acl_rules(void)
{
	etd.cmd = ATHR_ACL_COMMIT;
    	if (ioctl(s,ATHR_HW_ACL_IOC,&ifr) < 0)
        	 err(1,etd.ad_name);
  
}
static void athr_flush_acl_rules(void)
{
	etd.cmd = ATHR_ACL_FLUSH;
    	if (ioctl(s,ATHR_HW_ACL_IOC,&ifr) < 0)
        	err(1,etd.ad_name);
}
static void athr_flow_link (int portno, int val)
{
	etd.cmd = ATHR_FLOW_LINK_EN;
	etd.val = val;
	etd.portnum = portno;
	if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
	       	err(1,etd.ad_name);
}
static void athr_txflctrl (int portno, int val)
{
	if (portno == 0x3f) {
		etd.val = val; 
        	etd.cmd = ATHR_GMAC_TX_FLOW_CTRL; 
        if (ioctl (s, ATHR_GMAC_CTRL_IOC, &ifr) < 0)
        	printf("%s ioctl error\n",__func__);
            
    	} else { 
        	etd.cmd		= ATHR_PHY_TXFCTL;
        	etd.portnum 	= portno;
		etd.val		= val; 
        if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
            printf("%s ioctl error\n",__func__);
              
    	}
    
       
}
static void athr_gmac_flow_ctrl(int val)
{
	etd.val = val;
        etd.cmd = ATHR_GMAC_FLOW_CTRL;
        if (ioctl (s, ATHR_GMAC_CTRL_IOC, &ifr) < 0)
             printf("%s ioctl error\n",__func__);
	
}
static void athr_phy_flow_ctrl(int val, int portno)
{
	etd.val = val;
        etd.cmd = ATHR_PHY_FLOW_CTRL;
   	etd.portnum = portno;
        if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
             printf("%s ioctl error\n",__func__);
}
static void athr_rxflctrl (int portno, int val)
{
	if (portno == 0x3f) {
		etd.val = val; 
        	etd.cmd = ATHR_GMAC_RX_FLOW_CTRL; 
        	if (ioctl (s, ATHR_GMAC_CTRL_IOC, &ifr) < 0)
            		printf("%s ioctl error\n",__func__);
            
    	} else { 
        	etd.cmd	    = ATHR_PHY_RXFCTL;
        	etd.portnum = portno;
		etd.val	    = val; 
        if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
            printf("%s ioctl error\n",__func__);
              
    }
    
       
}
static void athr_set_mib(int val)
{
	etd.cmd       = ATHR_PHY_MIB;
        etd.portnum = 0x3f;
	etd.val       = val;
	if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
        	err(1, etd.ad_name);
}
static void athr_disp_stats(int portno)
{
	if (portno == 0x3f) {
		etd.cmd = ATHR_GMAC_STATS;
       		if (ioctl (s, ATHR_GMAC_CTRL_IOC, &ifr) < 0){
           	err(1, etd.ad_name);
		} else {
			rx_mac_stats ();
           		tx_mac_stats ();

	}
       
   	} else {
       		etd.cmd = ATHR_PHY_STATS;
       		etd.portnum = portno;
       		if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
           		err(1, etd.ad_name);
        	else{
            		rx_stats ();
            		tx_stats ();

       		}
   	}
	
}
static void athr_dma_check(int val)
{
	etd.cmd = ATHR_GMAC_DMA_CHECK;
	etd.val = val;
    	if (ioctl (s, ATHR_GMAC_CTRL_IOC, &ifr) < 0)
		err(1, etd.ad_name);
    
}

static void athr_set_qos(int val)
{
	etd.cmd = ATHR_QOS_ETH_SOFT_CLASS;
	etd.val = val;
	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
       		err (1, etd.ad_name);
}
static void athr_set_port_pri(int portno, int val)
{
	etd.cmd     = ATHR_QOS_ETH_PORT;
   	etd.portnum = portno;
   	etd.val     = val;
   	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
       		err (1, etd.ad_name);
      
}
static void athr_ip_qos (int tos, int val)
{
	etd.cmd = ATHR_QOS_ETH_IP;
   	etd.val = val;
   	etd.tos = tos;
   	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
        	err (1, etd.ad_name);
       
}
static void athr_vlan_qos (int vlan_id, int val)
{
	etd.cmd    = ATHR_QOS_ETH_VLAN;
   	etd.val    = val;
   	etd.vlanid = vlan_id;
   	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
       		err (1, etd.ad_name);
      
}
static void athr_mac_qos(int portno, int val, char *mac_addr)
{
	etd.cmd     = ATHR_QOS_ETH_DA;
   	etd.val     = val;
   	etd.portnum = portno;
   	sscanf (mac_addr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
              &etd.mac_addr[0], &etd.mac_addr[1],
              &etd.mac_addr[2], &etd.mac_addr[3],
              &etd.mac_addr[4], &etd.mac_addr[5]);
   	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
       		err (1, etd.ad_name);
       
}

static int athr_port_st(int portno)
{
        char str[][MAX_SIZ] = {"10Mbps","100Mbps","1000Mbps"};

	etd.cmd     = ATHR_PORT_STATS;
   	etd.portnum = portno;
   	if (etd.portnum > 5){
       		printf ("port usage <0-5>");
       		return -EINVAL;
   	}
   	if (ioctl (s, ATHR_PHY_CTRL_IOC, &ifr) < 0)
       		err (1, etd.ad_name);
        printf("\t\t\t____phy%d stats____\n",etd.portnum);
        printf("Link:\t\t%s\n",etd.phy_st.link ? "alive":"Not alive");
        printf("Speed:\t\t%s\n",str[etd.phy_st.speed]);
        printf("Duplex:\t\t%s\n",etd.phy_st.duplex ? "Full duplex":"Half-duplex");
        printf("Rx flowctrl:\t%s\n",etd.phy_st.rxflctrl ? "Enabled": "Disabled");
        printf("Tx flowctrl:\t%s\n",etd.phy_st.txflctrl ? "Enabled": "Disabled");
   	return 0;
   
}
static void athr_process_egress(int portno, int val)
{
	etd.cmd =  ATHR_QOS_PORT_ELIMIT;
	etd.val = val;
	etd. portnum  = portno;
	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
		err (1, etd.ad_name);
       
}
static void athr_process_igress(int portno, int val)
{
	etd.cmd =  ATHR_QOS_PORT_ILIMIT;
   	etd.val = val;
   	etd. portnum  = portno;
   	if (ioctl (s, ATHR_GMAC_QOS_CTRL_IOC, &ifr) < 0)
       	     	err (1, etd.ad_name);
       
}


static void regwrite(u_int32_t phy_reg,u_int32_t val,u_int16_t portno)
{
        
	etd.val     = val;
	etd.phy_reg = phy_reg;
        etd.portnum = portno;
        if(opt_force)  {
             etd.duplex   = duplex;
             etd.cmd      = ATHR_PHY_FORCE;
	    if (ioctl(s,ATHR_PHY_CTRL_IOC, &ifr) < 0)
		err(1, etd.ad_name);
            opt_force = 0;
        }
        else {
            etd.cmd = ATHR_PHY_WR;
	    if (ioctl(s,ATHR_PHY_CTRL_IOC, &ifr) < 0)
		err(1, etd.ad_name);
        }
}

static void usage(void)
{
	fprintf(stderr, "usage: %s [-i ifname] [-p portnum] offset[=value]\n", progname);
	fprintf(stderr, "usage: %s [-f]  -p portnum =10/100/0 [-d duplex]\n", progname);
        fprintf(stderr, "usage: %s [-i ifname][-x]\n", progname);
        fprintf(stderr, "usage: %s [-i ifname][-c]\n", progname);
        fprintf(stderr, "usage: %s [-i ifname][-s value]\n", progname);
        fprintf(stderr, "usage: %s [-i ifname][-j 0|1]\n", progname);
        fprintf(stderr, "usage: %s [--txfctl] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--txfctl] [-i ifname] -v [0|1] -p <portno>\n", progname);
        fprintf(stderr, "usage: %s [--rxfctl] [-i ifname] -v [0|1]\n",progname);
        fprintf(stderr, "usage: %s [--rxfctl] [-i ifname] -v [0|1] -p <portno>\n", progname);
        fprintf(stderr, "usage: %s [--macfl] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--swfl] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--dma] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--f_link] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--mib] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--stats] [-i ifname]\n", progname);
        fprintf(stderr, "usage: %s [--stats] [-i ifname] -p <portno>\n", progname);
        fprintf(stderr, "usage: %s [--qos] [-i ifname] -v [0|1]\n", progname);
        fprintf(stderr, "usage: %s [--ipqos] [-i ifname] -t <tos> -v <val>\n", progname);
        fprintf(stderr, "usage: %s [--vqos] [-i ifname] -l <vlanid> -v <val>\n", progname);
        fprintf(stderr, "usage: %s [--mqos [-i ifname] -v <val> -p <portnum> -m <macaddr>\n", progname);
        fprintf(stderr, "usage: %s [--p_st] [-i ifname] -p <portno>\n", progname);
        fprintf(stderr, "usage: %s [--igrl] [-i ifname] -p <portno> -v <val>\n", progname);
        fprintf(stderr, "usage: %s [--egrl] [-i ifname] -p <portno> -v <val>\n", progname);
        fprintf(stderr, "usage: %s [-i ifname][-s value]\n",progname);
        fprintf(stderr, "usage: %s [-i ifname][-j 0|1]\n",progname);
        exit(-1);
}


int
ethreg_main(int argc, char *argv[])
{
	const char *ifname = "eth0";
	int c,portnum = 0x3f,cmd = 0,value = -1;
        int optionindex = 0;
        int vlanid = 0;
        char *mac = NULL;
        int tos = -1;
        char *opt = "xfhci:d:s:j:v:t:p:m:l:";

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");

        opt_force = 0;
	progname = argv[0];
        

	struct option long_options[] =
        {
            { "f_link", no_argument, 0, ATHR_FLOW_LINK_EN},
            { "txfctl", no_argument, 0, ATHR_PHY_TXFCTL},
            { "rxfctl", no_argument, 0, ATHR_PHY_RXFCTL},
            { "stats" , no_argument, 0, ATHR_GMAC_STATS},
            { "mib"   , no_argument, 0, ATHR_PHY_MIB},
            { "dma"   , no_argument, 0, ATHR_GMAC_DMA_CHECK},
            { "qos"   , no_argument, 0, ATHR_QOS_ETH_SOFT_CLASS},
            { "ppri"  , no_argument, 0, ATHR_QOS_ETH_PORT},
            { "ipqos" , no_argument, 0, ATHR_QOS_ETH_IP},
            { "vqos"  , no_argument, 0, ATHR_QOS_ETH_VLAN},
            { "mqos"  , no_argument, 0, ATHR_QOS_ETH_DA},
            { "igrl"  , no_argument, 0, ATHR_QOS_PORT_ELIMIT},
            { "egrl"  , no_argument, 0, ATHR_QOS_PORT_ILIMIT},
            { "p_st"  , no_argument, 0, ATHR_PORT_STATS},
            { "macfl" , no_argument, 0, ATHR_GMAC_FLOW_CTRL},
            { "swfl"  , no_argument, 0, ATHR_PHY_FLOW_CTRL},
            { 0,0,0,0}
       	};
          
           
	while ((c = getopt_long(argc, argv,
                    opt, long_options, &optionindex)) != -1) { 
	switch (c) {
        	case ATHR_FLOW_LINK_EN:
                	cmd = ATHR_FLOW_LINK_EN;
                        break;
        	case ATHR_PHY_TXFCTL:
                        cmd = ATHR_PHY_TXFCTL;
                        break;
                case ATHR_PHY_RXFCTL:
                        cmd = ATHR_PHY_RXFCTL;
                        break;
                case ATHR_PHY_MIB:
                        cmd = ATHR_PHY_MIB;
                        break;
                case ATHR_GMAC_STATS:
                        cmd = ATHR_GMAC_STATS;
                        break;
                case ATHR_GMAC_DMA_CHECK:
                        cmd = ATHR_GMAC_DMA_CHECK;
                        break;
                case ATHR_QOS_ETH_SOFT_CLASS:
                        cmd = ATHR_QOS_ETH_SOFT_CLASS;
                        break;
                case ATHR_QOS_ETH_PORT:
                        cmd = ATHR_QOS_ETH_PORT;
                        break;
                case ATHR_QOS_ETH_VLAN:
                        cmd = ATHR_QOS_ETH_VLAN;
                        break;
                case ATHR_QOS_ETH_IP:
                        cmd = ATHR_QOS_ETH_IP;
                        break;
                case ATHR_QOS_ETH_DA:
                        cmd = ATHR_QOS_ETH_DA;
                        break; 
                case ATHR_PORT_STATS:
                        cmd = ATHR_PORT_STATS;
                        break;
                case ATHR_QOS_PORT_ELIMIT:
                        cmd = ATHR_QOS_PORT_ELIMIT;
                        break;
                case ATHR_QOS_PORT_ILIMIT:
                        cmd = ATHR_QOS_PORT_ILIMIT;
                        break;
                case ATHR_GMAC_FLOW_CTRL:
                        cmd = ATHR_GMAC_FLOW_CTRL;
                        break;
                case ATHR_PHY_FLOW_CTRL:
                        cmd = ATHR_PHY_FLOW_CTRL;
                        break;
                case 'm':
                        mac = optarg;
                        break;
                case 'v':
                        value = strtoul(optarg, 0, 0);
                        break;
		case 'i':
			ifname = optarg;
			break;
                case 't':
                        tos = strtoul(optarg, 0, 0);
                        break;
		case 'p':
                        portnum = strtoul(optarg, 0, 0);
			break;
		case 'f':
			opt_force = 1;
			break;
                case 'd':
			duplex = strtoul(optarg, 0, 0);
			break;
                case 'c':
			cmd = ATHR_ACL_COMMIT;
                        break;
                case 'x':
			cmd = ATHR_ACL_FLUSH;
                        break;
                case 's':
                        cmd = ATHR_FRAME_SIZE_CTL;
                        value = strtoul(optarg, 0, 0);
                        break;
		case 'j':
                        cmd = ATHR_JUMBO_FRAME;
                        value = strtoul(optarg, 0, 0);
                        break;
                case 'l':
                        vlanid = strtoul (optarg, 0, 0);
                        break;
                case 'h':
                        usage();
                        break;
		default:
			usage();
			/*NOTREACHED*/
		}

        }

	argc -= optind;
	argv += optind;
	strncpy(etd.ad_name, ifname, sizeof (etd.ad_name));
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
        ifr.ifr_data = (void *) &etd;

        if (cmd == ATHR_ACL_COMMIT) {
        	athr_commit_acl_rules();
              	return 0;
        }
        else if (cmd == ATHR_ACL_FLUSH) {
              	athr_flush_acl_rules();
              	return 0;
        }
        else if(cmd == ATHR_FRAME_SIZE_CTL) {
        	if (value == -1) {
                	printf ("usage:ethreg -i <if_name> -s <val>\n");
                  	return -1; 
              	} else {
	          	athr_set_framesize(value);
              	}
              	return 0;
        }
        else if (cmd == ATHR_JUMBO_FRAME) {
        	if (value == -1) {
                	printf ("usage: ethreg -i <if_name> -j <0|1>\n");
                	return -1;
             	} else {
	        	athr_en_jumboframe(value);
             	}
             return 0;
	}
        else if (cmd == ATHR_FLOW_LINK_EN) {
        	if (value == -1 || portnum == 0x3f) {
                	printf ("usage: ethreg --f_link -i <ifname> -p <portnum> -v 1\n");
                 	return -1;
             	} else {
                	athr_flow_link(portnum, value);
             	}
             	return 0;
        }
        else if (cmd == ATHR_PHY_RXFCTL) {
        	if (value == -1) {
                	printf ("usage: ethreg --rxfctl -i <ifname> -p <portnum> -v [0|1]\n");
                	printf ("usage: ethreg --rxfctl -i <ifname> -v [0|1]\n");
                 	return -1;
             	} else {
                 	athr_rxflctrl(portnum, value);
             	}
             	return 0;
        }
        else if (cmd == ATHR_PHY_TXFCTL) {
        	if (value == -1) {
                	printf ("usage: ethreg --txfctl -i <ifname> -p <portnum> -v [0|1]\n");
                	printf ("usage: ethreg --txfctl -i <ifname> -v [0|1]\n");
                 	return -1;
             	} else {
                	 athr_txflctrl(portnum, value);
             	}
             	return 0;
        }
        else if (cmd == ATHR_PHY_MIB) {
        	if (value == -1) {
        		printf ("usage: ethreg --mib -i <ifname> -v 1\n");
		 	return -1;
             	} else {
                	athr_set_mib(value);
             	}
             	return 0;
        }
	else if (cmd == ATHR_GMAC_STATS) {
        	athr_disp_stats(portnum);
	     	return 0;
	}
	else if (cmd == ATHR_GMAC_DMA_CHECK) {
                if (value == -1) {
                	printf ("usage: ethreg --dma -i <ifname> -v [0|1]\n");
                        return -1;
                } else {
        		athr_dma_check(value);
                }
             	return 0;
        }
        else if (cmd == ATHR_QOS_ETH_SOFT_CLASS) {
        	if (value == -1) {
                	printf ("usage: ethreg --qos -i <ifname> -v [0|1]\n");
                	return -1;
             	} else {
                	athr_set_qos(value);
             	}
             	return 0;
               
        }
        else if (cmd == ATHR_QOS_ETH_PORT) {
        	if (value == -1) {
                	printf("usage: ethreg --ppri -i <ifname> -p <portno> -v <val>\n");
                	return -1;
             	} else {
                 	athr_set_port_pri(portnum, value);
             	}
             	return 0;
        }
        else if (cmd == ATHR_QOS_ETH_IP) {
        	if (tos == -1 || value == -1) {
                 	printf ("usage: ethreg --ipqos -i <ifname> -v <val> -t <tos>\n");
                 	return -1;
             	} else {
                 	athr_ip_qos(tos, value);
             	}
             	return 0;
        }
        else if (cmd == ATHR_QOS_ETH_VLAN) {
        	if (value == -1 || vlanid == -1) {
                	printf ("usage: ethreg --vqos -i <ifname> -v <val> -l <vlanid>\n");
                 	return -1;
             	} else {
                	athr_vlan_qos(vlanid, value);
                 
             	}
             	return 0;
        }
        else if (cmd == ATHR_QOS_ETH_DA) {
        	if (portnum == 0x3f || value == -1 || mac == NULL) {
                	printf ("usage: ethreg --mqos -i <ifname> -v <val> -p <portnum> -m <macaddr>\n");
                 	return -1;
             	} else {
                	athr_mac_qos(portnum, value, mac);
             	}
             	return 0;
        }
        else if (cmd == ATHR_PORT_STATS) {
        	if (portnum == 0x3f) {
                	printf ("usage: ethreg --port_st -i <ifname> -p <portno>\n");
			return -1;
             	} else {
                	athr_port_st(portnum);
             	}
             	return 0;
        }
        else if (cmd == ATHR_QOS_PORT_ELIMIT) {
        	if (portnum == 0x3f || value == -1 ) {
                	printf("usage: ethreg --egrl -i <ifname> -p <portnum> -v <val>\n");
                 	return -1;
             	} else {
                	athr_process_egress(portnum, value);
             	}
             	return 0;
       	}
       	else if (cmd == ATHR_QOS_PORT_ILIMIT) {
       		if (portnum == 0x3f || value == -1 ) {
                	printf("usage: ethreg --igrl -i <ifname> -p <portnum> -v <val>\n");
                 	return -1;
             	} else {
                 	athr_process_igress(portnum, value);
             	}
             	return 0;
	}
        else if (cmd == ATHR_GMAC_FLOW_CTRL){
        	athr_gmac_flow_ctrl(value);
                return 0;
        }
        else if (cmd == ATHR_PHY_FLOW_CTRL){
        	athr_phy_flow_ctrl(value, portnum);
                return 0;
        }

	for (; argc > 0; argc--, argv++) {
		u_int32_t   off;
                u_int32_t  val, oval;
                char *cp;

                cp = strchr(argv[0], '=');

                if (cp != NULL)
                        *cp = '\0';

                off = (u_int) strtoul(argv[0], 0, 0);

                if (off == 0 && errno == EINVAL)
               		errx(1, "%s: invalid reg offset %s",
                              progname, argv[0]);

                if (cp == NULL) {
                	val = regread(off,portnum);
                    	printf("Read Reg: 0x%08x = 0x%08x\n",off, val);
                    	return 0;
                } else {
                	val = (u_int32_t) strtoul(cp+1, 0, 0);
                    	if (val == 0 && errno == EINVAL) {
                        	errx(1, "%s: invalid reg value %s",
                                        progname, cp+1);
                    	}
                     	else {
                        	oval = regread(off,portnum);
				if(opt_force == 0) {
                            		printf("Write Reg: 0x%08x: Oldval = 0x%08x Newval = 0x%08x\n", off, oval, val);

                        	} else if(opt_force == 1 && portnum == 0x3f) {
                            		fprintf(stderr, "usage: %s [-f]  -p portnum =10/100/0 [-d duplex]\n", progname);
                            		return -1;
                        	}
                            	regwrite(off,val,portnum);
                     	}	
                }
        }
        return 0;
}

