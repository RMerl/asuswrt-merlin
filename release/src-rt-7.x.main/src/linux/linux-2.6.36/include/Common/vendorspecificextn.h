/* 
*vendorspecificextn.h
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


#ifndef __VENDOR_EXTN_NVM_H__
#define __VENDOR_EXTN_NVM_H__

#define CONTINUE_COMMON_PATH 0xFFFF

INT vendorextnGetSectionInfo(PVOID  pContext,PFLASH2X_VENDORSPECIFIC_INFO pVendorInfo);
INT vendorextnExit(PMINI_ADAPTER Adapter);
INT vendorextnInit(PMINI_ADAPTER Adapter);
INT vendorextnIoctl(PMINI_ADAPTER Adapter, UINT cmd, ULONG arg);
INT vendorextnReadSection(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal, 
			UINT offset, UINT numOfBytes);
INT vendorextnWriteSection(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal,
			UINT offset, UINT numOfBytes, BOOLEAN bVerify);
INT vendorextnWriteSectionWithoutErase(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal,
			UINT offset, UINT numOfBytes);
INT vendorextndefineCS(PMINI_ADAPTER Adapter);
#endif /*  */
