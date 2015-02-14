/* 
* InterfaceIdleMode.h
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


#ifndef _INTERFACE_IDLEMODE_H
#define _INTERFACE_IDLEMODE_H

INT InterfaceIdleModeWakeup(PMINI_ADAPTER Adapter);

INT InterfaceIdleModeRespond(PMINI_ADAPTER Adapter,PINT puiBuffer);

VOID InterfaceWriteIdleModeWakePattern(PMINI_ADAPTER Adapter);

INT InterfaceAbortIdlemode(PMINI_ADAPTER Adapter, unsigned int Pattern);

INT InterfaceWakeUp(PMINI_ADAPTER Adapter);

VOID InterfaceHandleShutdownModeWakeup(PMINI_ADAPTER Adapter);
#endif

