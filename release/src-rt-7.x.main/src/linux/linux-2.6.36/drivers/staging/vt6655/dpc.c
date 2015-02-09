/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * File: dpc.c
 *
 * Purpose: handle dpc rx functions
 *
 * Author: Lyndon Chen
 *
 * Date: May 20, 2003
 *
 * Functions:
 *      device_receive_frame - Rcv 802.11 frame function
 *      s_bAPModeRxCtl- AP Rcv frame filer Ctl.
 *      s_bAPModeRxData- AP Rcv data frame handle
 *      s_bHandleRxEncryption- Rcv decrypted data via on-fly
 *      s_bHostWepRxEncryption- Rcv encrypted data via host
 *      s_byGetRateIdx- get rate index
 *      s_vGetDASA- get data offset
 *      s_vProcessRxMACHeader- Rcv 802.11 and translate to 802.3
 *
 * Revision History:
 *
 */

#include "device.h"
#include "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "wroute.h"
#include "hostap.h"
#include "rf.h"
#include "iowpa.h"
#include "aes_ccmp.h"

//#define	PLICE_DEBUG


/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

const unsigned char acbyRxRate[MAX_RATE] =
{2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108};


/*---------------------  Static Functions  --------------------------*/

/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Functions  --------------------------*/

static unsigned char s_byGetRateIdx(unsigned char byRate);


static void
s_vGetDASA(unsigned char *pbyRxBufferAddr, unsigned int *pcbHeaderSize,
		PSEthernetHeader psEthHeader);

static void
s_vProcessRxMACHeader(PSDevice pDevice, unsigned char *pbyRxBufferAddr,
		unsigned int cbPacketSize, bool bIsWEP, bool bExtIV,
		unsigned int *pcbHeadSize);

static bool s_bAPModeRxCtl(
    PSDevice pDevice,
    unsigned char *pbyFrame,
    int      iSANodeIndex
    );



static bool s_bAPModeRxData (
    PSDevice pDevice,
    struct sk_buff* skb,
    unsigned int FrameSize,
    unsigned int cbHeaderOffset,
    int      iSANodeIndex,
    int      iDANodeIndex
    );


static bool s_bHandleRxEncryption(
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    unsigned char *pbyNewRsr,
    PSKeyItem   *pKeyOut,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    );

static bool s_bHostWepRxEncryption(

    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    bool bOnFly,
    PSKeyItem    pKey,
    unsigned char *pbyNewRsr,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16

    );

/*---------------------  Export Variables  --------------------------*/

/*+
 *
 * Description:
 *    Translate Rcv 802.11 header to 802.3 header with Rx buffer
 *
 * Parameters:
 *  In:
 *      pDevice
 *      dwRxBufferAddr  - Address of Rcv Buffer
 *      cbPacketSize    - Rcv Packet size
 *      bIsWEP          - If Rcv with WEP
 *  Out:
 *      pcbHeaderSize   - 802.11 header size
 *
 * Return Value: None
 *
-*/
static void
s_vProcessRxMACHeader(PSDevice pDevice, unsigned char *pbyRxBufferAddr,
		unsigned int cbPacketSize, bool bIsWEP, bool bExtIV,
		unsigned int *pcbHeadSize)
{
    unsigned char *pbyRxBuffer;
    unsigned int cbHeaderSize = 0;
    unsigned short *pwType;
    PS802_11Header  pMACHeader;
    int             ii;


    pMACHeader = (PS802_11Header) (pbyRxBufferAddr + cbHeaderSize);

    s_vGetDASA((unsigned char *)pMACHeader, &cbHeaderSize, &pDevice->sRxEthHeader);

    if (bIsWEP) {
        if (bExtIV) {
            // strip IV&ExtIV , add 8 byte
            cbHeaderSize += (WLAN_HDR_ADDR3_LEN + 8);
        } else {
            // strip IV , add 4 byte
            cbHeaderSize += (WLAN_HDR_ADDR3_LEN + 4);
        }
    }
    else {
        cbHeaderSize += WLAN_HDR_ADDR3_LEN;
    };

    pbyRxBuffer = (unsigned char *) (pbyRxBufferAddr + cbHeaderSize);
    if (!compare_ether_addr(pbyRxBuffer, &pDevice->abySNAP_Bridgetunnel[0])) {
        cbHeaderSize += 6;
    }
    else if (!compare_ether_addr(pbyRxBuffer, &pDevice->abySNAP_RFC1042[0])) {
        cbHeaderSize += 6;
        pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
        if ((*pwType!= TYPE_PKT_IPX) && (*pwType != cpu_to_le16(0xF380))) {
        }
        else {
            cbHeaderSize -= 8;
            pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
            if (bIsWEP) {
                if (bExtIV) {
                    *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 8);    // 8 is IV&ExtIV
                } else {
                    *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 4);    // 4 is IV
                }
            }
            else {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN);
            }
        }
    }
    else {
        cbHeaderSize -= 2;
        pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
        if (bIsWEP) {
            if (bExtIV) {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 8);    // 8 is IV&ExtIV
            } else {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 4);    // 4 is IV
            }
        }
        else {
            *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN);
        }
    }

    cbHeaderSize -= (ETH_ALEN * 2);
    pbyRxBuffer = (unsigned char *) (pbyRxBufferAddr + cbHeaderSize);
    for(ii=0;ii<ETH_ALEN;ii++)
        *pbyRxBuffer++ = pDevice->sRxEthHeader.abyDstAddr[ii];
    for(ii=0;ii<ETH_ALEN;ii++)
        *pbyRxBuffer++ = pDevice->sRxEthHeader.abySrcAddr[ii];

    *pcbHeadSize = cbHeaderSize;
}




static unsigned char s_byGetRateIdx (unsigned char byRate)
{
    unsigned char byRateIdx;

    for (byRateIdx = 0; byRateIdx <MAX_RATE ; byRateIdx++) {
        if (acbyRxRate[byRateIdx%MAX_RATE] == byRate)
            return byRateIdx;
    }
    return 0;
}


static void
s_vGetDASA(unsigned char *pbyRxBufferAddr, unsigned int *pcbHeaderSize,
	PSEthernetHeader psEthHeader)
{
    unsigned int cbHeaderSize = 0;
    PS802_11Header  pMACHeader;
    int             ii;

    pMACHeader = (PS802_11Header) (pbyRxBufferAddr + cbHeaderSize);

    if ((pMACHeader->wFrameCtl & FC_TODS) == 0) {
        if (pMACHeader->wFrameCtl & FC_FROMDS) {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr1[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr3[ii];
            }
        }
        else {
            // IBSS mode
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr1[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr2[ii];
            }
        }
    }
    else {
        // Is AP mode..
        if (pMACHeader->wFrameCtl & FC_FROMDS) {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr3[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr4[ii];
                cbHeaderSize += 6;
            }
        }
        else {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr3[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr2[ii];
            }
        }
    };
    *pcbHeaderSize = cbHeaderSize;
}




//PLICE_DEBUG ->

void	MngWorkItem(void *Context)
{
	PSRxMgmtPacket			pRxMgmtPacket;
	PSDevice	pDevice =  (PSDevice) Context;
	//printk("Enter MngWorkItem,Queue packet num is %d\n",pDevice->rxManeQueue.packet_num);
	spin_lock_irq(&pDevice->lock);
	 while(pDevice->rxManeQueue.packet_num != 0)
	 {
		 pRxMgmtPacket =  DeQueue(pDevice);
        		vMgrRxManagePacket(pDevice, pDevice->pMgmt, pRxMgmtPacket);
	}
	spin_unlock_irq(&pDevice->lock);
}


//PLICE_DEBUG<-



bool
device_receive_frame (
    PSDevice pDevice,
    PSRxDesc pCurrRD
    )
{

    PDEVICE_RD_INFO  pRDInfo = pCurrRD->pRDInfo;
#ifdef	PLICE_DEBUG
	//printk("device_receive_frame:pCurrRD is %x,pRDInfo is %x\n",pCurrRD,pCurrRD->pRDInfo);
#endif
    struct net_device_stats* pStats=&pDevice->stats;
    struct sk_buff* skb;
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    PSRxMgmtPacket  pRxPacket = &(pDevice->pMgmt->sRxPacket);
    PS802_11Header  p802_11Header;
    unsigned char *pbyRsr;
    unsigned char *pbyNewRsr;
    unsigned char *pbyRSSI;
    PQWORD          pqwTSFTime;
    unsigned short *pwFrameSize;
    unsigned char *pbyFrame;
    bool bDeFragRx = false;
    bool bIsWEP = false;
    unsigned int cbHeaderOffset;
    unsigned int FrameSize;
    unsigned short wEtherType = 0;
    int             iSANodeIndex = -1;
    int             iDANodeIndex = -1;
    unsigned int ii;
    unsigned int cbIVOffset;
    bool bExtIV = false;
    unsigned char *pbyRxSts;
    unsigned char *pbyRxRate;
    unsigned char *pbySQ;
    unsigned int cbHeaderSize;
    PSKeyItem       pKey = NULL;
    unsigned short wRxTSC15_0 = 0;
    unsigned long dwRxTSC47_16 = 0;
    SKeyItem        STempKey;
    // 802.11h RPI
    unsigned long dwDuration = 0;
    long            ldBm = 0;
    long            ldBmThreshold = 0;
    PS802_11Header pMACHeader;
 bool bRxeapol_key = false;

//    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---------- device_receive_frame---\n");

    skb = pRDInfo->skb;


//PLICE_DEBUG->
	pci_unmap_single(pDevice->pcid, pRDInfo->skb_dma,
                     pDevice->rx_buf_sz, PCI_DMA_FROMDEVICE);
//PLICE_DEBUG<-
    pwFrameSize = (unsigned short *)(skb->data + 2);
    FrameSize = cpu_to_le16(pCurrRD->m_rd1RD1.wReqCount) - cpu_to_le16(pCurrRD->m_rd0RD0.wResCount);

    // Max: 2312Payload + 30HD +4CRC + 2Padding + 4Len + 8TSF + 4RSR
    // Min (ACK): 10HD +4CRC + 2Padding + 4Len + 8TSF + 4RSR
    if ((FrameSize > 2364)||(FrameSize <= 32)) {
        // Frame Size error drop this packet.
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---------- WRONG Length 1 \n");
        return false;
    }

    pbyRxSts = (unsigned char *) (skb->data);
    pbyRxRate = (unsigned char *) (skb->data + 1);
    pbyRsr = (unsigned char *) (skb->data + FrameSize - 1);
    pbyRSSI = (unsigned char *) (skb->data + FrameSize - 2);
    pbyNewRsr = (unsigned char *) (skb->data + FrameSize - 3);
    pbySQ = (unsigned char *) (skb->data + FrameSize - 4);
    pqwTSFTime = (PQWORD) (skb->data + FrameSize - 12);
    pbyFrame = (unsigned char *)(skb->data + 4);

    // get packet size
    FrameSize = cpu_to_le16(*pwFrameSize);

    if ((FrameSize > 2346)|(FrameSize < 14)) { // Max: 2312Payload + 30HD +4CRC
                                               // Min: 14 bytes ACK
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---------- WRONG Length 2 \n");
        return false;
    }
//PLICE_DEBUG->
	// update receive statistic counter
    STAvUpdateRDStatCounter(&pDevice->scStatistic,
                            *pbyRsr,
                            *pbyNewRsr,
                            *pbyRxRate,
                            pbyFrame,
                            FrameSize);

  pMACHeader=(PS802_11Header)((unsigned char *) (skb->data)+8);
//PLICE_DEBUG<-
	if (pDevice->bMeasureInProgress == true) {
        if ((*pbyRsr & RSR_CRCOK) != 0) {
            pDevice->byBasicMap |= 0x01;
        }
        dwDuration = (FrameSize << 4);
        dwDuration /= acbyRxRate[*pbyRxRate%MAX_RATE];
        if (*pbyRxRate <= RATE_11M) {
            if (*pbyRxSts & 0x01) {
                // long preamble
                dwDuration += 192;
            } else {
                // short preamble
                dwDuration += 96;
            }
        } else {
            dwDuration += 16;
        }
        RFvRSSITodBm(pDevice, *pbyRSSI, &ldBm);
        ldBmThreshold = -57;
        for (ii = 7; ii > 0;) {
            if (ldBm > ldBmThreshold) {
                break;
            }
            ldBmThreshold -= 5;
            ii--;
        }
        pDevice->dwRPIs[ii] += dwDuration;
        return false;
    }

    if (!is_multicast_ether_addr(pbyFrame)) {
        if (WCTLbIsDuplicate(&(pDevice->sDupRxCache), (PS802_11Header) (skb->data + 4))) {
            pDevice->s802_11Counter.FrameDuplicateCount++;
            return false;
        }
    }


    // Use for TKIP MIC
    s_vGetDASA(skb->data+4, &cbHeaderSize, &pDevice->sRxEthHeader);

    // filter packet send from myself
    if (!compare_ether_addr((unsigned char *)&(pDevice->sRxEthHeader.abySrcAddr[0]), pDevice->abyCurrentNetAddr))
        return false;

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) || (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA)) {
        if (IS_CTL_PSPOLL(pbyFrame) || !IS_TYPE_CONTROL(pbyFrame)) {
            p802_11Header = (PS802_11Header) (pbyFrame);
            // get SA NodeIndex
            if (BSSDBbIsSTAInNodeDB(pMgmt, (unsigned char *)(p802_11Header->abyAddr2), &iSANodeIndex)) {
                pMgmt->sNodeDBTable[iSANodeIndex].ulLastRxJiffer = jiffies;
                pMgmt->sNodeDBTable[iSANodeIndex].uInActiveCount = 0;
            }
        }
    }

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        if (s_bAPModeRxCtl(pDevice, pbyFrame, iSANodeIndex) == true) {
            return false;
        }
    }


    if (IS_FC_WEP(pbyFrame)) {
        bool bRxDecryOK = false;

        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"rx WEP pkt\n");
        bIsWEP = true;
        if ((pDevice->bEnableHostWEP) && (iSANodeIndex >= 0)) {
            pKey = &STempKey;
            pKey->byCipherSuite = pMgmt->sNodeDBTable[iSANodeIndex].byCipherSuite;
            pKey->dwKeyIndex = pMgmt->sNodeDBTable[iSANodeIndex].dwKeyIndex;
            pKey->uKeyLength = pMgmt->sNodeDBTable[iSANodeIndex].uWepKeyLength;
            pKey->dwTSC47_16 = pMgmt->sNodeDBTable[iSANodeIndex].dwTSC47_16;
            pKey->wTSC15_0 = pMgmt->sNodeDBTable[iSANodeIndex].wTSC15_0;
            memcpy(pKey->abyKey,
                &pMgmt->sNodeDBTable[iSANodeIndex].abyWepKey[0],
                pKey->uKeyLength
                );

            bRxDecryOK = s_bHostWepRxEncryption(pDevice,
                                                pbyFrame,
                                                FrameSize,
                                                pbyRsr,
                                                pMgmt->sNodeDBTable[iSANodeIndex].bOnFly,
                                                pKey,
                                                pbyNewRsr,
                                                &bExtIV,
                                                &wRxTSC15_0,
                                                &dwRxTSC47_16);
        } else {
            bRxDecryOK = s_bHandleRxEncryption(pDevice,
                                                pbyFrame,
                                                FrameSize,
                                                pbyRsr,
                                                pbyNewRsr,
                                                &pKey,
                                                &bExtIV,
                                                &wRxTSC15_0,
                                                &dwRxTSC47_16);
        }

        if (bRxDecryOK) {
            if ((*pbyNewRsr & NEWRSR_DECRYPTOK) == 0) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV Fail\n");
                if ( (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPAPSK) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA2) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK)) {

                    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
                        pDevice->s802_11Counter.TKIPICVErrors++;
                    } else if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_CCMP)) {
                        pDevice->s802_11Counter.CCMPDecryptErrors++;
                    } else if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_WEP)) {
//                      pDevice->s802_11Counter.WEPICVErrorCount.QuadPart++;
                    }
                }
                return false;
            }
        } else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"WEP Func Fail\n");
            return false;
        }
        if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_CCMP))
            FrameSize -= 8;         // Message Integrity Code
        else
            FrameSize -= 4;         // 4 is ICV
    }


    //
    // RX OK
    //
    //remove the CRC length
    FrameSize -= ETH_FCS_LEN;

    if (( !(*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI))) && // unicast address
        (IS_FRAGMENT_PKT((skb->data+4)))
        ) {
        // defragment
        bDeFragRx = WCTLbHandleFragment(pDevice, (PS802_11Header) (skb->data+4), FrameSize, bIsWEP, bExtIV);
        pDevice->s802_11Counter.ReceivedFragmentCount++;
        if (bDeFragRx) {
            // defrag complete
            skb = pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx].skb;
            FrameSize = pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx].cbFrameLength;

        }
        else {
            return false;
        }
    }


// Management & Control frame Handle
    if ((IS_TYPE_DATA((skb->data+4))) == false) {
        // Handle Control & Manage Frame

        if (IS_TYPE_MGMT((skb->data+4))) {
            unsigned char *pbyData1;
            unsigned char *pbyData2;

            pRxPacket->p80211Header = (PUWLAN_80211HDR)(skb->data+4);
            pRxPacket->cbMPDULen = FrameSize;
            pRxPacket->uRSSI = *pbyRSSI;
            pRxPacket->bySQ = *pbySQ;
            HIDWORD(pRxPacket->qwLocalTSF) = cpu_to_le32(HIDWORD(*pqwTSFTime));
            LODWORD(pRxPacket->qwLocalTSF) = cpu_to_le32(LODWORD(*pqwTSFTime));
            if (bIsWEP) {
                // strip IV
                pbyData1 = WLAN_HDR_A3_DATA_PTR(skb->data+4);
                pbyData2 = WLAN_HDR_A3_DATA_PTR(skb->data+4) + 4;
                for (ii = 0; ii < (FrameSize - 4); ii++) {
                    *pbyData1 = *pbyData2;
                     pbyData1++;
                     pbyData2++;
                }
            }
            pRxPacket->byRxRate = s_byGetRateIdx(*pbyRxRate);
            pRxPacket->byRxChannel = (*pbyRxSts) >> 2;
//PLICE_DEBUG->
//EnQueue(pDevice,pRxPacket);

#ifdef	THREAD
		EnQueue(pDevice,pRxPacket);

		//printk("enque time is %x\n",jiffies);
		//up(&pDevice->mlme_semaphore);
			//Enque (pDevice->FirstRecvMngList,pDevice->LastRecvMngList,pMgmt);
#else

#ifdef	TASK_LET
		EnQueue(pDevice,pRxPacket);
		tasklet_schedule(&pDevice->RxMngWorkItem);
#else
//printk("RxMan\n");
	vMgrRxManagePacket((void *)pDevice, pDevice->pMgmt, pRxPacket);
           //tasklet_schedule(&pDevice->RxMngWorkItem);
#endif

#endif
//PLICE_DEBUG<-
			//vMgrRxManagePacket((void *)pDevice, pDevice->pMgmt, pRxPacket);
            // hostap Deamon handle 802.11 management
            if (pDevice->bEnableHostapd) {
	            skb->dev = pDevice->apdev;
	            skb->data += 4;
	            skb->tail += 4;
                     skb_put(skb, FrameSize);
		skb_reset_mac_header(skb);
	            skb->pkt_type = PACKET_OTHERHOST;
    	        skb->protocol = htons(ETH_P_802_2);
	            memset(skb->cb, 0, sizeof(skb->cb));
	            netif_rx(skb);
                return true;
	        }
        }
        else {
            // Control Frame
        };
        return false;
    }
    else {
        if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
            //In AP mode, hw only check addr1(BSSID or RA) if equal to local MAC.
            if ( !(*pbyRsr & RSR_BSSIDOK)) {
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                        pDevice->dev->name);
                    }
                }
                return false;
            }
        }
        else {
            // discard DATA packet while not associate || BSSID error
            if ((pDevice->bLinkPass == false) ||
                !(*pbyRsr & RSR_BSSIDOK)) {
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                        pDevice->dev->name);
                    }
                }
                return false;
            }
   //mike add:station mode check eapol-key challenge--->
   	  {
   	    unsigned char Protocol_Version;    //802.1x Authentication
	    unsigned char Packet_Type;           //802.1x Authentication
              if (bIsWEP)
                  cbIVOffset = 8;
              else
                  cbIVOffset = 0;
              wEtherType = (skb->data[cbIVOffset + 8 + 24 + 6] << 8) |
                          skb->data[cbIVOffset + 8 + 24 + 6 + 1];
	      Protocol_Version = skb->data[cbIVOffset + 8 + 24 + 6 + 1 +1];
	      Packet_Type = skb->data[cbIVOffset + 8 + 24 + 6 + 1 +1+1];
	     if (wEtherType == ETH_P_PAE) {         //Protocol Type in LLC-Header
                  if(((Protocol_Version==1) ||(Protocol_Version==2)) &&
		     (Packet_Type==3)) {  //802.1x OR eapol-key challenge frame receive
                        bRxeapol_key = true;
                  }
	      }
   	  }
    //mike add:station mode check eapol-key challenge<---
        }
    }


// Data frame Handle


    if (pDevice->bEnablePSMode) {
        if (IS_FC_MOREDATA((skb->data+4))) {
            if (*pbyRsr & RSR_ADDROK) {
                //PSbSendPSPOLL((PSDevice)pDevice);
            }
        }
        else {
            if (pDevice->pMgmt->bInTIMWake == true) {
                pDevice->pMgmt->bInTIMWake = false;
            }
        }
    };

    // Now it only supports 802.11g Infrastructure Mode, and support rate must up to 54 Mbps
    if (pDevice->bDiversityEnable && (FrameSize>50) &&
        (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE) &&
        (pDevice->bLinkPass == true)) {
	//printk("device_receive_frame: RxRate is %d\n",*pbyRxRate);
		BBvAntennaDiversity(pDevice, s_byGetRateIdx(*pbyRxRate), 0);
    }


    if (pDevice->byLocalID != REV_ID_VT3253_B1) {
        pDevice->uCurrRSSI = *pbyRSSI;
    }
    pDevice->byCurrSQ = *pbySQ;

    if ((*pbyRSSI != 0) &&
        (pMgmt->pCurrBSS!=NULL)) {
        RFvRSSITodBm(pDevice, *pbyRSSI, &ldBm);
        // Moniter if RSSI is too strong.
        pMgmt->pCurrBSS->byRSSIStatCnt++;
        pMgmt->pCurrBSS->byRSSIStatCnt %= RSSI_STAT_COUNT;
        pMgmt->pCurrBSS->ldBmAverage[pMgmt->pCurrBSS->byRSSIStatCnt] = ldBm;
        for(ii=0;ii<RSSI_STAT_COUNT;ii++) {
            if (pMgmt->pCurrBSS->ldBmAverage[ii] != 0) {
            pMgmt->pCurrBSS->ldBmMAX = max(pMgmt->pCurrBSS->ldBmAverage[ii], ldBm);
            }
        }
    }

    // -----------------------------------------------

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) && (pDevice->bEnable8021x == true)){
        unsigned char abyMacHdr[24];

        // Only 802.1x packet incoming allowed
        if (bIsWEP)
            cbIVOffset = 8;
        else
            cbIVOffset = 0;
        wEtherType = (skb->data[cbIVOffset + 4 + 24 + 6] << 8) |
                    skb->data[cbIVOffset + 4 + 24 + 6 + 1];

	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"wEtherType = %04x \n", wEtherType);
        if (wEtherType == ETH_P_PAE) {
            skb->dev = pDevice->apdev;

            if (bIsWEP == true) {
                // strip IV header(8)
                memcpy(&abyMacHdr[0], (skb->data + 4), 24);
                memcpy((skb->data + 4 + cbIVOffset), &abyMacHdr[0], 24);
            }
            skb->data +=  (cbIVOffset + 4);
            skb->tail +=  (cbIVOffset + 4);
            skb_put(skb, FrameSize);
	    skb_reset_mac_header(skb);

	skb->pkt_type = PACKET_OTHERHOST;
            skb->protocol = htons(ETH_P_802_2);
            memset(skb->cb, 0, sizeof(skb->cb));
            netif_rx(skb);
            return true;

}
        // check if 802.1x authorized
        if (!(pMgmt->sNodeDBTable[iSANodeIndex].dwFlags & WLAN_STA_AUTHORIZED))
            return false;
    }


    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
        if (bIsWEP) {
            FrameSize -= 8;  //MIC
        }
    }

    //--------------------------------------------------------------------------------
    // Soft MIC
    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
        if (bIsWEP) {
            unsigned long *pdwMIC_L;
            unsigned long *pdwMIC_R;
            unsigned long dwMIC_Priority;
            unsigned long dwMICKey0 = 0, dwMICKey1 = 0;
            unsigned long dwLocalMIC_L = 0;
            unsigned long dwLocalMIC_R = 0;
            viawget_wpa_header *wpahdr;


            if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
                dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[24]));
                dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[28]));
            }
            else {
                if (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[16]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[20]));
                } else if ((pKey->dwKeyIndex & BIT28) == 0) {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[16]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[20]));
                } else {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[24]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[28]));
                }
            }

            MIC_vInit(dwMICKey0, dwMICKey1);
            MIC_vAppend((unsigned char *)&(pDevice->sRxEthHeader.abyDstAddr[0]), 12);
            dwMIC_Priority = 0;
            MIC_vAppend((unsigned char *)&dwMIC_Priority, 4);
            // 4 is Rcv buffer header, 24 is MAC Header, and 8 is IV and Ext IV.
            MIC_vAppend((unsigned char *)(skb->data + 4 + WLAN_HDR_ADDR3_LEN + 8),
                        FrameSize - WLAN_HDR_ADDR3_LEN - 8);
            MIC_vGetMIC(&dwLocalMIC_L, &dwLocalMIC_R);
            MIC_vUnInit();

            pdwMIC_L = (unsigned long *)(skb->data + 4 + FrameSize);
            pdwMIC_R = (unsigned long *)(skb->data + 4 + FrameSize + 4);
            //DBG_PRN_GRP12(("RxL: %lx, RxR: %lx\n", *pdwMIC_L, *pdwMIC_R));
            //DBG_PRN_GRP12(("LocalL: %lx, LocalR: %lx\n", dwLocalMIC_L, dwLocalMIC_R));
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"dwMICKey0= %lx,dwMICKey1= %lx \n", dwMICKey0, dwMICKey1);


            if ((cpu_to_le32(*pdwMIC_L) != dwLocalMIC_L) || (cpu_to_le32(*pdwMIC_R) != dwLocalMIC_R) ||
                (pDevice->bRxMICFail == true)) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIC comparison is fail!\n");
                pDevice->bRxMICFail = false;
                //pDevice->s802_11Counter.TKIPLocalMICFailures.QuadPart++;
                pDevice->s802_11Counter.TKIPLocalMICFailures++;
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                            pDevice->dev->name);
                    }
                }
               //2008-0409-07, <Add> by Einsn Liu
       #ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
				//send event to wpa_supplicant
				//if(pDevice->bWPADevEnable == true)
				{
					union iwreq_data wrqu;
					struct iw_michaelmicfailure ev;
					int keyidx = pbyFrame[cbHeaderSize+3] >> 6; //top two-bits
					memset(&ev, 0, sizeof(ev));
					ev.flags = keyidx & IW_MICFAILURE_KEY_ID;
					if ((pMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
							(pMgmt->eCurrState == WMAC_STATE_ASSOC) &&
								(*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) {
						ev.flags |= IW_MICFAILURE_PAIRWISE;
					} else {
						ev.flags |= IW_MICFAILURE_GROUP;
					}

					ev.src_addr.sa_family = ARPHRD_ETHER;
					memcpy(ev.src_addr.sa_data, pMACHeader->abyAddr2, ETH_ALEN);
					memset(&wrqu, 0, sizeof(wrqu));
					wrqu.data.length = sizeof(ev);
					wireless_send_event(pDevice->dev, IWEVMICHAELMICFAILURE, &wrqu, (char *)&ev);

				}
         #endif


                if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
                     wpahdr = (viawget_wpa_header *)pDevice->skb->data;
                     if ((pDevice->pMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
                         (pDevice->pMgmt->eCurrState == WMAC_STATE_ASSOC) &&
                         (*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) {
                         //s802_11_Status.Flags = NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR;
                         wpahdr->type = VIAWGET_PTK_MIC_MSG;
                     } else {
                         //s802_11_Status.Flags = NDIS_802_11_AUTH_REQUEST_GROUP_ERROR;
                         wpahdr->type = VIAWGET_GTK_MIC_MSG;
                     }
                     wpahdr->resp_ie_len = 0;
                     wpahdr->req_ie_len = 0;
                     skb_put(pDevice->skb, sizeof(viawget_wpa_header));
                     pDevice->skb->dev = pDevice->wpadev;
		     skb_reset_mac_header(pDevice->skb);
                     pDevice->skb->pkt_type = PACKET_HOST;
                     pDevice->skb->protocol = htons(ETH_P_802_2);
                     memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
                     netif_rx(pDevice->skb);
                     pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
                 };

                return false;

            }
        }
    } //---end of SOFT MIC-----------------------------------------------------------------------

    // ++++++++++ Reply Counter Check +++++++++++++

    if ((pKey != NULL) && ((pKey->byCipherSuite == KEY_CTL_TKIP) ||
                           (pKey->byCipherSuite == KEY_CTL_CCMP))) {
        if (bIsWEP) {
            unsigned short wLocalTSC15_0 = 0;
            unsigned long dwLocalTSC47_16 = 0;
            unsigned long long       RSC = 0;
            // endian issues
            RSC = *((unsigned long long *) &(pKey->KeyRSC));
            wLocalTSC15_0 = (unsigned short) RSC;
            dwLocalTSC47_16 = (unsigned long) (RSC>>16);

            RSC = dwRxTSC47_16;
            RSC <<= 16;
            RSC += wRxTSC15_0;
            memcpy(&(pKey->KeyRSC), &RSC,  sizeof(QWORD));

            if ( (pDevice->sMgmtObj.eCurrMode == WMAC_MODE_ESS_STA) &&
                 (pDevice->sMgmtObj.eCurrState == WMAC_STATE_ASSOC)) {
                // check RSC
                if ( (wRxTSC15_0 < wLocalTSC15_0) &&
                     (dwRxTSC47_16 <= dwLocalTSC47_16) &&
                     !((dwRxTSC47_16 == 0) && (dwLocalTSC47_16 == 0xFFFFFFFF))) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC is illegal~~!\n ");
                    if (pKey->byCipherSuite == KEY_CTL_TKIP)
                        //pDevice->s802_11Counter.TKIPReplays.QuadPart++;
                        pDevice->s802_11Counter.TKIPReplays++;
                    else
                        //pDevice->s802_11Counter.CCMPReplays.QuadPart++;
                        pDevice->s802_11Counter.CCMPReplays++;

                    if (bDeFragRx) {
                        if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                            DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                                pDevice->dev->name);
                        }
                    }
                    return false;
                }
            }
        }
    } // ----- End of Reply Counter Check --------------------------



    if ((pKey != NULL) && (bIsWEP)) {
//      pDevice->s802_11Counter.DecryptSuccessCount.QuadPart++;
    }


    s_vProcessRxMACHeader(pDevice, (unsigned char *)(skb->data+4), FrameSize, bIsWEP, bExtIV, &cbHeaderOffset);
    FrameSize -= cbHeaderOffset;
    cbHeaderOffset += 4;        // 4 is Rcv buffer header

    // Null data, framesize = 14
    if (FrameSize < 15)
        return false;

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        if (s_bAPModeRxData(pDevice,
                            skb,
                            FrameSize,
                            cbHeaderOffset,
                            iSANodeIndex,
                            iDANodeIndex
                            ) == false) {

            if (bDeFragRx) {
                if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                    DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                    pDevice->dev->name);
                }
            }
            return false;
        }

//        if(pDevice->bRxMICFail == false) {
//           for (ii =0; ii < 100; ii++)
//                printk(" %02x", *(skb->data + ii));
//           printk("\n");
//	    }

    }

	skb->data += cbHeaderOffset;
	skb->tail += cbHeaderOffset;
    skb_put(skb, FrameSize);
    skb->protocol=eth_type_trans(skb, skb->dev);


	//drop frame not met IEEE 802.3
/*
	if (pDevice->flags & DEVICE_FLAGS_VAL_PKT_LEN) {
		if ((skb->protocol==htons(ETH_P_802_3)) &&
			(skb->len!=htons(skb->mac.ethernet->h_proto))) {
			pStats->rx_length_errors++;
			pStats->rx_dropped++;
            if (bDeFragRx) {
                if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                    DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                    pDevice->dev->name);
                }
            }
			return false;
		}
	}
*/

    skb->ip_summed=CHECKSUM_NONE;
    pStats->rx_bytes +=skb->len;
    pStats->rx_packets++;
    netif_rx(skb);

    if (bDeFragRx) {
        if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
            DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                pDevice->dev->name);
        }
        return false;
    }

    return true;
}


static bool s_bAPModeRxCtl (
    PSDevice pDevice,
    unsigned char *pbyFrame,
    int      iSANodeIndex
    )
{
    PS802_11Header      p802_11Header;
    CMD_STATUS          Status;
    PSMgmtObject        pMgmt = pDevice->pMgmt;


    if (IS_CTL_PSPOLL(pbyFrame) || !IS_TYPE_CONTROL(pbyFrame)) {

        p802_11Header = (PS802_11Header) (pbyFrame);
        if (!IS_TYPE_MGMT(pbyFrame)) {

            // Data & PS-Poll packet
            // check frame class
            if (iSANodeIndex > 0) {
                // frame class 3 fliter & checking
                if (pMgmt->sNodeDBTable[iSANodeIndex].eNodeState < NODE_AUTH) {
                    // send deauth notification
                    // reason = (6) class 2 received from nonauth sta
                    vMgrDeAuthenBeginSta(pDevice,
                                         pMgmt,
                                         (unsigned char *)(p802_11Header->abyAddr2),
                                         (WLAN_MGMT_REASON_CLASS2_NONAUTH),
                                         &Status
                                         );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDeAuthenBeginSta 1\n");
                    return true;
                };
                if (pMgmt->sNodeDBTable[iSANodeIndex].eNodeState < NODE_ASSOC) {
                    // send deassoc notification
                    // reason = (7) class 3 received from nonassoc sta
                    vMgrDisassocBeginSta(pDevice,
                                         pMgmt,
                                         (unsigned char *)(p802_11Header->abyAddr2),
                                         (WLAN_MGMT_REASON_CLASS3_NONASSOC),
                                         &Status
                                         );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDisassocBeginSta 2\n");
                    return true;
                };

                if (pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable) {
                    // delcare received ps-poll event
                    if (IS_CTL_PSPOLL(pbyFrame)) {
                        pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                        bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 1\n");
                    }
                    else {
                        // check Data PS state
                        // if PW bit off, send out all PS bufferring packets.
                        if (!IS_FC_POWERMGT(pbyFrame)) {
                            pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = false;
                            pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                            bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 2\n");
                        }
                    }
                }
                else {
                   if (IS_FC_POWERMGT(pbyFrame)) {
                       pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = true;
                       // Once if STA in PS state, enable multicast bufferring
                       pMgmt->sNodeDBTable[0].bPSEnable = true;
                   }
                   else {
                      // clear all pending PS frame.
                      if (pMgmt->sNodeDBTable[iSANodeIndex].wEnQueueCnt > 0) {
                          pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = false;
                          pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                          bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                         DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 3\n");

                      }
                   }
                }
            }
            else {
                  vMgrDeAuthenBeginSta(pDevice,
                                       pMgmt,
                                       (unsigned char *)(p802_11Header->abyAddr2),
                                       (WLAN_MGMT_REASON_CLASS2_NONAUTH),
                                       &Status
                                       );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDeAuthenBeginSta 3\n");
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BSSID:%02x-%02x-%02x=%02x-%02x-%02x \n",
                                p802_11Header->abyAddr3[0],
                                p802_11Header->abyAddr3[1],
                                p802_11Header->abyAddr3[2],
                                p802_11Header->abyAddr3[3],
                                p802_11Header->abyAddr3[4],
                                p802_11Header->abyAddr3[5]
                               );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "ADDR2:%02x-%02x-%02x=%02x-%02x-%02x \n",
                                p802_11Header->abyAddr2[0],
                                p802_11Header->abyAddr2[1],
                                p802_11Header->abyAddr2[2],
                                p802_11Header->abyAddr2[3],
                                p802_11Header->abyAddr2[4],
                                p802_11Header->abyAddr2[5]
                               );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "ADDR1:%02x-%02x-%02x=%02x-%02x-%02x \n",
                                p802_11Header->abyAddr1[0],
                                p802_11Header->abyAddr1[1],
                                p802_11Header->abyAddr1[2],
                                p802_11Header->abyAddr1[3],
                                p802_11Header->abyAddr1[4],
                                p802_11Header->abyAddr1[5]
                               );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: wFrameCtl= %x\n", p802_11Header->wFrameCtl );
                    VNSvInPortB(pDevice->PortOffset + MAC_REG_RCR, &(pDevice->byRxMode));
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc:pDevice->byRxMode = %x\n", pDevice->byRxMode );
                    return true;
            }
        }
    }
    return false;

}

static bool s_bHandleRxEncryption (
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    unsigned char *pbyNewRsr,
    PSKeyItem   *pKeyOut,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    )
{
    unsigned int PayloadLen = FrameSize;
    unsigned char *pbyIV;
    unsigned char byKeyIdx;
    PSKeyItem       pKey = NULL;
    unsigned char byDecMode = KEY_CTL_WEP;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    *pwRxTSC15_0 = 0;
    *pdwRxTSC47_16 = 0;

    pbyIV = pbyFrame + WLAN_HDR_ADDR3_LEN;
    if ( WLAN_GET_FC_TODS(*(unsigned short *)pbyFrame) &&
         WLAN_GET_FC_FROMDS(*(unsigned short *)pbyFrame) ) {
         pbyIV += 6;             // 6 is 802.11 address4
         PayloadLen -= 6;
    }
    byKeyIdx = (*(pbyIV+3) & 0xc0);
    byKeyIdx >>= 6;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\nKeyIdx: %d\n", byKeyIdx);

    if ((pMgmt->eAuthenMode == WMAC_AUTH_WPA) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPAPSK) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPA2) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK)) {
        if (((*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) &&
            (pDevice->pMgmt->byCSSPK != KEY_CTL_NONE)) {
            // unicast pkt use pairwise key
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"unicast pkt\n");
            if (KeybGetKey(&(pDevice->sKey), pDevice->abyBSSID, 0xFFFFFFFF, &pKey) == true) {
                if (pDevice->pMgmt->byCSSPK == KEY_CTL_TKIP)
                    byDecMode = KEY_CTL_TKIP;
                else if (pDevice->pMgmt->byCSSPK == KEY_CTL_CCMP)
                    byDecMode = KEY_CTL_CCMP;
            }
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"unicast pkt: %d, %p\n", byDecMode, pKey);
        } else {
            // use group key
            KeybGetKey(&(pDevice->sKey), pDevice->abyBSSID, byKeyIdx, &pKey);
            if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
                byDecMode = KEY_CTL_TKIP;
            else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
                byDecMode = KEY_CTL_CCMP;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"group pkt: %d, %d, %p\n", byKeyIdx, byDecMode, pKey);
        }
    }
    // our WEP only support Default Key
    if (pKey == NULL) {
        // use default group key
        KeybGetKey(&(pDevice->sKey), pDevice->abyBroadcastAddr, byKeyIdx, &pKey);
        if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
            byDecMode = KEY_CTL_TKIP;
        else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
            byDecMode = KEY_CTL_CCMP;
    }
    *pKeyOut = pKey;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"AES:%d %d %d\n", pDevice->pMgmt->byCSSPK, pDevice->pMgmt->byCSSGK, byDecMode);

    if (pKey == NULL) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"pKey == NULL\n");
        if (byDecMode == KEY_CTL_WEP) {
//            pDevice->s802_11Counter.WEPUndecryptableCount.QuadPart++;
        } else if (pDevice->bLinkPass == true) {
//            pDevice->s802_11Counter.DecryptFailureCount.QuadPart++;
        }
        return false;
    }
    if (byDecMode != pKey->byCipherSuite) {
        if (byDecMode == KEY_CTL_WEP) {
//            pDevice->s802_11Counter.WEPUndecryptableCount.QuadPart++;
        } else if (pDevice->bLinkPass == true) {
//            pDevice->s802_11Counter.DecryptFailureCount.QuadPart++;
        }
        *pKeyOut = NULL;
        return false;
    }
    if (byDecMode == KEY_CTL_WEP) {
        // handle WEP
        if ((pDevice->byLocalID <= REV_ID_VT3253_A1) ||
            (((PSKeyTable)(pKey->pvKeyTable))->bSoftWEP == true)) {
            // Software WEP
            // 1. 3253A
            // 2. WEP 256

            PayloadLen -= (WLAN_HDR_ADDR3_LEN + 4 + 4); // 24 is 802.11 header,4 is IV, 4 is crc
            memcpy(pDevice->abyPRNG, pbyIV, 3);
            memcpy(pDevice->abyPRNG + 3, pKey->abyKey, pKey->uKeyLength);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pKey->uKeyLength + 3);
            rc4_encrypt(&pDevice->SBox, pbyIV+4, pbyIV+4, PayloadLen);

            if (ETHbIsBufferCrc32Ok(pbyIV+4, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
            }
        }
    } else if ((byDecMode == KEY_CTL_TKIP) ||
               (byDecMode == KEY_CTL_CCMP)) {
        // TKIP/AES

        PayloadLen -= (WLAN_HDR_ADDR3_LEN + 8 + 4); // 24 is 802.11 header, 8 is IV&ExtIV, 4 is crc
        *pdwRxTSC47_16 = cpu_to_le32(*(unsigned long *)(pbyIV + 4));
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ExtIV: %lx\n",*pdwRxTSC47_16);
        if (byDecMode == KEY_CTL_TKIP) {
            *pwRxTSC15_0 = cpu_to_le16(MAKEWORD(*(pbyIV+2), *pbyIV));
        } else {
            *pwRxTSC15_0 = cpu_to_le16(*(unsigned short *)pbyIV);
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC0_15: %x\n", *pwRxTSC15_0);

        if ((byDecMode == KEY_CTL_TKIP) &&
            (pDevice->byLocalID <= REV_ID_VT3253_A1)) {
            // Software TKIP
            // 1. 3253 A
            PS802_11Header  pMACHeader = (PS802_11Header) (pbyFrame);
            TKIPvMixKey(pKey->abyKey, pMACHeader->abyAddr2, *pwRxTSC15_0, *pdwRxTSC47_16, pDevice->abyPRNG);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
            rc4_encrypt(&pDevice->SBox, pbyIV+8, pbyIV+8, PayloadLen);
            if (ETHbIsBufferCrc32Ok(pbyIV+8, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV OK!\n");
            } else {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV FAIL!!!\n");
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"PayloadLen = %d\n", PayloadLen);
            }
        }
    }// end of TKIP/AES

    if ((*(pbyIV+3) & 0x20) != 0)
        *pbExtIV = true;
    return true;
}


static bool s_bHostWepRxEncryption (
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    bool bOnFly,
    PSKeyItem    pKey,
    unsigned char *pbyNewRsr,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    )
{
    unsigned int PayloadLen = FrameSize;
    unsigned char *pbyIV;
    unsigned char byKeyIdx;
    unsigned char byDecMode = KEY_CTL_WEP;
    PS802_11Header  pMACHeader;



    *pwRxTSC15_0 = 0;
    *pdwRxTSC47_16 = 0;

    pbyIV = pbyFrame + WLAN_HDR_ADDR3_LEN;
    if ( WLAN_GET_FC_TODS(*(unsigned short *)pbyFrame) &&
         WLAN_GET_FC_FROMDS(*(unsigned short *)pbyFrame) ) {
         pbyIV += 6;             // 6 is 802.11 address4
         PayloadLen -= 6;
    }
    byKeyIdx = (*(pbyIV+3) & 0xc0);
    byKeyIdx >>= 6;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\nKeyIdx: %d\n", byKeyIdx);


    if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
        byDecMode = KEY_CTL_TKIP;
    else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
        byDecMode = KEY_CTL_CCMP;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"AES:%d %d %d\n", pDevice->pMgmt->byCSSPK, pDevice->pMgmt->byCSSGK, byDecMode);

    if (byDecMode != pKey->byCipherSuite) {
        if (byDecMode == KEY_CTL_WEP) {
//            pDevice->s802_11Counter.WEPUndecryptableCount.QuadPart++;
        } else if (pDevice->bLinkPass == true) {
//            pDevice->s802_11Counter.DecryptFailureCount.QuadPart++;
        }
        return false;
    }

    if (byDecMode == KEY_CTL_WEP) {
        // handle WEP
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"byDecMode == KEY_CTL_WEP \n");
        if ((pDevice->byLocalID <= REV_ID_VT3253_A1) ||
            (((PSKeyTable)(pKey->pvKeyTable))->bSoftWEP == true) ||
            (bOnFly == false)) {
            // Software WEP
            // 1. 3253A
            // 2. WEP 256
            // 3. NotOnFly

            PayloadLen -= (WLAN_HDR_ADDR3_LEN + 4 + 4); // 24 is 802.11 header,4 is IV, 4 is crc
            memcpy(pDevice->abyPRNG, pbyIV, 3);
            memcpy(pDevice->abyPRNG + 3, pKey->abyKey, pKey->uKeyLength);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pKey->uKeyLength + 3);
            rc4_encrypt(&pDevice->SBox, pbyIV+4, pbyIV+4, PayloadLen);

            if (ETHbIsBufferCrc32Ok(pbyIV+4, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
            }
        }
    } else if ((byDecMode == KEY_CTL_TKIP) ||
               (byDecMode == KEY_CTL_CCMP)) {
        // TKIP/AES

        PayloadLen -= (WLAN_HDR_ADDR3_LEN + 8 + 4); // 24 is 802.11 header, 8 is IV&ExtIV, 4 is crc
        *pdwRxTSC47_16 = cpu_to_le32(*(unsigned long *)(pbyIV + 4));
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ExtIV: %lx\n",*pdwRxTSC47_16);

        if (byDecMode == KEY_CTL_TKIP) {
            *pwRxTSC15_0 = cpu_to_le16(MAKEWORD(*(pbyIV+2), *pbyIV));
        } else {
            *pwRxTSC15_0 = cpu_to_le16(*(unsigned short *)pbyIV);
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC0_15: %x\n", *pwRxTSC15_0);

        if (byDecMode == KEY_CTL_TKIP) {

            if ((pDevice->byLocalID <= REV_ID_VT3253_A1) || (bOnFly == false)) {
                // Software TKIP
                // 1. 3253 A
                // 2. NotOnFly
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"soft KEY_CTL_TKIP \n");
                pMACHeader = (PS802_11Header) (pbyFrame);
                TKIPvMixKey(pKey->abyKey, pMACHeader->abyAddr2, *pwRxTSC15_0, *pdwRxTSC47_16, pDevice->abyPRNG);
                rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
                rc4_encrypt(&pDevice->SBox, pbyIV+8, pbyIV+8, PayloadLen);
                if (ETHbIsBufferCrc32Ok(pbyIV+8, PayloadLen)) {
                    *pbyNewRsr |= NEWRSR_DECRYPTOK;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV OK!\n");
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV FAIL!!!\n");
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"PayloadLen = %d\n", PayloadLen);
                }
            }
        }

        if (byDecMode == KEY_CTL_CCMP) {
            if (bOnFly == false) {
                // Software CCMP
                // NotOnFly
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"soft KEY_CTL_CCMP\n");
                if (AESbGenCCMP(pKey->abyKey, pbyFrame, FrameSize)) {
                    *pbyNewRsr |= NEWRSR_DECRYPTOK;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CCMP MIC compare OK!\n");
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CCMP MIC fail!\n");
                }
            }
        }

    }// end of TKIP/AES

    if ((*(pbyIV+3) & 0x20) != 0)
        *pbExtIV = true;
    return true;
}



static bool s_bAPModeRxData (
    PSDevice pDevice,
    struct sk_buff* skb,
    unsigned int FrameSize,
    unsigned int cbHeaderOffset,
    int      iSANodeIndex,
    int      iDANodeIndex
    )
{
    PSMgmtObject        pMgmt = pDevice->pMgmt;
    bool bRelayAndForward = false;
    bool bRelayOnly = false;
    unsigned char byMask[8] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80};
    unsigned short wAID;


    struct sk_buff* skbcpy = NULL;

    if (FrameSize > CB_MAX_BUF_SIZE)
        return false;
    // check DA
    if(is_multicast_ether_addr((unsigned char *)(skb->data+cbHeaderOffset))) {
       if (pMgmt->sNodeDBTable[0].bPSEnable) {

           skbcpy = dev_alloc_skb((int)pDevice->rx_buf_sz);

        // if any node in PS mode, buffer packet until DTIM.
           if (skbcpy == NULL) {
               DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "relay multicast no skb available \n");
           }
           else {
               skbcpy->dev = pDevice->dev;
               skbcpy->len = FrameSize;
               memcpy(skbcpy->data, skb->data+cbHeaderOffset, FrameSize);
               skb_queue_tail(&(pMgmt->sNodeDBTable[0].sTxPSQueue), skbcpy);

               pMgmt->sNodeDBTable[0].wEnQueueCnt++;
               // set tx map
               pMgmt->abyPSTxMap[0] |= byMask[0];
           }
       }
       else {
           bRelayAndForward = true;
       }
    }
    else {
        // check if relay
        if (BSSDBbIsSTAInNodeDB(pMgmt, (unsigned char *)(skb->data+cbHeaderOffset), &iDANodeIndex)) {
            if (pMgmt->sNodeDBTable[iDANodeIndex].eNodeState >= NODE_ASSOC) {
                if (pMgmt->sNodeDBTable[iDANodeIndex].bPSEnable) {
                    // queue this skb until next PS tx, and then release.

	                skb->data += cbHeaderOffset;
	                skb->tail += cbHeaderOffset;
                    skb_put(skb, FrameSize);
                    skb_queue_tail(&pMgmt->sNodeDBTable[iDANodeIndex].sTxPSQueue, skb);
                    pMgmt->sNodeDBTable[iDANodeIndex].wEnQueueCnt++;
                    wAID = pMgmt->sNodeDBTable[iDANodeIndex].wAID;
                    pMgmt->abyPSTxMap[wAID >> 3] |=  byMask[wAID & 7];
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "relay: index= %d, pMgmt->abyPSTxMap[%d]= %d\n",
                               iDANodeIndex, (wAID >> 3), pMgmt->abyPSTxMap[wAID >> 3]);
                    return true;
                }
                else {
                    bRelayOnly = true;
                }
            }
        };
    }

    if (bRelayOnly || bRelayAndForward) {
        // relay this packet right now
        if (bRelayAndForward)
            iDANodeIndex = 0;

        if ((pDevice->uAssocCount > 1) && (iDANodeIndex >= 0)) {
            ROUTEbRelay(pDevice, (unsigned char *)(skb->data + cbHeaderOffset), FrameSize, (unsigned int)iDANodeIndex);
        }

        if (bRelayOnly)
            return false;
    }
    // none associate, don't forward
    if (pDevice->uAssocCount == 0)
        return false;

    return true;
}
