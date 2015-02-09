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
	action.c

    Abstract:
    Handle association related requests either from WSTA or from local MLME

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
	Jan Lee		2006	  	created for rt2860
 */

#include "../rt_config.h"
#include "action.h"

static void ReservedAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem);

/*
    ==========================================================================
    Description:
        association state machine init, including state transition and timer init
    Parameters:
        S - pointer to the association state machine
    Note:
        The state machine looks like the following

                                    ASSOC_IDLE
        MT2_MLME_DISASSOC_REQ    mlme_disassoc_req_action
        MT2_PEER_DISASSOC_REQ    peer_disassoc_action
        MT2_PEER_ASSOC_REQ       drop
        MT2_PEER_REASSOC_REQ     drop
        MT2_CLS3ERR              cls3err_action
    ==========================================================================
 */
void ActionStateMachineInit(struct rt_rtmp_adapter *pAd,
			    struct rt_state_machine *S,
			    OUT STATE_MACHINE_FUNC Trans[])
{
	StateMachineInit(S, (STATE_MACHINE_FUNC *) Trans, MAX_ACT_STATE,
			 MAX_ACT_MSG, (STATE_MACHINE_FUNC) Drop, ACT_IDLE,
			 ACT_MACHINE_BASE);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_SPECTRUM_CATE,
			      (STATE_MACHINE_FUNC) PeerSpectrumAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_QOS_CATE,
			      (STATE_MACHINE_FUNC) PeerQOSAction);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_DLS_CATE,
			      (STATE_MACHINE_FUNC) ReservedAction);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_BA_CATE,
			      (STATE_MACHINE_FUNC) PeerBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_HT_CATE,
			      (STATE_MACHINE_FUNC) PeerHTAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_ADD_BA_CATE,
			      (STATE_MACHINE_FUNC) MlmeADDBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_ORI_DELBA_CATE,
			      (STATE_MACHINE_FUNC) MlmeDELBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_REC_DELBA_CATE,
			      (STATE_MACHINE_FUNC) MlmeDELBAAction);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_PUBLIC_CATE,
			      (STATE_MACHINE_FUNC) PeerPublicAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_RM_CATE,
			      (STATE_MACHINE_FUNC) PeerRMAction);

	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_QOS_CATE,
			      (STATE_MACHINE_FUNC) MlmeQOSAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_DLS_CATE,
			      (STATE_MACHINE_FUNC) MlmeDLSAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_ACT_INVALID,
			      (STATE_MACHINE_FUNC) MlmeInvalidAction);
}

void MlmeADDBAAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	struct rt_mlme_addba_req *pInfo;
	u8 Addr[6];
	u8 *pOutBuffer = NULL;
	int NStatus;
	unsigned long Idx;
	struct rt_frame_addba_req Frame;
	unsigned long FrameLen;
	struct rt_ba_ori_entry *pBAEntry = NULL;

	pInfo = (struct rt_mlme_addba_req *)Elem->Msg;
	NdisZeroMemory(&Frame, sizeof(struct rt_frame_addba_req));

	if (MlmeAddBAReqSanity(pAd, Elem->Msg, Elem->MsgLen, Addr)) {
		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);	/*Get an unused nonpaged memory */
		if (NStatus != NDIS_STATUS_SUCCESS) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("BA - MlmeADDBAAction() allocate memory failed \n"));
			return;
		}
		/* 1. find entry */
		Idx =
		    pAd->MacTab.Content[pInfo->Wcid].BAOriWcidArray[pInfo->TID];
		if (Idx == 0) {
			MlmeFreeMemory(pAd, pOutBuffer);
			DBGPRINT(RT_DEBUG_ERROR,
				 ("BA - MlmeADDBAAction() can't find BAOriEntry \n"));
			return;
		} else {
			pBAEntry = &pAd->BATable.BAOriEntry[Idx];
		}

		{
			if (ADHOC_ON(pAd))
				ActHeaderInit(pAd, &Frame.Hdr, pInfo->pAddr,
					      pAd->CurrentAddress,
					      pAd->CommonCfg.Bssid);
			else
				ActHeaderInit(pAd, &Frame.Hdr,
					      pAd->CommonCfg.Bssid,
					      pAd->CurrentAddress,
					      pInfo->pAddr);
		}

		Frame.Category = CATEGORY_BA;
		Frame.Action = ADDBA_REQ;
		Frame.BaParm.AMSDUSupported = 0;
		Frame.BaParm.BAPolicy = IMMED_BA;
		Frame.BaParm.TID = pInfo->TID;
		Frame.BaParm.BufSize = pInfo->BaBufSize;
		Frame.Token = pInfo->Token;
		Frame.TimeOutValue = pInfo->TimeOutValue;
		Frame.BaStartSeq.field.FragNum = 0;
		Frame.BaStartSeq.field.StartSeq =
		    pAd->MacTab.Content[pInfo->Wcid].TxSeq[pInfo->TID];

		*(u16 *) (&Frame.BaParm) =
		    cpu2le16(*(u16 *) (&Frame.BaParm));
		Frame.TimeOutValue = cpu2le16(Frame.TimeOutValue);
		Frame.BaStartSeq.word = cpu2le16(Frame.BaStartSeq.word);

		MakeOutgoingFrame(pOutBuffer, &FrameLen,
				  sizeof(struct rt_frame_addba_req), &Frame, END_OF_ARGS);

		MiniportMMRequest(pAd,
				  (MGMT_USE_QUEUE_FLAG |
				   MapUserPriorityToAccessCategory[pInfo->TID]),
				  pOutBuffer, FrameLen);

		MlmeFreeMemory(pAd, pOutBuffer);

		DBGPRINT(RT_DEBUG_TRACE,
			 ("BA - Send ADDBA request. StartSeq = %x,  FrameLen = %ld. BufSize = %d\n",
			  Frame.BaStartSeq.field.StartSeq, FrameLen,
			  Frame.BaParm.BufSize));
	}
}

/*
    ==========================================================================
    Description:
        send DELBA and delete BaEntry if any
    Parametrs:
        Elem - MLME message struct rt_mlme_delba_req

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
void MlmeDELBAAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	struct rt_mlme_delba_req *pInfo;
	u8 *pOutBuffer = NULL;
	u8 *pOutBuffer2 = NULL;
	int NStatus;
	unsigned long Idx;
	struct rt_frame_delba_req Frame;
	unsigned long FrameLen;
	struct rt_frame_bar FrameBar;

	pInfo = (struct rt_mlme_delba_req *)Elem->Msg;
	/* must send back DELBA */
	NdisZeroMemory(&Frame, sizeof(struct rt_frame_delba_req));
	DBGPRINT(RT_DEBUG_TRACE,
		 ("==> MlmeDELBAAction(), Initiator(%d) \n", pInfo->Initiator));

	if (MlmeDelBAReqSanity(pAd, Elem->Msg, Elem->MsgLen)) {
		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);	/*Get an unused nonpaged memory */
		if (NStatus != NDIS_STATUS_SUCCESS) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("BA - MlmeDELBAAction() allocate memory failed 1. \n"));
			return;
		}

		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer2);	/*Get an unused nonpaged memory */
		if (NStatus != NDIS_STATUS_SUCCESS) {
			MlmeFreeMemory(pAd, pOutBuffer);
			DBGPRINT(RT_DEBUG_ERROR,
				 ("BA - MlmeDELBAAction() allocate memory failed 2. \n"));
			return;
		}
		/* SEND BAR (Send BAR to refresh peer reordering buffer.) */
		Idx =
		    pAd->MacTab.Content[pInfo->Wcid].BAOriWcidArray[pInfo->TID];

		BarHeaderInit(pAd, &FrameBar,
			      pAd->MacTab.Content[pInfo->Wcid].Addr,
			      pAd->CurrentAddress);

		FrameBar.StartingSeq.field.FragNum = 0;	/* make sure sequence not clear in DEL funciton. */
		FrameBar.StartingSeq.field.StartSeq = pAd->MacTab.Content[pInfo->Wcid].TxSeq[pInfo->TID];	/* make sure sequence not clear in DEL funciton. */
		FrameBar.BarControl.TID = pInfo->TID;	/* make sure sequence not clear in DEL funciton. */
		FrameBar.BarControl.ACKPolicy = IMMED_BA;	/* make sure sequence not clear in DEL funciton. */
		FrameBar.BarControl.Compressed = 1;	/* make sure sequence not clear in DEL funciton. */
		FrameBar.BarControl.MTID = 0;	/* make sure sequence not clear in DEL funciton. */

		MakeOutgoingFrame(pOutBuffer2, &FrameLen,
				  sizeof(struct rt_frame_bar), &FrameBar, END_OF_ARGS);
		MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer2, FrameLen);
		MlmeFreeMemory(pAd, pOutBuffer2);
		DBGPRINT(RT_DEBUG_TRACE,
			 ("BA - MlmeDELBAAction() . Send BAR to refresh peer reordering buffer \n"));

		/* SEND DELBA FRAME */
		FrameLen = 0;

		{
			if (ADHOC_ON(pAd))
				ActHeaderInit(pAd, &Frame.Hdr,
					      pAd->MacTab.Content[pInfo->Wcid].
					      Addr, pAd->CurrentAddress,
					      pAd->CommonCfg.Bssid);
			else
				ActHeaderInit(pAd, &Frame.Hdr,
					      pAd->CommonCfg.Bssid,
					      pAd->CurrentAddress,
					      pAd->MacTab.Content[pInfo->Wcid].
					      Addr);
		}

		Frame.Category = CATEGORY_BA;
		Frame.Action = DELBA;
		Frame.DelbaParm.Initiator = pInfo->Initiator;
		Frame.DelbaParm.TID = pInfo->TID;
		Frame.ReasonCode = 39;	/* Time Out */
		*(u16 *) (&Frame.DelbaParm) =
		    cpu2le16(*(u16 *) (&Frame.DelbaParm));
		Frame.ReasonCode = cpu2le16(Frame.ReasonCode);

		MakeOutgoingFrame(pOutBuffer, &FrameLen,
				  sizeof(struct rt_frame_delba_req), &Frame, END_OF_ARGS);
		MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
		MlmeFreeMemory(pAd, pOutBuffer);
		DBGPRINT(RT_DEBUG_TRACE,
			 ("BA - MlmeDELBAAction() . 3 DELBA sent. Initiator(%d)\n",
			  pInfo->Initiator));
	}
}

void MlmeQOSAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
}

void MlmeDLSAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
}

void MlmeInvalidAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	/*u8 *                  pOutBuffer = NULL; */
	/*Return the receiving frame except the MSB of category filed set to 1.  7.3.1.11 */
}

void PeerQOSAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
}

void PeerBAAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	u8 Action = Elem->Msg[LENGTH_802_11 + 1];

	switch (Action) {
	case ADDBA_REQ:
		PeerAddBAReqAction(pAd, Elem);
		break;
	case ADDBA_RESP:
		PeerAddBARspAction(pAd, Elem);
		break;
	case DELBA:
		PeerDelBAAction(pAd, Elem);
		break;
	}
}

void PeerPublicAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	if (Elem->Wcid >= MAX_LEN_OF_MAC_TABLE)
		return;
}

static void ReservedAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	u8 Category;

	if (Elem->MsgLen <= LENGTH_802_11) {
		return;
	}

	Category = Elem->Msg[LENGTH_802_11];
	DBGPRINT(RT_DEBUG_TRACE,
		 ("Rcv reserved category(%d) Action Frame\n", Category));
	hex_dump("Reserved Action Frame", &Elem->Msg[0], Elem->MsgLen);
}

void PeerRMAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	return;
}

static void respond_ht_information_exchange_action(struct rt_rtmp_adapter *pAd,
						   struct rt_mlme_queue_elem *Elem)
{
	u8 *pOutBuffer = NULL;
	int NStatus;
	unsigned long FrameLen;
	struct rt_frame_ht_info HTINFOframe, *pFrame;
	u8 *pAddr;

	/* 2. Always send back ADDBA Response */
	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);	/*Get an unused nonpaged memory */

	if (NStatus != NDIS_STATUS_SUCCESS) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ACTION - respond_ht_information_exchange_action() allocate memory failed \n"));
		return;
	}
	/* get RA */
	pFrame = (struct rt_frame_ht_info *) & Elem->Msg[0];
	pAddr = pFrame->Hdr.Addr2;

	NdisZeroMemory(&HTINFOframe, sizeof(struct rt_frame_ht_info));
	/* 2-1. Prepare ADDBA Response frame. */
	{
		if (ADHOC_ON(pAd))
			ActHeaderInit(pAd, &HTINFOframe.Hdr, pAddr,
				      pAd->CurrentAddress,
				      pAd->CommonCfg.Bssid);
		else
			ActHeaderInit(pAd, &HTINFOframe.Hdr,
				      pAd->CommonCfg.Bssid, pAd->CurrentAddress,
				      pAddr);
	}

	HTINFOframe.Category = CATEGORY_HT;
	HTINFOframe.Action = HT_INFO_EXCHANGE;
	HTINFOframe.HT_Info.Request = 0;
	HTINFOframe.HT_Info.Forty_MHz_Intolerant =
	    pAd->CommonCfg.HtCapability.HtCapInfo.Forty_Mhz_Intolerant;
	HTINFOframe.HT_Info.STA_Channel_Width =
	    pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;

	MakeOutgoingFrame(pOutBuffer, &FrameLen,
			  sizeof(struct rt_frame_ht_info), &HTINFOframe, END_OF_ARGS);

	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pAd, pOutBuffer);
}

void PeerHTAction(struct rt_rtmp_adapter *pAd, struct rt_mlme_queue_elem *Elem)
{
	u8 Action = Elem->Msg[LENGTH_802_11 + 1];

	if (Elem->Wcid >= MAX_LEN_OF_MAC_TABLE)
		return;

	switch (Action) {
	case NOTIFY_BW_ACTION:
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ACTION - HT Notify Channel bandwidth action----> \n"));

		if (pAd->StaActive.SupportedPhyInfo.bHtEnable == FALSE) {
			/* Note, this is to patch DIR-1353 AP. When the AP set to Wep, it will use legacy mode. But AP still keeps */
			/* sending BW_Notify Action frame, and cause us to linkup and linkdown. */
			/* In legacy mode, don't need to parse HT action frame. */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("ACTION -Ignore HT Notify Channel BW when link as legacy mode. BW = %d---> \n",
				  Elem->Msg[LENGTH_802_11 + 2]));
			break;
		}

		if (Elem->Msg[LENGTH_802_11 + 2] == 0)	/* 7.4.8.2. if value is 1, keep the same as supported channel bandwidth. */
			pAd->MacTab.Content[Elem->Wcid].HTPhyMode.field.BW = 0;

		break;
	case SMPS_ACTION:
		/* 7.3.1.25 */
		DBGPRINT(RT_DEBUG_TRACE, ("ACTION - SMPS action----> \n"));
		if (((Elem->Msg[LENGTH_802_11 + 2] & 0x1) == 0)) {
			pAd->MacTab.Content[Elem->Wcid].MmpsMode = MMPS_ENABLE;
		} else if (((Elem->Msg[LENGTH_802_11 + 2] & 0x2) == 0)) {
			pAd->MacTab.Content[Elem->Wcid].MmpsMode = MMPS_STATIC;
		} else {
			pAd->MacTab.Content[Elem->Wcid].MmpsMode = MMPS_DYNAMIC;
		}

		DBGPRINT(RT_DEBUG_TRACE,
			 ("Aid(%d) MIMO PS = %d\n", Elem->Wcid,
			  pAd->MacTab.Content[Elem->Wcid].MmpsMode));
		/* rt2860c : add something for smps change. */
		break;

	case SETPCO_ACTION:
		break;
	case MIMO_CHA_MEASURE_ACTION:
		break;
	case HT_INFO_EXCHANGE:
		{
			struct rt_ht_information_octet *pHT_info;

			pHT_info =
			    (struct rt_ht_information_octet *) & Elem->Msg[LENGTH_802_11 +
								 2];
			/* 7.4.8.10 */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("ACTION - HT Information Exchange action----> \n"));
			if (pHT_info->Request) {
				respond_ht_information_exchange_action(pAd,
								       Elem);
			}
		}
		break;
	}
}

/*
	==========================================================================
	Description:
		Retry sending ADDBA Reqest.

	IRQL = DISPATCH_LEVEL

	Parametrs:
	p8023Header: if this is already 802.3 format, p8023Header is NULL

	Return	: TRUE if put into rx reordering buffer, shouldn't indicaterxhere.
				FALSE , then continue indicaterx at this moment.
	==========================================================================
 */
void ORIBATimerTimeout(struct rt_rtmp_adapter *pAd)
{
	struct rt_mac_table_entry *pEntry;
	int i, total;
	u8 TID;

	total = pAd->MacTab.Size * NUM_OF_TID;

	for (i = 1; ((i < MAX_LEN_OF_BA_ORI_TABLE) && (total > 0)); i++) {
		if (pAd->BATable.BAOriEntry[i].ORI_BA_Status == Originator_Done) {
			pEntry =
			    &pAd->MacTab.Content[pAd->BATable.BAOriEntry[i].
						 Wcid];
			TID = pAd->BATable.BAOriEntry[i].TID;

			ASSERT(pAd->BATable.BAOriEntry[i].Wcid <
			       MAX_LEN_OF_MAC_TABLE);
		}
		total--;
	}
}

void SendRefreshBAR(struct rt_rtmp_adapter *pAd, struct rt_mac_table_entry *pEntry)
{
	struct rt_frame_bar FrameBar;
	unsigned long FrameLen;
	int NStatus;
	u8 *pOutBuffer = NULL;
	u16 Sequence;
	u8 i, TID;
	u16 idx;
	struct rt_ba_ori_entry *pBAEntry;

	for (i = 0; i < NUM_OF_TID; i++) {
		idx = pEntry->BAOriWcidArray[i];
		if (idx == 0) {
			continue;
		}
		pBAEntry = &pAd->BATable.BAOriEntry[idx];

		if (pBAEntry->ORI_BA_Status == Originator_Done) {
			TID = pBAEntry->TID;

			ASSERT(pBAEntry->Wcid < MAX_LEN_OF_MAC_TABLE);

			NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);	/*Get an unused nonpaged memory */
			if (NStatus != NDIS_STATUS_SUCCESS) {
				DBGPRINT(RT_DEBUG_ERROR,
					 ("BA - MlmeADDBAAction() allocate memory failed \n"));
				return;
			}

			Sequence = pEntry->TxSeq[TID];

			BarHeaderInit(pAd, &FrameBar, pEntry->Addr,
				      pAd->CurrentAddress);

			FrameBar.StartingSeq.field.FragNum = 0;	/* make sure sequence not clear in DEL function. */
			FrameBar.StartingSeq.field.StartSeq = Sequence;	/* make sure sequence not clear in DEL funciton. */
			FrameBar.BarControl.TID = TID;	/* make sure sequence not clear in DEL funciton. */

			MakeOutgoingFrame(pOutBuffer, &FrameLen,
					  sizeof(struct rt_frame_bar), &FrameBar,
					  END_OF_ARGS);
			/*if (!(CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_RALINK_CHIPSET))) */
			if (1)	/* Now we always send BAR. */
			{
				/*MiniportMMRequestUnlock(pAd, 0, pOutBuffer, FrameLen); */
				MiniportMMRequest(pAd,
						  (MGMT_USE_QUEUE_FLAG |
						   MapUserPriorityToAccessCategory
						   [TID]), pOutBuffer,
						  FrameLen);

			}
			MlmeFreeMemory(pAd, pOutBuffer);
		}
	}
}

void ActHeaderInit(struct rt_rtmp_adapter *pAd,
		   struct rt_header_802_11 * pHdr80211,
		   u8 *Addr1, u8 *Addr2, u8 *Addr3)
{
	NdisZeroMemory(pHdr80211, sizeof(struct rt_header_802_11));
	pHdr80211->FC.Type = BTYPE_MGMT;
	pHdr80211->FC.SubType = SUBTYPE_ACTION;

	COPY_MAC_ADDR(pHdr80211->Addr1, Addr1);
	COPY_MAC_ADDR(pHdr80211->Addr2, Addr2);
	COPY_MAC_ADDR(pHdr80211->Addr3, Addr3);
}

void BarHeaderInit(struct rt_rtmp_adapter *pAd,
		   struct rt_frame_bar * pCntlBar, u8 *pDA, u8 *pSA)
{
	NdisZeroMemory(pCntlBar, sizeof(struct rt_frame_bar));
	pCntlBar->FC.Type = BTYPE_CNTL;
	pCntlBar->FC.SubType = SUBTYPE_BLOCK_ACK_REQ;
	pCntlBar->BarControl.MTID = 0;
	pCntlBar->BarControl.Compressed = 1;
	pCntlBar->BarControl.ACKPolicy = 0;

	pCntlBar->Duration =
	    16 + RTMPCalcDuration(pAd, RATE_1, sizeof(struct rt_frame_ba));

	COPY_MAC_ADDR(pCntlBar->Addr1, pDA);
	COPY_MAC_ADDR(pCntlBar->Addr2, pSA);
}

/*
	==========================================================================
	Description:
		Insert Category and action code into the action frame.

	Parametrs:
		1. frame buffer pointer.
		2. frame length.
		3. category code of the frame.
		4. action code of the frame.

	Return	: None.
	==========================================================================
 */
void InsertActField(struct rt_rtmp_adapter *pAd,
		    u8 *pFrameBuf,
		    unsigned long *pFrameLen, u8 Category, u8 ActCode)
{
	unsigned long TempLen;

	MakeOutgoingFrame(pFrameBuf, &TempLen,
			  1, &Category, 1, &ActCode, END_OF_ARGS);

	*pFrameLen = *pFrameLen + TempLen;

	return;
}
