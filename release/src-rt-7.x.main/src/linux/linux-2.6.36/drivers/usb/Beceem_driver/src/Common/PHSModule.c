/* 
* PHSModule.c
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


#include <headers.h>

#define IN
#define OUT

void DumpDataPacketHeader(PUCHAR pPkt);

/*
Function:				PHSTransmit

Description:			This routine handle PHS(Payload Header Suppression for Tx path.
					It extracts a fragment of the NDIS_PACKET containing the header
					to be suppressed.It then supresses the header by invoking PHS exported compress routine.
					The header data after supression is copied back to the NDIS_PACKET. 
						

Input parameters:		IN PMINI_ADAPTER Adapter         - Miniport Adapter Context
						IN Packet 				- NDIS packet containing data to be transmitted
						IN USHORT Vcid        - vcid pertaining to connection on which the packet is being sent.Used to 
										        identify PHS rule to be applied.
						B_UINT16 uiClassifierRuleID - Classifier Rule ID
						BOOLEAN bHeaderSuppressionEnabled - indicates if header suprression is enabled for SF.

Return:					STATUS_SUCCESS - If the send was successful.
						Other          - If an error occured.
*/

int PHSTransmit(PMINI_ADAPTER Adapter,
					 struct sk_buff	**pPacket,
					 USHORT Vcid,
					 B_UINT16 uiClassifierRuleID,
					 BOOLEAN bHeaderSuppressionEnabled,
					 UINT *PacketLen, 
					 UCHAR bEthCSSupport)
{

	//PHS Sepcific
	UINT    unPHSPktHdrBytesCopied = 0;
	UINT	unPhsOldHdrSize = 0;
	UINT	unPHSNewPktHeaderLen = 0;
	/* Pointer to PHS IN Hdr Buffer */
	PUCHAR pucPHSPktHdrInBuf = 
				Adapter->stPhsTxContextInfo.ucaHdrSupressionInBuf;
	/* Pointer to PHS OUT Hdr Buffer */
	PUCHAR  pucPHSPktHdrOutBuf = 
					Adapter->stPhsTxContextInfo.ucaHdrSupressionOutBuf;
	UINT       usPacketType;
	UINT       BytesToRemove=0;
	BOOLEAN  bPHSI = 0;
	LONG ulPhsStatus = 0;
	UINT 	numBytesCompressed = 0;
	struct sk_buff *newPacket = NULL; 
	struct sk_buff *Packet = *pPacket;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL, "In PHSTransmit");

	if(!bEthCSSupport)
		BytesToRemove=ETH_HLEN;
	/* 
		Accumulate the header upto the size we support supression 
		from NDIS packet 
	*/

	usPacketType=((struct ethhdr *)(Packet->data))->h_proto;

	
	pucPHSPktHdrInBuf = Packet->data + BytesToRemove;
	//considering data after ethernet header
	if((*PacketLen - BytesToRemove) < MAX_PHS_LENGTHS)
	{

		unPHSPktHdrBytesCopied = (*PacketLen - BytesToRemove);
	}
	else
	{
		unPHSPktHdrBytesCopied = MAX_PHS_LENGTHS;
	}

	if( (unPHSPktHdrBytesCopied > 0 ) && 
		(unPHSPktHdrBytesCopied <= MAX_PHS_LENGTHS))
	{

				
		//DumpDataPacketHeader(pucPHSPktHdrInBuf);

		// Step 2 Supress Header using PHS and fill into intermediate ucaPHSPktHdrOutBuf.
	// Suppress only if IP Header and PHS Enabled For the Service Flow
		if(((usPacketType == ETHERNET_FRAMETYPE_IPV4) || 
			(usPacketType == ETHERNET_FRAMETYPE_IPV6)) && 
			(bHeaderSuppressionEnabled)) 
		{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nTrying to PHS Compress Using Classifier rule 0x%X",uiClassifierRuleID);

				
				unPHSNewPktHeaderLen = unPHSPktHdrBytesCopied;
				ulPhsStatus = PhsCompress(&Adapter->stBCMPhsContext,
					Vcid,
					uiClassifierRuleID,
					pucPHSPktHdrInBuf,
					pucPHSPktHdrOutBuf,
					&unPhsOldHdrSize,
					&unPHSNewPktHeaderLen);
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nPHS Old header Size : %d New Header Size  %d\n",unPhsOldHdrSize,unPHSNewPktHeaderLen);
				
				if(unPHSNewPktHeaderLen == unPhsOldHdrSize)
				{
					if(  ulPhsStatus == STATUS_PHS_COMPRESSED)
							bPHSI = *pucPHSPktHdrOutBuf;
					ulPhsStatus = STATUS_PHS_NOCOMPRESSION;
				}
				
				if(  ulPhsStatus == STATUS_PHS_COMPRESSED)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"PHS Sending packet Compressed");
					
					if(skb_cloned(Packet))
					{
						newPacket = skb_copy(Packet, GFP_ATOMIC);
						
						if(newPacket == NULL)
							return STATUS_FAILURE;
						
						bcm_kfree_skb(Packet);
						*pPacket = Packet = newPacket;
						pucPHSPktHdrInBuf = Packet->data  + BytesToRemove;
					}
				
					numBytesCompressed = unPhsOldHdrSize - (unPHSNewPktHeaderLen+PHSI_LEN);
				
					OsalMemMove(pucPHSPktHdrInBuf + numBytesCompressed, pucPHSPktHdrOutBuf, unPHSNewPktHeaderLen + PHSI_LEN);
					OsalMemMove(Packet->data + numBytesCompressed, Packet->data, BytesToRemove);
					skb_pull(Packet, numBytesCompressed);
					
					return STATUS_SUCCESS;
				}

				else
				{
					//if one byte headroom is not available, increase it through skb_cow
					if(!(skb_headroom(Packet) > 0)) 
					{
						if(skb_cow(Packet, 1))
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "SKB Cow Failed\n");
							return STATUS_FAILURE;
						}
					}
					skb_push(Packet, 1);

					// CAUTION: The MAC Header is getting corrupted here for IP CS - can be saved by copying 14 Bytes.  not needed .... hence corrupting it.
					*(Packet->data + BytesToRemove) = bPHSI;
					return STATUS_SUCCESS;
			}
		}
		else
		{
			if(!bHeaderSuppressionEnabled)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nHeader Suppression Disabled For SF: No PHS\n");
			}

			return STATUS_SUCCESS;
		}
	}
	
	//BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"PHSTransmit : Dumping data packet After PHS");
	return STATUS_SUCCESS;
}

int PHSRecieve(PMINI_ADAPTER Adapter,
					USHORT usVcid,
					struct sk_buff *packet,
					UINT *punPacketLen,
					UCHAR *pucEthernetHdr,
					UINT	bHeaderSuppressionEnabled)
{
	u32   nStandardPktHdrLen            		= 0;
	u32   nTotalsupressedPktHdrBytes  = 0;
	int     ulPhsStatus 		= 0;
	PUCHAR pucInBuff = NULL ;
	UINT TotalBytesAdded = 0;
	if(!bHeaderSuppressionEnabled)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nPhs Disabled for incoming packet");
		return ulPhsStatus;
	}
	
	pucInBuff = packet->data;

	//Restore  PHS suppressed header
	nStandardPktHdrLen = packet->len;
	ulPhsStatus = PhsDeCompress(&Adapter->stBCMPhsContext,
		usVcid,
		pucInBuff,
		Adapter->ucaPHSPktRestoreBuf,
		&nTotalsupressedPktHdrBytes,
		&nStandardPktHdrLen);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nSupressed PktHdrLen : 0x%x Restored PktHdrLen : 0x%x",
					nTotalsupressedPktHdrBytes,nStandardPktHdrLen);

	if(ulPhsStatus != STATUS_PHS_COMPRESSED)
	{
		skb_pull(packet, 1);
		return STATUS_SUCCESS;
	}
	else
	{
		TotalBytesAdded = nStandardPktHdrLen - nTotalsupressedPktHdrBytes - PHSI_LEN;
		if(TotalBytesAdded)
		{
			if(skb_headroom(packet) >= (SKB_RESERVE_ETHERNET_HEADER + TotalBytesAdded))
				skb_push(packet, TotalBytesAdded);
			else
			{
				if(skb_cow(packet, skb_headroom(packet) + TotalBytesAdded))
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "cow failed in receive\n");
					return STATUS_FAILURE;
				}
				
				skb_push(packet, TotalBytesAdded);
			}
		}

		OsalMemMove(packet->data, Adapter->ucaPHSPktRestoreBuf, nStandardPktHdrLen);
	}

	return STATUS_SUCCESS;
}

void DumpDataPacketHeader(PUCHAR pPkt)
{
	struct iphdr *iphd = (struct iphdr*)pPkt;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Phs Send/Recieve : IP Packet Hdr \n");
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"TOS : %x \n",iphd->tos);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Src  IP : %x \n",iphd->saddr);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Dest IP : %x \n \n",iphd->daddr);

}

void DumpFullPacket(UCHAR *pBuf,UINT nPktLen)
{
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"Dumping Data Packet");
    BCM_DEBUG_PRINT_BUFFER(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,pBuf,nPktLen);
}

//-----------------------------------------------------------------------------
// Procedure:   phs_init
//
// Description: This routine is responsible for allocating memory for classifier and
// PHS rules.
//
// Arguments:
// pPhsdeviceExtension - ptr to Device extension containing PHS Classifier rules and PHS Rules , RX, TX buffer etc
//
// Returns:
// TRUE(1)	-If allocation of memory was success full.
// FALSE	-If allocation of memory fails.
//-----------------------------------------------------------------------------
int phs_init(PPHS_DEVICE_EXTENSION pPhsdeviceExtension,PMINI_ADAPTER Adapter)
{
	int i;
	S_SERVICEFLOW_TABLE *pstServiceFlowTable;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nPHS:phs_init function ");

	if(pPhsdeviceExtension->pstServiceFlowPhsRulesTable)
		return -EINVAL;

	pPhsdeviceExtension->pstServiceFlowPhsRulesTable = 
      (S_SERVICEFLOW_TABLE*)OsalMemAlloc(sizeof(S_SERVICEFLOW_TABLE),
            PHS_MEM_TAG);

    if(pPhsdeviceExtension->pstServiceFlowPhsRulesTable)
	{
		OsalZeroMemory(pPhsdeviceExtension->pstServiceFlowPhsRulesTable,
              sizeof(S_SERVICEFLOW_TABLE));
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation ServiceFlowPhsRulesTable failed");
		return -ENOMEM;
	}

	pstServiceFlowTable = pPhsdeviceExtension->pstServiceFlowPhsRulesTable;
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		S_SERVICEFLOW_ENTRY sServiceFlow = pstServiceFlowTable->stSFList[i];
		sServiceFlow.pstClassifierTable = (S_CLASSIFIER_TABLE*)OsalMemAlloc(
            sizeof(S_CLASSIFIER_TABLE), PHS_MEM_TAG);
		if(sServiceFlow.pstClassifierTable)
		{
			OsalZeroMemory(sServiceFlow.pstClassifierTable,sizeof(S_CLASSIFIER_TABLE));
			pstServiceFlowTable->stSFList[i].pstClassifierTable = sServiceFlow.pstClassifierTable;
    	}
		else
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
			free_phs_serviceflow_rules(pPhsdeviceExtension->
                pstServiceFlowPhsRulesTable);
			pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
			return -ENOMEM;
		}
	}
	
	
	pPhsdeviceExtension->CompressedTxBuffer = 
          OsalMemAlloc(PHS_BUFFER_SIZE,PHS_MEM_TAG);

    if(pPhsdeviceExtension->CompressedTxBuffer == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
		free_phs_serviceflow_rules(pPhsdeviceExtension->pstServiceFlowPhsRulesTable);
		pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
		return -ENOMEM;
	}
	
	pPhsdeviceExtension->UnCompressedRxBuffer = 
      OsalMemAlloc(PHS_BUFFER_SIZE,PHS_MEM_TAG);
	if(pPhsdeviceExtension->UnCompressedRxBuffer == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
		OsalMemFree(pPhsdeviceExtension->CompressedTxBuffer,PHS_BUFFER_SIZE);
		free_phs_serviceflow_rules(pPhsdeviceExtension->pstServiceFlowPhsRulesTable);
		pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
		return -ENOMEM;
	}


		
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\n phs_init Successfull");
	return STATUS_SUCCESS;
}


int PhsCleanup(IN PPHS_DEVICE_EXTENSION pPHSDeviceExt)
{
	if(pPHSDeviceExt->pstServiceFlowPhsRulesTable)
	{
		free_phs_serviceflow_rules(pPHSDeviceExt->pstServiceFlowPhsRulesTable);
		pPHSDeviceExt->pstServiceFlowPhsRulesTable = NULL;
	}

	if(pPHSDeviceExt->CompressedTxBuffer)
	{
		OsalMemFree(pPHSDeviceExt->CompressedTxBuffer,PHS_BUFFER_SIZE);
		pPHSDeviceExt->CompressedTxBuffer = NULL;
	}
	if(pPHSDeviceExt->UnCompressedRxBuffer)
	{
		OsalMemFree(pPHSDeviceExt->UnCompressedRxBuffer,PHS_BUFFER_SIZE);
		pPHSDeviceExt->UnCompressedRxBuffer = NULL;
	}

	return 0;
}



//PHS functions
/*++
PhsUpdateClassifierRule

Routine Description:
    Exported function to add or modify a PHS Rule.

Arguments:
	IN void* pvContext - PHS Driver Specific Context 
	IN B_UINT16 uiVcid    - The Service Flow ID for which the PHS rule applies
	IN B_UINT16  uiClsId   - The Classifier ID within the Service Flow for which the PHS rule applies.
	IN S_PHS_RULE *psPhsRule - The PHS Rule strcuture to be added to the PHS Rule table.
   
Return Value:

    0 if successful,
    >0 Error.

--*/
ULONG PhsUpdateClassifierRule(IN void* pvContext,
								IN B_UINT16  uiVcid ,
								IN B_UINT16  uiClsId   ,
								IN S_PHS_RULE *psPhsRule,
								IN B_UINT8  u8AssociatedPHSI)
{
	ULONG lStatus =0;
	UINT nSFIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    


	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"PHS With Corr2 Changes \n");

	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"Invalid Device Extension\n");
		return ERR_PHS_INVALID_DEVICE_EXETENSION;
	}

	
	if(u8AssociatedPHSI == 0)
	{
		return ERR_PHS_INVALID_PHS_RULE;
	}

	/* Retrieve the SFID Entry Index for requested Service Flow */
  
	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable, 
	                uiVcid,&pstServiceFlowEntry);

    if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		/* This is a new SF. Create a mapping entry for this */
		lStatus = CreateSFToClassifierRuleMapping(uiVcid, uiClsId,
		      pDeviceExtension->pstServiceFlowPhsRulesTable, psPhsRule, u8AssociatedPHSI);
		return lStatus;
	}

	/* SF already Exists Add PHS Rule to existing SF */
  	lStatus = CreateClassiferToPHSRuleMapping(uiVcid, uiClsId,
  	          pstServiceFlowEntry, psPhsRule, u8AssociatedPHSI);

    return lStatus;
}

/*++
PhsDeletePHSRule

Routine Description:
   Deletes the specified phs Rule within Vcid

Arguments:
	IN void* pvContext - PHS Driver Specific Context 
	IN B_UINT16  uiVcid    - The Service Flow ID for which the PHS rule applies
	IN B_UINT8  u8PHSI   - the PHS Index identifying PHS rule to be deleted.
	   
Return Value:

    0 if successful,
    >0 Error.

--*/

ULONG PhsDeletePHSRule(IN void* pvContext,IN B_UINT16 uiVcid,IN B_UINT8 u8PHSI)
{
	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_TABLE *pstClassifierRulesTable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    

	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "======>\n");

	if(pDeviceExtension)
	{

		//Retrieve the SFID Entry Index for requested Service Flow
		nSFIndex = GetServiceFlowEntry(pDeviceExtension
		      ->pstServiceFlowPhsRulesTable,uiVcid,&pstServiceFlowEntry);

       if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		pstClassifierRulesTable=pstServiceFlowEntry->pstClassifierTable;
		if(pstClassifierRulesTable)
		{
			for(nClsidIndex=0;nClsidIndex<MAX_PHSRULE_PER_SF;nClsidIndex++)
			{
				if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].bUsed && pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                        .pstPhsRule->u8PHSI == u8PHSI)
					{
						if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule
                                                ->u8RefCnt)
							pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule
						          ->u8RefCnt--;
						if(0 == pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                            .pstPhsRule->u8RefCnt)
							OsalMemFree(pstClassifierRulesTable
						    ->stActivePhsRulesList[nClsidIndex].pstPhsRule,
						      sizeof(S_PHS_RULE));
						OsalZeroMemory(&pstClassifierRulesTable
							->stActivePhsRulesList[nClsidIndex], 
							sizeof(S_CLASSIFIER_ENTRY));
					}
				}
			}
		}

	}
	return lStatus;
}

/*++
PhsDeleteClassifierRule

Routine Description:
    Exported function to Delete a PHS Rule for the SFID,CLSID Pair.

Arguments:
	IN void* pvContext - PHS Driver Specific Context 
	IN B_UINT16  uiVcid    - The Service Flow ID for which the PHS rule applies
	IN B_UINT16  uiClsId   - The Classifier ID within the Service Flow for which the PHS rule applies.
	   
Return Value:

    0 if successful,
    >0 Error.

--*/
ULONG PhsDeleteClassifierRule(IN void* pvContext,IN B_UINT16 uiVcid ,IN B_UINT16  uiClsId)
{
	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	if(pDeviceExtension)
	{
		//Retrieve the SFID Entry Index for requested Service Flow
		nSFIndex = GetServiceFlowEntry(pDeviceExtension
		      ->pstServiceFlowPhsRulesTable, uiVcid, &pstServiceFlowEntry);
		if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                  uiClsId, eActiveClassifierRuleContext, &pstClassifierEntry);
		if((nClsidIndex != PHS_INVALID_TABLE_INDEX) && (!pstClassifierEntry->bUnclassifiedPHSRule))
		{
			if(pstClassifierEntry->pstPhsRule)
			{
				if(pstClassifierEntry->pstPhsRule->u8RefCnt)
				pstClassifierEntry->pstPhsRule->u8RefCnt--;
				if(0==pstClassifierEntry->pstPhsRule->u8RefCnt)
				OsalMemFree(pstClassifierEntry->pstPhsRule,sizeof(S_PHS_RULE));
					
			}
			OsalZeroMemory(pstClassifierEntry,sizeof(S_CLASSIFIER_ENTRY));
		}

		nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                    uiClsId,eOldClassifierRuleContext,&pstClassifierEntry);

	   if((nClsidIndex != PHS_INVALID_TABLE_INDEX) && (!pstClassifierEntry->bUnclassifiedPHSRule))
		{
			if(pstClassifierEntry->pstPhsRule)
			//Delete the classifier entry
			OsalMemFree(pstClassifierEntry->pstPhsRule,sizeof(S_PHS_RULE));
			OsalZeroMemory(pstClassifierEntry,sizeof(S_CLASSIFIER_ENTRY));
		}
	}
	return lStatus;
}

/*++
PhsDeleteSFRules

Routine Description:
    Exported function to Delete a all PHS Rules for the SFID.

Arguments:
	IN void* pvContext - PHS Driver Specific Context 
	IN B_UINT16 uiVcid   - The Service Flow ID for which the PHS rules need to be deleted
	   
Return Value:

    0 if successful,
    >0 Error.

--*/
ULONG PhsDeleteSFRules(IN void* pvContext,IN B_UINT16 uiVcid)
{

	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0  ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_TABLE *pstClassifierRulesTable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"====> \n");

	if(pDeviceExtension)
	{
		//Retrieve the SFID Entry Index for requested Service Flow
		nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
		                  uiVcid,&pstServiceFlowEntry);
		if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		pstClassifierRulesTable=pstServiceFlowEntry->pstClassifierTable;
		if(pstClassifierRulesTable)
		{
			for(nClsidIndex=0;nClsidIndex<MAX_PHSRULE_PER_SF;nClsidIndex++)
			{
				if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                                        .pstPhsRule->u8RefCnt)
						pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
						                                    .pstPhsRule->u8RefCnt--;
					if(0==pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                                          .pstPhsRule->u8RefCnt)
						OsalMemFree(pstClassifierRulesTable
						            ->stActivePhsRulesList[nClsidIndex].pstPhsRule,
						             sizeof(S_PHS_RULE));
					    pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                        .pstPhsRule = NULL;
				}
				OsalZeroMemory(&pstClassifierRulesTable
                    ->stActivePhsRulesList[nClsidIndex],sizeof(S_CLASSIFIER_ENTRY));
				if(pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                                        .pstPhsRule->u8RefCnt)
						pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
						                  .pstPhsRule->u8RefCnt--;
					if(0 == pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                                        .pstPhsRule->u8RefCnt)
						OsalMemFree(pstClassifierRulesTable
						      ->stOldPhsRulesList[nClsidIndex].pstPhsRule,
						       sizeof(S_PHS_RULE));
					pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                              .pstPhsRule = NULL;
				}
				OsalZeroMemory(&pstClassifierRulesTable
                  ->stOldPhsRulesList[nClsidIndex],
                   sizeof(S_CLASSIFIER_ENTRY));
			}
		}
		pstServiceFlowEntry->bUsed = FALSE;
		pstServiceFlowEntry->uiVcid = 0;

	}

	return lStatus;
}


/*++
PhsCompress

Routine Description:
    Exported function to compress the data using PHS.

Arguments:
	IN void* pvContext - PHS Driver Specific Context. 
	IN B_UINT16 uiVcid    - The Service Flow ID to which current packet header compression applies.
	IN UINT  uiClsId   - The Classifier ID to which current packet header compression applies.
	IN void *pvInputBuffer - The Input buffer containg packet header data
	IN void *pvOutputBuffer - The output buffer returned by this function after PHS
	IN UINT *pOldHeaderSize  - The actual size of the header before PHS
	IN UINT *pNewHeaderSize - The new size of the header after applying PHS
   
Return Value:

    0 if successful,
    >0 Error.

--*/
ULONG PhsCompress(IN void* pvContext,
				  IN B_UINT16 uiVcid,
				  IN B_UINT16 uiClsId,
				  IN void *pvInputBuffer,
				  OUT void *pvOutputBuffer,
				  OUT UINT *pOldHeaderSize,
				  OUT UINT *pNewHeaderSize )
{
	UINT nSFIndex =0, nClsidIndex =0  ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
	S_PHS_RULE *pstPhsRule = NULL;
	ULONG lStatus =0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    
    

	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;


	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Invalid Device Extension\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;

	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Suppressing header \n");


	//Retrieve the SFID Entry Index for requested Service Flow
	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
	                  uiVcid,&pstServiceFlowEntry);
	if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"SFID Match Failed\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}

	nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                uiClsId,eActiveClassifierRuleContext,&pstClassifierEntry);

    if(nClsidIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"No PHS Rule Defined For Classifier\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}


	//get rule from SF id,Cls ID pair and proceed
	pstPhsRule =  pstClassifierEntry->pstPhsRule;

	if(!ValidatePHSRuleComplete(pstPhsRule))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"PHS Rule Defined For Classifier But Not Complete\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}

	//Compress Packet
	lStatus = phs_compress(pstPhsRule,(PUCHAR)pvInputBuffer, 
	      (PUCHAR)pvOutputBuffer, pOldHeaderSize,pNewHeaderSize);

	if(lStatus == STATUS_PHS_COMPRESSED)
	{
		pstPhsRule->PHSModifiedBytes += *pOldHeaderSize - *pNewHeaderSize - 1;
		pstPhsRule->PHSModifiedNumPackets++;
	}
	else
		pstPhsRule->PHSErrorNumPackets++;

	return lStatus;
}

/*++
PhsDeCompress

Routine Description:
    Exported function to restore the packet header in Rx path.

Arguments:
	IN void* pvContext - PHS Driver Specific Context. 
	IN B_UINT16 uiVcid    - The Service Flow ID to which current packet header restoration applies.
	IN  void *pvInputBuffer - The Input buffer containg suppressed packet header data
	OUT void *pvOutputBuffer - The output buffer returned by this function after restoration
	OUT UINT *pHeaderSize   - The packet header size after restoration is returned in this parameter.
   
Return Value:

    0 if successful,
    >0 Error.

--*/
ULONG PhsDeCompress(IN void* pvContext,
				  IN B_UINT16 uiVcid,
				  IN void *pvInputBuffer,
				  OUT void *pvOutputBuffer,
				  OUT UINT *pInHeaderSize,
				  OUT UINT *pOutHeaderSize )
{
	UINT nSFIndex =0, nPhsRuleIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_PHS_RULE *pstPhsRule = NULL;
	UINT phsi;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension= 
        (PPHS_DEVICE_EXTENSION)pvContext;

	*pInHeaderSize = 0;	

	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"Invalid Device Extension\n");
		return ERR_PHS_INVALID_DEVICE_EXETENSION;
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"Restoring header \n");

	phsi = *((unsigned char *)(pvInputBuffer));
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"PHSI To Be Used For restore : %x \n",phsi);
    if(phsi == UNCOMPRESSED_PACKET )
	{
		return STATUS_PHS_NOCOMPRESSION;
	}

	//Retrieve the SFID Entry Index for requested Service Flow
	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
	      uiVcid,&pstServiceFlowEntry);
	if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"SFID Match Failed During Lookup\n");
		return ERR_SF_MATCH_FAIL;
	}

	nPhsRuleIndex = GetPhsRuleEntry(pstServiceFlowEntry->pstClassifierTable,phsi,
          eActiveClassifierRuleContext,&pstPhsRule);
	if(nPhsRuleIndex == PHS_INVALID_TABLE_INDEX)
	{
		//Phs Rule does not exist in  active rules table. Lets try in the old rules table.
		nPhsRuleIndex = GetPhsRuleEntry(pstServiceFlowEntry->pstClassifierTable,
		      phsi,eOldClassifierRuleContext,&pstPhsRule);
		if(nPhsRuleIndex == PHS_INVALID_TABLE_INDEX)
		{
			return ERR_PHSRULE_MATCH_FAIL;	
		}
	
	}

	*pInHeaderSize = phs_decompress((PUCHAR)pvInputBuffer,
            (PUCHAR)pvOutputBuffer,pstPhsRule,pOutHeaderSize);
	
	pstPhsRule->PHSModifiedBytes += *pOutHeaderSize - *pInHeaderSize - 1;

	pstPhsRule->PHSModifiedNumPackets++;
	return STATUS_PHS_COMPRESSED;
}


//-----------------------------------------------------------------------------
// Procedure:   free_phs_serviceflow_rules
//
// Description: This routine is responsible for freeing memory allocated for PHS rules.
//
// Arguments:
// rules	- ptr to S_SERVICEFLOW_TABLE structure.
//
// Returns:
// Does not return any value.
//-----------------------------------------------------------------------------

void free_phs_serviceflow_rules(S_SERVICEFLOW_TABLE *psServiceFlowRulesTable)
{
	int i,j;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "=======>\n");
    if(psServiceFlowRulesTable)
	{
		for(i=0;i<MAX_SERVICEFLOWS;i++)
		{
			S_SERVICEFLOW_ENTRY stServiceFlowEntry = 
                psServiceFlowRulesTable->stSFList[i];
			S_CLASSIFIER_TABLE *pstClassifierRulesTable = 
                stServiceFlowEntry.pstClassifierTable;
							
			if(pstClassifierRulesTable)
			{
				for(j=0;j<MAX_PHSRULE_PER_SF;j++)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule)
					{
						if(pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
                                                                                        ->u8RefCnt)
							pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
  							                                                ->u8RefCnt--;
						if(0==pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
                                                                ->u8RefCnt)
							OsalMemFree(pstClassifierRulesTable->stActivePhsRulesList[j].
							                                              pstPhsRule, sizeof(S_PHS_RULE));
						pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule = NULL;
					}
					if(pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule)
					{
						if(pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule   
                                                                ->u8RefCnt)
							pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule
							                                          ->u8RefCnt--;
						if(0==pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule
                                                                      ->u8RefCnt)
							OsalMemFree(pstClassifierRulesTable->stOldPhsRulesList[j]
							                        .pstPhsRule,sizeof(S_PHS_RULE));
						pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule = NULL;
					}
				}
			    OsalMemFree(pstClassifierRulesTable,sizeof(S_CLASSIFIER_TABLE));
			    stServiceFlowEntry.pstClassifierTable = pstClassifierRulesTable = NULL;
			}
		}
	}

	OsalMemFree(psServiceFlowRulesTable,sizeof(S_SERVICEFLOW_TABLE));
	psServiceFlowRulesTable = NULL;
}



BOOLEAN ValidatePHSRuleComplete(IN S_PHS_RULE *psPhsRule)
{
	if(psPhsRule)
	{
		if(!psPhsRule->u8PHSI)
		{
			// PHSI is not valid 
			return FALSE;
		}

		if(!psPhsRule->u8PHSS)
		{
			//PHSS Is Undefined
			return FALSE;
		}

		//Check if PHSF is defines for the PHS Rule
		if(!psPhsRule->u8PHSFLength) // If any part of PHSF is valid then Rule contains valid PHSF
		{
			return FALSE;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

UINT UpdateServiceFlowParams(S_SERVICEFLOW_TABLE *psServiceFlowTable,B_UINT16 uiVcid,B_UINT16 uiNewVcid)
{
	int  i;
	
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		if(psServiceFlowTable->stSFList[i].bUsed)
		{
			if(psServiceFlowTable->stSFList[i].uiVcid == uiVcid)
			{
				psServiceFlowTable->stSFList[i].uiVcid = uiNewVcid;
				break;
			}
		}
	}

	return TRUE;
} /* UpdateServiceFlowParams() */

UINT GetServiceFlowEntry(IN S_SERVICEFLOW_TABLE *psServiceFlowTable,
    IN B_UINT16 uiVcid,S_SERVICEFLOW_ENTRY **ppstServiceFlowEntry)
{
	int  i;
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		if(psServiceFlowTable->stSFList[i].bUsed)
		{
			if(psServiceFlowTable->stSFList[i].uiVcid == uiVcid)
			{
				*ppstServiceFlowEntry = &psServiceFlowTable->stSFList[i];
				return i;
			}
		}
	}

	*ppstServiceFlowEntry = NULL;
	return PHS_INVALID_TABLE_INDEX;
}


UINT GetClassifierEntry(IN S_CLASSIFIER_TABLE *pstClassifierTable,
        IN B_UINT32 uiClsid,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,
        OUT S_CLASSIFIER_ENTRY **ppstClassifierEntry)
{
	int  i;
	S_CLASSIFIER_ENTRY *psClassifierRules = NULL;
	for(i=0;i<MAX_PHSRULE_PER_SF;i++)
	{

		if(eClsContext == eActiveClassifierRuleContext)
		{
			psClassifierRules = &pstClassifierTable->stActivePhsRulesList[i];
		}
		else
		{
			psClassifierRules = &pstClassifierTable->stOldPhsRulesList[i];
		}

		if(psClassifierRules->bUsed)
		{
			if(psClassifierRules->uiClassifierRuleId == uiClsid)
			{
				*ppstClassifierEntry = psClassifierRules;
				return i;
			}
		}
		
	}

	*ppstClassifierEntry = NULL;
	return PHS_INVALID_TABLE_INDEX;
}

UINT GetPhsRuleEntry(IN S_CLASSIFIER_TABLE *pstClassifierTable,
      IN B_UINT32 uiPHSI,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,
      OUT S_PHS_RULE **ppstPhsRule)
{
	int  i;
	S_CLASSIFIER_ENTRY *pstClassifierRule = NULL;
	for(i=0;i<MAX_PHSRULE_PER_SF;i++)
	{
		if(eClsContext == eActiveClassifierRuleContext)
		{
			pstClassifierRule = &pstClassifierTable->stActivePhsRulesList[i];
		}
		else
		{
			pstClassifierRule = &pstClassifierTable->stOldPhsRulesList[i];
		}
		if(pstClassifierRule->bUsed)
		{
			if(pstClassifierRule->u8PHSI == uiPHSI)
			{
				*ppstPhsRule = pstClassifierRule->pstPhsRule;
				return i;
			}
		}
		
	}

	*ppstPhsRule = NULL;
	return PHS_INVALID_TABLE_INDEX;
}

UINT CreateSFToClassifierRuleMapping(IN B_UINT16 uiVcid,IN B_UINT16  uiClsId,
                      IN S_SERVICEFLOW_TABLE *psServiceFlowTable,S_PHS_RULE *psPhsRule,
                      B_UINT8 u8AssociatedPHSI)
{

    S_CLASSIFIER_TABLE *psaClassifiertable = NULL;
	UINT uiStatus = 0;
	int iSfIndex;
	BOOLEAN bFreeEntryFound =FALSE;
	//Check for a free entry in SFID table
	for(iSfIndex=0;iSfIndex < MAX_SERVICEFLOWS;iSfIndex++)
	{
		if(!psServiceFlowTable->stSFList[iSfIndex].bUsed)
		{
			bFreeEntryFound = TRUE;
			break;
		}
	}

	if(!bFreeEntryFound)
		return ERR_SFTABLE_FULL;
	
	
	psaClassifiertable = psServiceFlowTable->stSFList[iSfIndex].pstClassifierTable;
	uiStatus = CreateClassifierPHSRule(uiClsId,psaClassifiertable,psPhsRule,      
	                      eActiveClassifierRuleContext,u8AssociatedPHSI);
	if(uiStatus == PHS_SUCCESS)
	{
		//Add entry at free index to the SF
		psServiceFlowTable->stSFList[iSfIndex].bUsed = TRUE;
		psServiceFlowTable->stSFList[iSfIndex].uiVcid = uiVcid;
	}
	
	return uiStatus;

}

UINT CreateClassiferToPHSRuleMapping(IN B_UINT16 uiVcid,                        
            IN B_UINT16  uiClsId,IN S_SERVICEFLOW_ENTRY *pstServiceFlowEntry,
              S_PHS_RULE *psPhsRule,B_UINT8 u8AssociatedPHSI)
{
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
	UINT uiStatus =PHS_SUCCESS;
	UINT nClassifierIndex = 0;
	S_CLASSIFIER_TABLE *psaClassifiertable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    psaClassifiertable = pstServiceFlowEntry->pstClassifierTable;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "==>");

	/* Check if the supplied Classifier already exists */
	nClassifierIndex =GetClassifierEntry(
	            pstServiceFlowEntry->pstClassifierTable,uiClsId,
	            eActiveClassifierRuleContext,&pstClassifierEntry);
	if(nClassifierIndex == PHS_INVALID_TABLE_INDEX)
	{
		/*
		    The Classifier doesn't exist. So its a new classifier being added.
		     Add new entry to associate PHS Rule to the Classifier
		*/
		
		uiStatus = CreateClassifierPHSRule(uiClsId,psaClassifiertable,
		    psPhsRule,eActiveClassifierRuleContext,u8AssociatedPHSI);
		return uiStatus;
	}

	/* 
	  The Classifier exists.The PHS Rule for this classifier 
	  is being modified
	  */
	if(pstClassifierEntry->u8PHSI == psPhsRule->u8PHSI)
	{
		if(pstClassifierEntry->pstPhsRule == NULL)
			return ERR_PHS_INVALID_PHS_RULE;

		/* 
		    This rule already exists if any fields are changed for this PHS 
		    rule update them.
		 */
		 /* If any part of PHSF is valid then we update PHSF */
		if(psPhsRule->u8PHSFLength)
		{
			//update PHSF
			OsalMemMove(pstClassifierEntry->pstPhsRule->u8PHSF, 
			    psPhsRule->u8PHSF , MAX_PHS_LENGTHS);
		}
		if(psPhsRule->u8PHSFLength)
		{
			//update PHSFLen
			pstClassifierEntry->pstPhsRule->u8PHSFLength = 
			    psPhsRule->u8PHSFLength;
		}
		if(psPhsRule->u8PHSMLength)
		{
			//update PHSM
			OsalMemMove(pstClassifierEntry->pstPhsRule->u8PHSM, 
			    psPhsRule->u8PHSM, MAX_PHS_LENGTHS);
		}
		if(psPhsRule->u8PHSMLength)
		{
			//update PHSM Len
			pstClassifierEntry->pstPhsRule->u8PHSMLength = 
			    psPhsRule->u8PHSMLength;
		}
		if(psPhsRule->u8PHSS)
		{
			//update PHSS
			pstClassifierEntry->pstPhsRule->u8PHSS = psPhsRule->u8PHSS;
		}
		
		//update PHSV
		pstClassifierEntry->pstPhsRule->u8PHSV = psPhsRule->u8PHSV;
		
	}
	else
	{
		/*
		  A new rule is being set for this classifier. 
		*/
		uiStatus=UpdateClassifierPHSRule( uiClsId,  pstClassifierEntry,  
		      psaClassifiertable,  psPhsRule, u8AssociatedPHSI);
	}



	return uiStatus;
}

UINT CreateClassifierPHSRule(IN B_UINT16  uiClsId,
    S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,
    E_CLASSIFIER_ENTRY_CONTEXT eClsContext,B_UINT8 u8AssociatedPHSI)
{
	UINT iClassifierIndex = 0;
	BOOLEAN bFreeEntryFound = FALSE;
	S_CLASSIFIER_ENTRY *psClassifierRules = NULL;
	UINT nStatus = PHS_SUCCESS;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"Inside CreateClassifierPHSRule");
    if(psaClassifiertable == NULL)
	{
		return ERR_INVALID_CLASSIFIERTABLE_FOR_SF;
	}

	if(eClsContext == eOldClassifierRuleContext)
	{
		/* If An Old Entry for this classifier ID already exists in the 
		    old rules table replace it. */
		    
		iClassifierIndex = 
		GetClassifierEntry(psaClassifiertable, uiClsId,
		            eClsContext,&psClassifierRules);
		if(iClassifierIndex != PHS_INVALID_TABLE_INDEX)
		{
			/*
			    The Classifier already exists in the old rules table
		        Lets replace the old classifier with the new one.
			*/
			bFreeEntryFound = TRUE;
		}
	}

	if(!bFreeEntryFound)
	{
		/*
		  Continue to search for a free location to add the rule 
		*/
		for(iClassifierIndex = 0; iClassifierIndex < 
            MAX_PHSRULE_PER_SF; iClassifierIndex++)
		{
			if(eClsContext == eActiveClassifierRuleContext)
			{
				psClassifierRules = 
              &psaClassifiertable->stActivePhsRulesList[iClassifierIndex];
			}
			else
			{
				psClassifierRules = 
                &psaClassifiertable->stOldPhsRulesList[iClassifierIndex];
			}

			if(!psClassifierRules->bUsed)
			{
				bFreeEntryFound = TRUE;
				break;
			}
		}
	}

	if(!bFreeEntryFound)
	{
		if(eClsContext == eActiveClassifierRuleContext)
		{
			return ERR_CLSASSIFIER_TABLE_FULL;
		}
		else
		{
			//Lets replace the oldest rule if we are looking in old Rule table
			if(psaClassifiertable->uiOldestPhsRuleIndex >= 
                MAX_PHSRULE_PER_SF)
			{
				psaClassifiertable->uiOldestPhsRuleIndex =0;
			}

			iClassifierIndex = psaClassifiertable->uiOldestPhsRuleIndex;
			psClassifierRules = 
              &psaClassifiertable->stOldPhsRulesList[iClassifierIndex];

          (psaClassifiertable->uiOldestPhsRuleIndex)++;
		}
	}

	if(eClsContext == eOldClassifierRuleContext)
	{
		if(psClassifierRules->pstPhsRule == NULL)
		{
			psClassifierRules->pstPhsRule = (S_PHS_RULE*)OsalMemAlloc
                (sizeof(S_PHS_RULE),PHS_MEM_TAG);

          if(NULL == psClassifierRules->pstPhsRule)
				return ERR_PHSRULE_MEMALLOC_FAIL;
		}

		psClassifierRules->bUsed = TRUE;
		psClassifierRules->uiClassifierRuleId = uiClsId;
		psClassifierRules->u8PHSI = psPhsRule->u8PHSI;
		psClassifierRules->bUnclassifiedPHSRule = psPhsRule->bUnclassifiedPHSRule;

        /* Update The PHS rule */
		OsalMemMove(psClassifierRules->pstPhsRule, 
		    psPhsRule, sizeof(S_PHS_RULE));
	}
	else
	{
		nStatus = UpdateClassifierPHSRule(uiClsId,psClassifierRules,
            psaClassifiertable,psPhsRule,u8AssociatedPHSI);
	}
	return nStatus;
}


UINT UpdateClassifierPHSRule(IN B_UINT16  uiClsId,
      IN S_CLASSIFIER_ENTRY *pstClassifierEntry, 
      S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,
      B_UINT8 u8AssociatedPHSI)
{
	S_PHS_RULE *pstAddPhsRule = NULL;
	UINT              nPhsRuleIndex = 0;
	BOOLEAN       bPHSRuleOrphaned = FALSE;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	psPhsRule->u8RefCnt =0;

	/* Step 1 Deref Any Exisiting PHS Rule in this classifier Entry*/
	bPHSRuleOrphaned = DerefPhsRule( uiClsId, psaClassifiertable, 
	    pstClassifierEntry->pstPhsRule);

	//Step 2 Search if there is a PHS Rule with u8AssociatedPHSI in Classifier table for this SF
	nPhsRuleIndex =GetPhsRuleEntry(psaClassifiertable,u8AssociatedPHSI, 
	    eActiveClassifierRuleContext, &pstAddPhsRule);
	if(PHS_INVALID_TABLE_INDEX == nPhsRuleIndex)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAdding New PHSRuleEntry For Classifier");

		if(psPhsRule->u8PHSI == 0)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nError PHSI is Zero\n");	
			return ERR_PHS_INVALID_PHS_RULE;
		}
		//Step 2.a PHS Rule Does Not Exist .Create New PHS Rule for uiClsId
		if(FALSE == bPHSRuleOrphaned)
		{
			pstClassifierEntry->pstPhsRule = (S_PHS_RULE*)OsalMemAlloc(sizeof(S_PHS_RULE),PHS_MEM_TAG);
			if(NULL == pstClassifierEntry->pstPhsRule)
			{
				return ERR_PHSRULE_MEMALLOC_FAIL;
			}
		}
		OsalMemMove(pstClassifierEntry->pstPhsRule, psPhsRule, sizeof(S_PHS_RULE));
	
	}
	else
	{
		//Step 2.b PHS Rule  Exists Tie uiClsId with the existing PHS Rule
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nTying Classifier to Existing PHS Rule");
		if(bPHSRuleOrphaned)
		{
		    if(pstClassifierEntry->pstPhsRule)
		    {
		    	//Just Free the PHS Rule as Ref Count is Zero
		    	OsalMemFree(pstClassifierEntry->pstPhsRule,sizeof(S_PHS_RULE));
			pstClassifierEntry->pstPhsRule = NULL;
			
		    }
		    
		}
		pstClassifierEntry->pstPhsRule = pstAddPhsRule;
	
	}
	pstClassifierEntry->bUsed = TRUE;
	pstClassifierEntry->u8PHSI = pstClassifierEntry->pstPhsRule->u8PHSI;
	pstClassifierEntry->uiClassifierRuleId = uiClsId;
	pstClassifierEntry->pstPhsRule->u8RefCnt++;
	pstClassifierEntry->bUnclassifiedPHSRule = pstClassifierEntry->pstPhsRule->bUnclassifiedPHSRule;

	return PHS_SUCCESS;

}

BOOLEAN DerefPhsRule(IN B_UINT16  uiClsId,S_CLASSIFIER_TABLE *psaClassifiertable,S_PHS_RULE *pstPhsRule)
{
	if(pstPhsRule==NULL)
		return FALSE;
	if(pstPhsRule->u8RefCnt)
		pstPhsRule->u8RefCnt--;
	if(0==pstPhsRule->u8RefCnt)
	{
		/*if(pstPhsRule->u8PHSI)
		//Store the currently active rule into the old rules list
		CreateClassifierPHSRule(uiClsId,psaClassifiertable,pstPhsRule,eOldClassifierRuleContext,pstPhsRule->u8PHSI);*/
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void DumpBuffer(PVOID BuffVAddress, int xferSize) 
{
	int i;
	int iPrintLength;
	PUCHAR temp=(PUCHAR)BuffVAddress;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	iPrintLength=(xferSize<32?xferSize:32);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\n");

	for (i=0;i < iPrintLength;i++) {
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "%x|",temp[i]);
	}		
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\n");
}
	

void DumpPhsRules(PPHS_DEVICE_EXTENSION pDeviceExtension)
{
	int i,j,k,l;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n Dumping PHS Rules : \n");
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		S_SERVICEFLOW_ENTRY stServFlowEntry = 
				pDeviceExtension->pstServiceFlowPhsRulesTable->stSFList[i];
		if(stServFlowEntry.bUsed)
		{
			for(j=0;j<MAX_PHSRULE_PER_SF;j++)
			{
				for(l=0;l<2;l++)
				{
					S_CLASSIFIER_ENTRY stClsEntry;
					if(l==0)
					{
						stClsEntry = stServFlowEntry.pstClassifierTable->stActivePhsRulesList[j];
						if(stClsEntry.bUsed)
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n Active PHS Rule : \n");
					}
					else 
					{
						stClsEntry = stServFlowEntry.pstClassifierTable->stOldPhsRulesList[j];
						if(stClsEntry.bUsed)
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n Old PHS Rule : \n");
					}
					if(stClsEntry.bUsed)
					{

						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n VCID  : %#X",stServFlowEntry.uiVcid);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n ClassifierID  : %#X",stClsEntry.uiClassifierRuleId);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSRuleID  : %#X",stClsEntry.u8PHSI);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n****************PHS Rule********************\n");
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSI  : %#X",stClsEntry.pstPhsRule->u8PHSI);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSFLength : %#X ",stClsEntry.pstPhsRule->u8PHSFLength);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSF : ");
						for(k=0;k<stClsEntry.pstPhsRule->u8PHSFLength;k++)
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "%#X  ",stClsEntry.pstPhsRule->u8PHSF[k]);
						}
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSMLength  : %#X",stClsEntry.pstPhsRule->u8PHSMLength);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSM :");
						for(k=0;k<stClsEntry.pstPhsRule->u8PHSMLength;k++)
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "%#X  ",stClsEntry.pstPhsRule->u8PHSM[k]);
						}
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSS : %#X ",stClsEntry.pstPhsRule->u8PHSS);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSV  : %#X",stClsEntry.pstPhsRule->u8PHSV);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n********************************************\n");
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Procedure:   phs_decompress
//
// Description: This routine restores the static fields within the packet.
//
// Arguments:
//	in_buf			- ptr to incoming packet buffer.
//	out_buf			- ptr to output buffer where the suppressed header is copied.
//	decomp_phs_rules - ptr to PHS rule.
//	header_size		- ptr to field which holds the phss or phsf_length.	
//
// Returns:
//	size -The number of bytes of dynamic fields present with in the incoming packet
//			header.
//	0	-If PHS rule is NULL.If PHSI is 0 indicateing packet as uncompressed.
//-----------------------------------------------------------------------------

int phs_decompress(unsigned char *in_buf,unsigned char *out_buf,
 S_PHS_RULE   *decomp_phs_rules,UINT *header_size)
{
	int phss,size=0;
	 S_PHS_RULE   *tmp_memb;
	int bit,i=0;
	unsigned char *phsf,*phsm;
	int in_buf_len = *header_size-1;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	in_buf++;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"====>\n");
	*header_size = 0;
	
	if((decomp_phs_rules == NULL ))
		return 0;
	
	
	tmp_memb = decomp_phs_rules;
	//BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nDECOMP:In phs_decompress PHSI 1  %d",phsi));
	//*header_size = tmp_memb->u8PHSFLength;
	phss         = tmp_memb->u8PHSS;
	phsf         = tmp_memb->u8PHSF;
	phsm         = tmp_memb->u8PHSM;

	if(phss > MAX_PHS_LENGTHS)
		phss = MAX_PHS_LENGTHS;	
	//BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nDECOMP:In phs_decompress PHSI  %d phss %d index %d",phsi,phss,index));
	while((phss > 0) && (size < in_buf_len))
	{
		bit =  ((*phsm << i)& SUPPRESS);
		
		if(bit == SUPPRESS)
		{
			*out_buf = *phsf;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nDECOMP:In phss  %d phsf %d ouput %d",
              phss,*phsf,*out_buf);
		}
		else
		{
			*out_buf = *in_buf;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECIEVE,DBG_LVL_ALL,"\nDECOMP:In phss  %d input %d ouput %d",
            phss,*in_buf,*out_buf);
			in_buf++;
			size++;
		}
		out_buf++;
		phsf++;
		phss--;
		i++;
		*header_size=*header_size + 1;
		
		if(i > MAX_NO_BIT)
		{
			i=0;
			phsm++;
		}
	}
	return size;
}
	



//-----------------------------------------------------------------------------
// Procedure:   phs_compress
//
// Description: This routine suppresses the static fields within the packet.Before 
// that it will verify the fields to be suppressed with the corresponding fields in the
// phsf. For verification it checks the phsv field of PHS rule. If set and verification
// succeeds it suppresses the field.If any one static field is found different none of 
// the static fields are suppressed then the packet is sent as uncompressed packet with
// phsi=0.
//
// Arguments:
//	phs_rule - ptr to PHS rule.
//	in_buf		- ptr to incoming packet buffer.
//	out_buf		- ptr to output buffer where the suppressed header is copied.
//	header_size	- ptr to field which holds the phss.	
//
// Returns:
//	size-The number of bytes copied into the output buffer i.e dynamic fields
//	0	-If PHS rule is NULL.If PHSV field is not set.If the verification fails.
//-----------------------------------------------------------------------------
int phs_compress(S_PHS_RULE  *phs_rule,unsigned char *in_buf
						,unsigned char *out_buf,UINT *header_size,UINT *new_header_size)
{
	unsigned char *old_addr = out_buf;
	int supress = 0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    if(phs_rule == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nphs_compress(): phs_rule null!");
		*out_buf = ZERO_PHSI;
		return STATUS_PHS_NOCOMPRESSION;
	}


	if(phs_rule->u8PHSS <= *new_header_size)
	{
		*header_size = phs_rule->u8PHSS;
	}
	else
	{
		*header_size = *new_header_size;
	}	
	//To copy PHSI
	out_buf++;
	supress = verify_suppress_phsf(in_buf,out_buf,phs_rule->u8PHSF,
        phs_rule->u8PHSM, phs_rule->u8PHSS, phs_rule->u8PHSV,new_header_size);

	if(supress == STATUS_PHS_COMPRESSED)
	{
		*old_addr = (unsigned char)phs_rule->u8PHSI;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In phs_compress phsi %d",phs_rule->u8PHSI);
	}
	else
	{
		*old_addr = ZERO_PHSI;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In phs_compress PHSV Verification failed");
	}
	return supress;
}


//-----------------------------------------------------------------------------
// Procedure:	verify_suppress_phsf
//
// Description: This routine verifies the fields of the packet and if all the 
// static fields are equal it adds the phsi of that PHS rule.If any static 
// field differs it woun't suppress any field.
//
// Arguments:
// rules_set	- ptr to classifier_rules.
// in_buffer	- ptr to incoming packet buffer.
// out_buffer	- ptr to output buffer where the suppressed header is copied.
// phsf			- ptr to phsf.
// phsm			- ptr to phsm.
// phss			- variable holding phss.
//
// Returns:
//	size-The number of bytes copied into the output buffer i.e dynamic fields.
//	0	-Packet has failed the verification.
//-----------------------------------------------------------------------------

 int verify_suppress_phsf(unsigned char *in_buffer,unsigned char *out_buffer,
								unsigned char *phsf,unsigned char *phsm,unsigned int phss,
								unsigned int phsv,UINT* new_header_size)
{
	unsigned int size=0;
	int bit,i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf PHSM - 0x%X",*phsm);

	
	if(phss>(*new_header_size))
	{
		phss=*new_header_size;
	}
	while(phss > 0)
	{
		bit = ((*phsm << i)& SUPPRESS);
		if(bit == SUPPRESS)
		{
			
			if(*in_buffer != *phsf)
			{
				if(phsv == VERIFY)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf failed for field  %d buf  %d phsf %d",phss,*in_buffer,*phsf);
					return STATUS_PHS_NOCOMPRESSION;
				}
			}
			else
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf success for field  %d buf  %d phsf %d",phss,*in_buffer,*phsf);
		}
		else
		{
			*out_buffer = *in_buffer;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In copying_header input %d  out %d",*in_buffer,*out_buffer);
			out_buffer++;
			size++;
		}
		in_buffer++;
		phsf++;
		phss--;
		i++;
		if(i > MAX_NO_BIT)
		{
			i=0;
			phsm++;
		}
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf success");
	*new_header_size = size;
	return STATUS_PHS_COMPRESSED;
}






