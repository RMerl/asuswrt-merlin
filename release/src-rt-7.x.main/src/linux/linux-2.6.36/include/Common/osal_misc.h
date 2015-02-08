/* 
* osal_misc.h
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
*	Provides the OS Abstracted macros to access:
*			Linked Lists
*			Dispatcher Objects(Events,Semaphores,Spin Locks and the like)
*			Files
***********************************************************************/


#ifndef _OSAL_MISC_H_
#define _OSAL_MISC_H_
//OSAL Macros
//OSAL Primitives
typedef PUCHAR  POSAL_NW_PACKET  ;		//Nw packets

								
#define OsalMemAlloc(n,t) kmalloc(n,GFP_KERNEL)

#define OsalMemFree(x,n) bcm_kfree(x)

#define OsalMemMove(dest, src, len)		\
{										\
			memcpy(dest,src, len);		\
}

#define OsalZeroMemory(pDest, Len)		\
{										\
			memset(pDest,0,Len);		\
}

//#define OsalMemSet(pSrc,Char,Len) memset(pSrc,Char,Len)

bool OsalMemCompare(void *dest, void *src, UINT len);

#endif
	
