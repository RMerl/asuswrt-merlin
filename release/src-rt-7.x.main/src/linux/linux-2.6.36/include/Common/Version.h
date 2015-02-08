/* 
* Version.h
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

/*Copyright (c) 2005 Beceem Communications Inc.

Module Name:

  Version.h

Abstract:

    
--*/

#ifndef VERSION_H
#define VERSION_H


#define VER_FILETYPE                VFT_DRV
#define VER_FILESUBTYPE             VFT2_DRV_NETWORK


#define VER_FILEVERSION             5.2.47
#define VER_FILEVERSION_STR         "5.2.47"

#undef VER_PRODUCTVERSION
#define VER_PRODUCTVERSION          VER_FILEVERSION

#undef VER_PRODUCTVERSION_STR
#define VER_PRODUCTVERSION_STR      VER_FILEVERSION_STR




//#include "common.ver"

#endif 	//VERSION_H
