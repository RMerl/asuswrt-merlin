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

#ifndef ATHRS_CTRL_H
#define ATHRS_CTRL_H

/* Ioctl subroutines */
#define ATHR_GMAC_QOS_CTRL_IOC      (SIOCDEVPRIVATE | 0x01)
#define ATHR_GMAC_CTRL_IOC          (SIOCDEVPRIVATE | 0x02)
#define ATHR_PHY_CTRL_IOC           (SIOCDEVPRIVATE | 0x03)
#define ATHR_VLAN_IGMP_IOC          (SIOCDEVPRIVATE | 0x04)
#define ATHR_HW_ACL_IOC             (SIOCDEVPRIVATE | 0x05)

/* 
 *GMAC_CTRL_IOC_COMMANDS
 */
#define ATHR_GMAC_TX_FLOW_CTRL            ((ATHR_GMAC_CTRL_IOC << 16) | 0x1)
#define ATHR_GMAC_RX_FLOW_CTRL            ((ATHR_GMAC_CTRL_IOC << 16) | 0x2)
#define ATHR_GMAC_DMA_CHECK               ((ATHR_GMAC_CTRL_IOC << 16) | 0x3)
#define ATHR_GMAC_SOFT_LED_BLINK          ((ATHR_GMAC_CTRL_IOC << 16) | 0x4)
#define ATHR_GMAC_SW_ONLY_MODE            ((ATHR_GMAC_CTRL_IOC << 16) | 0x5)
#define ATHR_GMAC_STATS                   ((ATHR_GMAC_CTRL_IOC << 16) | 0x6)
#define ATHR_JUMBO_FRAME                  ((ATHR_GMAC_CTRL_IOC << 16) | 0x7)
#define ATHR_FRAME_SIZE_CTL               ((ATHR_GMAC_CTRL_IOC << 16) | 0x8)
#define ATHR_GMAC_FLOW_CTRL               ((ATHR_GMAC_CTRL_IOC << 16) | 0x9)                   /* GMAC FLOW CTRL*/

#define ATHR_DBG_CONFIG                   ((ATHR_GMAC_CTRL_IOC << 16) | 0xa)   /* To config debug markers */
#define ATHR_DBG_RESTART                  ((ATHR_GMAC_CTRL_IOC << 16) | 0xb)   /* To restart ethdebug */
#define ATHR_DBG_STATS                    ((ATHR_GMAC_CTRL_IOC << 16) | 0xc)   /* For checking current stats */
#define ATHR_DBG_ENABLE                   ((ATHR_GMAC_CTRL_IOC << 16) | 0xd)   /* To enabled and disabled ethdebug */

/*
 *PHY_CTRL_COMMANDS
 */
#define ATHR_PHY_FORCE           ((ATHR_PHY_CTRL_IOC << 16) | 0x1)
#define ATHR_PHY_RD              ((ATHR_PHY_CTRL_IOC << 16) | 0x2)
#define ATHR_PHY_WR              ((ATHR_PHY_CTRL_IOC << 16) | 0x3)
#define ATHR_PHY_MIB             ((ATHR_PHY_CTRL_IOC << 16) | 0X4)
#define ATHR_PHY_STATS           ((ATHR_PHY_CTRL_IOC << 16) | 0X5)
#define ATHR_PORT_STATS          ((ATHR_PHY_CTRL_IOC << 16) | 0X6)
#define ATHR_PORT_LINK           ((ATHR_PHY_CTRL_IOC << 16) | 0x7)
#define ATHR_FLOW_LINK_EN        ((ATHR_PHY_CTRL_IOC << 16) | 0x8)
#define ATHR_PHY_RXFCTL          ((ATHR_PHY_CTRL_IOC << 16) | 0x9)
#define ATHR_PHY_TXFCTL          ((ATHR_PHY_CTRL_IOC << 16) | 0x10)
#define ATHR_PHY_FLOW_CTRL       ((ATHR_PHY_CTRL_IOC << 16) | 0x11)


#define ATHR_PACKET_FLAG         ((ATHR_VLAN_IGMP_IOC << 16) | 0x1)
#define ATHR_VLAN_ADDPORTS       ((ATHR_VLAN_IGMP_IOC << 16) | 0x2)
#define ATHR_VLAN_DELPORTS       ((ATHR_VLAN_IGMP_IOC << 16) | 0x3)
#define ATHR_VLAN_SETTAGMODE     ((ATHR_VLAN_IGMP_IOC << 16) | 0x4)
#define ATHR_VLAN_SETDEFAULTID   ((ATHR_VLAN_IGMP_IOC << 16) | 0x5)
#define ATHR_VLAN_ENABLE         ((ATHR_VLAN_IGMP_IOC << 16) | 0x6)
#define ATHR_VLAN_DISABLE        ((ATHR_VLAN_IGMP_IOC << 16) | 0x7)
#define ATHR_IGMP_ON_OFF         ((ATHR_VLAN_IGMP_IOC << 16) | 0x8)
#define ATHR_LINK_GETSTAT        ((ATHR_VLAN_IGMP_IOC << 16) | 0x9)
#define ATHR_ARL_ADD             ((ATHR_VLAN_IGMP_IOC << 16) | 0xa)
#define ATHR_ARL_DEL             ((ATHR_VLAN_IGMP_IOC << 16) | 0xb)
#define ATHR_MCAST_CLR           ((ATHR_VLAN_IGMP_IOC << 16) | 0xc)
/*
 * GMC_QOS_COMMANDS
 */
#define ATHR_QOS_ETH_SOFT_CLASS   ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x1)
#define ATHR_QOS_ETH_PORT         ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x2)
#define ATHR_QOS_ETH_VLAN         ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x3)
#define ATHR_QOS_ETH_DA           ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x4)
#define ATHR_QOS_ETH_IP           ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x5)
#define ATHR_QOS_PORT_ILIMIT      ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x6)
#define ATHR_QOS_PORT_ELIMIT      ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x7)
#define ATHR_QOS_PORT_EQLIMIT     ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x8)
#define MAX_QOS_COMMAND           ((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x9)

/*
 * ACL COMMANDS
 */
#define ATHR_ACL_COMMIT ((ATHR_HW_ACL_IOC << 16) | 0x1)
#define ATHR_ACL_FLUSH  ((ATHR_HW_ACL_IOC << 16) | 0x2)


struct rx_stats{
        int rx_broad;
        int rx_pause;
        int rx_multi;
        int rx_fcserr;
        int rx_allignerr;
        int rx_runt;
        int rx_frag;
        int rx_64b;
        int rx_128b;
        int rx_256b;
        int rx_512b;
        int rx_1024b;
        int rx_1518b;
        int rx_maxb;
        int rx_tool;
        int rx_goodbl;
        int rx_goodbh;
        int rx_overflow;
        int rx_badbl;
        int rx_badbu;
};

struct tx_stats{
        int tx_broad;
        int tx_pause;
        int tx_multi;
        int tx_underrun;
        int tx_64b;
        int tx_128b;
        int tx_256b;
        int tx_512b;
        int tx_1024b;
        int tx_1518b;
        int tx_maxb;
        int tx_oversiz;
        int tx_bytel;
        int tx_byteh;
        int tx_collision;
        int tx_abortcol;
        int tx_multicol;
        int tx_singalcol;
        int tx_execdefer;
        int tx_defer;
        int tx_latecol;
};

struct tx_mac_stats {

        int pkt_cntr;
        int byte_cntr;
        int mcast_cntr;
        int bcast_cntr;
        int pctrlframe_cntr;
        int deferal_cntr;
        int excess_deferal_cntr;
        int single_col_cntr;
        int multi_col_cntr;
        int late_col_cntr;
        int excess_col_cntr;
        int total_col_cntr;
        int honored_cntr;
        int dropframe_cntr;
        int jabberframe_cntr;
        int fcserr_cntr;
        int ctrlframe_cntr;
        int oz_frame_cntr;
        int us_frame_cntr;
        int frag_frame_cntr;

};

struct rx_mac_stats {

        int byte_cntr;
        int pkt_cntr;
        int fcserr_cntr;
        int mcast_cntr;
        int bcast_cntr;
        int ctrlframe_cntr;
        int pausefr_cntr;
        int unknownop_cntr;
        int allignerr_cntr;
        int framelerr_cntr;
        int codeerr_cntr;
        int carriersenseerr_cntr;
        int underszpkt_cntr;
        int ozpkt_cntr;
        int fragment_cntr;
        int jabber_cntr;
        int rcvdrop_cntr;
        u_int32_t rxoverfl; 
};


typedef struct {
        u_int32_t link       :1;
        u_int32_t speed      :2;
        u_int32_t duplex     :1;
        u_int32_t rxflctrl   :1;
        u_int32_t txflctrl   :1;
}phystats;

#endif       //ATHR_CTRL_H

