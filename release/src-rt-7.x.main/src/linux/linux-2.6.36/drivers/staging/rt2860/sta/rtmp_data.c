/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

	Module Name:
	rtmp_data.c

	Abstract:
	Data path subroutines

	Revision History:
	Who 		When			What
	--------	----------		----------------------------------------------
*/
#include "../rt_config.h"

void STARxEAPOLFrameIndicate(struct rt_rtmp_adapter *pAd,
			     struct rt_mac_table_entry *pEntry,
			     struct rt_rx_blk *pRxBlk, u8 FromWhichBSSID)
{
	PRT28XX_RXD_STRUC pRxD = &(pRxBlk->RxD);
	struct rt_rxwi * pRxWI = pRxBlk->pRxWI;
	u8 *pTmpBuf;

	if (pAd->StaCfg.WpaSupplicantUP) {
		/* All EAPoL frames have to pass to upper layer (ex. WPA_SUPPLICANT daemon) */
		/* TBD : process fragmented EAPol frames */
		{
			/* In 802.1x mode, if the received frame is EAP-SUCCESS packet, turn on the PortSecured variable */
			if (pAd->StaCfg.IEEE8021X == TRUE &&
			    (EAP_CODE_SUCCESS ==
			     WpaCheckEapCode(pAd, pRxBlk->pData,
					     pRxBlk->DataSize,
					     LENGTH_802_1_H))) {
				u8 *Key;
				u8 CipherAlg;
				int idx = 0;

				DBGPRINT_RAW(RT_DEBUG_TRACE,
					     ("Receive EAP-SUCCESS Packet\n"));
				/*pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; */
				STA_PORT_SECURED(pAd);

				if (pAd->StaCfg.IEEE8021x_required_keys ==
				    FALSE) {
					idx = pAd->StaCfg.DesireSharedKeyId;
					CipherAlg =
					    pAd->StaCfg.DesireSharedKey[idx].
					    CipherAlg;
					Key =
					    pAd->StaCfg.DesireSharedKey[idx].
					    Key;

					if (pAd->StaCfg.DesireSharedKey[idx].
					    KeyLen > 0) {
#ifdef RTMP_MAC_PCI
						struct rt_mac_table_entry *pEntry =
						    &pAd->MacTab.
						    Content[BSSID_WCID];

						/* Set key material and cipherAlg to Asic */
						AsicAddSharedKeyEntry(pAd, BSS0,
								      idx,
								      CipherAlg,
								      Key, NULL,
								      NULL);

						/* Assign group key info */
						RTMPAddWcidAttributeEntry(pAd,
									  BSS0,
									  idx,
									  CipherAlg,
									  NULL);

						/* Assign pairwise key info */
						RTMPAddWcidAttributeEntry(pAd,
									  BSS0,
									  idx,
									  CipherAlg,
									  pEntry);

						pAd->IndicateMediaState =
						    NdisMediaStateConnected;
						pAd->ExtraInfo =
						    GENERAL_LINK_UP;
#endif /* RTMP_MAC_PCI // */
#ifdef RTMP_MAC_USB
						union {
							char buf[sizeof
								 (struct rt_ndis_802_11_wep)
								 +
								 MAX_LEN_OF_KEY
								 - 1];
							struct rt_ndis_802_11_wep keyinfo;
						}
						WepKey;
						int len;

						NdisZeroMemory(&WepKey,
							       sizeof(WepKey));
						len =
						    pAd->StaCfg.
						    DesireSharedKey[idx].KeyLen;

						NdisMoveMemory(WepKey.keyinfo.
							       KeyMaterial,
							       pAd->StaCfg.
							       DesireSharedKey
							       [idx].Key,
							       pAd->StaCfg.
							       DesireSharedKey
							       [idx].KeyLen);

						WepKey.keyinfo.KeyIndex =
						    0x80000000 + idx;
						WepKey.keyinfo.KeyLength = len;
						pAd->SharedKey[BSS0][idx].
						    KeyLen =
						    (u8)(len <= 5 ? 5 : 13);

						pAd->IndicateMediaState =
						    NdisMediaStateConnected;
						pAd->ExtraInfo =
						    GENERAL_LINK_UP;
						/* need to enqueue cmd to thread */
						RTUSBEnqueueCmdFromNdis(pAd,
									OID_802_11_ADD_WEP,
									TRUE,
									&WepKey,
									sizeof
									(WepKey.
									 keyinfo)
									+ len -
									1);
#endif /* RTMP_MAC_USB // */
						/* For Preventing ShardKey Table is cleared by remove key procedure. */
						pAd->SharedKey[BSS0][idx].
						    CipherAlg = CipherAlg;
						pAd->SharedKey[BSS0][idx].
						    KeyLen =
						    pAd->StaCfg.
						    DesireSharedKey[idx].KeyLen;
						NdisMoveMemory(pAd->
							       SharedKey[BSS0]
							       [idx].Key,
							       pAd->StaCfg.
							       DesireSharedKey
							       [idx].Key,
							       pAd->StaCfg.
							       DesireSharedKey
							       [idx].KeyLen);
					}
				}
			}

			Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
			return;
		}
	} else {
		/* Special DATA frame that has to pass to MLME */
		/*       1. Cisco Aironet frames for CCX2. We need pass it to MLME for special process */
		/*       2. EAPOL handshaking frames when driver supplicant enabled, pass to MLME for special process */
		{
			pTmpBuf = pRxBlk->pData - LENGTH_802_11;
			NdisMoveMemory(pTmpBuf, pRxBlk->pHeader, LENGTH_802_11);
			REPORT_MGMT_FRAME_TO_MLME(pAd, pRxWI->WirelessCliID,
						  pTmpBuf,
						  pRxBlk->DataSize +
						  LENGTH_802_11, pRxWI->RSSI0,
						  pRxWI->RSSI1, pRxWI->RSSI2,
						  pRxD->PlcpSignal);
			DBGPRINT_RAW(RT_DEBUG_TRACE,
				     ("report EAPOL/AIRONET DATA to MLME (len=%d) !\n",
				      pRxBlk->DataSize));
		}
	}

	RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
	return;

}

void STARxDataFrameAnnounce(struct rt_rtmp_adapter *pAd,
			    struct rt_mac_table_entry *pEntry,
			    struct rt_rx_blk *pRxBlk, u8 FromWhichBSSID)
{

	/* non-EAP frame */
	if (!RTMPCheckWPAframe
	    (pAd, pEntry, pRxBlk->pData, pRxBlk->DataSize, FromWhichBSSID)) {

		{
			/* drop all non-EAP DATA frame before */
			/* this client's Port-Access-Control is secured */
			if (pRxBlk->pHeader->FC.Wep) {
				/* unsupported cipher suite */
				if (pAd->StaCfg.WepStatus ==
				    Ndis802_11EncryptionDisabled) {
					/* release packet */
					RELEASE_NDIS_PACKET(pAd,
							    pRxBlk->pRxPacket,
							    NDIS_STATUS_FAILURE);
					return;
				}
			} else {
				/* encryption in-use but receive a non-EAPOL clear text frame, drop it */
				if ((pAd->StaCfg.WepStatus !=
				     Ndis802_11EncryptionDisabled)
				    && (pAd->StaCfg.PortSecured ==
					WPA_802_1X_PORT_NOT_SECURED)) {
					/* release packet */
					RELEASE_NDIS_PACKET(pAd,
							    pRxBlk->pRxPacket,
							    NDIS_STATUS_FAILURE);
					return;
				}
			}
		}
		RX_BLK_CLEAR_FLAG(pRxBlk, fRX_EAP);
		if (!RX_BLK_TEST_FLAG(pRxBlk, fRX_ARALINK)) {
			/* Normal legacy, AMPDU or AMSDU */
			CmmRxnonRalinkFrameIndicate(pAd, pRxBlk,
						    FromWhichBSSID);

		} else {
			/* ARALINK */
			CmmRxRalinkFrameIndicate(pAd, pEntry, pRxBlk,
						 FromWhichBSSID);
		}
	} else {
		RX_BLK_SET_FLAG(pRxBlk, fRX_EAP);

		if (RX_BLK_TEST_FLAG(pRxBlk, fRX_AMPDU)
		    && (pAd->CommonCfg.bDisableReordering == 0)) {
			Indicate_AMPDU_Packet(pAd, pRxBlk, FromWhichBSSID);
		} else {
			/* Determin the destination of the EAP frame */
			/*  to WPA state machine or upper layer */
			STARxEAPOLFrameIndicate(pAd, pEntry, pRxBlk,
						FromWhichBSSID);
		}
	}
}

/* For TKIP frame, calculate the MIC value */
BOOLEAN STACheckTkipMICValue(struct rt_rtmp_adapter *pAd,
			     struct rt_mac_table_entry *pEntry, struct rt_rx_blk *pRxBlk)
{
	struct rt_header_802_11 * pHeader = pRxBlk->pHeader;
	u8 *pData = pRxBlk->pData;
	u16 DataSize = pRxBlk->DataSize;
	u8 UserPriority = pRxBlk->UserPriority;
	struct rt_cipher_key *pWpaKey;
	u8 *pDA, *pSA;

	pWpaKey = &pAd->SharedKey[BSS0][pRxBlk->pRxWI->KeyIndex];

	pDA = pHeader->Addr1;
	if (RX_BLK_TEST_FLAG(pRxBlk, fRX_INFRA)) {
		pSA = pHeader->Addr3;
	} else {
		pSA = pHeader->Addr2;
	}

	if (RTMPTkipCompareMICValue(pAd,
				    pData,
				    pDA,
				    pSA,
				    pWpaKey->RxMic,
				    UserPriority, DataSize) == FALSE) {
		DBGPRINT_RAW(RT_DEBUG_ERROR, ("Rx MIC Value error 2\n"));

		if (pAd->StaCfg.WpaSupplicantUP) {
			WpaSendMicFailureToWpaSupplicant(pAd,
							 (pWpaKey->Type ==
							  PAIRWISEKEY) ? TRUE :
							 FALSE);
		} else {
			RTMPReportMicError(pAd, pWpaKey);
		}

		/* release packet */
		RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket,
				    NDIS_STATUS_FAILURE);
		return FALSE;
	}

	return TRUE;
}

/* */
/* All Rx routines use struct rt_rx_blk structure to hande rx events */
/* It is very important to build pRxBlk attributes */
/*  1. pHeader pointer to 802.11 Header */
/*  2. pData pointer to payload including LLC (just skip Header) */
/*  3. set payload size including LLC to DataSize */
/*  4. set some flags with RX_BLK_SET_FLAG() */
/* */
void STAHandleRxDataFrame(struct rt_rtmp_adapter *pAd, struct rt_rx_blk *pRxBlk)
{
	PRT28XX_RXD_STRUC pRxD = &(pRxBlk->RxD);
	struct rt_rxwi * pRxWI = pRxBlk->pRxWI;
	struct rt_header_802_11 * pHeader = pRxBlk->pHeader;
	void *pRxPacket = pRxBlk->pRxPacket;
	BOOLEAN bFragment = FALSE;
	struct rt_mac_table_entry *pEntry = NULL;
	u8 FromWhichBSSID = BSS0;
	u8 UserPriority = 0;

	{
		/* before LINK UP, all DATA frames are rejected */
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)) {
			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
			return;
		}
		/* Drop not my BSS frames */
		if (pRxD->MyBss == 0) {
			{
				/* release packet */
				RELEASE_NDIS_PACKET(pAd, pRxPacket,
						    NDIS_STATUS_FAILURE);
				return;
			}
		}

		pAd->RalinkCounters.RxCountSinceLastNULL++;
		if (pAd->CommonCfg.bAPSDCapable
		    && pAd->CommonCfg.APEdcaParm.bAPSDCapable
		    && (pHeader->FC.SubType & 0x08)) {
			u8 *pData;
			DBGPRINT(RT_DEBUG_INFO, ("bAPSDCapable\n"));

			/* Qos bit 4 */
			pData = (u8 *)pHeader + LENGTH_802_11;
			if ((*pData >> 4) & 0x01) {
				DBGPRINT(RT_DEBUG_INFO,
					 ("RxDone- Rcv EOSP frame, driver may fall into sleep\n"));
				pAd->CommonCfg.bInServicePeriod = FALSE;

				/* Force driver to fall into sleep mode when rcv EOSP frame */
				if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)) {
					u16 TbttNumToNextWakeUp;
					u16 NextDtim =
					    pAd->StaCfg.DtimPeriod;
					unsigned long Now;

					NdisGetSystemUpTime(&Now);
					NextDtim -=
					    (u16)(Now -
						      pAd->StaCfg.
						      LastBeaconRxTime) /
					    pAd->CommonCfg.BeaconPeriod;

					TbttNumToNextWakeUp =
					    pAd->StaCfg.DefaultListenCount;
					if (OPSTATUS_TEST_FLAG
					    (pAd, fOP_STATUS_RECEIVE_DTIM)
					    && (TbttNumToNextWakeUp > NextDtim))
						TbttNumToNextWakeUp = NextDtim;

					RTMP_SET_PSM_BIT(pAd, PWR_SAVE);
					/* if WMM-APSD is failed, try to disable following line */
					AsicSleepThenAutoWakeup(pAd,
								TbttNumToNextWakeUp);
				}
			}

			if ((pHeader->FC.MoreData)
			    && (pAd->CommonCfg.bInServicePeriod)) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("Sending another trigger frame when More Data bit is set to 1\n"));
			}
		}
		/* Drop NULL, CF-ACK(no data), CF-POLL(no data), and CF-ACK+CF-POLL(no data) data frame */
		if ((pHeader->FC.SubType & 0x04))	/* bit 2 : no DATA */
		{
			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
			return;
		}
		/* Drop not my BSS frame (we can not only check the MyBss bit in RxD) */

		if (INFRA_ON(pAd)) {
			/* Infrastructure mode, check address 2 for BSSID */
			if (!RTMPEqualMemory
			    (&pHeader->Addr2, &pAd->CommonCfg.Bssid, 6)) {
				/* Receive frame not my BSSID */
				/* release packet */
				RELEASE_NDIS_PACKET(pAd, pRxPacket,
						    NDIS_STATUS_FAILURE);
				return;
			}
		} else		/* Ad-Hoc mode or Not associated */
		{
			/* Ad-Hoc mode, check address 3 for BSSID */
			if (!RTMPEqualMemory
			    (&pHeader->Addr3, &pAd->CommonCfg.Bssid, 6)) {
				/* Receive frame not my BSSID */
				/* release packet */
				RELEASE_NDIS_PACKET(pAd, pRxPacket,
						    NDIS_STATUS_FAILURE);
				return;
			}
		}

		/* */
		/* find pEntry */
		/* */
		if (pRxWI->WirelessCliID < MAX_LEN_OF_MAC_TABLE) {
			pEntry = &pAd->MacTab.Content[pRxWI->WirelessCliID];
		} else {
			/* 1. release packet if infra mode */
			/* 2. new a pEntry if ad-hoc mode */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
			return;
		}

		/* infra or ad-hoc */
		if (INFRA_ON(pAd)) {
			RX_BLK_SET_FLAG(pRxBlk, fRX_INFRA);
			ASSERT(pRxWI->WirelessCliID == BSSID_WCID);
		}
		/* check Atheros Client */
		if ((pEntry->bIAmBadAtheros == FALSE) && (pRxD->AMPDU == 1)
		    && (pHeader->FC.Retry)) {
			pEntry->bIAmBadAtheros = TRUE;
			pAd->CommonCfg.IOTestParm.bCurrentAtheros = TRUE;
			pAd->CommonCfg.IOTestParm.bLastAtheros = TRUE;
			if (!STA_AES_ON(pAd)) {
				AsicUpdateProtect(pAd, 8, ALLN_SETPROTECT, TRUE,
						  FALSE);
			}
		}
	}

	pRxBlk->pData = (u8 *) pHeader;

	/* */
	/* update RxBlk->pData, DataSize */
	/* 802.11 Header, QOS, HTC, Hw Padding */
	/* */

	/* 1. skip 802.11 HEADER */
	{
		pRxBlk->pData += LENGTH_802_11;
		pRxBlk->DataSize -= LENGTH_802_11;
	}

	/* 2. QOS */
	if (pHeader->FC.SubType & 0x08) {
		RX_BLK_SET_FLAG(pRxBlk, fRX_QOS);
		UserPriority = *(pRxBlk->pData) & 0x0f;
		/* bit 7 in QoS Control field signals the HT A-MSDU format */
		if ((*pRxBlk->pData) & 0x80) {
			RX_BLK_SET_FLAG(pRxBlk, fRX_AMSDU);
		}
		/* skip QOS contorl field */
		pRxBlk->pData += 2;
		pRxBlk->DataSize -= 2;
	}
	pRxBlk->UserPriority = UserPriority;

	/* check if need to resend PS Poll when received packet with MoreData = 1 */
	if ((pAd->StaCfg.Psm == PWR_SAVE) && (pHeader->FC.MoreData == 1)) {
		if ((((UserPriority == 0) || (UserPriority == 3)) &&
		     pAd->CommonCfg.bAPSDAC_BE == 0) ||
		    (((UserPriority == 1) || (UserPriority == 2)) &&
		     pAd->CommonCfg.bAPSDAC_BK == 0) ||
		    (((UserPriority == 4) || (UserPriority == 5)) &&
		     pAd->CommonCfg.bAPSDAC_VI == 0) ||
		    (((UserPriority == 6) || (UserPriority == 7)) &&
		     pAd->CommonCfg.bAPSDAC_VO == 0)) {
			/* non-UAPSD delivery-enabled AC */
			RTMP_PS_POLL_ENQUEUE(pAd);
		}
	}
	/* 3. Order bit: A-Ralink or HTC+ */
	if (pHeader->FC.Order) {
#ifdef AGGREGATION_SUPPORT
		if ((pRxWI->PHYMODE <= MODE_OFDM)
		    && (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED)))
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_ARALINK);
		} else
#endif /* AGGREGATION_SUPPORT // */
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_HTC);
			/* skip HTC contorl field */
			pRxBlk->pData += 4;
			pRxBlk->DataSize -= 4;
		}
	}
	/* 4. skip HW padding */
	if (pRxD->L2PAD) {
		/* just move pData pointer */
		/* because DataSize excluding HW padding */
		RX_BLK_SET_FLAG(pRxBlk, fRX_PAD);
		pRxBlk->pData += 2;
	}

	if (pRxD->BA) {
		RX_BLK_SET_FLAG(pRxBlk, fRX_AMPDU);
	}
	/* */
	/* Case I  Process Broadcast & Multicast data frame */
	/* */
	if (pRxD->Bcast || pRxD->Mcast) {
		INC_COUNTER64(pAd->WlanCounters.MulticastReceivedFrameCount);

		/* Drop Mcast/Bcast frame with fragment bit on */
		if (pHeader->FC.MoreFrag) {
			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
			return;
		}
		/* Filter out Bcast frame which AP relayed for us */
		if (pHeader->FC.FrDs
		    && MAC_ADDR_EQUAL(pHeader->Addr3, pAd->CurrentAddress)) {
			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
			return;
		}

		Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
		return;
	} else if (pRxD->U2M) {
		pAd->LastRxRate =
		    (u16)((pRxWI->MCS) + (pRxWI->BW << 7) +
			      (pRxWI->ShortGI << 8) + (pRxWI->PHYMODE << 14));

		if (ADHOC_ON(pAd)) {
			pEntry = MacTableLookup(pAd, pHeader->Addr2);
			if (pEntry)
				Update_Rssi_Sample(pAd, &pEntry->RssiSample,
						   pRxWI);
		}

		Update_Rssi_Sample(pAd, &pAd->StaCfg.RssiSample, pRxWI);

		pAd->StaCfg.LastSNR0 = (u8)(pRxWI->SNR0);
		pAd->StaCfg.LastSNR1 = (u8)(pRxWI->SNR1);

		pAd->RalinkCounters.OneSecRxOkDataCnt++;

		if (!((pHeader->Frag == 0) && (pHeader->FC.MoreFrag == 0))) {
			/* re-assemble the fragmented packets */
			/* return complete frame (pRxPacket) or NULL */
			bFragment = TRUE;
			pRxPacket = RTMPDeFragmentDataFrame(pAd, pRxBlk);
		}

		if (pRxPacket) {
			pEntry = &pAd->MacTab.Content[pRxWI->WirelessCliID];

			/* process complete frame */
			if (bFragment && (pRxD->Decrypted)
			    && (pEntry->WepStatus ==
				Ndis802_11Encryption2Enabled)) {
				/* Minus MIC length */
				pRxBlk->DataSize -= 8;

				/* For TKIP frame, calculate the MIC value */
				if (STACheckTkipMICValue(pAd, pEntry, pRxBlk) ==
				    FALSE) {
					return;
				}
			}

			STARxDataFrameAnnounce(pAd, pEntry, pRxBlk,
					       FromWhichBSSID);
			return;
		} else {
			/* just return */
			/* because RTMPDeFragmentDataFrame() will release rx packet, */
			/* if packet is fragmented */
			return;
		}
	}

	ASSERT(0);
	/* release packet */
	RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
}

void STAHandleRxMgmtFrame(struct rt_rtmp_adapter *pAd, struct rt_rx_blk *pRxBlk)
{
	PRT28XX_RXD_STRUC pRxD = &(pRxBlk->RxD);
	struct rt_rxwi * pRxWI = pRxBlk->pRxWI;
	struct rt_header_802_11 * pHeader = pRxBlk->pHeader;
	void *pRxPacket = pRxBlk->pRxPacket;

	do {

		/* check if need to resend PS Poll when received packet with MoreData = 1 */
		if ((pAd->StaCfg.Psm == PWR_SAVE)
		    && (pHeader->FC.MoreData == 1)) {
			/* for UAPSD, all management frames will be VO priority */
			if (pAd->CommonCfg.bAPSDAC_VO == 0) {
				/* non-UAPSD delivery-enabled AC */
				RTMP_PS_POLL_ENQUEUE(pAd);
			}
		}

		/* TODO: if MoreData == 0, station can go to sleep */

		/* We should collect RSSI not only U2M data but also my beacon */
		if ((pHeader->FC.SubType == SUBTYPE_BEACON)
		    && (MAC_ADDR_EQUAL(&pAd->CommonCfg.Bssid, &pHeader->Addr2))
		    && (pAd->RxAnt.EvaluatePeriod == 0)) {
			Update_Rssi_Sample(pAd, &pAd->StaCfg.RssiSample, pRxWI);

			pAd->StaCfg.LastSNR0 = (u8)(pRxWI->SNR0);
			pAd->StaCfg.LastSNR1 = (u8)(pRxWI->SNR1);
		}

		/* First check the size, it MUST not exceed the mlme queue size */
		if (pRxWI->MPDUtotalByteCount > MGMT_DMA_BUFFER_SIZE) {
			DBGPRINT_ERR(("STAHandleRxMgmtFrame: frame too large, size = %d \n", pRxWI->MPDUtotalByteCount));
			break;
		}

		REPORT_MGMT_FRAME_TO_MLME(pAd, pRxWI->WirelessCliID, pHeader,
					  pRxWI->MPDUtotalByteCount,
					  pRxWI->RSSI0, pRxWI->RSSI1,
					  pRxWI->RSSI2, pRxD->PlcpSignal);
	} while (FALSE);

	RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_SUCCESS);
}

void STAHandleRxControlFrame(struct rt_rtmp_adapter *pAd, struct rt_rx_blk *pRxBlk)
{
	struct rt_rxwi * pRxWI = pRxBlk->pRxWI;
	struct rt_header_802_11 * pHeader = pRxBlk->pHeader;
	void *pRxPacket = pRxBlk->pRxPacket;

	switch (pHeader->FC.SubType) {
	case SUBTYPE_BLOCK_ACK_REQ:
		{
			CntlEnqueueForRecv(pAd, pRxWI->WirelessCliID,
					   (pRxWI->MPDUtotalByteCount),
					   (struct rt_frame_ba_req *) pHeader);
		}
		break;
	case SUBTYPE_BLOCK_ACK:
	case SUBTYPE_ACK:
	default:
		break;
	}

	RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
}

/*
	========================================================================

	Routine Description:
		Process RxDone interrupt, running in DPC level

	Arguments:
		pAd Pointer to our adapter

	Return Value:
		None

	IRQL = DISPATCH_LEVEL

	Note:
		This routine has to maintain Rx ring read pointer.
		Need to consider QOS DATA format when converting to 802.3
	========================================================================
*/
BOOLEAN STARxDoneInterruptHandle(struct rt_rtmp_adapter *pAd, IN BOOLEAN argc)
{
	int Status;
	u32 RxProcessed, RxPending;
	BOOLEAN bReschedule = FALSE;
	PRT28XX_RXD_STRUC pRxD;
	u8 *pData;
	struct rt_rxwi * pRxWI;
	void *pRxPacket;
	struct rt_header_802_11 * pHeader;
	struct rt_rx_blk RxCell;

	RxProcessed = RxPending = 0;

	/* process whole rx ring */
	while (1) {

		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF |
				   fRTMP_ADAPTER_RESET_IN_PROGRESS |
				   fRTMP_ADAPTER_HALT_IN_PROGRESS |
				   fRTMP_ADAPTER_NIC_NOT_EXIST) ||
		    !RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)) {
			break;
		}
#ifdef RTMP_MAC_PCI
		if (RxProcessed++ > MAX_RX_PROCESS_CNT) {
			/* need to reschedule rx handle */
			bReschedule = TRUE;
			break;
		}
#endif /* RTMP_MAC_PCI // */

		RxProcessed++;	/* test */

		/* 1. allocate a new data packet into rx ring to replace received packet */
		/*    then processing the received packet */
		/* 2. the callee must take charge of release of packet */
		/* 3. As far as driver is concerned , */
		/*    the rx packet must */
		/*      a. be indicated to upper layer or */
		/*      b. be released if it is discarded */
		pRxPacket =
		    GetPacketFromRxRing(pAd, &(RxCell.RxD), &bReschedule,
					&RxPending);
		if (pRxPacket == NULL) {
			/* no more packet to process */
			break;
		}
		/* get rx ring descriptor */
		pRxD = &(RxCell.RxD);
		/* get rx data buffer */
		pData = GET_OS_PKT_DATAPTR(pRxPacket);
		pRxWI = (struct rt_rxwi *) pData;
		pHeader = (struct rt_header_802_11 *) (pData + RXWI_SIZE);

		/* build RxCell */
		RxCell.pRxWI = pRxWI;
		RxCell.pHeader = pHeader;
		RxCell.pRxPacket = pRxPacket;
		RxCell.pData = (u8 *) pHeader;
		RxCell.DataSize = pRxWI->MPDUtotalByteCount;
		RxCell.Flags = 0;

		/* Increase Total receive byte counter after real data received no mater any error or not */
		pAd->RalinkCounters.ReceivedByteCount +=
		    pRxWI->MPDUtotalByteCount;
		pAd->RalinkCounters.OneSecReceivedByteCount +=
		    pRxWI->MPDUtotalByteCount;
		pAd->RalinkCounters.RxCount++;

		INC_COUNTER64(pAd->WlanCounters.ReceivedFragmentCount);

		if (pRxWI->MPDUtotalByteCount < 14)
			Status = NDIS_STATUS_FAILURE;

		if (MONITOR_ON(pAd)) {
			send_monitor_packets(pAd, &RxCell);
			break;
		}

		/* STARxDoneInterruptHandle() is called in rtusb_bulk.c */

		/* Check for all RxD errors */
		Status = RTMPCheckRxError(pAd, pHeader, pRxWI, pRxD);

		/* Handle the received frame */
		if (Status == NDIS_STATUS_SUCCESS) {
			switch (pHeader->FC.Type) {
				/* CASE I, receive a DATA frame */
			case BTYPE_DATA:
				{
					/* process DATA frame */
					STAHandleRxDataFrame(pAd, &RxCell);
				}
				break;
				/* CASE II, receive a MGMT frame */
			case BTYPE_MGMT:
				{
					STAHandleRxMgmtFrame(pAd, &RxCell);
				}
				break;
				/* CASE III. receive a CNTL frame */
			case BTYPE_CNTL:
				{
					STAHandleRxControlFrame(pAd, &RxCell);
				}
				break;
				/* discard other type */
			default:
				RELEASE_NDIS_PACKET(pAd, pRxPacket,
						    NDIS_STATUS_FAILURE);
				break;
			}
		} else {
			pAd->Counters8023.RxErrors++;
			/* discard this frame */
			RELEASE_NDIS_PACKET(pAd, pRxPacket,
					    NDIS_STATUS_FAILURE);
		}
	}

	return bReschedule;
}

/*
	========================================================================

	Routine Description:
	Arguments:
		pAd 	Pointer to our adapter

	IRQL = DISPATCH_LEVEL

	========================================================================
*/
void RTMPHandleTwakeupInterrupt(struct rt_rtmp_adapter *pAd)
{
	AsicForceWakeup(pAd, FALSE);
}

/*
========================================================================
Routine Description:
    Early checking and OS-depened parsing for Tx packet send to our STA driver.

Arguments:
    void *	MiniportAdapterContext	Pointer refer to the device handle, i.e., the pAd.
	void **	ppPacketArray			The packet array need to do transmission.
	u32			NumberOfPackets			Number of packet in packet array.

Return Value:
	NONE

Note:
	This function do early checking and classification for send-out packet.
	You only can put OS-depened & STA related code in here.
========================================================================
*/
void STASendPackets(void *MiniportAdapterContext,
		    void **ppPacketArray, u32 NumberOfPackets)
{
	u32 Index;
	struct rt_rtmp_adapter *pAd = (struct rt_rtmp_adapter *)MiniportAdapterContext;
	void *pPacket;
	BOOLEAN allowToSend = FALSE;

	for (Index = 0; Index < NumberOfPackets; Index++) {
		pPacket = ppPacketArray[Index];

		do {
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)
			    || RTMP_TEST_FLAG(pAd,
					      fRTMP_ADAPTER_HALT_IN_PROGRESS)
			    || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)) {
				/* Drop send request since hardware is in reset state */
				break;
			} else if (!INFRA_ON(pAd) && !ADHOC_ON(pAd)) {
				/* Drop send request since there are no physical connection yet */
				break;
			} else {
				/* Record that orignal packet source is from NDIS layer,so that */
				/* later on driver knows how to release this NDIS PACKET */
				RTMP_SET_PACKET_WCID(pPacket, 0);	/* this field is useless when in STA mode */
				RTMP_SET_PACKET_SOURCE(pPacket, PKTSRC_NDIS);
				NDIS_SET_PACKET_STATUS(pPacket,
						       NDIS_STATUS_PENDING);
				pAd->RalinkCounters.PendingNdisPacketCount++;

				allowToSend = TRUE;
			}
		} while (FALSE);

		if (allowToSend == TRUE)
			STASendPacket(pAd, pPacket);
		else
			RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
	}

	/* Dequeue outgoing frames from TxSwQueue[] and process it */
	RTMPDeQueuePacket(pAd, FALSE, NUM_OF_TX_RING, MAX_TX_PROCESS);

}

/*
========================================================================
Routine Description:
	This routine is used to do packet parsing and classification for Tx packet
	to STA device, and it will en-queue packets to our TxSwQueue depends on AC
	class.

Arguments:
	pAd    		Pointer to our adapter
	pPacket 	Pointer to send packet

Return Value:
	NDIS_STATUS_SUCCESS			If success to queue the packet into TxSwQueue.
	NDIS_STATUS_FAILURE			If failed to do en-queue.

Note:
	You only can put OS-indepened & STA related code in here.
========================================================================
*/
int STASendPacket(struct rt_rtmp_adapter *pAd, void *pPacket)
{
	struct rt_packet_info PacketInfo;
	u8 *pSrcBufVA;
	u32 SrcBufLen;
	u32 AllowFragSize;
	u8 NumberOfFrag;
	u8 RTSRequired;
	u8 QueIdx, UserPriority;
	struct rt_mac_table_entry *pEntry = NULL;
	unsigned int IrqFlags;
	u8 FlgIsIP = 0;
	u8 Rate;

	/* Prepare packet information structure for buffer descriptor */
	/* chained within a single NDIS packet. */
	RTMP_QueryPacketInfo(pPacket, &PacketInfo, &pSrcBufVA, &SrcBufLen);

	if (pSrcBufVA == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("STASendPacket --> pSrcBufVA == NULL !SrcBufLen=%x\n",
			  SrcBufLen));
		/* Resourece is low, system did not allocate virtual address */
		/* return NDIS_STATUS_FAILURE directly to upper layer */
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		return NDIS_STATUS_FAILURE;
	}

	if (SrcBufLen < 14) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("STASendPacket --> Ndis Packet buffer error!\n"));
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		return (NDIS_STATUS_FAILURE);
	}
	/* In HT rate adhoc mode, A-MPDU is often used. So need to lookup BA Table and MAC Entry. */
	/* Note multicast packets in adhoc also use BSSID_WCID index. */
	{
		if (INFRA_ON(pAd)) {
			{
				pEntry = &pAd->MacTab.Content[BSSID_WCID];
				RTMP_SET_PACKET_WCID(pPacket, BSSID_WCID);
				Rate = pAd->CommonCfg.TxRate;
			}
		} else if (ADHOC_ON(pAd)) {
			if (*pSrcBufVA & 0x01) {
				RTMP_SET_PACKET_WCID(pPacket, MCAST_WCID);
				pEntry = &pAd->MacTab.Content[MCAST_WCID];
			} else {
				pEntry = MacTableLookup(pAd, pSrcBufVA);
			}
			Rate = pAd->CommonCfg.TxRate;
		}
	}

	if (!pEntry) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("STASendPacket->Cannot find pEntry(%2x:%2x:%2x:%2x:%2x:%2x) in MacTab!\n",
			  PRINT_MAC(pSrcBufVA)));
		/* Resourece is low, system did not allocate virtual address */
		/* return NDIS_STATUS_FAILURE directly to upper layer */
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		return NDIS_STATUS_FAILURE;
	}

	if (ADHOC_ON(pAd)
	    ) {
		RTMP_SET_PACKET_WCID(pPacket, (u8)pEntry->Aid);
	}
	/* */
	/* Check the Ethernet Frame type of this packet, and set the RTMP_SET_PACKET_SPECIFIC flags. */
	/*              Here we set the PACKET_SPECIFIC flags(LLC, VLAN, DHCP/ARP, EAPOL). */
	RTMPCheckEtherType(pAd, pPacket);

	/* */
	/* WPA 802.1x secured port control - drop all non-802.1x frame before port secured */
	/* */
	if (((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
	     || (pAd->StaCfg.IEEE8021X == TRUE)
	    )
	    && ((pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)
		|| (pAd->StaCfg.MicErrCnt >= 2))
	    && (RTMP_GET_PACKET_EAPOL(pPacket) == FALSE)
	    ) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("STASendPacket --> Drop packet before port secured!\n"));
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);

		return (NDIS_STATUS_FAILURE);
	}

	/* STEP 1. Decide number of fragments required to deliver this MSDU. */
	/*         The estimation here is not very accurate because difficult to */
	/*         take encryption overhead into consideration here. The result */
	/*         "NumberOfFrag" is then just used to pre-check if enough free */
	/*         TXD are available to hold this MSDU. */

	if (*pSrcBufVA & 0x01)	/* fragmentation not allowed on multicast & broadcast */
		NumberOfFrag = 1;
	else if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED))
		NumberOfFrag = 1;	/* Aggregation overwhelms fragmentation */
	else if (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_AMSDU_INUSED))
		NumberOfFrag = 1;	/* Aggregation overwhelms fragmentation */
	else if ((pAd->StaCfg.HTPhyMode.field.MODE == MODE_HTMIX)
		 || (pAd->StaCfg.HTPhyMode.field.MODE == MODE_HTGREENFIELD))
		NumberOfFrag = 1;	/* MIMO RATE overwhelms fragmentation */
	else {
		/* The calculated "NumberOfFrag" is a rough estimation because of various */
		/* encryption/encapsulation overhead not taken into consideration. This number is just */
		/* used to make sure enough free TXD are available before fragmentation takes place. */
		/* In case the actual required number of fragments of an NDIS packet */
		/* excceeds "NumberOfFrag"caculated here and not enough free TXD available, the */
		/* last fragment (i.e. last MPDU) will be dropped in RTMPHardTransmit() due to out of */
		/* resource, and the NDIS packet will be indicated NDIS_STATUS_FAILURE. This should */
		/* rarely happen and the penalty is just like a TX RETRY fail. Affordable. */

		AllowFragSize =
		    (pAd->CommonCfg.FragmentThreshold) - LENGTH_802_11 -
		    LENGTH_CRC;
		NumberOfFrag =
		    ((PacketInfo.TotalPacketLength - LENGTH_802_3 +
		      LENGTH_802_1_H) / AllowFragSize) + 1;
		/* To get accurate number of fragmentation, Minus 1 if the size just match to allowable fragment size */
		if (((PacketInfo.TotalPacketLength - LENGTH_802_3 +
		      LENGTH_802_1_H) % AllowFragSize) == 0) {
			NumberOfFrag--;
		}
	}

	/* Save fragment number to Ndis packet reserved field */
	RTMP_SET_PACKET_FRAGMENTS(pPacket, NumberOfFrag);

	/* STEP 2. Check the requirement of RTS: */
	/*         If multiple fragment required, RTS is required only for the first fragment */
	/*         if the fragment size large than RTS threshold */
	/*     For RT28xx, Let ASIC send RTS/CTS */
/*      RTMP_SET_PACKET_RTS(pPacket, 0); */
	if (NumberOfFrag > 1)
		RTSRequired =
		    (pAd->CommonCfg.FragmentThreshold >
		     pAd->CommonCfg.RtsThreshold) ? 1 : 0;
	else
		RTSRequired =
		    (PacketInfo.TotalPacketLength >
		     pAd->CommonCfg.RtsThreshold) ? 1 : 0;

	/* Save RTS requirement to Ndis packet reserved field */
	RTMP_SET_PACKET_RTS(pPacket, RTSRequired);
	RTMP_SET_PACKET_TXRATE(pPacket, pAd->CommonCfg.TxRate);

	/* */
	/* STEP 3. Traffic classification. outcome = <UserPriority, QueIdx> */
	/* */
	UserPriority = 0;
	QueIdx = QID_AC_BE;
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) &&
	    CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_WMM_CAPABLE)) {
		u16 Protocol;
		u8 LlcSnapLen = 0, Byte0, Byte1;
		do {
			/* get Ethernet protocol field */
			Protocol =
			    (u16)((pSrcBufVA[12] << 8) + pSrcBufVA[13]);
			if (Protocol <= 1500) {
				/* get Ethernet protocol field from LLC/SNAP */
				if (Sniff2BytesFromNdisBuffer
				    (PacketInfo.pFirstBuffer, LENGTH_802_3 + 6,
				     &Byte0, &Byte1) != NDIS_STATUS_SUCCESS)
					break;

				Protocol = (u16)((Byte0 << 8) + Byte1);
				LlcSnapLen = 8;
			}
			/* always AC_BE for non-IP packet */
			if (Protocol != 0x0800)
				break;

			/* get IP header */
			if (Sniff2BytesFromNdisBuffer
			    (PacketInfo.pFirstBuffer, LENGTH_802_3 + LlcSnapLen,
			     &Byte0, &Byte1) != NDIS_STATUS_SUCCESS)
				break;

			/* return AC_BE if packet is not IPv4 */
			if ((Byte0 & 0xf0) != 0x40)
				break;

			FlgIsIP = 1;
			UserPriority = (Byte1 & 0xe0) >> 5;
			QueIdx = MapUserPriorityToAccessCategory[UserPriority];

			/* TODO: have to check ACM bit. apply TSPEC if ACM is ON */
			/* TODO: downgrade UP & QueIdx before passing ACM */
			/*
			   Under WMM ACM control, we dont need to check the bit;
			   Or when a TSPEC is built for VO but we will change to issue
			   BA session for BE here, so we will not use BA to send VO packets.
			 */
			if (pAd->CommonCfg.APEdcaParm.bACM[QueIdx]) {
				UserPriority = 0;
				QueIdx = QID_AC_BE;
			}
		} while (FALSE);
	}

	RTMP_SET_PACKET_UP(pPacket, UserPriority);

	/* Make sure SendTxWait queue resource won't be used by other threads */
	RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
	if (pAd->TxSwQueue[QueIdx].Number >= MAX_PACKETS_IN_QUEUE) {
		RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);

		return NDIS_STATUS_FAILURE;
	} else {
		InsertTailQueueAc(pAd, pEntry, &pAd->TxSwQueue[QueIdx],
				  PACKET_TO_QUEUE_ENTRY(pPacket));
	}
	RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);

	if ((pAd->CommonCfg.BACapability.field.AutoBA == TRUE) &&
	    IS_HT_STA(pEntry)) {
		/*struct rt_mac_table_entry *pMacEntry = &pAd->MacTab.Content[BSSID_WCID]; */
		if (((pEntry->TXBAbitmap & (1 << UserPriority)) == 0) &&
		    ((pEntry->BADeclineBitmap & (1 << UserPriority)) == 0) &&
		    (pEntry->PortSecured == WPA_802_1X_PORT_SECURED)
		    /* For IOT compatibility, if */
		    /* 1. It is Ralink chip or */
		    /* 2. It is OPEN or AES mode, */
		    /* then BA session can be bulit. */
		    && ((pEntry->ValidAsCLI && pAd->MlmeAux.APRalinkIe != 0x0)
			|| (pEntry->WepStatus != Ndis802_11WEPEnabled
			    && pEntry->WepStatus !=
			    Ndis802_11Encryption2Enabled))
		    ) {
			BAOriSessionSetUp(pAd, pEntry, UserPriority, 0, 10,
					  FALSE);
		}
	}

	pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++;	/* TODO: for debug only. to be removed */
	return NDIS_STATUS_SUCCESS;
}

/*
	========================================================================

	Routine Description:
		This subroutine will scan through releative ring descriptor to find
		out avaliable free ring descriptor and compare with request size.

	Arguments:
		pAd Pointer to our adapter
		QueIdx		Selected TX Ring

	Return Value:
		NDIS_STATUS_FAILURE 	Not enough free descriptor
		NDIS_STATUS_SUCCESS 	Enough free descriptor

	IRQL = PASSIVE_LEVEL
	IRQL = DISPATCH_LEVEL

	Note:

	========================================================================
*/
#ifdef RTMP_MAC_PCI
int RTMPFreeTXDRequest(struct rt_rtmp_adapter *pAd,
			       u8 QueIdx,
			       u8 NumberRequired, u8 *FreeNumberIs)
{
	unsigned long FreeNumber = 0;
	int Status = NDIS_STATUS_FAILURE;

	switch (QueIdx) {
	case QID_AC_BK:
	case QID_AC_BE:
	case QID_AC_VI:
	case QID_AC_VO:
		if (pAd->TxRing[QueIdx].TxSwFreeIdx >
		    pAd->TxRing[QueIdx].TxCpuIdx)
			FreeNumber =
			    pAd->TxRing[QueIdx].TxSwFreeIdx -
			    pAd->TxRing[QueIdx].TxCpuIdx - 1;
		else
			FreeNumber =
			    pAd->TxRing[QueIdx].TxSwFreeIdx + TX_RING_SIZE -
			    pAd->TxRing[QueIdx].TxCpuIdx - 1;

		if (FreeNumber >= NumberRequired)
			Status = NDIS_STATUS_SUCCESS;
		break;

	case QID_MGMT:
		if (pAd->MgmtRing.TxSwFreeIdx > pAd->MgmtRing.TxCpuIdx)
			FreeNumber =
			    pAd->MgmtRing.TxSwFreeIdx - pAd->MgmtRing.TxCpuIdx -
			    1;
		else
			FreeNumber =
			    pAd->MgmtRing.TxSwFreeIdx + MGMT_RING_SIZE -
			    pAd->MgmtRing.TxCpuIdx - 1;

		if (FreeNumber >= NumberRequired)
			Status = NDIS_STATUS_SUCCESS;
		break;

	default:
		DBGPRINT(RT_DEBUG_ERROR,
			 ("RTMPFreeTXDRequest::Invalid QueIdx(=%d)\n", QueIdx));
		break;
	}
	*FreeNumberIs = (u8)FreeNumber;

	return (Status);
}
#endif /* RTMP_MAC_PCI // */
#ifdef RTMP_MAC_USB
/*
	Actually, this function used to check if the TxHardware Queue still has frame need to send.
	If no frame need to send, go to sleep, else, still wake up.
*/
int RTMPFreeTXDRequest(struct rt_rtmp_adapter *pAd,
			       u8 QueIdx,
			       u8 NumberRequired, u8 *FreeNumberIs)
{
	/*unsigned long         FreeNumber = 0; */
	int Status = NDIS_STATUS_FAILURE;
	unsigned long IrqFlags;
	struct rt_ht_tx_context *pHTTXContext;

	switch (QueIdx) {
	case QID_AC_BK:
	case QID_AC_BE:
	case QID_AC_VI:
	case QID_AC_VO:
		{
			pHTTXContext = &pAd->TxContext[QueIdx];
			RTMP_IRQ_LOCK(&pAd->TxContextQueueLock[QueIdx],
				      IrqFlags);
			if ((pHTTXContext->CurWritePosition !=
			     pHTTXContext->ENextBulkOutPosition)
			    || (pHTTXContext->IRPPending == TRUE)) {
				Status = NDIS_STATUS_FAILURE;
			} else {
				Status = NDIS_STATUS_SUCCESS;
			}
			RTMP_IRQ_UNLOCK(&pAd->TxContextQueueLock[QueIdx],
					IrqFlags);
		}
		break;
	case QID_MGMT:
		if (pAd->MgmtRing.TxSwFreeIdx != MGMT_RING_SIZE)
			Status = NDIS_STATUS_FAILURE;
		else
			Status = NDIS_STATUS_SUCCESS;
		break;
	default:
		DBGPRINT(RT_DEBUG_ERROR,
			 ("RTMPFreeTXDRequest::Invalid QueIdx(=%d)\n", QueIdx));
		break;
	}

	return (Status);
}
#endif /* RTMP_MAC_USB // */

void RTMPSendDisassociationFrame(struct rt_rtmp_adapter *pAd)
{
}

void RTMPSendNullFrame(struct rt_rtmp_adapter *pAd,
		       u8 TxRate, IN BOOLEAN bQosNull)
{
	u8 NullFrame[48];
	unsigned long Length;
	struct rt_header_802_11 * pHeader_802_11;

	/* WPA 802.1x secured port control */
	if (((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
	     (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
	     || (pAd->StaCfg.IEEE8021X == TRUE)
	    ) && (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)) {
		return;
	}

	NdisZeroMemory(NullFrame, 48);
	Length = sizeof(struct rt_header_802_11);

	pHeader_802_11 = (struct rt_header_802_11 *) NullFrame;

	pHeader_802_11->FC.Type = BTYPE_DATA;
	pHeader_802_11->FC.SubType = SUBTYPE_NULL_FUNC;
	pHeader_802_11->FC.ToDs = 1;
	COPY_MAC_ADDR(pHeader_802_11->Addr1, pAd->CommonCfg.Bssid);
	COPY_MAC_ADDR(pHeader_802_11->Addr2, pAd->CurrentAddress);
	COPY_MAC_ADDR(pHeader_802_11->Addr3, pAd->CommonCfg.Bssid);

	if (pAd->CommonCfg.bAPSDForcePowerSave) {
		pHeader_802_11->FC.PwrMgmt = PWR_SAVE;
	} else {
		pHeader_802_11->FC.PwrMgmt =
		    (pAd->StaCfg.Psm == PWR_SAVE) ? 1 : 0;
	}
	pHeader_802_11->Duration =
	    pAd->CommonCfg.Dsifs + RTMPCalcDuration(pAd, TxRate, 14);

	pAd->Sequence++;
	pHeader_802_11->Sequence = pAd->Sequence;

	/* Prepare QosNull function frame */
	if (bQosNull) {
		pHeader_802_11->FC.SubType = SUBTYPE_QOS_NULL;

		/* copy QOS control bytes */
		NullFrame[Length] = 0;
		NullFrame[Length + 1] = 0;
		Length += 2;	/* if pad with 2 bytes for alignment, APSD will fail */
	}

	HAL_KickOutNullFrameTx(pAd, 0, NullFrame, Length);

}

/* IRQL = DISPATCH_LEVEL */
void RTMPSendRTSFrame(struct rt_rtmp_adapter *pAd,
		      u8 *pDA,
		      IN unsigned int NextMpduSize,
		      u8 TxRate,
		      u8 RTSRate,
		      u16 AckDuration, u8 QueIdx, u8 FrameGap)
{
}

/* -------------------------------------------------------- */
/*  FIND ENCRYPT KEY AND DECIDE CIPHER ALGORITHM */
/*              Find the WPA key, either Group or Pairwise Key */
/*              LEAP + TKIP also use WPA key. */
/* -------------------------------------------------------- */
/* Decide WEP bit and cipher suite to be used. Same cipher suite should be used for whole fragment burst */
/* In Cisco CCX 2.0 Leap Authentication */
/*                 WepStatus is Ndis802_11Encryption1Enabled but the key will use PairwiseKey */
/*                 Instead of the SharedKey, SharedKey Length may be Zero. */
void STAFindCipherAlgorithm(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	NDIS_802_11_ENCRYPTION_STATUS Cipher;	/* To indicate cipher used for this packet */
	u8 CipherAlg = CIPHER_NONE;	/* cipher alogrithm */
	u8 KeyIdx = 0xff;
	u8 *pSrcBufVA;
	struct rt_cipher_key *pKey = NULL;

	pSrcBufVA = GET_OS_PKT_DATAPTR(pTxBlk->pPacket);

	{
		/* Select Cipher */
		if ((*pSrcBufVA & 0x01) && (ADHOC_ON(pAd)))
			Cipher = pAd->StaCfg.GroupCipher;	/* Cipher for Multicast or Broadcast */
		else
			Cipher = pAd->StaCfg.PairCipher;	/* Cipher for Unicast */

		if (RTMP_GET_PACKET_EAPOL(pTxBlk->pPacket)) {
			ASSERT(pAd->SharedKey[BSS0][0].CipherAlg <=
			       CIPHER_CKIP128);

			/* 4-way handshaking frame must be clear */
			if (!(TX_BLK_TEST_FLAG(pTxBlk, fTX_bClearEAPFrame))
			    && (pAd->SharedKey[BSS0][0].CipherAlg)
			    && (pAd->SharedKey[BSS0][0].KeyLen)) {
				CipherAlg = pAd->SharedKey[BSS0][0].CipherAlg;
				KeyIdx = 0;
			}
		} else if (Cipher == Ndis802_11Encryption1Enabled) {
			KeyIdx = pAd->StaCfg.DefaultKeyId;
		} else if ((Cipher == Ndis802_11Encryption2Enabled) ||
			   (Cipher == Ndis802_11Encryption3Enabled)) {
			if ((*pSrcBufVA & 0x01) && (ADHOC_ON(pAd)))	/* multicast */
				KeyIdx = pAd->StaCfg.DefaultKeyId;
			else if (pAd->SharedKey[BSS0][0].KeyLen)
				KeyIdx = 0;
			else
				KeyIdx = pAd->StaCfg.DefaultKeyId;
		}

		if (KeyIdx == 0xff)
			CipherAlg = CIPHER_NONE;
		else if ((Cipher == Ndis802_11EncryptionDisabled)
			 || (pAd->SharedKey[BSS0][KeyIdx].KeyLen == 0))
			CipherAlg = CIPHER_NONE;
		else if (pAd->StaCfg.WpaSupplicantUP &&
			 (Cipher == Ndis802_11Encryption1Enabled) &&
			 (pAd->StaCfg.IEEE8021X == TRUE) &&
			 (pAd->StaCfg.PortSecured ==
			  WPA_802_1X_PORT_NOT_SECURED))
			CipherAlg = CIPHER_NONE;
		else {
			/*Header_802_11.FC.Wep = 1; */
			CipherAlg = pAd->SharedKey[BSS0][KeyIdx].CipherAlg;
			pKey = &pAd->SharedKey[BSS0][KeyIdx];
		}
	}

	pTxBlk->CipherAlg = CipherAlg;
	pTxBlk->pKey = pKey;
}

void STABuildCommon802_11Header(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	struct rt_header_802_11 *pHeader_802_11;

	/* */
	/* MAKE A COMMON 802.11 HEADER */
	/* */

	/* normal wlan header size : 24 octets */
	pTxBlk->MpduHeaderLen = sizeof(struct rt_header_802_11);

	pHeader_802_11 =
	    (struct rt_header_802_11 *) & pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];

	NdisZeroMemory(pHeader_802_11, sizeof(struct rt_header_802_11));

	pHeader_802_11->FC.FrDs = 0;
	pHeader_802_11->FC.Type = BTYPE_DATA;
	pHeader_802_11->FC.SubType =
	    ((TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) ? SUBTYPE_QDATA :
	     SUBTYPE_DATA);

	if (pTxBlk->pMacEntry) {
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bForceNonQoS)) {
			pHeader_802_11->Sequence =
			    pTxBlk->pMacEntry->NonQosDataSeq;
			pTxBlk->pMacEntry->NonQosDataSeq =
			    (pTxBlk->pMacEntry->NonQosDataSeq + 1) & MAXSEQ;
		} else {
			{
				pHeader_802_11->Sequence =
				    pTxBlk->pMacEntry->TxSeq[pTxBlk->
							     UserPriority];
				pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority] =
				    (pTxBlk->pMacEntry->
				     TxSeq[pTxBlk->UserPriority] + 1) & MAXSEQ;
			}
		}
	} else {
		pHeader_802_11->Sequence = pAd->Sequence;
		pAd->Sequence = (pAd->Sequence + 1) & MAXSEQ;	/* next sequence */
	}

	pHeader_802_11->Frag = 0;

	pHeader_802_11->FC.MoreData = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

	{
		if (INFRA_ON(pAd)) {
			{
				COPY_MAC_ADDR(pHeader_802_11->Addr1,
					      pAd->CommonCfg.Bssid);
				COPY_MAC_ADDR(pHeader_802_11->Addr2,
					      pAd->CurrentAddress);
				COPY_MAC_ADDR(pHeader_802_11->Addr3,
					      pTxBlk->pSrcBufHeader);
				pHeader_802_11->FC.ToDs = 1;
			}
		} else if (ADHOC_ON(pAd)) {
			COPY_MAC_ADDR(pHeader_802_11->Addr1,
				      pTxBlk->pSrcBufHeader);
			COPY_MAC_ADDR(pHeader_802_11->Addr2,
				      pAd->CurrentAddress);
			COPY_MAC_ADDR(pHeader_802_11->Addr3,
				      pAd->CommonCfg.Bssid);
			pHeader_802_11->FC.ToDs = 0;
		}
	}

	if (pTxBlk->CipherAlg != CIPHER_NONE)
		pHeader_802_11->FC.Wep = 1;

	/* ----------------------------------------------------------------- */
	/* STEP 2. MAKE A COMMON 802.11 HEADER SHARED BY ENTIRE FRAGMENT BURST. Fill sequence later. */
	/* ----------------------------------------------------------------- */
	if (pAd->CommonCfg.bAPSDForcePowerSave)
		pHeader_802_11->FC.PwrMgmt = PWR_SAVE;
	else
		pHeader_802_11->FC.PwrMgmt = (pAd->StaCfg.Psm == PWR_SAVE);
}

void STABuildCache802_11Header(struct rt_rtmp_adapter *pAd,
			       struct rt_tx_blk *pTxBlk, u8 * pHeader)
{
	struct rt_mac_table_entry *pMacEntry;
	struct rt_header_802_11 * pHeader80211;

	pHeader80211 = (struct rt_header_802_11 *) pHeader;
	pMacEntry = pTxBlk->pMacEntry;

	/* */
	/* Update the cached 802.11 HEADER */
	/* */

	/* normal wlan header size : 24 octets */
	pTxBlk->MpduHeaderLen = sizeof(struct rt_header_802_11);

	/* More Bit */
	pHeader80211->FC.MoreData = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

	/* Sequence */
	pHeader80211->Sequence = pMacEntry->TxSeq[pTxBlk->UserPriority];
	pMacEntry->TxSeq[pTxBlk->UserPriority] =
	    (pMacEntry->TxSeq[pTxBlk->UserPriority] + 1) & MAXSEQ;

	{
		/* Check if the frame can be sent through DLS direct link interface */
		/* If packet can be sent through DLS, then force aggregation disable. (Hard to determine peer STA's capability) */

		/* The addr3 of normal packet send from DS is Dest Mac address. */
		if (ADHOC_ON(pAd))
			COPY_MAC_ADDR(pHeader80211->Addr3,
				      pAd->CommonCfg.Bssid);
		else
			COPY_MAC_ADDR(pHeader80211->Addr3,
				      pTxBlk->pSrcBufHeader);
	}

	/* ----------------------------------------------------------------- */
	/* STEP 2. MAKE A COMMON 802.11 HEADER SHARED BY ENTIRE FRAGMENT BURST. Fill sequence later. */
	/* ----------------------------------------------------------------- */
	if (pAd->CommonCfg.bAPSDForcePowerSave)
		pHeader80211->FC.PwrMgmt = PWR_SAVE;
	else
		pHeader80211->FC.PwrMgmt = (pAd->StaCfg.Psm == PWR_SAVE);
}

static inline u8 *STA_Build_ARalink_Frame_Header(struct rt_rtmp_adapter *pAd,
						    struct rt_tx_blk *pTxBlk)
{
	u8 *pHeaderBufPtr;
	struct rt_header_802_11 *pHeader_802_11;
	void *pNextPacket;
	u32 nextBufLen;
	struct rt_queue_entry *pQEntry;

	STAFindCipherAlgorithm(pAd, pTxBlk);
	STABuildCommon802_11Header(pAd, pTxBlk);

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];
	pHeader_802_11 = (struct rt_header_802_11 *) pHeaderBufPtr;

	/* steal "order" bit to mark "aggregation" */
	pHeader_802_11->FC.Order = 1;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) {
		/* */
		/* build QOS Control bytes */
		/* */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);

		*(pHeaderBufPtr + 1) = 0;
		pHeaderBufPtr += 2;
		pTxBlk->MpduHeaderLen += 2;
	}
	/* padding at front of LLC header. LLC header should at 4-bytes aligment. */
	pTxBlk->HdrPadLen = (unsigned long)pHeaderBufPtr;
	pHeaderBufPtr = (u8 *)ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (unsigned long)(pHeaderBufPtr - pTxBlk->HdrPadLen);

	/* For RA Aggregation, */
	/* put the 2nd MSDU length(extra 2-byte field) after struct rt_qos_control in little endian format */
	pQEntry = pTxBlk->TxPacketList.Head;
	pNextPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	nextBufLen = GET_OS_PKT_LEN(pNextPacket);
	if (RTMP_GET_PACKET_VLAN(pNextPacket))
		nextBufLen -= LENGTH_802_1Q;

	*pHeaderBufPtr = (u8)nextBufLen & 0xff;
	*(pHeaderBufPtr + 1) = (u8)(nextBufLen >> 8);

	pHeaderBufPtr += 2;
	pTxBlk->MpduHeaderLen += 2;

	return pHeaderBufPtr;

}

static inline u8 *STA_Build_AMSDU_Frame_Header(struct rt_rtmp_adapter *pAd,
						  struct rt_tx_blk *pTxBlk)
{
	u8 *pHeaderBufPtr;	/*, pSaveBufPtr; */
	struct rt_header_802_11 *pHeader_802_11;

	STAFindCipherAlgorithm(pAd, pTxBlk);
	STABuildCommon802_11Header(pAd, pTxBlk);

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];
	pHeader_802_11 = (struct rt_header_802_11 *) pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	/* */
	/* build QOS Control bytes */
	/* */
	*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);

	/* */
	/* A-MSDU packet */
	/* */
	*pHeaderBufPtr |= 0x80;

	*(pHeaderBufPtr + 1) = 0;
	pHeaderBufPtr += 2;
	pTxBlk->MpduHeaderLen += 2;

	/*pSaveBufPtr = pHeaderBufPtr; */

	/* */
	/* padding at front of LLC header */
	/* LLC header should locate at 4-octets aligment */
	/* */
	/* @@@ MpduHeaderLen excluding padding @@@ */
	/* */
	pTxBlk->HdrPadLen = (unsigned long)pHeaderBufPtr;
	pHeaderBufPtr = (u8 *)ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (unsigned long)(pHeaderBufPtr - pTxBlk->HdrPadLen);

	return pHeaderBufPtr;

}

void STA_AMPDU_Frame_Tx(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	struct rt_header_802_11 *pHeader_802_11;
	u8 *pHeaderBufPtr;
	u16 FreeNumber;
	struct rt_mac_table_entry *pMacEntry;
	BOOLEAN bVLANPkt;
	struct rt_queue_entry *pQEntry;

	ASSERT(pTxBlk);

	while (pTxBlk->TxPacketList.Head) {
		pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
		pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
		if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE) {
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket,
					    NDIS_STATUS_FAILURE);
			continue;
		}

		bVLANPkt =
		    (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

		pMacEntry = pTxBlk->pMacEntry;
		if (pMacEntry->isCached) {
			/* NOTE: Please make sure the size of pMacEntry->CachedBuf[] is smaller than pTxBlk->HeaderBuf[]! */
			NdisMoveMemory((u8 *)& pTxBlk->
				       HeaderBuf[TXINFO_SIZE],
				       (u8 *)& pMacEntry->CachedBuf[0],
				       TXWI_SIZE + sizeof(struct rt_header_802_11));
			pHeaderBufPtr =
			    (u8 *)(&pTxBlk->
				      HeaderBuf[TXINFO_SIZE + TXWI_SIZE]);
			STABuildCache802_11Header(pAd, pTxBlk, pHeaderBufPtr);
		} else {
			STAFindCipherAlgorithm(pAd, pTxBlk);
			STABuildCommon802_11Header(pAd, pTxBlk);

			pHeaderBufPtr =
			    &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];
		}

		pHeader_802_11 = (struct rt_header_802_11 *) pHeaderBufPtr;

		/* skip common header */
		pHeaderBufPtr += pTxBlk->MpduHeaderLen;

		/* */
		/* build QOS Control bytes */
		/* */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
		*(pHeaderBufPtr + 1) = 0;
		pHeaderBufPtr += 2;
		pTxBlk->MpduHeaderLen += 2;

		/* */
		/* build HTC+ */
		/* HTC control filed following QoS field */
		/* */
		if ((pAd->CommonCfg.bRdg == TRUE)
		    && CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry,
					       fCLIENT_STATUS_RDG_CAPABLE)) {
			if (pMacEntry->isCached == FALSE) {
				/* mark HTC bit */
				pHeader_802_11->FC.Order = 1;

				NdisZeroMemory(pHeaderBufPtr, 4);
				*(pHeaderBufPtr + 3) |= 0x80;
			}
			pHeaderBufPtr += 4;
			pTxBlk->MpduHeaderLen += 4;
		}
		/*pTxBlk->MpduHeaderLen = pHeaderBufPtr - pTxBlk->HeaderBuf - TXWI_SIZE - TXINFO_SIZE; */
		ASSERT(pTxBlk->MpduHeaderLen >= 24);

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen -= LENGTH_802_3;

		/* skip vlan tag */
		if (bVLANPkt) {
			pTxBlk->pSrcBufData += LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}
		/* */
		/* padding at front of LLC header */
		/* LLC header should locate at 4-octets aligment */
		/* */
		/* @@@ MpduHeaderLen excluding padding @@@ */
		/* */
		pTxBlk->HdrPadLen = (unsigned long)pHeaderBufPtr;
		pHeaderBufPtr = (u8 *)ROUND_UP(pHeaderBufPtr, 4);
		pTxBlk->HdrPadLen = (unsigned long)(pHeaderBufPtr - pTxBlk->HdrPadLen);

		{

			/* */
			/* Insert LLC-SNAP encapsulation - 8 octets */
			/* */
			EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->
							    pSrcBufData - 2,
							    pTxBlk->
							    pExtraLlcSnapEncap);
			if (pTxBlk->pExtraLlcSnapEncap) {
				NdisMoveMemory(pHeaderBufPtr,
					       pTxBlk->pExtraLlcSnapEncap, 6);
				pHeaderBufPtr += 6;
				/* get 2 octets (TypeofLen) */
				NdisMoveMemory(pHeaderBufPtr,
					       pTxBlk->pSrcBufData - 2, 2);
				pHeaderBufPtr += 2;
				pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			}

		}

		if (pMacEntry->isCached) {
			RTMPWriteTxWI_Cache(pAd,
					    (struct rt_txwi *) (&pTxBlk->
							   HeaderBuf
							   [TXINFO_SIZE]),
					    pTxBlk);
		} else {
			RTMPWriteTxWI_Data(pAd,
					   (struct rt_txwi *) (&pTxBlk->
							  HeaderBuf
							  [TXINFO_SIZE]),
					   pTxBlk);

			NdisZeroMemory((u8 *)(&pMacEntry->CachedBuf[0]),
				       sizeof(pMacEntry->CachedBuf));
			NdisMoveMemory((u8 *)(&pMacEntry->CachedBuf[0]),
				       (u8 *)(&pTxBlk->
						 HeaderBuf[TXINFO_SIZE]),
				       (pHeaderBufPtr -
					(u8 *)(&pTxBlk->
						  HeaderBuf[TXINFO_SIZE])));
			pMacEntry->isCached = TRUE;
		}

		/* calculate Transmitted AMPDU count and ByteCount */
		{
			pAd->RalinkCounters.TransmittedMPDUsInAMPDUCount.u.
			    LowPart++;
			pAd->RalinkCounters.TransmittedOctetsInAMPDUCount.
			    QuadPart += pTxBlk->SrcBufLen;
		}

		/*FreeNumber = GET_TXRING_FREENO(pAd, QueIdx); */

		HAL_WriteTxResource(pAd, pTxBlk, TRUE, &FreeNumber);

		/* */
		/* Kick out Tx */
		/* */
		if (!RTMP_TEST_PSFLAG(pAd, fRTMP_PS_DISABLE_TX))
			HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;
	}

}

void STA_AMSDU_Frame_Tx(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	u8 *pHeaderBufPtr;
	u16 FreeNumber;
	u16 subFramePayloadLen = 0;	/* AMSDU Subframe length without AMSDU-Header / Padding. */
	u16 totalMPDUSize = 0;
	u8 *subFrameHeader;
	u8 padding = 0;
	u16 FirstTx = 0, LastTxIdx = 0;
	BOOLEAN bVLANPkt;
	int frameNum = 0;
	struct rt_queue_entry *pQEntry;

	ASSERT(pTxBlk);

	ASSERT((pTxBlk->TxPacketList.Number > 1));

	while (pTxBlk->TxPacketList.Head) {
		pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
		pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
		if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE) {
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket,
					    NDIS_STATUS_FAILURE);
			continue;
		}

		bVLANPkt =
		    (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen -= LENGTH_802_3;

		/* skip vlan tag */
		if (bVLANPkt) {
			pTxBlk->pSrcBufData += LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}

		if (frameNum == 0) {
			pHeaderBufPtr =
			    STA_Build_AMSDU_Frame_Header(pAd, pTxBlk);

			/* NOTE: TxWI->MPDUtotalByteCount will be updated after final frame was handled. */
			RTMPWriteTxWI_Data(pAd,
					   (struct rt_txwi *) (&pTxBlk->
							  HeaderBuf
							  [TXINFO_SIZE]),
					   pTxBlk);
		} else {
			pHeaderBufPtr = &pTxBlk->HeaderBuf[0];
			padding =
			    ROUND_UP(LENGTH_AMSDU_SUBFRAMEHEAD +
				     subFramePayloadLen,
				     4) - (LENGTH_AMSDU_SUBFRAMEHEAD +
					   subFramePayloadLen);
			NdisZeroMemory(pHeaderBufPtr,
				       padding + LENGTH_AMSDU_SUBFRAMEHEAD);
			pHeaderBufPtr += padding;
			pTxBlk->MpduHeaderLen = padding;
		}

		/* */
		/* A-MSDU subframe */
		/*   DA(6)+SA(6)+Length(2) + LLC/SNAP Encap */
		/* */
		subFrameHeader = pHeaderBufPtr;
		subFramePayloadLen = pTxBlk->SrcBufLen;

		NdisMoveMemory(subFrameHeader, pTxBlk->pSrcBufHeader, 12);

		pHeaderBufPtr += LENGTH_AMSDU_SUBFRAMEHEAD;
		pTxBlk->MpduHeaderLen += LENGTH_AMSDU_SUBFRAMEHEAD;

		/* */
		/* Insert LLC-SNAP encapsulation - 8 octets */
		/* */
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData - 2,
						    pTxBlk->pExtraLlcSnapEncap);

		subFramePayloadLen = pTxBlk->SrcBufLen;

		if (pTxBlk->pExtraLlcSnapEncap) {
			NdisMoveMemory(pHeaderBufPtr,
				       pTxBlk->pExtraLlcSnapEncap, 6);
			pHeaderBufPtr += 6;
			/* get 2 octets (TypeofLen) */
			NdisMoveMemory(pHeaderBufPtr, pTxBlk->pSrcBufData - 2,
				       2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			subFramePayloadLen += LENGTH_802_1_H;
		}
		/* update subFrame Length field */
		subFrameHeader[12] = (subFramePayloadLen & 0xFF00) >> 8;
		subFrameHeader[13] = subFramePayloadLen & 0xFF;

		totalMPDUSize += pTxBlk->MpduHeaderLen + pTxBlk->SrcBufLen;

		if (frameNum == 0)
			FirstTx =
			    HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum,
						     &FreeNumber);
		else
			LastTxIdx =
			    HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum,
						     &FreeNumber);

		frameNum++;

		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

		/* calculate Transmitted AMSDU Count and ByteCount */
		{
			pAd->RalinkCounters.TransmittedAMSDUCount.u.LowPart++;
			pAd->RalinkCounters.TransmittedOctetsInAMSDU.QuadPart +=
			    totalMPDUSize;
		}

	}

	HAL_FinalWriteTxResource(pAd, pTxBlk, totalMPDUSize, FirstTx);
	HAL_LastTxIdx(pAd, pTxBlk->QueIdx, LastTxIdx);

	/* */
	/* Kick out Tx */
	/* */
	if (!RTMP_TEST_PSFLAG(pAd, fRTMP_PS_DISABLE_TX))
		HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);
}

void STA_Legacy_Frame_Tx(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	struct rt_header_802_11 *pHeader_802_11;
	u8 *pHeaderBufPtr;
	u16 FreeNumber;
	BOOLEAN bVLANPkt;
	struct rt_queue_entry *pQEntry;

	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE) {
		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

	if (pTxBlk->TxFrameType == TX_MCAST_FRAME) {
		INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
	}

	if (RTMP_GET_PACKET_RTS(pTxBlk->pPacket))
		TX_BLK_SET_FLAG(pTxBlk, fTX_bRtsRequired);
	else
		TX_BLK_CLEAR_FLAG(pTxBlk, fTX_bRtsRequired);

	bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

	if (pTxBlk->TxRate < pAd->CommonCfg.MinTxRate)
		pTxBlk->TxRate = pAd->CommonCfg.MinTxRate;

	STAFindCipherAlgorithm(pAd, pTxBlk);
	STABuildCommon802_11Header(pAd, pTxBlk);

	/* skip 802.3 header */
	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
	pTxBlk->SrcBufLen -= LENGTH_802_3;

	/* skip vlan tag */
	if (bVLANPkt) {
		pTxBlk->pSrcBufData += LENGTH_802_1Q;
		pTxBlk->SrcBufLen -= LENGTH_802_1Q;
	}

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];
	pHeader_802_11 = (struct rt_header_802_11 *) pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) {
		/* */
		/* build QOS Control bytes */
		/* */
		*(pHeaderBufPtr) =
		    ((pTxBlk->UserPriority & 0x0F) | (pAd->CommonCfg.
						      AckPolicy[pTxBlk->
								QueIdx] << 5));
		*(pHeaderBufPtr + 1) = 0;
		pHeaderBufPtr += 2;
		pTxBlk->MpduHeaderLen += 2;
	}
	/* The remaining content of MPDU header should locate at 4-octets aligment */
	pTxBlk->HdrPadLen = (unsigned long)pHeaderBufPtr;
	pHeaderBufPtr = (u8 *)ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (unsigned long)(pHeaderBufPtr - pTxBlk->HdrPadLen);

	{

		/* */
		/* Insert LLC-SNAP encapsulation - 8 octets */
		/* */
		/* */
		/* if original Ethernet frame contains no LLC/SNAP, */
		/* then an extra LLC/SNAP encap is required */
		/* */
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_START(pTxBlk->pSrcBufHeader,
						   pTxBlk->pExtraLlcSnapEncap);
		if (pTxBlk->pExtraLlcSnapEncap) {
			u8 vlan_size;

			NdisMoveMemory(pHeaderBufPtr,
				       pTxBlk->pExtraLlcSnapEncap, 6);
			pHeaderBufPtr += 6;
			/* skip vlan tag */
			vlan_size = (bVLANPkt) ? LENGTH_802_1Q : 0;
			/* get 2 octets (TypeofLen) */
			NdisMoveMemory(pHeaderBufPtr,
				       pTxBlk->pSrcBufHeader + 12 + vlan_size,
				       2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
		}

	}

	/* */
	/* prepare for TXWI */
	/* use Wcid as Key Index */
	/* */

	RTMPWriteTxWI_Data(pAd, (struct rt_txwi *) (&pTxBlk->HeaderBuf[TXINFO_SIZE]),
			   pTxBlk);

	/*FreeNumber = GET_TXRING_FREENO(pAd, QueIdx); */

	HAL_WriteTxResource(pAd, pTxBlk, TRUE, &FreeNumber);

	pAd->RalinkCounters.KickTxCount++;
	pAd->RalinkCounters.OneSecTxDoneCount++;

	/* */
	/* Kick out Tx */
	/* */
	if (!RTMP_TEST_PSFLAG(pAd, fRTMP_PS_DISABLE_TX))
		HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);
}

void STA_ARalink_Frame_Tx(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	u8 *pHeaderBufPtr;
	u16 FreeNumber;
	u16 totalMPDUSize = 0;
	u16 FirstTx, LastTxIdx;
	int frameNum = 0;
	BOOLEAN bVLANPkt;
	struct rt_queue_entry *pQEntry;

	ASSERT(pTxBlk);

	ASSERT((pTxBlk->TxPacketList.Number == 2));

	FirstTx = LastTxIdx = 0;	/* Is it ok init they as 0? */
	while (pTxBlk->TxPacketList.Head) {
		pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
		pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);

		if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE) {
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket,
					    NDIS_STATUS_FAILURE);
			continue;
		}

		bVLANPkt =
		    (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen -= LENGTH_802_3;

		/* skip vlan tag */
		if (bVLANPkt) {
			pTxBlk->pSrcBufData += LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}

		if (frameNum == 0) {	/* For first frame, we need to create the 802.11 header + padding(optional) + RA-AGG-LEN + SNAP Header */

			pHeaderBufPtr =
			    STA_Build_ARalink_Frame_Header(pAd, pTxBlk);

			/* It's ok write the TxWI here, because the TxWI->MPDUtotalByteCount */
			/*      will be updated after final frame was handled. */
			RTMPWriteTxWI_Data(pAd,
					   (struct rt_txwi *) (&pTxBlk->
							  HeaderBuf
							  [TXINFO_SIZE]),
					   pTxBlk);

			/* */
			/* Insert LLC-SNAP encapsulation - 8 octets */
			/* */
			EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->
							    pSrcBufData - 2,
							    pTxBlk->
							    pExtraLlcSnapEncap);

			if (pTxBlk->pExtraLlcSnapEncap) {
				NdisMoveMemory(pHeaderBufPtr,
					       pTxBlk->pExtraLlcSnapEncap, 6);
				pHeaderBufPtr += 6;
				/* get 2 octets (TypeofLen) */
				NdisMoveMemory(pHeaderBufPtr,
					       pTxBlk->pSrcBufData - 2, 2);
				pHeaderBufPtr += 2;
				pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			}
		} else {	/* For second aggregated frame, we need create the 802.3 header to headerBuf, because PCI will copy it to SDPtr0. */

			pHeaderBufPtr = &pTxBlk->HeaderBuf[0];
			pTxBlk->MpduHeaderLen = 0;

			/* A-Ralink sub-sequent frame header is the same as 802.3 header. */
			/*   DA(6)+SA(6)+FrameType(2) */
			NdisMoveMemory(pHeaderBufPtr, pTxBlk->pSrcBufHeader,
				       12);
			pHeaderBufPtr += 12;
			/* get 2 octets (TypeofLen) */
			NdisMoveMemory(pHeaderBufPtr, pTxBlk->pSrcBufData - 2,
				       2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen = LENGTH_ARALINK_SUBFRAMEHEAD;
		}

		totalMPDUSize += pTxBlk->MpduHeaderLen + pTxBlk->SrcBufLen;

		/*FreeNumber = GET_TXRING_FREENO(pAd, QueIdx); */
		if (frameNum == 0)
			FirstTx =
			    HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum,
						     &FreeNumber);
		else
			LastTxIdx =
			    HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum,
						     &FreeNumber);

		frameNum++;

		pAd->RalinkCounters.OneSecTxAggregationCount++;
		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

	}

	HAL_FinalWriteTxResource(pAd, pTxBlk, totalMPDUSize, FirstTx);
	HAL_LastTxIdx(pAd, pTxBlk->QueIdx, LastTxIdx);

	/* */
	/* Kick out Tx */
	/* */
	if (!RTMP_TEST_PSFLAG(pAd, fRTMP_PS_DISABLE_TX))
		HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

}

void STA_Fragment_Frame_Tx(struct rt_rtmp_adapter *pAd, struct rt_tx_blk *pTxBlk)
{
	struct rt_header_802_11 *pHeader_802_11;
	u8 *pHeaderBufPtr;
	u16 FreeNumber;
	u8 fragNum = 0;
	struct rt_packet_info PacketInfo;
	u16 EncryptionOverhead = 0;
	u32 FreeMpduSize, SrcRemainingBytes;
	u16 AckDuration;
	u32 NextMpduSize;
	BOOLEAN bVLANPkt;
	struct rt_queue_entry *pQEntry;
	HTTRANSMIT_SETTING *pTransmit;

	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE) {
		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

	ASSERT(TX_BLK_TEST_FLAG(pTxBlk, fTX_bAllowFrag));
	bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

	STAFindCipherAlgorithm(pAd, pTxBlk);
	STABuildCommon802_11Header(pAd, pTxBlk);

	if (pTxBlk->CipherAlg == CIPHER_TKIP) {
		pTxBlk->pPacket =
		    duplicate_pkt_with_TKIP_MIC(pAd, pTxBlk->pPacket);
		if (pTxBlk->pPacket == NULL)
			return;
		RTMP_QueryPacketInfo(pTxBlk->pPacket, &PacketInfo,
				     &pTxBlk->pSrcBufHeader,
				     &pTxBlk->SrcBufLen);
	}
	/* skip 802.3 header */
	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
	pTxBlk->SrcBufLen -= LENGTH_802_3;

	/* skip vlan tag */
	if (bVLANPkt) {
		pTxBlk->pSrcBufData += LENGTH_802_1Q;
		pTxBlk->SrcBufLen -= LENGTH_802_1Q;
	}

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWI_SIZE];
	pHeader_802_11 = (struct rt_header_802_11 *) pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) {
		/* */
		/* build QOS Control bytes */
		/* */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);

		*(pHeaderBufPtr + 1) = 0;
		pHeaderBufPtr += 2;
		pTxBlk->MpduHeaderLen += 2;
	}
	/* */
	/* padding at front of LLC header */
	/* LLC header should locate at 4-octets aligment */
	/* */
	pTxBlk->HdrPadLen = (unsigned long)pHeaderBufPtr;
	pHeaderBufPtr = (u8 *)ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (unsigned long)(pHeaderBufPtr - pTxBlk->HdrPadLen);

	/* */
	/* Insert LLC-SNAP encapsulation - 8 octets */
	/* */
	/* */
	/* if original Ethernet frame contains no LLC/SNAP, */
	/* then an extra LLC/SNAP encap is required */
	/* */
	EXTRA_LLCSNAP_ENCAP_FROM_PKT_START(pTxBlk->pSrcBufHeader,
					   pTxBlk->pExtraLlcSnapEncap);
	if (pTxBlk->pExtraLlcSnapEncap) {
		u8 vlan_size;

		NdisMoveMemory(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);
		pHeaderBufPtr += 6;
		/* skip vlan tag */
		vlan_size = (bVLANPkt) ? LENGTH_802_1Q : 0;
		/* get 2 octets (TypeofLen) */
		NdisMoveMemory(pHeaderBufPtr,
			       pTxBlk->pSrcBufHeader + 12 + vlan_size, 2);
		pHeaderBufPtr += 2;
		pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
	}

	/* If TKIP is used and fragmentation is required. Driver has to */
	/*      append TKIP MIC at tail of the scatter buffer */
	/*      MAC ASIC will only perform IV/EIV/ICV insertion but no TKIP MIC */
	if (pTxBlk->CipherAlg == CIPHER_TKIP) {
		RTMPCalculateMICValue(pAd, pTxBlk->pPacket,
				      pTxBlk->pExtraLlcSnapEncap, pTxBlk->pKey,
				      0);

		/* NOTE: DON'T refer the skb->len directly after following copy. Becasue the length is not adjust */
		/*                      to correct lenght, refer to pTxBlk->SrcBufLen for the packet length in following progress. */
		NdisMoveMemory(pTxBlk->pSrcBufData + pTxBlk->SrcBufLen,
			       &pAd->PrivateInfo.Tx.MIC[0], 8);
		/*skb_put((RTPKT_TO_OSPKT(pTxBlk->pPacket))->tail, 8); */
		pTxBlk->SrcBufLen += 8;
		pTxBlk->TotalFrameLen += 8;
		pTxBlk->CipherAlg = CIPHER_TKIP_NO_MIC;
	}
	/* */
	/* calcuate the overhead bytes that encryption algorithm may add. This */
	/* affects the calculate of "duration" field */
	/* */
	if ((pTxBlk->CipherAlg == CIPHER_WEP64)
	    || (pTxBlk->CipherAlg == CIPHER_WEP128))
		EncryptionOverhead = 8;	/*WEP: IV[4] + ICV[4]; */
	else if (pTxBlk->CipherAlg == CIPHER_TKIP_NO_MIC)
		EncryptionOverhead = 12;	/*TKIP: IV[4] + EIV[4] + ICV[4], MIC will be added to TotalPacketLength */
	else if (pTxBlk->CipherAlg == CIPHER_TKIP)
		EncryptionOverhead = 20;	/*TKIP: IV[4] + EIV[4] + ICV[4] + MIC[8] */
	else if (pTxBlk->CipherAlg == CIPHER_AES)
		EncryptionOverhead = 16;	/* AES: IV[4] + EIV[4] + MIC[8] */
	else
		EncryptionOverhead = 0;

	pTransmit = pTxBlk->pTransmit;
	/* Decide the TX rate */
	if (pTransmit->field.MODE == MODE_CCK)
		pTxBlk->TxRate = pTransmit->field.MCS;
	else if (pTransmit->field.MODE == MODE_OFDM)
		pTxBlk->TxRate = pTransmit->field.MCS + RATE_FIRST_OFDM_RATE;
	else
		pTxBlk->TxRate = RATE_6_5;

	/* decide how much time an ACK/CTS frame will consume in the air */
	if (pTxBlk->TxRate <= RATE_LAST_OFDM_RATE)
		AckDuration =
		    RTMPCalcDuration(pAd,
				     pAd->CommonCfg.ExpectedACKRate[pTxBlk->
								    TxRate],
				     14);
	else
		AckDuration = RTMPCalcDuration(pAd, RATE_6_5, 14);

	/* Init the total payload length of this frame. */
	SrcRemainingBytes = pTxBlk->SrcBufLen;

	pTxBlk->TotalFragNum = 0xff;

	do {

		FreeMpduSize = pAd->CommonCfg.FragmentThreshold - LENGTH_CRC;

		FreeMpduSize -= pTxBlk->MpduHeaderLen;

		if (SrcRemainingBytes <= FreeMpduSize) {	/* this is the last or only fragment */

			pTxBlk->SrcBufLen = SrcRemainingBytes;

			pHeader_802_11->FC.MoreFrag = 0;
			pHeader_802_11->Duration =
			    pAd->CommonCfg.Dsifs + AckDuration;

			/* Indicate the lower layer that this's the last fragment. */
			pTxBlk->TotalFragNum = fragNum;
		} else {	/* more fragment is required */

			pTxBlk->SrcBufLen = FreeMpduSize;

			NextMpduSize =
			    min(((u32)SrcRemainingBytes - pTxBlk->SrcBufLen),
				((u32)pAd->CommonCfg.FragmentThreshold));
			pHeader_802_11->FC.MoreFrag = 1;
			pHeader_802_11->Duration =
			    (3 * pAd->CommonCfg.Dsifs) + (2 * AckDuration) +
			    RTMPCalcDuration(pAd, pTxBlk->TxRate,
					     NextMpduSize + EncryptionOverhead);
		}

		if (fragNum == 0)
			pTxBlk->FrameGap = IFS_HTTXOP;
		else
			pTxBlk->FrameGap = IFS_SIFS;

		RTMPWriteTxWI_Data(pAd,
				   (struct rt_txwi *) (&pTxBlk->
						  HeaderBuf[TXINFO_SIZE]),
				   pTxBlk);

		HAL_WriteFragTxResource(pAd, pTxBlk, fragNum, &FreeNumber);

		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

		/* Update the frame number, remaining size of the NDIS packet payload. */

		/* space for 802.11 header. */
		if (fragNum == 0 && pTxBlk->pExtraLlcSnapEncap)
			pTxBlk->MpduHeaderLen -= LENGTH_802_1_H;

		fragNum++;
		SrcRemainingBytes -= pTxBlk->SrcBufLen;
		pTxBlk->pSrcBufData += pTxBlk->SrcBufLen;

		pHeader_802_11->Frag++;	/* increase Frag # */

	} while (SrcRemainingBytes > 0);

	/* */
	/* Kick out Tx */
	/* */
	if (!RTMP_TEST_PSFLAG(pAd, fRTMP_PS_DISABLE_TX))
		HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);
}

#define RELEASE_FRAMES_OF_TXBLK(_pAd, _pTxBlk, _pQEntry, _Status) 										\
		while(_pTxBlk->TxPacketList.Head)														\
		{																						\
			_pQEntry = RemoveHeadQueue(&_pTxBlk->TxPacketList);									\
			RELEASE_NDIS_PACKET(_pAd, QUEUE_ENTRY_TO_PACKET(_pQEntry), _Status);	\
		}

/*
	========================================================================

	Routine Description:
		Copy frame from waiting queue into relative ring buffer and set
	appropriate ASIC register to kick hardware encryption before really
	sent out to air.

	Arguments:
		pAd 	Pointer to our adapter
		void *	Pointer to outgoing Ndis frame
		NumberOfFrag	Number of fragment required

	Return Value:
		None

	IRQL = DISPATCH_LEVEL

	Note:

	========================================================================
*/
int STAHardTransmit(struct rt_rtmp_adapter *pAd,
			    struct rt_tx_blk *pTxBlk, u8 QueIdx)
{
	char *pPacket;
	struct rt_queue_entry *pQEntry;

	/* --------------------------------------------- */
	/* STEP 0. DO SANITY CHECK AND SOME EARLY PREPARATION. */
	/* --------------------------------------------- */
	/* */
	ASSERT(pTxBlk->TxPacketList.Number);
	if (pTxBlk->TxPacketList.Head == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("pTxBlk->TotalFrameNum == %ld!\n",
			  pTxBlk->TxPacketList.Number));
		return NDIS_STATUS_FAILURE;
	}

	pPacket = QUEUE_ENTRY_TO_PACKET(pTxBlk->TxPacketList.Head);

	/* ------------------------------------------------------------------ */
	/* STEP 1. WAKE UP PHY */
	/*              outgoing frame always wakeup PHY to prevent frame lost and */
	/*              turn off PSM bit to improve performance */
	/* ------------------------------------------------------------------ */
	/* not to change PSM bit, just send this frame out? */
	if ((pAd->StaCfg.Psm == PWR_SAVE)
	    && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)) {
		DBGPRINT_RAW(RT_DEBUG_INFO, ("AsicForceWakeup At HardTx\n"));
#ifdef RTMP_MAC_PCI
		AsicForceWakeup(pAd, TRUE);
#endif /* RTMP_MAC_PCI // */
#ifdef RTMP_MAC_USB
		RTUSBEnqueueInternalCmd(pAd, CMDTHREAD_FORCE_WAKE_UP, NULL, 0);
#endif /* RTMP_MAC_USB // */
	}
	/* It should not change PSM bit, when APSD turn on. */
	if ((!
	     (pAd->CommonCfg.bAPSDCapable
	      && pAd->CommonCfg.APEdcaParm.bAPSDCapable)
	     && (pAd->CommonCfg.bAPSDForcePowerSave == FALSE))
	    || (RTMP_GET_PACKET_EAPOL(pTxBlk->pPacket))
	    || (RTMP_GET_PACKET_WAI(pTxBlk->pPacket))) {
		if ((pAd->StaCfg.Psm == PWR_SAVE) &&
		    (pAd->StaCfg.WindowsPowerMode ==
		     Ndis802_11PowerModeFast_PSP))
			RTMP_SET_PSM_BIT(pAd, PWR_ACTIVE);
	}

	switch (pTxBlk->TxFrameType) {
	case TX_AMPDU_FRAME:
		STA_AMPDU_Frame_Tx(pAd, pTxBlk);
		break;
	case TX_AMSDU_FRAME:
		STA_AMSDU_Frame_Tx(pAd, pTxBlk);
		break;
	case TX_LEGACY_FRAME:
		STA_Legacy_Frame_Tx(pAd, pTxBlk);
		break;
	case TX_MCAST_FRAME:
		STA_Legacy_Frame_Tx(pAd, pTxBlk);
		break;
	case TX_RALINK_FRAME:
		STA_ARalink_Frame_Tx(pAd, pTxBlk);
		break;
	case TX_FRAG_FRAME:
		STA_Fragment_Frame_Tx(pAd, pTxBlk);
		break;
	default:
		{
			/* It should not happened! */
			DBGPRINT(RT_DEBUG_ERROR,
				 ("Send a packet was not classified! It should not happen!\n"));
			while (pTxBlk->TxPacketList.Number) {
				pQEntry =
				    RemoveHeadQueue(&pTxBlk->TxPacketList);
				pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
				if (pPacket)
					RELEASE_NDIS_PACKET(pAd, pPacket,
							    NDIS_STATUS_FAILURE);
			}
		}
		break;
	}

	return (NDIS_STATUS_SUCCESS);

}

unsigned long HashBytesPolynomial(u8 * value, unsigned int len)
{
	unsigned char *word = value;
	unsigned int ret = 0;
	unsigned int i;

	for (i = 0; i < len; i++) {
		int mod = i % 32;
		ret ^= (unsigned int)(word[i]) << mod;
		ret ^= (unsigned int)(word[i]) >> (32 - mod);
	}
	return ret;
}

void Sta_Announce_or_Forward_802_3_Packet(struct rt_rtmp_adapter *pAd,
					  void *pPacket,
					  u8 FromWhichBSSID)
{
	if (TRUE) {
		announce_802_3_packet(pAd, pPacket);
	} else {
		/* release packet */
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
	}
}
