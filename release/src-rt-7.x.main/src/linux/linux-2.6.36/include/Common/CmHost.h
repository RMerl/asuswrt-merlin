/* 
* CmHost.h
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


/**********************************************************************
*	Definitions for Connection Management Requests structure
*      which we will use to setup our connection structures.Its high   
*      time we had a header file for CmHost.cpp to isolate the way
*      f/w sends DSx messages and the way we interpret them in code.
***********************************************************************/


#ifndef _CM_HOST_H
#define _CM_HOST_H

#pragma once
#pragma pack (push,4)

#define  DSX_MESSAGE_EXCHANGE_BUFFER        0xBF60AC84 // This contains the pointer
#define  DSX_MESSAGE_EXCHANGE_BUFFER_SIZE   72000 // 24 K Bytes

/// \brief structure stLocalSFAddRequest
typedef struct stLocalSFAddRequestAlt{
	B_UINT8                         u8Type;
	B_UINT8      u8Direction;
	
	B_UINT16                        u16TID;
   /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;

	
	/// \brief structure ParameterSet
    stServiceFlowParamSI              sfParameterSet;
	
    //USE_MEMORY_MANAGER();
}stLocalSFAddRequestAlt;

/// \brief structure stLocalSFAddIndication
typedef struct stLocalSFAddIndicationAlt{
    B_UINT8                         u8Type;
	B_UINT8      u8Direction;
	B_UINT16                         u16TID;
    /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;
	/// \brief structure AuthorizedSet
    stServiceFlowParamSI              sfAuthorizedSet;
    /// \brief structure AdmittedSet
    stServiceFlowParamSI              sfAdmittedSet;
	/// \brief structure ActiveSet
    stServiceFlowParamSI              sfActiveSet;
	
	B_UINT8 						u8CC;	/**<  Confirmation Code*/	
	B_UINT8 						u8Padd; 	/**<  8-bit Padding */
	B_UINT16						u16Padd;	/**< 16 bit Padding */
//    USE_MEMORY_MANAGER(); 
}stLocalSFAddIndicationAlt;

/// \brief structure stLocalSFAddConfirmation
typedef struct stLocalSFAddConfirmationAlt{
	B_UINT8                     u8Type;
	B_UINT8      				u8Direction;
	B_UINT16					u16TID;
    /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;
    /// \brief structure AuthorizedSet
    stServiceFlowParamSI              sfAuthorizedSet;
    /// \brief structure AdmittedSet
    stServiceFlowParamSI              sfAdmittedSet;
    /// \brief structure ActiveSet
    stServiceFlowParamSI              sfActiveSet;
}stLocalSFAddConfirmationAlt;


/// \brief structure stLocalSFChangeRequest
typedef struct stLocalSFChangeRequestAlt{
    B_UINT8                         u8Type;
	B_UINT8      u8Direction;
	B_UINT16					u16TID;
    /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;
	/*
	//Pointer location at which following Service Flow param Structure can be read
	//from the target. We get only the address location and we need to read out the
	//entire SF param structure at the given location on target
	*/
    /// \brief structure AuthorizedSet
    stServiceFlowParamSI              sfAuthorizedSet;
    /// \brief structure AdmittedSet
    stServiceFlowParamSI              sfAdmittedSet;
    /// \brief structure ParameterSet
    stServiceFlowParamSI              sfActiveSet;
	
	B_UINT8 						u8CC;	/**<  Confirmation Code*/	
	B_UINT8 						u8Padd; 	/**<  8-bit Padding */
	B_UINT16						u16Padd;	/**< 16 bit */

}stLocalSFChangeRequestAlt;

/// \brief structure stLocalSFChangeConfirmation
typedef struct stLocalSFChangeConfirmationAlt{
	B_UINT8                         u8Type;
	B_UINT8      					u8Direction;
	B_UINT16						u16TID;
    /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;
    /// \brief structure AuthorizedSet
    stServiceFlowParamSI              sfAuthorizedSet;
    /// \brief structure AdmittedSet
    stServiceFlowParamSI              sfAdmittedSet;
    /// \brief structure ActiveSet
    stServiceFlowParamSI              sfActiveSet;

}stLocalSFChangeConfirmationAlt;

/// \brief structure stLocalSFChangeIndication
typedef struct stLocalSFChangeIndicationAlt{
	B_UINT8                         u8Type;
		B_UINT8      u8Direction;
	B_UINT16						u16TID;
    /// \brief 16bitCID
    B_UINT16                        u16CID;
    /// \brief 16bitVCID
    B_UINT16                        u16VCID;
    /// \brief structure AuthorizedSet
    stServiceFlowParamSI              sfAuthorizedSet;
    /// \brief structure AdmittedSet
    stServiceFlowParamSI              sfAdmittedSet;
    /// \brief structure ActiveSet
    stServiceFlowParamSI              sfActiveSet;

	B_UINT8 						u8CC;	/**<  Confirmation Code*/	
	B_UINT8 						u8Padd; 	/**<  8-bit Padding */
	B_UINT16						u16Padd;	/**< 16 bit */

}stLocalSFChangeIndicationAlt;

ULONG StoreCmControlResponseMessage(PMINI_ADAPTER Adapter,PVOID pvBuffer,UINT *puBufferLength);

ULONG GetNextTargetBufferLocation(PMINI_ADAPTER Adapter,B_UINT16 tid);

INT AllocAdapterDsxBuffer(PMINI_ADAPTER Adapter);

INT FreeAdapterDsxBuffer(PMINI_ADAPTER Adapter);
ULONG SetUpTargetDsxBuffers(PMINI_ADAPTER Adapter);

BOOLEAN CmControlResponseMessage(PMINI_ADAPTER Adapter,PVOID pvBuffer);

VOID deleteSFBySfid(PMINI_ADAPTER Adapter, UINT uiSearchRuleIndex);

#pragma pack (pop)

#endif
