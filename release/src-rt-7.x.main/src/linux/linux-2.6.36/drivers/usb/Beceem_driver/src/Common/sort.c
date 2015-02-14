/* 
* sort.c
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


/**********************************************************************
*	 This file contains the routines sorting the classification rules. 
***********************************************************************/

VOID SortPackInfo(PMINI_ADAPTER Adapter)
{
	UINT nIndex1;
	UINT nIndex2;

	BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CONN_MSG, DBG_LVL_ALL, "<=======");

	for(nIndex1 = 0; nIndex1 < NO_OF_QUEUES -2 ; nIndex1++)
	{
		for(nIndex2 = nIndex1 + 1 ; nIndex2 < NO_OF_QUEUES -1  ; nIndex2++)
		{
			if(Adapter->PackInfo[nIndex1].bValid && Adapter->PackInfo[nIndex2].bValid)
			{
				if(Adapter->PackInfo[nIndex2].u8TrafficPriority < 
						Adapter->PackInfo[nIndex1].u8TrafficPriority)
				{
					PacketInfo stTemppackInfo = Adapter->PackInfo[nIndex2];
					Adapter->PackInfo[nIndex2] = Adapter->PackInfo[nIndex1];
					Adapter->PackInfo[nIndex1] = stTemppackInfo;

				}
			}
		}
	}
}

VOID SortClassifiers(PMINI_ADAPTER Adapter)
{
	UINT nIndex1;
	UINT nIndex2;

	BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CONN_MSG, DBG_LVL_ALL, "<=======");

	for(nIndex1 = 0; nIndex1 < MAX_CLASSIFIERS -1 ; nIndex1++)
	{
		for(nIndex2 = nIndex1 + 1 ; nIndex2 < MAX_CLASSIFIERS  ; nIndex2++)
		{
			if(Adapter->astClassifierTable[nIndex1].bUsed && Adapter->astClassifierTable[nIndex2].bUsed)
			{
				if(Adapter->astClassifierTable[nIndex2].u8ClassifierRulePriority < 
					Adapter->astClassifierTable[nIndex1].u8ClassifierRulePriority)
				{
					S_CLASSIFIER_RULE stTempClassifierRule = Adapter->astClassifierTable[nIndex2];
					Adapter->astClassifierTable[nIndex2] = Adapter->astClassifierTable[nIndex1];
					Adapter->astClassifierTable[nIndex1] = stTempClassifierRule;

				}
			}
		}
	}
}
